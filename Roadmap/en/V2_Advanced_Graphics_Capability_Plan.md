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

General parallel recording, fence-retired worker descriptors and allocators, compiler/artifact matrix
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
