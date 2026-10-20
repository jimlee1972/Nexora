# Nexora rendering contracts (V1-M2 / V1-M3 / V2-M2 / V2-M3)

## V2-GFX pipeline admission

Pipeline cache keys now include all per-feature minimum tiers and the minimum DXIL device Shader
Model from `PipelineDescriptor::graphics_requirements`, in addition to the existing complete
layout/shader/generation/format/type identity. Failed capability admission remains a failed future;
it cannot alias a successfully cached baseline PSO. Default requirements preserve existing callers.
The RHI's explicit `SelectGraphicsPath` policy evaluates preferred and fallback requirement sets
without running callbacks or substituting shaders. Renderer callers own the chosen variant and its
artifact validation. RenderGraph scheduling, native barriers, descriptor ownership and submission
lifetimes are unchanged by this capability foundation; parallel recording remains a separate gate.

Vulkan compute dispatches now retain command-list-owned immutable descriptor snapshots, including
when independent lists record concurrently against pre-created disjoint buffers. This closes the
shared compute-set overwrite hazard; it does not change RenderGraph's serial execution, captured
buffer ownership, resource-state planning or capability admission. See the RHI descriptor ownership
contract and the paired V2 graphics plans for the focused native acceptance and remaining gates.

## Dedicated 2D extraction

✅ The optional [TwoD module](../TwoD/README.md) extracts existing World affine poses through a
rotated orthographic camera, preserves stable alpha/Y/layer order, culls offscreen sprites, and
merges only adjacent matching atlas batches. It submits backend-neutral triangles through the
existing Presentation unlit path; Renderer/RHI do not acquire a private 2D backend or new native
public types. Mixed stress retains CPU/GPU intervals and covered-AABB diagnostics. Linux Vulkan
correctness does not establish Windows target performance or calibrated fragment overdraw.


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
budget/count, and a stable standard 64-bit FNV-1a checksum (offset basis `0xcbf29ce484222325`). Platform runners can feed captured swapchain/offscreen
pixels into this helper without introducing backend types into the renderer API.
`renderer.vulkan_golden_triangle` (Slang-enabled Linux Vulkan builds) renders the triangle frame
offscreen at 64x64 on Mesa lavapipe through `ReadTextureForTesting`, requires two renders to be
identical and to contain real coverage, and compares against the committed baseline
`Tests/Renderer/Golden/triangle_vulkan_lavapipe_64x64.rgba` (per-channel tolerance 2, at most 1%
differing pixels). Run the test binary with `NEXORA_GOLDEN_UPDATE=<path>` to regenerate it. This is a
Linux software-rasterizer reference only; DX12, Metal, and physical-GPU baselines remain
target-host gates.

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

## Material and shading integration contract

`MaterialSchema` records a stable shading-model enum, an independent surface mode, feature bits,
shader/profile identity, typed parameters, and texture resource identities. The six shared models
(PBR, StylizedPBR, Anime, Vegetation, Water, and Unlit) select variants in the same renderer;
IBL, shadow/post-process, instancing, skinning, and Forward+ are explicit feature-key dimensions.
`StripMaterialVariants()` retains only validated, unique variants observed during asset scanning and
does not impose a global variant-count ceiling.

Materials store no backend descriptor objects. `BindMaterialResources()` resolves resource IDs at
the binding boundary and substitutes a semantic-specific fallback for missing textures, while
reporting each substitution. The fallback is requested from the resolver only for textures that
actually failed to resolve. `ReflectMaterial()` exposes typed fields, constant-buffer offsets,
texture semantics, and a stable layout hash to Material Inspector adapters.

`MaterialRegistry` owns published CPU schemas. Publish and reload are externally synchronized;
invalid input is rejected without changing the live entry. Successful hot reload atomically replaces
the schema and advances its generation, invalidating prior handles. Returned schema pointers remain
owned by the registry and are valid only until the next mutation. These portable contracts and the
shared Slang helpers are covered in Linux; DXIL execution, native Metal module execution, and
physical-GPU golden images remain target-host acceptance gates.

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
empty geometry, invalid indices, non-finite or inverted clip planes, an invalid field of view, a
non-finite camera position or target or a camera targeting its own position, non-finite base color,
roughness or metallic outside `[0, 1]`, a non-finite or zero light direction, a non-finite light
color, and a non-finite or negative light intensity. Every comparison is NaN-safe because these
values are uploaded verbatim into the constant buffer.

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

