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

Vulkan retains a geometrically grown Scene vertex/index/instance upload allocation in each
fence-protected frame slot. Acquire waits that slot before any write or replacement; smaller and
Scene-free frames retain capacity. Growth stages a new bound buffer/memory pair before releasing the
old pair, so allocation failure preserves the old upload for a corrected smaller draw. Descriptor
budgets cap each retained buffer at 8 MiB (up to three slots); temporary replacement may hold both
old and new allocations. Every accepted submission still copies fresh geometry and packed instances.
Resize/recovery drains GPU work and releases all slot uploads; teardown is idempotent, including an
abandoned recording. Scene color/depth targets remain per-draw resources. This reuses upload capacity
and does not provide persistent per-asset GPU geometry caching. Public descriptors, shaders and
submission order are unchanged. `window_presentation.vulkan_scene_upload_reuse` compiles the actual
private adapter with test-only Vulkan call tracing. It verifies 100 steady draws without new buffer
allocations, maximum descriptor capacity, changed pixels, failed growth, slot fencing, resize and
leak-free teardown.
No tracing or native handles are exported by the production API.

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

## Exact affine scene instances

`SceneInstance::model_transform` optionally supplies a row-major 4x4 affine model matrix with
last row `[0, 0, 0, 1]`. It overrides translation/scale/rotation (unused TRS values are ignored),
while tint is always checked. `ValidateSceneInstance` is a pure CPU validator shared with native
packing. Reject nonfinite, nonaffine or singular matrices, and inverse-transpose coefficients
outside the finite float range. Error-free binary64 product/sum expansions classify the determinant
of binary32 inputs exactly, rejecting dependent rows despite large-term cancellation and retaining
invertible cancellation cases. Legacy TRS retains the existing nonzero scale and quaternion
validation. Empty instances still select one identity. See [ADR-0003](../../Roadmap/en/ADR-0003-Presentation-Affine-Instances.md).

Vulkan/DX12/Metal convert descriptors into private 112-byte model/normal/tint records. Geometry
uses exact affine rows; normals use the inverse-transpose linear part. Normal rows may share a positive rescale, preserving direction while bounding GPU arithmetic.
Shaders normalize finite input/output normals and light vectors with magnitude scaling, including
zero normals (ambient only), to avoid overflow or underflow during length calculation.
The public optional C++
descriptor is never uploaded directly. DrawScene borrows source spans for the call and copies packed
bytes into fence-owned frame storage before returning; no CPU descriptor or backend handle escapes.
Malformed instances return InvalidDescriptor before recording, leaving the scene submission available
for a later valid draw. Existing instance/batch budgets and frame ordering remain unchanged.

`window_presentation.scene_instance_contract` checks exact points, normal/tangent orthogonality,
mirrors, singular/nonaffine/nonfinite rejection, unused TRS override, identity and bounded uploads.
The native Vulkan pixel gate compares sheared/mirrored instances against independently baked
geometry/normals, including invalid-then-valid submission. Windows/DX12 and macOS/Metal runtime
evidence depends on their CI/target hosts. Source consumers rebuild for the appended C++ field;
stable C/Zig wires and serialized scene formats are unchanged. The Editor's Scene/Game authored
meshes now consume exact matrices through the CPU validator; persistent GPU mesh caching and full
graphical acceptance remain open.

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
The Windows package verifier optionally checks `-ExpectedVulkanDriverLibrary` against the running
process module paths and records that DLL's SHA-256. The hosted Vulkan gate selects a checksum-verified
Mesa lavapipe ICD outside the package and requires software-driver diagnostics, preserving distinct
physical-display/clean-host attestations.

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

## Per-batch opaque materials

`SceneDrawData::materials` borrows at most 64 `SceneMaterial` slots. Each batch's `materialIndex`
selects its base color and immutable texture generation; empty batches use slot zero. An empty
palette preserves the global `base_color` and `textureId` shading path. All colors must be finite
in [0,1], alpha must equal one, and texture zero selects an internal white texture. The reserved
`UINT64_MAX` ID is rejected. Every referenced nonzero texture must already be resident or supplied
in this submission's bounded RGBA8 uploads. Existing descriptor fields still undergo validation.
Material slots are ephemeral and have no Runtime asset identity. Texture changes require new IDs.

