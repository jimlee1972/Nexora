#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>

namespace nexora::math {

inline constexpr float kEpsilon = 1.0e-6F;
inline constexpr float kPi = 3.14159265358979323846F;
[[nodiscard]] constexpr float Radians(float degrees) noexcept { return degrees * kPi / 180.0F; }
[[nodiscard]] constexpr float Degrees(float radians) noexcept { return radians * 180.0F / kPi; }
[[nodiscard]] constexpr bool NearlyEqual(float a, float b, float epsilon = kEpsilon) noexcept {
  return (a > b ? a - b : b - a) <= epsilon;
}

struct Vector2 final {
  float x{}, y{};
};
struct Vector3 final {
  float x{}, y{}, z{};
  friend constexpr Vector3 operator+(Vector3 a, Vector3 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z};
  }
  friend constexpr Vector3 operator-(Vector3 a, Vector3 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z};
  }
  friend constexpr Vector3 operator*(Vector3 a, float s) { return {a.x * s, a.y * s, a.z * s}; }
};
struct Vector4 final {
  float x{}, y{}, z{}, w{};
};
[[nodiscard]] constexpr float Dot(Vector3 a, Vector3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
[[nodiscard]] constexpr Vector3 Cross(Vector3 a, Vector3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
[[nodiscard]] inline float Length(Vector3 value) { return std::sqrt(Dot(value, value)); }
[[nodiscard]] inline Vector3 NormalizeSafe(Vector3 value, Vector3 fallback = {}) {
  const float length = Length(value);
  return std::isfinite(length) && length > kEpsilon ? value * (1.0F / length) : fallback;
}
[[nodiscard]] constexpr Vector3 Lerp(Vector3 a, Vector3 b, float t) { return a + (b - a) * t; }

struct Quaternion final {
  float x{}, y{}, z{}, w{1.0F};
  [[nodiscard]] static Quaternion FromAxisAngleRadians(Vector3 axis, float angle) {
    axis = NormalizeSafe(axis, {0, 1, 0});
    const float half = angle * 0.5F;
    const float s = std::sin(half);
    return {axis.x * s, axis.y * s, axis.z * s, std::cos(half)};
  }
};
[[nodiscard]] inline Quaternion NormalizeSafe(Quaternion q) {
  const float n = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
  return std::isfinite(n) && n > kEpsilon ? Quaternion{q.x / n, q.y / n, q.z / n, q.w / n}
                                          : Quaternion{};
}
[[nodiscard]] inline Quaternion Slerp(Quaternion a, Quaternion b, float t) {
  float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
  if (dot < 0) {
    b = {-b.x, -b.y, -b.z, -b.w};
    dot = -dot;
  }
  if (dot > 0.9995F)
    return NormalizeSafe({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t,
                          a.w + (b.w - a.w) * t});
  const float theta = std::acos(std::clamp(dot, -1.0F, 1.0F)), s = std::sin(theta);
  const float x = std::sin((1 - t) * theta) / s, y = std::sin(t * theta) / s;
  return {a.x * x + b.x * y, a.y * x + b.y * y, a.z * x + b.z * y, a.w * x + b.w * y};
}

// Row-major storage, column vectors, right-handed/Y-up public convention.
struct Matrix4 final {
  std::array<float, 16> values{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  [[nodiscard]] constexpr float &operator()(std::size_t row, std::size_t column) {
    return values[row * 4 + column];
  }
  [[nodiscard]] constexpr float operator()(std::size_t row, std::size_t column) const {
    return values[row * 4 + column];
  }
};
using Matrix3 = std::array<float, 9>;
[[nodiscard]] inline Matrix4 operator*(const Matrix4 &a, const Matrix4 &b) {
  Matrix4 r{};
  r.values.fill(0);
  for (size_t i = 0; i < 4; ++i)
    for (size_t j = 0; j < 4; ++j)
      for (size_t k = 0; k < 4; ++k)
        r(i, j) += a(i, k) * b(k, j);
  return r;
}
[[nodiscard]] inline Matrix4 PerspectiveRadians(float fov, float aspect, float near_plane,
                                                float far_plane) {
  Matrix4 r{};
  r.values.fill(0);
  const float f = 1 / std::tan(fov / 2);
  r(0, 0) = f / aspect;
  r(1, 1) = f;
  r(2, 2) = far_plane / (near_plane - far_plane);
  r(2, 3) = far_plane * near_plane / (near_plane - far_plane);
  r(3, 2) = -1;
  return r;
}
struct Transform final {
  Vector3 translation{};
  Quaternion rotation{};
  Vector3 scale{1, 1, 1};
};
[[nodiscard]] inline Matrix4 Compose(const Transform &t) {
  const auto q = NormalizeSafe(t.rotation);
  Matrix4 r;
  const float xx = q.x * q.x, yy = q.y * q.y, zz = q.z * q.z, xy = q.x * q.y, xz = q.x * q.z,
              yz = q.y * q.z, wx = q.w * q.x, wy = q.w * q.y, wz = q.w * q.z;
  r(0, 0) = (1 - 2 * (yy + zz)) * t.scale.x;
  r(0, 1) = 2 * (xy - wz) * t.scale.y;
  r(0, 2) = 2 * (xz + wy) * t.scale.z;
  r(0, 3) = t.translation.x;
  r(1, 0) = 2 * (xy + wz) * t.scale.x;
  r(1, 1) = (1 - 2 * (xx + zz)) * t.scale.y;
  r(1, 2) = 2 * (yz - wx) * t.scale.z;
  r(1, 3) = t.translation.y;
  r(2, 0) = 2 * (xz - wy) * t.scale.x;
  r(2, 1) = 2 * (yz + wx) * t.scale.y;
  r(2, 2) = (1 - 2 * (xx + yy)) * t.scale.z;
  r(2, 3) = t.translation.z;
  return r;
}
struct Color final {
  float r{}, g{}, b{}, a{1};
};
struct Rect final {
  float x{}, y{}, width{}, height{};
  [[nodiscard]] constexpr bool Contains(Vector2 p) const {
    return p.x >= x && p.y >= y && p.x <= x + width && p.y <= y + height;
  }
};
struct Ray final {
  Vector3 origin{}, direction{0, 0, -1};
};
struct Plane final {
  Vector3 normal{0, 1, 0};
  float distance{};
};
struct Aabb final {
  Vector3 minimum{}, maximum{};
  [[nodiscard]] constexpr bool Contains(Vector3 p) const {
    return p.x >= minimum.x && p.y >= minimum.y && p.z >= minimum.z && p.x <= maximum.x &&
           p.y <= maximum.y && p.z <= maximum.z;
  }
};
struct Sphere final {
  Vector3 center{};
  float radius{};
};
struct Frustum final {
  std::array<Plane, 6> planes{};
};
[[nodiscard]] inline std::optional<float> Intersect(const Ray &r, const Plane &p) {
  const float d = Dot(p.normal, r.direction);
  if (std::abs(d) <= kEpsilon)
    return {};
  const float t = -(Dot(p.normal, r.origin) + p.distance) / d;
  return t >= 0 ? std::optional<float>{t} : std::nullopt;
}
[[nodiscard]] inline bool Intersects(const Sphere &s, const Aabb &b) {
  const auto clamp = [](float v, float lo, float hi) { return std::clamp(v, lo, hi); };
  const Vector3 p{clamp(s.center.x, b.minimum.x, b.maximum.x),
                  clamp(s.center.y, b.minimum.y, b.maximum.y),
                  clamp(s.center.z, b.minimum.z, b.maximum.z)};
  const auto d = s.center - p;
  return Dot(d, d) <= s.radius * s.radius;
}
} // namespace nexora::math
