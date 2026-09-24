#include "Nexora/Presentation/RenderSurface.h"

#include <utility>
#include <vector>

namespace Nexora::Presentation {

struct RenderSurface::State final {
  std::unique_ptr<Window::IWindowSystem> windows;
  Window::WindowHandle window;
  std::unique_ptr<ISurface> surface;
  SurfaceInputSnapshot input;
  std::vector<Window::WindowEvent> events;
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
  const auto pumped = state_->windows->PumpEvents();
  state_->events.assign(pumped.begin(), pumped.end());
  for (const auto &event : state_->events) {
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

std::span<const Window::WindowEvent> RenderSurface::Events() const noexcept {
  return state_ ? std::span<const Window::WindowEvent>(state_->events)
                : std::span<const Window::WindowEvent>{};
}

Window::WindowError RenderSurface::SetImeCandidatePosition(std::int32_t x, std::int32_t y) {
  return state_ && !state_->destroyed
             ? state_->windows->SetImeCandidatePosition(state_->window, x, y)
             : Window::WindowError::InvalidHandle;
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
#if defined(_WIN32)
  if (descriptor.backend == SurfaceBackend::Metal)
#elif defined(__linux__)
  if (descriptor.backend == SurfaceBackend::Dx12 || descriptor.backend == SurfaceBackend::Metal)
#elif defined(__APPLE__)
  if (descriptor.backend == SurfaceBackend::Dx12 || descriptor.backend == SurfaceBackend::Vulkan)
#else
  if (true)
#endif
    return {{}, SurfaceStatus::Unsupported, "requested presentation backend is unsupported"};
  auto state = std::make_unique<RenderSurface::State>();
  state->windows = Window::CreateWindowSystem();
  if (!state->windows)
    return {{}, SurfaceStatus::Unsupported, "native window system is unavailable"};
  const auto created = state->windows->Create(
      {descriptor.title, descriptor.width, descriptor.height, descriptor.resizable, true});
  if (!created)
    return {{},
            created.error == Window::WindowError::Unsupported ? SurfaceStatus::Unsupported
                                                              : SurfaceStatus::SurfaceLost,
            "native window creation failed"};
  state->window = created.handle;
  state->surface =
      CreateSurface({created.handle, descriptor.width, descriptor.height, 2, descriptor.presentMode,
                     descriptor.colorSpace, descriptor.backend},
                    *state->windows);
  if (!state->surface) {
    state->windows->Destroy(created.handle);
    return {{}, SurfaceStatus::Unsupported, "requested swapchain creation is unavailable"};
  }
  return {std::unique_ptr<RenderSurface>(new RenderSurface(std::move(state))),
          SurfaceStatus::Ready,
          {}};
}

std::string_view ToString(SurfaceBackend backend) noexcept {
  switch (backend) {
  case SurfaceBackend::Automatic:
    return "auto";
  case SurfaceBackend::Dx12:
    return "dx12";
  case SurfaceBackend::Vulkan:
    return "vulkan";
  case SurfaceBackend::Metal:
    return "metal";
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

SurfaceAction RecoveryAction(SurfaceStatus status) noexcept {
  switch (status) {
  case SurfaceStatus::Ready:
    return SurfaceAction::Render;
  case SurfaceStatus::ZeroExtent:
  case SurfaceStatus::Occluded:
    return SurfaceAction::Suspend;
  case SurfaceStatus::OutOfDate:
  case SurfaceStatus::SurfaceLost:
    return SurfaceAction::RecreateSurface;
  case SurfaceStatus::DeviceLost:
    return SurfaceAction::RecreateDevice;
  case SurfaceStatus::Unsupported:
  case SurfaceStatus::InvalidDescriptor:
  case SurfaceStatus::WrongThread:
    return SurfaceAction::Abort;
  }
  return SurfaceAction::Abort;
}

std::string_view ToString(SurfaceAction action) noexcept {
  switch (action) {
  case SurfaceAction::Render:
    return "render";
  case SurfaceAction::Suspend:
    return "suspend";
  case SurfaceAction::RecreateSurface:
    return "recreate_surface";
  case SurfaceAction::RecreateDevice:
    return "recreate_device";
  case SurfaceAction::Abort:
    return "abort";
  }
  return "abort";
}

} // namespace Nexora::Presentation
