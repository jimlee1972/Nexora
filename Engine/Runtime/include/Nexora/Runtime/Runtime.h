#pragma once

#include "Nexora/RHI/Device.h"
#include "Nexora/Runtime/Api.h"

#include <array>
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
// Position, rotation, and scale of an entity, in the conventions Unity and Unreal users expect: one
// component, a unit quaternion for rotation, and per-axis (possibly non-uniform) scale. Euler
// angles are an Editor presentation, not stored here. The position members come first so `{x, y,
// z}` initialization keeps working; rotation defaults to identity and scale to one.
//
// Values that reach a World (command buffer, snapshot) are validated and the quaternion normalized;
// see IsValidTransform and NormalizedTransform. Negative scale mirrors an axis; zero is invalid.
struct Transform final {
  double x{}, y{}, z{};
  double qx{}, qy{}, qz{}, qw{1.0};
  double sx{1.0}, sy{1.0}, sz{1.0};
  friend bool operator==(const Transform &, const Transform &) = default;
};

// True when every component is finite, no scale component is zero, and the quaternion has a usable
// (finite, non-zero) length. A quaternion that is valid but not unit length is accepted here and
// normalized by NormalizedTransform.
[[nodiscard]] NEXORA_RUNTIME_API bool IsValidTransform(const Transform &transform) noexcept;
// The transform with its rotation scaled to unit length, or nullopt when it is not valid.
[[nodiscard]] NEXORA_RUNTIME_API std::optional<Transform>
NormalizedTransform(Transform transform) noexcept;
// A copy of `transform` with only the position replaced. Position-only writers (the gameplay
// bridge, character movement) must use this so they do not reset an entity's rotation and scale.
[[nodiscard]] inline Transform WithPosition(Transform transform, double x, double y,
                                            double z) noexcept {
  transform.x = x;
  transform.y = y;
  transform.z = z;
  return transform;
}
// Column-major 4x4 affine matrix (translation * rotation * scale), double precision.
using TransformMatrix = std::array<double, 16>;
[[nodiscard]] NEXORA_RUNTIME_API TransformMatrix ToMatrix(const Transform &transform) noexcept;
// `a * b` (b applied first). World matrices are products in root-to-leaf order.
[[nodiscard]] NEXORA_RUNTIME_API TransformMatrix
MultiplyMatrices(const TransformMatrix &a, const TransformMatrix &b) noexcept;
// `child` expressed in `parent`'s space, composed into one transform. Exact unless the parent has a
// non-uniform scale and the child is rotated: that produces shear, which a transform cannot hold,
// so the result keeps the component-wise product of the scales (Unity's `lossyScale` behaves the
// same way). Use the matrices when the exact result matters.
[[nodiscard]] NEXORA_RUNTIME_API Transform ComposeTransforms(const Transform &parent,
                                                             const Transform &child) noexcept;
// The inverse of ComposeTransforms: the local transform that, under `parent`, yields `world`.
[[nodiscard]] NEXORA_RUNTIME_API Transform RelativeTransform(const Transform &parent,
                                                             const Transform &world) noexcept;
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
  // 0 for a root. A parent is always an entity of the same scene, and the hierarchy is acyclic.
  Id parent{};
  // Relative to `parent` (Unity's localPosition/localRotation/localScale); for a root it is the
  // world transform. See World::WorldTransform.
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
  // Editor-only atomic replacement. Preserves scene ID and lifecycle state; rejects corrupt data,
  // unloading scenes and entity-ID collisions with other scenes. Only target entity borrows expire.
  [[nodiscard]] bool ReplaceSceneSnapshot(Id scene, std::string_view snapshot);
  [[nodiscard]] std::optional<std::string> SaveScene(Id scene) const;
  // Uses the same schema-3 serializer, rejecting before appending beyond max_bytes. Returns no
  // partial snapshot on overflow or stream failure. Bounds logical output bytes, not allocator/
  // formatting temporary memory. Both overloads are synchronous and mutate no World state.
  [[nodiscard]] std::optional<std::string> SaveScene(Id scene, std::size_t max_bytes) const;
  bool Activate(Id scene);
  bool RequestUnload(Id scene);
  void EndFrame();
  Entity &CreateEntity(Id scene);
  [[nodiscard]] const Scene *FindScene(Id scene) const;
  [[nodiscard]] const Entity *FindEntity(Id entity) const;
  // The entity's parent (0 for a root), or nullopt when the entity does not exist.
  [[nodiscard]] std::optional<Id> Parent(Id entity) const;
  // Direct children in scene storage order.
  [[nodiscard]] std::vector<Id> Children(Id entity) const;
  // Position among the children of the entity's parent (or the roots of its scene), in order; this
  // order is the scene storage order, so snapshots keep it. nullopt for a missing entity.
  [[nodiscard]] std::optional<std::size_t> SiblingIndex(Id entity) const;
  // The entity and all its descendants, parents before children.
  [[nodiscard]] std::vector<Id> Subtree(Id entity) const;
  // Exact world origin; composed rotation and component-wise scale are lossy under shear.
  [[nodiscard]] std::optional<Transform> WorldTransform(Id entity) const;
  // Exact world matrix (product of the chain's matrices), including any shear.
  [[nodiscard]] std::optional<TransformMatrix> WorldMatrix(Id entity) const;
  [[nodiscard]] std::size_t ActiveSceneCount() const;
  [[nodiscard]] World CloneForPlay() const;
  [[nodiscard]] WorldKind Kind() const noexcept { return kind_; }

