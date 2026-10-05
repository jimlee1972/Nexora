#pragma once
#include "PbrTransparencyFixtures.h"
namespace PbrRefractionFixtures {
using namespace Nexora::Presentation;
struct Fixture final {
  std::array<SceneVertex, 18> vertices{};
  std::array<std::uint16_t, 18> indices{};
  std::array<SceneMaterial, 4> materials{};
  // Deliberately submit the glass before its opaque background.
  std::array<SceneMeshBatch, 3> batches{{{12, 6, 0, 1, 3}, {0, 6, 0, 1, 0}, {6, 6, 0, 1, 1}}};
  explicit Fixture(unsigned mode) {
    const auto quad = [&](unsigned first, float left, float right, float z) {
      const std::array<std::array<float, 2>, 6> corners{{{left, -0.8F},
                                                         {right, -0.8F},
                                                         {right, 0.8F},
                                                         {left, -0.8F},
                                                         {right, 0.8F},
                                                         {left, 0.8F}}};
      for (unsigned i = 0; i < 6; ++i) {
        auto &v = vertices[first + i];
        v.position[0] = corners[i][0];
        v.position[1] = corners[i][1];
        v.position[2] = z;
        v.normal[2] = -1;
        v.tangent[0] = v.tangent[3] = 1;
      }
    };
    quad(0, -0.95F, 0, 0.7F);
    quad(6, 0, 0.95F, mode == 5 ? 0.1F : 0.7F);
    quad(12, -0.3F, 0.3F, 0.2F);
    for (unsigned i = 0; i < indices.size(); ++i)
      indices[i] = static_cast<std::uint16_t>(i);
    for (auto &material : materials) {
      material.baseColor = {0, 0, 0, 1};
      material.castsShadow = false;
    }
    materials[0].unlit = materials[1].unlit = true;
    materials[0].emission = {0, 0.5F, 0};
    materials[1].emission = mode == 5 ? std::array{0.0F, 0.0F, 0.5F} : std::array{0.5F, 0.0F, 0.0F};
    materials[3].opacity = 0.5F;
    materials[3].refractionIndex = mode == 0 ? 1 : 1.5F;
    materials[3].refractionThickness = mode == 0 || mode == 4 ? 0 : 0.7F;
    for (unsigned i = 12; i < vertices.size(); ++i) {
      auto &v = vertices[i];
      v.normal[0] = mode == 2 ? 0.6F : -0.6F;
      v.normal[2] = -0.8F;
      v.tangent[0] = 0.8F;
      v.tangent[2] = mode == 2 ? 0.6F : -0.6F;
    }
  }
  SceneDrawData Draw() const {
    SceneDrawData draw{};
    draw.vertices = vertices;
    draw.indices = indices;
    draw.materials = materials;
    draw.batches = batches;
    draw.pbr = draw.hdr = draw.offscreen = true;
    draw.cameraPosition = {0, 0, -3};
    return draw;
  }
};
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &left, const Rgb &right) {
  const auto green = [](const auto &p) {
    return p[0] < 5 &&
           std::abs(static_cast<int>(p[1]) - PbrTransparencyFixtures::Encoded(0.25F)) <= 2 &&
           p[2] < 5;
  };
  const auto red = [](const auto &p) {
    return p[1] < 5 &&
           std::abs(static_cast<int>(p[0]) - PbrTransparencyFixtures::Encoded(0.25F)) <= 2 &&
           p[2] < 5;
  };
  if (mode == 0 || mode == 4)
    return green(left) && red(right);
  if (mode == 2)
    return green(left) && green(right);
  if (mode == 5)
    return green(left) && right[0] < 5 && right[1] < 5 &&
           std::abs(static_cast<int>(right[2]) - PbrTransparencyFixtures::Encoded(0.5F)) <= 2;
  return red(left) && red(right);
}
} // namespace PbrRefractionFixtures
