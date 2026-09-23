# Nexora Window contract (WP-M1)

`Nexora::Window` exposes a backend-neutral contract and a Win32 implementation selected by
`CreateWindowSystem()`. The application owns `IWindowSystem` and each `WindowHandle`; handles must be
destroyed before the system. Native handles and Win32 message structures remain private, with only an
opaque identity exposed to the Presentation adapter.

Creation, show/hide, client-area resize, destruction, and event pumping run exclusively on
`OwnerThread()`. Win32 opts into per-monitor-v2 DPI awareness before creating a window and converts the
requested logical client extent to the correct outer rectangle. Events carry steady-clock monotonic
timestamps. A pump drains the Win32 queue deterministically and coalesces consecutive resize events for
a window to the last extent. A zero width or height represents minimization and is a valid event.
Returned event spans are borrowed until the next pump or system destruction. Destroy removes queued
events for that window, and no callbacks or deferred work survive destruction.

Win32 translates close, resize, DPI, focus, key up/down, UTF-16 text and committed IME text, pointer,
and horizontal/vertical wheel messages. The payload remains backend-neutral; key values are Win32
virtual-key values and text values are UTF-16 code units until the future normalized input milestone.

`NEXORA_ENABLE_WINDOW_PRESENTATION=OFF` removes this module and Presentation from the build graph, so
a headless consumer has no desktop/window SDK dependency. Non-Windows builds retain the contract and
return no native implementation.
