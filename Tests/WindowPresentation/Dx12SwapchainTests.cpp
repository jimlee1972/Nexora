// Exercise the actual private DX12 adapter with native wait/handle tracing.
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <iostream>
#include <stdexcept>
#include <unordered_set>
#include <utility>
#include <windows.h>

namespace Dx12Trace {
std::unordered_set<HANDLE> events, pacing;
std::size_t pacingWaits{}, pacingCloses{}, violations{};
bool failWait{};
HANDLE MakeEvent(LPSECURITY_ATTRIBUTES attributes, BOOL manual, BOOL initial, LPCWSTR name) {
  const auto handle = CreateEventW(attributes, manual, initial, name);
  if (handle)
    events.insert(handle);
  return handle;
}
DWORD Wait(HANDLE handle, DWORD milliseconds) {
  if (!events.contains(handle)) {
    pacing.insert(handle);
    ++pacingWaits;
    if (std::exchange(failWait, false))
      return WAIT_FAILED;
  }
  return WaitForSingleObject(handle, milliseconds);
}
BOOL Close(HANDLE handle) {
  if (!events.erase(handle)) {
    violations += pacing.erase(handle) != 1;
    ++pacingCloses;
  }
  return CloseHandle(handle);
}
} // namespace Dx12Trace

#define CreateEventW Dx12Trace::MakeEvent
#define WaitForSingleObject Dx12Trace::Wait
#define CloseHandle Dx12Trace::Close
#define NEXORA_DX12_UI_TESTING 1
#include "../../Engine/Presentation/src/Dx12Surface.cpp"
#undef CreateEventW
#undef WaitForSingleObject
#undef CloseHandle

namespace {
using namespace Nexora;
using namespace Nexora::Presentation;
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run(PresentMode mode, std::uint8_t slots) {
  auto windows = Window::CreateWindowSystem();
  const auto window = windows->Create({"Nexora DX12 swapchain contracts", 320, 240, true, true});
  Require(bool(window), "DX12 native fixture window unavailable");
  const bool vsync = mode == PresentMode::VSync;
  const auto closes = Dx12Trace::pacingCloses;
  Dx12Surface surface(
      {window.handle, 320, 240, slots, mode, ColorSpace::Srgb, SurfaceBackend::Dx12, true},
      *windows);
  const auto acquired = [&] {
    static_cast<void>(windows->PumpEvents());
    const auto waited = Dx12Trace::pacingWaits;
    Require(surface.Acquire() == SurfaceStatus::Ready, "DX12 acquire failed");
    Require(Dx12Trace::pacingWaits == waited + (vsync ? 1 : 0),
            "VSync CPU work lacks frame admission or Immediate gained a pacing wait");
    Require(Dx12Trace::pacing.size() == (vsync ? 1U : 0U),
            "DX12 pacing allocates a handle stream instead of one swapchain handle");
  };
  const auto presented = [&] {
    Require(surface.Present() == SurfaceStatus::Ready, "DX12 presentation failed");
  };
  for (unsigned i = 0; i < 48; ++i) {
    acquired();
    Require(surface.Acquire() == SurfaceStatus::InvalidDescriptor,
            "duplicate acquire consumed presentation readiness");
    presented();
  }
  Require(surface.Diagnostics().negotiatedPresentMode == mode &&
              surface.Diagnostics().gpuTiming.completedSubmission > 0,
          "DX12 pacing changed mode or stopped completed-slot GPU timing");
  Require(surface.ReleaseSpriteTextures({}) == SurfaceStatus::Ready, "DX12 resource drain failed");
  for (unsigned i = 0; i < 12; ++i) {
    acquired();
    presented();
  }
  SurfaceStatus wrongThread{};
  std::thread other([&] { wrongThread = surface.Acquire(); });
  other.join();
  Require(wrongThread == SurfaceStatus::WrongThread, "DX12 admitted a foreign render thread");
  if (vsync) {
    Dx12Trace::failWait = true;
    Require(surface.Acquire() == SurfaceStatus::SurfaceLost,
            "DX12 wait failure admitted an acquired command list");
    acquired(); // Failed wait must not consume the real signal or reset the upload slot.
    presented();
  }
  const auto waited = Dx12Trace::pacingWaits;
  Require(surface.NotifyWindowExtent(0, 0) == SurfaceStatus::ZeroExtent &&
              surface.Acquire() == SurfaceStatus::ZeroExtent && Dx12Trace::pacingWaits == waited,
          "DX12 zero extent waited on presentation instead of suspending");
  Require(surface.NotifyWindowExtent(400, 300) == SurfaceStatus::Ready,
          "DX12 resume notification failed");
  acquired();
  presented();
  Require(surface.Diagnostics().resizeGenerations > 0, "DX12 resize flags rejected recovery");
  acquired(); // Teardown also owns an acquired, unsubmitted command list.
  Require(surface.DrainAndDestroy() == SurfaceStatus::Ready &&
              surface.DrainAndDestroy() == SurfaceStatus::Ready,
          "DX12 drain is not idempotent");
  Require(Dx12Trace::pacing.empty() && Dx12Trace::events.empty() && Dx12Trace::violations == 0 &&
              Dx12Trace::pacingCloses == closes + (vsync ? 1 : 0),
          "DX12 swapchain pacing handle leaked or closed more than once");
  Require(windows->Destroy(window.handle) == Window::WindowError::None,
          "DX12 native fixture teardown failed");
}
} // namespace

