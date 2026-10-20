# Nexora RHI contracts (V1-M2)

## V2-GFX capability admission foundation

`Device::Capabilities()` returns an owning construction-time snapshot without recording or
submitting work. Each feature independently records device/driver support (`Unknown`, `Unsupported`,
`Supported`), its portable numeric tier, whether it is enabled on this device and whether the
backend implements its execution path. Missing SDK queries, failed optional queries and unknown
native tier values remain `Unknown`; they must not be diagnosed as unsupported hardware.
`validation_only` distinguishes the synthetic validation model from native API execution;
`software_device` identifies the validation model and native software adapters such as WARP/CPU
Vulkan. Adapter vendor/device and the opaque backend driver identifier are diagnostic metadata.

DX12 queries available RT, VRS, mesh, sampler-feedback, Enhanced Barrier and Work Graph tiers with
SDK guards, plus the highest device Shader Model by retrying known query values through 6.10.
Feedback's native 90/100 values map to portable 0.9/1.0; they are not tiers 9.0/10.0. Vulkan reports
the selected queue's compute support and enabled indirect support. The bounded tessellation slice
now enables the queried Vulkan feature when supported and implements D3D12 hull/domain patches.
Metal retains explicit baked-mesh fallback; its native hull/domain path is not implemented.
Other unqueried advanced features stay unknown. Full parallel recording and SER remain disabled;
SER is not inferred from RT tier or Shader Model.

`PipelineDescriptor::graphics_requirements` defaults to no additional requirements, preserving
existing aggregate callers. Nonzero `minimum_tiers` require the corresponding support, sufficient
tier, implementation and enablement. `{0, 0}` is unused; `{1, 0}` represents a tierless requirement.
The DXIL-only minimum Shader Model is compared numerically, so 6.10 is newer than 6.9; Vulkan,
Metal and validation do not advertise DXIL support. This device limit does not prove compiler,
preview opt-in, artifact presence, format or reflection compatibility. Existing artifact admission
remains independently required; no shader profile or toolchain baseline is raised here.

`CheckGraphicsRequirements` returns the first failing feature/reason deterministically.
`SelectGraphicsPath` checks both caller-supplied requirement sets and reports preferred, fallback
or unavailable; it never substitutes a shader or executes a path. All built-in devices enforce
requirements before PSO allocation. Rejection throws `std::invalid_argument` and performs no
submission. PSO cache identity includes the complete requirements, so a cached baseline pipeline
cannot authorize an unsupported advanced variant. Capability queries are safe for concurrent reads
after device construction; devices and the caller's snapshot copies own no shared mutable query
state. This extends the C++ RHI interface; all modules must be rebuilt together. The stable C ABI
is unchanged.

The [V2 graphics plan](../../Roadmap/en/V2_Advanced_Graphics_Capability_Plan.md) retains the native
feature gates, parallel allocator/descriptor lifetime work and performance acceptance separately.

## Vulkan compute descriptor ownership (V2-GFX-1 slice)

Each live Vulkan command list exclusively owns its compute descriptor pools. `BindStorageBuffer` stores only
list-local handles; `Dispatch` revalidates all four canonical slots, allocates a fresh set and
copies the complete binding snapshot before recording. Sets keep immutable contents for the lifetime of their recording list, so later rebinding
(including binding without dispatch) cannot redirect earlier work.
Missing or stale bindings throw `std::logic_error` before descriptor allocation or dispatch.
Pools grow in blocks of 32 sets; recorded dispatches retain their sets until list destruction.
After retirement the explicit reuse option may reset/cache native pool storage; every subsequent
dispatch still allocates a fresh set.

A command list has one recording owner at a time. Independent compute lists may record concurrently
against pre-created immutable pipelines and disjoint buffers. The device and referenced resources
must outlive all lists; callers serialize resource creation, upload, mutation and destruction against
recording/submission. Submission remains caller-serialized and synchronous in this backend; pool
and command allocator retirement (destruction or explicitly requested reset/cache) happens after
`Submit` has waited for its native completion fence,
or when a never-submitted list is abandoned. Never destroy a list while `Submit` is using it.
This is a prerequisite for broader worker scheduling, not an asynchronous retirement implementation.

`renderer.vulkan_descriptor_isolation` compares 320 unique dispatch results against the CPU
reference in serial and four-worker recording modes, rebinds after every dispatch, crosses pool
boundaries and submits only after every worker joins. It also rejects missing/stale bindings and
checks no worker submission/readback. With no Vulkan device/artifacts it reports an explicit skip;
Slang-enabled native builds require execution. Bounded graph compute recording is described below;
general graph parallel execution, DX12/Metal worker lifetimes, GPU validation-layer evidence and
matched physical-GPU performance remain open.
`ParallelCommandRecording` remains disabled/unimplemented in the capability snapshot.

## Vulkan texture snapshots and framebuffer retirement (V2-GFX-1 slice)

Each command list captures a validated texture handle/generation, committed revision/state and
immutable native image/view/format/extent metadata on first use. Recording transitions updates
only that list's private state; rendering/pipeline binding validates against that state. Even a
same-state transition validates/captures its texture before returning. Undefined destinations and
invalid queues are rejected before native barrier emission. Rendering extents cannot exceed the
attachment. Recording after native list closure is rejected, including after submission errors.

`Submit` validates every captured texture revision before closing or queueing the native buffer.
Only an accepted `vkQueueSubmit` commits final logical states and increments revisions. Another
accepted use or texture readback makes older recorded lists stale, even if the final state is the
same; re-record them. Readback restores the committed layout but still invalidates old snapshots.
Destroy/recreate is detected through handle generations. This conservative rule does not admit
pre-recorded lists sharing a texture, including reads. Resource creation/mutation/destruction,
readback and submission stay caller-serialized against recording; device/resources outlive all
lists and their completion. No texture-upload implementation is added by this slice.

Every rendering scope owns a distinct framebuffer retained by its command list, including multiple
scopes in one list. Failed vector growth destroys only the new unused framebuffer. After native
fence completion, submission destroys every retained framebuffer; an abandoned list frees its
command buffer/pool before destroying its unsubmitted framebuffers. Failed submission still needs
draining before retirement. This replaces the previous single field that lost earlier scopes'
framebuffers. Graphics descriptors/uniforms and render passes remain immutable device-owned objects.

Independent graphics lists may record concurrently against disjoint textures and immutable
pre-created pipelines under those ownership rules. `renderer.vulkan_texture_recording` forces
four-way overlap, records six scopes per list over 24 mixed-format/extent targets, submits in reverse
order after joining, and requires 24 exact serial/parallel pixel matches with nonempty triangle
references. It also checks abandoned state/output reuse, stale submission/readback/handle rejection,
bounded render areas and failed/replayed serial graph reuse. The existing Linux Editor native
validation step includes this and descriptor isolation with synchronization/object validation and
fails on VUID/SYNC/error output. Missing native artifacts/device explicitly skips optional builds;
Slang native builds require execution and supply both required SPIR-V artifacts through CTest.

This is native ownership foundation, not general RenderGraph graphics admission. The graph's
limited parallel scheduler still admits only disjoint buffer-only Compute passes. DX12/Metal
texture recording, general graphics/shared-read scheduling, asynchronous queue transfers, reusable
retirement and matched performance remain separate gates. The full parallel feature, shader/SDK
baselines and stable C ABI remain unchanged.

## Completed

- `Shaders/Triangle.slang` is the canonical graphics and compute acceptance shader.
- With `NEXORA_ENABLE_SLANG=ON`, the CMake pipeline compiles the source to SPIR-V and Metal
  source everywhere, plus DXIL on Windows (see "DXIL scope" below), emits Slang reflection JSON
  for each target, and normalizes the results into one backend-neutral metadata document.
- The normalized metadata is checked against `TrianglePipelineLayout()` and
  `ComputeLayoutHash()` by the `build.shader_crosscompile` test. The checked-in
  `Shaders/Triangle.reflection.json` remains the reviewable canonical fixture.
- `ShaderModuleArtifact` is the compiler-independent boundary for compiled modules: DX12 consumes
  DXIL, Vulkan consumes SPIR-V, and Metal consumes Slang-generated MSL source.
  `IsArtifactCompatible` checks target format, payload presence, schema, and the existing canonical
  reflection/layout hash before an upper layer accepts an artifact for submission.
- `ShaderResourceBindingMetadata` and `ValidateShaderResourceBindings` provide the backend-neutral
  argument-buffer/device admission gate. They reject duplicate bindings, invalid stage masks,
  read/write misuse, and argument-buffer groups that exceed physical device capability; native
  backend code remains responsible for creating final descriptor/argument-buffer objects.
- `Shaders/Nexora/Common.slang` is a reusable Slang module for common color transforms, luminance,
  ACES-fitted tone mapping, safe normalization, and a single-light Cook–Torrance/GGX BRDF.
  `CommonSmoke.slang` imports it and is part of the optional cross-compile gate; it does not change
  the triangle or GPU-driven acceptance shader outputs.
- Public RHI headers expose only backend-neutral descriptors, enums, handles, and interfaces.
  Native DX12, Vulkan, and Metal types are not part of the public contract.
- `NEXORA_ENABLE_SLANG` is disabled by default so environments without `slangc` retain the
  existing contract-only build path.
- V1-M3 adds private native implementations for the platform backends: DX12 on Windows, Vulkan
  on Windows/Linux, and Metal on Apple. Each backend owns device creation, texture allocation,
  resource-state transitions, triangle PSO creation, command recording, submission fencing, and
  the offscreen `Present` contract without leaking native types into public headers.
- The native Vulkan RHI requires Vulkan 1.1 and `shaderDrawParameters` for canonical Slang
  SPIR-V 1.3 artifacts, plus `multiDrawIndirect` for GPU-driven submission. Device selection rejects unsupported candidates before shader
  creation and enables the required features on the selected graphics device.
- `renderer.contracts` uses the native backend on the host platform when Slang is enabled. Set
  `NEXORA_SLANG_SPIRV_PATH` or `NEXORA_SLANG_METAL_PATH` manually when running the executable
  outside CTest; CMake sets these paths automatically for the CI test.
