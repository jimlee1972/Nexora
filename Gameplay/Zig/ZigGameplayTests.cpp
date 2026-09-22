#include "Nexora/Foundation/GameplayABI.h"
#include "Nexora/Runtime/GameplayModuleHost.h"

#include <cassert>
#include <cmath>
#include <cstdint>
#include <string_view>

extern "C" int32_t NexoraGameModuleLoad(uint32_t requested_abi, NexoraGameModuleV3 *module);
extern "C" uint32_t NexoraGameModuleUpdateCount();
extern "C" double NexoraGameModuleElapsedSeconds();

namespace {

struct HostState final {
  bool received_log{};
  std::uint32_t component_value{41};
};

void Log(void *context, uint32_t, const char *message, uint32_t length) {
  auto &state = *static_cast<HostState *>(context);
  state.received_log = std::string_view(message, length) == "Zig gameplay initialized";
}

int32_t ReadComponent(void *context, uint64_t entity, uint64_t type, void *data, uint32_t size) {
  if (entity != 1 || type != 0x2001 || size != sizeof(std::uint32_t))
    return -1;
  *static_cast<std::uint32_t *>(data) = static_cast<HostState *>(context)->component_value;
  return 0;
}

int32_t WriteComponent(void *context, uint64_t entity, uint64_t type, const void *data,
                       uint32_t size) {
  if (entity != 1 || type != 0x2001 || size != sizeof(std::uint32_t))
    return -1;
  static_cast<HostState *>(context)->component_value = *static_cast<const std::uint32_t *>(data);
  return 0;
}

} // namespace

int main() {
  HostState state;
  NexoraGameplayHostV3 api{sizeof(NexoraGameplayHostV3),
                           NEXORA_GAMEPLAY_ABI_VERSION,
                           0,
                           &state,
                           Log,
                           ReadComponent,
                           WriteComponent};
  nexora::runtime::GameplayModuleHost host(api);

  assert(host.Load(NexoraGameModuleLoad));
  assert(state.received_log);
  assert(host.FixedUpdate(1.0 / 60.0));
  assert(host.Update(0.25));
  assert(host.Update(0.5));
  assert(NexoraGameModuleUpdateCount() == 2);
  assert(std::abs(NexoraGameModuleElapsedSeconds() - 0.75) < 0.000001);
  assert(state.component_value == 43);
  assert(host.Reload(NexoraGameModuleLoad));
  assert(NexoraGameModuleUpdateCount() == 2);
  assert(host.GetReloadStats().migrated_bytes > 0);
  host.Unload();
}
