#pragma once
#include "Nexora/Presentation/Surface.h"
#include <array>
#include <bit>

namespace PbrEnvironmentFixtures {
using namespace Nexora::Presentation;
static_assert(std::endian::native == std::endian::little);
// Exact binary16 fixtures: radiance 4 is deliberately outside the UNORM range.
inline constexpr std::array<std::uint16_t, 4> blueIrradiance{0, 0, 0x4248, 0x3c00};
inline constexpr std::array<std::uint16_t, 4> black{0, 0, 0, 0x3c00};
inline constexpr std::array<std::uint16_t, 12> specularMips{0x4400, 0, 0,      0x3c00, 0x4400, 0, 0,
                                                            0x3c00, 0, 0x4400, 0,      0x3c00};
inline constexpr std::array<std::uint16_t, 4> brdf{0x3c00, 0, 0, 0x3c00};
inline constexpr std::array<std::uint16_t, 32> angular{
    0x4400, 0, 0, 0x3c00, 0x4400, 0, 0, 0x3c00, 0, 0x4400, 0, 0x3c00, 0, 0x4400, 0, 0x3c00,
    0x4400, 0, 0, 0x3c00, 0x4400, 0, 0, 0x3c00, 0, 0x4400, 0, 0x3c00, 0, 0x4400, 0, 0x3c00};
inline const std::array uploads{
    SceneLinearTextureUpload{81, 1, 1, 1, std::as_bytes(std::span{blueIrradiance})},
    SceneLinearTextureUpload{82, 1, 1, 1, std::as_bytes(std::span{black})},
    SceneLinearTextureUpload{83, 2, 1, 2, std::as_bytes(std::span{specularMips})},
    SceneLinearTextureUpload{84, 1, 1, 1, std::as_bytes(std::span{brdf})},
    SceneLinearTextureUpload{85, 4, 2, 1, std::as_bytes(std::span{angular})}};

inline void Configure(SceneDrawData &draw, std::span<SceneMaterial> materials, unsigned mode) {
  materials[0] = materials[1] = {};
  draw.cameraPosition = {0, 0, 3};
  draw.light_direction[0] = draw.light_direction[1] = 0;
  draw.light_direction[2] = -1;
  draw.light_color[0] = draw.light_color[1] = draw.light_color[2] = 0;
  draw.pbr = true;
  draw.environment = SceneEnvironment{81, 82, 84, 1.0F, 0.0F, 1};
  materials[1].metallic = 1;
  if (mode > 0) {
    materials[0].metallic = 1;
    materials[0].roughness = materials[1].roughness = 0;
    draw.environment->diffuseTextureId = 82;
    draw.environment->specularTextureId = 85;
  }
  if (mode == 1) {
    draw.environment->specularTextureId = 83;
    draw.environment->specularMipLevels = 2;
    materials[1].roughness = 1;
  } else if (mode == 2) {
    draw.environment.reset();
  } else if (mode == 4) {
    draw.environment->rotationRadians = 3.14159265F;
  } else if (mode == 5) {
    draw.cameraPosition = {3, 0, 0.2F};
  }
}

template <typename Rgb> inline bool Pixels(unsigned mode, const Rgb &left, const Rgb &right) {
  if (mode == 0)
    return left[2] > 220 && left[0] < 5 && right[0] < 5 && right[1] < 5 && right[2] < 5;
  if (mode == 1)
    return left[0] > 240 && left[1] < 5 && right[1] > 240 && right[0] < 5;
  if (mode == 2)
    return left[0] < 5 && left[1] < 5 && left[2] < 5 && right[0] < 5 && right[1] < 5 &&
           right[2] < 5;
  if (mode == 4)
    return left[0] > 240 && right[0] > 240 && left[1] < 5 && right[1] < 5;
  if (mode == 5)
    return left[0] > 200 && right[0] > 200 && left[1] > 200 && right[1] > 200;
  return left[1] > 240 && right[1] > 240 && left[0] < 5 && right[0] < 5;
}
} // namespace PbrEnvironmentFixtures
