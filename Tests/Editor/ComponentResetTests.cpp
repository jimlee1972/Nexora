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
