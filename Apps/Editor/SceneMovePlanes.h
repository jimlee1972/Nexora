#pragma once

#include "Nexora/Editor/ViewportMath.h"
#include "Nexora/Presentation/RenderSurface.h"

#include <array>
#include <cmath>
#include <vector>

namespace nexora::editor::preview {
using MovePlane = std::array<ViewportVector, 2>;

struct MovePlaneHandle final {
  MovePlane plane;
  PickCandidate bounds;
  Nexora::Presentation::SceneInstance instance;
};

// Numeric copies only. The same oriented boxes supply native drawing and exact/padded picking.
[[nodiscard]] inline std::vector<MovePlaneHandle> MovePlaneHandles(const runtime::Transform &pose,
                                                                   bool local_axes) {
  const auto axes = GizmoAxes(pose, local_axes ? GizmoSpace::Local : GizmoSpace::World);
  const auto component = [](ViewportVector value, std::size_t coordinate) {
    return coordinate == 0 ? value.x : coordinate == 1 ? value.y : value.z;
  };
  const std::array pivot{pose.x, pose.y + 0.5, pose.z};
  std::vector<MovePlaneHandle> result;
  for (std::size_t normal = 0; normal < 3; ++normal) {
    const auto first = (normal + 1) % 3, second = (normal + 2) % 3;
    MovePlaneHandle handle{};
    handle.plane = {axes[first], axes[second]};
    handle.bounds.entity = normal + 4; // Appends after the three single-axis Move handles.
    std::array<double, 3> minimum{}, maximum{};
    for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
      const auto position = pivot[coordinate] + 0.65 * (component(axes[first], coordinate) +
                                                        component(axes[second], coordinate));
      double radius = 0;
      for (std::size_t axis = 0; axis < 3; ++axis)
        radius += std::abs(component(axes[axis], coordinate)) * (axis == normal ? 0.015 : 0.12);
      minimum[coordinate] = position - radius;
      maximum[coordinate] = position + radius;
      handle.instance.translation[coordinate] = static_cast<float>(position);
      handle.instance.scale[coordinate] = coordinate == normal ? 0.015F : 0.12F;
      handle.instance.color[coordinate] = coordinate == normal ? 0.12F : 0.95F;
    }
    handle.bounds.min = {minimum[0], minimum[1], minimum[2]};
    handle.bounds.max = {maximum[0], maximum[1], maximum[2]};
    if (local_axes) {
      handle.instance.rotation[0] = static_cast<float>(pose.qx);
      handle.instance.rotation[1] = static_cast<float>(pose.qy);
      handle.instance.rotation[2] = static_cast<float>(pose.qz);
      handle.instance.rotation[3] = static_cast<float>(pose.qw);
    }
    result.push_back(handle);
  }
  return result;
}

// A drag is measured from its start, snapped in the captured orthonormal plane basis, then
// converted to a world delta. Parallel, behind, distant or malformed rays have no defined move.
[[nodiscard]] inline std::optional<std::array<double, 3>>
MovePlaneDelta(const ViewportRay &start, const ViewportRay &current, ViewportVector origin,
               const MovePlane &plane, double snap_step) {
  const auto finite = [](ViewportVector value) {
    return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
  };
  const auto dot = [](ViewportVector a, ViewportVector b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
  };
  if (!finite(origin) || !finite(plane[0]) || !finite(plane[1]) ||
      std::abs(dot(plane[0], plane[0]) - 1) > 1e-6 ||
      std::abs(dot(plane[1], plane[1]) - 1) > 1e-6 || std::abs(dot(plane[0], plane[1])) > 1e-6)
    return std::nullopt;
  const ViewportVector normal{plane[0].y * plane[1].z - plane[0].z * plane[1].y,
                              plane[0].z * plane[1].x - plane[0].x * plane[1].z,
                              plane[0].x * plane[1].y - plane[0].y * plane[1].x};
  const auto hit = [&](const ViewportRay &ray) -> std::optional<ViewportVector> {
    if (!finite(ray.origin) || !finite(ray.direction))
      return std::nullopt;
    const auto length = std::sqrt(dot(ray.direction, ray.direction));
    if (!std::isfinite(length) || length < 1e-12)
      return std::nullopt;
    const ViewportVector direction{ray.direction.x / length, ray.direction.y / length,
                                   ray.direction.z / length};
    const auto facing = dot(direction, normal);
    if (std::abs(facing) < 1e-6)
      return std::nullopt;
    const auto distance =
        dot({origin.x - ray.origin.x, origin.y - ray.origin.y, origin.z - ray.origin.z}, normal) /
        facing;
    if (!std::isfinite(distance) || distance < 0 || distance > 500)
      return std::nullopt;
    return ViewportVector{ray.origin.x + direction.x * distance,
                          ray.origin.y + direction.y * distance,
                          ray.origin.z + direction.z * distance};
  };
  const auto from = hit(start), to = hit(current);
  if (!from || !to)
    return std::nullopt;
  const ViewportVector movement{to->x - from->x, to->y - from->y, to->z - from->z};
  const auto first = SnapToStep(dot(movement, plane[0]), snap_step);
  const auto second = SnapToStep(dot(movement, plane[1]), snap_step);
  const std::array result{plane[0].x * first + plane[1].x * second,
                          plane[0].y * first + plane[1].y * second,
                          plane[0].z * first + plane[1].z * second};
  for (const auto value : result)
    if (!std::isfinite(value) || std::abs(value) > 100000)
      return std::nullopt;
  return result;
}
} // namespace nexora::editor::preview
