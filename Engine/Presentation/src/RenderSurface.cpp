#include "Nexora/Presentation/RenderSurface.h"

#include <utility>

namespace Nexora::Presentation {

struct RenderSurface::State final {
  std::unique_ptr<Window::IWindowSystem> windows;
  Window::WindowHandle window;
  std::unique_ptr<ISurface> surface;
  SurfaceInputSnapshot input;
  bool closeRequested = false;
  bool destroyed = false;
};

RenderSurface::RenderSurface(std::unique_ptr<State> state) noexcept : state_(std::move(state)) {}
RenderSurface::~RenderSurface() { static_cast<void>(DrainAndDestroy()); }
RenderSurface::RenderSurface(RenderSurface &&) noexcept = default;
RenderSurface &RenderSurface::operator=(RenderSurface &&) noexcept = default;

SurfaceStatus RenderSurface::BeginFrame() {
  if (!state_ || state_->destroyed)
    return SurfaceStatus::SurfaceLost;
  for (const auto &event : state_->windows->PumpEvents()) {
    ++state_->input.sequence;
    switch (event.type) {
    case Window::WindowEventType::CloseRequested:
      state_->closeRequested = true;
      break;
    case Window::WindowEventType::Resized:
      state_->surface->NotifyWindowExtent(event.width, event.height);
      break;
    case Window::WindowEventType::FocusChanged:
      state_->input.focused = event.value0 != 0;
      break;
    case Window::WindowEventType::Key:
      state_->input.lastKey = event.value0;
      state_->input.lastKeyDown = event.value1 != 0;
      break;
    case Window::WindowEventType::Pointer:
      state_->input.pointerX = event.value0;
      state_->input.pointerY = event.value1;
      break;
    default:
      break;
    }
  }
  return state_->surface->Acquire();
}

SurfaceStatus RenderSurface::EndFrame() {
  return !state_ || state_->destroyed ? SurfaceStatus::SurfaceLost : state_->surface->Present();
}

bool RenderSurface::CloseRequested() const noexcept { return !state_ || state_->closeRequested; }

const SurfaceInputSnapshot &RenderSurface::Input() const noexcept {
  static const SurfaceInputSnapshot empty;
  return state_ ? state_->input : empty;
}

SurfaceDiagnostics RenderSurface::Diagnostics() const noexcept {
  return state_ && state_->surface ? state_->surface->Diagnostics() : SurfaceDiagnostics{};
}

SurfaceStatus RenderSurface::DrainAndDestroy() {
  if (!state_ || state_->destroyed)
    return SurfaceStatus::Ready;
  const auto status = state_->surface->DrainAndDestroy();
  if (status != SurfaceStatus::Ready)
    return status;
  const auto windowStatus = state_->windows->Destroy(state_->window);
  if (windowStatus != Window::WindowError::None)
    return windowStatus == Window::WindowError::WrongThread ? SurfaceStatus::WrongThread
                                                            : SurfaceStatus::SurfaceLost;
  state_->destroyed = true;
  return SurfaceStatus::Ready;
}

RenderSurfaceResult CreateRenderSurface(const RenderSurfaceDescriptor &descriptor) {
  if (descriptor.width == 0 || descriptor.height == 0)
    return {{}, SurfaceStatus::InvalidDescriptor, "render surface extent must be non-zero"};
#if !defined(_WIN32)
  (void)descriptor;
  return {{},
          SurfaceStatus::Unsupported,
          "native presentation is unavailable on this host; DX12 requires Windows"};
#else
  if (descriptor.backend != SurfaceBackend::Automatic && descriptor.backend != SurfaceBackend::Dx12)
    return {{}, SurfaceStatus::Unsupported, "requested presentation backend is unsupported"};
  auto state = std::make_unique<RenderSurface::State>();
  state->windows = Window::CreateWindowSystem();
  if (!state->windows)
    return {{}, SurfaceStatus::Unsupported, "Win32 window system is unavailable"};
  const auto created = state->windows->Create(
      {descriptor.title, descriptor.width, descriptor.height, descriptor.resizable, true});
  if (!created)
    return {{}, SurfaceStatus::SurfaceLost, "Win32 window creation failed"};
  state->window = created.handle;
  state->surface = CreateSurface({created.handle, descriptor.width, descriptor.height, 2,
                                  descriptor.presentMode, ColorSpace::Srgb},
                                 *state->windows);
  if (!state->surface) {
    state->windows->Destroy(created.handle);
    return {{}, SurfaceStatus::Unsupported, "DX12 swapchain creation is unavailable"};
  }
  return {std::unique_ptr<RenderSurface>(new RenderSurface(std::move(state))),
          SurfaceStatus::Ready,
          {}};
#endif
}

std::string_view ToString(SurfaceBackend backend) noexcept {
  switch (backend) {
  case SurfaceBackend::Automatic:
    return "auto";
  case SurfaceBackend::Dx12:
    return "dx12";
  }
  return "unknown";
}

std::string_view ToString(SurfaceStatus status) noexcept {
  switch (status) {
  case SurfaceStatus::Ready:
    return "ready";
  case SurfaceStatus::ZeroExtent:
    return "zero_extent";
  case SurfaceStatus::OutOfDate:
    return "out_of_date";
  case SurfaceStatus::SurfaceLost:
    return "surface_lost";
  case SurfaceStatus::DeviceLost:
    return "device_lost";
  case SurfaceStatus::Unsupported:
    return "unsupported";
  case SurfaceStatus::InvalidDescriptor:
    return "invalid_descriptor";
  case SurfaceStatus::WrongThread:
    return "wrong_thread";
  case SurfaceStatus::Occluded:
    return "occluded";
  }
  return "unknown";
}

} // namespace Nexora::Presentation
