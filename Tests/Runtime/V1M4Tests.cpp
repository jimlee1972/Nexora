#include "Nexora/RHI/ShaderReflection.h"
#include "Nexora/Runtime/Runtime.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int RunTests() {
  using namespace nexora;
  runtime::World editor{runtime::WorldKind::Editor};
  const auto main_scene = editor.LoadScene("Main", true);
  const auto lighting_scene = editor.LoadScene("Lighting");
  auto &camera = editor.CreateEntity(main_scene);
  camera.camera = true;
  camera.transform = {1000000000.25, 3.0, -2.0};
  auto &model = editor.CreateEntity(main_scene);
  const auto model_id = model.id;
  model.mesh_renderer = true;
  model.mesh_data = {41, {73}};
  auto &light = editor.CreateEntity(lighting_scene);
  light.light = true;
  light.light_data.intensity = 4.0F;
  Require(editor.Activate(main_scene) && editor.Activate(lighting_scene),
          "additive scenes did not activate");

  runtime::SystemScheduler scheduler;
  Require(scheduler.Add("animation", {},
                        [model_id](auto &, auto &commands) {
                          commands.SetTransform(model_id, {4.0, 5.0, 6.0});
                        }) &&
              scheduler.Add("presentation", {"animation"}, [](auto &, auto &) {}) &&
              scheduler.Execute(editor),
          "dependency-ordered systems did not apply deferred commands");
  const auto *scheduled_model = editor.FindEntity(model_id);
  Require(scheduled_model != nullptr && scheduled_model->transform.x == 4.0,
          "deferred transform was not applied");

  const auto saved = editor.SaveScene(main_scene);
  if (!saved)
    throw std::runtime_error("active scene could not be saved");
  runtime::World restored;
  const auto restored_scene = restored.LoadSceneSnapshot(*saved);
  if (!restored_scene)
    throw std::runtime_error("scene snapshot could not be loaded");
  Require(restored.SaveScene(*restored_scene) == saved, "scene save/load was not deterministic");
  Require(!restored.LoadSceneSnapshot("NEXORA_SCENE 99 \"broken\" 0 0").has_value(),
          "unsupported scene version was accepted");

  auto play = editor.CloneForPlay();
  Require(play.Kind() == runtime::WorldKind::Play && play.ActiveSceneCount() == 2,
          "editor world was not isolated into a play world");
  Require(!play.RequestUnload(main_scene), "persistent scene was allowed to unload");
  Require(play.RequestUnload(lighting_scene), "safe unload was not queued");
  Require(play.FindScene(lighting_scene)->state == runtime::SceneState::Unloading,
          "scene skipped the unloading state");
  play.EndFrame();
  Require(play.FindScene(lighting_scene)->state == runtime::SceneState::Unloaded,
          "scene did not unload at the frame boundary");

  if (!runtime::SceneRenderingEnabled())
    return 0;

  auto device = rhi::CreateValidationDevice();
  const auto layout = rhi::TrianglePipelineLayout();
  const auto pipeline = device->CreatePipeline(
      {layout.layout_hash, 0x4d34, rhi::TextureFormat::Rgba8Unorm, "M4 scene"});
  const rhi::TextureDescriptor descriptor{640, 360, rhi::TextureFormat::Rgba8Unorm,
                                          rhi::ResourceState::Present, "M4 target"};
  const auto target = device->CreateTexture(descriptor);
  const auto frame = runtime::RenderSceneFrame(editor, *device, target, descriptor, pipeline);
  if (!frame)
    throw std::runtime_error("complete scene was rejected by render extraction");
  Require(frame->visible_meshes == 1 && frame->passes == 5 && frame->barriers == 6 &&
              device->Diagnostics().draw_calls == 3,
          "scene components did not reach the render graph");
  device->DestroyTexture(target);
  device->DestroyPipeline(pipeline);

  runtime::World incomplete;
  const auto incomplete_scene = incomplete.LoadScene("Incomplete");
  incomplete.CreateEntity(incomplete_scene).mesh_renderer = true;
  Require(incomplete.Activate(incomplete_scene) &&
              !runtime::RenderSceneFrame(incomplete, *device, {}, descriptor, {}).has_value(),
          "render extraction accepted a scene without camera/light/material");

  runtime::SystemScheduler cyclic;
  Require(cyclic.Add("a", {"b"}, [](auto &, auto &) {}) &&
              cyclic.Add("b", {"a"}, [](auto &, auto &) {}) && !cyclic.Execute(editor),
          "system dependency cycle was accepted");

  const auto started = std::chrono::steady_clock::now();
  runtime::World stress;
  const auto stress_scene = stress.LoadScene("Stress");
  for (std::size_t index = 0; index < 10000; ++index)
    stress.CreateEntity(stress_scene).transform.x = static_cast<double>(index);
  const auto snapshot = stress.SaveScene(stress_scene);
  if (!snapshot)
    throw std::runtime_error("stress scene could not be saved");
  Require(snapshot->size() < 2'000'000 &&
              std::chrono::steady_clock::now() - started < std::chrono::seconds(5),
          "10k-entity scene baseline exceeded its time or size budget");
  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
