#include "Nexora/Window/Window.h"
#include <X11/Xlib.h>
#undef None
#include <cassert>
#include <cstdint>
#include <iostream>
using namespace Nexora::Window;
namespace {
int observedErrors{};
int ObserveError(Display *, XErrorEvent *) {
  ++observedErrors;
  return 0;
}
} // namespace
int main() {
  auto windows = CreateWindowSystem();
  assert(windows);
  auto *other = XOpenDisplay(nullptr);
  if (!other)
    return 77;
  for (int iteration = 0; iteration < 8; ++iteration) {
    const auto created = windows->Create({"Metrics close race", 320, 240});
    assert(created);
    WindowMetrics live{};
    assert(windows->Metrics(created.handle, live) == WindowError::None);
    assert(live.width == 320 && live.height == 240 && live.dpiScale == 1);
    const auto native = reinterpret_cast<uintptr_t>(windows->NativeHandle(created.handle));
    XDestroyWindow(other, static_cast<::Window>(native));
    XSync(other, False); // DestroyNotify remains unpumped on the owner's connection.
    WindowMetrics unchanged{71, 73, 2, true};
    assert(windows->Metrics(created.handle, unchanged) == WindowError::InvalidHandle);
    assert(unchanged.width == 71 && unchanged.height == 73 && unchanged.dpiScale == 2 &&
           unchanged.borderless);
    if (iteration % 2) {
      assert(windows->Destroy(created.handle) == WindowError::None);
    } else {
      bool destroyed{};
      for (const auto &event : windows->PumpEvents())
        destroyed |=
            event.window == created.handle && event.type == WindowEventType::WindowDestroyed;
      assert(destroyed);
      assert(windows->Destroy(created.handle) == WindowError::None);
      assert(windows->Destroy(created.handle) == WindowError::InvalidHandle);
    }
  }
  const auto created = windows->Create({"Handler restoration", 320, 240});
  assert(created);
  const auto native = reinterpret_cast<uintptr_t>(windows->NativeHandle(created.handle));
  auto previous = XSetErrorHandler(ObserveError);
  WindowMetrics live{};
  assert(windows->Metrics(created.handle, live) == WindowError::None);
  XWindowAttributes unused{};
  XGetWindowAttributes(other, static_cast<::Window>(native) + 0x10000000, &unused);
  XSync(other, False);
  assert(observedErrors == 1); // The query guard does not leak into later requests.
  XSetErrorHandler(previous);
  assert(windows->Destroy(created.handle) == WindowError::None);
  XCloseDisplay(other);
  std::cout << "PASS: native X11 live metrics, destroyed-before-pump, output preservation and "
               "handler restoration\n";
}
