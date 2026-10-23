#pragma once

#include <X11/Xlib.h>
#undef None

namespace Nexora::Window::Detail {
// Legacy server autorepeat is an adjacent genuine release/press pair for one native key and
// server timestamp. Explicit SendEvent input and every identity mismatch remain independent.
[[nodiscard]] inline bool IsLegacyX11KeyRepeatPair(const XEvent &release,
                                                   const XEvent &press) noexcept {
  return release.type == KeyRelease && press.type == KeyPress && !release.xkey.send_event &&
         !press.xkey.send_event && release.xkey.window == press.xkey.window &&
         release.xkey.keycode == press.xkey.keycode && release.xkey.time == press.xkey.time;
}
} // namespace Nexora::Window::Detail
