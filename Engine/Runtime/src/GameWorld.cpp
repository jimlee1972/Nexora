#include "Nexora/Game/GameWorld.h"

namespace nexora::game {

InputSnapshot CaptureInput(runtime::InputSystem &input, runtime::InputUserId user) {
  return {input.Consume(user)};
}

runtime::Id GameWorld::SpawnEntity(runtime::Id scene, EntitySpawnDescriptor descriptor) {
  auto &entity = world_.CreateEntity(scene);
  entity.transform = descriptor.transform;
  if (descriptor.camera) {
    entity.camera = true;
    entity.camera_data = *descriptor.camera;
  }
  if (descriptor.light) {
    entity.light = true;
    entity.light_data = *descriptor.light;
  }
  if (descriptor.mesh_renderer) {
    entity.mesh_renderer = true;
    entity.mesh_data = *descriptor.mesh_renderer;
  }
  return entity.id;
}

bool GameWorld::DestroyEntity(runtime::Id entity) {
  runtime::WorldCommandBuffer commands;
  commands.DestroyEntity(entity);
  return commands.Apply(world_);
}

std::optional<EntitySnapshot> GameWorld::GetEntity(runtime::Id entity) const {
  const auto *found = world_.FindEntity(entity);
  if (found == nullptr)
    return std::nullopt;
  EntitySnapshot snapshot;
  snapshot.id = found->id;
  snapshot.transform = found->transform;
  snapshot.has_camera = found->camera;
  snapshot.has_light = found->light;
  snapshot.has_mesh_renderer = found->mesh_renderer;
  snapshot.camera = found->camera_data;
  snapshot.light = found->light_data;
  snapshot.mesh = found->mesh_data;
  return snapshot;
}

bool GameWorld::SetTransform(runtime::Id entity, runtime::Transform transform) {
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(entity, transform);
  return commands.Apply(world_);
}

std::vector<runtime::Id> GameWorld::Query(runtime::Id scene, QueryMask mask) const {
  std::vector<runtime::Id> result;
  const auto *found_scene = world_.FindScene(scene);
  if (found_scene == nullptr)
    return result;
  for (const auto &entity : found_scene->entities) {
    const bool matches = mask == 0 || ((mask & kQueryCamera) != 0 && entity.camera) ||
                         ((mask & kQueryLight) != 0 && entity.light) ||
                         ((mask & kQueryMeshRenderer) != 0 && entity.mesh_renderer);
    if (matches)
      result.push_back(entity.id);
  }
  return result;
}

} // namespace nexora::game
