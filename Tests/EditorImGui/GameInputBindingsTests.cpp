#include "EditorImGuiTestAccess.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using namespace editor;
using Access = imgui::EditorImGuiTestAccess;
using Control = PlayInputControl;
using namespace Nexora::Window;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-game-bindings-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  ProjectWorkspace workspace, observer;
  ProjectWorkspace *active = &workspace;
  runtime::World world;
  runtime::Id scene{};
  std::unique_ptr<SceneDocument> document;
  runtime::PlaySession play{world};
  ProductShell shell;
  imgui::EditorImGuiHost ui;
  float dpi;
  Fixture(float scale, bool macos) : dpi(scale) {
    Require(workspace.Create(root, "Input bindings") &&
                observer.Open(root, ProjectAccess::ReadOnly),
            "workspace setup failed");
    scene = world.LoadScene("Bindings");
    Require(world.Activate(scene), "activate failed");
    document = std::make_unique<SceneDocument>(world, scene);
    ui.SetSceneFileContext({workspace.Project().id, document->Generation()}, {});
    ui.SetDisplay(1280 * dpi, 900 * dpi, dpi);
    Access::ConfigureSyntheticInput(ui, macos);
    Focus(true);
    for (int i = 0; i < 4; ++i)
      Draw();
    FocusGame();
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, document.get(), active, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Focus(bool focused) {
    WindowEvent e;
    e.type = WindowEventType::FocusChanged;
    e.value0 = focused;
    ui.ProcessEvents(std::array{e});
  }
  void FocusGame() {
    Access::FocusGame(ui);
    Draw();
    Draw();
  }
  void Click(std::optional<std::array<float, 2>> point) {
    Require(point.has_value(), "binding control absent");
    WindowEvent pointer, button;
    pointer.type = WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * dpi);
    pointer.value1 = static_cast<int>((*point)[1] * dpi);
    button.type = WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer});
    Draw();
    ui.ProcessEvents(std::array{button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
    Draw();
  }
  void Click(std::size_t control) {
    const auto point = Access::GameInputBindingPosition(ui, control);
    if (!point)
      throw std::runtime_error("binding control absent: " + std::to_string(control));
    Click(point);
  }
  void Choose(std::size_t control, Control choice) {
    Click(control);
    const auto point = Access::GameInputChoicePosition(ui, choice);
    if (!point) {
      std::string visible;
      for (int c = 0; c < static_cast<int>(Control::Count); ++c)
        if (Access::GameInputChoicePosition(ui, static_cast<Control>(c)))
          visible += " " + std::to_string(c);
      throw std::runtime_error(
          "binding choice absent: " + std::to_string(static_cast<int>(choice)) + " in control " +
          std::to_string(control) + " visible:" + visible);
    }
    Click(point);
  }
  void Press(Key key, KeyModifiers modifiers = KeyModifiers::None) {
    WindowEvent e;
    e.type = WindowEventType::Key;
    e.value0 = static_cast<int>(key);
    e.value1 = 1;
    e.modifiers = modifiers;
    ui.ProcessEvents(std::array{e});
    Draw();
    e.value1 = 0;
    e.modifiers = KeyModifiers::None;
    ui.ProcessEvents(std::array{e});
    Draw();
  }
  void Open() {
    Click(0);
    Require(Access::GameInputBindingsOpen(ui), "bindings did not open");
  }
};
void Run(float dpi, bool macos) {
  Fixture f(dpi, macos);
  const PlayInputBindings defaults;
  const bool originally_dirty = f.document->Dirty();
  f.Open();
  f.Choose(3, Control::B); // Right primary, retain Right-arrow alternate.
  Require(f.ui.GameInputBindings() == defaults, "editing draft published before Apply");
  f.Press(Key::F5);
  f.Press(Key::N, static_cast<KeyModifiers>(3));
  f.Press(Key::O, macos ? KeyModifiers::Super : KeyModifiers::Control);
  Require(f.ui.TakePlayCommand() == imgui::PlayCommand::None && f.document->Nodes().empty(),
          "binding modal admitted Play/authoring commands");
  Require(!f.ui.TakeSceneFileRequest(), "binding modal admitted scene file request");
  f.Click(19);
  auto committed = defaults;
  committed.controls[1][0] = Control::B;
  Require(f.ui.GameInputBindings() == committed, "Apply did not publish owning profile");
  Require(!Access::GameInputBindingsOpen(f.ui) && f.document->Dirty() == originally_dirty,
          "Apply retained modal or changed scene dirty state");
  Require(!f.ui.TakeGameInputBindingsSaveRequest(), "session Apply emitted persistence request");
  const auto copy = f.ui.GameInputBindings();
  f.FocusGame();
  f.Open();
  f.Choose(3, Control::C);
  f.Click(22);
  auto request = f.ui.TakeGameInputBindingsSaveRequest();
  auto saved = committed;
  saved.controls[1][0] = Control::C;
  Require(request && request->project == f.workspace.Project().id &&
              request->root == f.workspace.Root() && request->bindings == saved &&
              f.ui.GameInputBindings() == saved && !f.ui.TakeGameInputBindingsSaveRequest() &&
              !std::filesystem::exists(f.root / ".nexora/play-input.ini"),
          "Apply/save did not emit one owning scoped request or widgets performed IO");
  std::string save_error;
  Require(f.workspace.SavePlayInputBindings(request->bindings, &save_error), "owner save failed");
  imgui::EditorImGuiHost reopened;
  const auto persisted = f.observer.LoadPlayInputBindings(&save_error);
  Require(persisted && reopened.SetGameInputBindings(*persisted, f.observer) &&
              reopened.GameInputBindings() == saved &&
              f.ui.SetGameInputBindings(committed, f.workspace),
          "reopened host did not restore owning profile or reset session failed");
  f.FocusGame();
  f.Open();
  f.Choose(5, Control::B); // Duplicates Right's concrete control.
  f.Click(22);
  Require(!f.ui.TakeGameInputBindingsSaveRequest(), "invalid draft emitted save request");
  f.Click(19);
  Require(Access::GameInputBindingsOpen(f.ui) && !Access::GameInputBindingsError(f.ui).empty() &&
              f.ui.GameInputBindings() == committed,
          "invalid duplicate profile partially published");
  f.Press(Key::Escape);
  Require(!Access::GameInputBindingsOpen(f.ui) && f.ui.GameInputBindings() == committed,
          "Cancel changed committed bindings");
  f.FocusGame();
  f.Open();
  f.Click(20);
  Require(f.ui.GameInputBindings() == committed, "Reset draft published before Apply");
  f.Click(21);
  Require(f.ui.GameInputBindings() == committed, "Reset/Cancel lost committed bindings");
  f.active = &f.observer;
  f.FocusGame();
  f.Open();
  f.Click(20);
  f.Click(22);
  Require(!f.ui.TakeGameInputBindingsSaveRequest() && Access::GameInputBindingsOpen(f.ui),
          "read-only draft emitted save request");
  f.Click(19);
  Require(f.ui.GameInputBindings() == defaults && copy == committed,
          "read-only Reset/Apply failed or owning copy changed");
  f.FocusGame();
  f.Open();
  f.Choose(3, Control::C);
  f.Focus(false);
  f.Focus(true);
  f.Draw();
  Require(!Access::GameInputBindingsOpen(f.ui) && f.ui.GameInputBindings() == defaults,
          "loss/regain without drawing revived binding draft");
  f.FocusGame();
  f.Open();
  f.Choose(3, Control::C);
  std::ofstream(f.root / ".nexora/workspace.recovery") << "schema=1\n";
  f.Draw();
  Require(!Access::GameInputBindingsOpen(f.ui) && f.ui.GameInputBindings() == defaults,
          "recovery retained editable binding draft");
  std::filesystem::remove(f.root / ".nexora/workspace.recovery");
  f.Draw();
  f.FocusGame();
  f.Open();
  f.Choose(3, Control::B);
  f.Click(19);
  Require(f.ui.GameInputBindings() == committed, "binding retry failed");
  Require(f.play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }), "Play failed");
  f.Draw();
  f.Draw();
  f.Click(0);
  Require(!Access::GameInputBindingsOpen(f.ui) && f.ui.GameInputBindings() == committed,
          "running Play allowed binding edits");
  Require(f.play.Pause(), "Pause failed");
  f.Draw();
  f.Click(0);
  Require(!Access::GameInputBindingsOpen(f.ui), "paused Play allowed binding edits");
  Require(f.play.Stop(), "Stop failed");
  f.Draw();
  f.active = &f.workspace;
  f.FocusGame();
  f.Open();
  f.Choose(3, Control::C);
  f.ui.RequestCloseConfirmation();
  f.Draw();
  Require(!Access::GameInputBindingsOpen(f.ui) && f.ui.GameInputBindings() == committed,
          "close prompt retained editable draft or changed applied profile");
  f.Press(Key::Escape);
  f.FocusGame();
  f.Open();
  f.Click(22);
  f.active = nullptr;
  f.Draw();
  Require(!Access::GameInputBindingsOpen(f.ui) && f.ui.GameInputBindings() == defaults &&
              !f.ui.TakeGameInputBindingsSaveRequest(),
          "project detach retained profile or draft");
}
} // namespace
int main() {
  try {
    Run(1, false);
    Run(2, false);
    Run(1, true);
    std::cout << "Game input binding drafts and scope contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