Validation precedes scene commands; rejected descriptors leave the acquired frame available for a
corrected draw. Vulkan push constants/descriptors, DX12 aligned per-slot CBVs/descriptors and Metal
copied constants/textures change per batch within the shared depth pass. Existing frame fences own
uploads and texture lifetimes, including offscreen composition and resize. Consumers must rebuild
for the appended C++ fields; stable C/Zig and persisted schemas are unchanged. This Lambert slice
establishes binding only; shared PBR, cooked IBL and opt-in floating-point HDR are described below.
See [ADR-0004](../../Roadmap/en/ADR-0004-Showcase-Materials-HDR.md).

## Shared direct-light PBR

`SceneDrawData::pbr` explicitly selects the shared Slang native entry; legacy Lambert remains the
comparison path. Camera position, vertex tangent XYZ/handedness W and opaque material factors are
borrowed for the call. Tangents must be finite, nonzero and orthogonal to the finite nonzero normal;
W is +1 or -1. Empty palettes also validate the selected global opaque color. PBR light radiance
is finite in [0,65504]. The private normal-row padding carries an exact CPU determinant sign,
avoiding rounded GPU determinant cancellation; geometry directions normalize after positive scaling. Base and emission RGBA8 maps decode sRGB exactly once; +Y normal and ORM
(R=AO, G=perceptual roughness, B=metallic) stay linear. Optional map IDs select immutable native
resources; missing base/ORM/emission select white (emission is multiplied by its zero default),
and a missing normal selects a flat texture with normal scale zero. `UINT64_MAX` and
`UINT64_MAX-1` are internal reserved texture IDs and are rejected in caller descriptors.

The private material packing contract (currently 240 bytes) is shared across adapters. Vulkan allocates aligned
UBOs and descriptor sets per protecting frame; DX12 uses aligned paired CBVs; Metal copies constants
and binds four material maps plus three environment resources/samplers. Rejected missing maps/invalid factors do not consume scene submissions.
Colors are evaluated in linear space and tone-mapped with shared ACES, followed by a single manual
sRGB transfer for UNORM targets (hardware transfer for sRGB attachments). The legacy RGBA8 path
retains this behavior; opt-in HDR defers these operations to Main composition as described below. Public C++ consumers rebuild; Runtime
mesh wire formats and stable C/Zig ABI remain unchanged.

PBR base/emission maps use hardware sRGB views, decoding texels before linear filtering. Each
immutable scene texture owns a paired UNORM/sRGB view: Vulkan uses mutable-format images, DX12
uses typeless resources with two SRVs, and Metal uses pixel-format views. Normal/ORM and legacy
Lambert bindings retain UNORM views. Both views share the texture generation and protecting-fence
lifetime; UI textures retain their existing UNORM contract. Shared shaders consume sampled colors
as linear values and do not decode them again. Midpoint black/white pixel tests compare base and
emission against linear 0.5 factors, and separately verify linear ORM and legacy filtering.
Color filtering is also used by the opt-in floating HDR path; it never adds another display transfer.

## Bounded linear environment textures

`SceneLinearTextureUpload` borrows a tightly packed little-endian RGBA16F mip chain for one draw.
Dimensions are nonzero powers of two, at most 256 per axis; levels span no more than the complete
chain (at most nine). Half values must be finite and nonnegative. Up to 16 uploads and 16 resident
linear generations, including an internal black fallback, are allowed. IDs are immutable, nonzero,
and cannot alias RGBA8 scene generations; reserved IDs are rejected. A failed descriptor does not
consume the frame. Native resource allocation or upload failure reports an error before publication.
Vulkan/DX12 check float texture sampling support and report Unsupported when unavailable.

An optional `SceneEnvironment` names diffuse irradiance, roughness-prefiltered specular and BRDF LUT
resources. Diffuse/LUT have one level; the named specular count must match resident or same-draw
upload metadata. Intensity is finite in [0,32], +Y rotation in [-2pi,2pi]. Latlong U wraps and V clamps;
the LUT clamps both axes. The shared shader samples roughness across the prefiltered mip range,
rotates reflection/normal coordinates consistently, and passes actual linear radiance to shared PBR.
A missing environment explicitly contributes zero, including on retained Lambert comparison draws.

