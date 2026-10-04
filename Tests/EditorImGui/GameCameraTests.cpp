#include "EditorImGuiTestAccess.h"
#include "GameViewPreview.h"

#include <array>
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
      ("nexora-game-camera-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  editor::ProjectWorkspace writer, reader;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Cameras");
  editor::SceneDocument scene{world, scene_id};
  runtime::Id first{}, second{}, hidden{};
  runtime::PlaySession play{world};
  editor::MeshAssetCatalog assets;
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  std::optional<std::string> baseline;
  Fixture() {
    Require(writer.Create(root, "Cameras") && reader.Open(root, editor::ProjectAccess::ReadOnly) &&
                world.Activate(scene_id),
            "camera workspace failed");
    first = scene.Create("First");
    second = scene.Create("Second");
    Require(scene.SetCamera(*scene.Key(first), runtime::CameraComponent{}) &&
                scene.SetCamera(*scene.Key(second), runtime::CameraComponent{45, 0.1, 1000}) &&
                scene.SetTransforms(
                    std::array{*scene.Key(first), *scene.Key(second)},
                    std::array{runtime::Transform{0, 0, 5}, runtime::Transform{3, 2, 8}}) &&
                scene.Save(root / "Content/Cameras.scene"),
            "camera scene failed");
    const auto inactive = world.LoadScene("Inactive");
    hidden = world.CreateEntity(inactive).id;
    runtime::WorldCommandBuffer camera;
    camera.SetCamera(hidden, runtime::CameraComponent{});
    Require(camera.Apply(world) &&
                play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }) &&
                play.Pause(),
            "Play failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1280, 900, 1);
    Access::SetInputTrickle(ui, false);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    for (int i = 0; i < 4; ++i)
      Draw();
  }
  ~Fixture() { std::filesystem::remove_all(root); }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, &reader, nullptr, nullptr, nullptr, nullptr, &play);
    static_cast<void>(ui.EndFrame());
  }
  void Click(std::array<float, 2> point) {
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>(point[0]);
    pointer.value1 = static_cast<int>(point[1]);
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
  void Open() {
    const auto point = Access::GameCameraPosition(ui, std::nullopt);
    Require(point.has_value(), "camera combo is absent");
    Click(*point);
  }
  void Choose(runtime::Id camera) {
    Open();
    const auto point = Access::GameCameraPosition(ui, camera);
    Require(point.has_value(), "camera candidate is absent");
    Click(*point);
    Require(ui.GameCameraSelection() == camera, "camera click did not select preview");
  }
  editor::preview::GameFrame Frame(runtime::Id camera = 0) {
    return editor::preview::BuildGameFrame(*play.PlayWorld(), play.Inspect(), assets, 2, camera);
  }
};
} // namespace
int main() {
  try {
    Fixture f;
    Require(f.Frame().camera == f.first && f.ui.GameCameraSelection() == 0,
            "automatic camera order changed");
    const auto before_play = f.play.PlayWorld()->SaveScene(f.scene_id);
    f.Choose(f.second);
    const auto selected = f.Frame(f.ui.GameCameraSelection());
    Require(selected.camera == f.second &&
                selected.view_projection.values ==
                    runtime::CameraView(*f.play.PlayWorld(), f.second, 2)->view_projection.values &&
                selected.view_projection.values != f.Frame().view_projection.values &&
                f.world.SaveScene(f.scene_id) == f.baseline && !f.scene.Dirty() &&
                f.play.PlayWorld()->SaveScene(f.scene_id) == before_play,
            "preview selection mutated either World or ignored runtime projection");
    Require(f.Frame(f.hidden).camera == f.first && f.Frame(999999).camera == f.first &&
                !editor::preview::BuildGameFrame(*f.play.PlayWorld(), f.play.Inspect(), f.assets, 0,
                                                 f.second)
                     .camera,
            "inactive/missing camera fallback or invalid aspect failed");
    f.Choose(0);
    Require(f.Frame(f.ui.GameCameraSelection()).camera == f.first, "Automatic did not restore");
    f.Open();
    Require(!Access::GameCameraPosition(f.ui, f.hidden), "inactive camera offered in chooser");
    const auto point = Access::GameCameraPosition(f.ui, f.second);
    Require(point.has_value(), "second camera is absent");
    f.Click(*point);
    f.ui.RequestCloseConfirmation();
    f.Draw();
    f.Open();
    Require(f.ui.GameCameraSelection() == f.second && !Access::GameCameraPosition(f.ui, f.first),
            "close modal permitted camera selection");
    Nexora::Window::WindowEvent escape;
    escape.type = Nexora::Window::WindowEventType::Key;
    escape.value0 = static_cast<int>(Nexora::Window::Key::Escape);
    escape.value1 = 1;
    f.ui.ProcessEvents(std::array{escape});
    f.Draw();
    escape.value1 = 0;
    f.ui.ProcessEvents(std::array{escape});
    f.Draw();
    Require(f.ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
            "close cancellation failed");
    const auto stale = f.play.Inspect();
    runtime::WorldCommandBuffer remove;
    remove.SetCamera(f.second, std::nullopt);
    Require(remove.Apply(*f.play.PlayWorld()), "runtime camera removal failed");
    Require(
        editor::preview::BuildGameFrame(*f.play.PlayWorld(), stale, f.assets, 2, f.second).camera ==
            f.first,
        "stale snapshot retained removed preferred camera");
    f.Draw();
    Require(f.ui.GameCameraSelection() == 0, "UI retained removed camera");
    runtime::WorldCommandBuffer restore;
    restore.SetCamera(f.second, runtime::CameraComponent{});
    Require(restore.Apply(*f.play.PlayWorld()), "runtime restore failed");
    f.Draw();
    Require(f.ui.GameCameraSelection() == 0, "restored component revived selection");
    f.Choose(f.second);
    // A valid Runtime double field of view can round to 180 in native float projection.
    runtime::WorldCommandBuffer narrow;
    narrow.SetCamera(f.second, runtime::CameraComponent{179.9999999, 0.1, 1000});
    Require(narrow.Apply(*f.play.PlayWorld()) && f.Frame(f.second).camera == f.first,
            "unrenderable projection did not fall back");
    f.Draw();
    Require(f.ui.GameCameraSelection() == 0, "unrenderable camera retained selection");
    restore.SetCamera(f.second, runtime::CameraComponent{});
    Require(restore.Apply(*f.play.PlayWorld()), "projection restore failed");
    f.Draw();
    f.Choose(f.second);
    // Stop/Start between UI frames must still invalidate session-bound IDs and open popups.
    f.Open();
    Require(f.play.Stop() &&
                f.play.Start(1.0 / 60.0, [](runtime::World &, double) { return true; }),
            "restart failed");
    f.Draw();
    f.Draw();
    Require(f.ui.GameCameraSelection() == 0 && !Access::GameCameraPosition(f.ui, f.second),
            "restart retained camera or popup");
    f.Choose(f.second);
    const auto held = f.play.Inspect();
    Require(f.play.PlayWorld()->RequestUnload(f.scene_id), "scene unload failed");
    Require(
        !editor::preview::BuildGameFrame(*f.play.PlayWorld(), held, f.assets, 2, f.second).camera,
        "unloading scene retained camera from stale snapshot");
    f.Draw();
    Require(f.ui.GameCameraSelection() == 0, "inactive scene retained UI selection");
    Require(f.play.Stop(), "Stop failed");
    f.Draw();
    Require(f.ui.GameCameraSelection() == 0 && !Access::GameCameraPosition(f.ui, std::nullopt) &&
                f.world.SaveScene(f.scene_id) == f.baseline && !f.scene.Dirty(),
            "Stop retained chooser or changed Editor state");
    Require(f.scene.Undo() && f.scene.Transform(f.second)->z == 0 && f.scene.Redo() &&
                f.world.SaveScene(f.scene_id) == f.baseline,
            "preview choice entered authoring Undo history");
    std::cout << "Game preview camera selection contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
