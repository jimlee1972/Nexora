#pragma once

#include "Nexora/Network/Api.h"
#include "Nexora/Network/EntityMapping.h"
#include "Nexora/Network/Replication.h"

#include <cstdint>
#include <unordered_map>

namespace nexora::network {

using DirtyGeneration = std::uint64_t;

enum class ReplicationAction : std::uint8_t { None, Delta, FullSnapshot, Despawn };

struct ReplicationDecision final {
  ReplicationAction action{ReplicationAction::None};
  DirtyGeneration generation{};
  std::uint64_t baseline_id{};
  bool woke_from_dormancy{};
};

// Tracks authoritative dirty state and connection-local acknowledgement/baseline state.
// The caller serializes access and keeps entity/session identities valid.
class NEXORA_NETWORK_API DormancyManager final {
public:
  [[nodiscard]] DirtyGeneration Register(NetworkEntityID entity);
  [[nodiscard]] DirtyGeneration MarkDirty(NetworkEntityID entity);
  void SetDormant(NetworkEntityID entity, bool dormant);
  void RemoveEntity(NetworkEntityID entity);

  [[nodiscard]] ReplicationDecision Evaluate(ConnectionID connection, NetworkEntityID entity,
                                             bool interested);
  [[nodiscard]] bool Acknowledge(ConnectionID connection, NetworkEntityID entity,
                                 DirtyGeneration generation, std::uint64_t baseline_id);
  void RemoveConnection(ConnectionID connection);

  [[nodiscard]] DirtyGeneration Generation(NetworkEntityID entity) const noexcept;
  [[nodiscard]] bool IsDormant(NetworkEntityID entity) const noexcept;

private:
  struct EntityHash final {
    [[nodiscard]] std::size_t operator()(NetworkEntityID entity) const noexcept {
      return static_cast<std::size_t>(entity.index) * 0x9e3779b1u ^ entity.generation;
    }
  };
  struct EntityState final {
    DirtyGeneration generation{1};
    DirtyGeneration wake_generation{};
    bool dormant{};
  };
  struct ConnectionEntityState final {
    DirtyGeneration acknowledged{};
    std::uint64_t baseline_id{};
    bool interested{};
  };

  std::unordered_map<NetworkEntityID, EntityState, EntityHash> entities_;
  std::unordered_map<ConnectionID,
                     std::unordered_map<NetworkEntityID, ConnectionEntityState, EntityHash>>
      connections_;
};

} // namespace nexora::network