- `Tools/Build/NormalizeShaderReflection.py`'s constant-buffer size lookup has now been run
  against a real `slangc` (2026.18) SPIR-V and Metal reflection JSON, not just written against
  the documented schema: the initial key-name guesses (`uniformSize`/`byteSize`/`size` directly
  on the constant-buffer node) did not match, exactly as the module's original docstring warned
  they might, and were replaced with the byte size real slangc actually reports
  (`elementType.sizes[*].value`, with the flat keys and the container's own `sizes` kept as
  fallbacks). `build.shader_crosscompile` passing is now real end-to-end validation, not just a
  script that has never seen real slangc output.

## DXIL scope

The portable Slang release does not ship `dxcompiler` for Linux or macOS, so a stock portable installation cannot run `slangc -target dxil`
without a separately supplied downstream compiler (`error[E00100]: failed to load downstream compiler 'dxc'`) -- this was confirmed
against slangc 2026.18, not assumed. The default build therefore compiles and validates DXIL only on Windows,
where `dxcompiler` is reliably available; `NEXORA_SLANG_DXIL_OUTPUT` and the crosscompile test's
`DXIL_ARTIFACT` are simply absent on other hosts rather than pointing at a path that was never
produced, and the canonical reflection's `backends` list only ever names backends actually
validated on that host (never a hardcoded `["dxil", "spirv", "msl"]` regardless of what ran).

## V1-M3 scope

- The milestone gate is the same basic offscreen workload on DX12, Vulkan, and Metal. `Present`
  validates the final resource state; it is not a window-system swapchain present.
- The RHI intentionally keeps the public API small: the native implementations currently cover
  the texture/pipeline/graphics-command subset exercised by the V1-M3 gate. Compute/copy queues,
  multi-queue timelines, descriptor indexing, persistent PSO disk caches, transient heap aliasing,
  and window-system swapchains are later expansion points rather than silently emulated here.
- The DXIL leg of `build.shader_crosscompile` is exercised by the Windows CI matrix; local builds
  can keep `NEXORA_ENABLE_SLANG=OFF` when `slangc` is not installed.

## V2-M3 command contract

Command lists expose backend-neutral compute dispatch and indirect-draw recording. Diagnostics count
compute dispatches, indirect draws, and readbacks independently, allowing the normal GPU-driven path
to assert that it never maps GPU output. Resource barriers include source and destination queues so
RenderGraph, rather than a backend or pass callback, owns queue transfers. The validation backend
executes the complete portable contract. Vulkan now also records a real `vkCmdDrawIndirect` from
host-visible indirect command storage and reports it independently in device diagnostics; the
offscreen gate exercises that path on a Vulkan-capable Linux host. Vulkan also creates compute pipelines, exposes four backend-neutral storage-buffer slots, and provides the complete deterministic culling, LOD, Hi-Z, compaction, classification, and command-generation kernel. The Linux native gate requires an exact CPU/GPU comparison before RenderGraph binds its generated Vulkan-compatible indirect buffer for drawing; compilation, dispatch, and counters alone do not satisfy the gate. Readback is available only through the explicitly test-only
`ReadBufferForTesting` API and is counted independently; production recording remains readback-free. `ReadTextureForTesting` is the equivalent test-only texture seam: it waits for device idle, copies the whole 4-byte-per-pixel texture into tightly packed host memory, restores the texture's recorded layout, and counts as a readback. Vulkan, D3D12 and Metal implement buffer readback for their native acceptance gates; custom backends throw `std::logic_error` until their own native evidence exists, and depth textures are rejected.
D3D12 now records compute dispatch and canonical-stride `ExecuteIndirect`, accounts for both in
diagnostics, and obtains its compute PSO bytecode from the Windows DXIL artifact. Its buffer
path keeps upload staging separate from default-heap UAV-capable GPU storage, binds the four
fixed shader slots (`t0`, `u1`, `u2`, `u3`), records the required copy/state transitions, and
provides test-only buffer readback. On 2026-10-02, the local Windows NVIDIA GTX 960 host passed
`renderer.v2_gpu_driven` with an SDK `dxc`-generated `sm_6_0` artifact and an exact
`CompareGPUDrivenResults()` match. Metal now loads the same full kernel from a separate Slang-generated artifact and runs it through
the same graph/CPU oracle on macOS. Native queue/timeline separation and full target-host parity
evidence remain open before V2-M3 can be accepted.

`Nexora/RHI/IndirectCommand.h` fixes the indirect-buffer ABI once for every backend. Its first four
32-bit words are the common non-indexed draw arguments used by Vulkan, D3D12, and Metal; five
classification words follow, for a 36-byte stride. `IndirectCommandABI.inc` is the shared C++/Slang
source of word offsets. Native backends consume the four-word prefix at that stride and must not
declare a backend-private command record. This is an append-only binary contract: changing an
existing word or its offset requires a new ABI version rather than an in-place reinterpretation.

## Editor draw-list command contract

The public command-list contract includes vertex/index-buffer binding, indexed draws with base
index and vertex offsets, per-command scissor rectangles, and texture-slot binding. Devices also
expose transient buffer creation and bounded uploads so UI hosts do not need backend-native buffer
types. The validation backend checks resource existence, upload bounds, sampled-texture state, and
all bindings required by an indexed draw. Phase B adds explicit, validated constant-buffer binding;
scene submission binds vertex, index, constant, sampled-texture, and scissor state. Backends that
have not implemented this expanded subset
fail explicitly through the default interface methods; they must not silently translate indexed UI
geometry into non-indexed draws.

Texture uploads use tightly packed RGBA8 bytes plus an explicit row pitch. `Submit` returns a
monotonic completion value; resources referenced by that submission remain in use until
`CompletedSubmissionValue` reaches the value or `WaitForSubmission` returns. This lets bounded UI
upload rings and replaced atlas generations retire without assuming that queue submission is
synchronous. Current native devices complete `Submit` before returning, while the contract and
validation backend preserve the completion-value boundary for asynchronous implementations.

## External HDR format metadata

The appended `TextureFormat::Rgba16Float` describes Presentation-owned HDR resources imported into
RenderGraph's external executor. It does not extend the independent native RHI triangle pipeline:
its allocation/format routing rejects RGBA16F explicitly rather than substituting RGBA8. The external
owner allocates its float target and records native barriers; graph execution never allocates it.

## Whole-buffer access dependencies (V2-GFX-1 prerequisite)

`CommandList::TransitionBuffer` accepts backend-neutral `BufferBarrier` scopes: `ShaderRead`,
`StorageReadWrite` and `IndirectRead`. `Undefined` is allowed only as the conservative prior scope,
never a destination or a discard operation. Dependencies cover the entire buffer. Invalid handles,
enums, rendering-scope recording and post-submission recording fail before native barrier emission.
Access scopes describe GPU dependencies; unlike texture layouts, they are not a second global
resource-state machine. Existing device diagnostics count texture and buffer barriers together.
The default method throws for custom backends that do not implement the extension.

Vulkan emits a whole-buffer memory barrier with shader/indirect stage and access masks, including
same-access storage WAW dependencies. Unknown prior access uses all-command memory read/write
scope. Logical queues share the existing native family, so queue-family indices remain ignored.
D3D12 uses its actual tracked GPU buffer state and existing upload preparation, maps scopes to
SRV/UAV/indirect states, and emits a UAV barrier for storage destinations even without a state
change. Metal uses tracked shared-buffer hazards across encoder/command-buffer boundaries; a
buffer dependency ends an active compute encoder and requires rebinding pipeline and buffers for
subsequent dispatch. No native asynchronous multi-queue synchronization is implied.

The caller owns buffers through submission completion and serializes resource uploads, mutation,
destruction and submission against recording. D3D12 buffer recording now uses the private snapshots
described below; texture recording now uses the transactional snapshots described at the end. All C++ modules must rebuild for the extended virtual interface;
the stable C ABI and shader artifact/profile requirements are unchanged.

## Limited independent compute recording admission

`GraphicsDeviceCapabilities::independent_compute_recording` defaults false for unmodified/custom
backends. Validation advertises it as a portable model, D3D12 and Metal admit the isolated buffer
paths below, and Vulkan advertises it when its selected native queue supports compute. It admits exclusively owned buffer-only compute lists recorded
against disjoint, caller-retained buffers and immutable pre-created pipelines. Vulkan's list-local
command pools and immutable per-dispatch descriptor sets provide that isolation; handle/pipeline
lookups remain protected by the device mutex. Creation, upload, mutation, destruction and submission
stay serialized by the caller. This compute flag does not admit graphics, textures or shared-buffer reads; the separate limited
color-target flag below admits graphics.

RenderGraph consumes this limited flag for its explicit opt-in bounded scheduler. Every worker
finishes before coordinator submission, and the graph waits for each returned completion value
before list retirement. Native logical queues still alias one queue. Allocator/descriptor reuse
across asynchronous submissions and separate queue-family transfers remain unimplemented.
The broad `GraphicsFeature::ParallelCommandRecording` remains disabled/unimplemented until its
remaining backend, lifetime, toolchain and performance gates pass. Existing shader feature
admission cannot use the limited flag to bypass that feature gate.

## D3D12 buffer recording snapshots (V2-GFX-1 slice)

Each D3D12 command list owns its allocator, native list, binding addresses and buffer preparation
snapshots. First use captures the validated generation, device revision, committed native state and
pending-upload flag under the device mutex. Copies and state transitions update only that list's
snapshot. Abandoning a list does not consume staging data or modify committed GPU state. Resource
creation, upload, mutation, destruction and submission remain caller-serialized against recording;
resources and immutable pipelines must outlive all lists and their submission completion.

Before closing or executing a list, `Submit` checks every referenced buffer still exists at the
captured revision. CPU uploads, accepted potential GPU writes and nonempty readbacks invalidate
older snapshots, including readers. Compatible immutable SRV/indirect submissions now preserve
that data revision, admitting the shared reads described below. Stale lists throw `std::logic_error`
before execution and require fresh recording; no stale command repair is attempted. `Dispatch` also requires all
four canonical bindings and revalidates their handles before recording.

After `ExecuteCommandLists` accepts work, submission commits consumed uploads and advances the
revisions of buffers prepared for potential UAV writes, even if a later signal/wait fails. Buffers decay to `COMMON` at the
execution boundary on the single DIRECT queue; subsequent lists transition from that committed
entry state. Test-only readback likewise commits after native execution and leaves buffers in
`COMMON`. Never replay an already accepted list. Failure after queue acceptance still requires
draining before list/resource destruction; device-loss recovery remains separate.

