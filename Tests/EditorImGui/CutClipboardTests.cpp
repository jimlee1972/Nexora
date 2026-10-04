#include "EditorImGuiTestAccess.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-cut-clipboard-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace workspace, observer;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Cut clipboard");
  editor::SceneDocument scene{world, scene_id};
  runtime::Id a{}, child{}, b{}, prior{};
  std::vector<runtime::Id> selected;
  std::optional<std::string> baseline;
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  float scale;
  explicit Fixture(float dpi = 1) : scale(dpi) {
    Require(workspace.Create(root, "Cut Clipboard") &&
                observer.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "cut workspace failed");
    a = scene.Create("A");
    child = scene.Create("Child", a);
    b = scene.Create("B");
    prior = scene.Create("Prior");
    Require(scene.SetTransform(a, {3, 2, -1}) &&
                scene.SetCamera(*scene.Key(a), runtime::CameraComponent{72, 0.2, 800}) &&
                scene.SetMeshRenderer(*scene.Key(child), runtime::MeshComponent{51, {99}}) &&
                scene.SetOpaqueComponent(*scene.Key(child), {91, "Missing", {1, 2, 255}}) &&
                scene.Select(std::array{prior}) && scene.CopySelection(),
            "cut payload setup failed");
    selected = {b, child, a};
    Require(scene.Select(selected) && scene.Save(root / "Content/Before.scene"),
            "cut baseline failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1280, 900, scale);
    Access::SetInputTrickle(ui, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int frame = 0; frame < 4; ++frame)
      Draw();
    Access::FocusHierarchy(ui);
    Draw();
    Draw();
  }
  ~Fixture() { std::filesystem::remove_all(root); }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active);
    static_cast<void>(ui.EndFrame());
  }
  void Tap(Nexora::Window::Key key) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = Nexora::Window::KeyModifiers::Control;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void ClickCut() {
    const auto point = Access::HierarchyCutPosition(ui);
    Require(point.has_value(), "Hierarchy Cut button is absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer});
    Draw();
    ui.ProcessEvents(std::array{button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
  }
  void Unchanged() const {
    Require(world.SaveScene(scene_id) == baseline && scene.Nodes().size() == 4 &&
                std::ranges::equal(scene.Selection(), selected),
            "blocked Cut mutated the selection");
  }
  void VerifyPastedCut() {
    Require(scene.Selection().size() == 2 && scene.Nodes().size() == 4,
            "Cut Paste did not restore the complete forest");
    runtime::Id pasted_a{}, pasted_b{};
    for (const auto id : scene.Selection()) {
      if (scene.Name(id) == "A")
        pasted_a = id;
      if (scene.Name(id) == "B")
        pasted_b = id;
    }
    Require(pasted_a && pasted_b && pasted_a != a && pasted_b != b &&
                scene.Camera(*scene.Key(pasted_a))->vertical_field_of_view == 72 &&
                scene.Transform(pasted_a)->x == 3 && world.Children(pasted_a).size() == 1,
            "Cut Paste lost names, new identity or initialized root payloads");
    const auto pasted_child = world.Children(pasted_a).front();
    Require(scene.Name(pasted_child) == "Child" &&
                scene.MeshRenderer(*scene.Key(pasted_child))->material.shader == 99 &&
                scene.OpaqueComponents(*scene.Key(pasted_child))->front().data ==
                    std::vector<std::uint8_t>({1, 2, 255}),
            "Cut lost descendant data");
  }
};
} // namespace
int main() {
  try {
    using Key = Nexora::Window::Key;
    for (bool click : {false, true})
      for (float dpi : {1.0F, 2.0F}) {
        Fixture f(dpi);
        if (click)
          f.ClickCut();
        else
          f.Tap(Key::X);
        Require(f.shell.LastCommand() == "editor.scene.cut" && f.scene.Nodes().size() == 1 &&
                    f.scene.Name(f.prior) == "Prior" && f.scene.Selection().empty(),
                "Cut did not delete all selected subtrees");
        f.Tap(Key::Z);
        f.Unchanged();
        Require(!f.scene.Dirty(), "Cut Undo failed to restore saved cleanliness");
        f.Tap(Key::Y);
        f.Tap(Key::V);
        f.VerifyPastedCut();
        const auto pasted = f.world.SaveScene(f.scene_id);
        f.Tap(Key::Z);
        Require(f.scene.Nodes().size() == 1, "Cut Paste was not one creation Undo");
        f.Tap(Key::Y);
        Require(f.world.SaveScene(f.scene_id) == pasted,
                "Cut Paste Redo lost initialized identity");
        Require(f.scene.Save(f.root / "Content/Cut.scene") &&
                    f.scene.Reload(f.root / "Content/Cut.scene"),
                "Cut Paste persistence failed");
        Require(f.scene.Nodes().size() == 4, "Cut Paste persistence lost a subtree");
      }
    for (int gate = 0; gate < 4; ++gate) {
      Fixture f;
      if (gate == 0)
        f.active = &f.observer;
      if (gate == 1)
        std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\n";
      if (gate == 2) {
        Require(f.scene.SetTransform(f.prior, {20, 0, 0}), "close gate setup failed");
        f.baseline = f.world.SaveScene(f.scene_id);
        f.ui.RequestCloseConfirmation();
      }
      if (gate == 3) {
        Access::FocusInspectorTransformField(f.ui, 0);
        f.Draw();
        f.Draw();
      }
      f.Draw();
      f.Tap(Key::X);
      f.Unchanged();
      if (gate != 3) {
        f.ClickCut();
        f.Unchanged();
      }
      // Inspect the private clipboard only through its normal public Paste operation.
      Require(f.scene.Paste() && f.scene.Selection().size() == 1 &&
                  f.scene.Name(f.scene.Selection().front()) == "Prior Copy" && f.scene.Undo(),
              "blocked Cut replaced the previous clipboard");
      f.Unchanged();
    }
    {
      Fixture f;
      Require(f.scene.CutSelection() && f.scene.Select(std::span<const runtime::Id>{}) &&
                  !f.scene.CutSelection() && f.scene.Select(std::array{f.prior}) &&
                  f.scene.DuplicateSelection() && f.scene.Undo() && f.scene.Paste(),
              "failed Cut or Duplicate consumed a pending cut clipboard");
      f.VerifyPastedCut();
      Require(f.scene.Paste() && f.scene.Selection().size() == 2 && f.scene.Nodes().size() == 7,
              "retained Cut snapshot was unavailable for subsequent Copy-style Paste");
      for (const auto id : f.scene.Selection())
        Require(f.scene.Name(id) == "A Copy" || f.scene.Name(id) == "B Copy",
                "successful Cut Paste did not consume pending root-name preservation");
    }
    std::cout << "Graphical Cut and owning clipboard lifecycle contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
