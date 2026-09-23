# Nexora Window contract (WP-M0)

`Nexora::Window` is a backend-neutral contract module. It never includes or links a desktop/window
SDK. The application owns `IWindowSystem` and each `WindowHandle`; handles must be destroyed before
the system. Native handles and message structures belong only in future private adapter translation
units.

Creation, destruction, and event pumping run exclusively on `OwnerThread()`. Events carry monotonic
timestamps, and a pump coalesces consecutive pending resize events for a window to the last extent.
A zero width or height represents minimization and is a valid event, not a window failure. Returned
event spans are borrowed until the next pump or system destruction. No callbacks or deferred event
work may survive destruction.

`NEXORA_ENABLE_WINDOW_PRESENTATION=OFF` removes this module and Presentation from the build graph,
so a headless consumer has no desktop/window SDK dependency.