The bounded graph scheduler admits disjoint buffer-only Compute passes through
`independent_compute_recording`. It joins every worker before ordered synchronous submission and
waits completion before allocator retirement. `renderer.d3d12_recording_isolation` uses the same
native workload and CPU oracle as the Vulkan test: 320 isolated dispatch comparisons, 48 graph
comparisons, forced four-way overlap, failed-batch sentinels and successful buffer reuse. Additional
D3D12 cases verify abandoned initial uploads/state changes, stale upload/submission/readback/handle
rejection and six CPU comparisons. Without native DXIL/device support it explicitly skips; Slang
Windows builds require execution. This does not enable texture/graphics concurrency, asynchronous
allocator reuse, shared reads, GPU validation-layer acceptance or the full parallel feature gate.

## Metal GPU-driven compute and worker ownership (V2-GFX-1 slice)

With Slang enabled, `GPUDriven.slang` produces a separate `GPUDriven.metal` artifact on every host.
`NEXORA_SLANG_COMPUTE_METAL_PATH` selects that artifact at device construction; the native backend
compiles its `computeMain` and retains the library/function for the device lifetime. A requested
missing/empty/non-UTF-8/invalid artifact or missing entry point throws; it never substitutes the
embedded no-op. Without that opt-in, the previous triangle/compute-smoke baseline is preserved and
cannot satisfy the full GPU-driven acceptance gate. No shader or SDK baseline is raised.

Each list owns its command buffer, compute encoder and four-slot buffer-handle snapshot. Bindings
may precede `BindPipeline`; setting a compute pipeline creates an encoder as needed and replays
all cached bindings. A whole-buffer barrier ends the active compute encoder and invalidates the
pipeline binding; callers rebind the pipeline before dispatch. The new encoder replays retained
binding identities. Canonical GPU-driven dispatch requires all four slots and revalidates handles.
Metal's tracked shared-buffer hazards order encoder boundaries and the existing single queue.

Compute recording entry points, command-buffer creation, PSO creation and idle waits use local
`@autoreleasepool` scopes; ARC strong fields retain encoders/command buffers past pool drainage.
Buffer lookups use the device mutex. Lists have one recording owner at a time and independent,
disjoint buffer-only compute lists may record concurrently. Callers pre-create immutable pipelines
and resources and serialize creation, upload, mutation, destruction, submission and device/pool
lifecycle against recording. Referenced resources/device outlive lists and completion. Submission
stays coordinator-owned and synchronous; allocator/encoder reuse across asynchronous submissions
and general texture/graphics concurrency are not admitted.

`ReadBufferForTesting` waits for the queue, validates the whole range, copies shared storage and
counts one nonempty readback. Empty in-range reads validate but do not increment the counter;
invalid ranges/stale handles throw. This seam is for test acceptance; production compute/indirect
recording remains readback-free. Managed/private buffers are outside this storage contract.

Slang-enabled macOS gates require both full `renderer.v2_gpu_driven` stage/CPU comparison and
`renderer.metal_recording_isolation`: 320 isolated dispatch comparisons, 48 serial/four-worker graph
comparisons, timed four-way overlap, aborted-batch sentinel preservation and successful buffer
reuse, plus encoder WAW-boundary replay and offset/empty/invalid/stale readback cases. Missing
native artifacts/device is an explicit skip in optional builds and a failure when native execution
is required. Linux cross-compilation proves artifact generation, not Metal runtime correctness.
The broad parallel feature, GPU validation-layer acceptance, full V2-M3 parity and measured physical
GPU performance remain open.

## Independent color-target recording and transactional textures

`independent_graphics_recording` defaults false for custom devices. Validation and the native
Vulkan, D3D12 and Metal backends admit disjoint color-target lists with immutable, pre-created
pipelines. Each list has one recording owner; the caller retains device/resources through list
retirement and completion, and serializes creation, upload, mutation, destruction, submission,
readback and lifecycle against recording. Shared textures never qualify; compatible buffer reads
also require the separate admission below. This limited flag leaves the broad parallel feature disabled.

Texture first use captures generation, revision, committed state and native metadata under the
device mutex. Transitions and render-target validation use private state; abandoned lists do not
change committed state. `Submit` validates every captured texture before native execution and
commits final state/revision only after queue acceptance. Readback invalidates prior snapshots;
Validation texture uploads do too. Stale lists throw before execution and require fresh recording.
This policy includes same-state and read-only references. Accepted work must complete/drain before
list destruction; caller mutation cannot race snapshots. It does not add asynchronous retirement.

D3D12 snapshots retain native resources; its allocator/list retire before snapshots release. RTV
slots come from a checked 256-entry heap and return on caller-authorized texture destruction;
allocation failure returns the unpublished slot. Never destroy a texture before all lists referring
to it retire, even when no work was submitted. Metal snapshots retain textures with ARC and local
autorelease pools cover graphics recording; encoders/command buffers release before snapshots.
Graphics PSO binding validates the captured target format, and render areas cannot exceed its extent.

D3D12/Metal `ReadTextureForTesting` waits for the queue and copies RGBA8/BGRA8 through a private
staging buffer, packing padded GPU rows into exactly width * height * 4 bytes. D3D12 restores the
committed native state; Metal uses tracked blit hazards. Queue acceptance advances the texture
revision even if a later wait fails. Invalid size/state/stale handles reject; successful copies
count one readback. Production graph execution performs none. This adds no native texture-upload
API, depth/float readback or separate native queues.

All native texture recording targets share one oracle: 24 mixed-format/extent targets, six scopes
per list, forced four-worker overlap, reverse submission and exact nonempty serial/parallel pixels;
abandonment/retry and stale submit/readback/handle rejection; 512 sequential retired allocations;
and eight color-target graph comparisons with failed/cancelled-batch sentinel preservation and replay.
Optional builds explicitly skip absent artifacts/devices; Slang-enabled native CI requires execution.
Linux Vulkan validation is software-device correctness evidence. Windows/Metal runtime results
require their hosted jobs. Physical-GPU performance, shared-texture reads and asynchronous reuse
remain open.


## Immutable shared buffer reads

`shared_buffer_read_recording` defaults false and is independent of compute/graphics admission.
Validation and Vulkan/D3D12/Metal implement compatible same-queue, same-access immutable buffer
reads with separate writable outputs. The graph still requires explicit callback opt-in and complete
resource declarations. Any shared write, access/queue handoff or dependency ends the cohort;
textures remain disjoint. All workers join before ordered, synchronous coordinator submission.
The caller keeps buffers, upload sources, pipelines and device alive and immutable through all
recording/submission/completion, and serializes mutation, readback and lifecycle. This flag adds
no asynchronous reuse, GPU queue concurrency or automatic protection from undeclared callback writes.

D3D12 marks a snapshot potentially writable whenever UAV preparation occurs, even without dispatch
or when shader source happens to read only. Callbacks must declare UAV bindings as writable;
shared-reader admission covers SRV/indirect access, without shader-source inference.
Accepted potential writes advance the buffer data revision; compatible SRV/indirect uses preserve
it. CPU uploads and nonempty readbacks still advance it, so older readers reject before execution.
Upload consumption and COMMON decay still commit on queue acceptance. Multiple initially recorded
readers may copy the same retained staging revision: later copies promote a buffer from COMMON to
COPY_DEST when needed, and each individual ExecuteCommandLists boundary decays buffers to COMMON.
The native backend still submits one list per synchronous Execute call. Changing queue/list
batching or staging ownership must revisit this invariant. Abandoned/cancelled copies never consume
committed upload state. Empty in-range readback validates without invalidating snapshots.

The shared compute oracle runs parallel first over an initially pending shared input, then serial,
plus failed/cancelled sentinel preservation and replay: 64 exact CPU comparisons and timed four-way
overlap. The graphics oracle shares one fresh indirect argument buffer across two overlapping
color-target passes and compares both outputs exactly with serial direct drawing. D3D12 adds reverse
pure-reader submission, upload/readback/GPU-writer invalidation, empty-read compatibility and recovery.
Portable tests keep custom admission closed and reject write/read, read/write, write/write and
access handoffs. Native Windows/Metal execution requires hosted acceptance; Linux Vulkan correctness
and validation do not establish physical-GPU performance or close the broad parallel feature gate.
All C++ modules rebuild for the appended capability; stable C ABI and shader/SDK baselines stay fixed.

## DXIL artifact stage and profile admission

Device Shader Model support and selected bytecode readiness are separate checks. The portable
`InspectDxilProgram` reads DXBC container version/size and bounded part ranges, rejects duplicate
or overlapping executable parts, validates the embedded DXIL program/bitcode range and LLVM magic,
and returns the entry stage and numeric Shader Model. It supports the implemented vertex,
fragment and compute entry stages; libraries and advanced stages remain closed. Unaligned input
is safe. Scratch storage/time are bounded by the supplied part table; allocation failure returns
no metadata. Input storage stays caller-owned for the duration of the synchronous call.

`RequireDxilArtifact` throws `std::invalid_argument` for absent/malformed metadata, a mismatched
stage or an artifact older than the requested profile. The D3D12 constructor applies this to every
explicitly requested Slang entry artifact. PSO creation first checks device requirements, then
checks every selected stage's actual artifact profile before native PSO allocation. Compute and
graphics use separate artifacts: compute's SM6.0 does not limit a graphics SM6.6 pipeline, and a
high device limit cannot authorize a newer requirement against older bytecode. With no DXIL
minimum requirement, the existing absent-artifact FXC triangle fallback remains available.

This is metadata admission, not a DXIL validator, checksum/signature verifier, compiler-provenance
check, preview opt-in or proof of executable feature support. Native PSO creation still validates
its shader/root-signature contract. `CheckGraphicsRequirements`/`SelectGraphicsPath` remain device
checks; callers separately admit the selected artifact. No shader profile, SDK or compiler baseline
is raised, and no SM6.9/6.10 Preview feature becomes implemented. The complete compiler/artifact
readiness matrix and target-host advanced execution gates remain open.

`renderer.dxil_artifact_readiness` runs portable stage/profile, numeric 6.9/6.10, truncation,
malformed/out-of-range/overlapping/duplicate-part and unordered-table checks on synthetic metadata.
Those fixtures are not executable shaders. Slang-enabled Windows native builds additionally inspect
the real generated vertex/fragment SM6.6 and compute SM6.0 artifacts, create baseline PSOs and
reject a newer artifact requirement when the device itself supports it, and reject an explicitly
requested compute artifact that contains a vertex program. The native fence event is created last
so constructor rejection cannot leak its raw handle. These checks submit no GPU work. Linux portable acceptance cannot certify D3D12 execution. The new exported C++ helpers require
module rebuild; the stable C ABI is unchanged.

