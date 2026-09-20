#pragma once

#include "Nexora/RHI/Device.h"
#include "Nexora/Runtime/Api.h"

#include <cstddef>
#include <cstdint>
#include <deque>
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
  friend bool operator==(const Transform &, const Transform &) = default;
};
struct CameraComponent final {
  double vertical_field_of_view{60.0};
  double near_plane{0.1};
  double far_plane{1000.0};
};
struct LightComponent final {
  float intensity{1.0F};
};
struct MaterialComponent final {
  Id shader{};
};
struct MeshComponent final {
  Id mesh{};
  MaterialComponent material{};
};
struct Entity final {
  Id id{};
  Transform transform{};
  bool camera{};
  bool light{};
  bool mesh_renderer{};
  CameraComponent camera_data{};
  LightComponent light_data{};
  MeshComponent mesh_data{};
};
struct Scene final {
  Id id{};
  std::string name;
  SceneState state{SceneState::LoadedInactive};
  bool persistent{};
  std::vector<Entity> entities;
};

enum class WorldKind { Editor, Play };
struct SceneFrameResult;

class NEXORA_RUNTIME_API World final {
public:
  explicit World(WorldKind kind = WorldKind::Editor) noexcept : kind_(kind) {}
  Id LoadScene(std::string name, bool persistent = false);
  [[nodiscard]] std::optional<Id> LoadSceneSnapshot(std::string_view snapshot);
  [[nodiscard]] std::optional<std::string> SaveScene(Id scene) const;
  bool Activate(Id scene);
  bool RequestUnload(Id scene);
  void EndFrame();
  Entity &CreateEntity(Id scene);
  [[nodiscard]] const Scene *FindScene(Id scene) const;
  [[nodiscard]] const Entity *FindEntity(Id entity) const;
  [[nodiscard]] std::size_t ActiveSceneCount() const;
  [[nodiscard]] World CloneForPlay() const;
  [[nodiscard]] WorldKind Kind() const noexcept { return kind_; }

private:
  friend class WorldCommandBuffer;
  friend NEXORA_RUNTIME_API std::optional<SceneFrameResult>
  RenderSceneFrame(const World &, rhi::Device &, rhi::TextureHandle,
                   const rhi::TextureDescriptor &, rhi::PipelineHandle);
  Id next_id_{1};
  std::vector<Scene> scenes_;
  WorldKind kind_;
};

class NEXORA_RUNTIME_API WorldCommandBuffer final {
public:
  void SetTransform(Id entity, Transform transform);
  void DestroyEntity(Id entity);
  [[nodiscard]] bool Apply(World &world);
  [[nodiscard]] std::size_t Size() const noexcept { return commands_.size(); }

private:
  struct Command final {
    Id entity{};
    std::optional<Transform> transform;
  };
  std::vector<Command> commands_;
};

class NEXORA_RUNTIME_API SystemScheduler final {
public:
  using System = std::function<void(World &, WorldCommandBuffer &)>;
  bool Add(std::string name, std::vector<std::string> after, System system);
  [[nodiscard]] bool Execute(World &world) const;

private:
  struct Entry final {
    std::string name;
    std::vector<std::string> after;
    System system;
  };
  std::vector<Entry> systems_;
};

struct SceneFrameResult final {
  std::size_t visible_meshes{};
  std::size_t passes{};
  std::size_t barriers{};
};
[[nodiscard]] NEXORA_RUNTIME_API bool SceneRenderingEnabled() noexcept;
[[nodiscard]] NEXORA_RUNTIME_API std::optional<SceneFrameResult>
RenderSceneFrame(const World &world, rhi::Device &device, rhi::TextureHandle target,
                 const rhi::TextureDescriptor &target_descriptor, rhi::PipelineHandle pipeline);

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
  void Execute(const std::function<void()> &apply, std::function<void()> undo);
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

struct NavigationNode final {
  Id id{};
  double x{}, z{};
  std::vector<Id> neighbours;
};
class NEXORA_RUNTIME_API NavigationGraph final {
public:
  bool AddNode(NavigationNode node);
  [[nodiscard]] std::vector<Id> FindPath(Id start, Id goal) const;

private:
  std::unordered_map<Id, NavigationNode> nodes_;
};

struct LocalizedEntry final {
  std::string key;
  std::unordered_map<std::string, std::string> translations;
};
class NEXORA_RUNTIME_API LocalizationCatalog final {
public:
  explicit LocalizationCatalog(std::string fallback_locale = "en")
      : fallback_locale_(std::move(fallback_locale)) {}
  bool Add(LocalizedEntry entry);
  [[nodiscard]] std::string_view Resolve(std::string_view key, std::string_view locale) const;

private:
  std::string fallback_locale_;
  std::unordered_map<std::string, LocalizedEntry> entries_;
};

struct AnimationClip final {
  Id id{};
  double duration{};
  bool looping{};
};
class NEXORA_RUNTIME_API AnimationPlayer final {
public:
  bool Play(AnimationClip clip);
  void Advance(double seconds);
  [[nodiscard]] double Time() const noexcept { return time_; }
  [[nodiscard]] bool Playing() const noexcept { return playing_; }

private:
  AnimationClip clip_{};
  double time_{};
  bool playing_{};
};

struct AudioVoice final {
  Id resource{};
  float gain{1.0F};
  bool spatial{};
};
class NEXORA_RUNTIME_API AudioMixer final {
public:
  explicit AudioMixer(std::size_t voice_limit) : voice_limit_(voice_limit) {}
  bool Play(AudioVoice voice);
  bool Stop(Id resource);
  void SetMasterGain(float gain) noexcept;
  [[nodiscard]] std::size_t ActiveVoiceCount() const noexcept { return voices_.size(); }
  [[nodiscard]] float MasterGain() const noexcept { return master_gain_; }

private:
  std::size_t voice_limit_{};
  float master_gain_{1.0F};
  std::vector<AudioVoice> voices_;
};

struct VideoFrame final {
  std::uint64_t sequence{};
  double presentation_time{};
};
class NEXORA_RUNTIME_API MediaQueue final {
public:
  explicit MediaQueue(std::size_t capacity) : capacity_(capacity) {}
  bool Push(VideoFrame frame);
  [[nodiscard]] std::optional<VideoFrame> PopReady(double clock);
  void Seek(double presentation_time);
  [[nodiscard]] std::size_t Size() const noexcept { return frames_.size(); }

private:
  std::size_t capacity_{};
  std::uint64_t last_sequence_{};
  double seek_time_{};
  std::deque<VideoFrame> frames_;
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
