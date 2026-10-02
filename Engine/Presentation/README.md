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
device handle escapes. `DrawScene` similarly borrows one indexed `SceneDrawData` mesh, transform,
light, and base color for the duration of the call and records a depth-tested native scene draw on
the render thread. DX12 and Vulkan own their depth buffers, pipelines, and bounded per-frame upload storage;
backends without a native geometry path return `Unsupported` rather than silently compositing a
fallback. `SurfaceDiagnostics::sceneDrawCalls` counts accepted native scene draws. `CompositeRgba8`
remains a legacy full-frame upload for non-Editor clients; the production Editor does not call it.

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

## Vulkan Showcase scene/UI ownership

Vulkan accepts one indexed scene batch per acquired frame (up to 65,535 vertices and 1,048,576
indices), rejects invalid indices/non-finite input and repeated scene batches, and keeps vertex/index
uploads plus D32 depth/framebuffer resources in the owning fence-protected frame slot. Resize drains
GPU work before destroying the scene/UI resources and releasing their command buffers. Native GLSL
scene/UI sources and embedded SPIR-V are under `shaders/` and `src/*VulkanShaders.h`;
`shaders/GenerateShaders.py --check` verifies deterministic regeneration with glslangValidator.
Neither UI nor scene rendering requires the graphical Editor or a runtime shader compiler.

UI loads a prior scene color target rather than erasing it; a UI-only frame explicitly clears its
background. Atlas uploads must be resubmitted after swapchain recreation. RenderSurface stops before
Acquire when a close request is pumped, leaving no newly acquired frame without presentation during
normal shutdown. Xvfb/lavapipe acceptance covers native scene/UI, interaction and resize; it is not
physical-GPU, Windows or Metal acceptance.

`DrawScene` uses the row-major MVP, Vulkan clip-space Y conversion and padded 112-byte
light/material push constants. The public scene format remains separate from Renderer Slang
shaders. Full-frame composition cannot be combined with a scene or UI submission; the scene
must precede its optional single UI submission. Duplicate submissions return InvalidDescriptor.
Destroying an abandoned acquired frame clears acquisition/validity before releasing resources.
The retained `window_presentation.vulkan_scene` target-host gate reads X11 pixels in the test,
checking near/far depth-order invariance, lighting, matrix translation, two resizes, rejected
inputs and idempotent teardown; it executes separately from the Showcase room/input gates.
