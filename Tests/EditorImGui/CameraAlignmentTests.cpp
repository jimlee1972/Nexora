#include "EditorImGuiTestAccess.h"
#include "Nexora/Editor/InspectorRotation.h"
#include "Nexora/Runtime/RenderSync.h"
#include "TemporaryDirectoryCleanup.h"

#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using Access = editor::imgui::EditorImGuiTestAccess;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
bool Near(double a, double b) { return std::abs(a - b) < 1e-5; }
struct Fixture final {
  std::filesystem::path root =
      std::filesystem::temp_directory_path() /
      ("nexora-camera-alignment-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  nexora::editor::test::TemporaryDirectoryCleanup cleanup{root};
  runtime::World world;
  runtime::Id scene_id = world.LoadScene("Alignment");
  editor::SceneDocument scene{world, scene_id};
  runtime::Id ancestor{}, parent{}, camera{}, other{};
  editor::ProjectWorkspace writer, observer;
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  editor::ProjectWorkspace *active = &writer;
  std::optional<std::string> original;
  runtime::Transform original_pose;
  const runtime::CameraComponent lens{54, 0.35, 222};
  Fixture(bool parented) {
    Require(writer.Create(root, "Alignment") &&
                observer.Open(root, editor::ProjectAccess::ReadOnly) && world.Activate(scene_id),
            "alignment fixture failed");
    if (parented) {
      ancestor = scene.Create("Ancestor");
      parent = scene.Create("Parent", ancestor);
      const auto first = *editor::WithEulerDegrees({3, -2, 4, 0, 0, 0, 1, -2, 3, 4}, {10, 30, 15});
      const auto second =
          *editor::WithEulerDegrees({-2, 5, 1, 0, 0, 0, 1, 1, -2, 3}, {-20, 10, 25});
      Require(scene.SetTransforms(std::array{*scene.Key(ancestor), *scene.Key(parent)},
                                  std::array{first, second}),
              "sheared parent fixture failed");
    }
    camera = scene.Create("Camera", parent);
    other = scene.Create("Other");
    original_pose = *editor::WithEulerDegrees({1, 2, 3, 0, 0, 0, 1, -2, 3, 4}, {15, 35, 10});
    Require(scene.SetCamera(*scene.Key(camera), lens) &&
                scene.SetTransforms(std::array{*scene.Key(camera)}, std::array{original_pose}) &&
                scene.Select(std::array{camera}) && scene.Save(root / "Content/Alignment.scene"),
            "Camera fixture failed");
    original = world.SaveScene(scene_id);
    ui.SetDisplay(1600, 1000, 1);
    Access::ConfigureSyntheticInput(ui);
    Nexora::Window::WindowEvent focus;
    focus.type = Nexora::Window::WindowEventType::FocusChanged;
    focus.value0 = 1;
    ui.ProcessEvents(std::array{focus});
    ui.SetNativeScenePreview(true);
    for (int i = 0; i < 4; ++i)
      Draw();
    Access::FocusInspector(ui);
    Draw();
    Draw();
  }
  void Draw() {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene, active);
    static_cast<void>(ui.EndFrame());
  }
  void ClickAlign() {
    const auto point = Access::CameraAlignPosition(ui);
    Require(point.has_value(), "Camera align button is absent");
    Nexora::Window::WindowEvent pointer, button;
    pointer.type = Nexora::Window::WindowEventType::Pointer;
    pointer.value0 = static_cast<int>((*point)[0]);
    pointer.value1 = static_cast<int>((*point)[1]);
    button.type = Nexora::Window::WindowEventType::PointerButton;
    button.value0 = 0;
    button.value1 = 1;
    ui.ProcessEvents(std::array{pointer, button});
    Draw();
    button.value1 = 0;
    ui.ProcessEvents(std::array{button});
    Draw();
  }
  void Verify(runtime::Transform target) const {
    const auto matrix = world.WorldMatrix(camera);
    const auto pose = world.WorldTransform(camera);
    const auto local = scene.Transform(camera);
    Require(matrix && pose && local && Near((*matrix)[12], target.x) &&
                Near((*matrix)[13], target.y) && Near((*matrix)[14], target.z) &&
                editor::SameRotation(*pose, target) && local->sx == original_pose.sx &&
                local->sy == original_pose.sy && local->sz == original_pose.sz &&
                world.FindEntity(camera)->parent == parent &&
                scene.Camera(*scene.Key(camera))->vertical_field_of_view ==
                    lens.vertical_field_of_view &&
                scene.Camera(*scene.Key(camera))->near_plane == lens.near_plane &&
                scene.Camera(*scene.Key(camera))->far_plane == lens.far_plane,
            "Camera world pose, scale, parent or lens was not preserved");
    runtime::World expected;
    const auto expected_scene = expected.LoadScene("Expected");
    Require(expected.Activate(expected_scene), "expected camera scene failed");
    const auto expected_camera = expected.CreateEntity(expected_scene).id;
    runtime::WorldCommandBuffer commands;
    commands.SetCamera(expected_camera, lens);
    commands.SetTransform(expected_camera, target);
    Require(commands.Apply(expected), "expected camera pose failed");
    const auto actual_view = runtime::CameraView(world, camera, 1.5F);
    const auto expected_view = runtime::CameraView(expected, expected_camera, 1.5F);
    Require(actual_view && expected_view, "aligned camera cannot render");
    for (std::size_t i = 0; i < 16; ++i)
      Require(
          Near(actual_view->view_projection.values[i], expected_view->view_projection.values[i]),
          "Runtime camera view did not match the requested world pose");
  }
};
} // namespace
int main() {
  try {
    for (const bool parented : {false, true}) {
      Fixture f(parented);
      const auto key = *f.scene.Key(f.camera);
      const auto desired = *editor::WithEulerDegrees({-8, 7, 12}, {-25, 55, 20});
      Require(f.scene.AlignCameraToWorldPose(key, desired), "world-pose alignment failed");
      f.Verify(desired);
      const auto aligned = f.world.SaveScene(f.scene_id);
      Require(f.scene.Dirty() && f.scene.Undo() && f.world.SaveScene(f.scene_id) == f.original &&
                  !f.scene.Dirty(),
              "alignment did not Undo as one transaction");
      auto current = *f.world.WorldTransform(f.camera);
      const auto exact = *f.world.WorldMatrix(f.camera);
      current.x = exact[12];
      current.y = exact[13];
      current.z = exact[14];
      Require(f.scene.AlignCameraToWorldPose(key, current) && f.scene.Redo() &&
                  f.world.SaveScene(f.scene_id) == aligned,
              "equivalent alignment cleared Redo");
      Require(f.scene.AlignCameraToWorldPose(key, desired) && f.scene.Undo() &&
                  f.world.SaveScene(f.scene_id) == f.original,
              "repeated alignment added an Undo entry");
      auto invalid = desired;
      invalid.x = std::numeric_limits<double>::infinity();
      auto stale = key;
      ++stale.document_generation;
      Require(!f.scene.AlignCameraToWorldPose(key, invalid) &&
                  !f.scene.AlignCameraToWorldPose(stale, desired) &&
                  !f.scene.AlignCameraToWorldPose(*f.scene.Key(f.other), desired) &&
                  f.world.SaveScene(f.scene_id) == f.original && f.scene.Redo(),
              "rejected alignment mutated state/history");
      Require(f.scene.Save(f.root / "Content/Aligned.scene") &&
                  f.scene.Reload(f.root / "Content/Aligned.scene"),
              "aligned save/reload failed");
      f.Verify(desired);
      Require(!f.scene.AlignCameraToWorldPose(key, {}), "reload retained a stale document key");
    }
    for (int gate = 0; gate < 5; ++gate) {
      Fixture f(true);
      if (gate == 1)
        f.active = &f.observer;
      if (gate == 2)
        f.ui.SetNativeScenePreview(false);
      const double center_x = gate == 3   ? 200000.0
                              : gate == 4 ? std::numeric_limits<float>::max() * 0.5
                                          : 4;
      const double center_z = gate == 3   ? -300000.0
                              : gate == 4 ? -std::numeric_limits<float>::max() * 0.5
                                          : -3;
      Require(f.ui.SetSceneOverviewCamera({center_x, center_z, 32}) &&
                  f.ui.SetNativeSceneOrbit({0.7, 0.4, 12, 2}),
              "Scene view setup failed");
      f.Draw();
      f.ClickAlign();
      if (gate == 1 || gate == 2) {
        Require(f.world.SaveScene(f.scene_id) == f.original && !f.scene.Dirty(),
                "blocked Camera align button mutated the scene");
      } else {
        const float x = static_cast<float>(std::clamp(center_x, -100000.0, 100000.0)) +
                        static_cast<float>(12 * std::sin(0.7) * std::cos(0.4));
        const float y = 2.0F + static_cast<float>(12 * std::sin(0.4));
        const float z = static_cast<float>(std::clamp(center_z, -100000.0, 100000.0)) +
                        static_cast<float>(12 * std::cos(0.7) * std::cos(0.4));
        const auto target = *editor::WithEulerDegrees(
            {x, y, z}, {-0.4 * 180 / std::numbers::pi, 0.7 * 180 / std::numbers::pi, 0});
        f.Verify(target);
        Require(f.scene.Undo() && f.world.SaveScene(f.scene_id) == f.original,
                "UI Camera align did not have one Undo step");
      }
    }
    std::cout << "Camera Scene-view alignment contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
