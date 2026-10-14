#include "Nexora/Foundation/GameplayABI.h"
#include <cstdint>
#include <cstdio>

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
  std::uint64_t updates;
  std::uint32_t last_buttons{};
  std::uint64_t pointer_sequence{};
};
struct Position {
  double x, y, z;
};
int32_t Create(void **state, const NexoraGameplayHostV3 *host) {
  if (!host || !host->allocate || !host->deallocate || !host->read_component ||
      !host->write_component)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#if defined(NEXORA_FIXTURE_POINTER_MOTION)
  if (host->struct_size < NEXORA_GAMEPLAY_HOST_V3_POINTER_SIZE ||
      !(host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_POINTER_INPUT) ||
      !host->capture_pointer_input)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#endif
  auto *memory = host->allocate(host->context, 7, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  auto *created = static_cast<State *>(memory);
  created->host = *host;
  created->updates = 0;
  created->pointer_sequence = 0;
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
  NexoraFrameDiagnostics diagnostics{};
  if (!host.get_diagnostics ||
      host.get_diagnostics(host.context, &diagnostics) != NEXORA_GAMEPLAY_OK ||
      diagnostics.frame != 0 || diagnostics.scene_entities == 0 || diagnostics.debug_lines != 0)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  const char message[] = "Editor gameplay fixture started";
  host.log(host.context, 1, message, sizeof(message) - 1);
  return NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *opaque, double seconds) {
  auto &host = static_cast<State *>(opaque)->host;
#if defined(NEXORA_FIXTURE_POINTER_MOTION)
  static_cast<void>(seconds);
#endif
  Position pose{};
  const auto read = host.read_component(host.context, 20, TransformType(), &pose, sizeof(pose));
  if (read != NEXORA_GAMEPLAY_OK)
    return read;
#if defined(NEXORA_FIXTURE_POINTER_MOTION)
  auto &state = *static_cast<State *>(opaque);
  NexoraPointerInputSnapshotV1 input{}, repeat{};
  input.struct_size = repeat.struct_size = sizeof(input);
  NexoraInputSnapshot legacy{};
  if (NexoraCapturePointerInputV1(&host, 0, &input) != NEXORA_GAMEPLAY_OK ||
      NexoraCapturePointerInputV1(&host, 0, &repeat) != NEXORA_GAMEPLAY_OK ||
      host.capture_input(host.context, 0, &legacy) != NEXORA_GAMEPLAY_OK ||
      input.sequence != repeat.sequence || input.delta_x != repeat.delta_x ||
      input.delta_y != repeat.delta_y || input.wheel_x != repeat.wheel_x ||
      input.wheel_y != repeat.wheel_y || input.sequence != legacy.sequence)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  if (state.pointer_sequence != input.sequence) {
    state.pointer_sequence = input.sequence;
    // Displacement is already integrated over the input batch; never multiply by tick time.
    pose.x += input.delta_x * .01 + input.wheel_y / 600.0;
    pose.y -= input.delta_y * .01 + input.wheel_x / 600.0;
    if (input.delta_x || input.delta_y || input.wheel_x || input.wheel_y)
      std::fprintf(stderr, "pointer motion evidence: dx=%.0f dy=%.0f wx=%.0f wy=%.0f\n",
                   input.delta_x, input.delta_y, input.wheel_x, input.wheel_y);
  }
#elif defined(NEXORA_FIXTURE_INPUT_ONLY)
  NexoraInputSnapshot input{};
  if (!host.capture_input || host.capture_input(host.context, 0, &input) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#if defined(NEXORA_FIXTURE_POINTER_ONLY)
  auto &state = *static_cast<State *>(opaque);
  if (input.buttons != state.last_buttons) {
    std::fprintf(stderr, "pointer input buttons=%u\n", input.buttons);
    state.last_buttons = input.buttons;
  }
  pose.x += seconds * ((input.buttons & 14U) ? 1 : 0);
#else
  pose.x += seconds * input.move_x * 2;
#endif
#else
  pose.x += seconds * 0.5;
#endif
  return host.write_component(host.context, 20, TransformType(), &pose, sizeof(pose));
}
int32_t Update(void *opaque, double) {
#if defined(NEXORA_FIXTURE_POINTER_MOTION)
  // Some input batches have no fixed tick. Variable Update must consume them too;
  // sequence deduplication prevents a second application when Fixed already ran.
  const auto motion = Fixed(opaque, 0);
  if (motion != NEXORA_GAMEPLAY_OK)
    return motion;
#endif
  auto &state = *static_cast<State *>(opaque);
  NexoraFrameDiagnostics diagnostics{};
  if (state.host.get_diagnostics(state.host.context, &diagnostics) != NEXORA_GAMEPLAY_OK ||
      diagnostics.frame != ++state.updates || diagnostics.scene_entities == 0 ||
      diagnostics.debug_lines != 0)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  if (state.updates == 1)
    std::fprintf(stderr, "play diagnostics fixture evidence: frame=1 entities=%llu errors=%llu\n",
                 static_cast<unsigned long long>(diagnostics.scene_entities),
                 static_cast<unsigned long long>(diagnostics.api_errors));
  return NEXORA_GAMEPLAY_OK;
}
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
