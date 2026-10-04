#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/ViewportMath.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <locale>
#include <ranges>
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

std::uint64_t NextDocumentGeneration() noexcept {
  static std::atomic_uint64_t next{1};
  auto generation = next.fetch_add(1, std::memory_order_relaxed);
  if (generation == 0)
    generation = next.fetch_add(1, std::memory_order_relaxed);
  return generation;
}

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
    // Close before checking so a failed flush (e.g. a full disk) is not renamed over a good file.
    if (!output || !(output << contents) || (output.close(), output.fail())) {
      output.close();
      std::filesystem::remove(temporary, ec);
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
  if (ec) {
    std::error_code cleanup;
    std::filesystem::remove(temporary, cleanup);
    if (error)
      *error = "could not replace " + path.string() + ": " + ec.message();
  }
  return !ec;
}
std::string Lower(std::string_view value) {
  std::string result(value);
  std::ranges::transform(result, result.begin(),
                         [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
  return result;
}

void StripCarriageReturn(std::string &line) {
  if (!line.empty() && line.back() == '\r')
    line.pop_back();
}

runtime::AssetUuid DerivedAssetIdentity(std::string_view relative, std::uint64_t salt = 0) {
  std::string key(relative);
  if (salt != 0)
    key += "#" + std::to_string(salt);
  return {Hash(key, 1469598103934665603ULL), Hash(key, 1099511628211ULL)};
}

bool ReadAssetIdentity(const std::filesystem::path &path, runtime::AssetUuid &id, std::string &type,
                       std::string &error) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  const auto size = std::filesystem::file_size(path, ec);
  if (ec || std::filesystem::is_symlink(status) || !std::filesystem::is_regular_file(status) ||
      size > 4096) {
    error = "asset identity sidecar is unavailable, unsafe, or too large: " + path.string();
    return false;
  }
  std::ifstream input(path, std::ios::binary);
  std::string schema, uuid, type_line, extra;
  if (!input || !std::getline(input, schema) || !std::getline(input, uuid) ||
      !std::getline(input, type_line) || std::getline(input, extra)) {
    error = "asset identity sidecar is malformed: " + path.string();
    return false;
  }
  StripCarriageReturn(schema);
  StripCarriageReturn(uuid);
  StripCarriageReturn(type_line);
  const auto parsed = uuid.starts_with("uuid=")
                          ? runtime::AssetUuid::Parse(std::string_view(uuid).substr(5))
                          : std::nullopt;
  if (schema != "schema=1" || !parsed || !type_line.starts_with("type=")) {
    error = "asset identity sidecar has an invalid or unsupported schema: " + path.string();
    return false;
  }
  id = *parsed;
  type = Lower(std::string_view(type_line).substr(5));
  return true;
}

bool WriteAssetIdentity(const std::filesystem::path &path, runtime::AssetUuid id,
                        std::string_view type, std::string &error) {
  if (id == runtime::AssetUuid{} || type.find('\n') != std::string_view::npos ||
      type.find('\r') != std::string_view::npos) {
    error = "asset identity metadata is invalid";
    return false;
  }
  return AtomicWrite(path, "schema=1\nuuid=" + id.ToString() + "\ntype=" + std::string(type) + "\n",
                     &error);
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

std::filesystem::path AssetWorkspace::IdentitySidecar(const std::filesystem::path &asset_path) {
  auto sidecar = asset_path;
  sidecar += ".meta";
  return sidecar;
}

bool AssetWorkspace::ImportTree(const std::filesystem::path &content_root, Cancelled cancelled,
                                Progress progress, AssetIdentityMode identity_mode,
                                std::string *error) {
  std::error_code ec;
  const auto root = std::filesystem::canonical(content_root, ec);
  if (ec || !std::filesystem::is_directory(root, ec)) {
    if (error)
      *error = "content root is unavailable";
    return false;
  }
  std::vector<std::filesystem::path> files;
  for (std::filesystem::recursive_directory_iterator it(root, ec), end; !ec && it != end;
       it.increment(ec)) {
    const auto status = it->symlink_status(ec);
    if (ec)
      break;
    if (std::filesystem::is_regular_file(status) &&
        Lower(it->path().extension().string()) != ".meta")
      files.push_back(it->path());
  }
  if (ec) {
    if (error)
      *error = "content tree could not be enumerated: " + ec.message();
    return false;
  }
  std::ranges::sort(files);

  struct Identity final {
    runtime::AssetUuid id;
    std::string type;
  };
  std::unordered_map<std::string, Identity> identities;
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> used_ids;
  if (identity_mode != AssetIdentityMode::DerivedFromPath) {
    for (const auto &file : files) {
      const auto relative = std::filesystem::relative(file, root, ec).generic_string();
      if (ec) {
        if (error)
          *error = "asset path could not be made project-relative: " + ec.message();
        return false;
      }
      const auto sidecar = IdentitySidecar(file);
      const auto sidecar_status = std::filesystem::symlink_status(sidecar, ec);
      if (ec == std::errc::no_such_file_or_directory) {
        ec.clear();
        continue;
      }
      if (ec) {
        if (error)
          *error = "asset identity sidecar could not be inspected: " + ec.message();
        return false;
      }
      if (!std::filesystem::exists(sidecar_status))
        continue;
      Identity identity;
      std::string identity_error;
      if (!ReadAssetIdentity(sidecar, identity.id, identity.type, identity_error) ||
          !used_ids.insert(identity.id).second) {
        if (error)
          *error = identity_error.empty() ? "asset identity UUID is duplicated: " + sidecar.string()
                                          : std::move(identity_error);
        return false;
      }
      identities.emplace(relative, std::move(identity));
    }
  }

  std::vector<AssetEntry> entries;
  entries.reserve(files.size());
  std::size_t mesh_bytes{};
  for (std::size_t index = 0; index < files.size(); ++index) {
    const auto relative = std::filesystem::relative(files[index], root, ec).generic_string();
    if (ec) {
      if (error)
        *error = "asset path could not be made project-relative: " + ec.message();
      return false;
    }
    auto type = Lower(files[index].extension().string());
    auto id = DerivedAssetIdentity(relative);
    if (identity_mode != AssetIdentityMode::DerivedFromPath) {
      const auto existing = identities.find(relative);
      if (existing != identities.end()) {
        id = existing->second.id;
        type = existing->second.type;
      } else {
        if (identity_mode == AssetIdentityMode::PersistentReadOnly) {
          if (error)
            *error = "asset identity sidecar is missing in read-only mode: " +
                     IdentitySidecar(files[index]).string();
          return false;
        }
        std::uint64_t salt = 0;
        while (id == runtime::AssetUuid{} || used_ids.contains(id)) {
          if (salt == std::numeric_limits<std::uint64_t>::max()) {
            if (error)
              *error = "asset identity space is exhausted";
            return false;
          }
          id = DerivedAssetIdentity(relative, ++salt);
        }
        if (cancelled && cancelled()) {
          // Do not create identity sidecars for a cancelled import; the result is discarded.
          entries.push_back({id, relative, std::move(type), {}, ImportState::Cancelled, {}});
          if (progress)
            progress(index + 1, files.size());
          continue;
        }
        std::string identity_error;
        if (!WriteAssetIdentity(IdentitySidecar(files[index]), id, type, identity_error)) {
          if (error)
            *error = std::move(identity_error);
          return false;
        }
        used_ids.insert(id);
      }
    }
    AssetEntry entry{id, relative, std::move(type), {}, ImportState::Pending, {}};
    if (cancelled && cancelled()) {
      entry.state = ImportState::Cancelled;
      entries.push_back(std::move(entry));
      if (progress)
        progress(index + 1, files.size());
      continue;
    }
    std::ifstream input(files[index], std::ios::binary);
    std::ostringstream bytes;
    bool oversized = false;
    bool read_cancelled = false;
    if (entry.type == ".obj") {
      std::array<char, 8192> block{};
      std::size_t total{};
      while (input && total <= kMaximumObjSourceBytes) {
        if (cancelled && cancelled()) {
          read_cancelled = true;
          break;
        }
        const auto count = std::min(block.size(), kMaximumObjSourceBytes + 1 - total);
        input.read(block.data(), static_cast<std::streamsize>(count));
        const auto read = input.gcount();
        total += static_cast<std::size_t>(read);
        if (total > kMaximumObjSourceBytes) {
          oversized = true;
          break;
        }
        bytes.write(block.data(), read);
      }
    } else {
      bytes << input.rdbuf();
    }
    if (read_cancelled) {
      entry.state = ImportState::Cancelled;
    } else if (oversized) {
      entry.state = ImportState::Failed;
      entry.error = "OBJ source exceeds the 16 MiB limit.";
    } else if (!input.is_open() || (!input.good() && !input.eof())) {
      entry.state = ImportState::Failed;
      entry.error = "read failed";
    } else {
      const auto source = bytes.str();
      if (entry.type == ".obj") {
        auto imported = ImportObjMesh(source, cancelled);
        if (imported.cancelled) {
          entry.state = ImportState::Cancelled;
        } else if (!imported.geometry) {
          entry.state = ImportState::Failed;
          entry.error = "OBJ line " + std::to_string(imported.line) + ": " + imported.error;
        } else {
          const auto required = imported.geometry->vertices.capacity() * sizeof(MeshVertex) +
                                imported.geometry->indices.capacity() * sizeof(std::uint16_t);
          if (required > kMaximumWorkspaceMeshBytes - mesh_bytes) {
            entry.state = ImportState::Failed;
            entry.error = "Workspace CPU mesh geometry exceeds the 128 MiB budget.";
          } else {
            mesh_bytes += required;
            entry.mesh = std::make_shared<const MeshGeometry>(std::move(*imported.geometry));
            entry.state = ImportState::Imported;
          }
        }
      } else {
        entry.state = ImportState::Imported;
      }
      if (entry.state == ImportState::Imported)
        entry.artifact_hash = Hex(Hash(source, Hash(entry.id.ToString(), 1469598103934665603ULL)));
    }
    entries.push_back(std::move(entry));
    if (progress)
      progress(index + 1, files.size());
  }
  entries_ = std::move(entries);
  content_root_ = root;
  identity_mode_ = identity_mode;
  if (error)
    error->clear();
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
    : world_(world), scene_(scene), editor_(world), document_generation_(NextDocumentGeneration()) {
  saved_signature_ = StateSignature().value_or(std::string{});
}
void SceneDocument::PushUndo(UndoEntry entry) {
  redo_.clear();
  undo_.push_back(std::move(entry));
}
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
  const auto entity_id = editor_.CreateEntity(scene_, parent);
  if (entity_id == 0)
    return 0;
  const auto generation = next_entity_generation_++;
  if (next_entity_generation_ == 0)
    ++next_entity_generation_;
  nodes_.push_back({entity_id, std::move(name), generation});
  PushUndo({});
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
bool SceneDocument::Select(std::span<const NodeKey> entities) {
  std::vector<runtime::Id> ids;
  ids.reserve(entities.size());
  for (const auto key : entities) {
    const auto found = std::ranges::find(nodes_, key.id, &Node::id);
    if (key.document_generation != document_generation_ || found == nodes_.end() ||
        found->generation != key.entity_generation || world_.FindEntity(key.id) == nullptr)
      return false;
    ids.push_back(key.id);
  }
  return Select(ids);
}
bool SceneDocument::Rename(NodeKey entity, std::string name) {
  if (name.empty() || name.find('\n') != std::string::npos || name.find('\r') != std::string::npos)
    return false;
  const auto found = std::ranges::find(nodes_, entity.id, &Node::id);
  if (entity.document_generation != document_generation_ || found == nodes_.end() ||
      found->generation != entity.entity_generation || world_.FindEntity(entity.id) == nullptr)
    return false;
  if (found->name == name)
    return true;
  PushUndo({UndoEntry::Kind::Rename, entity, std::exchange(found->name, std::move(name))});
  return true;
}
bool SceneDocument::Reparent(runtime::Id entity, runtime::Id parent) {
  if (std::ranges::find(nodes_, entity, &Node::id) == nodes_.end() ||
      (parent && std::ranges::find(nodes_, parent, &Node::id) == nodes_.end()))
    return false;
  // The runtime rejects self-parenting and cycles.
  if (!editor_.SetParent(entity, parent, true))
    return false;
  PushUndo({});
  return true;
}
bool SceneDocument::Move(runtime::Id entity, runtime::Id parent, std::size_t index) {
  if (std::ranges::find(nodes_, entity, &Node::id) == nodes_.end() ||
      (parent && std::ranges::find(nodes_, parent, &Node::id) == nodes_.end()))
    return false;
  if (!editor_.Move(entity, parent, index, true))
    return false;
  PushUndo({});
  return true;
}
bool SceneDocument::Move(NodeKey entity, std::optional<NodeKey> parent, std::size_t index) {
  const auto current = Key(entity.id);
  if (!current || *current != entity)
    return false;
  if (parent) {
    const auto current_parent = Key(parent->id);
    if (!current_parent || *current_parent != *parent)
      return false;
  }
  return Move(entity.id, parent ? parent->id : 0, index);
}
bool SceneDocument::SetTransform(runtime::Id entity, runtime::Transform transform) {
  const auto key = Key(entity);
  if (!key)
    return false;
  const std::array keys{*key};
  const std::array transforms{transform};
  return SetTransforms(keys, transforms);
}
bool SceneDocument::SetCamera(NodeKey entity, std::optional<runtime::CameraComponent> camera) {
  if (Key(entity.id) != entity)
    return false;
  const auto *existing = world_.FindEntity(entity.id);
  if (existing == nullptr)
    return false;
  if (existing->camera == camera.has_value() &&
      (!camera || (existing->camera_data.vertical_field_of_view == camera->vertical_field_of_view &&
                   existing->camera_data.near_plane == camera->near_plane &&
                   existing->camera_data.far_plane == camera->far_plane)))
    return true;
  if (!editor_.SetCamera(entity.id, camera))
    return false;
  PushUndo({});
  return true;
}
bool SceneDocument::SetLight(NodeKey entity, std::optional<runtime::LightComponent> light) {
  if (Key(entity.id) != entity)
    return false;
  const auto *existing = world_.FindEntity(entity.id);
  if (existing == nullptr)
    return false;
  if (existing->light == light.has_value() &&
      (!light || existing->light_data.intensity == light->intensity))
    return true;
  if (!editor_.SetLight(entity.id, light))
    return false;
  PushUndo({});
  return true;
}
bool SceneDocument::SetMeshRenderer(NodeKey entity, std::optional<runtime::MeshComponent> mesh) {
  if (Key(entity.id) != entity)
    return false;
  const auto *existing = world_.FindEntity(entity.id);
  if (existing == nullptr)
    return false;
  if (existing->mesh_renderer == mesh.has_value() &&
      (!mesh || (existing->mesh_data.mesh == mesh->mesh &&
                 existing->mesh_data.material.shader == mesh->material.shader)))
    return true;
  if (!editor_.SetMeshRenderer(entity.id, mesh))
    return false;
  PushUndo({});
  return true;
}
bool SceneDocument::SetTransforms(std::span<const NodeKey> entities,
                                  std::span<const runtime::Transform> transforms) {
  if (entities.size() != transforms.size())
    return false;
  std::vector<runtime::Id> ids;
  UndoEntry undo;
  ids.reserve(entities.size());
  undo.previous_hints.reserve(entities.size());
  for (const auto key : entities) {
    if (Key(key.id) != key)
      return false;
    ids.push_back(key.id);
    const auto node = std::ranges::find(nodes_, key.id, &Node::id);
    undo.previous_hints.emplace_back(key, node->euler_hint);
  }
  if (!editor_.SetTransforms(ids, transforms))
    return false;
  PushUndo(std::move(undo));
  return true;
}

bool SceneDocument::TranslateSelectionXZ(std::span<const NodeKey> entities, double dx, double dz) {
  return TranslateSelection(entities, dx, 0.0, dz);
}

bool SceneDocument::TranslateSelection(std::span<const NodeKey> entities, double dx, double dy,
                                       double dz) {
  if (entities.empty() || !std::isfinite(dx) || !std::isfinite(dy) || !std::isfinite(dz) ||
      (dx == 0.0 && dy == 0.0 && dz == 0.0))
    return false;
  GizmoOperation translation;
  translation.kind = GizmoOperation::Kind::Translate;
  translation.translation = {dx, dy, dz};
  return ApplySelectionGizmo(entities, translation);
}

bool SceneDocument::ApplySelectionGizmo(std::span<const NodeKey> entities,
                                        const GizmoOperation &operation) {
  const auto edits = SelectionGizmoEdits(entities, operation);
  if (!edits)
    return false;
  std::vector<NodeKey> keys;
  std::vector<runtime::Transform> transforms;
  for (const auto &[key, transform] : *edits) {
    keys.push_back(key);
    transforms.push_back(transform);
  }
  return SetTransforms(keys, transforms);
}

std::optional<std::vector<std::pair<SceneDocument::NodeKey, runtime::Transform>>>
SceneDocument::SelectionGizmoEdits(std::span<const NodeKey> entities,
                                   const GizmoOperation &operation) const {
  if (entities.empty())
    return std::nullopt;
  std::vector<runtime::Id> ids;
  std::unordered_set<runtime::Id> unique;
  ids.reserve(entities.size());
  for (const auto key : entities) {
    if (Key(key.id) != key || !unique.insert(key.id).second)
      return std::nullopt;
    ids.push_back(key.id);
  }
  const auto roots = GizmoRoots(world_, ids);
  const auto targets = GizmoTargets(world_, roots);
  if (!targets || targets->size() != roots.size())
    return std::nullopt;
  const auto transforms = ApplyGizmo(*targets, operation);
  if (!transforms)
    return std::nullopt;
  std::vector<std::pair<NodeKey, runtime::Transform>> edits;
  for (std::size_t i = 0; i < roots.size(); ++i)
    edits.emplace_back(*Key(roots[i]), (*transforms)[i]);
  return edits;
}

std::optional<std::unordered_map<runtime::Id, runtime::Transform>>
SceneDocument::PreviewSelectionGizmo(std::span<const NodeKey> entities,
                                     const GizmoOperation &operation) const {
  const auto edits = SelectionGizmoEdits(entities, operation);
  if (!edits)
    return std::nullopt;
  std::unordered_map<runtime::Id, runtime::Transform> overrides, poses;
  for (const auto &[key, transform] : *edits)
    overrides.emplace(key.id, transform);
  poses.reserve(nodes_.size());
  // Memoized, iterative ancestry traversal avoids recursion and recomputing shared parents.
  for (const auto &node : nodes_) {
    std::vector<runtime::Id> chain;
    std::unordered_set<runtime::Id> visiting;
    auto id = node.id;
    while (id != 0 && !poses.contains(id)) {
      const auto *entity = world_.FindEntity(id);
      if (!entity || !visiting.insert(id).second)
        return std::nullopt;
      chain.push_back(id);
      id = entity->parent;
    }
    runtime::Transform parent = id == 0 ? runtime::Transform{} : poses.at(id);
    for (auto it = chain.rbegin(); it != chain.rend(); ++it) {
      const auto replacement = overrides.find(*it);
      const auto local =
          replacement == overrides.end() ? world_.FindEntity(*it)->transform : replacement->second;
      parent = runtime::ComposeTransforms(parent, local);
      if (!runtime::IsValidTransform(parent))
        return std::nullopt;
      poses.emplace(*it, parent);
    }
  }
  return poses;
}

std::optional<runtime::Transform> SceneDocument::SelectionGizmoFrame(GizmoPivot pivot) const {
  const auto roots = GizmoRoots(world_, selection_);
  if (roots.empty())
    return std::nullopt;
  const auto pose = world_.WorldTransform(roots.front());
  if (!pose || pivot == GizmoPivot::Pivot)
    return pose;
  const auto center = SelectionCenter(world_, roots);
  if (!center)
    return std::nullopt;
  return runtime::WithPosition(*pose, center->x, center->y, center->z);
}

bool SceneDocument::SetEulerField(std::span<const NodeKey> entities, std::size_t axis,
                                  double degrees) {
  if (axis >= 3 || entities.empty() || !std::isfinite(degrees))
    return false;
  std::vector<runtime::Transform> transforms;
  std::vector<EulerDegrees> angles;
  transforms.reserve(entities.size());
  angles.reserve(entities.size());
  for (const auto key : entities) {
    if (Key(key.id) != key)
      return false;
    auto authored = EulerAngles(key.id);
    const auto transform = Transform(key.id);
    if (!authored || !transform)
      return false;
    (*authored)[axis] = degrees;
    const auto changed = WithEulerDegrees(*transform, *authored);
    if (!changed)
      return false;
    transforms.push_back(*changed);
    angles.push_back(*authored);
  }
  if (!SetTransforms(entities, transforms))
    return false;
  for (std::size_t i = 0; i < entities.size(); ++i) {
    const auto node = std::ranges::find(nodes_, entities[i].id, &Node::id);
    node->euler_hint = EulerHint{*Transform(entities[i].id), angles[i]};
  }
  return true;
}

std::optional<EulerDegrees> SceneDocument::EulerAngles(runtime::Id entity) const noexcept {
  const auto node = std::ranges::find(nodes_, entity, &Node::id);
  const auto *live = world_.FindEntity(entity);
  if (node == nodes_.end() || live == nullptr)
    return std::nullopt;
  if (node->euler_hint && SameRotation(node->euler_hint->transform, live->transform))
    return node->euler_hint->degrees;
  return ToEulerDegrees(live->transform);
}

std::optional<runtime::Transform> SceneDocument::Transform(runtime::Id entity) const noexcept {
  const auto *found = world_.FindEntity(entity);
  if (found == nullptr || std::ranges::find(nodes_, entity, &Node::id) == nodes_.end())
    return std::nullopt;
  return found->transform;
}
std::optional<runtime::CameraComponent> SceneDocument::Camera(NodeKey entity) const noexcept {
  if (Key(entity.id) != entity)
    return std::nullopt;
  const auto *found = world_.FindEntity(entity.id);
  return found != nullptr && found->camera ? std::optional(found->camera_data) : std::nullopt;
}
std::optional<runtime::LightComponent> SceneDocument::Light(NodeKey entity) const noexcept {
  if (Key(entity.id) != entity)
    return std::nullopt;
  const auto *found = world_.FindEntity(entity.id);
  return found != nullptr && found->light ? std::optional(found->light_data) : std::nullopt;
}
std::optional<runtime::MeshComponent> SceneDocument::MeshRenderer(NodeKey entity) const noexcept {
  if (Key(entity.id) != entity)
    return std::nullopt;
  const auto *found = world_.FindEntity(entity.id);
  return found != nullptr && found->mesh_renderer ? std::optional(found->mesh_data) : std::nullopt;
}
std::optional<runtime::Transform> SceneDocument::WorldTransform(runtime::Id entity) const noexcept {
  if (std::ranges::find(nodes_, entity, &Node::id) == nodes_.end())
    return std::nullopt;
  return world_.WorldTransform(entity);
}
bool SceneDocument::CopySelection() {
  std::vector<ClipboardNode> captured;
  captured.reserve(selection_.size());
  for (const auto id : selection_) {
    const auto found = std::ranges::find(nodes_, id, &Node::id);
    const auto pose = world_.WorldTransform(id);
    if (found == nodes_.end() || !pose)
      return false;
    captured.push_back({found->name, *pose});
  }
  if (captured.empty())
    return false;
  clipboard_ = std::move(captured);
  return !clipboard_.empty();
}
bool SceneDocument::Paste() {
  if (clipboard_.empty())
    return false;
  const auto previous_selection = selection_;
  std::vector<runtime::Id> pasted;
  pasted.reserve(clipboard_.size());
  bool placed_all = true;
  for (const auto &source : clipboard_) {
    const auto id = Create(source.name + " Copy");
    if (id == 0) {
      placed_all = false;
      break;
    }
    pasted.push_back(id);
    // Create owns this undo step. Applying the copied world pose directly makes one Undo remove
    // the pasted entity, rather than first resetting its transform and leaving it behind.
    runtime::WorldCommandBuffer place;
    place.SetTransform(id, source.world_transform);
    if (!place.Apply(world_)) {
      placed_all = false;
      break;
    }
  }
  if (!placed_all) {
    for (std::size_t index = 0; index < pasted.size(); ++index)
      static_cast<void>(Undo());
    selection_ = previous_selection;
    return false;
  }
  selection_ = std::move(pasted);
  return true;
}
bool SceneDocument::DuplicateSelection() {
  auto previous_clipboard = std::move(clipboard_);
  const bool duplicated = CopySelection() && Paste();
  clipboard_ = std::move(previous_clipboard);
  return duplicated;
}
bool SceneDocument::DeleteSelection() {
  if (selection_.empty())
    return false;
  const auto *scene = world_.FindScene(scene_);
  if (scene == nullptr)
    return false;
  std::unordered_set<runtime::Id> selected(selection_.begin(), selection_.end());
  if (selected.size() != selection_.size())
    return false;
  for (const auto id : selection_)
    if (!Key(id) ||
        std::ranges::find(scene->entities, id, &runtime::Entity::id) == scene->entities.end())
      return false;

  std::vector<runtime::Id> roots;
  for (const auto id : selection_) {
    auto ancestor = world_.Parent(id).value_or(0);
    while (ancestor != 0 && !selected.contains(ancestor))
      ancestor = world_.Parent(ancestor).value_or(0);
    if (ancestor == 0)
      roots.push_back(id);
  }
  std::size_t deleted = 0;
  for (const auto root : roots) {
    const auto subtree = world_.Subtree(root);
    std::unordered_set<runtime::Id> subtree_ids(subtree.begin(), subtree.end());
    UndoEntry entry;
    entry.previous_selection = selection_;
    for (const auto &node : nodes_)
      if (subtree_ids.contains(node.id))
        entry.deleted_nodes.push_back(node);
    if (!editor_.DestroyEntity(scene_, root)) {
      for (std::size_t index = 0; index < deleted; ++index)
        static_cast<void>(Undo());
      redo_.clear();
      return false;
    }
    PushUndo(std::move(entry));
    std::erase_if(nodes_, [&](const Node &node) { return subtree_ids.contains(node.id); });
    std::erase_if(selection_, [&](runtime::Id id) { return subtree_ids.contains(id); });
    ++deleted;
  }
  return deleted != 0;
}
bool SceneDocument::Undo() {
  if (undo_.empty())
    return false;
  auto &entry = undo_.back();
  entry.redo_nodes = nodes_;
  entry.redo_selection = selection_;
  if (entry.kind == UndoEntry::Kind::Runtime) {
    if (!editor_.Undo())
      return false;
    // Runtime undo can destroy a newly created entity. Remove its authoring metadata and
    // selection before a later save serializes nodes that no longer exist in the snapshot.
    std::erase_if(nodes_,
                  [this](const Node &node) { return world_.FindEntity(node.id) == nullptr; });
    std::erase_if(selection_, [this](runtime::Id id) { return world_.FindEntity(id) == nullptr; });
    for (const auto &[key, hint] : entry.previous_hints) {
      const auto node = std::ranges::find(nodes_, key.id, &Node::id);
      if (node != nodes_.end() && key.document_generation == document_generation_ &&
          node->generation == key.entity_generation)
        node->euler_hint = hint;
    }
    if (!entry.deleted_nodes.empty()) {
      for (const auto &node : entry.deleted_nodes)
        if (world_.FindEntity(node.id) != nullptr)
          nodes_.push_back(node);
      selection_ = entry.previous_selection;
      std::erase_if(selection_,
                    [this](runtime::Id id) { return world_.FindEntity(id) == nullptr; });
    }
  } else {
    const auto found = std::ranges::find(nodes_, entry.entity.id, &Node::id);
    if (entry.entity.document_generation != document_generation_ || found == nodes_.end() ||
        found->generation != entry.entity.entity_generation)
      return false;
    found->name = entry.previous_name;
  }
  redo_.push_back(std::move(entry));
  undo_.pop_back();
  return true;
}
bool SceneDocument::Redo() {
  if (redo_.empty())
    return false;
  auto &entry = redo_.back();
  if (entry.kind == UndoEntry::Kind::Runtime && !editor_.Redo())
    return false;
  if (entry.kind == UndoEntry::Kind::Rename) {
    const auto found = std::ranges::find(nodes_, entry.entity.id, &Node::id);
    if (entry.entity.document_generation != document_generation_ || found == nodes_.end() ||
        found->generation != entry.entity.entity_generation)
      return false;
  }
  nodes_ = entry.redo_nodes;
  selection_ = entry.redo_selection;
  std::erase_if(nodes_, [this](const Node &node) { return world_.FindEntity(node.id) == nullptr; });
  std::erase_if(selection_, [this](runtime::Id id) { return world_.FindEntity(id) == nullptr; });
  undo_.push_back(std::move(entry));
  redo_.pop_back();
  return true;
}

std::optional<std::string> SceneDocument::StateSignature() const {
  const auto snapshot = world_.SaveScene(scene_);
  const auto *scene = world_.FindScene(scene_);
  if (!snapshot || scene == nullptr)
    return std::nullopt;
  std::istringstream input(*snapshot);
  std::string header, line;
  if (!std::getline(input, header))
    return std::nullopt;
  std::unordered_map<runtime::Id, std::string> entity_lines;
  std::unordered_map<runtime::Id, std::vector<runtime::Id>> children;
  for (const auto &entity : scene->entities) {
    if (!std::getline(input, line))
      return std::nullopt;
    entity_lines.emplace(entity.id, std::move(line));
    children[entity.parent].push_back(entity.id);
  }
  if (std::getline(input, line))
    return std::nullopt;
  std::string signature = header + '\n';
  std::vector<runtime::Id> pending;
  for (const auto id : std::views::reverse(children[0]))
    pending.push_back(id);
  std::unordered_set<runtime::Id> visited;
  while (!pending.empty()) {
    const auto id = pending.back();
    pending.pop_back();
    const auto found = entity_lines.find(id);
    if (!visited.insert(id).second || found == entity_lines.end())
      return std::nullopt;
    signature += found->second + '\n';
    for (const auto child : std::views::reverse(children[id]))
      pending.push_back(child);
  }
  if (visited.size() != scene->entities.size())
    return std::nullopt;

  std::vector<const Node *> ordered_nodes;
  ordered_nodes.reserve(nodes_.size());
  for (const auto &node : nodes_)
    ordered_nodes.push_back(&node);
  std::ranges::sort(ordered_nodes, {}, [](const Node *node) { return node->id; });
  std::ostringstream metadata;
  metadata.imbue(std::locale::classic());
  metadata << std::setprecision(std::numeric_limits<double>::max_digits10);
  for (const auto *node : ordered_nodes) {
    metadata << "node " << node->id << ' ' << node->name.size() << ':' << node->name << '\n';
    const auto *entity = world_.FindEntity(node->id);
    if (node->euler_hint && entity && SameRotation(node->euler_hint->transform, entity->transform))
      metadata << "euler " << node->id << ' ' << node->euler_hint->degrees[0] << ' '
               << node->euler_hint->degrees[1] << ' ' << node->euler_hint->degrees[2] << '\n';
  }
  signature += metadata.str();
  return signature;
}

bool SceneDocument::Dirty() const {
  const auto signature = StateSignature();
  return !signature || *signature != saved_signature_;
}

bool SceneDocument::Save(const std::filesystem::path &path) const {
  const auto signature = StateSignature();
  if (!signature)
    return false;
  const auto snapshot = world_.SaveScene(scene_);
  if (!snapshot)
    return false;
  std::string output = "NEXORA_EDITOR_SCENE 2\n";
  // The parent column duplicates the runtime hierarchy (snapshot version 3) for older readers.
  for (const auto &node : nodes_)
    output += "node " + std::to_string(node.id) + " " +
              std::to_string(world_.Parent(node.id).value_or(0)) + " " + node.name + "\n";
  std::ostringstream hints;
  hints.imbue(std::locale::classic());
  hints << std::setprecision(std::numeric_limits<double>::max_digits10);
  for (const auto &node : nodes_) {
    const auto *entity = world_.FindEntity(node.id);
    if (node.euler_hint && entity && SameRotation(node.euler_hint->transform, entity->transform))
      hints << "euler " << node.id << ' ' << node.euler_hint->degrees[0] << ' '
            << node.euler_hint->degrees[1] << ' ' << node.euler_hint->degrees[2] << '\n';
  }
  output += hints.str();
  output += "world\n" + *snapshot;
  if (!AtomicWrite(path, output, nullptr))
    return false;
  saved_signature_ = *signature;
  return true;
}
bool SceneDocument::Reload(const std::filesystem::path &path) {
  std::ifstream input(path, std::ios::binary);
  std::string line, world_data;
  struct LoadedNode final {
    runtime::Id id{}, parent{};
    std::string name;
  };
  std::vector<LoadedNode> loaded;
  std::unordered_map<runtime::Id, EulerDegrees> hints;
  if (!input || !std::getline(input, line) ||
      (line != "NEXORA_EDITOR_SCENE 1" && line != "NEXORA_EDITOR_SCENE 2"))
    return false;
  const bool supports_hints = line == "NEXORA_EDITOR_SCENE 2";
  while (std::getline(input, line) && line != "world") {
    if (supports_hints && line.starts_with("euler ")) {
      std::istringstream parser(line.substr(6));
      parser.imbue(std::locale::classic());
      runtime::Id id{};
      EulerDegrees degrees{};
      if (!(parser >> id >> degrees[0] >> degrees[1] >> degrees[2]) || !id ||
          !WithEulerDegrees({}, degrees) || !hints.emplace(id, degrees).second)
        return false;
      parser >> std::ws;
      if (!parser.eof() || hints.size() > loaded.size())
        return false;
      continue;
    }
    if (!line.starts_with("node "))
      return false;
    std::istringstream parser(line.substr(5));
    LoadedNode node;
    // Save writes exactly one space before the name, so consume only that one: skipping all
    // whitespace would drop leading spaces and turn a whitespace-only name into an empty one.
    if (!(parser >> node.id >> node.parent) || parser.get() != ' ' ||
        !std::getline(parser, node.name) || node.name.empty())
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
  std::string snapshot_to_load = world_data;
  if (migrate || !hints.empty()) {
    // Validate hints and migration before touching the live World or replacing its authoring state.
    runtime::World rehearsal{world_.Kind()};
    const auto staged_scene = rehearsal.LoadSceneSnapshot(world_data);
    if (!staged_scene || (migrate && !migration().Apply(rehearsal)))
      return false;
    if (migrate) {
      const auto migrated_snapshot = rehearsal.SaveScene(*staged_scene);
      if (!migrated_snapshot)
        return false;
      snapshot_to_load = *migrated_snapshot;
    }
    for (const auto &[id, degrees] : hints) {
      const auto *entity = rehearsal.FindEntity(id);
      const auto authored = WithEulerDegrees({}, degrees);
      if (!ids.contains(id) || !entity || !authored || !SameRotation(*authored, entity->transform))
        return false;
    }
  }
  std::vector<Node> staged_nodes;
  staged_nodes.reserve(loaded.size());
  auto next_generation = next_entity_generation_;
  for (auto &node : loaded) {
    const auto generation = next_generation++;
    if (next_generation == 0)
      ++next_generation;
    staged_nodes.push_back({node.id, std::move(node.name), generation});
    if (const auto hint = hints.find(node.id); hint != hints.end())
      staged_nodes.back().euler_hint = EulerHint{*WithEulerDegrees({}, hint->second), hint->second};
  }
  if (!world_.ReplaceSceneSnapshot(scene_, snapshot_to_load))
    return false;
  document_generation_ = NextDocumentGeneration();
  next_entity_generation_ = next_generation;
  nodes_ = std::move(staged_nodes);
  selection_.clear();
  clipboard_.clear();
  undo_.clear();
  redo_.clear();
  editor_.ClearUndo();
  saved_signature_ = StateSignature().value_or(std::string{});
  return true;
}
std::optional<SceneDocument::NodeKey> SceneDocument::Key(runtime::Id entity) const noexcept {
  const auto found = std::ranges::find(nodes_, entity, &Node::id);
  if (found == nodes_.end() || world_.FindEntity(entity) == nullptr)
    return std::nullopt;
  return NodeKey{entity, found->generation, document_generation_};
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
  std::unordered_map<runtime::Id, const Node *> indexed;
  indexed.reserve(nodes_.size());
  for (const auto &node : nodes_)
    indexed.emplace(node.id, &node);
  if (const auto *scene = world_.FindScene(scene_))
    for (const auto &entity : scene->entities)
      if (const auto found = indexed.find(entity.id); found != indexed.end())
        result.push_back({entity.id, entity.parent, found->second->name, found->second->generation,
                          document_generation_});
  return result;
}
} // namespace nexora::editor