`InstanceId()` is a process-unique, nonzero identity for a scene's contents. Handles carry no scene
identity, so code that keeps handles across calls binds to it: it moves with the contents (a
moved-to scene keeps it) and changes on `Clear()`, after which slot/generation pairs restart and
every earlier handle is stale.

Entity transforms reach the GPUScene only through the Runtime's `RenderSceneSync`, which writes each
entity's exact world matrix (never a local transform) and conservative world bounds; see the Runtime
README's "Render sync" section. The renderer does not depend on the Runtime.
`ExecuteSceneFrame` with zero visible meshes (everything culled) still runs every pass and clears
its targets, but records no mesh draw: a zero-instance draw is invalid on every backend.

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
but may not translate this buffer into backend-specific command layouts. Every backend checks the
full range a `DrawIndirect(command_count)` reads against the bound buffer with the shared
`rhi::IndirectDrawRangeFits` rule (`(command_count - 1) * stride + offset + 16 <= size`) and
rejects an undersized buffer instead of reading past it; a buffer that cannot hold one draw does not
bind.

`RecordGPUDrivenCompute()` and `RecordGPUDrivenIndirect()` are the per-pass stage recorders for RenderGraph compute and graphics callbacks; `RecordGPUDrivenExecution()` delegates to those same functions for callers holding both command lists. Passes bind their own pipeline and buffers before recording. The stage recorders do not submit, allocate, synchronize, or read back resources. RenderGraph owns only resources declared to it; buffers captured by callbacks remain caller-owned and must stay valid through graph execution. Passes now declare imported buffer reads/writes explicitly; RenderGraph derives their hazards and emits whole-buffer dependencies before callbacks. Hidden callback accesses remain the caller's responsibility, and the graph never retains or destroys imported buffers. The focused Linux Vulkan gate runs these recorders with Slang 2026.18 on Mesa lavapipe and compares bounded readbacks against the CPU reference; the full development preset gate also passes in GitHub Actions (run 36609837931). Physical-GPU performance and other native backend parity require separate evidence.

RenderGraph tracks a logical owner queue for every resource. A use on a different compute/graphics
queue emits an ownership barrier even when the resource state is unchanged, and statistics expose
those transfers separately from ordinary state transitions. The graph retains transient ownership
until all submitted work is idle, then releases the resources; imported resources remain
caller-owned. Vulkan's native gate is defined to execute the full frustum/distance/LOD/Hi-Z/compaction/classification/indirect-generation kernel through a RenderGraph compute pass, require an exact CPU/GPU result comparison, transfer ownership to a graphics pass, and issue native indirect drawing. Its four storage slots represent packed scene/view/Hi-Z input, compacted instances, indirect arguments, and statistics. The acceptance test waits for completion, reconstructs the backend result through the explicitly test-only readback seam, and requires an exact `CompareGPUDrivenResults()` match; normal recording performs no readback. The local Windows/DX12 gate now also passes this comparison through the same four-slot shader contract; Metal is now wired to the same full kernel and CPU oracle; native queue/timeline separation and full target-host parity remain open gates. None of those open gates is inferred from Linux Vulkan coverage.

## Native-owner RenderGraph execution

`ImportExternalTexture` records a positive-extent logical color resource owned by a native adapter;
it does not import a fabricated RHI handle, allocate storage or expose native devices/images.
`AddExternalPass` declares its reads/writes and owner callback on the graphics queue. RHI and native
ownership cannot be mixed within a pass; each execution method rejects the other resource/callback
mode before dispatch. Both modes share dependency derivation, cycle rejection and topological order.
`ExecuteExternal` borrows pass names, texture uses and requested before/after states only for each
callback. The owner materializes these states in its native operations and retains physical storage,
GPU barriers, fences and teardown. Logical format metadata describes shader-visible color; the owner
negotiates its compatible native presentation format. No callback/context may escape the execution.
Exceptions stop successors immediately; the external owner must drain or abandon its acquired frame
safely. Graph destruction releases CPU declarations/captures without releasing owner GPU resources.
`external_transition_count` counts successfully requested logical state changes, separate from the
RHI executor's `barrier_count`; `completed_pass_count` counts successful callbacks/submissions and is
not a GPU completion fence. Portable tests cover ordering, ownership mismatch, cycles and callback
failure containment. Native pixel/interaction acceptance verifies the actual bound operations.