private:
  friend class WorldCommandBuffer;
  friend class SceneEditor;
  friend class PlaySession;
  friend class RenderSceneSync;
  friend NEXORA_RUNTIME_API std::optional<SceneFrameResult>
  RenderSceneFrame(const World &, rhi::Device &, rhi::TextureHandle, const rhi::TextureDescriptor &,
                   rhi::PipelineHandle);
  Id next_id_{1};
  std::vector<Scene> scenes_;
  WorldKind kind_;
};

class NEXORA_RUNTIME_API WorldCommandBuffer final {
public:
  void SetTransform(Id entity, Transform transform);
  // Unity's SetParent(parent, worldPositionStays). `parent` 0 makes the entity a root. With
  // `keep_world` the entity keeps its world pose and its local transform is recomputed; otherwise
  // its local transform is kept and it moves with the new parent. The parent must be in the same
  // scene and must not be the entity or one of its descendants.
  void SetParent(Id entity, Id parent, bool keep_world = true);
  // Unity's SetSiblingIndex: moves the entity to position `index` among the children of its parent
  // (the roots of its scene for a root), clamped to the last position. A reparent makes the entity
  // its new parent's last child, as in Unity.
  void SetSiblingIndex(Id entity, std::size_t index);
  void SetCamera(Id entity, std::optional<CameraComponent> camera);
  void SetLight(Id entity, std::optional<LightComponent> light);
  void SetMeshRenderer(Id entity, std::optional<MeshComponent> mesh);
  // Destroys the entity and all its descendants (Unity's Destroy on a GameObject).
  void DestroyEntity(Id entity);
  // Applies every command or none: the whole batch is validated against a simulated hierarchy
  // first, so a failing command leaves the world untouched.
  [[nodiscard]] bool Apply(World &world);
  [[nodiscard]] std::size_t Size() const noexcept { return commands_.size(); }
  // Every entity removed by the last successful Apply, including cascaded descendants. Owners of
  // per-entity resources (audio, physics, characters) must release them for each id listed here.
  [[nodiscard]] std::span<const Id> LastDestroyed() const noexcept { return last_destroyed_; }

private:
  struct Command final {
    enum class Kind { Transform, Parent, SiblingIndex, Camera, Light, MeshRenderer, Destroy };
    Id entity{};
    Kind kind{};
    Transform transform{};
    Id parent{};
    bool keep_world{true};
    std::size_t sibling_index{};
    std::optional<CameraComponent> camera;
    std::optional<LightComponent> light;
    std::optional<MeshComponent> mesh;
  };
  std::vector<Command> commands_;
  std::vector<Id> last_destroyed_;
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
  // Records a transaction already applied by its owner. A failed callback leaves the cursor in
  // place, so callers can report a rejected Undo or Redo without losing the history entry.
  void Record(std::function<bool()> undo, std::function<bool()> redo);
  bool Undo();
  bool Redo();

private:
  struct Operation final {
    std::function<bool()> undo;
    std::function<bool()> redo;
  };
  std::vector<Operation> operations_;
  std::size_t cursor_{};
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

#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
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
#endif

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

} // namespace nexora::runtime
