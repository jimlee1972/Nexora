#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

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
void ExpansionPresentation(float dpi, bool macos) {
  Fixture f(dpi, macos);
  auto key = *f.scene.Key(f.roots.front());
  const auto expect = [&](bool open) {
    Require(Access::HierarchyRowExpanded(f.ui, key) == std::optional{open},
            "actual rendered branch expansion disagrees with current scoped state");
  };
  expect(false);
  Access::SetHierarchyExpanded(f.ui, std::array{key, *f.scene.Key(f.child)});
  f.Draw();
  expect(true);
  Require(Access::HierarchyRowExpanded(f.ui, *f.scene.Key(f.child)) == std::optional{true},
          "expanded descendant widget was not open");
  Access::QueueHierarchyExpansion(f.ui, key, false);
  f.Draw();
  expect(false);
  Require(!Access::HierarchyRowExpanded(f.ui, *f.scene.Key(f.child)),
          "clipped descendant exposed old widget expansion");
  auto wrong_entity = key;
  ++wrong_entity.entity_generation;
  auto wrong_document = key;
  ++wrong_document.document_generation;
  Access::SetHierarchyExpanded(f.ui, std::array{wrong_entity, wrong_document});
  f.Draw();
  expect(false);
  Access::SetHierarchyExpanded(f.ui, std::array{wrong_entity, key, wrong_document});
  f.Draw();
  expect(true);
  const auto old_key = key;
  Require(f.scene.Reload(f.root / "Content/Main.scene"), "same-ID expansion reload failed");
  key = *f.scene.Key(f.roots.front());
  f.Draw();
  Require(key != old_key && !Access::HierarchyRowExpanded(f.ui, old_key),
          "old scoped widget survived replacement");
  expect(false);
  f.FocusHierarchy();
  f.Press(Key::Home);
  f.Press(Key::RightArrow);
  expect(true);
  f.Press(Key::LeftArrow);
  expect(false);
  const auto click_arrow = [&] {
    const auto position = Access::HierarchyRootArrowPosition(f.ui, key);
    Require(position.has_value(), "root arrow is not rendered");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*position)[0] * dpi);
    pointer.value1 = static_cast<int>((*position)[1] * dpi);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    f.ui.ProcessEvents(std::array{pointer, button});
    f.Draw();
    button.value1 = 0;
    f.ui.ProcessEvents(std::array{button});
    f.Draw();
  };
  click_arrow();
  expect(true);
  // Distinct clicks: a rapid second click intentionally begins Hierarchy Rename.
  for (int i = 0; i < 25; ++i)
    f.Draw();
  click_arrow();
  expect(false);
  f.active = &f.observer;
  f.Draw();
  for (int i = 0; i < 25; ++i)
    f.Draw();
  click_arrow();
  expect(true);
  for (int i = 0; i < 25; ++i)
    f.Draw();
  click_arrow();
  expect(false);
  f.document = nullptr;
  f.Draw();
  Require(!Access::HierarchyRowExpanded(f.ui, key), "detached scene exposed old widget state");
  f.document = &f.scene;
  f.Draw();
  expect(false);
  Require(!f.scene.Dirty() && !f.scene.Undo() && !f.scene.Redo() &&
              f.world.SaveScene(f.scene_id) == f.baseline,
          "expansion observation changed saved source or authoring history");
}
void ExpansionPresentations() {
  for (const float dpi : {1.F, 2.F})
    for (const bool macos : {false, true})
      ExpansionPresentation(dpi, macos);
}

