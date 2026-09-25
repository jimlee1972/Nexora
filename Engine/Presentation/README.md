# Nexora Presentation contract (WP-M4)

`RenderSurface` is the application-facing owner shared by Showcase and Editor Scene/Game views. It
owns one window system, window, and `ISurface`, forwards normalized events and resize state, and always
destroys the GPU surface before its window. Runtime remains independent of Presentation and Editor.
The borrowed `Events()` span and `FrameInfo()` snapshot remain valid until the next `BeginFrame()`;
`FrameInfo()` tracks the latest client extent and DPI scale so UI hosts do not duplicate window state.
After a successful `BeginFrame`, `RenderUi` borrows backend-neutral textured/indexed geometry,
scissors, offsets, and generation-checked texture uploads and records native GPU draws directly into
the acquired image. Vulkan, DX12, and Metal keep their pipeline, sampler, texture descriptors, and
bounded per-frame upload buffers below this boundary; resources replaced by a later atlas generation
are released only after the protecting frame fence/command buffer completes. No native image or
device handle escapes. `CompositeRgba8` remains a legacy full-frame upload for non-Editor clients;
the production Editor does not call it.

DX12 uses a DXGI flip-discard swapchain, Vulkan uses the host WSI swapchain (Xlib on Linux and Win32 on
Windows), and Metal uses `CAMetalLayer`. Their native devices, queues, images, synchronization objects,
and handles stay private. `Automatic` selects the host backend; explicit unsupported selections fail
with a visible reason. HDR10 and immediate presentation are negotiated rather than assumed, with the
actual color space and present mode recorded in `SurfaceDiagnostics`.

`Acquire`, `Present`, fullscreen transitions, and teardown are serialized on their owner/render thread.
Resize publication may come from the window owner and atomically replaces older pending extents. Zero
extent suspends work. Swapchain replacement drains work before releasing images, and a generation is
committed only after replacement resources succeed. `OutOfDate`, `SurfaceLost`, `DeviceLost`,
`Occluded`, and `Unsupported` distinguish recovery scopes; recovery and resize generations are
observable diagnostics. `RecoveryAction()` is the application policy boundary: zero extent/occlusion suspend, out-of-date or surface loss recreate the surface, and device loss requires device recreation rather than an unsafe surface-only retry.

`DrainAndDestroy()` is idempotent, waits for submitted GPU work, and releases all backend objects before
the source window. Portable tests cover multi-surface lifetime, 2,048 resize cycles, zero extent,
failure injection, ordering, and idempotent teardown. Real acquire/render/present acceptance remains
separate target-host evidence for Windows/DX12, Linux and Windows/Vulkan, and macOS/Metal.

Application owners may request a client resize through `RenderSurface::Resize`; the call follows the
window owner-thread rule and the resulting event publishes the new extent on a later `BeginFrame`.
This keeps resize requests above the native window abstraction while swapchain recreation remains
private to Presentation.
