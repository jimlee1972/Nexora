#pragma once

// API-M5 World/Game facade (Roadmap/*/Engine_API_*Roadmap.md section 3.5):
// spawn/destroy, get/set, batch query, deferred command, scene load, asset
// reference, and input snapshot, all through opaque nexora::runtime::Id
// values and by-value snapshots. nexora::runtime::World::FindEntity and
// FindScene return a pointer into World's own std::vector<Entity>/
// std::vector<Scene> storage, which CreateEntity/LoadScene can reallocate at
// any time; this facade is the boundary that never lets such a pointer
// escape to a caller (a Zig game module, a plugin, or any consumer meant to
// only see the supported public API), matching the roadmap's explicit rule
// that Zig must never receive a movable C++ ECS storage pointer.
//
// runtime::Id itself is not wrapped in a generational handle: it is already
// a monotonically increasing, never-reused identifier (see World::next_id_),
// which is the "EntityID" category the V1 Complete Plan's ABI rules list as
// an accepted C-ABI-crossing type in its own right, separate from an
// index+generation Opaque Handle. Adding a second handle type here on top of
// an identifier that already can't collide with a stale reference would be
// redundant, not safer.
//
// Camera, light, and mesh-renderer are entity-integrated here (set at spawn
// time or attached, updated, and removed through component setters, then
// read back via EntitySnapshot). Physics and audio are
// NOT entity-integrated by this facade: nexora::runtime::PhysicsWorld,
// CharacterController, and AudioMixer remain standalone systems with their
// own SimulationId/resource-id space, unconnected to nexora::runtime::Id.
// Building a real entity<->physics-body / entity<->audio-voice binding is a
// larger design than this pass covers; treat that as still open rather than
// implied "done" by this file's existence.
#include "Nexora/Runtime/Api.h"
#include "Nexora/Runtime/AssetPipeline.h"
#include "Nexora/Runtime/InputUi.h"
#include "Nexora/Runtime/Runtime.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nexora::game {

// Already a stable POD identifier (see AssetPipeline.h); re-exported under
// the facade's own name for API-M5's "asset reference" deliverable rather
// than being reimplemented.
using AssetRef = runtime::AssetUuid;

// Everything GetEntity can tell a caller about one entity, copied out by
// value. Never holds a pointer or reference into World's storage.
struct EntitySnapshot final {
  runtime::Id id{};
  runtime::Transform transform{};
  bool has_camera{};
  bool has_light{};
  bool has_mesh_renderer{};
  runtime::CameraComponent camera{};
  runtime::LightComponent light{};
  runtime::MeshComponent mesh{};
};

// Components can be attached at spawn time and subsequently updated or
// removed through GameWorld or DeferredCommands.
struct EntitySpawnDescriptor final {
  runtime::Transform transform{};
  std::optional<runtime::CameraComponent> camera;
  std::optional<runtime::LightComponent> light;
  std::optional<runtime::MeshComponent> mesh_renderer;
};

// Read-only, by-value capture of one input user's events for this frame,
// decoupled from InputSystem's own internal per-user event buffers.
struct InputSnapshot final {
  std::vector<runtime::RawInputEvent> events;
};

// A caller-owned mutation batch. Submit is atomic with respect to invalid
// entity handles: if any referenced entity is stale, none of the commands are
// applied. Successfully submitted commands are consumed.
class NEXORA_RUNTIME_API DeferredCommands final {
public:
  void SetTransform(runtime::Id entity, runtime::Transform transform) {
    commands_.SetTransform(entity, transform);
  }
  void SetCamera(runtime::Id entity, std::optional<runtime::CameraComponent> camera) {
    commands_.SetCamera(entity, camera);
  }
  void SetLight(runtime::Id entity, std::optional<runtime::LightComponent> light) {
    commands_.SetLight(entity, light);
  }
  void SetMeshRenderer(runtime::Id entity, std::optional<runtime::MeshComponent> mesh) {
    commands_.SetMeshRenderer(entity, mesh);
  }
  void DestroyEntity(runtime::Id entity) { commands_.DestroyEntity(entity); }
  [[nodiscard]] std::size_t Size() const noexcept { return commands_.Size(); }

private:
  friend class GameWorld;
  runtime::WorldCommandBuffer commands_;
};
[[nodiscard]] NEXORA_RUNTIME_API InputSnapshot CaptureInput(runtime::InputSystem &input,
                                                            runtime::InputUserId user);

// Owns one nexora::runtime::World and exposes it only through handle-safe,
// by-value operations. SpawnEntity/DestroyEntity/SetTransform can throw
// std::invalid_argument exactly when the underlying World operation they
// wrap would (an unloaded/unloading/missing scene); that is consistent with
// World's own contract and is fine at this C++ facade layer -- exceptions
// are not yet meant to cross a C ABI boundary, and nothing here is that
// boundary. A future API-M6 C ABI wrapper around this facade is responsible
// for converting exceptions to error codes at the point it actually crosses
// into extern "C".
class NEXORA_RUNTIME_API GameWorld final {
public:
  explicit GameWorld(runtime::WorldKind kind = runtime::WorldKind::Editor) : world_(kind) {}

  [[nodiscard]] runtime::Id LoadScene(std::string name, bool persistent = false) {
    return world_.LoadScene(std::move(name), persistent);
  }
  bool ActivateScene(runtime::Id scene) { return world_.Activate(scene); }
  bool RequestUnloadScene(runtime::Id scene) { return world_.RequestUnload(scene); }
  void EndFrame() { world_.EndFrame(); }

  [[nodiscard]] runtime::Id SpawnEntity(runtime::Id scene, EntitySpawnDescriptor descriptor = {});
  bool DestroyEntity(runtime::Id entity);
  [[nodiscard]] bool IsAlive(runtime::Id entity) const {
    return world_.FindEntity(entity) != nullptr;
  }
  [[nodiscard]] std::optional<EntitySnapshot> GetEntity(runtime::Id entity) const;
  bool SetTransform(runtime::Id entity, runtime::Transform transform);
  bool SetCamera(runtime::Id entity, std::optional<runtime::CameraComponent> camera);
  bool SetLight(runtime::Id entity, std::optional<runtime::LightComponent> light);
  bool SetMeshRenderer(runtime::Id entity, std::optional<runtime::MeshComponent> mesh);
  bool Submit(DeferredCommands &commands) { return commands.commands_.Apply(world_); }

  // OR semantics: an entity matches if it has ANY component flag set in
  // `mask` (not all of them). mask == 0 matches every entity in the scene.
  using QueryMask = std::uint32_t;
  static constexpr QueryMask kQueryCamera = 1U << 0U;
  static constexpr QueryMask kQueryLight = 1U << 1U;
  static constexpr QueryMask kQueryMeshRenderer = 1U << 2U;
  [[nodiscard]] std::vector<runtime::Id> Query(runtime::Id scene, QueryMask mask = 0) const;

  // Escape hatch for code that legitimately needs the full World/
  // WorldCommandBuffer/SystemScheduler surface (e.g. the renderer, or a
  // system registered on SystemScheduler) -- not for a Zig module or
  // plugin, which should never see this reference.
  [[nodiscard]] runtime::World &InternalWorld() noexcept { return world_; }
  [[nodiscard]] const runtime::World &InternalWorld() const noexcept { return world_; }

private:
  runtime::World world_;
};

} // namespace nexora::game
