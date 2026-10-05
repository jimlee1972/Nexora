#include "ShowcaseRooms.h"
#include <cassert>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

using nexora::showcase::RoomSession;
using Nexora::Window::Key;
using Nexora::Window::WindowEvent;
using Nexora::Window::WindowEventType;
static void Press(RoomSession &session, Key key) {
  WindowEvent event{};
  event.type = WindowEventType::Key;
  event.value0 = static_cast<int>(key);
  event.value1 = 1;
  session.Event(event, 1280, 720);
  event.value1 = 0;
  session.Event(event, 1280, 720);
}
int main() {
#ifdef NEXORA_SHOWCASE_TEST_PLUGIN
  RoomSession session("hub", false, false, NEXORA_SHOWCASE_TEST_PLUGIN);
#else
  RoomSession session("hub");
#endif
  session.Tick(1.0 / 60);
  assert(session.Healthy());
  assert(session.Probes().size() == 13);
#if NEXORA_ASSET_PIPELINE_ENABLED
  for (const auto error : {nexora::showcase::ErrorInjection::InvalidAsset,
                           nexora::showcase::ErrorInjection::DependencyCycle,
                           nexora::showcase::ErrorInjection::Rollback}) {
    session.RerunProbe(5, error);
    assert(session.Probes()[5].status == nexora::showcase::ProbeStatus::Pass);
    assert(session.Probes()[5].issues.empty());
  }
#endif
#if NEXORA_EDITOR_SDK_ENABLED && defined(NEXORA_SHOWCASE_TEST_PLUGIN)
  session.RerunProbe(6, nexora::showcase::ErrorInjection::PluginAbiMismatch);
  assert(session.Probes()[6].status == nexora::showcase::ProbeStatus::Pass);
  assert(session.Report().find("Plugin ABI mismatch rejected before registration") !=
         std::string::npos);
  RoomSession missingPlugin("scene", false, false, "missing-lab-plugin.so");
  missingPlugin.Tick(0.01);
  missingPlugin.RerunProbe(6, nexora::showcase::ErrorInjection::PluginAbiMismatch);
  assert(!missingPlugin.Healthy());
  assert(missingPlugin.Probes()[6].status == nexora::showcase::ProbeStatus::Fail);
#endif
#if NEXORA_SHIPPING_ENABLED
  session.RerunProbe(12, nexora::showcase::ErrorInjection::Rollback);
  assert(session.Probes()[12].status == nexora::showcase::ProbeStatus::Pass);
#endif
  // Courtyard cameras replay exactly after orbit input; diagnostic-free output owns no UI.
  RoomSession courtyard("courtyard");
  courtyard.Tick(0.01);
  const auto wide = courtyard.Scene(1280, 720);
  assert(wide.vertices.size() > 2000 && wide.indices.size() > 3000);
  for (const auto index : wide.indices)
    assert(index < wide.vertices.size());
  assert(wide.pbr);
  assert(wide.materials.size() == 18 && !wide.batches.empty());
  assert(Nexora::Presentation::ValidateSceneMaterials(wide.materials, wide.batches));
  std::size_t covered = 0;
  std::vector<bool> selectedMaterials(wide.materials.size());
  for (const auto &batch : wide.batches) {
    assert(batch.firstIndex == covered && batch.firstInstance == 0 && batch.instanceCount == 1);
    covered += batch.indexCount;
    assert(batch.materialIndex < selectedMaterials.size());
    selectedMaterials[batch.materialIndex] = true;
  }
  assert(covered == wide.indices.size());
  for (std::size_t i = 0; i < 7; ++i)
    assert(selectedMaterials[i]);
  for (std::size_t i = 8; i < 11; ++i) {
    assert(selectedMaterials[i] && !wide.materials[i].castsShadow);
  }
  assert(wide.batches[wide.batches.size() - 2].materialIndex == 6 &&
         wide.batches[wide.batches.size() - 2].indexCount == 36);
  assert(wide.batches.back().materialIndex == 11 && wide.materials[11].emission[0] > 1);
  const auto &skybox = wide.batches[wide.batches.size() - 2];
  std::array<float, 3> skyCenter{};
  for (std::size_t i = 0; i < 24; ++i) {
    const auto &vertex = wide.vertices[wide.indices[skybox.firstIndex] + i];
    for (std::size_t axis = 0; axis < 3; ++axis) {
      assert(std::abs(std::abs(vertex.position[axis] - wide.cameraPosition[axis]) - 120) < 1e-4F);
      skyCenter[axis] += vertex.position[axis] / 24;
    }
  }
  for (std::size_t axis = 0; axis < 3; ++axis)
    assert(std::abs(skyCenter[axis] - wide.cameraPosition[axis]) < 1e-4F);
  assert(wide.materials[13].metallic == 1 && !wide.materials[13].castsShadow);
  assert(!wide.materials[14].castsShadow);
  std::array<float, 3> solarOffset{};
  const auto &solarBatch = wide.batches.back();
  for (std::size_t i = 0; i < solarBatch.indexCount; ++i) {
    const auto &v = wide.vertices[wide.indices[solarBatch.firstIndex + i]];
    for (std::size_t axis = 0; axis < 3; ++axis)
      solarOffset[axis] += (v.position[axis] - wide.cameraPosition[axis]) / solarBatch.indexCount;
  }
  float lightLength = 0;
  for (const auto d : wide.light_direction)
    lightLength += d * d;
  lightLength = std::sqrt(lightLength);
  for (std::size_t axis = 0; axis < 3; ++axis)
    assert(std::abs(solarOffset[axis] / 104 + wide.light_direction[axis] / lightLength) < 1e-4F);
  assert(wide.materials[6].unlit && !wide.materials[6].castsShadow);
  for (const auto &vertex : wide.vertices) {
    float orthogonal = 0, length = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
      orthogonal += vertex.normal[axis] * vertex.tangent[axis];
      length += vertex.tangent[axis] * vertex.tangent[axis];
    }
    assert(std::abs(orthogonal) < 1e-4F && std::abs(length - 1) < 1e-4F);
    assert(std::abs(vertex.tangent[3]) == 1);
  }