Showcase binds Offscreen (private color/depth draw), Main (GPU color copy or HDR tone mapping into the acquired image),
UI (attachment load/native UI draw) and Present to this graph. Main briefly transitions its destination
through copy state and restores RenderTarget internally; its logical write describes that output
state. Vulkan/DX12 Presentation own and validate all physical transitions. The native graph counters
and actual successful callback order are reported separately from the RHI offscreen contract graph.

## Mesh tangent preparation

`GenerateMeshTangents` returns owning unit tangent XYZ and handedness W for a bounded indexed mesh;
its input is borrowed and never mutated. UV and normal seams, including mirrored UV islands, must
already have split vertices. Double-precision accumulation and Gram-Schmidt orthogonalization retain
UV orientation; degenerate UVs use a stable orthogonal basis. Invalid indices, non-finite input,
zero normals or descriptor budgets return `nullopt`. The Renderer mesh's serialized layout is unchanged.
Showcase prepares tangents once for its immutable courtyard geometry and copies the owning results
into the native submission; native adapters transform tangents with the model matrix, normals with
the inverse transpose and mirrored handedness with the model determinant.

External HDR imports describe the actual `Rgba16Float` format. Main requests ShaderRead for these
inputs; legacy RGBA8 copy inputs request CopySource. The Presentation owner records the physical
barriers and fullscreen exposure/ACES/display pass before UI. Logical counters continue to describe
successful real callbacks; format metadata never grants access to native resource handles.

## V2 graph buffer dependencies and compiled synchronization

`ImportBuffer` borrows a generational handle, an initial GPU access scope and logical queue.
The default `Undefined` scope conservatively synchronizes prior GPU work; it does not discard data.
Explicit initial scopes are caller entry contracts on every execution. Each physical imported
buffer or texture has one graph ID; duplicate imports are rejected to prevent hidden aliases.
Buffers have no transient allocation or subrange tracking in this slice. Callers retain the device,
resources and captured callback data through completion and serialize graph mutation/execution.

`PassDescriptor::buffer_reads` accepts `ShaderRead` or `IndirectRead`; `buffer_writes` accepts
`StorageReadWrite`. A buffer appears once per pass; a storage declaration includes its reads.
The callback still receives the texture-handle span and captures its borrowed buffer handles.
Every actual GPU access must be declared; callback captures cannot be inferred. External-owner
passes/execution reject RHI buffers. Existing aggregate fields and texture-only statistics retain
meaning; appended buffer statistics count whole-buffer barriers and logical queue handoffs.

`Compile` infers texture and buffer RAW/WAR/WAW edges and orders read/read uses that differ in
state/access or logical queue, waiting for all earlier readers. Same-queue, same-state readers
remain reorderable. A deterministic topological order fixes immutable per-pass barrier plans,
including same-access storage WAW dependencies and the first use of each buffer (which validates
stale handles before its callback). Failed compilation invalidates execution. Execution replays
those plans without recomputing ownership, submits serially, and waits idle before releasing
transients, including on exceptions. Callback failure stops submission and successors; prior
accepted work and CPU effects remain visible. All built-in texture paths use list-local state,
committed only after queue acceptance; unsubmitted failed/cancelled recordings can retry with
unchanged entry contracts and fresh lists after every worker joins. D3D12 pending buffer uploads
also remain available after abandonment. Custom backends must provide equivalent isolation before
advertising limited parallel admission. Successful repeated buffer execution with default
`Undefined` remains conservative.

`renderer.graph_buffer_dependencies` covers hazards, read/read handoffs, cycles, declarations,
aliases, stale imports, repeated execution, callback failure and initial transient queue ownership.
`renderer.contracts` exercises native whole-buffer WAW/RAW recording on each required host backend.
`renderer.v2_gpu_driven` declares actual input/output/indirect buffers, recomputes twice with WAW
ordering, derives the graphics dependency from its indirect read, and compares test-only readbacks
to the CPU reference on Vulkan/D3D12. Production passes perform no readback.

Default recording remains serial; the bounded opt-in compute/color-target paths are described below. Logical
queues currently alias one native queue. Asynchronous queue-family transfers, general parallel
recording, Enhanced Barriers, aliasing and reusable fence-retired workers remain open gates.

