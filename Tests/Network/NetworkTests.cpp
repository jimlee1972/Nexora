#include "Nexora/Network/Transport.h"

#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

using namespace nexora::network;

namespace {

constexpr std::byte kHandshake{1};

[[noreturn]] void Fail(std::string_view message) {
  std::cerr << "network test failure: " << message << '\n';
  std::exit(EXIT_FAILURE);
}

void Require(bool condition, std::string_view message) {
  if (!condition) {
    Fail(message);
  }
}

void Advance(Connection &client, Connection &server, std::uint64_t time) {
  client.Tick(time);
  server.Tick(time);
  client.Tick(time);
}

std::span<const std::byte> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::byte *>(text.data()), text.size()};
}

void Connect(Connection &client, Connection &server, std::uint64_t time = 0) {
  Require(client.Connect(), "client could not start connection");
  Advance(client, server, time);
  Require(client.State() == ConnectionState::Connected, "client did not connect");
  Require(server.State() == ConnectionState::Connected, "server did not connect");
}

std::vector<std::pair<std::uint64_t, std::uint64_t>> SimulationTrace(std::uint64_t seed) {
  auto pair = CreateSimulatedTransportPair({35, 20, 10, seed});
  for (std::uint64_t sequence = 1; sequence <= 64; ++sequence) {
    Packet packet{1, ChannelSemantics::Unreliable, sequence, {std::byte{0x5a}}};
    Require(pair.client->Send(std::move(packet)), "simulated transport send failed");
  }

  std::vector<std::pair<std::uint64_t, std::uint64_t>> trace;
  for (std::uint64_t time = 0; time <= 40; ++time) {
    pair.server->Advance(time);
    Packet packet;
    while (pair.server->Poll(packet)) {
      trace.emplace_back(time, packet.sequence);
    }
  }
  return trace;
}

void TestLoopbackTransport() {
  auto pair = CreateLoopbackTransportPair();
  Connection client(*pair.client, {7, "build-a"}, false);
  Connection server(*pair.server, {7, "build-a"}, true);
  Connect(client, server);

  Require(client.Send(2, ChannelSemantics::ReliableOrdered, Bytes("hello")),
          "loopback user send failed");
  Advance(client, server, 0);
  Packet packet;
  Require(server.Poll(packet), "loopback packet was not delivered");
  Require(packet.channel == 2, "loopback channel changed");
  Require(packet.semantics == ChannelSemantics::ReliableOrdered,
          "loopback channel semantics changed");
  Require(packet.payload == std::vector<std::byte>(Bytes("hello").begin(), Bytes("hello").end()),
          "loopback payload changed");

  client.Disconnect();
  Advance(client, server, 0);
  Require(server.State() == ConnectionState::Disconnected,
          "loopback disconnect did not reach server");
}

void TestDeterministicSimulation() {
  const auto first = SimulationTrace(0x1234);
  const auto second = SimulationTrace(0x1234);
  const auto different_seed = SimulationTrace(0x5678);
  Require(first == second, "identical simulation seeds produced different traces");
  Require(first != different_seed, "different simulation seeds produced identical traces");
  Require(!first.empty() && first.size() < 64, "loss simulation did not retain and drop packets");
  for (const auto &[delivery_time, sequence] : first) {
    (void)sequence;
    Require(delivery_time >= 10 && delivery_time <= 30,
            "latency/jitter delivery fell outside the configured bounds");
  }

  auto loss_pair = CreateSimulatedTransportPair({100, 0, 0, 1});
  Packet packet{1, ChannelSemantics::Unreliable, 1, {std::byte{1}}};
  Require(loss_pair.client->Send(packet), "dropped datagram was reported as a send failure");
  loss_pair.server->Advance(1000);
  Require(!loss_pair.server->Poll(packet), "100 percent loss delivered a datagram");

  auto latency_pair = CreateSimulatedTransportPair({0, 25, 0, 1});
  Require(latency_pair.client->Send(packet), "latency datagram send failed");
  latency_pair.server->Advance(24);
  Require(!latency_pair.server->Poll(packet), "latency datagram arrived early");
  latency_pair.server->Advance(25);
  Require(latency_pair.server->Poll(packet), "latency datagram did not arrive on schedule");
}

void TestMalformedHandshakes() {
  const std::vector<std::vector<std::byte>> malformed{
      {},
      {kHandshake},
      {kHandshake, std::byte{7}, std::byte{0}, std::byte{0}, std::byte{0}},
      {kHandshake, std::byte{7}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{2},
       std::byte{'x'}},
      {kHandshake, std::byte{7}, std::byte{0}, std::byte{0}, std::byte{0}, std::byte{0},
       std::byte{'x'}},
  };

  for (const auto &payload : malformed) {
    auto pair = CreateLoopbackTransportPair();
    Connection server(*pair.server, {7, "build-a"}, true);
    Require(pair.client->Send(
                {0, ChannelSemantics::ReliableOrdered, 0, std::vector<std::byte>(payload)}),
            "malformed packet injection failed");
    server.Tick(0);
    if (payload.empty()) {
      Require(server.State() == ConnectionState::Disconnected,
              "empty packet changed connection state");
    } else {
      Require(server.State() == ConnectionState::Rejected, "malformed handshake was not rejected");
      Require(server.Rejection() == RejectReason::MalformedHandshake,
              "malformed handshake returned the wrong rejection reason");
    }
  }
}

void TestIdentityMismatchRejection() {
  for (const auto &[client_identity, expected] :
       std::vector<std::pair<ProtocolIdentity, RejectReason>>{
           {{8, "build-a"}, RejectReason::ProtocolMismatch},
           {{7, "build-b"}, RejectReason::BuildMismatch}}) {
    auto pair = CreateLoopbackTransportPair();
    Connection client(*pair.client, client_identity, false);
    Connection server(*pair.server, {7, "build-a"}, true);
    Require(client.Connect(), "mismatched client could not send handshake");
    Advance(client, server, 0);
    Require(client.State() == ConnectionState::Rejected, "identity mismatch was not rejected");
    Require(client.Rejection() == expected, "identity mismatch returned the wrong reason");
    Require(server.State() == ConnectionState::Rejected,
            "server did not enter rejected state for identity mismatch");
  }
}

void TestReconnectDisconnectStress() {
  auto pair = CreateLoopbackTransportPair();
  Connection client(*pair.client, {7, "build-a"}, false);
  Connection server(*pair.server, {7, "build-a"}, true);
  for (std::uint64_t iteration = 0; iteration < 1000; ++iteration) {
    Connect(client, server, iteration);
    Require(client.Send(3, ChannelSemantics::UnreliableSequenced, Bytes("cycle")),
            "stress packet send failed");
    Advance(client, server, iteration);
    Packet packet;
    Require(server.Poll(packet), "reconnected session retained stale sequence state");
    Require(packet.sequence == 1, "reconnected session did not reset its sequence");
    client.Disconnect();
    Advance(client, server, iteration);
    Require(client.State() == ConnectionState::Disconnected,
            "client remained connected during stress test");
    Require(server.State() == ConnectionState::Disconnected,
            "server remained connected during stress test");
    Require(!server.Poll(packet), "disconnect retained a received packet");
  }
}

} // namespace

int main() {
  TestLoopbackTransport();
  TestDeterministicSimulation();
  TestMalformedHandshakes();
  TestIdentityMismatchRejection();
  TestReconnectDisconnectStress();
}
