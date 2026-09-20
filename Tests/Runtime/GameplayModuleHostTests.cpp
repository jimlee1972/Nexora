#include "Nexora/Runtime/GameplayModuleHost.h"

#include <cassert>
#include <cstdint>
#include <limits>

namespace {

struct State final {
  std::uint32_t updates{};
  bool stopped{};
};

State first_state;
State second_state;

int32_t InitializeFirst(void **state, const NexoraGameplayHostV1 *) {
  *state = &first_state;
  return 0;
}
int32_t InitializeSecond(void **state, const NexoraGameplayHostV1 *) {
  *state = &second_state;
  return 0;
}
void Update(void *state, double) { ++static_cast<State *>(state)->updates; }
void Shutdown(void *state) { static_cast<State *>(state)->stopped = true; }

int32_t Populate(NexoraGameModuleV1 *module,
                 int32_t (*initialize)(void **, const NexoraGameplayHostV1 *)) {
  module->struct_size = sizeof(*module);
  module->abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  module->initialize = initialize;
  module->update = Update;
  module->shutdown = Shutdown;
  return 0;
}

int32_t LoadFirst(uint32_t requested, NexoraGameModuleV1 *module) {
  return requested == NEXORA_GAMEPLAY_ABI_VERSION ? Populate(module, InitializeFirst) : -1;
}
int32_t LoadSecond(uint32_t requested, NexoraGameModuleV1 *module) {
  return requested == NEXORA_GAMEPLAY_ABI_VERSION ? Populate(module, InitializeSecond) : -1;
}
int32_t LoadInvalid(uint32_t, NexoraGameModuleV1 *module) {
  module->struct_size = sizeof(*module);
  module->abi_version = NEXORA_GAMEPLAY_ABI_VERSION + 1;
  return 0;
}

} // namespace

int main() {
  NexoraGameplayHostV1 api{sizeof(NexoraGameplayHostV1), NEXORA_GAMEPLAY_ABI_VERSION, nullptr,
                           nullptr};
  nexora::runtime::GameplayModuleHost host(api);

  assert(!host.Update(0.016));
  assert(!host.Load(LoadInvalid));
  assert(host.Load(LoadFirst));
  assert(!host.Load(LoadFirst));
  assert(host.Update(0.016));
  assert(!host.Update(-1.0));
  assert(!host.Update(std::numeric_limits<double>::infinity()));
  assert(first_state.updates == 1);

  assert(!host.Reload(LoadInvalid));
  assert(!first_state.stopped);
  assert(host.Reload(LoadSecond));
  assert(first_state.stopped);
  assert(host.Update(0.016));
  assert(second_state.updates == 1);

  host.Unload();
  assert(second_state.stopped);
  assert(!host.IsLoaded());
  host.Unload();
}
