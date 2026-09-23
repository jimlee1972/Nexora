# Nexora Presentation contract (WP-M2)

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
