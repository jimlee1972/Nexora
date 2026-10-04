#include "EditorImGuiTestAccess.h"

#include <algorithm>
#include <chrono>
#include <filesystem>
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
void RuntimeForest() {
  runtime::World world;
  const auto scene = world.LoadScene("Initialized forest");
  const auto other = world.LoadScene("Collision owner");
  runtime::SceneEditor history(world);
  runtime::Entity child, root, second;
  child.id = 61;
  child.parent = 63; // Forward reference is supported.
  child.transform = {1, 2, 3, 0, 0, 0, 2, -2, 3, 4};
  child.camera = true;
  child.camera_data = {72, 0.2, 800};
  child.mesh_renderer = true;
  child.mesh_data = {std::numeric_limits<runtime::Id>::max(), {99}};
  root.id = 63;
  root.transform = {3, 2, -1};
  second.id = 64;
  second.light = true;
  second.light_data = {7};
  std::array prototypes{child, second, root};
  const auto ids = history.CloneEntityForest(scene, prototypes);
  Require(ids.size() == 3 && history.UndoDepth() == 1 && world.Parent(ids[0]) == ids[2] &&
              world.FindEntity(ids[0])->transform.qw == 1 &&
              world.FindEntity(ids[0])->mesh_data.mesh == std::numeric_limits<runtime::Id>::max(),
          "initialized forest lost parent remapping, normalization or full-width IDs");
  const auto created = world.SaveScene(scene);
  const auto external_child = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer attach;
  attach.SetParent(external_child, ids[2], false);
  Require(attach.Apply(world) && !history.Undo() && history.UndoDepth() == 1 &&
              world.FindEntity(ids[0]),
          "forest Undo deleted an unrecorded child");
  runtime::WorldCommandBuffer discard;
  discard.DestroyEntity(external_child);
  Require(discard.Apply(world) && history.Undo() && world.FindScene(scene)->entities.empty(),
          "forest Undo did not remove the complete initialized batch");
  for (int invalid = 0; invalid < 7; ++invalid) {
    auto rejected = prototypes;
    if (invalid == 0)
      rejected[1].id = child.id;
    if (invalid == 1)
      rejected[0].parent = 999999;
    if (invalid == 2)
      rejected[2].parent = child.id;
    if (invalid == 3)
      rejected[0].transform.sx = 0;
    if (invalid == 4)
      rejected[0].camera_data.far_plane = 0.1;
    if (invalid == 5)
      rejected[1].light_data.intensity = -1;
    if (invalid == 6)
      rejected[0].id = 0;
    Require(history.CloneEntityForest(scene, rejected).empty() && history.UndoDepth() == 0 &&
                world.FindScene(scene)->entities.empty(),
            "rejected forest consumed World/history");
  }
  Require(history.CloneEntityForest(scene, {}).empty() &&
              history.CloneEntityForest(999999, prototypes).empty(),
          "missing/empty forest accepted");
  runtime::Entity collision;
  collision.id = ids[1];
  const_cast<runtime::Scene *>(world.FindScene(other))->entities.push_back(collision);
  Require(!history.Redo() && world.FindScene(scene)->entities.empty() && history.UndoDepth() == 0,
          "forest Redo restored a partial batch during an ID collision");
  runtime::WorldCommandBuffer remove_collision;
  remove_collision.DestroyEntity(collision.id);
  prototypes[0].camera_data = {90, 1, 100}; // History must own the original initialized values.
  prototypes[2].transform.x = 999;
  Require(remove_collision.Apply(world) && history.Redo() && world.SaveScene(scene) == created,
          "forest Redo lost owning initialized payloads or stable identity");
  for (int replay = 0; replay < 1000; ++replay)
    Require(history.Undo() && history.Redo() && world.SaveScene(scene) == created,
            "1,000 initialized forest replay cycles changed the snapshot");
  Require(world.RequestUnload(scene) && history.CloneEntityForest(scene, prototypes).empty() &&
              !history.Undo() && history.UndoDepth() == 1,
          "expired scene accepted forest writes");
}
void GraphicalClipboard() {
  const auto directory =
      std::filesystem::temp_directory_path() /
      ("nexora-clipboard-forest-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(directory);
  runtime::World world;
  const auto scene_id = world.LoadScene("Clipboard forest");
  Require(world.Activate(scene_id), "clipboard scene activation failed");
  editor::SceneDocument scene(world, scene_id);
  const auto a = scene.Create("A"), child = scene.Create("Child", a);
  const auto grand = scene.Create("Grand", child), b = scene.Create("B");
  const auto outside = scene.Create("Outside");
  Require(scene.SetTransform(a, {3, 2, -1}) && scene.SetTransform(child, {1, 2, 3}) &&
              scene.SetEulerField(std::array{*scene.Key(child)}, 2, 720) &&
              scene.SetCamera(*scene.Key(child), runtime::CameraComponent{72, 0.2, 800}) &&
              scene.SetLight(*scene.Key(b), runtime::LightComponent{7}) &&
              scene.SetMeshRenderer(*scene.Key(grand), runtime::MeshComponent{51, {99}}) &&
              scene.SetOpaqueComponent(*scene.Key(grand), {91, "Missing plugin", {1, 2, 255}}) &&
              scene.SetTransform(outside, {30, 0, 0}),
          "clipboard payload setup failed");
  const std::array selected{b, child, a};
  Require(scene.Select(selected), "forest selection failed");
  const auto opaque = scene.OpaqueComponents(*scene.Key(grand));
  editor::ProductShell shell;
  editor::imgui::EditorImGuiHost ui;
  ui.SetDisplay(1280, 900, 1);
  Access::SetInputTrickle(ui, false);
  Nexora::Window::WindowEvent focus;
  focus.type = Nexora::Window::WindowEventType::FocusChanged;
  focus.value0 = 1;
  ui.ProcessEvents(std::array{focus});
  const auto draw = [&] {
    ui.BeginFrame();
    ui.DrawProductShell(shell, &scene);
    static_cast<void>(ui.EndFrame());
  };
  for (int frame = 0; frame < 4; ++frame)
    draw();
  Access::FocusHierarchy(ui);
  draw();
  draw();
  const auto tap = [&](Nexora::Window::Key key) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = Nexora::Window::KeyModifiers::Control;
    ui.ProcessEvents(std::array{event});
    draw();
    event.value1 = 0;
    event.modifiers = Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    draw();
  };
  tap(Nexora::Window::Key::C);
  Require(scene.SetTransform(a, {12, 0, 0}) &&
              scene.SetCamera(*scene.Key(child), runtime::CameraComponent{90, 1, 100}),
          "copy-time snapshot mutation failed");
  const auto baseline = world.SaveScene(scene_id);
  tap(Nexora::Window::Key::V);
  Require(scene.Nodes().size() == 9 && scene.Selection().size() == 2,
          "graphical Paste did not create/select two complete root forests");
  runtime::Id copied_a{}, copied_b{}, copied_child{}, copied_grand{};
  for (const auto id : scene.Selection()) {
    if (scene.Name(id) == "A Copy")
      copied_a = id;
    if (scene.Name(id) == "B Copy")
      copied_b = id;
  }
  Require(copied_a && copied_b && world.Children(copied_a).size() == 1,
          "forest root naming/parent mapping failed");
  copied_child = world.Children(copied_a).front();
  Require(world.Children(copied_child).size() == 1, "copied grandchild is absent");
  copied_grand = world.Children(copied_child).front();
  const auto verify = [&] {
    Require(scene.Transform(copied_a)->x == 3 && scene.Name(copied_child) == "Child" &&
                scene.Camera(*scene.Key(copied_child))->vertical_field_of_view == 72 &&
                scene.Light(*scene.Key(copied_b))->intensity == 7 &&
                scene.MeshRenderer(*scene.Key(copied_grand))->mesh == 51 &&
                scene.MeshRenderer(*scene.Key(copied_grand))->material.shader == 99 &&
                (*scene.EulerAngles(copied_child))[2] == 720 &&
                scene.OpaqueComponents(*scene.Key(copied_grand)) == opaque,
            "clipboard lost copy-time pose, components, hints or opaque payloads");
  };
  verify();
  const auto created = world.SaveScene(scene_id);
  for (int step = 1; step <= 1000; ++step)
    Require(scene.SetTransform(outside, {30.0 + step, 0, 0}), "document history setup failed");
  for (int step = 0; step < 1000; ++step)
    Require(scene.Undo(), "1,000-step document Undo failed");
  Require(world.SaveScene(scene_id) == created, "document Undo changed the initialized forest");
  for (int step = 0; step < 1000; ++step)
    Require(scene.Redo(), "1,000-step document Redo failed");
  Require(scene.Transform(outside)->x == 1030, "document Redo lost its final transform");
  for (int step = 0; step < 1000; ++step)
    Require(scene.Undo(), "document history reset failed");
  for (int replay = 0; replay < 3; ++replay) {
    tap(Nexora::Window::Key::Z);
    Require(world.SaveScene(scene_id) == baseline &&
                std::ranges::equal(scene.Selection(), selected),
            "one Paste Undo did not remove all new entities and restore prior selection");
    tap(Nexora::Window::Key::Y);
    Require(world.SaveScene(scene_id) == created && scene.Nodes().size() == 9,
            "Paste Redo lost initialized forest/identity");
    verify();
  }
  // Duplicating a child detaches its captured world root and preserves its descendants.
  const auto child_world = scene.WorldTransform(child);
  Require(scene.Select(std::array{child}), "duplicate child selection failed");
  tap(Nexora::Window::Key::D);
  const auto duplicated = scene.Selection().front();
  Require(scene.Nodes().size() == 11 && scene.Name(duplicated) == "Child Copy" &&
              scene.Parent(duplicated) == 0 && scene.WorldTransform(duplicated) == child_world &&
              world.Children(duplicated).size() == 1 && scene.Undo() &&
              scene.Selection().size() == 1 && scene.Selection().front() == child,
          "Duplicate lost detached world pose, descendants or atomic Undo");
  tap(Nexora::Window::Key::V);
  Require(scene.Nodes().size() == 13 && scene.Selection().size() == 2 && scene.Undo() &&
              world.SaveScene(scene_id) == created,
          "Duplicate replaced the previous forest clipboard");
  Require(scene.Save(directory / "Created.scene") && scene.Reload(directory / "Created.scene") &&
              scene.Nodes().size() == 9,
          "forest persistence failed");
  verify();
  std::filesystem::remove_all(directory);
}
} // namespace
int main() {
  try {
    RuntimeForest();
    GraphicalClipboard();
    std::cout << "Owning initialized clipboard forest contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
