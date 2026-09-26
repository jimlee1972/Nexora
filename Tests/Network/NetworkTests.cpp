#include "Nexora/Network/Dormancy.h"
#include "Nexora/Network/EntityMapping.h"
#include "Nexora/Network/Prediction.h"
#include "Nexora/Network/Replication.h"
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

void TestNetworkEntityMapping() {
  ServerEntityMap server;
  ClientEntityMap client;
  constexpr LocalEntityID server_local = 17;
  constexpr LocalEntityID client_local = 9'000'001;

  const auto first = server.Spawn(server_local);
  Require(first.has_value(), "server did not allocate a network entity ID");
  Require(server.Resolve(*first) == server_local, "server network mapping did not resolve");
  Require(client.Spawn(*first, client_local), "client did not bind the server spawn");
  Require(client.Resolve(*first) == client_local, "client network mapping did not resolve");
  Require(server_local != client_local, "test local entity IDs unexpectedly match");
  Require(server.Resolve(*first) != client.Resolve(*first),
          "network identity did not decouple peer-local entity IDs");

  Require(server.Despawn(*first), "server did not despawn network entity");
  Require(client.Despawn(*first), "client did not apply network despawn");
  Require(!server.Resolve(*first).has_value(), "server accepted a stale network entity ID");
  Require(!client.Resolve(*first).has_value(), "client accepted a stale network entity ID");
  Require(!client.Spawn(*first, client_local + 1), "client rebound a stale generation");

  const auto second = server.Spawn(server_local + 1);
  Require(second.has_value(), "server did not reuse a released network slot");
  Require(second->index == first->index && second->generation > first->generation,
          "server did not advance the reused slot generation");
  Require(client.Spawn(*second, client_local + 1), "client rejected a fresh generation");
  Require(!client.Despawn(*first), "stale despawn removed a fresh client binding");
  Require(client.Resolve(*second) == client_local + 1,
          "stale despawn corrupted the fresh client binding");

  server.ResetSession();
  client.ResetSession();
  Require(server.Size() == 0 && client.Size() == 0, "reconnect reset retained entity mappings");
  Require(!server.Resolve(*second).has_value(), "server reconnect retained an old mapping");
  Require(!client.Resolve(*second).has_value(), "client reconnect retained an old mapping");

  const auto reconnected = server.Spawn(server_local + 2);
  Require(reconnected.has_value() && reconnected->generation > second->generation,
          "server reconnect did not invalidate the previous session generation");
  Require(client.Spawn(*reconnected, client_local + 2),
          "client could not bind an entity after reconnect");
  Require(!server.Spawn(server_local + 2).has_value(),
          "server allocated two IDs for one local entity");
}

ReplicationSchema TestSchema() {
  return {
      0x10203040,
      2,
      {{1, WireType::Unsigned, FieldCompatibility::Required, Quantization{0, 1000, 10}, "health"},
       {7, WireType::Bytes, FieldCompatibility::Optional, std::nullopt, "name"}}};
}

std::vector<std::byte> ByteValues(std::initializer_list<unsigned> values) {
  std::vector<std::byte> result;
  for (const auto value : values) {
    result.push_back(static_cast<std::byte>(value));
  }
  return result;
}

void TestReplicationSchemaAndSnapshot() {
  const auto schema = TestSchema();
  Require(schema.IsValid(), "replication schema was invalid");
  Require(schema.Hash() == 0xa634de9a8a78180cull, "cross-build schema hash changed");
  ReplicationSchema older{
      0x10203040,
      1,
      {{1, WireType::Unsigned, FieldCompatibility::Required, Quantization{0, 1000, 10}, "hp"}}};
  Require(schema.IsBackwardCompatibleWith(older), "optional schema extension was incompatible");
  ReplicationSchema breaking{
      0x10203040,
      3,
      {{1, WireType::Signed, FieldCompatibility::Required, Quantization{0, 1000, 10}, "health"}}};
  Require(!breaking.IsBackwardCompatibleWith(schema), "wire type change was compatible");

  Snapshot input{42,
                 {{7, WireType::Bytes, ByteValues({'n', 'p', 'c'})},
                  {1, WireType::Unsigned, ByteValues({0x64, 0x00})}},
                 {}};
  const auto encoded = EncodeSnapshot(schema, input);
  const auto golden = ByteValues({
      0x50, 0x52, 0x53, 0x4e, 0x40, 0x30, 0x20, 0x10, 0x02, 0x00, 0x0c, 0x18,
      0x78, 0x8a, 0x9a, 0xde, 0x34, 0xa6, 0x2a, 0x00, 0x00, 0x00, 0x00, 0x00,
      0x00, 0x00, 0x02, 0x00, 0x01, 0x00, 0x01, 0x02, 0x00, 0x00, 0x00, 0x64,
      0x00, 0x07, 0x00, 0x03, 0x03, 0x00, 0x00, 0x00, 0x6e, 0x70, 0x63,
  });
  Require(encoded == golden, "snapshot golden byte vector changed");
  Snapshot decoded;
  Require(DecodeSnapshot(schema, encoded, decoded) == SnapshotError::None,
          "snapshot decode failed");
  Require(EncodeSnapshot(schema, decoded) == golden, "snapshot round trip was not deterministic");

  ReplicationSchema old_reader{
      0x10203040,
      1,
      {{1, WireType::Unsigned, FieldCompatibility::Required, Quantization{0, 1000, 10}, "hp"}}};
  Snapshot forwarded;
  Require(DecodeSnapshot(old_reader, encoded, forwarded) == SnapshotError::None,
          "older schema rejected an optional unknown field");
  Require(forwarded.unknown_fields.size() == 1 &&
              forwarded.unknown_fields.front() == input.fields.front(),
          "unknown field was not preserved byte-for-byte");

  for (std::size_t size = 0; size < encoded.size(); ++size) {
    Snapshot rejected;
    Require(DecodeSnapshot(schema, std::span{encoded}.first(size), rejected) != SnapshotError::None,
            "truncated snapshot was accepted");
  }
}

