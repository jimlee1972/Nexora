#pragma once

#include "Nexora/Network/Api.h"
#include "Nexora/Network/Transport.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <map>
#include <span>
#include <vector>

namespace nexora::network {

using ServerClientID = std::uint64_t;

struct ServerRuntimeConfig {
  std::uint32_t ticks_per_second{60};
  std::uint32_t max_catch_up_ticks{4};
  std::size_t max_clients{64};
  std::size_t packets_per_client_per_tick{128};
  std::size_t bytes_per_client_per_tick{64 * 1024};
  std::uint32_t shutdown_drain_ticks{120};
};

enum class ServerRunState : std::uint8_t { Running, Draining, Stopped };
enum class AdmissionResult : std::uint8_t { Accepted, NotConnected, Duplicate, Capacity, Draining };

struct CapturedServerPacket {
  std::uint64_t tick{};
  ServerClientID client{};
  Packet packet;

  bool operator==(const CapturedServerPacket &) const = default;
};

class NEXORA_NETWORK_API IServerSimulation {
public:
  virtual ~IServerSimulation() = default;
  virtual void Receive(ServerClientID client, const Packet &packet) = 0;
  virtual void FixedTick(std::uint64_t tick) = 0;
  virtual void CaptureCanonicalState(std::vector<std::byte> &output) const = 0;
};

// Caller-owned and single-threaded. ServerRuntime is the sole owner of fixed-step scheduling, but
// only borrows admitted Connection objects; they must outlive admission and be removed before
// destruction. Wall-clock time is never passed into simulation callbacks.
class NEXORA_NETWORK_API ServerRuntime final {
public:
  ServerRuntime(ServerRuntimeConfig config, IServerSimulation &simulation);

  [[nodiscard]] AdmissionResult Admit(ServerClientID id, Connection &connection);
  void Remove(ServerClientID id);
  void Advance(std::chrono::nanoseconds elapsed);
  void RequestShutdown();

  [[nodiscard]] ServerRunState State() const noexcept { return state_; }
  [[nodiscard]] std::uint64_t TickIndex() const noexcept { return tick_index_; }
  [[nodiscard]] std::uint64_t StateHash() const noexcept { return state_hash_; }
  [[nodiscard]] std::size_t ClientCount() const noexcept { return clients_.size(); }
  [[nodiscard]] const std::vector<CapturedServerPacket> &Capture() const noexcept {
    return capture_;
  }

private:
  void RunTick();
  void FinishShutdown();

  ServerRuntimeConfig config_;
  IServerSimulation &simulation_;
  std::map<ServerClientID, Connection *> clients_;
  std::vector<CapturedServerPacket> capture_;
  std::chrono::nanoseconds accumulator_{};
  std::chrono::nanoseconds tick_duration_{};
  ServerRunState state_{ServerRunState::Running};
  std::uint64_t tick_index_{};
  std::uint64_t state_hash_{14695981039346656037ull};
  std::uint32_t draining_ticks_{};
};

} // namespace nexora::network
