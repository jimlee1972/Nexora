#pragma once

#include "Nexora/Network/Api.h"

#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace nexora::network {

enum class ChannelSemantics : std::uint8_t {
  Unreliable,
  UnreliableSequenced,
  ReliableOrdered,
};

struct Packet {
  std::uint8_t channel{};
  ChannelSemantics semantics{ChannelSemantics::Unreliable};
  std::uint64_t sequence{};
  std::vector<std::byte> payload;
};

class NEXORA_NETWORK_API INetTransport {
public:
  virtual ~INetTransport() = default;
  [[nodiscard]] virtual bool Send(Packet packet) = 0;
  [[nodiscard]] virtual bool Poll(Packet &packet) = 0;
  virtual void Advance(std::uint64_t now_milliseconds) = 0;
  virtual void Close() = 0;
  [[nodiscard]] virtual bool IsOpen() const noexcept = 0;
};

struct PacketSimulation {
  std::uint32_t loss_percent{};
  std::uint32_t latency_milliseconds{};
  std::uint32_t jitter_milliseconds{};
  std::uint64_t seed{1};
};

struct TransportPair {
  std::unique_ptr<INetTransport> client;
  std::unique_ptr<INetTransport> server;
};

[[nodiscard]] NEXORA_NETWORK_API TransportPair CreateLoopbackTransportPair();

[[nodiscard]] NEXORA_NETWORK_API TransportPair CreateSimulatedTransportPair(
    PacketSimulation client_to_server = {}, PacketSimulation server_to_client = {});

struct ProtocolIdentity {
  std::uint32_t protocol_version{};
  std::string build_id;
};

enum class ConnectionState : std::uint8_t {
  Disconnected,
  Connecting,
  Connected,
  Rejected,
};

enum class RejectReason : std::uint8_t {
  None,
  ProtocolMismatch,
  BuildMismatch,
  MalformedHandshake,
};

class NEXORA_NETWORK_API Connection final {
public:
  Connection(INetTransport &transport, ProtocolIdentity identity, bool server);

  [[nodiscard]] bool Connect();
  void Disconnect();
  void Tick(std::uint64_t now_milliseconds);
  [[nodiscard]] bool Send(std::uint8_t channel, ChannelSemantics semantics,
                          std::span<const std::byte> payload);
  [[nodiscard]] bool Poll(Packet &packet);

  [[nodiscard]] ConnectionState State() const noexcept { return state_; }
  [[nodiscard]] RejectReason Rejection() const noexcept { return rejection_; }

private:
  INetTransport &transport_;
  ProtocolIdentity identity_;
  bool server_{};
  ConnectionState state_{ConnectionState::Disconnected};
  RejectReason rejection_{RejectReason::None};
  std::uint64_t next_sequence_{};
  std::uint64_t last_sequenced_received_{};
  std::deque<Packet> received_;
};

} // namespace nexora::network
