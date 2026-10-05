#include "EditorImGuiTestAccess.h"

#include <algorithm>
#include <array>
#include <chrono>
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
  Access::SetInputTrickle(ui, false);
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
  // Q and click are deliberately one input batch: tools resolve before drag setup.
  ui.ProcessEvents(std::array{pointer, key(Key::Q, true), button});
  draw();
  Require(ui.GetNativeSceneTool() == Tool::Select && ui.NativeScenePick().has_value() &&
              !ui.NativeScenePick()->additive,
          "same-frame Q/click lost selection picking");
  pointer.value0 += static_cast<int>(40 * dpi);
  pointer.value1 += static_cast<int>(20 * dpi);
  ui.ProcessEvents(std::array{pointer, key(Key::Q, false)});
  draw();
  Require(!ui.NativeSceneDragPreview(), "Select created a transform preview");
  button.value1 = 0;
  ui.ProcessEvents(std::array{button});
  draw();
  Require(!ui.NativeSceneDrag(), "Select created a released transform transaction");
  ui.ProcessEvents(std::array{key(Key::LeftControl, true)});
  button.value1 = 1;
  ui.ProcessEvents(std::array{button});
  draw();
  Require(ui.NativeScenePick() && ui.NativeScenePick()->additive,
          "Select lost Ctrl-toggle picking");
  button.value1 = 0;
  ui.ProcessEvents(std::array{button, key(Key::LeftControl, false)});
  draw();
  press(Key::Home); // ImGui navigation must not replace the actual canvas hover check.
  press(Key::W);
  Require(ui.GetNativeSceneTool() == Tool::Move, "W did not restore Move");
  press(Key::X);
  press(Key::P);
  Require(ui.NativeSceneLocalAxes() && ui.NativeSceneCenterPivot(),
          "Home navigation blocked gizmo mode shortcuts");
  press(Key::X);
  press(Key::P);
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
  Require(ui.GetNativeSceneTool() == Tool::Select, "read-only tool navigation was rejected");
  press(Key::X);
  press(Key::P);
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
    press(Key::P);
    Require(!ui.NativeSceneLocalAxes() && !ui.NativeSceneCenterPivot(),
            "blocked input changed gizmo settings");
  };
  Access::FocusHierarchy(ui);
  draw();
  press(Key::W);
  blocked_modes();
  Require(ui.GetNativeSceneTool() == Tool::Select, "Scene hover stole another panel's key");
  Access::FocusScene(ui);
  draw();
  focus.value0 = 0;
  ui.ProcessEvents(std::array{focus});
  draw();
  press(Key::W);
  blocked_modes();
  Require(ui.GetNativeSceneTool() == Tool::Select, "unfocused W changed the Scene tool");
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  draw();
  ui.RequestCloseConfirmation();
  draw();
  press(Key::W);
  blocked_modes();
  Require(ui.GetNativeSceneTool() == Tool::Select, "modal W changed the Scene tool");
  press(Key::Escape);
  Require(ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
          "tool close fixture did not cancel");
  workspace = &writer;
  Access::FocusInspector(ui);
  Access::FocusInspectorTransformField(ui, 0);
  draw();
  press(Key::W);
  blocked_modes();
  Require(ui.GetNativeSceneTool() == Tool::Select, "text input changed the Scene tool");
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
