#pragma once

#include "Nexora/Network/Api.h"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <unordered_map>
#include <vector>

namespace nexora::network {

// A session-scoped replication identity. It is deliberately not a local ECS
// handle: only the server allocator creates values and each peer owns a local
// mapping for them.
struct NetworkEntityID final {
  std::uint32_t index{std::numeric_limits<std::uint32_t>::max()};
  std::uint32_t generation{};

  [[nodiscard]] constexpr bool IsValid() const noexcept {
    return generation != 0 && index != std::numeric_limits<std::uint32_t>::max();
  }
  friend constexpr bool operator==(NetworkEntityID, NetworkEntityID) noexcept = default;
};

using LocalEntityID = std::uint64_t;

// Server-owned allocator and bidirectional local binding. ResetSession releases
// all live identities while preserving their advanced generations, so delayed
// IDs from the previous connection remain stale.
class NEXORA_NETWORK_API ServerEntityMap final {
public:
  [[nodiscard]] std::optional<NetworkEntityID> Spawn(LocalEntityID local_entity);
  [[nodiscard]] bool Despawn(NetworkEntityID network_entity);
  void ResetSession();

  [[nodiscard]] std::optional<LocalEntityID> Resolve(NetworkEntityID network_entity) const;
  [[nodiscard]] std::optional<NetworkEntityID> Resolve(LocalEntityID local_entity) const;
  [[nodiscard]] bool Contains(NetworkEntityID network_entity) const noexcept;
  [[nodiscard]] std::size_t Size() const noexcept { return local_to_network_.size(); }

private:
  struct Slot final {
    std::uint32_t generation{1};
    LocalEntityID local_entity{};
    bool alive{};
    bool retired{};
  };

  void ReleaseSlot(std::uint32_t index);

  std::vector<Slot> slots_;
  std::vector<std::uint32_t> free_;
  std::unordered_map<LocalEntityID, NetworkEntityID> local_to_network_;
};

// Client-owned mapping. Spawn consumes an ID received from the server and
// rejects duplicate local handles, conflicting bindings, and generations known
// to be stale. Despawn tombstones the generation; ResetSession clears both
// bindings and tombstones because NetworkEntityID is scoped to that session.
class NEXORA_NETWORK_API ClientEntityMap final {
public:
  [[nodiscard]] bool Spawn(NetworkEntityID network_entity, LocalEntityID local_entity);
  [[nodiscard]] bool Despawn(NetworkEntityID network_entity);
  void ResetSession();

  [[nodiscard]] std::optional<LocalEntityID> Resolve(NetworkEntityID network_entity) const;
  [[nodiscard]] std::optional<NetworkEntityID> Resolve(LocalEntityID local_entity) const;
  [[nodiscard]] std::size_t Size() const noexcept { return local_to_network_.size(); }

private:
  struct Binding final {
    std::uint32_t generation{};
    LocalEntityID local_entity{};
  };

  std::unordered_map<std::uint32_t, Binding> network_to_local_;
  std::unordered_map<LocalEntityID, NetworkEntityID> local_to_network_;
  std::unordered_map<std::uint32_t, std::uint32_t> last_generation_;
};

} // namespace nexora::network
