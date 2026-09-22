#include "Nexora/Runtime/GameplayModuleHost.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

struct State final {
  std::uint32_t updates{};
  std::uint32_t fixed_updates{};
  bool started{};
  bool stopped{};
};

State first_state;
State second_state;

int32_t InitializeFirst(void **state, const NexoraGameplayHostV3 *) {
  *state = &first_state;
  return 0;
}
int32_t InitializeSecond(void **state, const NexoraGameplayHostV3 *) {
  *state = &second_state;
  return 0;
}
int32_t Start(void *state) {
  static_cast<State *>(state)->started = true;
  return NEXORA_GAMEPLAY_OK;
}
int32_t FixedUpdate(void *state, double) {
  ++static_cast<State *>(state)->fixed_updates;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *state, double) {
  ++static_cast<State *>(state)->updates;
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *state) { static_cast<State *>(state)->stopped = true; }
void Destroy(void *) {}
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

int32_t Populate(NexoraGameModuleV3 *module,
                 int32_t (*initialize)(void **, const NexoraGameplayHostV3 *)) {
  module->struct_size = sizeof(*module);
  module->abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  module->capabilities =
      NEXORA_GAMEPLAY_CAPABILITY_STATE_MIGRATION | NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE;
  module->create = initialize;
  module->on_start = Start;
  module->fixed_update = FixedUpdate;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  module->save_state = SaveState;
  module->load_state = LoadState;
  return 0;
}

int32_t LoadFirst(uint32_t requested, NexoraGameModuleV3 *module) {
  return requested == NEXORA_GAMEPLAY_ABI_VERSION ? Populate(module, InitializeFirst) : -1;
}
int32_t LoadSecond(uint32_t requested, NexoraGameModuleV3 *module) {
  return requested == NEXORA_GAMEPLAY_ABI_VERSION ? Populate(module, InitializeSecond) : -1;
}
int32_t LoadInvalid(uint32_t, NexoraGameModuleV3 *module) {
  module->struct_size = sizeof(*module);
  module->abi_version = NEXORA_GAMEPLAY_ABI_VERSION + 1;
  return 0;
}

} // namespace

int main() {
  NexoraGameplayHostV3 api{};
  api.struct_size = sizeof(api);
  api.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  nexora::runtime::GameplayModuleHost host(api);

  assert(!host.Update(0.016));
  assert(!host.Load(LoadInvalid));
  assert(host.Load(LoadFirst));
  assert(first_state.started);
  assert(!host.Load(LoadFirst));
  assert(host.Update(0.016));
  assert(!host.Update(-1.0));
  assert(!host.Update(std::numeric_limits<double>::infinity()));
  assert(first_state.updates == 1);
  assert(host.FixedUpdate(1.0 / 60.0));
  assert(!host.FixedUpdate(0.0));
  assert(first_state.fixed_updates == 1);

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
