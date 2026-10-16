#include "nexora/nexora.h"
#include <cstdio>
#include <new>

namespace {
struct State {
  NexoraGameplayHostV3 host;
  NexoraGameplayUiApiV1 ui{};
  NexoraStorageApiV1 storage{};
  unsigned char visits{};
};
int32_t Create(void **output, const NexoraGameplayHostV3 *host) {
  if (!host || !host->allocate || !host->deallocate)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  void *memory = host->allocate(host->context, 1, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  *output = new (memory) State{*host};
  return 0;
}
int32_t Start(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  NexoraGameplayServicesApiV1 services{};
  services.struct_size = sizeof(services);
  services.abi_version = 1;
  NexoraTwoDApiV1 twod{};
  twod.struct_size = sizeof(twod);
  NexoraGameplayInputApiV1 input{};
  input.struct_size = sizeof(input);
  input.abi_version = 1;
  NexoraGameplayAudioApiV1 audio{};
  audio.struct_size = sizeof(audio);
  audio.abi_version = 1;
  state.ui.struct_size = sizeof(state.ui);
  state.ui.abi_version = 1;
  state.storage.struct_size = sizeof(state.storage);
  state.storage.abi_version = 1;
  uint64_t scene{};
  if (state.host.load_scene(state.host.context, "Main", 4, 0, &scene) ||
      state.host.activate_scene(state.host.context, scene) ||
      state.host.get_twod_api(state.host.context, &twod) ||
      NexoraGetGameplayServicesV1(&state.host, &services) ||
      services.get_input_api(services.context, &input) ||
      services.get_audio_api(services.context, &audio) ||
      services.get_ui_api(services.context, &state.ui) ||
      services.get_storage_api(services.context, &state.storage))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraStorageInfoV1 info{sizeof(info), 1, 0, 0};
  const auto loaded =
      state.storage.load(state.storage.context, "visits", 6, &state.visits, 1, &info);
  if (loaded != NEXORA_STORAGE_NOT_FOUND &&
      (loaded != 0 || info.schema_version != 1 || info.payload_size != 1))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  std::fprintf(stderr, "project visits loaded=%u\n", static_cast<unsigned>(state.visits));
  ++state.visits;
  return 0;
}
int32_t Update(void *opaque, double) {
  auto &state = *static_cast<State *>(opaque);
  auto &ui = state.ui;
  // No sprites or pools: a UI-only game is a valid project, not the fixed sample fixture.
  if (ui.begin_frame(ui.context, 640, 480) ||
      ui.panel(ui.context, {16, 16, 128, 48}, 0xffdd6622U) || ui.end_frame(ui.context))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  return state.storage.save(state.storage.context, "visits", 6, 1, &state.visits, 1);
}
void Stop(void *) { std::fprintf(stderr, "project stopped\n"); }
void Destroy(void *opaque) {
  auto *state = static_cast<State *>(opaque);
  const auto host = state->host;
  state->~State();
  host.deallocate(host.context, 1, state, sizeof(State), alignof(State));
}
} // namespace
#if defined(_WIN32)
#define PROJECT_EXPORT __declspec(dllexport)
#else
#define PROJECT_EXPORT __attribute__((visibility("default")))
#endif
extern "C" PROJECT_EXPORT int32_t NexoraGameModuleLoad(uint32_t abi, NexoraGameModuleV3 *module) {
  if (abi != NEXORA_GAMEPLAY_ABI_VERSION || !module || module->struct_size < sizeof(*module))
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {sizeof(*module), abi,    0,    nullptr, Create,  Start,
             nullptr,         Update, Stop, Destroy, nullptr, nullptr};
  return 0;
}
