#pragma once
#include "Nexora/Presentation/Surface.h"
#include <algorithm>
#include <array>

namespace Nexora::Presentation {
// PBR validation is shared; resident map references and native capabilities remain adapter-owned.
[[nodiscard]] inline bool ValidatePbrData(const SceneDrawData &draw) noexcept {
  if (!draw.pbr)
    return true;
  if (draw.materials.empty()) {
    const auto legacy = ResolveSceneMaterial(draw, 0);
    if (!ValidateSceneMaterials({&legacy, 1}, {}))
      return false;
  }
  for (const auto value : draw.cameraPosition)
    if (!std::isfinite(value))
      return false;
  for (const auto value : draw.light_color)
    if (!std::isfinite(value) || value < 0 || value > 65504)
      return false;
  for (const auto value : draw.light_direction)
    if (!std::isfinite(value))
      return false;
  if (draw.light_direction[0] == 0 && draw.light_direction[1] == 0 && draw.light_direction[2] == 0)
    return false;
  for (const auto value : draw.model_view_projection)
    if (!std::isfinite(value))
      return false;
  for (const auto &vertex : draw.vertices) {
    for (const auto value : vertex.position)
      if (!std::isfinite(value))
        return false;
    for (const auto value : vertex.uv)
      if (!std::isfinite(value))
        return false;
    for (const auto value : vertex.normal)
      if (!std::isfinite(value))
        return false;
    for (const auto value : vertex.tangent)
      if (!std::isfinite(value))
        return false;
    if (std::abs(vertex.tangent[3]) != 1)
      return false;
    double normalLength = 0, tangentLength = 0, dot = 0;
    for (std::size_t axis = 0; axis < 3; ++axis) {
      const double n = vertex.normal[axis], t = vertex.tangent[axis];
      normalLength += n * n;
      tangentLength += t * t;
      dot += n * t;
    }
    if (normalLength == 0 || tangentLength == 0 ||
        std::abs(dot) > 1e-3 * std::sqrt(normalLength * tangentLength))
      return false;
  }
  return true;
}

// Matches MaterialConstants in scene_pbr.slang: four float4s, independent of native UBO alignment.
using PbrMaterialUpload = std::array<float, 16>;
static_assert(sizeof(PbrMaterialUpload) == 64);
[[nodiscard]] inline PbrMaterialUpload PackPbrMaterial(const SceneDrawData &draw,
                                                       const SceneMaterial &material,
                                                       bool manualSrgbTransfer) noexcept {
  PbrMaterialUpload parameters{};
  std::copy(draw.cameraPosition.begin(), draw.cameraPosition.end(), parameters.begin());
  parameters[3] = manualSrgbTransfer ? 1 : 0;
  std::copy(material.baseColor.begin(), material.baseColor.end(), parameters.begin() + 4);
  std::copy(material.emission.begin(), material.emission.end(), parameters.begin() + 8);
  parameters[11] = material.roughness;
  parameters[12] = material.metallic;
  parameters[13] = material.occlusion;
  // Bypass quantization bias in the RGBA8 flat-normal fallback.
  parameters[14] = material.normalTextureId ? material.normalScale : 0;
  return parameters;
}
} // namespace Nexora::Presentation
