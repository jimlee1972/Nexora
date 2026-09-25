# V2-M3 GPU-Driven Rendering — Native Backend Execution Plan

> Version: v1.0 | Status: in progress; Linux Vulkan Phase 2 acceptance pending | Updated: 2026-09-25 | Relates to:
> `Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md` §V2-M3

## 1. Purpose

V2-M3's gate has four checked items (portable command batching, no-readback contract diagnostics,
RenderGraph queue/barrier ownership, CPU-reference correctness comparison) and one unchecked item:
**native DX12/Vulkan/Metal target-tier parity on target hosts**. This document plans the work
needed to check that last box. Phases 1a and 1b are complete and Phase 2 has a Linux Vulkan implementation whose native comparison evidence is still pending; the milestone remains open and no roadmap progress percentage changes until all remaining phases land and pass their gates.

## 2. Current baseline (verified against source, not just roadmap prose)

`Engine/Renderer/src/GPUDrivenPipeline.cpp`'s `RecordGPUDrivenExecution()` is the single call site
that drives the real (non-test) GPU-driven path: it calls `compute_commands.Dispatch(...)` for
culling/compaction and `graphics_commands.DrawIndirect(...)` for the compacted draw. Both are
virtual methods on `rhi::CommandList` (`Engine/RHI/include/Nexora/RHI/Device.h`) whose *base*
implementations unconditionally throw (`"compute dispatch is unsupported"` /
`"indirect drawing is unsupported"`) unless a backend overrides them. Verified per backend:

| Backend | `Dispatch` | `DrawIndirect` | Evidence |
| --- | --- | --- | --- |
| `ValidationDevice` (portable CPU reference) | ✅ overridden | ✅ overridden | `renderer.v2_gpu_driven` exercises the full culling/Hi-Z/compaction/indirect-generation pipeline deterministically. |
| `VulkanDevice` | ❌ not overridden (throws) | ✅ overridden (`vkCmdDrawIndirect`) | `renderer.contracts` (`Tests/Renderer/RendererTests.cpp::VerifyNativeBackend`) exercises `DrawIndirect` on real Linux Vulkan through a minimal triangle frame -- **not** through `RecordGPUDrivenExecution`, and never calls `Dispatch`. |
| `D3D12Device` | ❌ not overridden (throws) | ❌ not overridden (throws) | Zero occurrences of either symbol in `Engine/RHI/src/D3D12Device.cpp`. |
| `MetalDevice` | ❌ not overridden (throws) | ❌ not overridden (throws) | Zero occurrences of either symbol in `Engine/RHI/src/MetalDevice.mm`. |

In short: the full GPU-driven pipeline (`RecordGPUDrivenExecution`) has **only ever run against the
CPU reference**. Vulkan has real indirect-draw evidence from an unrelated, simpler smoke test.
Compute dispatch has never executed on any native backend. D3D12 and Metal cannot execute any part
of this path today -- calling into either throws immediately. This matches
`Engine/Renderer/README.md`'s own statement: "Native Vulkan compute pipelines, native
queue/timeline integration, DX12/Metal execution, and target-host parity remain open gates; none
is inferred from validation-backend or Vulkan-indirect coverage." This plan is the first attempt to
write that evidence down as an ordered set of engineering steps.

## 3. Scope and non-goals

In scope: implementing `Dispatch`/`DrawIndirect` (where missing) on all three native backends,
writing the actual compute shaders the culling/Hi-Z/compaction/indirect-generation stages need,
wiring `RecordGPUDrivenExecution` to run unmodified against each native device, and target-host
validation.

Out of scope (separate future work, not touched by this plan): V2-M3's later "then add" items
(temporal upscaler interface, compute skinning, meshlet metadata) -- those follow after target-tier
parity, not before it; async compute queue scheduling beyond what `RenderGraph`'s existing
ownership-barrier tracking already covers; any V2-M4+ milestone.

## 4. Dependencies and constraints

