#pragma once
#include "Nexora/Presentation/TwoDLighting.h"
#include <algorithm>
#include <array>
#include <vector>
namespace Nexora::Presentation {
// Private native targets 0..4 are down levels, 5..8 are up levels; -1 is scene emission.
struct TwoDBloomStep final {
  int source{-1}, detail{-1}, output{};
  std::uint32_t width{}, height{};
  std::array<float, 2> texel{};
  bool up{};
};
struct TwoDBloomPlan final {
  std::vector<TwoDBloomStep> steps;
  int result{-1};
};
[[nodiscard]] inline TwoDBloomPlan PrepareTwoDBloom(std::uint32_t width, std::uint32_t height,
                                                    const TwoDLightingUniform *u) {
  TwoDBloomPlan plan;
  if (!u || !width || !height || !u->counts[3] || !u->bloom[0] || !u->ambientStrength[3])
    return plan;
  const auto levels = std::min(u->counts[3], 5U);
  std::array<std::array<std::uint32_t, 2>, 5> sizes{};
  // Divide the configured displacement budget across down (half-tap) and up
  // (full-tap) passes in output pixels. Reconstruction has its own sampling footprint.
  const float spread = u->bloom[1] / (1.5F * levels - 1.F);
  auto sourceWidth = width, sourceHeight = height;
  for (unsigned i = 0; i < levels; ++i) {
    const auto w = std::max(1U, (sourceWidth + 1) / 2), h = std::max(1U, (sourceHeight + 1) / 2);
    sizes[i] = {w, h};
    plan.steps.push_back({int(i) - 1, -1, int(i), w, h, {spread / width, spread / height}, false});
    sourceWidth = w;
    sourceHeight = h;
  }
  plan.result = int(levels) - 1;
  for (int i = int(levels) - 2; i >= 0; --i) {
    const auto [w, h] = sizes[unsigned(i)];
    plan.steps.push_back({plan.result, i, 5 + i, w, h, {spread / width, spread / height}, true});
    sourceWidth = w;
    sourceHeight = h;
    plan.result = 5 + i;
  }
  return plan;
}
} // namespace Nexora::Presentation
