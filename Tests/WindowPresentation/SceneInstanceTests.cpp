#include "Nexora/Presentation/Surface.h"
#include "PbrEnvironmentFixtures.h"
#include "PbrMaterialUpload.h"
#include "SceneInstanceUpload.h"
#include "SceneTextureMipmaps.h"
#include "ToneParametersUpload.h"

#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {
using namespace Nexora::Presentation;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
bool Near(float a, float b) { return std::abs(a - b) < 1e-5F; }
void Run() {
  const auto linearUpload = PbrEnvironmentFixtures::uploads[2];
  Require(ValidateSceneLinearTexture(linearUpload), "valid RGBA16F mip chain rejected");
  for (const auto width : {0U, 3U, 512U}) {
    auto invalid = linearUpload;
    invalid.width = width;
    Require(!ValidateSceneLinearTexture(invalid), "invalid linear texture dimensions accepted");
  }
  auto invalidLinear = linearUpload;
  invalidLinear.mipLevels = 3;
  Require(!ValidateSceneLinearTexture(invalidLinear), "excess linear mip levels accepted");
  invalidLinear = linearUpload;
  invalidLinear.pixels = linearUpload.pixels.first(linearUpload.pixels.size() - 1);
  Require(!ValidateSceneLinearTexture(invalidLinear), "truncated linear mip payload accepted");
  for (const auto id : std::array<std::uint64_t, 3>{0, UINT64_MAX - 1, UINT64_MAX}) {
    invalidLinear = linearUpload;
    invalidLinear.textureId = id;
    Require(!ValidateSceneLinearTexture(invalidLinear), "reserved linear texture ID accepted");
  }
  for (const unsigned half : {0x8000U, 0x7c00U, 0x7e00U}) {
    std::array<std::byte, 8> bytes{};
    bytes[0] = static_cast<std::byte>(half & 255U);
    bytes[1] = static_cast<std::byte>(half >> 8);
    Require(!ValidateSceneLinearTexture({10, 1, 1, 1, bytes}), "negative/nonfinite half accepted");
  }
  SceneDrawData environmentDraw{};
  environmentDraw.pbr = true;
  environmentDraw.linearTextureUploads = PbrEnvironmentFixtures::uploads;
  environmentDraw.environment = SceneEnvironment{81, 83, 84, 1.0F, 0.0F, 2};
  Require(ValidatePbrData(environmentDraw), "bounded environment rejected");
  Require(ValidateEnvironmentResidency(environmentDraw, [](std::uint64_t) { return 0U; }),
          "same-submission environment uploads rejected");
  Require(!ValidateEnvironmentResidency(environmentDraw, [](std::uint64_t) { return 1U; }),
          "resident prefilter level mismatch accepted");
  for (const auto intensity : {-1.0F, 33.0F, std::numeric_limits<float>::quiet_NaN()}) {
    environmentDraw.environment->intensity = intensity;
    Require(!ValidatePbrData(environmentDraw), "invalid environment intensity accepted");
  }
  environmentDraw.environment->intensity = 1;
  environmentDraw.environment->rotationRadians = 7;
  Require(!ValidatePbrData(environmentDraw), "unbounded environment rotation accepted");
  environmentDraw.environment->rotationRadians = 0;
  environmentDraw.environment->specularTextureId = 0;
  Require(!ValidatePbrData(environmentDraw), "missing environment identity accepted");
  environmentDraw.environment->specularTextureId = 83;
  environmentDraw.environment->specularMipLevels = 0;
  Require(!ValidatePbrData(environmentDraw), "empty prefilter chain accepted");
  environmentDraw.environment->specularMipLevels = 2;
  std::array duplicateLinear{linearUpload, linearUpload};
  environmentDraw.linearTextureUploads = duplicateLinear;
  Require(!ValidatePbrData(environmentDraw), "duplicate linear upload accepted");
  environmentDraw.linearTextureUploads = {};
  environmentDraw.pbr = false;
  Require(!ValidatePbrData(environmentDraw), "environment accepted by legacy shading");
  std::vector<SceneMaterial> materials(64);
  std::array materialBatches{SceneMeshBatch{0, 3, 0, 1, 63}};
  Require(ValidateSceneMaterials(materials, materialBatches), "maximum material palette rejected");
  materials.emplace_back();
  Require(!ValidateSceneMaterials(materials, materialBatches),
          "unbounded material palette accepted");
  materials.resize(64);
  materialBatches[0].materialIndex = 64;
  Require(!ValidateSceneMaterials(materials, materialBatches), "invalid material slot accepted");
  materialBatches[0].materialIndex = 0;
  for (const auto invalid : {-0.1F, 1.1F, std::numeric_limits<float>::quiet_NaN(),
                             std::numeric_limits<float>::infinity()}) {
    materials[0].baseColor[0] = invalid;
    Require(!ValidateSceneMaterials(materials, materialBatches), "invalid material color accepted");
  }
  materials[0] = {};
  materials[0].baseColor[3] = 0.5F;
  Require(!ValidateSceneMaterials(materials, materialBatches), "unsupported alpha accepted");
  materials[0] = {};
  materials[0].textureId = UINT64_MAX;
  Require(!ValidateSceneMaterials(materials, materialBatches), "reserved texture ID accepted");
  SceneDrawData legacyDraw;
  legacyDraw.base_color[0] = 0.25F;
  legacyDraw.textureId = 42;
  const auto resolved = ResolveSceneMaterial(legacyDraw, 0);
  Require(resolved.baseColor[0] == 0.25F && resolved.textureId == 42,
          "legacy material selection changed");
  materialBatches[0].materialIndex = 1;
  Require(!ValidateSceneMaterials({}, materialBatches), "nonzero legacy material slot accepted");
  std::array<SceneVertex, 1> pbrVertices{{{{0, 0, 0}, {0, 0, 1}, {0, 0}, {1, 0, 0, 1}}}};
  SceneDrawData pbr;
  pbr.pbr = true;
  pbr.vertices = pbrVertices;
  Require(ValidatePbrData(pbr), "valid PBR geometry rejected");
  pbr.hdr = true;
  Require(!ValidatePbrData(pbr), "direct HDR accepted");
  pbr.offscreen = true;
  Require(ValidatePbrData(pbr), "offscreen HDR rejected");
  pbr.postProcessAntiAliasing = true;
  Require(ValidatePbrData(pbr), "valid HDR anti-aliasing rejected");
  pbr.hdr = false;
  Require(!ValidatePbrData(pbr), "non-HDR anti-aliasing accepted");
  pbr.hdr = true;
  pbr.postProcessAntiAliasing = false;
  const auto aaPacked = PackToneParameters(1, false, SceneBloom{0, 1, 12}, SceneColorGrade{}, 640,
                                           480, SceneDepthOfField{10, 0, 12}, true);
  const auto defaultTone =
      PackToneParameters(1, false, SceneBloom{0, 1, 12}, SceneColorGrade{}, 640, 480);
  Require(sizeof(aaPacked) == 64 && aaPacked[12] == 1.0F / 640 && aaPacked[13] == 1.0F / 480 &&
              aaPacked[14] == 1 && aaPacked[15] == 0 && defaultTone[14] == 0,
          "anti-aliasing tone packet layout/default changed");
  pbr.depthOfField = SceneDepthOfField{3, 1, 12};
  Require(ValidatePbrData(pbr), "valid HDR focus rejected");
  for (const float invalid : {0.0F, -1.0F, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
    pbr.depthOfField->focusDistance = invalid;
    Require(!ValidatePbrData(pbr), "invalid focus distance accepted");
  }
  pbr.depthOfField = SceneDepthOfField{3, 5, 12};
  Require(!ValidatePbrData(pbr), "unbounded focus strength accepted");
  pbr.depthOfField = SceneDepthOfField{3, 1, 33};
  Require(!ValidatePbrData(pbr), "unbounded focus radius accepted");
  pbr.depthOfField = SceneDepthOfField{3, 1, 12};
  pbr.hdr = false;
  Require(!ValidatePbrData(pbr), "non-HDR focus accepted");
  pbr.hdr = true;
  pbr.depthOfField.reset();
  pbr.cameraPosition = {0, 2, 4};
  pbr.planarReflection = ScenePlanarReflection{};
  Require(ValidatePbrData(pbr), "valid bounded planar reflection rejected");
  for (const auto count : {0U, 3U}) {
    pbr.planarReflection->regionCount = count;
    Require(!ValidatePbrData(pbr), "invalid mirror region count accepted");
  }
  pbr.planarReflection = ScenePlanarReflection{};
  for (const float invalid : {0.0F, -1.0F, std::numeric_limits<float>::quiet_NaN()}) {
    pbr.planarReflection->regions[0].radiusX = invalid;
    Require(!ValidatePbrData(pbr), "invalid mirror radius accepted");
  }
  pbr.planarReflection = ScenePlanarReflection{};
  for (const float invalid : {-0.01F, 0.21F, std::numeric_limits<float>::infinity(),
                              std::numeric_limits<float>::quiet_NaN()}) {
    pbr.planarReflection->shorelineVariation = invalid;
    Require(!ValidatePbrData(pbr), "invalid shoreline variation accepted");
  }
  pbr.planarReflection->shorelineVariation = 0.18F;
  Require(ValidatePbrData(pbr), "bounded shoreline variation rejected");
  Require(PackPbrMaterial(pbr, SceneMaterial{}, false)[77] == 0.18F,
          "shoreline upload differs from shader constants");
  pbr.planarReflection = ScenePlanarReflection{};
  pbr.planarReflection->reflectance = 1.1F;
  Require(!ValidatePbrData(pbr), "unbounded mirror reflectance accepted");
  pbr.planarReflection = ScenePlanarReflection{};
  pbr.planarReflection->planeHeight = std::numeric_limits<float>::infinity();
  Require(!ValidatePbrData(pbr), "nonfinite mirror plane accepted");
  pbr.planarReflection = ScenePlanarReflection{};
  pbr.cameraPosition[1] = 0;
  Require(!ValidatePbrData(pbr), "camera on mirror plane accepted");
  pbr.cameraPosition[1] = 2;
  std::array<SceneMaterial, 1> reflectionMaterials{};
  reflectionMaterials[0].reflectionRole = SceneReflectionRole::ReflectedGeometry;
  pbr.materials = reflectionMaterials;
  Require(!ValidatePbrData(pbr), "reflected shadow caster accepted");
  reflectionMaterials[0].castsShadow = false;
  Require(ValidatePbrData(pbr), "valid reflected material rejected");
  const auto reflectionPacked = PackPbrMaterial(pbr, reflectionMaterials[0], false);
  Require(reflectionPacked[60] == 0 && reflectionPacked[61] == 2 && reflectionPacked[62] == 0.04F &&
              reflectionPacked[63] == 1 && reflectionPacked[66] == 1 && reflectionPacked[67] == 1,
          "mirror material upload layout differs from shader constants");
  pbr.planarReflection.reset();
  Require(!ValidatePbrData(pbr), "mirror material without plane accepted");
  pbr.materials = {};
  reflectionMaterials[0].reflectionRole = SceneReflectionRole::None;
  reflectionMaterials[0].opacity = 0.5F;
  pbr.materials = reflectionMaterials;
  Require(ValidatePbrData(pbr), "valid HDR blend rejected");
  pbr.hdr = false;
  Require(!ValidatePbrData(pbr), "non-HDR blend accepted");
  pbr.hdr = true;
  reflectionMaterials[0].castsShadow = true;
  Require(!ValidatePbrData(pbr), "translucent shadow caster accepted");
  reflectionMaterials[0].castsShadow = false;
  for (const float invalid : {-1.0F, 1.1F, std::numeric_limits<float>::quiet_NaN()}) {
    reflectionMaterials[0].opacity = invalid;
    Require(!ValidateSceneMaterials(reflectionMaterials, {}), "invalid blend coverage accepted");
  }
  reflectionMaterials[0].opacity = 0.5F;
  for (const float invalid : {-1.0F, 1.1F, std::numeric_limits<float>::quiet_NaN()}) {
    reflectionMaterials[0].transparencyTint[0] = invalid;
    Require(!ValidateSceneMaterials(reflectionMaterials, {}), "invalid transparency tint accepted");
  }
  reflectionMaterials[0].transparencyTint = {0.2F, 0.4F, 0.6F};
  const auto translucentPacked = PackPbrMaterial(pbr, reflectionMaterials[0], false);
  Require(translucentPacked[72] == 0.5F && translucentPacked[73] == 0.2F &&
              translucentPacked[74] == 0.4F && translucentPacked[75] == 0.6F,
          "transparency upload layout differs from shader constants");
  const std::array<std::byte, 8> mipChecker{std::byte{0},   std::byte{0},   std::byte{0},
                                            std::byte{255}, std::byte{255}, std::byte{255},
                                            std::byte{255}, std::byte{255}};
  const UiTextureUpload mipSource{930, 2, 1, 8, mipChecker};
  const auto colorMips = BuildSceneTextureMipmaps(mipSource, SceneMipSemantic::Srgb);
  const auto linearMips = BuildSceneTextureMipmaps(mipSource, SceneMipSemantic::Linear);
  Require(colorMips.levels == 2 && colorMips.bytes.size() == 12 &&
              colorMips.bytes[8] == std::byte{188} && colorMips.bytes[11] == std::byte{255} &&
              linearMips.bytes[8] == std::byte{128},
          "mip filtering did not preserve linear color/data");
  const std::array<std::byte, 8> mipNormals{std::byte{255}, std::byte{128}, std::byte{255},
                                            std::byte{255}, std::byte{0},   std::byte{128},
                                            std::byte{255}, std::byte{255}};
  const auto normalMips =
      BuildSceneTextureMipmaps({931, 2, 1, 8, mipNormals}, SceneMipSemantic::Normal);
  Require(normalMips.bytes[8] == std::byte{128} && normalMips.bytes[9] == std::byte{128} &&
              normalMips.bytes[10] == std::byte{255},
          "normal mip filtering did not renormalize vectors");
  // A minified half-alpha silhouette would lose its thinner lobes under plain averaging.
  // Colors under zero coverage must not introduce magenta fringes into the green leaves.
  std::array<std::byte, 64> cutoutPixels{};
  for (unsigned y = 0; y < 4; ++y)
    for (unsigned x = 0; x < 4; ++x) {
      const bool opaque = (y < 2 && x < 2) || (y < 2 && x == 2) || (y == 2 && x % 2 == 0);
      const auto offset = (y * 4 + x) * 4;
      cutoutPixels[offset] = cutoutPixels[offset + 2] = opaque ? std::byte{0} : std::byte{255};
      cutoutPixels[offset + 1] = opaque ? std::byte{255} : std::byte{0};
      cutoutPixels[offset + 3] = opaque ? std::byte{128} : std::byte{0};
    }
  const auto cutoutMips =
      BuildSceneTextureMipmaps({932, 4, 4, 16, cutoutPixels}, SceneMipSemantic::SrgbCutoutHalf);
  Require(cutoutMips.levels == 3 && cutoutMips.bytes.size() == 84 &&
              std::equal(cutoutPixels.begin(), cutoutPixels.end(), cutoutMips.bytes.begin()),
          "cutout mip bounds or authored level changed");
  unsigned coveredCutout = 0;
  for (unsigned i = 0; i < 4; ++i) {
    const auto offset = 64 + i * 4;
    Require(cutoutMips.bytes[offset] == std::byte{0} &&
                cutoutMips.bytes[offset + 1] == std::byte{255} &&
                cutoutMips.bytes[offset + 2] == std::byte{0},
            "transparent colors polluted filtered foliage");
    coveredCutout += std::to_integer<unsigned>(cutoutMips.bytes[offset + 3]) >= 128;
  }
  Require(coveredCutout == 2, "minified foliage silhouette coverage was not preserved");
  reflectionMaterials[0].textureId = 930;
  Require(ResolveSceneMipSemantic(pbr, 930) == SceneMipSemantic::Srgb,
          "opaque color mip role lost");
  reflectionMaterials[0].alphaCutoff = 0.5F;
  Require(ResolveSceneMipSemantic(pbr, 930) == SceneMipSemantic::SrgbCutoutHalf,
          "half-cutoff foliage lost coverage-aware mip role");
  reflectionMaterials[0].alphaCutoff = 0.4F;
  Require(ResolveSceneMipSemantic(pbr, 930) == SceneMipSemantic::None,
          "unsupported cutout threshold was mip filtered");
  reflectionMaterials[0].alphaCutoff = 0.5F;
  reflectionMaterials[0].unlit = true;
  Require(ResolveSceneMipSemantic(pbr, 930) == SceneMipSemantic::None,
          "unlit cutout atlas was mip filtered");
  reflectionMaterials[0].unlit = false;
  reflectionMaterials[0].alphaCutoff = 0;
  reflectionMaterials[0].normalTextureId = 930;
  Require(ResolveSceneMipSemantic(pbr, 930) == SceneMipSemantic::None,
          "ambiguous mip role accepted");
  reflectionMaterials[0].normalTextureId = reflectionMaterials[0].textureId = 0;
  reflectionMaterials[0].refractionIndex = 1.5F;
  reflectionMaterials[0].refractionThickness = 0.7F;
  Require(ValidateSceneMaterials(reflectionMaterials, {}) && ValidatePbrData(pbr) &&
              HasSceneRefraction(pbr),
          "valid refractive material rejected");
  reflectionMaterials[0].refractionFrontSurfaceOnly = true;
  Require(ValidatePbrData(pbr), "valid closed-glass front filtering rejected");
  Require(PackPbrMaterial(pbr, reflectionMaterials[0], false)[78] == 1,
          "front-surface flag not packed into reserved slot");
  reflectionMaterials[0].refractionIndex = 1;
  Require(!ValidatePbrData(pbr), "front filtering without refraction index accepted");
  reflectionMaterials[0].refractionIndex = 1.5F;
  reflectionMaterials[0].refractionThickness = 0;
  Require(!ValidatePbrData(pbr), "front filtering without slab thickness accepted");
  reflectionMaterials[0].refractionThickness = 0.7F;
  reflectionMaterials[0].refractionFrontSurfaceOnly = false;
  const auto refractionPacked = PackPbrMaterial(pbr, reflectionMaterials[0], false, true, 640, 480);
  Require(refractionPacked[88] == 1.5F && refractionPacked[89] == 0.7F &&
              refractionPacked[90] == 1.0F / 640 && refractionPacked[91] == 1.0F / 480,
          "refraction upload target dimensions differ from shader constants");
  for (const float invalid : {0.9F, 2.6F, std::numeric_limits<float>::quiet_NaN()}) {
    reflectionMaterials[0].refractionIndex = invalid;
    Require(!ValidateSceneMaterials(reflectionMaterials, {}), "invalid refraction index accepted");
  }
  reflectionMaterials[0].refractionIndex = 1.5F;
  for (const float invalid : {-0.1F, 1.1F, std::numeric_limits<float>::quiet_NaN()}) {
    reflectionMaterials[0].refractionThickness = invalid;
    Require(!ValidateSceneMaterials(reflectionMaterials, {}),
            "invalid refraction thickness accepted");
  }
  reflectionMaterials[0].refractionThickness = 0.7F;
  reflectionMaterials[0].opacity = 1;
  Require(!ValidatePbrData(pbr), "opaque refraction accepted");
  reflectionMaterials[0].opacity = 0.5F;
  reflectionMaterials[0].unlit = true;
  Require(!ValidatePbrData(pbr), "unlit refraction accepted");
  reflectionMaterials[0].unlit = false;
  pbr.hdr = false;
  Require(!ValidatePbrData(pbr), "non-HDR refraction accepted");
  pbr.hdr = true;
  reflectionMaterials[0].refractionIndex = 1;
  reflectionMaterials[0].refractionThickness = 0;
  Require(!HasSceneRefraction(pbr), "default material requests refraction target");
  SceneDrawData pointDraw{};
  pointDraw.pbr = pointDraw.hdr = pointDraw.offscreen = true;
  const auto noPoint = PackPbrMaterial(pointDraw, SceneMaterial{}, false);
  for (unsigned i = 92; i < 100; ++i)
    Require(noPoint[i] == 0, "default scene carries point light constants");
  pointDraw.pointLight = ScenePointLight{{-1, 2, 3}, {0.3F, 8, 12}, 4.5F};
  Require(ValidatePbrData(pointDraw), "valid point light rejected");
  const auto pointPacked = PackPbrMaterial(pointDraw, SceneMaterial{}, false);
  Require(pointPacked[92] == -1 && pointPacked[93] == 2 && pointPacked[94] == 3 &&
              pointPacked[95] == 4.5F && pointPacked[96] == 0.3F && pointPacked[97] == 8 &&
              pointPacked[98] == 12 && pointPacked[99] == 0,
          "point light constants differ from shader layout");
  for (const float invalid : {0.0F, 0.09F, 64.01F, std::numeric_limits<float>::quiet_NaN()}) {
    pointDraw.pointLight->radius = invalid;
    Require(!ValidatePbrData(pointDraw), "invalid point light radius accepted");
  }
  pointDraw.pointLight = ScenePointLight{};
  for (const float invalid : {10001.0F, std::numeric_limits<float>::infinity()}) {
    pointDraw.pointLight->position[0] = invalid;
    Require(!ValidatePbrData(pointDraw), "invalid point light position accepted");
  }
  pointDraw.pointLight = ScenePointLight{};
  for (const float invalid : {-0.1F, 32.1F, std::numeric_limits<float>::quiet_NaN()}) {
    pointDraw.pointLight->radiance[0] = invalid;
    Require(!ValidatePbrData(pointDraw), "invalid point radiance accepted");
  }
  pointDraw.pointLight = ScenePointLight{};
  pointDraw.hdr = false;
  Require(!ValidatePbrData(pointDraw), "non-HDR point light accepted");
  pointDraw.pointLight.reset();
  Require(ValidatePbrData(pointDraw), "default scene lighting changed");

  pbr.materials = {};
  pbr.materials = reflectionMaterials;
  for (const float invalid : {-1.0F, 17.0F, std::numeric_limits<float>::quiet_NaN()}) {
    reflectionMaterials[0].worldTextureScale = invalid;
    Require(!ValidateSceneMaterials(reflectionMaterials, {}),
            "invalid world texture scale accepted");
  }
  reflectionMaterials[0].worldTextureScale = 0.5F;
  reflectionMaterials[0].opacity = 1;
  const auto mappedPacked = PackPbrMaterial(pbr, reflectionMaterials[0], false);
  Require(mappedPacked[76] == 0.5F && mappedPacked[77] == 0 && mappedPacked[78] == 0 &&
              mappedPacked[79] == 0,
          "world mapping layout differs from shader constants");
  reflectionMaterials[0].twoSidedLighting = true;
  Require(ValidatePbrData(pbr) && PackPbrMaterial(pbr, reflectionMaterials[0], false)[79] == 1,
          "two-sided lighting packet rejected");
  reflectionMaterials[0].unlit = true;
  Require(!ValidatePbrData(pbr), "unlit two-sided lighting accepted");
  reflectionMaterials[0].unlit = false;
  reflectionMaterials[0].worldTextureScale = 0;
  pbr.pbr = false;
  pbr.hdr = false;
  Require(!ValidatePbrData(pbr), "Lambert two-sided lighting accepted");
  reflectionMaterials[0].twoSidedLighting = false;
  reflectionMaterials[0].worldTextureScale = 0.5F;
  Require(!ValidatePbrData(pbr), "Lambert world mapping accepted");
  pbr.pbr = pbr.hdr = true;
  pbr.materials = {};
  pbr.atmosphere = SceneAtmosphere{};
  Require(ValidatePbrData(pbr), "valid HDR atmosphere rejected");
  for (const float invalid : {-1.0F, 1.1F, std::numeric_limits<float>::quiet_NaN()}) {
    pbr.atmosphere->strength = invalid;
    Require(!ValidatePbrData(pbr), "invalid atmosphere strength accepted");
  }
  pbr.atmosphere = SceneAtmosphere{};
  for (const float invalid : {-1.0F, 33.0F, std::numeric_limits<float>::infinity()}) {
    pbr.atmosphere->color[0] = invalid;
    Require(!ValidatePbrData(pbr), "invalid atmosphere radiance accepted");
  }
  pbr.atmosphere = SceneAtmosphere{};
  pbr.atmosphere->endDistance = pbr.atmosphere->startDistance;
  Require(!ValidatePbrData(pbr), "empty atmosphere range accepted");
  pbr.atmosphere->startDistance = -1;
  Require(!ValidatePbrData(pbr), "negative atmosphere start accepted");
  pbr.atmosphere = SceneAtmosphere{{0.5F, 1, 2}, 0.5F, 10, 50};
  const auto fogPacked = PackPbrMaterial(pbr, SceneMaterial{}, false);
  Require(fogPacked[80] == 0.5F && fogPacked[82] == 2 && fogPacked[83] == 0.5F &&
              fogPacked[84] == 10 && fogPacked[85] == 50 && fogPacked[86] == 0 &&
              fogPacked[87] == 0,
          "atmosphere packet differs from shader constants");
  pbr.hdr = false;
  Require(!ValidatePbrData(pbr), "non-HDR atmosphere accepted");
  pbr.hdr = true;
  pbr.atmosphere.reset();
  pbr.shadow = SceneDirectionalShadow{};
  Require(ValidatePbrData(pbr), "valid shadow rejected");
  pbr.shadow->resolution = 300;
  Require(!ValidatePbrData(pbr), "non-tier shadow resolution accepted");
  pbr.shadow->resolution = 1024;
  pbr.shadow->slopeBias = -1;
  Require(!ValidatePbrData(pbr), "negative shadow bias accepted");
  pbr.shadow->slopeBias = 0.0015F;
  pbr.shadow->lightViewProjection[0] = std::numeric_limits<float>::infinity();
  Require(!ValidatePbrData(pbr), "nonfinite light matrix accepted");
  pbr.shadow.reset();
  pbr.lightingStyle = SceneLightingStyle{};
  Require(ValidatePbrData(pbr), "valid style rejected");
  pbr.lightingStyle->shadowTint[0] = -1;
  Require(!ValidatePbrData(pbr), "negative shadow tint accepted");
  pbr.lightingStyle->shadowTint[0] = 0.5F;
  pbr.lightingStyle->rampSoftness = 0;
  Require(!ValidatePbrData(pbr), "unbounded hard ramp accepted");
  pbr.lightingStyle.reset();
  pbr.exposure = -1;
  Require(!ValidatePbrData(pbr), "negative exposure accepted");
  pbr.exposure = 33;
  Require(!ValidatePbrData(pbr), "unbounded exposure accepted");
  pbr.exposure = std::numeric_limits<float>::quiet_NaN();
  Require(!ValidatePbrData(pbr), "NaN exposure accepted");
  pbr.exposure = 1;
  pbr.pbr = false;
  Require(!ValidatePbrData(pbr), "Lambert HDR accepted");
  pbr.pbr = true;
  pbr.hdr = false;
  pbr.offscreen = false;
  pbr.base_color[3] = 0.5F;
  Require(!ValidatePbrData(pbr), "translucent legacy color accepted as opaque PBR");
  pbr.base_color[3] = 1;
  pbrVertices[0].tangent[0] = 0;
  pbrVertices[0].tangent[2] = 1;
  Require(!ValidatePbrData(pbr), "parallel normal/tangent accepted");
  pbrVertices[0].tangent[0] = 1;
  pbrVertices[0].tangent[2] = 0;
  pbr.cameraPosition[0] = std::numeric_limits<float>::quiet_NaN();
  Require(!ValidatePbrData(pbr), "nonfinite PBR camera accepted");
  SceneInstance instance;
  Require(ValidateSceneInstance(instance), "identity instance rejected");
  instance.model_transform =
      std::array<float, 16>{-2, 1, 0, 7, 0, 3, 0, -4, 0.5F, 0, 1, 2, 0, 0, 0, 1};
  // Override means stale/invalid TRS is ignored, while tint is always validated.
  instance.translation[0] = std::numeric_limits<float>::quiet_NaN();
  instance.scale[0] = 0;
  instance.rotation[3] = 0;
  SceneInstanceUpload packed;
  Require(ValidateSceneInstance(instance) && PackSceneInstance(instance, packed),
          "valid affine override rejected because of unused TRS");
  const float point[4]{1, 2, 3, 1};
  const float expected[3]{7, 2, 5.5F};
  for (std::size_t row = 0; row < 3; ++row) {
    float actual = 0;
    for (std::size_t column = 0; column < 4; ++column)
      actual += packed.model[row][column] * point[column];
    Require(Near(actual, expected[row]), "row-major model transform changed the affine point");
  }
  Require(packed.normal[0][3] == -1, "mirrored affine tangent sign was lost");
  // A^-T times +Z is (0.25, -1/12, 1); it remains orthogonal to both transformed tangents.
  Require(Near(packed.normal[0][2], 0.25F) && Near(packed.normal[1][2], -1.0F / 12) &&
              Near(packed.normal[2][2], 1),
          "mirrored/sheared inverse-transpose normal is wrong");
  for (std::size_t tangent = 0; tangent < 2; ++tangent) {
    float dot = 0;
    for (std::size_t row = 0; row < 3; ++row)
      dot += packed.normal[row][2] * packed.model[row][tangent];
    Require(Near(dot, 0), "normal lost orthogonality to a transformed surface tangent");
  }
  const auto valid = instance;
  const auto retained = packed;
  const auto rejects = [&](SceneInstance invalid) {
    Require(!ValidateSceneInstance(invalid) && !PackSceneInstance(invalid, packed),
            "invalid affine instance accepted");
    for (std::size_t row = 0; row < 3; ++row)
      for (std::size_t column = 0; column < 4; ++column)
        Require(packed.model[row][column] == retained.model[row][column] &&
                    packed.normal[row][column] == retained.normal[row][column],
                "failed packing changed caller output");
  };
  for (std::size_t index = 0; index < 16; ++index) {
    auto invalid = valid;
    (*invalid.model_transform)[index] = std::numeric_limits<float>::infinity();
    rejects(invalid);
  }
  for (const auto index : {12, 13, 14, 15}) {
    auto invalid = valid;
    (*invalid.model_transform)[index] = index == 15 ? 2 : 0.1F;
    rejects(invalid);
  }
  auto singular = valid;
  (*singular.model_transform)[4] = (*singular.model_transform)[0];
  (*singular.model_transform)[5] = (*singular.model_transform)[1];
  (*singular.model_transform)[6] = (*singular.model_transform)[2];
  rejects(singular);
  // All entries are exact binary32 integers, and row 3 is exactly row 1 + row 2.
  // A rounded double cofactor expansion incorrectly produces determinant -32768.
  singular.model_transform =
      std::array<float, 16>{7985202, -7733472,  -2128495, 0, -3315472, -2593841, -4390545, 0,
                            4669730, -10327313, -6519040, 0, 0,        0,        0,        1};
  rejects(singular);
  SceneInstance cancellation;
  // Integer row additions from identity give determinant exactly 1. Rounded double
  // cofactor summation produces zero; rejecting it would discard a valid affine instance.
  cancellation.model_transform =
      std::array<float, 16>{-103399, -1557824, 519658,   0, 319990, 3082953, -1028862, 0,
                            208866,  3146810,  -1049711, 0, 0,      0,       0,        1};
  SceneInstanceUpload cancellation_upload;
  Require(AffineDeterminant(*cancellation.model_transform) == 1 &&
              PackSceneInstance(cancellation, cancellation_upload) &&
              ValidateSceneInstance(cancellation),
          "exactly invertible cancellation matrix rejected");
  Require(cancellation_upload.normal[0][3] == 1, "cancelling affine tangent sign was rounded");
  // The cofactor direction remains orthogonal despite the nearly dependent integer rows.
  for (std::size_t tangent = 0; tangent < 2; ++tangent) {
    double dot = 0, magnitude = 0;
    for (std::size_t row = 0; row < 3; ++row) {
      const double term = static_cast<double>(cancellation_upload.normal[row][2]) *
                          cancellation_upload.model[row][tangent];
      dot += term;
      magnitude += std::abs(term);
    }
    Require(std::abs(dot) <= magnitude * 1e-6, "cancellation matrix normal lost orthogonality");
  }
  auto bad_color = valid;
  bad_color.color[2] = std::numeric_limits<float>::quiet_NaN();
  rejects(bad_color);
  auto unrepresentable = valid;
  unrepresentable.model_transform = std::array<float, 16>{
      std::numeric_limits<float>::denorm_min(), 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  rejects(unrepresentable);
  SceneInstance legacy;
  legacy.translation[0] = 3;
  legacy.scale[0] = -2;
  legacy.scale[1] = 3;
  legacy.rotation[2] = legacy.rotation[3] = std::sqrt(0.5F);
  Require(PackSceneInstance(legacy, packed) && Near(packed.model[0][3], 3) &&
              Near(packed.model[1][0], -2) && Near(packed.model[0][1], -3) &&
              Near(packed.normal[1][0], -0.5F) && Near(packed.normal[0][1], -1.0F / 3),
          "legacy quaternion/nonuniform mirrored scale semantics changed");
  legacy.scale[0] = 0;
  Require(!ValidateSceneInstance(legacy), "legacy zero scale accepted");
  legacy = {};
  legacy.rotation[3] = 0;
  Require(!ValidateSceneInstance(legacy), "legacy nonunit quaternion accepted");
  for (const auto factor : {1e-30F, std::numeric_limits<float>::max()}) {
    SceneInstance extreme;
    extreme.model_transform =
        std::array<float, 16>{factor, 0, 0, 0, 0, factor, 0, 0, 0, 0, factor, 0, 0, 0, 0, 1};
    Require(PackSceneInstance(extreme, packed), "representable extreme affine scale rejected");
    const auto direction = packed.normal[2][2];
    Require(Near(direction, 1) && std::isfinite(direction),
            "normal packing is not bounded for stable shader normalization");
  }
  const auto identity = PackSceneInstances({});
  Require(identity && identity->size() == 1 && identity->front().model[0][0] == 1 &&
              identity->front().normal[2][2] == 1,
          "empty instance span did not select one packed identity");
  std::vector<SceneInstance> bounded(4096);
  Require(PackSceneInstances(bounded).has_value(), "maximum instance count rejected");
  bounded.emplace_back();
  Require(!PackSceneInstances(bounded), "unbounded upload accepted");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "Scene instance affine contracts passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
