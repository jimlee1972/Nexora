#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Key = Nexora::Window::Key;
using Mod = Nexora::Window::KeyModifiers;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-hierarchy-navigation-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace, observer;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Hierarchy navigation");
  editor::SceneDocument scene{world, scene_id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  editor::SceneDocument *document = &scene;
  std::vector<runtime::Id> roots;
  runtime::Id child{}, grandchild{}, sibling{};
  float dpi;
  std::optional<std::string> baseline;
  Fixture(float scale, bool macos) : dpi(scale) {
    Require(workspace.Create(root, "Hierarchy navigation") &&
                observer.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "project setup failed");
    for (int i = 0; i < 128; ++i)
      roots.push_back(scene.Create((i % 2 ? "Other " : "Match ") + std::to_string(i)));
    child = scene.Create("Match child", roots.front());
    grandchild = scene.Create("Match grandchild", child);
    sibling = scene.Create("Other child", roots.front());
    Require(child && grandchild && sibling && scene.Save(root / "Content/Main.scene"),
            "hierarchy setup failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1280 * dpi, 900 * dpi, dpi);
    Access::ConfigureSyntheticInput(ui, macos);
    Focus(true);
    for (int i = 0; i < 4; ++i)
      Draw();
    FocusHierarchy();
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, document, active);
    static_cast<void>(ui.EndFrame());
  }
  void FocusHierarchy() {
    Access::FocusHierarchy(ui);
    Draw();
    Draw();
  }
  void Focus(bool focused) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::FocusChanged;
    event.value0 = focused;
    ui.ProcessEvents(std::array{event});
  }
  void Press(Key key, Mod modifiers = Mod::None) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = modifiers;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = Mod::None;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  bool Single(runtime::Id id) const {
    return scene.Selection().size() == 1 && scene.Selection().front() == id;
  }
  void Click(runtime::Id id, bool shift = false) {
    const auto position = Access::HierarchyRowPosition(ui, *scene.Key(id));
    Require(position.has_value(), "hierarchy row is clipped");
    Nexora::Window::WindowEvent pointer, button, modifier;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*position)[0] * dpi);
    pointer.value1 = static_cast<int>((*position)[1] * dpi);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    modifier.type = Nexora::Window::WindowEventType::Key;
    modifier.value0 = static_cast<int>(Key::LeftShift);
    modifier.value1 = shift;
    modifier.modifiers = shift ? Mod::Shift : Mod::None;
    ui.ProcessEvents(std::array{pointer, modifier});
    Draw();
    ui.ProcessEvents(std::array{button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
    modifier.value1 = 0;
    modifier.modifiers = Mod::None;
    ui.ProcessEvents(std::array{modifier});
    Draw();
  }
};
void Navigation(float dpi, bool macos) {
  Fixture f(dpi, macos);
  const auto single = [&](runtime::Id id, const char *message) { Require(f.Single(id), message); };
  Require(!Access::HierarchyRowPosition(f.ui, *f.scene.Key(f.roots.back())),
          "fixture does not clip rows");
  const auto sentinel = f.scene.Create("Redo sentinel");
  Require(sentinel && f.scene.Undo(), "redo setup failed");
  f.Draw();
  f.Press(Key::DownArrow);
  single(f.roots[0], "initial Down did not select first root");
  f.Press(Key::DownArrow);
  single(f.roots[1], "Down included a collapsed descendant");
  f.Press(Key::DownArrow, Mod::Shift);
  Require(f.scene.Selection().size() == 2 && f.scene.Selection().front() == f.roots[1] &&
              f.scene.Selection().back() == f.roots[2],
          "Shift Down did not extend range");
  f.Press(Key::UpArrow, Mod::Shift);
  single(f.roots[1], "Shift Up did not shrink range");
  f.Press(Key::End, Mod::Shift);
  Require(f.scene.Selection().size() == 127 &&
              Access::HierarchyRowPosition(f.ui, *f.scene.Key(f.roots.back())),
          "Shift End missed clipped rows or reveal");
  f.Press(Key::Home, Mod::Shift);
  Require(f.scene.Selection().size() == 2 && f.scene.Selection().front() == f.roots[0],
          "Shift Home did not reverse around anchor");
  f.Press(Key::End);
  single(f.roots.back(), "End did not select last row");
  f.Press(Key::DownArrow);
  single(f.roots.back(), "Down escaped last row");
  f.Press(Key::Home);
  f.Press(Key::UpArrow);
  single(f.roots.front(), "Home Up did not clamp");
  Nexora::Window::WindowEvent held;
  held.type = Nexora::Window::WindowEventType::Key;
  held.value0 = static_cast<int>(Key::DownArrow);
  held.value1 = 1;
  f.ui.ProcessEvents(std::array{held});
  for (int i = 0; i < 45; ++i)
    f.Draw();
  held.value1 = 0;
  f.ui.ProcessEvents(std::array{held});
  f.Draw();
  Require(f.scene.Selection().size() == 1 && !f.Single(f.roots[0]) && !f.Single(f.roots[1]),
          "held Down did not repeat");
  f.Press(Key::Home);
  for (const auto modifier : {Mod::Control, Mod::Alt, Mod::Super})
    f.Press(Key::End, modifier);
  single(f.roots.front(), "modified End changed selection");
  f.Press(Key::RightArrow);
  single(f.roots.front(), "Right expansion moved selection");
  Require(Access::Inspect(f.ui).hierarchy_visible_rows == 130, "Right did not expand parent");
  f.Press(Key::RightArrow);
  single(f.child, "Right did not enter first child");
  f.Press(Key::RightArrow);
  Require(Access::Inspect(f.ui).hierarchy_visible_rows == 131 && f.Single(f.child),
          "Right did not expand nested child");
  f.Press(Key::RightArrow);
  single(f.grandchild, "Right did not enter grandchild");
  f.Press(Key::LeftArrow);
  single(f.child, "Left did not return to parent");
  f.Press(Key::LeftArrow);
  Require(Access::Inspect(f.ui).hierarchy_visible_rows == 130 && f.Single(f.child),
          "Left did not collapse expanded child");
  f.Press(Key::LeftArrow);
  single(f.roots.front(), "Left did not return to root");
  f.Press(Key::LeftArrow);
  Require(Access::Inspect(f.ui).hierarchy_visible_rows == 128, "Left did not collapse root");
  f.Press(Key::LeftArrow);
  single(f.roots.front(), "Left escaped root");
  f.Click(f.roots[0]);
  f.Click(f.roots[3], true);
  Require(f.scene.Selection().size() == 4, "Shift pointer did not use keyboard anchor");
  f.Press(Key::DownArrow, Mod::Shift);
  Require(f.scene.Selection().size() == 5 && f.scene.Selection().back() == f.roots[4],
          "keyboard cursor did not follow Shift pointer endpoint");
  f.active = &f.observer;
  f.FocusHierarchy();
  f.Press(Key::End);
  single(f.roots.back(), "read-only navigation blocked");
  f.Press(Key::Home);
  f.Press(Key::RightArrow);
  Require(Access::Inspect(f.ui).hierarchy_visible_rows == 130, "read-only expansion blocked");
  Require(f.scene.Select(std::array{f.roots[1]}), "hidden anchor setup failed");
  Access::SetHierarchyFilter(f.ui, "Match");
  f.Draw();
  f.Press(Key::End, Mod::Shift);
  single(f.grandchild, "filter change revived stale range anchor");
  f.Press(Key::Home, Mod::Shift);
  Require(f.scene.Selection().size() == 66, "filtered range excluded descendants or other rows");
  const auto filtered = std::vector(f.scene.Selection().begin(), f.scene.Selection().end());
  f.Press(Key::LeftArrow);
  f.Press(Key::RightArrow);
  Require(std::ranges::equal(filtered, f.scene.Selection()), "flat filter changed tree selection");
  Access::SetHierarchyFilter(f.ui, "No matching rows");
  f.Draw();
  f.Press(Key::DownArrow);
  f.Press(Key::Home, Mod::Shift);
  Require(std::ranges::equal(filtered, f.scene.Selection()), "empty rows changed selection");
  Access::SetHierarchyFilter(f.ui, "");
  f.active = &f.workspace;
  f.FocusHierarchy();
  f.Press(Key::End);
  f.Press(Key::F2);
  Require(Access::HierarchyRenameText(f.ui) == f.scene.Name(f.roots.back()),
          "navigation Rename did not use selected endpoint");
  f.Press(Key::Home);
  single(f.roots.back(), "Rename admitted navigation");
  f.Press(Key::Escape);
  Require(f.world.SaveScene(f.scene_id) == f.baseline && !f.scene.Dirty() && f.scene.Redo() &&
              f.scene.Name(sentinel) == "Redo sentinel",
          "navigation consumed redo or authored World");
  const auto old_key = *f.scene.Key(f.roots.back());
  Require(f.scene.Reload(f.root / "Content/Main.scene") && f.scene.Key(old_key.id) != old_key,
          "document generation replacement failed");
  f.FocusHierarchy();
  f.Press(Key::End, Mod::Shift);
  Require(f.Single(f.roots.back()) && !Access::HierarchyRowPosition(f.ui, old_key),
          "replacement document revived old anchor, cursor or row key");
  f.document = nullptr;
  f.Draw();
  f.document = &f.scene;
  f.FocusHierarchy();
  f.Press(Key::Home, Mod::Shift);
  Require(f.scene.Selection().size() == 128,
          "reattachment did not anchor to its current visible selection");
}
void DragGate() {
  Fixture f(2, false);
  Require(f.scene.Select(std::array{f.roots[3]}), "drag selection failed");
  f.Draw();
  const auto point = Access::HierarchyRowPosition(f.ui, *f.scene.Key(f.roots[3]));
  Require(point.has_value(), "drag row absent");
  Nexora::Window::WindowEvent pointer, button;
  pointer.type = Nexora::Window::WindowEventType::Pointer;
  pointer.value0 = static_cast<int>((*point)[0] * f.dpi);
  pointer.value1 = static_cast<int>((*point)[1] * f.dpi);
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer});
  f.Draw();
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
  pointer.value0 += 48;
  f.ui.ProcessEvents(std::array{pointer});
  f.Draw();
  f.Draw();
  Require(Access::HierarchyDragActive(f.ui), "hierarchy drag did not begin");
  f.Press(Key::Home);
  f.Press(Key::End, Mod::Shift);
  Require(f.Single(f.roots[3]) && f.world.SaveScene(f.scene_id) == f.baseline,
          "active Hierarchy drag admitted navigation");
  button.value1 = 0;
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
}
void PendingSceneDragGate() {
  Fixture f(1, false);
  Require(f.scene.Select(std::array{f.roots.back()}), "pending drag selection failed");
  f.ui.SetNativeScenePreview(true);
  Access::FocusScene(f.ui);
  f.Draw();
  f.Draw();
  const auto viewport = f.ui.NativeScenePreviewViewport();
  Require(viewport.has_value(), "native drag viewport absent");
  Nexora::Window::WindowEvent pointer, button;
  pointer.type = Nexora::Window::WindowEventType::Pointer;
  pointer.value0 = static_cast<int>(viewport->x + viewport->width / 2);
  pointer.value1 = static_cast<int>(viewport->y + viewport->height / 2);
  button.type = Nexora::Window::WindowEventType::PointerButton;
  button.value0 = 0;
  button.value1 = 1;
  f.ui.ProcessEvents(std::array{pointer});
  f.Draw();
  f.Press(Key::W);
  f.ui.ProcessEvents(std::array{button});
  f.Draw();
  pointer.value0 += 40;
  f.ui.ProcessEvents(std::array{pointer});
  f.Draw();
  Require(f.ui.NativeSceneDragPreview().has_value(), "native preview did not begin");
  // The release and Hierarchy command share a frame. The pending native transaction must
  // retain the selection that was captured while the pointer was held.
  Nexora::Window::WindowEvent home;
  home.type = Nexora::Window::WindowEventType::Key;
  home.value0 = static_cast<int>(Key::Home);
  home.value1 = 1;
  button.value1 = 0;
  Access::FocusHierarchy(f.ui);
  f.ui.ProcessEvents(std::array{button, home});
  f.Draw();
  const auto pending = f.ui.NativeSceneDrag();
  Require(pending.has_value() && f.Single(f.roots.back()) &&
              f.world.SaveScene(f.scene_id) == f.baseline,
          "release-frame navigation cancelled a native transaction or changed its selection");
  // Native requests are frame-local; after the owning host consumes the frame, navigation resumes.
  home.value1 = 0;
  f.ui.ProcessEvents(std::array{home});
  f.Draw();
  f.FocusHierarchy();
  f.Press(Key::Home);
  Require(f.Single(f.roots.front()), "resolved native drag did not restore navigation");
}
void Gates() {
  for (int gate = 0; gate < 5; ++gate) {
    Fixture f(1, false);
    Require(f.scene.Select(std::array{f.roots.back()}), "gate selection failed");
    f.Draw();
    if (gate == 0) {
      f.Focus(false);
      f.Draw();
    }
    if (gate == 1) {
      Access::FocusInspector(f.ui);
      f.Draw();
      f.Draw();
    }
    if (gate == 2) {
      Access::FocusInspectorTransformField(f.ui, 0);
      f.Draw();
      f.Draw();
    }
    if (gate == 3) {
      std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\n";
      f.Draw();
    }
    if (gate == 4) {
      f.ui.RequestCloseConfirmation();
      f.Draw();
    }
    f.Press(Key::Home);
    f.Press(Key::LeftArrow);
    f.Press(Key::DownArrow, Mod::Shift);
    if (!f.Single(f.roots.back()))
      throw std::runtime_error("blocked input changed Hierarchy selection: gate " +
                               std::to_string(gate));
  }
}
} // namespace
int main() {
  try {
    Navigation(1, false);
    Navigation(2, false);
    Navigation(1, true);
    Gates();
    DragGate();
    PendingSceneDragGate();
    std::cout << "Hierarchy keyboard navigation passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
