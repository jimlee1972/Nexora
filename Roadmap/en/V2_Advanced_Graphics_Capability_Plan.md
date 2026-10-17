# V2 Advanced Graphics Capability Plan

Status: V2-GFX-1 and bounded V2-GFX-2 implementation in progress; full acceptance remains open.
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

General parallel recording, fence-retired worker descriptors and allocators, compiler/artifact matrix
integration, complete CPU/GPU budgets and all five full gates remain open. The first bounded V2-GFX-2
native tessellation slice is recorded separately below; other advanced native execution remains open. No native advanced feature is marked complete by admission
tests or device hardware queries.

✅ Vulkan compute descriptor isolation is the next delivered prerequisite: command-list-owned
pools and immutable per-dispatch binding snapshots replace the shared mutable device set.
The native test compares 320 unique workloads in serial/four-worker recording modes, including
40 dispatches per list (crossing the 32-set pool boundary), final binding without dispatch,
reverse ordered submission after all workers join, completion-before-destruction and missing/stale
binding rejection. All output identities, classification data and indirect draw prefixes match
CPU references. The snapshot capability remains disabled/unimplemented for parallel recording;
General RenderGraph scheduling and other backend worker ownership are not complete.

### Retired Vulkan descriptor pool storage — 2026-10-17

✅ The opt-in allocation cache additionally retains up to two reset native descriptor pools per idle
entry (eight command allocations, sixteen idle descriptor pools). A live list exclusively consumes
prewarmed pools before growth and allocates a fresh immutable set per dispatch. Command-pool reset
precedes descriptor reset/transfer; reset failure, extra pools or cache overflow destroy storage.
Constructor failure and shutdown account for every acquired pool. Query pools remain per-list;
no extra waits, SDK/module dependency, stable C ABI or asynchronous queue path is added.

The native oracle adds96 unique workloads crossing three pools, abandon/two-pool retention,
re-record/overflow, held-completed-list ownership and192 CPU comparisons. Existing native
serial/four-worker and failed/cancelled replay coverage remains active. JSON reports native
command/descriptor creation/acquisition and idle counts, with fresh/default versus warmed/reuse
smoke checks. Beads `nexora-45y.2.14`; source `feat/v2-vulkan-descriptor-pool-reuse`.
Slang Development configure/build and Xvfb/native-required full CTest **237/237 pass, zero skips/
failures,253.15 seconds** (Showcase on, graphical shell off). Focused timing/native reuse/report
**7/7 pass,1.14 seconds**; Shipping Monolithic/Minimal LTO passes. Khronos1.3.275 object/synchronization
validation **10/10 pass,16.13 seconds**,25 loader-confirmed insertions and zero Validation
Error/VUID/SYNC-HAZARD markers. Touched formatting/diff checks pass. The extended compute oracle
creates13/reuses196 command allocations and creates11/acquires368 descriptor pools, ending with
8 idle command and8 idle descriptor pools; held-list, three-pool overflow and192 extra CPU
comparisons pass. Independent21x40 runs verify3,360 outputs and21 native GPU samples per mode/
policy; measured descriptor creation/acquisition changes from **168/0** to **0/168**, with8 idle
pools for the warmed four-list workload. No physical performance or memory-budget claim.
Hosted platform/ABI and native Vulkan gates remain required. Prior `.2.13` evidence covers the initial command-only cache; this slice
extends retired native storage without sharing snapshots. Full asynchronous/parallel/performance
acceptance and physical memory budgets remain open.

### Timing, measurement and bounded reuse integration — 2026-10-17

The follow-up integrates `.2.11` native timestamps, `.2.12` matched recording measurements and
`.2.13` bounded Vulkan allocator reuse on the PR628 prerequisites. Additional Vulkan
`--benchmark-reuse` evidence reports explicit cache admission and measured per-mode native pool
creation/acquisition counts, comparing the same workloads with fresh/default allocations. The
native report smoke checks warmed cache acquisitions and no fresh allocations, while ordinary
mode retains no cache and allocates fresh pools. ✅ Integrated Slang Development configure/build and Xvfb/native-required full CTest pass
**213/213, zero skips/failures, 126.86 seconds** (Showcase/graphical shell off; Slang/Zig/native Vulkan
on). Focused six timing/measurement/reuse targets **6/6 pass, 1.09 seconds**. Shipping configure/build
passes (Monolithic/Minimal, LTO). Khronos1.3.275 object/synchronization validation **10/10 pass,
10.32 seconds**, 25 loader-confirmed insertions, zero Validation Error/VUID/SYNC-HAZARD markers.
Touched formatting and diff checks pass. No external/module dependency is added.

Independent21x40 default/reuse runs verify3,360 outputs per mode/policy, all21 GPU samples available.
Default serial/parallel CPU setup+record median/p95: **0.241745/0.318109** and
**0.606625/0.666376 ms**; reuse: **0.241805/0.306012** and **0.637702/0.778514 ms**.
Measured native pool creation/acquisition per mode changes from **84/0** to **0/84**, with four
idle cached pools after reuse. Default GPU sum median/p95: **4.773421/5.834963** and
**4.682682/5.984547 ms**; reuse: **4.714421/5.534808** and **4.657415/5.955893 ms**.
Warmed/max sampled RSS: default51,847,168/51,912,704 bytes, reuse51,830,784/51,892,224 bytes.
These software-host observations do not establish reduced CPU overhead, memory budgets or physical
performance. Hosted Windows/Metal/graphical regressions remain required. This is measurement and
synchronous reuse plumbing; all five full acceptance gates remain open.

### Optional bounded Vulkan command allocator reuse — 2026-10-17

