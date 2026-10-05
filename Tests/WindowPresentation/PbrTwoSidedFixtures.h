#pragma once
#include "PbrPointLightFixtures.h"
namespace PbrTwoSidedFixtures {
struct Fixture final {
  PbrPointLightFixtures::Fixture geometry{1};
  explicit Fixture(unsigned mode) {
    for (auto &vertex : geometry.vertices)
      vertex.normal[2] = mode == 1 || mode == 2 ? 1 : -1;
    for (unsigned i = 0; i < 2; ++i)
      geometry.materials[i].twoSidedLighting = mode >= 2;
  }
  Nexora::Presentation::SceneDrawData Draw() const { return geometry.Draw(); }
};
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &left, const Rgb &right) {
  return PbrPointLightFixtures::Pixels(mode == 1 ? 0 : 1, left, right);
}
} // namespace PbrTwoSidedFixtures
