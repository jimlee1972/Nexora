#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

// SIMD is an implementation detail. SSE2 is the x86-64 baseline and NEON is
// the AArch64 baseline; every accelerated reduction has a scalar reference
// implementation below so its tolerance can be tested independently.
#if defined(__SSE2__) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2) || defined(_M_X64)
#define NEXORA_MATH_HAS_SSE2 1
#include <emmintrin.h>
#else
#define NEXORA_MATH_HAS_SSE2 0
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__)
#define NEXORA_MATH_HAS_NEON 1
#include <arm_neon.h>
#else
#define NEXORA_MATH_HAS_NEON 0
#endif

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
  friend constexpr Vector4 operator+(Vector4 a, Vector4 b) {
    return {a.x + b.x, a.y + b.y, a.z + b.z, a.w + b.w};
  }
  friend constexpr Vector4 operator-(Vector4 a, Vector4 b) {
    return {a.x - b.x, a.y - b.y, a.z - b.z, a.w - b.w};
  }
  friend constexpr Vector4 operator*(Vector4 a, float s) {
    return {a.x * s, a.y * s, a.z * s, a.w * s};
  }
};
// The scalar reference implementation behind Dot(Vector4, Vector4) below:
// always available (not just as an ARM/no-SSE2 fallback), so the SIMD path
// can be tested against them for tolerance rather than assumed correct.
// Never called directly outside this header and its test.
namespace detail {
[[nodiscard]] constexpr float DotScalar(Vector4 a, Vector4 b) noexcept {
  return a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
}
} // namespace detail
#if NEXORA_MATH_HAS_SSE2 || NEXORA_MATH_HAS_NEON
namespace detail {
// Sums a __m128's four lanes into lane 0 via a shuffle-and-add tree, not a
// left-to-right scalar accumulation -- floating-point addition isn't
// associative, so this can differ from DotScalar in the last ULP or two.
// That's exactly why Dot(Vector4, Vector4)'s tolerance test compares against
// DotScalar with a small epsilon instead of requiring bit-exact equality.
[[nodiscard]] inline float DotSimd(Vector4 a, Vector4 b) noexcept {
#if NEXORA_MATH_HAS_SSE2
  const __m128 va = _mm_loadu_ps(&a.x);
  const __m128 vb = _mm_loadu_ps(&b.x);
  const __m128 products = _mm_mul_ps(va, vb);
  const __m128 shuffled = _mm_shuffle_ps(products, products, _MM_SHUFFLE(2, 3, 0, 1));
  const __m128 sums = _mm_add_ps(products, shuffled);
  const __m128 high = _mm_movehl_ps(shuffled, sums);
  const __m128 total = _mm_add_ss(sums, high);
  return _mm_cvtss_f32(total);
#else
  const float32x4_t products = vmulq_f32(vld1q_f32(&a.x), vld1q_f32(&b.x));
#if defined(__aarch64__)
  return vaddvq_f32(products);
#else
  const float32x2_t pairs = vadd_f32(vget_low_f32(products), vget_high_f32(products));
  return vget_lane_f32(vpadd_f32(pairs, pairs), 0);
#endif
#endif
}
} // namespace detail
#endif
// Dot/Length/NormalizeSafe/Lerp for Vector4 share their names with Vector3's
// overloads of the same operation on purpose (matching Vector3's API shape),
// but are declared as templates constrained to exactly Vector4 rather than
// plain Vector4 overloads. A plain overload would be ambiguous for any bare
// brace-init-list call -- e.g. `Dot({1,2,3}, {4,5,6})`, since an aggregate
// with a 3-of-4-members initializer list is an equally good conversion to
// both Vector3 and Vector4 -- which would silently break the brace-init
// calling convention this file already relies on elsewhere (see
// Quaternion::FromAxisAngleRadians's `NormalizeSafe(axis, {0, 1, 0})` a few
// lines down). Template argument deduction is never attempted from a bare
// braced-init-list against a plain type-template parameter, so these
// templates simply aren't viable candidates for such a call and the
// ambiguity never arises; called with an already-typed `Vector4` (or
// anywhere T can be deduced from another argument), they bind normally.
template <typename T>
  requires std::is_same_v<T, Vector4>
[[nodiscard]] inline float Dot(T a, T b) noexcept {
#if NEXORA_MATH_HAS_SSE2 || NEXORA_MATH_HAS_NEON
  return detail::DotSimd(a, b);
#else
  return detail::DotScalar(a, b);
#endif
}
template <typename T>
  requires std::is_same_v<T, Vector4>
