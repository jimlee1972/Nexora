#include "Nexora/Network/Transport.h"

#include <cassert>
#include <cstddef>
#include <string_view>

using namespace nexora::network;

namespace {

void Advance(Connection &client, Connection &server, std::uint64_t time) {
  client.Tick(time);
  server.Tick(time);
  client.Tick(time);
}

std::span<const std::byte> Bytes(std::string_view text) {
  return {reinterpret_cast<const std::byte *>(text.data()), text.size()};
}

} // namespace

int main() {
  {
    auto pair = CreateSimulatedTransportPair({0, 20, 5, 42}, {0, 20, 5, 84});
    Connection client(*pair.client, {7, "build-a"}, false);
    Connection server(*pair.server, {7, "build-a"}, true);
    assert(client.Connect());
    Advance(client, server, 100);
    Advance(client, server, 200);
    assert(client.State() == ConnectionState::Connected);
    assert(server.State() == ConnectionState::Connected);
    assert(client.Send(2, ChannelSemantics::ReliableOrdered, Bytes("hello")));
    Advance(client, server, 300);
    Packet packet;
    assert(server.Poll(packet));
    assert(packet.channel == 2);
    assert(packet.payload == std::vector<std::byte>(Bytes("hello").begin(), Bytes("hello").end()));
    client.Disconnect();
    Advance(client, server, 400);
    assert(server.State() == ConnectionState::Disconnected);
  }
  {
    auto pair = CreateSimulatedTransportPair();
    Connection client(*pair.client, {8, "build-a"}, false);
    Connection server(*pair.server, {7, "build-a"}, true);
    assert(client.Connect());
    Advance(client, server, 0);
    assert(client.State() == ConnectionState::Rejected);
    assert(client.Rejection() == RejectReason::ProtocolMismatch);
  }
  {
    auto pair = CreateSimulatedTransportPair({100, 0, 0, 1});
    Packet packet{1, ChannelSemantics::Unreliable, 1, {std::byte{1}}};
    assert(pair.client->Send(packet));
    pair.server->Advance(1000);
    assert(!pair.server->Poll(packet));
  }
}
