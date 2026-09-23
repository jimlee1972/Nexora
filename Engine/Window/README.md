# Nexora Window contract (WP-M4)

`Nexora::Window` exposes one backend-neutral contract with Win32, X11, and Cocoa implementations.
The application owns `IWindowSystem` and every `WindowHandle`; handles must be destroyed before the
system. `NativeHandle` is an opaque adapter-only identity and no native platform type appears in a
public descriptor or event.

Creation, visibility, client resize, fullscreen transitions, destruction, and event pumping run only
on `OwnerThread()`. Events use steady-clock timestamps and consecutive resize notifications for one
window are coalesced. Zero extent means minimization. A returned event span is borrowed until the next
pump or system destruction. Destroy removes queued events and no callback or deferred work survives it.

Win32 provides per-monitor DPI, keyboard/text/IME, pointer and wheel translation. X11 provides close,
configure, focus, keyboard, pointer and wheel translation plus EWMH fullscreen. Cocoa provides native
window lifetime, resize observation, visibility, and Spaces fullscreen. Display/DPI changes are
represented by backend-neutral `DisplayChanged` and `DpiChanged` events where a host reports them.

`NEXORA_ENABLE_WINDOW_PRESENTATION=OFF` removes Window and Presentation, preserving a headless build
without desktop SDK dependencies. Runtime validation for each native implementation belongs to that
target host; compiling or testing portable contracts on another host is not acceptance evidence.
