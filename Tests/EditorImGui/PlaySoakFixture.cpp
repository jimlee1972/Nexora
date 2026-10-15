#include "Nexora/Foundation/GameplayABI.h"
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace {
constexpr std::uint64_t TransformType() {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const auto c : "Nexora.Transform") {
    if (!c)
      break;
    hash = (hash ^ static_cast<unsigned char>(c)) * 1099511628211ULL;
  }
  return hash;
}
struct Position {
  double x, y, z;
};
struct State {
  NexoraGameplayHostV3 host;
  std::uint64_t sequence{}, ticks{};
  // Touch every page: repeated native lifecycles exercise real host allocation residency.
  unsigned char scratch[4 * 1024 * 1024];
};
bool Neutral(const NexoraGameplayHostV3 &host) {
  NexoraInputSnapshot legacy{};
  NexoraPointerInputSnapshotV1 pointer{};
  pointer.struct_size = sizeof(pointer);
  Position pose{};
  return host.capture_input(host.context, 0, &legacy) == NEXORA_GAMEPLAY_OK &&
         NexoraCapturePointerInputV1(&host, 0, &pointer) == NEXORA_GAMEPLAY_OK &&
         host.read_component(host.context, 20, TransformType(), &pose, sizeof(pose)) ==
             NEXORA_GAMEPLAY_OK &&
         !legacy.move_x && !legacy.move_y && !legacy.buttons && !pointer.delta_x &&
         !pointer.delta_y && !pointer.wheel_x && !pointer.wheel_y && !pointer.buttons &&
         pointer.sequence == legacy.sequence;
}
int32_t Create(void **output, const NexoraGameplayHostV3 *host) {
  if (!host || host->struct_size < NEXORA_GAMEPLAY_HOST_V3_POINTER_SIZE ||
      host->abi_version != NEXORA_GAMEPLAY_ABI_VERSION || !host->allocate || !host->deallocate ||
      !host->capture_input || !host->read_component || !host->write_component ||
      !(host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_POINTER_INPUT) ||
      !host->capture_pointer_input || !Neutral(*host))
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  auto *state =
      static_cast<State *>(host->allocate(host->context, 7, sizeof(State), alignof(State)));
  if (!state)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  state->host = *host;
  state->sequence = state->ticks = 0;
  std::memset(state->scratch, 0xa5, sizeof(state->scratch));
  *output = state;
  std::fprintf(stderr, "soak lifecycle create bytes=%zu\n", sizeof(State));
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *opaque) {
  const auto &host = static_cast<State *>(opaque)->host;
  Position pose{};
  if (!Neutral(host) ||
      host.read_component(host.context, 20, TransformType(), &pose, sizeof(pose)) || pose.x ||
      pose.y || pose.z)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  std::fprintf(stderr, "soak lifecycle start fresh=1 neutral=1\n");
  return NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *opaque, double) {
  auto &state = *static_cast<State *>(opaque);
  std::fprintf(stderr, "soak fixed tick=%llu neutral=%d\n",
               static_cast<unsigned long long>(++state.ticks), Neutral(state.host));
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *opaque, double) {
  auto &state = *static_cast<State *>(opaque);
  auto &host = state.host;
  NexoraPointerInputSnapshotV1 pointer{};
  pointer.struct_size = sizeof(pointer);
  if (NexoraCapturePointerInputV1(&host, 0, &pointer) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  if (pointer.sequence == state.sequence)
    return NEXORA_GAMEPLAY_OK;
  state.sequence = pointer.sequence;
  if (!pointer.delta_x && !pointer.delta_y && !pointer.wheel_x && !pointer.wheel_y)
    return NEXORA_GAMEPLAY_OK;
  Position pose{};
  if (host.read_component(host.context, 20, TransformType(), &pose, sizeof(pose)))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  pose.x += pointer.delta_x * .01 + pointer.wheel_y / 600;
  pose.y -= pointer.delta_y * .01 + pointer.wheel_x / 600;
  if (host.write_component(host.context, 20, TransformType(), &pose, sizeof(pose)))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  std::fprintf(stderr, "soak applied dx=%.0f dy=%.0f wx=%.0f wy=%.0f\n", pointer.delta_x,
               pointer.delta_y, pointer.wheel_x, pointer.wheel_y);
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *opaque) {
  std::fprintf(stderr, "soak lifecycle stop neutral=%d\n",
               Neutral(static_cast<State *>(opaque)->host));
}
void Destroy(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  bool pages = true;
  for (std::size_t page = 0; page < sizeof(state.scratch); page += 4096)
    pages &= state.scratch[page] == 0xa5;
  std::fprintf(stderr, "soak lifecycle destroy neutral=%d pages=%d\n", Neutral(state.host), pages);
  const auto host = state.host;
  host.deallocate(host.context, 7, opaque, sizeof(State), alignof(State));
}
} // namespace
#if defined(_WIN32)
#define NEXORA_SOAK_EXPORT __declspec(dllexport)
#else
#define NEXORA_SOAK_EXPORT __attribute__((visibility("default")))
#endif
extern "C" NEXORA_SOAK_EXPORT int32_t NexoraGameModuleLoad(uint32_t requested,
                                                           NexoraGameModuleV3 *module) {
  if (!module || requested != NEXORA_GAMEPLAY_ABI_VERSION)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = requested;
  module->capabilities = NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE;
  module->create = Create;
  module->on_start = Start;
  module->fixed_update = Fixed;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
