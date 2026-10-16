# V2 Advanced Graphics Capability Plan

Status: Planned; all V2-GFX gates below remain open. Added 2026-10-16.

This is a normative V2 scope extension shared by the V2 Complete and AI implementation plans.
It brings the missing DirectX-era rendering capabilities into V2 delivery. V3 may extend these
systems but is no longer their first-delivery target. This document plans implementation; it
does not claim native support or change current RHI contracts.

## Baseline and scope

The source review at `5201d9f6e02d2a24406004174a50f61ad9cebf9b` found native DX12 queues,
command lists, descriptor heaps, traditional resource barriers, graphics/compute PSOs, compute
dispatch and indirect drawing. Preserve these implementations rather than count them as new work.
The Windows DXIL build uses SM 6.6 and SM 6.0 profiles; native Presentation also uses SM 5.0.
None of those facts establishes DX12 Ultimate or newer shader-feature acceptance.

Programmable shaders already exist. Unified shader architecture is a hardware property. Separate
DX9/DX10/DX11 API backends are outside this extension; their relevant missing rendering capability,
tessellation, is included below. Lower CPU overhead requires measurements against the existing
serial submission path, not an API-version claim.

## Delivery gates

All rows are **planned**, including capability-dependent paths. A runtime fallback makes a device
usable but does not complete the accelerated implementation or its target-host gate.

| Gate | Planned V2 work | Dependencies | Acceptance and fallback |
| --- | --- | --- | --- |
| V2-GFX-1 | Capability/SDK/compiler matrix; parallel command recording; CPU-overhead benchmark | Existing V2-M3 contracts | Query independent feature tiers and shader targets. Give each recording worker exclusive command allocator/list ownership; retire allocators, descriptors and resources by completion fence. RenderGraph owns dependency order and submission. Compare serial/parallel output and run validation, lifetime stress and CPU/GPU timing. Keep serial recording selectable. |
| V2-GFX-2 | Tessellation and Enhanced Barriers | V2-GFX-1 capability and lifetime contracts | Add patch topology, hull/domain stages, reflection and pipeline/cook keys; verify tessellated output against a bounded reference, with pre-tessellated mesh fallback. Map graph access/sync/layout to Enhanced Barriers, including UAV, copy, aliasing and queue handoff cases; compare against the legacy barrier path under GPU validation. Keep both barrier paths. |
| V2-GFX-3 | DXR baseline through DXR 1.2; optional Shader Execution Reordering (SER) | V2-GFX-1, compatible barrier implementation from V2-GFX-2 | Implement BLAS/TLAS build/update/compaction and lifetimes, RT pipeline/shader tables and native ray dispatch; validate deterministic scenes for shadow/reflection/AO. Query actual RT tiers, document the official SDK mapping for the requested DXR 1.2 tier, and test the features that distinguish it. Add SER as a separately queried/compiler-gated path and compare reordered/unreordered results. Raster lighting remains available. |
| V2-GFX-4 | Mesh Shader, Variable Rate Shading (VRS), Sampler Feedback; DX12 Ultimate coverage | V2-GFX-1; existing V2-M3 compute/indirect baseline | Implement native amplification/mesh stages and meshlet submission, VRS tier-dependent rate/image controls, and feedback-map write/resolve/streaming consumption. Verify culling/geometry against indexed-indirect fallback; test VRS quality/error bounds with full-rate fallback, and residency/mip correctness against feedback-disabled streaming. Report DXR, Mesh Shader, VRS and Sampler Feedback tiers independently; claim Ultimate coverage only after all four native paths pass. |
| V2-GFX-5 | Work Graphs, Shader Model 6.9, Shader Model 6.10 Preview | V2-GFX-1 through V2-GFX-4 and pinned compatible toolchains | Implement Work Graph state objects/program selection, backing-memory lifetime, node-record ABI and native graph dispatch. Compare bounded node fan-out/output against compute+indirect scheduling. Compile and execute focused SM 6.9 cases and the requested SM 6.10 Preview linear-algebra and variable group-shared-memory cases on supported devices. Record the exact published specification/feature names; unsupported or unavailable preview functionality stays open. Preserve compute+indirect and lower-SM shader variants. |