#if NEXORA_ASSET_PIPELINE_ENABLED
  assert(wide.environment && wide.linearTextureUploads.size() == 3);
  assert(wide.environment->specularMipLevels == 7);
  for (const auto &upload : wide.linearTextureUploads)
    assert(Nexora::Presentation::ValidateSceneLinearTexture(upload));
  Press(courtyard, Key::O);
  assert(!courtyard.Scene(1280, 720).environment);
  Press(courtyard, Key::O);
  assert(courtyard.Scene(1280, 720).environment);
  assert(courtyard.Report().find("\"environment_loaded\":true") != std::string::npos);
#endif
  assert(wide.hdr && wide.offscreen && wide.exposure == 1 && wide.bloom && wide.depthOfField);
  Press(courtyard, Key::J);
  assert(!courtyard.Scene(1280, 720).depthOfField);
  Press(courtyard, Key::J);
  assert(courtyard.Scene(1280, 720).depthOfField);
  Press(courtyard, Key::K);
  assert(!courtyard.Scene(1280, 720).bloom);
  Press(courtyard, Key::K);
  assert(courtyard.Scene(1280, 720).bloom);
  assert(wide.shadow && wide.lightingStyle && wide.shadow->resolution == 1024);
  Press(courtyard, Key::F6);
  assert(!courtyard.Scene(1280, 720).shadow);
  Press(courtyard, Key::F6);
  assert(courtyard.Scene(1280, 720).shadow);
  Press(courtyard, Key::G);
  assert(!courtyard.Scene(1280, 720).lightingStyle);
  Press(courtyard, Key::G);
  assert(courtyard.Scene(1280, 720).lightingStyle);
  Press(courtyard, Key::RightBracket);
  assert(courtyard.Scene(1280, 720).shadow->normalBias == wide.shadow->normalBias * 2);
  Press(courtyard, Key::LeftBracket);
  assert(courtyard.Scene(1280, 720).shadow->normalBias == wide.shadow->normalBias);
  Press(courtyard, Key::E);
  assert(courtyard.Scene(1280, 720).exposure == 0.25F);
  Press(courtyard, Key::E);
  assert(courtyard.Scene(1280, 720).exposure == 1);
  Press(courtyard, Key::P);
  assert(!courtyard.Scene(1280, 720).pbr);
  assert(!courtyard.Scene(1280, 720).hdr);
  assert(!courtyard.Scene(1280, 720).environment &&
         courtyard.Scene(1280, 720).linearTextureUploads.empty());
  Press(courtyard, Key::P);
  assert(courtyard.Scene(1280, 720).pbr);
  const auto wideMatrix = std::to_array(wide.model_view_projection);
  Press(courtyard, Key::B);
  const auto closeMatrix = std::to_array(courtyard.Scene(1280, 720).model_view_projection);
  assert(closeMatrix != wideMatrix);
  Press(courtyard, Key::B);
  const auto motionMatrix = std::to_array(courtyard.Scene(1280, 720).model_view_projection);
  assert(motionMatrix != closeMatrix && motionMatrix != wideMatrix);
  Press(courtyard, Key::B);
  assert(std::to_array(courtyard.Scene(1280, 720).model_view_projection) == wideMatrix);
  Nexora::Presentation::SurfaceDiagnostics diagnostics{};
  assert(!courtyard.Overlay(1280, 720, "validation", diagnostics, 0).vertices.empty());
  Press(courtyard, Key::F4);
  const auto hidden = courtyard.Overlay(1280, 720, "validation", diagnostics, 0);
  assert(hidden.vertices.empty() && hidden.commands.empty() && hidden.textureUploads.empty());
  Press(courtyard, Key::F4);
  assert(!courtyard.Overlay(1280, 720, "validation", diagnostics, 0).vertices.empty());
  Press(courtyard, Key::Digit1);
  assert(courtyard.Selected() == "hub");
  Press(courtyard, Key::Digit9);
  assert(courtyard.Selected() == "courtyard");
  assert(std::to_array(courtyard.Scene(1280, 720).model_view_projection) == wideMatrix);
