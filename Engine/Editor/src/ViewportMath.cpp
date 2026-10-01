#include "Nexora/Editor/ViewportMath.h"

#include <algorithm>
#include <cmath>
#include <numbers>

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
    // Slab test. A zero direction component either always or never lies inside its slab.
    double t_near = 0.0;
    double t_far = std::numeric_limits<double>::infinity();
    bool hit = true;
    const double origin[3]{ray.origin.x, ray.origin.y, ray.origin.z};
    const double direction[3]{ray.direction.x, ray.direction.y, ray.direction.z};
    const double low[3]{candidate.min.x, candidate.min.y, candidate.min.z};
    const double high[3]{candidate.max.x, candidate.max.y, candidate.max.z};
    for (int axis = 0; axis < 3 && hit; ++axis) {
      if (direction[axis] == 0.0) {
        hit = origin[axis] >= low[axis] && origin[axis] <= high[axis];
        continue;
      }
      auto t0 = (low[axis] - origin[axis]) / direction[axis];
      auto t1 = (high[axis] - origin[axis]) / direction[axis];
      if (t0 > t1)
        std::swap(t0, t1);
      t_near = std::max(t_near, t0);
      t_far = std::min(t_far, t1);
      hit = t_near <= t_far;
    }
    if (!hit || t_near > max_distance)
      continue;
    if (!best || t_near < best->distance ||
        (t_near == best->distance && candidate.entity < best->entity)) {
      best = PickHit{candidate.entity, t_near};
    }
  }
  return best;
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
  return std::round(value / step) * step;
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

} // namespace nexora::editor
