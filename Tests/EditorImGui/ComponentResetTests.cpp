#include "EditorImGuiTestAccess.h"

#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-inspector-reset-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  runtime::World world;
  runtime::Id id = world.LoadScene("Inspector Reset");
  editor::SceneDocument scene{world, id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  runtime::Id parent{}, first{}, second{};
  float scale;
  std::optional<std::string> original;
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "Inspector Reset") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(id),
            "Inspector reset workspace failed");
    parent = scene.Create("Parent");
    first = scene.Create("First", parent);
    second = scene.Create("Second");
    Require(
        scene.SetTransforms(Keys(), std::array{runtime::Transform{1, 2, 3, 0, 0, 0, 1, -2, 3, 4},
                                               runtime::Transform{4, 5, 6}}) &&
            scene.SetEulerField(std::array{Keys()[0]}, 0, 720) &&
            scene.SetEulerField(std::array{Keys()[1]}, 2, -450) &&
            scene.SetCameras(Keys(),
                             std::array{std::optional{runtime::CameraComponent{45, 0.2, 200}},
                                        std::optional{runtime::CameraComponent{75, 0.5, 400}}}) &&
            scene.SetLights(Keys(), std::array{std::optional{runtime::LightComponent{4}},
                                               std::optional{runtime::LightComponent{7}}}) &&
            scene.SetMeshRenderer(Keys()[0], runtime::MeshComponent{7, {9}}) &&
            scene.SetOpaqueComponent(Keys()[0], {31, "Missing plugin", {1, 2, 3}}) &&
            scene.Select(Keys()) && scene.Save(Path()),
        "Inspector reset entity fixture failed");
    original = world.SaveScene(id);
    ui.SetDisplay(1600, 1200, scale);
    Access::SetInputTrickle(ui, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusInspector(ui);
    Draw();
  }
  ~Fixture() {
    reader = editor::ProjectWorkspace{};
    writer = editor::ProjectWorkspace{};
    std::filesystem::remove_all(root);
  }
  std::array<editor::SceneDocument::NodeKey, 2> Keys() const {
    return {*scene.Key(first), *scene.Key(second)};
  }
  std::filesystem::path Path() const { return root / "Content/reset.scene"; }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active);
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::size_t component) {
    const auto point = Access::InspectorResetPosition(ui, component);
    Require(point.has_value(), "Inspector reset control absent");
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
  }
  void Key(Nexora::Window::Key key, Nexora::Window::KeyModifiers modifiers = {}) {
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
  }
  void Draft(std::size_t component) {
    Access::FocusInspector(ui);
    if (component == 0)
      Access::FocusInspectorTransformField(ui, 0);
    else if (component == 1)
      Access::FocusInspectorCameraField(ui, 0);
    else
      Access::FocusInspectorLightField(ui);
    Draw();
    Draw();
    Key(Nexora::Window::Key::A, Nexora::Window::KeyModifiers::Control);
    for (char character : std::string("999")) {
      Nexora::Window::WindowEvent text;
      text.type = Nexora::Window::WindowEventType::Text;
      text.value0 = character;
      ui.ProcessEvents(std::array{text});
      Draw();
    }
    Require((component == 0 ? Access::InspectorTransformText(ui, 0)
                            : Access::InspectorComponentText(ui, component == 1 ? 0 : 3)) == "999",
            "reset draft fixture did not receive keyboard text");
    Require(world.SaveScene(id) == original && !scene.Dirty(),
            "reset draft changed the scene before submission");
  }
  void Verify(std::size_t component) {
    Require(scene.Parent(first) == parent, "Inspector reset changed parent");
    Require(scene.Selection().size() == 2, "Inspector reset changed selection");
    Require(scene.OpaqueComponents(Keys()[0])->front().data == std::vector<std::uint8_t>{1, 2, 3} &&
                scene.MeshRenderer(Keys()[0])->material.shader == 9,
            "Inspector reset lost hierarchy, selection or unrelated owned payloads");
    if (component == 0) {
      for (const auto key : Keys())
        Require(scene.Transform(key.id) == runtime::Transform{} &&
                    scene.EulerAngles(key.id) == editor::EulerDegrees{},
                "Reset Transform retained pose or revolutions");
      Require(scene.Camera(Keys()[0])->vertical_field_of_view == 45 &&
                  scene.Light(Keys()[1])->intensity == 7,
              "Reset Transform changed Camera/Light");
    } else if (component == 1) {
      for (const auto key : Keys())
        Require(scene.Camera(key)->vertical_field_of_view == 60 &&
                    scene.Camera(key)->near_plane == 0.1 && scene.Camera(key)->far_plane == 1000,
                "Reset Camera retained a nondefault lens field");
      Require(scene.EulerAngles(first)->at(0) == 720 && scene.Light(Keys()[1])->intensity == 7,
              "Reset Camera changed Transform/Light");
    } else {
      for (const auto key : Keys())
        Require(scene.Light(key)->intensity == 1, "Reset Light retained nondefault intensity");
      Require(scene.EulerAngles(first)->at(0) == 720 &&
                  scene.Camera(Keys()[1])->vertical_field_of_view == 75,
              "Reset Light changed Transform/Camera");
    }
  }
};
void Run(float dpi, std::size_t component) {
  Fixture f(dpi);
  f.Draft(component);
  f.Click(component);
  f.Verify(component);
  const auto reset = f.world.SaveScene(f.id);
  f.Key(Nexora::Window::Key::Enter);
  Require(f.world.SaveScene(f.id) == reset, "reset revived an abandoned field draft");
  Require(f.scene.Undo() && f.world.SaveScene(f.id) == f.original && !f.scene.Dirty() &&
              f.scene.EulerAngles(f.first)->at(0) == 720 && f.scene.Redo(),
          "Inspector reset did not undo all selected entities and metadata in one step");
  f.Draw();
  const auto extra = f.scene.Create("Retained Redo");
  Require(f.scene.Undo(), "reset Redo fixture failed");
  f.Draw();
  f.Click(component);
  Require(f.world.SaveScene(f.id) == reset && f.scene.Redo() &&
              f.scene.Name(extra) == "Retained Redo" && f.scene.Undo(),
          "already-default Reset consumed Redo");
  Require(f.scene.Save(f.Path()) && f.scene.Reload(f.Path()), "Inspector reset persistence failed");
  Require(f.scene.Select(f.Keys()), "reload reset selection failed");
  f.Draw();
  f.Verify(component);
  Require(f.scene.SetTransform(f.first, {9, 8, 7}) &&
              f.scene.SetCamera(f.Keys()[0], runtime::CameraComponent{45, 0.2, 200}) &&
              f.scene.SetLight(f.Keys()[0], runtime::LightComponent{4}),
          "access reset fixture failed");
  const auto protected_world = f.world.SaveScene(f.id);
  f.active = &f.reader;
  f.Draw();
  f.Click(component);
  Require(f.world.SaveScene(f.id) == protected_world, "read-only Inspector admitted reset");
  f.active = &f.writer;
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Click(component);
  Require(f.world.SaveScene(f.id) == protected_world, "close modal admitted reset");
}
void RunMixedPresence(float dpi, std::size_t component) {
  Fixture f(dpi);
  Require(component == 1 ? f.scene.SetCamera(f.Keys()[1], std::nullopt)
                         : f.scene.SetLight(f.Keys()[1], std::nullopt),
          "mixed reset presence fixture failed");
  const auto before = f.world.SaveScene(f.id);
  f.Draw();
  f.Click(component);
  Require(component == 1
              ? f.scene.Camera(f.Keys()[0])->vertical_field_of_view == 60 &&
                    !f.scene.Camera(f.Keys()[1])
              : f.scene.Light(f.Keys()[0])->intensity == 1 && !f.scene.Light(f.Keys()[1]),
          "mixed reset added an absent component or retained nondefault data");
  Require(f.scene.EulerAngles(f.first)->at(0) == 720 && f.scene.Undo() &&
              f.world.SaveScene(f.id) == before && f.scene.Redo(),
          "mixed reset changed Transform or lost one-step Undo");
  Require(component == 1 ? f.scene.SetCamera(f.Keys()[0], std::nullopt)
                         : f.scene.SetLight(f.Keys()[0], std::nullopt),
          "all-absent reset fixture failed");
  const auto absent = f.world.SaveScene(f.id);
  const auto extra = f.scene.Create("Retained absent Redo");
  Require(f.scene.Undo(), "all-absent Redo fixture failed");
  f.Draw();
  f.Click(component);
  Require(f.world.SaveScene(f.id) == absent && f.scene.Redo() &&
              f.scene.Name(extra) == "Retained absent Redo",
          "disabled all-absent reset modified data or Redo");
}
} // namespace
int main() {
  try {
    for (const auto dpi : {1.0F, 2.0F}) {
      for (std::size_t component = 0; component < 3; ++component)
        Run(dpi, component);
      RunMixedPresence(dpi, 1);
      RunMixedPresence(dpi, 2);
    }
    std::cout << "Inspector component reset input contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
