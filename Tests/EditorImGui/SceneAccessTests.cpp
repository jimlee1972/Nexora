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
      ("nexora-scene-access-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  ~TemporaryProject() { std::filesystem::remove_all(root); }
};
struct Fixture final {
  TemporaryProject temporary;
  editor::ProjectWorkspace workspace, observer;
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Scene Access");
  editor::SceneDocument scene{world, scene_id};
  runtime::Id first{}, second{};
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &workspace;
  std::optional<std::string> baseline;
  Fixture() {
    Require(workspace.Create(temporary.root, "Scene Access") && world.Activate(scene_id) &&
                observer.Open(temporary.root, editor::ProjectAccess::ReadOnly),
            "workspace fixture failed");
    first = scene.Create("First");
    second = scene.Create("Second");
    auto first_pose = *scene.Transform(first), second_pose = *scene.Transform(second);
    first_pose.x = 2;
    second_pose.x = -2;
    Require(scene.SetTransforms(std::array{*scene.Key(first), *scene.Key(second)},
                                std::array{first_pose, second_pose}) &&
                scene.Select(std::array{first}) && scene.Save(temporary.root / "Content/scene"),
            "scene fixture failed");
    first_pose.x = 10;
    Require(scene.SetTransforms(std::array{*scene.Key(first)}, std::array{first_pose}) &&
                scene.Undo(),
            "Redo setup failed");
    baseline = world.SaveScene(scene_id);
    ui.SetDisplay(1280, 900, 1);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::FocusChanged;
    event.value0 = 1;
    ui.ProcessEvents(std::array{event});
    for (int i = 0; i < 4; ++i)
      Draw();
    FocusHierarchy();
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active);
    static_cast<void>(ui.EndFrame());
  }
  void FocusHierarchy() {
    Access::FocusHierarchy(ui);
    Draw();
    Draw();
  }
  void Tap(Nexora::Window::Key key, Nexora::Window::KeyModifiers modifiers) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = modifiers;
    ui.ProcessEvents(std::array{event});
    Draw();
    event.value1 = 0;
    event.modifiers = Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    Draw();
  }
  void Pointer(float x, float y, bool down) {
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>(x);
    pointer.value1 = static_cast<int>(y);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = down;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
  }
  void Unchanged(const char *message) const {
    Require(world.SaveScene(scene_id) == baseline && !scene.Dirty(), message);
  }
  void Requests() {
    const auto a = *scene.Key(first), b = *scene.Key(second);
    Access::QueueHierarchyRename(ui, a, "Forbidden");
    Access::QueueHierarchyCreate(ui, "Forbidden", std::nullopt);
    Access::QueueHierarchyMove(ui, a, b, 0);
    Access::QueueHierarchyReorder(ui, 1);
    Draw();
  }
};
} // namespace

