#include "Nexora/Runtime/GameplayModuleHost.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

struct State final {
  std::uint32_t updates{};
  bool stopped{};
};

State first_state;
State second_state;

int32_t InitializeFirst(void **state, const NexoraGameplayHostV2 *) {
  *state = &first_state;
  return 0;
}
int32_t InitializeSecond(void **state, const NexoraGameplayHostV2 *) {
  *state = &second_state;
  return 0;
}
void Update(void *state, double) { ++static_cast<State *>(state)->updates; }
void Shutdown(void *state) { static_cast<State *>(state)->stopped = true; }
uint32_t SaveState(void *state, void *data, uint32_t size) {
  if (data != nullptr && size >= sizeof(State))
    std::memcpy(data, state, sizeof(State));
  return sizeof(State);
}
int32_t LoadState(void *state, const void *data, uint32_t size) {
  if (size != sizeof(State))
    return -1;
  std::memcpy(state, data, sizeof(State));
  return 0;
}

int32_t Populate(NexoraGameModuleV2 *module,
                 int32_t (*initialize)(void **, const NexoraGameplayHostV2 *)) {
  module->struct_size = sizeof(*module);
  module->abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  module->initialize = initialize;
  module->update = Update;
  module->shutdown = Shutdown;
  module->save_state = SaveState;
  module->load_state = LoadState;
  return 0;
}

int32_t LoadFirst(uint32_t requested, NexoraGameModuleV2 *module) {
  return requested == NEXORA_GAMEPLAY_ABI_VERSION ? Populate(module, InitializeFirst) : -1;
}
int32_t LoadSecond(uint32_t requested, NexoraGameModuleV2 *module) {
  return requested == NEXORA_GAMEPLAY_ABI_VERSION ? Populate(module, InitializeSecond) : -1;
}
int32_t LoadInvalid(uint32_t, NexoraGameModuleV2 *module) {
  module->struct_size = sizeof(*module);
  module->abi_version = NEXORA_GAMEPLAY_ABI_VERSION + 1;
  return 0;
}

} // namespace

int main() {
  NexoraGameplayHostV2 api{};
  api.struct_size = sizeof(api);
  api.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
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
  assert(second_state.updates == 1);
  assert(host.Update(0.016));
  assert(second_state.updates == 2);
  const auto stats = host.GetReloadStats();
  assert(stats.successful_reloads == 1);
  assert(stats.migrated_bytes == sizeof(State));
  assert(stats.last_reload_duration.count() > 0);

  host.Unload();
  assert(second_state.stopped);
  assert(!host.IsLoaded());
  host.Unload();
}