## Opt-in completed submission GPU timing

`CreateDevice(backend)` keeps timing off. `CreateDeviceWithOptions(backend, {true})` requests
best-effort native timing; `gpu_timing_support` and `gpu_timing_enabled` distinguish API support
from the construction-time opt-in. Custom devices default unknown/disabled; Validation reports
unsupported and never fabricates a GPU observation. Default native lists allocate no timing query
or readback resources, record no timing commands and publish no timing completion stream.

`DeviceDiagnostics::gpu_timing` owns only source, the last observed user-list completion token and
optional milliseconds. Tokens can be sparse (private D3D12 fence operations use the same timeline).
No value means unavailable; an observed zero interval remains distinct from unavailable. Timing
measures the native command-buffer interval, including its resource work, not CPU recording,
display latency, utilization or physical-GPU provenance. Query collection occurs after existing
submission completion; it adds no GPU wait or completion callback. Disabled/private readback/idle
operations do not replace the last user-list observation. Resource readback counters continue to
count the explicit data-testing seams, separately from opt-in completed timestamp collection.

Each Vulkan list optionally owns one two-query pool. It resets/writes TOP/BOTTOM timestamps,
checks queue counter width and period, and reads value/availability pairs after the existing fence
without WAIT. D3D12 lists own query pairs and readback buffers, use the observed queue frequency,
and collect after the existing fence; device-removed fence sentinels reject completion. Query
resources retire after native list/pool cleanup or completed submission. Metal uses completed
command-buffer GPUStartTime/GPUEndTime with runtime availability and valid nonzero origin checks.
No additional Metal query allocation or callback is introduced.

Optional allocation/query failure, invalid periods/intervals and ambiguous wrap windows remain
unavailable while rendering proceeds. The counter-width/CPU-completion upper-bound conversion is
shared with Presentation through the RHI detail helper; CPU duration never substitutes for native
time. Abandoned, stale-rejected or cancelled unsubmitted lists publish nothing. An accepted
submission failure retains the existing drain-before-retirement/no-replay contract.

`renderer.submission_gpu_timing` verifies default-off and Validation-unavailable behavior, reverse
native submission, owning observations after query retirement, exact pixels and timed-cohort
cancellation/replay. The native isolation executables also run with `--gpu-timing`, preserving all
compute CPU/sentinel/revision checks while collecting completed native intervals. Missing native
artifacts/device fails native-required Slang gates; optional runs report native unavailability.
Software timestamps prove API/lifetime correctness; physical calibration and matched serial/parallel
CPU/GPU/memory budgets remain separate acceptance. C++ modules rebuild for the appended options/
capabilities/diagnostics and new factory; stable C ABI, SDK/shader baselines and module relationships
are unchanged.

## Optional bounded Vulkan command allocator reuse

`DeviceCreationOptions::enable_recording_resource_reuse` is default off. Vulkan explicitly enables
its `recording_resource_reuse_enabled` snapshot when requested; Null, D3D12 and Metal leave it false.
The old device factory and default options keep allocating/freeing list command pools as before.
All C++ modules rebuild for appended options/capabilities/diagnostics; stable C ABI is unchanged.

With this option, the device owns at most eight idle command pool/primary-buffer pairs. Each live
list exclusively owns its pair, including after its submission completes, until list destruction.
Never-submitted lists can retire immediately; submitted lists become reusable only after Submit's
existing native fence succeeds and records that list's completion token. Acceptance followed by a
submission/wait failure leaves it ineligible for reuse. No extra GPU wait is added. Query pools,
upload resources and framebuffers retain their existing list/completion ownership and are not cached.
Native descriptor pool storage can follow the bounded allocation after exclusive retirement, as below.

Retirement resets an eligible command pool before releasing any remaining descriptor, query or
framebuffer references. Reset failure, ineligible retirement and cache overflow destroy the pair.
A fixed device array makes retirement allocation-free; a device mutex protects cache pop/push and
counters. The newly owning list begins the reset primary buffer and records independently. Device
shutdown performs its existing drain then destroys every cached pool before other native objects.
The caller must still retain submitted lists/resources until completion and destroy lists before
the device; device/resource mutation and queue submission retain their existing synchronization.

Diagnostics report successful newly allocated pairs, cache acquisitions and current idle count.
These are native command-pool events, not descriptor reuse, GPU memory, actual worker concurrency
or asynchronous allocator/timeline support. Eight limits the count, not a byte budget. The complete
ParallelCommandRecording feature remains disabled/unimplemented and Submit remains synchronous.
Native compute and color-target oracles run with timing enabled, test held completed/abandoned
lists, exclusive reuse and overflow, then compare every real GPU output across repeated cohorts,
failures, cancellation, stale snapshots and 512 target allocation/recycling cycles.

### Retired descriptor pool storage

The same opt-in Vulkan cache retains at most two native descriptor pools per idle allocation:
eight command allocations and sixteen idle descriptor pools total. Live lists may grow beyond two
pools for larger workloads; the limit applies to retained idle storage, not active work or bytes.
Retirement first resets the completed/unsubmitted command pool, dropping recorded references,
then resets candidate descriptor pools. Successful resets transfer at most two pools into fixed
cache storage. Reset failure, extra pools and cache overflow destroy storage after command-buffer
cleanup. Acquisition removes the whole package exclusively; constructor allocation/begin failures
destroy all acquired native storage. Shutdown destroys the cached pools after its existing drain.

A new list consumes every prewarmed pool in order before allocating more 32-set blocks. Allocation
failure does not advance the pool cursor. `vkAllocateDescriptorSets` and full snapshot writes still
run for each dispatch; live sets are never shared or rewritten. Native reset frees the previous
sets only after their list retires. Per-list timestamp/query resources remain independent and new.
Diagnostics add native descriptor pool creation, cache acquisition and current idle counts, all
Vulkan-specific. Acquisition counts storage transfers, not reused logical sets or GPU memory.

The native reuse oracle records96 unique workloads across three pools, abandons them, retains only
two pools, re-records using both before creating the third, holds completed lists without returning
storage, and verifies192 CPU results across execution and another abandoned replay. Existing
serial/four-worker, 320 dispatch,48 graph,64 shared-input, failed/cancelled and graphics/query oracles
also run. Benchmark JSON reports matched descriptor storage creation/acquisition counters; smoke
checks verify fresh default pools and exclusively warmed opt-in storage. This is synchronous
retirement reuse; asynchronous timelines, full feature admission and physical budgets remain open.

## Opt-in D3D12 Enhanced Barriers

`DeviceCreationOptions::enable_enhanced_barriers` defaults to false. D3D12 marks the bounded
implementation present only when the build SDK exposes `ID3D12GraphicsCommandList7`; enablement
also requires the separately queried hardware support and a successful native list7 probe. An
unavailable optional request retains legacy barriers. Other backends ignore this D3D12 request;
no feature support is inferred. Existing factories, SDK/shader baselines and stable C ABI remain
unchanged; all C++ modules rebuild for the appended option and diagnostic fields.

The private adapter projects each admitted whole-resource transition into separate synchronization,
memory-access and texture-layout fields. Copy, shader, render-target and indirect usage have
explicit scopes. Shader synchronization includes graphics DRAW finalization for static descriptors.
Undefined/common layout-only points use ALL with NO_ACCESS, avoiding NONE's prohibition on later
resource access in the same execution scope. Undefined texture contents are discarded. UAV/WAW
buffer dependencies emit same-access buffer barriers; buffers have no texture layout. Unknown or
unadmitted projections reject before native emission. Existing revision validation and transactional
list-local snapshots still control submission; failed/cancelled recordings commit no resource state.

An enabled device uses enhanced texture barriers for both user lists and private texture readback,
which restores the committed layout. It does not mix legacy/enhanced texture transitions without a
COMMON bridge. Buffer readback starts a separate ExecuteCommandLists scope, where buffers implicitly
return to legacy COMMON. The legacy path remains the default and provides the same output oracle.
Accepted user-list diagnostics count enhanced buffer/texture barrier emission; rejected/abandoned
lists and private readback do not increment those counters.

`renderer.enhanced_barrier_projection` verifies portable usage policy and explicit Null unavailability.
The Windows compute/graphics recording oracles also accept `--enhanced-barriers`, retaining CPU/pixel,
shared-reader, stale/replay, cancellation and timing checks. They report actual enabled status and
require nonzero native counters when enabled; an unavailable device explicitly tests legacy fallback.
Portable projection, SDK-header syntax checks or enabled=0 output do not prove enhanced native
execution. Aliasing, subresource ranges, multiple native queues and asynchronous handoffs remain
outside this slice; full V2-GFX-2 GPU-validation/target acceptance stays open.

## Bounded native tessellation and baked mesh fallback

`PipelineDescriptor` appends backend-neutral `topology`, `patch_control_points` and
`graphics_shader`. Defaults retain the existing triangle path. PatchList requires exactly three
control points and the Triangle bootstrap shader variant; TriangleList requires zero patch points.
PreTessellatedPatch selects a baked 16-triangle/48-vertex mesh shader. Compute rejects graphics
variants/topology, and invalid enum/combinations reject before native allocation. The public
`EffectivePipelineRequirements` adds tessellation1.0 for a patch without weakening explicit tiers;
`RequirePipelineCapabilities` applies it on every backend. No implicit fallback occurs on creation.

Vulkan enables core tessellation only when queried supported, then builds a four-stage patch PSO
with explicit tessellation state. D3D12 assembles VS/HS/DS/PS with PATCH topology and records a
three-control-point IA draw. Their tessellation implementation flag describes this bounded route;
it does not close V2-GFX-2. Metal rejects native patches through admission and compiles the explicit
PreTessellatedPatch MSL fallback. The caller selects preferred/fallback requirements explicitly.
Requested missing, malformed or wrong-stage artifacts throw; there is no silent shader replacement.

The bootstrap curved patch has fixed factor4 and a nontrivial bowed boundary. Native draws require
exactly3 vertices, baked fallback draws48; indexed/indirect paths for these bounded variants are
not admitted. Ordinary triangle draws retain their existing behavior. All modules rebuild for
appended C++ descriptors/cache keys; stable C ABI and existing SDK/profile baselines stay unchanged.
Arbitrary patch counts, fully general shader assembly and Enhanced Barriers remain separate work.

