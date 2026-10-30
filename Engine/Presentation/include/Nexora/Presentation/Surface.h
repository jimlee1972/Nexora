#pragma once

#include "Nexora/Presentation/Api.h"
#include "Nexora/Window/Window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <thread>
#include <vector>

namespace Nexora::Presentation {

enum class PresentMode : std::uint8_t { VSync, Immediate };
enum class ColorSpace : std::uint8_t { Srgb, Hdr10 };
enum class SurfaceBackend : std::uint8_t { Automatic, Dx12, Vulkan, Metal };

struct SurfaceDescriptor final {
  Window::WindowHandle window;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint8_t framesInFlight = 2;
  PresentMode presentMode = PresentMode::VSync;
  ColorSpace colorSpace = ColorSpace::Srgb;
  SurfaceBackend backend = SurfaceBackend::Automatic;
  // Optional native profiling; default consumers allocate/record no timestamp queries.
  bool enableGpuTiming = false;
  // UI/Sprite-only surface: require hardware sRGB attachment encoding of linear input.
  // No UNORM fallback; DrawScene is unsupported. Default preserves existing mixed 3D consumers.
  bool srgbUiOutput = false;
};

enum class DriverVersionFormat : std::uint8_t { Unavailable, VulkanRaw, DxgiUmd };

// Copied observations of the selected native device, never a borrowed handle. Empty name,
// unavailable IDs and Unavailable driver format are explicit absence, not inferred host metadata.
struct SurfaceDeviceInfo final {
  std::array<char, 512> name{};
  std::uint32_t vendorId = 0;
  std::uint32_t deviceId = 0;
  bool deviceIdsAvailable = false;
  std::uint64_t driverVersion = 0;
  DriverVersionFormat driverVersionFormat = DriverVersionFormat::Unavailable;
};

enum class GpuTimingSource : std::uint8_t {
  Unavailable,
  VulkanTimestamps,
  Dx12Timestamps,
  MetalCommandBuffer
};

// Latest completed native command-buffer interval, not CPU wall time or display latency.
// Read on the existing render thread. A missing value explicitly denotes unavailable timing.
struct SurfaceGpuTiming final {
  GpuTimingSource source{GpuTimingSource::Unavailable};
  std::uint64_t completedSubmission{};
  std::optional<double> milliseconds;
};

// Copied observations of geometry-prefix reuse, independent of instance/material uploads.
// Unavailable backends leave available false. retainedCpuBytes counts owned comparison payloads,
// not device memory; uploads/hits count actual writes/skipped writes, including later draw
// failures.
struct SceneGeometryUploadCacheDiagnostics final {
  bool available{};
  std::uint64_t uploads{}, hits{}, retainedCpuBytes{};
};

// Immutable mesh buffers. Native allocation bytes include the adapter's allocation requirements;
// they are not a device-local VRAM measurement. Comparison keys own separate CPU payloads.
struct SceneGeometryResidencyDiagnostics final {
  bool available{};
  std::uint64_t uploads{}, hits{}, evictions{}, retainedCpuBytes{}, nativeAllocationBytes{};
};

struct SurfaceDiagnostics final {
  std::uint64_t twoDLinearFrames{}, twoDLinearTargetBytes{}, twoDLinearTargetAllocations{};
  SurfaceGpuTiming gpuTiming;
  std::uint64_t acquiredFrames = 0;
  std::uint64_t presentedFrames = 0;
  std::uint64_t resizeGenerations = 0;
  std::uint64_t fenceWaits = 0;
  std::int64_t lastPlatformResult = 0;
  ColorSpace negotiatedColorSpace = ColorSpace::Srgb;
  PresentMode negotiatedPresentMode = PresentMode::VSync;
  std::uint64_t surfaceRecoveries = 0;
  std::uint64_t nativeUiDrawCalls = 0;
  std::uint64_t spriteInstancedDrawCalls = 0;
  std::uint64_t spriteGpuInstances = 0;
  std::uint64_t spriteInstanceUploadBytes = 0;
  std::uint64_t nativeUiBufferReallocations = 0;
  std::uint64_t nativeUiTextureUploads = 0;
  std::uint64_t nativeUiRejectedTextures = 0;
  std::uint64_t sceneDrawCalls = 0;
  std::uint64_t sceneShadowPasses = 0;
  std::uint64_t sceneShadowInstances = 0;
  std::uint64_t sceneInstances = 0;
  std::uint64_t sceneTextureUploads = 0;
  std::uint64_t sceneOffscreenDrawCalls = 0;
  std::uint64_t sceneComposites = 0;
  bool softwareRasterizer = false;
  // Submitted frames that require swapchain replacement instead of reporting successful present.
  std::uint64_t recoverablePresentFrames = 0;
  SurfaceDeviceInfo device;
  SceneGeometryUploadCacheDiagnostics sceneGeometryUploadCache;
  SceneGeometryResidencyDiagnostics sceneGeometryResidency;
};

struct UiVertex final {
  float position[2]{};
  float uv[2]{};
  std::uint32_t color{};
};

// Screen-space affine Quad, expanded by vertex ID in the native vertex shader.
// UV is (u0,v0,u1,v1); negative UV spans preserve flips. Packed tint matches UiVertex.
struct SpriteGpuInstance final {
  float origin[2]{};
  float axisX[2]{};
  float axisY[2]{};
  float uv[4]{};
  std::uint32_t color{};
};
static_assert(sizeof(SpriteGpuInstance) == 44);

struct UiDrawCommand final {
  std::int32_t clipX{};
  std::int32_t clipY{};
  std::uint32_t clipWidth{};
  std::uint32_t clipHeight{};
  std::uint64_t textureId{};
  std::uint32_t elementCount{};
  std::uint32_t indexOffset{};
  std::int32_t vertexOffset{};
  // Nonzero selects six generated vertices per instance; index/vertex offsets must be zero.
  std::uint32_t instanceCount{};
  std::uint32_t firstInstance{};
};

struct UiTextureUpload final {
  std::uint64_t textureId{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t rowPitch{};
  std::span<const std::byte> pixels;
  bool nearest{};
  // Immutable generations may reuse a resident ID. Changed pixels require a fresh ID.
  bool immutable{};
};

// Borrowed tightly packed little-endian linear RGBA16F mip chain, largest level first.
// Nonzero IDs identify immutable scene generations and cannot alias RGBA8 scene textures.
struct SceneLinearTextureUpload final {
  std::uint64_t textureId{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t mipLevels{1};
  std::span<const std::byte> pixels;
};

[[nodiscard]] inline std::size_t SceneLinearTextureByteSize(std::uint32_t width,
                                                            std::uint32_t height,
                                                            std::uint32_t levels) noexcept {
  if (!width || !height || width > 256 || height > 256 || (width & (width - 1)) ||
      (height & (height - 1)) || !levels || levels > 9)
    return 0;
  std::size_t size = 0;
  for (std::uint32_t level = 0; level < levels; ++level) {
    size += static_cast<std::size_t>(width) * height * 8;
    if (width == 1 && height == 1 && level + 1 < levels)
      return 0;
    width = std::max(1U, width / 2);
    height = std::max(1U, height / 2);
  }
  return size;
}

[[nodiscard]] inline bool
ValidateSceneLinearTexture(const SceneLinearTextureUpload &upload) noexcept {
  const auto size = SceneLinearTextureByteSize(upload.width, upload.height, upload.mipLevels);
  if (!upload.textureId || upload.textureId >= UINT64_MAX - 1 || !size ||
      size != upload.pixels.size())
    return false;
  for (std::size_t i = 0; i < size; i += 2) {
    const auto half = std::to_integer<unsigned>(upload.pixels[i]) |
                      (std::to_integer<unsigned>(upload.pixels[i + 1]) << 8);
    if ((half & 0x8000U) || (half & 0x7c00U) == 0x7c00U)
      return false; // Nonnegative finite radiance/LUT data; no infinities or NaNs.
  }
  return true;
}

struct SceneEnvironment final {
  std::uint64_t diffuseTextureId{};
  std::uint64_t specularTextureId{};
  std::uint64_t brdfTextureId{};
  float intensity{1};
  float rotationRadians{};            // +Y rotation; latlong U wraps, V clamps.
  std::uint32_t specularMipLevels{1}; // Roughness spans the complete prefiltered chain.
};

struct SceneVertex final {
  float position[3]{};
  float normal[3]{};
  float uv[2]{};
  float tangent[4]{1, 0, 0, 1}; // XYZ direction, W handedness, used by PBR only.
};

// Translation, nonzero axis scale, and a unit quaternion transform one shared mesh. Colors
// multiply base_color. Rotation defaults to identity for existing instance users. An affine
// override supplies exact model/normal transforms for sheared or mirrored hierarchies.
// Empty SceneDrawData::instances selects one identity instance. Spans are borrowed for the call.
struct SceneInstance final {
  float translation[3]{};
  float scale[3]{1, 1, 1};
  float color[4]{1, 1, 1, 1};
  float rotation[4]{0, 0, 0, 1}; // x, y, z, w
  // Optional row-major affine model matrix, overriding translation/scale/rotation. Its last
  // row must be [0, 0, 0, 1]; the linear part must be invertible with finite float normal data.
  std::optional<std::array<float, 16>> model_transform{};
};

// Pure CPU validation shared with native packing. Invalid descriptors do not consume a draw.
[[nodiscard]] NEXORA_PRESENTATION_API bool ValidateSceneInstance(const SceneInstance &) noexcept;

// Shared-upload ranges for one indexed mesh draw. Indices address the whole vertex upload;
// transforms/tints come from the selected instance range. Triangle starts/counts are multiples
// of three. Borrowed for DrawScene; overlapping ranges are supported.
struct SceneMeshBatch final {
  std::uint32_t firstIndex{};
  std::uint32_t indexCount{};
  std::uint32_t firstInstance{};
  std::uint32_t instanceCount{};
  std::uint32_t materialIndex{};
};

// Opaque Lambert/PBR submission material. Slots are borrowed for one DrawScene, not persistent
// Runtime identities. Texture zero selects white; nonzero IDs denote immutable generations.
// PBR maps use sRGB base/emission, +Y normals and linear ORM (R=AO,G=roughness,B=metallic).
// Base/emission factors are linear in PBR. PBR alpha cutout uses the base texture alpha; surviving
// pixels remain opaque.
enum class SceneReflectionRole : std::uint32_t { None, Receiver, ReflectedGeometry };
struct SceneMaterial final {
  std::array<float, 4> baseColor{1, 1, 1, 1};
  std::uint64_t textureId{};
  std::uint64_t normalTextureId{};
  std::uint64_t ormTextureId{};
  std::uint64_t emissionTextureId{};
  float metallic{};
  float roughness{0.5F};
  float occlusion{1};
  float normalScale{1};
  std::array<float, 3> emission{}; // Linear radiance factor, bounded to half-float range.
  float alphaCutoff{};   // Zero keeps opaque behavior; otherwise sample base alpha in main/shadow.
  float windAmplitude{}; // World-space bend, 0..0.5; UV.y is root-to-tip bend weight.
  float transmissionThickness{}; // Thin-leaf back lighting, 0..1; no alpha blending.
  std::array<float, 3> transmissionColor{0.2F, 0.5F, 0.08F};
  bool unlit{};            // PBR emission-only path; retains alpha cutout and common color output.
  bool twoSidedLighting{}; // Lit PBR sheets: orient the shading normal toward the viewer.
  bool castsShadow{true};  // Exclude non-casters from the protecting-frame prepass.
  SceneReflectionRole reflectionRole{}; // Horizontal planar mirror mask; opt-in PBR only.
  float refractionIndex{1};    // [1,2.5]; above 1 enables bounded opaque-HDR background refraction.
  float refractionThickness{}; // [0,1] world units; requires non-casting translucent HDR PBR.
  bool refractionFrontSurfaceOnly{}; // Closed glass: reject rear geometric facets before shading.
  bool dielectricRefraction{}; // Opt-in thin dielectric: Fresnel transmission, no diffuse surface.
  float opacity{1};            // Linear HDR blend coverage; below 1 requires non-casting HDR PBR.
  std::array<float, 3> transparencyTint{1, 1, 1}; // Linear attenuation of the transmitted scene.
  float
      worldTextureScale{}; // Base/normal/ORM triplanar repeats per world unit; zero keeps mesh UVs.
};

[[nodiscard]] inline bool ValidateSceneMaterials(std::span<const SceneMaterial> materials,
                                                 std::span<const SceneMeshBatch> batches) noexcept {
  if (materials.size() > 64)
    return false;
  for (const auto &material : materials) {
    if (material.reflectionRole != SceneReflectionRole::None &&
        material.reflectionRole != SceneReflectionRole::Receiver &&
        material.reflectionRole != SceneReflectionRole::ReflectedGeometry)
      return false;
    if (material.textureId >= UINT64_MAX - 1 || material.baseColor[3] != 1)
      return false;
    if (material.normalTextureId >= UINT64_MAX - 1 || material.ormTextureId >= UINT64_MAX - 1 ||
        material.emissionTextureId >= UINT64_MAX - 1)
      return false;
    for (const auto value : {material.metallic, material.roughness, material.occlusion})
      if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
    if (!std::isfinite(material.refractionIndex) || material.refractionIndex < 1 ||
        material.refractionIndex > 2.5F || !std::isfinite(material.refractionThickness) ||
        material.refractionThickness < 0 || material.refractionThickness > 1)
      return false;
    if (!std::isfinite(material.worldTextureScale) || material.worldTextureScale < 0 ||
        material.worldTextureScale > 16)
      return false;
    if (!std::isfinite(material.normalScale) || material.normalScale < 0 ||
        material.normalScale > 4)
      return false;
    for (const auto value :
         {material.alphaCutoff, material.transmissionThickness, material.opacity})
      if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
    for (const float value : material.transparencyTint)
      if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
    if (!std::isfinite(material.windAmplitude) || material.windAmplitude < 0 ||
        material.windAmplitude > 0.5F)
      return false;
    for (const auto value : material.transmissionColor)
      if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
    for (const auto value : material.emission)
      if (!std::isfinite(value) || value < 0 || value > 65504)
        return false;
    for (const auto value : material.baseColor)
      if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
  }
  for (const auto &batch : batches)
    if (materials.empty() ? batch.materialIndex != 0 : batch.materialIndex >= materials.size())
      return false;
  return true;
}

[[nodiscard]] inline bool ValidateSceneMeshBatches(std::span<const SceneMeshBatch> batches,
                                                   std::size_t indexCount,
                                                   std::size_t instanceCount) noexcept {
  if (batches.size() > 4096)
    return false;
  for (const auto &batch : batches) {
    if (batch.indexCount == 0 || batch.instanceCount == 0 || batch.firstIndex % 3 != 0 ||
        batch.indexCount % 3 != 0 || batch.firstIndex > indexCount ||
        batch.indexCount > indexCount - batch.firstIndex || batch.firstInstance > instanceCount ||
        batch.instanceCount > instanceCount - batch.firstInstance)
      return false;
  }
  return true;
}

// Physical pixel rectangle within the acquired surface. All zero selects the whole surface.
// Nonzero rectangles must fit completely; offscreen scene copies always use the whole surface.
struct SceneViewport final {
  std::uint32_t x{};
  std::uint32_t y{};
  std::uint32_t width{};
  std::uint32_t height{};
};

// Rectangles use physical pixels; touching edges and empty rectangles do not overlap.
[[nodiscard]] constexpr bool SceneViewportsOverlap(SceneViewport a, SceneViewport b) noexcept {
  return a.width && a.height && b.width && b.height &&
         static_cast<std::uint64_t>(a.x) < static_cast<std::uint64_t>(b.x) + b.width &&
         static_cast<std::uint64_t>(b.x) < static_cast<std::uint64_t>(a.x) + a.width &&
         static_cast<std::uint64_t>(a.y) < static_cast<std::uint64_t>(b.y) + b.height &&
         static_cast<std::uint64_t>(b.y) < static_cast<std::uint64_t>(a.y) + a.height;
}

[[nodiscard]] constexpr std::optional<SceneViewport>
ResolveSceneViewport(SceneViewport viewport, std::uint32_t surface_width,
                     std::uint32_t surface_height) noexcept {
  if (surface_width == 0 || surface_height == 0)
    return std::nullopt;
  if (viewport.x == 0 && viewport.y == 0 && viewport.width == 0 && viewport.height == 0)
    return SceneViewport{0, 0, surface_width, surface_height};
  if (viewport.width == 0 || viewport.height == 0 || viewport.x >= surface_width ||
      viewport.y >= surface_height || viewport.width > surface_width - viewport.x ||
      viewport.height > surface_height - viewport.y)
    return std::nullopt;
  return viewport;
}

// An indexed, lit mesh submission. Spans are borrowed for the call. The matrix is row-major,
// matching Nexora::Math::Matrix4's storage, so backends that want row_major in HLSL need no
// transpose; light/base_color give a minimal single-directional-light Lambertian material.
struct SceneDirectionalShadow final {
  std::array<float, 16> lightViewProjection{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  std::uint32_t resolution = 1024; // 256, 512, 1024 or 2048; protecting-frame map.
  float normalBias = 0.0008F;
  float slopeBias = 0.0015F;
};
struct SceneLightingStyle final {
  std::array<float, 3> shadowTint{0.45F, 0.62F, 0.8F};
  std::array<float, 3> lightTint{1.0F, 0.9F, 0.75F};
  float rampOffset = 0;
  float rampScale = 1;
  float rampSoftness = 0.15F;
};
struct SceneColorGrade final {
  float saturation = 1.0F;
  float contrast = 1.0F;
};
struct SceneBloom final {
  float intensity = 0.15F;
  float threshold = 1.0F;
  float radiusPixels = 12.0F;
};
struct SceneReflectionEllipse final {
  float centerX{}, centerZ{}, radiusX{1}, radiusZ{1};
};
struct ScenePlanarReflection final {
  float planeHeight{};      // Caller mirrors reflected geometry around this horizontal world plane.
  float reflectance{0.04F}; // Fresnel F0; reflected surfaces use a dark water substrate.
  std::array<SceneReflectionEllipse, 2> regions{};
  std::uint32_t regionCount{1}; // One or two bounded non-recursive water regions.
  float shorelineVariation{};   // Inward contour variation [0,0.2]; zero preserves ellipses.
};
// Distance haze in linear HDR; sky/unlit emitters retain their authored radiance.
struct SceneAtmosphere final {
  std::array<float, 3> color{0.65F, 0.7F, 0.8F};
  float strength{0.6F};
  float startDistance{12};
  float endDistance{60};
};
struct SceneDepthOfField final {
  float focusDistance = 10.0F; // World units, measured from cameraPosition.
  float strength = 1.0F;
  float radiusPixels = 12.0F;
};
// Perspective-only screen-space contact shading from the HDR geometry-distance channel.
struct SceneScreenSpaceOcclusion final {
  float strength = 0.7F;            // [0,1]; zero bypasses all sampling.
  float radius = 0.5F;              // [0.01,2] world units, bounded to 32 screen pixels.
  float bias = 0.025F;              // World units [0,radius], suppresses planar self-occlusion.
  float verticalFovRadians = 0.85F; // Must match the perspective projection; [0.1,3].
};
// One finite unshadowed point source in linear HDR, copied for this scene submission.
struct ScenePointLight final {
  std::array<float, 3> position{};
  std::array<float, 3> radiance{1, 1, 1};
  float radius{4}; // Smooth finite influence [0.1,64] world units.
};
struct SceneDrawData final {
  std::span<const SceneVertex> vertices;
  std::span<const std::uint16_t> indices;
  std::span<const SceneInstance> instances{};
  std::uint64_t textureId{};
  std::span<const UiTextureUpload> textureUploads{};
  bool offscreen = false;
  SceneViewport viewport{};
  float model_view_projection[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  float light_direction[3]{-0.4F, -1.0F, -0.2F};
  float light_color[3]{1.0F, 0.95F, 0.85F};
  float base_color[4]{1.0F, 1.0F, 1.0F, 1.0F};
  // Empty selects the entire index upload and all instances, preserving existing draws.
  std::span<const SceneMeshBatch> batches{};
  // Empty preserves the original global base_color/textureId; otherwise batches select slots.
  std::span<const SceneMaterial> materials{};
  bool pbr{}; // Explicit shared PBR path; unsupported hosts must report Unsupported.
  std::array<float, 3> cameraPosition{};
  std::span<const SceneLinearTextureUpload> linearTextureUploads{};
  std::optional<SceneEnvironment> environment{};
  std::optional<ScenePointLight> pointLight{}; // HDR PBR only; absent preserves existing lighting.
  // Linear RGBA16F offscreen PBR; CompositeScene applies exposure, ACES and display transfer.
  bool hdr{};
  bool postProcessAntiAliasing = false;
  float exposure = 1.0F;
  std::optional<SceneDirectionalShadow> shadow{};
  std::optional<SceneLightingStyle> lightingStyle{};
  std::optional<SceneBloom> bloom{};
  std::optional<SceneColorGrade> colorGrade{};
  float vegetationTime{}; // Finite bounded seconds [0,3600]; caller controls pause/replay.
  std::optional<SceneDepthOfField> depthOfField{}; // HDR PBR only; absent preserves sharp output.
  std::optional<SceneScreenSpaceOcclusion> screenSpaceOcclusion{}; // Absent preserves existing HDR.
  std::optional<SceneAtmosphere> atmosphere{};             // Finite bounded HDR distance haze.
  std::optional<ScenePlanarReflection> planarReflection{}; // Borrowed geometry, scalar mask copy.
  // Optional exact-content mesh residency. Always provide current geometry; unavailable meshes
  // cannot be recovered by an ID alone. Unsupported adapters report Unsupported when requested.
  bool retainMeshGeometry{};
};

// Call only after material/batch validation. Returned values own their scalar storage.
[[nodiscard]] inline SceneMaterial ResolveSceneMaterial(const SceneDrawData &data,
                                                        std::size_t index) noexcept {
  if (!data.materials.empty())
    return data.materials[index];
  SceneMaterial legacy;
  std::copy(std::begin(data.base_color), std::end(data.base_color), legacy.baseColor.begin());
  legacy.textureId = data.textureId;
  return legacy;
}

struct UiDrawData final {
  std::span<const UiVertex> vertices;
  std::span<const std::byte> indices;
  std::span<const UiDrawCommand> commands;
  std::span<const UiTextureUpload> textureUploads;
  bool indices32Bit = false;
  std::span<const SpriteGpuInstance> spriteInstances{};
};
// Separate 2D-only FP16 path. World is prepared Sprite geometry; overlay is final linear UI.
// All spans borrow the call. All-zero viewport selects the entire image; otherwise x/y/w/h
// must be nonempty and wholly contained. The compositor leaves outside pixels black.
struct TwoDLinearDrawData final {
  UiDrawData world;
  UiDrawData overlay;
  std::array<std::uint32_t, 4> viewport{};
};
// Shared bounded validation before any native upload or command recording.
[[nodiscard]] inline bool ValidateSpriteInstances(const UiDrawData &data) noexcept {
  if (data.spriteInstances.size() > 100000 || data.commands.size() > 100000)
    return false;
  for (const auto &v : data.spriteInstances) {
    for (const auto *pair : {v.origin, v.axisX, v.axisY})
      for (unsigned i = 0; i < 2; ++i)
        if (!std::isfinite(pair[i]) || std::abs(pair[i]) > 1e12F)
          return false;
    for (float f : v.uv)
      if (!std::isfinite(f) || std::abs(f) > 1e12F)
        return false;
  }
  for (const auto &c : data.commands) {
    if (c.instanceCount) {
      if (c.elementCount != 6 || c.indexOffset || c.vertexOffset ||
          c.firstInstance > data.spriteInstances.size() ||
          c.instanceCount > data.spriteInstances.size() - c.firstInstance)
        return false;
    } else if (c.firstInstance)
      return false;
  }
  return true;
}

// Inputs have already passed ValidateTwoDLinearDraws; empty intersections skip the draw.
[[nodiscard]] inline std::array<std::uint32_t, 4>
TwoDLinearClip(const UiDrawCommand &command, const std::array<std::uint32_t, 4> &viewport,
               std::uint32_t width, std::uint32_t height) noexcept {
  const auto v = viewport == std::array<std::uint32_t, 4>{}
                     ? std::array<std::uint32_t, 4>{0, 0, width, height}
                     : viewport;
  const auto x = std::max(std::uint32_t(command.clipX), v[0]);
  const auto y = std::max(std::uint32_t(command.clipY), v[1]);
  const auto right = std::min(std::uint32_t(command.clipX) + command.clipWidth, v[0] + v[2]);
  const auto bottom = std::min(std::uint32_t(command.clipY) + command.clipHeight, v[1] + v[3]);
  return {x, y, right > x ? right - x : 0, bottom > y ? bottom - y : 0};
}

[[nodiscard]] inline bool ValidateTwoDLinearDraws(const TwoDLinearDrawData &data,
                                                  std::uint32_t width, std::uint32_t height) {
  if (!width || !height || width > 16384 || height > 16384)
    return false;
  const auto &v = data.viewport;
  if (v != std::array<std::uint32_t, 4>{} && (!v[2] || !v[3] || v[0] > width || v[1] > height ||
                                              v[2] > width - v[0] || v[3] > height - v[1]))
    return false;
  if (data.world.commands.empty() || data.world.vertices.empty() || data.world.indices.empty())
    return false;
  std::map<std::uint64_t, UiTextureUpload> textures;
  for (const auto *packet : {&data.world, &data.overlay}) {
    if (!ValidateSpriteInstances(*packet) || packet->vertices.size() > 1000000 ||
        packet->textureUploads.size() > 129)
      return false;
    const auto stride = packet->indices32Bit ? 4U : 2U;
    if (packet->indices.size() % stride || packet->indices.size() / stride > 2000000)
      return false;
    for (const auto &vertex : packet->vertices)
      for (const auto f : {vertex.position[0], vertex.position[1], vertex.uv[0], vertex.uv[1]})
        if (!std::isfinite(f) || std::abs(f) > 1e12F)
          return false;
    for (const auto &command : packet->commands) {
      if (!command.textureId || command.clipX < 0 || command.clipY < 0 ||
          std::uint32_t(command.clipX) > width || std::uint32_t(command.clipY) > height ||
          command.clipWidth > width - std::uint32_t(command.clipX) ||
          command.clipHeight > height - std::uint32_t(command.clipY))
        return false;
      if (command.instanceCount)
        continue;
      const auto count = packet->indices.size() / stride;
      if (!command.elementCount || command.elementCount % 3 || command.indexOffset > count ||
          command.elementCount > count - command.indexOffset)
        return false;
      for (std::size_t i = command.indexOffset; i < command.indexOffset + command.elementCount;
           ++i) {
        std::uint32_t index{};
        if (stride == 4)
          std::memcpy(&index, packet->indices.data() + i * stride, 4);
        else {
          std::uint16_t small{};
          std::memcpy(&small, packet->indices.data() + i * stride, 2);
          index = small;
        }
        const auto vertex = std::int64_t(index) + command.vertexOffset;
        if (vertex < 0 || std::uint64_t(vertex) >= packet->vertices.size())
          return false;
      }
    }
    for (const auto &upload : packet->textureUploads) {
      if (!upload.textureId || !upload.width || !upload.height || upload.width > 16384 ||
          upload.height > 16384 || upload.rowPitch != upload.width * 4 ||
          upload.pixels.size() != std::size_t(upload.rowPitch) * upload.height)
        return false;
      if (upload.textureId == UINT64_MAX - 1 &&
          (upload.width != 1 || upload.height != 1 || !upload.nearest || !upload.immutable ||
           !std::all_of(upload.pixels.begin(), upload.pixels.end(),
                        [](std::byte b) { return b == std::byte{255}; })))
        return false;
      auto [it, inserted] = textures.emplace(upload.textureId, upload);
      if (!inserted &&
          (it->second.width != upload.width || it->second.height != upload.height ||
           it->second.nearest != upload.nearest || it->second.immutable != upload.immutable ||
           (it->second.pixels.data() != upload.pixels.data() &&
            !std::equal(it->second.pixels.begin(), it->second.pixels.end(),
                        upload.pixels.begin()))))
        return false;
    }
  }
  return textures.size() <= 128 || (textures.size() == 129 && textures.contains(UINT64_MAX - 1));
}

// Geometry owns its bytes; texture pixel spans still borrow the input owners until submission.
// The reserved white texture UINT64_MAX-1 is used only by the scene background.
struct PreparedSpriteDraws final {
  std::vector<UiVertex> vertices;
  std::vector<std::uint32_t> indices;
  std::vector<UiDrawCommand> commands;
  std::vector<UiTextureUpload> textureUploads;
  std::vector<SpriteGpuInstance> spriteInstances;
  [[nodiscard]] UiDrawData View() const noexcept {
    return {vertices,       std::as_bytes(std::span(indices)), commands, textureUploads, true,
            spriteInstances};
  }
};
[[nodiscard]] NEXORA_PRESENTATION_API std::optional<PreparedSpriteDraws>
PrepareSpriteDraws(const UiDrawData &sprites, const UiDrawData &overlay, std::uint32_t width,
                   std::uint32_t height, bool instanceQuads = false,
                   std::uint32_t background = 0xFF29140A);

enum class SurfaceStatus : std::uint8_t {
  Ready,
  ZeroExtent,
  OutOfDate,
  SurfaceLost,
  DeviceLost,
  Unsupported,
  InvalidDescriptor,
  WrongThread,
  Occluded,
};

// The adapter owns its swapchain/backbuffers; the application owns the adapter and source window.
// The window must outlive this object. Calls run on RenderThread(), except NotifyWindowExtent may
// be called by the window owner and only records the newest extent for the next Acquire/Present.
class NEXORA_PRESENTATION_API ISurface {
public:
  [[nodiscard]] virtual bool SupportsSceneGeometryResidency() const noexcept { return false; }
  // Two disjoint explicit direct LDR/non-shadow viewports after one UI submission. Other scene
  // modes retain one submission per frame. Each view owns distinct fence-protected uploads.
  [[nodiscard]] virtual bool SupportsBoundedSceneViews() const noexcept { return false; }
  virtual ~ISurface() = default;
  [[nodiscard]] virtual std::thread::id RenderThread() const noexcept = 0;
  virtual SurfaceStatus NotifyWindowExtent(std::uint32_t width, std::uint32_t height) noexcept = 0;
  virtual SurfaceStatus Acquire() = 0;
  // Copies a complete tightly-packed RGBA8 frame into the currently acquired backbuffer.
  virtual SurfaceStatus CompositeRgba8(std::span<const std::byte>, std::uint32_t, std::uint32_t) {
    return SurfaceStatus::Unsupported;
  }
  // Records textured indexed UI geometry directly into the acquired presentation image. All spans
  // are borrowed for this call; texture IDs are generation-checked opaque values.
  virtual SurfaceStatus RenderUi(const UiDrawData &) { return SurfaceStatus::Unsupported; }
  [[nodiscard]] virtual bool SupportsTwoDLinear() const noexcept { return false; }
  // Explicit opt-in; separate FP16 world/emission, fixed SDR composition, then untouched UI.
  virtual SurfaceStatus DrawTwoDLinear(const TwoDLinearDrawData &) {
    return SurfaceStatus::Unsupported;
  }
  // Owner thread, between frames; drains submitted work before reclaiming immutable textures.
  virtual SurfaceStatus ReleaseSpriteTextures(std::span<const std::uint64_t>) {
    return SurfaceStatus::Unsupported;
  }
  // Draws one indexed, depth-tested, lit mesh into the acquired image or, when offscreen is
  // requested, a fence-owned private color/depth target requiring CompositeScene. Backends without
  // a real geometry pipeline report Unsupported rather than silently falling back to a 2D
  // composite. A bounded direct scene draw may follow UI to replace only its viewport pixels;
  // offscreen and default full-surface draws must precede UI.
  virtual SurfaceStatus DrawScene(const SceneDrawData &) { return SurfaceStatus::Unsupported; }
  // Composites a completed offscreen scene entirely on the GPU: legacy color copy, or HDR
  // exposure/ACES/display transfer into the acquired image. A pending
  // offscreen draw must be composited once before UI/Present; direct scene callers need no copy.
  virtual SurfaceStatus CompositeScene() { return SurfaceStatus::Unsupported; }
  virtual SurfaceStatus Present() = 0;
  [[nodiscard]] virtual SurfaceDiagnostics Diagnostics() const noexcept = 0;
  // Waits for submitted GPU work and releases all swapchain resources. Idempotent and render-thread
  // only.
  virtual SurfaceStatus DrainAndDestroy() = 0;
};

[[nodiscard]] NEXORA_PRESENTATION_API std::unique_ptr<ISurface>
CreateSurface(const SurfaceDescriptor &descriptor, Window::IWindowSystem &windows);

} // namespace Nexora::Presentation
