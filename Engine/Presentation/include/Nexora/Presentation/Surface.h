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
  virtual SurfaceStatus Present() = 0;
  [[nodiscard]] virtual SurfaceDiagnostics Diagnostics() const noexcept = 0;
  // Waits for submitted GPU work and releases all swapchain resources. Idempotent and render-thread
  // only.
  virtual SurfaceStatus DrainAndDestroy() = 0;
};

[[nodiscard]] NEXORA_PRESENTATION_API std::unique_ptr<ISurface>
CreateSurface(const SurfaceDescriptor &descriptor, Window::IWindowSystem &windows);

} // namespace Nexora::Presentation
