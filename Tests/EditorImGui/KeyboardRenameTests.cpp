#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <fstream>
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
      ("nexora-keyboard-rename-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace workspace, observer;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Keyboard rename");
  editor::SceneDocument scene{world, scene_id};
  runtime::PlaySession play{world};
  runtime::Id first{}, second{}, sentinel{};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::optional<std::string> baseline;
  float scale;
  Fixture(float dpi = 1) : scale(dpi) {
    Require(workspace.Create(root, "Keyboard Rename") &&
                observer.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "workspace failed");
    first = scene.Create("Original");
    second = scene.Create("Clipboard");
    Require(scene.Select(std::array{second}) && scene.CopySelection() &&
                scene.Select(std::array{first}) && scene.Save(Path()),
            "baseline failed");
    baseline = world.SaveScene(scene_id);
    sentinel = scene.Create("Redo sentinel");
    Require(sentinel && scene.Undo(), "Redo setup failed");
    ui.SetDisplay(1280, 900, scale);
    Access::SetInputTrickle(ui, false);
    Focus(true);
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusHierarchy(ui);
    Draw();
    Draw();
  }
  ~Fixture() { std::filesystem::remove_all(root); }
  std::filesystem::path Path() const { return root / "Content/Main.scene"; }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Tap(Key key, bool control = false) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers =
        control ? Nexora::Window::KeyModifiers::Control : Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Text(std::u32string_view value) {
    for (const auto codepoint : value) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = static_cast<int>(codepoint);
      ui.ProcessEvents(std::array{event});
      Draw();
    }
  }
  void Open() {
    Tap(Key::F2);
    Draw();
    Require(Access::HierarchyRenameOpen(ui) && shell.LastCommand() == "editor.scene.rename",
            "F2 failed to open Rename");
  }
  void Focus(bool focus) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::FocusChanged;
    event.value0 = focus;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void ClickRename() {
    const auto point = Access::HierarchyRenamePosition(ui);
    Require(point.has_value(), "Rename button absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    ui.ProcessEvents(std::array{pointer});
    Draw();
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
  }
  void Unchanged() const {
    Require(world.SaveScene(scene_id) == baseline && scene.Name(first) == "Original" &&
                !scene.Dirty(),
            "Rename draft/gate modified the document");
  }
};
} // namespace
int main() {
  try {
    for (float dpi : {1.0F, 2.0F}) {
      Fixture f(dpi);
      f.Open();
      f.Text(U"新名稱🙂");
      Require(Access::HierarchyRenameText(f.ui) == "新名稱🙂",
              "Rename did not focus/select original UTF-8 name");
      f.Unchanged();
      f.Tap(Key::Enter);
      Require(!Access::HierarchyRenameOpen(f.ui) && f.scene.Name(f.first) == "新名稱🙂" &&
                  f.scene.Dirty() && f.scene.Undo(),
              "Enter rename was not one metadata Undo");
      f.Unchanged();
      Require(f.scene.Redo() && f.scene.Name(f.first) == "新名稱🙂" && f.scene.Save(f.Path()) &&
                  f.scene.Reload(f.Path()) && f.scene.Name(f.first) == "新名稱🙂",
              "Rename Redo/persistence lost UTF-8");
    }
    {
      Fixture f;
      f.Open();
      f.Tap(Key::A, true);
      f.Tap(Key::Backspace);
      f.ClickRename();
      Require(Access::HierarchyRenameOpen(f.ui) && Access::HierarchyRenameText(f.ui).empty(),
              "empty Rename did not remain retryable");
      for (const auto key : {Key::C, Key::X, Key::V, Key::D, Key::Z, Key::Y, Key::S, Key::A}) {
        f.Tap(key, true);
        f.Unchanged();
        Require(!f.ui.TakeSceneSaveRequest(), "Rename modal requested Save");
      }
      f.Tap(Key::F5);
      Require(f.ui.TakePlayCommand() == editor::imgui::PlayCommand::None,
              "Rename modal admitted Play");
      Access::QueueHierarchyRename(f.ui, *f.scene.Key(f.first), "Queued");
      f.Draw();
      f.Unchanged();
      f.Tap(Key::Escape);
      Require(!Access::HierarchyRenameOpen(f.ui), "Escape did not close Rename");
      Require(f.scene.Paste() && f.scene.Name(f.scene.Selection().front()) == "Clipboard Copy" &&
                  f.scene.Undo(),
              "Rename modal replaced the scene clipboard");
      f.Unchanged();
    }
    for (int lifecycle = 0; lifecycle < 5; ++lifecycle) {
      Fixture f;
      f.Open();
      f.Text(U"Canceled");
      if (lifecycle == 0) {
        f.Tap(Key::Escape);
      }
      if (lifecycle == 1) {
        f.Focus(false);
        f.Focus(true);
      }
      if (lifecycle == 2) {
        f.active = &f.observer;
        f.Draw();
        f.active = &f.workspace;
        f.Draw();
      }
      if (lifecycle == 3) {
        std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\n";
        f.Draw();
        Require(f.workspace.DiscardRecovery(), "discard failed");
        f.Draw();
      }
      if (lifecycle == 4) {
        Require(f.scene.Reload(f.Path()) && f.scene.Select(std::array{f.first}), "reload failed");
        f.Draw();
      }
      Require(!Access::HierarchyRenameOpen(f.ui), "lifecycle change retained Rename target");
      f.Tap(Key::Enter);
      f.Unchanged();
      if (lifecycle != 4)
        Require(f.scene.Redo() && f.scene.Name(f.sentinel) == "Redo sentinel",
                "canceled Rename consumed Redo");
    }
    for (int gate = 0; gate < 4; ++gate) {
      Fixture f;
      if (gate == 0)
        f.active = &f.observer;
      if (gate == 1)
        Require(f.scene.Select(std::array{f.first, f.second}), "multi-select failed");
      if (gate == 2) {
        Access::FocusInspector(f.ui);
        f.Draw();
        f.Draw();
      }
      if (gate == 3) {
        Access::FocusInspectorTransformField(f.ui, 0);
        f.Draw();
        f.Draw();
      }
      f.Tap(Key::F2);
      Require(!Access::HierarchyRenameOpen(f.ui), "F2 ignored access/selection/focus");
      f.Unchanged();
    }
    std::cout << "Keyboard Rename modal and lifecycle contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
