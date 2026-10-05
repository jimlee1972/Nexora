#pragma once
#include "PbrShadowFixtures.h"

namespace PbrBloomFixtures {
struct Fixture final {
  PbrShadowFixtures::Fixture geometry{0};
  Fixture() {
    geometry.materials[0].baseColor = {0, 0, 0, 1};
    geometry.materials[0].metallic = geometry.materials[1].metallic = 1;
    geometry.materials[1].emission = {8, 3, 1};
  }
  Nexora::Presentation::SceneDrawData Draw(unsigned mode) const {
    auto draw = geometry.Draw(2);
    if (mode == 1 || mode == 2)
      draw.bloom = Nexora::Presentation::SceneBloom{0.4F, mode == 2 ? 32.0F : 1.0F, 32};
    if (mode == 3)
      draw.colorGrade = Nexora::Presentation::SceneColorGrade{0, 1};
    if (mode == 4)
      draw.depthOfField = Nexora::Presentation::SceneDepthOfField{1, 4, 32};
    return draw;
  }
};
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &halo, const Rgb &center) {
  return center[0] > 230 && center[1] > 200 &&
         (mode != 3 || std::abs(static_cast<int>(center[0]) - static_cast<int>(center[2])) <= 2) &&
         (mode == 1   ? halo[0] > 30 && halo[0] > halo[2] + 10
          : mode == 4 ? halo[0] > 30
                      : halo[0] < 5);
}
} // namespace PbrBloomFixtures
