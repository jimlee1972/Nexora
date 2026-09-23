# Nexora Presentation contract (WP-M3)

`RenderSurface` is the application-facing native presentation owner shared by Showcase and future
Editor Scene/Game views. It owns an `IWindowSystem`, one window, and one `ISurface`, pumps and
normalizes window input, forwards resize events, and preserves the required surface-before-window
teardown order. This module depends on Window and RHI; Runtime has no dependency on Presentation or
Editor. Factory failures include a human-readable reason, and callers must report any fallback.

The application owns a presentation adapter and its source window. The Win32 adapter privately owns
its DXGI flip-discard swapchain, D3D12 device/queue, render-target backbuffers, per-frame command
allocators, and fence timeline; it does not expose DXGI or D3D12 types. The window must outlive the
surface. Before destroying a surface or its window, the application calls `DrainAndDestroy()` on the
render thread; it waits for submitted GPU work and is idempotent.

`Acquire`, `Present`, and teardown are serialized on `RenderThread()`. `Acquire` waits only when the
selected frame is still in flight and records a deterministic clear as the validation render pass.
The window owner may publish resize notifications from another thread; the adapter atomically consumes
only the newest extent before render submission. Zero extent suspends acquire/submission until restored.
Resize first drains submitted work, releases backbuffers, and commits the new generation only after
`ResizeBuffers` and all replacement render targets succeed.

The swapchain uses `R8G8B8A8_UNORM`, two or three frames in flight, FIFO presentation for `VSync`, and
immediate presentation with `DXGI_PRESENT_ALLOW_TEARING` only when supported. `Occluded`, `OutOfDate`,
`SurfaceLost`, and `DeviceLost` distinguish recovery scopes. `SurfaceDiagnostics` supplies acquire,
present, resize-generation, fence-wait, and last-HRESULT evidence; screenshots are supplementary and
never replace these counters and correctness assertions. Non-Windows builds expose no native surface.

`BeginFrame()` pumps window events and acquires the swapchain image; `EndFrame()` submits the clear
and presents it. `SurfaceInputSnapshot` is retained by the render-surface owner and remains valid
until the next event pump. Both calls, creation, and teardown run on the owning application thread.
