# Nexora Window and Native Presentation Roadmap

✅ Native device provenance delivery: Vulkan/DX12 reports retain the selected device name, vendor/device IDs and observed raw driver version; Metal retains its selected device name with unavailable IDs/driver. Decimal strings preserve DXGI 64-bit precision. Quality benchmarks retain identity in JSON/Markdown and reject differing observed devices/drivers; historical reports remain explicitly unidentified. Seven policy tests pass in normal and optimized Python. Build 1778 (source `46eac3a50f83`) passes all 18 jobs, Linux 145/145, Windows 128/128 and macOS 127/127, native Shipping gates and three quality measurements with actual llvmpipe/driver observations. Quality-benchmark GPU timestamps and display refresh remain unavailable; VIS stays 5/7 with physical GPU budgets and final art acceptance open. [Acceptance evidence](../../Apps/Showcase/evidence/VIS-M6-Native-Device-Provenance-CI-2026-10-07/acceptance.md).

> Version: v1.0 | Status: planning baseline | Updated: 2026-10-02

> **Progress: implementation complete** (WP-M0 through WP-M4 are implemented. Windows/DX12 acceptance for WP-M1/WP-M2 is recorded; Linux Showcase Vulkan composition now has non-skip Xvfb/lavapipe acceptance; physical-display, Windows/Vulkan, and macOS/Metal still require target-host runners.)

## 1. Purpose and ownership

This roadmap owns the missing OS-window and window-system presentation boundary shared by the Zig
Showcase, the graphical Editor, and future game executables. RHI continues to own GPU resources and
queues; a platform window module owns native handles and events; the presentation adapter owns the
swapchain and converts window changes into backend-neutral surface events. Zig gameplay never
receives a native window, device, queue, or swapchain pointer.

## 2. Current baseline

- ✅ Optional native GPU timing now publishes copied completed submission IDs and optional
  milliseconds from Vulkan/DX12 timestamp queries and completed Metal command buffers. Editor
  opts in; other surfaces keep the no-query default. Linux real query/failure/fence/resize/drain
  and default opt-out contracts pass; physical calibration and Showcase benchmark ingestion remain
  separate. [Editor evidence](../../Tools/Build/evidence/EditorEDM6-NativeGpuTiming-Linux-2026-10-09.md).

- ✅ Native UI replacement descriptors are now fence-recycled on DX12; Vulkan's bounded UI pool
  supports 64 mutable Editor images and in-flight generations. The Editor native lifetime gate
  exercises more than 4096 uploads. Target-host visual acceptance remains separate.

- ✅ X11 modifier events report post-transition flags, preserve held left/right partners, and
  clear tracking on focus loss/destruction. The native Xvfb gate verifies all four modifier families.

- ✅ Validation and native RHI devices execute deterministic offscreen workloads.
- ✅ Renderer scene extraction and offscreen `Present` state validation are covered by tests.
- ✅ `NexoraShowcase` preserves its headless lifecycle and can own a reusable native render surface.
- Implemented: Win32 window/input translation and DX12 presentation have WP-M1/WP-M2 Windows acceptance evidence; Showcase integration remains a separate target-host gate.

✅ `RenderSurface::UiResourceDomain()` now exposes an opaque process-local cache identity,
stable across resize/move and invalid after teardown. Editor native replacement/DPI tests reject
stale upload acknowledgements without caching an owner or exposing native image handles.

## 3. Required contracts

- Backend-neutral `WindowDescriptor`, `WindowHandle`, `WindowEvent`, and `SurfaceDescriptor` types.
- Explicit ownership: the application owns the window; the presentation surface cannot outlive it;
  GPU work is drained before swapchain or window destruction.
- Threading rules for event pumping, resize, fullscreen transitions, and render submission.
- Recoverable results for unsupported backend, surface loss, out-of-date swapchain, minimized/zero
  extent, device loss, and display/DPI changes.
- Input snapshots derived from timestamped window events without exposing platform message structs.
- Native types remain in private platform/backend translation units.

## 4. Milestones

### ✅ WP-M0 — Contract and module boundary

- Define the public window/surface descriptors, events, errors, ownership, and threading contract.
- Add feature options and module-graph declarations without making headless builds depend on a window SDK.
- Add fake-window and fake-surface lifecycle, resize-coalescing, zero-extent, and teardown tests.

Delivered evidence: `Nexora::Window` and `Nexora::Presentation` expose only backend-neutral public
types; their ownership, lifetime, threading, resize, and recovery rules are recorded in module
READMEs. Both modules are guarded by `NEXORA_ENABLE_WINDOW_PRESENTATION`, their dependencies are in
the validated module graph, and `window_presentation.contracts` exercises the fake lifecycle gate.
No native window or swapchain backend is claimed by this milestone.