Vulkan uses one eight-binding descriptor set per protecting frame; paired material constants grow
from 64 to 80 bytes. DX12 uses seven SRV tables/static samplers and paired CBVs; Metal uses seven
textures/samplers and copied constants. Linear textures share the existing protecting-fence/drain
boundary, with all mip subresources uploaded and transitioned before sampling. Vulkan candidates
allocate fully before recording copy commands. Public C++ consumers rebuild; stable C/Zig and NXAB
schemas are unchanged. RGBA16F environment resources do not yet mean floating HDR scene targets or
HDR10 monitor output. Native tests exercise HDR radiance, diffuse/metal separation, roughness levels,
rotation, view-dependent reflections, U-seam filtering, IBL disable and resize/re-upload.

## Linear HDR scene composition

`SceneDrawData::hdr` defaults false and requires PBR plus a full-surface offscreen draw. Exposure is
finite in [0,32], defaults 1, and is validated before recording on either path. Invalid combinations
return InvalidDescriptor; unsupported float rendering reports Unsupported. The private material
packet uses its reserved properties.w for linear HDR output within the original 80-byte layout; directional shadows extend it to 208 bytes, and vegetation to 240 bytes.
HDR fragments preserve nonnegative finite radiance up to RGBA16F's 65504 range; they perform no
ACES or display transfer. Each protecting frame owns a RGBA16F color target, depth target and
composite bindings. Main samples that target, applies exposure and the shared ACES implementation,
then performs exactly one manual sRGB transfer for UNORM presentation or hardware transfer for
sRGB attachments. UI follows Main and retains its existing color/blending behavior.

Vulkan records ColorAttachment → ShaderRead with a frame descriptor and framebuffer; DX12 records
RenderTarget → PixelShaderResource with reserved frame SRVs and root constants; Metal ends the scene
encoder and samples its private texture in a drawable encoder. No production CPU pixel readback is
involved. Format changes, frame reuse, resize and shutdown preserve protecting-fence ownership and
drain before release. Legacy offscreen RGBA8 still uses the GPU copy path; direct/Lambert draws remain
available. Public C++ consumers rebuild; stable C/Zig and persistent asset schemas are unchanged.
HDR storage precision does not negotiate an HDR10 display or swapchain.

The tone pass uses one explicit 48-byte fullscreen triangle (float2 position plus float2 UV),
shared by all adapters. Vulkan/DX12 append it to their protecting-frame scene upload; Metal
copies it into the encoder. This avoids compiler-dependent SV_VertexID builtin declaration
ordering while keeping the generated shader artifact checks exact.

## Directional shadows and lighting style

`SceneDrawData::shadow` is optional and requires PBR plus offscreen rendering. Its row-major
light view/projection is finite; resolution is one of 256/512/1024/2048 and normal/slope bias
are finite in [0,0.05]. Each protecting frame owns an R32Float shadow color map and D32Float
visibility depth target. The prepass renders the same indexed instance batches, storing normalized
light-space depth into R32Float after depth testing. Main samples it with four point taps and
shared PCF/bias functions; out-of-frustum receivers remain lit. Vulkan transitions the map
ColorAttachment → ShaderRead; DX12 uses RenderTarget → PixelShaderResource; Metal ends the
shadow encoder before main. Fences, resize and shutdown protect map ownership and release.
Unsupported map formats report Unsupported rather than inventing a shadow.

The shadow portion extends the private material packet to 208 bytes (240 with vegetation), with light matrix, bias/texel settings, tint and
ramp fields. Vulkan's material binding range/stride follows this size; DX12 retains 512-byte
paired constant slots (112-byte scene plus material at offset 256); Metal copies the same packet.
Main adds one shadow map binding (eight sampled maps total). No persistent asset or stable
C/Zig ABI changes. `SceneLightingStyle` optionally supplies bounded shadow/light tint and ramp
controls using shared Common shader functions. Direct light receives PCF visibility; emission
remains independent. Shadow diagnostics count actual successfully recorded passes/instances.

## Bounded HDR bloom and color grade

