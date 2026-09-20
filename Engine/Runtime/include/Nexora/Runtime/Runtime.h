#pragma once

#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace nexora::runtime {

using Id = std::uint64_t;

enum class SceneState { LoadedInactive, Active, Unloading, Unloaded };
struct Transform final {
  double x{}, y{}, z{};
};
struct Entity final {
  Id id{};
  Transform transform{};
  bool camera{};
  bool light{};
  bool mesh_renderer{};
};
struct Scene final {
  Id id{};
  std::string name;
  SceneState state{SceneState::LoadedInactive};
  bool persistent{};
  std::vector<Entity> entities;
};

class NEXORA_RUNTIME_API World final {
public:
  Id LoadScene(std::string name, bool persistent = false);
  bool Activate(Id scene);
  bool RequestUnload(Id scene);
  void EndFrame();
  Entity &CreateEntity(Id scene);
  [[nodiscard]] const Scene *FindScene(Id scene) const;
  [[nodiscard]] std::size_t ActiveSceneCount() const;

private:
  Id next_id_{1};
  std::vector<Scene> scenes_;
};

struct AssetRecord final {
  Id id{};
  std::string hash;
  std::vector<Id> dependencies;
  std::uint64_t generation{};
};
class NEXORA_RUNTIME_API AssetRegistry final {
public:
  bool Stage(std::vector<AssetRecord> assets);
  bool ActivateStaged();
  bool Rollback();
  void PinGeneration(std::uint64_t generation);
  void UnpinGeneration(std::uint64_t generation);
  [[nodiscard]] std::uint64_t ActiveGeneration() const noexcept { return active_; }
  [[nodiscard]] bool IsPinned(std::uint64_t generation) const;

private:
  bool IsAcyclic(const std::vector<AssetRecord> &assets) const;
  std::uint64_t active_{}, previous_{}, staged_{};
  std::unordered_set<std::uint64_t> pins_;
};

struct PluginDescriptor final {
  std::string name;
  std::uint32_t abi{};
  std::vector<std::string> services;
};
class NEXORA_RUNTIME_API ExtensionRegistry final {
public:
  explicit ExtensionRegistry(std::uint32_t host_abi) : host_abi_(host_abi) {}
  bool Load(PluginDescriptor descriptor);
  [[nodiscard]] bool HasService(std::string_view service) const;

private:
  std::uint32_t host_abi_;
  std::vector<PluginDescriptor> plugins_;
};

class NEXORA_RUNTIME_API UndoStack final {
public:
  void Execute(std::function<void()> apply, std::function<void()> undo);
  bool Undo();

private:
  std::vector<std::function<void()>> undo_;
};

enum class InputKind { Keyboard, Mouse, Gamepad, Touch, Virtual };
struct InputEvent final {
  InputKind kind{};
  std::uint64_t pointer_id{};
  bool consumed{};
};
class NEXORA_RUNTIME_API InputRouter final {
public:
  bool Route(InputEvent event);
  void EndFrame();
  [[nodiscard]] std::size_t DeliveredCount() const noexcept { return delivered_; }

private:
  std::unordered_set<std::uint64_t> touch_ids_;
  std::size_t delivered_{};
};
struct NEXORA_RUNTIME_API VirtualList final {
  std::size_t item_count{}, first_visible{}, visible_count{};
  [[nodiscard]] std::size_t ElementCount() const noexcept;
};

struct CharacterIntent final {
  double requested_x{}, requested_z{};
};
struct CharacterMotion final {
  double actual_x{}, actual_z{};
  bool grounded{};
};
class NEXORA_RUNTIME_API CharacterMotor final {
public:
  CharacterMotion Simulate(CharacterIntent intent, double max_speed, bool ground_contact) const;
};
struct NavigationOutput final {
  double desired_x{}, desired_z{};
};

class NEXORA_RUNTIME_API ResidencySet final {
public:
  void Acquire(Id resource);
  void Release(Id resource);
  [[nodiscard]] bool Resident(Id resource) const;

private:
  std::unordered_map<Id, std::size_t> references_;
};

struct StreamingCell final {
  Id cell_id{}, bundle_id{};
  std::size_t ram{}, vram{};
  bool full{}, hlod{}, occupied{};
};
class NEXORA_RUNTIME_API StreamingWorld final {
public:
  bool Add(StreamingCell cell);
  bool UnloadFull(Id cell);
  [[nodiscard]] std::pair<std::size_t, std::size_t> Usage() const;
  [[nodiscard]] const StreamingCell *Find(Id cell) const;

private:
  std::vector<StreamingCell> cells_;
};

enum class AppState { Foreground, Background, Suspended };
struct PlatformPolicy final {
  bool reduce_quality{}, release_caches{};
};
class NEXORA_RUNTIME_API PlatformRuntime final {
public:
  void SetState(AppState state) noexcept { state_ = state; }
  PlatformPolicy Pressure(bool thermal, bool memory) const noexcept;
  [[nodiscard]] AppState State() const noexcept { return state_; }
  [[nodiscard]] bool RouteWebViewPointer(bool inside_native_view) const noexcept;

private:
  AppState state_{AppState::Foreground};
};

enum class ShippingProfile { Minimal, Full, Dedicated };
struct PackageInput final {
  std::vector<std::string> plugins, shader_families, assets;
};
struct PackageManifest final {
  std::vector<std::string> files;
  bool presentation{};
};
class NEXORA_RUNTIME_API Packager final {
public:
  PackageManifest Build(ShippingProfile profile, const PackageInput &input,
                        std::span<const std::string> enabled_plugins,
                        std::span<const std::string> enabled_shaders) const;
};

} // namespace nexora::runtime
