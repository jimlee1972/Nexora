#pragma once
#include "Nexora/Presentation/Surface.h"
#include <array>
#include <cmath>
namespace PbrOcclusionFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  std::array<SceneVertex, 18> vertices{};
  std::array<std::uint16_t, 18> indices{};
  std::array<SceneMaterial, 4> materials{};
  std::array<SceneMeshBatch, 3> batches{{{0, 6, 0, 1, 0}, {6, 6, 0, 1, 1}, {12, 6, 0, 1, 3}}};
  explicit Fixture(float distanceScale = 1) {
    const auto quad = [&](unsigned first, float left, float right, float z) {
      const std::array<std::array<float, 2>, 6> corners{
          {{left, -3}, {right, -3}, {right, 3}, {left, -3}, {right, 3}, {left, 3}}};
      for (unsigned i = 0; i < 6; ++i) {
        auto &v = vertices[first + i];
        v.position[0] = corners[i][0];
        v.position[1] = corners[i][1];
        v.position[2] = z;
        v.normal[2] = v.tangent[0] = v.tangent[3] = 1;
      }
    };
    quad(0, -4, 4, -3);
    quad(6, 0, 0.5F, -2.8F);
    quad(12, 0.75F, 1.5F, -2.9F);
    for (auto &vertex : vertices)
      for (auto &component : vertex.position)
        component *= distanceScale;
    for (unsigned i = 0; i < indices.size(); ++i)
      indices[i] = static_cast<std::uint16_t>(i);
    for (auto &material : materials) {
      material.baseColor = {0, 0, 0, 1};
      material.unlit = true;
      material.castsShadow = false;
      material.emission = {0.5F, 0.5F, 0.5F};
    }
    materials[3].emission = {3, 3, 3};
  }
  SceneDrawData Draw(unsigned mode, unsigned width, unsigned height) const {
    SceneDrawData draw;
    draw.vertices = vertices;
    draw.indices = indices;
    draw.materials = materials;
    draw.batches = batches;
    draw.pbr = draw.hdr = draw.offscreen = true;
    std::fill(std::begin(draw.model_view_projection), std::end(draw.model_view_projection), 0.0F);
    const float f = 1 / std::tan(0.425F);
    draw.model_view_projection[0] = f * height / width;
    draw.model_view_projection[5] = f;
    draw.model_view_projection[10] = -300 / 299.9F;
    draw.model_view_projection[11] = -30 / 299.9F;
    draw.model_view_projection[14] = -1;
    if (mode == 1 || mode == 3 || mode == 4)
      draw.screenSpaceOcclusion = SceneScreenSpaceOcclusion{mode == 3 ? 0.0F : 1.0F};
    return draw;
  }
};
} // namespace PbrOcclusionFixtures
