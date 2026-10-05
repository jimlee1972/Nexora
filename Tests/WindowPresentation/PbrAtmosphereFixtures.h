#pragma once
#include "PbrTransparencyFixtures.h"
namespace PbrAtmosphereFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  PbrShadowFixtures::Fixture geometry{0};
  explicit Fixture(unsigned mode) {
    for (auto &material : geometry.materials) {
      material.baseColor = {0, 0, 0, 1};
      material.unlit = mode == 4;
    }
    geometry.materials[0].emission = {0, 0.5F, 0};
    geometry.materials[1].emission = {4, 0, 0};
    geometry.materials[2].unlit = true; // UI marker retains authored radiance.
  }
  SceneDrawData Draw(unsigned mode) const {
    auto draw = geometry.Draw(2);
    draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 0;
    if (mode != 0) {
      draw.atmosphere = SceneAtmosphere{{0, 0.25F, 2}, mode == 2 ? 1.0F : 0.5F, 0, 0.1F};
      if (mode == 5) {
        draw.atmosphere->startDistance = 100;
        draw.atmosphere->endDistance = 200;
      }
    }
    return draw;
  }
};
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &background, const Rgb &center) {
  const float amount = mode == 0 || mode == 4 || mode == 5 ? 0.0F : (mode == 2 ? 1.0F : 0.5F);
  const std::array<float, 3> bg{0, 0.5F * (1 - amount) + 0.25F * amount, 2 * amount};
  const std::array<float, 3> fg{4 * (1 - amount), 0.25F * amount, 2 * amount};
  for (unsigned channel = 0; channel < 3; ++channel)
    if (std::abs(static_cast<int>(background[channel]) -
                 PbrTransparencyFixtures::Encoded(bg[channel])) > 2 ||
        std::abs(static_cast<int>(center[channel]) -
                 PbrTransparencyFixtures::Encoded(fg[channel])) > 2)
      return false;
  return true;
}
} // namespace PbrAtmosphereFixtures
