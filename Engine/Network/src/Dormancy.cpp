#include "Nexora/Network/Dormancy.h"

#include <limits>

namespace nexora::network {

DirtyGeneration DormancyManager::Register(NetworkEntityID entity) {
  return entities_.try_emplace(entity).first->second.generation;
}

DirtyGeneration DormancyManager::MarkDirty(NetworkEntityID entity) {
  auto &state = entities_.try_emplace(entity).first->second;
  const bool was_dormant = state.dormant;
  auto &generation = state.generation;
  if (generation != std::numeric_limits<DirtyGeneration>::max()) {
    ++generation;
  }
  state.dormant = false;
  if (was_dormant) {
    state.wake_generation = generation;
  }
  return generation;
}

void DormancyManager::SetDormant(NetworkEntityID entity, bool dormant) {
  entities_.try_emplace(entity).first->second.dormant = dormant;
}

void DormancyManager::RemoveEntity(NetworkEntityID entity) {
  entities_.erase(entity);
  for (auto &[connection, states] : connections_) {
    (void)connection;
    states.erase(entity);
  }
}

ReplicationDecision DormancyManager::Evaluate(ConnectionID connection, NetworkEntityID entity,
                                              bool interested) {
  const auto entity_found = entities_.find(entity);
  if (entity_found == entities_.end()) {
    return {};
  }
  auto &state = connections_[connection][entity];
  if (!interested) {
    const bool was_interested = state.interested;
    state = {};
    return {was_interested ? ReplicationAction::Despawn : ReplicationAction::None,
            entity_found->second.generation, 0, false};
  }

  if (!state.interested) {
    state.interested = true;
    state.baseline_id = 0;
    return {ReplicationAction::FullSnapshot, entity_found->second.generation, 0,
            entity_found->second.wake_generation == entity_found->second.generation};
  }
  if (state.acknowledged >= entity_found->second.generation) {
    return {};
  }
  return {state.baseline_id == 0 ? ReplicationAction::FullSnapshot : ReplicationAction::Delta,
          entity_found->second.generation, state.baseline_id,
          entity_found->second.wake_generation == entity_found->second.generation};
}

bool DormancyManager::Acknowledge(ConnectionID connection, NetworkEntityID entity,
                                  DirtyGeneration generation, std::uint64_t baseline_id) {
  const auto entity_found = entities_.find(entity);
  const auto connection_found = connections_.find(connection);
  if (entity_found == entities_.end() || connection_found == connections_.end()) {
    return false;
  }
  const auto state_found = connection_found->second.find(entity);
  if (state_found == connection_found->second.end() || !state_found->second.interested ||
      generation < state_found->second.acknowledged ||
      generation > entity_found->second.generation || baseline_id == 0) {
    return false;
  }
  state_found->second.acknowledged = generation;
  state_found->second.baseline_id = baseline_id;
  return true;
}

void DormancyManager::RemoveConnection(ConnectionID connection) { connections_.erase(connection); }

DirtyGeneration DormancyManager::Generation(NetworkEntityID entity) const noexcept {
  const auto found = entities_.find(entity);
  return found == entities_.end() ? 0 : found->second.generation;
}

bool DormancyManager::IsDormant(NetworkEntityID entity) const noexcept {
  const auto found = entities_.find(entity);
  return found != entities_.end() && found->second.dormant;
}

} // namespace nexora::network
