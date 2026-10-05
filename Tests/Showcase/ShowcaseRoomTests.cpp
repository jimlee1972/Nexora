#include "ShowcaseRooms.h"
#include <cassert>
#include <cmath>
#include <stdexcept>
#include <string>

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
  assert(wide.materials.size() == 6 && !wide.batches.empty());
  assert(Nexora::Presentation::ValidateSceneMaterials(wide.materials, wide.batches));
  std::size_t covered = 0;
  std::array<bool, 6> selectedMaterials{};
  for (const auto &batch : wide.batches) {
    assert(batch.firstIndex == covered && batch.firstInstance == 0 && batch.instanceCount == 1);
    covered += batch.indexCount;
    selectedMaterials[batch.materialIndex] = true;
  }
  assert(covered == wide.indices.size());
  for (const auto selected : selectedMaterials)
    assert(selected);
  for (const auto &vertex : wide.vertices) {
    float orthogonal = 0, length = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
      orthogonal += vertex.normal[axis] * vertex.tangent[axis];
      length += vertex.tangent[axis] * vertex.tangent[axis];
    }
    assert(std::abs(orthogonal) < 1e-4F && std::abs(length - 1) < 1e-4F);
    assert(std::abs(vertex.tangent[3]) == 1);
  }
  Press(courtyard, Key::P);
  assert(!courtyard.Scene(1280, 720).pbr);
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
  assert(adopted.textureId == 2 && adopted.textureUploads.size() == 1);
  assert(adopted.textureUploads[0].pixels.size() == 64 * 64 * 4);
#endif
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
