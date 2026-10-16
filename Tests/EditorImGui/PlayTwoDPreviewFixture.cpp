#include "Nexora/Foundation/GameplayABI.h"
#include <cstdint>
#include <cstdio>
#include <new>

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
  NexoraGameplayHostV3 host{};
  NexoraTwoDApiV1 api{};
  NexoraTwoDInstanceHandle pool{};
};
int32_t Create(void **output, const NexoraGameplayHostV3 *host) {
  if (!host || host->struct_size < NEXORA_GAMEPLAY_HOST_V3_2D_SIZE ||
      !(host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API) || !host->get_twod_api ||
      !host->allocate || !host->deallocate)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  auto *memory = host->allocate(host->context, 91, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  auto *state = new (memory) State{};
  state->host = *host;
  state->api.struct_size = sizeof(state->api);
  if (host->get_twod_api(host->context, &state->api)) {
    state->~State();
    host->deallocate(host->context, 91, memory, sizeof(State), alignof(State));
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  }
  *output = state;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *opaque) {
  auto &s = *static_cast<State *>(opaque);
  const NexoraTwoDSprite sprite{17, 23, 1, 8, {4, 4}, {.5F, .5F}, {1, 1, 1, 1}, 0, 0};
  auto pool_sprite = sprite;
  pool_sprite.atlas_high = 31;
  pool_sprite.atlas_low = 37;
  pool_sprite.color[3] = .5F;
  const NexoraTwoDInstance instance{{{1, 0, 0}, {0, 0, 0, 1}, {1, 1, 1}}, pool_sprite};
  const NexoraTwoDCamera camera{{0, 0}, 10, 0};
  if (s.api.set_sprite(s.api.context, 3, &sprite) ||
      s.api.create_instances(s.api.context, &instance, 1, &s.pool) ||
      s.api.set_camera(s.api.context, &camera) || s.api.play_animation(s.api.context, 3, 19, 29, 1))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  std::fprintf(stderr, "2d fixture start\n");
  return NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *opaque, double seconds) {
  auto &s = *static_cast<State *>(opaque);
  NexoraTwoDEvent event{};
  NexoraTwoDAnimationResult result{};
  return s.api.advance_animation(s.api.context, 3, seconds * 6, &event, 1, &result);
}
int32_t Update(void *opaque, double seconds) {
  auto &s = *static_cast<State *>(opaque);
  NexoraInputSnapshot input{};
  Position position{};
  if (s.host.capture_input(s.host.context, 0, &input) ||
      s.host.read_component(s.host.context, 3, TransformType(), &position, sizeof(position)))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  position.x += input.move_x * seconds * 4;
  if (input.move_x)
    std::fprintf(stderr, "2d fixture move x=%.9f\n", position.x);
  return s.host.write_component(s.host.context, 3, TransformType(), &position, sizeof(position));
}
void Stop(void *opaque) {
  auto &s = *static_cast<State *>(opaque);
  NexoraTwoDSprite sprite{};
  std::fprintf(stderr, "2d fixture stop live=%d\n",
               s.api.get_sprite(s.api.context, 3, &sprite) == NEXORA_GAMEPLAY_OK);
}
void Destroy(void *opaque) {
  auto &s = *static_cast<State *>(opaque);
  NexoraTwoDInstance instance{};
  std::fprintf(stderr, "2d fixture destroy live=%d\n",
               s.api.read_instances(s.api.context, &s.pool, 1, &instance) == NEXORA_GAMEPLAY_OK);
  const auto host = s.host;
  s.~State();
  host.deallocate(host.context, 91, opaque, sizeof(State), alignof(State));
}
} // namespace
#if defined(_WIN32)
#define NEXORA_PREVIEW_EXPORT __declspec(dllexport)
#else
#define NEXORA_PREVIEW_EXPORT __attribute__((visibility("default")))
#endif
extern "C" NEXORA_PREVIEW_EXPORT int32_t NexoraGameModuleLoad(std::uint32_t version,
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