Pipeline creation loads immutable stage artifacts; referenced temporary Vulkan modules retire
after native PSO creation. Registry publication failures release the unregistered Vulkan PSO.
Pipelines remain caller-owned until DestroyPipeline, outlive all recorded lists and are created
before worker recording. Resource/recording/submission ownership, transactional texture state,
failed/cancelled cohort retirement and default serial execution retain their existing contracts.

`ShaderStage` adds TessellationControl/Evaluation bits8/16; binding validation, layout hashes and
cooked binding metadata preserve them. PipelineCache identity includes topology, patch count and
shader variant, so native, baked and ordinary paths cannot alias even with the same shader hash.
Cooked format1 retains the same encoding; older readers reject unfamiliar stage bits. Entry names
remain part of cooked payload identity. This is metadata/cook coverage, not general Runtime native
hull/domain program assembly. `HasSpirvEntryPoint` reads bounded SPIR-V1.0-1.3 entry metadata for the
Vulkan1.1 baseline; it does not validate shader semantics, provenance or interfaces. Selected DXIL
hull/domain and fallback vertex artifacts undergo the same stage/explicit-minimum checks as other
entries. Hardware/model query success never substitutes for the selected artifact's profile.

The Slang build emits native TCS/TES SPIR-V, baked VS SPIR-V and VS/PS MSL; Windows additionally
emits HS/DS/baked VS DXIL SM6.0. Existing graphics SM6.6 and compute SM6.0 baselines are unchanged.
The generated Tessellation.reflection.json records real entry stages,64-byte fallback frame layout,
source/include/artifact hashes, topology and profile/variant identity. Baked SPIR-V must omit the
Tessellation capability. Configure the explicit environment paths through CTest or the shader
outputs documented in Shaders/README.md; an absent optional compiler does not install itself.

`renderer.tessellation` always checks portable admission, bounded entry parsing, new stage bits,
cache identity and draw bounds. Slang/native tests require real RGBA/BGRA images, nonempty curved
output different from the ordinary triangle, native/baked parity within one channel step and two
worker failure/cancellation/successful replay. Missing/wrong requested stages reject before native
PSO creation; Windows also checks selected HS/DS/fallback profiles. Metal runs real baked output
while reporting native tessellation unavailable. These tests do not certify physical performance,
all tessellation authoring, asynchronous scheduling or complete Enhanced Barrier acceptance.

## Bounded per-draw variable rate shading

`PipelineDescriptor::shading_rate` defaults to `Full` (1x1). `Coarse2x2` adds VRS tier1 admission
without weakening explicit requirements; invalid rates and compute-pipeline VRS reject before PSO
allocation. The rate participates in complete pipeline/cache identity even though native D3D12
stores it as dynamic command-list state. Other backends retain full rate and reject coarse requests;
callers select the full-rate fallback explicitly. No SDK/shader baseline or stable C ABI changes.

When list5 declarations exist, D3D12 records a tier1 implementation and probes the native interface
separately from hardware support before enabling it. Coarse graphics binds query a list-owned list5
reference and call RSSetShadingRate(2x2) with passthrough combiners. Every subsequent graphics bind
on that list restores its requested rate, including full-rate pipelines and later rendering scopes.
Live native references remain list-owned until the existing submission completion/abandonment
retirement. Accepted user-list diagnostics count coarse direct/indirect API draw calls; rejected or
cancelled lists publish no count. No per-primitive rate, rate image, tier2 combiner or queue change
is added. Full-rate-only lists make no shading-rate native calls.

`GraphicsFeatureCapability::implemented_tier` independently caps admitted backend implementations.
Zero preserves the previous four-field report semantics (all reported hardware tiers); a nonzero
ceiling rejects newer requirements with InsufficientImplementedTier. Hardware `tier` remains the
actual query result. D3D12 VRS advertises a tier1 implementation even on tier2 hardware, so tier2
requests cannot pass merely because the device supports them. All C++ modules rebuild together.

`renderer.shading_rate` checks admission/cache identity/implementation ceilings, and uses native
RGBA/BGRA gradients for the selected coarse or full-rate fallback. The oracle requires identical
triangle coverage, at most32/255 channel error at64x64, full-rate restoration after a coarse scope,
forced two-worker recording, abandonment/cancellation sentinels and replay. This fixed fixture's
quality bound is not a content-wide quality guarantee or performance budget. Tests report actual
native enablement/counts; a full-rate fallback does not certify VRS execution. Broader VRS/Ultimate,
physical performance and complete V2-GFX-4 acceptance remain open.

## Bounded D3D12 mesh shader and GPU-produced visibility

`GraphicsShaderVariant::MeshTriangle` selects a separate native MS/PS pipeline, with implicit
MeshShader1.0 and DXIL SM6.5 requirements and distinct cache identity. Patch/compute combinations
reject. The DXIL metadata reader admits Mesh stage bit32 only at SM6.5 or newer. D3D12 requires the
actual selected mesh artifact and DXIL fragment stage, checks explicit minimum profiles for both,
and creates an aligned native pipeline-state stream through Device2. It attaches no VS/HS/DS/GS,
input layout or stream output. The shared frame CBV is visible to graphics stages, including MS.
Missing/wrong/old requested artifacts fail before PSO allocation; no silent shader substitution.

SDK list6/Device2 availability, hardware tier and successful interface probes remain separate.
The implemented tier is capped at1.0. Native interfaces/PSOs remain privately owned; publication
failure returns the new handle and releases unregistered native state. Other backends reject the
mesh request and callers select their existing compute/indirect raster fallback explicitly.

A bound MeshTriangle pipeline admits `BindStorageBuffer(0, statistics)` inside its rendering scope.
This is a read-only, at-least-five-word uint buffer; other graphics storage slots/variants reject.
BindPipeline resets this private input, so it must be rebound explicitly. Preparation captures the
same handle generation/revision and immutable SRV address as existing buffer reads. CPU mutation,
nonempty readback or potential GPU writes invalidate stale recorded input. Compatible shared
readers preserve revisions. Device/resources must outlive all lists and completion as before.

`DispatchMesh(1,1,1)` is the only admitted built-in mesh dispatch. It requires rendering, the mesh
pipeline, a valid explicit input and list6. Ordinary/indirect IA draws reject on mesh pipelines.
GPUDriven.slang writes its visible count at statistics word4; the mesh shader reads that GPU value
and calls SetMeshOutputCounts for zero or three vertices/one triangle. The coordinator always
submits one mesh group; it performs no CPU count decision. Accepted-list diagnostics count mesh
API dispatches, including zero-output groups. Abandoned/stale/cancelled work publishes no count.

Mesh stage bit32 survives reflection/layout and cooked metadata; unknown bit64 still rejects.
The optional existing Windows Slang target emits MS6.5 DXIL plus real input/entry reflection and a
source/artifact identity manifest. Baseline VS/PS6.6, CS6.0 and SDK options remain unchanged.
Vulkan's bounded1.1/SPIR-V1.3 entry reader explicitly does not admit a Mesh execution model.
All C++ modules rebuild for appended types/diagnostics and the new virtual method; stable C ABI
and existing cooked format1 encoding remain unchanged. This does not add Runtime MS/PS assembly.

The native oracle compares GPU-computed inside/outside-frustum counts with the CPU reference,
then compares actual mesh output with GPU-generated native indirect draws in RGBA/BGRA. It also
checks bounded dispatch/input rejection, abandoned/stale work, shared-input two-worker recording,
failed/cancelled cohorts and successful replay. A fallback-only run is not mesh execution proof.
General meshlets, amplification shaders, indirect mesh dispatch, cross-backend mesh support,
physical performance and the full Ultimate/V2-GFX-4 gates remain open.

The mesh and shading-rate Windows oracles also accept `--enhanced-barriers`, running the same
output/lifetime/reset checks on the requested enhanced device and reporting actual accepted-list
barrier counts. Graph ShaderRead buffers may use pixel, non-pixel or combined native SRV states;
all three project to shader-read memory access. Mixed SRV/UAV states remain unadmitted. The SDK
adapter regression covers this boundary explicitly. Neither unavailable enhanced requests nor
Linux pure-policy checks certify native D3D12 execution.

## Optional bounded native Work Graphs

`NEXORA_ENABLE_D3D12_WORK_GRAPHS` defaults off. Opt-in requires Windows native D3D12, SDK list10
interfaces, a discovered SDK dxc.exe (or explicit NEXORA_DXC_EXECUTABLE), and the existing Slang
compute fallback. It independently compiles lib_6_8 HLSL nodes; baseline Slang/profiles stay unchanged.
Runtime device queries still separate hardware tier, compiled implementation and successful native
list10/Device5 admission. The implementation tier is capped at1.0. New C++ virtual methods/types
require module rebuild; stable C ABI and cooked ShaderModuleArtifact encoding remain unchanged.

CreateWorkGraph admits a bounded visible-record expansion recipe, implicitly requiring WG1.0 and
SM6.8 without weakening explicit requirements. It loads the requested bounded DXIL library, checks
actual profile against requested minimum and device maximum, then validates real native program/
node properties: one named program, producer/consumer nodes, one producer entry with eight-byte
CPU input, and no local root arguments. Missing/wrong library kinds reject before native state
creation; entry ABI mismatches release temporary native state before handle publication. A dedicated
root signature exposes only the output UAV. No arbitrary shader resources are implicitly inherited.

WorkGraphInput is two uint32 words, count/first_object_slot. Count is0..8 and slot ranges cannot
overflow or produce the invalid UINT32_MAX slot. The GPU producer fans out child records, whose
consumer writes five-word visible instances (slot,generation1,mesh11,material13,LOD0). This is a
fixed bootstrap recipe, not general graph authoring, GPUScene integration or frustum culling.
The existing GPUDriven compute/indirect path is the explicit fallback/reference for equivalent
known-visible candidates. Library kind/profile metadata never implies Node/RT exports: the
single-entry DXIL inspector still rejects libraries, and actual exports/ABI require native checks.

BindWorkGraph selects an immutable program and resets its explicit output binding. BindStorageBuffer
slot1 supplies at least160 bytes; DispatchWorkGraph validates it and records one CPU input record.
D3D12 copies that record during recording. Each list retains at most four program snapshots and
four dispatch contexts, each with a fresh private backing resource and INITIALIZE. Exact native
MinSizeInBytes is used, capped at64MiB/context; native-valid zero uses no fabricated allocation.
Command-list interfaces retire before private backing, state objects, roots and query resources.
No backing memory is shared between live contexts, cached or asynchronously reused.

