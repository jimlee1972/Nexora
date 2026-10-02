#pragma once

#include "Nexora/Presentation/Api.h"
#include "Nexora/Window/Window.h"

#include <cstddef>
#include <cstdint>
#include <memory>
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

struct SceneVertex final {
  float position[3]{};
  float normal[3]{};
  float uv[2]{};
};

// Translation and nonzero axis scale transform one shared mesh. Colors multiply base_color.
// Empty SceneDrawData::instances selects one identity instance. Spans are borrowed for the call.
struct SceneInstance final {
  float translation[3]{};
  float scale[3]{1, 1, 1};
  float color[4]{1, 1, 1, 1};
};

// A single indexed, lit mesh draw. Spans are borrowed for the call. The matrix is row-major,
// matching Nexora::Math::Matrix4's storage, so backends that want row_major in HLSL need no
// transpose; light/base_color give a minimal single-directional-light Lambertian material.
struct SceneDrawData final {
  std::span<const SceneVertex> vertices;
  std::span<const std::uint16_t> indices;
  std::span<const SceneInstance> instances{};
  std::uint64_t textureId{};
  std::span<const UiTextureUpload> textureUploads{};
  float model_view_projection[16]{1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1};
  float light_direction[3]{-0.4F, -1.0F, -0.2F};
  float light_color[3]{1.0F, 0.95F, 0.85F};
  float base_color[4]{1.0F, 1.0F, 1.0F, 1.0F};
};

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
  // Draws one indexed, depth-tested, lit mesh directly into the acquired presentation image and
  // its depth buffer. Backends without a real geometry pipeline report Unsupported rather than
  // silently falling back to a 2D composite.
  virtual SurfaceStatus DrawScene(const SceneDrawData &) { return SurfaceStatus::Unsupported; }
  virtual SurfaceStatus Present() = 0;
  [[nodiscard]] virtual SurfaceDiagnostics Diagnostics() const noexcept = 0;
  // Waits for submitted GPU work and releases all swapchain resources. Idempotent and render-thread
  // only.
  virtual SurfaceStatus DrainAndDestroy() = 0;
};

[[nodiscard]] NEXORA_PRESENTATION_API std::unique_ptr<ISurface>
CreateSurface(const SurfaceDescriptor &descriptor, Window::IWindowSystem &windows);

} // namespace Nexora::Presentation
