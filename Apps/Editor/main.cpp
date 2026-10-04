#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/AssetImport.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/ProjectContent.h"
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/SceneAuthoring.h"
#include "Nexora/Editor/ViewportMath.h"
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Math/Math.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Runtime/EditorSdk.h"
#endif

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <numbers>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {
struct ProjectState final {
  nexora::editor::ProjectWorkspace workspace;
  nexora::editor::AssetWorkspace assets;
};

bool OpenProjectWorkspace(const std::filesystem::path &root, nexora::editor::ProjectAccess access,
                          bool create, std::string name, ProjectState &destination,
                          std::string *error) {
  ProjectState candidate;
  if (create) {
    if (!candidate.workspace.Create(root, std::move(name), error))
      return false;
  } else if (!candidate.workspace.Open(root, access, error)) {
    return false;
  }
  destination = std::move(candidate);
  return true;
}

bool LoadProject(const std::filesystem::path &root, nexora::editor::ProjectAccess access,
                 bool create, std::string name, ProjectState &destination, std::string *error) {
  ProjectState candidate;
  if (!OpenProjectWorkspace(root, access, create, std::move(name), candidate, error))
    return false;
  if (!candidate.assets.ImportTree(candidate.workspace.Root() / "Content", {}, {},
                                   candidate.workspace.Writable()
                                       ? nexora::editor::AssetIdentityMode::PersistentReadWrite
                                       : nexora::editor::AssetIdentityMode::PersistentReadOnly,
                                   error))
    return false;
  destination = std::move(candidate);
  return true;
}

#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
struct NativeSceneProxyMesh final {
  std::array<Nexora::Presentation::SceneVertex, 24> vertices{};
  std::array<std::uint16_t, 36> indices{};

  NativeSceneProxyMesh() {
    constexpr std::array<std::array<float, 3>, 8> corners{{{-1, -1, 1},
                                                           {1, -1, 1},
                                                           {1, 1, 1},
                                                           {-1, 1, 1},
                                                           {-1, -1, -1},
                                                           {1, -1, -1},
                                                           {1, 1, -1},
                                                           {-1, 1, -1}}};
    constexpr std::array<std::array<std::uint16_t, 4>, 6> faces{
        {{0, 1, 2, 3}, {1, 5, 6, 2}, {5, 4, 7, 6}, {4, 0, 3, 7}, {3, 2, 6, 7}, {4, 5, 1, 0}}};
    constexpr std::array<std::array<float, 3>, 6> normals{
        {{0, 0, 1}, {1, 0, 0}, {0, 0, -1}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}}};
    for (std::size_t face = 0; face < faces.size(); ++face) {
      const auto base = face * 4;
      for (std::size_t corner = 0; corner < 4; ++corner) {
        const auto &point = corners[faces[face][corner]];
        const auto &normal = normals[face];
        vertices[base + corner] = {
            {point[0], point[1], point[2]}, {normal[0], normal[1], normal[2]}, {0, 0}};
      }
      for (std::size_t index = 0; index < 6; ++index)
        indices[face * 6 + index] = static_cast<std::uint16_t>(
            base + std::array<std::uint16_t, 6>{0, 1, 2, 2, 3, 0}[index]);
    }
  }
};

std::vector<nexora::editor::PickCandidate>
NativeSceneProxyCandidates(const nexora::editor::SceneDocument &scene) {
  std::vector<nexora::editor::PickCandidate> candidates;
  const auto nodes = scene.Nodes();
  candidates.reserve(std::min<std::size_t>(nodes.size(), 4092));
  for (const auto &node : nodes) {
    if (candidates.size() == 4092)
      break;
    const auto pose = scene.WorldTransform(node.id);
    if (!pose || !nexora::runtime::IsValidTransform(*pose) || std::abs(pose->x) > 100000.0 ||
        std::abs(pose->y) > 100000.0 || std::abs(pose->z) > 100000.0 ||
        std::abs(pose->sx) < 0.0001 || std::abs(pose->sy) < 0.0001 || std::abs(pose->sz) < 0.0001 ||
        std::abs(pose->sx) > 100000.0 || std::abs(pose->sy) > 100000.0 ||
        std::abs(pose->sz) > 100000.0)
      continue;
    const auto matrix = nexora::runtime::ToMatrix(*pose);
    nexora::editor::PickCandidate candidate;
    candidate.entity = node.id;
    candidate.min = {std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity(),
                     std::numeric_limits<double>::infinity()};
    candidate.max = {-candidate.min.x, -candidate.min.y, -candidate.min.z};
    for (const double x : {-0.45, 0.45})
      for (const double y : {-0.45, 0.45})
        for (const double z : {-0.45, 0.45}) {
          const double px = matrix[0] * x + matrix[4] * y + matrix[8] * z + matrix[12];
          const double py = matrix[1] * x + matrix[5] * y + matrix[9] * z + matrix[13] + 0.5;
          const double pz = matrix[2] * x + matrix[6] * y + matrix[10] * z + matrix[14];
          candidate.min.x = std::min(candidate.min.x, px);
          candidate.min.y = std::min(candidate.min.y, py);
          candidate.min.z = std::min(candidate.min.z, pz);
          candidate.max.x = std::max(candidate.max.x, px);
          candidate.max.y = std::max(candidate.max.y, py);
          candidate.max.z = std::max(candidate.max.z, pz);
        }
    if (!std::isfinite(candidate.min.x) || !std::isfinite(candidate.min.y) ||
        !std::isfinite(candidate.min.z) || !std::isfinite(candidate.max.x) ||
        !std::isfinite(candidate.max.y) || !std::isfinite(candidate.max.z))
      continue;
    candidates.push_back(candidate);
  }
  return candidates;
}

struct NativeSceneAxisHandle final {
  nexora::editor::ViewportVector axis{};
  nexora::editor::PickCandidate bounds{};
  Nexora::Presentation::SceneInstance instance{};
};

std::vector<NativeSceneAxisHandle>
NativeSceneAxisHandles(const nexora::editor::SceneDocument &scene,
                       std::span<const nexora::editor::PickCandidate> candidates, bool local_axes) {
  if (scene.Selection().empty())
    return {};
  const auto selected = scene.Selection().front();
  const auto found =
      std::find_if(candidates.begin(), candidates.end(),
                   [selected](const auto &candidate) { return candidate.entity == selected; });
  if (found == candidates.end())
    return {};
  const auto pose = scene.WorldTransform(selected);
  if (!pose)
    return {};
  const std::array<double, 3> pivot{pose->x, pose->y + 0.5, pose->z};
  const auto axes = nexora::editor::GizmoAxes(
      *pose, local_axes ? nexora::editor::GizmoSpace::Local : nexora::editor::GizmoSpace::World);
  const auto component = [](nexora::editor::ViewportVector vector, std::size_t coordinate) {
    return coordinate == 0 ? vector.x : coordinate == 1 ? vector.y : vector.z;
  };
  std::vector<NativeSceneAxisHandle> handles;
  handles.reserve(3);
  for (std::size_t axis = 0; axis < 3; ++axis) {
    NativeSceneAxisHandle handle;
    handle.axis = axes[axis];
    handle.bounds.entity = axis + 1;
    std::array<double, 3> minimum{}, maximum{};
    handle.instance.color[0] = handle.instance.color[1] = handle.instance.color[2] = 0.12F;
    for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
      const double position = pivot[coordinate] + 0.95 * component(axes[axis], coordinate);
      const double radius = std::abs(component(axes[0], coordinate)) * (axis == 0 ? 0.42 : 0.05) +
                            std::abs(component(axes[1], coordinate)) * (axis == 1 ? 0.42 : 0.05) +
                            std::abs(component(axes[2], coordinate)) * (axis == 2 ? 0.42 : 0.05);
      minimum[coordinate] = position - radius;
      maximum[coordinate] = position + radius;
      handle.instance.translation[coordinate] = static_cast<float>(position);
      handle.instance.scale[coordinate] = coordinate == axis ? 0.42F : 0.05F;
    }
    handle.instance.color[axis] = 0.95F;
    if (local_axes) {
      handle.instance.rotation[0] = static_cast<float>(pose->qx);
      handle.instance.rotation[1] = static_cast<float>(pose->qy);
      handle.instance.rotation[2] = static_cast<float>(pose->qz);
      handle.instance.rotation[3] = static_cast<float>(pose->qw);
    }
    handle.bounds.min = {minimum[0], minimum[1], minimum[2]};
    handle.bounds.max = {maximum[0], maximum[1], maximum[2]};
    handles.push_back(handle);
  }
  return handles;
}