### ✅ WP-M1 — Win32 window and input

- Create/close/show/resize a Win32 window with DPI-aware client sizing and a deterministic event pump.
- Translate keyboard, text/IME, pointer, wheel, focus, and close events into backend-neutral snapshots.
- Cover repeated create/destroy, resize storms, minimize/restore, and shutdown while events are queued.

### ✅ WP-M2 — DX12 swapchain presentation

- Create, acquire, render to, resize, and present a DXGI swapchain without leaking DXGI/D3D12 types.
- Define backbuffer/fence ownership, frames in flight, vsync/tearing policy, color format, and present diagnostics.
- Handle occlusion, zero extent, surface loss, device removal, and failed resize without corrupting the active generation.

Windows acceptance evidence is produced by `window_presentation.contracts`: it creates a real Win32 window, acquires, clears, and presents four DX12 frames, resizes the swapchain, and asserts diagnostic counters. On 2026-09-28, the local x64 `windows-dx12-development` preset built the native D3D12 path with Vulkan disabled and passed this contract, recording the WP-M1/WP-M2 target-host evidence. Linux contract results or screenshots alone are insufficient.

### ✅ WP-M3 — Showcase and Editor integration

- Run `NexoraShowcase --mode=interactive --backend=dx12` with visible output and input.
- Provide reusable render surfaces for Scene/Game views without making Runtime depend on Editor.
- Preserve the deterministic Linux headless path and make fallback reasons visible rather than silent.

Delivered evidence: `NexoraShowcase --mode=interactive --backend=dx12` creates the reusable
Presentation-owned `RenderSurface`, pumps input, and presents until close; bounded frame runs are
available for automation. The same public owner is usable by Editor Scene/Game views without adding
an Editor dependency to Runtime. Auto fallback emits and records its reason, explicit DX12 failure
does not fall back, and non-Windows contract tests retain the deterministic headless gate. Actual
Windows/DX12 execution of the Showcase command remains target-host acceptance evidence rather than a Linux-cloud claim.

### ✅ WP-M4 — Additional platforms and hardening

- Add Vulkan window-system surfaces on supported Linux/Windows hosts and Metal presentation on macOS.
- Validate multi-window/multi-surface lifetime, HDR/color-space negotiation, fullscreen, hot-plug, and long-run resize/device-loss stress.
- Record target-host evidence separately; cross-compilation alone is not runtime validation.

Delivered evidence: Linux uses an X11 window implementation and Vulkan WSI swapchain; Windows can select Vulkan alongside DX12; macOS uses a Cocoa window and `CAMetalLayer`. Backend negotiation records the selected present mode and color space, while fullscreen, multi-surface lifetime, 2,048-cycle resize stress, zero extent, out-of-date, surface-loss, and device-loss paths are covered by the portable contract gate. These sources and cross-platform contracts complete the implementation scope; WP-M1/WP-M2 Windows/DX12 runtime acceptance is recorded, and ✅ Linux Showcase Vulkan composition passes non-skip Xvfb/lavapipe acceptance on 2026-10-02 (67/67 Development tests). Physical-display and other platform runtime acceptance remain explicitly separate target-host gates. See the [acceptance record](../../Apps/Showcase/evidence/V1-Phase-A-Linux-Vulkan-2026-10-02/acceptance.md).

## 5. Validation and Definition of Done

- Unit/contract tests cover ownership, event ordering, resize, zero extent, and failure injection.
- A Windows/DX12 runner proves real acquire/render/present and captures diagnostics; a screenshot is
  supplemental visual evidence, not the correctness oracle.
- Linux headless configure/build/test remains independent of desktop display availability.
- Windowed shutdown produces no live GPU resources, queued callbacks, or native handles.
- The Showcase and Editor consume only public window/presentation contracts.

✅ The Linux/Vulkan scene boundary also passes native indexed/depth/light/transform pixel acceptance and 600-frame normalized camera-input integration under Xvfb/lavapipe (70/70 Development tests). See the [Phase B acceptance record](../../Apps/Showcase/evidence/V1-Phase-B-Linux-Vulkan-2026-10-02/acceptance.md). Physical-display and other-platform gates remain separate.

## V1 Visual Showcase follow-through (2026-10-03)

✅ The Linux/Vulkan application now presents eight live Runtime room views, readable native GPU UI,
held keyboard/pointer controls and a 210-second guided tour under Xvfb/lavapipe. Full package tooling
includes content, deterministic ZIP/SHA-256 and isolated Linux shared-library resolution. These are
V1 Visual Showcase integration results; they do not replace this roadmap's existing platform gates
or establish the expanded Windows version's clean-machine graphical acceptance.

