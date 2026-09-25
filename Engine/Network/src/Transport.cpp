#include "Nexora/Network/Transport.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <limits>
#include <utility>

namespace nexora::network {
namespace {

constexpr std::uint8_t kHandshake = 1;
constexpr std::uint8_t kAccept = 2;
constexpr std::uint8_t kReject = 3;
constexpr std::uint8_t kDisconnect = 4;
constexpr std::uint8_t kUser = 5;

struct QueuedPacket {
  std::uint64_t delivery_time{};
  std::uint64_t order{};
  Packet packet;
};

struct SharedLink {
  std::array<std::deque<QueuedPacket>, 2> pending;
  std::array<bool, 2> open{true, true};
  std::array<std::uint64_t, 2> now{};
  std::array<std::uint64_t, 2> random_state{};
  std::uint64_t order{};
};

std::uint64_t NextRandom(std::uint64_t &state) {
  state ^= state << 13U;
  state ^= state >> 7U;
  state ^= state << 17U;
  return state;
}

class SimulatedTransport final : public INetTransport {
public:
  SimulatedTransport(std::shared_ptr<SharedLink> link, std::size_t side,
                     PacketSimulation simulation)
      : link_(std::move(link)), side_(side), simulation_(simulation) {
    link_->random_state[side_] = simulation_.seed == 0 ? 1 : simulation_.seed;
  }

  bool Send(Packet packet) override {
    const auto peer = 1U - side_;
    if (!IsOpen() || !link_->open[peer]) {
      return false;
    }
    auto &random = link_->random_state[side_];
    if ((NextRandom(random) % 100U) < std::min(simulation_.loss_percent, 100U)) {
      return true;
    }
    const auto jitter_range = static_cast<std::uint64_t>(simulation_.jitter_milliseconds) * 2U + 1U;
    const auto jitter_sample = static_cast<std::int64_t>(NextRandom(random) % jitter_range) -
                               static_cast<std::int64_t>(simulation_.jitter_milliseconds);
    const auto delay = std::max<std::int64_t>(
        0, static_cast<std::int64_t>(simulation_.latency_milliseconds) + jitter_sample);
    link_->pending[peer].push_back(
        {link_->now[side_] + static_cast<std::uint64_t>(delay), link_->order++, std::move(packet)});
    return true;
  }

  bool Poll(Packet &packet) override {
    auto &queue = link_->pending[side_];
    const auto selected =
        std::min_element(queue.begin(), queue.end(), [](const auto &lhs, const auto &rhs) {
          return std::pair{lhs.delivery_time, lhs.order} < std::pair{rhs.delivery_time, rhs.order};
        });
    if (selected == queue.end() || selected->delivery_time > link_->now[side_]) {
      return false;
    }
    packet = std::move(selected->packet);
    queue.erase(selected);
    return true;
  }

  void Advance(std::uint64_t now_milliseconds) override {
    link_->now[side_] = std::max(link_->now[side_], now_milliseconds);
  }
  void Close() override { link_->open[side_] = false; }
  bool IsOpen() const noexcept override { return link_->open[side_]; }

private:
  std::shared_ptr<SharedLink> link_;
  std::size_t side_{};
  PacketSimulation simulation_;
};

Packet ControlPacket(std::uint8_t kind, std::span<const std::byte> body = {}) {
  Packet packet{0, ChannelSemantics::ReliableOrdered, 0, {std::byte{kind}}};
  packet.payload.insert(packet.payload.end(), body.begin(), body.end());
  return packet;
}

void AppendU32(std::vector<std::byte> &output, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8) {
    output.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
  }
}

std::uint32_t ReadU32(std::span<const std::byte> bytes) {
  std::uint32_t value{};
  for (unsigned index = 0; index < 4; ++index) {
    value |= std::to_integer<std::uint32_t>(bytes[index]) << (index * 8U);
  }
  return value;
}

} // namespace

TransportPair CreateSimulatedTransportPair(PacketSimulation client_to_server,
                                           PacketSimulation server_to_client) {
  auto link = std::make_shared<SharedLink>();
  return {std::make_unique<SimulatedTransport>(link, 0, client_to_server),
          std::make_unique<SimulatedTransport>(std::move(link), 1, server_to_client)};
}

TransportPair CreateLoopbackTransportPair() { return CreateSimulatedTransportPair(); }

