#include "Nexora/Core/JobSystem.h"
#include "Nexora/Editor/AssetImport.h"
#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/ProjectContent.h"
#if defined(NEXORA_EDITOR_GRAPHICAL_SHELL)
#include "CoreConsoleIngress.h"
#include "GameViewPreview.h"
#include "MaterialScenePreview.h"
#include "Nexora/Editor/AdditiveSceneComposition.h"
#include "Nexora/Editor/AdditiveSceneSession.h"
#include "Nexora/Editor/EditorProduction.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#include "Nexora/Editor/SceneAuthoring.h"
#include "Nexora/Editor/SceneComparisonJob.h"
#include "Nexora/Editor/SceneFiles.h"
#include "Nexora/Editor/StaticProjectExportJob.h"
#include "Nexora/Editor/ViewportMath.h"
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/Math/Math.h"
#include "Nexora/Presentation/RenderSurface.h"
#include "Nexora/RHI/Device.h"
#include "Nexora/Runtime/EditorSdk.h"
#include "PlayGameplayModule.h"
#include "PlayInputForwarding.h"
#include "SceneFileContinuation.h"
#include "SceneMeshPreview.h"
#include "SceneMovePlanes.h"
#include "ScenePreviewCandidates.h"
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
#include <memory>
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
using nexora::editor::preview::NativeSceneMeshes;
using nexora::editor::preview::NativeSceneProxyCandidates;
using nexora::editor::preview::NativeSceneProxyMesh;
using nexora::editor::preview::PrepareNativeSceneMeshes;

std::optional<nexora::runtime::Transform>
NativeSceneGizmoFrame(const nexora::editor::SceneDocument &scene, bool center_pivot) {
  return scene.SelectionGizmoFrame(center_pivot ? nexora::editor::GizmoPivot::Center
                                                : nexora::editor::GizmoPivot::Pivot);
}

void NativeSceneOperationPivot(const nexora::editor::SceneDocument &scene, bool center_pivot,
                               nexora::editor::GizmoOperation &operation) {
  if (center_pivot) {
    if (const auto frame = NativeSceneGizmoFrame(scene, true)) {
      operation.pivot = nexora::editor::GizmoPivot::Center;
      operation.center = {frame->x, frame->y, frame->z};
      operation.axes = nexora::editor::GizmoAxes(*frame, nexora::editor::GizmoSpace::Local);
    }
  }
}

struct NativeSceneAxisHandle final {
  nexora::editor::ViewportVector axis{};
  nexora::editor::PickCandidate bounds{};
  Nexora::Presentation::SceneInstance instance{};
  std::optional<nexora::editor::preview::MovePlane> plane;
};

std::vector<NativeSceneAxisHandle>
NativeSceneAxisHandles(const nexora::editor::SceneDocument &scene,
                       std::span<const nexora::editor::PickCandidate> candidates, bool local_axes,
                       bool center_pivot, bool include_planes = false) {
  if (scene.Selection().empty())
    return {};
  const auto selected = scene.Selection().front();
  const auto found =
      std::find_if(candidates.begin(), candidates.end(),
                   [selected](const auto &candidate) { return candidate.entity == selected; });
  if (found == candidates.end())
    return {};
  const auto pose = NativeSceneGizmoFrame(scene, center_pivot);
  if (!pose)
    return {};
  const std::array<double, 3> pivot{pose->x, pose->y + 0.5, pose->z};
  const auto axes = nexora::editor::GizmoAxes(
      *pose, local_axes ? nexora::editor::GizmoSpace::Local : nexora::editor::GizmoSpace::World);
  const auto component = [](nexora::editor::ViewportVector vector, std::size_t coordinate) {
    return coordinate == 0 ? vector.x : coordinate == 1 ? vector.y : vector.z;
  };
  std::vector<NativeSceneAxisHandle> handles;
  handles.reserve(include_planes ? 6 : 3);
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
  if (include_planes)
    for (const auto &plane : nexora::editor::preview::MovePlaneHandles(*pose, local_axes)) {
      NativeSceneAxisHandle handle;
      handle.bounds = plane.bounds;
      handle.instance = plane.instance;
      handle.plane = plane.plane;
      handles.push_back(handle);
    }
  return handles;
}

std::vector<NativeSceneAxisHandle>
NativeSceneRotationHandles(const nexora::editor::SceneDocument &scene,
                           std::span<const nexora::editor::PickCandidate> candidates,
                           bool local_axes, bool center_pivot) {
  if (scene.Selection().empty())
    return {};
  const auto selected = scene.Selection().front();
  if (std::none_of(candidates.begin(), candidates.end(),
                   [selected](const auto &candidate) { return candidate.entity == selected; }))
    return {};
  const auto pose = NativeSceneGizmoFrame(scene, center_pivot);
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
                        nexora::editor::imgui::NativeSceneOrbit orbit, bool center_pivot) {
  auto handles = NativeSceneAxisHandles(scene, candidates, true, center_pivot);
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
    const auto pose = NativeSceneGizmoFrame(scene, center_pivot);
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
  std::optional<nexora::editor::preview::MovePlane> plane{};
};

NativeScenePickHit PickNativeSceneProxy(const nexora::editor::SceneDocument &scene,
                                        Nexora::Presentation::SceneViewport viewport,
                                        nexora::editor::imgui::SceneOverviewCamera camera,
                                        nexora::editor::imgui::NativeSceneOrbit orbit,
                                        nexora::editor::imgui::NativeScenePickRequest request,
                                        bool local_axes, bool center_pivot,
                                        nexora::editor::imgui::NativeSceneTool tool,
                                        const NativeSceneMeshes &meshes) {
  if (request.x < viewport.x || request.y < viewport.y ||
      request.x >= viewport.x + viewport.width || request.y >= viewport.y + viewport.height)
    return {};
  const auto view = NativeSceneCamera(camera, orbit);
  const auto ray =
      nexora::editor::ViewportPickRay(view, viewport.width, viewport.height,
                                      request.x - viewport.x + 0.5, request.y - viewport.y + 0.5);
  if (!ray)
    return {};
  const auto candidates = NativeSceneProxyCandidates(scene, &meshes);
  std::optional<nexora::editor::PickHit> proxy;
  for (const auto &candidate : candidates) {
    if (!nexora::editor::PickNearest(*ray, std::span(&candidate, 1), 500.0))
      continue;
    const auto pose = scene.WorldTransform(candidate.entity);
    if (!pose)
      continue;
    const auto authored = meshes.entities.find(candidate.entity);
    const auto matrix = scene.WorldMatrix(candidate.entity);
    const auto distance =
        authored != meshes.entities.end()
            ? (matrix ? nexora::editor::preview::PickMesh(*ray, *authored->second.geometry, *matrix)
                      : std::nullopt)
            : nexora::editor::PickOrientedBox(
                  *ray, {pose->x, pose->y + 0.5, pose->z},
                  {std::abs(pose->sx) * 0.45, std::abs(pose->sy) * 0.45, std::abs(pose->sz) * 0.45},
                  {pose->qx, pose->qy, pose->qz, pose->qw}, 500.0);
    if (distance && (!proxy || *distance < proxy->distance ||
                     (*distance == proxy->distance && candidate.entity < proxy->entity)))
      proxy = nexora::editor::PickHit{candidate.entity, *distance};
  }
  const auto handles =
      tool == nexora::editor::imgui::NativeSceneTool::Select ? std::vector<NativeSceneAxisHandle>{}
      : tool == nexora::editor::imgui::NativeSceneTool::Rotate
          ? NativeSceneRotationHandles(scene, candidates, local_axes, center_pivot)
      : tool == nexora::editor::imgui::NativeSceneTool::Scale
          ? NativeSceneScaleHandles(scene, candidates, orbit, center_pivot)
          : NativeSceneAxisHandles(scene, candidates, local_axes, center_pivot, true);
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
            .axis = handles[gizmo->entity - 1].plane
                        ? std::nullopt
                        : std::optional{handles[gizmo->entity - 1].axis},
            .scale_axis = tool == nexora::editor::imgui::NativeSceneTool::Scale
                              ? std::optional<std::size_t>{gizmo->entity - 1}
                              : std::nullopt,
            .plane = handles[gizmo->entity - 1].plane};
  return {.entity = proxy ? std::optional{proxy->entity} : std::nullopt,
          .axis = std::nullopt,
          .scale_axis = std::nullopt};
}

