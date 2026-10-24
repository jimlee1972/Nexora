#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Action = editor::imgui::ProjectSelectorAction;
using Key = Nexora::Window::Key;
using Mods = Nexora::Window::KeyModifiers;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  editor::imgui::EditorImGuiHost ui;
  float scale;
  std::filesystem::path selected;
  std::string error;
  unsigned calls{};
  Action last{};
  explicit Fixture(float dpi) : scale(dpi) {
    ui.SetDisplay(1600, 1000, dpi);
    Access::ConfigureSyntheticInput(ui);
    ui.SetProjectLocationPicker(
        [this](Action action, std::string &message) -> std::optional<std::filesystem::path> {
          ++calls;
          last = action;
          message = error;
          if (selected.empty())
            return std::nullopt;
          return selected;
        });
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::FocusChanged;
    event.value0 = 1;
    ui.ProcessEvents(std::array{event});
    Draw();
    Draw();
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProjectSelector();
    static_cast<void>(ui.EndFrame());
  }
  void Tap(Key key, Mods modifiers = {}) {
    Nexora::Window::WindowEvent event;
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
    Draw();
  }
  void Click(std::size_t control) {
    const auto point = Access::ProjectUpgradePosition(ui, control);
    Require(point.has_value(), "Requested project picker control is absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0] * scale);
    pointer.value1 = static_cast<int>((*point)[1] * scale);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
    Draw();
  }
  void Name(std::string_view name) {
    Click(3);
    Tap(Key::A, Mods::Control);
    Tap(Key::Backspace);
    std::vector<Nexora::Window::WindowEvent> events;
    for (const char character : name) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = character;
      events.push_back(event);
    }
    ui.ProcessEvents(events);
    Draw();
    Draw();
  }
};
void Run(float scale) {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-project-picker-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  struct Cleanup final {
    std::filesystem::path path;
    ~Cleanup() {
      std::error_code ignored;
      std::filesystem::remove_all(path, ignored);
    }
  } cleanup{root};
  std::filesystem::create_directories(root / "Chosen Folder");
  Fixture f(scale);
  Require(!Access::Inspect(f.ui).selector_root_active && !Access::ProjectUpgradePosition(f.ui, 3),
          "Startup requires a raw root/name text field");
  f.Click(0);
  Require(f.calls == 1 && f.last == Action::Open && !f.ui.TakeProjectSelectorRequest() &&
              f.ui.ProjectSelectorError().empty() && !Access::ProjectSelectorErrorVisible(f.ui),
          "Cancelling the native chooser emitted work or an error");
  f.error = "Selection unavailable";
  f.Tap(Key::O, Mods::Control);
  Require(!f.ui.TakeProjectSelectorRequest() &&
              f.ui.ProjectSelectorError() == "Selection unavailable" &&
              Access::ProjectSelectorErrorVisible(f.ui),
          "Chooser failure was hidden or granted an Open request");
  f.error.clear();
  f.selected = root;
  f.Tap(Key::O, Mods::Control);
  const auto opened = f.ui.TakeProjectSelectorRequest();
  Require(opened && opened->action == Action::Open && opened->root == root &&
              opened->name.empty() && !f.ui.TakeProjectSelectorRequest(),
          "Open changed the selected project directory or requires a project name");
  const auto calls = f.calls;
  f.Tap(Key::M,
        static_cast<Mods>(static_cast<unsigned>(Mods::Control) | static_cast<unsigned>(Mods::Alt)));
  Require(f.calls == calls && !f.ui.TakeProjectSelectorRequest(),
          "Removed preview shortcut still emitted a standalone operation");
  f.Click(2);
  f.Click(0);
  const auto read = f.ui.TakeProjectSelectorRequest();
  Require(read && read->access == editor::ProjectAccess::ReadOnly,
          "Read-only Open lost its access policy");
  const auto read_calls = f.calls;
  f.Tap(Key::N, Mods::Control);
  Require(f.calls == read_calls && !f.ui.TakeProjectSelectorRequest(),
          "Read-only Create launched a chooser");
  f.Click(2);
  f.selected = "relative-folder";
  f.Click(0);
  Require(!f.ui.TakeProjectSelectorRequest() && !f.ui.ProjectSelectorError().empty() &&
              Access::ProjectSelectorErrorVisible(f.ui),
          "Invalid callback selection reached the application");
  f.selected = root / "Chosen Folder";
  f.Click(1);
  Require(f.last == Action::Create && !f.ui.TakeProjectSelectorRequest() &&
              Access::ProjectUpgradePosition(f.ui, 3),
          "Create did not ask for a name after choosing its folder");
  f.Name("");
  f.Click(4);
  Require(!f.ui.TakeProjectSelectorRequest() && !f.ui.ProjectSelectorError().empty(),
          "Empty project name was admitted");
  f.Name("Display Name");
  f.Click(4);
  const auto created = f.ui.TakeProjectSelectorRequest();
  Require(created && created->action == Action::Create && created->root == f.selected &&
              created->name == "Display Name",
          "Create appended the project name to the selected directory");
  f.ui.SetProjectSelectorError("Project writer access unavailable");
  f.Draw();
  Require(!Access::ProjectUpgradePosition(f.ui, 3) && Access::ProjectSelectorErrorVisible(f.ui),
          "Application failure disappeared after the Create dialog closed");
  {
    editor::ProjectWorkspace workspace;
    std::string error;
    Require(workspace.Create(created->root, created->name, &error),
            "Selected-directory request could not create a real project");
    Require(std::filesystem::is_regular_file(f.selected / "project.nexora") &&
                !std::filesystem::exists(f.selected / "Display Name"),
            "Project files were written to a different directory");
  }
  f.Click(1);
  f.Click(5);
  Require(!f.ui.TakeProjectSelectorRequest(), "Cancelled Create emitted a write request");
  f.ui.SetProjectSelectorStatus("Importing", true);
  const auto busy_calls = f.calls;
  f.Tap(Key::O, Mods::Control);
  f.Tap(Key::N, Mods::Control);
  Require(f.calls == busy_calls && !f.ui.TakeProjectSelectorRequest(),
          "Busy project import admitted a competing chooser");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Picked project open/create, cancellation and exact-folder publication passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