Output buffers use existing potential-UAV-write revisions and caller/graph-owned dependencies.
CPU mutation/readback invalidates stale lists. Program destroy/recreate invalidates recorded handles;
Submit validates all snapshots before closing/executing. Private references preserve safe cleanup
for abandoned/rejected lists, and all submitted objects still outlive completion. Ordered submission
remains synchronous on the existing native queue. Accepted diagnostics count graph API dispatches,
including zero-child fanout; they do not count GPU child invocations. Graph/ordinary-compute binding
switches restore their respective private root signatures and program state.

renderer.work_graph checks input/profile/tier admission, real GPU compute/CPU visible records with
untouched sentinels, fanout0/1/8 and high uint slots, forced two-worker failure/cancellation/replay,
missing/wrong libraries and actual compiled wrong entry ABI. Native runs also test stale output/
program generations, abandonment, four distinct copied CPU records/private contexts and mixed
Graph/Compute/Graph execution. Generated manifests record requested profile, actual compiler version
and source/binary hashes, with native execution explicitly pending. Fallback-only/compilation output
is not native Work Graph proof. General graphs, graphics nodes, async queues, physical budgets and
SM6.9/6.10/full V2-GFX-5 acceptance remain open.

The Windows node oracle also runs `--enhanced-barriers`, preserving the same CPU/compute,
revision and cancellation checks and requiring nonzero accepted enhanced buffer barriers when
the feature is enabled. A legacy/unavailable route must leave these counters zero.

## Bounded native triangle rays and compute reference

`RayTracing.h` adds C++ scene/program handles and fixed record layouts; the stable C ABI is
unchanged. A scene contains one to eight finite, nondegenerate, non-indexed triangles. Native
D3D12 privately owns upload vertices/instances and immutable BLAS/TLAS. Construction records
BLAS build, a UAV dependency, TLAS build and a second UAV dependency on the existing direct
queue, and waits for completion before publishing the handle. Build scratch retires after that
wait. This explicit resource creation does not count as an application ray dispatch or hide a
production readback. Resource creation/destruction, mutation and submit remain caller-serialized
against recording; independent lists may read the same immutable scene/program.

`CreateRayProgram` selects `Pipeline` (tier1.0, library SM6.3) or `InlineQuery` (tier1.1, compute
SM6.5). Explicit requirements can raise these minimums. Hardware, SDK, compiled artifacts and
implemented tier remain separate; the implementation ceiling is1.1 and never admits DXR1.2/SER.
With native Windows and the existing Slang option, real reflection validates `t0` acceleration
structure, `t1` 32-byte ray inputs and `u1` 16-byte hits, including member offsets. Generated
private arrays pin exact compiled recipe bytes, and the manifest records compiler/source/artifact
identity. Runtime validates actual library/entry metadata and profile, then exact recipe identity
before native creation. Library kind alone does not infer ray exports. The state object validates
three required exports, a triangle hit group, 16-byte payload, eight-byte attributes and recursion1;
private shader tables use 64-byte-aligned records and validated native shader identifiers. Missing,
wrong-kind, low-profile and same-profile changed-recipe artifacts fail before dispatch. General
custom ray program/cooked-library assembly is outside this bounded recipe.

`TraceRays(program, scene, input, output, count)` accepts1–64 rays, distinct input/output buffers
and at most eight immutable per-list snapshots. It privately binds the dedicated root and either
`DispatchRays` or an inline-RayQuery compute PSO; no CPU hit result chooses GPU work. Scene/program
state, shader tables, AS and captured buffer resources outlive every native command-list interface,
including abandonment. Submit revalidates program/scene generations and ordinary input/output
revisions before Close/Execute; rejected or cancelled lists publish no ray counters. Selecting rays
invalidates the ordinary bound pipeline; an explicit `BindPipeline` restores compute/graphics state.
`ray_dispatch_calls` and `inline_ray_dispatch_calls` count accepted API dispatches, including misses,
rather than GPU invocations or speedups.

Ray inputs store origin/minimum/direction/maximum. Invalid non-finite/zero direction or invalid
finite distance ranges produce primitive `0xfffffffe`, distance0 and zero barycentrics without
issuing hardware rays. Misses use primitive `0xffffffff`, the input maximum and zero barycentrics.
Hits report primitive index, ray-parameter distance and two triangle barycentrics. Fixtures avoid
ambiguous coincident/edge intersections; floating output comparisons allow1e-4, not a general
cross-device precision guarantee.

`ComputeShaderVariant::RayReference` explicitly selects a separate lower-SM float Moller–Trumbore
compute shader on Vulkan/D3D12/Metal and has a distinct complete pipeline-cache identity. It uses
packed counts/triangles/rays at slot0, four-word hits at slot1 and the existing compute binding
snapshot contract; default GPUDriven pipelines remain unchanged. Validation rejects this native
kernel rather than simulating a ray result. The double-precision CPU oracle compares real compute
and selected native ray results for1/2/8 triangles and1/64 rays, canaries, misses/nearest hits,
non-unit/invalid rays, forced two-worker failure/cancellation/replay and mixed Ray/Compute/Ray
roots. Native tests also cover stale/destroyed handles, CPU mutation, abandoned lists and snapshot
capacity, under both legacy and requested Enhanced Barriers. Fallback-only/compilation results do
not certify DXR execution. Native Vulkan/Metal RT, AS updates/compaction, broader lighting, DXR1.2,
SER, physical performance and the complete V2-GFX-3 gate remain open.

When both native programs are admitted, the ray oracle records Node/Ray/Node in one list and
compares both visible-record outputs and the ray result. Ray selection clears the logical bound
Work Graph; a later node dispatch requires explicit rebind. Each path retains its own roots, state,
backing/table/AS resources and native command-list interfaces until list destruction/completion.

## Bounded native triangle-scene refits

`UpdateRayScene(handle, triangles)` requires `Capabilities().ray_scene_refit_enabled`. This defaults
closed and is admitted only on the explicitly selected independent pinned runtime with native ray
support. Hosted system-runtime execution at0a4acdad segfaults in both ray fixtures, while both
isolated preview suites pass36 updates per barrier mode; the exact crashing native call is not yet
established. Preserve ordinary system-runtime immutable scene construction without ALLOW_UPDATE
and reject updates before GPU work. Raw DXR support alone does not authorize this bounded recipe.
The admitted D3D12 ordinary1–8-triangle recipe keeps the
same triangle count, one triangle geometry, vertex format/stride and one TLAS instance. Empty,
nonfinite/degenerate geometry, changed counts, stale handles and OMM scenes reject before native
execution or publication. Other backends and builds without pinned ray artifacts explicitly throw
unsupported. General topology/material/multi-instance edits and OMM updates/compaction remain open.
The C++ virtual surface/diagnostics require consumer rebuilds; the new diagnostic is appended to
preserve existing aggregate field order. The stable C ABI is unchanged.

`RebuildRayScene(handle, triangles)` is the explicit full-native-build alternative on admitted native
ray backends, including the ordinary system route. It accepts bounded count changes, replaces the
scene only after completion, preserves previously recorded snapshots, rejects OMM/stale/invalid
requests and increments only `ray_scene_rebuilds`. It does not imply update/performance support.

Only admitted updatable BLAS/TLAS builds request ALLOW_UPDATE. Each update allocates private replacement
vertex/instance/result storage and uses actual PERFORM_UPDATE with the previous BLAS/TLAS GPU
addresses as sources. Queried update scratch is separately bounded to64MiB per resource; zero
scratch remains valid. BLAS UAV dependency precedes TLAS update and a final TLAS dependency precedes
publication. The same direct queue completes a fence before replacing the handle's scene snapshot.
No CPU traversal result drives production geometry or GPU work. `ray_scene_refits` counts successful
published scene updates; initial builds and rejected/failed updates do not increment it.

Mutation stays caller-serialized against recording/submit. Lists recorded before publication retain
the old complete scene; later lists capture the new scene under the same live handle. Destroying the
handle still rejects submission of recorded lists. Source AS, upload/result/scratch resources,
allocator and list remain device-owned after native execution is accepted until completion. A late
Signal/wait failure retains one bounded pending build, leaves the published scene unchanged and
rejects further AS construction/refits until device recreation. Device shutdown attempts queue idle
before releasing retained resources. Native completion-failure injection and physical device-loss
acceptance remain separate from ordinary hosted output/lifetime coverage.

Native fixtures perform four updates for each1/2/8-triangle pipeline/inline recipe, compare512 old/
new outputs to the double-precision CPU oracle, keep earlier lists through later publication and
check invalid/topology/OMM/stale rejection and canaries. The same fixtures run with legacy/enhanced
barriers and the existing preview/SER suite. Physical performance and full V2-GFX-3 remain open.

## Bounded shared-BLAS instance sets

`CreateInstancedRayScene(triangles, transforms, options)` copies one to eight absolute row-major
transforms into an ordinary scene sharing one BLAS. Local triangles times instance count must not
exceed eight. Every matrix and transformed geometry extent validates before GPU allocation.
Default scene creation still selects one identity instance. Ordinary system runtime creation and
full rebuild retain their immutable build flags; compaction opt-in and transform/geometry refit
still require the separately admitted private runtime. Vulkan/Metal/Validation creation explicitly
rejects; callers may select the existing compute reference with baked world-space triangles.

Hits use the flattened instance-major index `instance * localTriangleCount + primitive`, bounded
to0..7, without changing the16-byte hit ABI. Native instance IDs encode this base; pipeline,
inline-query and SER shaders add it to the local primitive. Single-instance/OMM indices stay
unchanged. The existing lower-profile reference sees the same instance-major world geometry and
returns matching indices/distances/barycentrics; it does not claim native instancing.

`SetRaySceneTransforms` replaces every absolute matrix with the same instance count and retains
the shared BLAS/vertices. The singular setter admits only a one-instance scene. Geometry refit,
compaction relinking and full rebuild preserve all matrices; rebuild recalculates primitive bases
if local triangle count changes, subject to the total bound. Changing instance count requires a
new scene handle. All operations are caller-serialized; old lists retain their complete prior AS,
instance upload and primitive mapping. Existing construction-fence publication and accepted-failure
quarantine apply. The transform diagnostic counts scene publications, not individual instances.
C++ consumers rebuild; stable C ABI, shader profiles and module dependencies are unchanged.

