#include "Nexora/Editor/EditorWorkspace.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <sstream>
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
  return WriteWorkspace(documents, error);
}
bool ProjectWorkspace::RecoverWorkspace(std::string *error) {
  std::ifstream input(root_ / ".nexora/workspace.recovery");
  std::string line;
  std::vector<std::string> recovered;
  if (!input || !std::getline(input, line) || line != "schema=1")
    return false;
  while (std::getline(input, line))
    if (line.starts_with("document="))
      recovered.push_back(line.substr(9));
    else
      return false;
  return WriteWorkspace(recovered, error);
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
  if (name.empty() || (parent != 0 && std::ranges::find(nodes_, parent, &Node::id) == nodes_.end()))
    return 0;
  auto &entity = editor_.CreateEntity(scene_);
  nodes_.push_back({entity.id, parent, std::move(name)});
  return entity.id;
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
  auto node = std::ranges::find(nodes_, entity, &Node::id);
  if (node == nodes_.end() || entity == parent ||
      (parent && std::ranges::find(nodes_, parent, &Node::id) == nodes_.end()))
    return false;
  for (auto ancestor = parent; ancestor != 0;) {
    if (ancestor == entity)
      return false;
    const auto found = std::ranges::find(nodes_, ancestor, &Node::id);
    ancestor = found == nodes_.end() ? 0 : found->parent;
  }
  node->parent = parent;
  return true;
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
    if (const auto *entity = world_.FindEntity(source.id))
      editor_.SetTransform(id, entity->transform);
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
  for (const auto &node : nodes_)
    output += "node " + std::to_string(node.id) + " " + std::to_string(node.parent) + " " +
              node.name + "\n";
  output += "world\n" + *snapshot;
  return AtomicWrite(path, output, nullptr);
}
bool SceneDocument::Reload(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::string line, world_data;
  std::vector<Node> loaded;
  if (!input || !std::getline(input, line) || line != "NEXORA_EDITOR_SCENE 1")
    return false;
  while (std::getline(input, line) && line != "world") {
    if (!line.starts_with("node "))
      return false;
    std::istringstream parser(line.substr(5));
    Node node;
    if (!(parser >> node.id >> node.parent >> std::ws) || !std::getline(parser, node.name) ||
        node.name.empty())
      return false;
    loaded.push_back(std::move(node));
  }
  world_data.assign(std::istreambuf_iterator<char>(input), {});
  const auto scene = world_.LoadSceneSnapshot(world_data);
  if (!scene)
    return false;
  scene_ = *scene;
  nodes_ = std::move(loaded);
  selection_.clear();
  return true;
}
std::optional<runtime::Id> SceneDocument::Parent(runtime::Id entity) const {
  const auto found = std::ranges::find(nodes_, entity, &Node::id);
  return found == nodes_.end() ? std::nullopt : std::optional(found->parent);
}
std::string_view SceneDocument::Name(runtime::Id entity) const {
  const auto found = std::ranges::find(nodes_, entity, &Node::id);
  return found == nodes_.end() ? std::string_view{} : found->name;
}
} // namespace nexora::editor