int main() {
  try {
    using Key = Nexora::Window::Key;
    using Modifiers = Nexora::Window::KeyModifiers;
    for (int gate = 0; gate < 3; ++gate) {
      Fixture f;
      if (gate == 0)
        f.active = &f.observer;
      else if (gate == 1)
        std::ofstream(f.temporary.root / ".nexora/workspace.recovery") << "schema=1\n";
      else
        f.ui.RequestCloseConfirmation();
      f.Draw();
      f.Requests();
      f.Unchanged("blocked Hierarchy requests mutated the scene");
      for (const auto key : {Key::D, Key::X, Key::V, Key::Z, Key::Y, Key::S}) {
        f.Tap(key, Modifiers::Control);
        f.Unchanged("blocked authoring shortcut mutated the scene");
        Require(!f.ui.TakeSceneSaveRequest(), "blocked shortcut requested Save");
      }
      f.Tap(Key::N, static_cast<Modifiers>(static_cast<std::uint32_t>(Modifiers::Control) |
                                           static_cast<std::uint32_t>(Modifiers::Shift)));
      f.Tap(Key::Delete, Modifiers::None);
      f.Unchanged("blocked create/delete shortcut mutated the scene");
      Require(f.scene.Redo() && f.scene.Transform(f.first)->x == 10 && f.scene.Undo(),
              "blocked controls discarded Redo or Undo");
      f.Unchanged("blocked controls changed history replay");
      if (gate == 0) {
        // Read-only inspection/Copy remain usable; writes resume only after explicit access change.
        Access::QueueHierarchySelection(f.ui, *f.scene.Key(f.second), false, false);
        f.Draw();
        Require(f.scene.Selection().size() == 1 && f.scene.Selection().front() == f.second,
                "read-only blocked selection");
        f.Tap(Key::C, Modifiers::Control);
        f.active = &f.workspace;
        f.Draw();
        f.Unchanged("returning to writable revived a blocked request");
        f.Tap(Key::V, Modifiers::Control);
        Require(f.scene.Nodes().size() == 3 && f.scene.Undo(),
                "read-only Copy or resumed Paste did not work");
        f.Unchanged("resumed Paste did not have one Undo step");
        Require(f.scene.Select(std::array{f.second}), "Duplicate selection failed");
        f.Tap(Key::D, Modifiers::Control);
        Require(f.scene.Nodes().size() == 3 && f.scene.Undo(),
                "writable Duplicate shortcut stopped working");
        f.Unchanged("writable Duplicate failed Undo");
      } else if (gate == 1) {
        Require(f.workspace.DiscardRecovery(), "recovery discard failed");
        f.Draw();
        f.Unchanged("recovery exit revived blocked requests");
      } else {
        f.Tap(Key::Escape, Modifiers::None);
        Require(f.ui.TakeCloseChoice() == editor::imgui::CloseChoice::Cancel,
                "Escape did not report close cancellation");
        f.Tap(Key::S, Modifiers::Control);
        Require(f.ui.TakeSceneSaveRequest(), "Escape did not resume Save after close cancellation");
        f.Unchanged("close cancellation revived queued edits");
      }
    }
    {
      Fixture f;
      f.active = &f.observer;
      f.Draw();
      Require(f.scene.Select(std::array{f.second}), "readonly picking setup failed");
      f.Draw();
      const auto point = Access::SceneMarkerPosition(f.ui, *f.scene.Key(f.first));
      Require(point.has_value(), "overview marker is absent");
      f.Pointer((*point)[0], (*point)[1], true);
      Require(f.scene.Selection().size() == 1 && f.scene.Selection().front() == f.first,
              "read-only overview blocked picking");
      f.Pointer((*point)[0] + 32, (*point)[1], true);
      f.Pointer((*point)[0] + 32, (*point)[1], false);
      f.Unchanged("read-only overview drag committed a transform");
    }
    for (const bool native : {false, true}) {
      Fixture f;
      f.ui.SetNativeScenePreview(native);
      f.Draw();
      std::array<float, 2> point{};
      if (native) {
        const auto viewport = f.ui.NativeScenePreviewViewport();
        Require(viewport.has_value(), "native canvas is absent");
        point = {static_cast<float>(viewport->x + viewport->width / 2),
                 static_cast<float>(viewport->y + viewport->height / 2)};
      } else {
        const auto marker = Access::SceneMarkerPosition(f.ui, *f.scene.Key(f.first));
        Require(marker.has_value(), "overview canvas is absent");
        point = *marker;
      }
      f.Pointer(point[0], point[1], true);
      f.Pointer(point[0] + 32, point[1], true);
      if (native)
        Require(f.ui.NativeSceneDragPreview().has_value(), "writable native drag did not start");
      else {
        const auto preview = Access::SceneMarkerPosition(f.ui, *f.scene.Key(f.first));
        Require(preview && (*preview)[0] > point[0] + 20,
                "writable overview preview did not start");
      }
      f.Unchanged("gesture preview mutated the authored scene");
      f.active = &f.observer;
      f.Draw();
      f.Pointer(point[0] + 64, point[1], false);
      Require(!f.ui.NativeSceneDrag() && !f.ui.NativeSceneDragPreview(),
              "access transition published a native drag");
      f.Unchanged("access transition committed an interrupted drag");
      f.active = &f.workspace;
      f.Draw();
      f.Unchanged("restored access revived an interrupted drag");
    }
    std::cout << "Scene authoring access contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
