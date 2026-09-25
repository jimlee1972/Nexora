#include "Nexora/Network/Transport.h"

#include <iostream>

int main() {
  auto transports = nexora::network::CreateSimulatedTransportPair();
  nexora::network::Connection server(*transports.server, {1, "nexora-dev"}, true);
  std::cout << "Nexora dedicated server ready (protocol 1, build nexora-dev)\n";
  return server.State() == nexora::network::ConnectionState::Disconnected ? 0 : 1;
}
