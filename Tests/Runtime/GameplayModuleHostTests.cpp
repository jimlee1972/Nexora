#include "GameplayConformanceVectors.h"
#include "Nexora/Runtime/GameplayModuleHost.h"

#include <cassert>
#include <cstdint>
#include <cstring>
#include <limits>

namespace {

struct State final {
  std::uint32_t updates{};
  std::uint32_t fixed_updates{};
  double elapsed_seconds{};
  bool started{};
  bool stopped{};
};

State first_state;
State second_state;
bool fail_create{};
bool fail_start{};
bool fail_fixed_update{};
bool fail_update{};
bool fail_save{};
bool fail_load{};

int32_t InitializeFirst(void **state, const NexoraGameplayHostV3 *) {
  if (fail_create)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  *state = &first_state;
  return 0;
}
int32_t InitializeSecond(void **state, const NexoraGameplayHostV3 *) {
  *state = &second_state;
  return 0;
}
int32_t Start(void *state) {
  if (fail_start)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  static_cast<State *>(state)->started = true;
  return NEXORA_GAMEPLAY_OK;
}
int32_t FixedUpdate(void *state, double) {
  if (fail_fixed_update)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  ++static_cast<State *>(state)->fixed_updates;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *state, double delta_seconds) {
  if (fail_update)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  auto &value = *static_cast<State *>(state);
  ++value.updates;
  value.elapsed_seconds += delta_seconds;
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *state) { static_cast<State *>(state)->stopped = true; }
void Destroy(void *) {}
uint32_t SaveState(void *state, void *data, uint32_t size) {
  if (fail_save && data != nullptr)
    return 0;
  if (data != nullptr && size >= sizeof(State))
    std::memcpy(data, state, sizeof(State));
  return sizeof(State);
}
int32_t LoadState(void *state, const void *data, uint32_t size) {
  if (fail_load)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
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

enum class MissingCallback { Create, Start, Update, Stop, Destroy };
MissingCallback missing_callback{};

int32_t LoadMissingCallback(uint32_t requested, NexoraGameModuleV3 *module) {
  if (requested != NEXORA_GAMEPLAY_ABI_VERSION)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  Populate(module, InitializeFirst);
  switch (missing_callback) {
  case MissingCallback::Create:
    module->create = nullptr;
    break;
  case MissingCallback::Start:
    module->on_start = nullptr;
    break;
  case MissingCallback::Update:
    module->update = nullptr;
    break;
  case MissingCallback::Stop:
    module->on_stop = nullptr;
    break;
  case MissingCallback::Destroy:
    module->destroy = nullptr;
    break;
  }
  return NEXORA_GAMEPLAY_OK;
}

int32_t LoadUndersized(uint32_t, NexoraGameModuleV3 *module) {
  Populate(module, InitializeFirst);
  module->struct_size = sizeof(module->struct_size) + sizeof(module->abi_version);
  return NEXORA_GAMEPLAY_OK;
}

} // namespace

int main() {
  NexoraGameplayHostV3 api{};
  api.struct_size = sizeof(api);
  api.abi_version = NEXORA_GAMEPLAY_ABI_VERSION;
  nexora::runtime::GameplayModuleHost host(api);

  assert(!host.Update(0.016));
  assert(!host.Load(nullptr)); // Models a missing required loader symbol.
  assert(!host.Load(LoadInvalid));
  assert(!host.Load(LoadUndersized));
  for (const auto callback :
       {MissingCallback::Create, MissingCallback::Start, MissingCallback::Update,
        MissingCallback::Stop, MissingCallback::Destroy}) {
    missing_callback = callback;
    assert(!host.Load(LoadMissingCallback));
  }
  fail_create = true;
  assert(!host.Load(LoadFirst));
  fail_create = false;
  fail_start = true;
  assert(!host.Load(LoadFirst));
  fail_start = false;
  assert(host.Load(LoadFirst));
  assert(first_state.started);
  assert(!host.Load(LoadFirst));
  assert(nexora::test::RunGameplayConformanceVectors(host));
  assert(!host.Update(-1.0));
  assert(!host.Update(std::numeric_limits<double>::infinity()));
  assert(first_state.updates == nexora::test::kExpectedUpdates);
  assert(first_state.fixed_updates == nexora::test::kExpectedFixedUpdates);
  assert(first_state.elapsed_seconds == nexora::test::kExpectedElapsedSeconds);
  fail_update = true;
  assert(!host.Update(0.016));
  fail_update = false;
  fail_fixed_update = true;
  assert(!host.FixedUpdate(1.0 / 60.0));
  fail_fixed_update = false;
  assert(!host.FixedUpdate(0.0));

  assert(!host.Reload(LoadInvalid));
  assert(!first_state.stopped);
  fail_save = true;
  assert(!host.Reload(LoadSecond));
  fail_save = false;
  fail_load = true;
  assert(!host.Reload(LoadSecond));
  fail_load = false;
  assert(host.Reload(LoadSecond));
  assert(first_state.stopped);
  assert(second_state.updates == nexora::test::kExpectedUpdates);
  assert(host.Update(0.016));
  assert(second_state.updates == nexora::test::kExpectedUpdates + 1);
  const auto stats = host.GetReloadStats();
  assert(stats.successful_reloads == 1);
  assert(stats.migrated_bytes == sizeof(State));
  assert(stats.last_reload_duration.count() > 0);

  host.Unload();
  assert(second_state.stopped);
  assert(!host.IsLoaded());
  host.Unload();

  auto undersized_api = api;
  undersized_api.struct_size =
      sizeof(undersized_api.struct_size) + sizeof(undersized_api.abi_version);
  nexora::runtime::GameplayModuleHost undersized_host(undersized_api);
  assert(!undersized_host.Load(LoadFirst));
  auto incompatible_api = api;
  incompatible_api.abi_version += 1;
  nexora::runtime::GameplayModuleHost incompatible_host(incompatible_api);
  assert(!incompatible_host.Load(LoadFirst));
}