[[nodiscard]] inline float Length(T value) {
  return std::sqrt(Dot(value, value));
}
template <typename T>
  requires std::is_same_v<T, Vector4>
[[nodiscard]] inline Vector4 NormalizeSafe(T value, T fallback = {}) {
  const float length = Length(value);
  return std::isfinite(length) && length > kEpsilon ? value * (1.0F / length) : fallback;
}
template <typename T>
  requires std::is_same_v<T, Vector4>
[[nodiscard]] constexpr Vector4 Lerp(T a, T b, float t) {
  return a + (b - a) * t;
}
// Dot4/Length4/NormalizeSafe4/Lerp4: thin forwarding wrappers kept for any
// caller that adopted these names during the brief window they were the
// only spelling for Vector4's Dot/Length/NormalizeSafe/Lerp (this file's
// prior commit). Safe to keep alongside the templates above: since these
// are plain, non-template Vector4 overloads with their own distinct names,
// they never participate in the Vector3/Vector4 bare-brace overload
// resolution the templates above exist to avoid.
[[nodiscard]] inline float Dot4(Vector4 a, Vector4 b) noexcept { return Dot(a, b); }
[[nodiscard]] inline float Length4(Vector4 value) { return Length(value); }
[[nodiscard]] inline Vector4 NormalizeSafe4(Vector4 value, Vector4 fallback = {}) {
  return NormalizeSafe(value, fallback);
}
[[nodiscard]] constexpr Vector4 Lerp4(Vector4 a, Vector4 b, float t) { return Lerp(a, b, t); }