std::optional<std::array<double, 3>> NativeSceneDragDelta(
    Nexora::Presentation::SceneViewport viewport, nexora::editor::imgui::SceneOverviewCamera camera,
    nexora::editor::imgui::NativeSceneOrbit orbit,
    nexora::editor::imgui::NativeSceneDragRequest request, const nexora::runtime::Transform &pose,
    std::optional<nexora::editor::ViewportVector> handle_axis = std::nullopt,
    std::optional<nexora::editor::preview::MovePlane> handle_plane = std::nullopt) {
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
  if (handle_plane) {
    const auto start = pick_ray(request.start_x, request.start_y);
    const auto end = pick_ray(request.end_x, request.end_y);
    return start && end ? nexora::editor::preview::MovePlaneDelta(*start, *end,
                                                                  {pose.x, pose.y + 0.5, pose.z},
                                                                  *handle_plane, request.snap_step)
                        : std::nullopt;
  }
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
    nexora::editor::imgui::NativeSceneOrbit orbit, bool local_axes, bool center_pivot,
    nexora::editor::imgui::NativeSceneTool tool, std::optional<std::array<double, 3>> drag_preview,
    std::optional<std::pair<nexora::editor::ViewportVector, double>> rotation_preview,
    std::optional<std::pair<std::size_t, double>> scale_preview, const NativeSceneMeshes &meshes,
    const nexora::editor::MaterialAssetCatalog &materials, std::uint64_t generation) {
  const auto candidates = NativeSceneProxyCandidates(scene, &meshes);
  std::vector<nexora::runtime::Id> material_entities;
  for (const auto &candidate : candidates)
    if (meshes.entities.contains(candidate.entity))
      material_entities.push_back(candidate.entity);
  const auto palette = nexora::editor::preview::PrepareMaterialPalette(scene, materials, generation,
                                                                       material_entities);
  const auto handles =
      tool == nexora::editor::imgui::NativeSceneTool::Select ? std::vector<NativeSceneAxisHandle>{}
      : tool == nexora::editor::imgui::NativeSceneTool::Rotate
          ? NativeSceneRotationHandles(scene, candidates, local_axes, center_pivot)
      : tool == nexora::editor::imgui::NativeSceneTool::Scale
          ? NativeSceneScaleHandles(scene, candidates, orbit, center_pivot)
          : NativeSceneAxisHandles(scene, candidates, local_axes, center_pivot, true);
  const std::unordered_set<nexora::runtime::Id> selected(scene.Selection().begin(),
                                                         scene.Selection().end());
  std::optional<nexora::editor::GizmoOperation> operation;
  if (drag_preview) {
    operation.emplace();
    operation->translation = {(*drag_preview)[0], (*drag_preview)[1], (*drag_preview)[2]};
  } else if (rotation_preview) {
    operation.emplace();
    operation->kind = nexora::editor::GizmoOperation::Kind::Rotate;
    operation->axis = rotation_preview->first;
    operation->angle = rotation_preview->second;
  } else if (scale_preview) {
    operation.emplace();
    operation->kind = nexora::editor::GizmoOperation::Kind::Scale;
    const auto [axis, factor] = *scale_preview;
    if (axis == 0)
      operation->factors.x = factor;
    else if (axis == 1)
      operation->factors.y = factor;
    else if (axis == 2)
      operation->factors.z = factor;
    else
      operation->factors = {factor, factor, factor};
  }
  std::optional<std::unordered_map<nexora::runtime::Id, nexora::runtime::Transform>> preview;
  std::optional<std::unordered_map<nexora::runtime::Id, nexora::runtime::TransformMatrix>>
      matrix_preview;
  if (operation) {
    std::vector<nexora::editor::SceneDocument::NodeKey> keys;
    for (const auto id : scene.Selection()) {
      if (const auto key = scene.Key(id))
        keys.push_back(*key);
    }
    NativeSceneOperationPivot(scene, center_pivot, *operation);
    preview = scene.PreviewSelectionGizmo(keys, *operation);
    matrix_preview = scene.PreviewSelectionGizmoMatrices(keys, *operation);
  }
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
  struct AuthoredInstance {
    std::uint64_t resource;
    nexora::runtime::Id entity;
    Nexora::Presentation::SceneInstance instance;
  };
  std::vector<AuthoredInstance> authored_instances;
  for (const auto &candidate : candidates) {
    const auto pose = preview ? std::optional{preview->at(candidate.entity)}
                              : scene.WorldTransform(candidate.entity);
    if (!pose)
      continue;
    Nexora::Presentation::SceneInstance instance{};
    instance.translation[0] = static_cast<float>(pose->x);
    instance.translation[1] = static_cast<float>(pose->y + 0.5);
    instance.translation[2] = static_cast<float>(pose->z);
    instance.scale[0] = static_cast<float>(pose->sx * 0.45);
    instance.scale[1] = static_cast<float>(pose->sy * 0.45);
    instance.scale[2] = static_cast<float>(pose->sz * 0.45);
    instance.rotation[0] = static_cast<float>(pose->qx);
    instance.rotation[1] = static_cast<float>(pose->qy);
    instance.rotation[2] = static_cast<float>(pose->qz);
    instance.rotation[3] = static_cast<float>(pose->qw);
    const bool is_selected = selected.contains(candidate.entity);
    instance.color[0] = is_selected ? 1.0F : 0.35F;
    instance.color[1] = is_selected ? 0.75F : 0.65F;
    instance.color[2] = is_selected ? 0.2F : 1.0F;
    if (const auto authored = meshes.entities.find(candidate.entity);
        authored != meshes.entities.end()) {
      const auto matrix = matrix_preview ? std::optional{matrix_preview->at(candidate.entity)}
                                         : scene.WorldMatrix(candidate.entity);
      auto exact = matrix ? nexora::editor::preview::AffineInstance(*matrix) : std::nullopt;
      if (!exact)
        continue;
      std::ranges::copy(instance.color, exact->color);
      if (palette.entities.at(candidate.entity) != 0)
        std::fill(std::begin(exact->color), std::end(exact->color), 1.0F);
      authored_instances.push_back({authored->second.resource, candidate.entity, *exact});
    } else {
      instances.push_back(instance);
    }
  }
  for (const auto &handle : handles) {
    auto instance = handle.instance;
    if (drag_preview)
      for (std::size_t coordinate = 0; coordinate < 3; ++coordinate)
        instance.translation[coordinate] += static_cast<float>((*drag_preview)[coordinate]);
    instances.push_back(instance);
  }
  std::vector<Nexora::Presentation::SceneMeshBatch> batches;
  batches.push_back({0, 36, 0, static_cast<std::uint32_t>(instances.size())});
  for (const auto &[resource, entity, instance] : authored_instances) {
    const auto range = meshes.ranges.at(resource);
    batches.push_back({range.firstIndex, range.indexCount,
                       static_cast<std::uint32_t>(instances.size()), 1,
                       palette.entities.at(entity)});
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
  std::optional<nexora::editor::preview::Geometry> lit_geometry;
  if (palette.authored) {
    lit_geometry = meshes.geometry;
    if (!nexora::editor::preview::PrepareMaterialTangents(*lit_geometry))
      return Nexora::Presentation::SurfaceStatus::InvalidDescriptor;
    draw.pbr = true;
    draw.materials = palette.materials;
    draw.cameraPosition = {eye.x, eye.y, eye.z};
  }
  draw.vertices = lit_geometry ? lit_geometry->vertices : meshes.geometry.vertices;
  draw.indices = meshes.geometry.indices;
  draw.batches = batches;
  draw.instances = instances;
  draw.viewport = viewport;
  std::memcpy(draw.model_view_projection, mvp.values.data(), sizeof(draw.model_view_projection));
  return surface.DrawScene(draw);
}

int RunGraphical(std::optional<ProjectState> project,
                 nexora::editor::RecentProjectStore &recent_projects,
                 nexora::editor::ProjectAccess selector_access, std::uint32_t frame_limit,
                 bool native_scene_preview,
                 const std::optional<std::string> &initial_gameplay_library) {
  auto created = Nexora::Presentation::CreateRenderSurface(
      {"Nexora Editor", 1280, 720, true, Nexora::Presentation::SurfaceBackend::Automatic,
       Nexora::Presentation::PresentMode::VSync, Nexora::Presentation::ColorSpace::Srgb, true});
  if (!created) {
    std::cerr << "graphical shell unavailable: " << created.reason << '\n';
    return 1;
  }
  nexora::editor::imgui::EditorImGuiHost ui;
  nexora::editor::TelemetryConsent local_diagnostics;
  std::cerr << "diagnostic privacy: enabled=0 retained=0 storage=none transport=none\n";
  ui.SetNativeScenePreview(native_scene_preview);
  if (initial_gameplay_library)
    ui.SetGameplayLibrary(*initial_gameplay_library);
  nexora::core::JobSystem import_jobs{1};
  import_jobs.Start();
  nexora::editor::AssetImportQueue imports{import_jobs};
  nexora::editor::StaticProjectExportJob static_export{import_jobs};
  nexora::editor::SceneComparisonJob scene_comparison{import_jobs};
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
  bool gameplay_settings_invalid = false;
  std::string loaded_gameplay_library;
  bool input_settings_deferred = false;
  const auto load_input_settings = [&](nexora::editor::ProjectWorkspace &workspace) {
    input_settings_deferred = workspace.HasRecoveryJournal();
    std::string error;
    const auto bindings = workspace.LoadPlayInputBindings(&error);
    static_cast<void>(
        ui.SetGameInputBindings(bindings.value_or(nexora::editor::PlayInputBindings{}), workspace));
    ui.SetGameInputBindingsStatus(error);
    if (!error.empty())
      std::cerr << "ignored input settings: " << error << '\n';
  };
  const auto load_gameplay_settings = [&](nexora::editor::ProjectWorkspace &workspace) {
    std::string error;
    const auto saved = workspace.LoadGameplayLibrary(&error);
    gameplay_settings_invalid = !error.empty();
    loaded_gameplay_library = saved.value_or("");
    if (!error.empty())
      std::cerr << "ignored gameplay settings: " << error << '\n';
    ui.SetGameplayLibrary(initial_gameplay_library.value_or(loaded_gameplay_library),
                          content.Browser().ProjectGeneration());
    load_input_settings(workspace);
  };
  std::uint64_t project_generation = 1;
  if (project) {
    if (!content.Open(project->workspace, project->assets, project_generation,
                      project->workspace.Writable(), &layout_error)) {
      std::cerr << layout_error << '\n';
      return 1;
    }
    load_layout(project->workspace);
    load_gameplay_settings(project->workspace);
  }
  nexora::editor::MeshAssetCatalog meshes;
  nexora::editor::MaterialAssetCatalog materials;
  if (project && !materials.PublishContent(content.Browser(), &layout_error))
    std::cerr << "material catalog warning: " << layout_error << '\n';
  if (project && !meshes.PublishContent(content.Browser(), &layout_error))
    std::cerr << "mesh catalog warning: " << layout_error << '\n';
  std::uint64_t mesh_content_revision = content.Browser().Revision();
  std::uint64_t mesh_content_generation = content.Browser().ProjectGeneration();
  nexora::editor::ProductShell shell;
  nexora::runtime::World world;
  const auto scene_id = world.LoadScene("Main");
  if (!world.Activate(scene_id)) {
    std::cerr << "failed to activate the editor scene\n";
    return 1;
  }
  nexora::editor::SceneDocument primary_scene(world, scene_id);
  std::unique_ptr<nexora::editor::SceneFileSession> primary_scene_files;
  std::unique_ptr<nexora::editor::AdditiveSceneSession> scene_documents;
  std::unique_ptr<nexora::editor::AdditiveSceneComposition> scene_composition;
  bool composition_restore_blocked = false;
  std::string composition_restore_error;
  bool composition_was_persisted = false;
  // The presentation host borrows the active document for selection/view state. Source mutation
  // requires scene_authoring_allowed(), in addition to the ImGui reference controls.
  const auto active_scene = [&]() -> nexora::editor::SceneDocument & {
    if (scene_documents)
      if (const auto id = scene_documents->Active())
        if (const auto *document = scene_documents->Document(*id))
          return *const_cast<nexora::editor::SceneDocument *>(document);
    return primary_scene;
  };
  const auto active_files = [&]() -> nexora::editor::SceneFileSession * {
    if (scene_documents)
      if (const auto id = scene_documents->Active())
        if (const auto *files = scene_documents->Files(*id))
          return const_cast<nexora::editor::SceneFileSession *>(files);
    return primary_scene_files.get();
  };
  const auto scene_authoring_allowed = [&] {
    if (!project || !project->workspace.Writable() || composition_restore_blocked)
      return false;
    if (!scene_documents)
      return !project->workspace.HasRecoveryJournal();
    const auto id = scene_documents->Active();
    return id && scene_documents->EditableDocument(*id);
  };
  const auto initialize_scene_documents = [&] {
    if (!project || !primary_scene_files || project->workspace.HasRecoveryJournal() ||
        project->workspace.HasExternalChange())
      return;
    auto candidate =
        std::make_unique<nexora::editor::AdditiveSceneSession>(project->workspace, world);
    if (candidate->Attach(primary_scene, *primary_scene_files)) {
      auto composition = std::make_unique<nexora::editor::AdditiveSceneComposition>(*candidate);
      composition_restore_error.clear();
      composition_restore_blocked = composition->Restore(&composition_restore_error) ==
                                    nexora::editor::SceneCompositionStatus::Rejected;
      scene_documents = std::move(candidate);
      scene_composition = std::move(composition);
      if (composition_restore_blocked)
        std::cerr << "saved scene set could not be restored: " << composition_restore_error << '\n';
    }
  };
  struct SceneViews final {
    nexora::editor::imgui::SceneOverviewCamera overview;
    nexora::editor::imgui::NativeSceneOrbit orbit;
    bool overview_failed{}, preview_failed{};
  };
  std::unordered_map<std::filesystem::path, SceneViews> retained_scene_views;
  nexora::runtime::PlaySession play(world);
  nexora::core::AsyncLogService core_logs{1024};
  core_logs.Start();
  nexora::runtime::RuntimeConsole console{1024};
  nexora::editor::preview::CoreConsoleIngress console_ingress{core_logs, console, "NexoraEditor"};
  nexora::editor::ProfileSession profile{240};
  std::optional<Nexora::Presentation::SceneViewport> native_scene_viewport_reported;
  std::optional<NativeSceneMeshes> native_scene_meshes;
  std::optional<nexora::editor::MeshAssetCatalog> play_meshes;
  std::optional<nexora::editor::preview::MaterialPalette> play_materials;
  std::optional<Nexora::Presentation::SceneViewport> native_game_viewport_reported;
  std::size_t unavailable_meshes = 0;
  std::optional<std::array<std::size_t, 3>> native_mesh_geometry_reported;
  std::optional<nexora::editor::ViewportVector> native_scene_drag_axis;
  std::optional<nexora::editor::preview::MovePlane> native_scene_drag_plane;
  bool native_scene_drag_rotate = false;
  std::optional<std::size_t> native_scene_drag_scale_axis;
  const auto log = [&](nexora::runtime::RuntimeLogSeverity severity, std::string category,
                       std::string message) {
    auto level = nexora::core::LogLevel::Info;
    switch (severity) {
    case nexora::runtime::RuntimeLogSeverity::Trace:
      level = nexora::core::LogLevel::Trace;
      break;
    case nexora::runtime::RuntimeLogSeverity::Info:
      break;
    case nexora::runtime::RuntimeLogSeverity::Warning:
      level = nexora::core::LogLevel::Warning;
      break;
    case nexora::runtime::RuntimeLogSeverity::Error:
      level = nexora::core::LogLevel::Error;
      break;
    case nexora::runtime::RuntimeLogSeverity::Fatal:
      level = nexora::core::LogLevel::Fatal;
      break;
    }
    core_logs.Write(level, std::move(category), std::move(message));
  };
  nexora::editor::preview::PlayGameplayModule gameplay(
      [&](std::uint32_t level, std::string message) {
        core_logs.Write(static_cast<nexora::core::LogLevel>(level), "Gameplay", std::move(message));
      },
      [&] { core_logs.ReportRejected(); });
  log(nexora::runtime::RuntimeLogSeverity::Info, "Editor", "Graphical session started.");
  const auto view_base = [](const std::filesystem::path &relative) {
    return relative == ".nexora/scenes/Main.scene"
               ? relative
               : std::filesystem::path(".nexora/scenes/views") / relative;
  };
  const auto overview_path = [&](const nexora::editor::ProjectWorkspace &workspace) {
    auto path = workspace.Root() / view_base(*active_files()->CurrentPath());
    path.replace_extension(".overview.camera");
    return path;
  };
  const auto preview_camera_path = [&](const nexora::editor::ProjectWorkspace &workspace) {
    auto path = workspace.Root() / view_base(*active_files()->CurrentPath());
    path.replace_extension(".preview.camera");
    return path;
  };
  bool scene_load_failed = false;
  bool overview_load_failed = false;
  bool preview_camera_load_failed = false;
  const auto load_scene_views = [&] {
    static_cast<void>(ui.SetSceneOverviewCamera({}));
    static_cast<void>(ui.SetNativeSceneOrbit({}));
    overview_load_failed = false;
    preview_camera_load_failed = false;
    const auto current_path = active_files() ? active_files()->CurrentPath() : std::nullopt;
    if (!current_path)
      return;
    if (const auto retained = retained_scene_views.find(*current_path);
        retained != retained_scene_views.end()) {
      static_cast<void>(ui.SetSceneOverviewCamera(retained->second.overview));
      static_cast<void>(ui.SetNativeSceneOrbit(retained->second.orbit));
      overview_load_failed = retained->second.overview_failed;
      preview_camera_load_failed = retained->second.preview_failed;
      return;
    }
    nexora::editor::SceneFileSession view_scope(project->workspace, active_scene());
    if (!view_scope.BindCurrent(view_base(*current_path))) {
      overview_load_failed = preview_camera_load_failed = true;
      log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene",
          "Scene view metadata is unavailable.");
      return;
    }
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
  const auto open_scene = [&] {
    scene_composition.reset();
    scene_documents.reset();
    composition_restore_blocked = false;
    composition_restore_error.clear();
    retained_scene_views.clear();
    primary_scene_files =
        std::make_unique<nexora::editor::SceneFileSession>(project->workspace, primary_scene);
    // Validate the legacy settings even when composition selects the bootstrap. RememberCurrent
    // requires this observation and must preserve rejected settings rather than overwrite them.
    const auto startup = primary_scene_files->RestoreStartup(primary_scene_files->Token());
    std::string bootstrap_error;
    const auto bootstrap = nexora::editor::AdditiveSceneComposition::BootstrapScene(
        project->workspace, &bootstrap_error);
    composition_was_persisted = bootstrap.has_value();
    const auto restored =
        bootstrap ? active_files()->Open(active_files()->Token(), *bootstrap, true) : startup;
    if (restored.Applied()) {
      scene_load_failed = false;
      initialize_scene_documents();
      log(nexora::runtime::RuntimeLogSeverity::Info, "Scene", "Restored last scene.");
      load_scene_views();
      return;
    }
    if (restored.status != nexora::editor::SceneFileStatus::NeedsPath) {
      std::cerr << "ignored scene startup settings: " << restored.message << '\n';
      log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene", restored.message);
    }
    constexpr std::string_view initial_path = ".nexora/scenes/Main.scene";
    const auto path = project->workspace.Root() / initial_path;
    std::error_code error;
    const bool exists = std::filesystem::exists(path, error);
    const bool bound = active_files()->BindCurrent(std::filesystem::path(initial_path));
    const bool loaded =
        !error && bound &&
        (!exists || active_files()
                        ->Open(active_files()->Token(), std::filesystem::path(initial_path), true)
                        .Applied());
    if (!loaded) {
      static_cast<void>(active_files()->BindCurrent(std::filesystem::path(initial_path), true));
      scene_load_failed = true;
      const auto encoded = path.generic_u8string();
      const auto message =
          "Scene could not be loaded: " + std::string(encoded.begin(), encoded.end());
      ui.SetSceneSaveResult(message, false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene", message);
      return;
    }
    scene_load_failed = false;
    if (!exists && active_scene().Nodes().empty())
      active_scene().Create("Scene Root");
    log(nexora::runtime::RuntimeLogSeverity::Info, "Scene",
        exists ? "Opened saved scene." : "Created starter scene.");
    initialize_scene_documents();
    load_scene_views();
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
  const auto publish_saved_scene = [&] {
    const auto relative = active_files()->CurrentPath();
    if (!relative || *relative->begin() != "Content")
      return;
    const auto asset_path = relative->lexically_relative("Content");
    const auto encoded = asset_path.generic_u8string();
    const std::string asset_key(encoded.begin(), encoded.end());
    std::string error;
    if (!project->assets.ImportSavedScene(asset_path, &error)) {
      log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene",
          "Scene saved; content import failed: " + error);
      return;
    }
    for (const auto &entry : project->assets.Entries()) {
      if (entry.relative_path != asset_key)
        continue;
      if (content.Browser().Find(entry.id))
        static_cast<void>(content.Browser().PublishArtifact(
            entry.id, entry.artifact_hash, nexora::editor::ThumbnailState::Ready, &error));
      else if (content.Browser().Discover({entry.id,
                                           *relative,
                                           entry.type,
                                           entry.artifact_hash,
                                           nexora::editor::ThumbnailState::Ready,
                                           {}}))
        static_cast<void>(content.Dependencies().Set(entry.id, {}));
      else
        error = "Saved scene could not be published to Content.";
      if (!error.empty())
        log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene", error);
      break;
    }
  };
  const auto remember_scene = [&] {
    if (!project || !project->workspace.Writable() || !active_files() ||
        !active_files()->CurrentPath())
      return;
    const auto remembered = active_files()->RememberCurrent(active_files()->Token());
    if (!remembered.Applied()) {
      std::cerr << "scene startup settings were not saved: " << remembered.message << '\n';
      log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene", remembered.message);
    }
  };
  const auto publish_all_saved_scenes = [&] {
    if (!scene_documents) {
      publish_saved_scene();
      return;
    }
    const auto active = scene_documents->Active();
    const auto original = active && scene_documents->Files(*active)
                              ? std::optional(scene_documents->Files(*active)->Token())
                              : std::nullopt;
    for (const auto &row : scene_documents->Snapshot())
      if (row.owned && scene_documents->Select(row.id, row.token))
        publish_saved_scene();
    if (active && original)
      static_cast<void>(scene_documents->Select(*active, *original));
  };
  nexora::editor::SceneFileToken content_location_token{};
  std::optional<std::filesystem::path> content_location_path;
  std::uint64_t content_location_revision = std::numeric_limits<std::uint64_t>::max();
  const auto refresh_scene_location = [&](bool force = false) {
    if (!project || !active_files() || scene_load_failed)
      return;
    const auto token = active_files()->Token();
    const auto old_path = active_files()->CurrentPath();
    const auto revision = content.Browser().Revision();
    if (!force && token == content_location_token && old_path == content_location_path &&
        revision == content_location_revision)
      return;
    content_location_token = token;
    content_location_revision = revision;
    const auto result = active_files()->SynchronizeContent(token, content);
    content_location_path = active_files()->CurrentPath();
    if (!result.Applied()) {
      ui.SetSceneSaveResult(result.message, false);
      log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene", result.message);
      return;
    }
    if (old_path && content_location_path && old_path != content_location_path) {
      retained_scene_views[*content_location_path] = {
          ui.GetSceneOverviewCamera(), ui.GetNativeSceneOrbit(), overview_load_failed,
          preview_camera_load_failed};
      retained_scene_views.erase(*old_path);
      remember_scene();
    }
  };
  const auto save_scene = [&](bool close_after_save = false) {
    refresh_scene_location(true);
    if (scene_load_failed) {
      ui.SetSceneSaveResult("Scene load failed. Resolve the scene file before saving.", false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene", "Save blocked by failed load.");
      return false;
    }
    if (!scene_authoring_allowed()) {
      ui.SetSceneSaveResult("Scene is read-only. Reopen the project for writing.", false);
      log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene",
          "Save blocked by read-only access.");
      return false;
    }
    if (!active_files())
      return false;
    const auto saved = active_files()->Save(active_files()->Token());
    if (saved.status == nexora::editor::SceneFileStatus::NeedsPath) {
      ui.RequestSceneSaveAs(close_after_save);
      return false;
    }
    if (saved.status == nexora::editor::SceneFileStatus::NeedsOverwrite) {
      nexora::editor::imgui::SceneFileRequest request{
          nexora::editor::imgui::SceneFileAction::SaveAs, active_files()->Token(),
          *active_files()->CurrentPath()};
      request.close_after_save = close_after_save;
      if (!PrepareSceneOverwriteRequest(request, saved, active_files()->CurrentPath()))
        return false;
      ui.SetSceneSaveResult(saved.message, false);
      ui.RequestSceneOverwrite(std::move(request));
      return false;
    }
    if (!saved.Applied()) {
      ui.SetSceneSaveResult(saved.message, false);
      log(nexora::runtime::RuntimeLogSeverity::Error, "Scene", saved.message);
      return false;
    }
    publish_saved_scene();
    remember_scene();
    ui.SetSceneSaveResult("Scene saved.", true);
    log(nexora::runtime::RuntimeLogSeverity::Info, "Scene", "Scene saved.");
    return true;
  };
  const auto has_unsaved_owned_scenes = [&] {
    if (!scene_documents)
      return primary_scene.Dirty();
    const auto rows = scene_documents->Snapshot();
    return std::ranges::any_of(rows, [](const auto &row) { return row.owned && row.dirty; });
  };
  const auto update_scene_tabs = [&] {
    std::vector<nexora::editor::imgui::SceneTabItem> tabs;
    if (scene_documents) {
      for (const auto &row : scene_documents->Snapshot()) {
        std::string label = "Scene " + std::to_string(row.id);
        if (row.path) {
          const auto bytes = row.path->filename().generic_u8string();
          label.assign(bytes.begin(), bytes.end());
        } else if (const auto *record = world.FindScene(row.id)) {
          label = record->name;
        }
        for (char &byte : label)
          if (static_cast<unsigned char>(byte) < 32 || byte == 127)
            byte = '?';
        if (label.size() > 256) {
          label.resize(256);
          while (!label.empty() && !nexora::foundation::IsValidUtf8(label))
            label.pop_back();
        }
        if (label.empty() || !nexora::foundation::IsValidUtf8(label))
          label = "Scene " + std::to_string(row.id);
        tabs.push_back({row.id, row.token, std::move(label), row.path, row.owned, row.dirty,
                        row.id != scene_id, composition_restore_blocked});
      }
    }
    static_cast<void>(ui.SetSceneTabs(tabs,
                                      scene_documents ? scene_documents->Active().value_or(0) : 0,
                                      static_export.Busy() || composition_restore_blocked));
    if (composition_restore_blocked)
      ui.SetSceneTabStatus(
          "Cannot restore the saved scene set. Repair missing/conflicting sources or "
          "saved scene settings, then reopen the project. " +
              composition_restore_error,
          false);
  };
  const auto synchronize_all_scenes = [&] {
    if (!scene_documents)
      return;
    for (const auto &row : scene_documents->Snapshot()) {
      const auto *files = scene_documents->Files(row.id);
      if (!files)
        continue;
      auto *view_files = const_cast<nexora::editor::SceneFileSession *>(files);
      const auto previous = files->CurrentPath();
      static_cast<void>(view_files->SynchronizeContent(row.token, content));
      if (previous && files->CurrentPath() && previous != files->CurrentPath()) {
        if (const auto found = retained_scene_views.find(*previous);
            found != retained_scene_views.end()) {
          retained_scene_views[*files->CurrentPath()] = found->second;
          retained_scene_views.erase(*previous);
        }
      }
    }
  };
  const auto save_as_active = [&](nexora::editor::SceneFileToken token,
                                  const std::filesystem::path &path, bool replace,
                                  std::optional<nexora::editor::SceneOverwriteToken> overwrite) {
    if (!scene_authoring_allowed() || !active_files())
      return nexora::editor::SceneFileResult{nexora::editor::SceneFileStatus::Rejected,
                                             "This scene is a read-only reference."};
    if (scene_documents)
      return scene_documents->SaveAs(*scene_documents->Active(), token, path, replace, overwrite);
    return active_files()->SaveAs(token, path, replace, overwrite);
  };
  const auto destination_open_elsewhere = [&](const std::filesystem::path &path) {
    if (!scene_documents)
      return false;
    const auto portable_key = [](const std::filesystem::path &value) {
      const auto bytes = value.lexically_normal().generic_u8string();
      std::string key(bytes.begin(), bytes.end());
      std::ranges::transform(key, key.begin(), [](unsigned char byte) {
        return static_cast<char>(byte >= 'A' && byte <= 'Z' ? byte + 32 : byte);
      });
      return key;
    };
    for (const auto &row : scene_documents->Snapshot()) {
      if (row.id == scene_documents->Active() || !row.path)
        continue;
      std::error_code error;
      if (portable_key(path) == portable_key(*row.path) ||
          std::filesystem::equivalent(project->workspace.Root() / path,
                                      project->workspace.Root() / *row.path, error))
        return true;
    }
    return false;
  };
  const auto save_all_and_exit = [&] {
    if (composition_restore_blocked) {
      ui.SetSceneSaveResult("Resolve the saved scene set before saving source files.", false);
      return false;
    }
    if (!scene_documents)
      return save_scene(true);
    if (static_export.Busy()) {
      ui.SetSceneSaveResult("Wait for or cancel the project export before saving and exiting.",
                            false);
      return false;
    }
    if (play.State() != nexora::runtime::PlayState::Stopped) {
      gameplay.Unload();
      static_cast<void>(play.Stop());
      play_meshes.reset();
      play_materials.reset();
      ui.SetNativeGameStatus({});
    }
    for (const auto &row : scene_documents->Snapshot()) {
      if (row.owned && !row.path) {
        if (!scene_documents->Select(row.id, row.token))
          return false;
        load_scene_views();
        ui.SetSceneFileContext(active_files()->Token(), active_files()->CurrentPath(),
                               active_files()->SaveBlocked());
        update_scene_tabs();
        ui.RequestSceneSaveAs(true);
        return false;
      }
    }
    synchronize_all_scenes();
    const auto saved = scene_documents->SaveAll();
    if (saved.Published())
      publish_all_saved_scenes();
    ui.SetSceneSaveResult(saved.message, saved.Published());
    return saved.Published();
  };
  while (!exit_requested && !created.surface->CloseRequested() &&
         (frame_limit == 0 || frames < frame_limit)) {
    console_ingress.Poll();
    const auto begin_frame_status = created.surface->BeginFrame();
    if (created.surface->CloseRequested()) {
      if (project && has_unsaved_owned_scenes() && created.surface->CancelCloseRequest()) {
        ui.RequestCloseConfirmation();
        continue;
      }
      break;
    }
    const auto &frame = created.surface->FrameInfo();
    const auto frame_started = std::chrono::steady_clock::now();
    const auto dpi = std::isfinite(frame.dpiScale) ? std::max(frame.dpiScale, 0.25F) : 1.0F;
    ui.SetDisplay(static_cast<float>(frame.width) / dpi, static_cast<float>(frame.height) / dpi,
                  dpi);
    ui.ProcessEvents(created.surface->Events());
    const auto action = Nexora::Presentation::RecoveryAction(begin_frame_status);
    if (action == Nexora::Presentation::SurfaceAction::Abort) {
      result = 1;
      break;
    }
    if (action != Nexora::Presentation::SurfaceAction::Render || frame.width == 0 ||
        frame.height == 0) {
      nexora::editor::preview::ForwardPlayInput(play, gameplay, ui.GameInputFocused(),
                                                created.surface->Events(), ui.GameInputBindings());
      continue;
    }
    ui.UpdateImeCandidate(*created.surface);
    const auto native_gpu = created.surface->Diagnostics();
    auto gpu_source = nexora::editor::GpuProfileSource::Unavailable;
    switch (native_gpu.gpuTiming.source) {
    case Nexora::Presentation::GpuTimingSource::VulkanTimestamps:
      gpu_source = nexora::editor::GpuProfileSource::VulkanTimestamps;
      break;
    case Nexora::Presentation::GpuTimingSource::Dx12Timestamps:
      gpu_source = nexora::editor::GpuProfileSource::Dx12Timestamps;
      break;
    case Nexora::Presentation::GpuTimingSource::MetalCommandBuffer:
      gpu_source = nexora::editor::GpuProfileSource::MetalCommandBuffer;
      break;
    case Nexora::Presentation::GpuTimingSource::Unavailable:
      break;
    }
    static_cast<void>(profile.ObserveGpuFrame(
        created.surface->UiResourceDomain(), gpu_source, native_gpu.softwareRasterizer,
        {native_gpu.gpuTiming.completedSubmission, native_gpu.gpuTiming.milliseconds}));
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
            if (!materials.PublishContent(content.Browser(), &selector_error))
              std::cerr << "material catalog warning: " << selector_error << '\n';
            if (!meshes.PublishContent(content.Browser(), &selector_error))
              std::cerr << "mesh catalog warning: " << selector_error << '\n';
            pending_project.reset();
            selector_result = was_created ? "created" : "opened";
            if (!recent_projects.Record(project->workspace, &selector_error))
              std::cerr << "recent-project warning: " << selector_error << '\n';
            load_layout(project->workspace);
            load_gameplay_settings(project->workspace);
            open_scene();
            ui.SetProjectSelectorError({});
            ui.SetProjectSelectorStatus({}, false);
          }
        }
      }
    }
    if (project) {
      if (!scene_documents)
        initialize_scene_documents();
      refresh_scene_location();
      if (active_files())
        ui.SetSceneFileContext(active_files()->Token(), active_files()->CurrentPath(),
                               scene_load_failed || active_files()->SaveBlocked());
      ui.SetStaticExportStatus(static_export.Snapshot(), static_export.Busy());
      if (active_files() && scene_comparison.Poll(*active_files())) {
        const auto compared = scene_comparison.Snapshot();
        if (compared.result)
          std::cerr << "scene comparison ready fields=" << compared.result->rows.size()
                    << " conflicts=" << compared.result->conflicts << '\n';
        else
          std::cerr << "scene comparison finished: " << compared.message << '\n';
      }
      ui.SetSceneComparisonStatus(scene_comparison.Snapshot(), scene_comparison.Busy());
      update_scene_tabs();
      ui.DrawProductShell(shell, &active_scene(), &project->workspace, &content, &recent_projects,
                          &imports, &console, &play, &profile, &meshes, &materials);
      if (auto request = ui.TakeSceneTabRequest()) {
        std::string error;
        bool applied = false;
        const bool current = scene_documents && active_files() && !composition_restore_blocked &&
                             request->source == active_files()->Token() &&
                             play.State() == nexora::runtime::PlayState::Stopped &&
                             !static_export.Busy() && !project->workspace.HasRecoveryJournal() &&
                             !project->workspace.HasExternalChange();
        if (!current) {
          error = "Scene action is stale, or Play/export/recovery is active.";
        } else {
          const auto old_path = active_files()->CurrentPath();
          if (old_path)
            retained_scene_views[*old_path] = {ui.GetSceneOverviewCamera(),
                                               ui.GetNativeSceneOrbit(), overview_load_failed,
                                               preview_camera_load_failed};
          using TabAction = nexora::editor::imgui::SceneTabAction;
          switch (request->action) {
          case TabAction::Select:
            applied = scene_documents->Select(request->target, request->target_token, &error);
            break;
          case TabAction::New:
            applied = scene_documents->New("Untitled", &error).has_value();
            break;
          case TabAction::OpenOwned:
          case TabAction::OpenReference:
            applied = scene_documents
                          ->Open(request->path, request->action == TabAction::OpenOwned, {}, &error)
                          .has_value();
            break;
          case TabAction::Close:
            if (request->target == scene_id) {
              error = "The primary scene stays open for the project session.";
              break;
            }
            if (!scene_documents->Files(request->target) ||
                scene_documents->Files(request->target)->Token() != request->target_token) {
              error = "Close action belongs to an expired document.";
              break;
            }
            if (request->save_before_close) {
              synchronize_all_scenes();
              const auto saved = scene_documents->SaveAll();
              if (!saved.Published()) {
                error = saved.message;
                break;
              }
              publish_all_saved_scenes();
            }
            applied = scene_documents->Remove(request->target, request->target_token,
                                              request->discard_dirty, &error);
            break;
          case TabAction::SaveAll: {
            synchronize_all_scenes();
            const auto saved = scene_documents->SaveAll();
            applied = saved.Published();
            if (applied)
              publish_all_saved_scenes();
            error = saved.message;
            break;
          }
          }
          if (applied) {
            native_scene_meshes.reset();
            native_scene_drag_axis.reset();
            native_scene_drag_plane.reset();
            native_scene_drag_rotate = false;
            native_scene_drag_scale_axis.reset();
            native_scene_viewport_reported.reset();
            scene_load_failed = false;
            load_scene_views();
            if (active_files())
              ui.SetSceneFileContext(active_files()->Token(), active_files()->CurrentPath(),
                                     active_files()->SaveBlocked());
            update_scene_tabs();
          }
        }
        ui.SetSceneTabStatus(applied ? (error.empty() ? "Scene action completed." : error) : error,
                             applied);
      }
      if (ui.TakeSceneComparisonCancelRequest())
        static_cast<void>(scene_comparison.Cancel());
      if (const auto request = ui.TakeSceneComparisonRequest(); request && active_files()) {
        std::string comparison_error;
        if (*request != active_files()->Token() ||
            play.State() != nexora::runtime::PlayState::Stopped)
          comparison_error = "Scene comparison request has an old scope or active Play session.";
        else if (scene_comparison.Start(*active_files(), &comparison_error))
          std::cerr << "scene comparison started\n";
        if (!comparison_error.empty())
          ui.SetSceneComparisonStatus(
              {0,
               nexora::editor::SceneComparisonPhase::Failed,
               *request,
               active_files()->CurrentPath().value_or(std::filesystem::path{}),
               comparison_error,
               {}},
              scene_comparison.Busy());
      }
      if (input_settings_deferred && !project->workspace.HasRecoveryJournal())
        load_input_settings(project->workspace);
      refresh_scene_location();
      if (auto request = ui.TakeGameInputBindingsSaveRequest()) {
        std::string error;
        const bool current = request->project == project->workspace.Project().id &&
                             request->root == project->workspace.Root() &&
                             play.State() == nexora::runtime::PlayState::Stopped;
        const bool saved =
            current && project->workspace.SavePlayInputBindings(request->bindings, &error);
        if (!saved && error.empty())
          error = "Input settings request belongs to another project or active Play session.";
        ui.SetGameInputBindingsStatus(saved ? "Input bindings saved to project." : error);
        log(saved ? nexora::runtime::RuntimeLogSeverity::Info
                  : nexora::runtime::RuntimeLogSeverity::Error,
            "Input", saved ? "Project input bindings saved." : error);
      }
      if (ui.TakeGpuExportRequest()) {
        std::string error;
        const auto observation = profile.GpuTiming();
        const bool saved = project->workspace.ExportGpuTimingJson(
            observation.source, observation.software_rasterizer, profile.GpuSamples(),
            profile.GpuDroppedCount(), &error);
        ui.SetProfileExportStatus(saved ? "Saved .nexora/gpu-timing.json" : error);
        log(saved ? nexora::runtime::RuntimeLogSeverity::Info
                  : nexora::runtime::RuntimeLogSeverity::Error,
            "Profiler", saved ? "Native GPU timing trace exported." : error);
      }
      if (ui.TakeGpuImportRequest()) {
        std::string error;
        auto capture = project->workspace.ImportGpuTimingJson(&error);
        const bool loaded = capture && ui.SetImportedGpuCapture(std::move(*capture));
        if (!loaded && error.empty())
          error = "Imported GPU timing snapshot was rejected.";
        ui.SetProfileExportStatus(loaded ? "Loaded .nexora/gpu-timing.json (static)" : error);
        log(loaded ? nexora::runtime::RuntimeLogSeverity::Info
                   : nexora::runtime::RuntimeLogSeverity::Error,
            "Profiler",
            loaded ? "Native GPU timing trace imported; live capture unchanged." : error);
      }
      if (ui.TakeMemoryExportRequest()) {
        std::string error;
        const bool saved = project->workspace.ExportProcessMemoryJson(
            profile.MemorySamples(), profile.MemoryDroppedCount(), &error);
        ui.SetProfileExportStatus(saved ? "Saved .nexora/process-memory.json" : error);
        log(saved ? nexora::runtime::RuntimeLogSeverity::Info
                  : nexora::runtime::RuntimeLogSeverity::Error,
            "Profiler", saved ? "Process memory trace exported." : error);
      }
      if (ui.TakeMemoryImportRequest()) {
        std::string error;
        auto capture = project->workspace.ImportProcessMemoryJson(&error);
        const bool loaded = capture && ui.SetImportedMemoryCapture(std::move(*capture));
        if (!loaded && error.empty())
          error = "Imported process memory snapshot was rejected.";
        ui.SetProfileExportStatus(loaded ? "Loaded .nexora/process-memory.json (static)" : error);
        log(loaded ? nexora::runtime::RuntimeLogSeverity::Info
                   : nexora::runtime::RuntimeLogSeverity::Error,
            "Profiler", loaded ? "Process memory trace imported; live capture unchanged." : error);
      }
      if (ui.TakeProfileCsvImportRequest()) {
        std::string error;
        auto imported = project->workspace.ImportEditorFrameProcessingCsv(&error);
        const bool loaded = imported && ui.SetImportedProfileCapture(std::move(*imported));
        if (!loaded && error.empty())
          error = "Imported CSV snapshot was rejected.";
        ui.SetProfileExportStatus(loaded ? "Loaded .nexora/frame-processing.csv (static)" : error);
        log(loaded ? nexora::runtime::RuntimeLogSeverity::Info
                   : nexora::runtime::RuntimeLogSeverity::Error,
            "Profiler", loaded ? "Frame processing CSV imported; live capture unchanged." : error);
      }
      if (ui.TakeProfileJsonImportRequest()) {
        std::string error;
        auto imported = project->workspace.ImportEditorFrameProcessingJson(&error);
        const bool loaded = imported && ui.SetImportedProfileCapture(std::move(*imported));
        if (!loaded && error.empty())
          error = "Imported JSON snapshot was rejected.";
        ui.SetProfileExportStatus(loaded ? "Loaded .nexora/frame-processing.json (static)" : error);
        log(loaded ? nexora::runtime::RuntimeLogSeverity::Info
                   : nexora::runtime::RuntimeLogSeverity::Error,
            "Profiler", loaded ? "Frame processing JSON imported; live capture unchanged." : error);
      }
      if (ui.TakeProfileExportRequest()) {
        std::string error;
        const bool exported = project->workspace.ExportEditorFrameProcessing(
            profile.Samples(), profile.DroppedCount(), &error);
        ui.SetProfileExportStatus(exported ? "Saved .nexora/frame-processing.csv" : error);
        log(exported ? nexora::runtime::RuntimeLogSeverity::Info
                     : nexora::runtime::RuntimeLogSeverity::Error,
            "Profiler", exported ? "Frame processing CSV exported." : error);
      }
      if (ui.TakeProfileJsonExportRequest()) {
        std::string error;
        const bool exported = project->workspace.ExportEditorFrameProcessingJson(
            profile.Samples(), profile.DroppedCount(), &error);
        ui.SetProfileExportStatus(exported ? "Saved .nexora/frame-processing.json" : error);
        log(exported ? nexora::runtime::RuntimeLogSeverity::Info
                     : nexora::runtime::RuntimeLogSeverity::Error,
            "Profiler", exported ? "Frame processing JSON exported." : error);
      }
      if (auto review = ui.TakePlayApplyRequest()) {
        std::string error;
        const auto status =
            scene_authoring_allowed() && !project->workspace.HasRecoveryJournal()
                ? nexora::editor::ApplyReviewedPlayTransforms(active_scene(), play, *review, &error)
                : nexora::editor::PlayTransformApplyStatus::Failed;
        if (status == nexora::editor::PlayTransformApplyStatus::Applied) {
          gameplay.Unload();
          static_cast<void>(play.Stop());
          play_meshes.reset();
          play_materials.reset();
          native_game_viewport_reported.reset();
          ui.SetNativeGameStatus({});
          play_accumulator = 0;
          ui.SetGameplayStatus("Play transforms applied. Undo restores the Editor values.");
          log(nexora::runtime::RuntimeLogSeverity::Info, "PIE",
              "Play transforms applied as one Undo step.");
        } else {
          if (error.empty())
            error = "Project write access or recovery prevents applying Play transforms.";
          ui.SetGameplayStatus(error);
          log(nexora::runtime::RuntimeLogSeverity::Error, "PIE", error);
        }
      }
      switch (ui.TakePlayCommand()) {
      case nexora::editor::imgui::PlayCommand::Start: {
        auto frozen_materials = nexora::editor::preview::FreezeGameMaterials(
            active_scene(), materials, content.Browser().ProjectGeneration());
        if (!frozen_materials) {
          ui.SetGameplayStatus(
              "Play material snapshot is unavailable; refresh the project assets.");
          log(nexora::runtime::RuntimeLogSeverity::Error, "PIE", "Play material snapshot failed.");
          break;
        }
        if (play.Start(1.0 / 60.0, [&](nexora::runtime::World &, double seconds) {
              return gameplay.FixedUpdate(seconds);
            })) {
          std::string gameplay_error;
          if (!ui.GameplayLibrary().empty() &&
              !gameplay.LoadRelative(*play.PlayWorld(), project->workspace.Root(),
                                     ui.GameplayLibrary(), gameplay_error)) {
            static_cast<void>(play.Stop());
            ui.SetGameplayStatus(gameplay_error);
            log(nexora::runtime::RuntimeLogSeverity::Error, "PIE", gameplay_error);
            break;
          }
          ui.SetGameplayStatus(gameplay.IsLoaded()
                                   ? "Gameplay module running."
                                   : "Inspection only: no gameplay library selected.");
          play_meshes = meshes;
          play_materials = std::move(frozen_materials);
          play_accumulator = 0.0;
          last_play_frame = std::chrono::steady_clock::now();
          log(nexora::runtime::RuntimeLogSeverity::Info, "PIE", "Isolated Play World started.");
        }
        break;
      }
      case nexora::editor::imgui::PlayCommand::Pause:
        static_cast<void>(play.Pause());
        break;
      case nexora::editor::imgui::PlayCommand::Resume:
        last_play_frame = std::chrono::steady_clock::now();
        static_cast<void>(play.Resume());
        break;
      case nexora::editor::imgui::PlayCommand::Step:
        if (!play.Step() && play.LastPauseReason() == nexora::runtime::PauseReason::RuntimeFailure)
          log(nexora::runtime::RuntimeLogSeverity::Error, "PIE", "Gameplay fixed step failed.");
        break;
      case nexora::editor::imgui::PlayCommand::Stop:
        gameplay.Unload();
        static_cast<void>(play.Stop());
        ui.SetGameplayStatus("Play stopped.");
        play_meshes.reset();
        play_materials.reset();
        native_game_viewport_reported.reset();
        ui.SetNativeGameStatus({});
        play_accumulator = 0.0;
        log(nexora::runtime::RuntimeLogSeverity::Info, "PIE", "Play World discarded.");
        break;
      case nexora::editor::imgui::PlayCommand::None:
        break;
      }
      nexora::editor::preview::ForwardPlayInput(play, gameplay, ui.GameInputFocused(),
                                                created.surface->Events(), ui.GameInputBindings());
      const auto play_now = std::chrono::steady_clock::now();
      const double elapsed =
          std::clamp(std::chrono::duration<double>(play_now - last_play_frame).count(), 0.0, 0.25);
      last_play_frame = play_now;
      if (play.State() == nexora::runtime::PlayState::Playing) {
        play_accumulator += elapsed;
        for (int tick = 0; tick < 4 && play_accumulator >= 1.0 / 60.0; ++tick) {
          if (!play.Tick()) {
            log(nexora::runtime::RuntimeLogSeverity::Error, "PIE", "Gameplay fixed update failed.");
            break;
          }
          play_accumulator -= 1.0 / 60.0;
        }
        play_accumulator = std::min(play_accumulator, 4.0 / 60.0);
        if (play.State() == nexora::runtime::PlayState::Playing && !gameplay.Update(elapsed)) {
          static_cast<void>(play.ReportRuntimeFailure());
          ui.SetGameplayStatus("Gameplay update failed; Play paused. Stop to reload the module.");
          log(nexora::runtime::RuntimeLogSeverity::Error, "PIE", "Gameplay update failed.");
        }
      } else {
        play_accumulator = 0.0;
      }
      if (play.State() == nexora::runtime::PlayState::Paused &&
          play.LastPauseReason() == nexora::runtime::PauseReason::RuntimeFailure)
        ui.SetGameplayStatus("Gameplay callback failed; Play paused. Stop to reload the module.");
      if (mesh_content_revision != content.Browser().Revision() ||
          mesh_content_generation != content.Browser().ProjectGeneration()) {
        std::string mesh_error;
        if (!materials.PublishContent(content.Browser(), &mesh_error))
          log(nexora::runtime::RuntimeLogSeverity::Error, "Content", mesh_error);
        if (!meshes.PublishContent(content.Browser(), &mesh_error))
          log(nexora::runtime::RuntimeLogSeverity::Error, "Content", mesh_error);
        mesh_content_revision = content.Browser().Revision();
        mesh_content_generation = content.Browser().ProjectGeneration();
      }
      // Commit this frame's native authoring input before Save or Save-and-exit.
      if (const auto viewport = ui.NativeScenePreviewViewport()) {
        native_scene_meshes =
            PrepareNativeSceneMeshes(active_scene(), meshes, content, project_generation);
        const std::array geometry_counts{native_scene_meshes->entities.size(),
                                         native_scene_meshes->geometry.vertices.size(),
                                         native_scene_meshes->geometry.indices.size()};
        if (geometry_counts[0] != 0 && native_mesh_geometry_reported != geometry_counts) {
          native_mesh_geometry_reported = geometry_counts;
          std::cerr << "native mesh geometry: meshes=" << geometry_counts[0]
                    << " vertices=" << geometry_counts[1] << " indices=" << geometry_counts[2]
                    << '\n';
        }
        if (native_scene_meshes->unavailable != unavailable_meshes) {
          unavailable_meshes = native_scene_meshes->unavailable;
          if (unavailable_meshes != 0)
            log(nexora::runtime::RuntimeLogSeverity::Warning, "Scene",
                std::to_string(unavailable_meshes) +
                    " mesh objects use proxies because their assets are unavailable, exceed "
                    "preview "
                    "limits, or their transforms cannot be displayed.");
        }
        if (const auto request = ui.NativeScenePick()) {
          const auto hit = PickNativeSceneProxy(
              active_scene(), *viewport, ui.GetSceneOverviewCamera(), ui.GetNativeSceneOrbit(),
              *request, ui.NativeSceneLocalAxes(), ui.NativeSceneCenterPivot(),
              ui.GetNativeSceneTool(), *native_scene_meshes);
          native_scene_drag_axis = request->additive ? std::nullopt : hit.axis;
          native_scene_drag_plane = request->additive ? std::nullopt : hit.plane;
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
                std::find(active_scene().Selection().begin(), active_scene().Selection().end(),
                          *hit.entity) != active_scene().Selection().end();
            if (request->additive || already_selected)
              selection.assign(active_scene().Selection().begin(),
                               active_scene().Selection().end());
            const auto existing = std::find(selection.begin(), selection.end(), *hit.entity);
            if (existing == selection.end())
              selection.push_back(*hit.entity);
            else if (request->additive)
              selection.erase(existing);
            static_cast<void>(active_scene().Select(selection));
          } else if (!hit.axis && !hit.plane && !request->additive) {
            static_cast<void>(active_scene().Select(std::span<const nexora::runtime::Id>{}));
          }
        }
        if (const auto drag = ui.NativeSceneDrag();
            drag && scene_authoring_allowed() && !active_scene().Selection().empty()) {
          const auto pose = NativeSceneGizmoFrame(active_scene(), ui.NativeSceneCenterPivot());
          if (pose) {
            std::vector<nexora::editor::SceneDocument::NodeKey> keys;
            for (const auto id : active_scene().Selection()) {
              if (const auto key = active_scene().Key(id))
                keys.push_back(*key);
            }
            if (keys.size() == active_scene().Selection().size()) {
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
                    NativeSceneOperationPivot(active_scene(), ui.NativeSceneCenterPivot(), scale);
                    static_cast<void>(active_scene().ApplySelectionGizmo(keys, scale));
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
                    NativeSceneOperationPivot(active_scene(), ui.NativeSceneCenterPivot(),
                                              rotation);
                    static_cast<void>(active_scene().ApplySelectionGizmo(keys, rotation));
                  }
                }
              } else if (ui.GetNativeSceneTool() == nexora::editor::imgui::NativeSceneTool::Move) {
                const auto delta = NativeSceneDragDelta(
                    *viewport, ui.GetSceneOverviewCamera(), ui.GetNativeSceneOrbit(), *drag, *pose,
                    native_scene_drag_axis, native_scene_drag_plane);
                if (delta)
                  static_cast<void>(active_scene().TranslateSelection(keys, (*delta)[0],
                                                                      (*delta)[1], (*delta)[2]));
              }
            }
          }
          native_scene_drag_axis.reset();
          native_scene_drag_plane.reset();
          native_scene_drag_rotate = false;
          native_scene_drag_scale_axis.reset();
        }
        if (const auto request = ui.TakeNativeSceneSelectAllRequest(); request && active_files()) {
          const auto candidates = NativeSceneProxyCandidates(active_scene(), &*native_scene_meshes);
          static_cast<void>(nexora::editor::preview::SelectNativeSceneCandidates(
              active_scene(), active_files()->Token(), *request, candidates));
        }
        if (const auto request = ui.TakeNativeSceneFrameAllRequest();
            request && active_files() && *request == active_files()->Token()) {
          const auto candidates = NativeSceneProxyCandidates(active_scene(), &*native_scene_meshes);
          static_cast<void>(ui.ApplyNativeSceneFrameAll(*request, candidates));
        }
      }
      if (ui.TakeSceneSaveRequest())
        static_cast<void>(save_scene());
      if (auto request = ui.TakeSceneFileRequest(); request && active_files()) {
        using FileStatus = nexora::editor::SceneFileStatus;
        using FileAction = nexora::editor::imgui::SceneFileAction;
        nexora::editor::SceneFileResult file_result;
        const bool reading = request->action == FileAction::Open && !request->save_current;
        bool can_apply = (reading || scene_authoring_allowed()) &&
                         request->token == active_files()->Token() &&
                         (request->action == FileAction::SaveAs ||
                          play.State() == nexora::runtime::PlayState::Stopped) &&
                         !static_export.Busy();
        if (request->action == FileAction::Open && destination_open_elsewhere(request->path))
          can_apply = false;
        const auto old_path = active_files()->CurrentPath();
        const SceneViews old_views{ui.GetSceneOverviewCamera(), ui.GetNativeSceneOrbit(),
                                   overview_load_failed, preview_camera_load_failed};
        const bool retain_views = old_path && !scene_load_failed && !active_files()->SaveBlocked();
        if (!can_apply)
          file_result = {
              FileStatus::Rejected,
              "Scene action is stale, read-only, busy, or its destination is already open."};
        if (can_apply && request->save_current) {
          file_result = request->save_path
                            ? save_as_active(request->token, *request->save_path,
                                             request->replace_existing, request->overwrite_token)
                            : active_files()->Save(request->token, request->overwrite_token);
          can_apply = file_result.Applied();
          if (can_apply) {
            publish_saved_scene();
            remember_scene();
            scene_load_failed = false;
            if (const auto saved_path = active_files()->CurrentPath())
              retained_scene_views[*saved_path] = old_views;
          }
        }
        if (can_apply) {
          switch (request->action) {
          case FileAction::New:
            file_result = active_files()->New(request->token, request->discard_unsaved);
            break;
          case FileAction::Open:
            file_result =
                active_files()->Open(request->token, request->path, request->discard_unsaved);
            break;
          case FileAction::SaveAs:
            file_result = save_as_active(request->token, request->path, request->replace_existing,
                                         request->overwrite_token);
            break;
          }
        }
        if (file_result.status == FileStatus::NeedsOverwrite) {
          ui.SetSceneSaveResult(file_result.message, false);
          if (PrepareSceneOverwriteRequest(*request, file_result, active_files()->CurrentPath()))
            ui.RequestSceneOverwrite(std::move(*request));
        } else if (file_result.status == FileStatus::NeedsUnsavedChoice)
          ui.RequestSceneUnsavedChoice(std::move(*request));
        else {
          ui.SetSceneSaveResult(file_result.message, file_result.Applied());
          log(file_result.Applied() ? nexora::runtime::RuntimeLogSeverity::Info
                                    : nexora::runtime::RuntimeLogSeverity::Error,
              "Scene", file_result.message);
          if (file_result.Applied()) {
            if (request->action == FileAction::SaveAs)
              publish_saved_scene();
            if (request->action != FileAction::New)
              remember_scene();
            if (retain_views)
              retained_scene_views[*old_path] = old_views;
            scene_load_failed = false;
            if (request->action != FileAction::SaveAs)
              load_scene_views();
            if (request->close_after_save)
              exit_requested = save_all_and_exit();
          } else if (request->close_after_save) {
            ui.RequestSceneSaveAs(true, request->path);
          }
        }
      }
      const auto close_choice = ui.TakeCloseChoice();
      if (close_choice == nexora::editor::imgui::CloseChoice::SaveAndExit) {
        exit_requested = save_all_and_exit();
      } else if (close_choice == nexora::editor::imgui::CloseChoice::DiscardAndExit)
        exit_requested = true;
      if (const auto choice = ui.TakeRecoveryChoice();
          choice != nexora::editor::imgui::RecoveryChoice::None)
        recovery_choice = choice;
      if (auto request = ui.TakeStaticExportRequest()) {
        std::string error;
        bool accepted = false;
        if (request->cancel) {
          accepted = static_export.Cancel();
          if (!accepted)
            error = "No pending StaticView export to cancel.";
        } else if (active_files() && request->token == active_files()->Token() &&
                   active_files()->CurrentPath() && !scene_load_failed &&
                   !active_files()->SaveBlocked() &&
                   play.State() == nexora::runtime::PlayState::Stopped && !exit_requested &&
                   recovery_choice == nexora::editor::imgui::RecoveryChoice::None) {
          nexora::runtime::AssetUuid scene_asset;
          for (const auto &item : content.Browser().Items())
            if (item.type == ".scene" && item.path == *active_files()->CurrentPath()) {
              scene_asset = item.id;
              break;
            }
          accepted = static_export.Start(project->workspace, active_scene(), content, meshes,
                                         materials, scene_asset, &error);
        } else {
          error = "StaticView export request is stale or authoring is blocked.";
        }
        if (!accepted) {
          ui.SetStaticExportStatus(
              {0, nexora::editor::StaticExportPhase::Failed, error, {}, {}, {}, 0, {}, 0},
              static_export.Busy());
          log(nexora::runtime::RuntimeLogSeverity::Error, "Export", error);
        } else {
          ui.SetStaticExportStatus(static_export.Snapshot(), static_export.Busy());
        }
      }
      if (static_export.Poll(project->workspace, active_scene(), content, meshes, materials)) {
        const auto status = static_export.Snapshot();
        ui.SetStaticExportStatus(status, false);
        log(status.phase == nexora::editor::StaticExportPhase::Published
                ? nexora::runtime::RuntimeLogSeverity::Info
                : nexora::runtime::RuntimeLogSeverity::Warning,
            "Export", status.message);
      }
    } else {
      ui.DrawProjectSelector(&recent_projects, selector_access);
      if (ui.TakeProjectSelectorCancel() && pending_project) {
        static_cast<void>(imports.Cancel(pending_project->import));
        ui.SetProjectSelectorStatus("Cancelling project import", true);
      }
      if (auto request = ui.TakeProjectSelectorRequest()) {
        if (pending_project) {
          ui.SetProjectSelectorError("A project import is already running.");
        } else if (request->action == nexora::editor::imgui::ProjectSelectorAction::Preview) {
          std::string preview_error;
          const auto preview =
              nexora::editor::ProjectWorkspace::PreviewUpgrade(request->root, &preview_error);
          if (preview &&
              ui.SetProjectUpgradePreview(nexora::editor::imgui::ProjectUpgradeObservation{
                  request->root, preview->root, preview->project.id, preview->project.name,
                  preview->project.schema_version,
                  nexora::editor::ProjectDescriptor::kSchemaVersion,
                  preview->original_descriptor.size(), preview->documents.size(),
                  preview->state == nexora::editor::ProjectUpgradeState::Required})) {
            ui.SetProjectSelectorError({});
            std::cerr << "project upgrade preview ready from=" << preview->project.schema_version
                      << " required="
                      << (preview->state == nexora::editor::ProjectUpgradeState::Required)
                      << " documents=" << preview->documents.size() << '\n';
          } else {
            static_cast<void>(ui.SetProjectUpgradePreview(std::nullopt));
            ui.SetProjectSelectorError(preview_error.empty()
                                           ? "Project preview is stale or cannot be displayed."
                                           : std::move(preview_error));
            std::cerr << "project upgrade preview failed\n";
          }
        } else if (request->action != nexora::editor::imgui::ProjectSelectorAction::Open &&
                   request->action != nexora::editor::imgui::ProjectSelectorAction::Create) {
          ui.SetProjectSelectorError("Unsupported project request.");
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
    const auto consent_before = local_diagnostics.Enabled();
    const auto events_before = local_diagnostics.Events().size();
    ui.DrawDiagnosticPrivacy(local_diagnostics, project ? project->workspace.Project().id
                                                        : nexora::foundation::Uuid{});
    if (consent_before != local_diagnostics.Enabled())
      std::cerr << "diagnostic consent changed enabled=" << local_diagnostics.Enabled()
                << " retained=" << local_diagnostics.Events().size() << '\n';
    else if (events_before && local_diagnostics.Events().empty())
      std::cerr << "diagnostic queue cleared retained=0\n";
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
        std::optional<std::array<double, 3>> drag_preview;
        std::optional<std::pair<nexora::editor::ViewportVector, double>> rotation_preview;
        std::optional<std::pair<std::size_t, double>> scale_preview;
        if (const auto drag = ui.NativeSceneDragPreview();
            drag && !active_scene().Selection().empty()) {
          const auto pose = NativeSceneGizmoFrame(active_scene(), ui.NativeSceneCenterPivot());
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
                                                  native_scene_drag_axis, native_scene_drag_plane);
            }
          }
        }
        const auto scene_status = DrawNativeScenePreview(
            *created.surface, active_scene(), *viewport, ui.GetSceneOverviewCamera(),
            ui.GetNativeSceneOrbit(), ui.NativeSceneLocalAxes(), ui.NativeSceneCenterPivot(),
            ui.GetNativeSceneTool(), drag_preview, rotation_preview, scale_preview,
            *native_scene_meshes, materials, project_generation);
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
    if (const auto viewport = ui.NativeGameViewport(); viewport && play.PlayWorld() &&
                                                       play_meshes && play_materials &&
                                                       !ui.NativeScenePreviewViewport()) {
      const auto game = nexora::editor::preview::BuildGameFrame(
          *play.PlayWorld(), play.Inspect(), *play_meshes,
          static_cast<float>(viewport->width) / viewport->height, ui.GameCameraSelection(),
          &*play_materials);
      if (!game.camera || game.instances.empty()) {
        ui.SetNativeGameStatus(!game.camera
                                   ? "Add an active camera to the scene before Play."
                                   : "No resolved mesh geometry in the active Play scenes.");
      } else {
        const auto status = created.surface->DrawScene(game.DrawData(*viewport));
        ui.SetNativeGameStatus(
            (game.unavailable
                 ? std::to_string(game.unavailable) +
                       " mesh renderers could not be displayed (asset, budget, or transform). "
                 : "") +
                (game.unavailable_materials ? std::to_string(game.unavailable_materials) +
                                                  " materials use fallback shading."
                                            : ""),
            status != Nexora::Presentation::SurfaceStatus::Unsupported);
        if (status == Nexora::Presentation::SurfaceStatus::Ready &&
            (!native_game_viewport_reported || native_game_viewport_reported->x != viewport->x ||
             native_game_viewport_reported->y != viewport->y ||
             native_game_viewport_reported->width != viewport->width ||
             native_game_viewport_reported->height != viewport->height)) {
          std::cerr << "native game viewport: " << viewport->x << ' ' << viewport->y << ' '
                    << viewport->width << ' ' << viewport->height << '\n';
          native_game_viewport_reported = *viewport;
        }
        if (status != Nexora::Presentation::SurfaceStatus::Ready &&
            status != Nexora::Presentation::SurfaceStatus::Unsupported) {
          if (surface_recoverable(status))
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
    // OS observation belongs to the application, after a successfully presented frame. The
    // owner throttles reads independently of frame rate; drawing widgets only copies its result.
    static_cast<void>(profile.SampleProcessMemory(std::chrono::steady_clock::now()));
    if (frames % 60 == 0) {
      const bool was_empty = local_diagnostics.Events().empty();
      if (local_diagnostics.Record("frame.presented") && was_empty)
        std::cerr << "diagnostic queue retained=" << local_diagnostics.Events().size() << '\n';
    }
    ++frames;
  }
  if (result == 0 && project && project->workspace.Writable() &&
      !project->workspace.HasRecoveryJournal() &&
      (!gameplay_settings_invalid || initial_gameplay_library ||
       ui.GameplayLibrary() != loaded_gameplay_library)) {
    std::string error;
    if (!project->workspace.SaveGameplayLibrary(ui.GameplayLibrary(), &error))
      std::cerr << "gameplay settings were not saved: " << error << '\n';
  }
  if (project && project->workspace.Writable() &&
      !project->workspace.SaveEditorLayout(ui.SaveLayout(), &layout_error))
    std::cerr << layout_error << '\n';
  if (active_files() && active_files()->CurrentPath() && !scene_load_failed)
    retained_scene_views[*active_files()->CurrentPath()] = {
        ui.GetSceneOverviewCamera(), ui.GetNativeSceneOrbit(), overview_load_failed,
        preview_camera_load_failed};
  if (project && project->workspace.Writable())
    for (const auto &[relative, saved_views] : retained_scene_views) {
      nexora::editor::SceneFileSession scope(project->workspace, active_scene());
      const auto metadata = view_base(relative);
      if (!scope.BindCurrent(metadata) ||
          (saved_views.overview_failed && saved_views.preview_failed))
        continue;
      auto camera_path = project->workspace.Root() / metadata;
      camera_path.replace_extension(".overview.camera");
      std::error_code directory_error;
      std::filesystem::create_directories(camera_path.parent_path(), directory_error);
      if (directory_error) {
        std::cerr << "scene overview camera directory could not be created: "
                  << directory_error.message() << '\n';
      } else {
        const auto view = saved_views.overview;
        if (!saved_views.overview_failed) {
          nexora::editor::SceneCameraState camera;
          camera.transform.x = view.x;
          camera.transform.z = view.z;
          camera.orthographic = true;
          camera.orthographic_size = 320.0 / view.pixels_per_unit;
          std::string camera_error;
          if (!nexora::editor::CameraPersistence::Save(camera_path, camera, &camera_error))
            std::cerr << "scene overview camera could not be saved: " << camera_error << '\n';
        }
        if (!saved_views.preview_failed) {
          const auto orbit = saved_views.orbit;
          nexora::editor::SceneCameraState camera;
          camera.transform.x = view.x;
          camera.transform.y = orbit.target_y;
          camera.transform.z = view.z;
          camera.pitch = orbit.pitch;
          camera.yaw = orbit.yaw;
          camera.movement_speed = orbit.distance;
          std::string camera_error;
          auto preview_path = project->workspace.Root() / metadata;
          preview_path.replace_extension(".preview.camera");
          if (!nexora::editor::CameraPersistence::Save(preview_path, camera, &camera_error))
            std::cerr << "scene preview camera could not be saved: " << camera_error << '\n';
        }
      }
    }

  local_diagnostics.Set(false);
  std::cerr << "diagnostic privacy close: enabled=0 retained=0\n";
  static_cast<void>(static_export.Cancel());
  imports.Shutdown();
  scene_comparison.Shutdown();
  static_export.Shutdown();
  gameplay.Unload();
  if (play.State() != nexora::runtime::PlayState::Stopped)
    static_cast<void>(play.Stop());
  if (result == 0 && project && project->workspace.Writable() && scene_composition &&
      !composition_restore_blocked &&
      (composition_was_persisted || scene_documents->Snapshot().size() > 1)) {
    std::string error;
    if (!scene_composition->Save(&error))
      std::cerr << "scene set was not saved: " << error << '\n';
  }
  core_logs.Stop();
  console_ingress.Poll();
  const auto console_records = console.Snapshot();
  std::cerr << "console producer evidence: source=core_async_log forwarded="
            << console_ingress.ForwardedCount() << " retained=" << console_records.size()
            << " dropped=" << console.DroppedCount() << " gameplay_retained="
            << std::ranges::count(console_records, std::string("Gameplay"),
                                  &nexora::runtime::RuntimeLogRecord::category)
            << '\n';
  const auto diagnostics = created.surface->Diagnostics();
  const auto gpu_timing = profile.GpuTiming();
  std::cout << "GPU timing evidence: scope=native_command_buffer_interval unit=milliseconds source="
            << static_cast<unsigned>(gpu_timing.source)
            << " software=" << gpu_timing.software_rasterizer
            << " completed=" << gpu_timing.completed_submission
            << " retained=" << profile.GpuSamples().size()
            << " dropped=" << profile.GpuDroppedCount() << " latest=";
  if (gpu_timing.milliseconds)
    std::cout << *gpu_timing.milliseconds;
  else
    std::cout << "unavailable";
  std::cout << '\n';
  const auto process_memory = profile.ProcessMemory();
  std::cerr << "process memory evidence: scope=current_process_resident_set unit=bytes attempts="
            << process_memory.attempts << " successful=" << process_memory.successful_samples
            << " latest=";
  if (process_memory.resident_bytes)
    std::cerr << *process_memory.resident_bytes;
  else
    std::cerr << "unavailable";
  std::cerr << " observed_peak=";
  if (process_memory.observed_peak_bytes)
    std::cerr << *process_memory.observed_peak_bytes;
  else
    std::cerr << "unavailable";
  std::cerr << '\n';
  const auto remaining_documents = scene_documents
                                       ? scene_documents->Snapshot()
                                       : std::vector<nexora::editor::AdditiveDocumentView>{};
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
            << " scene_nodes=" << active_scene().Nodes().size()
            << " scene_selected=" << active_scene().Selection().size()
            << " scene_documents=" << remaining_documents.size() << " scene_references="
            << std::ranges::count(remaining_documents, false,
                                  &nexora::editor::AdditiveDocumentView::owned)
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
  std::optional<std::string> gameplay_library;
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
    else if (argument.starts_with("--gameplay-library="))
      gameplay_library = argument.substr(19);
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
                   "[--frames=N] [--recent-projects=PATH] [--native-scene-preview] "
                   "[--gameplay-library=RELATIVE_PATH]\n";
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
  if (gameplay_library && gameplay_library->size() >= 1024) {
    std::cerr << "--gameplay-library must be shorter than 1024 UTF-8 bytes\n";
    return 2;
  }
  if (gameplay_library && !graphical) {
    std::cerr << "--gameplay-library requires --graphical\n";
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
                        frame_limit, native_scene_preview, gameplay_library);
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
