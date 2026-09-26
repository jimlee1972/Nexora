#pragma once

#include "Nexora/Runtime/Api.h"
#include "Nexora/Runtime/Runtime.h"

#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace nexora::runtime {

// ---- Reflection metadata ----

using TypeId = std::uint64_t;
[[nodiscard]] NEXORA_RUNTIME_API TypeId HashTypeName(std::string_view name) noexcept;

struct FieldDescriptor final {
  std::string name;
  TypeId type{};
  std::size_t offset{};
  std::size_t size{};
};
struct TypeDescriptor final {
  std::string name;
  TypeId id{};
  std::vector<FieldDescriptor> fields;
};

class NEXORA_RUNTIME_API ReflectionRegistry final {
public:
  bool Register(TypeDescriptor descriptor);
  [[nodiscard]] const TypeDescriptor *Find(std::string_view name) const;
  [[nodiscard]] const TypeDescriptor *FindById(TypeId id) const;
  [[nodiscard]] std::size_t Size() const noexcept { return by_id_.size(); }

private:
  std::unordered_map<TypeId, TypeDescriptor> by_id_;
  std::unordered_map<std::string, TypeId> by_name_;
};

// ---- Service registry ----
// (declared before PluginHost: Load() optionally publishes a plugin's
// services into one)

class NEXORA_RUNTIME_API ServiceRegistry final {
public:
  bool Register(std::string name, void *service);
  bool Unregister(std::string_view name);
  [[nodiscard]] void *Find(std::string_view name) const;
  [[nodiscard]] std::size_t Size() const noexcept { return services_.size(); }

private:
  std::unordered_map<std::string, void *> services_;
};

// ---- Plugin host: stable C ABI, real dynamic loading ----
//
// The required contract is the single exported symbol `NexoraPluginAbiVersion()`
// (see Plugins/Example/ExamplePlugin.cpp and Nexora/Foundation/PluginAbi.h).
// PluginHost resolves it and rejects the plugin before any other use -- before
// resolving or calling anything else, including the optional registration
// entry point below -- if the symbol is missing or the reported ABI does not
// match the host's. No private engine header is required to author a plugin,
// and no engine source changes are required to add one.
//
// A plugin that additionally exports `NexoraPluginRegister` (optional; see
// PluginAbi.h for its C function-pointer signature) gets it called once,
// after the ABI check passes, with a callback it can use to publish services
// into the ServiceRegistry passed to Load(). Passing no ServiceRegistry (the
// default) skips registration entirely, e.g. for a Load() call that only
// wants to verify ABI compatibility.

enum class PluginLoadError { None, OpenFailed, MissingAbiSymbol, AbiMismatch };
struct PluginLoadResult final {
  bool loaded{};
  PluginLoadError error{PluginLoadError::None};
  std::uint32_t reported_abi{};
  bool registered{};
};

class NEXORA_RUNTIME_API PluginHost final {
public:
  explicit PluginHost(std::uint32_t engine_abi) noexcept : engine_abi_(engine_abi) {}
  ~PluginHost();
  PluginHost(const PluginHost &) = delete;
  PluginHost &operator=(const PluginHost &) = delete;

  [[nodiscard]] PluginLoadResult Load(const std::string &library_path,
                                      ServiceRegistry *services = nullptr);
  void UnloadAll() noexcept;
  [[nodiscard]] std::size_t LoadedCount() const noexcept { return handles_.size(); }

private:
  std::uint32_t engine_abi_;
  std::vector<void *> handles_;
};

// ---- Scene editor: Create / Modify / Undo over World ----
//
// Composes World, WorldCommandBuffer, and UndoStack. Undoing a destroyed
// entity restores its component data and stable ID, keeping older undo
// transactions valid.

class NEXORA_RUNTIME_API SceneEditor final {
public:
  explicit SceneEditor(World &world) noexcept : world_(world) {}

