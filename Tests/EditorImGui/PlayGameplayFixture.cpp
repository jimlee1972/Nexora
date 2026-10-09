#include "Nexora/Foundation/GameplayABI.h"
#include <cstdint>
#if defined(NEXORA_FIXTURE_SCENES)
#include <cstdio>
#endif

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
#if defined(NEXORA_FIXTURE_SCENES)
  if (!(host.capabilities & NEXORA_GAMEPLAY_CAPABILITY_SCENE_API) || !host.load_scene ||
      !host.activate_scene || !host.spawn_entity || !host.despawn_entity)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  std::uint64_t scene{}, entity{};
  NexoraEntitySpawnDescriptor descriptor{};
  descriptor.struct_size = sizeof(descriptor);
  descriptor.components = NEXORA_SPAWN_LIGHT;
  descriptor.position = {1, 2, 3};
  descriptor.light_intensity = 2.5F;
#if defined(NEXORA_FIXTURE_PHYSICS)
  descriptor.components |= NEXORA_SPAWN_PHYSICS;
  descriptor.bounds_minimum = {-1, -1, -1};
  descriptor.bounds_maximum = {1, 1, 1};
  if (!host.raycast)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#endif
  constexpr char name[] = "Dynamic Play scene";
  if (host.load_scene(host.context, name, sizeof(name) - 1, 0, &scene) != NEXORA_GAMEPLAY_OK ||
      host.activate_scene(host.context, scene) != NEXORA_GAMEPLAY_OK ||
      host.spawn_entity(host.context, scene, &descriptor, &entity) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
#if defined(NEXORA_FIXTURE_PHYSICS)
  const NexoraRaycastRequest request{{1, 2, 6}, {0, 0, -1}, 10};
  NexoraRaycastHit hit{};
  if (host.raycast(host.context, &request, &hit) != NEXORA_GAMEPLAY_OK || hit.entity != entity ||
      hit.distance != 2 || hit.point.x != 1 || hit.point.y != 2 || hit.point.z != 4)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  const auto retained_hit = hit;
#endif
  if (host.despawn_entity(host.context, entity) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
#if defined(NEXORA_FIXTURE_PHYSICS)
  if (host.raycast(host.context, &request, &hit) != NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT ||
      hit.entity != retained_hit.entity || hit.distance != retained_hit.distance ||
      hit.point.x != retained_hit.point.x || hit.point.y != retained_hit.point.y ||
      hit.point.z != retained_hit.point.z)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  std::fprintf(stderr,
               "play physics fixture evidence: distance=%.0f point_z=%.0f entity=%llu "
               "despawn_miss=1\n",
               retained_hit.distance, retained_hit.point.z,
               static_cast<unsigned long long>(retained_hit.entity));
#endif
  std::fprintf(stderr, "play scene fixture evidence: loaded=1 activated=1 spawned=1 despawned=1\n");
#endif
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