No new third-party dependency is introduced by this plan -- it uses the Vulkan/D3D12/Metal APIs
`Engine/RHI` already links against, plus the Slang shader toolchain already gated behind
`NEXORA_ENABLE_SLANG` (see `build.shader_contract`, `Tools/Build/ValidateShaderContract.cmake`).
New compute shader sources go under `Shaders/` alongside the existing `Triangle.slang` and must
pass the same shader-contract validation. This keeps the work inside the "no new dependency, no CI
change" category CLAUDE.md asks to be explained before crossing -- it is not crossed here.

Target-host acceptance for Windows/DX12 and macOS/Metal cannot be produced from this cloud
session (no MSVC, no Apple host); those phases are implementable and locally testable for Vulkan
here, but their DX12/Metal counterparts need to be run and evidenced on native hosts, same as
`Window_Presentation_Roadmap.md`'s WP-M1/WP-M2 already established for the windowing side.

**Operational note, learned the hard way in Phase 1a**: `cmake --preset linux-development` on its
own (this repo's default) does **not** enable Slang, and `VulkanDevice`'s constructor unconditionally
requires a compiled SPIR-V artifact via `NEXORA_SLANG_SPIRV_PATH` -- without it, every native-Vulkan
code path silently reports itself unavailable via `IsBackendAvailable` and every test guarded by it
silently skips with a passing exit code. Genuinely exercising any native-Vulkan work in this plan
requires `-DNEXORA_ENABLE_SLANG=ON` (matching `build.yml`'s actual desktop-job invocation) with a
`slangc` binary on `PATH` (this session installed `shader-slang/slang` v2026.18 the same way
`build.yml`'s "Install Slang (Linux and macOS)" step does), *and* the relevant CTest target needs
the same `NEXORA_SLANG_SPIRV_PATH`/`NEXORA_REQUIRE_NATIVE_BACKENDS` environment properties
`renderer.contracts` already has in `Tests/Renderer/CMakeLists.txt` -- a new test target does not
inherit them automatically. A near-instant runtime for a test that claims to exercise real Vulkan
is a reliable tell that it silently skipped instead.

## 5. Phased plan

### ✅ Phase 1a -- Vulkan compute dispatch, shape-level (done, corrected)

- ✅ Implemented `VulkanCommandList::Dispatch` (`vkCmdDispatch`), mirroring the existing
  `VulkanCommandList::DrawIndirect` pattern; wired `DeviceDiagnostics::compute_dispatches`
  aggregation in `VulkanDevice::Submit`, matching how `DrawCalls()`/`IndirectDraws()` already
  aggregate.
- ✅ Fixed a real, previously-latent bug found while getting genuine verification working:
  `VulkanDevice::CreateCommandList` unconditionally rejected `QueueType::Compute`
  (`"Vulkan triangle backend only supports graphics queue"`), even though the underlying command
  pool is created against the one graphics-capable queue family regardless of the requested queue
  type and that family supports compute on every host this backend targets. Relaxed the check to
  accept `Graphics`/`Compute` and reject only `Copy` (genuinely unsupported -- no separate transfer
  queue family is selected).
- **Correction to this plan's own earlier claim**: an initial version of this phase added a test
  (`TestNormalPathOnVulkan`) that ran `RecordGPUDrivenExecution` end-to-end against real Vulkan and
  claimed (in the PR that introduced it) to have "confirmed it exercises the real Vulkan path...
  not silently skipped." That claim was wrong. The local build used to check it had
  `NEXORA_ENABLE_SLANG` off (this repo's default), under which `VulkanDevice`'s constructor -- which
  unconditionally requires the `NEXORA_SLANG_SPIRV_PATH` environment variable and throws otherwise
  -- always failed, so `IsBackendAvailable(Vulkan)` always returned `false` and the test always
  silently skipped. The exit code looked identical to a genuine pass either way, which is precisely
  how this went unnoticed initially. Root-caused and fixed properly: installed the pinned Slang
  toolchain this repo's CI uses (`shader-slang/slang` v2026.18) into this session, reconfigured with
  `-DNEXORA_ENABLE_SLANG=ON` (matching `build.yml`'s actual desktop-job invocation, which this
  session's plain `cmake --preset linux-development` does not match by default), and added the same
  `NEXORA_SLANG_SPIRV_PATH`/`NEXORA_REQUIRE_NATIVE_BACKENDS` CTest environment properties to
  `renderer.v2_gpu_driven` that `renderer.contracts` already had (only that one test had them).
  `renderer.contracts` went from ~0.00s to ~0.1s once genuinely exercising Vulkan -- a useful tell
  for "was this actually skipped" that's worth checking on any future native-backend claim in this
  repo.
- **With genuine Vulkan execution turned on, `TestNormalPathOnVulkan` immediately segfaulted** --
  not gracefully, a real crash, confirmed via `gdb` to be inside Mesa's `libvulkan_lvp.so` (the
  software Vulkan driver this sandbox uses), on a driver worker thread, during queue execution.
  Root cause: `RecordGPUDrivenExecution`'s `Dispatch` call has no compute pipeline bound --
  correct per its "shape-only" design (see §2), and harmless against `ValidationDevice`, which
  doesn't model pipeline-binding state -- but `vkCmdDispatch` with no bound compute pipeline is
  undefined behavior per the Vulkan spec, and this sandbox has no validation layers installed to
  turn that into a clean error instead of a driver crash. Since there is no way to create *any*
  compute pipeline yet (Phase 1b), there is no way to make this call safely today. **Removed
  `TestNormalPathOnVulkan`** and replaced it with `TestDispatchPreconditionsOnVulkan`, which only
  exercises `Dispatch`'s own precondition checks (e.g. rejecting a zero group count) -- these throw
  before recording anything, so they never reach the driver. A real end-to-end native dispatch test
  has to wait for Phase 1b to provide something to bind.
- Verified (with Slang genuinely enabled this time): `linux-development` (36/36 ctest, including
  `build.shader_crosscompile` which only exists under `NEXORA_ENABLE_SLANG`). Also re-verified the
  plain `linux-development` preset without Slang (this repo's default) still degrades cleanly
  (35/35, the extra shader-crosscompile test absent as expected).
- `linux-sanitizers` with Slang on is 35/36: `renderer.contracts` now fails under ASan with a
  112-byte/2-allocation leak, stack entirely inside `libNexoraCore.so` on a `core::JobSystem`
  worker thread (`asan_thread_start` -> `start_thread`, no Vulkan/RHI symbol anywhere in the
  trace). Confirmed this is pre-existing and unrelated to this phase's changes, not something this
  work introduced: reproduces identically (`git stash` back to the prior commit, rebuild, rerun)
  with none of Phase 1a's `Dispatch`/`CreateCommandList` changes present. It was never caught
  before simply because this is the first time in this session `renderer.contracts` has run under
  ASan *and* genuinely exercised real Vulkan at the same time (every earlier sanitizer run in this
  session also had Slang off). Left unfixed here -- it's a `core::JobSystem` thread-lifecycle issue,
  a different subsystem than V2-M3's RHI/Renderer scope -- but flagged here rather than silently
  ignored, since it's a real, reproducible ASan finding a future session should pick up.
- **What Phase 1a actually established**, stated precisely this time: `Dispatch` is implemented and
  its precondition checks are real-Vulkan-verified; `CreateCommandList(Compute)` now works; the
  `renderer.contracts` triangle-frame test was already genuinely exercising `DrawIndirect` on real
  Vulkan once Slang is on (that part of the original roadmap claim holds). Nothing about
  `RecordGPUDrivenExecution`'s actual culling/compute output has been verified against real
  hardware -- that remains entirely Phase 1b + Phase 2 work.

### ✅ Phase 1b -- buffer resources and compute-pipeline creation in the RHI (done)

> **Update (2026-09-25):** the paragraphs below describe the gap as found while starting Phase 1a.
> Since then, a parallel effort (`Editor_ImGui_Integration_Plan.md`, driven by a different agent
> session) has added real `Device::CreateBuffer`/`WriteBuffer`/`DestroyBuffer`,
> `CommandList::BindVertexBuffer`/`BindIndexBuffer`/`BindTexture`, `DrawIndexed`, `SetScissor`, and
> a proper submission timeline (`Submit` returns a completion value; `CompletedSubmissionValue`/
> `WaitForSubmission`) to `Engine/RHI/include/Nexora/RHI/Device.h` -- verified present on `main` as
> of this update. That closes the "no way to create/upload/destroy a buffer at all" half of this
> gap. It does **not** close the compute-specific half: `PipelineDescriptor` is unchanged (still no
> compute path, still hardcoded `vkCreateGraphicsPipelines` in `VulkanDevice::CreatePipeline`), and
> there is still no way to bind a buffer as a compute shader's storage resource (only
> vertex/index/texture binding exists, all graphics-oriented -- unsurprising, since that work's
> purpose was ImGui rendering, not compute culling). So Phase 1b now has a narrower, real remaining
> scope: compute pipeline creation and compute-resource (descriptor-set) binding, reusing the new
> buffer create/write/destroy primitives rather than re-inventing them. The rest of this section is
> kept as-written for the record of what was actually verified at the time, not edited to look like
> a correct prediction in hindsight.

Verified against source while starting Phase 1a: the RHI has **no way to create, upload to, bind,
or read back a GPU buffer**, and `Device::CreatePipeline` **unconditionally builds a graphics
pipeline** (hardcoded vertex+fragment stages, `vkCreateGraphicsPipelines`) with no compute path.
Specifically:

- `Types.h` already declares `BufferHandle`/`BufferTag`/`BufferDescriptor`/
  `BindingType::StorageBuffer` -- but nothing in `Device` or `CommandList` ever creates, destroys,
  binds, or maps one. These are unused scaffolding, presumably put in place for exactly this work
  and never finished.
- `VulkanDevice.cpp` already loads the raw Vulkan function pointers this needs
  (`vkCreateBuffer`, `vkGetBufferMemoryRequirements`, `vkBindBufferMemory`, `vkMapMemory`/
  `vkUnmapMemory`, `vkCreateDescriptorSetLayout`, `vkCreateDescriptorPool`,
  `vkAllocateDescriptorSets`, `vkUpdateDescriptorSets`) and uses them internally for exactly one
  fixed-purpose object: a single 64-byte uniform buffer bound at descriptor set 0/binding 0 for the
  hardcoded triangle pipeline. None of this is exposed publicly or general enough to bind an
  arbitrary storage buffer for a compute shader's scene/view/output data.
- A real culling compute shader needs, at minimum: a read-only structured/storage buffer of scene
  object data, a storage buffer for compacted-instance output, a storage buffer for indirect
  command output, and a small uniform buffer for view/frustum parameters -- several buffers of
  varying type and size, not the one fixed uniform binding that exists today.

Closing this gap means extending the shared `rhi::Device`/`rhi::CommandList` abstract interface
(`Engine/RHI/include/Nexora/RHI/Device.h`) with buffer create/destroy/upload and a
compute-resource-binding mechanism, and adding a compute pipeline creation path alongside the
existing graphics one. Because these are pure-virtual additions to a shared interface, **every**
backend (`ValidationDevice`, `VulkanDevice`, `D3D12Device`, `MetalDevice`) needs at least a
minimal implementation for the build to keep compiling, even though only Validation and Vulkan
need to work correctly right now. This is a genuinely separate, foundational piece of work -- not
"write one shader" -- and is exactly the kind of RHI-wide interface change this repository's
standing rules ask to be discussed before starting, even though it introduces no new third-party
dependency or CI change. **Completed with a backend-neutral compute pipeline kind, four fixed storage-buffer slots, host-visible uploads, and an explicitly test-only bounded readback seam. The fixed slots keep this phase narrow; a general descriptor builder remains future work.**

### Phase 2 -- Full compute stages on Vulkan (acceptance pending)

> **Update (2026-09-25): implementation complete; native acceptance pending.** The shader and native test cover every Phase 2 stage, but Phase 2 is not marked ✅ until the Slang-enabled Linux Vulkan test actually runs and `CompareGPUDrivenResults()` passes. Shader compilation, a non-throwing dispatch, and diagnostics counters are not acceptance evidence.

- `GPUDriven.slang` now executes frustum rejection, distance rejection and LOD selection,
  conservative max-depth Hi-Z testing (including relaxed and invalidated policies), visible-instance
  compaction, deterministic material/mesh/LOD classification, and indirect-command generation.
- The Linux native acceptance packs the same scene, view, LOD thresholds, and Hi-Z pyramid used
  by `BuildGPUDrivenCommands()`, reads back only after completion, reconstructs the backend result,
  and requires `CompareGPUDrivenResults()` to match every instance, command, and statistic.
- The acceptance workload executes through RenderGraph compute and graphics passes and verifies
  both ownership transfers before issuing native indirect drawing. Normal execution remains
  readback-free; the bounded readbacks exist only in the correctness gate.
- The deterministic single-invocation kernel is deliberately an acceptance implementation: it
  is intended to prove the complete native semantics without subgroup/atomic ordering differences. Parallel scan,
  radix classification, and production-scale performance tuning remain optimization work and do not
  alter the result contract.

### Phase 3 -- D3D12 backend

- Implement `Dispatch` and `DrawIndirect` on `D3D12Device`'s command list (currently absent
  entirely): `ID3D12GraphicsCommandList::Dispatch` and `ExecuteIndirect` with a command signature
  matching the indirect-buffer layout `BuildGPUDrivenCommands()` already produces.
- Port the compute shaders from Phase 1/2 (Slang already targets multiple backends per the
  existing shader-contract validation; confirm HLSL/DXIL output needs no stage-semantic changes).
- This phase's actual execution and `CompareGPUDrivenResults()` gate can only be run on a Windows
  host -- out of this cloud session's reach. The code and shaders can be written and reviewed here;
  the pass/fail evidence cannot.

### Phase 4 -- Metal backend

- Implement `Dispatch` and `DrawIndirect` on `MetalDevice` (currently absent entirely):
  `dispatchThreadgroups`/`dispatchThreads` and `drawIndexedPrimitives(indirectBuffer:)`.
- Same shader-porting and target-host-only-evidence caveat as Phase 3, this time for macOS.

### Phase 5 -- Target-tier parity closeout

- With all three native backends executing the identical pipeline and passing
  `CompareGPUDrivenResults()` on their respective target hosts, check V2-M3's remaining gate item
  and update `Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md`'s gate checklist and progress
  percentage to reflect it -- not before.

## 6. Validation and Definition of Done

- Every new native code path is proven against `CompareGPUDrivenResults()`, not merely "compiles
  and runs without throwing."
- Vulkan phases (1-2) are fully verifiable on this repository's Linux CI/cloud gate.
- D3D12 (Phase 3) and Metal (Phase 4) require target-host runners; this plan does not claim their
  acceptance from Linux-only evidence, matching this repository's standing rule against claiming
  unverified platform coverage.
- No change to the CPU-reference (`BuildGPUDrivenCommands()`, `CompareGPUDrivenResults()`) semantics
  is in scope -- backends conform to it, it does not change to accommodate a backend.

## 7. Risks

- Compute shader correctness bugs are easy to introduce and hard to spot without the
  `CompareGPUDrivenResults()` gate catching them immediately; each stage should land with its
  comparison test in the same change, not deferred.
- `ExecuteIndirect`'s command-signature setup on D3D12 is a common source of subtle
  layout-mismatch bugs; the indirect-buffer layout must be pinned down once (Phase 1, Vulkan) and
  reused verbatim rather than redefined per backend.
- Async/queue-ownership behavior is the one area where the portable contract's "logical tracking"
  and real hardware queues can diverge; Phase 2's RenderGraph integration step exists specifically
  to catch that before D3D12/Metal work begins on top of an unverified assumption.
