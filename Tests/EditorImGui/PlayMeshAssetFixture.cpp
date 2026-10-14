#include "Nexora/Foundation/GameplayABI.h"
#include <cstdint>
#include <cstdio>
#include <string_view>

namespace {
constexpr std::uint64_t Type(std::string_view name) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (const auto ch : name)
    hash = (hash ^ static_cast<unsigned char>(ch)) * 1099511628211ULL;
  return hash;
}
struct State {
  NexoraGameplayHostV3 host;
  NexoraAssetHandle mesh;
  std::uint64_t entity{}, updates{};
  bool stopped{};
};
struct MeshWire {
  std::uint64_t mesh, shader;
};
struct Position {
  double x, y, z;
};
int32_t Resolve(State &state, NexoraAssetHandle &out) {
  return state.host.resolve_asset(state.host.context, 0x123456789abcdef0ULL, 0xfedcba9876543210ULL,
                                  &out);
}
int32_t Create(void **output, const NexoraGameplayHostV3 *host) {
  if (!output || !host || !host->allocate || !host->deallocate || !host->resolve_asset ||
      !host->read_component || !host->write_component || !host->load_scene ||
      !host->activate_scene || !host->spawn_entity || !host->despawn_entity || !host->log)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  auto *memory = host->allocate(host->context, 7, sizeof(State), alignof(State));
  if (!memory)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  auto &state = *static_cast<State *>(memory);
  state = State{*host, {}};
  if (Resolve(state, state.mesh) != NEXORA_GAMEPLAY_OK || !state.mesh.value) {
    host->deallocate(host->context, 7, memory, sizeof(State), alignof(State));
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  }
  *output = memory;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  auto &host = state.host;
  MeshWire authored{};
  NexoraAssetHandle again{};
  if (Resolve(state, again) != NEXORA_GAMEPLAY_OK || again.value != state.mesh.value ||
      host.read_component(host.context, 20, Type("Nexora.MeshRenderer"), &authored,
                          sizeof(authored)) != NEXORA_GAMEPLAY_OK ||
      authored.mesh != state.mesh.value)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  constexpr char name[] = "Resolved Play mesh";
  std::uint64_t scene{};
  NexoraEntitySpawnDescriptor descriptor{};
  descriptor.struct_size = sizeof(descriptor);
  descriptor.components = NEXORA_SPAWN_MESH;
  descriptor.mesh = state.mesh;
  if (host.load_scene(host.context, name, sizeof(name) - 1, 0, &scene) != NEXORA_GAMEPLAY_OK ||
      host.activate_scene(host.context, scene) != NEXORA_GAMEPLAY_OK ||
      host.spawn_entity(host.context, scene, &descriptor, &state.entity) != NEXORA_GAMEPLAY_OK ||
      host.despawn_entity(host.context, 20) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  constexpr char message[] = "Resolved Play mesh asset fixture started";
  host.log(host.context, 1, message, sizeof(message) - 1);
  std::fprintf(stderr,
               "play mesh asset fixture evidence: create=1 start=1 resolved=%llu spawned=%llu "
               "replaced_clone=1\n",
               static_cast<unsigned long long>(state.mesh.value),
               static_cast<unsigned long long>(state.entity));
  return NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *opaque, double seconds) {
  auto &state = *static_cast<State *>(opaque);
  Position position{};
  if (state.host.read_component(state.host.context, state.entity, Type("Nexora.Transform"),
                                &position, sizeof(position)) != NEXORA_GAMEPLAY_OK)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  position.x += seconds * 0.5;
  return state.host.write_component(state.host.context, state.entity, Type("Nexora.Transform"),
                                    &position, sizeof(position));
}
int32_t Update(void *opaque, double) {
  auto &state = *static_cast<State *>(opaque);
  NexoraAssetHandle out{};
  if (Resolve(state, out) != NEXORA_GAMEPLAY_OK || out.value != state.mesh.value)
    return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
  ++state.updates;
  if (state.host.get_diagnostics) {
    NexoraFrameDiagnostics diagnostics{};
    if (state.host.get_diagnostics(state.host.context, &diagnostics) != NEXORA_GAMEPLAY_OK ||
        diagnostics.frame != state.updates || diagnostics.scene_entities == 0)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    if (state.updates == 1)
      std::fprintf(stderr, "play diagnostics fixture evidence: frame=1 entities=%llu errors=%llu\n",
                   static_cast<unsigned long long>(diagnostics.scene_entities),
                   static_cast<unsigned long long>(diagnostics.api_errors));
  }
  return NEXORA_GAMEPLAY_OK;
}
void Stop(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  NexoraAssetHandle out{};
  state.stopped = Resolve(state, out) == NEXORA_GAMEPLAY_OK && out.value == state.mesh.value;
}
void Destroy(void *opaque) {
  auto &state = *static_cast<State *>(opaque);
  NexoraAssetHandle out{};
  const bool resolved = Resolve(state, out) == NEXORA_GAMEPLAY_OK && out.value == state.mesh.value;
  std::fprintf(stderr, "play mesh asset fixture teardown: stop=%d destroy=%d\n", state.stopped,
               resolved);
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
