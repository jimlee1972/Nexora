#pragma once
#include "PbrWorldMappingFixtures.h"
namespace PbrWorldNormalFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  PbrWorldMappingFixtures::Fixture geometry{1};
  std::array<std::byte, 4> normal{std::byte{128}, std::byte{128}, std::byte{255}, std::byte{255}};
  std::array<UiTextureUpload, 2> uploads{};
  explicit Fixture(unsigned mode) {
    geometry.geometry.materials[0].baseColor = {0.35F, 0.35F, 0.35F, 1};
    // Texture IDs identify immutable generations; flat and tilted maps use separate IDs.
    const std::uint64_t normalId = mode == 0 ? 911 : 912;
    geometry.geometry.materials[0].normalTextureId = normalId;
    geometry.geometry.materials[0].normalScale = mode == 2 ? 0.0F : 0.125F;
    if (mode != 0) {
      normal[0] = std::byte{255};
      normal[2] = std::byte{128};
    }
    uploads[0] = geometry.uploads[0];
    uploads[1] = {normalId, 1, 1, 4, normal};
  }
  SceneDrawData Draw() const {
    auto draw = geometry.Draw(1);
    draw.textureUploads = uploads;
    draw.light_direction[0] = -1;
    return draw;
  }
};
template <typename Rgb> bool Pixels(const Rgb &left, const Rgb &right) {
  return left[1] > 100 && left[0] < 25 && left[2] < 25 && right[0] > 100 && right[1] < 25 &&
         right[2] < 25;
}
} // namespace PbrWorldNormalFixtures
