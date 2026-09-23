#pragma once

#include "Nexora/Presentation/Api.h"
#include "Nexora/Window/Window.h"

#include <cstdint>
#include <thread>

namespace Nexora::Presentation {

enum class PresentMode : std::uint8_t { VSync, Immediate };
enum class ColorSpace : std::uint8_t { Srgb, Hdr10 };

struct SurfaceDescriptor final {
  Window::WindowHandle window;
  std::uint32_t width = 0;
  std::uint32_t height = 0;
  std::uint8_t framesInFlight = 2;
  PresentMode presentMode = PresentMode::VSync;
  ColorSpace colorSpace = ColorSpace::Srgb;
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
  virtual SurfaceStatus Present() = 0;
  // Waits for submitted GPU work and releases all swapchain resources. Idempotent and render-thread
  // only.
  virtual SurfaceStatus DrainAndDestroy() = 0;
};

} // namespace Nexora::Presentation
