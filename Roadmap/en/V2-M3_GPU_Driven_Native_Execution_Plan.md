# V2-M3 GPU-Driven Rendering — Native Backend Execution Plan

> Version: v1.0 | Status: proposed plan, not yet started | Updated: 2026-09-24 | Relates to:
> `Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md` §V2-M3

## 1. Purpose

V2-M3's gate has four checked items (portable command batching, no-readback contract diagnostics,
RenderGraph queue/barrier ownership, CPU-reference correctness comparison) and one unchecked item:
**native DX12/Vulkan/Metal target-tier parity on target hosts**. This document plans the work
needed to check that last box. It is a plan, not a milestone status update: nothing in this
document is implemented yet, and no roadmap progress percentage changes until the phases below
actually land and pass their gates.

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

## 5. Phased plan

### Phase 1a -- Vulkan compute dispatch, shape-level (done)

- ✅ Implemented `VulkanCommandList::Dispatch` (`vkCmdDispatch`), mirroring the existing
  `VulkanCommandList::DrawIndirect` pattern; wired `DeviceDiagnostics::compute_dispatches`
  aggregation in `VulkanDevice::Submit`, matching how `DrawCalls()`/`IndirectDraws()` already
  aggregate.
- ✅ Added `GPUDrivenPipelineTests.cpp::TestNormalPathOnVulkan`, which runs
  `RecordGPUDrivenExecution` end-to-end against a real `rhi::CreateDevice(Backend::Vulkan)` device
  (skipping cleanly via `IsBackendAvailable` where Vulkan isn't present) and asserts
  `diagnostics.compute_dispatches == 1 && diagnostics.indirect_draw_calls == 1 &&
  diagnostics.draw_calls == 1` and `diagnostics.readbacks == 0`, exactly mirroring the existing
  validation-backend assertion in the same file. Verified: `linux-development` (35/35 ctest) and
  `linux-sanitizers` (ASan+UBSan, 35/35 ctest).
- This closes the literal gap the plan opened with (`Dispatch` throwing on Vulkan) and proves the
  real `RecordGPUDrivenExecution` path -- not just an unrelated triangle-frame smoke test --
  genuinely dispatches and indirect-draws on real Vulkan hardware/driver.
- **What this does not yet prove**: `RecordGPUDrivenExecution`'s `Dispatch`/`DrawIndirect` calls
  take only plain counts (`candidate_count`, `indirect_command_count`) -- no scene data, view
  parameters, or output buffer are bound to the dispatch. The compute shader work below needs real
  GPU-visible input/output buffers, which is a bigger prerequisite than this plan originally
  assumed; see Phase 1b.

### Phase 1b -- prerequisite: buffer resources and compute-pipeline creation in the RHI (not started, needs confirmation)

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
dependency or CI change. **Not started; needs confirmation on the shape of the new API (buffer
lifetime/ownership model, upload path -- staging buffer vs. host-visible mapping, binding
model -- fixed slots vs. a general descriptor-set builder) before Phase 1's actual compute shader
work can begin.**

### Phase 2 -- Remaining compute stages on Vulkan

- Hi-Z occlusion (conservative test against the existing `HiZPyramid` contract), visible-instance
  compaction, material/mesh/LOD classification, and indirect-command generation, each as
  compute shaders, added incrementally with the same CPU-reference comparison gate per stage.
- RenderGraph integration: confirm the existing queue-ownership barrier tracking
  (`Engine/Renderer/README.md` §RenderGraph) correctly sequences these new compute passes against
  the graphics passes that consume their output, on real Vulkan queues (not just the validation
  device's logical tracking).

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