## Vulkan texture recording isolation

Vulkan lists now keep texture entry revisions, native metadata and logical transitions privately.
Rendering validates the list-local state; the device commits state only after queue acceptance and
rejects stale generations/revisions before native execution. Readback also invalidates older
recorded snapshots. All framebuffers from every rendering scope retire after completion or after
an unsubmitted command buffer/pool is freed. Callers retain resources, pre-create immutable
pipelines, serialize mutation/submission and never record the same list from two threads.

The native graphics test records six scopes per list over 24 disjoint mixed-format/extent targets,
forces four workers to overlap, joins before reverse submission and compares every pixel exactly
to nonempty serial references. Abandonment preserves state/output; stale lists fail before queueing,
and a serial graph can recover from a callback failure and replay the same imported target.
Synchronization/object validation covers this path and the existing compute descriptor test.

That prerequisite isolated Vulkan recording. The independent color-target admission below now
adds bounded graph graphics and D3D12/Metal texture snapshots. Shared-resource policies,
asynchronous reuse and matched performance remain open. See the
[RHI lifetime contract](../RHI/README.md#vulkan-texture-snapshots-and-framebuffer-retirement-v2-gfx-1-slice).

## Bounded opt-in parallel compute and color-target recording

The existing `Execute(device)` stays serial. Callers can pass `ExecutionOptions` with
`RecordingMode::Parallel`, a borrowed started `core::JobSystem` and a nonzero batch limit (default
four). Each participating pass must explicitly set `allow_parallel_recording`. This is a promise
that the callback records admitted buffer-only compute or color-target graphics commands, declares
every accessed texture/buffer, uses pre-created immutable pipelines/resources and has thread-safe
captures. Callbacks must not allocate, upload,
mutate or destroy device resources, submit/wait/read back, mutate the graph, or control the job
pool's lifecycle. CPU effects of successful siblings in a failed batch cannot be rolled back.

Parallel admission additionally requires the device's limited `independent_compute_recording`
or `independent_graphics_recording` contract for the corresponding pass. Validation, supported Vulkan compute devices and isolated D3D12/Metal buffer recording admit it;
custom devices default to serial fallback. The full `ParallelCommandRecording` feature stays
disabled/unimplemented. This is limited admission, with no shader requirement, SDK tier or performance claim.

A batch contains two or more contiguous passes in the compiled topological order, bounded by both
the requested limit and the pool's worker count. Compute passes declare at least one buffer and
no textures. Graphics passes declare at least one color-target write, exclusively in RenderTarget
state, no texture reads and no buffer writes. Declared read-only buffers may supply graphics data.
Batches may mix these independently admitted queues, but have no dependency edge within the batch
and share no texture. Buffers also stay disjoint unless `shared_buffer_read_recording` admits
compatible same-queue/same-access read/read sharing. Every shared buffer write still ends the cohort,
as do state/queue dependencies; custom backends default closed. A conflict ends the batch; singleton,
texture-sampling, copy, external, unmarked and unsupported paths remain serial. Explicit and inferred dependencies still determine
order. Each completed batch submits and waits before recording the next batch, so dependent
callbacks cannot run before their predecessors complete. External execution never uses this pool.

The coordinator allocates all lists/allocators before launching jobs, hands each job exclusive
recording ownership, joins every started job and submits in compiled order, regardless of callback
completion order. A callback or scheduling exception submits none of that batch and prevents
successors. Already submitted earlier batches are retained. Errors are selected in graph order
among callbacks, ahead of a later scheduling error. Submission and `WaitForSubmission` execute
only on the coordinator; lists and their descriptor pools survive completion. A submit/wait error
attempts `WaitIdle` before freeing the affected lists, then propagates. This waits for retirement;
it does not implement asynchronous worker reuse or multi-queue timelines.

`Execute` uses the calling thread as coordinator. The caller keeps the supplied pool started
and alive and serializes its Start/Stop/destruction against the call; resource/device/graph mutation
also stays serialized. A missing, stopped or one-worker pool, or a limit of one, selects serial
fallback. Invalid modes and a zero limit fail before recording. A coordinator on the same
recording pool now selects serial fallback, preventing worker exhaustion. A worker of a different
pool can coordinate parallel recording if the pools have no cyclic blocking dependencies. This
does not make general JobSystem waits reentrant or permit worker-controlled pool shutdown.

Statistics distinguish successful parallel/serial recording, parallel batch count, serial fallback
under a parallel request and maximum scheduled batch size. The last is a scheduling bound, not a
measured concurrency/timing peak. Successful siblings may count as recorded even when their failed
batch never submits; `completed_pass_count` counts only submissions whose completion wait returns.
Buffer/texture barrier counters still count emitted recording work. All C++ modules must rebuild
for the appended capability/descriptor/statistics fields; the stable C ABI is unchanged.

`renderer.parallel_graph_recording` forces overlap and reverse callback completion, checks exact
submission order and batch bounds, resource/explicit-edge fallbacks, dependent waves, default
serial compatibility, missing/stopped/single-worker pools, deterministic callback errors, no failed
batch submission and completion-before-retirement (including submit errors after modeled queue
acceptance). The existing Vulkan descriptor test additionally runs 24 unique workloads through two
WAW graph waves in serial and four-worker modes, requires native worker overlap, compares 48
results to the CPU reference and verifies that aborted recording preserves all sentinel output
before reusing the same buffers. Tests do not establish reduced CPU overhead. Shared-texture reads,
asynchronous retirement, compiler/artifact readiness and matched CPU/GPU/memory benchmarks remain
separate gates.

The Windows `renderer.d3d12_recording_isolation` target reuses the Vulkan workload and exact CPU
oracle for the same 320 dispatch and 48 graph comparisons. It also exercises D3D12 abandoned
upload/state recovery and stale-list rejection. D3D12 lists capture buffer revisions while
recording; upload, accepted submission or nonempty readback invalidates previously recorded lists
that mention those buffers. Re-record rejected lists before retrying. Graph batch disjointness and
completion between dependent batches satisfy this restriction without a shared preparation state.
See the [RHI snapshot contract](../RHI/README.md#d3d12-buffer-recording-snapshots-v2-gfx-1-slice).

`renderer.metal_recording_isolation` runs the same 320 dispatch and 48 graph CPU comparisons on
macOS using `NEXORA_SLANG_COMPUTE_METAL_PATH`. Slang-enabled native builds require the real
GPU-driven artifact and run the full culling/LOD/Hi-Z/compaction/classification/indirect comparison;
the old no-op compute smoke cannot satisfy that gate. Metal lists cache bindings before encoder
creation and replay them when a buffer barrier ends the encoder and the pipeline is rebound.
Worker compute entry points manage autorelease pools while ARC retains list-owned native objects.
The test-only shared-buffer readback waits for queue completion; production passes never read back.
This remains the disjoint buffer-only contract, with no graphics/texture concurrency or asynchronous
reuse. See [Metal ownership](../RHI/README.md#metal-gpu-driven-compute-and-worker-ownership-v2-gfx-1-slice).

## Recording cancellation and nested coordinators

`ExecutionOptions::cancellation` is an optional owning copy of a Core cancellation token; its
empty default preserves existing calls, including three-field aggregate initialization. Another
thread may signal the corresponding source while execution is active, but graph/device/pool
lifecycle and the options object remain caller-serialized. `RenderGraphCancelled` is a typed
`std::exception` with the diagnostic `render graph recording cancelled`.

After normal graph/options validation, pre-cancelled execution resets execution counters and
throws before any RHI resource/list allocation, callback, submission or GPU wait. During execution,
the coordinator observes cancellation before each recording cohort and again after recording,
before submitting an unsubmitted serial pass or parallel batch. Cancellation does not skip queued
recording jobs: every started job is joined and its resources remain alive. Callbacks may also
cooperate through captured tokens. Callback errors precede scheduling errors, which precede a
coordinator-observed cancellation; CPU effects and previously submitted passes cannot roll back.

Once submission of a cohort begins, a later cancellation does not interrupt it: all accepted
submissions wait for completion and the batch retires normally. The next cohort stops before
allocation/recording. Submission/wait failures still follow the existing drain-before-retirement
error path. Cancellation arriving after acceptance of the final cohort returns normal completion;
all passes have finished. This is cooperative boundary observation, not preemption or an atomic
transaction with a concurrent `Cancel` call. `completed_pass_count` preserves completed prior work;
recording counters may include a cancelled batch that never submitted.

Failed/cancelled unsubmitted compute and admitted color-target batches preserve output and may
retry with fresh or empty cancellation state after every worker joins. Texture snapshots remain
private on all built-in backends. Cancellation never resets a source. External-owner execution
retains its own cancellation/acquired-frame policy.

Tests saturate both pool workers with nested graph coordinators and require serial completion,
while a foreign-pool coordinator still records concurrently. They cover pre-cancellation, serial
transient cleanup, simultaneous callbacks, failure precedence, deferred completion after accepted
batches and late cancellation after the final batch. The native Vulkan graph additionally requires
cancelled recording to preserve every output sentinel with no submission/readback, followed by a
successful reuse. Full graphics recording, asynchronous reuse, shared-texture reads and matched
performance remain separate gates. All C++ modules must rebuild for the appended options/token and
new exported query/exception; the stable C ABI is unchanged.


## Native color-target graph acceptance

All three native backends and Validation use list-private texture state, committed only when
submission is accepted. Aborted graph callbacks can reuse imported color targets with fresh lists;
stale texture generations/revisions reject before queueing. Each graph batch joins all workers
before ordered synchronous submission. Graphics admission is independent of compute admission,
and default Execute remains serial. Full parallel feature admission remains disabled/unimplemented.

The shared native texture oracle adds eight disjoint graphics passes, two four-worker cohorts and
exact pixel comparison with serial output, plus failed/cancelled-batch output preservation and
successful replay. Restore passes remain serial. The portable model checks independent capability admission,
texture alias/dependency/shared-buffer-read conflicts, unmarked/sampling/copy fallback, ordered
submission, deferred completion before retirement and transactional upload/submit/handle rejection.
See the [RHI contract](../RHI/README.md#independent-color-target-recording-and-transactional-textures).


## Shared immutable input acceptance

The additional limited `shared_buffer_read_recording` flag lets admitted Compute/color-target
cohorts share declared read-only buffers. Complete declarations, immutable resource lifetime,
per-queue/access dependencies, default serial behavior, cancellation and coordinator-owned ordered
submission/retirement remain required. UAV preparation counts as a potential write in declarations,
even when the shader would only read; such buffers stay disjoint. Shared writes and textures remain excluded.

The common native compute test compares 16 workloads sharing one input in initial parallel, serial
and failed/cancelled replay modes (64 CPU comparisons); output buffers remain distinct and every
aborted output sentinel survives. The native graphics test uses two separate color targets with
one shared indirect argument buffer, forces overlapping workers and compares both images exactly
with direct serial references. Production execution performs no readback. D3D12 preserves buffer
revisions for compatible readers and advances them on potential GPU writes, CPU uploads or nonempty
readback, so shared readers coexist while stale mutation snapshots still reject. See the
[RHI contract](../RHI/README.md#immutable-shared-buffer-reads). Full parallel/performance acceptance
and asynchronous allocator reuse remain open.

## Completed GPU intervals for recording measurements

RHI devices can explicitly request native submission GPU timing through `CreateDeviceWithOptions`;
normal `CreateDevice` stays off. Each observed interval is an owning diagnostic tied to a completed
user-list token, with unavailable represented separately from zero. Per-list query resources follow
the same worker ownership, ordered coordinator submission and completion retirement as descriptors.
Cancelled/unsubmitted cohorts preserve the previous observation and every pixel sentinel; successful
replay can collect new intervals. Private data readback/idle operations leave that observation intact.

The CPU-completion bound guards native counter validity and never supplies GPU duration. This is
measurement plumbing; it does not establish reduced CPU recording overhead, GPU utilization, physical
hardware provenance or matched benchmark acceptance. See the
[RHI timing contract](../RHI/README.md#opt-in-completed-submission-gpu-timing). Full parallel feature
admission, asynchronous reuse and physical performance gates remain open.

## Matched native command-recording measurement

The native descriptor/compute oracle also accepts `--benchmark [samples dispatches-per-list]`
(default 21 samples/mode, 40 dispatches/list; bounded to 101 and 64 respectively). It emits two
JSON lines with schema `nexora.recording-benchmark.v1`. Four fresh command lists record identical,
nonempty GPUDriven workloads in serial and on a persistent four-worker JobSystem. Two complete
warmup pairs precede measured pairs, and the first mode alternates each pair. Inputs, pipelines and
buffers are created before measurement; list/allocator/query setup is measured separately, then
recording includes job scheduling and joining. CPU setup+recording median/p95 excludes native list
finalization, ordered coordinator Submit/Wait and query retrieval, which have their own CPU
submit/wait median/p95. This is a direct-list recording benchmark, not RenderGraph execution timing.

Each mode submits the same four lists in the same order and waits before retirement. Optional
GPU duration sums the four completed native intervals only when all are available; it excludes
inter-submit gaps and cannot represent frame latency. Missing intervals report null, with an
explicit native source and sample count. Timing is enabled in both modes, so instrumentation costs
are matched. There is no CPU substitute or required speedup. Output sentinels are reset before each
sample; every output is checked against the same CPU oracle after timing. Readback, resource writes,
list destruction and process-memory sampling are outside timing windows.

Reports include compiler family/version, opaque vendor/device/driver IDs and the backend's software
flag. The flag alone cannot establish physical hardware, so physical verification/acceptance stay
false. Linux reports RSS after warmup and the maximum sampled process RSS (including verification);
other hosts report null. This is neither native GPU memory nor a continuous process high-water mark.
Performance/memory target budgets remain null until defined and calibrated on target hardware.
`renderer.{vulkan,d3d12,metal}_recording_benchmark` runs a small real workload and validates the JSON,
metric availability/order, matched identity and bounded arguments. Native execution and software
correctness evidence do not close the full parallel/performance gate.

On Vulkan, `--benchmark-reuse [samples dispatches-per-list]` runs the same paired workload with
bounded completed command-pool reuse requested. JSON includes explicit reuse enablement, per-mode
measured native command/descriptor pool creation/cache-acquisition counts and final idle cache
snapshots. Descriptor pool storage resets after list retirement; each dispatch still owns a fresh
immutable set. The
additional native CTest `renderer.vulkan_recording_benchmark_reuse` verifies all measured lists use
warm retired allocations; ordinary benchmark mode verifies fresh allocations and no cache use.
Both modes retain their own warmups, output sentinels, timing and CPU oracle. This option changes
allocation policy only; it does not enable the full parallel feature or asynchronous submission.

## Optional D3D12 enhanced synchronization

The same RenderGraph access/dependency plans can execute on an explicitly enabled D3D12 Enhanced
Barriers device without changing scheduling or revision rules. RHI projects admitted whole-buffer
and whole-texture usage into independent sync/access/layout fields, including UAV/WAW and private
texture readback. Default devices retain legacy barriers; unavailable requests report fallback.
Cancelled cohorts still submit nothing and preserve output/state. This adds neither aliasing nor
multi-native-queue handoffs. See the [RHI contract](../RHI/README.md#opt-in-d3d12-enhanced-barriers).


## Bounded tessellated graphics pipelines

PipelineCache keys include primitive topology, patch control-point count and bootstrap graphics
shader variant in addition to shader/layout/generation/format/requirements. Native patches and
baked fallback cannot reuse an ordinary triangle PSO accidentally. Patch topology implies native
tessellation admission before allocation; callers select a fallback explicitly when unavailable.
The first native route is a three-control-point, factor4 curved patch, with a16-triangle baked
fallback on all backends. Vulkan/D3D12 execute native hull/domain equivalents; Metal's path stays
fallback. This is a bounded bootstrap route, not general material/program assembly.

Pre-create immutable PSOs/resources before graph callbacks. The existing limited disjoint graphics
cohort contract admits these color-target draws with the same ordered completion and cancellation
boundaries. The native oracle forces two workers, verifies RGBA/BGRA parity against the baked mesh,
preserves sentinels on failure/cancellation, then replays successfully. Draw bounds and unsupported
indexed/indirect paths reject before native recording. Full parallel/asynchronous performance and
the combined Tessellation/Enhanced Barriers gate remain open. See the
[RHI contract](../RHI/README.md#bounded-native-tessellation-and-baked-mesh-fallback).

## Bounded VRS selection and pipeline identity

Graphics pipelines can explicitly request full or2x2 shading. Effective admission adds VRS tier1
for coarse shading; cache identity includes the rate. A separate backend implementation-tier ceiling
prevents a tier1 path from admitting tier2 hardware features. RenderGraph scheduling, list ownership
and cancellation remain unchanged; rate state is private to each recording list, and a full-rate
bind restores1x1 after coarse work. Select the full-rate descriptor explicitly on unavailable
backends. See the [RHI contract](../RHI/README.md#bounded-per-draw-variable-rate-shading).

## Bounded GPU-selected mesh geometry

The optional D3D12 MeshTriangle path consumes the GPUDriven producer's visible count through a
private read-only statistics binding and executes one mesh group, without a CPU count decision.
Mesh stage/profile/variant requirements are part of admission and cache identity; unsupported
backends explicitly retain GPU-generated indirect raster draws. RenderGraph declares the shared
statistics input as ShaderRead (the fallback declares its argument buffer as IndirectRead), so
compatible reads may record together while producer dependencies and revision checks remain intact.
Cancelled cohorts submit nothing. See the [RHI contract](../RHI/README.md#bounded-d3d12-mesh-shader-and-gpu-produced-visibility).
General meshlet scheduling, amplification and full Ultimate/performance acceptance remain open.

## Optional bounded Work Graph recording

The optional D3D12 visible-expansion program turns one count/first-slot CPU record into up to eight
GPU child-node writes. Renderer declares its output as StorageReadWrite and provides dependencies;
independent lists may record disjoint outputs using immutable pre-created programs and private
backing contexts. Cancellation submits no cohort and preserves every sentinel; stale resources or
program generations reject before native execution. An explicit GPUDriven compute/indirect fallback
produces the same known-visible records. Capability matching does not prove artifact availability.
See the [RHI contract](../RHI/README.md#optional-bounded-native-work-graphs). This does not complete
general graph authoring, native queue separation or performance/preview acceptance.

Bounded ray tests use explicit immutable RHI scene/program handles and record rays in existing
buffer-only compute graph callbacks. The graph still owns input/output access ordering and cohort
submission. Scenes/programs are pre-created, immutable and retained by each native list; they are
not mutable graph resources. Two workers use disjoint buffer outputs, and failure/cancellation
abandons both lists before submit. A distinct `ComputeShaderVariant::RayReference` participates in
the complete pipeline-cache key and selects actual lower-SM traversal on supported native backends.
This bounded hit oracle is not integration of ray shadows/reflections/AO, general scene AS updates,
DXR1.2/SER or a physical performance acceptance result.

Explicit bounded instance sets may share one BLAS across up to eight transforms within the eight
total-triangle budget. Pre-create/mutate these scenes outside graph callbacks. Each list retains
the scene's AS/instance data and instance-major flattened primitive mapping; graph buffer access,
worker ownership and cancellation/submission rules are unchanged. The compute reference consumes
explicitly baked instance-major world triangles with matching hit IDs. This adds no automatic
fallback selection, heterogeneous scene assembly or material/lighting integration. See the
[RHI instance-set contract](../RHI/README.md#bounded-shared-blas-instance-sets).

Bounded sampler-feedback callbacks consume immutable request buffers and write disjoint decoded-map
buffers in existing compute graph cohorts. The RHI program owns a fixed metadata-only paired texture;
per-list contexts own opaque/decoded textures and descriptor heaps. Graph ordering/submission and
failure containment remain unchanged. A separate compute-reference cache variant preserves an
explicit fallback. This bootstrap does not attach feedback to application materials or implement
virtual-texture streaming/residency/performance acceptance.

Modern compute preview admission uses distinct `LongVector69`, `LinearAlgebra610` and
`GroupShared610` pipeline variants. Complete cache identity includes explicit experimental Shader
Model permission, so an accepted baseline or preview PSO cannot authorize another policy. Native
DXIL recipes require the actual device profile and separate RHI/runtime/artifact checks; the
Renderer does not emulate modern GPU instructions. Full modern shader/cooking and fallback
integration remain open in the paired V2 graphics plan.

The fixed modern compute recipes additionally have explicit lower-profile RHI equivalents preserving
the128-byte input/32-byte output ABI. Renderer callers may use existing capability checks and explicit
preferred/fallback selection before requesting the chosen descriptor from PipelineCache; the device
never silently changes a selected variant. Reference and modern variants have distinct cache identity.
Existing RenderGraph worker failure/cancellation rules retain output/counters until cohort acceptance.
