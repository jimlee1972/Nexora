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
enum class RayTracingMode : std::uint8_t { Pipeline, InlineQuery };
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
} // namespace nexora::rhi