Fixtures pair1/2/8 local triangles with8/4/1 instances, compare every instance's primitive base and
exercise two overlapping workers, failure/cancellation/replay, retained old/new lists and
refit/compaction/rebuild mapping. Private fixtures compare704 CPU results each; system and baked
compute fixtures compare320 each. Private acceptance requires63 refits/27 rebuilds/36 compactions/
45 transform publications per barrier mode. System modes require12 rebuilds with all three update
counters0. General heterogeneous meshes, instance masks/materials, lighting, asynchronous retirement
and physical/full V2-GFX acceptance remain open.

## Bounded TLAS instance transforms

`SetRaySceneTransform(handle, RayInstanceTransform)` replaces the ordinary scene's absolute
object-to-world3x4 row-major matrix; identity restores object space. It requires the admitted
private `ray_scene_refit_enabled` profile and rejects OMM/stale/unadmitted requests before GPU work.
The bounded recipe accepts finite translation components up to1e8 in magnitude and basis-row
lengths from1e-4 to1e4. A determinant magnitude greater than1e-6 times the product of row lengths
rejects singular/nearly dependent bases. CPU geometry bounds conservatively reject transforms
whose absolute intermediate world extents exceed finite float storage. Reflection is admitted;
this is not a general unbounded numerical-transform contract.

A transform operation retains the existing BLAS/object-space vertices, allocates private instance/
TLAS/update-scratch storage and records only actual out-of-place TLAS PERFORM_UPDATE with the
matching original build flags. A TLAS UAV dependency and queue completion precede publication.
Previous lists retain their complete old snapshot; new recording captures the replacement under
the live handle. The appended `ray_scene_transform_updates` counts successful publication only,
separately from geometry refits, full rebuilds and compactions. These operations preserve the
current matrix, including TLAS relinking after BLAS compaction. The shared pending-context policy
retains all accepted sources/results/scratch/allocator/list resources after Signal/wait failure;
subsequent AS construction/mutation rejects until device recreation. Mutation remains caller-
serialized against recording/submit. Stable C ABI, shader profiles and module dependencies stay
unchanged; C++ consumers rebuild.

Fixtures compare translation, rotation/nonuniform scale and reflection with CPU-transformed
geometry, retained old/new lists and identity reset, then verify geometry-refit/compaction/rebuild
preservation, invalid/stale/OMM rejection and canaries. Each native fixture compares704 outputs;
private legacy/enhanced acceptance before the additional instance-set fixtures requires36 transform updates,54 explicit geometry refits,
18 full rebuilds and27 compactions per mode. Unsupported native recipes exercise the existing
world-space GPU compute reference with192 comparisons per size/mode without claiming TLAS work.
Multi-instance scenes, general edits/lighting, completion-failure injection and physical acceptance
remain open.

## Explicit bounded BLAS/TLAS compaction

