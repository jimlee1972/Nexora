#pragma once

#include "Nexora/Presentation/Surface.h"

#include <memory>
#include <span>
#include <string>
#include <string_view>

namespace Nexora::Presentation {

enum class SurfaceAction : std::uint8_t { Render, Suspend, RecreateSurface, RecreateDevice, Abort };

struct RenderSurfaceDescriptor final {
  std::string_view title = "Nexora";
  std::uint32_t width = 1280;
  std::uint32_t height = 720;
  bool resizable = true;
  SurfaceBackend backend = SurfaceBackend::Automatic;
  PresentMode presentMode = PresentMode::VSync;
  ColorSpace colorSpace = ColorSpace::Srgb;
};

struct SurfaceInputSnapshot final {
  std::uint64_t sequence = 0;
  std::int32_t pointerX = 0;
  std::int32_t pointerY = 0;
  std::int32_t lastKey = 0;
  bool lastKeyDown = false;
  bool focused = false;
};

// Application-facing owner for a native window and presentation surface. Showcase and editor views
// can share this boundary without placing either application policy in Runtime.
class NEXORA_PRESENTATION_API RenderSurface final {
public:
  struct State;

  ~RenderSurface();
  RenderSurface(RenderSurface &&) noexcept;
  RenderSurface &operator=(RenderSurface &&) noexcept;
  RenderSurface(const RenderSurface &) = delete;
  RenderSurface &operator=(const RenderSurface &) = delete;

  [[nodiscard]] SurfaceStatus BeginFrame();
  [[nodiscard]] SurfaceStatus EndFrame();
  [[nodiscard]] bool CloseRequested() const noexcept;
  [[nodiscard]] const SurfaceInputSnapshot &Input() const noexcept;
  [[nodiscard]] std::span<const Window::WindowEvent> Events() const noexcept;
  [[nodiscard]] Window::WindowError SetImeCandidatePosition(std::int32_t x, std::int32_t y);
  [[nodiscard]] SurfaceDiagnostics Diagnostics() const noexcept;
  [[nodiscard]] SurfaceStatus DrainAndDestroy();

  // Internal construction hook used by CreateRenderSurface; consumers should use the factory.
  explicit RenderSurface(std::unique_ptr<State> state) noexcept;

private:
  std::unique_ptr<State> state_;
};

struct RenderSurfaceResult final {
  std::unique_ptr<RenderSurface> surface;
  SurfaceStatus status = SurfaceStatus::Unsupported;
  // Always populated on failure so backend selection/fallback is never silent.
  std::string reason;

  [[nodiscard]] explicit operator bool() const noexcept { return surface != nullptr; }
};

[[nodiscard]] NEXORA_PRESENTATION_API RenderSurfaceResult
CreateRenderSurface(const RenderSurfaceDescriptor &descriptor);
[[nodiscard]] NEXORA_PRESENTATION_API std::string_view ToString(SurfaceBackend backend) noexcept;
[[nodiscard]] NEXORA_PRESENTATION_API std::string_view ToString(SurfaceStatus status) noexcept;
// Maps backend status to application recovery scope. Device loss is deliberately distinct from
// surface recreation so applications never retry a dead device as if it were a resize.
[[nodiscard]] NEXORA_PRESENTATION_API SurfaceAction RecoveryAction(SurfaceStatus status) noexcept;
[[nodiscard]] NEXORA_PRESENTATION_API std::string_view ToString(SurfaceAction action) noexcept;

} // namespace Nexora::Presentation
