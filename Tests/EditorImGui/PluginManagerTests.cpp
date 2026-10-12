#include "EditorImGuiTestAccess.h"
#include "Nexora/Foundation/BuildInfo.h"
#include "TemporaryDirectoryCleanup.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
using Action = editor::imgui::ExtensionManagerAction;
using Key = Nexora::Window::Key;
using Mods = Nexora::Window::KeyModifiers;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-manager-ui-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::test::TemporaryDirectoryCleanup cleanup{root};
  editor::ProjectWorkspace workspace;
  runtime::World world;
  editor::SceneDocument scene{world, world.LoadScene("Manager")};
  editor::ProductShell shell;
  runtime::PlaySession play{world};
  editor::imgui::EditorImGuiHost ui;
  editor::imgui::ExtensionManagerObservation observation;
  float scale;
  bool settings{true}, authoring{true};
  explicit Fixture(float dpi) : scale(dpi) {
    Require(workspace.Create(root, "Manager UI"), "Project failed");
    scene.Create("Subject");
    observation.project = workspace.Project().id;
    observation.root = root;
    observation.scope = 42;
    ui.SetDisplay(1600, 1200, dpi);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
    Tap(Key::E,
        static_cast<Mods>(static_cast<unsigned>(Mods::Control) | static_cast<unsigned>(Mods::Alt)));
  }
  void Draw() {
    Require(ui.SetExtensionManagerObservation(observation), "Owning observation failed");
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, &workspace, nullptr, nullptr, nullptr, nullptr, &play);
    ui.DrawExtensionManager(settings, authoring);
    static_cast<void>(ui.EndFrame());
  }
  void Tap(Key key, Mods mods) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = mods;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = {};
    ui.ProcessEvents(std::array{event});
    Draw();
    Draw();
  }
  void Click(std::string_view id) {
    const auto point = Access::ExtensionManagerPosition(ui, id);
    if (!point)
      throw std::runtime_error("Actual extension control absent: " + std::string(id));
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
    Draw();
  }
  void Type(std::string_view value) {
    std::vector<Nexora::Window::WindowEvent> events;
    for (unsigned char c : value) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = c;
      events.push_back(event);
    }
    ui.ProcessEvents(events);
    Draw();
    Draw();
  }
  void Expect(Action action) {
    auto request = ui.TakeExtensionManagerRequest();
    Require(request && request->action == action && request->scope == observation.scope &&
                request->project == observation.project && request->root == root &&
                !ui.TakeExtensionManagerRequest(),
            "Owning single-use request missing/stale");
  }
};
void Run(float dpi) {
  Fixture f(dpi);
  f.Click("publisher");
  f.Type("known.vendor");
  f.Click("public-key");
  f.Type("zz");
  f.Click("trust-key");
  Require(!f.ui.TakeExtensionManagerRequest(), "Invalid public key queued trust");
  f.Click("public-key");
  f.Tap(Key::A, Mods::Control);
  f.Type("d75a980182b10ab7d54bfed3c964073a0ee172f3daa62325af021a68f707511a");
  f.Click("trust-key");
  auto request = f.ui.TakeExtensionManagerRequest();
  Require(request && request->action == Action::SetPublisher && request->id == "known.vendor" &&
              request->public_key[0] == std::byte{0xd7},
          "Actual trust inputs failed");
  f.Click("permission-0");
  f.Click("permission-1");
  f.Click("apply-permissions");
  request = f.ui.TakeExtensionManagerRequest();
  Require(request && request->action == Action::SetPermissions && request->permissions == 3,
          "Capability drafts overwritten between frames");
  f.observation.permissions = 3;
  f.Click("packages-tab");
  f.Click("package-path");
  f.Type("input.nxpkg");
  f.Click("review");
  request = f.ui.TakeExtensionManagerRequest();
  Require(request && request->action == Action::Review && request->package_path == "input.nxpkg",
          "Package path input failed");
  editor::ExtensionManifest manifest{
      "sample.plugin", "1.0.0", "known.vendor", "linux-x86_64", foundation::kEngineAbiVersion, 3,
      {"core"},        {}};
  manifest.artifact_digest[0] = std::byte{1};
  f.observation.review = editor::ExtensionPackageReview{manifest, 1000, 42, 9};
  f.Draw();
  f.Draw();
  f.authoring = false;
  f.Draw();
  f.Click("install");
  Require(!f.ui.TakeExtensionManagerRequest(), "Read-only installation queued");
  f.authoring = true;
  f.Draw();
  f.Click("install");
  request = f.ui.TakeExtensionManagerRequest();
  Require(request && request->action == Action::Install && request->configuration == 9,
          "Install lost private review revision");
  f.observation.review.reset();
  f.observation.packages = {{manifest, ".nexora/extensions/13-sample.plugin-1.0.0.nxpkg", 1000}};
  f.Draw();
  f.Draw();
  f.Click("sample.plugin/1.0.0/enable");
  f.Expect(Action::Enable);
  f.observation.packages[0].state = editor::ManagedExtensionState::Loaded;
  f.Draw();
  f.Click("sample.plugin/1.0.0/remove");
  Require(!f.ui.TakeExtensionManagerRequest(), "Loaded removal queued");
  f.Click("sample.plugin/1.0.0/disable");
  f.Expect(Action::Disable);
  f.observation.packages[0].state = editor::ManagedExtensionState::Disabled;
  f.observation.restart_required = true;
  f.Draw();
  f.Click("sample.plugin/1.0.0/enable");
  Require(!f.ui.TakeExtensionManagerRequest(), "Restart-required native enable queued");
  f.observation.restart_required = false;
  f.settings = false;
  f.Draw();
  f.Click("refresh");
  Require(!f.ui.TakeExtensionManagerRequest(), "Blocked settings emitted request");
  f.settings = true;
  f.Draw();
  f.Click("refresh");
  ++f.observation.scope;
  f.Draw();
  Require(!f.ui.TakeExtensionManagerRequest(), "Old project request survived scope replacement");
  auto invalid = f.observation;
  invalid.root = "relative";
  Require(!f.ui.SetExtensionManagerObservation(invalid), "Relative project observation accepted");
  invalid = f.observation;
  invalid.packages[0].state = static_cast<editor::ManagedExtensionState>(255);
  Require(!f.ui.SetExtensionManagerObservation(invalid), "Invalid native state accepted");
  f.ui.RequestCloseConfirmation();
  f.Draw();
  Require(!f.ui.ExtensionManagerInteractionAllowed(), "Close modal left native operations enabled");
}
} // namespace
int main() {
  try {
    Run(1);
    Run(2);
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
