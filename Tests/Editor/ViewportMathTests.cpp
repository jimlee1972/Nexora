#include "Nexora/Editor/SceneAuthoring.h"
#include "Nexora/Editor/ViewportMath.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <numbers>
#include <stdexcept>
#include <vector>

namespace {
using namespace nexora::editor;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

bool Near(double a, double b, double epsilon = 1e-9) { return std::abs(a - b) <= epsilon; }

ViewportCamera FrontCamera() {
  ViewportCamera camera;
  camera.position = {0.0, 0.0, 5.0};
  camera.target = {0.0, 0.0, 0.0};
  camera.vertical_fov_degrees = 90.0;
  return camera;
}

void TestPickRay() {
  const auto center = ViewportPickRay(FrontCamera(), 100.0, 100.0, 50.0, 50.0);
  Require(center && Near(center->direction.x, 0.0) && Near(center->direction.y, 0.0) &&
              Near(center->direction.z, -1.0) && Near(center->origin.z, 5.0),
          "the centre pixel must look straight down the view direction");

  // 90 degree FOV on a square viewport: the top-left corner is (-1, +1, -1) normalised, so the
  // image is not mirrored and +y pixels map to world down.
  const auto corner = ViewportPickRay(FrontCamera(), 100.0, 100.0, 0.0, 0.0);
  const auto inv = 1.0 / std::sqrt(3.0);
  Require(corner && Near(corner->direction.x, -inv) && Near(corner->direction.y, inv) &&
              Near(corner->direction.z, -inv),
          "the top-left pixel ray is mirrored or mis-scaled");
  const auto bottom_right = ViewportPickRay(FrontCamera(), 100.0, 100.0, 100.0, 100.0);
  Require(bottom_right && bottom_right->direction.x > 0.0 && bottom_right->direction.y < 0.0,
          "the bottom-right pixel ray points the wrong way");

  // A wider viewport widens the horizontal field, not the vertical one.
  const auto wide = ViewportPickRay(FrontCamera(), 200.0, 100.0, 0.0, 50.0);
  Require(wide && Near(wide->direction.x, -2.0 / std::sqrt(5.0)) && Near(wide->direction.y, 0.0),
          "aspect ratio did not widen the horizontal field of view");

  const auto nan = std::numeric_limits<double>::quiet_NaN();
  auto bad = FrontCamera();
  bad.target = bad.position;
  Require(!ViewportPickRay(bad, 100, 100, 50, 50), "camera looking at itself was accepted");
  bad = FrontCamera();
  bad.up = {0.0, 0.0, 1.0}; // parallel to the view direction
  Require(!ViewportPickRay(bad, 100, 100, 50, 50),
          "an up vector parallel to the view was accepted");
  bad = FrontCamera();
  bad.vertical_fov_degrees = 180.0;
  Require(!ViewportPickRay(bad, 100, 100, 50, 50), "a 180 degree field of view was accepted");
  bad = FrontCamera();
  bad.position.x = nan;
  Require(!ViewportPickRay(bad, 100, 100, 50, 50), "a NaN camera was accepted");
  Require(!ViewportPickRay(FrontCamera(), 0.0, 100.0, 0.0, 0.0), "a zero-width viewport was used");
  Require(!ViewportPickRay(FrontCamera(), 100.0, 100.0, 101.0, 50.0),
          "a pixel outside the viewport produced a ray");
  Require(!ViewportPickRay(FrontCamera(), 100.0, 100.0, nan, 50.0), "a NaN pixel produced a ray");
}

PickCandidate Box(std::uint64_t entity, double z_near, double z_far) {
  PickCandidate candidate;
  candidate.entity = entity;
  candidate.min = {-1.0, -1.0, z_near};
  candidate.max = {1.0, 1.0, z_far};
  return candidate;
}

void TestPicking() {
  const ViewportRay ray{{0.0, 0.0, 10.0}, {0.0, 0.0, -1.0}};
  std::vector<PickCandidate> candidates{Box(1, -5.0, -3.0), Box(2, 2.0, 4.0), Box(3, 6.0, 8.0)};
  auto hit = PickNearest(ray, candidates);
  Require(hit && hit->entity == 3 && Near(hit->distance, 2.0), "the nearest box was not picked");

  candidates[2].visible = false;
  hit = PickNearest(ray, candidates);
  Require(hit && hit->entity == 2 && Near(hit->distance, 6.0), "a hidden object was pickable");
  candidates[1].locked = true;
  hit = PickNearest(ray, candidates);
  Require(hit && hit->entity == 1, "a locked object was pickable");
  candidates[0].visible = false;
  Require(!PickNearest(ray, candidates), "picking with nothing eligible returned a hit");

  // Ray origin inside a box hits it immediately; boxes entirely behind the ray do not count.
  const ViewportRay inside{{0.0, 0.0, 3.0}, {0.0, 0.0, -1.0}};
  hit = PickNearest(inside, std::array{Box(7, 2.0, 4.0), Box(8, 6.0, 8.0)});
  Require(hit && hit->entity == 7 && Near(hit->distance, 0.0),
          "a ray starting inside a box must hit at distance zero");

  // Equal distances never depend on candidate order.
  hit = PickNearest(ray, std::array{Box(9, 2.0, 4.0), Box(4, 2.0, 4.0)});
  const auto swapped = PickNearest(ray, std::array{Box(4, 2.0, 4.0), Box(9, 2.0, 4.0)});
  Require(hit && swapped && hit->entity == 4 && swapped->entity == 4,
          "tied picks depend on candidate order");

  Require(!PickNearest(ray, std::array{Box(1, 2.0, 4.0)}, 5.0),
          "a hit beyond the maximum distance was returned");
  Require(PickNearest(ray, std::array{Box(1, 2.0, 4.0)}, 6.0).has_value(),
          "a hit within the maximum distance was dropped");

  // A ray parallel to a slab outside it misses; malformed boxes are ignored, not matched.
  const ViewportRay offside{{5.0, 0.0, 10.0}, {0.0, 0.0, -1.0}};
  Require(!PickNearest(offside, std::array{Box(1, 2.0, 4.0)}), "a parallel ray outside hit a box");
  auto inverted = Box(1, 2.0, 4.0);
  inverted.min.x = 2.0;
  inverted.max.x = -2.0;
  auto non_finite = Box(2, 2.0, 4.0);
  non_finite.max.y = std::numeric_limits<double>::infinity();
  Require(!PickNearest(ray, std::array{inverted, non_finite}), "a malformed box was picked");
  Require(!PickNearest(ViewportRay{{0.0, 0.0, 10.0}, {0.0, 0.0, 0.0}}, candidates),
          "a zero-direction ray picked something");
  Require(!PickNearest(ray, candidates, std::numeric_limits<double>::quiet_NaN()),
          "a NaN maximum distance picked something");

  // End to end: a pixel ray from the camera picks the box in front of it.
  const auto pixel_ray = ViewportPickRay(FrontCamera(), 100.0, 100.0, 50.0, 50.0);
  hit = PickNearest(*pixel_ray, std::array{Box(5, -1.0, 1.0)});
  Require(pixel_ray && hit && hit->entity == 5 && Near(hit->distance, 4.0),
          "pixel ray picking did not reach the box");
}

void TestAxisDrag() {
  const ViewportVector origin{0.0, 0.0, 0.0};
  const ViewportVector x_axis{1.0, 0.0, 0.0};
  const ViewportRay straight{{3.0, 0.0, 5.0}, {0.0, 0.0, -1.0}};
  auto distance = AxisDragDistance(straight, origin, x_axis);
  Require(distance && Near(*distance, 3.0), "dragging along +X measured the wrong distance");
  const ViewportRay skew{{2.0, 1.0, 5.0}, {0.0, 0.0, -1.0}};
  distance = AxisDragDistance(skew, origin, x_axis);
  Require(distance && Near(*distance, 2.0), "a skew ray did not project onto the axis");
  distance = AxisDragDistance(skew, origin, {-1.0, 0.0, 0.0});
  Require(distance && Near(*distance, -2.0), "a negated axis did not flip the sign");
  distance = AxisDragDistance(skew, {1.0, 0.0, 0.0}, {2.0, 0.0, 0.0}); // non-unit axis, shifted
  Require(distance && Near(*distance, 1.0), "axis direction was not normalised");

  Require(!AxisDragDistance(ViewportRay{{0.0, 1.0, 0.0}, {1.0, 0.0, 0.0}}, origin, x_axis),
          "a ray parallel to the axis produced a drag distance");
  Require(!AxisDragDistance(straight, origin, {0.0, 0.0, 0.0}), "a zero axis produced a distance");
  Require(!AxisDragDistance(straight, {std::nan(""), 0.0, 0.0}, x_axis),
          "a NaN axis origin produced a distance");
}

void TestSnapping() {
  Require(Near(SnapToStep(1.26, 0.25), 1.25), "snap to the nearest step failed");
  Require(Near(SnapToStep(-0.13, 0.25), -0.25), "negative snap failed");
  Require(Near(SnapToStep(0.125, 0.25), 0.25) && Near(SnapToStep(-0.125, 0.25), -0.25),
          "halves must round away from zero symmetrically");
  Require(SnapToStep(1.26, 0.0) == 1.26 && SnapToStep(1.26, -1.0) == 1.26 &&
              SnapToStep(1.26, std::nan("")) == 1.26,
          "a disabled step must leave the value unchanged");
  Require(std::isnan(SnapToStep(std::nan(""), 0.25)), "snapping must not launder NaN");
}

void TestResizeFilter() {
  ViewportResizeFilter filter{8, 3};
  Require(filter.Update(800, 600) && filter.Width() == 800 && filter.Height() == 600,
          "the first size must be adopted");
  Require(!filter.Update(800, 600), "an unchanged size reported a resize");
  Require(!filter.Update(0, 600) && filter.Width() == 800, "a zero-area size was adopted");

  // Panel jitter inside the dead band is ignored while it keeps changing...
  Require(!filter.Update(802, 600) && !filter.Update(799, 601) && !filter.Update(803, 598) &&
              filter.Width() == 800,
          "jitter reallocated the render target");
  // ...a large change is adopted at once...
  Require(filter.Update(900, 600) && filter.Width() == 900, "a large resize was delayed");
  // ...and a small but settled change is adopted after enough stable updates.
  Require(!filter.Update(903, 600) && !filter.Update(903, 600) && filter.Update(903, 600) &&
              filter.Width() == 903,
          "a settled small resize was never adopted");
  Require(!filter.Update(903, 600), "the adopted size reported another resize");
}
using nexora::runtime::Id;
using nexora::runtime::Transform;
constexpr double kHalfSqrt2 = 0.70710678118654752440;
constexpr double kPi = std::numbers::pi;

Transform Turned(double x, double y, double z, double qy, double qw) {
  Transform transform{x, y, z};
  transform.qy = qy;
  transform.qw = qw;
  return transform;
}

bool SamePose(const Transform &a, const Transform &b, double epsilon = 1e-9) {
  const double sign = a.qx * b.qx + a.qy * b.qy + a.qz * b.qz + a.qw * b.qw < 0.0 ? -1.0 : 1.0;
  return Near(a.x, b.x, epsilon) && Near(a.y, b.y, epsilon) && Near(a.z, b.z, epsilon) &&
         Near(a.qx, sign * b.qx, epsilon) && Near(a.qy, sign * b.qy, epsilon) &&
         Near(a.qz, sign * b.qz, epsilon) && Near(a.qw, sign * b.qw, epsilon) &&
         Near(a.sx, b.sx, epsilon) && Near(a.sy, b.sy, epsilon) && Near(a.sz, b.sz, epsilon);
}

void TestGizmoAxesAndDrags() {
  const auto world_axes = GizmoAxes(Turned(0, 0, 0, kHalfSqrt2, kHalfSqrt2), GizmoSpace::World);
  Require(world_axes[0].x == 1.0 && world_axes[1].y == 1.0 && world_axes[2].z == 1.0,
          "Global axes must be the world axes");
  auto mirrored = Turned(0, 0, 0, kHalfSqrt2, kHalfSqrt2); // 90 degrees about +Y
  mirrored.sx = -2.0;
  const auto local = GizmoAxes(mirrored, GizmoSpace::Local);
  Require(Near(local[0].x, 0.0) && Near(local[0].z, -1.0) && Near(local[1].y, 1.0) &&
              Near(local[2].x, 1.0),
          "Local axes must follow the world rotation and ignore a mirroring scale");

  // Looking down at the XZ plane: +X to -Z is a positive turn about +Y (right-hand rule).
  const ViewportRay from{{1.0, 5.0, 0.0}, {0.0, -1.0, 0.0}};
  const ViewportRay to{{0.0, 5.0, -1.0}, {0.0, -1.0, 0.0}};
  const ViewportVector up{0.0, 1.0, 0.0};
  const auto angle = RotationDragAngle(from, to, {}, up);
  Require(angle && Near(*angle, kPi / 2.0), "a quarter turn about +Y was mismeasured");
  Require(Near(*RotationDragAngle(to, from, {}, up), -kPi / 2.0) &&
              Near(*RotationDragAngle(from, to, {}, {0.0, -2.0, 0.0}), -kPi / 2.0),
          "the angle sign must follow the axis direction (right-hand rule)");
  Require(!RotationDragAngle(from, {{0.0, 5.0, 0.0}, {1.0, 0.0, 0.0}}, {}, up) &&
              !RotationDragAngle(from, {{0.0, 5.0, 0.0}, {0.0, 1.0, 0.0}}, {}, up) &&
              !RotationDragAngle(from, {{0.0, 5.0, 0.0}, {0.0, -1.0, 0.0}}, {}, up) &&
              !RotationDragAngle(from, to, {}, {0.0, 0.0, 0.0}),
          "a parallel ray, a hit behind the camera, a hit at the centre, or a zero axis must "
          "give no angle");

  Require(Near(*ScaleDragFactor(2.0, 3.0), 1.5) && Near(*ScaleDragFactor(-2.0, -1.0), 0.5),
          "the scale factor is the ratio of handle distances");
  Require(*ScaleDragFactor(2.0, -1.0) == kMinGizmoScaleFactor &&
              *ScaleDragFactor(2.0, 0.0) == kMinGizmoScaleFactor,
          "a scale drag must never cross zero (it cannot create or remove a mirror)");
  Require(!ScaleDragFactor(0.0, 1.0) && !ScaleDragFactor(1.0, std::nan("")),
          "a degenerate scale drag must give no factor");
}

GizmoOperation Rotation(double angle, GizmoPivot pivot, ViewportVector center = {}) {
  GizmoOperation operation;
  operation.kind = GizmoOperation::Kind::Rotate;
  operation.axis = {0.0, 1.0, 0.0};
  operation.angle = angle;
  operation.pivot = pivot;
  operation.center = center;
  return operation;
}

void TestApplyGizmo() {
  using Kind = GizmoOperation::Kind;
  // Parent at (10, 0, 0), turned 90 degrees about +Y, uniform scale 2; child at local (1, 0, 0),
  // which is world (10, 0, -2).
  auto parent_world = Turned(10.0, 0.0, 0.0, kHalfSqrt2, kHalfSqrt2);
  parent_world.sx = parent_world.sy = parent_world.sz = 2.0;
  const GizmoTarget child{2, Transform{1.0, 0.0, 0.0}, parent_world};
  const GizmoTarget root{3, Turned(3.0, 0.0, 0.0, 0.0, 1.0), {}};

  GizmoOperation move;
  move.kind = Kind::Translate;
  move.translation = {0.0, 0.0, -2.0};
  const std::array<GizmoTarget, 2> both{child, root};
  auto result = ApplyGizmo(both, move);
  Require(result && SamePose((*result)[0], Transform{2.0, 0.0, 0.0}) &&
              SamePose((*result)[1], Transform{3.0, 0.0, -2.0}),
          "a world-space move must be re-expressed in each target's parent space");
  Require((*result)[0].qw == 1.0 && (*result)[0].sx == 1.0,
          "a move must keep the local rotation and scale exactly");

  // Pivot: each entity turns in place.
  result = ApplyGizmo(std::span(&root, 1), Rotation(kPi / 2.0, GizmoPivot::Pivot));
  Require(result && (*result)[0].x == 3.0 && (*result)[0].z == 0.0 &&
              SamePose((*result)[0], Turned(3.0, 0.0, 0.0, kHalfSqrt2, kHalfSqrt2)),
          "a Pivot rotation must turn the entity about its own origin");
  // Under a turned parent the world rotation is turn * world, expressed back in the parent.
  result = ApplyGizmo(std::span(&child, 1), Rotation(kPi / 2.0, GizmoPivot::Pivot));
  const auto child_world = nexora::runtime::ComposeTransforms(parent_world, (*result)[0]);
  auto expected_world = Turned(10.0, 0.0, -2.0, 1.0, 0.0); // 180 degrees about +Y
  expected_world.sx = expected_world.sy = expected_world.sz = 2.0;
  Require(result && SamePose(child_world, expected_world) && (*result)[0].x == 1.0,
          "a child's rotation must be applied in world space and stored relative to its parent");

  // The turn is about a world axis, applied after the existing rotation: an entity already turned
  // 90 degrees about +Y, then turned 90 degrees about world +X, has its local X axis pointing +Y.
  const GizmoTarget turned{8, Turned(0.0, 0.0, 0.0, kHalfSqrt2, kHalfSqrt2), {}};
  auto about_x = Rotation(kPi / 2.0, GizmoPivot::Pivot);
  about_x.axis = {1.0, 0.0, 0.0};
  result = ApplyGizmo(std::span(&turned, 1), about_x);
  const auto x_axis = GizmoAxes((*result)[0], GizmoSpace::Local)[0];
  Require(result && Near(x_axis.x, 0.0) && Near(x_axis.y, 1.0) && Near(x_axis.z, 0.0),
          "a gizmo rotation must be about the world axis, after the existing rotation");

  // Center: the selection turns about its mean position.
  const std::array<GizmoTarget, 2> pair{GizmoTarget{4, Transform{1.0, 0.0, 0.0}, {}},
                                        GizmoTarget{5, Transform{3.0, 0.0, 0.0}, {}}};
  result = ApplyGizmo(pair, Rotation(kPi, GizmoPivot::Center, {2.0, 0.0, 0.0}));
  Require(result && SamePose((*result)[0], Turned(3.0, 0.0, 0.0, 1.0, 0.0)) &&
              SamePose((*result)[1], Turned(1.0, 0.0, 0.0, 1.0, 0.0)),
          "a Center rotation must swing the selection about its centre");

  // Scale: factors multiply local scale, keep a mirror, and with Center move the offsets.
  auto mirrored = Transform{3.0, 1.0, 0.0};
  mirrored.sx = -2.0;
  GizmoOperation grow;
  grow.kind = Kind::Scale;
  grow.factors = {1.5, 1.0, 1.0};
  const GizmoTarget mirrored_target{6, mirrored, {}};
  result = ApplyGizmo(std::span(&mirrored_target, 1), grow);
  Require(result && (*result)[0].sx == -3.0 && (*result)[0].sy == 1.0 && (*result)[0].x == 3.0,
          "a Pivot scale must multiply the local scale, keep the mirror, and not move");
  grow.pivot = GizmoPivot::Center;
  grow.center = {2.0, 0.0, 0.0};
  grow.factors = {2.0, 1.0, 1.0};
  result = ApplyGizmo(std::span(&mirrored_target, 1), grow);
  Require(result && Near((*result)[0].x, 4.0) && Near((*result)[0].y, 1.0) &&
              (*result)[0].sx == -4.0,
          "a Center scale must scale the offset from the centre along the gizmo axes");
  grow.factors = {2.0, 2.0, 2.0};
  result = ApplyGizmo(pair, grow);
  Require(result && Near((*result)[0].x, 0.0) && Near((*result)[1].x, 4.0) &&
              (*result)[0].sx == 2.0 && (*result)[1].sz == 2.0,
          "a uniform Center scale must spread the selection from its centre");

  // Malformed operations and unrepresentable results reject the frame.
  grow.factors = {0.0, 1.0, 1.0};
  auto zero_axis = Rotation(1.0, GizmoPivot::Pivot);
  zero_axis.axis = {};
  auto runaway = move;
  runaway.translation = {1e308, 0.0, 0.0};
  const GizmoTarget far{7, Transform{1e308, 0.0, 0.0}, {}};
  auto skewed = grow;
  skewed.factors = {2.0, 1.0, 1.0};
  skewed.axes = {{{1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
  Require(!ApplyGizmo(pair, skewed), "Center scaling along non-orthogonal axes must be rejected");
  Require(!ApplyGizmo(pair, grow) && !ApplyGizmo(pair, zero_axis) &&
              !ApplyGizmo(std::span(&far, 1), runaway),
          "a zero factor, a zero axis, or an overflowing move must be rejected");
}

void TestGizmoSelection() {
  nexora::runtime::World world;
  const auto scene = world.LoadScene("Gizmo");
  const auto create = [&](Transform transform) {
    auto &entity = world.CreateEntity(scene);
    entity.transform = transform;
    return entity.id;
  };
  auto parent_pose = Turned(10.0, 0.0, 0.0, kHalfSqrt2, kHalfSqrt2);
  parent_pose.sx = parent_pose.sy = parent_pose.sz = 2.0;
  const auto parent = create(parent_pose);
  const auto child = create(Transform{1.0, 0.0, 0.0});
  const auto grandchild = create(Transform{0.0, 1.0, 0.0});
  const auto other = create(Transform{-4.0, 0.0, 0.0});
  nexora::runtime::WorldCommandBuffer hierarchy;
  hierarchy.SetParent(child, parent, false);
  hierarchy.SetParent(grandchild, child, false);
  Require(hierarchy.Apply(world), "hierarchy setup failed");

  const std::vector<Id> selection{grandchild, parent, other, grandchild, 999'999};
  Require((GizmoRoots(world, selection) == std::vector<Id>{parent, other}),
          "an entity whose ancestor is selected must not be moved twice");
  const std::vector<Id> children{child, grandchild};
  Require((GizmoRoots(world, children) == std::vector<Id>{child}), "a grandchild was moved twice");

  const auto center = SelectionCenter(world, std::vector<Id>{parent, other});
  Require(center && Near(center->x, 3.0) && Near(center->z, 0.0), "the selection centre is wrong");
  Require(!SelectionCenter(world, std::vector<Id>{}) &&
              !SelectionCenter(world, std::vector<Id>{999'999}) &&
              !GizmoTargets(world, std::vector<Id>{child, 999'999}),
          "an empty selection or a missing entity must give no centre or targets");
  const auto targets = GizmoTargets(world, std::vector<Id>{grandchild});
  Require(targets && SamePose(targets->front().parent_world, *world.WorldTransform(child)),
          "a target must carry its parent's world transform");

  // One gesture: capture at Begin, apply the whole delta from the start every frame, cancel
  // restores exactly, commit keeps the result; the child follows its moved parent.
  const auto read = [&](Id id, Transform &out) {
    const auto *entity = world.FindEntity(id);
    if (!entity)
      return false;
    out = entity->transform;
    return true;
  };
  const auto apply = [&](Id id, Transform transform) {
    nexora::runtime::WorldCommandBuffer commands;
    commands.SetTransform(id, transform);
    return commands.Apply(world);
  };
  const auto roots = GizmoRoots(world, std::vector<Id>{parent, child, other});
  const auto captured = GizmoTargets(world, roots);
  const auto before = *world.WorldTransform(grandchild);
  GizmoTransaction gesture;
  Require(captured && gesture.Begin(roots, read), "the gesture failed to begin");
  for (const double degrees : {10.0, 45.0, 90.0}) {
    auto turn = Rotation(SnapToStep(degrees, 15.0) * kPi / 180.0, GizmoPivot::Pivot);
    const auto frame = ApplyGizmo(*captured, turn);
    Require(frame && gesture.Update(*frame, apply), "a gesture frame was rejected");
  }
  Require(!SamePose(*world.WorldTransform(grandchild), before) && gesture.Cancel(apply) &&
              SamePose(*world.WorldTransform(grandchild), before, 0.0),
          "cancelling a gesture must restore every entity exactly");
  Require(gesture.Begin(roots, read), "the second gesture failed to begin");
  const auto frame = ApplyGizmo(*captured, Rotation(kPi / 2.0, GizmoPivot::Pivot));
  Require(frame && gesture.Update(*frame, apply) && gesture.Commit(),
          "the second gesture failed to commit");
  Require(SamePose(*world.WorldTransform(grandchild),
                   nexora::runtime::ComposeTransforms(
                       *world.WorldTransform(parent),
                       nexora::runtime::ComposeTransforms(Transform{1.0, 0.0, 0.0},
                                                          Transform{0.0, 1.0, 0.0}))) &&
              world.FindEntity(child)->transform == Transform{1.0, 0.0, 0.0},
          "descendants must follow their rotated ancestor without being edited");
}
} // namespace

int main() {
  try {
    TestPickRay();
    TestPicking();
    TestAxisDrag();
    TestSnapping();
    TestResizeFilter();
    TestGizmoAxesAndDrags();
    TestApplyGizmo();
    TestGizmoSelection();
    std::cout << "Editor viewport math contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
