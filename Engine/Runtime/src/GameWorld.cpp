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
  const auto id = entity.id;
  if (descriptor.audio && !SetAudio(id, descriptor.audio)) {
    DestroyEntity(id);
    throw std::invalid_argument("invalid or duplicate audio binding");
  }
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  if (descriptor.physics && !SetPhysics(id, descriptor.physics)) {
    DestroyEntity(id);
    throw std::invalid_argument("invalid physics binding");
  }
  if (descriptor.character && !SetCharacter(id, descriptor.character)) {
    DestroyEntity(id);
    throw std::invalid_argument("invalid character binding");
  }
#endif
  return id;
}

bool GameWorld::DestroyEntity(runtime::Id entity) {
  runtime::WorldCommandBuffer commands;
  commands.DestroyEntity(entity);
  if (!commands.Apply(world_))
    return false;
  RemoveBindings(entity);
  return true;
}

void GameWorld::RemoveBindings(runtime::Id entity) {
  StopAudio(entity);
  entity_audio_.erase(entity);
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  if (physics_entities_.erase(entity) != 0)
    physics_.RemoveBody(entity);
  characters_.erase(entity);
#endif
}

bool GameWorld::Submit(DeferredCommands &commands) {
  if (!commands.commands_.Apply(world_))
    return false;
  for (const auto entity : commands.destroyed_)
    RemoveBindings(entity);
  commands.destroyed_.clear();
  return true;
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
  if (const auto audio = entity_audio_.find(entity); audio != entity_audio_.end()) {
    snapshot.has_audio = true;
    snapshot.audio = audio->second;
  }
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  snapshot.has_physics = physics_entities_.contains(entity);
  if (const auto character = characters_.find(entity); character != characters_.end()) {
    snapshot.has_character = true;
    snapshot.character = character->second.state;
  }
#endif
  return snapshot;
}

bool GameWorld::SetTransform(runtime::Id entity, runtime::Transform transform) {
  runtime::WorldCommandBuffer commands;
  commands.SetTransform(entity, transform);
  return commands.Apply(world_);
}

bool GameWorld::SetCamera(runtime::Id entity, std::optional<runtime::CameraComponent> camera) {
  runtime::WorldCommandBuffer commands;
  commands.SetCamera(entity, camera);
  return commands.Apply(world_);
}

bool GameWorld::SetLight(runtime::Id entity, std::optional<runtime::LightComponent> light) {
  runtime::WorldCommandBuffer commands;
  commands.SetLight(entity, light);
  return commands.Apply(world_);
}

bool GameWorld::SetMeshRenderer(runtime::Id entity, std::optional<runtime::MeshComponent> mesh) {
  runtime::WorldCommandBuffer commands;
  commands.SetMeshRenderer(entity, mesh);
  return commands.Apply(world_);
}

bool GameWorld::SetAudio(runtime::Id entity, std::optional<runtime::AudioVoice> audio) {
  if (!IsAlive(entity))
    return false;
  if (!audio) {
    StopAudio(entity);
    return entity_audio_.erase(entity) != 0;
  }
  if (audio->resource == 0)
    return false;
  for (const auto &[owner, binding] : entity_audio_) {
    if (owner != entity && binding.resource == audio->resource)
      return false;
  }
  if (playing_audio_.contains(entity))
    return false;
  entity_audio_[entity] = *audio;
  return true;
}

bool GameWorld::PlayAudio(runtime::Id entity) {
  const auto binding = entity_audio_.find(entity);
  if (binding == entity_audio_.end() || playing_audio_.contains(entity) ||
      !audio_.Play(binding->second))
    return false;
  playing_audio_.insert(entity);
  return true;
}

bool GameWorld::StopAudio(runtime::Id entity) {
  const auto binding = entity_audio_.find(entity);
  if (binding == entity_audio_.end() || !playing_audio_.contains(entity))
    return false;
  if (!audio_.Stop(binding->second.resource))
    return false;
  playing_audio_.erase(entity);
  return true;
}

#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
bool GameWorld::SetPhysics(runtime::Id entity, std::optional<runtime::PhysicsBody> body) {
  if (!IsAlive(entity))
    return false;
  if (!body) {
    if (physics_entities_.erase(entity) == 0)
      return false;
    return physics_.RemoveBody(entity);
  }
  body->id = entity;
  if (physics_entities_.contains(entity)) {
    physics_.RemoveBody(entity);
    physics_entities_.erase(entity);
  }
  if (!physics_.AddBody(*body))
    return false;
  physics_entities_.insert(entity);
  return true;
}

std::optional<runtime::Id> GameWorld::RaycastEntity(const runtime::RaycastRequest &request) const {
  const auto hit = physics_.Raycast(request);
  if (!hit || !IsAlive(hit->body))
    return std::nullopt;
  return hit->body;
}

bool GameWorld::SetCharacter(runtime::Id entity,
                             std::optional<runtime::CharacterControllerConfig> config) {
  const auto snapshot = GetEntity(entity);
  if (!snapshot)
    return false;
  if (!config)
    return characters_.erase(entity) != 0;
  CharacterBinding binding(*config);
  binding.state.position = {snapshot->transform.x, snapshot->transform.y, snapshot->transform.z};
  characters_.insert_or_assign(entity, std::move(binding));
  return true;
}

std::optional<runtime::CharacterState> GameWorld::GetCharacter(runtime::Id entity) const {
  const auto found = characters_.find(entity);
  if (found == characters_.end())
    return std::nullopt;
  return found->second.state;
}

std::optional<runtime::CharacterMoveResult>
GameWorld::TickCharacter(runtime::Id entity, const runtime::CharacterInput &input, double seconds,
                         bool ground_ready) {
  const auto found = characters_.find(entity);
  if (found == characters_.end() || !IsAlive(entity))
    return std::nullopt;
  auto &binding = found->second;
  auto result =
      binding.motor.Tick(binding.state, input, seconds, physics_, binding.controller, ground_ready);
  if (!SetTransform(entity,
                    {binding.state.position.x, binding.state.position.y, binding.state.position.z}))
    return std::nullopt;
  return result;
}
#endif

std::vector<runtime::Id> GameWorld::Query(runtime::Id scene, QueryMask mask) const {
  std::vector<runtime::Id> result;
  const auto *found_scene = world_.FindScene(scene);
  if (found_scene == nullptr)
    return result;
  for (const auto &entity : found_scene->entities) {
    const bool matches = mask == 0 || ((mask & kQueryCamera) != 0 && entity.camera) ||
                         ((mask & kQueryLight) != 0 && entity.light) ||
                         ((mask & kQueryMeshRenderer) != 0 && entity.mesh_renderer) ||
                         ((mask & kQueryAudio) != 0 && entity_audio_.contains(entity.id))
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
                         ||
                         ((mask & kQueryPhysics) != 0 && physics_entities_.contains(entity.id)) ||
                         ((mask & kQueryCharacter) != 0 && characters_.contains(entity.id))
#endif
        ;
    if (matches)
      result.push_back(entity.id);
  }
  return result;
}

} // namespace nexora::game
