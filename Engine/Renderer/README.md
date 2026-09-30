# Nexora rendering contracts (V1-M2 / V1-M3 / V2-M2 / V2-M3)

This slice establishes the backend-neutral contracts, a validation backend, and the native
offscreen execution path used by the V1-M3 platform gates.

## V1-M2 contract

- `Shaders/Triangle.slang` is the canonical shader source.
- `Triangle.reflection.json` is the reviewable, versioned canonical pipeline-layout fixture for
  DXIL, SPIR-V, and MSL contract validation. With `NEXORA_ENABLE_SLANG=ON`, CMake regenerates
  target reflection and checks it against this fixture and the C++ RHI layout hash.
- Public RHI types contain only descriptors, enums, and generational handles. DX12, Vulkan, and Metal native objects remain isolated in private backend modules.
- Canonical reflection includes stable resource identity, binding, type, stage visibility, constant byte size, and a deterministic layout hash.

Slang cross-compilation is an optional build-time gate because not every environment has `slangc`.
When enabled, `build.shader_crosscompile` verifies non-empty DXIL, SPIR-V, and MSL outputs,
canonical reflection equality, the C++ layout hash, and the absence of backend-native types in
public RHI headers. `renderer.contracts` then feeds the generated artifact paths to the platform
native device and executes the same triangle workload.

`Nexora/Renderer/GoldenImage.h` supplies a deterministic, named-case RGBA8 harness used by
`renderer.golden_image_acceptance`: exact dimensions, per-channel tolerance, differing-pixel
budget/count, and a stable FNV-1a checksum. Platform runners can feed captured swapchain/offscreen
pixels into this helper without introducing backend types into the renderer API.

## Shared shader library contract

`Shaders/Nexora/Common.slang` is the only shared shader authoring module. Its helper sources cover
the roadmap's portable PBR/IBL, StylizedPBR, Anime, Vegetation, Water, Unlit, shadow/post-process,
skinning/instancing/Forward+, variant-key, and retained-mode UI math. Resource sampling remains in
the owning Slang entry point so tier-1 bindings and bindless paths can share the same calculations;
RHI never owns this compiler or material policy.

`PbrSmoke.slang` is the representative PBR vertex/fragment contract. It exercises base-color,
normal, ORM, emission, irradiance cube, prefiltered environment cube, BRDF LUT, and texture-array resources
with explicit Vulkan binding sets while retaining DX register declarations. `UiSmoke.slang` is the
representative UI vertex/fragment contract for logical-coordinate transforms, atlas sampling,
nine-slice UVs, rect clip, and straight-alpha composition. `ShaderLibrarySmoke.slang` keeps the
remaining pure helper functions visible to the build contract. All three produce SPIR-V and MSL on
Slang-enabled Linux/macOS builds and DXIL on Windows; platform execution, native pipeline creation,
Metal fallback parity, and golden-image checks remain target-host gates.

## V1-M3 contract

`RenderGraph` derives RAW/WAR/WAW dependencies, rejects cycles, topologically orders passes, computes transient lifetimes, and emits state transitions. `PipelineCache` coalesces identical asynchronous requests. The validation device rejects stale resources, invalid transitions, rendering-scope violations, missing pipelines, and presenting a non-Present resource.

Passes must declare each texture exactly once: a texture cannot be both read and written by the
same pass. Unused transients are lifetime-elided and never allocated. Command lists are single-use;
submission while a rendering scope is open, submission to another device, a second submission, or
recording after submission is rejected. `MakePipelineCacheKey` makes shader generation explicit;
pipeline identity is the complete layout/shader/generation/format/type key
(debug labels are deliberately excluded), so hash collisions cannot alias cache entries.

The executable test runs `Offscreen -> Main -> Present` through this contract and verifies pass,
barrier, draw, submit, and present counts. The platform CI matrix requires DX12 on Windows,
Vulkan on Linux, and Metal on macOS to execute the same workload. The current milestone is
offscreen; descriptor indexing, multi-queue synchronization, transient heap aliasing, and
window-system presentation remain explicit expansion points.

## Ownership and lifetime

`NexoraRenderer -> NexoraRHI -> NexoraCore`. RenderGraph owns transient textures only for one execution and waits idle before releasing them. Imported resources remain caller-owned. A `PipelineCache` must be destroyed before its device and job system; destruction waits outstanding creation jobs and releases cached pipelines.

## Showcase Phase B scene-frame contract

`SceneFrame` is the Renderer-owned public model for the first 3D slice: camera, indexed mesh,
material, and directional light values own their CPU storage. `MakeProceduralRenderingRoom()`
returns deterministic cube content without asset dependencies, while `ValidateSceneFrame()` rejects
empty geometry, invalid indices, invalid clip planes, and non-finite material or light values.

`FrameResources` exclusively owns a vertex buffer, index buffer, constant buffer, depth texture,
sampled albedo texture, and immutable sampler policy. It is non-copyable and non-movable, waits for
device idle at shutdown, and releases resources in reverse dependency order. The device must outlive
it and callers serialize access. The validation backend provides headless contract evidence; native
descriptor/depth binding and interactive visual acceptance remain separate target-host gates.

