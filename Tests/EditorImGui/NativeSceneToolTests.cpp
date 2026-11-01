#include "EditorImGuiTestAccess.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Tool = editor::imgui::NativeSceneTool;
using Access = editor::imgui::EditorImGuiTestAccess;
using Event = Nexora::Window::WindowEvent;
using EventType = Nexora::Window::WindowEventType;
using Key = Nexora::Window::Key;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(float dpi) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-native-tools-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  Require(writer.Create(root, "Native tools") && reader.Open(root, editor::ProjectAccess::ReadOnly),
          "tool workspace failed");
  runtime::World world;
  const auto scene_id = world.LoadScene("Tool gestures");
  Require(world.Activate(scene_id), "tool world activation failed");
  editor::SceneDocument scene(world, scene_id);
  const auto entity = scene.Create("Selected");
  Require(scene.Select(std::array{entity}) && scene.Save(root / "Content/Tools.scene"),
          "tool scene baseline failed");
  const auto redo = scene.Create("Retained Redo");
  Require(scene.Undo(), "tool history fixture failed");
  const auto baseline = world.SaveScene(scene_id);
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  ui.SetDisplay(1600, 1000, dpi);
  ui.SetNativeScenePreview(true);
  Access::ConfigureSyntheticInput(ui);
  editor::ProjectWorkspace *workspace = &writer;
  const auto draw = [&] {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, workspace);
    static_cast<void>(ui.EndFrame());
  };
  Event focus{};
  focus.type = EventType::FocusChanged;
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  for (int i = 0; i < 4; ++i)
    draw();
  Access::FocusScene(ui);
  draw();
  const auto viewport = ui.NativeScenePreviewViewport();
  Require(viewport.has_value(), "tool viewport missing");
  Event pointer{}, button{};
  pointer.type = EventType::Pointer;
  pointer.value0 = static_cast<int>(viewport->x + viewport->width / 2);
  pointer.value1 = static_cast<int>(viewport->y + viewport->height / 2);
  button.type = EventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  const auto key = [&](Key value, bool down) {
    Event event{};
    event.type = EventType::Key;
    event.value0 = static_cast<int>(value);
    event.value1 = down ? 1 : 0;
    if (value == Key::LeftControl && down)
      event.modifiers = Nexora::Window::KeyModifiers::Control;
    return event;
  };
  const auto press = [&](Key value) {
    ui.ProcessEvents(std::array{key(value, true)});
    draw();
    ui.ProcessEvents(std::array{key(value, false)});
    draw();
  };
  // Q must pan the view without picking, authoring, or producing a transform gesture.
  const auto camera_before = ui.GetSceneOverviewCamera();
  ui.ProcessEvents(std::array{pointer, key(Key::Q, true), button});
  draw();
  Require(ui.GetNativeSceneTool() == Tool::Hand && !ui.NativeScenePick(),
          "same-frame Q/click did not enter the Hand tool");
  pointer.value0 += static_cast<int>(40 * dpi);
  pointer.value1 += static_cast<int>(20 * dpi);
  ui.ProcessEvents(std::array{pointer, key(Key::Q, false)});
  draw();
  Require(!ui.NativeSceneDragPreview() && ui.GetSceneOverviewCamera().x != camera_before.x,
          "Hand did not pan or created a transform preview");
  button.value1 = 0;
  ui.ProcessEvents(std::array{button});
  draw();
  Require(!ui.NativeSceneDrag(), "Hand created a released transform transaction");
  press(Key::W);
  const auto space_camera = ui.GetSceneOverviewCamera();
  ui.ProcessEvents(std::array{key(Key::Space, true)});
  draw();
  button.value1 = 1;
  ui.ProcessEvents(std::array{button});
  draw();
  pointer.value0 += static_cast<int>(12 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(ui.GetNativeSceneTool() == Tool::Move && !ui.NativeScenePick() &&
              !ui.NativeSceneDragPreview() && ui.GetSceneOverviewCamera().x != space_camera.x,
          "Space pan changed tools, selected geometry, or failed to move the camera");
  button.value1 = 0;
  ui.ProcessEvents(std::array{button, key(Key::Space, false)});
  draw();
  // The explicit Select toolbar remains available for picking without a transform.
  const auto select_initial = Access::NativeSceneToolPosition(ui, Tool::Select);
  Require(select_initial.has_value(), "Select toolbar position missing");
  auto toolbar_pointer = pointer;
  toolbar_pointer.value0 = static_cast<int>((*select_initial)[0] * dpi);
  toolbar_pointer.value1 = static_cast<int>((*select_initial)[1] * dpi);
  button.value1 = 1;
  ui.ProcessEvents(std::array{toolbar_pointer, button});
  draw();
  button.value1 = 0;
  ui.ProcessEvents(std::array{button});
  draw();
  Require(ui.GetNativeSceneTool() == Tool::Select, "Select toolbar did not select");
  ui.ProcessEvents(std::array{pointer});
  draw();
  ui.ProcessEvents(std::array{key(Key::LeftControl, true)});
  button.value1 = 1;
  ui.ProcessEvents(std::array{button});
  draw();
  Require(ui.NativeScenePick() && ui.NativeScenePick()->additive,
          "Select lost Ctrl-toggle picking");
  button.value1 = 0;
  ui.ProcessEvents(std::array{button, key(Key::LeftControl, false)});
  draw();
  const auto eye = [&] {
    const auto center = ui.GetSceneOverviewCamera();
    const auto orbit = ui.GetNativeSceneOrbit();
    return std::array{center.x + orbit.distance * std::sin(orbit.yaw) * std::cos(orbit.pitch),
                      orbit.target_y + orbit.distance * std::sin(orbit.pitch),
                      center.z + orbit.distance * std::cos(orbit.yaw) * std::cos(orbit.pitch)};
  };
  const auto near = [](const auto &a, const auto &b) {
    for (std::size_t i = 0; i < a.size(); ++i)
      if (std::abs(a[i] - b[i]) > 0.0001)
        return false;
    return true;
  };
  const auto initial_eye = eye();
  auto right = button;
  right.value0 = 1;
  right.value1 = 1;
  ui.ProcessEvents(std::array{right});
  draw();
  const auto look_before = ui.GetNativeSceneOrbit();
  pointer.value0 += static_cast<int>(24 * dpi);
  pointer.value1 -= static_cast<int>(18 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(near(initial_eye, eye()) && ui.GetNativeSceneOrbit().yaw != look_before.yaw &&
              ui.GetNativeSceneOrbit().pitch < look_before.pitch,
          "RMB look moved the eye or turned vertically in the wrong direction");
  const auto fly_distance = ui.GetNativeSceneOrbit().distance;
  Event wheel{};
  wheel.type = EventType::Wheel;
  wheel.value1 = 120;
  ui.ProcessEvents(std::array{wheel});
  draw();
  Require(ui.GetNativeSceneOrbit().distance == fly_distance && near(initial_eye, eye()),
          "fly wheel zoomed the camera instead of changing speed");
  const auto look_eye = eye();
  ui.ProcessEvents(std::array{key(Key::W, true), key(Key::Q, true)});
  draw();
  Require(ui.GetNativeSceneTool() == Tool::Select && !near(look_eye, eye()) &&
              !ui.NativeScenePick() && !ui.NativeSceneDragPreview(),
          "fly keys changed tools, failed to move, or entered authoring");
  right.value1 = 0;
  ui.ProcessEvents(std::array{right, key(Key::W, false), key(Key::Q, false)});
  draw();
  auto alt = key(Key::LeftAlt, true);
  alt.modifiers = Nexora::Window::KeyModifiers::Alt;
  ui.ProcessEvents(std::array{alt});
  draw();
  const auto orbit_center = ui.GetSceneOverviewCamera();
  const auto orbit_height = ui.GetNativeSceneOrbit().target_y;
  button.value1 = 1;
  ui.ProcessEvents(std::array{button});
  draw();
  pointer.value0 -= static_cast<int>(22 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(ui.GetSceneOverviewCamera().x == orbit_center.x &&
              ui.GetSceneOverviewCamera().z == orbit_center.z &&
              ui.GetNativeSceneOrbit().target_y == orbit_height && !ui.NativeScenePick(),
          "Alt+LMB orbit moved the pivot or selected an entity");
  button.value1 = 0;
  ui.ProcessEvents(std::array{button});
  draw();
  const auto zoom_distance = ui.GetNativeSceneOrbit().distance;
  right.value1 = 1;
  ui.ProcessEvents(std::array{right});
  draw();
  pointer.value0 += static_cast<int>(18 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(ui.GetNativeSceneOrbit().distance > zoom_distance && !ui.NativeScenePick(),
          "Alt+RMB did not zoom without picking");
  right.value1 = 0;
  ui.ProcessEvents(std::array{right, key(Key::LeftAlt, false)});
  draw();
  auto middle = button;
  middle.value0 = 2;
  middle.value1 = 1;
  ui.ProcessEvents(std::array{middle});
  draw();
  const auto pan_height = ui.GetNativeSceneOrbit().target_y;
  const auto pan_yaw = ui.GetNativeSceneOrbit().yaw;
  pointer.value1 += static_cast<int>(17 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(ui.GetNativeSceneOrbit().target_y > pan_height &&
              ui.GetNativeSceneOrbit().yaw == pan_yaw && !ui.NativeSceneDragPreview(),
          "MMB did not pan in the camera plane");
  middle.value1 = 0;
  ui.ProcessEvents(std::array{middle});
  draw();
  press(Key::Home); // ImGui navigation must not replace the actual canvas hover check.
  press(Key::W);
  Require(ui.GetNativeSceneTool() == Tool::Move, "W did not restore Move");
  press(Key::X);
  press(Key::Z);
  Require(ui.NativeSceneLocalAxes() && ui.NativeSceneCenterPivot(),
          "Home navigation blocked gizmo mode shortcuts");
  press(Key::X);
  press(Key::Z);
  Require(!ui.NativeSceneLocalAxes() && !ui.NativeSceneCenterPivot(),
          "gizmo mode shortcuts did not toggle back");
  // Resolve both modes before the same-frame press starts a gesture.
  button.value1 = 1;
  ui.ProcessEvents(std::array{key(Key::X, true), key(Key::P, true), button});
  draw();
  Require(ui.NativeSceneLocalAxes() && ui.NativeSceneCenterPivot(),
          "same-frame mode/click used old gizmo settings");
  ui.ProcessEvents(std::array{key(Key::X, false), key(Key::P, false)});
  draw();
  pointer.value0 += static_cast<int>(40 * dpi);
  ui.ProcessEvents(std::array{pointer, key(Key::Q, true), key(Key::X, true), key(Key::P, true)});
  draw();
  Require(ui.GetNativeSceneTool() == Tool::Move && ui.NativeSceneDragPreview() &&
              ui.NativeSceneLocalAxes() && ui.NativeSceneCenterPivot(),
          "Q interrupted an active Move gesture");
  ui.ProcessEvents(std::array{key(Key::X, false), key(Key::P, false)});
  draw();
  button.value1 = 0;
  ui.ProcessEvents(std::array{button, key(Key::Q, false), key(Key::E, true), key(Key::X, true),
                              key(Key::P, true)});
  draw();
  Require(ui.GetNativeSceneTool() == Tool::Move && ui.NativeSceneDrag() &&
              ui.NativeSceneLocalAxes() && ui.NativeSceneCenterPivot(),
          "E changed tools before a released Move gesture committed");
  ui.ProcessEvents(std::array{key(Key::E, false), key(Key::X, false), key(Key::P, false)});
  draw();
  press(Key::E);
  Require(ui.GetNativeSceneTool() == Tool::Rotate, "E did not restore Rotate");
  press(Key::R);
  Require(ui.GetNativeSceneTool() == Tool::Scale && ui.NativeSceneLocalAxes(),
          "R did not retain Scale local axes");
  press(Key::X);
  Require(ui.NativeSceneLocalAxes(), "X enabled unsupported world-axis Scale");
  // Ctrl can snap a transform handle from the initial press. Native hit testing owns
  // admission; a Ctrl-click on an ordinary entity cancels the tentative gesture.
  ui.ProcessEvents(std::array{key(Key::LeftControl, true)});
  draw();
  button.value1 = 1;
  ui.ProcessEvents(std::array{button});
  draw();
  Require(ui.NativeScenePick() && ui.NativeScenePick()->additive,
          "Ctrl transform press lost its selection intent");
  pointer.value0 += static_cast<int>(20 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(ui.NativeSceneDragPreview() && ui.NativeSceneDragPreview()->vertical &&
              ui.NativeSceneDragPreview()->snap_step > 0,
          "Ctrl did not enable incremental transform snapping");
  ui.CancelNativeSceneTransform();
  Require(!ui.NativeSceneDragPreview() && !ui.NativeSceneDrag(),
          "ordinary Ctrl pick did not cancel the tentative transform");
  pointer.value0 += static_cast<int>(10 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(!ui.NativeSceneDragPreview(), "canceled Ctrl gesture restarted while held");
  button.value1 = 0;
  ui.ProcessEvents(std::array{button, key(Key::LeftControl, false)});
  draw();
  // Actual toolbar pointer input follows the same tool state as Q.
  const auto select = Access::NativeSceneToolPosition(ui, Tool::Select);
  Require(select.has_value(), "Select toolbar widget missing");
  pointer.value0 = static_cast<int>((*select)[0] * dpi);
  pointer.value1 = static_cast<int>((*select)[1] * dpi);
  button.value1 = 1;
  ui.ProcessEvents(std::array{pointer, button});
  draw();
  button.value1 = 0;
  ui.ProcessEvents(std::array{button});
  draw();
  Require(ui.GetNativeSceneTool() == Tool::Select && !ui.NativeScenePick(),
          "Select toolbar did not switch cleanly");
  pointer.value0 = static_cast<int>(viewport->x + viewport->width / 2);
  pointer.value1 = static_cast<int>(viewport->y + viewport->height / 2);
  ui.ProcessEvents(std::array{pointer});
  draw();
  workspace = &reader;
  press(Key::W);
  press(Key::Q);
  Require(ui.GetNativeSceneTool() == Tool::Hand, "read-only tool navigation was rejected");
  press(Key::X);
  press(Key::Z);
  Require(!ui.NativeSceneLocalAxes() && !ui.NativeSceneCenterPivot(),
          "read-only gizmo mode navigation was rejected");
  for (const auto modifier :
       {Nexora::Window::KeyModifiers::Control, Nexora::Window::KeyModifiers::Alt,
        Nexora::Window::KeyModifiers::Super}) {
    for (const auto value : {Key::X, Key::P}) {
      auto event = key(value, true);
      event.modifiers = modifier;
      ui.ProcessEvents(std::array{event});
      draw();
      ui.ProcessEvents(std::array{key(value, false)});
      draw();
    }
  }
  Require(!ui.NativeSceneLocalAxes() && !ui.NativeSceneCenterPivot(),
          "modified shortcuts toggled gizmo settings");
  const auto blocked_modes = [&] {
    press(Key::X);
    press(Key::Z);
    Require(!ui.NativeSceneLocalAxes() && !ui.NativeSceneCenterPivot(),
            "blocked input changed gizmo settings");
  };
  Access::FocusHierarchy(ui);
  draw();
  press(Key::W);
  Require(ui.GetNativeSceneTool() == Tool::Move, "Hierarchy focus blocked the global Move tool");
  press(Key::X);
  press(Key::Z);
  Require(ui.NativeSceneLocalAxes() && ui.NativeSceneCenterPivot(),
          "Hierarchy focus blocked global axis/pivot shortcuts");
  press(Key::X);
  press(Key::Z);
  press(Key::Q);
  Access::FocusScene(ui);
  draw();
  focus.value0 = 0;
  ui.ProcessEvents(std::array{focus});
  draw();
  press(Key::W);
  blocked_modes();
  Require(ui.GetNativeSceneTool() == Tool::Hand, "unfocused W changed the Scene tool");
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  draw();
  ui.RequestCloseConfirmation();
  draw();
  press(Key::W);
  blocked_modes();
  Require(ui.GetNativeSceneTool() == Tool::Hand, "modal W changed the Scene tool");
  press(Key::Escape);
  Require(ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
          "tool close fixture did not cancel");
  workspace = &writer;
  // The overview has the same Hand admission, plus two-dimensional Alt+RMB zoom.
  ui.SetNativeScenePreview(false);
  Access::FocusScene(ui);
  draw();
  const auto overview = ui.SceneCanvasViewport();
  Require(overview.has_value(), "overview viewport absent");
  pointer.value0 = static_cast<int>(overview->x + overview->width / 2);
  pointer.value1 = static_cast<int>(overview->y + overview->height / 2);
  ui.ProcessEvents(std::array{pointer});
  draw();
  const auto overview_before = ui.GetSceneOverviewCamera();
  button.value1 = 1;
  ui.ProcessEvents(std::array{button});
  draw();
  pointer.value0 += static_cast<int>(24 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(ui.GetSceneOverviewCamera().x != overview_before.x && !ui.NativeSceneDragPreview(),
          "overview Hand did not pan without authoring");
  button.value1 = 0;
  ui.ProcessEvents(std::array{button, alt});
  draw();
  const auto overview_scale = ui.GetSceneOverviewCamera().pixels_per_unit;
  right.value1 = 1;
  ui.ProcessEvents(std::array{right});
  draw();
  pointer.value0 += static_cast<int>(16 * dpi);
  ui.ProcessEvents(std::array{pointer});
  draw();
  Require(ui.GetSceneOverviewCamera().pixels_per_unit < overview_scale,
          "overview Alt+RMB did not zoom");
  right.value1 = 0;
  ui.ProcessEvents(std::array{right, key(Key::LeftAlt, false)});
  draw();
  ui.SetNativeScenePreview(true);
  Access::FocusInspector(ui);
  Access::FocusInspectorTransformField(ui, 0);
  draw();
  press(Key::W);
  blocked_modes();
  Require(ui.GetNativeSceneTool() == Tool::Hand, "text input changed the Scene tool");
  Require(world.SaveScene(scene_id) == baseline && !scene.Dirty() &&
              std::ranges::equal(scene.Selection(), std::array{entity}) && scene.Redo() &&
              scene.Name(redo) == "Retained Redo",
          "tool navigation mutated authoring/history");
  reader = editor::ProjectWorkspace{};
  writer = editor::ProjectWorkspace{};
  std::filesystem::remove_all(root);
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Native Scene selection tool contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