#if NEXORA_ASSET_PIPELINE_ENABLED
  assert(courtyard.Report().find("\"representative_asset_loaded\":true") != std::string::npos);
  assert(courtyard.Report().find("\"adopted_mesh_count\":3") != std::string::npos);
  const auto adopted = courtyard.Scene(1280, 720);
  assert(adopted.textureId == 2 && adopted.textureUploads.size() == 10);
  assert(adopted.materials[0].normalTextureId == 11 && adopted.materials[1].ormTextureId == 15);
  assert(adopted.textureUploads[0].pixels.size() == 64 * 64 * 4);
  assert(adopted.textureUploads[1].width == 256 && adopted.textureUploads[1].height == 256);
  assert(adopted.textureUploads[1].pixels.size() == 256 * 256 * 4);
  assert(adopted.textureUploads.back().width == 384 && adopted.textureUploads.back().height == 256);
  assert(adopted.textureUploads.back().pixels.size() == 384 * 256 * 4);
#endif
  assert(courtyard.Scene(1280, 720).materials[5].alphaCutoff == 0.5F);
  courtyard.Tick(0.5);
  const float animatedTime = courtyard.Scene(1280, 720).vegetationTime;
  Press(courtyard, Key::Space);
  courtyard.Tick(0.5);
  assert(courtyard.Scene(1280, 720).vegetationTime == animatedTime);
  Press(courtyard, Key::Enter);
  const auto active = courtyard.Scene(1280, 720);
  assert(active.batches.back().materialIndex == 7 && active.batches.back().indexCount == 48 * 6);
  const std::vector<Nexora::Presentation::SceneVertex> frozen(active.vertices.begin(),
                                                              active.vertices.end());
  courtyard.Tick(0.5);
  const auto pausedScene = courtyard.Scene(1280, 720);
  assert(std::memcmp(frozen.data(), pausedScene.vertices.data(),
                     pausedScene.vertices.size_bytes()) == 0);
  Press(courtyard, Key::R);
  assert(courtyard.Scene(1280, 720).vegetationTime == 0);
  Press(courtyard, Key::Enter);
  assert(courtyard.Scene(1280, 720).batches.back().materialIndex != 7);
  session.RerunProbe(0, nexora::showcase::ErrorInjection::DependencyCycle);
  assert(session.Probes()[0].status == nexora::showcase::ProbeStatus::Unsupported);
  assert(session.Healthy());
  session.RerunProbe(0);
  assert(session.Markdown().find("sample_tick") != std::string::npos);
  const auto scene = session.Scene(1280, 720);
  assert(!scene.vertices.empty() && !scene.indices.empty());
  for (const auto index : scene.indices)
    assert(index < scene.vertices.size());
  const auto originalMatrix =
      std::array{scene.model_view_projection[0], scene.model_view_projection[2]};
  WindowEvent move{};
  move.type = WindowEventType::Key;
  move.value0 = static_cast<int>(Key::D);
  move.value1 = 1;
  session.Event(move, 1280, 720);
  session.Tick(0.5);
  const auto movedScene = session.Scene(1280, 720);
  assert(originalMatrix[0] != movedScene.model_view_projection[0] ||
         originalMatrix[1] != movedScene.model_view_projection[2]);
  move.value1 = 0;
  session.Event(move, 1280, 720);
  Press(session, Key::Digit2);
  const auto instanced = session.Scene(1280, 720);
  assert(instanced.vertices.size() == 24 && instanced.indices.size() == 36);
  assert(instanced.instances.size() == 4);
  assert(instanced.instances[0].scale[0] == 6 && instanced.instances[1].translation[1] == 1.5F);
  Press(session, Key::P);
  const auto quad = session.Scene(1280, 720);
  assert(quad.vertices.size() == 28 && quad.indices.size() == 42 && quad.instances.empty());
  Press(session, Key::P);
  const auto triangle = session.Scene(1280, 720);
  assert(triangle.vertices.size() == 27 && triangle.indices.size() == 39 &&
         triangle.instances.empty());
  Press(session, Key::P);
  assert(session.Scene(1280, 720).instances.size() == 4);
  Press(session, Key::Digit3);
  assert(session.Selected() == "scene");
