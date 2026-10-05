#include "Nexora/Presentation/Surface.h"
#include "PbrEnvironmentFixtures.h"
#include "PbrMaterialUpload.h"
#include "SceneInstanceUpload.h"

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
  pbr.materials = {};
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
