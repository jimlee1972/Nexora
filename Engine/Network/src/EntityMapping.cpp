#include "Nexora/Network/EntityMapping.h"

#include <algorithm>

namespace nexora::network {

std::optional<NetworkEntityID> ServerEntityMap::Spawn(LocalEntityID local_entity) {
  if (local_entity == 0 || local_to_network_.contains(local_entity)) {
    return std::nullopt;
  }

  while (!free_.empty() && slots_[free_.back()].retired) {
    free_.pop_back();
  }

  std::uint32_t index{};
  if (free_.empty()) {
    if (slots_.size() >= std::numeric_limits<std::uint32_t>::max()) {
      return std::nullopt;
    }
    index = static_cast<std::uint32_t>(slots_.size());
    slots_.push_back({});
  } else {
    index = free_.back();
    free_.pop_back();
  }

  auto &slot = slots_[index];
  slot.alive = true;
  slot.local_entity = local_entity;
  const NetworkEntityID network_entity{index, slot.generation};
  local_to_network_.emplace(local_entity, network_entity);
  return network_entity;
}

bool ServerEntityMap::Despawn(NetworkEntityID network_entity) {
  if (!Contains(network_entity)) {
    return false;
  }
  local_to_network_.erase(slots_[network_entity.index].local_entity);
  ReleaseSlot(network_entity.index);
  return true;
}

void ServerEntityMap::ResetSession() {
  local_to_network_.clear();
  free_.clear();
  for (std::uint32_t index = 0; index < slots_.size(); ++index) {
    if (slots_[index].alive) {
      ReleaseSlot(index);
    } else if (!slots_[index].retired) {
      free_.push_back(index);
    }
  }
  std::sort(free_.begin(), free_.end());
  free_.erase(std::unique(free_.begin(), free_.end()), free_.end());
}

std::optional<LocalEntityID> ServerEntityMap::Resolve(NetworkEntityID network_entity) const {
  if (!Contains(network_entity)) {
    return std::nullopt;
  }
  return slots_[network_entity.index].local_entity;
}

std::optional<NetworkEntityID> ServerEntityMap::Resolve(LocalEntityID local_entity) const {
  const auto found = local_to_network_.find(local_entity);
  return found == local_to_network_.end() ? std::nullopt
                                          : std::optional<NetworkEntityID>{found->second};
}

bool ServerEntityMap::Contains(NetworkEntityID network_entity) const noexcept {
  return network_entity.IsValid() && network_entity.index < slots_.size() &&
         slots_[network_entity.index].alive &&
         slots_[network_entity.index].generation == network_entity.generation;
}

void ServerEntityMap::ReleaseSlot(std::uint32_t index) {
  auto &slot = slots_[index];
  slot.alive = false;
  slot.local_entity = 0;
  if (slot.generation == std::numeric_limits<std::uint32_t>::max()) {
    slot.retired = true;
  } else {
    ++slot.generation;
    free_.push_back(index);
  }
}

bool ClientEntityMap::Spawn(NetworkEntityID network_entity, LocalEntityID local_entity) {
  if (!network_entity.IsValid() || local_entity == 0 || local_to_network_.contains(local_entity) ||
      network_to_local_.contains(network_entity.index)) {
    return false;
  }
  const auto previous = last_generation_.find(network_entity.index);
  if (previous != last_generation_.end() && network_entity.generation <= previous->second) {
    return false;
  }

  network_to_local_.emplace(network_entity.index, Binding{network_entity.generation, local_entity});
  local_to_network_.emplace(local_entity, network_entity);
  last_generation_[network_entity.index] = network_entity.generation;
  return true;
}

bool ClientEntityMap::Despawn(NetworkEntityID network_entity) {
  const auto found = network_to_local_.find(network_entity.index);
  if (!network_entity.IsValid() || found == network_to_local_.end() ||
      found->second.generation != network_entity.generation) {
    return false;
  }
  local_to_network_.erase(found->second.local_entity);
  last_generation_[network_entity.index] = network_entity.generation;
  network_to_local_.erase(found);
  return true;
}

void ClientEntityMap::ResetSession() {
  network_to_local_.clear();
  local_to_network_.clear();
  last_generation_.clear();
}

std::optional<LocalEntityID> ClientEntityMap::Resolve(NetworkEntityID network_entity) const {
  const auto found = network_to_local_.find(network_entity.index);
  if (!network_entity.IsValid() || found == network_to_local_.end() ||
      found->second.generation != network_entity.generation) {
    return std::nullopt;
  }
  return found->second.local_entity;
}

std::optional<NetworkEntityID> ClientEntityMap::Resolve(LocalEntityID local_entity) const {
  const auto found = local_to_network_.find(local_entity);
  return found == local_to_network_.end() ? std::nullopt
                                          : std::optional<NetworkEntityID>{found->second};
}

} // namespace nexora::network
