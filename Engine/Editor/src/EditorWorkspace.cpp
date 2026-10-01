#include "Nexora/Editor/EditorWorkspace.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <unordered_map>
#include <unordered_set>

namespace nexora::editor {
namespace {
constexpr std::array kPanels{PanelDescriptor{"nexora.project", "Project"},
                             PanelDescriptor{"nexora.hierarchy", "Hierarchy"},
                             PanelDescriptor{"nexora.scene", "Scene"},
                             PanelDescriptor{"nexora.game", "Game"},
                             PanelDescriptor{"nexora.inspector", "Inspector"},
                             PanelDescriptor{"nexora.content", "Content"},
                             PanelDescriptor{"nexora.console", "Console"},
                             PanelDescriptor{"nexora.profiler", "Profiler"}};

std::uint64_t Hash(std::string_view text, std::uint64_t seed) {
  auto value = seed;
  for (const unsigned char byte : text) {
    value ^= byte;
    value *= 1099511628211ULL;
  }
  return value;
}
std::string Hex(std::uint64_t value) {
  std::ostringstream stream;
  stream << std::hex << std::setfill('0') << std::setw(16) << value;
  return stream.str();
}
bool AtomicWrite(const std::filesystem::path &path, std::string_view contents, std::string *error) {
  std::error_code ec;
  std::filesystem::create_directories(path.parent_path(), ec);
  const auto temporary = path.string() + ".tmp";
  {
    std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
    if (!output || !(output << contents)) {
      if (error)
        *error = "could not write " + temporary;
      return false;
    }
  }
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
  }
  if (ec && error)
    *error = "could not replace " + path.string() + ": " + ec.message();
  return !ec;
}
std::string Lower(std::string_view value) {
  std::string result(value);
  std::ranges::transform(result, result.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}
} // namespace

std::span<const PanelDescriptor> ProductShell::Panels() noexcept { return kPanels; }
bool ProductShell::IsStablePanelId(std::string_view id) noexcept {
  return std::ranges::find(kPanels, id, &PanelDescriptor::id) != kPanels.end();
}
bool ProductShell::RouteCommand(std::string command) {
  if (command.empty() || !command.starts_with("editor."))
    return false;
  last_command_ = std::move(command);
  return true;
}

bool ProjectWorkspace::Create(const std::filesystem::path &root, std::string name,
                              std::string *error) {
  if (root.empty() || name.empty())
    return false;
  root_ = root;
  project_ = {std::move(name), ProjectDescriptor::kSchemaVersion};
  std::error_code ec;
  std::filesystem::create_directories(root_ / "Content", ec);
  std::filesystem::create_directories(root_ / ".nexora", ec);
  if (ec) {
    if (error)
      *error = ec.message();
    return false;
  }
  if (!AtomicWrite(root_ / "project.nexora", "schema=1\nname=" + project_.name + "\n", error))
    return false;
  return SaveWorkspace({}, error);
}
bool ProjectWorkspace::Open(const std::filesystem::path &root, std::string *error) {
  std::ifstream input(root / "project.nexora");
  std::string schema, name;
  if (!input || !std::getline(input, schema) || !std::getline(input, name) ||
      schema != "schema=1" || !name.starts_with("name=") || name.size() == 5) {
    if (error)
      *error = "invalid or unsupported project descriptor";
    return false;
  }
  root_ = root;
  project_ = {name.substr(5), 1};
  documents_.clear();
  std::ifstream workspace(root_ / ".nexora/workspace");
  std::string line;
  if (workspace && std::getline(workspace, line) && line == "schema=1")
    while (std::getline(workspace, line))
      if (line.starts_with("document="))
        documents_.push_back(line.substr(9));
  std::error_code ec;
  workspace_write_time_ = std::filesystem::last_write_time(root_ / ".nexora/workspace", ec);
  return true;
}
bool ProjectWorkspace::WriteWorkspace(std::span<const std::string> documents, std::string *error) {
  std::string contents = "schema=1\n";
  for (const auto &document : documents) {
    if (document.find('\n') != std::string::npos)
      return false;
    contents += "document=" + document + "\n";
  }
  if (!AtomicWrite(root_ / ".nexora/workspace", contents, error))
    return false;
  std::error_code ec;
  workspace_write_time_ = std::filesystem::last_write_time(root_ / ".nexora/workspace", ec);
  documents_.assign(documents.begin(), documents.end());
  return true;
}
bool ProjectWorkspace::SaveWorkspace(std::span<const std::string> documents, std::string *error) {
  std::string journal = "schema=1\n";
  for (const auto &document : documents)
    journal += "document=" + document + "\n";
  if (!AtomicWrite(root_ / ".nexora/workspace.recovery", journal, error))
    return false;
  if (!WriteWorkspace(documents, error))
    return false;
  std::error_code ec;
  std::filesystem::remove(root_ / ".nexora/workspace.recovery", ec);
  if (ec && error)
    *error = "workspace saved but recovery journal cleanup failed: " + ec.message();
  return !ec;
}
bool ProjectWorkspace::RecoverWorkspace(std::string *error) {
  std::ifstream input(root_ / ".nexora/workspace.recovery");
  std::string line;
  std::vector<std::string> recovered;
  if (!input || !std::getline(input, line) || line != "schema=1") {
    if (error)
      *error = "recovery journal is missing, unreadable, or uses an unsupported schema";
    return false;
  }
  while (std::getline(input, line))
    if (line.starts_with("document="))
      recovered.push_back(line.substr(9));
    else {
      if (error)
        *error = "recovery journal contains an invalid entry";
      return false;
    }
  input.close();
  if (!WriteWorkspace(recovered, error))
    return false;
  std::error_code ec;
  std::filesystem::remove(root_ / ".nexora/workspace.recovery", ec);
  if (ec && error)
    *error = "workspace recovered but journal cleanup failed: " + ec.message();
  return !ec;
}
bool ProjectWorkspace::DiscardRecovery(std::string *error) {
  std::error_code ec;
  const bool removed = std::filesystem::remove(root_ / ".nexora/workspace.recovery", ec);
  if (ec && error)
    *error = "could not discard recovery journal: " + ec.message();
  else if (!removed && error)
    *error = "recovery journal does not exist";
  return !ec && removed;
}
bool ProjectWorkspace::SaveEditorLayout(std::string_view layout, std::string *error) {
  if (error)
    error->clear();
  if (root_.empty() || layout.empty() || layout.find('\0') != std::string_view::npos) {
    if (error)
      *error = "editor layout is empty or invalid";
    return false;
  }
  return AtomicWrite(root_ / ".nexora/editor-layout.ini", "schema=1\n" + std::string(layout),
                     error);
}
std::optional<std::string> ProjectWorkspace::LoadEditorLayout(std::string *error) const {
  if (error)
    error->clear();
  std::ifstream input(root_ / ".nexora/editor-layout.ini", std::ios::binary);
  if (!input)
    return std::nullopt;
  std::string schema;
  if (!std::getline(input, schema)) {
    if (error)
      *error = "invalid or unsupported editor layout";
    return std::nullopt;
  }
  if (!schema.empty() && schema.back() == '\r')
    schema.pop_back();
  if (schema != "schema=0" && schema != "schema=1") {
    if (error)
      *error = "invalid or unsupported editor layout";
    return std::nullopt;
  }
  std::ostringstream contents;
  contents << input.rdbuf();
  if (!input.good() && !input.eof()) {
    if (error)
      *error = "could not read editor layout";
    return std::nullopt;
  }
  auto layout = contents.str();
  for (auto position = layout.find("\r\n"); position != std::string::npos;
       position = layout.find("\r\n", position))
    layout.replace(position, 2, "\n");
  if (layout.empty()) {
    if (error)
      *error = "editor layout is empty";
    return std::nullopt;
  }
  return layout;
}
bool ProjectWorkspace::HasRecoveryJournal() const {
  std::error_code ec;
  return !root_.empty() &&
         std::filesystem::is_regular_file(root_ / ".nexora/workspace.recovery", ec);
}
bool ProjectWorkspace::HasExternalChange() const {
  std::error_code ec;
  const auto current = std::filesystem::last_write_time(root_ / ".nexora/workspace", ec);
  return !ec && current != workspace_write_time_;
}

bool AssetWorkspace::ImportTree(const std::filesystem::path &content_root, Cancelled cancelled,
                                Progress progress) {
  entries_.clear();
  std::error_code ec;
  std::vector<std::filesystem::path> files;
  for (std::filesystem::recursive_directory_iterator it(content_root, ec), end; !ec && it != end;
       it.increment(ec))
    if (it->is_regular_file())
      files.push_back(it->path());
  if (ec)
    return false;
  std::ranges::sort(files);
  for (std::size_t index = 0; index < files.size(); ++index) {
    const auto relative = std::filesystem::relative(files[index], content_root).generic_string();
    AssetEntry entry{{Hash(relative, 1469598103934665603ULL), Hash(relative, 1099511628211ULL)},
                     relative,
                     Lower(files[index].extension().string()),
                     {},
                     ImportState::Pending,
                     {}};
    if (cancelled && cancelled()) {
      entry.state = ImportState::Cancelled;
      entries_.push_back(std::move(entry));
      if (progress)
        progress(index + 1, files.size());
      continue;
    }
    std::ifstream input(files[index], std::ios::binary);
    std::ostringstream bytes;
    bytes << input.rdbuf();
    if (!input.good() && !input.eof()) {
      entry.state = ImportState::Failed;
      entry.error = "read failed";
    } else {
      entry.artifact_hash = Hex(Hash(bytes.str(), Hash(relative, 1469598103934665603ULL)));
      entry.state = ImportState::Imported;
    }
    entries_.push_back(std::move(entry));
    if (progress)
      progress(index + 1, files.size());
  }
  return true;
}
std::vector<const AssetEntry *> AssetWorkspace::Search(std::string_view query,
                                                       std::string_view type) const {
  const auto needle = Lower(query), wanted = Lower(type);
  std::vector<const AssetEntry *> matches;
  for (const auto &entry : entries_)
    if ((needle.empty() || Lower(entry.relative_path).find(needle) != std::string::npos) &&
        (wanted.empty() || entry.type == wanted))
      matches.push_back(&entry);
  return matches;
}
const AssetEntry *AssetWorkspace::Find(runtime::AssetUuid id) const {
  const auto found = std::ranges::find(entries_, id, &AssetEntry::id);
  return found == entries_.end() ? nullptr : &*found;
}

SceneDocument::SceneDocument(runtime::World &world, runtime::Id scene)
    : world_(world), scene_(scene), editor_(world) {}
runtime::Id SceneDocument::Create(std::string name, runtime::Id parent) {
  // Save() persists each node as a single "node <id> <parent> <name>\n" line and
  // Reload() parses strictly line-by-line, so an embedded newline would split one
  // node into two physical lines and make the file permanently unloadable.
  if (name.empty() || name.find('\n') != std::string::npos ||
      name.find('\r') != std::string::npos ||
      (parent != 0 && std::ranges::find(nodes_, parent, &Node::id) == nodes_.end()))
    return 0;
  // The node list can outlive its entity (an undone creation), so the parent must also be a live
  // entity of this scene; checking first means the attach below cannot fail after creation.
  if (parent != 0) {
    const auto *scene = world_.FindScene(scene_);
    if (scene == nullptr ||
        std::ranges::find(scene->entities, parent, &runtime::Entity::id) == scene->entities.end())
      return 0;
  }
  const auto entity_id = editor_.CreateEntity(scene_);
  if (parent != 0) {
    // Part of creating the node rather than a separate undo step: undoing the creation removes it.
    runtime::WorldCommandBuffer attach;
    attach.SetParent(entity_id, parent, false);
    if (!attach.Apply(world_))
      return 0;
  }
  nodes_.push_back({entity_id, std::move(name)});
  return entity_id;
}
bool SceneDocument::Select(std::span<const runtime::Id> entities) {
  std::unordered_set<runtime::Id> unique;
  for (const auto id : entities)
    if (!world_.FindEntity(id) || !unique.insert(id).second)
      return false;
  selection_.assign(entities.begin(), entities.end());
  return true;
}
bool SceneDocument::Reparent(runtime::Id entity, runtime::Id parent) {
  if (std::ranges::find(nodes_, entity, &Node::id) == nodes_.end() ||
      (parent && std::ranges::find(nodes_, parent, &Node::id) == nodes_.end()))
    return false;
  // The runtime rejects self-parenting and cycles.
  return editor_.SetParent(entity, parent, true);
}
bool SceneDocument::Move(runtime::Id entity, runtime::Id parent, std::size_t index) {
  if (std::ranges::find(nodes_, entity, &Node::id) == nodes_.end() ||
      (parent && std::ranges::find(nodes_, parent, &Node::id) == nodes_.end()))
    return false;
  return editor_.Move(entity, parent, index, true);
}
bool SceneDocument::SetTransform(runtime::Id entity, runtime::Transform transform) {
  return editor_.SetTransform(entity, transform);
}
bool SceneDocument::CopySelection() {
  clipboard_.clear();
  for (const auto id : selection_) {
    const auto found = std::ranges::find(nodes_, id, &Node::id);
    if (found != nodes_.end())
      clipboard_.push_back(*found);
  }
  return !clipboard_.empty();
}
bool SceneDocument::Paste() {
  if (clipboard_.empty())
    return false;
  selection_.clear();
  for (const auto &source : clipboard_) {
    const auto id = Create(source.name + " Copy");
    // The copy is a root, so give it the source's world pose to make it appear in the same place.
    if (const auto world = world_.WorldTransform(source.id))
      editor_.SetTransform(id, *world);
    selection_.push_back(id);
  }
  return true;
}
bool SceneDocument::Undo() { return editor_.Undo(); }
bool SceneDocument::Save(const std::filesystem::path &path) const {
  const auto snapshot = world_.SaveScene(scene_);
  if (!snapshot)
    return false;
  std::string output = "NEXORA_EDITOR_SCENE 1\n";
  // The parent column duplicates the runtime hierarchy (snapshot version 3) for older readers.
  for (const auto &node : nodes_)
    output += "node " + std::to_string(node.id) + " " +
              std::to_string(world_.Parent(node.id).value_or(0)) + " " + node.name + "\n";
  output += "world\n" + *snapshot;
  return AtomicWrite(path, output, nullptr);
}
bool SceneDocument::Reload(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::string line, world_data;
  struct LoadedNode final {
    runtime::Id id{}, parent{};
    std::string name;
  };
  std::vector<LoadedNode> loaded;
  if (!input || !std::getline(input, line) || line != "NEXORA_EDITOR_SCENE 1")
    return false;
  while (std::getline(input, line) && line != "world") {
    if (!line.starts_with("node "))
      return false;
    std::istringstream parser(line.substr(5));
    LoadedNode node;
    if (!(parser >> node.id >> node.parent >> std::ws) || !std::getline(parser, node.name) ||
        node.name.empty())
      return false;
    loaded.push_back(std::move(node));
  }
  if (line != "world")
    return false;
  world_data.assign(std::istreambuf_iterator<char>(input), {});
  std::istringstream header(world_data);
  std::string magic;
  unsigned version{};
  if (!(header >> magic >> version))
    return false;
  std::unordered_set<runtime::Id> ids;
  for (const auto &node : loaded)
    if (!node.id || !ids.insert(node.id).second)
      return false;
  // From world snapshot version 3 on, the snapshot is authoritative for the hierarchy and the
  // node-line parent column is informational (a node may legitimately have a parent entity that is
  // not a node), so only legacy files, whose migration uses that column, validate it.
  for (const auto &node : loaded) {
    if (version >= 3)
      break;
    if (node.parent && !ids.contains(node.parent))
      return false;
    std::unordered_set<runtime::Id> ancestors;
    for (auto parent = node.parent; parent;) {
      if (!ancestors.insert(parent).second)
        return false;
      const auto found = std::ranges::find(loaded, parent, &LoadedNode::id);
      if (found == loaded.end())
        return false;
      parent = found->parent;
    }
  }
  // Before snapshot version 3 the hierarchy existed only in these node lines and never moved
  // anything, so every transform was authored in world space: apply those parents keeping the world
  // pose, so the migrated scene looks exactly as it did.
  const auto migration = [&loaded, version] {
    runtime::WorldCommandBuffer commands;
    if (version < 3)
      for (const auto &node : loaded)
        if (node.parent != 0)
          commands.SetParent(node.id, node.parent, true);
    return commands;
  };
  const bool migrate = migration().Size() != 0;
  if (migrate) {
    // Rehearse on a scratch world first so a failed migration never leaves a half-loaded scene.
    runtime::World rehearsal{world_.Kind()};
    if (!rehearsal.LoadSceneSnapshot(world_data) || !migration().Apply(rehearsal))
      return false;
  }
  const auto scene = world_.LoadSceneSnapshot(world_data);
  if (!scene || (migrate && !migration().Apply(world_)))
    return false;
  scene_ = *scene;
  nodes_.clear();
  for (auto &node : loaded)
    nodes_.push_back({node.id, std::move(node.name)});
  selection_.clear();
  return true;
}
std::optional<runtime::Id> SceneDocument::Parent(runtime::Id entity) const {
  if (std::ranges::find(nodes_, entity, &Node::id) == nodes_.end())
    return std::nullopt;
  return world_.Parent(entity);
}
std::string_view SceneDocument::Name(runtime::Id entity) const {
  const auto found = std::ranges::find(nodes_, entity, &Node::id);
  return found == nodes_.end() ? std::string_view{} : found->name;
}
std::vector<SceneDocument::NodeView> SceneDocument::Nodes() const {
  std::vector<NodeView> result;
  result.reserve(nodes_.size());
  std::unordered_map<runtime::Id, std::string_view> names;
  for (const auto &node : nodes_)
    names.emplace(node.id, node.name);
  if (const auto *scene = world_.FindScene(scene_))
    for (const auto &entity : scene->entities)
      if (const auto found = names.find(entity.id); found != names.end())
        result.push_back({entity.id, entity.parent, found->second});
  return result;
}
} // namespace nexora::editor