`CreateRaySceneWithOptions(triangles, {true})` explicitly admits compaction metadata for the ordinary
1–8-triangle/one-instance recipe. Default `CreateRayScene` and false options retain their previous
runtime-specific immutable/updatable build behavior. Compaction additionally requires the admitted
private refit runtime because TLAS relocation performs an update; unadmitted requests reject before
GPU work. Compactable BLAS/TLAS additionally request ALLOW_COMPACTION;
refits preserve that choice. The [official flag contract](https://microsoft.github.io/DirectX-Specs/d3d/Raytracing.html#d3d12_raytracing_acceleration_structure_build_flags)
permits both flags together but warns of increased initial storage/build cost. Other backends reject
explicit compaction admission; the stable C ABI is unchanged and C++ consumers rebuild.

`CompactRayScene(handle)` rejects stale, unopted and OMM scenes before execution. It synchronously
runs three bounded resource-construction phases on the existing direct queue: GPU postbuild query
of BLAS compacted size and explicit eight-byte metadata readback; actual BLAS COMPACT copy and TLAS
out-of-place refit linking the new BLAS, followed by TLAS compacted-size query/readback; then actual
TLAS COMPACT copy. UAV dependencies order writes, queries and copies. Queried sizes must be
positive and fit both64MiB and the original per-AS buffer width. Allocate the exact queried extent and separately validate
256-byte destination GPU-address alignment; DXR does not require256-byte-aligned buffer lengths.
Each AS owns a committed resource, so adjacent suballocation strides do not need padding.
A relinked TLAS that cannot fit the original buffer bound rejects without replacing the scene. No strict shrinkage is
required. This is an explicit allocation operation with three completion waits and two metadata
readbacks, not an implicit frame/render or CPU-hit-result path.

Publication occurs only after final completion. Older command lists keep the old scene; new lists
capture the compacted scene. Compacted scenes retain update/compaction permission, tested with a
compaction/refit/compaction round trip. Full rebuilds preserve the original opt-in choice. Source/results/instances/query/readback/scratch/allocator/
list resources stay in the device-owned pending context throughout each accepted phase, including
late Signal/wait failure. The existing one-pending-build failure policy applies. Metadata validation
failure after a completed phase leaves the original scene unchanged; it publishes no counters.

Appended diagnostics count successful scene compactions and sum before/after native AS **buffer
widths**, not physical heap bytes, RSS, performance or guaranteed memory savings. Tests cover1/2/8
triangles across pipeline/inline/SER, old/new CPU output/canary checks and rejected admission/lifetime
cases. Private preview acceptance requires45 explicit refits (the original36 plus nine round trips)
and18 scene compactions per barrier mode before the additional transform fixtures above. OMM relocation, general scene compaction, native failure
injection, physical memory/performance and full V2-GFX-3 acceptance remain open.

## Bounded min-mip sampler feedback footprint

`SamplerFeedback.h` exposes an explicit metadata-footprint bootstrap program and16-byte GPU
requests (`float u/v`, `uint mip/enabled`). This recipe privately pairs a64x64 RGBA texture with
seven mips and16x16 mip regions; it does not attach to application textures or sample color data.
The paired texture supplies sampling metadata, so color upload is unnecessary. General material
feedback, virtual pages and residency integration remain separate work.

Native D3D12 requires independently queried feedback tier0.9/SM6.5, compiled Device8/list1 and the
existing Slang option. Its implementation ceiling is0.9 even on tier1.0 hardware. A full-mip/base0/
plane0/minLOD0 SRV and static point/clamp sampler satisfy tier0.9 restrictions. Actual reflection
validates the MIN_MIP feedback mode, root bindings,16-byte request offsets/types and16-byte root
constants; exact generated recipe bytes pin the write/pack programs before native PSO creation.
Shader provenance is retained without inferring native execution. Missing/wrong-stage/low-profile
or changed-recipe artifacts reject; builds without pinned artifacts inherit explicit unsupported
factories. Stable C ABI and baseline shader/SDK profiles are unchanged.

`DispatchSamplerFeedback(program, input, output, count)` accepts0–64 requests, distinct buffers,
input storage for at least one request and at least64 output bytes. Enabled1 requests with finite
UV in[0,1) and integer mip0–2 write feedback; other requests are ignored on the GPU. These mip
levels avoid small-tail ambiguity for this fixed footprint. Every call owns a fresh opaque
feedback texture, decoded4x4 R8_UINT texture and immutable shader-visible/CPU-clear descriptor
heaps. Clearing ignores the provided values and decodes to255. After write/UAV dependency,
ResolveSubresourceRegion(DECODE_SAMPLER_FEEDBACK) and a shader-read transition, a GPU kernel widens
16 decoded bytes to16 row-major uint32 outputs. Duplicate requests take the minimum mip; untouched
regions remain255. There is no production CPU result readback.

A live list retains at most four private contexts and immutable program/paired/root/PSO ownership,
all beyond native command-list interface lifetime. Submit checks program generations and ordinary
input/output revisions before Close/Execute. Abandoned/cancelled/stale lists publish no accepted
feedback calls. Resource creation/mutation/destruction/submit remain caller-serialized against
recording; disjoint workers share only immutable program/paired metadata. The diagnostic counter
counts accepted API calls, including clear-only count0, rather than sampler instructions or speedup.
Feedback selection invalidates the ordinary pipeline and Work Graph selection; explicit rebind
restores the chosen program. Private feedback textures use their own legacy transitions while
user buffers retain their requested legacy/enhanced route.

`ComputeShaderVariant::SamplerFeedbackReference` has a distinct complete pipeline-cache identity
and executes lower-profile compute on Vulkan/D3D12/Metal. Slot0 packs count plus four words/request;
slot1 returns the16 uint map, using existing four-slot compute snapshots. Validation refuses to
simulate this native kernel. Native tests compare real compute and CPU maps for0/1/16/64 requests,
invalid-only inputs, clear255, duplicate minima, untouched cells and output canaries. They also test
forced two-worker failure/cancellation/replay, wrong artifacts, stale/destroyed/abandoned resources,
four-context bounds, Feedback/Compute/Feedback and Node/Feedback/Node roots/heaps under both legacy
and requested Enhanced Barriers. Compilation and fallback-only enabled0 runs are not native feedback
proof. Arrayed/other feedback formats, application texture sampling/streaming/residency, physical
budgets and full V2-GFX-4/Ultimate acceptance remain open.

MIN_MIP decode explicitly uses source subresource UINT_MAX (the complete flattened opaque mip
chain) and destination slice0. Per-mip source0 is not valid for this recipe. Windows execution
remains required independently from the compute oracle; mismatch diagnostics retain all map words.

## Explicit D3D12 adapter selection

`DeviceCreationOptions::d3d12_adapter_policy` applies to the ordinary and isolated preview factories.
`Default` preserves ordinary high-performance hardware preference with WARP fallback and preview
WARP selection. `Software` explicitly selects WARP; `PreferHardware` tries nonsoftware DXGI adapters
in high-performance order before WARP; `RequireHardware` never selects WARP. Explicit hardware
policies also exclude known Microsoft Basic Render Driver/WARP identity1414:008c, even when DXGI
omits its software flag. The ordinary `Default` retains its existing flag-based enumeration for
compatibility. Exhausted compatible hardware discovery throws `D3D12HardwareUnavailableError`.
DXGI enumeration/description failures,
resource exhaustion, device removal and other runtime failures propagate as ordinary runtime errors;
only unsupported feature-level/interface results allow the next candidate. Enumeration is bounded
to64 adapters and exceeding the bound fails. Invalid enum values and explicit policies on other
backends fail with `invalid_argument` before backend creation. Existing aggregate boolean fields keep
their order; all C++ consumers rebuild for the appended field. The stable C ABI is unchanged.

Both routes select the adapter through the same helper while retaining their own device creation
entry points. The independent preview factory still outlives its resources and never publishes global
state. Selection does not enable additional features or relax actual Shader Model, private-runtime,
artifact or per-pipeline admission. `software_device` conservatively reports DXGI software flags or
known WARP identity1414:008c, not physical-host acceptance. This corrects the hosted flag omission
without changing the ordinary Default adapter choice.
Windows tests exercise all four policies, submission/buffer witnesses, retained-device lifetime and
hardware-required success or its specific exhaustion error. The preview test has its own executable,
WARP/Agility deployment directory and working directory. Physical modern-feature correctness and
performance require separate target-GPU evidence.

## Isolated modern compute opt-in

`NEXORA_ENABLE_D3D12_PREVIEW` is default off and Windows-only. Stage the hash-pinned packages with
`Tools/Build/PrepareD3D12Preview.py --destination <external-directory>` and configure
`NEXORA_D3D12_PREVIEW_ROOT`. Only dedicated preview tests receive local WARP/Agility DLLs. The explicit
`DeviceCreationOptions::enable_d3d12_preview_runtime` uses an independent SDK721 factory; it never
changes global D3D12 configuration or OS settings. Experimental shaders require host Developer Mode.
The factory remains alive beyond the native device and resources. Other backends reject this option.

The fixed `LongVector69`, `LinearAlgebra610` and `GroupShared610` compute recipes require SM6.9 or
SM6.10;6.10 additionally requires per-pipeline `allow_preview_shader_models`, the private runtime
and independently queried F32 matrix/vector support or at least65536 shared bytes. Unknown queries
fail closed. Pipeline cache identity includes the permission. Actual DXIL stage/profile and exact
build-generated recipe bytes are checked before PSO creation. Dispatch accepts one group only,
distinct input/output buffers at slots0/1 with at least128/32 bytes, and the existing four bindings.
Existing recording snapshots/revisions handle abandonment and stale resources. Outputs are integer
weighted sums, four F32 matrix/vector results or a16384-element shared-memory reduction. The declared
shared ceiling is static, not a runtime allocation API. Baseline SDK, Slang profiles, stable C ABI and
cooked wire formats stay unchanged; all C++ modules require a coordinated rebuild. No Vulkan/Metal
execution is implied. Both PR635 Windows preview jobs verify all three native recipes on pinned
app-local WARP, including generic SM6.10 permission admission. Broader modern shader/cooking
integration and physical GPU performance remain open. Variable group-shared limits retain static
array sizes as required by the published specification; runtime dynamic allocation is not required.
execution is implied. Both PR635 native Windows preview jobs accept these three fixed recipes
on pinned app-local WARP. The published variable group-shared feature changes the device limit and
compile-time ceiling; static array sizes remain required. General shader/cooking and physical GPU
acceptance remain open.

Actual attached DXIL models are checked independently from omitted caller minimums. SM6.9+ stages
must satisfy the queried device limit; actual6.10 bytecode additionally requires explicit pipeline
preview permission and the private runtime, including the generic GPUDriven entry. Only trusted
in-process FXC5.0 fallback stages bypass DXIL metadata parsing. Lower-profile default behavior is
preserved. The native test includes undeclared-minimum6.10 rejection and opted PSO creation.

Sampler feedback separates queried support from runtime enablement. Hosted system-runtime
execution returns mip0 for an explicit mip2 request despite correct GPU inputs; the identical
strict fixture passes on the isolated pinned preview runtime. The failing adapter does not
report the software flag, so adapter classification cannot establish runtime readiness. Native
feedback is therefore admitted only with explicit independent preview-runtime creation and the
tested SM6.10 profile, independently of the adapter flag. Raw support/tier and implementation
metadata remain available. Other devices use the real lower-profile compute variant. This gate
is intentionally conservative until other runtime routes pass equivalent native acceptance.
Whole-map MIN_MIP decode uses UINT_MAX source/destination indices. Preview acceptance requires
native execution and exact output/canary equality; no runtime component or physical GPU is
inferred to be faulty from this comparison. Resource ownership/default device selection stay
unchanged. Physical-adapter acceptance remains separate.

## Bounded SER recipe

Default-off `NEXORA_ENABLE_D3D12_SER` requires the preview build and Slang baseline rays/reference.
`RayTracingMode::ReorderedPipeline` adds implicit SM6.9 and independent SER requirements. The
published HLSL Device Support rule requires both SM6.9 and raytracing; guarded OPTIONS22 separately
records `actual_shader_reordering_support`. False means no actual reordering, not invalid HitObject
or SER semantics. Missing queries remain unknown. Compiled recipe, support and enablement remain
separate. The RT implementation ceiling stays1.1 until OMM is implemented.

Actual DXC reflection checks t0/t1/u1 and32/16-byte ray/hit layout, exact pinned library bytes and
native shader identifiers before creation. HitObject TraceRay/Reorder/Invoke reuses the existing
root/state/table and per-list ownership/revision contract. `ser_ray_dispatch_calls` counts accepted
SER API calls only after submission, included in ordinary ray counts; rejection/cancellation does
not publish calls. Lower-profile ordinary rays/compute remain explicit alternatives. WARP validates
outputs without actual thread reordering; hardware coherence/performance and full SER acceptance
remain separate. Native preview Windows evidence is pending.

## Bounded opacity micromaps

`NEXORA_ENABLE_D3D12_OPACITY_MICROMAPS` is default off and requires the pinned preview SER recipe.
`CreateOpacityMicromapScene` pairs1–8 triangles with low-four-bit OC1 two-state subdivision1 masks
(A corner, center, B corner, C corner). Set bits are opaque. Native construction builds an actual
OMM array, UAV dependency, linked BLAS, UAV dependency and TLAS, then waits its private creation
fence. Mask/description/index/result resources are retained with the immutable scene; OMM owners
outlive linked BLAS/TLAS. Every native allocation remains bounded64 MiB; scratch retires after
construction completion. Creation/mutation/destruction/submit remain caller-serialized.

`ReorderedOpacityPipeline` adds actual RT1.2 to SM6.9/SER admission and config1 OMM permission.
Only the OMM build option raises the bounded implementation ceiling to1.2; SER alone stays1.1.
Scene/program opacity mismatches reject before dispatch. The existing pinned SER library,16-byte
payload, three shader-table records, dedicated roots and generation/revision/snapshot contract
apply. `opacity_ray_dispatch_calls` counts successful submitted OMM calls as a subset of SER/ray
calls. Vulkan/Metal run the real explicit-microgeometry compute oracle without native OMM.
All16 masks and nearest transparent-front occlusion are checked independently; native proof is
pending Windows CI. Four-state/any-hit, higher subdivisions, updates and general material/cook/
lighting/physical acceptance remain open. Stable C ABI and cooked formats remain unchanged.

## Immutable VRS Tier2 images

`ShadingRateImage.h` adds a separate typed immutable source, without adding public texture formats.
The capability snapshot records the independently queried tile size; only valid tier2 tile queries
raise the bounded VRS implementation ceiling to2.0. `ShadingRate::Image` implies tier2 in complete
pipeline admission/cache identity. Factories accept raster extents1–4096 and exact tile-grid bytes
with baseline1x1/1x2/2x1/2x2 encodings. Private R8_UINT upload/transition waits its creation fence;
caller storage may change immediately after creation. Construction/mutation/submit stay serialized.

A graphics list explicitly binds a matching source after selecting its image pipeline. Draw,
indirect and mesh paths reject missing sources. Each pipeline rebind resets native image/base/
combiners and requires explicit image rebind. Up to16 unique snapshots retain sources beyond all
native command-list interfaces; repeated binding consumes no new snapshot. Submit checks generations
before Close/Execute. Disjoint workers may share immutable sources; stale/abandoned/cancelled work
publishes no accepted image calls. `shading_rate_image_draw_calls` is distinct from per-draw coarse
counts and does not count shaded pixels or speedups. Full-rate native comparison remains available
on other backends; Validation does not emulate rate images.

Preview tests additionally compile a DXC6.4 hardware SV_ShadingRate witness with the separately
pinned compiler. This does not raise ordinary Slang/SDK profiles. Tier2 native execution/quality,
caller-memory independence, reset, bounds and lifetime proof remain pending Windows CI. Mutable
GPU-authored images, additional rates, primitive controls and full physical/Ultimate gates remain
open. Stable C ABI and cooked wire formats are unchanged; C++ modules rebuild together.

Ray build preflight requires a nonzero result within the64MiB bound and bounds scratch separately.
A zero scratch requirement retains no scratch resource and passes a null scratch GPU address;
positive scratch retains the existing creation-fence lifetime. Zero result sizes remain errors with
operation/result/scratch diagnostics. Native preview acceptance must confirm these build paths.

## Explicit lower-profile modern compute equivalents

`LongVectorReference`, `LinearAlgebraReference` and `GroupSharedReference` provide baseline
Slang SM6.0/SPIR-V/Metal implementations of the fixed modern recipes. No modern feature bit or
preview runtime is implied. All variants preserve the t0 input/u1 output and four-root binding ABI,
one group, distinct input/output resources of at least128/32 bytes, unchanged output canaries,
and the existing per-list snapshots/generation validation. Variant identity participates in PSO
cache identity. Append-only enum identities preserve all existing modern and feedback recipes;
all C++ modules require a coordinated rebuild, with no stable C ABI or cooked-format change.

The scalar group reduction computes the same unsigned result without64KiB shared storage. Every
entry uses one thread per group, matching the existing Metal dispatch contract. Artifact selection
is explicit and validates entry/stage through existing backend admission. Absent artifacts fail
before pipeline creation; no reference is silently substituted for a selected modern recipe.
Preferred/fallback choice remains the caller's responsibility. General shader authoring and physical
performance are outside this bounded equivalent; native and hosted acceptance remains pending.

For these reference variants, Vulkan and Metal capture all four buffer generations/data revisions
at each dispatch under the device mutex. CPU writes advance revisions; Submit rejects a changed or
recreated resource before native Close/queue acceptance. Accepted GPU writes publish revisions;
immutable shared inputs preserve theirs. This reference bookkeeping is per command list. Callers
still retain resources until completion under the existing synchronous ownership contract.
