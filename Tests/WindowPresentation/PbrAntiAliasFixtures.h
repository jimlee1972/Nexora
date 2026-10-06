#pragma once
#include "Nexora/Presentation/Surface.h"
#include <cstdlib>
namespace PbrAntiAliasFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  std::array<SceneVertex, 8> vertices{{{{-1, -1, 0}},
                                       {{1, -1, 0}},
                                       {{-1, 1, 0}},
                                       {{1, 1, 0}},
                                       {{-1, -1, 0.5F}},
                                       {{1, -1, 0.5F}},
                                       {{-1, 1, 0.5F}},
                                       {{1, 1, 0.5F}}}};
  std::array<std::uint16_t, 12> indices{0, 1, 2, 2, 1, 3, 4, 5, 6, 6, 5, 7};
  std::array<SceneMaterial, 4> materials{};
  std::array<SceneMeshBatch, 2> batches{};
  bool enabled;
  explicit Fixture(unsigned mode) : enabled(mode == 1 || mode == 3) {
    for (auto &vertex : vertices) {
      vertex.normal[2] = -1.0F;
      vertex.tangent[0] = vertex.tangent[3] = 1.0F;
    }
    materials[0].unlit = true;
    materials[0].castsShadow = false;
    materials[0].emission = {4.0F, 4.0F, 4.0F};
    materials[1].unlit = true;
    materials[1].castsShadow = false;
    materials[1].baseColor = {0, 0, 0, 1};
    batches[0] = {0, mode == 3 ? 6U : 3U, 0, 1, 0};
    batches[1] = {6, 6, 0, 1, 1};
  }
  SceneDrawData Draw() const {
    SceneDrawData draw{};
    draw.vertices = vertices;
    draw.indices = indices;
    draw.materials = materials;
    draw.batches = batches;
    draw.pbr = draw.hdr = draw.offscreen = true;
    draw.postProcessAntiAliasing = enabled;
    draw.cameraPosition = {0, 0, -3};
    return draw;
  }
};
template <class ReadPixel>
unsigned Intermediate(unsigned width, unsigned height, const ReadPixel &read) {
  unsigned count = 0;
  for (unsigned y = height * 3 / 10; y < height * 7 / 10; ++y)
    for (unsigned x = width * 3 / 10; x < width * 7 / 10; ++x) {
      const auto pixel = read(x, y);
      if (pixel[0] > 4 && pixel[0] < 245 &&
          std::abs(static_cast<int>(pixel[0]) - static_cast<int>(pixel[1])) <= 1 &&
          std::abs(static_cast<int>(pixel[0]) - static_cast<int>(pixel[2])) <= 1)
        ++count;
    }
  return count;
}
} // namespace PbrAntiAliasFixtures
