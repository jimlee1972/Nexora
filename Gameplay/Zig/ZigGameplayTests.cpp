#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Runtime/GameplayModuleHost.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string_view>

extern "C" int32_t NexoraGameModuleLoad(uint32_t requested_abi, NexoraGameModuleV1 *module);
extern "C" uint32_t NexoraGameModuleUpdateCount();
extern "C" double NexoraGameModuleElapsedSeconds();

namespace {

void Log(void *context, uint32_t, const char *message, uint32_t length) {
  auto &received = *static_cast<bool *>(context);
  received = std::string_view(message, length) == "Zig gameplay initialized";
}

} // namespace

int main() {
  bool received_log = false;
  NexoraGameplayHostV1 api{sizeof(NexoraGameplayHostV1), NEXORA_GAMEPLAY_ABI_VERSION, &received_log,
                           Log};
  nexora::runtime::GameplayModuleHost host(api);

  assert(host.Load(NexoraGameModuleLoad));
  assert(received_log);
  assert(host.Update(0.25));
  assert(host.Update(0.5));
  assert(NexoraGameModuleUpdateCount() == 2);
  assert(std::abs(NexoraGameModuleElapsedSeconds() - 0.75) < 0.000001);
  host.Unload();
}
