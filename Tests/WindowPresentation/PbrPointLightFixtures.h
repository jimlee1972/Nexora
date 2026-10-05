#pragma once
#include "Nexora/Presentation/Surface.h"
#include <algorithm>
namespace PbrPointLightFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  std::array<SceneVertex, 12> vertices{};
  std::array<std::uint16_t, 12> indices{};
  std::array<SceneMaterial, 4> materials{};
  std::array<SceneMeshBatch, 2> batches{{{0, 6, 0, 1, 0}, {6, 6, 0, 1, 1}}};
  unsigned mode{};
  explicit Fixture(unsigned value) : mode(value) {
    for (unsigned side = 0; side < 2; ++side) {
      const float left = side ? 0 : -0.95F, right = side ? 0.95F : 0;
      const std::array<std::array<float, 2>, 6> corners{{{left, -0.8F},
                                                         {right, -0.8F},
                                                         {right, 0.8F},
                                                         {left, -0.8F},
                                                         {right, 0.8F},
                                                         {left, 0.8F}}};
      for (unsigned i = 0; i < corners.size(); ++i) {
        auto &vertex = vertices[side * 6 + i];
        vertex.position[0] = corners[i][0];
        vertex.position[1] = corners[i][1];
        vertex.position[2] = 0.5F;
        vertex.normal[2] = -1;
        vertex.tangent[0] = vertex.tangent[3] = 1;
      }
      materials[side].baseColor = {0.5F, 0.5F, 0.5F, 1};
      materials[side].roughness = 0.8F;
      materials[side].castsShadow = false;
      materials[side].unlit = mode == 5;
    }
    for (unsigned i = 0; i < indices.size(); ++i)
      indices[i] = static_cast<std::uint16_t>(i);
  }
  SceneDrawData Draw() const {
    SceneDrawData draw{};
    draw.vertices = vertices;
    draw.indices = indices;
    draw.materials = materials;
    draw.batches = batches;
    draw.pbr = draw.hdr = draw.offscreen = true;
    draw.cameraPosition = {0, 0, -3};
    std::fill(std::begin(draw.light_color), std::end(draw.light_color), 0.0F);
    if (mode != 0 && mode != 4)
      draw.pointLight = ScenePointLight{{mode == 2 ? 0.8F : -0.8F, 0, -1}, {2, 0, 0}, 4};
    return draw;
  }
};
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &left, const Rgb &right) {
  if (mode == 0 || mode == 4 || mode == 5)
    return left[0] < 5 && left[1] < 5 && left[2] < 5 && right[0] < 5 && right[1] < 5 &&
           right[2] < 5;
  const auto &nearPixel = mode == 2 ? right : left;
  const auto &farPixel = mode == 2 ? left : right;
  return nearPixel[0] > farPixel[0] + 8 && farPixel[0] > 30 && nearPixel[1] < 5 &&
         nearPixel[2] < 5 && farPixel[1] < 5 && farPixel[2] < 5;
}
} // namespace PbrPointLightFixtures
