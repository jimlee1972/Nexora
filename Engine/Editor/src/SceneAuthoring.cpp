#include "Nexora/Editor/SceneAuthoring.h"

#include <algorithm>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace nexora::editor {
namespace {
void Error(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
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
  const auto *type = reflection_.FindById(property.component_type);
  if (!type || property.read_only || !write)
    return false;
  const auto field =
      std::ranges::find(type->fields, property.property_name, &runtime::FieldDescriptor::name);
  if (field == type->fields.end())
    return false;
  return std::ranges::all_of(entities, [&](const auto entity) {
    return write(entity, property.component_type, property.property_name, value);
  });
}

bool UnknownComponentStore::Set(runtime::Id entity, OpaqueComponent component) {
  if (!entity || !component.type || component.type_name.empty())
    return false;
  auto &components = components_[entity];
  const auto found = std::ranges::find(components, component.type, &OpaqueComponent::type);
  if (found == components.end())
    components.push_back(std::move(component));
  else
    *found = std::move(component);
  return true;
}

std::span<const OpaqueComponent> UnknownComponentStore::Find(runtime::Id entity) const {
  const auto found = components_.find(entity);
  return found == components_.end() ? std::span<const OpaqueComponent>{} : found->second;
}

std::string UnknownComponentStore::Serialize() const {
  std::ostringstream output;
  output << "NEXORA_OPAQUE_COMPONENTS 1\n";
  std::vector<runtime::Id> entities;
  entities.reserve(components_.size());
  for (const auto &[entity, unused] : components_)
    entities.push_back(entity);
  std::ranges::sort(entities);
  for (const auto entity : entities)
    for (const auto &component : components_.at(entity)) {
      output << entity << ' ' << component.type << ' ' << std::quoted(component.type_name) << ' ';
      if (component.data.empty())
        output << '-';
      else
        for (const auto byte : component.data)
          output << "0123456789abcdef"[byte >> 4] << "0123456789abcdef"[byte & 15];
      output << '\n';
    }
  return output.str();
}

bool UnknownComponentStore::Deserialize(std::string_view serialized) {
  std::istringstream input{std::string(serialized)};
  std::string line;
  std::unordered_map<runtime::Id, std::vector<OpaqueComponent>> loaded;
  if (!std::getline(input, line) || line != "NEXORA_OPAQUE_COMPONENTS 1")
    return false;
  while (std::getline(input, line)) {
    if (line.empty())
      continue;
    std::istringstream parser(line);
    runtime::Id entity{};
    OpaqueComponent component;
    std::string hex, trailing;
    if (!(parser >> entity >> component.type >> std::quoted(component.type_name) >> hex) ||
        !entity || !component.type || component.type_name.empty() ||
        (hex != "-" && hex.size() % 2 != 0) || (parser >> trailing))
      return false;
    if (hex == "-")
      hex.clear();
    for (std::size_t index = 0; index < hex.size(); index += 2) {
      unsigned value{};
      const auto parsed = std::from_chars(hex.data() + index, hex.data() + index + 2, value, 16);
      if (parsed.ec != std::errc{} || parsed.ptr != hex.data() + index + 2)
        return false;
      component.data.push_back(static_cast<std::uint8_t>(value));
    }
    auto &components = loaded[entity];
    if (std::ranges::find(components, component.type, &OpaqueComponent::type) != components.end())
      return false;
    components.push_back(std::move(component));
  }
  components_ = std::move(loaded);
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
    if (!apply(entities_[index], transforms[index]))
      return false;
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
  std::filesystem::rename(temporary, path, ec);
  if (ec) {
    std::filesystem::remove(path, ec);
    ec.clear();
    std::filesystem::rename(temporary, path, ec);
  }
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
      camera.movement_speed <= 0.0 || camera.orthographic_size <= 0.0 || (input >> trailing)) {
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
} // namespace nexora::editor
