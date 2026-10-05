#pragma once
#include "Nexora/Presentation/Surface.h"
#include <array>

namespace PbrShadowFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  std::array<SceneVertex, 12> vertices{};
  std::array<std::uint16_t, 12> indices{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
  std::array<SceneMeshBatch, 3> batches{{{0, 6, 0, 1, 0}, {6, 3, 0, 1, 2}, {9, 3, 0, 1, 1}}};
  std::array<SceneMaterial, 4> materials{};
  explicit Fixture(unsigned mode) {
    const std::array<std::array<float, 3>, 12> positions{{{-0.95F, -0.85F, 0.7F},
                                                          {0.95F, -0.85F, 0.7F},
                                                          {0.95F, 0.85F, 0.7F},
                                                          {-0.95F, -0.85F, 0.7F},
                                                          {0.95F, 0.85F, 0.7F},
                                                          {-0.95F, 0.85F, 0.7F},
                                                          {-0.1F, 0.6F, 0.1F},
                                                          {0.1F, 0.6F, 0.1F},
                                                          {0, 0.9F, 0.1F},
                                                          {-0.3F, -0.5F, 0.2F},
                                                          {0.3F, -0.5F, 0.2F},
                                                          {0, 0.5F, 0.2F}}};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
      std::copy(positions[i].begin(), positions[i].end(), vertices[i].position);
      vertices[i].normal[2] = -1;
      vertices[i].tangent[0] = vertices[i].tangent[3] = 1;
      vertices[i].uv[0] = vertices[i].uv[1] = 0.5F;
      if (i >= 9 && mode == 1)
        vertices[i].position[0] += 1;
      if (i >= 9 && mode == 5)
        vertices[i].position[1] += 0.8F;
    }
    materials[0].baseColor = {0.5F, 0.5F, 0.5F, 1};
    materials[0].roughness = 1;
    materials[1].baseColor = {0, 0, 0, 1};
    materials[2].baseColor = {0, 0, 0, 1};
    materials[2].metallic = materials[2].roughness = 1;
  }
  SceneDrawData Draw(unsigned mode) const {
    SceneDrawData draw{};
    draw.vertices = vertices;
    draw.indices = indices;
    draw.materials = materials;
    draw.batches = batches;
    draw.pbr = draw.hdr = draw.offscreen = true;
    draw.cameraPosition =
        mode == 3 ? std::array<float, 3>{3, 0, -3} : std::array<float, 3>{0, 0, -3};
    draw.light_direction[0] = -1;
    draw.light_direction[1] = 0;
    draw.light_direction[2] = 1;
    draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 3;
    if (mode != 2 && mode != 4) {
      draw.shadow = SceneDirectionalShadow{};
      draw.shadow->resolution = mode == 3 ? 512U : 256U;
      draw.shadow->lightViewProjection[0] = 0.5F;
      draw.shadow->lightViewProjection[2] =
          0.5F; // x'=0.5(x+z): separated camera silhouettes cast offset shadows.
    }
    if (mode == 4) {
      draw.lightingStyle = SceneLightingStyle{};
      draw.lightingStyle->lightTint = {0.3F, 1, 0.3F};
    }
    return draw;
  }
};
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &left, const Rgb &right) {
  if (mode == 0 || mode == 3)
    return left[0] < 10 && right[0] > 100;
  if (mode == 1)
    return left[0] > 100 && right[0] < 10;
  if (mode == 4)
    return left[1] > left[0] + 25 && right[1] > right[0] + 25;
  return left[0] > 100 && right[0] > 100;
}
} // namespace PbrShadowFixtures
