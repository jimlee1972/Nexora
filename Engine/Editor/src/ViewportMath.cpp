#include "Nexora/Editor/ViewportMath.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <unordered_set>

namespace nexora::editor {
namespace {

using Vec = ViewportVector;

bool Finite(const Vec &v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
Vec Sub(const Vec &a, const Vec &b) { return {a.x - b.x, a.y - b.y, a.z - b.z}; }
Vec Add(const Vec &a, const Vec &b) { return {a.x + b.x, a.y + b.y, a.z + b.z}; }
Vec Scale(const Vec &a, double s) { return {a.x * s, a.y * s, a.z * s}; }
double Dot(const Vec &a, const Vec &b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec Cross(const Vec &a, const Vec &b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
double Length(const Vec &a) { return std::sqrt(Dot(a, a)); }

// Smallest direction length that still counts as a direction; guards division by ~zero.
constexpr double kMinLength = 1e-12;

std::optional<Vec> Normalized(const Vec &a) {
  const auto length = Length(a);
  if (!std::isfinite(length) || length < kMinLength)
    return std::nullopt;
  return Scale(a, 1.0 / length);
}

struct Quat final {
  double x{}, y{}, z{}, w{1.0};
};
Quat RotationOf(const runtime::Transform &t) { return {t.qx, t.qy, t.qz, t.qw}; }
Quat Multiply(const Quat &a, const Quat &b) {
  return {
      a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y, a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
      a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w, a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}
Vec Rotate(const Quat &q, const Vec &v) {
  // v' = v + 2w(u x v) + 2u x (u x v), with u the vector part of the unit quaternion.
  const Vec u{q.x, q.y, q.z};
  const auto t = Scale(Cross(u, v), 2.0);
  return Add(Add(v, Scale(t, q.w)), Cross(u, t));
}
std::optional<Quat> Normalized(const Quat &q) {
  const auto length = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  if (!std::isfinite(length) || length < kMinLength)
    return std::nullopt;
  return Quat{q.x / length, q.y / length, q.z / length, q.w / length};
}
Vec PositionOf(const runtime::Transform &t) { return {t.x, t.y, t.z}; }

// Where the ray meets the plane through `point` with unit normal `normal`; nullopt when the ray is
// (nearly) parallel to it or meets it behind its origin.
std::optional<Vec> PlaneHit(const ViewportRay &ray, const Vec &point, const Vec &normal) {
  const auto direction = Normalized(ray.direction);
  if (!direction || !Finite(ray.origin))
    return std::nullopt;
  const auto facing = Dot(*direction, normal);
  if (std::abs(facing) < 1e-6)
    return std::nullopt;
  const auto t = Dot(Sub(point, ray.origin), normal) / facing;
  if (!std::isfinite(t) || t < 0.0)
    return std::nullopt;
  return Add(ray.origin, Scale(*direction, t));
}

std::optional<double> SlabDistance(const Vec &origin, const Vec &direction, const Vec &low,
                                   const Vec &high, double max_distance) {
  double t_near = 0.0;
  double t_far = std::numeric_limits<double>::infinity();
  const double starts[3]{origin.x, origin.y, origin.z};
  const double rays[3]{direction.x, direction.y, direction.z};
  const double minimum[3]{low.x, low.y, low.z};
  const double maximum[3]{high.x, high.y, high.z};
  for (std::size_t axis = 0; axis < 3; ++axis) {
    if (rays[axis] == 0.0) {
      if (starts[axis] < minimum[axis] || starts[axis] > maximum[axis])
        return std::nullopt;
      continue;
    }
    auto t0 = (minimum[axis] - starts[axis]) / rays[axis];
    auto t1 = (maximum[axis] - starts[axis]) / rays[axis];
    if (t0 > t1)
      std::swap(t0, t1);
    t_near = std::max(t_near, t0);
    t_far = std::min(t_far, t1);
    if (t_near > t_far)
      return std::nullopt;
  }
  return std::isfinite(t_near) && t_near <= max_distance ? std::optional{t_near} : std::nullopt;
}

} // namespace

std::optional<ViewportRay> ViewportPickRay(const ViewportCamera &camera, double viewport_width,
                                           double viewport_height, double pixel_x, double pixel_y) {
  if (!Finite(camera.position) || !Finite(camera.target) || !Finite(camera.up) ||
      !std::isfinite(viewport_width) || !std::isfinite(viewport_height) ||
      !std::isfinite(pixel_x) || !std::isfinite(pixel_y) || !(viewport_width > 0.0) ||
      !(viewport_height > 0.0) || pixel_x < 0.0 || pixel_y < 0.0 || pixel_x > viewport_width ||
      pixel_y > viewport_height || !(camera.vertical_fov_degrees > 0.0) ||
      !(camera.vertical_fov_degrees < 180.0)) {
    return std::nullopt;
  }
  const auto forward = Normalized(Sub(camera.target, camera.position));
  if (!forward)
    return std::nullopt;
  const auto right = Normalized(Cross(*forward, camera.up));
  if (!right)
    return std::nullopt;
  const auto up = Cross(*right, *forward);

  const auto half_height = std::tan(camera.vertical_fov_degrees * 0.5 * std::numbers::pi / 180.0);
  const auto half_width = half_height * (viewport_width / viewport_height);
  const auto ndc_x = pixel_x / viewport_width * 2.0 - 1.0;
  const auto ndc_y = 1.0 - pixel_y / viewport_height * 2.0;
  const auto direction = Normalized(
      Add(*forward, Add(Scale(*right, ndc_x * half_width), Scale(up, ndc_y * half_height))));
  if (!direction)
    return std::nullopt;
  return ViewportRay{camera.position, *direction};
}

std::optional<PickHit> PickNearest(const ViewportRay &ray,
                                   std::span<const PickCandidate> candidates, double max_distance) {
  if (!Finite(ray.origin) || !Finite(ray.direction) || std::isnan(max_distance) ||
      max_distance < 0.0 || Length(ray.direction) < kMinLength) {
    return std::nullopt;
  }
  std::optional<PickHit> best;
  for (const auto &candidate : candidates) {
    if (!candidate.visible || candidate.locked || !Finite(candidate.min) ||
        !Finite(candidate.max) || candidate.min.x > candidate.max.x ||
        candidate.min.y > candidate.max.y || candidate.min.z > candidate.max.z) {
      continue;
    }
    const auto distance =
        SlabDistance(ray.origin, ray.direction, candidate.min, candidate.max, max_distance);
    if (!distance)
      continue;
    if (!best || *distance < best->distance ||
        (*distance == best->distance && candidate.entity < best->entity)) {
      best = PickHit{candidate.entity, *distance};
    }
  }
  return best;
}

std::optional<double> PickOrientedBox(const ViewportRay &ray, const ViewportVector &center,
                                      const ViewportVector &half_extents,
                                      const std::array<double, 4> &rotation, double max_distance) {
  if (!Finite(ray.origin) || !Finite(ray.direction) || !Finite(center) || !Finite(half_extents) ||
      !(half_extents.x > 0.0) || !(half_extents.y > 0.0) || !(half_extents.z > 0.0) ||
      std::isnan(max_distance) || max_distance < 0.0)
    return std::nullopt;
  const auto direction = Normalized(ray.direction);
  const Quat q{rotation[0], rotation[1], rotation[2], rotation[3]};
  const double length_squared = q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w;
  if (!direction || !std::isfinite(length_squared) || std::abs(length_squared - 1.0) > 0.01)
    return std::nullopt;
  const auto unit = *Normalized(q);
  const Quat inverse{-unit.x, -unit.y, -unit.z, unit.w};
  const auto local_origin = Rotate(inverse, Sub(ray.origin, center));
  const auto local_direction = Rotate(inverse, *direction);
  if (!Finite(local_origin) || !Finite(local_direction))
    return std::nullopt;
  return SlabDistance(local_origin, local_direction,
                      {-half_extents.x, -half_extents.y, -half_extents.z}, half_extents,
                      max_distance);
}

std::optional<double> AxisDragDistance(const ViewportRay &ray, const ViewportVector &axis_origin,
                                       const ViewportVector &axis_direction) {
  if (!Finite(ray.origin) || !Finite(ray.direction) || !Finite(axis_origin) ||
      !Finite(axis_direction)) {
    return std::nullopt;
  }
  const auto axis = Normalized(axis_direction);
  const auto view = Normalized(ray.direction);
  if (!axis || !view)
    return std::nullopt;
  // Closest points between two lines: ray (o1 + s*d1) and axis (o2 + t*d2), unit directions.
  const auto offset = Sub(ray.origin, axis_origin);
  const auto b = Dot(*view, *axis);
  const auto denominator = 1.0 - b * b;
  if (denominator < 1e-9) // parallel: the closest point is not unique
    return std::nullopt;
  const auto d = Dot(*view, offset);
  const auto e = Dot(*axis, offset);
  const auto t = (e - b * d) / denominator;
  if (!std::isfinite(t))
    return std::nullopt;
  return t;
}

double SnapToStep(double value, double step) noexcept {
  if (!std::isfinite(value) || !std::isfinite(step) || !(step > 0.0))
    return value;
  const auto quotient = value / step;
  if (!std::isfinite(quotient))
    return value;
  const auto snapped = std::round(quotient) * step;
  return std::isfinite(snapped) ? snapped : value;
}

bool ViewportResizeFilter::Update(std::uint32_t width, std::uint32_t height) noexcept {
  if (width == 0 || height == 0)
    return false;
  if (width_ == 0 || height_ == 0) {
    width_ = width;
    height_ = height;
    pending_count_ = 0;
    return true;
  }
  if (width == width_ && height == height_) {
    pending_count_ = 0;
    return false;
  }
  const auto delta = [](std::uint32_t a, std::uint32_t b) { return a > b ? a - b : b - a; };
  if (delta(width, width_) >= hysteresis_ || delta(height, height_) >= hysteresis_) {
    width_ = width;
    height_ = height;
    pending_count_ = 0;
    return true;
  }
  // Within the dead band: only adopt it once the request has stopped changing.
  if (width == pending_width_ && height == pending_height_) {
    ++pending_count_;
  } else {
    pending_width_ = width;
    pending_height_ = height;
    pending_count_ = 1;
  }
  if (pending_count_ >= stable_frames_) {
    width_ = width;
    height_ = height;
    pending_count_ = 0;
    return true;
  }
  return false;
}

std::array<ViewportVector, 3> GizmoAxes(const runtime::Transform &world,
                                        GizmoSpace space) noexcept {
  std::array<ViewportVector, 3> axes{{{1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {0.0, 0.0, 1.0}}};
  if (space == GizmoSpace::World)
    return axes;
  const auto rotation = Normalized(RotationOf(world));
  if (!rotation)
    return axes;
  for (auto &axis : axes)
    axis = Rotate(*rotation, axis);
  return axes;
}

std::optional<double> RotationDragAngle(const ViewportRay &start, const ViewportRay &current,
                                        const ViewportVector &center, const ViewportVector &axis) {
  const auto normal = Normalized(axis);
  if (!normal || !Finite(center))
    return std::nullopt;
  const auto from = PlaneHit(start, center, *normal);
  const auto to = PlaneHit(current, center, *normal);
  if (!from || !to)
    return std::nullopt;
  const auto a = Sub(*from, center);
  const auto b = Sub(*to, center);
  // Relative to how far the camera is from the plane, a hit this close to the centre has no
  // meaningful direction.
  const auto scale =
      std::max({1.0, Length(Sub(start.origin, center)), Length(Sub(current.origin, center))});
  if (Length(a) < 1e-9 * scale || Length(b) < 1e-9 * scale)
    return std::nullopt;
  const auto angle = std::atan2(Dot(*normal, Cross(a, b)), Dot(a, b));
  if (!std::isfinite(angle))
    return std::nullopt;
  return angle;
}

std::optional<double> ScaleDragFactor(double start_distance, double current_distance) {
  if (!std::isfinite(start_distance) || !std::isfinite(current_distance) ||
      std::abs(start_distance) < kMinLength)
    return std::nullopt;
  const auto factor = current_distance / start_distance;
  if (!std::isfinite(factor))
    return std::nullopt;
  return std::max(factor, kMinGizmoScaleFactor);
}

std::optional<std::vector<runtime::Transform>> ApplyGizmo(std::span<const GizmoTarget> targets,
                                                          const GizmoOperation &operation) {
  using Kind = GizmoOperation::Kind;
  const bool center = operation.pivot == GizmoPivot::Center;
  if (center && !Finite(operation.center))
    return std::nullopt;
  std::optional<Quat> turn;
  std::array<Vec, 3> axes{};
  if (operation.kind == Kind::Translate) {
    if (!Finite(operation.translation))
      return std::nullopt;
  } else if (operation.kind == Kind::Rotate) {
    const auto axis = Normalized(operation.axis);
    if (!axis || !std::isfinite(operation.angle))
      return std::nullopt;
    const auto half = operation.angle * 0.5;
    turn = Quat{axis->x * std::sin(half), axis->y * std::sin(half), axis->z * std::sin(half),
                std::cos(half)};
  } else {
    const auto &f = operation.factors;
    if (!Finite(f) || !(f.x > 0.0) || !(f.y > 0.0) || !(f.z > 0.0))
      return std::nullopt;
    if (center) {
      for (std::size_t index = 0; index < 3; ++index) {
        const auto axis = Normalized(operation.axes[index]);
        if (!axis)
          return std::nullopt;
        axes[index] = *axis;
      }
      // Offsets are split along the axes and rebuilt, which is only exact for orthogonal axes.
      if (std::abs(Dot(axes[0], axes[1])) > 1e-6 || std::abs(Dot(axes[0], axes[2])) > 1e-6 ||
          std::abs(Dot(axes[1], axes[2])) > 1e-6)
        return std::nullopt;
    }
  }

  std::vector<runtime::Transform> results;
  results.reserve(targets.size());
  for (const auto &target : targets) {
    const auto world = runtime::ComposeTransforms(target.parent_world, target.local);
    auto moved = world;
    if (operation.kind == Kind::Translate) {
      const auto position = Add(PositionOf(world), operation.translation);
      moved = runtime::WithPosition(world, position.x, position.y, position.z);
    } else if (operation.kind == Kind::Rotate) {
      const auto rotation = Normalized(Multiply(*turn, RotationOf(world)));
      if (!rotation)
        return std::nullopt;
      moved.qx = rotation->x;
      moved.qy = rotation->y;
      moved.qz = rotation->z;
      moved.qw = rotation->w;
      if (center) {
        const auto position =
            Add(operation.center, Rotate(*turn, Sub(PositionOf(world), operation.center)));
        moved = runtime::WithPosition(moved, position.x, position.y, position.z);
      }
    } else if (center) {
      // Scale the offset from the centre along the gizmo axes.
      const auto offset = Sub(PositionOf(world), operation.center);
      const double factors[3]{operation.factors.x, operation.factors.y, operation.factors.z};
      auto position = operation.center;
      for (std::size_t index = 0; index < 3; ++index)
        position = Add(position, Scale(axes[index], Dot(offset, axes[index]) * factors[index]));
      moved = runtime::WithPosition(world, position.x, position.y, position.z);
    }
    // Back into the parent's space, taking from the result only what this operation changes and
    // keeping every other part exactly, so a round trip through the parent cannot drift it.
    auto local = runtime::RelativeTransform(target.parent_world, moved);
    // Rotating or scaling about each entity's own origin leaves its position where it was.
    if (operation.kind != Kind::Translate && !center)
      local = runtime::WithPosition(local, target.local.x, target.local.y, target.local.z);
    if (operation.kind != Kind::Rotate) {
      local.qx = target.local.qx;
      local.qy = target.local.qy;
      local.qz = target.local.qz;
      local.qw = target.local.qw;
    }
    local.sx = target.local.sx;
    local.sy = target.local.sy;
    local.sz = target.local.sz;
    if (operation.kind == Kind::Scale) {
      local.sx *= operation.factors.x;
      local.sy *= operation.factors.y;
      local.sz *= operation.factors.z;
    }
    const auto normalized = runtime::NormalizedTransform(local);
    if (!normalized)
      return std::nullopt;
    results.push_back(*normalized);
  }
  return results;
}

std::vector<runtime::Id> GizmoRoots(const runtime::World &world,
                                    std::span<const runtime::Id> selection) {
  const std::unordered_set<runtime::Id> selected(selection.begin(), selection.end());
  std::unordered_set<runtime::Id> seen;
  std::vector<runtime::Id> roots;
  for (const auto id : selection) {
    if (!world.FindEntity(id) || !seen.insert(id).second)
      continue;
    bool covered = false;
    // Bounded like the runtime's own ancestor walks, in case of a corrupted hierarchy.
    std::size_t steps = 0;
    for (auto parent = world.Parent(id).value_or(0); parent != 0 && steps < 1'000'000;
         parent = world.Parent(parent).value_or(0), ++steps)
      if (selected.contains(parent)) {
        covered = true;
        break;
      }
    if (!covered)
      roots.push_back(id);
  }
  return roots;
}

std::optional<std::vector<GizmoTarget>> GizmoTargets(const runtime::World &world,
                                                     std::span<const runtime::Id> entities) {
  std::vector<GizmoTarget> targets;
  targets.reserve(entities.size());
  for (const auto id : entities) {
    const auto *entity = world.FindEntity(id);
    if (!entity)
      return std::nullopt;
    GizmoTarget target{id, entity->transform, {}};
    if (entity->parent != 0) {
      const auto parent = world.WorldTransform(entity->parent);
      if (!parent)
        return std::nullopt;
      target.parent_world = *parent;
    }
    targets.push_back(target);
  }
  return targets;
}

std::optional<ViewportVector> SelectionCenter(const runtime::World &world,
                                              std::span<const runtime::Id> entities) {
  if (entities.empty())
    return std::nullopt;
  // Each position is divided before it is added, so the partial sums stay within the largest
  // coordinate and never overflow even when the positions are near the limit of a double.
  const auto weight = 1.0 / static_cast<double>(entities.size());
  Vec mean{};
  for (const auto id : entities) {
    const auto transform = world.WorldTransform(id);
    if (!transform)
      return std::nullopt;
    mean = Add(mean, Scale(PositionOf(*transform), weight));
  }
  if (!Finite(mean))
    return std::nullopt;
  return mean;
}

} // namespace nexora::editor
