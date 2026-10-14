#include "GameViewPreview.h"
#include "PlayGameplayModule.h"
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
NexoraGameplayHostV3 host{};
bool fail_start{}, stopped{}, destroyed{};
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
NexoraFrameDiagnostics Read() {
  NexoraFrameDiagnostics out{};
  Require(host.get_diagnostics(host.context, &out) == NEXORA_GAMEPLAY_OK,
          "diagnostics unavailable");
  return out;
}
int32_t Create(void **state, const NexoraGameplayHostV3 *api) {
  host = *api;
  *state = nullptr;
  return NEXORA_GAMEPLAY_OK;
}
int32_t Update(void *, double) { return NEXORA_GAMEPLAY_OK; }
int32_t Start(void *) {
  if (fail_start) {
    const NexoraDebugLine line{{-1, 0, 0}, {1, 0, 0}, 0xffffffff, 1};
    Require(host.debug_draw_line(host.context, &line) == NEXORA_GAMEPLAY_OK,
            "failed-start line admission failed");
  }
  return fail_start ? NEXORA_GAMEPLAY_ERROR_LIFECYCLE : NEXORA_GAMEPLAY_OK;
}
int32_t Fixed(void *, double) {
  const NexoraDebugLine line{{-1, 0, 0}, {1, 0, 0}, 0xffffffff, 0};
  return host.debug_draw_line(host.context, &line);
}
void Stop(void *) { stopped = Read().debug_lines > 0; }
void Destroy(void *) { destroyed = Read().debug_lines > 0; }
int32_t Loader(std::uint32_t version, NexoraGameModuleV3 *module) {
  *module = {};
  module->struct_size = sizeof(*module);
  module->abi_version = version;
  module->capabilities = NEXORA_GAMEPLAY_CAPABILITY_FIXED_UPDATE;
  module->create = Create;
  module->on_start = Start;
  module->update = Update;
  module->fixed_update = Fixed;
  module->on_stop = Stop;
  module->destroy = Destroy;
  return NEXORA_GAMEPLAY_OK;
}
} // namespace
int main() {
  try {
    using namespace nexora;
    runtime::World source;
    const auto scene = source.LoadScene("Source");
    Require(source.Activate(scene), "scene activation failed");
    editor::SceneDocument document(source, scene);
    const auto camera = document.Create("Camera");
    Require(document.SetCamera(*document.Key(camera), runtime::CameraComponent{}) &&
                document.SetTransform(camera, {0, 0, 5}),
            "camera setup failed");
    const auto baseline = source.SaveScene(scene);
    const auto dirty = document.Dirty();
    runtime::PlaySession play(source);
    editor::preview::PlayGameplayModule module;
    editor::MeshAssetCatalog meshes;
    for (int cycle = 0; cycle < 32; ++cycle) {
      fail_start = false;
      Require(!module.Load(source, Loader), "debug host admitted Editor World");
      Require(play.Start(0.25, [&](auto &, double dt) { return module.FixedUpdate(dt); }) &&
                  module.Load(*play.PlayWorld(), Loader),
              "Play load failed");
      Require(host.debug_draw_line && module.DebugLines().empty() && Read().debug_lines == 0,
              "Play debug line service unavailable or not reset");
      NexoraDebugLine transient{{-1, 0, 0}, {1, 0, 0}, 0x00ff00ff, 0};
      NexoraDebugLine persistent{{0, -1, 0}, {0, 1, 0}, 0xff0000ff, 1};
      const auto add = [&](const auto &line) { return host.debug_draw_line(host.context, &line); };
      Require(add(transient) == NEXORA_GAMEPLAY_OK && add(persistent) == NEXORA_GAMEPLAY_OK,
              "valid lines rejected");
      transient.start.x = 99;
      Require(module.DebugLines()[0].start.x == -1 && Read().debug_lines == 2,
              "line wire was retained by reference");
      const auto before_errors = Read().api_errors;
      auto invalid = persistent;
      for (const auto value : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(), 100000.0}) {
        invalid.start.x = value;
        Require(add(invalid) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
                "invalid coordinates admitted");
      }
      invalid = persistent;
      invalid.end = invalid.start;
      Require(add(invalid) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT, "degenerate line admitted");
      invalid = persistent;
      invalid.rgba = 0xfffffffe;
      Require(add(invalid) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT, "translucent line admitted");
      for (const auto value : {-1.0F, 3601.0F, std::numeric_limits<float>::quiet_NaN(),
                               std::numeric_limits<float>::infinity()}) {
        invalid = persistent;
        invalid.duration_seconds = value;
        Require(add(invalid) == NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT,
                "invalid duration admitted");
      }
      Require(host.debug_draw_line(host.context, nullptr) ==
                      NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT &&
                  Read().api_errors == before_errors + 10 && Read().debug_lines == 2,
              "rejected lines changed queue or error accounting");
      auto frame = editor::preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), meshes, 2);
      Require(frame.camera == camera && frame.instances.empty(), "mesh-free fixture invalid");
      editor::preview::AppendGameDebugLines(frame, module.DebugLines());
      Require(frame.debug_lines == 2 && frame.instances.size() == 2 &&
                  frame.geometry.vertices.size() == 8 && frame.geometry.indices.size() == 24 &&
                  frame.batches.size() == 2 && frame.instances[0].color[1] == 1 &&
                  frame.instances[0].color[0] == 0 && frame.instances[1].color[0] == 1,
              "debug lines did not produce bounded colored scene geometry");
      for (const auto &vertex : frame.geometry.vertices)
        for (const auto value : vertex.position)
          Require(std::isfinite(value), "nonfinite ribbon geometry");
      const auto before = module.DebugLines().size();
      Require(!module.AdvanceDebugLines(-1) &&
                  !module.AdvanceDebugLines(std::numeric_limits<double>::quiet_NaN()) &&
                  !module.AdvanceDebugLines(std::numeric_limits<double>::infinity()) &&
                  module.DebugLines().size() == before,
              "invalid frame delta changed queue");
      Require(play.Pause() && play.Step() && Read().debug_lines == 3,
              "paused fixed step aged lines or failed to admit its new line");
      Require(module.AdvanceDebugLines(0.25) && Read().debug_lines == 1 &&
                  module.DebugLines()[0].rgba == persistent.rgba && module.AdvanceDebugLines(0.5) &&
                  Read().debug_lines == 1 && module.AdvanceDebugLines(0.25) &&
                  Read().debug_lines == 0,
              "duration expiry/order/boundary is wrong");
      Require(add(persistent) == NEXORA_GAMEPLAY_OK && module.AdvanceDebugLines(1e100) &&
                  module.DebugLines().empty(),
              "large finite delta did not expire queue");
      for (std::size_t i = 0; i < editor::preview::PlayDebugLines::kCapacity; ++i)
        Require(add(persistent) == NEXORA_GAMEPLAY_OK, "bounded queue filled early");
      Require(add(persistent) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE && Read().debug_lines == 256,
              "full queue accepted extra line");
      auto full = editor::preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), meshes, 2);
      editor::preview::AppendGameDebugLines(full, module.DebugLines());
      Require(full.debug_lines == 256 && full.instances.size() == 256 &&
                  full.geometry.vertices.size() == 1024 && full.geometry.indices.size() == 3072,
              "maximum queue render budget failed");
      auto saturated =
          editor::preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), meshes, 2);
      saturated.instances.resize(4096);
      editor::preview::AppendGameDebugLines(saturated, module.DebugLines());
      Require(saturated.unavailable_debug_lines == 256 && saturated.geometry.vertices.empty() &&
                  saturated.instances.size() == 4096,
              "debug lines exceeded existing instance budget");
      // The PBR path uses the existing neutral fallback slot and preserves authored material slots.
      auto pbr = editor::preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), meshes, 2);
      pbr.pbr = true;
      pbr.materials.resize(64);
      pbr.materials.back().emission = {1, 2, 3};
      pbr.camera_position = {5, 0, 0};
      editor::preview::AppendGameDebugLines(pbr, module.DebugLines().first(1));
      Require(pbr.pbr && pbr.materials.size() == 64 && pbr.materials.back().emission[2] == 3 &&
                  pbr.batches[0].materialIndex == 0 && pbr.debug_lines == 1,
              "debug render changed existing PBR palette");
      for (const auto &vertex : pbr.geometry.vertices) {
        float dot = 0, magnitude = 0;
        for (std::size_t i = 0; i < 3; ++i) {
          dot += vertex.normal[i] * vertex.tangent[i];
          magnitude += vertex.tangent[i] * vertex.tangent[i];
        }
        Require(std::abs(dot) < 1e-5F && std::abs(magnitude - 1) < 1e-5F,
                "PBR ribbon tangent is not orthogonal to its normal");
      }
      stopped = destroyed = false;
      module.Unload();
      Require(stopped && destroyed && module.DebugLines().empty() && full.debug_lines == 256 &&
                  full.geometry.vertices.size() == 1024,
              "line ownership failed across teardown");
      Require(host.debug_draw_line(host.context, &persistent) == NEXORA_GAMEPLAY_ERROR_LIFECYCLE,
              "unbound line request admitted");
      fail_start = true;
      destroyed = false;
      Require(!module.Load(*play.PlayWorld(), Loader) && destroyed && module.DebugLines().empty(),
              "failed Start retained lines");
      Require(play.Stop() && source.SaveScene(scene) == baseline && document.Dirty() == dirty,
              "debug drawing changed authoring scene/history");
    }
    Require(document.Undo() && source.FindEntity(camera)->transform.z == 0 && document.Redo() &&
                source.SaveScene(scene) == baseline,
            "debug lifecycle changed Undo cursor");
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
