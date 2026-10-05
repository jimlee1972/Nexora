#pragma once
#include "Nexora/Presentation/Surface.h"
#include <algorithm>
#include <array>

namespace PbrReflectionFixtures {
using namespace Nexora::Presentation;
// An oblique camera must see the reflected emitter through both faces of a solid
// receiver. Moving the source must move its mirror image without replacing a texture.
struct Fixture final {
  std::array<SceneVertex, 18> vertices{};
  std::array<std::uint16_t, 18> indices{};
  std::array<SceneMaterial, 4> materials{};
  std::array<SceneInstance, 2> instances{};
  std::array<SceneMeshBatch, 3> batches{{{0, 12, 0, 1, 0}, {12, 6, 0, 1, 1}, {12, 6, 1, 1, 3}}};
  explicit Fixture(unsigned mode) {
    const std::array<std::array<float, 3>, 18> positions{{{-4, -0.01F, -4},
                                                          {4, -0.01F, -4},
                                                          {4, -0.01F, 4},
                                                          {-4, -0.01F, -4},
                                                          {4, -0.01F, 4},
                                                          {-4, -0.01F, 4},
                                                          {-4, -0.1F, -4},
                                                          {4, -0.1F, 4},
                                                          {4, -0.1F, -4},
                                                          {-4, -0.1F, -4},
                                                          {-4, -0.1F, 4},
                                                          {4, -0.1F, 4},
                                                          {-1.3F, 0.4F, -1.2F},
                                                          {-0.7F, 0.4F, -1.2F},
                                                          {-0.7F, 0.8F, -1.2F},
                                                          {-1.3F, 0.4F, -1.2F},
                                                          {-0.7F, 0.8F, -1.2F},
                                                          {-1.3F, 0.8F, -1.2F}}};
    for (std::size_t i = 0; i < vertices.size(); ++i) {
      std::copy(positions[i].begin(), positions[i].end(), vertices[i].position);
      if (i >= 12 && mode == 2)
        vertices[i].position[0] += 1;
      vertices[i].normal[1] = i < 6 ? 1.0F : -1.0F;
      vertices[i].tangent[0] = vertices[i].tangent[3] = 1;
      indices[i] = static_cast<std::uint16_t>(i);
    }
    for (auto &material : materials) {
      material.unlit = true;
      material.castsShadow = false;
      material.baseColor = {0, 0, 0, 1};
    }
    materials[0].emission = {0, 0.5F, 0};
    materials[0].reflectionRole =
        mode == 0 ? SceneReflectionRole::None : SceneReflectionRole::Receiver;
    materials[1].emission = {4, 0, 0};
    materials[3] = materials[1];
    materials[3].reflectionRole = SceneReflectionRole::ReflectedGeometry;
    instances[1].model_transform =
        std::array<float, 16>{1, 0, 0, 0, 0, -1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  }
  SceneDrawData Draw(unsigned mode) const {
    SceneDrawData draw{};
    draw.vertices = vertices;
    draw.indices = indices;
    draw.materials = materials;
    draw.instances = instances;
    draw.batches = std::span(batches).first(mode == 0 ? 2 : 3);
    draw.pbr = draw.hdr = draw.offscreen = true;
    draw.cameraPosition = {0, 2, 4};
    // Row-major orthographic view: right X, up (0,2,-1)/sqrt(5).
    const std::array matrix{0.5F,        0.0F, 0.0F, 0.0F,       0.0F,       0.5962848F,
                            -0.2981424F, 0.0F, 0.0F, -0.022473F, -0.044946F, 0.219708F,
                            0.0F,        0.0F, 0.0F, 1.0F};
    std::copy(matrix.begin(), matrix.end(), draw.model_view_projection);
    ScenePlanarReflection reflection{};
    reflection.regions[0] = {-0.8F, 0, 0.65F, 0.65F};
    draw.planarReflection = reflection;
    return draw;
  }
};
template <typename Rgb> bool Pixels(unsigned mode, const Rgb &left, const Rgb &right) {
  if (right[1] < 80 || right[0] > 40)
    return false;
  if (mode == 0)
    return left[1] > 80 && left[0] < 40;
  if (mode == 2)
    return left[0] < 80 && left[1] < 80;
  return left[0] > 120 && left[0] > left[1] + 70;
}
} // namespace PbrReflectionFixtures
