#include "Nexora/Runtime/EditorSdk.h"
#include "PlayGameplayModule.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace nexora::editor::preview {
struct PlayGameplayDiagnosticsTestAccess final {
  static void NearLimit(PlayGameplayModule &module) {
    module.diagnostics_frame_ = module.diagnostics_errors_ = UINT64_MAX - 1;
  }
};
} // namespace nexora::editor::preview

namespace {
using namespace nexora;
NexoraGameplayHostV3 host{};
NexoraFrameDiagnostics created{}, updated{}, destroyed{};
bool fail_update{};
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
NexoraFrameDiagnostics Read() {
  NexoraFrameDiagnostics out{99, 99, 99, 99};
  Require(host.get_diagnostics && host.get_diagnostics(host.context, &out) == NEXORA_GAMEPLAY_OK,
          "Play diagnostics callback unavailable");
  return out;
}
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  *state = nullptr;
  if (!host.get_diagnostics)
    return NEXORA_GAMEPLAY_ERROR_UNSUPPORTED;
  created = Read();
  return NEXORA_GAMEPLAY_OK;
}
int32_t Start(void *) { return NEXORA_GAMEPLAY_OK; }
int32_t Update(void *, double) {
  updated = Read();
  return fail_update ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
void Stop(void *) { destroyed = Read(); }
void Destroy(void *) { destroyed = Read(); }
int32_t Loader(std::uint32_t requested, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = requested;
  module->create = Create;
  module->on_start = Start;
  module->update = Update; // FixedUpdate is intentionally absent.
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
} // namespace
int main() {
  try {
    using namespace nexora;
    runtime::World source;
    const auto authored = source.LoadScene("Authored");
    Require(source.Activate(authored), "activate authored failed");
    const auto authored_entity = source.CreateEntity(authored).id;
    const auto inactive = source.LoadScene("Inactive");
    source.CreateEntity(inactive);
    source.CreateEntity(inactive);
    const auto baseline = source.SaveScene(authored),
               inactive_baseline = source.SaveScene(inactive);
    runtime::PlaySession play(source);
    editor::preview::PlayGameplayModule module;
    for (int cycle = 0; cycle < 32; ++cycle) {
      fail_update = false;
      Require(play.Start(0.25, [&](runtime::World &, double dt) { return module.FixedUpdate(dt); }),
              "Play start failed");
      Require(module.Load(*play.PlayWorld(), Loader), "diagnostics module load failed");
      Require(created.frame == 0 && created.scene_entities == 1 && created.api_errors == 0 &&
                  created.debug_lines == 0,
              "Create diagnostics not reset or active-only");
      Require(host.resolve_asset && host.debug_draw_line, "unavailable service advertised");
      Require(play.Tick() && Read().frame == 0, "optional fixed tick invented an update frame");
      Require(module.Update(0.1) && updated.frame == 1, "Update callback frame was stale");
      Require(!module.Update(-1) && !module.Update(std::numeric_limits<double>::quiet_NaN()) &&
                  !module.Update(std::numeric_limits<double>::infinity()) && Read().frame == 1,
              "invalid Update advanced diagnostic frame");
      const auto owned = Read();
      std::uint64_t scene{}, entity{};
      constexpr char name[] = "Diagnostic scene";
      Require(host.load_scene(host.context, name, sizeof(name) - 1, 0, &scene) ==
                  NEXORA_GAMEPLAY_OK,
              "scene load failed");
      NexoraEntitySpawnDescriptor descriptor{};
      descriptor.struct_size = sizeof(descriptor);
      Require(host.spawn_entity(host.context, scene, &descriptor, &entity) == NEXORA_GAMEPLAY_OK &&
                  Read().scene_entities == 1,
              "inactive entities counted");
      Require(host.activate_scene(host.context, scene) == NEXORA_GAMEPLAY_OK &&
                  Read().scene_entities == 2,
              "activated scene count stale");
      Require(owned.scene_entities == 1, "diagnostics snapshot borrowed mutable storage");
      Require(host.despawn_entity(host.context, entity) == NEXORA_GAMEPLAY_OK &&
                  Read().scene_entities == 1,
              "despawn count stale");
      Require(host.spawn_entity(host.context, scene, &descriptor, &entity) == NEXORA_GAMEPLAY_OK,
              "replacement spawn failed");
      Require(play.PlayWorld()->RequestUnload(scene) && Read().scene_entities == 1,
              "unloading entities counted");
      play.PlayWorld()->EndFrame();
      Require(Read().scene_entities == 1, "unloaded count stale");
      NexoraInputSnapshot input{77, 1, 1, 1, 0};
      Require(host.capture_input(host.context, 99, &input) ==
                      NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                  input.sequence == 77 && input.move_x == 1 && input.buttons == 1,
              "invalid user changed output");
      Require(host.get_diagnostics(host.context, nullptr) ==
                      NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                  Read().api_errors == 2,
              "rejected callback count incorrect");
      game::GameplayTransformWire wire{7, 8, 9};
      Require(host.read_component(host.context, 0, game::TransformComponentType(), &wire,
                                  sizeof(wire)) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                  wire.x == 7 && Read().api_errors == 3,
              "component rejection not counted or changed output");
      Require(host.despawn_entity(host.context, authored_entity) == NEXORA_GAMEPLAY_OK &&
                  Read().scene_entities == 0 && source.FindEntity(authored_entity),
              "Play diagnostics escaped source isolation");
      fail_update = true;
      Require(!module.Update(0.1) && updated.frame == 2 && updated.api_errors == 3,
              "failed update frame/error scope incorrect");
      module.Unload();
      Require(destroyed.frame == 2 && destroyed.scene_entities == 0 && destroyed.api_errors == 3,
              "Stop/Destroy lost bound diagnostic state");
      NexoraFrameDiagnostics out{11, 22, 33, 44};
      Require(host.get_diagnostics(host.context, &out) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE &&
                  out.frame == 11 && out.scene_entities == 22 && out.debug_lines == 33 &&
                  out.api_errors == 44,
              "unbound callback published output");
      Require(play.Stop() && source.SaveScene(authored) == baseline &&
                  source.SaveScene(inactive) == inactive_baseline,
              "diagnostic lifecycle changed authored source");
    }
    fail_update = false;
    Require(play.Start(0.25, [](runtime::World &, double) { return true; }) &&
                module.Load(*play.PlayWorld(), Loader),
            "boundary module load failed");
    editor::preview::PlayGameplayDiagnosticsTestAccess::NearLimit(module);
    Require(module.Update(0) && Read().frame == UINT64_MAX &&
                host.get_diagnostics(host.context, nullptr) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                Read().api_errors == UINT64_MAX,
            "counter limit not reached");
    Require(module.Update(0) &&
                host.get_diagnostics(host.context, nullptr) ==
                    NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                Read().frame == UINT64_MAX && Read().api_errors == UINT64_MAX,
            "diagnostic counter wrapped");
    module.Unload();
    Require(play.Stop(), "boundary Play stop failed");
    std::cout << "Play diagnostics: 32 isolated module lifecycles and saturation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