#if NEXORA_EDITOR_SDK_ENABLED
  Press(session, Key::E);
  assert(session.Report().find("Transform.x = 0.50") != std::string::npos);
  Press(session, Key::U);
  assert(session.Report().find("Transform.x = 0.00") != std::string::npos);
  Press(session, Key::P);
  session.Tick(0.1);
  assert(session.Report().find("Transform.x = 0.00") != std::string::npos); // Play is isolated.
  Press(session, Key::F5);
  Press(session, Key::E);
  Press(session, Key::U);
  assert(session.Report().find("Transform.x = 0.00") != std::string::npos);
#endif
  for (const auto room : {"input", "gameplay", "presentation", "streaming", "shipping"}) {
    session.Select(room);
    session.Tick(1.0 / 60);
    assert(session.Healthy());
    assert(!session.Scene(960, 540).vertices.empty());
  }
#if NEXORA_GAMEPLAY_SIMULATION_ENABLED
  session.Select("gameplay");
  const auto capsule = session.Scene(1280, 720);
  assert(capsule.vertices.size() > 250);
  assert(capsule.vertices[24].position[1] >= -0.0001F); // Feet-origin capsule stays above ground.
  for (const auto &vertex : capsule.vertices) {
    const float length =
        std::sqrt(vertex.normal[0] * vertex.normal[0] + vertex.normal[1] * vertex.normal[1] +
                  vertex.normal[2] * vertex.normal[2]);
    assert(std::abs(length - 1) < 0.001F);
  }
#endif
#if NEXORA_PRESENTATION_ENABLED
  session.Select("presentation");
  const auto before = session.Scene(1280, 720).vertices[96]; // After floor and 3 joint markers.
  Press(session, Key::J);
  session.Tick(0.3);
  const auto after = session.Scene(1280, 720).vertices[96];
  assert(before.position[0] != after.position[0] || before.position[1] != after.position[1]);