  // Returns the new entity's stable Id rather than a reference into World's
  // internal storage: World::CreateEntity's Entity& is only safe to use
  // before any other call that can mutate the owning scene's entity
  // storage (see Engine/Runtime/README.md), a discipline this class's own
  // external callers cannot be expected to know about.
  Id CreateEntity(Id scene);
  bool SetTransform(Id entity, Transform transform);
  bool DestroyEntity(Id scene, Id entity);
  bool Undo();
  [[nodiscard]] std::size_t UndoDepth() const noexcept { return depth_; }

private:
  World &world_;
  UndoStack undo_;
  std::size_t depth_{};
};

// ---- Play-in-Editor session ----

enum class RuntimeLogSeverity { Trace, Info, Warning, Error, Fatal };

struct RuntimeLogRecord final {
  std::uint64_t sequence{};
  RuntimeLogSeverity severity{RuntimeLogSeverity::Info};
  std::string category;
  std::uint64_t timestamp_nanoseconds{};
  std::string source;
  std::string message;
};

// Multi-producer, bounded Console ingress. Snapshot returns owning records in sequence order and
// never exposes storage that can be invalidated by a producer.
class NEXORA_RUNTIME_API RuntimeConsole final {
public:
  explicit RuntimeConsole(std::size_t capacity) noexcept : capacity_(capacity) {}
  bool Push(RuntimeLogRecord record);
  [[nodiscard]] std::vector<RuntimeLogRecord> Snapshot() const;
  [[nodiscard]] std::uint64_t DroppedCount() const;

private:
  const std::size_t capacity_;
  mutable std::mutex mutex_;
  std::vector<RuntimeLogRecord> records_;
  std::uint64_t next_sequence_{1};
  std::uint64_t dropped_{};
};

struct RuntimeEntitySnapshot final {
  Id id{};
  Id scene{};
  Transform transform{};
  bool camera{};
  bool light{};
  bool mesh_renderer{};
};

struct RuntimeInspectionSnapshot final {
  std::uint64_t fixed_tick{};
  std::vector<RuntimeEntitySnapshot> entities;
};

enum class PauseReason { None, User, StepComplete, DebuggerBreak, RuntimeFailure };
enum class DebuggerState { Detached, Running, Paused };

struct DebuggerLocation final {
  std::string source;
  std::uint32_t line{};
  std::uint32_t column{};
};

struct DebuggerSnapshot final {
  DebuggerState state{DebuggerState::Detached};
  std::optional<DebuggerLocation> location;
  std::vector<std::string> diagnostics;
};

// Adapter implementations own native debugger/IDE state. PlaySession only consumes copied state.
class NEXORA_RUNTIME_API DebuggerAdapter {
public:
  virtual ~DebuggerAdapter() = default;
  virtual bool Attach() = 0;
  virtual void Detach() noexcept = 0;
  [[nodiscard]] virtual DebuggerSnapshot Poll() = 0;
};

struct TransformApplyDiff final {
  Id entity{};
  Transform original{};
  Transform editor{};
  Transform runtime{};
  bool editor_exists{};
  bool conflict{};
};

enum class ApplyBackStatus { Discarded, Applied, Conflict, Failed };

enum class PlayState { Stopped, Playing, Paused };
enum class ApplyBackPolicy { Discard, Transforms };

struct PlaySessionStats final {
  std::uint64_t fixed_ticks{};
  std::uint64_t manual_steps{};
  std::uint64_t applied_transforms{};
  std::uint64_t crashes{};
};

// Owns an isolated Play World cloned from the Editor World. Simulation mutations never reach the
// Editor World until Stop(Transforms) explicitly applies stable-ID transform changes. Input focus
// is session policy only; platform events remain owned and routed by the embedding editor.
class NEXORA_RUNTIME_API PlaySession final {
public:
  using FixedUpdate = std::function<bool(World &, double)>;

