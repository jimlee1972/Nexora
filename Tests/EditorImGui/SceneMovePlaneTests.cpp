#include "Nexora/Editor/EditorWorkspace.h"
#include "SceneMovePlanes.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
using namespace nexora;
using namespace editor;
using namespace editor::preview;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
double Dot(ViewportVector a, ViewportVector b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
ViewportVector Normal(const MovePlane &plane) {
  const auto &a = plane[0], &b = plane[1];
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
ViewportRay Ray(ViewportVector point, ViewportVector normal) {
  return {{point.x + normal.x * 10, point.y + normal.y * 10, point.z + normal.z * 10},
          {-normal.x * 3, -normal.y * 3, -normal.z * 3}};
}
void TestPlanes(bool local) {
  runtime::Transform pose{3, -2, 1};
  pose.qy = pose.qw = std::sqrt(0.5);
  const auto handles = MovePlaneHandles(pose, local);
  Require(handles.size() == 3, "Move did not produce all three plane handles");
  const ViewportVector pivot{pose.x, pose.y + 0.5, pose.z};
  for (std::size_t index = 0; index < handles.size(); ++index) {
    const auto &handle = handles[index];
    const auto normal = Normal(handle.plane);
    const auto &instance = handle.instance;
    const ViewportVector center{instance.translation[0], instance.translation[1],
                                instance.translation[2]};
    const auto ray = Ray(center, normal);
    const auto box = PickOrientedBox(
        ray, center, {instance.scale[0], instance.scale[1], instance.scale[2]},
        {instance.rotation[0], instance.rotation[1], instance.rotation[2], instance.rotation[3]},
        500);
    Require(handle.bounds.entity == index + 4 && box &&
                PickNearest(ray, std::span(&handle.bounds, 1), 500),
            "rendered plane center could not be picked through its shared bounds");
    const auto &a = handle.plane[0], &b = handle.plane[1];
    const ViewportVector end{pivot.x + a.x * 1.3 - b.x * 0.76, pivot.y + a.y * 1.3 - b.y * 0.76,
                             pivot.z + a.z * 1.3 - b.z * 0.76};
    const auto delta =
        MovePlaneDelta(Ray(pivot, normal), Ray(end, normal), pivot, handle.plane, 0.5);
    Require(delta.has_value(), "valid Global/Local plane drag was rejected");
    const ViewportVector movement{(*delta)[0], (*delta)[1], (*delta)[2]};
    Require(std::abs(Dot(movement, a) - 1.5) < 1e-9 && std::abs(Dot(movement, b) + 1) < 1e-9 &&
                std::abs(Dot(movement, normal)) < 1e-9,
            "plane drag did not constrain and snap its two captured axes");
    auto parallel = ray;
    parallel.direction = a;
    Require(!MovePlaneDelta(ray, parallel, pivot, handle.plane, 0),
            "parallel plane ray produced a move");
    auto behind = ray;
    behind.direction = normal;
    Require(!MovePlaneDelta(ray, behind, pivot, handle.plane, 0),
            "behind-camera plane intersection produced a move");
    auto distant = ray;
    distant.origin = {center.x + normal.x * 501, center.y + normal.y * 501,
                      center.z + normal.z * 501};
    Require(!MovePlaneDelta(ray, distant, pivot, handle.plane, 0),
            "out-of-range plane intersection produced a move");
    auto invalid_ray = ray;
    invalid_ray.direction = {};
    Require(!MovePlaneDelta(ray, invalid_ray, pivot, handle.plane, 0),
            "zero-length plane ray produced a move");
    invalid_ray = ray;
    invalid_ray.origin.x = std::numeric_limits<double>::infinity();
    Require(!MovePlaneDelta(ray, invalid_ray, pivot, handle.plane, 0),
            "nonfinite plane ray produced a move");
    auto invalid = handle.plane;
    invalid[1] = invalid[0];
    Require(!MovePlaneDelta(ray, ray, pivot, invalid, 0), "degenerate plane was accepted");
    invalid = handle.plane;
    invalid[0].x = std::numeric_limits<double>::quiet_NaN();
    Require(!MovePlaneDelta(ray, ray, pivot, invalid, 0), "nonfinite plane was accepted");
  }
}
void TestAuthoring() {
  runtime::World world;
  const auto scene_id = world.LoadScene("Plane roots");
  Require(world.Activate(scene_id), "plane world failed");
  SceneDocument scene(world, scene_id);
  const auto parent = scene.Create("Mirrored parent");
  const auto child = scene.Create("Child", parent);
  const auto descendant = scene.Create("Selected descendant", child);
  const auto other = scene.Create("Other root");
  runtime::Transform parent_pose{4, 2, -1};
  parent_pose.sx = -2;
  parent_pose.sy = 3;
  parent_pose.sz = 4;
  parent_pose.qz = std::sin(0.3);
  parent_pose.qw = std::cos(0.3);
  Require(scene.SetTransform(parent, parent_pose) && scene.SetTransform(child, {1, 2, 3}) &&
              scene.SetTransform(descendant, {2, 1, 0}) && scene.SetTransform(other, {-3, 1, 2}) &&
              scene.SetOpaqueComponent(*scene.Key(child), {51, "Absent plugin", {0, 1, 255}}),
          "plane parent/component fixture failed");
  const auto retained = scene.Create("Retained Redo");
  Require(scene.Undo() && scene.Select(std::array{child, descendant, other}),
          "plane selection/history fixture failed");
  const auto baseline = world.SaveScene(scene_id);
  const auto dirty = scene.Dirty();
  const auto frame = scene.SelectionGizmoFrame(GizmoPivot::Center);
  Require(frame.has_value(), "plane center frame missing");
  const auto handle = MovePlaneHandles(*frame, true)[2];
  const auto normal = Normal(handle.plane);
  const ViewportVector origin{frame->x, frame->y + 0.5, frame->z};
  const ViewportVector end{origin.x + handle.plane[0].x + handle.plane[1].x,
                           origin.y + handle.plane[0].y + handle.plane[1].y,
                           origin.z + handle.plane[0].z + handle.plane[1].z};
  const auto delta =
      MovePlaneDelta(Ray(origin, normal), Ray(end, normal), origin, handle.plane, 0.5);
  Require(delta.has_value(), "parented Local plane move rejected");
  const std::array keys{*scene.Key(child), *scene.Key(descendant), *scene.Key(other)};
  const std::array before{*scene.WorldTransform(child), *scene.WorldTransform(descendant),
                          *scene.WorldTransform(other)};
  GizmoOperation operation;
  operation.translation = {(*delta)[0], (*delta)[1], (*delta)[2]};
  const auto preview = scene.PreviewSelectionGizmo(keys, operation);
  Require(preview && world.SaveScene(scene_id) == baseline && scene.Dirty() == dirty &&
              scene.Redo() && scene.Name(retained) == "Retained Redo" && scene.Undo(),
          "plane preview mutated World/history");
  Require(scene.Select(std::array{child, descendant, other}) &&
              scene.TranslateSelection(keys, (*delta)[0], (*delta)[1], (*delta)[2]),
          "plane release did not commit selected roots");
  for (std::size_t index = 0; index < keys.size(); ++index) {
    const auto after = scene.WorldTransform(keys[index].id);
    const auto expected = preview->at(keys[index].id);
    Require(after && std::abs(after->x - before[index].x - (*delta)[0]) < 1e-8 &&
                std::abs(after->y - before[index].y - (*delta)[1]) < 1e-8 &&
                std::abs(after->z - before[index].z - (*delta)[2]) < 1e-8 &&
                std::abs(after->x - expected.x) < 1e-8 && std::abs(after->y - expected.y) < 1e-8 &&
                std::abs(after->z - expected.z) < 1e-8,
            "plane preview/release disagreed or moved a selected descendant twice");
  }
  Require(scene.WorldTransform(parent)->x == parent_pose.x &&
              scene.InspectOpaqueComponents(keys[0])->front().preview ==
                  std::vector<std::uint8_t>({0, 1, 255}) &&
              scene.Undo() && world.SaveScene(scene_id) == baseline && scene.Dirty() == dirty,
          "plane move lost parent/component data or one-step Undo");
}
} // namespace
int main() {
  try {
    TestPlanes(false);
    TestPlanes(true);
    TestAuthoring();
    std::cout << "Native Move plane contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