void TestDeltaCompression() {
  const auto schema = TestSchema();
  Snapshot baseline{
      100,
      {{1, WireType::Unsigned, ByteValues({10})}, {7, WireType::Bytes, ByteValues({'a'})}},
      {}};
  Snapshot current{
      101,
      {{1, WireType::Unsigned, ByteValues({11})}, {7, WireType::Bytes, ByteValues({'a'})}},
      {}};
  const auto delta = EncodeDelta(schema, current, &baseline);
  Require(!delta.full_snapshot && delta.baseline_id == baseline.id,
          "delta did not identify its baseline");
  Snapshot decoded;
  Require(DecodeDelta(schema, delta, &baseline, decoded) == DeltaError::None && decoded == current,
          "delta deterministic round trip failed");
  Require(DecodeDelta(schema, delta, nullptr, decoded) == DeltaError::BaselineMissing,
          "missing baseline did not request fallback");
  const auto full = EncodeDelta(schema, current, nullptr);
  Require(full.full_snapshot && DecodeDelta(schema, full, nullptr, decoded) == DeltaError::None &&
              decoded == current,
          "full snapshot fallback failed");

  for (std::size_t size = 0; size < delta.bytes.size(); ++size) {
    auto truncated = delta;
    truncated.bytes.resize(size);
    Require(DecodeDelta(schema, truncated, &baseline, decoded) != DeltaError::None,
            "truncated delta was accepted");
  }
  auto corrupt = delta;
  corrupt.bytes[0] ^= std::byte{0xff};
  Require(DecodeDelta(schema, corrupt, &baseline, decoded) == DeltaError::Malformed,
          "corrupt delta was accepted");

  BaselineStore store(2);
  store.Insert(baseline);
  store.Insert(current);
  store.Insert(Snapshot{102, current.fields, {}});
  Require(store.Find(100) == nullptr && store.Find(101) != nullptr,
          "expired baseline was retained");
}

class ExplicitInterestProvider final : public IInterestProvider {
public:
  std::unordered_map<ConnectionID, std::vector<NetworkEntityID>> subscriptions;

  InterestQuery Query(ConnectionID connection, std::size_t cursor,
                      std::size_t max_work) const override {
    const auto found = subscriptions.find(connection);
    if (found == subscriptions.end()) {
      return {{}, 0, true};
    }
    const auto end = std::min(found->second.size(), cursor + max_work);
    return {{found->second.begin() + static_cast<std::ptrdiff_t>(cursor),
             found->second.begin() + static_cast<std::ptrdiff_t>(end)},
            end,
            end == found->second.size()};
  }
};

void TestInterestManagement() {
  ExplicitInterestProvider provider;
  provider.subscriptions[1] = {{1, 1}, {2, 1}, {3, 1}};
  provider.subscriptions[2] = {{99, 1}};
  InterestManager manager;
  const auto partial = manager.Update(1, provider, 2);
  Require(!partial.complete && partial.spawn.empty() && partial.work <= 2,
          "interest update exceeded budget or exposed a partial scan");
  const auto entered = manager.Update(1, provider, 2);
  Require(entered.complete && entered.spawn.size() == 3 && entered.work <= 2,
          "interest enter semantics failed");
  Require(!manager.Contains(1, {99, 1}), "connection interest degraded into global replication");
  Require(manager.Update(2, provider, 1).spawn == std::vector<NetworkEntityID>{{99, 1}},
          "connection-scoped interest sets leaked");

  provider.subscriptions[1] = {{2, 1}};
  const auto changed = manager.Update(1, provider, 4);
  Require(changed.spawn.empty() && changed.despawn.size() == 2 && manager.Contains(1, {2, 1}) &&
              !manager.Contains(1, {1, 1}),
          "interest leave/despawn semantics failed");
  manager.Remove(1);
  Require(!manager.Contains(1, {2, 1}), "removed connection retained interest state");
}

