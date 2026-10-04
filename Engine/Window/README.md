# Nexora Window contract (WP-M4)

`Nexora::Window` exposes one backend-neutral contract with Win32, X11, and Cocoa implementations.
The application owns `IWindowSystem` and every `WindowHandle`; handles must be destroyed before the
system. `NativeHandle` is an opaque adapter-only identity and no native platform type appears in a
public descriptor or event.

Creation, visibility, client resize, fullscreen transitions, destruction, and event pumping run only
on `OwnerThread()`. Events use steady-clock timestamps and consecutive resize notifications for one
window are coalesced. Zero extent means minimization. A returned event span is borrowed until the next
pump or system destruction. Destroy removes queued events and no callback or deferred work survives it.

Pointer `value0`/`value1` are native client pixels. UI consumers convert with the current
frame DPI; Window and RenderSurface keep native coordinates for other consumers.

Win32 provides per-monitor DPI, keyboard/text/IME, pointer-button and wheel translation, including
native IME candidate positioning. X11 provides close, configure, focus, physical-key, XIM-backed
UTF-8 text, pointer-button and two-axis wheel translation plus EWMH fullscreen; X11 buttons 2 and 3
map to the shared right-button index 1 and middle-button index 2 respectively. Each committed Unicode
scalar is a separate `Text` event, while candidate positioning reports `Unsupported`. An X11 input
context is owned per window and is destroyed before that window or the display connection.
X11 reports modifier flags after the key transition and tracks paired left/right modifier keys so
releasing one preserves the other, and releasing the last clears the family immediately. Focus loss
and window destruction clear that tracking state. The Linux Xvfb native gate injects real X11 key
events for Control, Shift, Alt, and Super to verify press/release ordering and paired-key behavior.
A window destroyed by the server or another client (no `WM_DELETE_WINDOW` is ever sent) is reported as
`WindowDestroyed`, and the system does not destroy it a second time. Ordinary close requests leave
the native window alive for application confirmation. Cocoa intercepts its native close action through
a delegate and reports it once; programmatic destruction bypasses that veto. Cocoa also provides
native window lifetime, physical-key/button/wheel translation, focus, backing-pixel extent and pointer
coordinates, DPI observation, visibility, and Spaces fullscreen. Repeated key-down events are
suppressed so held controls do not retrigger actions; modifier events preserve physical left/right
keys and focus loss clears modifier tracking. Cocoa text/IME composition remains unsupported. Display/DPI
changes are represented by backend-neutral `DisplayChanged` and `DpiChanged` events where a host
reports them.

`NEXORA_ENABLE_WINDOW_PRESENTATION=OFF` removes Window and Presentation, preserving a headless build
without desktop SDK dependencies. Runtime validation for each native implementation belongs to that
target host; compiling or testing portable contracts on another host is not acceptance evidence.
