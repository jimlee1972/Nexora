#include "Nexora/Game/GameWorld.h"

#include <algorithm>
#include <cmath>

namespace nexora::game {
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
namespace {
// The point whose image under the affine `matrix` (column-major) is `point`; nullopt for a singular
// or non-finite matrix or result.
std::optional<runtime::SimulationVector> InverseAffinePoint(const runtime::TransformMatrix &matrix,
                                                            runtime::SimulationVector point) {
  const double x[3]{matrix[0], matrix[1], matrix[2]};
  const double y[3]{matrix[4], matrix[5], matrix[6]};
  const double z[3]{matrix[8], matrix[9], matrix[10]};
  const double v[3]{point.x - matrix[12], point.y - matrix[13], point.z - matrix[14]};
  // Cramer's rule with det(a, b, c) = a . (b x c).
  const auto det = [](const double *a, const double *b, const double *c) {
    return a[0] * (b[1] * c[2] - b[2] * c[1]) - a[1] * (b[0] * c[2] - b[2] * c[0]) +
           a[2] * (b[0] * c[1] - b[1] * c[0]);
  };
  const auto d = det(x, y, z);
  if (!std::isfinite(d) || std::abs(d) < 1e-300)
    return std::nullopt;
  const runtime::SimulationVector result{det(v, y, z) / d, det(x, v, z) / d, det(x, y, v) / d};
  if (!std::isfinite(result.x) || !std::isfinite(result.y) || !std::isfinite(result.z))
    return std::nullopt;
  return result;
}
} // namespace
#endif

InputSnapshot CaptureInput(runtime::InputSystem &input, runtime::InputUserId user) {
  return {input.Consume(user)};
}

runtime::Id GameWorld::SpawnEntity(runtime::Id scene, EntitySpawnDescriptor descriptor) {
  // Validate before creating anything so an invalid transform leaves the world untouched.
  const auto transform = runtime::NormalizedTransform(descriptor.transform);
  if (!transform)
    throw std::invalid_argument(
        "invalid transform: non-finite value, zero scale, or zero rotation");
  auto &entity = world_.CreateEntity(scene);
  entity.transform = *transform;
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
  // Destruction cascades to descendants; release the bindings of every entity that was removed.
  for (const auto destroyed : commands.LastDestroyed())
    RemoveBindings(destroyed);
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
  for (const auto entity : commands.commands_.LastDestroyed())
    RemoveBindings(entity);
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

bool GameWorld::SetParent(runtime::Id entity, runtime::Id parent, bool keep_world) {
  runtime::WorldCommandBuffer commands;
  commands.SetParent(entity, parent, keep_world);
  return commands.Apply(world_);
}

bool GameWorld::SetSiblingIndex(runtime::Id entity, std::size_t index) {
  runtime::WorldCommandBuffer commands;
  commands.SetSiblingIndex(entity, index);
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
  // The controller works in world space; under a parent the entity's transform is local.
  const auto matrix = world_.WorldMatrix(entity);
  if (!matrix)
    return false;
  CharacterBinding binding(*config);
  binding.state.position = {(*matrix)[12], (*matrix)[13], (*matrix)[14]};
  binding.local = {snapshot->transform.x, snapshot->transform.y, snapshot->transform.z};
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
  const auto *current = world_.FindEntity(entity);
  const auto matrix = world_.WorldMatrix(entity);
  if (current == nullptr || !matrix)
    return std::nullopt;
  // Like Unity's CharacterController, start each move from where the transform is now in world
  // space. The exact matrix translation is used, since the world TRS is only approximate under a
  // sheared hierarchy. Work on a copy so a move that cannot be stored leaves the binding as it was.
  auto state = binding.state;
  const runtime::SimulationVector world_position{(*matrix)[12], (*matrix)[13], (*matrix)[14]};
  // A teleport is something else writing the entity's position: its local position changed and
  // so did its world position. A moved parent changes only the world position, and a keep-world
  // reparent only the local one; both carry the character and keep its ground contact.
  const auto &local = current->transform;
  const bool local_moved =
      local.x != binding.local.x || local.y != binding.local.y || local.z != binding.local.z;
  const auto near = [](double a, double b) {
    return std::abs(a - b) <= 1e-9 * std::max({1.0, std::abs(a), std::abs(b)});
  };
  const bool world_moved = !near(world_position.x, binding.state.position.x) ||
                           !near(world_position.y, binding.state.position.y) ||
                           !near(world_position.z, binding.state.position.z);
  if (local_moved && world_moved)
    binding.controller.Teleport(state, world_position, false, ground_ready);
  else
    state.position = world_position;
  auto result =
      binding.motor.Tick(state, input, seconds, physics_, binding.controller, ground_ready);
  // Store the new world position in the parent's space, changing only the local position.
  auto stored = state.position;
  if (current->parent != 0) {
    const auto parent = world_.WorldMatrix(current->parent);
    const auto inverse = parent ? InverseAffinePoint(*parent, state.position) : std::nullopt;
    if (!inverse)
      return std::nullopt;
    stored = *inverse;
  }
  if (!SetTransform(entity,
                    runtime::WithPosition(current->transform, stored.x, stored.y, stored.z)))
    return std::nullopt;
  binding.state = state;
  binding.local = stored;
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
