#include "Nexora/Foundation/GameplayABI.h"
#include <cmath>
#include <cstdint>
#include <cstdio>

namespace {
struct State {
  NexoraGameplayHostV3 host;
  std::uint64_t ticks{}, updates{};
  bool stopped{};
};
int32_t Draw(State &state) {
  const double x = std::sin(static_cast<double>(state.ticks) * 0.07) * 0.8;
  const NexoraDebugLine line{{x - 1, 0, 0}, {x + 1, 0, 0}, 0x00ff00ff, 0};
  return state.host.debug_draw_line(state.host.context, &line);
}
int32_t Create(void **out, const NexoraGameplayHostV3 *host) {
  if (!out || !host || !host->debug_draw_line || !host->get_diagnostics || !host->allocate ||
      !host->deallocate || !host->despawn_entity || !host->log)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  auto *memory = host->allocate(host->context, 7, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  *static_cast<State *>(memory) = State{*host};
  *out = memory;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  // Remove the only cloned mesh: native green pixels must come from the debug callback.
  if (state.host.despawn_entity(state.host.context, 20) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  constexpr char message[] = "Play debug line fixture started without meshes";
  state.host.log(state.host.context, 1, message, sizeof(message) - 1);
  return Draw(state);
}
int32_t Fixed(void *opaque, double) {
  auto &state = *static_cast<State *>(opaque);
  ++state.ticks;
  return Draw(state);
}
int32_t Update(void *opaque, double) {
  auto &state = *static_cast<State *>(opaque);
  ++state.updates;
  if (Draw(state) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraFrameDiagnostics diagnostics{};
  if (state.host.get_diagnostics(state.host.context, &diagnostics) != NEXORA_GAMEPLAY_OK ||
      diagnostics.frame != state.updates || diagnostics.scene_entities != 1 ||
      diagnostics.debug_lines == 0 || diagnostics.api_errors != 0)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  if (state.updates == 1) {
    std::fprintf(stderr, "play diagnostics fixture evidence: frame=1 entities=1 errors=0\n");
    std::fprintf(stderr, "play debug line fixture evidence: meshes=0 lines=%llu\n",
                 static_cast<unsigned long long>(diagnostics.debug_lines));
  }
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  state.stopped = Draw(state) == NEXORA_GAMEPLAY_OK;
}
void Destroy(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  const bool drawn = Draw(state) == NEXORA_GAMEPLAY_OK;
  std::fprintf(stderr, "play debug line fixture teardown: stop=%d destroy=%d\n", state.stopped,
               drawn);
  const auto host = state.host;
  host.deallocate(host.context, 7, opaque, sizeof(State), alignof(State));
}
} // namespace
#if defined(_WIN32)
#define NEXORA_FIXTURE_EXPORT __declspec(dllexport)
#else
#define NEXORA_FIXTURE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" NEXORA_FIXTURE_EXPORT int32_t NexoraGameModuleLoad(std::uint32_t version,
                                                              NexoraGameModuleV3 *module) {
  if (!module || version != NEXORA_GAMEPLAY_ABI_VERSION)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->capabilities = NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE;
  module->create = Create;
  module->on_start = Start;
  module->fixed_update = Fixed;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
