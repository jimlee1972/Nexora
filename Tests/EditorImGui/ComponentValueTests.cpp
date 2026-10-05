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
      ("nexora-inspector-values-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  editor::ProjectWorkspace *active = &writer;
  runtime::World world;
  runtime::Id id = world.LoadScene("Inspector Values");
  editor::SceneDocument scene{world, id};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  runtime::Id parent{}, first{}, second{}, source{};
  float scale;
  explicit Fixture(float dpi) : scale(dpi) {
    Require(writer.Create(root, "Inspector Values") &&
                reader.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(id),
            "Inspector values workspace failed");
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
        "Inspector values entity fixture failed");
    source = scene.Create("Copied source");
    Require(scene.SetTransform(source, {9, 8, 7, 0, 0, 0, 1, -2, 3, 4}) &&
                scene.SetEulerField(std::array{*scene.Key(source)}, 0, 720) &&
                scene.SetEulerField(std::array{*scene.Key(source)}, 1, 30) &&
                scene.SetEulerField(std::array{*scene.Key(source)}, 2, -450) &&
                scene.SetCamera(*scene.Key(source), runtime::CameraComponent{55, 0.3, 300}) &&
                scene.SetLight(*scene.Key(source), runtime::LightComponent{11}) &&
                scene.Save(Path()),
            "Source value fixture failed");
    ui.SetDisplay(1600, 1200, scale);
    Access::ConfigureSyntheticInput(ui);
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
  std::filesystem::path Path() const { return root / "Content/values.scene"; }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active);
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::size_t component, std::size_t control) {
    const auto point = Access::InspectorClipboardPosition(ui, component, control);
    Require(point.has_value(), "Inspector values control absent");
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
    const auto before = world.SaveScene(id);
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
            "values draft fixture did not receive keyboard text");
    Require(world.SaveScene(id) == before, "values draft changed the scene before submission");
  }
  void SelectSource() {
    Require(scene.Select(std::array{*scene.Key(source)}), "Source selection failed");
    Draw();
    Access::FocusInspector(ui);
    Draw();
  }
  void SelectTargets() {
    Require(scene.Select(Keys()), "Target selection failed");
    Draw();
    Access::FocusInspector(ui);
    Draw();
  }
  void Verify(std::size_t component, bool mixed = false) {
    Require(scene.Parent(first) == parent && scene.Selection().size() == 2 &&
                scene.MeshRenderer(Keys()[0])->material.shader == 9 &&
                scene.OpaqueComponents(Keys()[0])->front().data ==
                    std::vector<std::uint8_t>{1, 2, 3},
            "Paste values changed hierarchy, selection or unrelated payloads");
    for (std::size_t i = 0; i < Keys().size(); ++i) {
      const auto key = Keys()[i];
      if (component == 0) {
        const auto pose = *scene.Transform(key.id);
        Require(pose.x == 9 && pose.y == 8 && pose.z == 7 && pose.sx == -2 && pose.sy == 3 &&
                    pose.sz == 4 &&
                    scene.EulerAngles(key.id) == editor::EulerDegrees{720, 30, -450},
                "Pasted Transform lost local TRS or Euler revolutions");
      } else if (component == 1) {
        const auto camera = scene.Camera(key);
        Require(mixed && i == 1 ? !camera
                                : camera && camera->vertical_field_of_view == 55 &&
                                      camera->near_plane == 0.3 && camera->far_plane == 300,
                "Pasted Camera lost copied lens values or added a missing component");
      } else {
        const auto light = scene.Light(key);
        Require(mixed && i == 1 ? !light : light && light->intensity == 11,
                "Pasted Light lost copied intensity or added a missing component");
      }
    }
    if (component != 0)
      Require(scene.Transform(first)->x == 1 && scene.Transform(second)->x == 4 &&
                  scene.EulerAngles(first)->at(0) == 720 &&
                  scene.EulerAngles(second)->at(2) == -450,
              "Component value Paste changed Transform or authored hints");
    if (component != 1)
      Require(scene.Camera(Keys()[0])->vertical_field_of_view == 45 &&
                  scene.Camera(Keys()[1])->vertical_field_of_view == 75,
              "Component value Paste changed Camera");
    if (component != 2)
      Require(scene.Light(Keys()[0])->intensity == 4 && scene.Light(Keys()[1])->intensity == 7,
              "Component value Paste changed Light");
  }
};
void Run(float dpi, std::size_t component, bool mixed = false) {
  Fixture f(dpi);
  if (mixed)
    Require(component == 1 ? f.scene.SetCamera(f.Keys()[1], std::nullopt)
                           : f.scene.SetLight(f.Keys()[1], std::nullopt),
            "Mixed component fixture failed");
  f.SelectSource();
  Require(f.scene.CopySelection(), "Hierarchy clipboard fixture failed");
  const auto source_world = f.world.SaveScene(f.id);
  f.Draft(component);
  f.Click(component, 0);
  f.Key(Nexora::Window::Key::Enter);
  Require(f.world.SaveScene(f.id) == source_world, "Copy submitted an abandoned draft");
  f.SelectTargets();
  // Multi-selection Copy is disabled and leaves the previous captured values intact.
  f.Click(component, 0);
  const auto mismatch = f.world.SaveScene(f.id);
  f.Click((component + 1) % 3, 1);
  Require(f.world.SaveScene(f.id) == mismatch, "Mismatched component Paste was admitted");
  f.SelectSource();
  Require(f.scene.SetTransform(f.source, {101, 102, 103}) &&
              f.scene.SetCamera(*f.scene.Key(f.source), runtime::CameraComponent{80, 1, 500}) &&
              f.scene.SetLight(*f.scene.Key(f.source), runtime::LightComponent{22}) &&
              f.scene.DeleteSelection(),
          "Source change/deletion fixture failed");
  f.SelectTargets();
  const auto before = f.world.SaveScene(f.id);
  f.Draft(component == 0 ? 1 : 0); // Paste cancels drafts in other components as well.
  f.Click(component, 1);
  f.Verify(component, mixed);
  const auto pasted = f.world.SaveScene(f.id);
  f.Key(Nexora::Window::Key::Enter);
  Require(f.world.SaveScene(f.id) == pasted, "Paste revived an abandoned draft");
  Require(f.scene.Undo() && f.world.SaveScene(f.id) == before && f.scene.Redo(),
          "Paste values was not one complete multi-selection Undo step");
  f.Verify(component, mixed);
  const auto extra = f.scene.Create("Retained Redo");
  Require(f.scene.Undo(), "Paste values Redo fixture failed");
  f.Draw();
  f.Click(component, 1);
  Require(f.world.SaveScene(f.id) == pasted && f.scene.Redo() &&
              f.scene.Name(extra) == "Retained Redo" && f.scene.Undo(),
          "Equal-value Paste consumed Redo");
  Require(f.scene.Save(f.Path()) && f.scene.Reload(f.Path()), "Value persistence failed");
  f.SelectTargets();
  f.Verify(component, mixed);
  // The numeric clipboard survives document generation changes.
  Require(f.scene.ResetTransforms(f.Keys()) && f.scene.ResetCameras(f.Keys()) &&
              f.scene.ResetLights(f.Keys()),
          "Reload value reset fixture failed");
  f.Draw();
  f.Click(component, 1);
  if (component == 0)
    Require(f.scene.EulerAngles(f.first) == editor::EulerDegrees{720, 30, -450},
            "Reload invalidated owning Transform clipboard");
  else if (component == 1)
    Require(f.scene.Camera(f.Keys()[0])->vertical_field_of_view == 55,
            "Reload invalidated owning Camera clipboard");
  else
    Require(f.scene.Light(f.Keys()[0])->intensity == 11,
            "Reload invalidated owning Light clipboard");
}
void RunAccess(float dpi, std::size_t component) {
  Fixture f(dpi);
  f.SelectSource();
  Require(f.scene.CopySelection(), "Independent hierarchy clipboard fixture failed");
  f.active = &f.reader;
  f.Draw();
  const auto before_copy = f.world.SaveScene(f.id);
  f.Click(component, 0);
  Require(f.world.SaveScene(f.id) == before_copy, "Read-only Copy changed scene data");
  f.SelectTargets();
  f.Click(component, 1);
  Require(f.world.SaveScene(f.id) == before_copy, "Read-only Paste changed scene data");
  f.active = &f.writer;
  f.Draw();
  f.Click(component, 1);
  f.Verify(component);
  Require(f.scene.Undo(), "Read-only Copy did not retain usable values");
  f.Draw();
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 0;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  f.Click(component, 0);
  f.Click(component, 1);
  Require(f.world.SaveScene(f.id) == before_copy, "Unfocused clipboard controls admitted writes");
  focus.value0 = 1;
  f.ui.ProcessEvents(std::array{focus});
  f.Draw();
  f.Click(component, 1);
  f.Verify(component);
  Require(f.scene.Undo(), "Unfocused Copy replaced the retained payload");
  f.Draw();
  f.ui.RequestCloseConfirmation();
  f.Draw();
  f.Click(component, 0);
  f.Click(component, 1);
  Require(f.world.SaveScene(f.id) == before_copy, "Modal admitted clipboard mutation");
  // Component Copy is independent of the SceneDocument hierarchy clipboard.
  Require(f.scene.Paste() && f.scene.Selection().size() == 1 &&
              f.scene.Name(f.scene.Selection().front()) == "Copied source Copy" &&
              f.scene.Camera(*f.scene.Key(f.scene.Selection().front()))->vertical_field_of_view ==
                  55,
          "Component Copy replaced the hierarchy clipboard");
}
void RunAbsent(float dpi, std::size_t component) {
  Fixture f(dpi);
  f.SelectSource();
  f.Click(component, 0);
  f.SelectTargets();
  for (const auto key : f.Keys())
    Require(component == 1 ? f.scene.SetCamera(key, std::nullopt)
                           : f.scene.SetLight(key, std::nullopt),
            "Absent component fixture failed");
  const auto absent = f.world.SaveScene(f.id);
  const auto extra = f.scene.Create("Absent retained Redo");
  Require(f.scene.Undo(), "Absent Redo fixture failed");
  f.Draw();
  Require(f.scene.Select(std::array{f.Keys()[1]}), "Single absent selection failed");
  f.Draw();
  f.Click(component, 0);
  f.SelectTargets();
  f.Click(component, 1);
  Require(f.world.SaveScene(f.id) == absent && f.scene.Redo() &&
              f.scene.Name(extra) == "Absent retained Redo" && f.scene.Undo(),
          "All-absent clipboard controls changed data or consumed Redo");
  // A disabled Copy does not replace the captured payload.
  Require(component == 1 ? f.scene.SetCamera(f.Keys()[0], runtime::CameraComponent{})
                         : f.scene.SetLight(f.Keys()[0], runtime::LightComponent{}),
          "Absent restore fixture failed");
  f.Draw();
  f.Click(component, 1);
  Require(component == 1
              ? f.scene.Camera(f.Keys()[0])->vertical_field_of_view == 55 &&
                    !f.scene.Camera(f.Keys()[1])
              : f.scene.Light(f.Keys()[0])->intensity == 11 && !f.scene.Light(f.Keys()[1]),
          "Disabled Copy lost previous clipboard values");
}
} // namespace
int main() {
  try {
    for (const auto dpi : {1.0F, 2.0F}) {
      for (std::size_t component = 0; component < 3; ++component) {
        Run(dpi, component);
        RunAccess(dpi, component);
      }
      for (std::size_t component = 1; component < 3; ++component) {
        Run(dpi, component, true);
        RunAbsent(dpi, component);
      }
    }
    std::cout << "Inspector component value input contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