int main() {
  try {
    Run(PresentMode::VSync, 2);
    Run(PresentMode::VSync, 3);
    Run(PresentMode::Immediate, 2);
    auto windows = Window::CreateWindowSystem();
    const auto window = windows->Create({"SP-04 DX12 sRGB UI", 320, 240, true, true});
    Require(bool(window), "sRGB window");
    SurfaceDescriptor descriptor{
        window.handle, 320, 240, 2, PresentMode::Immediate, ColorSpace::Srgb, SurfaceBackend::Dx12};
    descriptor.srgbUiOutput = true;
    Dx12Surface colors(descriptor, *windows);
    Require(!colors.SupportsSceneGeometryResidency() && !colors.SupportsBoundedSceneViews(),
            "UI-only capabilities");
    const std::array<UiVertex, 4> vertices{{{{0, 0}, {0, 0}, 0xff373737},
                                            {{320, 0}, {0, 0}, 0xff373737},
                                            {{320, 240}, {0, 0}, 0xff373737},
                                            {{0, 240}, {0, 0}, 0xff373737}}};
    const std::array<std::uint16_t, 6> indices{0, 1, 2, 0, 2, 3};
    const UiDrawCommand command{0, 0, 320, 240, 1, 6, 0, 0};
    const std::array<std::byte, 4> white{std::byte{255}, std::byte{255}, std::byte{255},
                                         std::byte{255}};
    const UiTextureUpload whiteUpload{1, 1, 1, 4, white, true, true};
    Require(colors.Acquire() == SurfaceStatus::Ready &&
                colors.DrawScene({}) == SurfaceStatus::Unsupported &&
                colors.RenderUi({vertices, std::as_bytes(std::span(indices)),
                                 std::span(&command, 1), std::span(&whiteUpload, 1), false}) ==
                    SurfaceStatus::Ready,
            "sRGB UI render");
    const auto pixels = colors.PresentUiPixelsForTesting();
    Require(pixels.size() == 320 * 240 * 4, "sRGB native readback");
    for (unsigned channel = 0; channel < 3; ++channel)
      Require(std::abs(int(std::to_integer<unsigned>(pixels[(120 * 320 + 160) * 4 + channel])) -
                       128) <= 2,
              "DX12 must encode linear55 to sRGB128");
    Require(colors.DrainAndDestroy() == SurfaceStatus::Ready &&
                windows->Destroy(window.handle) == Window::WindowError::None,
            "sRGB cleanup");
    std::cout << "SP-04 DX12 native linear55 -> sRGB128 pixels passed\n";

    std::cout << "DX12 VSync admission, frame-slot, resize, wait failure and cleanup passed\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
