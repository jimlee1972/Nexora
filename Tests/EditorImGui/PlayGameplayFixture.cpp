#include "Nexora/Foundation/GameplayABI.h"
#include <cstdint>

namespace {
constexpr std::uint64_t TransformType() {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const auto character : "Nexora.Transform") {
    if (!character)
      break;
    hash = (hash ^ static_cast<unsigned char>(character)) * 1099511628211ULL;
  }
  return hash;
}
struct State {
  NexoraGameplayHostV3 host;
};
struct Position {
  double x, y, z;
};
int32_t Create(void **state, const NexoraGameplayHostV3 *host) {
  if (!host || !host->allocate || !host->deallocate || !host->read_component ||
      !host->write_component)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  auto *memory = host->allocate(host->context, 7, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  auto *created = static_cast<State *>(memory);
  created->host = *host;
  *state = created;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *opaque) {
  auto &host = static_cast<State *>(opaque)->host;
  const char message[] = "Editor gameplay fixture started";
  host.log(host.context, 1, message, sizeof(message) - 1);
  return NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *opaque, double seconds) {
  auto &host = static_cast<State *>(opaque)->host;
  Position pose{};
  const auto read = host.read_component(host.context, 20, TransformType(), &pose, sizeof(pose));
  if (read != NEXORA_GAMEPLAY_OK)
    return read;
#if defined(NEXORA_FIXTURE_INPUT_ONLY)
  NexoraInputSnapshot input{};
  if (!host.capture_input || host.capture_input(host.context, 0, &input) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  pose.x += seconds * input.move_x * 2;
#else
  pose.x += seconds * 0.5;
#endif
  return host.write_component(host.context, 20, TransformType(), &pose, sizeof(pose));
}
int32_t Update(void *, double) { return NEXORA_GAMEPLAY_OK; }
void Stop(void *) {}
void Destroy(void *opaque) {
  const auto host = static_cast<State *>(opaque)->host;
  host.deallocate(host.context, 7, opaque, sizeof(State), alignof(State));
}
} // namespace
#if defined(_WIN32)
#define NEXORA_FIXTURE_EXPORT __declspec(dllexport)
#else
#define NEXORA_FIXTURE_EXPORT __attribute__((visibility("default")))
#endif
extern "C" NEXORA_FIXTURE_EXPORT int32_t NexoraGameModuleLoad(uint32_t requested,
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