std::vector<NativeSceneAxisHandle>
NativeSceneRotationHandles(const nexora::editor::SceneDocument &scene,
                           std::span<const nexora::editor::PickCandidate> candidates,
                           bool local_axes) {
  if (scene.Selection().empty())
    return {};
  const auto selected = scene.Selection().front();
  if (std::none_of(candidates.begin(), candidates.end(),
                   [selected](const auto &candidate) { return candidate.entity == selected; }))
    return {};
  const auto pose = scene.WorldTransform(selected);
  if (!pose)
    return {};
  const auto axes = nexora::editor::GizmoAxes(
      *pose, local_axes ? nexora::editor::GizmoSpace::Local : nexora::editor::GizmoSpace::World);
  const nexora::editor::ViewportVector pivot{pose->x, pose->y + 0.5, pose->z};
  std::vector<NativeSceneAxisHandle> handles;
  constexpr std::size_t segments = 24;
  handles.reserve(3 * segments);
  for (std::size_t axis = 0; axis < 3; ++axis) {
    const auto u = axes[(axis + 1) % 3];
    const auto v = axes[(axis + 2) % 3];
    for (std::size_t segment = 0; segment < segments; ++segment) {
      const double angle = 2.0 * std::numbers::pi * segment / segments;
      NativeSceneAxisHandle handle;
      handle.axis = axes[axis];
      handle.bounds.entity = handles.size() + 1;
      const double coordinates[3]{pivot.x + 1.15 * (std::cos(angle) * u.x + std::sin(angle) * v.x),
                                  pivot.y + 1.15 * (std::cos(angle) * u.y + std::sin(angle) * v.y),
                                  pivot.z + 1.15 * (std::cos(angle) * u.z + std::sin(angle) * v.z)};
      for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
        handle.instance.translation[coordinate] = static_cast<float>(coordinates[coordinate]);
        handle.instance.scale[coordinate] = 0.08F;
      }
      handle.instance.color[0] = handle.instance.color[1] = handle.instance.color[2] = 0.12F;
      handle.instance.color[axis] = 0.95F;
      handle.bounds.min = {coordinates[0] - 0.08, coordinates[1] - 0.08, coordinates[2] - 0.08};
      handle.bounds.max = {coordinates[0] + 0.08, coordinates[1] + 0.08, coordinates[2] + 0.08};
      handles.push_back(handle);
    }
  }
  return handles;
}

std::vector<NativeSceneAxisHandle>
NativeSceneScaleHandles(const nexora::editor::SceneDocument &scene,
                        std::span<const nexora::editor::PickCandidate> candidates,
                        nexora::editor::imgui::NativeSceneOrbit orbit) {
  auto handles = NativeSceneAxisHandles(scene, candidates, true);
  for (auto &handle : handles) {
    for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
      handle.instance.scale[coordinate] = 0.16F;
      // A local cube may be rotated around any axis; sqrt(3) * 0.16 is conservative.
      const double center = handle.instance.translation[coordinate];
      const double radius = std::sqrt(3.0) * 0.16;
      const double low = center - radius, high = center + radius;
      if (coordinate == 0) {
        handle.bounds.min.x = low;
        handle.bounds.max.x = high;
      } else if (coordinate == 1) {
        handle.bounds.min.y = low;
        handle.bounds.max.y = high;
      } else {
        handle.bounds.min.z = low;
        handle.bounds.max.z = high;
      }
    }
  }
  if (handles.size() == 3) {
    const auto pose = scene.WorldTransform(scene.Selection().front());
    if (pose) {
      NativeSceneAxisHandle uniform;
      uniform.bounds.entity = 4;
      const double direction[3]{std::sin(orbit.yaw) * std::cos(orbit.pitch), std::sin(orbit.pitch),
                                std::cos(orbit.yaw) * std::cos(orbit.pitch)};
      const double pivot[3]{pose->x, pose->y + 0.5, pose->z};
      for (std::size_t coordinate = 0; coordinate < 3; ++coordinate) {
        uniform.instance.translation[coordinate] =
            static_cast<float>(pivot[coordinate] + 0.7 * direction[coordinate]);
        uniform.instance.scale[coordinate] = 0.13F;
      }
      uniform.instance.color[0] = 0.95F;
      uniform.instance.color[1] = 0.95F;
      uniform.instance.color[2] = 0.95F;
      const auto &center = uniform.instance.translation;
      uniform.bounds.min = {center[0] - 0.13, center[1] - 0.13, center[2] - 0.13};
      uniform.bounds.max = {center[0] + 0.13, center[1] + 0.13, center[2] + 0.13};
      handles.push_back(uniform);
    }
  }
  return handles;
}

nexora::editor::ViewportCamera NativeSceneCamera(nexora::editor::imgui::SceneOverviewCamera camera,
                                                 nexora::editor::imgui::NativeSceneOrbit orbit) {
  nexora::editor::ViewportCamera view;
  view.target = {std::clamp(camera.x, -100000.0, 100000.0), orbit.target_y,
                 std::clamp(camera.z, -100000.0, 100000.0)};
  view.position = {view.target.x + orbit.distance * std::sin(orbit.yaw) * std::cos(orbit.pitch),
                   view.target.y + orbit.distance * std::sin(orbit.pitch),
                   view.target.z + orbit.distance * std::cos(orbit.yaw) * std::cos(orbit.pitch)};
  view.vertical_fov_degrees = 0.85 * 180.0 / std::numbers::pi;
  return view;
}

struct NativeScenePickHit final {
  std::optional<nexora::runtime::Id> entity;
  std::optional<nexora::editor::ViewportVector> axis;
  std::optional<std::size_t> scale_axis;
};

NativeScenePickHit PickNativeSceneProxy(const nexora::editor::SceneDocument &scene,
                                        Nexora::Presentation::SceneViewport viewport,
                                        nexora::editor::imgui::SceneOverviewCamera camera,
                                        nexora::editor::imgui::NativeSceneOrbit orbit,
                                        nexora::editor::imgui::NativeScenePickRequest request,
                                        bool local_axes,
                                        nexora::editor::imgui::NativeSceneTool tool) {
  if (request.x < viewport.x || request.y < viewport.y ||
      request.x >= viewport.x + viewport.width || request.y >= viewport.y + viewport.height)
    return {};
  const auto view = NativeSceneCamera(camera, orbit);
  const auto ray =
      nexora::editor::ViewportPickRay(view, viewport.width, viewport.height,
                                      request.x - viewport.x + 0.5, request.y - viewport.y + 0.5);
  if (!ray)
    return {};
  const auto candidates = NativeSceneProxyCandidates(scene);
  std::optional<nexora::editor::PickHit> proxy;
  for (const auto &candidate : candidates) {
    if (!nexora::editor::PickNearest(*ray, std::span(&candidate, 1), 500.0))
      continue;
    const auto pose = scene.WorldTransform(candidate.entity);
    if (!pose)
      continue;
    const auto distance = nexora::editor::PickOrientedBox(
        *ray, {pose->x, pose->y + 0.5, pose->z},
        {std::abs(pose->sx) * 0.45, std::abs(pose->sy) * 0.45, std::abs(pose->sz) * 0.45},
        {pose->qx, pose->qy, pose->qz, pose->qw}, 500.0);
    if (distance && (!proxy || *distance < proxy->distance ||
                     (*distance == proxy->distance && candidate.entity < proxy->entity)))
      proxy = nexora::editor::PickHit{candidate.entity, *distance};
  }
  const auto handles = tool == nexora::editor::imgui::NativeSceneTool::Rotate
                           ? NativeSceneRotationHandles(scene, candidates, local_axes)
                       : tool == nexora::editor::imgui::NativeSceneTool::Scale
                           ? NativeSceneScaleHandles(scene, candidates, orbit)
                           : NativeSceneAxisHandles(scene, candidates, local_axes);
  std::optional<nexora::editor::PickHit> exact_gizmo;
  std::optional<nexora::editor::PickHit> padded_gizmo;
  for (const auto &handle : handles) {
    // About two screen pixels at the default camera distance. Thin visible handles need a
    // slightly larger hit volume because a rasterized edge pixel can fall outside the box ray.
    constexpr double pick_padding = 0.10;
    const auto &instance = handle.instance;
    const nexora::editor::ViewportVector center{instance.translation[0], instance.translation[1],
                                                instance.translation[2]};
    const std::array<double, 4> rotation{instance.rotation[0], instance.rotation[1],
                                         instance.rotation[2], instance.rotation[3]};
    const nexora::editor::ViewportVector half_extents{
        std::abs(instance.scale[0]), std::abs(instance.scale[1]), std::abs(instance.scale[2])};
    if (nexora::editor::PickNearest(*ray, std::span(&handle.bounds, 1), 500.0)) {
      const auto distance =
          nexora::editor::PickOrientedBox(*ray, center, half_extents, rotation, 500.0);
      if (distance &&
          (!exact_gizmo || *distance < exact_gizmo->distance ||
           (*distance == exact_gizmo->distance && handle.bounds.entity < exact_gizmo->entity)))
        exact_gizmo = nexora::editor::PickHit{handle.bounds.entity, *distance};
    }
    auto hit_bounds = handle.bounds;
    hit_bounds.min.x -= pick_padding;
    hit_bounds.min.y -= pick_padding;
    hit_bounds.min.z -= pick_padding;
    hit_bounds.max.x += pick_padding;
    hit_bounds.max.y += pick_padding;
    hit_bounds.max.z += pick_padding;
    if (!nexora::editor::PickNearest(*ray, std::span(&hit_bounds, 1), 500.0))
      continue;
    const auto distance = nexora::editor::PickOrientedBox(*ray, center,
                                                          {half_extents.x + pick_padding,
                                                           half_extents.y + pick_padding,
                                                           half_extents.z + pick_padding},
                                                          rotation, 500.0);
    if (distance &&
        (!padded_gizmo || *distance < padded_gizmo->distance ||
         (*distance == padded_gizmo->distance && handle.bounds.entity < padded_gizmo->entity)))
      padded_gizmo = nexora::editor::PickHit{handle.bounds.entity, *distance};
  }
  const auto gizmo = exact_gizmo ? exact_gizmo : padded_gizmo;
  if (gizmo && (!proxy || gizmo->distance < proxy->distance))
    return {.entity = std::nullopt,
            .axis = handles[gizmo->entity - 1].axis,
            .scale_axis = tool == nexora::editor::imgui::NativeSceneTool::Scale
                              ? std::optional<std::size_t>{gizmo->entity - 1}
                              : std::nullopt};
  return {.entity = proxy ? std::optional{proxy->entity} : std::nullopt,
          .axis = std::nullopt,
          .scale_axis = std::nullopt};
}

