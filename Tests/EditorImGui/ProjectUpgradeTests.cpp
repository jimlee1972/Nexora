#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Key = Nexora::Window::Key;
using Mods = Nexora::Window::KeyModifiers;
using Action = editor::imgui::ProjectSelectorAction;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-upgrade-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::imgui::EditorImGuiHost ui;
  float scale;
  explicit Fixture(float dpi) : scale(dpi) {
    std::filesystem::create_directories(root / "Content");
    std::ofstream(root / "project.nexora", std::ios::binary) << "schema=1\nname=Upgrade µ 空\n";
    ui.SetDisplay(1600, 1000, dpi);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
    const auto text = root.generic_u8string();
    Type({text.begin(), text.end()});
  }
  ~Fixture() {
    std::error_code ignored;
    std::filesystem::remove_all(root, ignored);
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProjectSelector();
    static_cast<void>(ui.EndFrame());
  }
  void Type(const std::string &text) {
    std::vector<Nexora::Window::WindowEvent> events;
    for (const unsigned char character : text) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = character;
      events.push_back(event);
    }
    ui.ProcessEvents(events);
    Draw();
    Draw();
  }
  void Tap(Key key, Mods modifiers) {
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
  void Preview() {
    Tap(Key::M,
        static_cast<Mods>(static_cast<unsigned>(Mods::Control) | static_cast<unsigned>(Mods::Alt)));
  }
  void Click(std::size_t control) {
    const auto point = Access::ProjectUpgradePosition(ui, control);
    Require(point.has_value(), "Actual project preview control is absent");
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
};
void Run(float dpi) {
  Fixture f(dpi);
  f.Preview();
  auto request = f.ui.TakeProjectSelectorRequest();
  Require(request && request->action == Action::Preview && request->root == f.root &&
              request->access == editor::ProjectAccess::ReadWrite &&
              !f.ui.TakeProjectSelectorRequest(),
          "Keyboard preview request lost owning root/access or repeated");
  const auto plan = editor::ProjectWorkspace::PreviewUpgrade(request->root);
  Require(plan && !std::filesystem::exists(f.root / ".nexora"), "Actual no-write plan failed");
  editor::imgui::ProjectUpgradeObservation observation{request->root,
                                                       plan->root,
                                                       plan->project.id,
                                                       plan->project.name,
                                                       1,
                                                       2,
                                                       plan->original_descriptor.size(),
                                                       plan->documents.size(),
                                                       true};
  Require(f.ui.SetProjectUpgradePreview(observation), "Actual supported preview rejected");
  const auto original_name = observation.name;
  observation.name = "caller changed";
  f.Draw();
  Require(f.ui.ProjectUpgradePreview() && f.ui.ProjectUpgradePreview()->name == original_name,
          "Preview retained caller borrows");
  observation.name = original_name;
  auto invalid = observation;
  invalid.documents = editor::ProjectWorkspace::kMaximumDocuments + 1;
  Require(!f.ui.SetProjectUpgradePreview(invalid), "Document budget overflow admitted");
  invalid = observation;
  invalid.source_bytes = editor::ProjectWorkspace::kMaximumProjectDescriptorBytes + 1;
  Require(!f.ui.SetProjectUpgradePreview(invalid), "Source budget overflow admitted");
  invalid = observation;
  invalid.project = {};
  Require(!f.ui.SetProjectUpgradePreview(invalid), "Missing stable project identity admitted");
  invalid = observation;
  invalid.from = 99;
  Require(!f.ui.SetProjectUpgradePreview(invalid), "Unsupported schema admitted");
  invalid = observation;
  invalid.required = false;
  Require(!f.ui.SetProjectUpgradePreview(invalid), "Contradictory upgrade state admitted");
  invalid = observation;
  invalid.canonical_root = "relative";
  Require(!f.ui.SetProjectUpgradePreview(invalid), "Relative canonical root admitted");
  for (const auto &name : {std::string(1025, 'x'), std::string("bad\nname"),
                           std::string("bad\xc2\x85name"), std::string("bad\xff")}) {
    invalid = observation;
    invalid.name = name;
    Require(!f.ui.SetProjectUpgradePreview(invalid) &&
                f.ui.ProjectUpgradePreview()->name == original_name,
            "Invalid display data replaced the accepted owning observation");
  }
  auto copied = f.ui.ProjectUpgradePreview();
  copied->name.clear();
  Require(f.ui.ProjectUpgradePreview()->name == original_name,
          "Returned observation aliases retained data");
  f.Click(1);
  request = f.ui.TakeProjectSelectorRequest();
  Require(request && request->action == Action::Preview && !f.ui.ProjectUpgradePreview(),
          "Actual preview button did not reset old observation/emit request");
  Require(f.ui.SetProjectUpgradePreview(observation), "Fresh observation failed");
  f.ui.SetProjectSelectorStatus("Importing", true);
  f.Preview();
  f.Click(1);
  Require(!f.ui.TakeProjectSelectorRequest() && !f.ui.ProjectUpgradePreview(),
          "Busy preview leaked requests/results");
  Access::QueueProjectSelection(f.ui,
                                {Action::Preview, f.root, {}, editor::ProjectAccess::ReadWrite});
  Require(!f.ui.TakeProjectSelectorRequest(), "Owner busy guard trusted injected widget intake");
  f.ui.SetProjectSelectorStatus({}, false);
  f.Draw();
  Require(!f.ui.TakeProjectSelectorRequest(), "Opening busy gate resurrected intake");
  f.Click(2);
  f.Preview();
  request = f.ui.TakeProjectSelectorRequest();
  Require(request && request->access == editor::ProjectAccess::ReadOnly &&
              request->action == Action::Preview,
          "Read-only browser cannot inspect");
  Require(f.ui.SetProjectUpgradePreview(observation), "Read-only publication failed");
  f.Tap(Key::N, Mods::Control);
  Require(!f.ui.TakeProjectSelectorRequest(), "Read-only Create shortcut granted intake");
  f.Click(0);
  f.Tap(Key::A, Mods::Control);
  f.Type("changed-root");
  Require(!f.ui.ProjectUpgradePreview() && !f.ui.SetProjectUpgradePreview(observation),
          "Changed chooser root accepted an old observation");
  Access::QueueProjectSelection(f.ui,
                                {Action::Preview, f.root, {}, editor::ProjectAccess::ReadOnly});
  Require(!f.ui.TakeProjectSelectorRequest(), "Changed chooser root consumed old preview request");
  f.Tap(Key::O, Mods::Control);
  request = f.ui.TakeProjectSelectorRequest();
  Require(request && request->action == Action::Open && request->root == "changed-root" &&
              request->access == editor::ProjectAccess::ReadOnly,
          "Existing read-only Open was changed");
  f.Click(2);
  f.Click(3);
  f.Tap(Key::A, Mods::Control);
  f.Tap(Key::Backspace, {});
  f.Tap(Key::N, Mods::Control);
  // Empty name retains the existing Create error without request.
  const auto create = f.ui.TakeProjectSelectorRequest();
  if (create || f.ui.ProjectSelectorError().empty())
    throw std::runtime_error(
        "Unnamed Create lost its existing validation; dpi=" + std::to_string(dpi) + " request=" +
        std::to_string(create.has_value()) + " error=" + std::string(f.ui.ProjectSelectorError()));
  Require(!std::filesystem::exists(f.root / ".nexora"),
          "Widget/inspection created project metadata");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    std::cout << "Actual 1x/2x owning project upgrade previews passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
