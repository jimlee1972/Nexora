#pragma once

#include "Nexora/Runtime/Runtime.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <numbers>
#include <optional>

namespace nexora::editor {

using EulerDegrees = std::array<double, 3>;

// Unity-style extrinsic Z-X-Y rotation: q = qY * qX * qZ. Runtime storage remains quaternion.
[[nodiscard]] inline std::optional<runtime::Transform>
WithEulerDegrees(runtime::Transform transform, const EulerDegrees &degrees) {
  if (!std::ranges::all_of(degrees, [](double value) { return std::isfinite(value); }))
    return std::nullopt;
  constexpr double half_radians = std::numbers::pi / 360.0;
  const auto x = std::remainder(degrees[0], 360.0) * half_radians;
  const auto y = std::remainder(degrees[1], 360.0) * half_radians;
  const auto z = std::remainder(degrees[2], 360.0) * half_radians;
  const auto sx = std::sin(x), cx = std::cos(x);
  const auto sy = std::sin(y), cy = std::cos(y);
  const auto sz = std::sin(z), cz = std::cos(z);
  transform.qx = cy * sx * cz + sy * cx * sz;
  transform.qy = sy * cx * cz - cy * sx * sz;
  transform.qz = cy * cx * sz - sy * sx * cz;
  transform.qw = cy * cx * cz + sy * sx * sz;
  return runtime::NormalizedTransform(transform);
}

[[nodiscard]] inline std::optional<EulerDegrees>
ToEulerDegrees(const runtime::Transform &transform) {
  const auto normalized = runtime::NormalizedTransform(transform);
  if (!normalized)
    return std::nullopt;
  const auto &q = *normalized;
  const double sine_x = std::clamp(2.0 * (q.qw * q.qx - q.qy * q.qz), -1.0, 1.0);
  const double cosine_x =
      std::hypot(2.0 * (q.qx * q.qz + q.qw * q.qy), 1.0 - 2.0 * (q.qx * q.qx + q.qy * q.qy));
  double x = std::atan2(sine_x, cosine_x);
  double y{}, z{};
  if (cosine_x > 1e-10) {
    y = std::atan2(2.0 * (q.qx * q.qz + q.qw * q.qy), 1.0 - 2.0 * (q.qx * q.qx + q.qy * q.qy));
    z = std::atan2(2.0 * (q.qx * q.qy + q.qw * q.qz), 1.0 - 2.0 * (q.qx * q.qx + q.qz * q.qz));
  } else {
    // At gimbal lock choose Z=0 and put the combined rotation in Y.
    y = std::atan2(2.0 * (q.qw * q.qy - q.qx * q.qz), 1.0 - 2.0 * (q.qy * q.qy + q.qz * q.qz));
  }
  constexpr double to_degrees = 180.0 / std::numbers::pi;
  return EulerDegrees{x * to_degrees, y * to_degrees, z * to_degrees};
}

[[nodiscard]] inline bool SameRotation(const runtime::Transform &a, const runtime::Transform &b) {
  const auto first = runtime::NormalizedTransform(a);
  const auto second = runtime::NormalizedTransform(b);
  if (!first || !second)
    return false;
  const double dot = first->qx * second->qx + first->qy * second->qy + first->qz * second->qz +
                     first->qw * second->qw;
  const double sign = dot < 0.0 ? -1.0 : 1.0;
  return std::abs(first->qx - sign * second->qx) < 1e-10 &&
         std::abs(first->qy - sign * second->qy) < 1e-10 &&
         std::abs(first->qz - sign * second->qz) < 1e-10 &&
         std::abs(first->qw - sign * second->qw) < 1e-10;
}

} // namespace nexora::editor
