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
device handle escapes. `DrawScene` similarly borrows indexed `SceneDrawData` geometry, transform,
light, and base color for the duration of the call and records a depth-tested native scene draw on
the render thread. DX12, Vulkan and Metal own their depth buffers, pipelines, and bounded per-frame upload storage;
`SceneDrawData::viewport` optionally bounds that draw to a physical-pixel rectangle of the acquired
surface (all zero means full surface). Invalid or out-of-bounds rectangles are rejected. Offscreen
scene copies require the full surface. A direct scene draw may precede one UI submission, or an
explicit bounded direct draw may follow UI to replace only the selected viewport pixels. Vulkan
loads the UI color target for that later draw; DX12 preserves the existing render target.
Backends without a native geometry path return `Unsupported` rather than silently compositing a
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

`Present` may report `OutOfDate` (including Vulkan `SUBOPTIMAL`) for a frame that was already submitted; the surface schedules swapchain replacement for the next `Acquire`, and callers drop that frame rather than treating it as fatal.

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

An application with unsaved work may call `CancelCloseRequest()` after `BeginFrame()` reports a user
close and before the next frame, then render its confirmation dialog. The call fails when the native
window was destroyed externally, or the surface has been drained. Existing consumers that do not
cancel keep the stop-before-Acquire close behavior.

UI loads a prior scene color target rather than erasing it; a UI-only frame explicitly clears its
background. Atlas uploads must be resubmitted after swapchain recreation. RenderSurface stops before
Acquire when a close request is pumped, leaving no newly acquired frame without presentation during
normal shutdown. Xvfb/lavapipe acceptance covers native scene/UI, interaction and resize; it is not
physical-GPU, Windows or Metal acceptance.

`DrawScene` uses the row-major MVP, Vulkan clip-space Y conversion and padded 112-byte
light/material push constants. The public scene format remains separate from Renderer Slang
shaders. Full-frame composition cannot be combined with a scene or UI submission. A full-surface
or offscreen scene must precede its optional single UI submission; a bounded direct scene may
follow UI. Duplicate submissions return InvalidDescriptor.
Destroying an abandoned acquired frame clears acquisition/validity before releasing resources.
The retained `window_presentation.vulkan_scene` target-host gate reads X11 pixels in the test,
checking near/far depth-order invariance, lighting, matrix translation, two resizes, rejected
inputs, clipped viewport pixels, and idempotent teardown; it executes separately from the Showcase
room/input gates.

## Native scene instances

`SceneDrawData::instances` borrows up to 4,096 translation/axis-scale/color/unit-quaternion
records; an empty span selects one identity instance for existing callers. Vulkan and DX12 copy
records into the acquired frame's fence-protected upload and submit one indexed hardware instance
draw. Scale components must
be finite and have magnitude at least 0.00001; translation/color must also be finite, and the
rotation quaternion must have finite components and squared length within 0.01 of one. The shader
rotates scaled positions and inverse-scaled normals before normalization. Instance bytes begin at
a four-byte aligned offset after indices, including odd triangle counts. No instance span survives
the call. `sceneInstances` counts
accepted instances cumulatively, while `sceneDrawCalls` counts submissions. The retained Vulkan pixel
gate verifies two independent instance positions/tints, a rotated instance's changed lighting,
rejected malformed inputs and identity
compatibility with depth, resize and lighting. DX12 target-host execution is a separate acceptance gate.

## Native sampled scene material

`SceneVertex::uv` and `SceneDrawData::textureId/textureUploads` provide one RGBA8 sampled material
per indexed batch, multiplied by directional lighting, base color and instance tint. Texture ID zero
uses a private white fallback. Nonzero IDs belong to a scene-only table, separate from UI IDs;
UINT64_MAX is reserved for the fallback. Each ID denotes immutable content: use a new generation ID
for changed pixels. Resubmitting an existing ID does not upload it again. Applications may submit
up to 16 tightly packed uploads per call, each at most 1,024 by 1,024; the cache accepts at most 64
IDs including the fallback, then returns Unsupported. Unknown IDs, invalid pitch/size, duplicate IDs
and nonfinite UVs return InvalidDescriptor. Upload spans last only for the call. Native textures stay
owned by the surface; staging survives the protecting frame fence, and teardown waits for GPU work.
Vulkan swapchain recreation clears the table, so the caller resubmits source uploads (DX12 retains
its device-owned table). Linear clamp sampling uses the surface's native descriptor/sampler machinery,
with separate scene upload diagnostics. No software image composite or native handle crosses the API.
Linux pixel acceptance verifies UV-selected red/green texels, cached reuse and reupload after resize;
DX12 implements matching UV/SRV/sampler bindings but requires its target-host execution evidence.