#endif
  auto overlay = session.Overlay(1280, 720, "validation", {}, 16.67);
  assert(!overlay.vertices.empty() && overlay.textureUploads.size() == 1);
  overlay = session.Overlay(960, 540, "validation", {}, 16.67);
  assert(overlay.textureUploads.empty());
  Nexora::Presentation::SurfaceDiagnostics resized{};
  resized.resizeGenerations = 1;
  assert(session.Overlay(960, 540, "validation", resized, 16.67).textureUploads.size() == 1);
  Press(session, Key::L);
  assert(session.Report().find("Missing translation uses English fallback") != std::string::npos ||
         session.Selected() != "input");
  session.Select("input");
  assert(session.Report().find("Missing translation uses English fallback") != std::string::npos);
  RoomSession visualTour("courtyard", true);
  assert(visualTour.Selected() == "courtyard" && !visualTour.TourComplete());
  const auto initialTourMatrix = std::to_array(visualTour.Scene(1280, 720).model_view_projection);
  for (int i = 0; i < 25; ++i)
    visualTour.Tick(1);
  assert(std::to_array(visualTour.Scene(1280, 720).model_view_projection) != initialTourMatrix);
  Press(visualTour, Key::Space);
  const auto pausedTourMatrix = std::to_array(visualTour.Scene(1280, 720).model_view_projection);
  visualTour.Tick(1);
  assert(std::to_array(visualTour.Scene(1280, 720).model_view_projection) == pausedTourMatrix);
  Press(visualTour, Key::Space);
  for (int i = 25; i < 100; ++i) {
    visualTour.Tick(1);
    static_cast<void>(visualTour.Scene(1280, 720));
  }
  assert(visualTour.TourComplete());
  Press(visualTour, Key::J);
  assert(!visualTour.Scene(1280, 720).depthOfField);
  assert(visualTour.Report().find("\"duration_seconds\":100") != std::string::npos);
  visualTour.ReplayTour();
  assert(!visualTour.TourComplete() && visualTour.Scene(1280, 720).vegetationTime == 0);
  assert(visualTour.Scene(1280, 720).depthOfField);
  assert(std::to_array(visualTour.Scene(1280, 720).model_view_projection) == initialTourMatrix);
  RoomSession quality("courtyard");
  quality.SetAnimationPaused(true);
  quality.SetDeviceActive(true);
  std::array<std::size_t, 3> qualityVertices{};
  for (unsigned tier = 0; tier < 3; ++tier) {
    const auto name = std::array<std::string_view, 3>{"basic", "standard", "high"}[tier];
    quality.SetQuality(name);
    quality.Tick(0.1);
    const auto draw = quality.Scene(1280, 720);
    assert(draw.vertices.size() < 65536);
    assert(draw.hdr && draw.pbr && draw.shadow && draw.shadow->resolution == (512U << tier));
    assert(draw.vegetationTime == 0 && quality.QualityName() == name);
    assert(draw.bloom.has_value() == (tier != 0));
#if NEXORA_ASSET_PIPELINE_ENABLED
    assert(draw.environment.has_value() == (tier != 0));
#endif
    assert(draw.materials[6].unlit && !draw.materials[6].castsShadow);
    assert(draw.materials[7].unlit && !draw.materials[7].castsShadow);
    const auto particleBatch =
        std::find_if(draw.batches.begin(), draw.batches.end(),
                     [](const auto &batch) { return batch.materialIndex == 7; });
    assert(particleBatch != draw.batches.end() && particleBatch->indexCount == (24U << tier) * 6);
    qualityVertices[tier] = draw.vertices.size();
  }
  assert(qualityVertices[0] < qualityVertices[1] && qualityVertices[1] < qualityVertices[2]);
  Press(quality, Key::Q);
  assert(quality.QualityName() == "basic");
  assert(quality.Scene(1280, 720).vertices.size() == qualityVertices[0]);
  bool qualityRejected = false;
  try {
    quality.SetQuality("invalid");
  } catch (const std::invalid_argument &) {
    qualityRejected = true;
  }
  assert(qualityRejected && quality.QualityName() == "basic");

  RoomSession explore("courtyard");
  Press(explore, Key::C);
  const auto freeStart = std::to_array(explore.Scene(1280, 720).model_view_projection);
  WindowEvent freeMove{};
  freeMove.type = WindowEventType::Key;
  freeMove.value0 = static_cast<int>(Key::W);
  freeMove.value1 = 1;
  explore.Event(freeMove, 1280, 720);
  explore.Tick(0.5);
  freeMove.value1 = 0;
  explore.Event(freeMove, 1280, 720);
  assert(std::to_array(explore.Scene(1280, 720).model_view_projection) != freeStart);
  assert(explore.Report().find("\"camera_mode\":\"free\"") != std::string::npos);
  Press(explore, Key::B);
  assert(explore.Report().find("\"camera_mode\":\"orbit\"") != std::string::npos);
  session.ReplayTour();
  session.Tick(1);
  Press(session, Key::Space);
  for (int i = 0; i < 40; ++i)
    session.Tick(1);
  assert(session.Selected() == "hub");
  Press(session, Key::Space);
  for (int i = 0; i < 210; ++i)
    session.Tick(1);
  assert(session.Selected() == "shipping");
  assert(session.Report().find("\"paused\":true") != std::string::npos);
  assert(session.Report().find("\"step\":6") != std::string::npos);
  Press(session, Key::R);
  assert(session.Selected() == "hub");
  bool invalid = false;
  try {
    session.Tick(-1);
  } catch (const std::invalid_argument &) {
    invalid = true;
  }
  assert(invalid);
  invalid = false;
  try {
    session.Select("missing");
  } catch (const std::invalid_argument &) {
    invalid = true;
  }
  assert(invalid);
  RoomSession minimal("gameplay", false, true);
  minimal.Tick(0.1);
  assert(minimal.Healthy());
  assert(minimal.Probes()[8].status == nexora::showcase::ProbeStatus::Unsupported);
}
