#pragma once
#include "PbrShadowFixtures.h"
namespace PbrWorldMappingFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  PbrShadowFixtures::Fixture geometry{0};
  std::array<SceneInstance, 1> instances{};
  const std::array<std::byte, 8> colors{std::byte{255}, std::byte{0},  std::byte{0},
                                        std::byte{255}, std::byte{0},  std::byte{255},
                                        std::byte{0},   std::byte{255}};
  std::array<UiTextureUpload, 1> uploads{};
  explicit Fixture(unsigned mode) {
    geometry.materials[0].baseColor = {1, 1, 1, 1};
    geometry.materials[0].normalScale = 0;
    geometry.materials[0].textureId = 910;
    geometry.materials[0].worldTextureScale = mode == 0 ? 0.0F : 0.5F;
    for (auto &vertex : geometry.vertices)
      vertex.uv[0] = 0.25F;
    uploads[0] = {910, 2, 1, 8, colors};
    if (mode == 2)
      instances[0].model_transform =
          std::array<float, 16>{1, 0, 0, 1, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  }
  SceneDrawData Draw(unsigned mode) const {
    auto draw = geometry.Draw(2);
    draw.instances = instances;
    draw.textureUploads = uploads;
    draw.light_direction[0] = draw.light_direction[1] = 0;
    draw.light_direction[2] = 1;
    if (mode == 2) {
      draw.model_view_projection[3] = -1;
      draw.cameraPosition[0] = 1;
    }
    return draw;
  }
};
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &left, const Rgb &right) {
  const auto red = [](const auto &p) { return p[0] > 100 && p[1] < 25 && p[2] < 25; };
  const auto green = [](const auto &p) { return p[1] > 100 && p[0] < 25 && p[2] < 25; };
  if (mode == 0)
    return red(left) && red(right);
  if (mode == 2)
    return red(left) && green(right);
  return green(left) && red(right);
}
} // namespace PbrWorldMappingFixtures