The CI TSan gate disables native backends because the system Mesa library is not instrumented and
reports driver-internal mutex races at teardown (run 37045590037). Portable engine concurrency remains
instrumented. Development and ASan/UBSan retain native Vulkan gates and their lifecycle checks.

## Native offscreen scene composition

`SceneDrawData::offscreen` selects a private backbuffer-compatible color attachment instead of the
acquired image; false preserves direct-draw compatibility. Vulkan/DX12 retain color/depth/upload
resources in the acquired frame slot protected by its fence. `CompositeScene` performs a native GPU
copy to the acquired image, transitions the source to CopySource and restores the destination to
RenderTarget for UI. It exports no pixels or native handles. A pending offscreen scene must be copied
exactly once before UI/Present; repeated scene/UI/copy or incompatible full-frame composite calls
return InvalidDescriptor. Resize drains prior work; destroying an abandoned acquired frame remains
safe and idempotent. `sceneOffscreenDrawCalls` and `sceneComposites` count accepted operations, separate
from legacy CPU RGBA8 composition. The Vulkan pixel gate checks the actual copied texture pixels,
resize, rejected out-of-order/duplicate composition and teardown. Application graph lifetime does
not determine GPU resource lifetime: the surface/fence owns submitted storage after callbacks return.
`softwareRasterizer` reports DX12 WARP and Vulkan CPU-device selection explicitly, including in the
Showcase profiler/report. Software-driver acceptance does not certify physical-GPU output.

## Native mesh batches

`SceneDrawData::batches` optionally selects up to 4,096 `SceneMeshBatch` index/instance ranges.
Each range starts on a triangle boundary and contains complete triangles and at least one instance.
Indices address the complete shared vertex upload; ranges may overlap for submeshes or repeated
geometry. Vulkan and DX12 validate every range, including arithmetic overflow, before GPU allocation
or recording. Invalid descriptors leave the frame available for a corrected submission.
Empty batches retain the existing whole-mesh/all-instances draw; empty instances still select one
identity transform. Total upload limits remain 65,535 vertices, 1,048,576 indices, and 4,096 instances.
The MVP, lighting, texture, and base material remain common, with per-instance transform/tint.
All draws share one depth attachment and fence-owned upload allocation. Clipping, offscreen copy,
UI order, resize/recovery, and one Scene submission per frame keep their existing rules.
`sceneDrawCalls` counts submissions, and `sceneInstances` counts uploaded records even when a range
is referenced more than once. The descriptor is a C++ boundary requiring consumer rebuild; no
serialized asset format or stable C ABI changed. See [ADR-0002](../../Roadmap/en/ADR-0002-Editor-Scene-Mesh-Batches.md).
Portable range tests and Vulkan X11 pixels verify distinct geometry/instance offsets and rejection
followed by a successful draw. DX12 execution acceptance and authored-mesh Editor residency remain open.

## Metal Showcase scene and frame ownership

Metal now implements the same indexed/lit/depth-tested scene, hardware instance, sampled material,
mesh-batch and offscreen-copy source contracts. It uses a BGRA8 private color attachment and D32
texture in each command-buffer-protected slot; `CompositeScene` blits on the GPU into a
non-framebuffer-only CAMetalLayer drawable. UI loads existing scene color. Row-major MVPs use explicit
row dot products, preserving the DX12/Metal clip-space Y convention. Scene and UI texture tables
remain separate; scene IDs are immutable and cached through resize, with shared/managed texture
storage selected for unified/discrete Macs. Upload bounds and submission-order errors are rejected
before encoding. Zero extent remains suspended until a nonzero resize; submitted work is drained
before resize and teardown, and command-buffer failures return DeviceLost. Per-call autorelease pools
bound temporary Objective-C objects in applications without an outer Cocoa run loop.

`window_presentation.metal_scene` compiles the actual private adapter into a test-only translation
unit to read GPU pixels without adding native handles to the public API. It exercises depth-order
invariance, sampled texels, instance/batch offsets, UI/copy ordering, rejected descriptors, resize,
zero extent and abandoned recording teardown; a PPM capture is retained. Cocoa synthetic key/pointer
translation is checked separately from human input acceptance. A missing Metal device or WindowServer
screen returns UNSUPPORTED (77), never PASS. ✅ The hosted macOS gate passes Development (73/73) and mimalloc (63/63) CTest, including native pixels,
Cocoa input, depth and lifecycle. Shipping/Full isolated packages also pass eight-room Metal graph
smoke (96 frames); see [the record](../../Apps/Showcase/evidence/V1-Metal-Hosted-CI-2026-10-04/acceptance.md).
These commands ran on GitHub macOS runners; physical Mac visuals and full interactive parity remain open.
