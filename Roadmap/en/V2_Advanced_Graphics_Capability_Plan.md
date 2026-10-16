# V2 Advanced Graphics Capability Plan

Status: V2-GFX-1 in progress; capability admission foundation validated on Linux.
All five complete gates remain open.
Added 2026-10-16.

This is a normative V2 scope extension shared by the V2 Complete and AI implementation plans.
It brings the missing DirectX-era rendering capabilities into V2 delivery. V3 may extend these
systems but is no longer their first-delivery target. This document plans implementation; it
does not claim that planned native paths are available. Delivered contract changes are tracked below.

## V2-GFX-1 construction status

✅ Capability admission is the first delivered portable/Linux implementation slice: independent device support/tier,
enablement and backend implementation snapshots; guarded DX12 feature/Shader Model queries;
Vulkan core capability discovery; explicit preferred/fallback selection; PSO creation admission;
and complete capability requirements in PSO cache identity. Unknown support fails closed, numeric
versions distinguish SM 6.10 from 6.9, and native feedback tiers 90/100 map to 0.9/1.0.

Parallel recording, fence-retired worker descriptors and allocators, compiler/artifact matrix
integration, CPU/GPU benchmarks and all V2-GFX-2 through
V2-GFX-5 native execution remain open. No native advanced feature is marked complete by admission
tests or device hardware queries.

✅ Vulkan compute descriptor isolation is the next delivered prerequisite: command-list-owned
pools and immutable per-dispatch binding snapshots replace the shared mutable device set.
The native test compares 320 unique workloads in serial/four-worker recording modes, including
40 dispatches per list (crossing the 32-set pool boundary), final binding without dispatch,
reverse ordered submission after all workers join, completion-before-destruction and missing/stale
binding rejection. All output identities, classification data and indirect draw prefixes match
CPU references. The snapshot capability remains disabled/unimplemented for parallel recording;
RenderGraph scheduling and other backend worker ownership are not complete.

### Buffer dependencies and compiled barrier plans — 2026-10-17

✅ The next V2-GFX-1 prerequisite delivers explicit borrowed buffer read/write declarations,
texture/buffer RAW/WAR/WAW edges, read/read queue or state/access ordering, and immutable per-pass
barrier plans built by `Compile`. Whole-buffer dependencies map to real Vulkan memory barriers,
D3D12 tracked states/UAV barriers and Metal tracked encoder boundaries. Imported aliases are
rejected; buffers remain caller-owned. Same-state storage writes still emit WAW barriers.

Source branch: `feat/v2-graph-buffer-dependencies`, base `ee8b37d64c9342702b8ec176c6792f3148d79982`.
Beads: `nexora-45y.2.2`; parent `nexora-45y.2` and the full V2-GFX-1 gate remain open.

- `cmake --preset linux-development` and `cmake --build --preset linux-development`: passed.
- `ctest --preset linux-development` under Xvfb with native-required Slang 2026.18 SPIR-V,
  Mesa lavapipe 24.0.5 and Zig 0.14.0: **191/191 passed, zero failures/skips, 105.43 seconds**.
- Focused `ctest --preset linux-development -R '^renderer\.(graph_buffer_dependencies|contracts|v2_gpu_driven)$' --output-on-failure`:
  **3/3 passed, zero skips, 0.18 seconds**.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed,
  Monolithic/Minimal with LTO; required because the public C++ command-list interface changed.
- `clang-format --dry-run --Werror` on touched C++ files and `git diff --check`: passed.

The focused contract captures exact handles/access scopes/queues before callbacks, checks repeated
plan replay, same-state WAW, all-reader handoffs, cycles, invalid/duplicate declarations, stale
imports, callback failure containment and first-use transient ownership. Native GPU-driven
acceptance removes the fake texture dependency and explicit compute-to-draw edge, declares all four
buffers, performs two compute writes with WAW dependencies, derives the indirect-read edge and
requires an exact CPU/GPU comparison. Normal recording remains readback-free. Native smoke coverage
is wired into each required platform's `renderer.contracts`; this local run establishes Vulkan
software-device correctness only. Windows/D3D12 and macOS/Metal results require hosted/target-host
execution; no local GPU validation layer or physical-GPU performance claim is made.

