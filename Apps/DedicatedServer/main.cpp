#include "Nexora/Network/ServerRuntime.h"

#include <chrono>
#include <iostream>
#include <vector>

namespace {

class EmptySimulation final : public nexora::network::IServerSimulation {
public:
  void Receive(nexora::network::ServerClientID, const nexora::network::Packet &) override {}
  void FixedTick(std::uint64_t tick) override { tick_ = tick; }
  void CaptureCanonicalState(std::vector<std::byte> &output) const override {
    output = {static_cast<std::byte>(tick_ & 0xffU)};
  }

private:
  std::uint64_t tick_{};
};

} // namespace

int main() {
  EmptySimulation simulation;
  nexora::network::ServerRuntime server({}, simulation);
  server.Advance(std::chrono::milliseconds(17));
  std::cout << "Nexora dedicated server ready (protocol 1, build nexora-dev)\n";
  server.RequestShutdown();
  return server.State() == nexora::network::ServerRunState::Stopped ? 0 : 1;
}