✅ Native SceneDrawData hardware instance records and bounded fence-owned uploads are implemented in Vulkan/DX12. Linux Vulkan pixel acceptance verifies independent translation/scale/tint; DX12 target-host execution remains pending.

✅ Native Vulkan/DX12 SceneDrawData draws now accept an in-bounds physical-pixel viewport for a docked Scene View. Portable bounds checks and Linux Vulkan pixel readback verify clipping; Editor integration and DX12 target-host viewport evidence remain open.

✅ Scene UV/RGBA8 material binding now has bounded immutable scene-only texture IDs, white fallback and fence-protected upload lifetime in Vulkan/DX12. Linux pixels verify sampler selection, cache reuse and resize resubmission; target-host DX12 material execution remains pending.

✅ Native-owner Offscreen → Main → UI → Present now uses fence-owned scene color and GPU copy in Vulkan/DX12, with duplicate acquire and pending-copy ordering rejected. Linux Vulkan pixel, interaction and synchronization-validation gates pass 75/75. The packaged Windows `accept-v1.ps1` records isolated-copy screenshots, checksums and native counters; the user will perform expanded physical-display/clean-host acceptance locally.

✅ Expanded Windows Full Shipping/DX12 isolated-copy graphical acceptance passes in [CI run 37053279518](https://github.com/jimlee1972/Nexora/actions/runs/37053279518): 14 checksums, 12 visible screenshots, sampled asset/plugin rejection, 339 native graph/copy/present frames and exit code 0. [Versioned Windows CI evidence](../../Apps/Showcase/evidence/Windows-V1-Native-Graph-CI-2026-10-03/acceptance.md). The hosted VM does not accept physical-display or independently provisioned clean-host gates; these remain the user’s local work.

✅ Windows DX12 local Development/Full and Shipping/Full native execution is recorded in
[Windows-V1-DX12-Local-2026-10-03](../../Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md).
The GTX 960 developer-machine run covers eight rooms, primitive/texture/instance rendering,
native offscreen graph/UI/present, input/edit/reload/animation captures, Lab asset/plugin rejection
and the complete 210-second guided tour. The explicit Development preset and stock PowerShell 5
default-path fix make the documented local workflow reproducible. Clean-host and physical-display
operator attestations remain pending; this does not complete V1's final acceptance.
✅ The Windows local verifier now requires fresh exports, successful edited-scene snapshot round trip,
reset tour progress and the exact sampled M5/M6 rejection inputs/outputs. Source/package hashes use
pinned LF bytes. Baseline e4a140139189 passes Linux Development 77/77 and Shipping package CI;
physical-display and clean-host final gates remain pending.

✅ SceneDrawData now supports bounded index/instance mesh batches inside one Vulkan/DX12 depth
pass. Empty batches preserve the existing draw. Portable overflow/bounds checks and Vulkan distinct-
geometry pixels cover range offsets and invalid-then-valid recovery; DX12 execution remains a target gate.

Metal Showcase scene/instance/material/depth/copy and Cocoa control/Retina source were extended on 2026-10-04. ✅ macOS hosted Shipping/Full isolated packages now execute eight rooms through Metal scene/copy/UI/present and resize (96 frames; [record](../../Apps/Showcase/evidence/V1-Metal-Hosted-CI-2026-10-04/acceptance.md)). ✅ Native pixel/input/depth/lifecycle CTest passes macOS Development (73/73) and mimalloc (63/63); physical Mac visuals remain pending. Linux distribution regression remains accepted at 80/80 with no skipped native gates. See [distribution evidence](../../Apps/Showcase/evidence/V1-Distribution-Linux-2026-10-04/acceptance.md).

✅ Optional row-major affine SceneInstance matrices and common private model/inverse-transpose
packing are implemented for Vulkan/DX12/Metal ([ADR-0003](ADR-0003-Presentation-Affine-Instances.md)).
Portable tests check exact points, normals, override semantics, exact determinant cancellation
classification and malformed input; Vulkan pixels
compare mirrored/sheared instances against independently baked geometry/normals. Legacy TRS,
empty identity, batch budgets and fence ownership remain compatible. Editor Scene/Game now
consume exact matrices through the CPU gate; physical-display/GPU acceptance remains separate.

✅ Vulkan Scene/Game uploads now reuse bounded capacity in fence-protected frame slots.
Steady, smaller and Scene-free frames retain allocation; growth stages replacement before retiring
old storage, and resize/teardown drains GPU work. Native call tracing and pixels verify 100 steady
frames, maximum descriptor budgets, failed growth and leak-free lifetime. Fresh geometry is still
copied per legacy draw; optional Vulkan mesh residency is extended below. Cross-backend
persistent per-asset GPU caching and full graphical acceptance remain open.

✅ Vulkan now optionally retains exact immutable mesh geometry independently of combined frame
layout. Editor Scene/Game use the capability; bounded keys/native allocations, current-draw leases
and completion-safe LRU eviction preserve failure fallback, resize reuse and teardown. Native
tracing/pixels and portable contracts cover canonical identity, changed geometry, fresh parameters,
allocation/mapping/completion failure and memory bounds. DX12/Metal residency remains open.
[ADR-0008](ADR-0008-Editor-Mesh-Geometry-Residency.md) ·
[Evidence](../../Tools/Build/evidence/EditorEDM2-VulkanMeshResidency-2026-10-13.md).

✅ DX12/Metal now support immutable mesh residency with Vulkan's shared canonical keys and
bounded duplicate/failure admissions. Native requirement/size accounting, completed retirement,
current-draw leases and legacy-prefix invalidation preserve fresh frame data and resize reuse.
DX12 native PBR and Metal Scene/UI/shadow/reflection guards request residency and switch modes;
portable admission tests and Vulkan tracing retain allocation/completion failure evidence. Actual
hardware OOM, broader policy and full target-host acceptance remain open.
[Evidence](../../Tools/Build/evidence/EditorEDM2-NativeMeshResidency-2026-10-13.md).

✅ All native adapters now resolve 4,096 mesh bindings with fixed metadata and at most 64 distinct
range admissions. Large repeated batches retain exact geometry reuse; extra ranges fall back
individually, retaining prior success/failure memo results. Portable full-limit bounds and real
native pixels verify this extension without increasing GPU entry/memory limits.
[Evidence](../../Tools/Build/evidence/EditorEDM2-FullBatchResidency-2026-10-13.md).

## Vulkan FIFO availability and presentation synchronization (2026-10-18)

✅ Shared Vulkan presentation now waits for FIFO image readiness with one reusable fence before
admitting CPU frame work. Immediate remains asynchronous. Presentation semaphores belong to
images; fence-protected uploads/timestamps follow the descriptor's bounded frame slots. Default
two-image VSync and public C++/C/Zig layouts are unchanged. Initial SUBOPTIMAL extent recovery and
partial allocation cleanup have native regression coverage. Full local MSVC Development/Shipping,
native lifecycle/fault tests and both six-case Vulkan physical 2D repeats pass.
[Evidence](../../Engine/TwoD/evidence/Windows-M6-VulkanPacing-2026-10-18/acceptance.md) separates per-frame diagnostics, formal screening and final PR CI.
This does not accept unrelated Showcase visual budgets, input-to-display latency, arbitrary GPU-heavy
throughput, production minimum-PC/soak or clean-host gates.
That prior mixed screen recorded 23/24, including a retained DX12 sparse maximum of 72.36 ms above 66.67 ms.

## DX12 VSync queue admission (2026-10-18)

✅ Shared DX12 VSync owns one DXGI waitable handle and admits each CPU frame after presentation
capacity is available. Queue capacity follows the existing two/three slots, preserving CPU/GPU
overlap; separate GPU fences still own allocator/upload/timestamp/retirement reuse. Immediate
retains asynchronous acquisition. Resize retains creation flags/handle, zero extent suspends
before waiting, wait failure rejects before slot reset, and teardown closes the handle once.
Native two/three-slot, first-frame, wrong-thread, wait-failure, resize/resume and abandoned-frame
regression passes; it rejects the old adapter for missing admission. Public headers/C/Zig ABI
and default buffer count are unchanged by this DX12 change.

The physical original-load 2D screen is now 24/24 with optional buffered per-frame evidence;
33/33 related local contracts and complete Development/Shipping builds pass.
[Evidence](../../Engine/TwoD/evidence/Windows-M6-DX12Pacing-2026-10-18/acceptance.md) retains the rejected single-queued-frame regression and the historical
72.3556 ms DX12 failure, whose unique cause remains untraced. Final-head hosted gates are on
[PR #644](https://github.com/jimlee1972/Nexora/pull/644). This does not accept Showcase visual
budgets, arbitrary GPU-heavy throughput, input-to-display latency, full M6 or minimum-PC/soak.

## Integrated 2D soak follow-up (2026-10-18)

The next fixed same-PC suite retains all original 10k workloads and adds UI/VFX plus 30 measured
minutes per native backend. Local related contracts pass 34/34 and all native functional/sample
checks pass, but current physical performance is **NOT ACCEPTED**: original23/24, integrated3/4,
soak1/2. DX12 soak cycle p99/max is 28.4662/62.8658 ms; Vulkan is 27.7955/**134.9232 ms**.

Vulkan frame24,415 spends 113.6250 ms in EndFrame; matching GPU submission24,416 measures
0.448736 ms. No reload/resource poll occurs on that frame. The boundary includes command
finalization, queue submit/present and metric reads; no individual API or OS/driver cause is
proved. No native API, buffer count, VSync policy or engine ABI changes in this follow-up.
[Raw evidence and retained failures](../../Engine/TwoD/evidence/Windows-M6-IntegratedSoak-2026-10-18/acceptance.md);
`nexora-j4q.2.6` tracks native API/thread-cycle/scheduling correlation. The older24/24 result is
historical evidence, not proof that every later or longer run passes. Full M6 remains open.

## M6 current-PC acceptance load (2026-10-19)

The user revised the required current-PC M6 integrated load to 5,000 ordinary objects; 1,000 is
functional smoke and 10,000 remains additional stress coverage. Keep the UI/VFX/query/update/reload
functions and existing 30 FPS/spike/resource limits. See the [current M6 plan](2D_Roadmap.md#m6-current-pc-workload-revision-2026-10-19).
The benchmark/runner now offers explicit `--objects 5000`, retaining default 10,000 stress and
1,000 functional smoke. All original six-case correctness/timing verdicts remain retained; only
cases at or below the selected load gate performance. Fresh 5,000-object integrated acceptance is NOT RUN, so M6 remains open.
Historical 10k failures and native submit/present follow-ups remain retained; target reduction
does not prove a stall fixed. Close M6 only after the revised required checks have passing evidence.

## SP-01 standalone display integration (2026-10-23)

✅ Portable implementation and regression checks: 252 passed, 16 native-prerequisite skips, 0 failed; Monolithic Shipping consumer passed.

The standalone 2D Player now has optional project window settings, an append-only independently
negotiated C/Zig display table, frame-stable pixel/logical/DPI snapshots, native window mode
requests, game-selected aspect bounds, safe-frame extraction/scissors and DPI-correct UI/input.
Historical service consumers and descriptors retain their prefixes/defaults. World camera span,
HUD layout and UI scale remain game-owned. This does not add Editor Play display wiring.

See [the display contract](../../Engine/TwoD/GameplayDisplay.md),
[Linux evidence](../../Tools/Build/evidence/SP01-GameplayDisplay-Linux-2026-10-23.md) and
[Windows procedure](../../Tools/TwoD/WindowsDisplayAcceptance.md).
Windows physical DPI/click/mode acceptance is NOT RUN / NOT ACCEPTED; 2D M6/M7 and existing
milestone counts remain unchanged. Portable implementation checks do not close those gates.

✅ Native X11 close-race regression: destroyed-before-pump metrics and cleanup pass on Xvfb; the preserved pre-fix DSO fails the same probe.

## SP-06 gameplay lifecycle, close handshake and rumble (2026-10-23)

✅ Optional Lifecycle V1 preserves every previous services prefix (including SP-05's 88 bytes),
with enforced C/Zig field order and callback metadata. Retained focus/minimize/power/disconnect
sequences are visible before fixed updates; an explicit guard routes native close through a token
that Zig can cancel or accept after a real successful save. Legacy close behavior is preserved.
✅ Windows translates suspend/resume and noncoalesced minimize/restore notifications. Bounded
XInput rumble reports support/native errors, expires on owner ticks, and requests zero on blur,
minimize, suspend, close and Stop. DirectInput/DS4/Linux rumble remains unsupported.
✅ A real Zig V3 diagnostic retains a pending marker, pauses before fixed simulation, retries failed
atomic snapshots, reopens paused, and requires explicit confirmation plus a three-second preparation.
Portable tests cover native-output failure retry, token/prefix/thread guards and actual Storage
failure preservation; the independent exported Player exercises negotiation/UI/ownership.

See [lifecycle contract](../../Engine/Runtime/GameplayLifecycle.md) and
[cloud evidence](../../Tools/Build/evidence/SP06-Lifecycle-Cloud-2026-10-23.md).
Final game card/transaction integration and physical Windows focus/minimize/controller/sleep/rumble
acceptance remain open (`nexora-4j7.1`). Message injection is not actual OS sleep or motor sensation.
This does not complete SP-06 product acceptance, M6/M7, or unrelated SP gaps.

Fresh post-foreground movement is retained; UI confirmation still waits for neutral rearming.
