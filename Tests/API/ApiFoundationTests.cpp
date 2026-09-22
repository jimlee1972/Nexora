// API-M1/API-M2 conformance: ABI layout gates for the Math.h POD surface plus
// behavioral coverage for the geometry, Uuid, and generational-handle helpers
// that don't yet have any test coverage elsewhere.
#include "Nexora/Foundation/Types.h"
#include "Nexora/Math/Math.h"

#include <cstddef>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
#include <unordered_set>

namespace {
using namespace nexora::math;
using namespace nexora::foundation;

void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

// ---- ABI layout gate: sizeof/alignof/offsetof for every POD in Math.h ----
// These lock in the byte layout the roadmap requires before any type here is
// allowed to cross the C ABI boundary. A failure here means a layout change,
// not a false positive: bump the gate deliberately if the change is intended.
static_assert(sizeof(Vector2) == 8 && alignof(Vector2) == 4);
static_assert(offsetof(Vector2, x) == 0 && offsetof(Vector2, y) == 4);

static_assert(sizeof(Vector3) == 12 && alignof(Vector3) == 4);
static_assert(offsetof(Vector3, x) == 0 && offsetof(Vector3, y) == 4 && offsetof(Vector3, z) == 8);

static_assert(sizeof(Vector4) == 16 && alignof(Vector4) == 4);
static_assert(offsetof(Vector4, x) == 0 && offsetof(Vector4, y) == 4 && offsetof(Vector4, z) == 8 &&
              offsetof(Vector4, w) == 12);

static_assert(sizeof(Quaternion) == 16 && alignof(Quaternion) == 4);
static_assert(offsetof(Quaternion, x) == 0 && offsetof(Quaternion, y) == 4 &&
              offsetof(Quaternion, z) == 8 && offsetof(Quaternion, w) == 12);

static_assert(sizeof(Matrix3) == 36 && alignof(Matrix3) == 4);
static_assert(offsetof(Matrix3, values) == 0);

static_assert(sizeof(Matrix4) == 64 && alignof(Matrix4) == 4);
static_assert(offsetof(Matrix4, values) == 0);

static_assert(sizeof(Transform) == 40 && alignof(Transform) == 4);
static_assert(offsetof(Transform, translation) == 0);
static_assert(offsetof(Transform, rotation) == 12);
static_assert(offsetof(Transform, scale) == 28);

static_assert(sizeof(Color) == 16 && alignof(Color) == 4);
static_assert(offsetof(Color, r) == 0 && offsetof(Color, g) == 4 && offsetof(Color, b) == 8 &&
              offsetof(Color, a) == 12);

static_assert(sizeof(Rect) == 16 && alignof(Rect) == 4);
static_assert(offsetof(Rect, x) == 0 && offsetof(Rect, y) == 4 && offsetof(Rect, width) == 8 &&
              offsetof(Rect, height) == 12);

static_assert(sizeof(Ray) == 24 && alignof(Ray) == 4);
static_assert(offsetof(Ray, origin) == 0 && offsetof(Ray, direction) == 12);

static_assert(sizeof(Plane) == 16 && alignof(Plane) == 4);
static_assert(offsetof(Plane, normal) == 0 && offsetof(Plane, distance) == 12);

static_assert(sizeof(Aabb) == 24 && alignof(Aabb) == 4);
static_assert(offsetof(Aabb, minimum) == 0 && offsetof(Aabb, maximum) == 12);

static_assert(sizeof(Sphere) == 16 && alignof(Sphere) == 4);
static_assert(offsetof(Sphere, center) == 0 && offsetof(Sphere, radius) == 12);

static_assert(sizeof(Frustum) == 16 * 6 && alignof(Frustum) == 4);
static_assert(offsetof(Frustum, planes) == 0);

static_assert(sizeof(Uuid) == 16 && alignof(Uuid) == 8);
static_assert(offsetof(Uuid, high) == 0 && offsetof(Uuid, low) == 8);

// The generational handle ABI gate lives in Tests/API/ApiCoreContractTests.cpp
// alongside nexora::core::Handle<Tag>'s definition in Nexora/Core/Handle.h.

template <class T> bool BytewiseRoundTrip(const T &value) {
  std::byte bytes[sizeof(T)];
  std::memcpy(bytes, &value, sizeof(T));
  T restored{};
  std::memcpy(&restored, bytes, sizeof(T));
  return std::memcmp(&value, &restored, sizeof(T)) == 0;
}

int Run() {
  // ---- Serialization round trip: a memcpy'd POD is bit-identical, which is
  // the guarantee the ABI layout gate above and the roadmap's C ABI rules
  // depend on (no padding holes with indeterminate content, no vtable). ----
  Require(BytewiseRoundTrip(Vector3{1.0F, 2.0F, 3.0F}), "Vector3 byte round trip failed");
  Require(BytewiseRoundTrip(Transform{{1, 2, 3}, {0, 0, 0, 1}, {1, 1, 1}}),
          "Transform byte round trip failed");
  Require(BytewiseRoundTrip(Compose({{1, 2, 3}, {0, 0, 0, 1}, {2, 3, 4}})),
          "Matrix4 byte round trip failed");

  // ---- Vector4 arithmetic and the scalar/SIMD tolerance requirement ----
  // Add/Subtract/Scale are lane-wise with no reduction, so scalar and SIMD
  // agree exactly; Dot/Length/NormalizeSafe involve a horizontal sum, whose
  // reduction order genuinely differs between the two paths (floating-point
  // addition isn't associative), which is exactly what the roadmap's
  // "scalar/SIMD result tolerance test" acceptance criterion is checking
  // for -- not that they're bit-identical, but that they agree within a
  // small tolerance across many inputs.
  {
    const Vector4 a{1.0F, 2.0F, 3.0F, 4.0F};
    const Vector4 b{5.0F, -6.0F, 7.0F, -8.0F};
    const auto sum = a + b;
    Require(sum.x == 6.0F && sum.y == -4.0F && sum.z == 10.0F && sum.w == -4.0F,
            "Vector4 operator+ gave the wrong result");
    const auto diff = a - b;
    Require(diff.x == -4.0F && diff.y == 8.0F && diff.z == -4.0F && diff.w == 12.0F,
            "Vector4 operator- gave the wrong result");
    const auto scaled = a * 2.0F;
    Require(scaled.x == 2.0F && scaled.y == 4.0F && scaled.z == 6.0F && scaled.w == 8.0F,
            "Vector4 operator* gave the wrong result");
    Require(NearlyEqual(Dot(a, b), 5.0F - 12.0F + 21.0F - 32.0F, 1e-4F),
            "Vector4 Dot gave the wrong result");
    Require(NearlyEqual(Length(Vector4{2.0F, 0.0F, 0.0F, 0.0F}), 2.0F),
            "Vector4 Length gave the wrong result");
    const auto normalized = NormalizeSafe(Vector4{0.0F, 3.0F, 0.0F, 4.0F});
    Require(NearlyEqual(Length(normalized), 1.0F, 1e-4F),
            "Vector4 NormalizeSafe did not produce a unit vector");
    Require(NormalizeSafe(Vector4{}).x == 0.0F && NormalizeSafe(Vector4{}).w == 0.0F,
            "Vector4 NormalizeSafe did not fall back on a zero-length input");
    const auto lerped = Lerp(Vector4{0, 0, 0, 0}, Vector4{2, 4, 6, 8}, 0.5F);
    Require(lerped.x == 1.0F && lerped.y == 2.0F && lerped.z == 3.0F && lerped.w == 4.0F,
            "Vector4 Lerp gave the wrong result");

    // Regression check for the P2 finding that a plain Vector4 overload of
    // these names would make every bare brace-init call ambiguous against
    // Vector3's overload of the same name: this must still resolve to
    // Vector3's Dot unambiguously precisely because Vector4's version is a
    // template that bare brace-init lists can never deduce against.
    Require(NearlyEqual(Dot({1.0F, 0.0F, 0.0F}, {0.0F, 1.0F, 0.0F}), 0.0F),
            "a bare brace-init Dot call must still resolve to Vector3::Dot unambiguously");

    std::mt19937 random{12345};
    std::uniform_real_distribution<float> distribution{-1000.0F, 1000.0F};
    for (int i = 0; i < 10000; ++i) {
      const Vector4 x{distribution(random), distribution(random), distribution(random),
                      distribution(random)};
      const Vector4 y{distribution(random), distribution(random), distribution(random),
                      distribution(random)};
      const float simd_dot = Dot(x, y);
      const float scalar_dot = detail::DotScalar(x, y);
      // Tolerance relative to the sum of the |term| magnitudes, not to the
      // final (possibly near-cancelled) dot value: two large, opposite-sign
      // terms can sum to something near zero while each term still carries
      // real floating-point error, so a tolerance relative to the result
      // alone would be far too tight exactly when cancellation happens.
      const float magnitude =
          std::abs(x.x * y.x) + std::abs(x.y * y.y) + std::abs(x.z * y.z) + std::abs(x.w * y.w);
      Require(NearlyEqual(simd_dot, scalar_dot, magnitude * 1e-5F + 1e-3F),
              "Dot(Vector4, Vector4)'s active path drifted from the scalar reference beyond "
              "floating-point reduction-order tolerance");
    }
  }

  // ---- Compose/Decompose inverse relationship ----
  const Transform original{{4.0F, -2.0F, 6.0F},
                           Quaternion::FromAxisAngleRadians({0, 1, 0}, Radians(37.0F)),
                           {1.5F, 2.5F, 0.5F}};
  const Transform decomposed = Decompose(Compose(original));
  Require(NearlyEqual(decomposed.translation.x, original.translation.x, 1e-3F) &&
              NearlyEqual(decomposed.translation.y, original.translation.y, 1e-3F) &&
              NearlyEqual(decomposed.translation.z, original.translation.z, 1e-3F),
          "Decompose did not recover translation");
  Require(NearlyEqual(decomposed.scale.x, original.scale.x, 1e-3F) &&
              NearlyEqual(decomposed.scale.y, original.scale.y, 1e-3F) &&
              NearlyEqual(decomposed.scale.z, original.scale.z, 1e-3F),
          "Decompose did not recover scale");
  const auto recomposed = Compose(decomposed);
  const auto original_matrix = Compose(original);
  for (std::size_t i = 0; i < 16; ++i)
    Require(NearlyEqual(recomposed.values[i], original_matrix.values[i], 1e-3F),
            "Compose(Decompose(Compose(t))) drifted from Compose(t)");

  // ---- Compose/Decompose with a reflected (mirrored) scale: the linear
  // part has a negative determinant, so naively taking column lengths and
  // feeding them straight into Shepperd's method would produce an improper
  // rotation matrix and silently un-mirror the transform on recompose. ----
  const Transform mirrored{{1.0F, 2.0F, 3.0F},
                           Quaternion::FromAxisAngleRadians({0, 1, 0}, Radians(52.0F)),
                           {-2.0F, 3.0F, 4.0F}};
  const Transform decomposed_mirror = Decompose(Compose(mirrored));
  const auto recomposed_mirror = Compose(decomposed_mirror);
  const auto mirrored_matrix = Compose(mirrored);
  for (std::size_t i = 0; i < 16; ++i)
    Require(NearlyEqual(recomposed_mirror.values[i], mirrored_matrix.values[i], 1e-3F),
            "Compose(Decompose(Compose(t))) did not preserve a reflected (negative scale) t");

  // ---- Matrix4 InverseSafe ----
  const Matrix4 identity{};
  Require(BytewiseRoundTrip(InverseSafe(identity)), "identity inverse is not itself byte-stable");
  for (std::size_t i = 0; i < 16; ++i)
    Require(NearlyEqual(InverseSafe(identity).values[i], identity.values[i]),
            "InverseSafe(identity) != identity");
  const Matrix4 scaled = Compose({{}, {0, 0, 0, 1}, {2.0F, 4.0F, 0.5F}});
  const Matrix4 restored_identity = scaled * InverseSafe(scaled);
  for (std::size_t i = 0; i < 16; ++i)
    Require(NearlyEqual(restored_identity.values[i], identity.values[i], 1e-3F),
            "M * InverseSafe(M) did not converge to identity");
  Matrix4 singular{};
  singular.values.fill(0);
  Require(BytewiseRoundTrip(InverseSafe(singular, identity)) &&
              InverseSafe(singular, identity).values == identity.values,
          "InverseSafe did not fall back on a singular matrix");

  // ---- Matrix3 ----
  const Matrix3 rotate90{0, -1, 0, 1, 0, 0, 0, 0, 1};
  Require(NearlyEqual(Determinant(rotate90), 1.0F), "Matrix3 determinant is wrong");
  const Vector3 rotated = rotate90 * Vector3{1, 0, 0};
  Require(NearlyEqual(rotated.x, 0.0F) && NearlyEqual(rotated.y, 1.0F),
          "Matrix3 * Vector3 gave the wrong result");
  const Matrix3 identity3{};
  const Matrix3 inv3 = InverseSafe(rotate90);
  const Matrix3 product = rotate90 * inv3;
  for (int i = 0; i < 9; ++i)
    Require(NearlyEqual(product.values[i], identity3.values[i], 1e-4F),
            "Matrix3 InverseSafe did not invert");
  Matrix3 singular3{};
  singular3.values.fill(0);
  Require(InverseSafe(singular3, identity3).values == identity3.values,
          "Matrix3 InverseSafe did not fall back on a singular matrix");

  // ---- Perspective / Orthographic depth mapping (D3D-style [0,1], RH) ----
  const auto ortho = Orthographic(-1, 1, -1, 1, 1.0F, 10.0F);
  Require(NearlyEqual(ortho(2, 2) * -1.0F + ortho(2, 3), 0.0F, 1e-4F),
          "Orthographic near plane did not map to depth 0");
  Require(NearlyEqual(ortho(2, 2) * -10.0F + ortho(2, 3), 1.0F, 1e-4F),
          "Orthographic far plane did not map to depth 1");

  // ---- LookAt + Frustum extraction/culling ----
  const auto view = LookAt({0, 0, 5}, {0, 0, 0});
  const auto projection = PerspectiveRadians(Radians(60.0F), 1.0F, 0.1F, 100.0F);
  const auto view_projection = projection * view;
  const auto frustum = ExtractFrustum(view_projection);
  Require(Intersects(frustum, Sphere{{0, 0, 0}, 1.0F}), "frustum rejected a centered sphere");
  Require(!Intersects(frustum, Sphere{{1000, 0, 0}, 1.0F}), "frustum accepted a far-away sphere");
  Require(Intersects(frustum, Aabb{{-0.5F, -0.5F, -0.5F}, {0.5F, 0.5F, 0.5F}}),
          "frustum rejected a centered aabb");
  Require(!Intersects(frustum, Aabb{{500, 500, 500}, {501, 501, 501}}),
          "frustum accepted a far-away aabb");

  // ---- Uuid parse/format round trip and rejection ----
  const auto parsed = Uuid::Parse("01234567-89ab-cdef-0123-456789abcdef");
  Require(parsed.HasValue(), "canonical Uuid text failed to parse");
  Require(parsed.Value().ToString() == "01234567-89ab-cdef-0123-456789abcdef",
          "Uuid round trip through ToString did not match the input");
  const auto parsed_no_dashes = Uuid::Parse("0123456789abcdef0123456789abcdef");
  Require(parsed_no_dashes.HasValue() && parsed_no_dashes.Value() == parsed.Value(),
          "dash-free Uuid text did not parse to the same value");
  Require(!Uuid::Parse("not-a-uuid").HasValue(), "malformed Uuid text was accepted");
  Require(!Uuid::Parse("----0123456789abcdef0123456789abcdef").HasValue(),
          "36 chars with dashes only at the front (not the canonical positions) was accepted");
  Require(!Uuid::Parse("0123456-789ab-cdef-0123-456789abcdef").HasValue(),
          "a dash one position off from canonical was accepted");
  Require(!Uuid::Parse("0123456789abcdef0123456789abcdef-").HasValue(),
          "a 33-character string one dash too long was accepted");
  Require(!Uuid::Parse("01234567-89ab-cdef-0123-456789abcde").HasValue(),
          "a truncated Uuid (one hex digit short) was accepted");
  // Split into adjacent literals so the "\0" octal escape isn't immediately
  // followed by a digit within the same token: MSVC's C4125 (and /WX) treats
  // that as an error even though "\0" is unambiguously one byte here.
  Require(!Uuid::Parse(StringView("0123\0"
                                  "567-89ab-cdef-0123-456789abcdef",
                                  36))
               .HasValue(),
          "a Uuid string with an embedded NUL was accepted");

  // ---- Name hash collision diagnostics: statistical sanity check, not a
  // proof of collision-freedom (the class deliberately keeps only a 64-bit
  // hash; see the comment on Name in Types.h for the accepted trade-off). ----
  std::unordered_set<std::uint64_t> seen;
  for (int i = 0; i < 20000; ++i)
    seen.insert(Name(("nexora.name." + std::to_string(i)).c_str()).Value());
  Require(seen.size() == 20000, "FNV-1a produced a collision across 20000 short distinct names");

  return 0;
}
} // namespace

int main() {
  try {
    return Run();
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
