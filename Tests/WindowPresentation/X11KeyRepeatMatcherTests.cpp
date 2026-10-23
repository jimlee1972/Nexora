#include "X11KeyRepeat.h"
#include <cstdio>

int main() {
  using Nexora::Window::Detail::IsLegacyX11KeyRepeatPair;
  XEvent released{}, pressed{};
  released.type = KeyRelease;
  released.xkey.window = 10;
  released.xkey.keycode = 20;
  released.xkey.time = 30;
  pressed.type = KeyPress;
  pressed.xkey.window = 10;
  pressed.xkey.keycode = 20;
  pressed.xkey.time = 30;
  unsigned cases = 0;
  const auto require = [&](bool condition) {
    ++cases;
    if (!condition)
      std::fprintf(stderr, "X11 repeat matcher case %u failed\n", cases);
    return condition;
  };
  if (!require(IsLegacyX11KeyRepeatPair(released, pressed)))
    return 1;
  const auto negative = [&](XEvent left, XEvent right) {
    return require(!IsLegacyX11KeyRepeatPair(left, right));
  };
  auto changed = released;
  changed.xkey.send_event = True;
  if (!negative(changed, pressed))
    return 2;
  changed = pressed;
  changed.xkey.send_event = True;
  if (!negative(released, changed))
    return 3;
  changed = released;
  changed.type = KeyPress;
  if (!negative(changed, pressed))
    return 4;
  changed = pressed;
  changed.type = KeyRelease;
  if (!negative(released, changed))
    return 5;
  changed = pressed;
  ++changed.xkey.window;
  if (!negative(released, changed))
    return 6;
  changed = pressed;
  ++changed.xkey.keycode;
  if (!negative(released, changed))
    return 7;
  changed = pressed;
  ++changed.xkey.time;
  if (!negative(released, changed))
    return 8;
  std::printf("X11 repeat pair matcher: %u positive/negative cases passed\n", cases);
  return 0;
}
