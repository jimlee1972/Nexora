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
the selected queue's compute support, enabled indirect support and queried but disabled
tessellation. Metal reports the existing compute/indirect implementation; unqueried advanced
features stay unknown. No advanced path, including parallel recording or SER, is enabled by this
foundation. SER is not inferred from RT tier or Shader Model.

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

Each Vulkan command list owns its compute descriptor pools. `BindStorageBuffer` stores only
list-local handles; `Dispatch` revalidates all four canonical slots, allocates a fresh set and
copies the complete binding snapshot before recording. Sets are never updated or reused after
recording, so later rebinding (including binding without dispatch) cannot redirect earlier work.
Missing or stale bindings throw `std::logic_error` before descriptor allocation or dispatch.
Pools grow in blocks of 32 sets; recorded dispatches retain their sets until list destruction.

A command list has one recording owner at a time. Independent compute lists may record concurrently
against pre-created immutable pipelines and disjoint buffers. The device and referenced resources
must outlive all lists; callers serialize resource creation, upload, mutation and destruction against
recording/submission. Submission remains caller-serialized and synchronous in this backend; pool
and command allocator destruction happens after `Submit` has waited for its native completion fence,
or when a never-submitted list is abandoned. Never destroy a list while `Submit` is using it.
This is a prerequisite for broader worker scheduling, not an asynchronous retirement implementation.

`renderer.vulkan_descriptor_isolation` compares 320 unique dispatch results against the CPU
reference in serial and four-worker recording modes, rebinds after every dispatch, crosses pool
boundaries and submits only after every worker joins. It also rejects missing/stale bindings and
checks no worker submission/readback. With no Vulkan device/artifacts it reports an explicit skip;
Slang-enabled native builds require execution. Graph-level parallel execution, DX12/Metal worker
lifetimes, GPU validation-layer evidence and matched physical-GPU performance remain open.
`ParallelCommandRecording` remains disabled/unimplemented in the capability snapshot.

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

The portable Slang release does not ship `dxcompiler` for Linux or macOS, so `slangc -target dxil`
cannot run there (`error[E00100]: failed to load downstream compiler 'dxc'`) -- this was confirmed
against slangc 2026.18, not assumed. DXIL is therefore only compiled and validated on Windows,
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
`ReadBufferForTesting` API and is counted independently; production recording remains readback-free. `ReadTextureForTesting` is the equivalent test-only texture seam: it waits for device idle, copies the whole 4-byte-per-pixel texture into tightly packed host memory, restores the texture's recorded layout, and counts as a readback. Vulkan and D3D12 implement buffer readback for their native acceptance gates; other backends throw `std::logic_error` until their own native evidence exists, and depth textures are rejected.
D3D12 now records compute dispatch and canonical-stride `ExecuteIndirect`, accounts for both in
diagnostics, and obtains its compute PSO bytecode from the Windows DXIL artifact. Its buffer
path keeps upload staging separate from default-heap UAV-capable GPU storage, binds the four
fixed shader slots (`t0`, `u1`, `u2`, `u3`), records the required copy/state transitions, and
provides test-only buffer readback. On 2026-10-02, the local Windows NVIDIA GTX 960 host passed
`renderer.v2_gpu_driven` with an SDK `dxc`-generated `sm_6_0` artifact and an exact
`CompareGPUDrivenResults()` match. Native queue/timeline separation, Metal compute/indirect
execution, and full target-host parity evidence remain open before V2-M3 can be accepted.

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
destruction and submission against recording. Plans do not make D3D12 shared upload/state mutation
safe for parallel recording. All C++ modules must rebuild for the extended virtual interface;
the stable C ABI and shader artifact/profile requirements are unchanged.
