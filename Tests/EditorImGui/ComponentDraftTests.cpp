#include "EditorImGuiTestAccess.h"

#include <array>
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
struct TemporaryProject final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-component-drafts-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~TemporaryProject() { std::filesystem::remove_all(root); }
};
struct Fixture final {
  TemporaryProject temporary;
  editor::ProjectWorkspace workspace;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Component Drafts");
  editor::SceneDocument scene{world, scene_id};
  runtime::Id first{}, second{};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  runtime::PlaySession play{world};
  editor::ProjectWorkspace *active = &workspace;
  std::optional<std::string> baseline;
  Fixture() {
    std::string error;
    Require(workspace.Create(temporary.root, "Component Drafts", &error) &&
                world.Activate(scene_id),
            "project activation failed");
    first = scene.Create("First");
    second = scene.Create("Second");
    const auto entities = Keys();
    Require(scene.SetCameras(entities,
                             std::array{std::optional{runtime::CameraComponent{45, 0.2, 100}},
                                        std::optional{runtime::CameraComponent{75, 0.5, 500}}}) &&
                scene.SetLights(entities, std::array{std::optional{runtime::LightComponent{1}},
                                                     std::optional{runtime::LightComponent{3}}}) &&
                scene.Select(entities) && scene.Save(Path()),
            "component fixture failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1280, 900, 1);
    Access::SetInputTrickle(ui, false);
    Focus(true);
    Draw();
    Draw();
    Access::FocusInspector(ui);
    Draw();
    Draw();
  }
  std::array<editor::SceneDocument::NodeKey, 2> Keys() const {
    return std::array{*scene.Key(first), *scene.Key(second)};
  }
  std::filesystem::path Path() const { return temporary.root / "Content/scene"; }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Focus(bool focused) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::FocusChanged;
    event.value0 = focused;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Key(Nexora::Window::Key code, bool down, bool control = false) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(code);
    event.value1 = down;
    event.modifiers =
        control ? Nexora::Window::KeyModifiers::Control : Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Tap(Nexora::Window::Key code) {
    Key(code, true);
    Key(code, false);
  }
  void Draft(std::size_t field, std::string_view text) {
    if (field < 3)
      Access::FocusInspectorCameraField(ui, field);
    else
      Access::FocusInspectorLightField(ui);
    Draw();
    Draw();
    Key(Nexora::Window::Key::A, true, true);
    Key(Nexora::Window::Key::A, false);
    const auto before = world.SaveScene(scene_id);
    for (const char character : text) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = character;
      ui.ProcessEvents(std::array{event});
      Draw();
      Require(world.SaveScene(scene_id) == before, "component typing mutated the scene");
    }
    Require(Access::InspectorComponentText(ui, field) == text, "component draft was not retained");
  }
  void Unchanged(const char *message) const {
    Require(world.SaveScene(scene_id) == baseline && !scene.Dirty(), message);
  }
  void Queue() {
    const auto keys = Keys();
    Access::QueueInspectorCameras(
        ui, keys,
        std::array{std::optional{runtime::CameraComponent{99, 0.2, 100}},
                   std::optional{runtime::CameraComponent{99, 0.5, 500}}});
    Access::QueueInspectorLights(ui, keys,
                                 std::array{std::optional{runtime::LightComponent{99}},
                                            std::optional{runtime::LightComponent{99}}});
  }
};
} // namespace

int main() {
  try {
    for (const std::size_t field : {0U, 3U}) {
      {
        Fixture f;
        f.Draft(field, "+9e1");
        f.Tap(Nexora::Window::Key::Enter);
        Require(field == 0 ? f.scene.Camera(f.Keys()[0])->vertical_field_of_view == 90 &&
                                 f.scene.Camera(f.Keys()[1])->vertical_field_of_view == 90 &&
                                 f.scene.Camera(f.Keys()[0])->near_plane == 0.2 &&
                                 f.scene.Camera(f.Keys()[1])->far_plane == 500
                           : f.scene.Light(f.Keys()[0])->intensity == 90 &&
                                 f.scene.Light(f.Keys()[1])->intensity == 90,
                "submitted field did not apply to the selection");
        Require(f.scene.Undo(), "component Undo failed");
        f.Unchanged("component input created extra Undo steps");
        Require(f.scene.Redo() && f.scene.Save(f.Path()) && f.scene.Reload(f.Path()),
                "component draft commit did not persist");
        Require(field == 0 ? f.scene.Camera(f.Keys()[0])->vertical_field_of_view == 90 &&
                                 f.scene.Camera(f.Keys()[1])->vertical_field_of_view == 90
                           : f.scene.Light(f.Keys()[0])->intensity == 90 &&
                                 f.scene.Light(f.Keys()[1])->intensity == 90,
                "saved component field was lost on reload");
      }
      for (int transition = 0; transition < 8; ++transition) {
        Fixture f;
        f.Draft(field, "99");
        switch (transition) {
        case 0:
          f.Tap(Nexora::Window::Key::Escape);
          break;
        case 1:
          f.Focus(false);
          f.Focus(true);
          break;
        case 2:
          Require(f.scene.Select(std::span<const runtime::Id>{}), "deselect failed");
          f.Draw();
          Require(f.scene.Select(f.Keys()), "reselect failed");
          f.Draw();
          break;
        case 3:
          Require(f.scene.Reload(f.Path()) && f.scene.Select(f.Keys()), "reload failed");
          f.Draw();
          break;
        case 4: {
          editor::ProjectWorkspace read_only;
          f.active = &read_only;
          f.Queue();
          f.Draw();
          f.Tap(Nexora::Window::Key::Enter);
          f.Unchanged("read-only admitted a component request");
          f.active = &f.workspace;
          f.Draw();
          break;
        }
        case 5: {
          std::ofstream(f.temporary.root / ".nexora/workspace.recovery") << "schema=1\n";
          f.Queue();
          f.Draw();
          f.Unchanged("recovery admitted a component request");
          Require(f.workspace.DiscardRecovery(), "recovery discard failed");
          f.Draw();
          break;
        }
        case 6:
          Require(f.play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }),
                  "Play start failed");
          Access::SelectPlayEntity(f.ui, f.first);
          f.Queue();
          f.Draw();
          Require(f.play.Stop(), "Play stop failed");
          f.Draw();
          break;
        case 7:
          Access::CollapseInspector(f.ui, true);
          f.Queue();
          f.Draw();
          Access::CollapseInspector(f.ui, false);
          f.Draw();
          break;
        }
        f.Tap(Nexora::Window::Key::Enter);
        f.Unchanged("lifecycle transition revived a component draft or pending request");
      }
      {
        Fixture f;
        f.Draft(field, "99");
        f.ui.RequestCloseConfirmation();
        f.Queue();
        f.Draw();
        f.Unchanged("first close frame admitted a component request");
        f.Queue();
        f.Draw();
        f.Tap(Nexora::Window::Key::Enter);
        f.Unchanged("open close modal admitted a component request");
      }
    }
    {
      Fixture f;
      f.Queue();
      Require(f.scene.Select(std::array{f.first}), "selection switch failed");
      f.Draw();
      f.Unchanged("pending batch changed entities outside the current selection");
    }
    std::cout << "Camera/Light draft lifecycle contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
