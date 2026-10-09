#include "Nexora/Editor/SceneAuthoring.h"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <locale>
#include <set>
#include <sstream>
#include <unordered_set>
#include <utility>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace nexora::editor {
namespace {
constexpr std::uint64_t MaxAutosavePayloadBytes = 64 * 1024 * 1024;

std::optional<InspectorEditBatch>
PrepareInspectorBatch(const runtime::ReflectionRegistry &reflection,
                      std::span<const runtime::Id> entities, const InspectorProperty &property,
                      const InspectorValue &value) {
  if (entities.empty() || entities.size() > InspectorPropertyAdapter::kMaximumBatchEntities ||
      property.read_only)
    return std::nullopt;
  const auto *type = reflection.FindById(property.component_type);
  if (!type || type->name != property.component_name)
    return std::nullopt;
  const auto field =
      std::ranges::find(type->fields, property.property_name, &runtime::FieldDescriptor::name);
  if (field == type->fields.end() || field->type != property.value_type ||
      std::ranges::count(type->fields, property.property_name, &runtime::FieldDescriptor::name) !=
          1)
    return std::nullopt;
  if (const auto *number = std::get_if<double>(&value); number && !std::isfinite(*number))
    return std::nullopt;
  std::unordered_set<runtime::Id> unique;
  unique.reserve(entities.size());
  for (const auto entity : entities)
    if (entity == 0 || !unique.insert(entity).second)
      return std::nullopt;
  return InspectorEditBatch{{entities.begin(), entities.end()}, property, value};
}

void Error(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
}

bool ReplaceFile(const std::filesystem::path &temporary, const std::filesystem::path &path,
                 std::error_code &error) {
#if defined(_WIN32)
  if (MoveFileExW(temporary.c_str(), path.c_str(),
                  MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
    error.clear();
    return true;
  }
  error = std::error_code(static_cast<int>(GetLastError()), std::system_category());
  return false;
#else
  std::filesystem::rename(temporary, path, error);
  return !error;
#endif
}
} // namespace

std::vector<InspectorProperty>
InspectorPropertyAdapter::Inspect(std::span<const runtime::Id> entities,
                                  std::span<const runtime::TypeId> components,
                                  const Read &read) const {
  std::vector<InspectorProperty> result;
  if (entities.empty() || !read)
    return result;
  for (const auto component : components) {
    const auto *type = reflection_.FindById(component);
    if (!type)
      continue;
    for (const auto &field : type->fields) {
      InspectorProperty property{component,    type->name, field.name, field.type,
                                 std::nullopt, false,      false};
      property.value = read(entities.front(), component, field.name);
      for (std::size_t index = 1; index < entities.size(); ++index) {
        if (read(entities[index], component, field.name) != property.value) {
          property.mixed = true;
          property.value.reset();
          break;
        }
      }
      result.push_back(std::move(property));
    }
  }
  return result;
}

bool InspectorPropertyAdapter::Apply(std::span<const runtime::Id> entities,
                                     const InspectorProperty &property, const InspectorValue &value,
                                     const Write &write) const {
  if (entities.size() != 1 || !write)
    return false;
  const auto request = PrepareInspectorBatch(reflection_, entities, property, value);
  if (!request)
    return false;
  return write(request->entities.front(), request->property.component_type,
               request->property.property_name, request->value);
}

bool InspectorPropertyAdapter::ApplyBatch(std::span<const runtime::Id> entities,
                                          const InspectorProperty &property,
                                          const InspectorValue &value,
                                          const WriteBatch &write) const {
  if (!write)
    return false;
  const auto request = PrepareInspectorBatch(reflection_, entities, property, value);
  return request && write(*request);
}

UnknownComponentStore::UnknownComponentStore(UnknownComponentStore &&other) noexcept
    : components_(std::move(other.components_)),
      payload_bytes_(std::exchange(other.payload_bytes_, 0)),
      component_count_(std::exchange(other.component_count_, 0)) {
  other.components_.clear();
}
UnknownComponentStore &UnknownComponentStore::operator=(UnknownComponentStore &&other) noexcept {
  if (this != &other) {
    components_ = std::move(other.components_);
    payload_bytes_ = std::exchange(other.payload_bytes_, 0);
    component_count_ = std::exchange(other.component_count_, 0);
    other.components_.clear();
  }
  return *this;
}

bool UnknownComponentStore::Set(runtime::Id entity, OpaqueComponent component) {
  if (!entity || !component.type || component.type_name.empty() ||
      component.type_name.size() > kMaximumNameBytes ||
      component.type_name.find_first_of("\r\n") != std::string::npos ||
      component.type_name.find('\0') != std::string::npos ||
      component.data.size() > kMaximumComponentBytes)
    return false;
  const auto current = Find(entity);
  const auto found = std::ranges::find(current, component.type, &OpaqueComponent::type);
  const bool replacing = found != current.end();
  const auto previous_bytes = replacing ? found->data.size() + found->type_name.size() : 0;
  const auto required = component.data.size() + component.type_name.size();
  if (required > kMaximumPayloadBytes - (payload_bytes_ - previous_bytes) ||
      (!replacing &&
       (component_count_ == kMaximumComponents || current.size() == kMaximumComponentsPerEntity)))
    return false;
  auto &components = components_[entity];
  if (!replacing) {
    components.push_back(std::move(component));
    ++component_count_;
  } else {
    *std::ranges::find(components, component.type, &OpaqueComponent::type) = std::move(component);
  }
  payload_bytes_ = payload_bytes_ - previous_bytes + required;
  return true;
}

std::span<const OpaqueComponent> UnknownComponentStore::Find(runtime::Id entity) const {
  const auto found = components_.find(entity);
  return found == components_.end() ? std::span<const OpaqueComponent>{} : found->second;
}

std::string UnknownComponentStore::Serialize() const {
  std::ostringstream output;
  output.imbue(std::locale::classic());
  output << "NEXORA_OPAQUE_COMPONENTS 1\n";
  std::vector<runtime::Id> entities;
  entities.reserve(components_.size());
  for (const auto &[entity, unused] : components_)
    entities.push_back(entity);
  std::ranges::sort(entities);
  for (const auto entity : entities) {
    std::vector<const OpaqueComponent *> ordered;
    for (const auto &component : components_.at(entity))
      ordered.push_back(&component);
    std::ranges::sort(ordered, {}, [](const auto *component) { return component->type; });
    for (const auto *component : ordered) {
      output << entity << ' ' << component->type << ' ' << std::quoted(component->type_name) << ' ';
      if (component->data.empty())
        output << '-';
      else
        for (const auto byte : component->data)
          output << "0123456789abcdef"[byte >> 4] << "0123456789abcdef"[byte & 15];
      output << '\n';
    }
  }
  return output.str();
}

bool UnknownComponentStore::Deserialize(std::string_view serialized) {
  if (serialized.size() > kMaximumSerializedBytes)
    return false;
  std::istringstream input{std::string(serialized)};
  input.imbue(std::locale::classic());
  std::string line;
  UnknownComponentStore loaded;
  if (!std::getline(input, line) || line != "NEXORA_OPAQUE_COMPONENTS 1")
    return false;
  while (std::getline(input, line)) {
    if (line.empty())
      continue;
    std::istringstream parser(line);
    parser.imbue(std::locale::classic());
    runtime::Id entity{};
    OpaqueComponent component;
    std::string hex, trailing;
    if (!(parser >> entity >> component.type >> std::quoted(component.type_name) >> hex) ||
        hex.size() > 2 * kMaximumComponentBytes || (hex != "-" && hex.size() % 2 != 0) ||
        (parser >> trailing))
      return false;
    if (hex == "-")
      hex.clear();
    component.data.reserve(hex.size() / 2);
    for (std::size_t index = 0; index < hex.size(); index += 2) {
      unsigned value{};
      const auto parsed = std::from_chars(hex.data() + index, hex.data() + index + 2, value, 16);
      if (parsed.ec != std::errc{} || parsed.ptr != hex.data() + index + 2)
        return false;
      component.data.push_back(static_cast<std::uint8_t>(value));
    }
    const auto existing = loaded.Find(entity);
    if (std::ranges::find(existing, component.type, &OpaqueComponent::type) != existing.end() ||
        !loaded.Set(entity, std::move(component)))
      return false;
  }
  if (input.bad())
    return false;
  *this = std::move(loaded);
  return true;
}

bool GizmoTransaction::Begin(std::span<const runtime::Id> entities, const Read &read) {
  if (state_ != GizmoState::Idle || entities.empty() || !read)
    return false;
  std::vector<runtime::Transform> initial(entities.size());
  for (std::size_t index = 0; index < entities.size(); ++index)
    if (!read(entities[index], initial[index]))
      return false;
  entities_.assign(entities.begin(), entities.end());
  initial_ = std::move(initial);
  state_ = GizmoState::Dragging;
  return true;
}

bool GizmoTransaction::Update(std::span<const runtime::Transform> transforms, const Apply &apply) {
  if (state_ != GizmoState::Dragging || transforms.size() != entities_.size() || !apply)
    return false;
  for (std::size_t index = 0; index < entities_.size(); ++index)
    if (!apply(entities_[index], transforms[index])) {
      for (std::size_t rollback = 0; rollback < index; ++rollback)
        static_cast<void>(apply(entities_[rollback], initial_[rollback]));
      return false;
    }
  return true;
}

bool GizmoTransaction::Commit() {
  if (state_ != GizmoState::Dragging)
    return false;
  state_ = GizmoState::Idle;
  entities_.clear();
  initial_.clear();
  return true;
}

bool GizmoTransaction::Cancel(const Apply &apply) {
  if (state_ != GizmoState::Dragging || !apply)
    return false;
  bool restored = true;
  for (std::size_t index = 0; index < entities_.size(); ++index)
    restored = apply(entities_[index], initial_[index]) && restored;
  state_ = GizmoState::Idle;
  entities_.clear();
  initial_.clear();
  return restored;
}

void AsyncPickingValidator::Reset(std::uint64_t scene_generation,
                                  std::uint64_t viewport_generation) noexcept {
  scene_generation_ = scene_generation;
  viewport_generation_ = viewport_generation;
  latest_request_ = 0;
}
PickRequest AsyncPickingValidator::Request() noexcept {
  latest_request_ = next_request_++;
  return {latest_request_, scene_generation_, viewport_generation_};
}
bool AsyncPickingValidator::Accept(PickRequest request) const noexcept {
  return request.request == latest_request_ && request.scene_generation == scene_generation_ &&
         request.viewport_generation == viewport_generation_;
}

bool CameraPersistence::Save(const std::filesystem::path &path, const SceneCameraState &camera,
                             std::string *error) {
  if (!runtime::NormalizedTransform(camera.transform) || !std::isfinite(camera.pitch) ||
      !std::isfinite(camera.yaw) || !std::isfinite(camera.movement_speed) ||
      !std::isfinite(camera.orthographic_size) || !(camera.movement_speed > 0.0) ||
      !(camera.orthographic_size > 0.0)) {
    Error(error, "invalid camera state");
    return false;
  }
  const auto temporary = path.string() + ".tmp";
  std::ofstream output(temporary, std::ios::trunc);
  output << std::setprecision(17) << "NEXORA_SCENE_CAMERA 1\n"
         << camera.transform.x << ' ' << camera.transform.y << ' ' << camera.transform.z << ' '
         << camera.pitch << ' ' << camera.yaw << ' ' << camera.movement_speed << ' '
         << camera.orthographic << ' ' << camera.orthographic_size << '\n';
  output.close();
  if (!output) {
    Error(error, "failed to write camera state");
    return false;
  }
  std::error_code ec;
  ReplaceFile(temporary, path, ec);
  if (ec) {
    Error(error, "failed to replace camera state: " + ec.message());
    return false;
  }
  return true;
}

std::optional<SceneCameraState> CameraPersistence::Load(const std::filesystem::path &path,
                                                        std::string *error) {
  std::ifstream input(path);
  std::string header, trailing;
  SceneCameraState camera;
  if (!std::getline(input, header) || header != "NEXORA_SCENE_CAMERA 1" ||
      !(input >> camera.transform.x >> camera.transform.y >> camera.transform.z >> camera.pitch >>
        camera.yaw >> camera.movement_speed >> camera.orthographic >> camera.orthographic_size) ||
      !runtime::NormalizedTransform(camera.transform) || !std::isfinite(camera.pitch) ||
      !std::isfinite(camera.yaw) || !std::isfinite(camera.movement_speed) ||
      !std::isfinite(camera.orthographic_size) || camera.movement_speed <= 0.0 ||
      camera.orthographic_size <= 0.0 || (input >> trailing)) {
    Error(error, "invalid camera state");
    return std::nullopt;
  }
  return camera;
}

bool UndoRedoHistory::Push(Operation operation) {
  if (!operation.undo || !operation.redo)
    return false;
  operations_.erase(operations_.begin() + static_cast<std::ptrdiff_t>(cursor_), operations_.end());
  operations_.push_back(std::move(operation));
  cursor_ = operations_.size();
  return true;
}
bool UndoRedoHistory::Undo() {
  if (!cursor_ || !operations_[cursor_ - 1].undo())
    return false;
  --cursor_;
  return true;
}
bool UndoRedoHistory::Redo() {
  if (cursor_ == operations_.size() || !operations_[cursor_].redo())
    return false;
  ++cursor_;
  return true;
}

bool AdditiveSceneGraph::Add(AdditiveScene scene) {
  if (!scene.id || scene.path.empty() || scenes_.contains(scene.id) ||
      std::ranges::any_of(scene.dependencies, [&](const auto dependency) {
        return dependency == scene.id || !scenes_.contains(dependency);
      }))
    return false;
  std::ranges::sort(scene.dependencies);
  scene.dependencies.erase(std::unique(scene.dependencies.begin(), scene.dependencies.end()),
                           scene.dependencies.end());
  // Existing scenes cannot depend on this new ID, so validated edges preserve acyclicity.
  return scenes_.emplace(scene.id, std::move(scene)).second;
}
const AdditiveScene *AdditiveSceneGraph::Find(SceneDocumentId id) const noexcept {
  const auto found = scenes_.find(id);
  return found == scenes_.end() ? nullptr : &found->second;
}
std::vector<SceneDocumentId> AdditiveSceneGraph::LoadOrder(std::string *error) const {
  std::unordered_map<SceneDocumentId, std::size_t> degree;
  std::unordered_map<SceneDocumentId, std::vector<SceneDocumentId>> outgoing;
  for (const auto &[id, scene] : scenes_) {
    degree[id] = scene.dependencies.size();
    for (const auto dependency : scene.dependencies)
      outgoing[dependency].push_back(id);
  }
  std::set<SceneDocumentId> ready;
  for (const auto &[id, count] : degree)
    if (!count)
      ready.insert(id);
  std::vector<SceneDocumentId> order;
  while (!ready.empty()) {
    const auto id = *ready.begin();
    ready.erase(ready.begin());
    order.push_back(id);
    for (const auto dependent : outgoing[id])
      if (!--degree[dependent])
        ready.insert(dependent);
  }
  if (order.size() != scenes_.size()) {
    Error(error, "scene dependency cycle");
    return {};
  }
  return order;
}
bool AdditiveSceneGraph::SetDependencies(SceneDocumentId id,
                                         std::vector<SceneDocumentId> dependencies) {
  const auto found = scenes_.find(id);
  if (found == scenes_.end() || std::ranges::any_of(dependencies, [&](const auto dependency) {
        return dependency == id || !scenes_.contains(dependency);
      }))
    return false;
  std::ranges::sort(dependencies);
  dependencies.erase(std::unique(dependencies.begin(), dependencies.end()), dependencies.end());
  const auto previous = found->second.dependencies;
  found->second.dependencies = std::move(dependencies);
  if (!scenes_.empty() && LoadOrder().empty()) {
    found->second.dependencies = previous;
    return false;
  }
  return true;
}
bool AdditiveSceneGraph::Remove(SceneDocumentId id) {
  if (!scenes_.contains(id) || std::ranges::any_of(scenes_, [&](const auto &entry) {
        return std::ranges::find(entry.second.dependencies, id) != entry.second.dependencies.end();
      }))
    return false;
  scenes_.erase(id);
  return true;
}
bool DocumentMigration::Register(std::uint32_t from, Step step) {
  return step && steps_.emplace(from, std::move(step)).second;
}
std::optional<MigrationReport> DocumentMigration::Run(std::uint32_t from, std::uint32_t to,
                                                      std::string_view path, std::string &document,
                                                      bool dry_run) const {
  if (from >= to)
    return std::nullopt;
  MigrationReport report{from, to, dry_run, {}};
  auto current = document;
  for (auto version = from; version < to; ++version) {
    const auto step = steps_.find(version);
    if (step == steps_.end())
      return std::nullopt;
    auto next = step->second(current);
    if (!next)
      return std::nullopt;
    report.changes.push_back({std::string(path), current, *next});
    current = std::move(*next);
  }
  if (!dry_run)
    document = std::move(current);
  return report;
}
bool AutosaveJournal::Write(const std::filesystem::path &path, std::uint64_t revision,
                            std::string_view payload, std::string *error) {
  if (payload.size() > MaxAutosavePayloadBytes) {
    Error(error, "autosave payload exceeds the 64 MiB recovery limit");
    return false;
  }
  auto temporary = path;
  temporary += ".tmp";
  std::error_code ec;
  const auto temporary_status = std::filesystem::symlink_status(temporary, ec);
  if (ec != std::errc::no_such_file_or_directory &&
      (ec || std::filesystem::exists(temporary_status))) {
    Error(error, "autosave temporary destination is already occupied");
    return false;
  }
  ec.clear();
  std::ofstream output(temporary, std::ios::binary | std::ios::trunc);
  output.imbue(std::locale::classic());
  output << "NEXORA_AUTOSAVE 1 " << revision << ' ' << payload.size() << '\n' << payload;
  output.close();
  if (!output) {
    std::filesystem::remove(temporary, ec);
    Error(error, "failed to write autosave journal");
    return false;
  }
  ReplaceFile(temporary, path, ec);
  if (ec) {
    std::error_code cleanup;
    std::filesystem::remove(temporary, cleanup);
    Error(error, ec.message());
  }
  return !ec;
}
std::optional<std::string> AutosaveJournal::Recover(const std::filesystem::path &path,
                                                    std::uint64_t *revision, std::string *error) {
  std::error_code ec;
  const auto status = std::filesystem::symlink_status(path, ec);
  if (ec || !std::filesystem::is_regular_file(status)) {
    Error(error, "autosave journal is unavailable or not a regular file");
    return std::nullopt;
  }
  constexpr std::size_t MaxHeaderBytes = 127;
  const auto file_size = std::filesystem::file_size(path, ec);
  if (ec || file_size > MaxAutosavePayloadBytes + MaxHeaderBytes + 1) {
    Error(error, "autosave journal is unavailable or too large");
    return std::nullopt;
  }
  std::ifstream input(path, std::ios::binary);
  std::array<char, MaxHeaderBytes + 1> buffer{};
  input.getline(buffer.data(), static_cast<std::streamsize>(buffer.size()));
  if (!input || input.eof()) {
    Error(error, "corrupt autosave header");
    return std::nullopt;
  }
  const auto header_bytes = static_cast<std::uint64_t>(input.gcount());
  std::string_view header(buffer.data(), static_cast<std::size_t>(header_bytes - 1));
  constexpr std::string_view prefix = "NEXORA_AUTOSAVE 1 ";
  const auto parse_unsigned = [](std::string_view token, std::uint64_t &value) {
    const auto result = std::from_chars(token.data(), token.data() + token.size(), value);
    return !token.empty() && result.ec == std::errc{} && result.ptr == token.data() + token.size();
  };
  if (!header.starts_with(prefix)) {
    Error(error, "corrupt autosave header");
    return std::nullopt;
  }
  header.remove_prefix(prefix.size());
  const auto separator = header.find(' ');
  std::uint64_t found_revision{}, size{};
  if (separator == std::string_view::npos ||
      !parse_unsigned(header.substr(0, separator), found_revision) ||
      !parse_unsigned(header.substr(separator + 1), size) || size > MaxAutosavePayloadBytes) {
    Error(error, "corrupt autosave header");
    return std::nullopt;
  }
  if (header_bytes > file_size || size != file_size - header_bytes) {
    Error(error, "corrupt autosave payload");
    return std::nullopt;
  }
  std::string payload(size, '\0');
  input.read(payload.data(), static_cast<std::streamsize>(size));
  if (input.bad() || static_cast<std::uint64_t>(input.gcount()) != size ||
      input.peek() != std::char_traits<char>::eof()) {
    Error(error, "corrupt autosave payload");
    return std::nullopt;
  }
  if (revision)
    *revision = found_revision;
  if (error)
    error->clear();
  return payload;
}
std::vector<MergeRecord> ThreeWayMerge(std::span<const MergeRecord> records) {
  std::vector<MergeRecord> result(records.begin(), records.end());
  for (auto &record : result) {
    if (record.local == record.remote || record.remote == record.base) {
      record.choice = MergeChoice::Local;
      record.resolution = record.local;
    } else if (record.local == record.base) {
      record.choice = MergeChoice::Remote;
      record.resolution = record.remote;
    } else {
      record.choice = MergeChoice::Manual;
      record.resolution.clear();
    }
  }
  std::ranges::sort(result, {}, &MergeRecord::stable_path);
  return result;
}
} // namespace nexora::editor
