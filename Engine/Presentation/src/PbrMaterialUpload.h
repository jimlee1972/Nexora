#pragma once
#include "Nexora/Presentation/Surface.h"
#include <algorithm>
#include <array>

namespace Nexora::Presentation {
// PBR validation is shared; resident map references and native capabilities remain adapter-owned.
[[nodiscard]] inline bool ValidatePbrData(const SceneDrawData &draw) noexcept {
  if (!draw.pbr)
    return draw.linearTextureUploads.empty() && !draw.environment;
  if (draw.linearTextureUploads.size() > 16)
    return false;
  for (std::size_t i = 0; i < draw.linearTextureUploads.size(); ++i) {
    const auto &upload = draw.linearTextureUploads[i];
    if (!ValidateSceneLinearTexture(upload))
      return false;
    for (std::size_t j = 0; j < i; ++j)
      if (draw.linearTextureUploads[j].textureId == upload.textureId)
        return false;
    for (const auto &rgba : draw.textureUploads)
      if (rgba.textureId == upload.textureId)
        return false;
  }
  if (draw.environment) {
    const auto &environment = *draw.environment;
    for (const auto id :
         {environment.diffuseTextureId, environment.specularTextureId, environment.brdfTextureId})
      if (!id || id >= UINT64_MAX - 1)
        return false;
    if (!std::isfinite(environment.intensity) || environment.intensity < 0 ||
        environment.intensity > 32 || !std::isfinite(environment.rotationRadians) ||
        std::abs(environment.rotationRadians) > 6.283186F || !environment.specularMipLevels ||
        environment.specularMipLevels > 9)
      return false;
  }
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

// Native adapters supply resident metadata; an upload in the same submission is also valid.
template <typename Lookup>
[[nodiscard]] inline bool ValidateEnvironmentResidency(const SceneDrawData &draw, Lookup lookup) {
  if (!draw.environment)
    return true;
  const auto resolveLevels = [&](std::uint64_t id) {
    const auto resident = lookup(id);
    if (resident)
      return resident;
    for (const auto &upload : draw.linearTextureUploads)
      if (upload.textureId == id)
        return upload.mipLevels;
    return 0U;
  };
  return resolveLevels(draw.environment->diffuseTextureId) == 1 &&
         resolveLevels(draw.environment->specularTextureId) ==
             draw.environment->specularMipLevels &&
         resolveLevels(draw.environment->brdfTextureId) == 1;
}

// Matches MaterialConstants in scene_pbr.slang: five float4s, independent of native UBO alignment.
using PbrMaterialUpload = std::array<float, 20>;
static_assert(sizeof(PbrMaterialUpload) == 80);
[[nodiscard]] inline PbrMaterialUpload PackPbrMaterial(const SceneDrawData &draw,
                                                       const SceneMaterial &material,
                                                       bool manualSrgbTransfer) noexcept {
  PbrMaterialUpload parameters{};
  std::copy(draw.cameraPosition.begin(), draw.cameraPosition.end(), parameters.begin());
  parameters[3] = manualSrgbTransfer ? 1.0F : 0.0F;
  std::copy(material.baseColor.begin(), material.baseColor.end(), parameters.begin() + 4);
  std::copy(material.emission.begin(), material.emission.end(), parameters.begin() + 8);
  parameters[11] = material.roughness;
  parameters[12] = material.metallic;
  parameters[13] = material.occlusion;
  // Bypass quantization bias in the RGBA8 flat-normal fallback.
  parameters[14] = material.normalTextureId ? material.normalScale : 0;
  if (draw.environment) {
    parameters[16] = draw.environment->intensity;
    parameters[17] = draw.environment->rotationRadians;
    parameters[18] = static_cast<float>(draw.environment->specularMipLevels - 1);
  }
  return parameters;
}
} // namespace Nexora::Presentation
