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
void RuntimeBatch() {
  runtime::World world;
  const auto scene = world.LoadScene("Batch delete");
  const auto other = world.LoadScene("Other");
  Require(world.Activate(scene), "runtime activation failed");
  const auto a = world.CreateEntity(scene).id;
  const auto survivor = world.CreateEntity(scene).id;
  const auto b = world.CreateEntity(scene).id;
  const auto a_child = world.CreateEntity(scene).id;
  const auto b_child = world.CreateEntity(scene).id;
  const auto foreign = world.CreateEntity(other).id;
  runtime::WorldCommandBuffer setup;
  setup.SetParent(a_child, a, false);
  setup.SetParent(b_child, b, false);
  setup.SetTransform(b, {3, 2, -4});
  setup.SetCamera(a_child, runtime::CameraComponent{75, 0.2, 900});
  setup.SetLight(b_child, runtime::LightComponent{7});
  setup.SetMeshRenderer(b, runtime::MeshComponent{41, {std::numeric_limits<runtime::Id>::max()}});
  Require(setup.Apply(world), "runtime batch setup failed");
  const auto before = world.SaveScene(scene);
  runtime::SceneEditor history(world);
  const std::array selected{b_child, b, a_child, a}; // Reversed, with selected descendants.
  Require(history.DestroyEntities(scene, selected) && history.UndoDepth() == 1 &&
              world.FindScene(scene)->entities.size() == 1 && world.FindEntity(survivor),
          "batch destruction did not collapse descendants into one transaction");
  // Deliberately introduce an ID collision from another owner to exercise atomic replay rejection.
  auto *collision_scene = const_cast<runtime::Scene *>(world.FindScene(other));
  runtime::Entity collision;
  collision.id = b;
  collision_scene->entities.push_back(collision);
  const auto deleted = world.SaveScene(scene);
  Require(!history.Undo() && history.UndoDepth() == 1 && world.SaveScene(scene) == deleted &&
              !world.FindEntity(a),
          "failed Undo restored part of a batch");
  runtime::WorldCommandBuffer remove_collision;
  remove_collision.DestroyEntity(b);
  Require(remove_collision.Apply(world) && history.Undo() && history.UndoDepth() == 0 &&
              world.SaveScene(scene) == before && world.SiblingIndex(a) == 0 &&
              world.SiblingIndex(survivor) == 1 && world.SiblingIndex(b) == 2,
          "batch Undo lost hierarchy, order, components or stable IDs");
  const std::array duplicate{a, a};
  const std::array cross_scene{a, foreign};
  const std::array missing{a, runtime::Id{999999}};
  Require(!history.DestroyEntities(scene, {}) && !history.DestroyEntities(scene, duplicate) &&
              !history.DestroyEntities(scene, cross_scene) &&
              !history.DestroyEntities(scene, missing) && history.UndoDepth() == 0 &&
              world.SaveScene(scene) == before,
          "invalid batch mutated the World or consumed history");
  const auto added = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer extra_child;
  extra_child.SetParent(added, a, false);
  Require(extra_child.Apply(world), "external child setup failed");
  const auto extended = world.SaveScene(scene);
  Require(!history.Redo() && world.SaveScene(scene) == extended && history.UndoDepth() == 0,
          "Redo expanded the recorded deletion to an external child");
  runtime::WorldCommandBuffer remove_child;
  remove_child.DestroyEntity(added);
  Require(remove_child.Apply(world) && history.Redo() && history.UndoDepth() == 1 &&
              history.Undo() && world.SaveScene(scene) == before,
          "rejected batch replay discarded Redo or component snapshots");
  for (int replay = 0; replay < 1000; ++replay)
    Require(history.Redo() && history.Undo() && history.UndoDepth() == 0 &&
                world.SaveScene(scene) == before,
            "1,000 batch replay cycles changed the snapshot");
  Require(world.RequestUnload(scene) && !history.DestroyEntities(scene, std::array{a, b}) &&
              !history.DestroyEntity(scene, a) && history.UndoDepth() == 0,
          "unloading scene accepted authoring deletion");
}
void OrphanRestoration() {
  runtime::World world;
  const auto scene = world.LoadScene("Orphan batch");
  const auto parent = world.CreateEntity(scene).id;
  const auto a = world.CreateEntity(scene).id;
  const auto b = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer setup;
  setup.SetTransform(parent, {10, 2, 3, 0, 0, 0, 1, -2, 3, 4});
  setup.SetTransform(a, {1, 2, 3});
  setup.SetTransform(b, {-1, 1, 2});
  setup.SetParent(a, parent, false);
  setup.SetParent(b, parent, false);
  Require(setup.Apply(world), "orphan setup failed");
  const auto pose_a = world.WorldTransform(a), pose_b = world.WorldTransform(b);
  runtime::SceneEditor history(world);
  Require(history.DestroyEntities(scene, std::array{b, a}), "orphan batch delete failed");
  runtime::WorldCommandBuffer remove_parent;
  remove_parent.DestroyEntity(parent);
  Require(remove_parent.Apply(world) && history.Undo() && world.Parent(a) == 0 &&
              world.Parent(b) == 0 && world.WorldTransform(a) == pose_a &&
              world.WorldTransform(b) == pose_b && world.SiblingIndex(a) == 0 &&
              world.SiblingIndex(b) == 1,
          "orphan batch lost captured world poses or sibling order");
}
void OrphansFromDifferentParents() {
  runtime::World world;
  const auto scene = world.LoadScene("Independent orphan groups");
  const auto parent_a = world.CreateEntity(scene).id;
  const auto a = world.CreateEntity(scene).id;
  const auto parent_b = world.CreateEntity(scene).id;
  const auto b = world.CreateEntity(scene).id;
  const auto survivor = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer setup;
  setup.SetTransform(parent_a, {10, 0, 0});
  setup.SetTransform(parent_b, {-10, 0, 0});
  setup.SetParent(a, parent_a, false);
  setup.SetParent(b, parent_b, false);
  Require(setup.Apply(world) && world.SiblingIndex(a) == 0 && world.SiblingIndex(b) == 0,
          "independent sibling fixture failed");
  const auto pose_a = world.WorldTransform(a), pose_b = world.WorldTransform(b);
  runtime::SceneEditor history(world);
  Require(history.DestroyEntities(scene, std::array{b, a}), "independent orphan delete failed");
  runtime::WorldCommandBuffer remove_parents;
  remove_parents.DestroyEntity(parent_a);
  remove_parents.DestroyEntity(parent_b);
  Require(remove_parents.Apply(world) && history.Undo() && world.WorldTransform(a) == pose_a &&
              world.WorldTransform(b) == pose_b && world.SiblingIndex(survivor) == 0 &&
              world.SiblingIndex(a) == 1 && world.SiblingIndex(b) == 2,
          "independent orphan indexes reversed restored or unrelated roots");
  const auto restored = world.SaveScene(scene);
  Require(history.Redo() && history.Undo() && world.SaveScene(scene) == restored,
          "independent orphan replay changed merged root order");
}
void UnrelatedCreation() {
  runtime::World world;
  const auto scene = world.LoadScene("Unrelated creation");
  const auto a = world.CreateEntity(scene).id;
  const auto b = world.CreateEntity(scene).id;
  runtime::SceneEditor history(world);
  Require(history.DestroyEntities(scene, std::array{b, a}), "unrelated setup failed");
  const auto extra = world.CreateEntity(scene).id;
  runtime::WorldCommandBuffer setup;
  setup.SetLight(extra, runtime::LightComponent{8});
  Require(setup.Apply(world) && history.Undo() &&
              world.FindEntity(extra)->light_data.intensity == 8 && world.SiblingIndex(a) == 0 &&
              world.SiblingIndex(b) == 1 && world.SiblingIndex(extra) == 2 && history.Redo() &&
              world.FindScene(scene)->entities.size() == 1 && world.FindEntity(extra) &&
              history.Undo() && world.FindEntity(extra)->light_data.intensity == 8,
          "batch replay lost unrelated entities or sibling order");
}
void GraphicalDelete() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-selection-delete-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  std::filesystem::create_directories(root);
  runtime::World world;
  const auto scene_id = world.LoadScene("Graphical delete");
  Require(world.Activate(scene_id), "document activation failed");
  editor::SceneDocument scene(world, scene_id);
  const auto a = scene.Create("Root A");
  const auto child = scene.Create("Nested", a);
  const auto survivor = scene.Create("Survivor");
  const auto b = scene.Create("Root B");
  const auto grandchild = scene.Create("Grandchild", child);
  const auto hint = *scene.Key(child);
  Require(
      scene.SetEulerField(std::array{hint}, 2, 720) &&
          scene.SetOpaqueComponent(*scene.Key(grandchild), {91, "Missing plugin", {1, 2, 255}}) &&
          scene.SetCamera(*scene.Key(b), runtime::CameraComponent{72, 0.2, 800}) &&
          scene.SetMeshRenderer(*scene.Key(child), runtime::MeshComponent{51, {99}}) &&
          scene.Save(root / "Before.scene"),
      "document payload setup failed");
  const std::array selected{b, grandchild, a, child};
  Require(scene.Select(selected), "document selection failed");
  const auto before = world.SaveScene(scene_id);
  const auto opaque = scene.OpaqueComponents(*scene.Key(grandchild));
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
  const auto tap = [&](Nexora::Window::Key key, Nexora::Window::KeyModifiers modifiers) {
    Nexora::Window::WindowEvent event;
    event.type = Nexora::Window::WindowEventType::Key;
    event.value0 = static_cast<int>(key);
    event.value1 = 1;
    event.modifiers = modifiers;
    ui.ProcessEvents(std::array{event});
    draw();
    event.value1 = 0;
    event.modifiers = Nexora::Window::KeyModifiers::None;
    ui.ProcessEvents(std::array{event});
    draw();
  };
  tap(Nexora::Window::Key::Delete, Nexora::Window::KeyModifiers::None);
  Require(shell.LastCommand() == "editor.scene.delete" && scene.Nodes().size() == 1 &&
              scene.Name(survivor) == "Survivor" && scene.Selection().empty() && scene.Dirty(),
          "graphical Delete did not remove all selected subtrees");
  for (int replay = 0; replay < 3; ++replay) {
    tap(Nexora::Window::Key::Z, Nexora::Window::KeyModifiers::Control);
    Require(world.SaveScene(scene_id) == before && !scene.Dirty() && scene.Nodes().size() == 5 &&
                std::ranges::equal(scene.Selection(), selected) && scene.Name(child) == "Nested" &&
                (*scene.EulerAngles(child))[2] == 720 &&
                scene.OpaqueComponents(*scene.Key(grandchild)) == opaque,
            "one graphical Undo failed to restore the complete selection/metadata");
    tap(Nexora::Window::Key::Y, Nexora::Window::KeyModifiers::Control);
    Require(scene.Nodes().size() == 1 && scene.Selection().empty(), "graphical Redo failed");
  }
  Require(scene.Save(root / "Deleted.scene") && scene.Reload(root / "Deleted.scene") &&
              scene.Nodes().size() == 1 && scene.Name(survivor) == "Survivor" && !scene.Undo(),
          "deleted selection resurrected after save/reload");
  Require(scene.Reload(root / "Before.scene") && scene.Nodes().size() == 5 &&
              scene.OpaqueComponents(*scene.Key(grandchild)) == opaque &&
              (*scene.EulerAngles(child))[2] == 720,
          "restored metadata failed persistence");
  std::filesystem::remove_all(root);
}
} // namespace
int main() {
  try {
    RuntimeBatch();
    OrphanRestoration();
    OrphansFromDifferentParents();
    UnrelatedCreation();
    GraphicalDelete();
    std::cout << "Atomic selected-subtree deletion contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
