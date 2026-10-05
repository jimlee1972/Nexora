#pragma once
#include "Nexora/Presentation/Surface.h"
#include <algorithm>
#include <array>

namespace Nexora::Presentation {
// PBR validation is shared; resident map references and native capabilities remain adapter-owned.
[[nodiscard]] inline bool ValidatePbrData(const SceneDrawData &draw) noexcept {
  if (!std::isfinite(draw.exposure) || draw.exposure < 0 || draw.exposure > 32 ||
      (draw.hdr && (!draw.pbr || !draw.offscreen)))
    return false;
  if (draw.colorGrade && (!draw.hdr || !std::isfinite(draw.colorGrade->saturation) ||
                          draw.colorGrade->saturation < 0 || draw.colorGrade->saturation > 2 ||
                          !std::isfinite(draw.colorGrade->contrast) ||
                          draw.colorGrade->contrast < 0 || draw.colorGrade->contrast > 2))
    return false;
  if (draw.bloom && (!draw.hdr || !std::isfinite(draw.bloom->intensity) ||
                     draw.bloom->intensity < 0 || draw.bloom->intensity > 1 ||
                     !std::isfinite(draw.bloom->threshold) || draw.bloom->threshold < 0 ||
                     draw.bloom->threshold > 32 || !std::isfinite(draw.bloom->radiusPixels) ||
                     draw.bloom->radiusPixels < 1 || draw.bloom->radiusPixels > 32))
    return false;
  if (!std::isfinite(draw.vegetationTime) || draw.vegetationTime < 0 || draw.vegetationTime > 3600)
    return false;
  if (!draw.pbr)
    for (const auto &material : draw.materials)
      if (material.alphaCutoff || material.windAmplitude || material.transmissionThickness ||
          material.unlit)
        return false;
  if (!draw.pbr)
    return draw.linearTextureUploads.empty() && !draw.environment && !draw.shadow &&
           !draw.lightingStyle;
  if (draw.shadow) {
    const auto &shadow = *draw.shadow;
    if (!draw.offscreen ||
        (shadow.resolution != 256 && shadow.resolution != 512 && shadow.resolution != 1024 &&
         shadow.resolution != 2048) ||
        !std::isfinite(shadow.normalBias) || shadow.normalBias < 0 || shadow.normalBias > 0.05F ||
        !std::isfinite(shadow.slopeBias) || shadow.slopeBias < 0 || shadow.slopeBias > 0.05F)
      return false;
    for (const auto value : shadow.lightViewProjection)
      if (!std::isfinite(value))
        return false;
  }
  if (draw.lightingStyle) {
    const auto &style = *draw.lightingStyle;
    for (const auto value : style.shadowTint)
      if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
    for (const auto value : style.lightTint)
      if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
    if (!std::isfinite(style.rampOffset) || std::abs(style.rampOffset) > 2 ||
        !std::isfinite(style.rampScale) || style.rampScale < 0 || style.rampScale > 4 ||
        !std::isfinite(style.rampSoftness) || style.rampSoftness < 0.01F ||
        style.rampSoftness > 0.5F)
      return false;
  }
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

// Matches MaterialConstants in scene_pbr.slang: fifteen float4s, independent of native UBO
// alignment.
using PbrMaterialUpload = std::array<float, 60>;
static_assert(sizeof(PbrMaterialUpload) == 240);
[[nodiscard]] inline PbrMaterialUpload PackPbrMaterial(const SceneDrawData &draw,
                                                       const SceneMaterial &material,
                                                       bool manualSrgbTransfer,
                                                       bool shadowYDown = true) noexcept {
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
  parameters[15] = draw.hdr ? 1.0F : 0.0F;
  if (draw.environment) {
    parameters[16] = draw.environment->intensity;
    parameters[17] = draw.environment->rotationRadians;
    parameters[18] = static_cast<float>(draw.environment->specularMipLevels - 1);
  }
  if (draw.shadow) {
    std::copy(draw.shadow->lightViewProjection.begin(), draw.shadow->lightViewProjection.end(),
              parameters.begin() + 20);
    parameters[36] = draw.shadow->normalBias;
    parameters[37] = draw.shadow->slopeBias;
    parameters[38] = 1.0F / static_cast<float>(draw.shadow->resolution);
    parameters[39] = shadowYDown ? -1.0F : 1.0F;
  }
  if (draw.lightingStyle) {
    std::copy(draw.lightingStyle->shadowTint.begin(), draw.lightingStyle->shadowTint.end(),
              parameters.begin() + 40);
    std::copy(draw.lightingStyle->lightTint.begin(), draw.lightingStyle->lightTint.end(),
              parameters.begin() + 44);
    parameters[43] = 1;
    parameters[48] = draw.lightingStyle->rampOffset;
    parameters[49] = draw.lightingStyle->rampScale;
    parameters[50] = draw.lightingStyle->rampSoftness;
  }
  parameters[52] = draw.vegetationTime;
  parameters[53] = material.windAmplitude;
  parameters[54] = material.alphaCutoff;
  parameters[55] = material.transmissionThickness;
  std::copy(material.transmissionColor.begin(), material.transmissionColor.end(),
            parameters.begin() + 56);
  parameters[59] = material.unlit ? 1.0F : 0.0F;
  return parameters;
}
} // namespace Nexora::Presentation
