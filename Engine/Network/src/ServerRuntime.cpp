#include "Nexora/Network/ServerRuntime.h"

#include <algorithm>
#include <stdexcept>

namespace nexora::network {
namespace {

std::uint64_t HashState(std::span<const std::byte> bytes) {
  std::uint64_t hash = 14695981039346656037ull;
  for (const auto byte : bytes) {
    hash ^= std::to_integer<std::uint8_t>(byte);
    hash *= 1099511628211ull;
  }
  return hash;
}

} // namespace

ServerRuntime::ServerRuntime(ServerRuntimeConfig config, IServerSimulation &simulation)
    : config_(config), simulation_(simulation) {
  if (config_.ticks_per_second == 0 || config_.max_catch_up_ticks == 0) {
    throw std::invalid_argument("server tick configuration must be non-zero");
  }
  tick_duration_ = std::chrono::nanoseconds{1'000'000'000 / config_.ticks_per_second};
}

AdmissionResult ServerRuntime::Admit(ServerClientID id, Connection &connection) {
  if (state_ != ServerRunState::Running) {
    return AdmissionResult::Draining;
  }
  if (connection.State() != ConnectionState::Connected) {
    return AdmissionResult::NotConnected;
  }
  if (clients_.contains(id)) {
    return AdmissionResult::Duplicate;
  }
  if (clients_.size() >= config_.max_clients) {
    return AdmissionResult::Capacity;
  }
  clients_.emplace(id, &connection);
  return AdmissionResult::Accepted;
}

void ServerRuntime::Remove(ServerClientID id) { clients_.erase(id); }

void ServerRuntime::Advance(std::chrono::nanoseconds elapsed) {
  if (state_ == ServerRunState::Stopped || elapsed <= std::chrono::nanoseconds::zero()) {
    return;
  }
  accumulator_ += elapsed;
  const auto available = accumulator_ / tick_duration_;
  const auto count = std::min<std::int64_t>(available, config_.max_catch_up_ticks);
  for (std::int64_t index = 0; index < count && state_ != ServerRunState::Stopped; ++index) {
    RunTick();
    accumulator_ -= tick_duration_;
  }
  // Bound overload rather than allowing an unbounded spiral of death.
  accumulator_ = std::min(accumulator_, tick_duration_ * config_.max_catch_up_ticks);
}

void ServerRuntime::RequestShutdown() {
  if (state_ == ServerRunState::Running) {
    state_ = ServerRunState::Draining;
    if (clients_.empty() || config_.shutdown_drain_ticks == 0) {
      FinishShutdown();
    }
  }
}

void ServerRuntime::RunTick() {
  ++tick_index_;
  const auto logical_time = (tick_index_ * 1000) / config_.ticks_per_second;
  for (auto iterator = clients_.begin(); iterator != clients_.end();) {
    auto &[id, connection] = *iterator;
    connection->Tick(logical_time);
    if (connection->State() != ConnectionState::Connected) {
      iterator = clients_.erase(iterator);
      continue;
    }
    std::size_t packets{};
    std::size_t bytes{};
    std::size_t pending_size{};
    // Peek the next packet's size before dequeuing it: Poll() unconditionally pops, so checking
    // the byte budget only after polling would permanently drop a packet that doesn't fit this
    // tick's remaining budget instead of deferring it to the next tick, unlike the packet-count
    // budget above (which already short-circuits before Poll() is ever called).
    while (packets < config_.packets_per_client_per_tick &&
           connection->PeekPendingPayloadSize(pending_size)) {
      // A packet larger than the *entire* per-tick budget can never satisfy the plain
      // over-budget check below on any tick (bytes resets to 0 each tick), which would leave it
      // stuck at the head of the queue forever and head-of-line-block every packet behind it.
      // Only defer for budget reasons once this tick has already accepted something: an empty
      // tick always takes at least the head packet, bounding the stall to one oversized packet
      // rather than blocking the connection indefinitely.
      if (bytes > 0 && pending_size > config_.bytes_per_client_per_tick -
                                          std::min(bytes, config_.bytes_per_client_per_tick)) {
        break;
      }
      Packet packet;
      static_cast<void>(connection->Poll(packet));
      bytes += packet.payload.size();
      ++packets;
      capture_.push_back({tick_index_, id, packet});
      simulation_.Receive(id, packet);
    }
    ++iterator;
  }
  simulation_.FixedTick(tick_index_);
  std::vector<std::byte> state;
  simulation_.CaptureCanonicalState(state);
  state_hash_ = HashState(state);

  if (state_ == ServerRunState::Draining &&
      (clients_.empty() || ++draining_ticks_ >= config_.shutdown_drain_ticks)) {
    FinishShutdown();
  }
}

void ServerRuntime::FinishShutdown() {
  for (auto &[id, connection] : clients_) {
    (void)id;
    connection->Disconnect();
  }
  clients_.clear();
  accumulator_ = {};
  state_ = ServerRunState::Stopped;
}

} // namespace nexora::network