Recording/submission remains serial and native logical queues still alias one synchronous queue.
Async queue-family transfers, worker lifetime/retirement, matched performance, broader buffer access
classes, transient buffer allocation and full parallel scheduling remain open. No advanced feature
capability is enabled, no shader/toolchain baseline changes, and the stable C ABI is unchanged.
See the [Renderer contract](../../Engine/Renderer/README.md#v2-graph-buffer-dependencies-and-compiled-synchronization)
and [RHI mapping](../../Engine/RHI/README.md#whole-buffer-access-dependencies-v2-gfx-1-prerequisite).

### Descriptor isolation evidence — 2026-10-16

Source branch: `feat/v2-parallel-command-recording`, based on merged capability foundation `4972c14b`.
Beads: `nexora-45y.2.1`; parent `nexora-45y.2` remains open.
The Linux native-required command is
`ctest --preset linux-development -R '^renderer\.(vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven)$' --output-on-failure`
with the same Slang 2026.18 artifacts and Mesa lavapipe 24.0.5 environment described below.
All four tests passed with zero skips (0.82 seconds); descriptor isolation took 0.27 seconds.
The full `cmake --preset linux-development`, `cmake --build --preset linux-development --parallel 4`
and `ctest --preset linux-development --output-on-failure` gate passed: 171 passed, 4 explicit
skips (three display tests plus this native test without artifacts), 175 total, 77.42 seconds.
The separate native-required run above executes the isolation test without skipping.
This is software-device correctness evidence, not a CPU-overhead or physical-GPU performance claim.
No Vulkan validation layer is installed locally; GPU validation and target-host gates remain open.
No SDK/compiler baseline or feature default changed. Pools currently retire with the single-use
list after synchronous native fence completion; asynchronous worker reuse remains a separate gate.

The initial hosted Linux push gate exposed an independent 2D native input timing assumption:
341/342 passed twice, with the new descriptor test passing each time; the identical PR head's
Linux gate passed. The product driver now waits for real movement and a checksum-valid SAVE
before OPEN, rather than inferring movement from a fixed delay. Five corrected native runs,
a missed-first-input recovery, an all-input-missing rejection and the local Zig-enabled full
**190/190 gate (zero skips/failures, 135.96 seconds)** pass. This retains all original native
assertions and adds exact restore comparison. See the
[2D acceptance follow-up](../../Engine/TwoD/evidence/Cooked-Services-Linux-2026-10-16/acceptance.md#movement-and-save-acknowledgement-follow-up).
Final hosted CI is still required for the corrected head.

### Acceptance evidence — 2026-10-16

Source branch: `feat/v2-graphics-capabilities`, based on `5f3da989`.
Beads: `nexora-45y.1`; remaining execution gates are `nexora-45y.2` through `nexora-45y.6`.

| Command / check | Observed result |
| --- | --- |
| `cmake --preset linux-development` | Passed; GNU 14.2, CMake 3.31.6, Ninja 1.11.1; isolated X11 development prefix in the local cache |
| `cmake --build --preset linux-development --parallel 4` | Passed, including native Vulkan source and the new capability test |
| `ctest --preset linux-development` | 0 failures: 171 passed, 3 display-dependent native Presentation tests skipped, 174 total; 75.62 seconds |
| `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping --parallel 3` | Passed, Monolithic/Minimal with LTO |
| Native-required `renderer.contracts`, `renderer.graphics_capabilities`, `renderer.v2_gpu_driven` | 3/3 passed, 0 skips; Mesa lavapipe 24.0.5, Slang 2026.18; 0.19 seconds |
| Changed Markdown/bilingual pairing, `git diff --check`, touched-file clang-format 18.1.8 check | Passed |

The production-toolchain test initially failed because `clang++` was absent; after preparing
Clang 19.1.7 the complete development suite passed. Tool dependencies were installed/extracted
outside the repository; no project SDK baseline or optional feature defaults changed.

The first Windows CI compile found a WinGDI `DeviceCapabilities` macro collision with the new
snapshot type. The type is now `GraphicsDeviceCapabilities`; the capability regression test also
loads Windows headers before the public RHI headers. This preserves the SDK macro for consumers
rather than undefining it. The failure enum likewise uses `NoFailure` instead of X11's `None`,
and Linux builds with Window enabled compile the admission header after Xlib. Final platform CI
acceptance remains required for the corrected head.

The native test uses Slang's existing `-target spirv -profile glsl_450
-fvk-use-entrypoint-name` contract with `vertexMain`/`fragmentMain` and `computeMain` artifacts,
`NEXORA_REQUIRE_NATIVE_BACKENDS=1`, the two `NEXORA_SLANG_*SPIRV_PATH` variables and a lavapipe ICD.
It runs `ctest --preset linux-development -R '^renderer\.(contracts|graphics_capabilities|v2_gpu_driven)$' -V`.
The retained capability output identifies backend Vulkan, software device, vendor 65541, device 0,
driver 1; compute/indirect are supported, enabled and implemented at 1.0. Parallel recording and
tessellation report support at 1.0 with no implementation/enablement. All other advanced features
remain unknown and unavailable. Pipeline admission rejects every unimplemented advanced path
before allocating a native PSO. The existing full GPU-driven reference comparison still passes.

Portable tests additionally verify independent tier encodings (including feedback 90/100 and DXR
1.2), unknown/unsupported/disabled/unimplemented failures, explicit fallback exhaustion, SM 6.10
ordering and cached baseline PSOs failing to authorize an advanced requirement. A Windows or Apple
native execution result and physical-GPU performance were not produced in this Linux environment;
the existing platform CI/target-host gates remain required. The SwiftShader device initially
available in the environment was rejected for lacking existing Vulkan baseline features; those
requirements were not relaxed to get a pass.

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

All complete gates remain **open**, including capability-dependent paths. A runtime fallback makes a device
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
