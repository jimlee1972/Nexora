#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Key = Nexora::Window::Key;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-hierarchy-context-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *workspace = &writer;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Context actions");
  editor::SceneDocument scene{world, scene_id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  runtime::Id first{}, second{}, child{};
  float dpi;
  std::optional<std::string> baseline;
  explicit Fixture(float scale) : dpi(scale) {
    Require(writer.Create(root, "Context actions") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "context workspace failed");
    first = scene.Create("First");
    second = scene.Create("Second");
    child = scene.Create("Child", first);
    Require(scene.SetCamera(*scene.Key(first), runtime::CameraComponent{45, .2, 200}) &&
                scene.SetOpaqueComponent(*scene.Key(first), {31, "Missing plugin", {1, 2, 3}}) &&
                scene.Save(root / "Content/Main.scene"),
            "context scene fixture failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1600 * dpi, 1000 * dpi, dpi);
    Access::ConfigureSyntheticInput(ui);
    Focus(true);
    for (int frame = 0; frame < 4; ++frame)
      Draw();
    Access::FocusHierarchy(ui);
    Draw();
    Draw();
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, workspace);
    static_cast<void>(ui.EndFrame());
  }
  void Focus(bool value) {
    Nexora::Window::WindowEvent event{};
    event.type = Nexora::Window::WindowEventType::FocusChanged;
    event.value0 = value;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Press(Key key, Nexora::Window::KeyModifiers modifiers = {}) {
    Nexora::Window::WindowEvent event{};
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = modifiers;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = {};
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Pointer(const std::array<float, 2> &point, int which) {
    Nexora::Window::WindowEvent position{}, button{};
    position.type = Nexora::Window::WindowEventType::Pointer;
    position.value0 = static_cast<int>(point[0] * dpi);
    position.value1 = static_cast<int>(point[1] * dpi);
    ui.ProcessEvents(std::array{position});
    Draw();
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = which;
    button.value1 = 1;
    ui.ProcessEvents(std::array{button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
  }
  void Open(runtime::Id target) {
    const auto row = Access::HierarchyRowPosition(ui, *scene.Key(target));
    Require(row.has_value(), "context target row is clipped");
    Pointer(*row, 1);
    Draw();
  }
  void Action(std::size_t index) {
    const auto point = Access::HierarchyContextPosition(ui, index);
    Require(point.has_value(), "Hierarchy object context action is absent");
    Pointer(*point, 0);
    Draw();
  }
  void Select(std::initializer_list<runtime::Id> values) {
    Require(scene.Select(std::span(values.begin(), values.size())), "context selection failed");
    Draw();
  }
  void Undo() {
    Require(scene.Undo() && world.SaveScene(scene_id) == baseline && !scene.Dirty(),
            "context action did not undo its complete subtree/payload in one step");
    Draw();
  }
};
void Actions(float dpi, bool narrow) {
  Fixture f(dpi);
  if (narrow) {
    auto *window = ImGui::FindWindowByName("Hierarchy###nexora.hierarchy");
    Require(window != nullptr, "Hierarchy window missing");
    ImGui::SetWindowDock(window, 0, ImGuiCond_Always);
    ImGui::SetWindowPos(window, {40, 80}, ImGuiCond_Always);
    ImGui::SetWindowSize(window, {150, 650}, ImGuiCond_Always);
    f.Draw();
    f.Draw();
  }
  f.Select({f.second});
  const auto sentinel = f.scene.Create("Retained Redo");
  Require(sentinel && f.scene.Undo(), "context Redo fixture failed");
  f.Draw();
  f.Open(f.first);
  Require(f.scene.Selection().size() == 1 && f.scene.Selection().front() == f.first,
          "right-click operated on the previously selected entity");
  for (std::size_t i = 0; i < 6; ++i)
    Require(Access::HierarchyContextPosition(f.ui, i).has_value(), "context action unreachable");
  f.Press(Key::Escape);
  Require(f.world.SaveScene(f.scene_id) == f.baseline && !f.scene.Dirty(),
          "opening/dismissing context menu changed authored data");
  Require(f.scene.Redo() && f.scene.Name(sentinel) == "Retained Redo" && f.scene.Undo(),
          "opening/dismissing context menu consumed Redo");
  f.Draw();
  f.Select({f.first, f.second});
  f.Open(f.first);
  Require(f.scene.Selection().size() == 2, "right-click discarded the selected group");
  f.Press(Key::Escape);
  f.Select({f.second});
  f.Open(f.first);
  f.Action(0); // Copy clicked subtree.
  Require(f.world.SaveScene(f.scene_id) == f.baseline && !f.scene.Dirty(), "Copy changed data");
  f.Open(f.second);
  f.Action(2);
  Require(f.scene.Nodes().size() == 5 && f.scene.Selection().size() == 1,
          "context Paste lost the subtree");
  const auto pasted = f.scene.Selection().front();
  Require(f.scene.Name(pasted) == "First Copy" && f.scene.Camera(*f.scene.Key(pasted)) &&
              f.scene.OpaqueComponents(*f.scene.Key(pasted))->front().data ==
                  std::vector<std::uint8_t>{1, 2, 3},
          "Paste lost owned component payloads");
  f.Undo();
  f.Open(f.first);
  f.Action(3);
  Require(f.scene.Nodes().size() == 5, "context Duplicate failed");
  f.Undo();
  f.Open(f.first);
  f.Action(1);
  Require(f.scene.Nodes().size() == 1 && f.scene.Name(f.second) == "Second",
          "context Cut target wrong");
  f.Undo();
  f.Select({f.first, f.second});
  f.Open(f.first);
  f.Action(5);
  Require(f.scene.Nodes().empty(), "context Delete lost multiselection");
  f.Undo();
  f.Open(f.first);
  f.Action(4);
  Require(!Access::HierarchyRenameOpen(f.ui), "group context admitted ambiguous Rename");
  f.Press(Key::Escape);
  f.Select({f.second});
  f.Open(f.first);
  f.Action(4);
  Require(Access::HierarchyRenameOpen(f.ui), "context Rename did not open normal modal");
  for (const unsigned char ch : std::string("Renamed")) {
    Nexora::Window::WindowEvent text{};
    text.type = Nexora::Window::WindowEventType::Text;
    text.value0 = ch;
    f.ui.ProcessEvents(std::array{text});
    f.Draw();
  }
  f.Press(Key::Enter);
  Require(f.scene.Name(f.first) == "Renamed" && !Access::HierarchyRenameOpen(f.ui),
          "context Rename bypassed the normal editor");
  f.Undo();
}
void AbandonDraft(float dpi) {
  Fixture f(dpi);
  f.Select({f.first});
  Access::FocusInspector(f.ui);
  Access::FocusInspectorTransformField(f.ui, 0);
  f.Draw();
  f.Draw();
  f.Press(Key::A, Nexora::Window::KeyModifiers::Control);
  for (const unsigned char ch : std::string("999")) {
    Nexora::Window::WindowEvent text{};
    text.type = Nexora::Window::WindowEventType::Text;
    text.value0 = ch;
    f.ui.ProcessEvents(std::array{text});
    f.Draw();
  }
  Require(Access::InspectorTransformText(f.ui, 0) == "999" &&
              f.world.SaveScene(f.scene_id) == f.baseline,
          "context draft fixture did not retain unsubmitted text");
  f.Open(f.first);
  f.Action(0);
  f.Press(Key::Enter);
  Require(f.world.SaveScene(f.scene_id) == f.baseline && !f.scene.Dirty(),
          "context menu revived an abandoned Inspector draft");
}
void Guards(float dpi) {
  Fixture f(dpi);
  f.workspace = &f.reader;
  f.Draw();
  f.Open(f.first);
  f.Action(0);
  f.Press(Key::Escape); // Copy may close the popup; dismiss no authoring modal here.
  for (const auto action : {1U, 2U, 3U, 4U, 5U}) {
    f.Open(f.first);
    f.Action(action);
    f.Press(Key::Escape);
    Require(f.world.SaveScene(f.scene_id) == f.baseline && !Access::HierarchyRenameOpen(f.ui),
            "read-only context admitted a write");
  }
  f.workspace = &f.writer;
  f.Draw();
  f.Open(f.second);
  f.Action(2);
  Require(f.scene.Nodes().size() == 5, "read-only context Copy did not retain usable data");
  f.Undo();
  f.Open(f.first);
  f.Press(Key::Delete);
  Require(f.world.SaveScene(f.scene_id) == f.baseline && Access::HierarchyContextPosition(f.ui, 5),
          "open menu leaked Delete to the underlying Hierarchy");
  f.Select({f.second});
  Require(!Access::HierarchyContextPosition(f.ui, 5), "selection change retained old context");
  f.Open(f.first);
  Require(f.scene.DeleteSelection(), "stale context fixture failed");
  f.Draw();
  Require(!Access::HierarchyContextPosition(f.ui, 5), "deleted entity retained context");
  f.Undo();
  f.Open(f.first);
  f.Focus(false);
  Require(!Access::HierarchyContextPosition(f.ui, 5), "focus loss retained context");
  f.Focus(true);
  f.Draw();
  Require(!Access::HierarchyContextPosition(f.ui, 5), "focus return revived cancelled context");
  f.Open(f.first);
  Require(f.scene.Reload(f.root / "Content/Main.scene"), "context reload fixture failed");
  f.Draw();
  Require(!Access::HierarchyContextPosition(f.ui, 5), "document reload retained context");
  f.Open(f.first);
  f.ui.RequestCloseConfirmation();
  f.Draw();
  Require(!Access::HierarchyContextPosition(f.ui, 5), "close modal retained actionable context");
  Require(f.world.SaveScene(f.scene_id) == f.baseline, "context guards changed committed scene");
}
} // namespace
int main() {
  try {
    for (const auto dpi : {1.0F, 2.0F}) {
      Actions(dpi, false);
      Actions(dpi, true);
      Guards(dpi);
      AbandonDraft(dpi);
    }
    std::cout << "Hierarchy object context contracts passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