✅ An explicit default-off device option admits a fixed eight-entry native command-pool/primary-buffer
cache. Live lists retain exclusive ownership even after completion. Only unsubmitted or individually
fence-completed lists reset and return their allocation; accepted-but-failed submissions remain
ineligible. Reset clears recorded references before descriptor/query/framebuffer destruction;
failure or overflow destroys the allocation. Counters expose creation, cache acquisition and idle
count. Shutdown drains and destroys cached pools. No query/descriptor caching, extra wait, SDK/C ABI
change or asynchronous Submit is introduced. All C++ modules rebuild; other backends stay disabled.
Native timing-enabled compute/texture oracles add held-list, abandon, cap/overflow and repeated
output/lifetime tests. Beads `nexora-45y.2.13`; source `feat/v2-vulkan-recording-resource-reuse`.
Slang Development configure/build and Xvfb/native-required CTest pass **211/211, zero skips/failures,
121.09 seconds**; Shipping Monolithic/Minimal LTO passes. Focused native reuse **2/2 pass, 2.81 seconds**.
Khronos1.3.275 object/synchronization validation **8/8 pass, 3.12 seconds**, 21 loader-confirmed
insertions and zero Validation Error/VUID/SYNC-HAZARD markers. Compute creates13 and reuses193
pool pairs, ending with8 cached; graphics creates4 and reuses608, ending with4 cached, including
512 repeated native render/readback pixel comparisons with fresh target/query resources. Existing
320 dispatch,48 graph,64 shared-input CPU comparisons and failed/cancelled replay still pass.
Null requests stay disabled. Windows/Metal ABI regressions and hosted Vulkan validation remain CI
gates. Full parallel, asynchronous descriptor/allocator reuse and physical performance acceptance
remain open. See the [RHI contract](../../Engine/RHI/README.md#optional-bounded-vulkan-command-allocator-reuse).

### Matched native serial/parallel recording benchmark — 2026-10-17

✅ A bounded `--benchmark [samples dispatches-per-list]` mode reuses the native GPUDriven compute
oracle with identical work, four lists and ordered synchronous submission in serial/four-worker
modes. Two warmup pairs and alternating first mode reduce order bias. JSON reports CPU setup,
recording/scheduling/join, setup+recording median/p95, separate Submit/Wait median/p95, optional
summed completed GPU intervals, compiler/device IDs and sampled Linux RSS. All output sentinels
reset before measurement and every output is CPU-verified outside timing. Missing metrics stay
null; software flags do not certify physical hardware. No speedup, physical acceptance or target
budget is asserted. The existing native test executable and CI validation step are reused; no new
dependency/job is added. Beads `nexora-45y.2.12`; source `feat/v2-recording-benchmark`.
Integrated with native timestamps and PR628 prerequisites, Slang Development configure/build and
Xvfb/native-required full CTest pass **234/234, zero skips/failures, 245.30 seconds** (GCC14.2,
Slang2026.18, Zig0.14, Mesa lavapipe24.0.5). Focused timing/benchmark **3/3 pass, 4.72 seconds**;
final report-validation recheck **1/1 pass, 0.53 seconds**. Shipping Monolithic/Minimal LTO passes.
Native Khronos1.3.275 object/synchronization validation **7/7 pass, 2.14 seconds**, 19 loader-confirmed
insertions and zero Validation Error/VUID/SYNC-HAZARD markers. No new external/module dependencies.

An additional isolated `--benchmark 21 40` run validates **3,360 measured outputs per mode** plus
warmups. Serial/four-worker CPU setup+record median/p95: **0.271299/0.608567 ms** versus
**0.573014/0.789110 ms**; completed GPU sum median/p95: **5.910813/9.522575 ms** versus
**6.099738/8.569123 ms**, all21 GPU samples available per mode. Warmed RSS51,412,992 bytes;
maximum sampled RSS51,630,080 bytes. Parallel recording is slower on this software host; these
are tool correctness observations, not a speedup or physical acceptance. Reports remain external
scratch evidence; CI logs retain the bounded smoke JSON. Windows/Metal execution requires hosted
native gates. The full gate and asynchronous allocator/descriptor reuse remain open. See the [measurement contract](../../Engine/Renderer/README.md#matched-native-command-recording-measurement).

### Optional completed native GPU submission timing — 2026-10-17

✅ RHI timing is explicitly requested with `CreateDeviceWithOptions`; ordinary creation stays off.
Construction-time support/enabled flags and owning source/completion/optional-millisecond observations
separate unavailable from zero. Vulkan lists own optional timestamp pairs, D3D12 lists own optional
queries/readback storage and queue frequency, and Metal reads completed command-buffer GPU times.
Collection uses existing completion only, with no query WAIT or callback. Default lists allocate/
record no timing resources or completion stream. Cancelled/unsubmitted work publishes nothing.

Presentation's validated counter-width/period/CPU-upper-bound conversion is shared from an RHI
detail header without changing its surface observations or slot lifecycle. Invalid/query/allocation
results stay unavailable; CPU time never replaces GPU time. Native per-list query resources retire
after completion or abandoned-buffer cleanup; D3D12 rejects the device-removed fence sentinel.
No SDK, shader profile, stable C ABI or module relationship is changed; C++ modules rebuild.

Source branch: `feat/v2-native-submission-timestamps`; Beads `nexora-45y.2.11`.
Slang-enabled Development configure/build and Xvfb/native-required full CTest pass **232/232,
zero skips/failures, 263.88 seconds** (Slang 2026.18, Mesa lavapipe 24.0.5, Zig 0.14.0).
`ctest --preset linux-development -R '^(window_presentation\.gpu_timing|renderer\.(submission_gpu_timing|vulkan_recording_gpu_timing|vulkan_texture_recording|vulkan_descriptor_isolation|parallel_graph_recording|dxil_artifact_readiness))$' -V`:
**7/7 pass, zero skips, 0.40 seconds**. Shipping configure/build passes (Monolithic/Minimal, LTO).
Native object/synchronization validation with Khronos layer 1.3.275 and inherited shader paths removed:
`ctest --preset linux-development -R '^renderer\.(submission_gpu_timing|vulkan_recording_gpu_timing|vulkan_texture_recording|contracts|v2_gpu_driven|vulkan_descriptor_isolation)$' -V`
passes **6/6, zero skips, 7.46 seconds**, with loader-confirmed insertion and zero Validation
Error/VUID/SYNC-HAZARD markers. Graphics gets **4/4 available, positive Vulkan observations**;
compute timing also remains available while every existing CPU comparison passes. Default-off,
Validation-unavailable, copied-observation lifetime, reverse submission, cancelled queries and
exact pixels pass. Existing Presentation wrap/invalid-counter tests exercise the shared conversion.
The current CI validation step includes the new timing targets; no new dependency/job is added.

Windows/Metal execution requires hosted native gates; software Vulkan is API/lifetime correctness
evidence only. The matched CPU recording/submission median/p95, GPU/memory budgets and physical-GPU
calibration benchmark remain open, along with asynchronous reuse and the full parallel feature.
See the [RHI contract](../../Engine/RHI/README.md#opt-in-completed-submission-gpu-timing).

### Shared-input and DXIL admission integration — 2026-10-17

✅ The follow-up combines immutable shared buffer reads with selected DXIL stage/profile checks,
including requested wrong-stage constructor rejection and fence-event cleanup. Integrated
Slang-enabled Development configure/build and Xvfb/native-required CTest pass **206/206,
zero skips/failures, 119.65 seconds**. The six-test focused renderer command, including
`renderer.dxil_artifact_readiness`, passes **6/6, zero skips, 0.29 seconds**. Shipping configure/build
passes (Monolithic/Minimal, LTO). The existing four-test native Vulkan object/synchronization
validation command passes **4/4, zero skips, 1.18 seconds**, with loader-confirmed layer insertion
and zero Validation Error/VUID/SYNC-HAZARD markers. Windows actual DXIL/PSO/read-only transaction
and Metal shared-read execution remain hosted gates. Beads `nexora-45y.2.9` and `.2.10` share this
follow-up; the complete toolchain, parallel/performance and preview gates stay open.

After PR626 merge, rebased onto main `bb39a646`, preserving upstream Windows VM tooling/evidence.
The complete Slang Development configure/build and Xvfb/native-required CTest pass again:
**207/207, zero skips/failures, 120.22 seconds**. The six-test focused renderer command passes
**6/6, zero skips, 0.26 seconds**; Shipping Monolithic/Minimal LTO configure/build passes.

### DXIL entry-stage and artifact-profile admission — 2026-10-17

✅ D3D12 requested entry artifacts now undergo portable, bounded DXBC/DXIL program metadata
inspection. The reader validates container/version/size, part and bitcode ranges, non-overlap,
one executable part and LLVM magic; it decodes implemented vertex/fragment/compute stages and
numeric Shader Models. Wrong-stage, malformed and older-profile artifacts reject before native
PSO allocation. Graphics checks both selected graphics stages independently from compute.
Device capability remains a separate check; hardware SM6.9/6.10 support cannot admit a newer
minimum against existing SM6.6 graphics or SM6.0 compute bytecode. Optional FXC baseline remains
available without a DXIL minimum. No native advanced feature or profile baseline is enabled.

Portable tests cover stage/profile decoding, numeric 6.9/6.10 order, every truncated prefix,
unordered offsets, malformed/overflowing/overlapping parts and duplicate programs. Windows
Slang-enabled native CI additionally reads actual generated artifacts, checks SM6.6 graphics and
SM6.0 compute, creates baseline PSOs and rejects newer requirements when the device supports them.
An explicitly requested wrong-stage compute artifact is also rejected at device construction;
the raw fence event is created last to avoid leaking it if constructor validation fails.
Metadata inspection does not validate LLVM, checksums/signatures, compiler provenance or preview
opt-in. It cannot establish SM6.9/6.10 execution or close the complete toolchain matrix gate.

Source branch: `feat/v2-dxil-artifact-admission`; Beads `nexora-45y.2.9`.
Slang-enabled `cmake --preset linux-development` and `cmake --build --preset linux-development`
pass. Xvfb/native-required `ctest --preset linux-development`: **205/205 pass, zero skips/failures,
122.74 seconds** (Slang 2026.18, Mesa lavapipe 24.0.5, Zig 0.14.0). Focused
`ctest --preset linux-development -R '^renderer\.(dxil_artifact_readiness|graphics_capabilities|contracts)$' --output-on-failure`:
**3/3 pass, zero skips, 0.07 seconds**. Shipping configure/build passes (Monolithic/Minimal, LTO).
A Linux metadata-only probe also accepts real vkd3d-proton DXIL fixtures: compute SM6.6 (2660 bytes),
fragment SM6.1 (2472 bytes), vertex SM6.0 (1988 bytes). Fixtures/probe remain outside the repository;
this is parsing evidence, not GPU execution. Windows runtime/PSO acceptance requires hosted CI.
The exported C++ helpers require module rebuild; stable C ABI and SDK/compiler baselines stay
unchanged. Full V2 graphics, compiler/artifact readiness and physical-GPU performance gates remain
open. See the [RHI contract](../../Engine/RHI/README.md#dxil-artifact-stage-and-profile-admission).

### Immutable shared buffer inputs — 2026-10-17

✅ A separately default-closed `shared_buffer_read_recording` capability admits compatible
same-queue/same-access read/read sharing in bounded compute and color-target cohorts. Writable
buffers remain separate; shared textures, all write conflicts and access/queue dependencies remain
excluded. Explicit callback opt-in, immutable caller-owned resources, join-before-submission,
ordered synchronous completion, cancellation and default serial execution stay unchanged.

D3D12 snapshots now distinguish potential UAV writes from immutable SRV/indirect reads. Accepted
potential writes, CPU uploads and nonempty readbacks advance data revisions; compatible reads
preserve them. Initial parallel readers can copy the same retained staging revision with COMMON
promotion/decay at each individual Execute boundary. Mutation still invalidates older readers;
unsubmitted upload copies remain private. Empty validated reads do not invalidate snapshots.

The shared native oracle adds 64 compute CPU comparisons (parallel first with pending input,
serial and failed/cancelled recovery), forced four-worker overlap, distinct output sentinels and
successful reuse. Two graphics workers share fresh indirect arguments with separate color targets
and both images match serial direct drawing exactly. D3D12 additionally checks reverse pure-reader
submission, upload/readback/GPU-writer invalidation, empty-read compatibility and recovery.
Portable tests preserve custom-backend fallback and write/read, read/write, write/write and access
hazard ordering. The full parallel feature remains disabled/unimplemented.

Source branch: `feat/v2-shared-buffer-recording`; Beads `nexora-45y.2.10`.
Slang-enabled `cmake --preset linux-development` and `cmake --build --preset linux-development`
pass. Xvfb/native-required `ctest --preset linux-development`: **205/205 pass, zero skips/failures,
137.44 seconds** (Slang 2026.18, Mesa lavapipe 24.0.5, Zig 0.14.0). Focused
`ctest --preset linux-development -R '^renderer\.(parallel_graph_recording|contracts|graphics_capabilities|vulkan_texture_recording|vulkan_descriptor_isolation)$' -V`:
**5/5 pass, zero skips, 0.28 seconds**. Shipping configure/build passes (Monolithic/Minimal, LTO).
Four native object/synchronization validation tests pass **4/4, zero skips, 2.53 seconds** with
Khronos layer 1.3.275, loader-confirmed insertion, inherited shader paths removed and zero
Validation Error/VUID/SYNC-HAZARD markers. Windows/Metal native acceptance requires hosted CI.
All C++ modules rebuild for the appended capability; stable C ABI and SDK/shader baselines stay
unchanged. Shared-texture reads, asynchronous reuse, full compiler/artifact readiness and matched
physical-GPU performance remain open. See the
[RHI contract](../../Engine/RHI/README.md#immutable-shared-buffer-reads).

### Native graphics/cancellation integration — 2026-10-17

✅ PR626 integrates cancellation/same-pool coordination, Vulkan framebuffer/texture isolation and
bounded color-target recording across Vulkan, D3D12 and Metal. The shared graphics oracle now
also cancels a clear-only parallel cohort, requires zero submission and unchanged pixel sentinels,
then replays with fresh cancellation state. Resource snapshots and every cancellation check/worker
identity guard coexist; the default path remains serial and the full parallel feature stays closed.
MSVC exports the exception's virtual diagnostic member while other platforms retain visible type
RTTI, preserving typed catches across module boundaries without suppressing compiler warnings.

Integrated Slang-enabled `cmake --preset linux-development` and
`cmake --build --preset linux-development` pass. Xvfb/native-required full
`ctest --preset linux-development`: **229/229 pass, zero skips/failures, 259.33 seconds**.
`ctest --preset linux-development -R '^(core\.runtime|renderer\.(parallel_graph_recording|vulkan_texture_recording|vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven))$' --output-on-failure`:
**7/7 pass, zero skips, 0.36 seconds**. Shipping configure/build passes (Monolithic/Minimal, LTO).
The four-test Vulkan object/synchronization validation command below passes **4/4, zero skips,
2.15 seconds** with inherited shader paths removed; loader output confirms 12 layer insertions,
and the full log contains zero Validation Error/VUID/SYNC-HAZARD markers. Windows/Metal runtime
acceptance requires the new-head hosted gates. Beads `nexora-45y.2.6`, `.2.7`, `.2.8` share PR626;
all complete V2-GFX and physical-performance gates remain open.

### Bounded native color-target graph recording — 2026-10-17

✅ The limited `independent_graphics_recording` contract adds disjoint color-target graphics
cohorts to the opt-in graph scheduler alongside independently admitted buffer-only Compute.
Graphics passes require complete declarations, at least one RenderTarget write, no texture reads
and no buffer writes. Shared resources (including read/read), dependencies, unsupported/unmarked
passes and singleton cohorts fall back to serial. Default Execute stays serial. Workers only
record; the coordinator joins, submits in graph order and waits before retirement.

D3D12/Metal now snapshot texture generations/revisions/native metadata and logical state per list,
validate before execution and commit only after queue acceptance. Readback invalidates older
snapshots; failed/abandoned recordings preserve committed state. Validation models the same policy,
including upload invalidation. D3D12 RTV allocation checks its 256-entry heap and recycles retired
slots. Test-only native color readback packs padded staging rows; production execution has none.
Metal graphics entry points use autorelease pools and retain list-owned textures/encoders with ARC.

The common Vulkan/D3D12/Metal oracle covers 24 exact mixed-format/extent serial/parallel pixel
comparisons, six scopes/list, four-worker overlap and reverse submission; stale-list and
abandonment recovery; 512 sequential target allocations/reclamation; and eight graph output
comparisons with aborted-batch sentinels and replay. Portable tests additionally cover capability
independence, mixed compute/graphics cohorts, shared reads, texture hazards/dependencies, opt-out,
sampling/copy fallback, ordered submission and completion-before-retirement.

Source branch: `feat/v2-native-graphics-recording`; Beads `nexora-45y.2.8`.
Slang-enabled `cmake --preset linux-development` and `cmake --build --preset linux-development`
pass. Xvfb/native-required `ctest --preset linux-development`: **205/205 pass, zero skips/failures,
134.48 seconds** (Slang 2026.18, Mesa lavapipe 24.0.5, Zig 0.14.0). Shipping configure/build passes
(Monolithic/Minimal, LTO). Native Vulkan object/synchronization validation with layer **1.3.275**
passes **4/4, zero skips, 8.24 seconds**, with loader-confirmed insertion and zero Validation Error,
VUID or SYNC-HAZARD markers. Native artifacts come from CTest, with inherited shader paths removed.
The final mixed-cohort portable test also passes. Windows/Metal runtime acceptance requires hosted
CI; Linux does not certify those backends. All C++ modules rebuild for the appended capability;
stable C ABI and shader/SDK baselines are unchanged. Full parallel feature admission, shared reads,
asynchronous queues/reuse, compiler/artifact readiness and matched physical-GPU performance remain
open. See the [graph contract](../../Engine/Renderer/README.md#native-color-target-graph-acceptance)
and [RHI contract](../../Engine/RHI/README.md#independent-color-target-recording-and-transactional-textures).

### Vulkan texture recording and complete framebuffer retirement — 2026-10-17

✅ Vulkan texture transitions now use list-owned generation/revision/state snapshots. Rendering
validates that private state, submission rejects stale snapshots before closing/queueing, and only
accepted work commits final device state. Readback invalidates older recordings while restoring
layout. Every framebuffer from every rendering scope retires after completion or abandoned-list
cleanup; a single field no longer loses earlier scopes. Undefined destinations, invalid queues,
oversized render areas and post-closure recording fail before native commands.

The native test uses 24 disjoint mixed-format/extent targets, six render scopes per list and four
recording workers with timed overlap, then reverse submission and 24 exact serial/parallel pixel
comparisons against nonempty references. It verifies abandoned state/output and retry, stale
submission/readback/handle rejection, callback failure recovery and repeated serial graph replay.
The existing CI synchronization/object-validation step now includes both graphics texture recording
and compute descriptor isolation; no new CI dependency or job is added. No texture upload API is
introduced. Resource mutation/submission remains caller-serialized against recording.

Source branch: `feat/v2-vulkan-texture-recording`; Beads `nexora-45y.2.7`.
Local Slang-enabled `cmake --preset linux-development`, `cmake --build --preset linux-development`
and Xvfb/native-required `ctest --preset linux-development` pass: **205/205, zero skips/failures,
118.15 seconds** (Slang 2026.18, Mesa lavapipe 24.0.5, Zig 0.14.0). Shipping configure/build passes
(Monolithic/Minimal, LTO). Native validation command
`ctest --preset linux-development -R '^renderer\.(vulkan_texture_recording|contracts|v2_gpu_driven|vulkan_descriptor_isolation)$' -V`
passes **4/4, zero skips, 0.87 seconds**, with inherited shader paths removed so CTest supplies both
artifacts. Ubuntu validation layer **1.3.275** is extracted outside the repository; loader output
proves insertion and the complete log contains **zero Validation Error/VUID/SYNC markers** with
synchronization validation enabled. Touched-file format and `git diff --check` pass. An earlier
Slang-off/manual-artifact full gate also passed 195/195; the Slang-enabled gate above is final.

General graph graphics admission, DX12/Metal texture isolation, shared reads, asynchronous queues/
reuse, compiler/artifact readiness and matched physical-GPU performance remain open. The broad
parallel feature stays disabled/unimplemented, and no shader/SDK baseline or stable C ABI changes.
See the [RHI ownership contract](../../Engine/RHI/README.md#vulkan-texture-snapshots-and-framebuffer-retirement-v2-gfx-1-slice).

### Metal full-kernel compute and worker ownership — 2026-10-17

✅ The Metal path now selects the real Slang-generated `GPUDriven.metal`, validates requested
artifacts without no-op substitution, snapshots four list-local storage bindings, replays bindings
after encoder boundaries and manages worker autorelease pools with ARC-retained native objects.
Shared-buffer test readback waits for completion and validates bounds. The limited independent
buffer-only Compute admission includes Metal; general parallel feature admission stays disabled.

The macOS native gate now executes full culling/distance/LOD/Hi-Z/compaction/classification/indirect
generation and requires the existing CPU oracle. `renderer.metal_recording_isolation` reuses 320
isolated dispatch comparisons, 48 serial/four-worker WAW graph comparisons, timed overlap and
failed-batch sentinel preservation/reuse. It additionally checks binding replay across WAW encoder
boundaries and offset/empty/invalid/stale readbacks. Native-required builds fail for absent real
artifacts/device; optional builds explicitly skip. This is not the previous no-op smoke gate.

Source branch: `feat/v2-metal-compute-recording`; Beads `nexora-45y.2.5`.
Local `cmake --preset linux-development` and `cmake --build --preset linux-development` pass
with Slang enabled. Xvfb/native-required `ctest --preset linux-development`: **228/228 pass, zero
skips/failures, 284.16 seconds**, including real Metal source generation and native Vulkan execution
(Slang 2026.18, Mesa lavapipe 24.0.5, Zig 0.14.0). Focused
`ctest --preset linux-development -R '^(build\.shader_crosscompile|renderer\.(v2_gpu_driven|vulkan_descriptor_isolation|contracts|graphics_capabilities|parallel_graph_recording|metal_source_contract))$' --output-on-failure`:
**7/7 pass, zero skips, 0.34 seconds** after final platform guards. `cmake --preset linux-shipping`
and `cmake --build --preset linux-shipping` pass (Monolithic/Minimal, LTO). Touched-file format and
`git diff --check` pass. Native missing-artifact rejection has a dedicated macOS test.

Linux Slang cross-compilation is artifact evidence only; macOS runtime acceptance must pass hosted
CI. Full V2-M3 parity, GPU validation, graphics/texture/shared-read concurrency, asynchronous queues
and reusable retirement, compiler/artifact readiness and matched physical-GPU performance remain
open. Shader/SDK baselines and stable C ABI remain unchanged. See the
[Metal ownership contract](../../Engine/RHI/README.md#metal-gpu-driven-compute-and-worker-ownership-v2-gfx-1-slice).

### D3D12 isolated buffer recording — 2026-10-17

✅ D3D12 buffer uploads/state preparation now belong to each command list. Recording and abandoning
lists leave committed device state untouched. Submission validates every buffer generation/revision
before native execution, then commits consumed uploads and `COMMON` decay after queue acceptance.
Upload, accepted submission and nonempty readback invalidate older snapshots; rejected lists must be
recorded again. Texture state preparation remains serialized and does not gain rollback semantics.

The limited `independent_compute_recording` admission now includes D3D12 disjoint buffer-only
Compute passes. The broad parallel feature stays disabled/unimplemented. The Windows-only
`renderer.d3d12_recording_isolation` target shares the existing Vulkan workload: 320 unique dispatch
comparisons, 48 serial/four-worker graph comparisons, forced callback overlap, aborted-batch
sentinels and successful reuse. Additional cases exercise abandoned initial upload/state preparation,
stale upload/submission/readback/handle rejection before execution and six exact CPU comparisons.

Source branch: `feat/v2-d3d12-recording-isolation`, base `8a12a0e2cf40ab524c659d868bfa447a908655e3`.
Beads: `nexora-45y.2.4`; parent `nexora-45y.2` and all five full gates remain open.
Local validation: `cmake --preset linux-development` and `cmake --build --preset linux-development`
passed; Xvfb/native-required `ctest --preset linux-development` passed **194/194, zero skips/failures,
97.62 seconds**, using Slang 2026.18 SPIR-V, Mesa lavapipe 24.0.5 and Zig 0.14.0. Focused
`ctest --preset linux-development -R '^renderer\.(parallel_graph_recording|graph_buffer_dependencies|vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven)$' --output-on-failure`
passed **6/6, zero skips, 0.28 seconds**. `cmake --preset linux-shipping` and
`cmake --build --preset linux-shipping` passed (Monolithic/Minimal, LTO). Touched-file
`clang-format --dry-run --Werror` and `git diff --check` passed.

Native Windows execution requires hosted CI; Linux cannot certify D3D12. No physical-GPU timing,
GPU validation-layer result, shared-read/texture concurrency, Metal worker ownership, asynchronous
queue separation or reusable allocator retirement is claimed. The compiler/artifact readiness and
matched performance gates remain open. Shader/SDK baselines and the stable C ABI are unchanged.
See the [RHI snapshot contract](../../Engine/RHI/README.md#d3d12-buffer-recording-snapshots-v2-gfx-1-slice).

### Cancellation and same-pool coordination — 2026-10-17

✅ Pool-local worker identity lets a graph coordinator inside its own recording pool select serial
fallback, avoiding starvation even when every pool worker coordinates a graph. A different-pool
coordinator retains bounded parallel admission. Start/Stop and object lifetime remain externally
serialized; general waits do not gain work stealing or cyclic-wait recovery.

An optional cancellation token stops before RHI allocations/callbacks on pre-cancelled execution,
or before submission of an unsubmitted recorded cohort. Every started job joins; callback/scheduler
errors precede observed cancellation. Once cohort submission begins it completes and retires;
next-cohort work stops, while late cancellation after the final accepted cohort returns completion.
A typed `RenderGraphCancelled` distinguishes cancellation. Existing defaults and stable C ABI stay
unchanged; C++ modules rebuild for the new query/exception and appended token.

Portable tests cover saturated nested coordinators, foreign-pool overlap, stopped/restarted identity,
pre/serial/parallel cancellation, transient cleanup, error precedence, accepted-batch completion and
late cancellation. Native Vulkan additionally verifies sentinel preservation, zero submission/readback
and buffer reuse after cancellation, retaining its existing 320 dispatch and 48 graph CPU comparisons.
Source branch: `feat/v2-recording-cancellation-and-nesting`; Beads `nexora-45y.2.6`.
Local `cmake --preset linux-development` and `cmake --build --preset linux-development` pass.
Xvfb/native-required `ctest --preset linux-development`: **194/194 pass, zero skips/failures,
107.80 seconds** (Slang 2026.18 SPIR-V, Mesa lavapipe 24.0.5, Zig 0.14.0). Focused
`ctest --preset linux-development -R '^(core\.runtime|renderer\.(parallel_graph_recording|vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven))$' --output-on-failure`:
**6/6 pass, zero skips, 0.27 seconds**. `cmake --preset linux-shipping` and
`cmake --build --preset linux-shipping` pass (Monolithic/Minimal, LTO). Touched-file
`clang-format --dry-run --Werror` and `git diff --check` pass.

Integration after PR625: rebased onto main `c5c5b897`; Slang-enabled full Development
configure/build and Xvfb/native-required CTest pass **228/228, zero skips/failures, 267.94 seconds**.
Final focused command above passes **6/6, zero skips, 0.27 seconds**. Shipping Monolithic/Minimal
LTO configure/build passes. Rebase from the dependency head preserves an identical validated source
tree; both D3D12 and Metal now compile the shared cancellation oracle in their hosted native gates.

All full gates, graphics/texture concurrency, shared reads, asynchronous queues/reuse, compiler/artifact
readiness and matched performance acceptance remain open. See the
[cancellation contract](../../Engine/Renderer/README.md#recording-cancellation-and-nested-coordinators).

### Bounded graph compute recording — 2026-10-17

✅ Explicit opt-in graph scheduling now records independent, buffer-only Compute passes on the
existing JobSystem in bounded batches. Validation and Vulkan expose limited admission separately
from the still-disabled full `ParallelCommandRecording` feature. Unmarked callbacks, textures,
other queues/backends, any shared buffer (including read/read), explicit dependencies and missing,
stopped or single-worker pools retain serial execution. The original `Execute(device)` stays serial.

The coordinator owns allocator/list creation, joins all started recording jobs, submits in compiled
order and waits each completion value before list retirement. A failed recording batch submits
nothing; deterministic callback errors stop successors. Submit/wait failures drain before releasing
lists. This is completion-aware retirement by waiting, not asynchronous worker reuse.

Source branch: `feat/v2-parallel-graph-recording`, base `63bb5f7a49e5ec9df0b285494e79ee046c61a6f7`.
Beads: `nexora-45y.2.3`; parent `nexora-45y.2` and full V2-GFX-1 remain open.

- `cmake --preset linux-development` and `cmake --build --preset linux-development`: passed.
- Xvfb/native-required `ctest --preset linux-development`: **194/194 passed, zero skips/failures,
  102.26 seconds**, using Slang 2026.18 SPIR-V, Mesa lavapipe 24.0.5 and Zig 0.14.0.
- `ctest --preset linux-development -R '^renderer\.(parallel_graph_recording|graph_buffer_dependencies|vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven)$' --output-on-failure`:
  **6/6 passed, zero skips, 0.27 seconds**.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed,
  Monolithic/Minimal with LTO, including the final completion-wait implementation.
- Touched-file `clang-format --dry-run --Werror` and `git diff --check`: passed.

Portable tests force concurrent callbacks and reverse completion, verify fixed submission and
bounded allocation, dependent waves, resource/queue/texture/explicit-edge fallbacks, legacy serial
execution, stopped/missing/single-worker pools, deterministic recording failure, no failed-batch
submission and completion-before-retirement through a modeled deferred device. Submission failure
injection models work accepted by a native queue and requires draining before allocator release,
in both serial and parallel paths. Scheduling exceptions also join every started job by design;
internal JobSystem allocation failure is not fault-injected.

The native Vulkan descriptor test retains its 320 isolated dispatch comparisons and additionally
runs 24 unique workloads through two WAW graph waves in serial/four-worker modes (48 exact CPU
reference comparisons). Timed rendezvous requires all four callbacks to overlap. An injected
recording failure preserves every output sentinel, performs no submission/readback, joins all
workers and permits successful reuse of the same borrowed buffers. The normal path stays
readback-free. This proves Linux software-device correctness; it does not establish CPU-overhead
reduction, physical-GPU timing, general graphics/texture recording, shared-read concurrency,
D3D12/Metal worker safety, cancellation or asynchronous multi-queue retirement. The compiler/artifact
matrix, matched CPU median/p95, GPU time/memory budgets and all full gates stay open. No shader,
SDK, feature default or stable C ABI changes. See the
[execution contract](../../Engine/Renderer/README.md#bounded-opt-in-parallel-compute-and-color-target-recording).

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
PR [#618](https://github.com/jimlee1972/Nexora/pull/618) subsequently merged after both hosted workflows succeeded.

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
and Linux builds with Window enabled compile the admission header after Xlib. PR [#615](https://github.com/jimlee1972/Nexora/pull/615) subsequently merged after all 36 hosted checks succeeded.

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

## V2-GFX-2 bounded tessellation construction status

✅ The independent portable/Linux slice adds three-control-point PatchList topology, hull/domain stage bits,
implied device admission, selected-stage artifact checks and geometry-specific pipeline/cache
identity. Vulkan enables queried tessellation and creates a real four-stage PSO; D3D12 builds
VS/HS/DS/PS with PATCH/three-point IA topology. Metal rejects native hull/domain and uses an
explicit baked16-triangle/48-vertex fallback. Defaults preserve ordinary triangles; malformed
combinations and bounded draw/indirect misuse reject before native recording. Fixed factor4 and
curved output are bootstrap limits, not fully general patch/material assembly.

Real Slang reflection validates hull/domain entry names and64-byte fallback frame layout; generated
source/include/artifact/profile/topology identity distinguishes programs. Baked SPIR-V omits the
Tessellation capability. New control/evaluation stage bits8/16 survive binding validation/layout
hashes and cooked format1 round-trip; selected entry identities differ and unknown bits reject.
SPIR-V1.0-1.3 entry metadata is bounded/read-only; it does not claim semantic/provenance validation.
Existing graphics SM6.6 and compute SM6.0 baselines stay unchanged; new DXIL stages use SM6.0.

Source `feat/v2-bounded-native-tessellation`; Beads `nexora-45y.3.1`. Native Vulkan standalone output
matches baked output exactly at64x64 (**466 covered pixels, maximum channel delta0**). The permanent
oracle checks RGBA/BGRA images within one channel step, changed/nonempty geometry, two forced workers,
failed/cancelled sentinels and successful replay. Focused admission/DXIL/native/cooked tests **5/5
pass,0.14 seconds**. Khronos1.3.275 object/synchronization validation **5/5 pass,3.19 seconds**,
14 loader-confirmed insertions, zero Validation Error/VUID/SYNC-HAZARD markers. Full Development
and Shipping validation pass: full Slang/Zig/native Development configure/build and Xvfb-required
CTest **208/208,zero skips/failures,129.49 seconds** (Showcase/graphical shell off); Shipping
Monolithic/Minimal LTO configure/build passes. Touched formatting/diff checks pass. No complete
gate is marked closed.

After integrating merged PR630/main `1d48e918`, full Development configure/build and Xvfb CTest
pass again: **214/214, zero skips/failures,188.04 seconds**. Focused native/admission/cooked/timing
checks **6/6,0.31 seconds**; Shipping passes. Combined native renderer object/synchronization
validation **11/11,13.94 seconds**,27 confirmed layer insertions, zero validation/VUID/sync errors.

A separately staged official Linux DXC **v1.9.2609 /1.9.2609.5** permits real Slang2026.18 DXIL
compiler/reflection/reader probes without changing project dependencies: control4008 bytes,
evaluation3960 bytes and baked vertex5488 bytes decode to stages8/16/1, all SM6.0. This does not run
D3D12. Windows HS/DS and Metal fallback hosted acceptance is recorded in the integrated section below; broader
patch authoring, Runtime program assembly/cook execution, Enhanced Barriers and full target-host/
performance acceptance remain open. All five full gates stay open.

## V2-GFX-4 bounded per-draw VRS construction status

A bounded D3D12 tier1 route is under construction: explicit full/2x2 pipeline shading rate,
implicit capability admission and complete cache identity, list-owned list5 recording and full-rate
restoration, accepted-list coarse draw counters, and independent implementation-tier ceilings.
Hardware tier2 cannot authorize unimplemented rate-image/combiner behavior. Other backends select
an explicit full-rate fallback. Native RGBA/BGRA gradient coverage/error, two-worker cancellation/
replay and reset oracles are being validated; no native or performance acceptance is claimed yet.
Source `feat/v2-d3d12-draw-shading-rate`, Beads `nexora-45y.5.1`. Full VRS/Ultimate, meshlets, feedback,
physical budgets and all full gates remain open.

✅ Local portable admission/cache/implementation ceilings and actual full-rate Vulkan fallback
pass. Full native/Slang/Zig Development configure/build and Xvfb CTest **215/215,zero failures/skips,
177.24 seconds** (Showcase/graphical shell off); focused **4/4,0.21 seconds**. Shipping
Monolithic/Minimal LTO passes. Combined native object/synchronization regression **12/12,18.32 seconds**,
29 layer insertions and zero validation/VUID/sync markers. Actual Vulkan fallback covers512 pixels
with zero changed channels; enabled=0/coarse_draws=0 is explicitly not native VRS execution.
Windows2x2 execution and hosted platform gates remain pending; no full gate closes.

## V2-GFX-4 bounded mesh shader construction status

A bounded native D3D12 MS/PS path is under construction, with explicit stage/profile/variant
admission, a list-owned read-only GPU statistics input and DispatchMesh(1,1,1). A real Slang2026.18/
DXC1.9.2609.5 probe emits4876-byte mesh DXIL at SM6.5, decoded as stage32; actual frame/SRV/entry
reflection and native SDK pipeline-stream syntax probes pass. No D3D12 execution is claimed from
these Linux probes. The oracle verifies GPU frustum culling0/1 against CPU, mesh versus generated
indirect RGBA/BGRA pixels, stale/abandoned input, shared workers and cancellation/replay. Source
`feat/v2-bounded-d3d12-mesh-shader`, Beads `nexora-45y.5.2`; native execution/hosted gates remain
pending. General meshlets/amplification/indirect mesh dispatch and all full gates remain open.

✅ Local mesh admission/cache/DXIL/profile/cooked-stage checks and real GPU compute/indirect
fallback pass. Full Slang/native/Zig Development configure/build and Xvfb CTest **216/216,zero
failures/skips,136.60 seconds** (Showcase/graphical shell off); focused **7/7,0.46 seconds**.
Shipping Monolithic/Minimal LTO passes. Combined native object/synchronization regression
**13/13,15.24 seconds**,31 layer insertions and zero validation/VUID/sync markers. Real staged
4876-byte Mesh DXIL passes the actual engine reader at stage32/SM6.5; the private native pipeline
adapter passes official SDK syntax checks. These Linux results do not execute D3D12 Mesh shaders.
Hosted native MS/PS execution remains pending; no full gate closes.

## Integrated Enhanced Barriers, mesh and VRS local acceptance

Own PR632 now integrates the three bounded D3D12 routes on merged Tessellation/main
`77090f7de8340c606c4e770218600c6d41e0fa58`. Windows mesh/VRS oracles run both legacy and requested
Enhanced Barriers, reporting actual enablement and accepted texture/buffer counters. The native
adapter admits pixel, non-pixel and combined ShaderRead masks while rejecting mixed read/UAV.
A real SDK-enum regression covers the graph's combined192 mask; this pure mapping check is not
D3D12 execution. Aliasing, multi-queue handoffs, tier2 rate images, general meshlets and full gates
remain open. Final hosted native acceptance of the integrated head is pending.

✅ Full integrated native/Slang/Zig Development configure/build and Xvfb CTest **241/241,zero
failures/skips,252.40 seconds** (Showcase on, graphical editor off); focused **8/8,0.31 seconds**.
Shipping Monolithic/Minimal LTO passes. Combined native Vulkan object/synchronization validation
**14/14,17.20 seconds**,31 layer insertions, zero validation/VUID/sync markers. Root README remains
reader-facing; all C++ modules rebuild, stable C ABI and baseline shader/SDK requirements unchanged.

✅ Tessellation's own PR631 was squash-merged as `77090f7d` after36/36 checks and both first-attempt
workflows39019440906/39019475539 passed on6b05393b: Linux360/360, Windows303/303 and macOS299/299
both runs. Windows executes HS/DS and Vulkan executes control/evaluation, with curved RGBA/BGRA
baked parity, artifact negatives and overlapping-worker cancellation/replay. Metal executes the
baked fallback and explicitly reports native tessellation unavailable. Broader authoring/cook and
physical acceptance remain open.

## V2-GFX-5 bounded Work Graph construction status

A default-off Windows-native option uses separately discovered SDK DXC for SM6.8 node libraries,
retaining Slang compute/indirect fallback and baseline profiles. Real Linux DXC1.9.2609.5 compiles
the two-node library; the actual engine library inspector reads SM6.8 while its single-entry
inspector rejects libraries. Slang2026.18 does not implement the required Node types, so the normal
Slang route is not used. Native SDK helper syntax checks pass; no D3D12 execution is claimed yet.

The bounded recipe expands one eight-byte CPU input into up to eight canonical visible records.
Native creation validates program/nodes/entry ABI and a dedicated output-only root. Each live list
owns fresh initialization/backing contexts, immutable program snapshots and existing output revision
checks. Oracles compare CPU/real GPU compute, wrong compiled ABI, stale/abandoned resources, copied
input records, mixed program/root state and cancellation/replay. Source
`feat/v2-native-work-graph-fanout`, Beads `nexora-45y.6.1`.

✅ Local library/admission and real compute fallback acceptance: Linux Development configure/build
and full CTest pass 217/217 with no failures or skips (133.53s); Shipping Minimal/Monolithic LTO
build passes. Focused library/mesh/VRS/work-graph/runtime tests pass 7/7 after the final input-bound
changes (0.26s). Khronos object/synchronization validation passes 14/14 (15.65s), with 33 confirmed
layer insertions and zero validation errors/VUID/synchronization hazards. Native Work Graph
execution remains pending on Windows; the Linux fallback reports enabled=0 and dispatches=0.
General graphs/graphics nodes, newer shader profiles, physical budgets and all full gates stay open.


The Work Graph candidate now includes PR632's latest combined barrier/mesh/VRS source dependency.
After integration, Development configure/build and full CTest pass **218/218**, zero failures/skips
(133.84s), Shipping Minimal/Monolithic LTO passes, and focused contracts/profile tests pass **8/8**.
The native Khronos object/sync selection passes **13/13** (4.06s), with24 confirmed layer insertions
and zero validation/VUID/sync errors. Windows legacy and enhanced Work Graph variants remain
pending; successful Linux compute fallback is not native node proof.

## V2-GFX-3 bounded triangle-ray construction status

`feat/v2-bounded-native-rays`, Beads `nexora-45y.4.2`, implements an immutable one-to-eight-triangle
BLAS/TLAS recipe, dedicated root/state/shader-table ownership, native `DispatchRays` and tier1.1
inline queries. It preserves caller-serialized creation/submission, pre-submit handle/revision
validation, privately retained per-list snapshots and ordinary compute state restoration. Existing
Slang opt-in emits independent library6.3/query6.5 targets and lower-SM compute traversal; baseline
profiles stay unchanged. Actual reflection checks root/32-byte input/16-byte hit ABI, and private
exact-byte identities reject changed same-profile recipes before native creation. No full gate closes.

✅ Real Slang2026.18 + official DXC1.9.2609.5 compiler probes, actual engine library/entry admission,
and Linux native Vulkan compute/reference focused checks pass. With precise floating-point mode,
the Slang library is6972bytes/SM6.3 and query5236bytes/SM6.5; these are actual artifacts, not native
execution proof. The double-precision CPU oracle matches real compute hit/miss/nearest/barycentric
outputs, invalid rays, canaries and forced two-worker failure/cancellation/replay for1/2/8 triangles
and1/64 rays. Native SDK helper syntax passes with warnings-as-errors. Windows AS build, ray/query
dispatch, stale/abandoned lifetimes and enhanced/legacy parity remain pending. General AS update/
compaction, shadow/reflection/AO integration, DXR1.2, SER and physical/full acceptance remain open.

The next own PR integrates the Work Graph and ray recipes because they share library admission
and native program ownership. On that combined source, Development configure/build and full CTest
pass **219/219**, zero failures/skips (134.55s), focused contracts/profile tests **9/9** (0.31s), and
Shipping Minimal/Monolithic LTO passes. Khronos object/synchronization validation with Xvfb passes
**17/17** (9.41s), with37 confirmed layer insertions and zero validation/VUID/sync errors.
The separate precise-mode ray candidate passed218/218 (123.92s); native Vulkan fallback remains
explicitly ray/node enabled=0. Actual production CMake artifact recipes execute successfully with
staged Linux DXC; the Windows SDK preflight, normal/wrong-entry node libraries and exact reflected
ray-byte identity generation pass. A real changed ray library remains valid SM6.3 while differing
from the pinned recipe, exercising the intended negative boundary. Combined SDK helper and Windows
conditional test syntax checks pass with warnings-as-errors. Windows additionally exercises
Node/Ray/Node program/root switching when both capabilities are admitted; this native interop
and both advanced program executions remain pending until hosted proof. No full gate closes.

## Hosted Enhanced Barriers, mesh and VRS acceptance (PR632)

✅ Own PR632 squash-merged as `bb61b59d3d9d1591932a866dcf9bf17c49ab0a64` after all36 final-head
checks and both first-attempt workflows39026010174/39026013053 succeeded on7f35467e. Push/PR suites:
Linux363/363 (1342.22/1349.13s), Windows310/310 (1278.89/1322.51s), macOS302/302 (640.91/563.69s).
Both Windows runs execute native mesh enabled1/16 dispatches and VRS enabled1/8 coarse calls in
legacy/enhanced modes. Enhanced Barriers report graphics texture182/buffer2, compute buffer2629,
VRS texture44 and mesh texture64/buffer40. Native CPU/pixel comparisons, artifact negatives,
full-rate reset, shared-input workers and stale/abandoned/cancelled replay all pass. Linux/Metal
execute real compute/indirect/full-rate fallback, explicitly mesh/VRS enabled0. Package, Vulkan
validation, Editor display, build-contract and sanitizer checks pass. This completes child tasks
nexora-45y.3.2/.5.1/.5.2, not full V2-GFX-2/4 or Ultimate. Broader topology/cook, aliasing,
multiqueue, general meshlets/amplification/indirect mesh, VRS2 images and physical budgets stay open.

The current hosted D3D12 query independently reports RT1.1, mesh1.0, VRS1.0, feedback1.0,
Work Graph1.0 and SM6.8. This supports implementing compatible remaining paths; it does not
certify their execution or admit RT1.2, SM6.9/6.10 or SER. The Node/Ray source rebase from632head
onto its squash has an identical tested tree (`99bf21beb28b15a99d9010a9e0d8c4f6f3c27577`);
no pending Tessellation/mesh/VRS/barrier commits are duplicated in the next PR.

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

## V2-GFX-2 bounded Enhanced Barriers construction status

✅ Portable independent sync/access/layout projection and explicit Null unavailability pass.
The default-off D3D12 option now has a guarded list7 implementation: independent sync/access/layout
projection for admitted copy, shader/UAV, render-target and indirect use; transactional snapshots;
same-access UAV dependencies; and enhanced texture readback without unsafe legacy interleaving.
SDK/hardware/interface availability is checked separately. Unsupported requests retain legacy output;
actual native enabled state and accepted-user-list buffer/texture counts are reported. Aliasing,
subresource ranges, native multi-queue handoffs and full GPU-validation acceptance remain open.
Portable tests and Linux compilation do not certify D3D12 execution. Source
`feat/v2-d3d12-enhanced-barriers`, Beads `nexora-45y.3.2`; hosted enabled/fallback evidence is pending.

Local validation on main `1d48e918`: full native/Slang/Zig Development configure/build and
Xvfb CTest **238/238, zero failures/skips,293.89 seconds** (Showcase on, graphical editor off).
Focused projection/native regression **6/6,0.71 seconds**; Shipping Monolithic/Minimal LTO passes.
Existing Vulkan native object/synchronization regressions **10/10,4.00 seconds**,25 layer insertions,
zero validation/VUID/sync markers. The private native adapter also passes Clang19 syntax checking
against separately staged Microsoft DirectX-Headers `461ef5138bd30922b44fbad77c4e47662812d2d5`;
this is SDK-identifier evidence, not native D3D12 execution, and adds no project dependency.

## Delivery gates

All complete gates remain **open**, including capability-dependent paths. A runtime fallback makes a device
usable but does not complete the accelerated implementation or its target-host gate.

| Gate | Planned V2 work | Dependencies | Acceptance and fallback |
| --- | --- | --- | --- |
| V2-GFX-1 | Capability/SDK/compiler matrix; parallel command recording; CPU-overhead benchmark | Existing V2-M3 contracts | Query independent feature tiers and shader targets. Give each recording worker exclusive command allocator/list ownership; retire allocators, descriptors and resources by completion fence. RenderGraph owns dependency order and submission. Compare serial/parallel output and run validation, lifetime stress and CPU/GPU timing. Keep serial recording selectable. |
| V2-GFX-2 | Tessellation and Enhanced Barriers | V2-GFX-1 capability and lifetime contracts | ✅ Bounded topology/stage/cache/cooked metadata and Linux native curved-patch/baked-mesh comparison (below). Finish broader patch/program assembly and hosted native evidence; retain baked fallback. Map graph access/sync/layout to Enhanced Barriers, including UAV, copy, aliasing and queue handoff cases; compare against the legacy barrier path under GPU validation. Keep both barrier paths. |
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

### Isolated modern compute programs — implementation in progress

The default-off `NEXORA_ENABLE_D3D12_PREVIEW` build stages separately hash-pinned Agility
1.721.3-preview, WARP1.65535.20-preview and DXC1.10.2605.37. Only the dedicated test executable
receives app-local WARP/Agility files; ordinary builds retain the existing SDK/compiler/profile.
An explicit D3D12 device option creates an independent SDK721 factory with singleton publication
disabled and enables experimental shader models only on that factory. It never applies factory
configuration to global D3D12 state. The factory outlives all device resources. A disposable CI
host enables Developer Mode; the backend does not modify OS settings.

Three fixed compute variants use existing SRVt0/UAVu1 roots: SM6.9 eight-component integer long
vectors, SM6.10 thread-scope F32 4x4 matrix/vector multiplication, and a128-thread reduction using
64 KiB shared storage plus `GroupSharedLimit(65536)`. The latter declares a static allocation
ceiling, not runtime dynamic shared-memory allocation. Native admission independently requires
the actual device profile, explicit6.10 permission/private runtime, F32 operation query or shared
capacity, actual DXIL stage/profile and exact build-generated recipe bytes. Permission and variant
participate in complete cache identity. One group, distinct128-byte input/32-byte output buffers
and ordinary immutable recording/revision lifetimes bound execution. Vulkan/Metal/validation
reject these DXIL recipes rather than claiming their execution.

Production compiler commands pass locally with the published preview compiler, including the
real65540-byte oversized-allocation negative. Actual emitted linear-algebra DXIL contains matrix
load/multiply operations. The Windows test requires all three native GPU outputs and canaries to
match CPU oracles, stale/abandoned-list rejection and unchanged accepted counters. Native Windows
execution and the full integrated CI gate remain pending. Generic modern shader authoring/cooking,
lower-profile compute fallback integration, DXR1.2/opacity micromaps/SER and physical-GPU performance
remain separate open work; this slice does not complete V2-GFX-5 or any enclosing acceptance gate.

Local integrated verification on the sampler-feedback parent passes Development220/220,zero failures/skips (132.25s), and Shipping Minimal/Monolithic/LTO. All three pinned public Windows archives verify their published SHA-256; the production compiler recipe also passes with the actual Windows package HLSL headers. Windows runtime execution remains pending.

After separating the sampler parent, the standalone PR633-based source passes Development219/219,zero skips/failures (154.39s), Shipping Minimal/Monolithic/LTO, and Xvfb Khronos object/synchronization17/17 (18.24s),37 confirmed layer insertions and zero Validation Error/VUID/SYNC-HAZARD markers. Non-Windows preview configure rejection and paired documentation checks pass. Native preview execution still requires Windows CI.

Prior head2f7d89c7 has both native Windows preview jobs117400353506/117402090855 passing1/1 (2.09/0.29s), actually loading app-local WARP SM6.10 with65536 shared bytes and F32 matvec support; all nine kernel outputs/canaries/lifetimes pass. A follow-up closes generic artifact admission when caller minimum is omitted: actual6.9+ DXIL must satisfy the device limit, and actual6.10 requires explicit permission/private runtime. The test adds generic unopted rejection/opted PSO creation without submission. Local Development219/219 passes again (122.37s); actual SDK admission helper and Windows-conditional test syntax pass. Guarded cleanup includes only three exact current graphics-session branches, owner/same-repository actors and obsolete live SHAs, preserving both latest-head events;25/25 cancellation-safety tests and16/16 documentation/workflow tests pass. Final follow-up Windows CI is required before merge.

### Bounded HitObject / SER semantic path — implementation in progress

A separate default-off `NEXORA_ENABLE_D3D12_SER` option requires the preview build and existing
Slang ray/compute artifacts. `RayTracingMode::ReorderedPipeline` uses a pinned DXC lib_6_9 recipe
with actual `HitObject::TraceRay`, `MaybeReorderThread` and `HitObject::Invoke`. Real DXC reflection
checks SRVt0/t1/UAVu1,32-byte ray input and16-byte hit/payload ABI; the manifest records compiler/
source/binary identity without claiming execution. A real same-profile changed-recipe negative
and required native exports/identifiers supplement metadata and exact-byte admission.

Published HLSL SER Device Support requires both SM6.9 and raytracing support. The guarded OPTIONS22
query independently reports whether the implementation actually reorders threads; false is valid
functional SER, including WARP. Unknown queries remain closed. Functional support, compiled recipe
and enablement remain separate. The existing bounded RT implementation ceiling stays1.1 until an
OMM path is implemented. A dedicated accepted SER-call diagnostic is published only after successful
submission and is a subset of ordinary ray dispatches. The existing dedicated roots/state/table,
immutable snapshots and generation/revision retirement are reused.

The native preview oracle extends ordinary ray/query fixtures with the SER mode for1/2/8 triangles,
1/64 rays, CPU/native compute hit/miss/nearest/barycentric/invalid parity, canaries, lifetime and
worker failure/cancellation/replay, legacy/enhanced barriers and changed/missing/wrong artifacts.
It requires actual native SER admission/calls and an independently available actual-reordering
query. WARP validates semantics and does not prove physical reordering, performance or coherence.
Native Windows proof remains pending; full DXR1.2/SER/performance acceptance remains open.

The SER candidate passes local native Slang/Zig Development219/219,zero failures/skips (163.27s), Shipping Minimal/Monolithic/LTO, and Xvfb Khronos object/synchronization17/17 (9.94s),37 confirmed layer insertions and zero Validation Error/VUID/SYNC-HAZARD markers. Actual production DXC preview compiles both normal/negative lib_6_9, validates real reflection/instructions and generates exact pins. Windows-conditional test and actual SDK OPTIONS22/factory syntax checks pass; this is not native Windows execution. Workflow routing16/16 and paired documentation checks pass.

### Bounded DXR1.2 opacity micromaps — implementation in progress

A separate default-off `NEXORA_ENABLE_D3D12_OPACITY_MICROMAPS` requires the pinned preview/SER
recipe. `CreateOpacityMicromapScene(triangles, masks)` accepts1–8 immutable finite triangles and
one low-four-bit mask each, encoding subdivision1 OC1 two-state micro-triangles in A/center/B/C
space-filling order. Unknown/any-hit states, higher subdivisions and mutable linkage are outside
this recipe. Native construction uploads128-byte-aligned mask input, eight-byte GPU descriptions
and R32 indices, queries actual bounded OMM-array prebuild requirements, then records actual OMM
array build/UAV dependency/linked BLAS build/UAV dependency/TLAS build/UAV dependency. The caller-
serialized construction fence completes before publication/scratch retirement. Linked OMM storage
is declared before BLAS/TLAS owners and each command-list scene snapshot retains all dependencies.

`RayTracingMode::ReorderedOpacityPipeline` independently requires actual RT1.2, SM6.9 and functional
SER. Its native config1 explicitly allows OMMs; the exact reflected SER library/root/payload/table
remain pinned. Scene/program opacity mismatches reject before native traversal, avoiding undefined
behavior from ordinary pipelines encountering OMMs. Only this additional compiled option raises
the bounded RT implementation ceiling from1.1 to1.2; SER alone does not. Native OMM calls publish
a separate accepted diagnostic after successful submission, included in SER and ordinary ray calls.

The oracle tests all16 masks on1/2 original triangles with1/64 rays, including every interior
micro-triangle, transparent-front/opaque-back nearest hits, misses, reverse/nonunit/invalid rays,
original primitive/barycentric results and canaries. An independent double CPU classifier is
cross-checked against real lower-profile GPU traversal of explicitly emitted opaque microgeometry,
including empty/all-transparent geometry; native OMM results must match both when admitted.
Eight-triangle all-opaque native cases reuse forced two-worker failure/cancellation/replay,
stale/destroyed/recreated scene/program/input/output, abandonment, eight-snapshot bounds and
Ray/Compute/Ray roots. Ordinary/OMM program/scene mismatches and missing/high mask bits reject.
Linux fallback and SDK syntax proof do not certify native OMM execution; Windows preview proof
remains pending. Four-state/any-hit, updates/compaction, general materials/lighting/cooking and
physical full DXR/Ultimate acceptance remain open.

The integrated OMM candidate passes local native Slang/Zig Development219/219,zero skips/failures (126.28s), Shipping Minimal/Monolithic/LTO, and Xvfb Khronos object/synchronization17/17 (10.50s),37 confirmed insertions,zero Validation Error/VUID/SYNC-HAZARD markers. The actual Agility721 OMM build/linkage/config1 helper and Windows-conditional SER/OMM tests compile with warnings-as-errors; paired documentation/workflow16/16 checks pass. Vulkan explicitly reports opacity native0 while executing the real microgeometry GPU oracle. Windows native OMM proof is pending.

### Native VRS Tier2 rate images — implementation in progress

The bounded VRS extension adds immutable `ShadingRateImageHandle` resources and an explicit
`ShadingRate::Image` pipeline mode. The independently queried tier2/tile size gates implementation
ceiling2.0; absent/invalid tile queries retain the1.0 ceiling and per-draw/full-rate alternatives.
Masks accept1x1/1x2/2x1/2x2 encodings only, raster extents1–4096 and exactly the queried tile grid.
Private R8_UINT storage avoids adding public texture/cooked formats. Creation validates bounded
copy footprints, copies caller bytes into a private upload, transitions to SHADING_RATE_SOURCE
and waits a caller-serialized creation fence before publishing/retiring temporary upload storage.

Each list retains up to16 unique immutable source snapshots before all native command-list
interfaces. Binding requires the selected image graphics pipeline and matching physical target
extent. Draw/indirect/mesh require an explicit image; pipeline rebind clears it and restores native
base/combiner/image state. Submit checks source handle generations before Close/Execute; abandoned,
stale and cancelled lists publish no image draw counters. Disjoint workers may share immutable
images. Image calls count accepted draws independently from per-draw coarse calls; no shading-
invocation/performance reduction is inferred.

The oracle compares real full-rate raster output with uniform/mixed images for RGBA/BGRA coverage,
32/255 gradient error and full-rate reset. It covers caller-memory mutation, missing/stale/recreated/
wrong-extent sources,16-snapshot bounds and forced two-worker failure/cancellation/replay. Dedicated
preview WARP tests require tier2 execution under legacy/enhanced color barriers, plus real DXC6.4
VS/PS artifacts that encode hardware `SV_ShadingRate` inputs. Requested1x1/1x2/2x1/2x2 values must
be observed on nontrivial covered pixels, allowing finer implementation rates at primitive edges.
The witness also mutates caller storage after creation. Native Windows evidence remains pending;
Vulkan/Metal explicitly run full-rate fallback and cannot certify native rate-image execution.
Additional4x rates, primitive-rate authoring, GPU-produced mutable images and physical budgets/full
Ultimate acceptance remain open. Default SDK/Slang profiles, stable C ABI and cooked formats remain
unchanged. Published WARP1.0.13 introduced VRS Tier2/feature level12_2; the pinned preview package
retains this documented route, which still requires actual output proof.

The rate-image candidate passes native Slang/Zig Development220/220,zero skips/failures (156.57s), Shipping Minimal/Monolithic/LTO, and Xvfb Khronos object/synchronization18/18 (10.50s),39 confirmed layer insertions and zero Validation Error/VUID/SYNC-HAZARD markers. Real DXC preview compiles the witness VS/PS6.4; actual SDK upload helper and Windows-conditional test syntax pass with warnings-as-errors. Vulkan reports enabled0/tile0/draws0 while executing real full-rate quality/workers. Four exact session branches now use guarded obsolete-run cleanup;27/27 safety and16/16 documentation/workflow tests pass. Native Windows image/witness execution remains pending and no full VRS/Ultimate gate closes.

DXR1.2/SER and tier2 images are integrated into one independent follow-up on the profile-runtime parent, sharing the optional native acceptance job. The first combined Development220/220 passes with zero skips/failures (122.61s), and Shipping Minimal/Monolithic/LTO passes. Actual Agility721 declarations confirm Work Graph tier1.1 has native value11; the portable decoder now preserves hardware1.1 while implementation remains capped1.0. Optional native acceptance enables the existing bounded Work Graph recipe with pinned preview DXC and supplies/depend-orders its library after test environment setup, extending Node/SER/Node interop without claiming mesh nodes. Final integrated regression and native Windows execution remain required.

Final integrated regression passes Development220/220, zero failures/skips (121.81s), Shipping Minimal/Monolithic/LTO, and Khronos object/synchronization18/18 (11.21s),39 confirmed layer insertions,zero Validation Error/VUID/SYNC-HAZARD markers. Actual Windows native SER/OMM/VRS2 acceptance remains pending.
