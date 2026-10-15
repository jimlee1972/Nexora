#include "nexora/nexora.h"
#include <cstdint>
#include <cstdio>
#include <new>

namespace {
struct State {
  NexoraGameplayHostV3 host;
  std::uint64_t scene{}, entity{}, fixed{}, updates{};
};
constexpr std::uint64_t TransformType() {
  std::uint64_t result = 14695981039346656037ULL;
  for (const char value : "Nexora.Transform")
    if (value)
      result = (result ^ static_cast<unsigned char>(value)) * 1099511628211ULL;
  return result;
}
int32_t Create(void **output, const NexoraGameplayHostV3 *host) {
  if (!host || !host->allocate || !host->load_scene || !host->spawn_entity || host->get_twod_api ||
      (host->capabilities & NEXORA_GAMEPLAY_CAPABILITY_2D_API))
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  void *memory = host->allocate(host->context, 1, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  *output = new (memory) State{*host};
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  auto &host = state.host;
  NexoraEntitySpawnDescriptor descriptor{};
  descriptor.struct_size = sizeof(descriptor);
  descriptor.position = {1, 2, 3};
  if (host.load_scene(host.context, "logic", 5, 0, &state.scene) ||
      host.activate_scene(host.context, state.scene) ||
      host.spawn_entity(host.context, state.scene, &descriptor, &state.entity))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraVec3 position{};
  if (host.read_component(host.context, state.entity, TransformType(), &position,
                          sizeof(position)) ||
      position.x != 1 || position.y != 2 || position.z != 3)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  position.x = 4;
  if (host.write_component(host.context, state.entity, TransformType(), &position,
                           sizeof(position)))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  // Existing authored ID 4 is the first visible mesh of the native package fixture.
  if (host.read_component(host.context, 4, TransformType(), &position, sizeof(position)))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  position.x += 0.25;
  if (host.write_component(host.context, 4, TransformType(), &position, sizeof(position)))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraGameplayServicesApiV1 services{};
  services.struct_size = sizeof(services);
  services.abi_version = 1;
  NexoraGameplayInputApiV1 input{};
  input.struct_size = sizeof(input);
  input.abi_version = 1;
  if (NexoraGetGameplayServicesV1(&host, &services) ||
      services.get_input_api(services.context, &input))
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  NexoraStorageApiV1 storage{};
  storage.struct_size = sizeof(storage);
  storage.abi_version = 1;
  const auto storage_result = services.get_storage_api(services.context, &storage);
  if (storage_result == 0) {
    constexpr char payload[] = "roundtrip";
    char loaded[sizeof(payload)]{};
    NexoraStorageInfoV1 info{sizeof(info), 1, 0, 0};
    if (storage.save(storage.context, "fixture", 7, 7, payload, sizeof(payload)) ||
        storage.load(storage.context, "fixture", 7, loaded, sizeof(loaded), &info) ||
        info.schema_version != 7 || info.payload_size != sizeof(payload) || loaded[0] != 'r' ||
        storage.remove(storage.context, "fixture", 7))
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    std::fprintf(stderr, "player storage roundtrip: schema=7 removed=1\n");
  } else if (storage_result != NEXORA_GAMEPLAY_ERROR_UNSUPPORTED)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  std::fprintf(stderr, "player gameplay start: world=1 spawn=1 component=1 authored_pose=1\n");
  return NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *opaque, double delta) {
  if (delta != 1.0 / 60.0)
    return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
  ++static_cast<State *>(opaque)->fixed;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *opaque, double delta) {
  auto &state = *static_cast<State *>(opaque);
  NexoraInputSnapshot input{};
  NexoraFrameDiagnostics diagnostics{};
  if (delta < 0 || delta > 0.25 || state.host.capture_input(state.host.context, 0, &input) ||
      state.host.get_diagnostics(state.host.context, &diagnostics) || !diagnostics.scene_entities)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  ++state.updates;
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  const auto removed = state.host.despawn_entity(state.host.context, state.entity);
  std::fprintf(stderr, "player gameplay stop: updates=%llu fixed=%llu despawn=%d\n",
               static_cast<unsigned long long>(state.updates),
               static_cast<unsigned long long>(state.fixed), removed == 0);
}
void Destroy(void *opaque) {
  auto *state = static_cast<State *>(opaque);
  const auto host = state->host;
  state->~State();
  host.deallocate(host.context, 1, state, sizeof(State), alignof(State));
  std::fprintf(stderr, "player gameplay destroy: released=1\n");
}
} // namespace
#if defined(_WIN32)
#define PLAYER_EXPORT __declspec(dllexport)
#else
#define PLAYER_EXPORT __attribute__((visibility("default")))
#endif
extern "C" PLAYER_EXPORT int32_t NexoraGameModuleLoad(uint32_t abi, NexoraGameModuleV3 *module) {
  if (abi != NEXORA_GAMEPLAY_ABI_VERSION || !module || module->struct_size < sizeof(*module))
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  *module = {sizeof(*module), abi,     NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE,
             nullptr,         Create,  Start,
             Fixed,           Update,  Stop,
             Destroy,         nullptr, nullptr};
  return NEXORA_GAMEPLAY_OK;
}
