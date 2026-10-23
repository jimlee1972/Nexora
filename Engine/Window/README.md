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
For `PointerButton`, `value0` identifies left/right/middle/back/forward as 0/1/2/3/4 and
`value1` is 1 for press, 0 for release. Win32 XBUTTON1/2 and X11 buttons8/9 map to back/forward;
Cocoa uses the matching native button numbers. Wheel events remain separate from held buttons.
Wheel `value0` is horizontal and `value1` vertical, in signed 120-unit steps (or finer deltas).
Positive horizontal deltas scroll right; positive vertical deltas scroll up. Cocoa negates its
native horizontal delta to match Win32 and X11. Consumers with a left-positive horizontal
convention, including Dear ImGui, convert that axis at their input boundary.

Win32 provides per-monitor DPI, keyboard/text/IME, pointer-button and wheel translation, including
native IME candidate positioning. Win32 forwards system-key press/release events and then preserves
default system-chord handling.
Alt+F4 therefore produces `CloseRequested` through `WM_CLOSE`, leaving the native window alive
for application confirmation; ordinary F4 remains a key event and does not request close.
X11 provides close, configure, focus, physical-key, XIM-backed
UTF-8 text, pointer-button and two-axis wheel translation plus EWMH fullscreen; X11 buttons 2 and 3
map to the shared middle-button index 2 and right-button index 1 respectively. Each committed Unicode
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

On X11 clients using detectable repeat, physical `Key` events represent transitions: a held key
emits one press and one release, while repeated native presses continue through XIM/UTF-8 text
translation. Detectable repeat is requested per client, and its request result and support flag
select the path. Only clients without active detectable repeat normalize strictly adjacent genuine
release/press pairs already buffered in Xlib, matching window, keycode and server timestamp.
Clients with active detectable repeat preserve genuine physical taps even when adjacent events
share the same server millisecond.
No release waits for a future event; split arrivals are forwarded normally. Equal native
timestamps are the legacy repeat heuristic, not a universal physical-device classifier. Explicit
SendEvent input and mismatched pairs retain their release. Focus loss and destruction clear
per-window held state. This never disables global repeat or changes the user's repeat rate.

`window.x11_held_key_native` uses its own Xvfb server and a real xdotool/XTEST held `n` gesture.
It checks one press/release pair, repeated text, focus/close lifetime, and unchanged enabled global
repeat. A separate held M is released in a second window after focus loss; refocusing the first
window and pressing M again must yield two M presses and only its final M release there, proving
that focus loss cleared held-key deduplication state. A test-only preload interposer observes the
same client Display's detectable-repeat state;
its second run injects capability-request rejection to exercise the legacy path. This is injected
unavailability, not evidence from a physically unsupported X server. The test interposer and its
mode variable do not form part of the production Window API. Pure matcher regressions reject
synthetic or mismatched release/press pairs. Raw native events, input commands, capability observations,
and summary facts are retained under the build artifacts directory for successful and failed attempts.

The test-only native observer also counts the exact KeyPress/KeyRelease stream consumed by the
backend. Every genuine N press must yield one committed N text event, while Window emits only
one physical press/release. Normal mode must receive one genuine native release; injected legacy
mode must actually receive repeated releases. These checks distinguish physical normalization
from lost text or an unexercised fallback. The probe/shim are identical for old and repaired
libraries, and `/proc` library-path/hash diagnostics supplement the functional input oracles.

Normal mode also exercises 20 genuine XTEST B tap pairs after confirming the consumer is stopped;
XSync processes the complete burst before resuming it in a finally block. The test requires all
20 physical press/release pairs and text commits, zero synthetic events, and at least one adjacent
release/press pair sharing a server timestamp. The legacy mode retains its held-key/focus/lifecycle
checks; the ambiguous rapid-tap gesture is not a legacy acceptance claim or a CTest skip.

The independent held-key repair/control evidence is recorded in
[the scoped Linux report](../../Tools/Build/evidence/EditorEDM0-X11HeldKey-Linux-2026-10-22.md).
Its focused Xvfb results do not replace physical-display acceptance or establish the cause of
all earlier Editor workflow stalls. The report separates the historical bc4 integrated pass from
the latest detectable-repeat boundary repair and its own complete-suite gate.

## Display metrics and fitted Windows windows

`Metrics` returns current client pixels and DPI on supported hosts; `SetResizable` is a serialized optional native operation. Windows queries initial DPI without waiting for WM_DPICHANGED and restores saved style/rectangle after borderless desktop mode. `WindowDescriptor::fitToWorkArea` opts into fitting/centering decorated client windows on creation/resize; its default false preserves existing Editor and other host sizing. The standalone 2D Player opts in through RenderSurface. X11 supplies actual client pixels/scale 1 and EWMH mode requests. Unsupported implementations return Unsupported. Native physical acceptance is documented separately from portable geometry tests.
