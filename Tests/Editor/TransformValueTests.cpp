#include "Nexora/Editor/EditorWorkspace.h"

#include <array>
#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(const std::filesystem::path &path) {
  runtime::World world;
  const auto id = world.LoadScene("Transform values");
  editor::SceneDocument scene(world, id);
  const auto parent = scene.Create("Parent");
  const auto first = scene.Create("First", parent);
  const auto child = scene.Create("Child", first);
  const std::array keys{*scene.Key(first), *scene.Key(child)};
  Require(scene.SetTransforms(
              keys, std::array{runtime::Transform{1, 2, 3}, runtime::Transform{4, 5, 6}}) &&
              scene.SetEulerField(std::array{keys[0]}, 0, 1080) &&
              scene.SetEulerField(std::array{keys[1]}, 2, 450) && scene.Select(keys) &&
              scene.SetCamera(keys[0], runtime::CameraComponent{45, 0.2, 200}) &&
              scene.SetLight(keys[1], runtime::LightComponent{4}) &&
              scene.SetMeshRenderer(keys[0], runtime::MeshComponent{7, {9}}) &&
              scene.SetOpaqueComponent(keys[1], {31, "Missing plugin", {1, 2, 3}}),
          "Transform value fixture failed");
  const auto original = world.SaveScene(id);
  const editor::EulerDegrees angles{720, 30, -450};
  auto value = *editor::WithEulerDegrees({9, 8, 7, 0, 0, 0, 1, -2, 3, 4}, angles);
  // Non-unit input is normalized once by Runtime; metadata must match the actual committed pose.
  value.qx *= 2;
  value.qy *= 2;
  value.qz *= 2;
  value.qw *= 2;
  const auto expected = *runtime::NormalizedTransform(value);
  Require(scene.SetTransformValues(keys, value, angles), "Transform values rejected");
  const auto pasted = world.SaveScene(id);
  for (const auto key : keys)
    Require(scene.Transform(key.id) == expected && scene.EulerAngles(key.id) == angles,
            "Transform values lost local pose, normalization or authored revolutions");
  Require(scene.Parent(first) == parent && scene.Parent(child) == first &&
              scene.Name(child) == "Child" && scene.Selection().size() == 2 &&
              scene.Camera(keys[0])->vertical_field_of_view == 45 &&
              scene.Light(keys[1])->intensity == 4 && scene.MeshRenderer(keys[0])->mesh == 7 &&
              scene.OpaqueComponents(keys[1])->front().data == std::vector<std::uint8_t>{1, 2, 3},
          "Transform values changed hierarchy, selection or other components");
  for (int cycle = 0; cycle < 100; ++cycle)
    Require(scene.Undo() && world.SaveScene(id) == original &&
                scene.EulerAngles(first)->at(0) == 1080 && scene.EulerAngles(child)->at(2) == 450 &&
                scene.Redo() && world.SaveScene(id) == pasted &&
                scene.EulerAngles(first) == angles && scene.EulerAngles(child) == angles,
            "Transform value replay was not one complete Runtime/metadata step");
  const auto extra = scene.Create("Retained Redo");
  Require(scene.Undo(), "Transform value Redo fixture failed");
  auto stale = keys;
  ++stale[1].entity_generation;
  auto invalid = value;
  invalid.sx = 0;
  auto nonfinite = angles;
  nonfinite[1] = std::numeric_limits<double>::infinity();
  Require(!scene.SetTransformValues({}, value, angles) &&
              !scene.SetTransformValues(stale, {}, {}) &&
              !scene.SetTransformValues(std::array{keys[0], keys[0]}, {}, {}) &&
              !scene.SetTransformValues(keys, invalid, angles) &&
              !scene.SetTransformValues(keys, value, nonfinite) &&
              !scene.SetTransformValues(keys, value, {}) &&
              scene.SetTransformValues(keys, value, angles) && world.SaveScene(id) == pasted &&
              scene.Redo() && scene.Name(extra) == "Retained Redo" && scene.Undo(),
          "Invalid/no-op Transform values changed World or consumed Redo");
  Require(scene.Save(path), "Transform values save failed");
  runtime::World reopened_world;
  editor::SceneDocument reopened(reopened_world, reopened_world.LoadScene("Reopened"));
  Require(reopened.Reload(path) && reopened.EulerAngles(first) == angles &&
              reopened.EulerAngles(child) == angles && reopened.Parent(child) == first &&
              !reopened.SetTransformValues(keys, value, angles) && !reopened.Dirty(),
          "Transform values persistence lost revolutions or admitted old keys");
}
void RunMetadata() {
  runtime::World world;
  const auto id = world.LoadScene("Metadata values");
  editor::SceneDocument scene(world, id);
  const auto entity = scene.Create("Target");
  const std::array keys{*scene.Key(entity)};
  const auto identity = world.SaveScene(id);
  const editor::EulerDegrees turns{720, 0, 0};
  Require(scene.SetTransformValues(keys, {}, turns) && world.SaveScene(id) == identity &&
              scene.EulerAngles(entity) == turns,
          "Metadata-only Transform values changed Runtime or lost turns");
  for (int cycle = 0; cycle < 100; ++cycle)
    Require(scene.Undo() && scene.EulerAngles(entity) == editor::EulerDegrees{} && scene.Redo() &&
                scene.EulerAngles(entity) == turns && world.SaveScene(id) == identity,
            "Metadata-only values desynchronized Undo history");
  Require(scene.SetEulerField(keys, 0, 450), "Latent hint fixture failed");
  const auto quarter_turn = *scene.Transform(entity);
  Require(scene.SetTransform(entity, {}) && scene.SetTransformValues(keys, {}, {}) &&
              scene.SetTransform(entity, quarter_turn) && scene.EulerAngles(entity)->at(0) < 91,
          "Pasting identity values revived a stale authored hint");
  Require(scene.Undo() && scene.Undo() && scene.SetTransform(entity, quarter_turn) &&
              scene.EulerAngles(entity)->at(0) == 450,
          "Pasting identity values lost latent hint Undo");
}
} // namespace
int main() {
  const auto path =
      std::filesystem::temp_directory_path() /
      ("nexora-transform-values-" +
       std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()) + ".scene");
  try {
    Run(path);
    RunMetadata();
    std::filesystem::remove(path);
    std::cout << "Atomic Transform value contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::filesystem::remove(path);
    std::cerr << error.what() << '\n';
    return 1;
  }
}
