#pragma once

#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/GameplaySimulation.h"
#include "Nexora/Runtime/Runtime.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <limits>

namespace nexora::editor::preview {
// Serialized game-thread services. Bind only a Play clone; clear after module destruction and
// before clone teardown. No World storage or caller pointer escapes a callback.
class PlaySceneServices final {
public:
  PlaySceneServices() = default;
  PlaySceneServices(const PlaySceneServices &) = delete;
  PlaySceneServices &operator=(const PlaySceneServices &) = delete;
  static constexpr std::uint32_t kMaximumNameBytes = 256;
  static constexpr std::size_t kMaximumScenes = 32, kMaximumSpawns = 4096;
  static constexpr std::size_t kMaximumColliders = 256;
  bool Bind(runtime::World &world) noexcept {
    Clear();
    if (world.Kind() != runtime::WorldKind::Play)
      return false;
    world_ = &world;
    return true;
  }
  void Clear() noexcept {
    world_ = nullptr;
    scenes_ = spawns_ = 0;
    collider_count_ = 0;
  }
  int32_t Load(const char *name, std::uint32_t length, std::uint32_t persistent,
               std::uint64_t *output) noexcept {
    if (!output || !name || !length || length > kMaximumNameBytes || persistent > 1)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (!world_ || scenes_ == kMaximumScenes)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    const std::string_view text{name, length};
    if (text.find('\0') != std::string_view::npos || !foundation::IsValidUtf8(text))
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    try {
      // Names identify new empty in-memory scenes; they are never project filesystem paths.
      const auto id = world_->LoadScene(std::string(text), persistent != 0);
      ++scenes_;
      *output = id;
      return NEXORA_GAMEPLAY_OK;
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }
  int32_t Activate(std::uint64_t scene) noexcept {
    if (!world_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    return world_->Activate(scene) ? NEXORA_GAMEPLAY_OK : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  }
  int32_t Spawn(std::uint64_t scene, const NexoraEntitySpawnDescriptor *wire,
                std::uint64_t *output) noexcept {
    if (!wire || !output || wire->struct_size < sizeof(*wire))
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    // Copy the admitted prefix once. Optional future suffixes are ignored.
    const auto descriptor = *wire;
    constexpr auto known =
        NEXORA_SPAWN_CAMERA | NEXORA_SPAWN_LIGHT | NEXORA_SPAWN_MESH | NEXORA_SPAWN_PHYSICS;
    if ((descriptor.components & ~known) || descriptor.reserved ||
        !std::isfinite(descriptor.position.x) || !std::isfinite(descriptor.position.y) ||
        !std::isfinite(descriptor.position.z) ||
        ((descriptor.components & NEXORA_SPAWN_CAMERA) &&
         (!std::isfinite(descriptor.camera_fov_degrees) || descriptor.camera_fov_degrees <= 0 ||
          descriptor.camera_fov_degrees >= 180)) ||
        ((descriptor.components & NEXORA_SPAWN_LIGHT) &&
         (!std::isfinite(descriptor.light_intensity) || descriptor.light_intensity < 0)) ||
        ((descriptor.components & NEXORA_SPAWN_MESH) && !descriptor.mesh.value))
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (!world_ || spawns_ == kMaximumSpawns)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    const bool physics = (descriptor.components & NEXORA_SPAWN_PHYSICS) != 0;
#if !NEXORA_GAMEPLAY_SIMULATION_ENABLED
    if (physics)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#else
    if (physics && !ValidBounds(descriptor))
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    PruneColliders();
    if (physics && collider_count_ == kMaximumColliders)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
#endif
    const auto *target = world_->FindScene(scene);
    if (!target || target->state == runtime::SceneState::Unloading ||
        target->state == runtime::SceneState::Unloaded)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    try {
      auto &entity = world_->CreateEntity(scene);
      // All remaining assignments are nonthrowing values, with validation completed before
      // creation.
      entity.transform = {descriptor.position.x, descriptor.position.y, descriptor.position.z};
      entity.camera = (descriptor.components & NEXORA_SPAWN_CAMERA) != 0;
      entity.light = (descriptor.components & NEXORA_SPAWN_LIGHT) != 0;
      entity.mesh_renderer = (descriptor.components & NEXORA_SPAWN_MESH) != 0;
      if (entity.camera)
        entity.camera_data = {descriptor.camera_fov_degrees, 0.1, 1000.0};
      if (entity.light)
        entity.light_data = {descriptor.light_intensity};
      if (entity.mesh_renderer)
        entity.mesh_data = {descriptor.mesh.value, {descriptor.material.value}};
      if (physics)
        colliders_[collider_count_++] = {entity.id, scene, descriptor.bounds_minimum,
                                         descriptor.bounds_maximum};
      ++spawns_;
      *output = entity.id;
      return NEXORA_GAMEPLAY_OK;
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }
  int32_t Despawn(std::uint64_t entity) noexcept {
    if (!world_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    try {
      runtime::WorldCommandBuffer commands;
      commands.DestroyEntity(entity);
      if (!commands.Apply(*world_))
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      const auto destroyed = commands.LastDestroyed();
      for (std::size_t i = 0; i < collider_count_;) {
        if (std::ranges::find(destroyed, colliders_[i].entity) != destroyed.end())
          colliders_[i] = colliders_[--collider_count_];
        else
          ++i;
      }
      return NEXORA_GAMEPLAY_OK;
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }

  int32_t Raycast(const NexoraRaycastRequest *wire, NexoraRaycastHit *output) noexcept {
    if (!wire || !output)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    const auto request = *wire;
    if (!Finite(request.origin) || !Finite(request.direction) || !std::isfinite(request.distance) ||
        request.distance < 0)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    const auto scale = std::max({std::abs(request.direction.x), std::abs(request.direction.y),
                                 std::abs(request.direction.z)});
    if (scale == 0)
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (!world_)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
#if !NEXORA_GAMEPLAY_SIMULATION_ENABLED
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#else
    try {
      PruneColliders();
      runtime::PhysicsWorld physics;
      for (std::size_t i = 0; i < collider_count_; ++i) {
        const auto &collider = colliders_[i];
        if (world_->FindScene(collider.scene)->state != runtime::SceneState::Active)
          continue;
        const auto matrix = world_->WorldMatrix(collider.entity);
        if (!matrix)
          return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
        runtime::PhysicsBody body;
        body.id = collider.entity;
        const auto infinity = std::numeric_limits<double>::infinity();
        body.minimum = {infinity, infinity, infinity};
        body.maximum = {-infinity, -infinity, -infinity};
        for (unsigned corner = 0; corner < 8; ++corner) {
          const auto x = (corner & 1) ? collider.maximum.x : collider.minimum.x;
          const auto y = (corner & 2) ? collider.maximum.y : collider.minimum.y;
          const auto z = (corner & 4) ? collider.maximum.z : collider.minimum.z;
          const NexoraVec3 point{
              (*matrix)[0] * x + (*matrix)[4] * y + (*matrix)[8] * z + (*matrix)[12],
              (*matrix)[1] * x + (*matrix)[5] * y + (*matrix)[9] * z + (*matrix)[13],
              (*matrix)[2] * x + (*matrix)[6] * y + (*matrix)[10] * z + (*matrix)[14]};
          if (!Finite(point))
            return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
          body.minimum = {std::min(body.minimum.x, point.x), std::min(body.minimum.y, point.y),
                          std::min(body.minimum.z, point.z)};
          body.maximum = {std::max(body.maximum.x, point.x), std::max(body.maximum.y, point.y),
                          std::max(body.maximum.z, point.z)};
        }
        if (!physics.AddBody(body))
          return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
      }
      runtime::SimulationVector direction{request.direction.x / scale, request.direction.y / scale,
                                          request.direction.z / scale};
      const auto length = std::hypot(direction.x, direction.y, direction.z);
      direction = {direction.x / length, direction.y / length, direction.z / length};
      const auto hit = physics.Raycast(
          {{request.origin.x, request.origin.y, request.origin.z}, direction, request.distance});
      if (!hit)
        return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
      const NexoraVec3 point{hit->point.x, hit->point.y, hit->point.z};
      if (!std::isfinite(hit->distance) || !Finite(point))
        return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
      *output = {hit->body, hit->distance, point};
      return NEXORA_GAMEPLAY_OK;
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
#endif
  }

private:
  struct Collider final {
    runtime::Id entity{}, scene{};
    NexoraVec3 minimum{}, maximum{};
  };
  static bool Finite(NexoraVec3 value) noexcept {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
  }
  static bool ValidBounds(const NexoraEntitySpawnDescriptor &wire) noexcept {
    const auto &a = wire.bounds_minimum, &b = wire.bounds_maximum, &p = wire.position;
    return Finite(a) && Finite(b) && a.x <= b.x && a.y <= b.y && a.z <= b.z &&
           Finite({p.x + a.x, p.y + a.y, p.z + a.z}) && Finite({p.x + b.x, p.y + b.y, p.z + b.z});
  }
  void PruneColliders() noexcept {
    for (std::size_t i = 0; i < collider_count_;) {
      const auto *scene = world_->FindScene(colliders_[i].scene);
      if (!scene || scene->state == runtime::SceneState::Unloading ||
          scene->state == runtime::SceneState::Unloaded ||
          !world_->FindEntity(colliders_[i].entity))
        colliders_[i] = colliders_[--collider_count_];
      else
        ++i;
    }
  }
  runtime::World *world_{};
  std::size_t scenes_{}, spawns_{};
  std::array<Collider, kMaximumColliders> colliders_{};
  std::size_t collider_count_{};
};
} // namespace nexora::editor::preview
