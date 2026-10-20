#pragma once
#include "Nexora/RHI/Types.h"
#include <cmath>
#include <span>
namespace nexora::rhi {
struct RaySceneTag;
struct RayProgramTag;
using RaySceneHandle = core::Handle<RaySceneTag>;
using RayProgramHandle = core::Handle<RayProgramTag>;
inline constexpr std::uint32_t RayMaximumTriangles = 8;
inline constexpr std::uint32_t RayMaximumInputs = 64;
inline constexpr std::uint32_t RayMaximumDispatchesPerList = 8;
inline constexpr std::uint32_t RayMissPrimitive = 0xffffffffU;
inline constexpr std::uint32_t RayInvalidPrimitive = 0xfffffffeU;
struct RayTriangle final {
  std::array<float, 9> vertices;
};
struct RayInput final {
  std::array<float, 3> origin{};
  float minimum{};
  std::array<float, 3> direction{0, 0, 1};
  float maximum{100};
};
struct RayHit final {
  std::uint32_t primitive{RayMissPrimitive};
  float distance{};
  std::array<float, 2> barycentrics{};
};
static_assert(sizeof(RayTriangle) == 36 && sizeof(RayInput) == 32 && sizeof(RayHit) == 16);
static_assert(offsetof(RayInput, minimum) == 12 && offsetof(RayInput, direction) == 16 &&
              offsetof(RayInput, maximum) == 28);
static_assert(offsetof(RayHit, distance) == 4 && offsetof(RayHit, barycentrics) == 8);
enum class RayTracingMode : std::uint8_t {
  Pipeline,
  InlineQuery,
  ReorderedPipeline,
  ReorderedOpacityPipeline
};
struct RayInstanceTransform final {
  // Absolute object-to-world affine matrix, row major. Default preserves existing geometry.
  std::array<float, 12> matrix{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0};
};
static_assert(sizeof(RayInstanceTransform) == 48);
inline bool IsRayInstanceTransformValid(const RayInstanceTransform &transform) noexcept {
  std::array<double, 12> m{};
  for (std::size_t i = 0; i < m.size(); ++i) {
    if (!std::isfinite(transform.matrix[i]) || (i % 4 == 3 && std::abs(transform.matrix[i]) > 1e8F))
      return false;
    m[i] = transform.matrix[i];
  }
  std::array<double, 3> lengths{};
  constexpr double minimum = static_cast<double>(1e-4F);
  for (std::size_t row = 0; row < 3; ++row) {
    for (std::size_t column = 0; column < 3; ++column)
      lengths[row] += m[row * 4 + column] * m[row * 4 + column];
    if (lengths[row] < minimum * minimum || lengths[row] > 1e8)
      return false;
  }
  const double determinant = m[0] * (m[5] * m[10] - m[6] * m[9]) -
                             m[1] * (m[4] * m[10] - m[6] * m[8]) +
                             m[2] * (m[4] * m[9] - m[5] * m[8]);
  return std::abs(determinant) > 1e-6 * std::sqrt(lengths[0] * lengths[1] * lengths[2]);
}
struct RaySceneCreationOptions final {
  // Explicit opt-in; native compaction metadata can increase initial AS storage/build cost.
  bool allow_compaction{};
};
struct RayProgramDescriptor final {
  RayTracingMode mode{RayTracingMode::Pipeline};
  GraphicsRequirements graphics_requirements{};
  std::string debug_name;
};
inline bool IsRayTriangleValid(const RayTriangle &triangle) noexcept {
  for (float value : triangle.vertices)
    if (!std::isfinite(value))
      return false;
  const auto &v = triangle.vertices;
  const std::array<float, 3> a{v[3] - v[0], v[4] - v[1], v[5] - v[2]},
      b{v[6] - v[0], v[7] - v[1], v[8] - v[2]};
  const std::array<float, 3> cross{a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2],
                                   a[0] * b[1] - a[1] * b[0]};
  const float area = cross[0] * cross[0] + cross[1] * cross[1] + cross[2] * cross[2];
  return std::isfinite(area) && area > 1e-16F;
}
inline bool IsRaySceneValid(std::span<const RayTriangle> triangles) noexcept {
  if (triangles.empty() || triangles.size() > RayMaximumTriangles)
    return false;
  for (const auto &triangle : triangles)
    if (!IsRayTriangleValid(triangle))
      return false;
  return true;
}
// Each low four-bit mask follows the OC1 subdivision1 order: A corner, center, B corner,
// C corner. Set bits are opaque; clear bits are transparent. No unknown/any-hit states.
inline bool IsRayOpacitySceneValid(std::span<const RayTriangle> triangles,
                                   std::span<const std::uint8_t> masks) noexcept {
  if (!IsRaySceneValid(triangles) || triangles.size() != masks.size())
    return false;
  for (const auto mask : masks)
    if (mask > 15)
      return false;
  return true;
}
} // namespace nexora::rhi