Optional `SceneBloom` requires HDR; intensity is finite [0,1], linear threshold [0,32],
and radius [1,32] pixels. Optional `SceneColorGrade` also requires HDR with finite saturation
and contrast in [0,2]. Defaults preserve the prior HDR output. Tone constants use three
float4s (48 bytes), packed identically by all adapters and copied into the recording frame;
the first two carry exposure/color/bloom and the third carries optional depth-aware focus.
After optional focus filtering, the tone entry uses twelve bounded neighboring bloom taps,
extracts thresholded radiance with shared math, adds shared bloom before exposure/ACES, then
applies shared color grade and exactly one display transfer. This is a compact two-scale
neighborhood filter, not a separable Gaussian or temporal bloom implementation. UI still follows
Main. No extra target, descriptor, CPU readback or temporal resource ownership is introduced.

Vulkan `recoverablePresentFrames` counts actual submitted frames whose native present reports
OutOfDate/Suboptimal and schedules swapchain replacement. `presentedFrames` still counts Ready
presents only. Resize acceptance accounts for these separate counters rather than assuming every
acquire must present successfully; neither recovery counter proves visual acceptance. Public C++
consumers rebuild, while stable C/Zig and persistent asset schemas remain unchanged.

## PBR vegetation and cutout submissions

`SceneMaterial::alphaCutoff` is finite [0,1]; zero preserves opaque behavior. A positive cutoff
samples the base map alpha in both main and directional shadow fragments and discards rejected
pixels before depth/color writes. Surviving pixels are opaque; this is not sorted alpha blending.
`windAmplitude` is finite [0,0.5] world units. Shared vertex wind bends world positions using
UV.y as root-to-tip weight, preserving UV.y=0 roots; main and shadow use the same caller-owned
`vegetationTime` ([0,3600]) and private material packet. Authored normals remain an approximation
for the bounded bend. `transmissionThickness` and RGB factors are finite [0,1]; shared thin-leaf
back lighting contributes linear radiance with shadow visibility, without making geometry blended.
Lambert submissions reject these PBR-only material effects. Defaults preserve existing clients.

The private PBR packet is 240 bytes / fifteen float4s. Vulkan exposes its UBO to vertex/fragment
stages and binds the PBR layout during shadows; DX12 uses matching CBVs/root signatures, and Metal
copies the packet to both stage bindings. The protecting frame owns copies and sampled texture
generations. No production readback or native handles escape. Public C++ clients rebuild; stable
C/Zig/NXAB formats remain unchanged. Native fixtures distinguish alpha-zero opaque/cutout behavior,
shadow agreement, two wind times, exact GPU replay and green back-light transmission.

## Emission-only materials and shadow work

PBR `SceneMaterial::unlit` samples emission and alpha cutout, then uses the same linear HDR or
tone/transfer output as lit PBR. It bypasses normal/ORM/BRDF/IBL/shadow sampling. Lambert rejects
unlit. `castsShadow` defaults true; false excludes a batch from the current frame shadow draw
while leaving depth-map clearing and all main-pass rendering intact. `sceneShadowInstances`
counts instances actually submitted per shadow batch, including repeated geometry batches.
The private packet stays 240 bytes; its previously reserved final float carries unlit. Public C++
consumers rebuild; stable C/Zig and NXAB contracts remain unchanged. Native GPU fixtures check an
emissive non-caster remains visible while the receiver becomes lit.

## Depth-aware HDR focus

Optional `SceneDepthOfField` requires offscreen HDR PBR. Focus distance is finite (0,10000]
world units measured from `cameraPosition`; strength is [0,4] and radius [1,32] physical pixels.
Absence or zero strength preserves sharp composition. The existing RGBA16F scene target stores
linear radiance in RGB and bounded camera distance in alpha; untouched pixels use 65504 as a
far-distance sentinel. Alpha is internal distance data, not blended transparency. Non-HDR scene
output and the final composite still have alpha 1. Cutout discard remains before depth/color writes.

The private tone packet is now three float4s / 48 bytes, copied into the protecting frame by
DX12, Vulkan and Metal. A bounded twelve-tap depth-aware neighborhood filter rejects samples
from different depth layers and filters linear radiance before bloom/exposure/ACES. This is an
approximate spatial focus filter, with no temporal history, additional target, depth descriptor or
production readback. UI is drawn after composition and remains sharp. Clients of the public C++
header rebuild; stable C/Zig and persistent asset schemas remain unchanged. Native fixtures verify
an out-of-focus emissive edge changes pixels while UI stays unchanged; input acceptance requires
exact focus off/on restoration. The depth-aware filter also keeps camera-centred skybox radiance
in the same HDR composition as foreground objects.