Execution order is V2-GFX-1, then V2-GFX-2, then V2-GFX-3/V2-GFX-4, then V2-GFX-5.
V2-M3 remains independently incomplete until its existing native parity gates pass. V2-GFX gates
extend V2 rendering acceptance and feed V2-M12 hardening; they do not renumber or reopen accepted
V2-M0 through V2-M12 milestones. V2 final acceptance additionally requires these new gates.
The historical 6/13 milestone ratio excludes this extension and is not an expanded-scope percentage.

## Capability and architecture contracts to implement

- Keep native APIs private. RHI exposes backend-neutral feature availability, supported tiers,
  shader-stage/pipeline descriptors and explicit unsupported results; Renderer chooses paths.
- Distinguish hardware support, OS/driver readiness, SDK/compiler support, built artifacts,
  implemented backend path and accepted target-host evidence. Do not infer a feature from an
  aggregate DirectX version or from successful shader compilation alone.
- Use independent opt-in features/profiles for advanced paths and preview toolchains. Preview
  support must not silently raise the baseline Windows SDK, Shader Model or device requirements.
  Pin Windows/Agility SDK, DXC, Slang, driver, OS, GPU and relevant specification versions.
- Shader cook/cache keys include target profile, feature requirements, stage, compiler version,
  reflection/layout identity and variant generation. Reject incompatible artifacts before submit.
- RenderGraph remains the sole owner of resource dependency and synchronization decisions.
  Parallel recording cannot reorder observable dependencies or share mutable recording state.
  Define per-worker descriptor allocation and fence-based retirement before enabling concurrency.
- Vulkan/Metal use equivalent native features only where supported and implemented; otherwise
  select and test the documented fallback. Do not promise API-name or advanced-tier parity.
- Keep the existing stable C ABI compatible. Any gameplay/editor feature controls use
  backend-neutral descriptors; plugins disabled must not impose shipping dependencies or cost.

## Evidence required before completion

For each V2-GFX gate, retain the tested commit, commands, toolchain/device matrix, capability
queries, selected path, native API execution evidence, deterministic reference comparisons,
validation output, fallback results and remaining limitations under the relevant test evidence.
Negative cases must exercise unsupported tiers, missing artifacts, disabled plugins and stale or
in-flight resources. A skipped native test remains pending; a portable mock is foundation evidence.

Parallel recording benchmarks report CPU recording/submission median and p95, draw/dispatch
counts, GPU frame time and memory under the same workload, device, build and warm-up policy.
Define a measurable target and allowed regression budget before performance acceptance; keep the
serial path for small workloads where parallel recording loses. Never infer reduced CPU overhead.

Implementation changes require the full Linux development gate and relevant Windows hardware/SDK
gates; linkage changes also require Linux Shipping. Linux validation does not certify DX12 features.
Add native Windows GPU validation and fallback coverage to CI/target-host evidence as each path
is delivered. Preview acceptance is conditional on a published toolchain and runnable target;
unavailable features cannot be marked complete through compilation stubs or fallback-only tests.

Only mark an item with the green completion marker after its implementation and acceptance
evidence exist. Update both language editions, the V2 plans and roadmap index together.

## Source references

- [V2 Complete Plan](Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md)
- [V2 AI Implementation Plan](Cross-platform_3D_Engine_V2_AI_Implementation_Technology_and_System_Plan_v1_2.md)
- [Existing V2-M3 native execution plan](V2-M3_GPU_Driven_Native_Execution_Plan.md)
- [RHI contracts](../../Engine/RHI/README.md)
- [Renderer contracts](../../Engine/Renderer/README.md)
- [Shader library and native Presentation scope](../../Shaders/README.md)
