#pragma once

#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Foundation/Types.h"
#include "Nexora/Runtime/Runtime.h"
#include <cmath>

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
    if (descriptor.components & NEXORA_SPAWN_PHYSICS)
      return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
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
      return commands.Apply(*world_) ? NEXORA_GAMEPLAY_OK : NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    } catch (...) {
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    }
  }

private:
  runtime::World *world_{};
  std::size_t scenes_{}, spawns_{};
};
} // namespace nexora::editor::preview