void StorageLifecycle(float dpi, bool macos) {
  Fixture f(dpi, macos);
  constexpr int marker = 0x4e58;
  Require(Access::SeedHierarchyStorageMarker(f.ui, marker), "storage marker setup failed");
  const auto initial = Access::HierarchyStorage(f.ui);
  Require(initial && initial->window_storage_active && initial->marker == marker,
          "initial hierarchy window storage is not active");
  std::size_t maximum_entries = initial->entries;
  const auto check = [&] {
    const auto storage = Access::HierarchyStorage(f.ui);
    Require(storage && storage->window_storage_active && storage->marker == marker,
            "tree drawing replaced the caller's storage or erased unrelated widget state");
    maximum_entries = std::max(maximum_entries, storage->entries);
    if (storage->entries > initial->entries)
      throw std::runtime_error("hierarchy window storage grew from " +
                               std::to_string(initial->entries) + " to " +
                               std::to_string(storage->entries) + " obsolete generation entries");
  };
  for (int cycle = 0; cycle < 128; ++cycle) {
    Require(f.scene.Reload(f.root / "Content/Main.scene"), "storage lifecycle reload failed");
    const auto root = *f.scene.Key(f.roots.front()), child = *f.scene.Key(f.child);
    Access::SetHierarchyExpanded(f.ui, std::array{root, child});
    f.Draw();
    Require(Access::HierarchyRowExpanded(f.ui, root) == std::optional{true} &&
                Access::HierarchyRowExpanded(f.ui, child) == std::optional{true},
            "storage lifecycle changed actual branch expansion");
    check();
    Access::QueueHierarchyExpansion(f.ui, root, false);
    f.Draw();
    Require(Access::HierarchyRowExpanded(f.ui, root) == std::optional{false},
            "storage lifecycle collapse was not rendered");
    check();
  }
  for (int cycle = 0; cycle < 32; ++cycle) {
    Require(f.scene.Select(std::array{f.roots.front()}) && f.scene.DeleteSelection(),
            "storage replacement delete failed");
    f.Draw();
    check();
    const auto replacement = f.scene.Create("Replacement root");
    Require(replacement && f.scene.Create("Replacement child", replacement),
            "storage replacement creation failed");
    const auto key = *f.scene.Key(replacement);
    Access::SetHierarchyExpanded(f.ui, std::array{key});
    const auto before = f.world.SaveScene(f.scene_id);
    f.Draw();
    f.FocusHierarchy();
    f.Press(Key::End); // The appended replacement branch must actually enter the clipper range.
    Require(Access::HierarchyRowExpanded(f.ui, key) == std::optional{true},
            "replacement branch was not rendered open");
    Require(f.world.SaveScene(f.scene_id) == before, "replacement observation changed the World");
    check();
    Require(f.scene.Reload(f.root / "Content/Main.scene"), "replacement restore failed");
    f.Draw();
    check();
  }
  f.document = nullptr;
  f.Draw();
  check();
  f.document = &f.scene;
  f.active = &f.observer;
  f.Draw();
  check();
  Require(!f.scene.Dirty() && !f.scene.Undo() && !f.scene.Redo() &&
              f.world.SaveScene(f.scene_id) == f.baseline,
          "widget storage lifecycle changed saved source/history");
  std::cout << "{\"scope\":\"hierarchy-window-storage\",\"dpi\":" << dpi
            << ",\"macos\":" << (macos ? "true" : "false")
            << ",\"reload_cycles\":128,\"replacement_cycles\":32,\"baseline_entries\":"
            << initial->entries << ",\"maximum_entries\":" << maximum_entries << "}\n";
}
void HiddenRequests(float dpi) {
  Fixture f(dpi, false);
  auto *window = ImGui::FindWindowByName("Hierarchy###nexora.hierarchy");
  Require(window != nullptr, "Hierarchy window missing");
  const auto hide = [&] {
    window->HiddenFramesCanSkipItems = 2;
    window->HiddenFramesCannotSkipItems = 0;
  };
  const auto check = [&](std::uint32_t rows) {
    const auto state = Access::Inspect(f.ui);
    Require(state.hierarchy_rendered_rows == 0 && state.hierarchy_visible_rows == rows,
            "Hidden Hierarchy rendered rows or lost its complete current row count");
  };
  Access::QueueHierarchySelection(f.ui, *f.scene.Key(f.roots[1]), false, false);
  hide();
  f.Draw();
  check(128);
  Require(f.Single(f.roots[1]), "Hidden pending selection was not applied");
  Access::QueueHierarchyMove(f.ui, *f.scene.Key(f.roots[1]), *f.scene.Key(f.roots[0]), 0);
  hide();
  f.Draw();
  Require(f.scene.Parent(f.roots[1]) == f.roots[0], "Hidden pending reparent was not applied");
  hide();
  f.Draw();
  check(127);
  Require(f.scene.Undo() && !f.scene.Dirty() && f.world.SaveScene(f.scene_id) == f.baseline,
          "Hidden reparent did not retain one-step Undo and source data");
  const auto old = *f.scene.Key(f.roots[1]);
  Require(f.scene.Reload(f.root / "Content/Main.scene"), "Hidden reload failed");
  Access::QueueHierarchySelection(f.ui, old, false, false);
  hide();
  f.Draw();
  check(128);
  Require(f.scene.Selection().empty() && !f.scene.Dirty() &&
              f.world.SaveScene(f.scene_id) == f.baseline,
          "Hidden stale request selected a replacement generation or changed source");
  f.document = nullptr;
  hide();
  f.Draw();
  check(0);
  f.document = &f.scene;
  window->HiddenFramesCanSkipItems = 0;
  window->HiddenFramesCannotSkipItems = 0;
  f.FocusHierarchy();
  const auto shown = Access::Inspect(f.ui);
  Require(shown.hierarchy_visible_rows == 128 && shown.hierarchy_rendered_rows > 0,
          "Hierarchy did not recover current rows after becoming visible");
}
void StorageLifecycles() {
  for (const float dpi : {1.F, 2.F})
    for (const bool macos : {false, true})
      StorageLifecycle(dpi, macos);
}
} // namespace
int main(int argc, char **argv) {
  try {
    if (argc == 2 && std::string_view(argv[1]) == "--hidden") {
      HiddenRequests(1);
      HiddenRequests(2);
      return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--storage") {
      StorageLifecycles();
      return 0;
    }
    if (argc == 2 && std::string_view(argv[1]) == "--expansion") {
      ExpansionPresentations();
      return 0;
    }
    Require(argc == 1, "expected no arguments, --expansion or --storage");
    StorageLifecycles();
    ExpansionPresentations();
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