  explicit PlaySession(World &editor_world) noexcept : editor_world_(editor_world) {}
  bool Start(double fixed_delta_seconds, FixedUpdate fixed_update);
  bool Pause() noexcept;
  bool Resume() noexcept;
  bool Tick();
  bool Step();
  bool Stop(ApplyBackPolicy policy = ApplyBackPolicy::Discard);
  [[nodiscard]] std::vector<TransformApplyDiff> PreviewTransformApplyBack() const;
  [[nodiscard]] RuntimeInspectionSnapshot Inspect() const;
  bool PollDebugger(DebuggerAdapter &debugger);
  void SetInputFocus(bool focused) noexcept { input_focused_ = focused && play_world_.has_value(); }
  [[nodiscard]] bool AcceptsInput() const noexcept {
    return input_focused_ && state_ != PlayState::Stopped;
  }
  [[nodiscard]] PlayState State() const noexcept { return state_; }
  [[nodiscard]] PauseReason LastPauseReason() const noexcept { return pause_reason_; }
  [[nodiscard]] ApplyBackStatus LastApplyBackStatus() const noexcept { return apply_status_; }
  [[nodiscard]] World *PlayWorld() noexcept { return play_world_ ? &*play_world_ : nullptr; }
  [[nodiscard]] const PlaySessionStats &Stats() const noexcept { return stats_; }

private:
  bool ExecuteFixedTick(bool manual);
  World &editor_world_;
  std::optional<World> play_world_;
  FixedUpdate fixed_update_;
  double fixed_delta_seconds_{};
  PlayState state_{PlayState::Stopped};
  bool input_focused_{};
  PlaySessionStats stats_{};
  std::unordered_map<Id, Transform> source_transforms_;
  PauseReason pause_reason_{PauseReason::None};
  ApplyBackStatus apply_status_{ApplyBackStatus::Discarded};
};

// ---- Prefab / nested prefab / variant ----

struct PrefabProperty final {
  std::string key;
  std::string value;
};
struct PrefabNode final {
  std::string name;
  std::vector<PrefabProperty> properties;
  std::vector<PrefabNode> children;
};

class NEXORA_RUNTIME_API Prefab final {
public:
  explicit Prefab(PrefabNode root) : root_(std::move(root)) {}
  [[nodiscard]] const PrefabNode &Root() const noexcept { return root_; }
  [[nodiscard]] const PrefabNode *Find(std::string_view path) const;

private:
  PrefabNode root_;
};

struct PrefabOverride final {
  std::string path;
  std::string key;
  std::string value;
};

class NEXORA_RUNTIME_API PrefabInstance final {
public:
  explicit PrefabInstance(std::shared_ptr<const Prefab> prefab) : prefab_(std::move(prefab)) {}
  bool SetOverride(std::string path, std::string key, std::string value);
  [[nodiscard]] std::optional<std::string> Resolve(std::string_view path,
                                                   std::string_view key) const;
  bool Rebase(std::shared_ptr<const Prefab> new_prefab);
  bool RevertOverride(std::string_view path, std::string_view key);
  void RevertAll() noexcept { overrides_.clear(); }
  // Applies the current override diff to a new immutable prefab revision and clears the diff.
  [[nodiscard]] std::shared_ptr<const Prefab> ApplyOverrides();
  [[nodiscard]] std::size_t OverrideCount() const noexcept { return overrides_.size(); }
  [[nodiscard]] std::span<const PrefabOverride> Overrides() const noexcept { return overrides_; }
  [[nodiscard]] const Prefab &Source() const noexcept { return *prefab_; }

private:
  std::shared_ptr<const Prefab> prefab_;
  std::vector<PrefabOverride> overrides_;
};

class NEXORA_RUNTIME_API PrefabVariant final {
public:
  PrefabVariant(std::shared_ptr<const Prefab> base, std::vector<PrefabOverride> overrides)
      : base_(std::move(base)), overrides_(std::move(overrides)) {}
  [[nodiscard]] std::shared_ptr<Prefab> Bake() const;

private:
  std::shared_ptr<const Prefab> base_;
  std::vector<PrefabOverride> overrides_;
};

// ---- Project settings ----

class NEXORA_RUNTIME_API ProjectSettings final {
public:
  void SetString(std::string key, std::string value);
  [[nodiscard]] std::optional<std::string> GetString(std::string_view key) const;
  [[nodiscard]] std::string GetStringOr(std::string_view key, std::string fallback) const;
  [[nodiscard]] std::size_t Size() const noexcept { return values_.size(); }

private:
  std::unordered_map<std::string, std::string> values_;
};

[[nodiscard]] NEXORA_RUNTIME_API bool EditorSdkEnabled() noexcept;

} // namespace nexora::runtime
