#pragma once

#include "Nexora/Presentation/Api.h"
#include "Nexora/Window/Window.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <thread>

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
};

struct SurfaceDiagnostics final {
  std::uint64_t acquiredFrames = 0;
  std::uint64_t presentedFrames = 0;
  std::uint64_t resizeGenerations = 0;
  std::uint64_t fenceWaits = 0;
  std::int64_t lastPlatformResult = 0;
  ColorSpace negotiatedColorSpace = ColorSpace::Srgb;
  PresentMode negotiatedPresentMode = PresentMode::VSync;
  std::uint64_t surfaceRecoveries = 0;
  std::uint64_t nativeUiDrawCalls = 0;
  std::uint64_t nativeUiBufferReallocations = 0;
  std::uint64_t nativeUiTextureUploads = 0;
  std::uint64_t nativeUiRejectedTextures = 0;
  std::uint64_t sceneDrawCalls = 0;
  std::uint64_t sceneInstances = 0;
  std::uint64_t sceneTextureUploads = 0;
  std::uint64_t sceneOffscreenDrawCalls = 0;
  std::uint64_t sceneComposites = 0;
  bool softwareRasterizer = false;
};

struct UiVertex final {
  float position[2]{};
  float uv[2]{};
  std::uint32_t color{};
};

struct UiDrawCommand final {
  std::int32_t clipX{};
  std::int32_t clipY{};
  std::uint32_t clipWidth{};
  std::uint32_t clipHeight{};
  std::uint64_t textureId{};
  std::uint32_t elementCount{};
  std::uint32_t indexOffset{};
  std::int32_t vertexOffset{};
};

struct UiTextureUpload final {
  std::uint64_t textureId{};
  std::uint32_t width{};
  std::uint32_t height{};
  std::uint32_t rowPitch{};
  std::span<const std::byte> pixels;
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
// Base/emission factors are linear in PBR. Alpha-cutout is a separate forthcoming contract.
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
};

[[nodiscard]] inline bool ValidateSceneMaterials(std::span<const SceneMaterial> materials,
                                                 std::span<const SceneMeshBatch> batches) noexcept {
  if (materials.size() > 64)
    return false;
  for (const auto &material : materials) {
    if (material.textureId >= UINT64_MAX - 1 || material.baseColor[3] != 1)
      return false;
    if (material.normalTextureId >= UINT64_MAX - 1 || material.ormTextureId >= UINT64_MAX - 1 ||
        material.emissionTextureId >= UINT64_MAX - 1)
      return false;
    for (const auto value : {material.metallic, material.roughness, material.occlusion})
      if (!std::isfinite(value) || value < 0 || value > 1)
        return false;
    if (!std::isfinite(material.normalScale) || material.normalScale < 0 ||
        material.normalScale > 4)
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
};

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
  // Draws one indexed, depth-tested, lit mesh into the acquired image or, when offscreen is
  // requested, a fence-owned private color/depth target requiring CompositeScene. Backends without
  // a real geometry pipeline report Unsupported rather than silently falling back to a 2D
  // composite. A bounded direct scene draw may follow UI to replace only its viewport pixels;
  // offscreen and default full-surface draws must precede UI.
  virtual SurfaceStatus DrawScene(const SceneDrawData &) { return SurfaceStatus::Unsupported; }
  // Copies a completed offscreen scene into the acquired image entirely on the GPU. A pending
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
