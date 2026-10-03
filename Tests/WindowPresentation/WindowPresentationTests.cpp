#include "Nexora/Presentation/RenderSurface.h"

#include <array>
#include <cassert>
#include <deque>
#include <ranges>
#include <thread>
#include <vector>

#if defined(_WIN32)
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <imm.h>
#include <windows.h>
#endif

namespace {
using namespace Nexora;

class FakeWindowSystem final : public Window::IWindowSystem {
public:
  std::thread::id OwnerThread() const noexcept override { return owner_; }
  Window::WindowResult Create(const Window::WindowDescriptor &descriptor) override {
    if (std::this_thread::get_id() != owner_)
      return {{}, Window::WindowError::WrongThread};
    if (descriptor.width == 0 || descriptor.height == 0)
      return {{}, Window::WindowError::InvalidDescriptor};
    alive_ = true;
    return {{1}, Window::WindowError::None};
  }
  Window::WindowError Destroy(Window::WindowHandle window) override {
    if (std::this_thread::get_id() != owner_)
      return Window::WindowError::WrongThread;
    if (!alive_ || window.value != 1)
      return Window::WindowError::InvalidHandle;
    alive_ = false;
    pending_.clear();
    pumped_.clear();
    return Window::WindowError::None;
  }
  Window::WindowError Show(Window::WindowHandle window, bool) override {
    return alive_ && window.value == 1 ? Window::WindowError::None
                                       : Window::WindowError::InvalidHandle;
  }
  Window::WindowError Resize(Window::WindowHandle window, std::uint32_t width,
                             std::uint32_t height) override {
    if (!alive_ || window.value != 1)
      return Window::WindowError::InvalidHandle;
    ResizeEvent(width, height, 0);
    return Window::WindowError::None;
  }
  Window::WindowError SetFullscreen(Window::WindowHandle window, bool fullscreen) override {
    if (!alive_ || window.value != 1)
      return Window::WindowError::InvalidHandle;
    fullscreen_ = fullscreen;
    return Window::WindowError::None;
  }
  void *NativeHandle(Window::WindowHandle) const noexcept override { return nullptr; }
  Window::WindowError SetImeCandidatePosition(Window::WindowHandle, std::int32_t,
                                              std::int32_t) override {
    return Window::WindowError::Unsupported;
  }
  std::span<const Window::WindowEvent> PumpEvents() override {
    pumped_.clear();
    for (const auto &event : pending_) {
      if (event.type == Window::WindowEventType::Resized && !pumped_.empty() &&
          pumped_.back().type == Window::WindowEventType::Resized &&
          pumped_.back().window == event.window) {
        pumped_.back() = event;
      } else {
        pumped_.push_back(event);
      }
    }
    pending_.clear();
    return pumped_;
  }
  void ResizeEvent(std::uint32_t width, std::uint32_t height, std::uint64_t timestamp) {
    pending_.push_back({{1}, Window::WindowEventType::Resized, timestamp, width, height});
  }
  [[nodiscard]] bool Alive() const noexcept { return alive_; }

private:
  std::thread::id owner_ = std::this_thread::get_id();
  bool alive_ = false;
  bool fullscreen_ = false;
  std::deque<Window::WindowEvent> pending_;
  std::vector<Window::WindowEvent> pumped_;
};

class FakeSurface final : public Presentation::ISurface {
public:
  explicit FakeSurface(const Presentation::SurfaceDescriptor &descriptor)
      : renderThread_(std::this_thread::get_id()), width_(descriptor.width),
        height_(descriptor.height) {}
  std::thread::id RenderThread() const noexcept override { return renderThread_; }
  Presentation::SurfaceStatus NotifyWindowExtent(std::uint32_t width,
                                                 std::uint32_t height) noexcept override {
    width_ = width;
    height_ = height;
    return Status();
  }
  Presentation::SurfaceStatus Acquire() override {
    if (destroyed_)
      return Presentation::SurfaceStatus::SurfaceLost;
    if (nextStatus_ != Presentation::SurfaceStatus::Ready) {
      const auto result = nextStatus_;
      nextStatus_ = Presentation::SurfaceStatus::Ready;
      return result;
    }
    return Status();
  }
  Presentation::SurfaceStatus Present() override {
    return destroyed_ ? Presentation::SurfaceStatus::SurfaceLost : Status();
  }
  Presentation::SurfaceStatus RenderUi(const Presentation::UiDrawData &drawData) override {
    if (destroyed_ || drawData.vertices.empty() || drawData.indices.empty() ||
        drawData.commands.empty())
      return Presentation::SurfaceStatus::InvalidDescriptor;
    const auto indexSize = drawData.indices32Bit ? 4U : 2U;
    for (const auto &command : drawData.commands)
      if (command.elementCount == 0 ||
          (static_cast<std::size_t>(command.indexOffset) + command.elementCount) * indexSize >
              drawData.indices.size() ||
          command.clipWidth == 0 || command.clipHeight == 0)
        return Presentation::SurfaceStatus::InvalidDescriptor;
    uiDrawCalls_ += drawData.commands.size();
    return Presentation::SurfaceStatus::Ready;
  }
  Presentation::SurfaceDiagnostics Diagnostics() const noexcept override { return {}; }
  Presentation::SurfaceStatus DrainAndDestroy() override {
    if (!destroyed_)
      ++drainCount_;
    destroyed_ = true;
    return Presentation::SurfaceStatus::Ready;
  }
  [[nodiscard]] std::uint32_t DrainCount() const noexcept { return drainCount_; }
  [[nodiscard]] std::uint64_t UiDrawCalls() const noexcept { return uiDrawCalls_; }
  void Inject(Presentation::SurfaceStatus status) noexcept { nextStatus_ = status; }

private:
  Presentation::SurfaceStatus Status() const noexcept {
    return width_ == 0 || height_ == 0 ? Presentation::SurfaceStatus::ZeroExtent
                                       : Presentation::SurfaceStatus::Ready;
  }
  std::thread::id renderThread_;
  std::uint32_t width_;
  std::uint32_t height_;
  std::uint32_t drainCount_ = 0;
  std::uint64_t uiDrawCalls_ = 0;
  bool destroyed_ = false;
  Presentation::SurfaceStatus nextStatus_ = Presentation::SurfaceStatus::Ready;
};
} // namespace