Connection::Connection(INetTransport &transport, ProtocolIdentity identity, bool server)
    : transport_(transport), identity_(std::move(identity)), server_(server) {}

bool Connection::Connect() {
  if (server_ || state_ != ConnectionState::Disconnected || identity_.build_id.size() > 255) {
    return false;
  }
  std::vector<std::byte> body;
  AppendU32(body, identity_.protocol_version);
  body.push_back(static_cast<std::byte>(identity_.build_id.size()));
  body.insert(
      body.end(), reinterpret_cast<const std::byte *>(identity_.build_id.data()),
      reinterpret_cast<const std::byte *>(identity_.build_id.data() + identity_.build_id.size()));
  rejection_ = RejectReason::None;
  next_sequence_ = 0;
  last_sequenced_received_ = 0;
  received_.clear();
  state_ = ConnectionState::Connecting;
  return transport_.Send(ControlPacket(kHandshake, body));
}

void Connection::Disconnect() {
  if (state_ == ConnectionState::Connected) {
    (void)transport_.Send(ControlPacket(kDisconnect));
  }
  state_ = ConnectionState::Disconnected;
  rejection_ = RejectReason::None;
  next_sequence_ = 0;
  last_sequenced_received_ = 0;
  received_.clear();
}

void Connection::Tick(std::uint64_t now_milliseconds) {
  transport_.Advance(now_milliseconds);
  Packet packet;
  while (transport_.Poll(packet)) {
    if (packet.payload.empty()) {
      continue;
    }
    const auto kind = std::to_integer<std::uint8_t>(packet.payload.front());
    if (kind == kHandshake && server_) {
      if (packet.payload.size() < 6) {
        rejection_ = RejectReason::MalformedHandshake;
      } else {
        const auto version = ReadU32(std::span(packet.payload).subspan(1, 4));
        const auto size = std::to_integer<std::size_t>(packet.payload[5]);
        if (packet.payload.size() != 6 + size) {
          rejection_ = RejectReason::MalformedHandshake;
        } else if (version != identity_.protocol_version) {
          rejection_ = RejectReason::ProtocolMismatch;
        } else if (std::string(reinterpret_cast<const char *>(packet.payload.data() + 6), size) !=
                   identity_.build_id) {
          rejection_ = RejectReason::BuildMismatch;
        } else {
          state_ = ConnectionState::Connected;
          rejection_ = RejectReason::None;
          next_sequence_ = 0;
          last_sequenced_received_ = 0;
          received_.clear();
          (void)transport_.Send(ControlPacket(kAccept));
          continue;
        }
      }
      state_ = ConnectionState::Rejected;
      const std::array body{static_cast<std::byte>(rejection_)};
      (void)transport_.Send(ControlPacket(kReject, body));
    } else if (kind == kAccept && !server_ && state_ == ConnectionState::Connecting) {
      state_ = ConnectionState::Connected;
    } else if (kind == kReject && !server_ && packet.payload.size() == 2) {
      state_ = ConnectionState::Rejected;
      rejection_ = static_cast<RejectReason>(std::to_integer<std::uint8_t>(packet.payload[1]));
    } else if (kind == kDisconnect) {
      state_ = ConnectionState::Disconnected;
      rejection_ = RejectReason::None;
      next_sequence_ = 0;
      last_sequenced_received_ = 0;
      received_.clear();
    } else if (kind == kUser && state_ == ConnectionState::Connected) {
      packet.payload.erase(packet.payload.begin());
      if (packet.semantics == ChannelSemantics::UnreliableSequenced &&
          packet.sequence <= last_sequenced_received_) {
        continue;
      }
      if (packet.semantics == ChannelSemantics::UnreliableSequenced) {
        last_sequenced_received_ = packet.sequence;
      }
      received_.push_back(std::move(packet));
    }
  }
}

bool Connection::Send(std::uint8_t channel, ChannelSemantics semantics,
                      std::span<const std::byte> payload) {
  if (state_ != ConnectionState::Connected) {
    return false;
  }
  Packet packet{channel, semantics, ++next_sequence_, {std::byte{kUser}}};
  packet.payload.insert(packet.payload.end(), payload.begin(), payload.end());
  return transport_.Send(std::move(packet));
}

bool Connection::Poll(Packet &packet) {
  if (received_.empty()) {
    return false;
  }
  packet = std::move(received_.front());
  received_.pop_front();
  return true;
}

} // namespace nexora::network
