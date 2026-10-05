#include "EditorImGuiTestAccess.h"
#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-component-multi-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    using namespace nexora;
    using Camera = std::optional<runtime::CameraComponent>;
    using Light = std::optional<runtime::LightComponent>;
    runtime::World runtime_world;
    const auto runtime_scene = runtime_world.LoadScene("Atomic components");
    const auto a = runtime_world.CreateEntity(runtime_scene).id;
    const auto b = runtime_world.CreateEntity(runtime_scene).id;
    runtime::SceneEditor runtime_editor(runtime_world);
    const std::array ids{a, b};
    const std::array<Camera, 2> cameras{runtime::CameraComponent{45, 0.2, 200},
                                        runtime::CameraComponent{75, 0.5, 500}};
    Require(runtime_editor.SetCameras(ids, cameras) && runtime_editor.UndoDepth() == 1 &&
                runtime_world.FindEntity(a)->camera && runtime_world.FindEntity(b)->camera &&
                runtime_editor.Undo() && !runtime_world.FindEntity(a)->camera &&
                !runtime_world.FindEntity(b)->camera && runtime_editor.Redo() &&
                runtime_world.FindEntity(b)->camera_data.far_plane == 500,
            "Runtime batch was not one owning undoable edit");
    for (int replay = 0; replay < 2; ++replay)
      Require(runtime_editor.Undo() && !runtime_world.FindEntity(a)->camera &&
                  runtime_editor.Redo() &&
                  runtime_world.FindEntity(a)->camera_data.vertical_field_of_view == 45,
              "repeated Undo/Redo consumed its stored commands");
    auto malformed = cameras;
    malformed.back()->far_plane = std::numeric_limits<double>::quiet_NaN();
    const auto before = runtime_world.SaveScene(runtime_scene);
    const auto depth = runtime_editor.UndoDepth();
    Require(
        !runtime_editor.SetCameras(ids, malformed) &&
            !runtime_editor.SetCameras(std::array{a, a}, cameras) &&
            !runtime_editor.SetCameras(std::array{a, runtime::Id{999999}}, cameras) &&
            !runtime_editor.SetCameras(ids, std::span<const Camera>{}) &&
            !runtime_editor.SetCameras(std::span<const runtime::Id>{}, std::span<const Camera>{}) &&
            runtime_world.SaveScene(runtime_scene) == before && runtime_editor.UndoDepth() == depth,
        "invalid camera batch partially mutated Runtime");
    const std::array<Light, 2> lights{runtime::LightComponent{1}, runtime::LightComponent{3}};
    auto invalid_lights = lights;
    invalid_lights.back()->intensity = -1;
    Require(!runtime_editor.SetLights(ids, invalid_lights) &&
                runtime_world.SaveScene(runtime_scene) == before &&
                runtime_editor.SetLights(ids, lights) && runtime_editor.Undo() &&
                !runtime_world.FindEntity(a)->light && !runtime_world.FindEntity(b)->light &&
                runtime_editor.Redo(),
            "light batch rollback or Undo failed");
    runtime::World world;
    const auto scene_id = world.LoadScene("Multi Inspector");
    Require(world.Activate(scene_id), "activation failed");
    editor::SceneDocument document(world, scene_id);
    const auto first = document.Create("First"), second = document.Create("Second");
    const std::array keys{*document.Key(first), *document.Key(second)};
    Require(document.SetCameras(keys, cameras) && document.SetLights(keys, lights) &&
                document.Select(keys),
            "document fixture failed");
    editor::ProductShell shell;
    editor::imgui::EditorImGuiHost ui;
    ui.SetDisplay(1280, 900, 1);
    using Access = editor::imgui::EditorImGuiTestAccess;
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focused;
    focused.type = Nexora::Window::WindowEventType::FocusChanged;
    focused.value0 = 1;
    ui.ProcessEvents(std::array{focused});
    const auto draw = [&](editor::ProjectWorkspace *workspace = nullptr) {
      ui.BeginFrame();
      ui.DrawProductShell(shell, &document, workspace);
      static_cast<void>(ui.EndFrame());
    };
    for (int i = 0; i < 3; ++i)
      draw();
    Require(Access::InspectorComponentMixed(ui) == std::array{false, true, true, true, false, true},
            "mixed camera fields/light intensity were not displayed");
    Access::FocusInspectorCameraField(ui, 0);
    draw();
    draw();
    const auto key_event = [&](Nexora::Window::Key key, bool down, bool control = false) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Key;
      event.value0 = static_cast<int>(key);
      event.value1 = down;
      event.modifiers =
          control ? Nexora::Window::KeyModifiers::Control : Nexora::Window::KeyModifiers::None;
      ui.ProcessEvents(std::array{event});
      draw();
    };
    key_event(Nexora::Window::Key::A, true, true);
    key_event(Nexora::Window::Key::A, false);
    for (const char character : std::string_view{"90"}) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = character;
      ui.ProcessEvents(std::array{event});
      draw();
    }
    Require(document.Camera(keys.front())->vertical_field_of_view == 45 &&
                document.Camera(keys.back())->vertical_field_of_view == 75,
            "typing committed a camera field before Enter");
    key_event(Nexora::Window::Key::Enter, true);
    key_event(Nexora::Window::Key::Enter, false);
    Require(document.Camera(keys.front())->vertical_field_of_view == 90 &&
                document.Camera(keys.back())->vertical_field_of_view == 90 &&
                document.Camera(keys.front())->near_plane == 0.2 &&
                document.Camera(keys.back())->far_plane == 500,
            "real Camera widget did not apply only its edited field to the selection");
    Require(document.Undo() && document.Camera(keys.front())->vertical_field_of_view == 45 &&
                document.Camera(keys.back())->vertical_field_of_view == 75 && document.Redo(),
            "multi-field edit was not one Undo step");
    Access::FocusInspectorLightField(ui);
    draw();
    draw();
    key_event(Nexora::Window::Key::A, true, true);
    key_event(Nexora::Window::Key::A, false);
    for (const char character : std::string_view{"7.5"}) {
      Nexora::Window::WindowEvent event;
      event.type = Nexora::Window::WindowEventType::Text;
      event.value0 = character;
      ui.ProcessEvents(std::array{event});
      draw();
    }
    Require(document.Light(keys.front())->intensity == 1 &&
                document.Light(keys.back())->intensity == 3,
            "Light typing committed before Enter");
    key_event(Nexora::Window::Key::Enter, true);
    key_event(Nexora::Window::Key::Enter, false);
    Require(document.Light(keys.front())->intensity == 7.5F &&
                document.Light(keys.back())->intensity == 7.5F && document.Undo() &&
                document.Light(keys.front())->intensity == 1 &&
                document.Light(keys.back())->intensity == 3,
            "real Light widget did not have one atomic Undo step");
    Require(document.SetCameras(keys,
                                std::array<Camera, 2>{std::nullopt, *document.Camera(keys.back())}),
            "mixed presence setup failed");
    draw();
    Require(Access::InspectorComponentMixed(ui)[0], "mixed Camera presence was hidden");
    Access::QueueInspectorCameras(ui, keys, cameras);
    draw();
    Require(document.Camera(keys.front()) && document.Camera(keys.back()) && document.Undo() &&
                !document.Camera(keys.front()) && document.Camera(keys.back()) && document.Redo(),
            "multi-presence edit did not retain prior components");
    const auto saved_camera = document.Camera(keys.front());
    auto stale = keys;
    ++stale.back().entity_generation;
    Access::QueueInspectorCameras(ui, stale, malformed);
    draw();
    Require(document.Camera(keys.front())->vertical_field_of_view ==
                saved_camera->vertical_field_of_view,
            "stale UI batch changed an earlier valid entity");
    Access::QueueInspectorLights(ui, keys, std::array<Light, 2>{std::nullopt, std::nullopt});
    draw();
    Require(!document.Light(keys.front()) && !document.Light(keys.back()) && document.Undo() &&
                document.Light(keys.front())->intensity == 1 &&
                document.Light(keys.back())->intensity == 3,
            "removing all Lights was not atomically undoable");
    editor::ProjectWorkspace read_only;
    Access::QueueInspectorLights(
        ui, keys, std::array<Light, 2>{runtime::LightComponent{7}, runtime::LightComponent{7}});
    auto readonly_cameras = cameras;
    readonly_cameras.front()->vertical_field_of_view = 99;
    Access::QueueInspectorCameras(ui, keys, readonly_cameras);
    draw(&read_only);
    Require(document.Camera(keys.front())->vertical_field_of_view == 45 &&
                document.Light(keys.front())->intensity == 1 &&
                document.Light(keys.back())->intensity == 3,
            "read-only Inspector admitted a pending component edit");
    Require(!document.SetCameras(stale, cameras) &&
                !document.SetLights(std::array{keys.front(), keys.front()}, lights),
            "document batch admitted invalid generations or duplicate IDs");
    std::filesystem::create_directories(root);
    Require(document.Save(root / "scene"), "save failed");
    runtime::World reopened_world;
    editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopened"));
    Require(reopened.Reload(root / "scene") &&
                reopened.Camera(*reopened.Key(first))->near_plane == 0.2 &&
                reopened.Camera(*reopened.Key(second))->far_plane == 500 &&
                reopened.Light(*reopened.Key(second))->intensity == 3,
            "multi-component edits did not survive save/reopen");
    std::filesystem::remove_all(root);
    std::cout << "Atomic Camera/Light multi-selection contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    std::filesystem::remove_all(root);
    return 1;
  }
}
