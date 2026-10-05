#pragma once
#include "PbrShadowFixtures.h"
#include <cmath>
namespace PbrTransparencyFixtures {
struct Fixture final {
  PbrShadowFixtures::Fixture geometry{0};
  explicit Fixture(unsigned mode) {
    geometry.materials[0].baseColor = {0, 0, 0, 1};
    geometry.materials[0].emission = {0, 0.5F, 0};
    geometry.materials[0].unlit = true;
    geometry.materials[1].emission = {4, 0, 0};
    geometry.materials[1].unlit = true;
    geometry.materials[1].castsShadow = false;
    geometry.materials[1].opacity = mode == 0 ? 1.0F : (mode == 2 ? 0.0F : 0.5F);
    if (mode == 5)
      geometry.materials[1].transparencyTint = {1, 0.25F, 1};
    // Deliberately place the translucent emitter before the opaque receiver.
    std::swap(geometry.batches[0], geometry.batches[2]);
  }
  Nexora::Presentation::SceneDrawData Draw(unsigned mode) const {
    auto draw = geometry.Draw(2);
    if (mode == 4)
      draw.depthOfField = Nexora::Presentation::SceneDepthOfField{3.7F, 4, 32};
    return draw;
  }
};
inline int Encoded(float value) {
  const auto mapped = value * (2.51F * value + 0.03F) / (value * (2.43F * value + 0.59F) + 0.14F);
  const auto encoded =
      mapped <= 0.0031308F ? 12.92F * mapped : 1.055F * std::pow(mapped, 1 / 2.4F) - 0.055F;
  return static_cast<int>(std::lround(255 * encoded));
}
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &background, const Rgb &center) {
  const auto opacity = mode == 0 ? 1.0F : (mode == 2 ? 0.0F : 0.5F);
  return background[0] < 5 && std::abs(static_cast<int>(background[1]) - Encoded(0.5F)) <= 2 &&
         std::abs(static_cast<int>(center[0]) - Encoded(4 * opacity)) <= 2 &&
         std::abs(static_cast<int>(center[1]) -
                  Encoded(0.5F * (1 - opacity) * (mode == 5 ? 0.25F : 1))) <= 2 &&
         center[2] < 5;
}
} // namespace PbrTransparencyFixtures
