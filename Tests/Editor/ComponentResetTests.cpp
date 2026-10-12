#include "Nexora/Editor/EditorWorkspace.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
bool DefaultCamera(const std::optional<runtime::CameraComponent> &camera) {
  return camera && camera->vertical_field_of_view == 60 && camera->near_plane == 0.1 &&
         camera->far_plane == 1000;
}
void RunDormantHistory() {
  runtime::World world;
  const auto id = world.LoadScene("Dormant component history");
  std::array<runtime::Id, 3> ids{};
  for (std::size_t i = 0; i < ids.size(); ++i) {
    // Populate each freshly created entity before another storage-changing call.
    auto &entity = world.CreateEntity(id);
    ids[i] = entity.id;
    entity.camera = entity.light = entity.mesh_renderer = i == 0;
    entity.camera_data = {91 - static_cast<double>(i), .2, 950};
    entity.light_data = {3.5F + static_cast<float>(i)};
    entity.mesh_data = {123456789012345ULL + i, {987654321098765ULL + i}};
  }
  editor::SceneDocument scene(world, id);
  const auto fixture = world.SaveScene(id);
  Require(fixture.has_value(), "Dormant runtime fixture capture failed");
  std::string authoring = "NEXORA_EDITOR_SCENE 3\n";
  for (const auto entity : ids)
    authoring += "node " + std::to_string(entity) + " 0 Dormant " + std::to_string(entity) + "\n";
  authoring += "world\n" + *fixture;
  Require(scene.ReloadBytes(authoring), "Dormant source adoption failed");
  const std::array keys{*scene.Key(ids[0]), *scene.Key(ids[1]), *scene.Key(ids[2])};
  Require(scene.Select(keys) && scene.CopySelection() &&
              scene.SetOpaqueComponent(keys[1], {71, "Absent.Provider", {0, 255, 27}}) &&
              scene.SetEulerField(std::array{keys[2]}, 0, 720),
          "Dormant authoring fixture failed");
  const auto original = *scene.PrepareSave();
  const auto selection = std::vector(scene.Selection().begin(), scene.Selection().end());
  const auto opaque = scene.OpaqueComponents(keys[1]);
  const auto redo_sentinel = scene.Create("Dormant no-op Redo");
  Require(scene.Undo() && scene.SetCamera(keys[2], std::nullopt) &&
              scene.SetLight(keys[2], std::nullopt) &&
              scene.SetMeshRenderer(keys[2], std::nullopt) && scene.MatchesPreparedSave(original) &&
              scene.Redo() && scene.Name(redo_sentinel) == "Dormant no-op Redo" && scene.Undo() &&
              scene.MatchesPreparedSave(original),
          "Absent component no-op reset dormant values or consumed Redo");
  const std::array<std::optional<runtime::CameraComponent>, 3> cameras{
      std::nullopt, runtime::CameraComponent{72, .3, 400}, std::nullopt};
  const std::array<std::optional<runtime::LightComponent>, 3> lights{
      std::nullopt, runtime::LightComponent{9}, std::nullopt};
  const std::array<std::optional<runtime::MeshComponent>, 3> meshes{
      std::nullopt, runtime::MeshComponent{8123, {9456}}, std::nullopt};
  const auto replay = [&] {
    const auto changed = *scene.PrepareSave();
    for (int cycle = 0; cycle < 40; ++cycle)
      Require(scene.Undo() && scene.MatchesPreparedSave(original) && scene.Redo() &&
                  scene.MatchesPreparedSave(changed),
              "Presence edit lost complete dormant values in one batch Undo/Redo");
    Require(scene.Undo() && scene.MatchesPreparedSave(original),
            "Presence edit did not restore exact authoring bytes");
  };
  Require(scene.SetCameras(keys, cameras), "Mixed Camera presence edit failed");
  Require(!world.FindEntity(ids[0])->camera &&
              world.FindEntity(ids[0])->camera_data.vertical_field_of_view == 60 &&
              world.FindEntity(ids[1])->camera,
          "Camera removal no longer reset its stored values");
  replay();
  auto invalid_cameras = cameras;
  invalid_cameras[2] = runtime::CameraComponent{180, .1, 100};
  Require(!scene.SetCameras(keys, invalid_cameras) && scene.MatchesPreparedSave(original) &&
              scene.Redo() && scene.Undo(),
          "Invalid Camera batch mutated dormant data or consumed Redo");
  Require(scene.SetLights(keys, lights), "Mixed Light presence edit failed");
  Require(!world.FindEntity(ids[0])->light && world.FindEntity(ids[0])->light_data.intensity == 1,
          "Light removal no longer reset its stored values");
  replay();
  auto invalid_lights = lights;
  invalid_lights[2] = runtime::LightComponent{-1};
  Require(!scene.SetLights(keys, invalid_lights) && scene.MatchesPreparedSave(original) &&
              scene.Redo() && scene.Undo(),
          "Invalid Light batch mutated dormant data or consumed Redo");
  Require(scene.SetMeshRenderers(keys, meshes), "Mixed Mesh presence edit failed");
  Require(!world.FindEntity(ids[0])->mesh_renderer &&
              world.FindEntity(ids[0])->mesh_data.mesh == 0 &&
              world.FindEntity(ids[0])->mesh_data.material.shader == 0,
          "Mesh removal no longer reset resource references");
  replay();
  auto stale = keys;
  ++stale[2].document_generation;
  Require(!scene.SetMeshRenderers(stale, meshes) &&
              !scene.SetMeshRenderers(std::array{keys[0], keys[0], keys[2]}, meshes) &&
              scene.MatchesPreparedSave(original) && scene.Redo() && scene.Undo() &&
              std::ranges::equal(scene.Selection(), selection) &&
              scene.OpaqueComponents(keys[1]) == opaque &&
              scene.EulerAngles(ids[2])->at(0) == 720 && scene.Key(ids[1]) == keys[1] &&
              scene.Paste() && scene.Undo() && scene.MatchesPreparedSave(original),
          "Presence history changed unrelated metadata, keys, clipboard or invalid-batch Redo");

  runtime::SceneEditor direct(world);
  Require(direct.SetCameras(ids, cameras), "Failed replay fixture edit failed");
  runtime::WorldCommandBuffer destroy;
  destroy.DestroyEntity(ids[2]);
  Require(destroy.Apply(world), "Failed replay fixture deletion failed");
  const auto deleted = world.SaveScene(id);
  const auto depth = direct.UndoDepth();
  Require(!direct.Undo() && direct.UndoDepth() == depth && world.SaveScene(id) == deleted,
          "Missing-target replay partially restored components or moved the history cursor");
}
void RunStaleHint() {
  runtime::World world;
  const auto id = world.LoadScene("Latent Euler reset");
  editor::SceneDocument scene(world, id);
  const auto entity = scene.Create("Previously rotated");
  const std::array keys{*scene.Key(entity)};
  Require(scene.SetEulerField(keys, 0, 450), "latent hint fixture failed");
  const auto quarter_turn = *scene.Transform(entity);
  Require(scene.SetTransform(entity, {}) && scene.EulerAngles(entity) == editor::EulerDegrees{},
          "latent hint was not hidden at identity");
  const auto identity = world.SaveScene(id);
  Require(scene.ResetTransforms(keys) && world.SaveScene(id) == identity &&
              scene.SetTransform(entity, quarter_turn) && scene.EulerAngles(entity)->at(0) > 89 &&
              scene.EulerAngles(entity)->at(0) < 91,
          "Reset revived latent 450-degree revolutions after another rotation");
  Require(scene.Undo() && scene.Undo() && world.SaveScene(id) == identity &&
              scene.SetTransform(entity, quarter_turn) && scene.EulerAngles(entity)->at(0) == 450,
          "metadata-only reset Undo did not restore the previous latent hint");
  Require(scene.Undo() && scene.ResetTransforms(keys), "latent reset replay fixture failed");
  for (int cycle = 0; cycle < 100; ++cycle)
    Require(scene.Undo() && scene.Redo() && world.SaveScene(id) == identity,
            "latent reset desynchronized Runtime and metadata replay");
  Require(scene.SetTransform(entity, quarter_turn) && scene.EulerAngles(entity)->at(0) < 91,
          "latent hint survived repeated reset replay");
}
void Run(const std::filesystem::path &root) {
  runtime::World world;
  const auto id = world.LoadScene("Reset components");
  Require(world.Activate(id), "reset scene activation failed");
  editor::SceneDocument scene(world, id);
  const auto parent = scene.Create("Unselected parent");
  const auto first = scene.Create("First", parent);
  const auto second = scene.Create("Child", first);
  const auto third = scene.Create("Missing components");
  const std::array keys{*scene.Key(first), *scene.Key(second), *scene.Key(third)};
  Require(
      scene.SetTransforms(keys, std::array{runtime::Transform{1, 2, 3, 0, 0, 0, 1, -2, 3, 4},
                                           runtime::Transform{-5, 6, 7}, runtime::Transform{}}) &&
          scene.SetEulerField(std::array{keys[0]}, 0, 720) &&
          scene.SetEulerField(std::array{keys[1]}, 2, -450) && scene.Select(keys),
      "reset transform fixture failed");
  const std::array<std::optional<runtime::CameraComponent>, 3> cameras{
      runtime::CameraComponent{45, 0.2, 100}, runtime::CameraComponent{75, 0.5, 200}, std::nullopt};
  const std::array<std::optional<runtime::LightComponent>, 3> lights{
      runtime::LightComponent{4}, std::nullopt, runtime::LightComponent{7}};
  Require(scene.SetCameras(keys, cameras) && scene.SetLights(keys, lights) &&
              scene.SetMeshRenderer(keys[0], runtime::MeshComponent{7, {9}}) &&
              scene.SetOpaqueComponent(keys[0], {31, "Missing plugin", {1, 2, 3}}),
          "reset component fixture failed");
  const auto original = world.SaveScene(id);
  const std::vector<runtime::Id> selected(scene.Selection().begin(), scene.Selection().end());
  const auto opaque = scene.OpaqueComponents(keys[0]);
  Require(scene.ResetTransforms(keys), "transform reset failed");
  for (const auto key : keys)
    Require(scene.Transform(key.id) == runtime::Transform{} &&
                scene.EulerAngles(key.id) == editor::EulerDegrees{},
            "reset did not clear local TRS and authored revolutions");
  Require(scene.Parent(first) == parent && scene.Parent(second) == first &&
              std::ranges::equal(scene.Selection(), selected) && scene.Name(second) == "Child" &&
              scene.OpaqueComponents(keys[0]) == opaque && scene.MeshRenderer(keys[0])->mesh == 7 &&
              scene.MeshRenderer(keys[0])->material.shader == 9 &&
              scene.Camera(keys[0])->vertical_field_of_view == 45 &&
              scene.Light(keys[2])->intensity == 7,
          "transform reset modified hierarchy, selection or unrelated component data");
  Require(scene.Undo() && world.SaveScene(id) == original &&
              scene.EulerAngles(first)->at(0) == 720 && scene.EulerAngles(second)->at(2) == -450 &&
              scene.Redo(),
          "transform reset was not one complete Runtime/metadata Undo step");
  const auto before_cameras = world.SaveScene(id);
  Require(scene.ResetCameras(keys) && DefaultCamera(scene.Camera(keys[0])) &&
              DefaultCamera(scene.Camera(keys[1])) && !scene.Camera(keys[2]) &&
              scene.Light(keys[2])->intensity == 7 && scene.Undo() &&
              world.SaveScene(id) == before_cameras && scene.Redo(),
          "Camera reset lost mixed presence or another component in one-step replay");
  const auto before_lights = world.SaveScene(id);
  Require(scene.ResetLights(keys) && scene.Light(keys[0])->intensity == 1 &&
              !scene.Light(keys[1]) && scene.Light(keys[2])->intensity == 1 &&
              DefaultCamera(scene.Camera(keys[0])) && scene.Undo() &&
              world.SaveScene(id) == before_lights && scene.Redo(),
          "Light reset lost mixed presence or another component in one-step replay");
  const auto reset = world.SaveScene(id);
  auto stale = keys;
  ++stale[1].document_generation;
  for (const auto &invalid : {stale, std::array{keys[0], keys[0], keys[2]}})
    Require(!scene.ResetTransforms(invalid) && !scene.ResetCameras(invalid) &&
                !scene.ResetLights(invalid) && world.SaveScene(id) == reset,
            "invalid reset batch partially mutated the World");
  Require(!scene.ResetTransforms({}) && !scene.ResetCameras({}) && !scene.ResetLights({}),
          "empty reset batch accepted");
  // Keep an exact identity quaternion and an authored 720-degree hint. The reset changes
  // metadata only, but must still own one matching Runtime Undo record for later replay.
  Require(scene.SetEulerField(std::array{keys[2]}, 1, 720) &&
              scene.SetTransform(third, runtime::Transform{}) &&
              scene.Transform(third) == runtime::Transform{} &&
              scene.EulerAngles(third)->at(1) == 720,
          "metadata-only reset fixture failed");
  const auto identity_world = world.SaveScene(id);
  Require(scene.ResetTransforms(std::array{keys[2]}) && world.SaveScene(id) == identity_world &&
              scene.EulerAngles(third) == editor::EulerDegrees{},
          "metadata-only reset changed identity Runtime data or retained revolutions");
  for (int cycle = 0; cycle < 100; ++cycle)
    Require(scene.Undo() && scene.EulerAngles(third)->at(1) == 720 && scene.Redo() &&
                scene.EulerAngles(third) == editor::EulerDegrees{} &&
                world.SaveScene(id) == identity_world,
            "metadata-only reset desynchronized Runtime and document histories");
  const auto extra = scene.Create("Retained Redo");
  Require(scene.Undo() && scene.ResetTransforms(keys) && scene.ResetCameras(keys) &&
              scene.ResetLights(keys) && scene.Redo() && scene.Name(extra) == "Retained Redo" &&
              scene.Undo(),
          "already-default reset consumed Redo");
  std::filesystem::create_directories(root);
  const auto path = root / "reset.scene";
  Require(scene.Save(path), "reset scene save failed");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopened"));
  Require(reopened.Reload(path), "reset scene reload failed");
  for (const auto key : keys)
    Require(reopened.Transform(key.id) == runtime::Transform{} &&
                reopened.EulerAngles(key.id) == editor::EulerDegrees{},
            "reset TRS/Euler metadata did not survive save/reload");
  Require(reopened.Parent(first) == parent && reopened.Parent(second) == first &&
              DefaultCamera(reopened.Camera(*reopened.Key(first))) &&
              !reopened.Camera(*reopened.Key(third)) && !reopened.Light(*reopened.Key(second)) &&
              reopened.OpaqueComponents(*reopened.Key(first)) == opaque &&
              !reopened.ResetTransforms(keys) && !reopened.ResetCameras(keys) &&
              !reopened.ResetLights(keys) && !reopened.Dirty(),
          "reload lost unrelated payloads or admitted pre-reload reset keys");
}
} // namespace
int main() {
  const auto root = std::filesystem::temp_directory_path() /
                    ("nexora-component-reset-" +
                     std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
  try {
    RunDormantHistory();
    RunStaleHint();
    Run(root);
    std::filesystem::remove_all(root);
    std::cout << "Atomic component reset contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove_all(root);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