int main() {
  const auto full_viewport = Presentation::ResolveSceneViewport({}, 640, 480);
  const auto inset_viewport = Presentation::ResolveSceneViewport({160, 120, 320, 240}, 640, 480);
  assert(full_viewport && full_viewport->width == 640 && full_viewport->height == 480 &&
         inset_viewport && inset_viewport->x == 160 && inset_viewport->height == 240 &&
         !Presentation::ResolveSceneViewport({630, 0, 20, 480}, 640, 480) &&
         !Presentation::ResolveSceneViewport({0, 0, 0, 240}, 640, 480));
  assert(Presentation::ToString(Presentation::SurfaceBackend::Dx12) == "dx12");
  assert(Presentation::ToString(Presentation::SurfaceBackend::Vulkan) == "vulkan");
  assert(Presentation::ToString(Presentation::SurfaceBackend::Metal) == "metal");
  assert(Presentation::ToString(Presentation::SurfaceStatus::DeviceLost) == "device_lost");
  assert(Presentation::RecoveryAction(Presentation::SurfaceStatus::Ready) ==
         Presentation::SurfaceAction::Render);
  assert(Presentation::RecoveryAction(Presentation::SurfaceStatus::ZeroExtent) ==
         Presentation::SurfaceAction::Suspend);
  assert(Presentation::RecoveryAction(Presentation::SurfaceStatus::OutOfDate) ==
         Presentation::SurfaceAction::RecreateSurface);
  assert(Presentation::RecoveryAction(Presentation::SurfaceStatus::DeviceLost) ==
         Presentation::SurfaceAction::RecreateDevice);
  assert(Presentation::ToString(Presentation::SurfaceAction::RecreateDevice) == "recreate_device");
#if !defined(_WIN32)
  const auto unsupported = Presentation::CreateRenderSurface(
      {"unsupported", 640, 480, true, Presentation::SurfaceBackend::Dx12});
  assert(!unsupported);
  assert(unsupported.status == Presentation::SurfaceStatus::Unsupported);
  assert(!unsupported.reason.empty());
#endif

  FakeWindowSystem windows;
  const auto created = windows.Create({"Fake", 640, 480, true, false});
  assert(created);
  assert(windows.SetFullscreen(created.handle, true) == Window::WindowError::None);
  assert(windows.SetFullscreen(created.handle, false) == Window::WindowError::None);
  Window::WindowResult crossThreadCreate;
  std::thread wrongThread([&] { crossThreadCreate = windows.Create({}); });
  wrongThread.join();
  assert(crossThreadCreate.error == Window::WindowError::WrongThread);

  FakeSurface surface({created.handle, 640, 480, 2});
  assert(surface.Acquire() == Presentation::SurfaceStatus::Ready);
  const std::array<Presentation::UiVertex, 3> uiVertices{};
  const std::array<std::uint16_t, 3> uiIndices{};
  const std::array uiCommands{Presentation::UiDrawCommand{0, 0, 64, 64, 1, 3, 0, 0}};
  assert(
      surface.RenderUi({uiVertices, std::as_bytes(std::span{uiIndices}), uiCommands, {}, false}) ==
      Presentation::SurfaceStatus::Ready);
  assert(surface.UiDrawCalls() == 1);
  // FakeSurface does not override DrawScene, so it must fall back to ISurface's base default --
  // Unsupported, never a silent no-op "success" -- exactly like every backend without a real
  // geometry pipeline (e.g. Vulkan today) reports it.
  const std::array<Presentation::SceneVertex, 3> sceneVertices{};
  const std::array<std::uint16_t, 3> sceneIndices{};
  assert(surface.DrawScene({sceneVertices, sceneIndices}) ==
         Presentation::SurfaceStatus::Unsupported);
  windows.ResizeEvent(800, 600, 1);
  windows.ResizeEvent(0, 0, 2);
  const auto events = windows.PumpEvents();
  assert(events.size() == 1 && events[0].width == 0 && events[0].height == 0);
  assert(surface.NotifyWindowExtent(events[0].width, events[0].height) ==
         Presentation::SurfaceStatus::ZeroExtent);
  assert(surface.Acquire() == Presentation::SurfaceStatus::ZeroExtent);
  assert(surface.NotifyWindowExtent(1920, 1080) == Presentation::SurfaceStatus::Ready);
  assert(surface.Present() == Presentation::SurfaceStatus::Ready);
  surface.Inject(Presentation::SurfaceStatus::OutOfDate);
  assert(surface.Acquire() == Presentation::SurfaceStatus::OutOfDate);
  surface.Inject(Presentation::SurfaceStatus::SurfaceLost);
  assert(surface.Acquire() == Presentation::SurfaceStatus::SurfaceLost);
  surface.Inject(Presentation::SurfaceStatus::DeviceLost);
  assert(surface.Acquire() == Presentation::SurfaceStatus::DeviceLost);

  // A bounded resize/failure stress pass gates recovery without requiring a display server.
  for (std::uint32_t iteration = 1; iteration <= 2048; ++iteration) {
    assert(surface.NotifyWindowExtent(320 + iteration % 17, 180 + iteration % 11) ==
           Presentation::SurfaceStatus::Ready);
    assert(surface.Acquire() == Presentation::SurfaceStatus::Ready);
    assert(surface.Present() == Presentation::SurfaceStatus::Ready);
  }

  FakeWindowSystem secondWindows;
  const auto secondCreated = secondWindows.Create({"Second", 320, 180, true, false});
  assert(secondCreated);
  FakeSurface secondSurface({secondCreated.handle, 320, 180, 3});
  assert(secondSurface.Acquire() == Presentation::SurfaceStatus::Ready);
  assert(secondSurface.Present() == Presentation::SurfaceStatus::Ready);
  assert(secondSurface.DrainAndDestroy() == Presentation::SurfaceStatus::Ready);
  assert(secondWindows.Destroy(secondCreated.handle) == Window::WindowError::None);

  assert(surface.DrainAndDestroy() == Presentation::SurfaceStatus::Ready);
  assert(surface.DrainAndDestroy() == Presentation::SurfaceStatus::Ready);
  assert(surface.DrainCount() == 1);
  assert(windows.Destroy(created.handle) == Window::WindowError::None);
  assert(!windows.Alive());

#if defined(_WIN32)
  auto nativeWindows = Window::CreateWindowSystem();
  assert(nativeWindows);
  const auto native = nativeWindows->Create({"Nexora DX12 validation", 320, 240, true, true});
  assert(native);
  auto nativeSurface =
      Presentation::CreateSurface({native.handle, 320, 240, 2, Presentation::PresentMode::Immediate,
                                   Presentation::ColorSpace::Srgb},
                                  *nativeWindows);
  assert(nativeSurface);
  for (int frame = 0; frame != 3; ++frame) {
    static_cast<void>(nativeWindows->PumpEvents());
    assert(nativeSurface->Acquire() == Presentation::SurfaceStatus::Ready);
    assert(nativeSurface->Present() == Presentation::SurfaceStatus::Ready);
  }
  assert(nativeWindows->Resize(native.handle, 400, 300) == Window::WindowError::None);
  static_cast<void>(nativeWindows->PumpEvents());
  assert(nativeSurface->NotifyWindowExtent(400, 300) == Presentation::SurfaceStatus::Ready);
  assert(nativeSurface->Acquire() == Presentation::SurfaceStatus::Ready);
  assert(nativeSurface->Present() == Presentation::SurfaceStatus::Ready);
  const auto diagnostics = nativeSurface->Diagnostics();
  assert(diagnostics.acquiredFrames == 4);
  assert(diagnostics.presentedFrames == 4);
  assert(diagnostics.resizeGenerations == 1);
  assert(nativeSurface->DrainAndDestroy() == Presentation::SurfaceStatus::Ready);
  assert(nativeWindows->Destroy(native.handle) == Window::WindowError::None);

  for (int iteration = 0; iteration != 8; ++iteration) {
    const auto repeated = nativeWindows->Create({"lifecycle", 64, 64, true, false});
    assert(repeated);
    assert(nativeWindows->Destroy(repeated.handle) == Window::WindowError::None);
  }

  const auto eventWindow = nativeWindows->Create({"events", 160, 90, true, false});
  assert(eventWindow);
  auto hwnd = static_cast<HWND>(nativeWindows->NativeHandle(eventWindow.handle));
  assert(hwnd);

  SendMessageW(hwnd, WM_CHAR, 0xD83D, 0);
  assert(nativeWindows->PumpEvents().empty());
  SendMessageW(hwnd, WM_CHAR, 0xDE80, 0);
  const auto supplementaryText = nativeWindows->PumpEvents();
  assert(supplementaryText.size() == 1);
  assert(supplementaryText[0].type == Window::WindowEventType::Text);
  assert(supplementaryText[0].value0 == 0x1F680);

  SendMessageW(hwnd, WM_CHAR, 0xDE80, 0);
  assert(nativeWindows->PumpEvents().empty());
  SendMessageW(hwnd, WM_CHAR, 0xD83D, 0);
  SendMessageW(hwnd, WM_CHAR, L'A', 0);
  const auto recoveredText = nativeWindows->PumpEvents();
  assert(recoveredText.size() == 1);
  assert(recoveredText[0].type == Window::WindowEventType::Text);
  assert(recoveredText[0].value0 == L'A');
  assert(SendMessageW(hwnd, WM_UNICHAR, UNICODE_NOCHAR, 0) == TRUE);
  SendMessageW(hwnd, WM_CHAR, 0xD83D, 0);
  SendMessageW(hwnd, WM_UNICHAR, 0x1F642, 0);
  const auto unicodeText = nativeWindows->PumpEvents();
  assert(unicodeText.size() == 1);
  assert(unicodeText[0].type == Window::WindowEventType::Text);
  assert(unicodeText[0].value0 == 0x1F642);
  SendMessageW(hwnd, WM_CHAR, 0xDE80, 0);
  assert(nativeWindows->PumpEvents().empty());

  RECT dpiRectangle{20, 30, 500, 300};
  SendMessageW(hwnd, WM_DPICHANGED, MAKELONG(144, 144), reinterpret_cast<LPARAM>(&dpiRectangle));
  const auto dpiEvents = nativeWindows->PumpEvents();
  const auto dpiEvent =
      std::ranges::find(dpiEvents, Window::WindowEventType::DpiChanged, &Window::WindowEvent::type);
  assert(dpiEvent != dpiEvents.end());
  assert(dpiEvent->scale == 1.5F);

  auto inputContext = ImmGetContext(hwnd);
  HIMC ownedInputContext = nullptr;
  HIMC previousInputContext = nullptr;
  if (!inputContext) {
    ownedInputContext = ImmCreateContext();
    assert(ownedInputContext);
    previousInputContext = ImmAssociateContext(hwnd, ownedInputContext);
    inputContext = ImmGetContext(hwnd);
  }
  assert(inputContext);
  assert(nativeWindows->SetImeCandidatePosition(eventWindow.handle, 42, 84) ==
         Window::WindowError::None);
  CANDIDATEFORM candidate{};
  assert(ImmGetCandidateWindow(inputContext, 0, &candidate));
  assert(candidate.dwStyle == CFS_CANDIDATEPOS);
  assert(candidate.ptCurrentPos.x == 42 && candidate.ptCurrentPos.y == 84);
  ImmReleaseContext(hwnd, inputContext);
  if (ownedInputContext) {
    ImmAssociateContext(hwnd, previousInputContext);
    assert(ImmDestroyContext(ownedInputContext));
  }

  PostMessageW(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(200, 100));
  PostMessageW(hwnd, WM_SIZE, SIZE_MINIMIZED, MAKELPARAM(0, 0));
  PostMessageW(hwnd, WM_SIZE, SIZE_RESTORED, MAKELPARAM(320, 180));
  auto resizeEvents = nativeWindows->PumpEvents();
  assert(resizeEvents.size() == 1);
  assert(resizeEvents[0].type == Window::WindowEventType::Resized);
  assert(resizeEvents[0].width == 320 && resizeEvents[0].height == 180);

  PostMessageW(hwnd, WM_CLOSE, 0, 0);
  assert(nativeWindows->Destroy(eventWindow.handle) == Window::WindowError::None);
  assert(nativeWindows->PumpEvents().empty());
#endif
  return 0;
}
