#pragma once

#include "Nexora/Editor/Api.h"
#include "Nexora/Runtime/EditorSdk.h"

#include <cstdint>
#include <filesystem>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <variant>
#include <vector>

namespace nexora::editor {

using InspectorValue = std::variant<bool, std::int64_t, double, std::string>;

struct InspectorProperty final {
  runtime::TypeId component_type{};
  std::string component_name;
  std::string property_name;
  runtime::TypeId value_type{};
  std::optional<InspectorValue> value;
  bool mixed{};
  bool read_only{};
};

// UI-neutral adapter used by graphical inspectors. A missing value means that the component or
// property is not present on an entity; differing present values are represented explicitly as
// mixed rather than selecting an arbitrary entity's value.
class NEXORA_EDITOR_API InspectorPropertyAdapter final {
public:
  using Read =
      std::function<std::optional<InspectorValue>(runtime::Id, runtime::TypeId, std::string_view)>;
  using Write =
      std::function<bool(runtime::Id, runtime::TypeId, std::string_view, const InspectorValue &)>;

  explicit InspectorPropertyAdapter(const runtime::ReflectionRegistry &reflection)
      : reflection_(reflection) {}
  [[nodiscard]] std::vector<InspectorProperty> Inspect(std::span<const runtime::Id> entities,
                                                       std::span<const runtime::TypeId> components,
                                                       const Read &read) const;
  bool Apply(std::span<const runtime::Id> entities, const InspectorProperty &property,
             const InspectorValue &value, const Write &write) const;

private:
  const runtime::ReflectionRegistry &reflection_;
};

struct OpaqueComponent final {
  runtime::TypeId type{};
  std::string type_name;
  std::vector<std::uint8_t> data;
};

class NEXORA_EDITOR_API UnknownComponentStore final {
public:
  bool Set(runtime::Id entity, OpaqueComponent component);
  [[nodiscard]] std::span<const OpaqueComponent> Find(runtime::Id entity) const;
  [[nodiscard]] std::string Serialize() const;
  bool Deserialize(std::string_view serialized);

private:
  std::unordered_map<runtime::Id, std::vector<OpaqueComponent>> components_;
};

enum class GizmoState { Idle, Dragging };
class NEXORA_EDITOR_API GizmoTransaction final {
public:
  using Read = std::function<bool(runtime::Id, runtime::Transform &)>;
  using Apply = std::function<bool(runtime::Id, runtime::Transform)>;
  bool Begin(std::span<const runtime::Id> entities, const Read &read);
  bool Update(std::span<const runtime::Transform> transforms, const Apply &apply);
  bool Commit();
  bool Cancel(const Apply &apply);
  [[nodiscard]] GizmoState State() const noexcept { return state_; }

private:
  GizmoState state_{GizmoState::Idle};
  std::vector<runtime::Id> entities_;
  std::vector<runtime::Transform> initial_;
};

struct PickRequest final {
  std::uint64_t request{};
  std::uint64_t scene_generation{};
  std::uint64_t viewport_generation{};
};
class NEXORA_EDITOR_API AsyncPickingValidator final {
public:
  void Reset(std::uint64_t scene_generation, std::uint64_t viewport_generation) noexcept;
  [[nodiscard]] PickRequest Request() noexcept;
  [[nodiscard]] bool Accept(PickRequest request) const noexcept;

private:
  std::uint64_t scene_generation_{}, viewport_generation_{}, next_request_{1}, latest_request_{};
};

struct SceneCameraState final {
  runtime::Transform transform{};
  double pitch{}, yaw{}, movement_speed{5.0};
  bool orthographic{};
  double orthographic_size{10.0};
  friend bool operator==(const SceneCameraState &, const SceneCameraState &) = default;
};
class NEXORA_EDITOR_API CameraPersistence final {
public:
  static bool Save(const std::filesystem::path &path, const SceneCameraState &camera,
                   std::string *error = nullptr);
  [[nodiscard]] static std::optional<SceneCameraState> Load(const std::filesystem::path &path,
                                                            std::string *error = nullptr);
};

class NEXORA_EDITOR_API UndoRedoHistory final {
public:
  struct Operation final {
    std::function<bool()> undo;
    std::function<bool()> redo;
  };
  bool Push(Operation operation);
  bool Undo();
  bool Redo();
  [[nodiscard]] std::size_t UndoDepth() const noexcept { return cursor_; }
  [[nodiscard]] std::size_t RedoDepth() const noexcept { return operations_.size() - cursor_; }

private:
  std::vector<Operation> operations_;
  std::size_t cursor_{};
};

} // namespace nexora::editor
