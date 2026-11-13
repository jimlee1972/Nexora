#include "nexora/nexora.h"
#include <chrono>
#include <cstdio>
#include <new>
#include <thread>
namespace {
struct State {
  NexoraGameplayHostV3 host;
  unsigned frames{};
};
int32_t Create(void **out, const NexoraGameplayHostV3 *host) {
  if (!host || !host->allocate || !host->deallocate)
    return -2;
  void *memory = host->allocate(host->context, 1, sizeof(State), alignof(State));
  if (!memory)
    return -3;
  *out = new (memory) State{*host};
  return 0;
}
int32_t Start(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  NexoraTwoDApiV1 api{};
  api.struct_size = sizeof(api);
  const NexoraTwoDCamera camera{{0, 0}, 10, 0};
  NexoraGameplayServicesApiV1 services{};
  services.struct_size = sizeof(services);
  services.abi_version = 1;
  NexoraGameplayContentApiV1 content{};
  content.struct_size = sizeof(content);
  content.abi_version = 1;
  NexoraContentAsset asset{};
  if (!state.host.get_twod_api || state.host.get_twod_api(state.host.context, &api) ||
      api.set_camera(api.context, &camera) || NexoraGetGameplayServicesV1(&state.host, &services) ||
      services.get_content_api(services.context, &content) ||
      content.describe_asset(content.context, 81, 82, &asset) ||
      asset.kind != NEXORA_CONTENT_SPRITE_SOURCE || asset.atlas_high != 71 ||
      asset.atlas_low != 72 || asset.width != 4 || asset.height != 4)
    return -3;
  if (content.describe_asset(content.context, 83, 84, &asset) ||
      asset.kind != NEXORA_CONTENT_CLIP || asset.atlas_high || asset.atlas_low || asset.count != 1)
    return -3;
  return 0;
}
int32_t Update(void *opaque, double) {
  auto &state = *static_cast<State *>(opaque);
  if (++state.frames == 6) {
    std::fprintf(stderr, "AUTHORED SOURCE CAPTURE\n");
    std::fflush(stderr);
    // Hold the already presented previous frame while the native harness reads the window.
    std::this_thread::sleep_for(std::chrono::milliseconds(750));
  }
  if (state.frames == 15)
    std::fprintf(stderr, "AUTHORED SOURCE PASS\n");
  return 0;
}
void Stop(void *) {}
void Destroy(void *opaque) {
  auto *state = static_cast<State *>(opaque);
  const auto host = state->host;
  state->~State();
  host.deallocate(host.context, 1, state, sizeof(State), alignof(State));
}
} // namespace
#if defined(_WIN32)
#define AUTHORED_EXPORT __declspec(dllexport)
#else
#define AUTHORED_EXPORT __attribute__((visibility("default")))
#endif
extern "C" AUTHORED_EXPORT int32_t NexoraGameModuleLoad(uint32_t abi, NexoraGameModuleV3 *module) {
  if (abi != NEXORA_GAMEPLAY_ABI_VERSION || !module || module->struct_size < sizeof(*module))
    return -2;
  *module = {sizeof(*module), abi,    0,    nullptr, Create,  Start,
             nullptr,         Update, Stop, Destroy, nullptr, nullptr};
  return 0;
}