std::optional<std::array<double, 3>> NativeSceneDragDelta(
    Nexora::Presentation::SceneViewport viewport, nexora::editor::imgui::SceneOverviewCamera camera,
    nexora::editor::imgui::NativeSceneOrbit orbit,
    nexora::editor::imgui::NativeSceneDragRequest request, const nexora::runtime::Transform &pose,
    std::optional<nexora::editor::ViewportVector> handle_axis = std::nullopt) {
  if (request.start_x < static_cast<std::int64_t>(viewport.x) ||
      request.start_y < static_cast<std::int64_t>(viewport.y) ||
      request.start_x >= static_cast<std::int64_t>(viewport.x) + viewport.width ||
      request.start_y >= static_cast<std::int64_t>(viewport.y) + viewport.height)
    return std::nullopt;
  const auto view = NativeSceneCamera(camera, orbit);
  const auto pick_ray = [&](std::int32_t x, std::int32_t y) {
    const auto pixel_x = std::clamp(static_cast<double>(x) - viewport.x + 0.5, 0.0,
                                    static_cast<double>(viewport.width));
    const auto pixel_y = std::clamp(static_cast<double>(y) - viewport.y + 0.5, 0.0,
                                    static_cast<double>(viewport.height));
    return nexora::editor::ViewportPickRay(view, viewport.width, viewport.height, pixel_x, pixel_y);
  };
  if (handle_axis || request.vertical) {
    const auto start_ray = pick_ray(request.start_x, request.start_y);
    const auto end_ray = pick_ray(request.end_x, request.end_y);
    if (!start_ray || !end_ray)
      return std::nullopt;
    const nexora::editor::ViewportVector axis_origin{pose.x, pose.y + 0.5, pose.z};
    const auto axis = handle_axis.value_or(nexora::editor::ViewportVector{0.0, 1.0, 0.0});
    const auto start = nexora::editor::AxisDragDistance(*start_ray, axis_origin, axis);
    const auto end = nexora::editor::AxisDragDistance(*end_ray, axis_origin, axis);
    if (!start || !end)
      return std::nullopt;
    const auto delta = nexora::editor::SnapToStep(*end - *start, request.snap_step);
    if (!std::isfinite(delta) || std::abs(delta) > 100000.0)
      return std::nullopt;
    return std::array{axis.x * delta, axis.y * delta, axis.z * delta};
  }
  const auto point_on_plane = [&](std::int32_t x,
                                  std::int32_t y) -> std::optional<nexora::editor::ViewportVector> {
    const auto ray = pick_ray(x, y);
    if (!ray || std::abs(ray->direction.y) < 1e-6)
      return std::nullopt;
    const auto distance = (pose.y - ray->origin.y) / ray->direction.y;
    if (!std::isfinite(distance) || distance < 0.0 || distance > 500.0)
      return std::nullopt;
    return nexora::editor::ViewportVector{ray->origin.x + ray->direction.x * distance, pose.y,
                                          ray->origin.z + ray->direction.z * distance};
  };
  const auto start = point_on_plane(request.start_x, request.start_y);
  const auto end = point_on_plane(request.end_x, request.end_y);
  if (!start || !end)
    return std::nullopt;
  const auto dx = nexora::editor::SnapToStep(end->x - start->x, request.snap_step);
  const auto dz = nexora::editor::SnapToStep(end->z - start->z, request.snap_step);
  if (!std::isfinite(dx) || !std::isfinite(dz) || std::abs(dx) > 100000.0 ||
      std::abs(dz) > 100000.0)
    return std::nullopt;
  return std::array{dx, 0.0, dz};
}

std::optional<double> NativeSceneDragAngle(Nexora::Presentation::SceneViewport viewport,
                                           nexora::editor::imgui::SceneOverviewCamera camera,
                                           nexora::editor::imgui::NativeSceneOrbit orbit,
                                           nexora::editor::imgui::NativeSceneDragRequest request,
                                           const nexora::runtime::Transform &pose,
                                           nexora::editor::ViewportVector axis) {
  if (request.start_x < static_cast<std::int64_t>(viewport.x) ||
      request.start_y < static_cast<std::int64_t>(viewport.y) ||
      request.start_x >= static_cast<std::int64_t>(viewport.x) + viewport.width ||
      request.start_y >= static_cast<std::int64_t>(viewport.y) + viewport.height)
    return std::nullopt;
  const auto view = NativeSceneCamera(camera, orbit);
  const auto ray = [&](std::int32_t x, std::int32_t y) {
    return nexora::editor::ViewportPickRay(view, viewport.width, viewport.height,
                                           std::clamp(static_cast<double>(x) - viewport.x + 0.5,
                                                      0.0, static_cast<double>(viewport.width)),
                                           std::clamp(static_cast<double>(y) - viewport.y + 0.5,
                                                      0.0, static_cast<double>(viewport.height)));
  };
  const auto start = ray(request.start_x, request.start_y);
  const auto end = ray(request.end_x, request.end_y);
  if (!start || !end)
    return std::nullopt;
  return nexora::editor::RotationDragAngle(*start, *end, {pose.x, pose.y + 0.5, pose.z}, axis);
}

std::optional<double> NativeSceneDragScaleFactor(
    Nexora::Presentation::SceneViewport viewport, nexora::editor::imgui::SceneOverviewCamera camera,
    nexora::editor::imgui::NativeSceneOrbit orbit,
    nexora::editor::imgui::NativeSceneDragRequest request, const nexora::runtime::Transform &pose,
    nexora::editor::ViewportVector axis) {
  if (request.start_x < static_cast<std::int64_t>(viewport.x) ||
      request.start_y < static_cast<std::int64_t>(viewport.y) ||
      request.start_x >= static_cast<std::int64_t>(viewport.x) + viewport.width ||
      request.start_y >= static_cast<std::int64_t>(viewport.y) + viewport.height)
    return std::nullopt;
  const auto view = NativeSceneCamera(camera, orbit);
  const auto ray = [&](std::int32_t x, std::int32_t y) {
    return nexora::editor::ViewportPickRay(view, viewport.width, viewport.height,
                                           std::clamp(static_cast<double>(x) - viewport.x + 0.5,
                                                      0.0, static_cast<double>(viewport.width)),
                                           std::clamp(static_cast<double>(y) - viewport.y + 0.5,
                                                      0.0, static_cast<double>(viewport.height)));
  };
  const auto start_ray = ray(request.start_x, request.start_y);
  const auto end_ray = ray(request.end_x, request.end_y);
  if (!start_ray || !end_ray)
    return std::nullopt;
  const nexora::editor::ViewportVector pivot{pose.x, pose.y + 0.5, pose.z};
  const auto start = nexora::editor::AxisDragDistance(*start_ray, pivot, axis);
  const auto end = nexora::editor::AxisDragDistance(*end_ray, pivot, axis);
  return start && end ? nexora::editor::ScaleDragFactor(*start, *end) : std::nullopt;
}

