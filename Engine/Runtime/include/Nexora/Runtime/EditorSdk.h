#pragma once

#include "Nexora/Runtime/Api.h"
#include "Nexora/Runtime/Runtime.h"

#include <cstdint>
#include <memory>
#include <optional>
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

  Entity &CreateEntity(Id scene);
  bool SetTransform(Id entity, Transform transform);
  bool DestroyEntity(Id scene, Id entity);
  bool Undo();
  [[nodiscard]] std::size_t UndoDepth() const noexcept { return depth_; }

private:
  World &world_;
  UndoStack undo_;
  std::size_t depth_{};
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
  [[nodiscard]] std::size_t OverrideCount() const noexcept { return overrides_.size(); }
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
