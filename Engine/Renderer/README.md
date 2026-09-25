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

## V1-M3 contract

`RenderGraph` derives RAW/WAR/WAW dependencies, rejects cycles, topologically orders passes, computes transient lifetimes, and emits state transitions. `PipelineCache` coalesces identical asynchronous requests. The validation device rejects stale resources, invalid transitions, rendering-scope violations, missing pipelines, and presenting a non-Present resource.

Passes must declare each texture exactly once: a texture cannot be both read and written by the
same pass. Unused transients are lifetime-elided and never allocated. Command lists are single-use;
submission while a rendering scope is open, submission to another device, a second submission, or
recording after submission is rejected. Pipeline identity is the complete layout/shader/format key
(debug labels are deliberately excluded), so hash collisions cannot alias cache entries.

The executable test runs `Offscreen -> Main -> Present` through this contract and verifies pass,
barrier, draw, submit, and present counts. The platform CI matrix requires DX12 on Windows,
Vulkan on Linux, and Metal on macOS to execute the same workload. The current milestone is
offscreen; descriptor indexing, multi-queue synchronization, transient heap aliasing, and
window-system presentation remain explicit expansion points.

## Ownership and lifetime

`NexoraRenderer -> NexoraRHI -> NexoraCore`. RenderGraph owns transient textures only for one execution and waits idle before releasing them. Imported resources remain caller-owned. A `PipelineCache` must be destroyed before its device and job system; destruction waits outstanding creation jobs and releases cached pipelines.

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

RenderGraph tracks a logical owner queue for every resource. A use on a different compute/graphics
queue emits an ownership barrier even when the resource state is unchanged, and statistics expose
those transfers separately from ordinary state transitions. The graph retains transient ownership
until all submitted work is idle, then releases the resources; imported resources remain
caller-owned. Vulkan's native gate is defined to execute the full frustum/distance/LOD/Hi-Z/compaction/classification/indirect-generation kernel through a RenderGraph compute pass, require an exact CPU/GPU result comparison, transfer ownership to a graphics pass, and issue native indirect drawing. Its four storage slots represent packed scene/view/Hi-Z input, compacted instances, indirect arguments, and statistics. The acceptance test waits for completion, reconstructs the backend result through the explicitly test-only readback seam, and requires an exact `CompareGPUDrivenResults()` match; normal recording performs no readback. Native queue/timeline separation, DX12/Metal execution, and target-host parity remain open gates; none is inferred from Linux Vulkan coverage.