std::optional<double>
NativeSceneDragUniformScaleFactor(Nexora::Presentation::SceneViewport viewport,
                                  nexora::editor::imgui::NativeSceneDragRequest request) {
  if (request.start_x < static_cast<std::int64_t>(viewport.x) ||
      request.start_y < static_cast<std::int64_t>(viewport.y) ||
      request.start_x >= static_cast<std::int64_t>(viewport.x) + viewport.width ||
      request.start_y >= static_cast<std::int64_t>(viewport.y) + viewport.height)
    return std::nullopt;
  return std::exp(
      std::clamp((static_cast<double>(request.start_y) - request.end_y) / 100.0, -4.0, 4.0));
}

double NativeSceneScaleFactorWithSnap(double factor, bool snap) {
  return snap ? std::max(nexora::editor::kMinGizmoScaleFactor,
                         1.0 + nexora::editor::SnapToStep(factor - 1.0, 0.25))
              : factor;
}

double NativeSceneRotationAngleWithSnap(double angle, bool snap) {
  return snap ? nexora::editor::SnapToStep(angle, std::numbers::pi / 12.0) : angle;
}

Nexora::Presentation::SurfaceStatus DrawNativeScenePreview(
    Nexora::Presentation::RenderSurface &surface, const nexora::editor::SceneDocument &scene,
    Nexora::Presentation::SceneViewport viewport, nexora::editor::imgui::SceneOverviewCamera camera,
    nexora::editor::imgui::NativeSceneOrbit orbit, bool local_axes,
    nexora::editor::imgui::NativeSceneTool tool,
    std::optional<std::array<double, 3>> drag_preview = std::nullopt,
    std::optional<std::pair<nexora::editor::ViewportVector, double>> rotation_preview =
        std::nullopt,
    std::optional<std::pair<std::size_t, double>> scale_preview = std::nullopt) {
  static const NativeSceneProxyMesh mesh;
  const auto candidates = NativeSceneProxyCandidates(scene);
  const auto handles = tool == nexora::editor::imgui::NativeSceneTool::Rotate
                           ? NativeSceneRotationHandles(scene, candidates, local_axes)
                       : tool == nexora::editor::imgui::NativeSceneTool::Scale
                           ? NativeSceneScaleHandles(scene, candidates, orbit)
                           : NativeSceneAxisHandles(scene, candidates, local_axes);
  const std::unordered_set<nexora::runtime::Id> selected(scene.Selection().begin(),
                                                         scene.Selection().end());
  std::unordered_map<nexora::runtime::Id, nexora::runtime::Id> root_cache;
  const auto selected_root = [&](nexora::runtime::Id entity) {
    std::vector<nexora::runtime::Id> chain;
    nexora::runtime::Id root = 0;
    while (entity != 0 && chain.size() <= candidates.size()) {
      if (const auto cached = root_cache.find(entity); cached != root_cache.end()) {
        if (cached->second != 0)
          root = cached->second;
        break;
      }
      chain.push_back(entity);
      if (selected.contains(entity))
        root = entity;
      const auto parent = scene.Parent(entity);
      entity = parent.value_or(0);
    }
    for (const auto id : chain)
      root_cache[id] = root;
    return root;
  };
  const auto rotate = [](nexora::editor::ViewportVector point, nexora::editor::ViewportVector axis,
                         double angle) {
    const double cosine = std::cos(angle), sine = std::sin(angle);
    const double dot = axis.x * point.x + axis.y * point.y + axis.z * point.z;
    return nexora::editor::ViewportVector{
        point.x * cosine + (axis.y * point.z - axis.z * point.y) * sine +
            axis.x * dot * (1 - cosine),
        point.y * cosine + (axis.z * point.x - axis.x * point.z) * sine +
            axis.y * dot * (1 - cosine),
        point.z * cosine + (axis.x * point.y - axis.y * point.x) * sine +
            axis.z * dot * (1 - cosine)};
  };
  camera.x = std::clamp(camera.x, -100000.0, 100000.0);
  camera.z = std::clamp(camera.z, -100000.0, 100000.0);
  std::vector<Nexora::Presentation::SceneInstance> instances;
  instances.reserve(candidates.size() + handles.size() + 1);
  Nexora::Presentation::SceneInstance ground{};
  ground.translation[0] = static_cast<float>(camera.x);
  ground.translation[1] = -0.35F;
  ground.translation[2] = static_cast<float>(camera.z);
  ground.scale[0] = ground.scale[2] = 12.0F;
  ground.scale[1] = 0.1F;
  ground.color[0] = 0.24F;
  ground.color[1] = 0.28F;
  ground.color[2] = 0.34F;
  instances.push_back(ground);
  std::unordered_map<nexora::runtime::Id, std::optional<nexora::runtime::Transform>> root_poses;
  for (const auto &candidate : candidates) {
    const auto pose = scene.WorldTransform(candidate.entity);
    if (!pose)
      continue;
    Nexora::Presentation::SceneInstance instance{};
    const auto root = selected_root(candidate.entity);
    const bool preview_moved = drag_preview && root != 0;
    instance.translation[0] = static_cast<float>((candidate.min.x + candidate.max.x) * 0.5 +
                                                 (preview_moved ? (*drag_preview)[0] : 0.0));
    instance.translation[1] = static_cast<float>((candidate.min.y + candidate.max.y) * 0.5 +
                                                 (preview_moved ? (*drag_preview)[1] : 0.0));
    instance.translation[2] = static_cast<float>((candidate.min.z + candidate.max.z) * 0.5 +
                                                 (preview_moved ? (*drag_preview)[2] : 0.0));
    instance.scale[0] = static_cast<float>(pose->sx * 0.45);
    instance.scale[1] = static_cast<float>(pose->sy * 0.45);
    instance.scale[2] = static_cast<float>(pose->sz * 0.45);
    instance.rotation[0] = static_cast<float>(pose->qx);
    instance.rotation[1] = static_cast<float>(pose->qy);
    instance.rotation[2] = static_cast<float>(pose->qz);
    instance.rotation[3] = static_cast<float>(pose->qw);
    if (rotation_preview && root != 0) {
      const auto [found, inserted] = root_poses.try_emplace(root);
      if (inserted)
        found->second = scene.WorldTransform(root);
      if (const auto &root_pose = found->second) {
        const nexora::editor::ViewportVector pivot{root_pose->x, root_pose->y + 0.5, root_pose->z};
        const nexora::editor::ViewportVector center{
            instance.translation[0], instance.translation[1], instance.translation[2]};
        const auto rotated = rotate({center.x - pivot.x, center.y - pivot.y, center.z - pivot.z},
                                    rotation_preview->first, rotation_preview->second);
        instance.translation[0] = static_cast<float>(pivot.x + rotated.x);
        instance.translation[1] = static_cast<float>(pivot.y + rotated.y);
        instance.translation[2] = static_cast<float>(pivot.z + rotated.z);
        const auto axis = rotation_preview->first;
        const double sine = std::sin(rotation_preview->second * 0.5);
        const double dx = axis.x * sine, dy = axis.y * sine, dz = axis.z * sine;
        const double dw = std::cos(rotation_preview->second * 0.5);
        instance.rotation[0] =
            static_cast<float>(dw * pose->qx + dx * pose->qw + dy * pose->qz - dz * pose->qy);
        instance.rotation[1] =
            static_cast<float>(dw * pose->qy - dx * pose->qz + dy * pose->qw + dz * pose->qx);
        instance.rotation[2] =
            static_cast<float>(dw * pose->qz + dx * pose->qy - dy * pose->qx + dz * pose->qw);
        instance.rotation[3] =
            static_cast<float>(dw * pose->qw - dx * pose->qx - dy * pose->qy - dz * pose->qz);
      }
    }
    if (scale_preview && root != 0) {
      const auto [found, inserted] = root_poses.try_emplace(root);
      if (inserted)
        found->second = scene.WorldTransform(root);
      if (const auto &root_pose = found->second) {
        const double factor = scale_preview->second;
        const nexora::editor::ViewportVector pivot{root_pose->x, root_pose->y + 0.5, root_pose->z};
        if (scale_preview->first == 3) {
          instance.translation[0] =
              static_cast<float>(pivot.x + (instance.translation[0] - pivot.x) * factor);
          instance.translation[1] =
              static_cast<float>(pivot.y + (instance.translation[1] - pivot.y) * factor);
          instance.translation[2] =
              static_cast<float>(pivot.z + (instance.translation[2] - pivot.z) * factor);
          for (auto &component : instance.scale)
            component *= static_cast<float>(factor);
        } else {
          const auto basis =
              nexora::editor::GizmoAxes(*root_pose, nexora::editor::GizmoSpace::Local);
          const auto axis = basis[scale_preview->first];
          const double offset = (instance.translation[0] - pivot.x) * axis.x +
                                (instance.translation[1] - pivot.y) * axis.y +
                                (instance.translation[2] - pivot.z) * axis.z;
          instance.translation[0] += static_cast<float>(axis.x * offset * (factor - 1.0));
          instance.translation[1] += static_cast<float>(axis.y * offset * (factor - 1.0));
          instance.translation[2] += static_cast<float>(axis.z * offset * (factor - 1.0));
          instance.scale[scale_preview->first] *= static_cast<float>(factor);
        }
      }
    }
    const bool is_selected = selected.contains(candidate.entity);
    instance.color[0] = is_selected ? 1.0F : 0.35F;
    instance.color[1] = is_selected ? 0.75F : 0.65F;
    instance.color[2] = is_selected ? 0.2F : 1.0F;
    instances.push_back(instance);
  }
  for (const auto &handle : handles) {
    auto instance = handle.instance;
    if (drag_preview)
      for (std::size_t coordinate = 0; coordinate < 3; ++coordinate)
        instance.translation[coordinate] += static_cast<float>((*drag_preview)[coordinate]);
    instances.push_back(instance);
  }
  const nexora::math::Vector3 target{static_cast<float>(camera.x),
                                     static_cast<float>(orbit.target_y),
                                     static_cast<float>(camera.z)};
  const nexora::math::Vector3 eye{
      target.x + static_cast<float>(orbit.distance * std::sin(orbit.yaw) * std::cos(orbit.pitch)),
      target.y + static_cast<float>(orbit.distance * std::sin(orbit.pitch)),
      target.z + static_cast<float>(orbit.distance * std::cos(orbit.yaw) * std::cos(orbit.pitch))};
  const auto mvp = nexora::math::PerspectiveRadians(
                       0.85F, static_cast<float>(viewport.width) / viewport.height, 0.1F, 500.0F) *
                   nexora::math::LookAt(eye, target);
  Nexora::Presentation::SceneDrawData draw{};
  draw.vertices = mesh.vertices;
  draw.indices = mesh.indices;
  draw.instances = instances;
  draw.viewport = viewport;
  std::memcpy(draw.model_view_projection, mvp.values.data(), sizeof(draw.model_view_projection));
  return surface.DrawScene(draw);
}