[[nodiscard]] constexpr float Dot(Vector3 a, Vector3 b) {
  return a.x * b.x + a.y * b.y + a.z * b.z;
}
[[nodiscard]] constexpr Vector3 Cross(Vector3 a, Vector3 b) {
  return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
[[nodiscard]] inline float Length(Vector3 value) { return std::sqrt(Dot(value, value)); }
// Two required arguments, plain (non-template) overload: this form was
// never ambiguous with Quaternion::NormalizeSafe below (which takes exactly
// one argument, so arity alone rules it out for a 2-argument call), so it
// stays a normal overload and keeps accepting bare brace-init lists for
// either or both arguments -- e.g. FromAxisAngleRadians's
// `NormalizeSafe(axis, {0, 1, 0})` below, or a fully bare
// `NormalizeSafe({3, 4, 0}, {0, 1, 0})`.
[[nodiscard]] inline Vector3 NormalizeSafe(Vector3 value, Vector3 fallback) {
  const float length = Length(value);
  return std::isfinite(length) && length > kEpsilon ? value * (1.0F / length) : fallback;
}
// One-argument convenience (default fallback = a zero Vector3), constrained
// to exactly Vector3 and declared as a template for the same reason as
// Vector4's NormalizeSafe above: this arity -- not the two-argument form --
// is what a bare `NormalizeSafe({1, 2, 3})` would otherwise resolve to
// ambiguously against Quaternion's one-argument NormalizeSafe below (both
// are aggregates with a 3-of-4-or-3-of-3 initializable-member shape).
// Deduction from a bare braced-init-list never happens, so neither
// one-argument template is a viable candidate for that call (a clear "no
// matching function" instead of silently picking one type or being
// ambiguous); called with an already-typed Vector3, T deduces normally.
template <typename T>
  requires std::is_same_v<T, Vector3>
[[nodiscard]] inline Vector3 NormalizeSafe(T value) {
  return NormalizeSafe(value, Vector3{});
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
template <typename T>
  requires std::is_same_v<T, Quaternion>
[[nodiscard]] inline Quaternion NormalizeSafe(T q) {
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
    return NormalizeSafe(Quaternion{a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t,
                                    a.z + (b.z - a.z) * t, a.w + (b.w - a.w) * t});
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
struct Matrix3 final {
  std::array<float, 9> values{1, 0, 0, 0, 1, 0, 0, 0, 1};
  [[nodiscard]] constexpr float &operator()(std::size_t row, std::size_t column) {
    return values[row * 3 + column];
  }
  [[nodiscard]] constexpr float operator()(std::size_t row, std::size_t column) const {
    return values[row * 3 + column];
  }
};
[[nodiscard]] inline Matrix3 operator*(const Matrix3 &a, const Matrix3 &b) {
  Matrix3 r{};
  r.values.fill(0);
  for (std::size_t i = 0; i < 3; ++i)
    for (std::size_t j = 0; j < 3; ++j)
      for (std::size_t k = 0; k < 3; ++k)
        r(i, j) += a(i, k) * b(k, j);
  return r;
}
[[nodiscard]] constexpr Vector3 operator*(const Matrix3 &m, Vector3 v) {
  return {m(0, 0) * v.x + m(0, 1) * v.y + m(0, 2) * v.z,
          m(1, 0) * v.x + m(1, 1) * v.y + m(1, 2) * v.z,
          m(2, 0) * v.x + m(2, 1) * v.y + m(2, 2) * v.z};
}
[[nodiscard]] constexpr float Determinant(const Matrix3 &m) {
  return m(0, 0) * (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) -
         m(0, 1) * (m(1, 0) * m(2, 2) - m(1, 2) * m(2, 0)) +
         m(0, 2) * (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0));
}
[[nodiscard]] inline Matrix3 InverseSafe(const Matrix3 &m, const Matrix3 &fallback = Matrix3{}) {
  const float det = Determinant(m);
  if (std::abs(det) <= kEpsilon)
    return fallback;
  const float inv_det = 1.0F / det;
  Matrix3 r{};
  r(0, 0) = (m(1, 1) * m(2, 2) - m(1, 2) * m(2, 1)) * inv_det;
  r(0, 1) = (m(0, 2) * m(2, 1) - m(0, 1) * m(2, 2)) * inv_det;
  r(0, 2) = (m(0, 1) * m(1, 2) - m(0, 2) * m(1, 1)) * inv_det;
  r(1, 0) = (m(1, 2) * m(2, 0) - m(1, 0) * m(2, 2)) * inv_det;
  r(1, 1) = (m(0, 0) * m(2, 2) - m(0, 2) * m(2, 0)) * inv_det;
  r(1, 2) = (m(0, 2) * m(1, 0) - m(0, 0) * m(1, 2)) * inv_det;
  r(2, 0) = (m(1, 0) * m(2, 1) - m(1, 1) * m(2, 0)) * inv_det;
  r(2, 1) = (m(0, 1) * m(2, 0) - m(0, 0) * m(2, 1)) * inv_det;
  r(2, 2) = (m(0, 0) * m(1, 1) - m(0, 1) * m(1, 0)) * inv_det;
  return r;
}
namespace detail {
[[nodiscard]] inline Matrix4 MultiplyScalar(const Matrix4 &a, const Matrix4 &b) noexcept {
  Matrix4 r{};
  r.values.fill(0);
  for (std::size_t i = 0; i < 4; ++i)
    for (std::size_t j = 0; j < 4; ++j)
      for (std::size_t k = 0; k < 4; ++k)
        r(i, j) += a(i, k) * b(k, j);
  return r;
}
#if NEXORA_MATH_HAS_SSE2 || NEXORA_MATH_HAS_NEON
[[nodiscard]] inline Matrix4 MultiplySimd(const Matrix4 &a, const Matrix4 &b) noexcept {
  Matrix4 result{};
  for (std::size_t row = 0; row < 4; ++row) {
#if NEXORA_MATH_HAS_SSE2
    const __m128 value =
        _mm_add_ps(_mm_add_ps(_mm_mul_ps(_mm_set1_ps(a(row, 0)), _mm_loadu_ps(&b.values[0])),
                              _mm_mul_ps(_mm_set1_ps(a(row, 1)), _mm_loadu_ps(&b.values[4]))),
                   _mm_add_ps(_mm_mul_ps(_mm_set1_ps(a(row, 2)), _mm_loadu_ps(&b.values[8])),
                              _mm_mul_ps(_mm_set1_ps(a(row, 3)), _mm_loadu_ps(&b.values[12]))));
    _mm_storeu_ps(&result.values[row * 4], value);
#else
    const float32x4_t value =
        vmlaq_n_f32(vmlaq_n_f32(vmulq_n_f32(vld1q_f32(&b.values[0]), a(row, 0)),
                                vld1q_f32(&b.values[4]), a(row, 1)),
                    vld1q_f32(&b.values[8]), a(row, 2));
    vst1q_f32(&result.values[row * 4], vmlaq_n_f32(value, vld1q_f32(&b.values[12]), a(row, 3)));
#endif
  }
  return result;
}
#endif
} // namespace detail
[[nodiscard]] inline Matrix4 operator*(const Matrix4 &a, const Matrix4 &b) noexcept {
#if NEXORA_MATH_HAS_SSE2 || NEXORA_MATH_HAS_NEON
  return detail::MultiplySimd(a, b);
#else
  return detail::MultiplyScalar(a, b);
#endif
}
// Gauss-Jordan elimination with partial pivoting; returns fallback (identity by
// default) when the matrix is singular within kEpsilon, matching the file's
// NormalizeSafe-style epsilon policy instead of propagating NaN/Inf.
[[nodiscard]] inline Matrix4 InverseSafe(const Matrix4 &m, const Matrix4 &fallback = Matrix4{}) {
  std::array<std::array<float, 8>, 4> a{};
  for (std::size_t row = 0; row < 4; ++row) {
    for (std::size_t column = 0; column < 4; ++column)
      a[row][column] = m(row, column);
    a[row][4 + row] = 1.0F;
  }
  for (std::size_t column = 0; column < 4; ++column) {
    std::size_t pivot = column;
    float best = std::abs(a[column][column]);
    for (std::size_t row = column + 1; row < 4; ++row) {
      const float candidate = std::abs(a[row][column]);
      if (candidate > best) {
        best = candidate;
        pivot = row;
      }
    }
    if (best <= kEpsilon)
      return fallback;
    if (pivot != column)
      std::swap(a[pivot], a[column]);
    const float scale = 1.0F / a[column][column];
    for (float &value : a[column])
      value *= scale;
    for (std::size_t row = 0; row < 4; ++row) {
      if (row == column)
        continue;
      const float factor = a[row][column];
      if (factor == 0.0F)
        continue;
      for (std::size_t k = 0; k < 8; ++k)
        a[row][k] -= factor * a[column][k];
    }
  }
  Matrix4 result{};
  for (std::size_t row = 0; row < 4; ++row)
    for (std::size_t column = 0; column < 4; ++column)
      result(row, column) = a[row][4 + column];
  return result;
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
// Right-handed, D3D-style [0,1] depth range to match PerspectiveRadians.
[[nodiscard]] inline Matrix4 Orthographic(float left, float right, float bottom, float top,
                                          float near_plane, float far_plane) {
  Matrix4 r{};
  r.values.fill(0);
  r(0, 0) = 2.0F / (right - left);
  r(0, 3) = -(right + left) / (right - left);
  r(1, 1) = 2.0F / (top - bottom);
  r(1, 3) = -(top + bottom) / (top - bottom);
  r(2, 2) = 1.0F / (near_plane - far_plane);
  r(2, 3) = near_plane / (near_plane - far_plane);
  r(3, 3) = 1.0F;
  return r;
}
// Right-handed view matrix; no angle parameter, so unlike PerspectiveRadians this
// name carries no Radians/Degrees suffix.
[[nodiscard]] inline Matrix4 LookAt(Vector3 eye, Vector3 target, Vector3 up = {0, 1, 0}) {
  const Vector3 z_axis = NormalizeSafe(eye - target, {0, 0, 1});
  const Vector3 x_axis = NormalizeSafe(Cross(up, z_axis), {1, 0, 0});
  const Vector3 y_axis = Cross(z_axis, x_axis);
  Matrix4 r{};
  r(0, 0) = x_axis.x;
  r(0, 1) = x_axis.y;
  r(0, 2) = x_axis.z;
  r(0, 3) = -Dot(x_axis, eye);
  r(1, 0) = y_axis.x;
  r(1, 1) = y_axis.y;
  r(1, 2) = y_axis.z;
  r(1, 3) = -Dot(y_axis, eye);
  r(2, 0) = z_axis.x;
  r(2, 1) = z_axis.y;
  r(2, 2) = z_axis.z;
  r(2, 3) = -Dot(z_axis, eye);
  r(3, 0) = 0;
  r(3, 1) = 0;
  r(3, 2) = 0;
  r(3, 3) = 1;
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
// Inverse of Compose(): extracts translation from column 3, scale from the
// length of each basis column, and rotation via Shepperd's method on the
// remaining orthonormal columns. Only exact for matrices Compose() could have
// produced (no shear); shear is dropped rather than reported. A reflected
// (mirrored) transform -- negative determinant, e.g. scale.x < 0 -- has an
// improper linear part; the sign is folded into scale.x rather than left in
// the rotation, so the recovered axes stay a proper (det +1) rotation, which
// Shepperd's method below assumes, and Compose(Decompose(m)) still equals m.
[[nodiscard]] inline Transform Decompose(const Matrix4 &m) {
  Transform t;
  t.translation = {m(0, 3), m(1, 3), m(2, 3)};
  const Vector3 column_x{m(0, 0), m(1, 0), m(2, 0)};
  const Vector3 column_y{m(0, 1), m(1, 1), m(2, 1)};
  const Vector3 column_z{m(0, 2), m(1, 2), m(2, 2)};
  t.scale = {Length(column_x), Length(column_y), Length(column_z)};
  if (Dot(column_x, Cross(column_y, column_z)) < 0.0F)
    t.scale.x = -t.scale.x;
  const Vector3 axis_x =
      std::abs(t.scale.x) > kEpsilon ? column_x * (1.0F / t.scale.x) : Vector3{1, 0, 0};
  const Vector3 axis_y = t.scale.y > kEpsilon ? column_y * (1.0F / t.scale.y) : Vector3{0, 1, 0};
  const Vector3 axis_z = t.scale.z > kEpsilon ? column_z * (1.0F / t.scale.z) : Vector3{0, 0, 1};
  const float m00 = axis_x.x, m10 = axis_x.y, m20 = axis_x.z;
  const float m01 = axis_y.x, m11 = axis_y.y, m21 = axis_y.z;
  const float m02 = axis_z.x, m12 = axis_z.y, m22 = axis_z.z;
  const float trace = m00 + m11 + m22;
  Quaternion q;
  if (trace > 0.0F) {
    const float s = std::sqrt(trace + 1.0F) * 2.0F;
    q = {(m21 - m12) / s, (m02 - m20) / s, (m10 - m01) / s, 0.25F * s};
  } else if (m00 > m11 && m00 > m22) {
    const float s = std::sqrt(1.0F + m00 - m11 - m22) * 2.0F;
    q = {0.25F * s, (m01 + m10) / s, (m02 + m20) / s, (m21 - m12) / s};
  } else if (m11 > m22) {
    const float s = std::sqrt(1.0F + m11 - m00 - m22) * 2.0F;
    q = {(m01 + m10) / s, 0.25F * s, (m12 + m21) / s, (m02 - m20) / s};
  } else {
    const float s = std::sqrt(1.0F + m22 - m00 - m11) * 2.0F;
    q = {(m02 + m20) / s, (m12 + m21) / s, 0.25F * s, (m10 - m01) / s};
  }
  t.rotation = NormalizeSafe(q);
  return t;
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
// Gribb-Hartmann plane extraction from a row-major, D3D-style [0,1]-depth
// view-projection matrix. Planes are ordered left, right, bottom, top, near,
// far and each is normalized so Intersects() can compare against a raw radius.
[[nodiscard]] inline Frustum ExtractFrustum(const Matrix4 &view_projection) {
  const auto &m = view_projection;
  const auto make_plane = [](float a, float b, float c, float d) {
    Plane p{{a, b, c}, d};
    const float length = Length(p.normal);
    if (length > kEpsilon) {
      p.normal = p.normal * (1.0F / length);
      p.distance /= length;
    }
    return p;
  };
  Frustum frustum;
  frustum.planes[0] = make_plane(m(0, 0) + m(3, 0), m(0, 1) + m(3, 1), m(0, 2) + m(3, 2),
                                 m(0, 3) + m(3, 3)); // left
  frustum.planes[1] = make_plane(m(3, 0) - m(0, 0), m(3, 1) - m(0, 1), m(3, 2) - m(0, 2),
                                 m(3, 3) - m(0, 3)); // right
  frustum.planes[2] = make_plane(m(1, 0) + m(3, 0), m(1, 1) + m(3, 1), m(1, 2) + m(3, 2),
                                 m(1, 3) + m(3, 3)); // bottom
  frustum.planes[3] =
      make_plane(m(3, 0) - m(1, 0), m(3, 1) - m(1, 1), m(3, 2) - m(1, 2), m(3, 3) - m(1, 3)); // top
  frustum.planes[4] = make_plane(m(2, 0), m(2, 1), m(2, 2), m(2, 3)); // near
  frustum.planes[5] =
      make_plane(m(3, 0) - m(2, 0), m(3, 1) - m(2, 1), m(3, 2) - m(2, 2), m(3, 3) - m(2, 3)); // far
  return frustum;
}
// Conservative (may accept AABBs only touching a far corner): tests the box's
// positive vertex against each plane rather than doing full separating-axis work.
[[nodiscard]] inline bool Intersects(const Frustum &frustum, const Aabb &box) {
  for (const auto &plane : frustum.planes) {
    const Vector3 positive{plane.normal.x >= 0 ? box.maximum.x : box.minimum.x,
                           plane.normal.y >= 0 ? box.maximum.y : box.minimum.y,
                           plane.normal.z >= 0 ? box.maximum.z : box.minimum.z};
    if (Dot(plane.normal, positive) + plane.distance < 0.0F)
      return false;
  }
  return true;
}
[[nodiscard]] inline bool Intersects(const Frustum &frustum, const Sphere &sphere) {
  for (const auto &plane : frustum.planes) {
    if (Dot(plane.normal, sphere.center) + plane.distance < -sphere.radius)
      return false;
  }
  return true;
}
} // namespace nexora::math
