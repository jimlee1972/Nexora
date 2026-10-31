#pragma once
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <span>

namespace Nexora::Presentation {
// GPU layout shared with twod_linear.slang. Values are linear, never sRGB colors.
struct alignas(16) TwoDPointLight final {
  std::array<float, 4> positionRadiusIntensity{};
  std::array<float, 4> color{};
  std::array<std::uint32_t, 4> masks{UINT32_MAX, 0, 0, 0};
  std::array<float, 4> shadow{}; // Softness, world-plane bias, reserved, reserved.
};
struct alignas(16) TwoDShadowEdge final {
  std::array<float, 4> points{};
  std::array<std::uint32_t, 4> masks{UINT32_MAX, 0, 0, 0};
};
struct alignas(16) TwoDLightingUniform final {
  std::array<float, 4> worldX{1, 0, 0, 0};
  std::array<float, 4> worldY{0, 1, 0, 0};
  std::array<float, 4> ambientStrength{1, 1, 1, 1};
  std::array<std::uint32_t, 4> counts{};
  std::array<std::array<std::uint32_t, 4>, 2> shadowLights{};
  std::array<std::uint32_t, 4> debug{}; // Mode, shadow slot, reserved, reserved.
  std::array<float, 4> bloom{}; // Intensity, diffusion in output pixels, reserved, reserved.
  std::array<TwoDPointLight, 32> lights{};
};
struct TwoDLightingStyle final {
  std::uint32_t groups{UINT32_MAX};
  std::uint32_t receiveLight{1};
  std::uint32_t receiveShadow{1};
  std::uint32_t reserved{};
  std::uint64_t emissionTexture{};
  std::array<float, 4> emission{};             // Linear RGB and intensity, before frame strength.
  std::array<float, 4> emissionUv{1, 1, 0, 0}; // Base UV -> mask UV scale/bias.
  friend bool operator==(const TwoDLightingStyle &, const TwoDLightingStyle &) = default;
};
struct TwoDLighting final {
  TwoDLightingUniform uniforms;
  // One style for every prepared world command, including the unlit background.
  std::span<const TwoDLightingStyle> styles;
  std::span<const TwoDShadowEdge> edges{};
  std::uint64_t effectsRevision{};
};
struct TwoDShaderConstants final {
  std::array<float, 4> clip{};
  std::array<std::uint32_t, 4> style{UINT32_MAX, 0, 0, 0};
  std::array<float, 4> emission{}, emissionUv{1, 1, 0, 0};
  void SetCompositeViewport(uint32_t width, uint32_t height,
                            std::array<uint32_t, 4> viewport) noexcept {
    if (viewport == std::array<uint32_t, 4>{})
      viewport = {0, 0, width, height};
    style = {UINT32_MAX, 0, 0, 0};
    emission = {};
    emissionUv = {float(width) / viewport[2], float(height) / viewport[3],
                  -float(viewport[0]) / viewport[2], -float(viewport[1]) / viewport[3]};
  }
  void SetStyle(const TwoDLightingStyle &s) noexcept {
    style = {s.groups, s.receiveLight, s.receiveShadow, s.reserved};
    emission = s.emission;
    emissionUv = s.emissionUv;
  }
};
static_assert(sizeof(TwoDPointLight) == 64 && sizeof(TwoDLightingUniform) == 2176);
static_assert(sizeof(TwoDShaderConstants) == 64);
[[nodiscard]] inline bool ValidateTwoDLighting(const TwoDLighting *lighting, std::size_t commands) {
  if (!lighting)
    return true;
  const auto &u = lighting->uniforms;
  if (lighting->styles.size() != commands || u.counts[0] > 32 || u.counts[1] > 8 ||
      u.counts[2] > 2048 || u.counts[2] != lighting->edges.size() || u.counts[3] > 5)
    return false;
  if (u.debug[0] > 1 || u.debug[1] >= 8 || (!u.debug[0] && u.debug[1]) || u.debug[2] || u.debug[3])
    return false;
  for (auto transform : {u.worldX, u.worldY})
    for (float f : transform)
      if (!std::isfinite(f) || std::abs(f) > 1e12F)
        return false;
  for (unsigned i = 0; i < 4; ++i)
    if (!std::isfinite(u.ambientStrength[i]) || u.ambientStrength[i] < 0 ||
        u.ambientStrength[i] > (i == 3 ? 1.F : 1000000.F))
      return false;
  if (!std::isfinite(u.bloom[0]) || u.bloom[0] < 0 || u.bloom[0] > 1000000 ||
      !std::isfinite(u.bloom[1]) || u.bloom[1] < 0 || u.bloom[1] > 64 || u.bloom[2] || u.bloom[3] ||
      (u.bloom[0] > 0 && !u.counts[3]))
    return false;
  for (const auto &s : lighting->styles) {
    if (s.receiveLight > 1 || s.receiveShadow > 1 || s.reserved ||
        (!s.emissionTexture && s.emission[3] != 0))
      return false;
    for (float f : s.emission)
      if (!std::isfinite(f) || f < 0 || f > 1000000)
        return false;
    for (float f : s.emissionUv)
      if (!std::isfinite(f) || std::abs(f) > 1000000)
        return false;
  }
  unsigned shadowCount{};
  for (unsigned i = 0; i < u.counts[0]; ++i) {
    const auto &l = u.lights[i];
    for (unsigned j = 0; j < 4; ++j)
      if (!std::isfinite(l.positionRadiusIntensity[j]) ||
          std::abs(l.positionRadiusIntensity[j]) > 1000000.F)
        return false;
    if (l.positionRadiusIntensity[2] <= 0 || l.positionRadiusIntensity[3] < 0)
      return false;
    if (l.color[3] != 0 || l.masks[1] > u.counts[1] || l.masks[2] || l.masks[3])
      return false;
    for (float f : l.color)
      if (!std::isfinite(f) || f < 0 || f > 1000000.F)
        return false;
    if (!std::isfinite(l.shadow[0]) || l.shadow[0] < 0 || l.shadow[0] > 1 ||
        !std::isfinite(l.shadow[1]) || l.shadow[1] < 0 ||
        l.shadow[1] > l.positionRadiusIntensity[2] || l.shadow[2] != 0 || l.shadow[3] != 0)
      return false;
    if (l.masks[1]) {
      const auto slot = l.masks[1] - 1;
      if (u.shadowLights[slot / 4][slot % 4] != i)
        return false;
      ++shadowCount;
    }
  }
  if (shadowCount != u.counts[1])
    return false;
  for (unsigned i = 0; i < u.counts[1]; ++i) {
    const auto index = u.shadowLights[i / 4][i % 4];
    if (index >= u.counts[0] || u.lights[index].masks[1] != i + 1)
      return false;
  }
  for (const auto &edge : lighting->edges) {
    for (float f : edge.points)
      if (!std::isfinite(f) || std::abs(f) > 1e12F)
        return false;
    if (edge.masks[1] || edge.masks[2] || edge.masks[3] ||
        (edge.points[0] == edge.points[2] && edge.points[1] == edge.points[3]))
      return false;
  }
  return true;
}
} // namespace Nexora::Presentation
