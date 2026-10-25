#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/ProjectBuildSettings.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Action = editor::imgui::ProjectBuildSettingsAction;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-build-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  editor::ProjectBuildSettings settings;
  editor::ContentBrowserModel catalog;
  runtime::World world;
  runtime::Id id = world.LoadScene("Title");
  editor::SceneDocument scene{world, id};
  runtime::PlaySession play{world};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  float scale;
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "Build UI"), "create project");
    scene.Create("Root");
    Require(scene.Save(root / "Title.scene") && scene.Save(root / "Level01.scene"), "save fixture");
    Require(catalog.Reset({}, 1), "catalog");
    editor::ProjectBuildSettingsSession session(writer);
    Require(session.Reload(), "settings reload");
    const auto a = session.Enroll("Title.scene", catalog),
               b = session.Enroll("Level01.scene", catalog);
    Require(a && b, "enrollment");
    settings = {a->id, {*a, *b}};
    Require(session.Save(settings, catalog), "settings save");
    ui.SetProjectBuildSettings(writer, settings, true);
    ui.SetDisplay(1600, 1000, dpi);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
  }
  ~Fixture() {
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  void Draw() {
    ui.SetSceneFileContext({active->Project().id, scene.Generation()}, "Title.scene");
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::size_t control) {
    const auto point = Access::ProjectBuildSettingsPosition(ui, control);
    Require(point.has_value(), "build settings control not rendered");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
    Draw();
  }
};
void Run(float scale) {
  Fixture f(scale);
  f.Click(0);  // Build menu.
  f.Click(1);  // Build Settings.
  f.Click(20); // Row 1: make Level01 startup.
  f.Click(16); // Row 0: exclude Title.
  f.Click(5);  // Save.
  auto request = f.ui.TakeProjectBuildSettingsRequest();
  Require(request && request->action == Action::Save && request->project == f.writer.Project().id &&
              request->root == f.writer.Root() &&
              request->settings.start_scene == f.settings.scenes[1].id &&
              !request->settings.scenes[0].enabled && request->settings.scenes[1].enabled &&
              !f.ui.TakeProjectBuildSettingsRequest(),
          "real UI did not produce scoped startup/exclusion");
  editor::ProjectBuildSettingsSession session(f.writer);
  Require(session.Reload() && session.Save(request->settings, f.catalog) &&
              session.Settings()->start_scene == f.settings.scenes[1].id,
          "UI request save failed");
  f.ui.SetProjectBuildSettings(f.writer, *session.Settings(), true, "Saved");
  f.Draw();
  f.Click(19); // Startup row checkbox is disabled.
  f.Click(5);
  request = f.ui.TakeProjectBuildSettingsRequest();
  Require(request && request->settings.scenes[1].enabled, "startup checkbox allowed exclusion");
  f.Click(6);
  request = f.ui.TakeProjectBuildSettingsRequest();
  Require(request && request->action == Action::Reload, "reload did not request disk observation");
  f.ui.SetProjectBuildSettings(f.writer, {}, true);
  f.Draw();
  f.Click(2);
  request = f.ui.TakeProjectBuildSettingsRequest();
  Require(request && request->action == Action::AddScene && request->path == "Title.scene" &&
              request->settings.scenes.empty(),
          "Add Current did not use the saved current scene");
  Require(f.reader.Open(f.root, editor::ProjectAccess::ReadOnly), "reader open");
  f.active = &f.reader;
  f.ui.SetProjectBuildSettings(f.reader, f.settings, true);
  f.Draw();
  f.Click(5);
  Require(!f.ui.TakeProjectBuildSettingsRequest(), "read-only UI emitted write");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Build Settings real controls and persistence passed at 1x/2x\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