int RunGraphical(std::optional<ProjectState> project,
                 nexora::editor::RecentProjectStore &recent_projects,
                 nexora::editor::ProjectAccess selector_access, std::uint32_t frame_limit,
                 bool native_scene_preview) {
  auto created = Nexora::Presentation::CreateRenderSurface(
      {"Nexora Editor", 1280, 720, true, Nexora::Presentation::SurfaceBackend::Automatic});
  if (!created) {
    std::cerr << "graphical shell unavailable: " << created.reason << '\n';
    return 1;
  }
  nexora::editor::imgui::EditorImGuiHost ui;
  ui.SetNativeScenePreview(native_scene_preview);
  nexora::core::JobSystem import_jobs{1};
  import_jobs.Start();
  nexora::editor::AssetImportQueue imports{import_jobs};
  nexora::editor::ProjectContentSession content;
  struct PendingProject final {
    ProjectState candidate;
    nexora::editor::ImportOperationId import{};
    bool created{};
  };
  std::optional<PendingProject> pending_project;
  std::string layout_error;
  const auto load_layout = [&](nexora::editor::ProjectWorkspace &workspace) {
    layout_error.clear();
    if (const auto layout = workspace.LoadEditorLayout(&layout_error);
        layout && !ui.LoadLayout(*layout))
      std::cerr << "ignored invalid editor layout\n";
    else if (!layout_error.empty())
      std::cerr << layout_error << '\n';
  };
  std::uint64_t project_generation = 1;
  if (project) {
    if (!content.Open(project->workspace, project->assets, project_generation,
                      project->workspace.Writable(), &layout_error)) {
      std::cerr << layout_error << '\n';
      return 1;
    }
    load_layout(project->workspace);
  }
  nexora::editor::ProductShell shell;
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Main");
  if (!world.Activate(scene_id)) {
    std::cerr << "failed to activate the editor scene\n";
    return 1;
  }
  nexora::editor::SceneDocument scene(world, scene_id);
  nexora::runtime::PlaySession play(world);
  nexora::runtime::RuntimeConsole console{1024};
  nexora::editor::ProfileSession profile{240};
  std::optional<Nexora::Presentation::SceneViewport> native_scene_viewport_reported;
  std::optional<nexora::editor::ViewportVector> native_scene_drag_axis;
  bool native_scene_drag_rotate = false;
  std::optional<std::size_t> native_scene_drag_scale_axis;
  const auto log = [&](nexora::runtime::RuntimeLogSeverity severity, std::string category,
                       std::string message) {
    const auto timestamp = std::chrono::duration_cast<std::chrono::nanoseconds>(
                               std::chrono::system_clock::now().time_since_epoch())
                               .count();
    static_cast<void>(
        console.Push({0, severity, std::move(category), static_cast<std::uint64_t>(timestamp),
                      "NexoraEditor", std::move(message)}));
  };
  log(nexora::runtime::RuntimeLogSeverity::Info, "Editor", "Graphical session started.");
  const auto scene_path = [](const nexora::editor::ProjectWorkspace &workspace) {
    return workspace.Root() / ".nexora" / "scenes" / "Main.scene";
  };
  const auto overview_path = [&](const nexora::editor::ProjectWorkspace &workspace) {
    auto path = scene_path(workspace);
    path.replace_extension(".overview.camera");
    return path;
  };
  const auto preview_camera_path = [&](const nexora::editor::ProjectWorkspace &workspace) {
    auto path = scene_path(workspace);
    path.replace_extension(".preview.camera");
    return path;
  };
  bool scene_load_failed = false;
  bool overview_load_failed = false;
  bool preview_camera_load_failed = false;
  const auto open_scene = [&] {
    const auto path = scene_path(project->workspace);
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    if (error || (exists && !scene.Reload(path))) {
      scene_load_failed = true;
      ui.SetSceneSaveResult("Scene could not be loaded: " + path.string(), false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene",
          "Scene could not be loaded: " + path.string());
      return;
    }
    scene_load_failed = false;
    if (!exists && scene.Nodes().empty())
      scene.Create("Scene Root");
    log(nexora::runtime::RuntimeLogSeverity::Info, "Scene",
        exists ? "Opened saved scene." : "Created starter scene.");
    static_cast<void>(ui.SetSceneOverviewCamera({}));
    static_cast<void>(ui.SetNativeSceneOrbit({}));
    overview_load_failed = false;
    preview_camera_load_failed = false;
    const auto camera_path = overview_path(project->workspace);
    std::error_code camera_error;
    const bool camera_exists = std::filesystem::exists(camera_path, camera_error);
    if (camera_error) {
      overview_load_failed = true;
      std::cerr << "scene overview camera could not be inspected: " << camera_error.message()
                << '\n';
    } else if (camera_exists) {
      std::string load_error;
      const auto camera = nexora::editor::CameraPersistence::Load(camera_path, &load_error);
      if (!camera || !camera->orthographic ||
          !ui.SetSceneOverviewCamera({camera->transform.x, camera->transform.z,
                                      std::clamp(320.0 / camera->orthographic_size, 4.0, 256.0)})) {
        overview_load_failed = true;
        std::cerr << "ignored invalid scene overview camera: " << camera_path << ' ' << load_error
                  << '\n';
      }
    }
    const auto preview_path = preview_camera_path(project->workspace);
    camera_error.clear();
    const bool preview_exists = std::filesystem::exists(preview_path, camera_error);
    if (camera_error) {
      preview_camera_load_failed = true;
      std::cerr << "scene preview camera could not be inspected: " << camera_error.message()
                << '\n';
    } else if (preview_exists) {
      std::string load_error;
      const auto camera = nexora::editor::CameraPersistence::Load(preview_path, &load_error);
      if (!camera || camera->orthographic ||
          !ui.SetNativeSceneOrbit(
              {camera->yaw, camera->pitch, camera->movement_speed, camera->transform.y})) {
        preview_camera_load_failed = true;
        std::cerr << "ignored invalid scene preview camera: " << preview_path << ' ' << load_error
                  << '\n';
      }
    }
  };
  if (project)
    open_scene();
  std::uint32_t frames = 0;
  int result = 0;
  bool exit_requested = false;
  auto last_play_frame = std::chrono::steady_clock::now();
  double play_accumulator = 0.0;
  auto recovery_choice = nexora::editor::imgui::RecoveryChoice::None;
  std::string selector_result = project ? "bypassed" : "none";
  const auto save_scene = [&] {
    if (scene_load_failed) {
      ui.SetSceneSaveResult("Scene load failed. Resolve the scene file before saving.", false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene", "Save blocked by failed load.");
      return false;
    }
    if (!project || !project->workspace.Writable()) {
      ui.SetSceneSaveResult("Scene is read-only. Reopen the project for writing.", false);
      log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene",
          "Save blocked by read-only access.");
      return false;
    }
    if (!scene.Save(scene_path(project->workspace))) {
      ui.SetSceneSaveResult("Scene could not be saved. Check the project directory.", false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene", "Scene save failed.");
      return false;
    }
    ui.SetSceneSaveResult("Scene saved.", true);
    log(nexora::runtime::RuntimeLogSeverity::Info, "Scene", "Scene saved.");
    return true;
  };
  while (!exit_requested && !created.surface->CloseRequested() &&
         (frame_limit == 0 || frames < frame_limit)) {
    const auto begin_frame_status = created.surface->BeginFrame();
    if (created.surface->CloseRequested()) {
      if (project && scene.Dirty() && created.surface->CancelCloseRequest()) {
        ui.RequestCloseConfirmation();
        continue;
      }
      break;
    }
    const auto action = Nexora::Presentation::RecoveryAction(begin_frame_status);
    if (action == Nexora::Presentation::SurfaceAction::Abort) {
      result = 1;
      break;
    }
    if (action != Nexora::Presentation::SurfaceAction::Render)
      continue;
    ui.ProcessEvents(created.surface->Events());
    const auto &frame = created.surface->FrameInfo();
    if (frame.width == 0 || frame.height == 0)
      continue;
    const auto frame_started = std::chrono::steady_clock::now();
    const auto dpi = std::max(frame.dpiScale, 0.25F);
    ui.SetDisplay(static_cast<float>(frame.width) / dpi, static_cast<float>(frame.height) / dpi,
                  dpi);
    ui.UpdateImeCandidate(*created.surface);
    ui.BeginFrame();
    if (!project && pending_project) {
      if (const auto snapshot = imports.Snapshot(pending_project->import)) {
        std::string status = "Importing project content";
        if (!snapshot->progress.empty()) {
          const auto &progress = snapshot->progress.back();
          if (progress.total != 0)
            status += " (" + std::to_string(progress.completed) + "/" +
                      std::to_string(progress.total) + ")";
        }
        ui.SetProjectSelectorStatus(std::move(status), true);
      }
      if (auto imported = imports.TakeResult(pending_project->import)) {
        if (imported->snapshot.state == nexora::editor::ImportOperationState::Cancelled) {
          ui.SetProjectSelectorError({});
          ui.SetProjectSelectorStatus({}, false);
          pending_project.reset();
        } else if (imported->snapshot.state !=
                       nexora::editor::ImportOperationState::AwaitingPublish ||
                   !imported->workspace) {
          const auto message = imported->snapshot.diagnostics.empty()
                                   ? "Project content import failed."
                                   : imported->snapshot.diagnostics.back().message;
          ui.SetProjectSelectorError(message);
          ui.SetProjectSelectorStatus({}, false);
          pending_project.reset();
        } else {
          pending_project->candidate.assets = std::move(*imported->workspace);
          nexora::editor::ProjectContentSession candidate_content;
          std::string selector_error;
          if (!candidate_content.Open(pending_project->candidate.workspace,
                                      pending_project->candidate.assets, project_generation,
                                      pending_project->candidate.workspace.Writable(),
                                      &selector_error)) {
            ui.SetProjectSelectorError(selector_error.empty() ? "Project content could not open."
                                                              : std::move(selector_error));
            ui.SetProjectSelectorStatus({}, false);
            pending_project.reset();
          } else {
            const bool was_created = pending_project->created;
            project = std::move(pending_project->candidate);
            content = std::move(candidate_content);
            pending_project.reset();
            selector_result = was_created ? "created" : "opened";
            if (!recent_projects.Record(project->workspace, &selector_error))
              std::cerr << "recent-project warning: " << selector_error << '\n';
            load_layout(project->workspace);
            open_scene();
            ui.SetProjectSelectorError({});
            ui.SetProjectSelectorStatus({}, false);
          }
        }
      }
    }
    if (project) {
      ui.DrawProductShell(shell, &scene, &project->workspace, &content, &recent_projects, &imports,
                          &console, &play, &profile);
      switch (ui.TakePlayCommand()) {
      case nexora::editor::imgui::PlayCommand::Start:
        if (play.Start(1.0 / 60.0, [](nexora::runtime::World &, double) { return true; })) {
          play_accumulator = 0.0;
          last_play_frame = std::chrono::steady_clock::now();
          log(nexora::runtime::RuntimeLogSeverity::Info, "PIE", "Isolated Play World started.");
        }
        break;
      case nexora::editor::imgui::PlayCommand::Pause:
        static_cast<void>(play.Pause());
        break;
      case nexora::editor::imgui::PlayCommand::Resume:
        last_play_frame = std::chrono::steady_clock::now();
        static_cast<void>(play.Resume());
        break;
      case nexora::editor::imgui::PlayCommand::Step:
        static_cast<void>(play.Step());
        break;
      case nexora::editor::imgui::PlayCommand::Stop:
        static_cast<void>(play.Stop());
        play_accumulator = 0.0;
        log(nexora::runtime::RuntimeLogSeverity::Info, "PIE", "Play World discarded.");
        break;
      case nexora::editor::imgui::PlayCommand::None:
        break;
      }
      const auto play_now = std::chrono::steady_clock::now();
      const double elapsed =
          std::clamp(std::chrono::duration<double>(play_now - last_play_frame).count(), 0.0, 0.25);
      last_play_frame = play_now;
      if (play.State() == nexora::runtime::PlayState::Playing) {
        play_accumulator += elapsed;
        for (int tick = 0; tick < 4 && play_accumulator >= 1.0 / 60.0; ++tick) {
          if (!play.Tick())
            break;
          play_accumulator -= 1.0 / 60.0;
        }
        play_accumulator = std::min(play_accumulator, 4.0 / 60.0);
      } else {
        play_accumulator = 0.0;
      }
      if (ui.TakeSceneSaveRequest())
        static_cast<void>(save_scene());
      const auto close_choice = ui.TakeCloseChoice();
      if (close_choice == nexora::editor::imgui::CloseChoice::SaveAndExit)
        exit_requested = save_scene();
      else if (close_choice == nexora::editor::imgui::CloseChoice::DiscardAndExit)
        exit_requested = true;
      if (const auto choice = ui.TakeRecoveryChoice();
          choice != nexora::editor::imgui::RecoveryChoice::None)
        recovery_choice = choice;
    } else {
      ui.DrawProjectSelector(&recent_projects, selector_access);
      if (ui.TakeProjectSelectorCancel() && pending_project) {
        static_cast<void>(imports.Cancel(pending_project->import));
        ui.SetProjectSelectorStatus("Cancelling project import", true);
      }
      if (auto request = ui.TakeProjectSelectorRequest()) {
        if (pending_project) {
          ui.SetProjectSelectorError("A project import is already running.");
        } else {
          ProjectState candidate;
          std::string selector_error;
          const bool create_project =
              request->action == nexora::editor::imgui::ProjectSelectorAction::Create;
          if (!OpenProjectWorkspace(request->root, request->access, create_project,
                                    std::move(request->name), candidate, &selector_error)) {
            ui.SetProjectSelectorError(selector_error.empty() ? "Project activation failed."
                                                              : std::move(selector_error));
          } else {
            const auto import =
                imports.Start({project_generation, candidate.workspace.Root() / "Content",
                               candidate.workspace.Writable()
                                   ? nexora::editor::AssetIdentityMode::PersistentReadWrite
                                   : nexora::editor::AssetIdentityMode::PersistentReadOnly},
                              &selector_error);
            if (import == 0) {
              ui.SetProjectSelectorError(selector_error.empty() ? "Project import could not start."
                                                                : std::move(selector_error));
            } else {
              ui.SetProjectSelectorError({});
              ui.SetProjectSelectorStatus("Importing project content", true);
              pending_project = PendingProject{std::move(candidate), import, create_project};
            }
          }
        }
      }
    }
    static_cast<void>(ui.EndFrame());
    // A surface-level loss (the window vanished, the swapchain went out of date) is recoverable and
    // is resolved by the next BeginFrame, which also pumps a pending close request. Anything else,
    // device loss included, is a real failure.
    const auto surface_recoverable = [](Nexora::Presentation::SurfaceStatus status) {
      const auto recovery = Nexora::Presentation::RecoveryAction(status);
      return recovery == Nexora::Presentation::SurfaceAction::RecreateSurface ||
             recovery == Nexora::Presentation::SurfaceAction::Suspend;
    };
    if (const auto render_status = ui.Render(*created.surface, frame.width, frame.height);
        render_status != Nexora::Presentation::SurfaceStatus::Ready) {
      if (surface_recoverable(render_status))
        continue;
      result = created.surface->CloseRequested() ? 0 : 1;
      break;
    }
    if (project) {
      if (const auto viewport = ui.NativeScenePreviewViewport()) {
        if (const auto request = ui.NativeScenePick()) {
          const auto hit = PickNativeSceneProxy(scene, *viewport, ui.GetSceneOverviewCamera(),
                                                ui.GetNativeSceneOrbit(), *request,
                                                ui.NativeSceneLocalAxes(), ui.GetNativeSceneTool());
          native_scene_drag_axis = request->additive ? std::nullopt : hit.axis;
          native_scene_drag_rotate =
              native_scene_drag_axis &&
              ui.GetNativeSceneTool() == nexora::editor::imgui::NativeSceneTool::Rotate;
          native_scene_drag_scale_axis =
              ui.GetNativeSceneTool() == nexora::editor::imgui::NativeSceneTool::Scale
                  ? hit.scale_axis
                  : std::nullopt;
          if (hit.entity) {
            std::vector<nexora::runtime::Id> selection;
            const bool already_selected =
                std::find(scene.Selection().begin(), scene.Selection().end(), *hit.entity) !=
                scene.Selection().end();
            if (request->additive || already_selected)
              selection.assign(scene.Selection().begin(), scene.Selection().end());
            const auto existing = std::find(selection.begin(), selection.end(), *hit.entity);
            if (existing == selection.end())
              selection.push_back(*hit.entity);
            else if (request->additive)
              selection.erase(existing);
            static_cast<void>(scene.Select(selection));
          } else if (!hit.axis && !request->additive) {
            static_cast<void>(scene.Select(std::span<const nexora::runtime::Id>{}));
          }
        }
        if (const auto drag = ui.NativeSceneDrag(); drag && !scene.Selection().empty()) {
          const auto pose = scene.WorldTransform(scene.Selection().front());
          if (pose) {
            std::vector<nexora::editor::SceneDocument::NodeKey> keys;
            for (const auto id : scene.Selection()) {
              if (const auto key = scene.Key(id))
                keys.push_back(*key);
            }
            if (keys.size() == scene.Selection().size()) {
              if (native_scene_drag_scale_axis && native_scene_drag_axis) {
                const auto factor =
                    *native_scene_drag_scale_axis == 3
                        ? NativeSceneDragUniformScaleFactor(*viewport, *drag)
                        : NativeSceneDragScaleFactor(*viewport, ui.GetSceneOverviewCamera(),
                                                     ui.GetNativeSceneOrbit(), *drag, *pose,
                                                     *native_scene_drag_axis);
                if (factor) {
                  const double snapped = NativeSceneScaleFactorWithSnap(*factor, drag->vertical);
                  if (std::abs(snapped - 1.0) > 1e-6) {
                    nexora::editor::GizmoOperation scale;
                    scale.kind = nexora::editor::GizmoOperation::Kind::Scale;
                    if (*native_scene_drag_scale_axis == 0)
                      scale.factors.x = snapped;
                    else if (*native_scene_drag_scale_axis == 1)
                      scale.factors.y = snapped;
                    else if (*native_scene_drag_scale_axis == 2)
                      scale.factors.z = snapped;
                    else
                      scale.factors = {snapped, snapped, snapped};
                    static_cast<void>(scene.ApplySelectionGizmo(keys, scale));
                  }
                }
              } else if (native_scene_drag_rotate && native_scene_drag_axis) {
                const auto angle = NativeSceneDragAngle(*viewport, ui.GetSceneOverviewCamera(),
                                                        ui.GetNativeSceneOrbit(), *drag, *pose,
                                                        *native_scene_drag_axis);
                if (angle) {
                  const double snapped = NativeSceneRotationAngleWithSnap(*angle, drag->vertical);
                  if (std::abs(snapped) > 1e-6) {
                    nexora::editor::GizmoOperation rotation;
                    rotation.kind = nexora::editor::GizmoOperation::Kind::Rotate;
                    rotation.axis = *native_scene_drag_axis;
                    rotation.angle = snapped;
                    static_cast<void>(scene.ApplySelectionGizmo(keys, rotation));
                  }
                }
              } else if (ui.GetNativeSceneTool() == nexora::editor::imgui::NativeSceneTool::Move) {
                const auto delta = NativeSceneDragDelta(*viewport, ui.GetSceneOverviewCamera(),
                                                        ui.GetNativeSceneOrbit(), *drag, *pose,
                                                        native_scene_drag_axis);
                if (delta)
                  static_cast<void>(
                      scene.TranslateSelection(keys, (*delta)[0], (*delta)[1], (*delta)[2]));
              }
            }
          }
          native_scene_drag_axis.reset();
          native_scene_drag_rotate = false;
          native_scene_drag_scale_axis.reset();
        }
        std::optional<std::array<double, 3>> drag_preview;
        std::optional<std::pair<nexora::editor::ViewportVector, double>> rotation_preview;
        std::optional<std::pair<std::size_t, double>> scale_preview;
        if (const auto drag = ui.NativeSceneDragPreview(); drag && !scene.Selection().empty()) {
          const auto pose = scene.WorldTransform(scene.Selection().front());
          if (pose) {
            if (native_scene_drag_scale_axis && native_scene_drag_axis) {
              const auto factor =
                  *native_scene_drag_scale_axis == 3
                      ? NativeSceneDragUniformScaleFactor(*viewport, *drag)
                      : NativeSceneDragScaleFactor(*viewport, ui.GetSceneOverviewCamera(),
                                                   ui.GetNativeSceneOrbit(), *drag, *pose,
                                                   *native_scene_drag_axis);
              if (factor)
                scale_preview = std::pair{*native_scene_drag_scale_axis,
                                          NativeSceneScaleFactorWithSnap(*factor, drag->vertical)};
            } else if (native_scene_drag_rotate && native_scene_drag_axis) {
              if (const auto angle = NativeSceneDragAngle(*viewport, ui.GetSceneOverviewCamera(),
                                                          ui.GetNativeSceneOrbit(), *drag, *pose,
                                                          *native_scene_drag_axis))
                rotation_preview =
                    std::pair{*native_scene_drag_axis,
                              NativeSceneRotationAngleWithSnap(*angle, drag->vertical)};
            } else if (ui.GetNativeSceneTool() == nexora::editor::imgui::NativeSceneTool::Move) {
              drag_preview = NativeSceneDragDelta(*viewport, ui.GetSceneOverviewCamera(),
                                                  ui.GetNativeSceneOrbit(), *drag, *pose,
                                                  native_scene_drag_axis);
            }
          }
        }
        const auto scene_status = DrawNativeScenePreview(
            *created.surface, scene, *viewport, ui.GetSceneOverviewCamera(),
            ui.GetNativeSceneOrbit(), ui.NativeSceneLocalAxes(), ui.GetNativeSceneTool(),
            drag_preview, rotation_preview, scale_preview);
        ui.SetNativeScenePreviewAvailable(scene_status !=
                                          Nexora::Presentation::SurfaceStatus::Unsupported);
        if (scene_status == Nexora::Presentation::SurfaceStatus::Ready &&
            (!native_scene_viewport_reported || native_scene_viewport_reported->x != viewport->x ||
             native_scene_viewport_reported->y != viewport->y ||
             native_scene_viewport_reported->width != viewport->width ||
             native_scene_viewport_reported->height != viewport->height)) {
          std::cerr << "native scene viewport: " << viewport->x << ' ' << viewport->y << ' '
                    << viewport->width << ' ' << viewport->height << '\n';
          native_scene_viewport_reported = *viewport;
        }
        if (scene_status != Nexora::Presentation::SurfaceStatus::Ready &&
            scene_status != Nexora::Presentation::SurfaceStatus::Unsupported) {
          if (surface_recoverable(scene_status))
            continue;
          result = 1;
          break;
        }
      }
    }
    const auto frame_processed = std::chrono::steady_clock::now();
    if (const auto end_status = created.surface->EndFrame();
        end_status != Nexora::Presentation::SurfaceStatus::Ready) {
      if (surface_recoverable(end_status))
        continue;
      result = created.surface->CloseRequested() ? 0 : 1;
      break;
    }
    static_cast<void>(profile.Add(
        {static_cast<std::uint64_t>(frames) + 1,
         std::chrono::duration<double, std::milli>(frame_processed - frame_started).count(), 0.0,
         0}));
    ++frames;
  }
  if (project && project->workspace.Writable() &&
      !project->workspace.SaveEditorLayout(ui.SaveLayout(), &layout_error))
    std::cerr << layout_error << '\n';
  if (project && project->workspace.Writable() && !scene_load_failed &&
      (!overview_load_failed || !preview_camera_load_failed)) {
    const auto camera_path = overview_path(project->workspace);
    std::error_code directory_error;
    std::filesystem::create_directories(camera_path.parent_path(), directory_error);
    if (directory_error) {
      std::cerr << "scene overview camera directory could not be created: "
                << directory_error.message() << '\n';
    } else {
      const auto view = ui.GetSceneOverviewCamera();
      if (!overview_load_failed) {
        nexora::editor::SceneCameraState camera;
        camera.transform.x = view.x;
        camera.transform.z = view.z;
        camera.orthographic = true;
        camera.orthographic_size = 320.0 / view.pixels_per_unit;
        std::string camera_error;
        if (!nexora::editor::CameraPersistence::Save(camera_path, camera, &camera_error))
          std::cerr << "scene overview camera could not be saved: " << camera_error << '\n';
      }
      if (!preview_camera_load_failed) {
        const auto orbit = ui.GetNativeSceneOrbit();
        nexora::editor::SceneCameraState camera;
        camera.transform.x = view.x;
        camera.transform.y = orbit.target_y;
        camera.transform.z = view.z;
        camera.pitch = orbit.pitch;
        camera.yaw = orbit.yaw;
        camera.movement_speed = orbit.distance;
        std::string camera_error;
        if (!nexora::editor::CameraPersistence::Save(preview_camera_path(project->workspace),
                                                     camera, &camera_error))
          std::cerr << "scene preview camera could not be saved: " << camera_error << '\n';
      }
    }
  }
  if (play.State() != nexora::runtime::PlayState::Stopped)
    static_cast<void>(play.Stop());
  const auto diagnostics = created.surface->Diagnostics();
  std::cerr << "graphical evidence: acquired=" << diagnostics.acquiredFrames
            << " presented=" << diagnostics.presentedFrames
            << " ui_draws=" << diagnostics.nativeUiDrawCalls
            << " ui_uploads=" << diagnostics.nativeUiTextureUploads
            << " ui_rejected=" << diagnostics.nativeUiRejectedTextures
            << " scene_draws=" << diagnostics.sceneDrawCalls << " recovery="
            << (recovery_choice == nexora::editor::imgui::RecoveryChoice::Recover   ? "recover"
                : recovery_choice == nexora::editor::imgui::RecoveryChoice::Discard ? "discard"
                                                                                    : "none")
            << " selector=" << selector_result << " access="
            << (project ? (project->workspace.Writable() ? "read-write" : "read-only") : "none")
            << " schema=" << (project ? project->workspace.Project().schema_version : 0)
            << " upgrade="
            << (!project ? "none"
                : project->workspace.UpgradeState() == nexora::editor::ProjectUpgradeState::Applied
                    ? "applied"
                : project->workspace.UpgradeState() == nexora::editor::ProjectUpgradeState::Required
                    ? "required"
                    : "current")
            << " recents=" << recent_projects.Entries().size()
            << " scene_nodes=" << scene.Nodes().size()
            << " scene_selected=" << scene.Selection().size()
            << " pie_ticks=" << play.Stats().fixed_ticks
            << " pie_steps=" << play.Stats().manual_steps << '\n';
  if (created.surface->DrainAndDestroy() != Nexora::Presentation::SurfaceStatus::Ready)
    result = 1;
  return result;
}
#endif

int Run(int argc, char **argv) {
  std::filesystem::path project;
  std::filesystem::path report;
  std::filesystem::path recent_projects_path;
  bool graphical = false;
  bool read_only = false;
  bool native_scene_preview = false;
  std::uint32_t frame_limit = 0;
  for (int index = 1; index < argc; ++index) {
    const std::string_view argument(argv[index]);
    if (argument.starts_with("--project="))
      project = argument.substr(10);
    else if (argument.starts_with("--report="))
      report = argument.substr(9);
    else if (argument.starts_with("--recent-projects="))
      recent_projects_path = argument.substr(18);
    else if (argument == "--graphical")
      graphical = true;
    else if (argument == "--read-only")
      read_only = true;
    else if (argument == "--native-scene-preview")
      native_scene_preview = true;
    else if (argument.starts_with("--frames=")) {
      const auto digits = argument.substr(9);
      const auto [end, status] =
          std::from_chars(digits.data(), digits.data() + digits.size(), frame_limit);
      if (digits.empty() || status != std::errc{} || end != digits.data() + digits.size()) {
        std::cerr << "--frames expects a non-negative 32-bit integer\n";
        return 2;
      }
    } else if (argument == "--help") {
      std::cout << "NexoraEditor [--project=PATH] [--read-only] [--report=PATH] [--graphical] "
                   "[--frames=N] [--recent-projects=PATH] [--native-scene-preview]\n";
      return 0;
    } else {
      std::cerr << "unknown argument: " << argument << '\n';
      return 2;
    }
  }
  if (project.empty() && !graphical) {
    std::cerr << "--project is required unless --graphical opens the project selector\n";
    return 2;
  }
  if (native_scene_preview && !graphical) {
    std::cerr << "--native-scene-preview requires --graphical\n";
    return 2;
  }
  std::string error;
  nexora::editor::RecentProjectStore recent_projects;
  if (recent_projects_path.empty())
    recent_projects_path = nexora::editor::RecentProjectStore::DefaultPath();
  if (!recent_projects.Open(recent_projects_path, &error))
    std::cerr << "recent-project warning: " << error << '\n';
  std::optional<ProjectState> project_state;
  if (!project.empty()) {
    ProjectState opened;
    if (!LoadProject(project,
                     read_only ? nexora::editor::ProjectAccess::ReadOnly
                               : nexora::editor::ProjectAccess::ReadWrite,
                     false, {}, opened, &error)) {
      std::cerr << "content indexing or project open failed: " << error << '\n';
      return 1;
    }
    project_state = std::move(opened);
    if (!recent_projects.Record(project_state->workspace, &error))
      std::cerr << "recent-project warning: " << error << '\n';
  }
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
  if (graphical)
    return RunGraphical(std::move(project_state), recent_projects,
                        read_only ? nexora::editor::ProjectAccess::ReadOnly
                                  : nexora::editor::ProjectAccess::ReadWrite,
                        frame_limit, native_scene_preview);
#else
  static_cast<void>(frame_limit);
  static_cast<void>(native_scene_preview);
  if (graphical) {
    std::cerr << "graphical shell was not enabled at build time\n";
    return 2;
  }
#endif
  if (!project_state) {
    std::cerr << "--project is required\n";
    return 2;
  }
  const std::string json =
      "{\n  \"application\": \"NexoraEditor\",\n  \"project\": \"" +
      project_state->workspace.Project().name +
      "\",\n  \"panels\": " + std::to_string(nexora::editor::ProductShell::Panels().size()) +
      ",\n  \"assets\": " + std::to_string(project_state->assets.Entries().size()) +
      ",\n  \"access\": \"" + (project_state->workspace.Writable() ? "read-write" : "read-only") +
      "\",\n  \"schema\": " + std::to_string(project_state->workspace.Project().schema_version) +
      "\n}\n";
  if (report.empty())
    std::cout << json;
  else {
    std::ofstream output(report);
    if (!output || !(output << json))
      return 1;
  }
  return 0;
}
} // namespace

int main(int argc, char **argv) { return Run(argc, argv); }
