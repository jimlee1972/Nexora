#pragma once
#include "PbrWorldMappingFixtures.h"
#include <vector>
namespace PbrMipFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  PbrWorldMappingFixtures::Fixture geometry{1};
  std::vector<std::byte> colors;
  std::array<UiTextureUpload, 1> uploads{};
  explicit Fixture(unsigned mode) {
    const unsigned size = mode == 0 ? 64 : 1;
    colors.resize(size * size * 4);
    for (unsigned y = 0; y < size; ++y)
      for (unsigned x = 0; x < size; ++x) {
        const auto value = static_cast<std::byte>(mode == 0 ? ((x + y) % 2 ? 255 : 0) : 188);
        for (unsigned c = 0; c < 3; ++c)
          colors[(y * size + x) * 4 + c] = value;
        colors[(y * size + x) * 4 + 3] = std::byte{255};
      }
    geometry.geometry.materials[0].textureId = mode == 0 ? 930 : 931;
    geometry.geometry.materials[0].worldTextureScale = 16;
    uploads[0] = {mode == 0 ? 930U : 931U, size, size, size * 4, colors};
  }
  SceneDrawData Draw() const {
    auto draw = geometry.Draw(1);
    draw.textureUploads = uploads;
    return draw;
  }
};
template <typename Rgb> bool Pixels(const Rgb &left, const Rgb &right) {
  return left[0] > 100 && right[0] > 100 && left[0] == left[1] && left[1] == left[2] &&
         right[0] == right[1] && right[1] == right[2];
}
} // namespace PbrMipFixtures
