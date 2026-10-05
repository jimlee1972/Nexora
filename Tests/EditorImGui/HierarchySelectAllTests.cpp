#include "EditorImGuiTestAccess.h"

#include <algorithm>
#include <chrono>
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
      ("nexora-select-all-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace workspace, observer;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Select all");
  editor::SceneDocument scene{world, scene_id};
  std::vector<runtime::Id> roots, matching;
  runtime::Id child{};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::optional<std::string> baseline;
  Fixture() {
    Require(workspace.Create(root, "Select All") &&
                observer.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "workspace failed");
    for (int i = 0; i < 128; ++i) {
      const auto id = scene.Create((i % 2 ? "Other " : "Match ") + std::to_string(i));
      roots.push_back(id);
      if (i % 2 == 0)
        matching.push_back(id);
    }
    child = scene.Create("Match child", roots.front());
    matching.push_back(child);
    Require(scene.Select(std::array{roots.back()}) && scene.Save(root / "Content/Main.scene"),
            "baseline failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1280, 900, 1);
    Access::ConfigureSyntheticInput(ui);
    for (int i = 0; i < 4; ++i)
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
  void SelectAll() {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(Nexora::Window::Key::A);
    event.value1 = 1;
    event.modifiers = Nexora::Window::KeyModifiers::Control;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    Draw();
    Require(world.SaveScene(scene_id) == baseline && !scene.Dirty(),
            "selection authored the World");
  }
};
} // namespace
int main() {
  try {
    {
      Fixture f;
      const auto sentinel = f.scene.Create("Redo sentinel");
      Require(sentinel && f.scene.Undo(), "Redo setup failed");
      f.Draw();
      f.SelectAll();
      Require(f.shell.LastCommand() == "editor.scene.select-all" &&
                  std::ranges::equal(f.scene.Selection(), f.roots) &&
                  Access::Inspect(f.ui).hierarchy_rendered_rows < f.roots.size(),
              "Select All missed clipped roots or included collapsed children");
      Access::QueueHierarchyExpansion(f.ui, *f.scene.Key(f.roots.front()), true);
      f.Draw();
      f.SelectAll();
      auto expanded = f.roots;
      expanded.insert(expanded.begin() + 1, f.child);
      Require(std::ranges::equal(f.scene.Selection(), expanded),
              "Select All missed expanded descendants");
      Access::SetHierarchyFilter(f.ui, "match");
      f.Draw();
      f.SelectAll();
      Require(std::ranges::equal(f.scene.Selection(), f.matching),
              "Select All ignored case-insensitive filtering or row order");
      f.active = &f.observer;
      Access::SetHierarchyFilter(f.ui, "");
      f.Draw();
      f.SelectAll();
      Require(std::ranges::equal(f.scene.Selection(), expanded), "read-only selection was blocked");
      Access::SetHierarchyFilter(f.ui, "No matching rows");
      f.Draw();
      f.SelectAll();
      Require(f.scene.Selection().empty() && f.scene.Redo() &&
                  f.scene.Name(sentinel) == "Redo sentinel",
              "empty filter selection consumed history");
    }
    for (int gate = 0; gate < 4; ++gate) {

      Fixture f;
      const auto selected = std::vector(f.scene.Selection().begin(), f.scene.Selection().end());
      if (gate == 0) {
        Access::FocusInspectorTransformField(f.ui, 0);
        f.Draw();
        f.Draw();
      }
      if (gate == 1) {
        Access::FocusInspector(f.ui);
        f.Draw();
        f.Draw();
      }
      if (gate == 2) {
        std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\n";
        f.Draw();
      }
      if (gate == 3) {
        f.ui.RequestCloseConfirmation();
        f.Draw();
      }
      f.SelectAll();
      Require(std::ranges::equal(f.scene.Selection(), selected),
              "text/foreign panel/modal focus admitted Hierarchy Select All");
    }
    std::cout << "Focused Hierarchy Select All contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
