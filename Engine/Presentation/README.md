# Nexora Presentation contract (WP-M0)

The application owns a presentation adapter and its source window. The adapter owns its swapchain,
backbuffers, and synchronization state, but not the RHI device/queue or window. The RHI device and
window must outlive the surface. Before destroying a surface or its window, the application calls
`DrainAndDestroy()` on the render thread; it waits for submitted GPU work and is idempotent.

`Acquire`, `Present`, and teardown are serialized on `RenderThread()`. The window owner may publish
resize notifications; the adapter atomically/coalescingly consumes only the newest extent before
render submission. Zero extent suspends acquire/submission and returns `ZeroExtent` until restored.
Fullscreen and swapchain recreation are render-thread operations. Display/DPI changes arrive as
window events and become an explicit resize/reconfiguration request rather than hidden GPU work.

`OutOfDate` requests recoverable recreation while retaining the old generation until GPU work is
drained. `SurfaceLost` requires rebuilding the native surface, `DeviceLost` requires device-level
recovery, and `Unsupported` rejects an unavailable backend. None of these results implicitly destroys
the application-owned window. Native swapchain and platform types remain private to future adapters.
