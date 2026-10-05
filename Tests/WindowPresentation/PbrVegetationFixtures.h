#pragma once
#include "PbrShadowFixtures.h"

namespace PbrVegetationFixtures {
struct Fixture final {
  PbrShadowFixtures::Fixture geometry{0};
  std::array<std::byte, 4> texel{std::byte{255}, std::byte{255}, std::byte{255}, std::byte{0}};
  Nexora::Presentation::UiTextureUpload upload{};
  explicit Fixture(unsigned mode) {
    geometry.materials[1].metallic = 1;
    geometry.materials[1].emission = {8, 3, 1};
    geometry.materials[1].textureId = mode == 1 ? 71 : 72;
    if (mode == 1)
      geometry.materials[1].alphaCutoff = 0.5F;
    if (mode >= 2) {
      texel[3] = std::byte{255};
      geometry.materials[1].textureId = 73;
      geometry.materials[1].alphaCutoff = 0.5F;
    }
    if (mode >= 3 && mode <= 5) {
      geometry.materials[1].windAmplitude = 0.5F;
      for (std::size_t i = 9; i < 12; ++i)
        geometry.vertices[i].uv[1] = 1;
    }
    if (mode >= 6) {
      geometry.materials[1].emission = {};
      geometry.materials[1].transmissionThickness = mode == 7 ? 1.0F : 0;
      geometry.materials[1].transmissionColor = {0, 1, 0};
      for (std::size_t i = 9; i < 12; ++i)
        geometry.vertices[i].normal[2] = 1;
    }
    upload = {geometry.materials[1].textureId, 1, 1, 4, texel};
  }
  Nexora::Presentation::SceneDrawData Draw(unsigned mode) const {
    auto draw = geometry.Draw(mode <= 5 ? 0 : 2);
    draw.textureUploads = {&upload, 1};
    draw.vegetationTime = mode == 4 ? 3.141593F : 0;
    return draw;
  }
};
template <class Rgb> bool Pixels(unsigned mode, const Rgb &shadow, const Rgb &center) {
  if (mode == 1)
    return shadow[0] > 100 && center[0] > 100 && center[0] < 230;
  if (mode == 0 || mode == 2)
    return shadow[0] < 10 && center[0] > 230;
  if (mode == 6)
    return center[0] < 5 && center[1] < 5;
  if (mode == 7)
    return center[0] < 5 && center[1] > 200;
  return true; // Motion and exact replay compare the independently captured central ROI.
}
} // namespace PbrVegetationFixtures