## V2-M2 GPUScene contract

`GPUScene` owns CPU-side render-object records and stable slot identities. A `GPUObjectHandle` is an
index plus a nonzero generation; create and ordinary updates retain the index. Destroy invalidates
the generation immediately, removes the object from reference extraction, and queues the slot for
reuse only after `Collect(completed_fence)` reaches its retirement fence. Repeated destroy and every
read or write through a stale handle fail without changing the free list.

Each active record stores current and previous transforms, world-space sphere bounds, mesh and
material resource indices, visibility flags, and selected-LOD metadata. Create initializes previous
from current. Transform writes never change previous; `CommitFrame()` advances previous from current
after extraction, so multiple writes in one frame preserve motion history. A reused slot is
initialized entirely from its new descriptor and cannot inherit history from its prior generation.

`ExtractUpdates()` consumes only dirty records and pending retirements. Dirty masks distinguish full
create, transform, bounds, resources, visibility, LOD, and retirement work. Records are ordered by
slot and generation for deterministic upload. `ExtractReferenceSnapshot()` independently emits all
active records in slot order and counts visible draw instances, providing the CPU reference used to
compare identity and every render datum before V2-M3 GPU culling is introduced.

GPUScene methods are externally synchronized: callers must serialize mutation, extraction,
collection, commit, and reads. Failed handle operations return `false` or `std::nullopt`; they do not
throw. Upload batches and snapshots own their returned data. `Clear()`/destruction discard active,
dirty, free, and pending-retirement state and therefore require the caller to have ended any GPU use
of those slots.

## V2-M3 GPU-driven contract

`BuildGPUDrivenCommands()` is the deterministic CPU reference for the backend compute pipeline. It
consumes an immutable GPUScene snapshot and performs visibility filtering, sphere/frustum culling,
distance rejection, distance-based LOD selection, conservative Hi-Z occlusion, visible-instance
compaction, material/mesh/LOD classification, and indirect-command generation. Results are ordered
by classification key and stable object identity. One command addresses each contiguous compacted
bin, so CPU submission scales with bins instead of individual objects.

`HiZPyramid` stores standard `[0, 1]` depth and reduces each mip with maximum depth. Missing or
invalid history accepts objects, `Invalidated` bypasses testing after camera teleports, fast rotation,
or major occluder changes, and `Relaxed` increases bias for uncertain history. Callers own snapshots
and pyramids and must keep a referenced pyramid alive during generation. The API retains no inputs,
allocates no GPU resources, and does not mutate GPUScene.

Backends must preserve these stage semantics. `CompareGPUDrivenResults()` provides an explicit
correctness gate that reports the first compacted-instance or indirect-command mismatch. It is not
called by the normal rendering path: `RecordGPUDrivenExecution()` records one compute dispatch and
one indirect submission for all generated bins without exposing a readback operation or issuing a
CPU draw for each object.

Generated bins use the single RHI-owned `GPUDrivenIndirectCommand` ABI. The native draw prefix and
classification suffix have a fixed 36-byte stride, and the compute shader imports the same word
offset definitions as C++. Vulkan, D3D12, and Metal adapters may select their native submission API,
but may not translate this buffer into backend-specific command layouts.

`RecordGPUDrivenCompute()` and `RecordGPUDrivenIndirect()` are the per-pass stage recorders for RenderGraph compute and graphics callbacks; `RecordGPUDrivenExecution()` delegates to those same functions for callers holding both command lists. Passes bind their own pipeline and buffers before recording. The stage recorders do not submit, allocate, synchronize, or read back resources. RenderGraph owns only resources declared to it; buffers captured by callbacks remain caller-owned and must stay valid through graph execution. Storage-buffer uses are not currently declared to RenderGraph, so it does not retain those buffers or emit per-buffer ownership barriers; callers must provide the required lifetime and synchronization. The focused Linux Vulkan gate runs these recorders with Slang 2026.18 on Mesa lavapipe and compares bounded readbacks against the CPU reference, ; the full development preset gate also passes in GitHub Actions (run 36609837931). Physical-GPU performance and other native backend parity require separate evidence.

RenderGraph tracks a logical owner queue for every resource. A use on a different compute/graphics
queue emits an ownership barrier even when the resource state is unchanged, and statistics expose
those transfers separately from ordinary state transitions. The graph retains transient ownership
until all submitted work is idle, then releases the resources; imported resources remain
caller-owned. Vulkan's native gate is defined to execute the full frustum/distance/LOD/Hi-Z/compaction/classification/indirect-generation kernel through a RenderGraph compute pass, require an exact CPU/GPU result comparison, transfer ownership to a graphics pass, and issue native indirect drawing. Its four storage slots represent packed scene/view/Hi-Z input, compacted instances, indirect arguments, and statistics. The acceptance test waits for completion, reconstructs the backend result through the explicitly test-only readback seam, and requires an exact `CompareGPUDrivenResults()` match; normal recording performs no readback. Native queue/timeline separation, DX12/Metal execution, and target-host parity remain open gates; none is inferred from Linux Vulkan coverage.
