#include "Nexora/Runtime/EditorSdk.h"
#include "PlayGameplayModule.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using Services = editor::preview::PlaySceneServices;
void Require(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}
NexoraGameplayHostV3 host{};
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  *state = nullptr;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) { return NEXORA_GAMEPLAY_OK; }
int32_t Update(void *, double) { return NEXORA_GAMEPLAY_OK; }
bool stopped{}, destroyed{};
std::uint64_t owned_scene{};
void Stop(void *) {
  std::uint64_t entity{};
  NexoraEntitySpawnDescriptor empty{};
  empty.struct_size = sizeof(empty);
  stopped = host.spawn_entity(host.context, owned_scene, &empty, &entity) == NEXORA_GAMEPLAY_OK;
  if (stopped)
    destroyed = host.despawn_entity(host.context, entity) == NEXORA_GAMEPLAY_OK;
}
void Destroy(void *) {
  destroyed = destroyed && host.activate_scene(host.context, owned_scene) ==
                               NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
}
int32_t Loader(std::uint32_t requested, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = requested;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
NexoraEntitySpawnDescriptor Descriptor() {
  NexoraEntitySpawnDescriptor wire{};
  wire.struct_size = sizeof(wire);
  wire.components = NEXORA_SPAWN_CAMERA | NEXORA_SPAWN_LIGHT | NEXORA_SPAWN_MESH;
  wire.position = {1, 2, 3};
  wire.camera_fov_degrees = 75;
  wire.light_intensity = 2.5F;
  wire.mesh.value = UINT64_MAX;
  wire.material.value = UINT64_MAX - 1;
  return wire;
}
} // namespace
int main() {
  try {
    runtime::World world;
    const auto authored_scene = world.LoadScene("Authored");
    Require(world.Activate(authored_scene), "activate authored scene failed");
    const auto authored_entity = world.CreateEntity(authored_scene).id;
    const auto baseline = *world.SaveScene(authored_scene);
    runtime::PlaySession play(world);
    editor::preview::PlayGameplayModule module;
    Require(!module.Load(world, Loader) && world.SaveScene(authored_scene) == baseline,
            "gameplay admitted an Editor World");
    for (int cycle = 0; cycle < 16; ++cycle) {
      Require(play.Start(0.25, [](runtime::World &, double) { return true; }) &&
                  module.Load(*play.PlayWorld(), Loader),
              "Play module start failed");
      Require(host.capabilities == (NEXORA_GAMEPLAY_CAPABILITY_HOST_ALLOCATOR |
                                    NEXORA_GAMEPLAY_CAPABILITY_SCENE_API) &&
                  host.load_scene && host.activate_scene && host.spawn_entity &&
                  host.despawn_entity &&
                  (static_cast<bool>(host.raycast) == (NEXORA_GAMEPLAY_SIMULATION_ENABLED != 0)) &&
                  !host.resolve_asset && !host.debug_draw_line && !host.get_diagnostics,
              "host advertised unavailable services");
      const std::string name = "Play 世界";
      Require(host.load_scene(host.context, name.data(), static_cast<uint32_t>(name.size()), 1,
                              &owned_scene) == NEXORA_GAMEPLAY_OK &&
                  host.activate_scene(host.context, owned_scene) == NEXORA_GAMEPLAY_OK,
              "real V3 scene load/activate failed");
      Require(play.PlayWorld()->FindScene(owned_scene)->name == name &&
                  play.PlayWorld()->FindScene(owned_scene)->persistent,
              "scene owning values were lost");
      auto wire = Descriptor();
      std::uint64_t entity = 999;
      Require(host.spawn_entity(host.context, owned_scene, &wire, &entity) == NEXORA_GAMEPLAY_OK,
              "real V3 spawn failed");
      const auto &created = *play.PlayWorld()->FindEntity(entity);
      Require(created.transform.x == 1 && created.transform.y == 2 && created.transform.z == 3 &&
                  created.camera && created.camera_data.vertical_field_of_view == 75 &&
                  created.light && created.light_data.intensity == 2.5F && created.mesh_renderer &&
                  created.mesh_data.mesh == UINT64_MAX &&
                  created.mesh_data.material.shader == UINT64_MAX - 1,
              "spawn descriptor component values were lost");
      runtime::WorldCommandBuffer parent;
      const auto child = play.PlayWorld()->CreateEntity(owned_scene).id;
      parent.SetParent(child, entity, false);
      Require(parent.Apply(*play.PlayWorld()) &&
                  host.despawn_entity(host.context, entity) == NEXORA_GAMEPLAY_OK &&
                  !play.PlayWorld()->FindEntity(child) &&
                  host.despawn_entity(host.context, entity) ==
                      NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
              "despawn did not cascade or stale deletion mutated the clone");
      Require(host.despawn_entity(host.context, authored_entity) == NEXORA_GAMEPLAY_OK &&
                  world.FindEntity(authored_entity),
              "despawn crossed into Editor World");
      std::uint64_t unchanged = 999;
      for (const auto &invalid_name : {std::string{}, std::string("a\0b", 3),
                                       std::string("\xc0\xaf", 2), std::string(257, 'n')})
        Require(host.load_scene(host.context, invalid_name.data(),
                                static_cast<uint32_t>(invalid_name.size()), 0,
                                &unchanged) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                    unchanged == 999,
                "invalid name changed output");
      Require(host.load_scene(host.context, reinterpret_cast<const char *>(1), UINT32_MAX, 0,
                              &unchanged) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                  host.load_scene(host.context, "a", 1, 2, &unchanged) ==
                      NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                  host.load_scene(host.context, nullptr, 1, 0, &unchanged) ==
                      NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                  unchanged == 999,
              "wire length/flags validation did not precede input read");
      const auto retained = *play.PlayWorld()->SaveScene(owned_scene);
      const auto nan = std::numeric_limits<double>::quiet_NaN();
      for (int invalid = 0; invalid < 9; ++invalid) {
        wire = Descriptor();
        switch (invalid) {
        case 0:
          wire.struct_size = sizeof(wire) - 1;
          break;
        case 1:
          wire.components |= 128;
          break;
        case 2:
          wire.reserved = 1;
          break;
        case 3:
          wire.position.y = nan;
          break;
        case 4:
          wire.camera_fov_degrees = 180;
          break;
        case 5:
          wire.camera_fov_degrees = nan;
          break;
        case 6:
          wire.light_intensity = -1;
          break;
        case 7:
          wire.light_intensity = std::numeric_limits<float>::infinity();
          break;
        case 8:
          wire.mesh.value = 0;
          break;
        }
        Require(host.spawn_entity(host.context, owned_scene, &wire, &unchanged) ==
                        NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                    unchanged == 999 && play.PlayWorld()->SaveScene(owned_scene) == retained,
                "invalid descriptor partially published an entity");
      }
      wire = Descriptor();
      wire.components |= NEXORA_SPAWN_PHYSICS;
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
      wire.bounds_minimum.x = std::numeric_limits<double>::quiet_NaN();
      constexpr auto physics_error = NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
#else
      constexpr auto physics_error = NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
#endif
      Require(host.spawn_entity(host.context, owned_scene, &wire, &unchanged) == physics_error &&
                  unchanged == 999 && play.PlayWorld()->SaveScene(owned_scene) == retained,
              "invalid or unsupported physics published a partial entity");
      stopped = destroyed = false;
      module.Unload();
      Require(stopped && destroyed &&
                  host.activate_scene(host.context, owned_scene) ==
                      NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
                  play.Stop() && world.SaveScene(authored_scene) == baseline,
              "services outlived clone or shutdown callbacks lost their World");
    }
    // Budgets count successful admission over one bind, independent of later despawn.
    auto clone = world.CloneForPlay();
    Services services;
    Require(services.Bind(clone), "bind failed");
    std::uint64_t output{};
    auto empty = Descriptor();
    empty.components = 0;
    empty.struct_size += 32;
    empty.camera_fov_degrees = std::numeric_limits<double>::quiet_NaN();
    Require(services.Spawn(authored_scene, &empty, &output) == NEXORA_GAMEPLAY_OK &&
                services.Despawn(output) == NEXORA_GAMEPLAY_OK,
            "future suffix or unused optional data changed prefix admission");
    output = 999;
    Require(services.Spawn(authored_scene, nullptr, &output) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                services.Spawn(authored_scene, &empty, nullptr) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                services.Load("a", 1, 0, nullptr) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                services.Spawn(UINT64_MAX, &empty, &output) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                output == 999,
            "null wires/output or missing scene published an object");
    const auto expiring = clone.LoadScene("Expiring");
    Require(clone.RequestUnload(expiring) &&
                services.Activate(expiring) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                services.Spawn(expiring, &empty, &output) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                output == 999,
            "unloading scene admitted an entity");
    clone.EndFrame();
    Require(services.Spawn(expiring, &empty, &output) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                output == 999 && services.Bind(clone),
            "unloaded scene admitted an entity or reset failed");
    const std::string exact(Services::kMaximumNameBytes, 'n');
    for (std::size_t index = 0; index < Services::kMaximumScenes; ++index)
      Require(services.Load(exact.data(), static_cast<uint32_t>(exact.size()), 0, &output) ==
                  NEXORA_GAMEPLAY_OK,
              "exact scene budget failed");
    for (std::size_t index = 0; index < Services::kMaximumSpawns; ++index) {
      Require(services.Spawn(authored_scene, &empty, &output) == NEXORA_GAMEPLAY_OK &&
                  services.Despawn(output) == NEXORA_GAMEPLAY_OK,
              "exact spawn budget failed");
    }
    output = 999;
    Require(services.Load("over", 4, 0, &output) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
                services.Spawn(authored_scene, &empty, &output) ==
                    NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
                output == 999 && clone.FindScene(authored_scene)->entities.size() == 1,
            "over budget admitted or despawn recycled lifetime quota");
    Require(services.Bind(clone) &&
                services.Spawn(authored_scene, &empty, &output) == NEXORA_GAMEPLAY_OK,
            "new binding did not reset admission quota");
    Require(!services.Bind(world) &&
                services.Despawn(authored_entity) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
                world.SaveScene(authored_scene) == baseline,
            "failed Editor bind retained prior clone access");
    std::cout << "Play V3 scene services, budgets and Editor isolation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