void TestDormancy() {
  DormancyManager manager;
  constexpr NetworkEntityID entity{7, 2};
  Require(manager.Register(entity) == 1, "entity did not receive an initial dirty generation");

  auto first = manager.Evaluate(10, entity, true);
  Require(first.action == ReplicationAction::FullSnapshot && first.generation == 1 &&
              first.baseline_id == 0,
          "first interest entry did not force a full snapshot");
  Require(manager.Acknowledge(10, entity, 1, 100), "connection did not acknowledge baseline");
  Require(manager.Evaluate(10, entity, true).action == ReplicationAction::None,
          "acknowledged clean entity replicated again");

  manager.SetDormant(entity, true);
  Require(manager.IsDormant(entity) &&
              manager.Evaluate(10, entity, true).action == ReplicationAction::None,
          "clean dormant entity was not suppressed");
  Require(manager.MarkDirty(entity) == 2 && !manager.IsDormant(entity),
          "dirty entity did not wake from dormancy");
  const auto wake = manager.Evaluate(10, entity, true);
  Require(wake.action == ReplicationAction::Delta && wake.baseline_id == 100 &&
              wake.woke_from_dormancy,
          "dormant wake-up did not use the connection baseline");

  const auto other_connection = manager.Evaluate(20, entity, true);
  Require(other_connection.action == ReplicationAction::FullSnapshot,
          "acknowledgement leaked between connections");
  Require(!manager.Acknowledge(10, entity, 3, 101), "future generation was acknowledged");
  Require(manager.Acknowledge(10, entity, 2, 101), "wake-up generation was not acknowledged");

  Require(manager.Evaluate(10, entity, false).action == ReplicationAction::Despawn,
          "interest departure did not emit despawn");
  const auto reentry = manager.Evaluate(10, entity, true);
  Require(reentry.action == ReplicationAction::FullSnapshot && reentry.baseline_id == 0,
          "interest re-entry did not invalidate the old baseline");
}

void TestPredictionAndReplay() {
  PredictionBuffer client;
  const auto first = client.Push(2);
  const auto second = client.Push(2);
  const auto third = client.Push(-1);
  Require(first.sequence == 1 && second.sequence == 2 && third.sequence == 3,
          "client input sequences were not monotonic");
  Require(client.State() == PredictedState{9, 3}, "prediction simulation changed unexpectedly");

  Require(client.Reconcile({1, {1, 1}}), "authoritative correction was rejected");
  Require(client.Pending().size() == 2 && client.Pending().front().sequence == 2,
          "acknowledged input was not removed from the pending queue");
  Require(client.State() == PredictedState{6, 2},
          "authoritative correction did not replay pending input");
  Require(!client.Reconcile({0, {0, 0}}), "stale correction was accepted");

  ReplayLog capture;
  capture.RecordInput(10, first);
  capture.RecordInput(20, second);
  capture.RecordCorrection(35, {1, {1, 1}});
  capture.RecordInput(40, third);
  const auto encoded = capture.Encode();
  ReplayLog decoded;
  Require(ReplayLog::Decode(encoded, decoded) && decoded.Events() == capture.Events(),
          "network replay log did not round trip");
  Require(decoded.Replay() == PredictedState{1, 0},
          "replay did not reproduce the captured correction/input ordering");
  for (std::size_t size = 0; size < encoded.size(); ++size) {
    ReplayLog rejected;
    Require(!ReplayLog::Decode(std::span{encoded}.first(size), rejected),
            "truncated replay log was accepted");
  }

  // Delivery ticks model test latency explicitly; repeated capture playback must be bit-identical.
  ReplayLog delayed;
  PredictedState server;
  for (std::uint64_t sequence = 1; sequence <= 64; ++sequence) {
    const InputCommand input{sequence, static_cast<std::int32_t>(sequence % 5) - 2};
    delayed.RecordInput(sequence + 6, input);
    server = SimulateInput(server, input);
    if (sequence % 8 == 0) {
      delayed.RecordCorrection(sequence + 12, {sequence, server});
    }
  }
  const auto expected = delayed.Replay();
  for (int run = 0; run < 100; ++run) {
    ReplayLog copy;
    Require(ReplayLog::Decode(delayed.Encode(), copy) && copy.Replay() == expected,
            "simulation under test latency was not deterministic");
  }
}

} // namespace

int main() {
  TestLoopbackTransport();
  TestDeterministicSimulation();
  TestMalformedHandshakes();
  TestIdentityMismatchRejection();
  TestReconnectDisconnectStress();
  TestNetworkEntityMapping();
  TestReplicationSchemaAndSnapshot();
  TestDeltaCompression();
  TestInterestManagement();
  TestDormancy();
  TestPredictionAndReplay();
}
