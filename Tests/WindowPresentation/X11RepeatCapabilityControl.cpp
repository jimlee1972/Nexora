// Test-only interposer. It never changes the server-wide repeat mode or repeat rate.
#include <X11/XKBlib.h>
#include <X11/Xlib.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dlfcn.h>

extern "C" __attribute__((visibility("default"))) Bool XkbSetDetectableAutoRepeat(Display *display,
                                                                                  Bool requested,
                                                                                  Bool *supported) {
  using Function = Bool (*)(Display *, Bool, Bool *);
  const auto symbol = dlsym(RTLD_NEXT, "XkbSetDetectableAutoRepeat");
  Function next{};
  static_assert(sizeof(next) == sizeof(symbol));
  std::memcpy(&next, &symbol, sizeof(next));
  if (!next) {
    std::fprintf(stderr, "{\"kind\":\"repeat_capability\",\"lookup_error\":1}\n");
    if (supported)
      *supported = False;
    return False;
  }
  XKeyboardState before{}, after{};
  XGetKeyboardControl(display, &before);
  const auto *mode = std::getenv("NEXORA_X11_REPEAT_TEST_MODE");
  const bool deny = mode && std::strcmp(mode, "legacy") == 0 && requested;
  Bool forwarded_support = False;
  const Bool result = next(display, deny ? False : requested, &forwarded_support);
  Bool query_supported = False;
  const Bool active = XkbGetDetectableAutoRepeat(display, &query_supported);
  XGetKeyboardControl(display, &after);
  if (supported)
    *supported = deny ? False : forwarded_support;
  std::fprintf(stderr,
               "{\"kind\":\"repeat_capability\",\"requested\":%d,\"injected_denial\":%u,"
               "\"active\":%d,\"query_supported\":%d,\"forwarded_result\":%d,"
               "\"global_before\":%d,\"global_after\":%d}\n",
               requested, unsigned(deny), active, query_supported, result,
               before.global_auto_repeat, after.global_auto_repeat);
  std::fflush(stderr);
  return deny ? False : result;
}

// Observe the exact native stream consumed by the backend without changing or consuming
// any extra event. In particular, legacy repeat release/press pairs remain intact here.
extern "C" __attribute__((visibility("default"))) int XNextEvent(Display *display, XEvent *event) {
  using Function = int (*)(Display *, XEvent *);
  const auto symbol = dlsym(RTLD_NEXT, "XNextEvent");
  Function next{};
  static_assert(sizeof(next) == sizeof(symbol));
  std::memcpy(&next, &symbol, sizeof(next));
  if (!next) {
    std::fprintf(stderr, "{\"kind\":\"native_key_event\",\"lookup_error\":1}\n");
    std::abort();
  }
  const auto result = next(display, event);
  if (event && (event->type == KeyPress || event->type == KeyRelease)) {
    std::fprintf(stderr,
                 "{\"kind\":\"native_key_event\",\"type\":%d,\"window\":%lu,"
                 "\"keycode\":%u,\"time\":%lu,\"send_event\":%d}\n",
                 event->type, event->xkey.window, event->xkey.keycode, event->xkey.time,
                 event->xkey.send_event);
    std::fflush(stderr);
  }
  return result;
}
