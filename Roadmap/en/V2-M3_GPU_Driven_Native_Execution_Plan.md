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

### Phase 1 -- Vulkan compute dispatch (Linux-verifiable here)

- Implement `VulkanCommandList::Dispatch` (`vkCmdDispatch`), mirroring the existing
  `VulkanCommandList::DrawIndirect` pattern (command pool/buffer already exist on this device).
- Write the first real compute shader: frustum + distance culling, matching
  `BuildGPUDrivenCommands()`'s CPU-reference semantics field-for-field (this is the shader
  `CompareGPUDrivenResults()` will validate against).
- Add a native-Vulkan test analogous to `renderer.contracts`'s `VerifyNativeBackend`, but calling
  `RecordGPUDrivenExecution` end-to-end (not just a bare triangle frame) and asserting
  `diagnostics.compute_dispatches`/`indirect_draw_calls` match the CPU reference's counts.
- Gate: `CompareGPUDrivenResults()` reports no mismatch between the Vulkan-executed output and the
  CPU reference, on this session's Linux host.

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
