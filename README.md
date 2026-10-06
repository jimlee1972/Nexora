# Nexora

✅ Linux Development full 97/97 tests passed (111.61 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.69 s wall time). Production freeze `79540805777114ab26d77dadda2258db39d43046`; evidence: `Apps/Showcase/evidence/VIS-Device-Inlays-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Courtyard device detail follow-up: original ellipsoidal bronze rivets and raised rune frames enrich the hero ring. Annular wedges now retain outward triangle winding after their orientation-reversing bend. The contained mineral core has bounded deterministic fracture offsets with normals derived from actual face geometry; clearer glass and warm ceramics refine the authored palette. Linux Development configure/build and full 97/97 tests passed (111.61 s, Khronos core/sync validation); Shipping verification passed. VIS remains 5/7; reference parity remains open.

✅ Linux Development full 97/97 tests passed (112.34 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.84 s wall time). Production freeze `d5676eea058ecb999979120119f2ac36c6d5bad9`; evidence: `Apps/Showcase/evidence/VIS-Background-Depth-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Courtyard background depth follow-up: upper tower stories have actual open apertures between instanced corner piers. The ridge shares its continuous grid vertices, freeing 6,791 native vertices without changing its authored extent or height function. Stone uses existing uniform material AO (foreground 0.45, background 0.5); lighter neutral background stone and reduced distant haze improve depth. Linux Development configure/build and full 97/97 tests passed (112.34 s, Khronos core/sync validation); Shipping verification passed. VIS remains 5/7; reference parity remains open.

✅ Linux Development full 97/97 tests passed (110.56 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.73 s wall time). Production freeze `5615a2d816db60f099792eb8abe2ba09585427c3`; evidence: `Apps/Showcase/evidence/VIS-Arcade-Stones-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Courtyard architecture follow-up: replace tube-shaped arcade spans with original jointed bevelled voussoirs, sharing 20 wedge meshes across four arches. Courtyard-facing column relief is visible from the wide composition. Normals use the positive bend inverse transpose; Linux Development configure/build and full 97/97 tests passed (110.56 s, Khronos core/sync validation); Shipping verification passed. VIS remains 5/7 and reference parity remains open.

✅ Linux Development full 97/97 tests passed (110.29 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.81 s wall time). Production freeze `040a3acf7c07351b98b13f2e0c5c89e4d1d3584b`; evidence: `Apps/Showcase/evidence/VIS-Crystal-Proportions-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Courtyard crystal proportion follow-up: display the existing cooked shell at scale (0.6, 0.8, 0.6) centered at Y=3.15, with a narrower opaque core, inverse-transpose normals, matching splinter proportions and aligned light/focus anchors. The stone material uses the existing uniform ambient-occlusion scalar at 0.65, preserving foliage IBL. F10 anti-aliasing is discoverable in the C comparison menu. Linux Development configure/build and full 97/97 tests passed (110.29 s, Khronos core/sync validation); Shipping verification passed; VIS remains 5/7 and preview parity remains open.

✅ Linux Development full 97/97 tests passed (109.50 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.89 s wall time). Production freeze `19899040d0c70d3ba2053bd918cae03230bed88f`; evidence: `Apps/Showcase/evidence/VIS-Courtyard-Coping-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Courtyard coping and shadow follow-up: replace the three perfect pedestal rims with 72 original jointed, bevelled stones over recessed supporting cores, with bounded radial/height variation and transformed normals. The 24×20 directional projection includes both side-arcade crowns. The light orientation, tier resolutions, shader packets, assets and shared animation clock remain unchanged. Linux Development configure/build and full 97/97 tests passed (109.50 s), including native shadow comparison/restoration with Khronos core/sync validation; Shipping verification passed; VIS remains 5/7 and reference parity remains open.

✅ Linux configure/build and full 97/97 tests passed (108.23 s, Khronos core/sync validation), including nine evidence-policy tests and 600 actual native camera frames. Isolated original Shipping acceptance passed. Interaction synchronization now waits for complete Lab JSON/Markdown export, holds D through the fixed 600-frame camera run, and requires stable foreground pixels at the resized viewport scale before shutdown. The resize-generation, camera-movement, pixel-restoration, frame-count and timeout gates remain in force. Runtime/movie freeze remains c815263b; evidence: `Apps/Showcase/evidence/VIS-Courtyard-Valley-Linux-2026-10-05/interaction-synchronization/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Release-verifier diagnostic follow-up: failed native/headless subprocesses now print a bounded stderr tail and the structured failure result to CI logs. Acceptance gates, exit status, runtime and the recorded Shipping/movie freeze are unchanged. Linux configure/build and full 97/97 tests passed (104.27 s) with Khronos core/synchronization validation; the original Shipping package passed isolated native acceptance. Evidence: `Apps/Showcase/evidence/VIS-Courtyard-Valley-Linux-2026-10-05/release-diagnostics/`. VIS remains 5/7; preview parity and physical-display acceptance remain open.

MSVC compiler follow-up: foreground sprigs use `sprigRadius` to avoid camera-member
shadowing under /WX. Exact source comparison after identifier normalization is unchanged.
✅ Linux configure/build and full 97/97 pass (102.71 seconds), including 85 native PBR
frames with core/sync validation. Evidence: `VIS-Courtyard-Masonry-Linux-2026-10-05/msvc-member-shadowing/`.
Existing Shipping/movie keep their recorded production freeze; Windows CI recheck is pending.


> Open-source cross-platform 3D engine architecture and roadmap  
> 開源跨平台 3D 引擎架構與 Roadmap

## English

Nexora is an open-source cross-platform 3D engine initiative focused on a high-performance C++20 core, Zig gameplay, a language-neutral stable C ABI, modern rendering, scalable world systems, and AI-assisted engineering.

This repository contains an executable C++20 engine/runtime baseline in addition to its architecture and implementation plans. The roadmap documents are available in English under `Roadmap/en/`, with their original Traditional Chinese editions preserved under `Roadmap/zh-TW/`.

### Direction

- Windows, macOS, Android, and iOS support
- C++20 engine core with Zig as the primary gameplay language
- DX12, Vulkan, and Metal through a unified RHI direction
- Node + Component authoring with data-oriented runtime storage
- GPU-driven rendering, large-world streaming, networking, animation, physics, AI, and editor tooling as staged capabilities
- AI-assisted development with automated validation gates, reproducible builds, and human review

### Roadmap

See the bilingual document index in [`Roadmap/README.md`](Roadmap/README.md).

Progress is measured against each document's explicit milestone acceptance gates, not by counting
paragraphs or treating a planned scope matrix as implementation. Completed acceptance items contribute only when evidence exists; partial work is recorded inside
the roadmap and never rounded up to a completed milestone. The V1
percentage is specifically the portable contract-foundation scope described below, not production
completion on every target platform.

Completed roadmap items use the green `✅` marker. Every content change must re-evaluate affected
roadmap items and keep this GitHub README's progress and status text synchronized with the evidence;
an unchecked or unmarked item remains incomplete.

| Roadmap | Progress | Basis |
| --- | ---: | --- |
| ✅ [V1 Complete Plan](Roadmap/en/Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md) | **100%** | 13/13 portable M0–M12 contract foundations delivered; native/product adapters remain separate gates. |
| ✅ [V1 AI Implementation Plan](Roadmap/en/Cross-platform_3D_Engine_V1_AI_Implementation_Technology_and_System_Plan_v1_2.md) | **100%** | Tracks the same accepted portable V1 implementation baseline. |
| [V2 Complete Plan](Roadmap/en/Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md) | **46%** | ✅ V2-M0 through ✅ V2-M2 and ✅ V2-M4 through ✅ V2-M6 are accepted. V2-M7 now includes bounded multi-world self-play coordination but remains in progress; V2-M9 through V2-M12 also have unaccepted portable foundations, while V2-M3 and V2-M8 remain open. |
| [V2 AI Implementation Plan](Roadmap/en/Cross-platform_3D_Engine_V2_AI_Implementation_Technology_and_System_Plan_v1_2.md) | **46%** | Tracks the same accepted V2 baseline plus the in-progress V2-M7 and V2-M9–M12 portable foundations; production backends, distributed infrastructure, cross-device diagnostics, and hardening acceptance remain open. |
| [V3 Complete Plan](Roadmap/en/Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md) | **0%** | No V3 delivery milestone has an accepted repository gate. |
| [V3 AI Implementation Plan](Roadmap/en/Cross-platform_3D_Engine_V3_AI_Implementation_Technology_and_System_Plan_v1_3.md) | **0%** | Execution plan only; no V3 milestone accepted. |
| ✅ [Engine API Foundation](Roadmap/en/Engine_API_Foundation_Roadmap.md) | **100%** | ✅ API-M1 through ✅ API-M6 complete for portable scope. |
| ✅ [Window and Native Presentation](Roadmap/en/Window_Presentation_Roadmap.md) | **100%** | ✅ WP-M0 through ✅ WP-M4 are implemented; Windows/DX12 WP-M1/WP-M2 runtime acceptance is recorded, while Linux Showcase Vulkan geometry and composition have Xvfb/lavapipe acceptance; physical-display and other native-host acceptance remain separate gates. |
| ✅ [Zig Showcase](Roadmap/en/Zig_Showcase_Roadmap.md) | **100%** | ✅ ZS-M0 through ✅ ZS-M5 are complete; the independently provisioned Windows clean-machine Development package acceptance is recorded with 16/16 checksums and a PASS launch report. |
| [Graphical Editor](Roadmap/en/Editor_Roadmap.md) | **0% (0/8)** | Repository audit confirms portable foundations for every ED track and an in-progress Dear ImGui shell, but no graphical ED milestone has passed all automated and target-host gates. |
| [Focused Roadmaps AI Plan](Roadmap/en/Focused_Roadmaps_AI_Implementation_Plan.md) | **60%** | Mean of API 100%, Zig Showcase 100%, and Editor 0%, rounded down to 10%. |
| [V1 Visual Showcase](Roadmap/en/V1-Visual-Showcase-Long-Term-Plan.md) | **Linux/Windows developer, clean-VM and physical-display slices verified** | ✅ Native 3D/UI, eight Runtime rooms, Validation Lab, sampled materials, hardware instances and offscreen RenderGraph; ✅ Windows DX12 Development/Full and Shipping/Full local screenshots, JSON and 210-second tour. Windows Development CTest 68/68 and Linux Development CTest 77/77. Clean Windows 10 VM (VirtualBox) launch and GTX 960 physical-display run accepted (the attestation flag was supplied by Claude on the user's instruction); Windows Vulkan physical-display run on the GTX 960 also passed (CI-built package); the per-tag release workflow has run on real test tags (rc.3 draft with Windows DX12/Vulkan, Linux x64 and macOS ARM64 packages); Mac remains incomplete and is deferred at the user's request, so V1 final acceptance is not complete. |

✅ [Linux Full distribution acceptance](Apps/Showcase/evidence/V1-Distribution-Linux-2026-10-04/acceptance.md): Development 80/80 with five non-skipped native gates, Minimal/Full Monolithic builds and checksum-verified isolated native package screenshots. Metal scene/input source, Mac package relocation and Linux/macOS release/CTest evidence jobs are now implemented; ✅ [macOS hosted Shipping/Full acceptance](Apps/Showcase/evidence/V1-Metal-Hosted-CI-2026-10-04/acceptance.md) now verifies Metal compilation and eight-room native graph/resize execution from isolated packages (96 frames). The native pixel/input/depth/lifecycle gate also passes macOS CTest (73/73 Development; 63/63 mimalloc); physical Mac visuals and clean-host deployment remain pending, so full V1 acceptance stays open.

✅ [Windows Vulkan hosted-driver acceptance](Apps/Showcase/evidence/V1-Windows-Vulkan-Lavapipe-CI-2026-10-04/acceptance.md): Mesa lavapipe 26.2.4 passes 14 checksums, 25 screenshots, nine interaction checks and the complete 210.006-second tour. Test tag rc.4 passes Build 16/16 and Release 5/5, retaining a separate Vulkan archive in the 17-asset unpublished draft. This is software-driver coverage; Mac remains incomplete and deferred.

✅ [Desktop tag rc.3 acceptance](Apps/Showcase/evidence/V1-Desktop-Tag-RC3-2026-10-04/acceptance.md): Build 16/16 and Release 5/5 pass after recorded targeted retries; the unpublished draft retains 16 attachments, four verified ZIPs and desktop CTest logs (Linux 80/80, macOS 73/73, Windows 72/72).

✅ Build run [37053279518](https://github.com/jimlee1972/Nexora/actions/runs/37053279518) passes all jobs for source revision `ecf2277f07bc51ef09112c652a0ccbc0511d0a99`, including Windows Full/DX12 isolated-copy visual acceptance, desktop Development, mimalloc and sanitizer contracts. [Versioned Windows evidence](Apps/Showcase/evidence/Windows-V1-Native-Graph-CI-2026-10-03/acceptance.md) retains screenshots, native counters and provenance. Physical-display and independently provisioned clean-host acceptance remain the user’s local gates.

Windows DX12 local delivery: [acceptance record](Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md). Use `windows-showcase-development` for Development/Full and `windows-showcase-shipping` for Shipping/Full; `-CompleteGuidedTour` verifies all 210 seconds. Developer-GPU evidence remains separate from clean-host and physical-display operator attestations. Fresh JSON verifies F5 snapshot round trips, tour replay resets and sampled Lab error cases. Clean-VM record: [acceptance](Apps/Showcase/evidence/Windows-V1-CleanVM-VirtualBox-2026-10-03/acceptance.md) (virtual GPU, not a physical display). Physical-display record: [acceptance](Apps/Showcase/evidence/Windows-V1-PhysicalDisplay-GTX960-2026-10-03/acceptance.md).

The courtyard is being expanded toward the approved concept: a golden-hour six-face skybox,
layered original background ruins/mountains, rooted vine wind, live crystal/splinter/water motion,
256x256 detail maps, HDR bloom and depth-aware focus. Reference art parity remains open;
implementation evidence does not establish pixel-identical concept matching or physical target acceptance.
The incremental golden-hour version passes Linux 97/97, both Shipping profiles, isolated native
launch and 45 PBR frames; its 100.27-second actual movie and nine same-executable quality runs
are retained in [background/HDR evidence](Apps/Showcase/evidence/VIS-Background-HDR-Linux-2026-10-05/README.md).

The [V1 Visual Identity Showcase Roadmap](Roadmap/en/V1-Visual-Identity-Roadmap.md)
records the ✅ user-confirmed art direction and retained concept preview for a stylized ruins courtyard,
with warm sunlight, cool shadows, turquoise runes, and a [free-model/texture shortlist](Roadmap/art/Free-Asset-Sourcing.md).
✅ VIS-M0 now adopts three CC0 architectural meshes and their palette texture through Runtime
import/cook/bundle loading, with sources/licenses packaged, reproducible shots and a three-run
performance baseline. [Linux acceptance](Apps/Showcase/evidence/VIS-M0-Linux-AdoptedAssets-2026-10-05/acceptance.md):
87/87 tests without skips and isolated Development package launch. VIS-M1 now integrates shared direct-light PBR, tangents and material-map bindings; Linux hardware sRGB color filtering passed 92/92; Linux cooked IBL passed 94/94 and Shipping packaging; Linux floating HDR composition and directional shadows/stylized lighting each passed 95/95; VIS-M1/M2 cross-platform acceptance each passed 18 checks in PRs #322/#324; hero art/bloom/color, living wind/cutout/activation, the 100-second visual tour, free camera and bounded quality tiers are implemented; VIS-M4/M5 passed all 18 checks in PRs #327/#328; VIS-M3 art and VIS-M6 target-hardware acceptance remain open
(5/7 accepted); software-rasterizer measurements do not establish the GTX 960 budget or final
V1 platform acceptance.

### Repository status

Build CI now routes Markdown-only changes through documentation validation and retains the full
matrix for code, shaders, build/CI settings, protected Markdown paths and tags. A fixed `CI result`
aggregates selected jobs; see [Build CI routing](Tools/Build/README.md). This does not change engine
milestone acceptance or repository branch-protection settings.

✅ [Hosted CI routing record](Tools/Build/evidence/DocumentationRouting-2026-10-05.md):
the full-build route passed 18/18 jobs; the Markdown-only route passed two lightweight jobs and skipped all nine expensive job groups.

The repository now builds and tests Foundation, Core, RHI, Renderer, Runtime, API samples, a Zig gameplay consumer, and `NexoraShowcase` with both deterministic headless and native Linux/Vulkan 3D modes. The milestone sections below describe the implemented portable contract foundations and explicitly call out platform or production backends that remain future work. Parsers for persisted or external data (for example scene snapshots) reject hostile size fields before allocating.

#### V2 networking status

V2-M5 is complete for the portable foundation: the `linux-headless` preset builds only the renderer-free server closure, and `NexoraNetwork` exposes synchronous loopback/simulated transports, a portable socket-provider boundary, and a connection contract with protocol/build identity checks, explicit channel semantics, and deterministic loss/latency/jitter simulation. Its server runtime owns fixed-step scheduling, bounded graceful drain, admission and per-client packet/byte budgets (both defer excess work to a later tick rather than dropping it, guaranteeing at least one packet's progress per tick even when a single packet exceeds the full per-tick byte budget), ordered replay capture, and canonical state hashing. V2-M6 is complete for its portable reference scope, adding dirty-generation dormancy with connection-local acknowledgement and re-entry baseline invalidation, sequenced client prediction, authoritative correction with pending-input replay, deterministic fixed-point latency tests, and versioned replay logs to the existing entity, schema, snapshot, delta, and interest foundations. Headless tests cover a fuzz-style malformed corpus, 10,000 reconnect cycles, scheduling/budget/drain behavior, malformed/truncated data, baseline expiry, connection isolation, dormant wake-up, deterministic correction, and network-bug reproduction. `NexoraDedicatedServer` links only through Network → Core → Foundation, keeping Renderer and presentation modules outside its dependency closure. Native UDP/DTLS adapters, encryption, and hosted production deployment remain backend gates; loopback/simulated transport does not establish production-networking completion.

#### V2 GPU-driven status

V2-M3 now has one fixed 36-byte indirect-command ABI shared by C++ and Slang: the Vulkan/D3D12/Metal-compatible non-indexed draw prefix is followed by backend-neutral classification metadata. Vulkan consumes this canonical stride directly. D3D12 now covers real storage-buffer binding, `Dispatch`, canonical-stride `ExecuteIndirect`, and test-only GPU readback; the local Windows NVIDIA GTX 960 host passed `renderer.v2_gpu_driven` with an SDK `dxc`-generated `sm_6_0` artifact and an exact `CompareGPUDrivenResults()` match. The Slang-enabled Linux Vulkan RenderGraph test passes on Mesa lavapipe, including `CompareGPUDrivenResults()`, and the full Linux development, shipping package/evidence, sanitizer, and TSan jobs passed in GitHub Actions run 36609837931. This Work Mode container lacks `clang++`, so its local full CTest rerun is 50/51; physical-GPU performance, Metal execution, and full target-tier parity remain open. Every backend now rejects a `DrawIndirect` that would read past its bound indirect buffer through one shared range rule (verified on the Validation device and Linux Vulkan; D3D12 and Metal are source-guarded and await their target hosts). V2-M3 remains open independently of the 46% roadmap total.
The Showcase Validation Lab now provides a portable M0-M12 probe registry, honest five-state status model, versioned JSON/Markdown reports, CTest card mapping, four contained error injections, and M7-M10 capability-aware headless room evidence. The 3D Hub now renders all thirteen cards from that shared model and reports stable room/world-object associations without executing CTest logic; authored room visuals and target-host evidence remain open.
The Linux Showcase interaction gate now waits up to 25 seconds for its first software Vulkan frame and requests a normal X11 window close under Xvfb.

#### Shader system status

The shader production pipeline now has a ✅ portable acceptance gate: the Editor invokes the
configured `slangc` process directly without a shell, parses native Slang 2026 and single-line
file/line/column/severity/backend/variant diagnostics, tracks source/include invalidation per compile
request, and caches successful Development variants against an explicit budget; results are stamped
with a pre-compile input snapshot, so a save landing mid-compile is discarded as stale.
Runtime serializes and loads checksummed cooked artifacts, enforces Shipping cooked-only admission,
creates backend modules through an injected native adapter, publishes generations transactionally,
and retires replaced modules only after their GPU fence. Renderer exposes generation-bearing
pipeline-state keys and a named golden-image harness; Slang-enabled Linux Vulkan builds also run
`renderer.vulkan_golden_triangle`, an offscreen Mesa lavapipe render compared against a committed
baseline. That is a Linux software-rasterizer reference only; DX12, Metal, and physical-GPU baselines
remain target-host gates. `Shaders/Nexora/Common.slang` now contains the
portable shared surface for PBR/IBL, StylizedPBR, Anime, Vegetation, Water, Unlit, shadow/post-process,
skinning/instancing/Forward+, variant keys, and retained-mode UI helpers. `PbrSmoke.slang` and
`UiSmoke.slang` are real vertex/fragment smoke entries (including IBL resources, Texture2DArray,
atlas sampling, clip, and nine-slice); Linux Slang 2026.18 SPIR-V/MSL compilation and the shader
contract/cross-compile tests pass. Actual DXIL/native backend execution and captured target-host
golden baselines remain open platform gates; the portable harness does not claim those results.
Renderer now also exposes a ✅ backend-neutral material schema and integration contract covering all
six shading models, resource binding with semantic missing-texture fallbacks, used-variant stripping,
generation-based hot reload, and stable Material Inspector reflection/layout hashes. The existing
shared Slang library supplies PBR/IBL and specialized shading helpers; native DXIL/Metal execution
and physical-GPU visual acceptance remain target-host gates.


#### V2 late-milestone portable status

V2-M9 now has backend-neutral Timeline/Camera Rig, Flex/Grid, localized RichText, Theme/StyleSheet,
accessibility, Surface UI projection, room/portal audio, HLS/DASH segment contracts, and optional
DRM/capture boundaries while preserving the V1 `VideoPlayer` contract. V2-M10 extends the existing
production commandlet with deterministic content-addressed work identities, local-first Shared DDC,
transactional patch verification, generation pin/drain, and native-code rejection. V2-M11 adds a
versioned Core diagnostics wire schema with headless Trace ID correlation and PluginID attribution.
V2-M12 defines the five reference-project capability profiles and fast hardening probes for bounded
growth, reconnect drain, rollback, save corruption, thermal-policy evidence, and V1-like footprint
limits. These are portable foundations only: M9-M12 remain unaccepted until their production,
target-host, full-reference-project, and long-soak gates are satisfied.

#### Engine API status

The Engine API is **complete for the portable roadmap scope** defined by the [Engine API Foundation Roadmap](Roadmap/en/Engine_API_Foundation_Roadmap.md). Platform-specific runtime evidence remains a target-platform validation responsibility and is not represented as missing API functionality.

| Track | Status | Available now / remaining gate |
| --- | --- | --- |
| API-M1 Math and geometry | Complete for the roadmap scope | Full math, geometry, transforms, ABI/layout tests, SSE2/NEON paths, independent DirectXMath coordinate goldens, and an executable sample are available; ARM runtime evidence remains target-host validation. |
| API-M2 Foundation data types | Complete for the roadmap scope | UTF-8 strings/views, buffers/spans, UUIDs, names, results, parsing, generational handles, and caller-owned or opaque engine-owned C ABI buffers are implemented and tested. |
| API-M3 VFS and file I/O | Complete for the roadmap scope | Directory, memory, read-only package, and bundle backends; streams, ranged/async reads, mapping, watches, atomic writes, Shipping host-mount restrictions, and >4 GiB sparse-offset gates are implemented. |
| API-M4 Engine services | Complete for the roadmap scope | Monotonic/game/fixed time, versioned deterministic random, configuration, logging, jobs, events, and profiling-marker emission are implemented and tested; JobSystem shutdown now has lifecycle-leak regression coverage for drain, join, capture release, and restart. |
| API-M5 World/game facade | Complete for the roadmap scope | Handle/value-based entity, scene, transform, camera, light, mesh-renderer, physics, character, audio, asset-reference, and input access are implemented and tested. |
| API-M6 Bindings and versioning | Complete for the roadmap scope | A canonical C11 header, machine-readable ABI manifest, C and Zig consumers, append-only compatibility gate, versioned descriptors, real `GameWorld` wire paths, and embedding-owned event/tick hooks are implemented and tested. |

#### Zig gameplay and Showcase status

Zig is no longer only a planned language direction. The repository builds a Zig 0.14.0 gameplay object and ABI smoke consumer. The current headless/static ZS-M1 verification slice has C++ own `main`, engine/world lifetime, fixed and variable updates, offscreen rendering, transactional reload, and shutdown, while Zig mutates a live entity Transform through the public V3 ABI. See the [Showcase README](Apps/Showcase/README.md) for the supported workflow.

ZS-M0 through ZS-M5 are complete: the capability-aware gallery supports camera input, selection raycasts, honest feature overlays and tested fallbacks; dynamic reload now stabilizes files, restores candidate state before `on_start` to prevent duplicate scenes, and reports callback/device-loss recovery scopes. Development-dynamic and Shipping-monolithic/static packages launch from checksum-verified isolated copies, and the independently provisioned Windows clean-machine Development package passed 16/16 checksums and emitted a PASS launch report. See the [ZS-M5 acceptance record](Apps/Showcase/evidence/ZS-M5-Windows-CleanMachine-2026-10-01/acceptance.md).

#### Window and native presentation status

The [Window and Native Presentation Roadmap](Roadmap/en/Window_Presentation_Roadmap.md) is **100% implementation complete**: WP-M0 through WP-M4 provide the module boundary, native window/input implementations, DX12/Vulkan/Metal presentation paths, reusable Showcase/Editor surfaces, and lifecycle/failure hardening. This percentage records implementation scope, not cross-platform runtime acceptance. Windows/DX12 WP-M1/WP-M2 runner evidence is now recorded; Linux Showcase geometry and composition pass Xvfb/lavapipe acceptance; physical-display, Windows/Vulkan, and macOS/Metal runtime acceptance remain target-host gates.

#### Editor status

ED-M1 now includes a real-index-backed graphical Content Browser with recoverable project-local
rename/move/delete/undo, generation-tagged drag/drop, dependency inspection, and cancellable
background reimport. An Editor-owned import queue also indexes selector projects in the background;
workers expose bounded progress and structured diagnostics, while the authoring thread alone
activates indexes or publishes revision-validated staged artifacts. Versioned sibling `.meta`
records preserve asset UUID and artifact identity across Editor moves and process reopen. Schema-2
project descriptors now carry stable UUIDs, schema-1
projects upgrade atomically under an OS-held writer lease, explicit read-only processes coexist
without project mutation, and the docked Project panel shows access/upgrade/recent-project status.
A graphical Project Browser now supports create, read-write/read-only open, and recent shortcuts;
activation is transactional and Linux Xvfb drives create/reopen from a launch without `--project`.
The Content panel now shows dependency cycles and blocks dirty external changes behind an explicit
Reload/Keep/Compare dialog; Compare preserves both hashes while terminal choices are recorded on
the authoring thread without direct UI filesystem access. Physical-display/Windows workflow
acceptance remains open, so milestone acceptance stays unchanged. The graphical Hierarchy now
provides a parent-aware expandable tree, filtering, generation-keyed expansion and anchored
multi-selection, clipped visible-row submission, undoable rename, sibling ordering, and cycle-safe
drag/drop reparenting through Editor Core while rejecting stale entity/document generations. The docked Inspector now edits local position, quaternion, and scale for single or mixed-value multi-selection through one generation-safe, atomic, undoable SceneDocument transaction; the single-selection Camera and Light sections now edit validated field of view, clipping planes, and nonnegative intensity with Undo and scene persistence; the complete reflected Inspector remains open. The
X11 window backend now owns one XIM input context per
window, decodes committed UTF-8 into backend-neutral `Text` events, and keeps physical keys separate
from text input.

The portable Editor Core now also preserves Content Browser selection through delete/undo, rolls
back partial gizmo previews, rejects invalid camera state, replaces camera/autosave/build files
without deleting an incompatible destination, keys and invokes shader variants with their defines,
rejects stale compiler output, and keeps finite snapping results finite. These robustness fixes do
not change graphical milestone acceptance.

The docked Console now shows bounded Runtime records
with text/severity filters, source, timestamps, and dropped-record count; startup and scene save
diagnostics are routed through it. Complete Game View materials and log routing remain open.

OBJ reimport now publishes immutable geometry with its hash after revision and memory-budget checks;
failed/cancelled/stale results preserve the previous mesh. Content revisions refresh native geometry,
and rename/delete Undo retains the newest payload.

Native Scene preview now draws and triangle-picks resolved OBJ geometry through bounded shared
mesh batches. Distinct triangle/quad Xvfb pixels cover Center preview/commit and Undo; missing or
oversized meshes warn and retain proxies. Authored Scene/Game geometry now uses exact world
matrices through mirrored/sheared ancestry, including Scene bounds/picking and prospective gestures.
Game uses live post-tick matrices and owns them after Stop. Materials, GPU caching and full
acceptance remain open.

Position/Scale Inspector fields now commit on Enter as one Undo step. Mixed-value drafts support
negative/scientific input and preserve each entity's current unrelated values; Escape, focus loss,
selection/reload, Play inspection and read-only/recovery/close transitions cancel pending edits.
Real keyboard tests verify no mutation while typing and persistence after committed edits.

The Inspector now assigns imported OBJ meshes or removes MeshRenderer across mixed multi-selection
as one atomic Undo/Redo step, preserves each entity's material, and rejects stale selection/project/
document requests. Actual combo/remove clicks, repeated replay and save/reload are contract-tested.
Full Scene View acceptance remains open.

Imported mesh geometry now has an owning, generation-checked CPU catalog with stable UUID-derived
64-bit scene resource IDs and atomic collision rejection. Persistent per-asset GPU caching and full Scene View acceptance remain open.

Xvfb Undo checks now retry only Save after a single Undo until committed scene bytes match.
X11 modifier releases now clear the released family immediately while preserving a held paired key;
native X11 event tests cover Control, Shift, Alt, and Super. Center preview pixels wait for a changed,
settled frame before release comparison.
Native authoring input now commits before Save/Save-and-exit and GPU submission, so an immediate
Save after mouse release includes that completed edit.
Scene drags now cancel on focus loss, Undo/Redo and Create/Paste/Duplicate shortcuts, document replacement, and hidden
canvas or recovery. Real Xvfb FocusOut and UI contracts verify that abandoned previews do not commit.

The public native SceneDrawData now supports multiple bounded indexed geometry/instance ranges
inside one depth pass in Vulkan/DX12. Portable range checks and distinct-geometry Vulkan pixels cover
the new boundary; persistent per-asset GPU caching and full Scene View acceptance remain open.

Editor SceneDocument now owns generation-safe MeshRenderer transactions and reads, with mesh/material
resource IDs retained across Undo/Redo and scene persistence. Full Scene View acceptance remains open.

Background workspace imports now stage immutable triangulated OBJ geometry with bounded source/
memory use, UVs, normal generation, local bounds, cancellation, and source-line diagnostics.
Typed OBJ geometry reimport now publishes atomically; full Scene View acceptance remains open.

The native 3D gizmo now exposes Pivot/Center (P), including common-center rotation and scale of
multiple selected roots, with matching previews and one-step Undo. A two-root Xvfb workflow checks
center scale, center rotation, and release pixels; full Scene View acceptance remains open.

Native gizmo previews now share SceneDocument root edits and Runtime hierarchy composition with
commit; rotated, mirrored, and nonuniform ancestors produce matching descendant poses. Prospective
snapshots leave scene content and Undo/Redo untouched. Deep mirrored/sheared ancestry now retains
exact world origins and gizmo position conversion, with owning world/preview matrices and
closed-form translation/Center rotation/scale, Undo/Redo and save/reload tests. Native authored-mesh
affine rendering/picking now consumes those matrices. Graphical milestone acceptance remains 0/8.

Presentation instances now support exact row-major affine model matrices and shared native
inverse-transpose normal packing. Exact binary32 determinant classification prevents cancellation
errors; portable contracts and Vulkan pixels compare mirrored/sheared geometry against an
independently baked reference. Scene/Game authored meshes now consume exact matrices through the
CPU validator, with closed-form picking and stale-snapshot/Stop ownership tests; graphical
milestones remain unaccepted.

The graphical Scene overview now offers optional 0.25–4 world-unit movement snapping; its drag
preview matches the committed, undoable move even for parented entities.
Scene Undo and Redo now replay stable entity IDs, hierarchy and components while preserving node
names, selection, and authored Euler revolutions. The Scene panel exposes both by button and
keyboard shortcut; a new edit discards the undone branch.

✅ Vulkan Scene/Game uploads now reuse bounded capacity in fence-protected frame slots.
Steady, smaller and Scene-free frames retain allocation; growth stages replacement before retiring
old storage, and resize/teardown drains GPU work. Native call tracing and pixels verify 100 steady
frames, maximum descriptor budgets, failed growth and leak-free lifetime. Fresh geometry is still
copied per draw; persistent per-asset GPU caching and full graphical acceptance remain open.

✅ Hierarchy now selects Empty, Camera or Light for Create root / Create child and Ctrl+Shift+N.
  Camera/Light are initialized at identity local TRS as one complete stable-ID Undo/Redo transaction.
  Default names follow type choices while custom names remain. Queued owning requests recheck
  access and scene/parent generations. Portable and 1x/2x real menu/pointer/key tests cover root/child,
  stale scenes/parents, read-only/modal gates, 100-step replay and save/reload. Full reflected component
  creation and target-host acceptance remain open.

✅ Inspector Copy values / Paste values now snapshots committed Transform/Euler, Camera or
  Light numeric values from one entity and applies them to multi-selection as one atomic Undo.
  Camera/Light preserve component absence; Transform retains authored turns. The typed owning
  clipboard survives source edits/deletion and reload, independently of hierarchy clipboard.
  Read-only Copy, disabled/mismatched Paste, draft cancellation, no-op Redo and persistence are
  covered by portable and 1x/2x real UI input tests. Full reflected Inspector remains open.

✅ Inspector now exposes Reset Transform, Reset Camera and Reset Light for multi-selection.
Transform reset clears local TRS and visible/stale Euler revolutions; Camera/Light reset preserves missing
components. Changed batches are atomic single-step Undo/Redo, and no-ops retain Redo. Reset cancels
unsubmitted drafts and Scene gestures; workspace/modal gates apply. Portable and 1x/2x real UI
input tests cover metadata-only Undo, mixed presence, unrelated payloads and save/reload.
Complete reflected Inspector and target-host acceptance remain open.

✅ Camera and Light Inspector fields now support multi-selection with mixed presence/value states.
Enabling a mixed component adds it to missing entities while preserving existing values; Enter applies
only the edited field as one generation-checked atomic Undo/Redo transaction. Invalid/stale/duplicate
batches and read-only/recovery writes reject the whole edit. Real keyboard tests cover Camera FOV and
Light intensity, unchanged fields, repeated Undo/Redo and save/reopen. Full reflected editing remains open.

Camera/Light drafts and pending requests now cancel on focus loss, deselection/reload, Inspector
collapse and Play inspection, plus read-only/recovery/close gates. Controls disable while blocked;
queued batches must match the current selection. Real keyboard lifecycle tests prevent revived edits.
Unavailable target-host acceptance is deferred while independent Editor implementation continues.

✅ File New/Open/Save/Save As now manage a single active scene with dirty Save/Discard/Cancel,
Untitled Save As and explicit overwrite confirmation. Document/project tokens reject stale actions;
failed Open keeps history and content. Failed replacement keeps its destination and preexisting
temporary paths. Canonical aliases enforce metadata scope and close saves stay retryable.
Content saves import only their own source and retain persistent asset identity and prior
content Undo; per-file camera state survives switches and writable shutdown. Real 1x/2x UI tests
and Linux Xvfb cover the application workflow and unchanged read-only project files. Content Unicode
folder/asset labels, search, rename/move and Undo use UTF-8 text and native filesystem paths.
✅ Startup now restores the last successfully opened/saved scene and its view state, including
read-only reopen. Invalid/unavailable startup data falls back to Main and remains preserved;
independent metadata-write failure does not undo a scene save. Linux Xvfb verifies process restart
by editing/saving the restored file and checking fallback. Additive scenes and full graphical
milestone acceptance remain open.

✅ Content Browser can open a scene by double-click, context Open scene, its Open scene button or
focused Enter. Owning requests preserve the selected UTF-8 destination through dirty decisions;
read-only is supported and Play/modal/stale tokens reject replacement. Real 1x/2x pointer/key tests
cover the workflow; additive scene tabs remain open.

✅ Focused Content F2 now renames one asset with selected UTF-8 filename input, Enter/Apply and
Escape/Cancel. Invalid names remain retryable, unchanged names clear errors and keep Undo, and
stale/access/modal gates cancel drafts without changing document history. Real 1x/2x and native Linux keyboard
workflows verify the action; physical IME and full graphical acceptance remain open.
Focus loss cancels immediately even when rendering is deferred until focus returns.

✅ The active Content scene now follows UUID-preserving rename/move and Content Undo without losing
document edits/history or view state. Committed relocation updates startup location even when
unsaved edits are discarded at exit. Deleted/unavailable tracked assets block ordinary Save;
restoration unblocks it and another UUID at the old path remains protected. Saved-scene indexing
retargets moved UUIDs without scanning unrelated sources. Portable and native Linux tests verify
relocation, Save, deletion and Undo; graphical milestone acceptance remains partial.

✅ Camera Inspector now aligns one Camera to the stored Scene 3D pose with one Undo, retaining
lens, scale and parent. Runtime matrices verify sheared/mirrored ancestry; equivalent poses keep
Redo, while read-only/unavailable views and stale targets reject edits. Extreme finite centers share
the native preview's +/-100,000 clamp. Save/reload retains alignment.

✅ Content now adds one resolved mesh to the Scene center and selects the new root. Initialized
creation has one Undo; Redo and Save/Reload retain stable ID, name, pose and mesh/material data.
Real UI clicks verify generation/access/modal gates, native center bounds and real Game geometry.
✅ Typed Content mesh drags now place a root at the overview pointer or native Scene ground point
with one Undo. Real 1x/2x DPI pointer tests cover release, persistence and rejected/canceled drags;
preview tooltips do not mutate the World. Surface snapping and geometry ghosts remain open.

✅ Focused Hierarchy F2 now opens Rename with focused/select-all name input; Enter commits one
  metadata Undo and Escape cancels. The dialog blocks other authoring/clipboard/Undo/Save/Play
  shortcuts and queued Hierarchy writes. Real 1x/2x input verifies UTF-8 CJK/supplementary characters,
  retryable empty names, clipboard/history retention, save/reload and cancellation on write loss,
  external modal, focus loss or stale document. ImGui/library consumers share 32-bit Unicode text;
  font coverage and physical Windows IME acceptance remain open.

✅ Focused Hierarchy Ctrl+A now selects all filtered/expanded visible rows, including clipped
  rows, through generation-keyed selection. Empty matches clear selection; read-only projects retain
  the action. Keyboard tests verify collapsed descendants, filter order, foreign-panel/text-input
  focus, recovery/close gates and unchanged World/Redo. Large-scene scale/soak acceptance stays open.

✅ Typed Content mesh drags now assign the Inspector Mesh field for the displayed selection.
  Hover only previews; release uses one generation-checked batch, retaining existing materials and
  adding missing MeshRenderer components. Real 1x/2x pointer tests cover initialized Undo/Redo,
  save/reload, non-mesh/stale catalog/project rejection, canceled input and workspace/modal gates.
  Rejected drops retain Redo; complete material/reflected Inspector workflows remain open.

✅ Hierarchy Cut and Ctrl+X now capture complete selected forests before one atomic deletion.
  Undo restores original IDs and selection; first successful Paste keeps root names with new IDs,
  then retained clipboard data uses Copy naming. Failed Cut/Paste and Duplicate preserve pending
  clipboard state. Real 1x/2x key/pointer, workspace/modal/text-input gates, replay and persistence
  tests cover this lifecycle. Fresh layouts dock Inspector on the right to keep Hierarchy usable.

✅ Copy/Paste/Duplicate now retains complete root forests, components, Euler hints and opaque
payloads in an owning copy-time snapshot. One Undo removes the whole created forest and restores
selection; Redo keeps initialized poses and stable IDs. Keyboard/persistence and 1,000-cycle tests cover it.

✅ Multi-selection Delete now records all selected subtrees as one atomic Undo. Keyboard replay
restores hierarchy, components, authoring metadata and the full selection; collision/lifecycle
rejections, unrelated-entity preservation and 1,000 replay cycles are covered.

Scene/Hierarchy controls, shortcuts and queued edits now share workspace write access and modal
gates. Read-only selection, Copy and camera navigation remain available; pointer tests verify
interrupted overview/native drags cannot commit after access changes, and Undo/Redo history survives.

✅ Missing-plugin components now have a bounded read-only Inspector showing owning names, full-width
entity/type IDs, byte counts, and at most 64 preview bytes. Scene format 3 retains opaque data through
save/reload, clone-by-clipboard, deletion, and independent metadata Undo/Redo; opaque-free scenes retain
format 2 and readers accept formats 1/2. Generation checks and 1 MiB/component, 16 MiB total payload,
64/entity, and 4096-record limits reject stale/oversized imports. Corrupt, duplicate, orphan and missing
entity records reject reload before live mutation. Plugin restoration/execution and full reflected
editing remain open.

✅ Native client-pixel pointer events now convert once through the current frame DPI before UI
hit testing. Cached positions reproject across scale changes; focus loss clears the cache and DPI
changes cancel interrupted Scene gestures. Tests cover 100/125/150/175/200%, fractional/negative
coordinates, event ordering, stationary pointers, invalid scale fallback, and 200% Apply dialog clicks.
Deferred/zero-extent frames also forward gameplay key releases and focus loss without ticking
Play or rendering a GUI frame. Target-host physical-display DPI evidence remains open.

Linux Xvfb acceptance now creates a Hierarchy root with Ctrl+Shift+N, saves the scene, restarts
the graphical Editor, and checks that the authored root reloads. Full ED-M2 visual acceptance
remains open.

The docked Game panel now controls an isolated Play World with Play/Stop, Pause/Resume, and one
fixed Step, plus a copied entity inspection list and bounded X/Z world-pose preview. F5/F6/F10 are exercised under Linux Xvfb.
✅ A bounded native Game View draws imported OBJ through the Play camera, freezes assets at
Start, and owns frame uploads after fixed ticks. Xvfb/lavapipe checks camera pixels,
Pause/Step/Stop, and the unchanged editor scene. Complete materials, multiple native canvases,
complete gameplay services, and expanded input routing remain open.
✅ Game View now offers an active-camera preview chooser and Automatic fallback. Choices are
Play-session scoped; removal, unloading, unrenderable projection and restart reset safely. Real UI
clicks verify read-only previews and unchanged Editor/Play data and Undo; frames revalidate after ticks.

✅ Runtime component wires now also target the isolated PlaySession World through shared decoding
and atomic command validation; Fixed/Step tests prove that writes never reach the Editor World.
✅ The Game panel loads optional project-relative V3 gameplay libraries into the clone, runs
FixedUpdate/Update, routes bounded module logs to Console, and unloads before Stop. Xvfb verifies
a real library moves the mesh while the authored scene stays unchanged. Expanded input routing, full
scene/physics services and hot reload remain open.
✅ Clicking the playing Game canvas now routes held keyboard movement and mouse/button controls
through owned user-zero gameplay snapshots. Escape, pointer exit, Pause/hide/blur/prompts clear
held state; captured keys cannot trigger authoring shortcuts. Xvfb verifies input-only native mesh
movement and Escape release while D remains held. Gamepad, pointer look, and rebinding remain open.
✅ Selecting a Game entity now opens a read-only Play Inspector with copied local/world poses,
parent and scene state, Camera/Light values, and full-width mesh/shader IDs. The Game panel shows
pause reasons and callback failure counts; per-frame and fixed callback failures both release input.
Snapshots remain valid after component removal and Stop; Editor selection and authoring stay separate.
✅ Gameplay library selection now persists in bounded schema-1 project settings, independently
of scenes and recovery journals. Read-only access never writes; invalid settings are preserved until
an explicit replacement, and an unresolved recovery journal blocks shutdown saves. Explicit CLI
paths (including empty) override the saved selection. Xvfb reopens without a CLI path and verifies
that the saved library still drives the Play mesh; reopening alone does not load a module.
✅ Game Apply Changes now pauses for an owning transform-diff review. Confirm rechecks Play,
document and entity generations plus every original/Editor/Play value, then applies one atomic
SceneDocument transaction and stops with clone discard. Conflicts/reparenting/unsupported scenes
reject the complete batch. Undo restores all applied values; component/create/delete changes are
never copied. Modal input blocks authoring/Play shortcuts; default Stop still discards.
The docked Profiler now plots a bounded live trace of Editor frame processing wall time, with
pause/clear, latest/average/peak values, and a dropped-frame count. GPU timing and process memory
remain explicitly unavailable.
✅ The Profiler now exports retained Editor frame-processing wall times to project CSV with
full double precision and an evicted-frame count. GPU/memory cells stay empty. The synchronous
writer validates 1-600 ordered finite samples, rejects read-only/recovery writes, and atomically
preserves the previous file on validation failure; real UI clicks emit one-shot requests.


The native Vulkan/DX12 scene draw contract now supports a clipped physical-pixel viewport, with
portable bounds checks and Vulkan Xvfb pixel evidence.
The docked Scene canvas now exposes its visible framebuffer-pixel rectangle after layout and DPI
scaling. The Editor's 3D Preview toggle now draws native depth-tested ground and live entity
position proxies in that rectangle after UI submission. Proxies now reflect composed world rotation
and scale. Resolved OBJ geometry now uses native mesh batches with exact affine world matrices;
material shaders and full graphical Scene View acceptance remain open.

✅ Native Move now adds Global/Local XY/XZ/YZ plane handles at Pivot/Center. Shared picking and
preview/release math preserve selection and constrain and snap both axes; Shift keeps the explicit plane.
Tests cover mirrored parents, descendants, unknown bytes and Undo/Redo; Linux Xvfb saves proxy/OBJ
roots through all six planes and verifies Escape cancellation. Full gizmo acceptance remains open.

✅ Native Scene X now switches Global/Local axes (Scale keeps Local), and P switches Pivot/Center
before same-frame click/drag setup. Real 1x/2x input verifies Home navigation and focus/modifier/modal/
gesture guards with retained World/Redo; Linux Xvfb verifies Home/P center gestures on proxy/OBJ roots.
Complete gizmo and target-host acceptance remain open.

✅ Focused Scene Ctrl+A / Select all now select overview nodes or the actual bounded native
submission without changing World, dirty state, clipboard or history. Owning tokens, current
NodeKeys, 1x/2x gates, 4,001-node limits and native proxy/OBJ selection are verified.
Complete Scene View acceptance remains open.

✅ Native Scene Select (Q) now picks entities without transform handles or drag commits; W/E/R
return to Move/Rotate/Scale. Real 1x/2x input tests cover tool gates, Home navigation and retained
Redo; Linux Xvfb verifies proxy/OBJ picking and unchanged saved bytes after Select drags.
Complete graphical Scene View acceptance remains open.

✅ Focused Scene Home and Frame all now frame the whole scene without altering selection or history.
Native framing uses the actual bounded submission candidates and affine mesh/proxy bounds;
overview framing fits all world origins. Owning session tokens apply once in the issuing frame.
Read-only, empty/rejected bounds and modal/focus/drag gates have real 1x/2x acceptance, and Linux
Xvfb verifies Home/pan/Home pixel restoration for proxies and authored OBJ meshes. Existing bounded
camera/zoom ranges apply. Tests cover release/Home and 4,001-node submission limits; complete
graphical Scene View acceptance remains open.

The preview camera supports right-drag orbit, middle-drag X/Z pan, Shift+middle height pan,
wheel zoom, and F or Frame selected to center selected forests using exact world-transformed
mesh/proxy bounds. Descendants count once; the narrower viewport FOV adjusts distance (2–100
world units). Real 1x/2x input verifies offset/sheared geometry, catalog replacement, read-only
navigation, portrait viewports and unchanged World/Redo. Orbit angle, distance, and target height persist per scene.
Clicking a visible 3D proxy now selects its scene node in Hierarchy and Inspector; Ctrl-click
toggles selection. Resolved meshes now use transformed bounds and two-sided triangle picking.
Picking now tests the rotated proxy and translation-handle boxes after a conservative bounds
filter, so empty corners of their bounds do not select them.
Dragging a proxy previews its world X/Z move, while Shift-drag moves along world Y. Selected
proxies show colored X/Y/Z handles that constrain a drag to one axis; Local axes rotates the
handles with the first selected node. Release commits one
undoable transaction; Escape cancels and optional world-unit steps snap preview and commit.
The Rotate tool (E over the canvas; W returns to Move) shows X/Y/Z rings in world or local
space, previews selected roots and descendants while dragging, and commits an in-place
selected-root rotation as one Undo step. The Scale tool (R over the canvas) shows local X/Y/Z
cubes and a white uniform cube, previews selected roots and descendants during drag, and commits axis or uniform local scale as one Undo step. Shift held at drag start snaps rotation to 15-degree steps and scale changes to 0.25-factor steps in both preview and commit.

Desktop graphical CI tests now distinguish deterministic Ctrl input fixtures from the native
macOS Cmd/Super undo/redo path; production retains its platform input policy. Fixtures now release workspace/file owners before deleting temporary projects and write canonical scene bytes in binary mode; the Linux interaction gate explicitly focuses the Scene canvas before shortcuts.

The [Graphical Editor Roadmap](Roadmap/en/Editor_Roadmap.md) remains at **0/8 (0%) graphical milestone acceptance**. Portable foundations now include ED-M4 additive-scene ownership and dependency ordering, migration dry-runs, bounded autosave recovery, and source-control-neutral three-way conflicts, and UI-neutral viewport pick-ray, AABB picking, axis-drag, snapping, and resize-hysteresis math, in addition to the existing ED-M1 through ED-M3 contracts. The [rotation and scale plan](Roadmap/en/Transform_Rotation_Scale_Plan.md) has ✅ all phases complete: `runtime::Transform` carries a quaternion rotation and per-axis scale following Unity/Unreal conventions, position-only writers preserve them, and `ViewportMath.h` provides Unity-style translate/rotate/scale gizmo math (Global/Local axes, Pivot/Center, parents, negative-scale rules, and multi-selection roots, covered by `editor.viewport_math`), and gameplay modules read and write `"Nexora.TransformV2"`, `"Nexora.WorldTransform"`, and `"Nexora.Parent"` through append-only C/Zig wires whose layouts are gated in C11, the ABI baseline, and Zig; the Graphical degree rotation fields now use Z-X-Y composition and one atomic multi-selection transaction; ✅ SceneDocument hints retain typed revolutions across selection/save/reload and undo, with validated version-2 metadata, version-1 compatibility, and atomic same-World reload. Phases 1 and 2 of the [entity parenting plan](Roadmap/en/Entity_Parenting_Plan.md) are ✅ complete: entities form a Unity-style hierarchy with local transforms, world transform and exact world matrix, keep-world reparenting, and cascading destroy; scene snapshots are version 3 (versions 1 and 2 still load), the Editor scene document uses the runtime hierarchy, and character controllers work under a parent the way Unity's do (a moving parent carries them). Hierarchy batches are all-or-nothing, snapshot validation and cascading destroy are linear in the scene size, and PIE apply-back rejects entities reparented during play (`runtime.entity_parenting`, `editor.preview_contract`; Linux development, full-feature, Shipping, ASan/UBSan, and Editor-SDK-off builds, plus the full CI matrix after merge). Unity-style sibling order (`SetSiblingIndex`, reparent-to-last) and an undoable Hierarchy drag model (`SceneDocument::Move`) are in place; `RenderSceneSync` mirrors mesh renderers into the `GPUScene` with each entity's exact world matrix and conservative bounds, so a moved parent re-renders its subtree, and cameras follow their parents too (`CameraView`, plus a frustum-culled `RenderSceneSync::RenderFrame`; `runtime.render_sync`); no application draw loop uses it yet. These are portable math and data contracts; local X/Y/Z scale cubes now work; uniform scaling is available through a white camera-facing cube. The Linux Editor display acceptance now passes locally on a virtual display (Xvfb with Mesa lavapipe) after fixing three real defects it exposed; a dedicated CI job (`editor-linux-display`) runs it, and a bounded Windows/DX12 developer-host shell smoke now also passes; physical-display Linux and Windows DPI/IME evidence remain open. The virtual-display recovery gate now fsyncs a seeded journal, SIGKILLs the real Editor, verifies that the journal and committed workspace survive, and reacquires the writer lease on relaunch before keyboard-only Recover/Discard. Recovery process failures cannot pass on diagnostics alone. The graphical shell now routes Ctrl+S and the Scene panel save button through an application-owned one-shot request, saves to the managed current path (startup still opens `.nexora/scenes/Main.scene`), and blocks read-only saves or overwriting an unreadable scene; the Scene panel also supports Undo by button or Ctrl+Z outside text input, and an undone creation no longer leaves stale nodes in saved scenes; the Hierarchy can now create named roots and children, select them, and reject stale parents, with Undo and save/reload coverage; Copy/Paste buttons and Ctrl+C/Ctrl+V now paste world-pose snapshots and select the new roots; the Hierarchy or hovered native 3D canvas can also delete selected subtrees and restore their metadata with Undo, or duplicate the current selection without replacing the clipboard; the central Scene panel now adds an interactive top-down X/Z entity overview with Ctrl/Shift selection, F to frame the selection, and one-step undoable X/Z marker dragging with visible single-axis handles; the overview camera now restores its center and zoom per scene, the Scene panel marks unsaved content until a successful save or matching Undo, and a native close request offers Save and Exit, Discard and Exit, or Cancel for unsaved content, while renderer-backed 3D Scene View and graphical save/restart acceptance remain open. The focused [ED-M0 Dear ImGui plan](Roadmap/en/Editor_ImGui_Integration_Plan.md) now has ✅ WP0 baseline acceptance ([evidence](Tools/Build/evidence/EditorEDM0-Linux-2026-10-05.md): graphical OFF 71/71, ON with Slang 115/115, no skips, and Shipping engine build passed), and remains **in progress**; graphical workflows, native debugger integration, physical-display evidence, and UI acceptance remain open. ED-M0 through ED-M7 are therefore unchecked; portable prerequisites are not rounded up into accepted graphical milestones.
The Linux virtual-display gates allow 90 seconds for recovery relaunch on a loaded CI host; the native Scene acceptance re-saves a settled Undo before reporting a mismatch.

### Important note

The files under `Roadmap/` are design and planning artifacts. Their prose is not an instruction to run commands, grant access, or change external systems. Operational changes are made only from an explicit request in the project workflow.

### Contributing

Issues and pull requests are welcome. Please keep architecture changes traceable to a roadmap document, describe compatibility or contract impact, and include validation evidence for implementation changes.

### Codex Cloud

Use [`cloud-setup.sh`](cloud-setup.sh) as the Codex Cloud environment setup script. It installs the
Linux toolchain, CMake, and Zig version used by this repository, then warms the development preset.
Repository-specific agent instructions, validation gates, and commit/PR conventions are defined in
[`AGENTS.md`](AGENTS.md).

### License

Nexora is released under the [MIT License](LICENSE).

## 繁體中文

Nexora 是一個開源跨平台 3D 引擎計畫，聚焦於高效能 C++20 核心、Zig Gameplay、語言中立的穩定 C ABI、現代化渲染、可擴展世界系統，以及 AI 輔助工程流程。

目前 repository 除了架構與施工規劃，也已包含可執行的 C++20 Engine／Runtime 基線。英文版 Roadmap 位於 `Roadmap/en/`，並保留 `Roadmap/zh-TW/` 下的繁體中文原文，方便貢獻者交叉參照。

### 發展方向

- 支援 Windows、macOS、Android 與 iOS
- C++20 Engine Core，並以 Zig 作為主要 Gameplay 語言
- 以統一 RHI 路線支援 DX12、Vulkan 與 Metal
- Node + Component 編輯流程與 Data-Oriented Runtime 儲存
- 分階段發展 GPU-Driven Rendering、大型世界串流、Networking、Animation、Physics、AI 與 Editor Tooling
- AI 輔助開發、自動化驗證 Gate、可重現 Build 與人工 Review

### Roadmap

請參閱 [`Roadmap/README.md`](Roadmap/README.md) 的中英文文件索引。
進度依各文件明定的 milestone acceptance gate 計算，不以段落數量或「已列入 scope」當成已實作。
只有具備證據的已完成驗收項目才計入；部分進度記錄在各 Roadmap 內，且不會向上取整為已完成 milestone。V1 百分比
特指下方所述的 portable contract-foundation scope，不代表所有目標平台的 production 實作均已完成。

已完成的 Roadmap 項目統一使用綠色 `✅` 標記。每次更新內容都必須重新檢視受影響的 Roadmap 項目，
並根據驗收證據同步這份 GitHub README 的進度或狀態文字；未打勾或未標記的項目視為尚未完成。

| Roadmap | 進度 | 計算依據 |
| --- | ---: | --- |
| ✅ [V1 完整規劃書](Roadmap/zh-TW/跨平台3D_Engine_V1_完整規劃書_v1_2.md) | **100%** | 13/13 個 portable M0–M12 contract foundation 已交付；native/product adapter 仍為獨立 gate。 |
| ✅ [V1 AI 施工規劃](Roadmap/zh-TW/跨平台3D_Engine_V1_AI施工技術與系統規劃_v1_2.md) | **100%** | 對應同一個已驗收的 portable V1 施工基線。 |
| [V2 完整規劃書](Roadmap/zh-TW/跨平台3D_Engine_V2_完整規劃書_v1_4.md) | **46%** | ✅ V2-M0 至 ✅ V2-M2 與 ✅ V2-M4 至 ✅ V2-M6 已驗收。V2-M7 現含有界 multi-world self-play 協調，但仍進行中；V2-M9 至 V2-M12 也仍是未驗收的 portable foundation，V2-M3 與 V2-M8 尚未完成。 |
| [V2 AI 施工規劃](Roadmap/zh-TW/跨平台3D_Engine_V2_AI施工技術與系統規劃_v1_2.md) | **46%** | 追蹤同一套已驗收的 V2 基線，以及進行中的 V2-M7 與 V2-M9～M12 portable foundation；production backend、distributed infrastructure、跨裝置 diagnostics 與 hardening 驗收仍待完成。 |
| [V3 完整規劃書](Roadmap/zh-TW/跨平台3D_Engine_V3_完整規劃書_v1_4.md) | **0%** | 尚無 V3 delivery milestone 通過 repository gate。 |
| [V3 AI 施工規劃](Roadmap/zh-TW/跨平台3D_Engine_V3_AI施工技術與系統規劃_v1_3.md) | **0%** | 僅為施工規劃；尚無 V3 milestone 驗收。 |
| ✅ [Engine API 基礎](Roadmap/zh-TW/Engine_API_基礎_Roadmap.md) | **100%** | ✅ API-M1 至 ✅ API-M6 完成 portable scope。 |
| ✅ [Window 與 Native Presentation](Roadmap/zh-TW/Window_Presentation_Roadmap.md) | **100%** | ✅ WP-M0 至 ✅ WP-M4 已實作；Windows/DX12 WP-M1/WP-M2 runtime 驗收已記錄，Linux Showcase Vulkan composition 已 ✅ 通過 Xvfb/lavapipe 驗收；physical-display 與其他 native-host 驗收仍為獨立 gate。 |
| ✅ [Zig Showcase](Roadmap/zh-TW/Zig_Showcase_Roadmap.md) | **100%** | ✅ ZS-M0 至 ✅ ZS-M5 已完成；獨立配置 Windows 乾淨機器的 Development package 已通過 16/16 checksum 並產出 PASS launch report。 |
| [圖形化 Editor](Roadmap/zh-TW/Editor_Roadmap.md) | **0%（0/8）** | Repository 稽核確認各 ED track 已有 portable foundation，Dear ImGui shell 亦在施工中，但尚無 graphical ED milestone 通過全部 automated 與 target-host gate。 |
| [聚焦 Roadmap AI 施工規劃](Roadmap/zh-TW/聚焦_Roadmap_AI施工技術與系統規劃.md) | **60%** | API 100%、Zig Showcase 100% 與 Editor 0% 的平均，向下取整至 10%。 |
| [V1 可視化 Showcase](Roadmap/zh-TW/V1-Visual-Showcase-Long-Term-Plan.md) | **Linux／Windows 開發機、乾淨 VM 與實體顯示切片已驗證** | ✅ 原生 3D/UI、八個 Runtime 房間、Validation Lab、sampled material、hardware instance 與 offscreen RenderGraph；✅ Windows DX12 Development/Full 與 Shipping/Full 本地截圖、JSON 及 210 秒導覽。Windows Development CTest 68/68、Linux Development CTest 77/77。乾淨 Windows 10 VM（VirtualBox）啟動與 GTX 960 實體顯示執行已驗收（聲明旗標由 Claude 依使用者指示提供）；Windows Vulkan 實體顯示（GTX 960，CI 套件）亦已通過；每個 tag 的 release 流程已在真實測試 tag 上執行（rc.3 draft 包含 Windows DX12／Vulkan、Linux x64 與 macOS ARM64 套件）；Mac 尚未完成，依使用者指示暫緩處理；V1 最終驗收尚未完成。 |

✅ [Linux Full 發佈套件驗收](Apps/Showcase/evidence/V1-Distribution-Linux-2026-10-04/acceptance.md)：Development 80/80，五個原生 gate 無 skip，Minimal／Full Monolithic build 與 checksum 驗證隔離副本原生截圖。Metal scene／input 原始碼、Mac 封裝重定位與 Linux／macOS release／CTest 證據流程已實作；✅ [macOS hosted Shipping/Full 驗收](Apps/Showcase/evidence/V1-Metal-Hosted-CI-2026-10-04/acceptance.md) 已確認 Metal 編譯與隔離封裝八房間原生 graph／resize 執行（96 幀）；原生像素／輸入／depth／lifecycle gate 亦已通過 macOS CTest（Development 73/73、mimalloc 63/63）；實體 Mac 畫面與乾淨主機部署仍待驗收，完整 V1 驗收維持未完成。

✅ [Windows Vulkan hosted 驅動驗收](Apps/Showcase/evidence/V1-Windows-Vulkan-Lavapipe-CI-2026-10-04/acceptance.md)：Mesa lavapipe 26.2.4 通過 14 個 checksum、25 張截圖、九個互動檢查及完整 210.006 秒導覽。測試 tag rc.4 的 Build 16/16、Release 5/5 通過，未發佈 draft 的 17 個附件包含獨立 Vulkan archive；此為軟體驅動覆蓋，Mac 尚未完成且暫緩處理。

✅ [桌面 tag rc.3 驗收](Apps/Showcase/evidence/V1-Desktop-Tag-RC3-2026-10-04/acceptance.md)：Build 16/16 與 Release 5/5 於紀錄的指定重跑後通過；未發佈 draft 保留 16 個附件、四個已驗證 ZIP 與桌面 CTest log（Linux 80/80、macOS 73/73、Windows 72/72）。

Windows DX12 本地交付：[驗收紀錄](Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md)。Development/Full 使用 `windows-showcase-development`，Shipping/Full 使用 `windows-showcase-shipping`；`-CompleteGuidedTour` 驗證完整 210 秒。開發機 GPU 證據不代表乾淨主機或實體顯示操作聲明已驗收。當次 JSON 驗證 F5 snapshot round trip、導覽重播歸零與指定 Lab 錯誤案例。乾淨 VM 紀錄：[驗收](Apps/Showcase/evidence/Windows-V1-CleanVM-VirtualBox-2026-10-03/acceptance.md)（虛擬 GPU，非實體顯示器）。實體顯示紀錄：[驗收](Apps/Showcase/evidence/Windows-V1-PhysicalDisplay-GTX960-2026-10-03/acceptance.md)。

[V1 視覺特色 Showcase Roadmap](Roadmap/zh-TW/V1-Visual-Identity-Roadmap.md)
已記錄 ✅ 使用者確認的美術方向並保存概念預覽：風格化遺跡庭院、暖陽、冷色陰影與青綠符文，
以及 [免費模型／貼圖候選](Roadmap/art/Free-Asset-Sourcing.md)。✅ VIS-M0 已採用三個 CC0 建築網格與 palette
貼圖，完成 Runtime 匯入／cook／bundle 載入、隨包來源／授權、可重現鏡頭及三次效能基線。
[Linux 驗收](Apps/Showcase/evidence/VIS-M0-Linux-AdoptedAssets-2026-10-05/acceptance.md)：
87/87 無 skip 與隔離 Development 套件啟動通過。VIS-M1 已整合共享直接光照 PBR、切線與材質貼圖綁定；Linux 硬體 sRGB 色彩過濾 92/92 通過，Linux cooked IBL 94/94 與 Shipping 套件通過，Linux 浮點 HDR 合成、方向光陰影與風格化光照各以 95/95 通過，VIS-M1／M2 跨平台驗收在 PR #322／#324 各以 18 項通過；主體美術／bloom／色彩、植被風動／裁切／啟動效果、100 秒導覽／自由鏡頭與實際品質設定已實作；VIS-M4／M5 在 PR #327／#328 全部 18 項通過。VIS-M3 美術與 VIS-M6 目標實機驗收仍待完成（5/7 通過）；
軟體渲染測量不代表 GTX 960 達標或 V1 最終平台驗收完成。

### Repository 狀態

Build CI 現依變更範圍分流：純 Markdown 執行文件檢查；程式、shader、建置／CI 設定、受保護
Markdown 路徑與 tag 保留完整矩陣。固定 `CI result` 彙總應執行工作；詳見
[Build CI 分流](Tools/Build/README.md)。此變更不改變引擎里程碑驗收或 repository branch protection。

✅ [Hosted CI 分流紀錄](Tools/Build/evidence/DocumentationRouting-2026-10-05.md)：
完整建置路線通過 18/18 工作；純 Markdown 路線只執行兩個輕量工作且通過，九個昂貴工作群組全部跳過。

目前已可建置及測試 Foundation、Core、RHI、Renderer、Runtime、API sample、Zig gameplay consumer 與 headless `NexoraShowcase`。下方里程碑章節會列出已實作的 portable contract foundation，並明確標示仍待完成的平台或 production backend。讀取持久化或外部資料的 parser（例如場景快照）會在配置記憶體前先拒絕惡意的大小欄位。

#### V2 Networking 狀態

V2-M5 portable foundation 已完成：`linux-headless` preset 僅建置 renderer-free server closure，而 `NexoraNetwork` 提供 synchronous loopback／simulated transport、portable socket-provider boundary 與 connection contract，包含 protocol／build identity 檢查、明確 channel semantics，以及 deterministic loss／latency／jitter simulation。Server runtime 擁有 fixed-step scheduling、bounded graceful drain、admission、per-client packet／byte budget（兩者都會把超出的工作延到下一個 tick，不會直接丟棄；即使單一封包超出整個 tick 的 byte budget，也保證每個 tick 至少能處理一個封包）、ordered replay capture 與 canonical state hash。V2-M6 已完成 portable reference scope：在既有 entity、schema、snapshot、delta 與 interest foundation 上，加入具 connection-local acknowledgement 與 re-entry baseline invalidation 的 dirty-generation dormancy、具序號的 client prediction、會 replay pending input 的 authoritative correction、deterministic fixed-point latency tests，以及 versioned replay log。Headless tests 涵蓋 fuzz-style malformed corpus、10,000 次 reconnect cycle、scheduling／budget／drain 行為、malformed／truncated data、baseline expiry、connection isolation、dormant wake-up、deterministic correction 與 network bug reproduction。`NexoraDedicatedServer` 僅透過 Network → Core → Foundation 連結，Renderer 與 presentation module 不會進入其 dependency closure。Native UDP／DTLS adapter、encryption 與 hosted production deployment 仍為 backend gate；loopback／simulated transport 不代表 production networking 已完成。

#### V2 GPU-driven 狀態

V2-M3 現已有一套由 C++ 與 Slang 共用的固定 36-byte indirect-command ABI：Vulkan／D3D12／Metal 相容的 non-indexed draw prefix 後接 backend-neutral classification metadata，且 Vulkan 直接消費此 canonical stride。D3D12 現已補上真實 storage-buffer binding、`Dispatch`、採 canonical stride 的 `ExecuteIndirect` 與 test-only GPU readback；本機 Windows NVIDIA GTX 960 以 Windows SDK `dxc` 產生的 `sm_6_0` artifact 通過 `renderer.v2_gpu_driven`，並取得精確的 `CompareGPUDrivenResults()` 比對。啟用 Slang 的 Linux Vulkan RenderGraph 專項測試已於 Mesa lavapipe 通過，包含 `CompareGPUDrivenResults()`；GitHub Actions run 36609837931 的完整 Linux development、shipping package/evidence、sanitizer 與 TSan jobs 亦全數通過。本 Work Mode 雲端容器缺少 `clang++`，因此本地完整 CTest 為 50/51；實體 GPU 效能、Metal execution 與完整 target-tier parity 仍待完成。所有 backend 現在都以同一條共用範圍規則，拒絕會讀超過所綁定 indirect buffer 的 `DrawIndirect`（已在 Validation device 與 Linux Vulkan 上驗證；D3D12 與 Metal 以 source contract 守住，仍待各自的目標主機）；因此 V2-M3 仍獨立於 46% Roadmap 總進度維持未完成。
Showcase Validation Lab 現提供 portable M0～M12 probe registry、誠實的五態 status model、版本化 JSON／Markdown report、CTest card mapping、四種受控 error injection，以及 M7～M10 capability-aware headless room evidence。3D Hub 現由同一 model 繪製全部十三張卡片，並回報穩定的 room/world-object 關聯，且不執行 CTest 邏輯；authored room visuals 與 target-host evidence 仍待完成。
Linux Showcase 互動驗收現會等待最多 25 秒取得首個軟體 Vulkan 畫面，並在 Xvfb 下使用標準 X11 關閉請求。

#### Shader 系統狀態

Shader production pipeline 現有 ✅ portable 驗收 gate：Editor 會不經 shell 直接呼叫設定的 `slangc` process、解析 Slang 2026 原生與單行 file／line／column／severity／backend／variant diagnostics、依每個 compile request 追蹤 source／include invalidation，並依明確 budget 快取成功的 Development variant；結果以編譯前的輸入快照標記，因此編譯期間存檔會被視為過期而丟棄。Runtime 會序列化及載入含 checksum 的 cooked artifact、強制 Shipping cooked-only admission、透過注入的 native adapter 建立 backend module、transactionally 發布 generation，且僅在 GPU fence 完成後回收被替換的 module。Renderer 提供包含 generation 的 pipeline-state key 與具命名 case 的 golden-image harness；啟用 Slang 與 Vulkan 的 Linux build 另有 `renderer.vulkan_golden_triangle`，在 Mesa lavapipe 上離屏渲染並與提交的基準圖比對。這只是 Linux 軟體光柵化的參考基準，DX12、Metal 與實體 GPU 基準仍是 target-host gate。`Shaders/Nexora/Common.slang` 現已提供 PBR／IBL、StylizedPBR、Anime、Vegetation、Water、Unlit、shadow／post-process、skinning／instancing／Forward+、variant key 與 retained-mode UI helper；`PbrSmoke.slang`／`UiSmoke.slang` 是含 IBL resource、Texture2DArray、atlas、clip、nine-slice 的實際 vertex／fragment smoke entry。Linux Slang 2026.18 的 SPIR-V／MSL 編譯與 shader contract／cross-compile tests 已通過；實際 DXIL／native backend execution 與 target-host capture golden baseline 仍是未完成的平台 gate，portable harness 不宣稱這些結果。
Renderer 現在也提供 ✅ backend-neutral material schema 與整合 contract，涵蓋六種 shading model、具 semantic 缺失貼圖 fallback 的 resource binding、used-variant stripping、generation-based hot reload，以及穩定的 Material Inspector reflection/layout hash。既有共用 Slang library 提供 PBR／IBL 與專用 shading helper；native DXIL／Metal execution 與實體 GPU 視覺驗收仍屬 target-host gate。


#### V2 後段 milestone portable 狀態

V2-M9 現已有 backend-neutral Timeline／Camera Rig、Flex／Grid、沿用 localization 的
RichText、Theme／StyleSheet、accessibility、Surface UI 投影、room／portal audio、
HLS／DASH segment contract 與 optional DRM／capture boundary，同時維持 V1
`VideoPlayer` contract。V2-M10 在既有 production commandlet 上加入 deterministic
content-addressed work identity、local-first Shared DDC、transactional patch verification、
generation pin／drain 與 native-code rejection。V2-M11 在 Core 加入版本化 diagnostics
wire schema、headless Trace ID correlation 與 PluginID attribution。V2-M12 定義五個
reference project 的 capability profile，並加入 bounded growth、reconnect drain、rollback、
save corruption、thermal-policy evidence 與 V1-like footprint limit 的快速 hardening probe。
這些仍只是 portable foundation；M9～M12 必須等 production、target-host、完整 reference
project 與長時間 soak gate 通過後才會驗收。

#### Engine API 狀態

Engine API 已完成 [Engine API 基礎 Roadmap](Roadmap/zh-TW/Engine_API_基礎_Roadmap.md) 定義的 **portable roadmap scope**。平台特定的 runtime evidence 仍須由目標平台驗證，不視為 API 功能缺漏。

| Track | 狀態 | 現有能力／剩餘 Gate |
| --- | --- | --- |
| API-M1 Math 與幾何 | Roadmap scope 已完成 | 已有完整 math、geometry、transform、ABI/layout tests、SSE2/NEON 路徑、獨立 DirectXMath 座標 golden 與可執行 sample；ARM runtime evidence 仍須在目標主機驗證。 |
| API-M2 基礎資料型別 | Roadmap scope 已完成 | UTF-8 string/view、buffer/span、UUID、name、result、parsing、generational handle，以及 caller-owned 或 opaque engine-owned C ABI buffer 均已實作及測試。 |
| API-M3 VFS 與檔案 I/O | Roadmap scope 已完成 | 已實作 directory、memory、唯讀 package 與 bundle backend、stream、range/async read、mapping、watch、atomic write、Shipping host-mount 限制及 >4 GiB sparse-offset gate。 |
| API-M4 Engine services | Roadmap scope 已完成 | Monotonic/game/fixed time、含版本的 deterministic random、configuration、logging、jobs、events 與 profiling-marker emission 均已有實作及測試；JobSystem shutdown 現有 drain、join、capture 釋放及 restart 的 lifecycle-leak regression coverage。 |
| API-M5 World/game facade | Roadmap scope 已完成 | Handle/value-based entity、scene、transform、camera、light、mesh-renderer、physics、character、audio、asset reference 與 input access 均已實作及測試。 |
| API-M6 Bindings 與版本化 | Roadmap scope 已完成 | 已實作 canonical C11 header、machine-readable ABI manifest、C 與 Zig consumer、append-only compatibility gate、versioned descriptor、連到真實 `GameWorld` 的 wire path，以及由 embedding host 擁有的 event／tick hook，並有測試覆蓋。 |

#### Zig Gameplay 與 Showcase 狀態

Zig 已不只是規劃中的語言方向。Repository 會建置 Zig 0.14.0 gameplay object 與 ABI smoke consumer；目前 headless/static ZS-M1 verification slice 由 C++ 擁有 `main`、Engine／World lifetime、fixed 與 variable update、offscreen rendering、transactional reload 及 shutdown，Zig 則透過公開 V3 ABI 修改真實 entity 的 Transform。支援的操作流程請參閱 [Showcase README](Apps/Showcase/README.md)。

ZS-M0 至 ZS-M5 已完成：capability-aware gallery 支援 camera input、selection raycast、誠實的 feature overlay 與已測 fallback；dynamic reload 也會穩定檔案、在 `on_start` 前恢復候選 state 以避免重複場景，並回報 callback/device-lost recovery scope。Windows 開發工作站上的 Development-dynamic 與 Shipping-monolithic/static 套件已從 checksum 驗證過的隔離副本啟動；獨立配置 Windows 乾淨機器的 Development package 已通過 16/16 checksum 並產出 PASS launch report。

#### Window 與 Native Presentation 狀態

[Window 與 Native Presentation Roadmap](Roadmap/zh-TW/Window_Presentation_Roadmap.md) 的**實作進度為 100%**：WP-M0 至 WP-M4 已交付模組邊界、native window／input 實作、DX12／Vulkan／Metal presentation path、Showcase／Editor 可重用 surface，以及 lifecycle／failure hardening。此百分比代表實作 scope，不代表跨平台 runtime 驗收已完成；Windows/DX12 WP-M1/WP-M2 runner evidence 已記錄，Showcase interactive、Linux/Windows Vulkan 與 macOS/Metal runtime 驗收仍須在對應 target host 通過。

#### Editor 狀態

ED-M1 現已有 real-index-backed 圖形化 Content Browser，以及可回復的 project-local
rename／move／delete／undo、generation-tagged drag/drop、dependency inspection 與可取消的
background reimport。Editor-owned import queue 也會在背景索引 selector project；worker 提供
bounded progress 與 structured diagnostic，只有 authoring thread 能啟用 index 或發布重新驗證
revision 後的 staged artifact。Versioned sibling `.meta` record 現可在 Editor 搬移與 process
reopen 後保留 asset UUID 與 artifact identity。Schema-2 project descriptor 現包含 stable UUID；
schema-1 project 會在
OS-held writer lease 下原子升級；明確的 read-only process 可共存且不能修改 project；
docked Project panel 會顯示 access／upgrade／recent-project 狀態。圖形化 Project Browser 現支援
create、read-write／read-only open 與 recent shortcut；project activation 為交易式，Linux Xvfb
也會從未提供 `--project` 的啟動流程操作 create/reopen。Content panel 現會顯示 dependency cycle，
並以明確的 Reload／Keep／Compare dialog 阻擋 dirty external change；Compare 會保留兩側 hash，
終局選擇由 authoring thread 記錄，UI 不直接操作 filesystem。實體顯示／Windows workflow
驗收仍待完成，因此 milestone 驗收比例不變。圖形化 Hierarchy 現已有 parent-aware expandable
tree、filter、以 generation 為 key 的 expansion 與 anchored multi-selection、可見列裁切提交、
可復原 rename、兄弟排序，以及透過 Editor Core 執行且拒絕 stale entity／document generation
的 cycle-safe drag/drop reparent。Docked Inspector 現可透過一個 generation-safe、atomic、可復原且經 Runtime validation 的 SceneDocument transaction，編輯單選或 mixed-value 多選的 local position、quaternion 與 scale；單選 Camera 與 Light 區也可新增／移除元件並編輯經驗證的視角、裁切面與非負亮度，支援 Undo 與場景持久化；完整 reflected Inspector 仍待完成。X11 window backend 現會以每視窗 XIM input context 將
committed UTF-8 解碼成
backend-neutral `Text` event，physical key 與 text input 維持分離。
圖形化 Hierarchy 或游標停留的原生 3D 畫布現可透過 Delete 刪除選取的 subtree，Undo 可復原；Hierarchy 也能在不改變剪貼簿的情況下複製目前選取。
中央 Scene panel 現有可點選物件的 X/Z 俯視概覽、Ctrl／Shift 多選、F 聚焦、平移、縮放及可單步復原的標記拖曳及可見的單軸把手，且逐場景保留概覽 camera 中心與縮放；Scene panel 也會標示未儲存內容，成功儲存或 Undo 回原狀後清除；原生關閉要求遇到未儲存內容時提供儲存後離開、捨棄後離開或取消；正式 3D renderer 輸出與 gizmo
仍待完成。

OBJ reimport 現在 revision 與記憶體預算檢查通過後一併發布不可變幾何及 hash；失敗／取消／過期
會保留舊 mesh。Content revision 會更新原生幾何，重新命名／刪除 Undo 也保留最新 payload。

原生 Scene 預覽現透過有界 shared mesh batch 繪製並以 triangle picking 選取解析後的 OBJ。
不同 triangle／quad 的 Xvfb 像素驗證 Center 預覽／提交及 Undo；缺失或超限 mesh 會警告並
保留代理。Authored Scene／Game geometry 現跨鏡像／剪切 ancestry 使用精確 world matrix，包含
Scene bounds／picking 及預期 gesture；Game 使用 live post-tick matrix 並在 Stop 後保持 ownership。
Material、GPU cache 與完整驗收仍待完成。

Position／Scale Inspector 現僅在 Enter 時提交為一個 Undo step。Mixed 草稿支援負數及科學
記號，保留各 entity 最新的其他欄位；Escape、失焦、selection／reload、Play Inspector 及
唯讀／復原／關閉確認會取消待提交編輯。真正鍵盤測試驗證輸入過程不修改場景及提交後持久化。

Inspector 現支援對 mixed 多選以一個 atomic Undo／Redo 指派匯入 OBJ mesh 或移除 MeshRenderer，
保留各 entity 的 material，並以真正 combo／remove 點擊、重複 replay 與 save／reload 驗證；拒絕過期選取／專案／
文件請求；完整 Scene View 驗收仍待完成。

匯入 mesh geometry 現有 owning、具 generation 檢查的 CPU catalog，以 UUID 穩定衍生 64-bit
場景資源 ID，並原子拒絕碰撞；持續的每資產 GPU cache 與完整 Scene View 驗收仍待完成。

Xvfb Undo 檢查現只在單次 Undo 後重試 Save，直到已提交的場景位元組相符。
X11 修飾鍵放開事件現會立即清除該組 flags，另一側仍按下時保留；原生 X11 事件測試涵蓋
Control、Shift、Alt 及 Super。Center 預覽像素先等待已變更且穩定的畫面，再比較放開後的結果。
原生編輯輸入現先於 Save／Save-and-exit 及 GPU 提交完成，因此放開拖曳後立即 Save 會包含
該次已完成的編輯。
Scene 拖曳現會在失焦、Undo／Redo 與建立／貼上／複製物件快捷鍵、文件替換、畫布隱藏或復原提示時取消；
真正的 Xvfb FocusOut 與 UI contract 驗證被放棄的預覽不會提交。

公共原生 SceneDrawData 現支援 Vulkan／DX12 同一個 depth pass 內的多組有界 indexed geometry／
instance 範圍。Portable 範圍檢查與不同幾何的 Vulkan 像素驗證涵蓋新邊界；Editor 實際 mesh 資產
residency 與完整 Scene View 驗收仍待完成。

Editor SceneDocument 現提供 generation-safe MeshRenderer 交易及查詢，mesh／material 資源 ID
可保留於 Undo／Redo 與場景儲存。完整 Scene View 驗收仍待完成。

背景 workspace 匯入現可 staging 不可變的已三角化 OBJ 幾何，包含有界來源／記憶體使用、UV、
法線產生、局部 bounds、取消及來源行號診斷。Typed OBJ geometry reimport 現可原子發布；持續的每資產 GPU cache 仍待完成。

原生 3D gizmo 現提供 Pivot／Center（P），支援多個選取根節點繞共同中心旋轉與縮放，
預覽與提交一致並可單次 Undo。雙根節點 Xvfb 流程驗證共同中心縮放、旋轉與放開後的畫面；完整 Scene View 驗收仍未完成。

原生 gizmo 預覽與提交現共用 SceneDocument 根節點編輯及 Runtime 階層組合；旋轉、鏡像與
非均勻縮放祖先下的子節點姿態一致。預期姿態快照不改動場景內容及 Undo／Redo。深層鏡像／剪切
ancestry 現保留精確世界原點與 gizmo 位置換算，提供 owning world／preview matrix，並以 closed-form
位移／Center 旋轉／縮放、Undo／Redo 及 save／reload 測試驗證。原生 authored-mesh affine
繪製／picking 現已使用這些 matrix，圖形化里程碑驗收仍為 0/8。

Presentation instance 現支援精確 row-major affine model matrix 與共用 native inverse-transpose
normal packing。精確 binary32 determinant 判定避免相消錯誤；portable contract 與 Vulkan pixel
比較鏡像／剪切 geometry 與獨立烘焙的參考結果。Scene／Game authored mesh 現透過 CPU validator
使用精確 matrix，並以 closed-form picking 及 stale-snapshot／Stop ownership 測試驗證；
graphical milestone 仍未驗收。

圖形化 Scene 概覽現可選擇 0.25 至 4 世界單位的移動吸附；拖曳預覽與可復原的提交位移一致，
包含有父節點的物件。
Scene 的 Undo／Redo 現會重播穩定 entity ID、階層與元件，並保留節點名稱、選取及輸入的 Euler
圈數。Scene panel 有按鈕與快捷鍵；新編輯會清除已撤銷的分支。

Portable Editor Core 現亦會在 delete/undo 間保留 Content Browser selection、回復局部套用的
gizmo preview、拒絕無效 camera state，且 camera／autosave／build file 替換不會刪除不相容的
destination；shader variant 的 key 與 compiler invocation 會包含 define、過期 compiler output
不會被接受，有限值 snapping 也不會溢位成無限值。這些 robustness 修正不改變圖形化 milestone
驗收比例。

Docked Console 現會顯示有界 Runtime 紀錄，提供文字／嚴重度篩選、來源、時間戳與
丟棄數；啟動與場景儲存診斷已接入。完整 Game View 材質與 log 路由仍待完成。
Docked Profiler 現會繪製有界的 Editor frame processing wall-time 即時曲線，支援暫停／
清除並顯示最新／平均／峰值與丟棄數；GPU 時間及 process memory 會明示為尚無量測。
✅ Profiler 現可把保留的 Editor frame-processing wall time 匯出為專案 CSV，保留 double
精度與丟棄 frame 數，GPU／memory 欄保持空白。同步 writer 驗證 1-600 筆有序且有限的 sample，
拒絕唯讀／recovery 寫入，驗證失敗會保留舊檔；實際 UI 點擊會送出一次性 request。

✅ Vulkan Scene／Game upload 現會重用 fence-protected frame slot 的有界容量。穩定、縮小
或沒有 Scene 的 frame 保留配置；放大時先完成新配置才釋放舊配置，resize／teardown 會等待
GPU 完成。Native call tracing 與像素測試驗證 100 個穩定 frame、最大 descriptor 預算、
放大失敗及無洩漏生命週期。每次 draw 仍複製最新 geometry；persistent per-asset GPU cache
及完整圖形化驗收仍待完成。

✅ Hierarchy 現可選擇 Empty、Camera 或 Light，套用於 Create root／Create child 及
  Ctrl+Shift+N。Camera／Light 以 identity local TRS 初始化，並具完整 stable-ID 的單步
  Undo／Redo。預設名稱隨型別選擇調整，自訂名稱保留。Owning queued request 重新核對
  存取權與 scene／parent generation。Portable 及 1x／2x 真正選單／pointer／key 測試涵蓋 root／child、
  過期 scene／parent、唯讀／modal gate、100-step replay 及 save／reload。完整 reflected component
  creation 與 target-host 驗收仍待完成。

✅ Inspector Copy values／Paste values 現可擷取單一 entity 已提交的 Transform／Euler、
  Camera 或 Light 數值，再以原子單步 Undo 套用到多選。Camera／Light 保留缺少元件的狀態；
  Transform 保留作者輸入的圈數。Typed owning clipboard 跨來源修改／刪除及 reload 保留，
  並獨立於 Hierarchy clipboard。Portable 及 1x／2x 真正 UI input 測試涵蓋唯讀 Copy、
  停用／不符型別的 Paste、草稿取消、no-op Redo 及持久化。完整 reflected Inspector 仍待完成。

✅ Inspector 現提供多選 Reset Transform、Reset Camera 及 Reset Light。Transform 重設會
清除 local TRS 與可見／過期的 Euler 圈數；Camera／Light 重設保留缺少元件的狀態。變更 batch 以原子
單步 Undo／Redo 提交，no-op 保留 Redo。重設取消未提交草稿及 Scene 手勢，並遵守 workspace／
modal gate。Portable 與 1x／2x 真正 UI 輸入測試涵蓋 metadata-only Undo、mixed presence、
無關 payload 保留及 save／reload。完整 reflected Inspector 與 target-host 驗收仍待完成。

✅ Camera／Light Inspector 現支援多選及 mixed presence／value。Mixed component 的 Enable
會補到缺少的 entity 並保留現有值；Enter 只套用編輯欄位，以 generation-checked 原子交易
完成單步 Undo／Redo。無效／過期／重複 batch 與唯讀／復原寫入會整批拒絕。真實鍵盤測試
涵蓋 Camera FOV、Light intensity、未編輯欄位保留、重複 Undo／Redo 及保存／重開；完整
reflected 編輯仍待完成。

Camera／Light 草稿及 pending request 現於失焦、取消選取／reload、Inspector 收合、Play
Inspector 與唯讀／復原／關閉確認時取消。阻擋期間停用控制項，queued batch 必須符合當前
選取；真正鍵盤生命週期測試防止舊編輯復活。不可用的 target-host 驗收暫緩，其他 Editor
實作繼續進行。

✅ File New／Open／Save／Save As 現管理單一活動場景，支援 dirty Save／Discard／Cancel、
Untitled Save As 與明確覆寫確認。Document／project token 拒絕過期操作；Open 失敗保留 history
與內容；覆寫失敗也會保留目的路徑及既有暫存檔。Canonical alias 維持 metadata 範圍，關閉
儲存失敗後可直接重試。Content 儲存只匯入自己的來源，並保留 persistent asset identity 與先前 Content Undo；各檔案 camera state
在切換及可寫 shutdown 時保留。真正 1×／2× UI 與 Linux Xvfb 驗證 application 流程及唯讀
專案檔案不變。Content 中文路徑、資料夾／檔名顯示、搜尋及改名／移動／Undo 使用 UTF-8 與原生檔案路徑。
✅ 啟動現會恢復上次成功 Open／Save 的場景及 view state，包含唯讀重開；無效／無法載入的
啟動資料回到 Main 並保留原檔案。獨立 metadata 儲存失敗不會回復場景儲存。Linux Xvfb 以真正
重啟後編輯／儲存目的檔案及 fallback 驗證流程；additive scenes 與完整圖形里程碑驗收仍待完成。

✅ Content Browser 可透過雙擊、context Open scene、Open scene 按鈕或 focused Enter 開啟
場景。Owning request 在 dirty 確認中保留 UTF-8 目的路徑，支援唯讀；Play／modal／過期 token
拒絕替換。真正 1×／2× pointer／key 測試驗證流程；additive scene tab 仍待完成。

✅ Focused Content F2 現可替單一資產改名，選取 UTF-8 檔名輸入並支援 Enter／Apply 與
Escape／Cancel。無效名稱可重試，同名清除錯誤並保留 Undo；stale／access／modal gate 取消 draft，
不改動文件 history。真正 1×／2× 及原生 Linux 鍵盤流程驗證操作；實體 IME 與完整圖形
驗收仍待完成。
失焦事件會立即取消 draft，即使繪製延後至恢復焦點之後才發生。

✅ 目前 Content 場景現以 UUID 跟隨重新命名／移動與 Content Undo，保留文件修改、history
及 view state。已提交的移動會更新啟動路徑，即使關閉時捨棄未儲存修改也能重開該來源。
刪除／失去追蹤資產後禁止一般 Save；還原後恢復儲存，同一路徑的不同 UUID
維持保護。已儲存場景 index 可更新移動後的 UUID 路徑，不掃描其他來源。Portable 與原生
Linux 測試驗證路徑跟隨、Save、刪除及 Undo；圖形里程碑驗收仍為部分完成。

✅ Camera Inspector 現可將單一 Camera 對齊已儲存的 Scene 3D pose，保留 lens、scale 及
parent，並以一次 Undo 還原。Runtime matrix 驗證 shear／mirrored 父鏈；等價 pose 保留
Redo，唯讀／不可用 view 與 stale 目標拒絕編輯；極端有限 center 共用 native preview 的
+/-100,000 限制；save／reload 保留對齊結果。

✅ Content 現可將單一已解析 mesh 放到 Scene center 並選取新 root；初始化建立共用一次
Undo，Redo 與 save／reload 保留 stable ID、名稱、pose 與 mesh／material 資料。真正 UI 點擊
驗證 generation／access／modal gate、native center 限制，以及真實 Game geometry。
✅ Typed Content mesh 拖曳現能以一次 Undo 在 overview 游標或 native Scene ground 落點建立
root。真正 1x／2x DPI pointer 測試涵蓋放開、持久化及拒絕／取消拖曳；tooltip 預覽不改動
World。表面吸附及 geometry ghost 仍待完成。

✅ Hierarchy 有焦點時，F2 現開啟 Rename 並聚焦／全選名稱；Enter 提交一次 metadata Undo，
  Escape 取消。Dialog 阻擋其他 authoring／clipboard／Undo／Save／Play 快捷鍵及 queued
  Hierarchy 寫入。真正 1x／2x input 驗證 UTF-8 CJK／supplementary 字元、空名稱重試、
  clipboard／history 保留、save／reload，以及 write loss、外部 modal、失焦或 stale document
  的取消。ImGui／library consumer 共用 32-bit Unicode text；字型涵蓋及實體 Windows IME
  驗收仍待完成。

✅ Hierarchy 有焦點時，Ctrl+A 現經 generation-keyed selection 選取全部 filter／expansion
  可見列，包含被裁切的列。空結果清除選取，唯讀 project 仍可使用。鍵盤測試驗證 collapsed
  descendant、filter 順序、其他 panel／text-input 焦點、recovery／close gate，以及 World／
  Redo 不變。大型 scene 的 scale／soak 驗收仍待完成。

✅ Typed Content mesh 拖曳現可指派 Inspector Mesh field 的顯示選取。Hover 只預覽，
  放開後以一次 generation-checked batch 保留既有材質並補上缺少的 MeshRenderer。真正
  1x／2x pointer 測試涵蓋初始化 Undo／Redo、save／reload、非 mesh／stale catalog／project
  拒絕、取消 input 與 workspace／modal gate；拒絕保留 Redo。完整材質及 reflected Inspector
  流程仍待完成。

✅ Hierarchy Cut 與 Ctrl+X 現先擷取完整選取 forest，再以一次 atomic transaction 刪除。
  Undo 還原原始 ID 與選取；首次成功 Paste 保留 root 名稱並建立新 ID，之後保留的 clipboard
  資料改用 Copy 命名。失敗 Cut／Paste 與 Duplicate 保留 pending clipboard state。真正
  1x／2x key／pointer、workspace／modal／text-input gate、replay 與持久化測試涵蓋生命週期。
  新 layout 將 Inspector dock 在右側，保持 Hierarchy 可操作。

✅ Copy／Paste／Duplicate 現以 owning copy-time snapshot 保留完整 root forest、元件、Euler
hint 與 opaque payload。一次 Undo 移除全部新 forest 並還原選取，Redo 保留初始化 pose 及
stable ID；鍵盤／持久化及 1,000 次 cycle 測試涵蓋此路徑。

✅ 多選 Delete 現將全部選取 subtree 記為一次 atomic Undo。鍵盤重播還原 hierarchy、
元件、authoring metadata 及完整選取；測試涵蓋 collision／lifecycle 拒絕、無關 entity 保留
及 1,000 次 replay cycle。

Scene／Hierarchy 控制項、快捷鍵與 queued 編輯現共用 workspace 可寫及 modal gate。
唯讀選取、Copy 與鏡頭導航仍可用；pointer 測試驗證權限切換後，已中斷的 overview／native
拖曳不會提交，且 Undo／Redo 歷史保持完整。

✅ 缺少外掛的元件現有有界唯讀 Inspector，顯示 owning 名稱、完整 entity／type ID、bytes
與最多 64-byte 預覽。Scene format 3 會在保存／重載、clipboard 複製、刪除及獨立 metadata
Undo／Redo 中保留 opaque data；無 opaque 資料仍寫 format 2，reader 相容 format 1／2。
Generation 與每元件 1 MiB、總 payload 16 MiB、每 entity 64、總 4096 records 限制會拒絕過期／
超限匯入。損壞、重複、orphan 與缺失 entity 紀錄在修改 live state 前拒絕。外掛還原／執行
及完整 reflected 編輯仍待完成。

✅ 原生 client-pixel pointer event 現會先依當前 frame DPI 轉成 UI 邏輯座標，再做 hit test。
scale 改變會重新投影快取位置，失焦會清除快取，DPI 改變會取消中斷的 Scene gesture。測試涵蓋
100／125／150／175／200%、小數／負座標、事件排序、靜止 pointer、無效 scale fallback，
以及 200% Apply dialog 點擊；render deferred／zero-extent frame 也會轉送 gameplay 按鍵釋放與失焦，
不需 GUI frame 或 Play tick；target-host 實體顯示器 DPI 證據仍待完成。

Linux Xvfb 驗收現會透過 Ctrl+Shift+N 建立 Hierarchy 根節點、儲存場景並重啟圖形化 Editor，
檢查新增節點重新載入後仍存在；完整 ED-M2 視覺驗收仍待完成。

Docked Game panel 現可控制隔離的 Play World，提供 Play／Stop、Pause／Resume、單步 fixed tick
與複製的 entity 檢視清單及有界的 X/Z 世界座標俯視預覽。Linux Xvfb 會操作 F5／F6／F10。
✅ 有界原生 Game View 已透過 Play camera 繪製 OBJ，Start 凍結資產，fixed tick 後 frame
擁有資料；Xvfb/lavapipe 已驗證像素與 Pause／Step／Stop、Editor 場景未變動。
完整材質、同視窗多個原生 canvas、完整 gameplay 服務與擴充 input routing 仍待完成。
✅ Game View 現有 active-camera 預覽選單與 Automatic 回退；選擇只屬於目前 Play session。
移除、scene 卸載、無效投影或新 Play 會安全重設。真正 UI 點擊驗證唯讀預覽可用，且 Editor／
Play 資料與 Undo 未變動；frame 會在 tick 後重新驗證。
✅ Runtime component wire 現可透過共用解碼與原子 command 驗證直接讀寫隔離的 PlaySession
World；Fixed／Step 測試證明寫入不會回流 Editor。
✅ Game panel 可載入 project-relative V3 gameplay library 到 clone，執行 FixedUpdate／Update、
把有界日誌送至 Console，並在 Stop 前卸載。Xvfb 已驗證真實 library 移動 mesh 且 Editor 場景
未變動。擴充 input routing、完整 scene／physics 服務、hot reload 仍待完成。
✅ 點擊 playing Game canvas 現可透過 owning user-zero snapshot 路由鍵盤位移與滑鼠／按鈕。
Escape、pointer 離開、Pause／隱藏／失焦／提示視窗會清除 held state；擷取中的按鍵不會觸發
Editor 快捷鍵。Xvfb 已驗證 input-only 原生 mesh 移動及 D 仍按住時 Escape 釋放。
Gamepad、pointer look 與 rebinding 仍待完成。

✅ 選取 Game entity 現會開啟唯讀 Play Inspector，顯示複製的 local／world pose、parent、
scene state、Camera／Light 與完整寬度的 mesh／shader ID。Game panel 顯示暫停原因及 callback
失敗次數；每 frame 與 fixed callback 失敗皆會釋放 input。快照在元件移除與 Stop 後仍有效，
Editor selection 與編輯狀態保持獨立。

✅ Gameplay library 選擇現會存入有界 schema-1 專案設定，與場景及 recovery journal 分開。
唯讀存取不會寫入；無效設定會保留至明確替換，未處理的 recovery journal 會阻擋關閉時儲存。
明確 CLI 路徑（含空字串）優先於已存設定。Xvfb 已驗證不帶 CLI 路徑重新開啟後，已存 library
仍可驅動 Play mesh；單純重新開啟不會載入模組。

✅ Game Apply Changes 現會先暫停並顯示 owning transform diff。確認時重新檢查 Play、document
與 entity generation，以及 original／Editor／Play 值，再用單次 atomic SceneDocument transaction
套用並停止、捨棄 clone。衝突／重新掛接／其他場景會拒絕整批；Undo 會還原所有套用值。
元件／建立／刪除不會複製；modal 會阻擋 authoring／Play 快捷鍵，預設 Stop 仍捨棄變更。

Desktop graphical CI test 現區分 deterministic Ctrl input fixture 與 native macOS Cmd／Super
Undo／Redo 路徑；production 保留平台原生 input policy。 Fixture 現在 workspace／file owner 銷毀後才刪除暫存 project，canonical scene 使用 binary 寫入；Linux interaction gate 明確取得 Scene canvas 焦點後執行 shortcut。

[圖形化 Editor Roadmap](Roadmap/zh-TW/Editor_Roadmap.md) 的**圖形化 milestone 驗收仍為 0/8（0%）**。Portable foundation 除既有 ED-M1 至 ED-M3 contract 外，現已加入 ED-M4 additive-scene ownership 與 dependency ordering、migration dry-run、bounded autosave recovery，以及 source-control-neutral three-way conflict，另有與 UI 無關的 viewport pick ray、AABB picking、軸向拖曳、snapping 與 resize hysteresis 數學。[旋轉與縮放計畫](Roadmap/zh-TW/Transform_Rotation_Scale_Plan.md)的 ✅ 所有階段皆已完成：`runtime::Transform` 含 quaternion 旋轉與逐軸縮放（沿用 Unity／Unreal 慣例），只寫位置的寫入者會保留它們，`ViewportMath.h` 提供 Unity 式的移動／旋轉／縮放 gizmo 數學（Global／Local 軸、Pivot／Center、父物件、負縮放規則與多選最上層判定，由 `editor.viewport_math` 涵蓋），gameplay module 可透過 append-only 的 C／Zig wire 讀寫 `"Nexora.TransformV2"`、`"Nexora.WorldTransform"` 與 `"Nexora.Parent"`，其 layout 由 C11、ABI baseline 與 Zig 檢查固定；圖形化旋轉欄位現使用度數、Z-X-Y composition 與單次 atomic 多選 transaction；✅ SceneDocument 提示跨 selection／save／reload 與 undo 保留輸入圈數，並具已驗證的 version-2 metadata、version-1 相容性與 atomic 同 World reload。[Entity parenting 計畫](Roadmap/zh-TW/Entity_Parenting_Plan.md)第 1、2 階段 ✅ 已完成：entity 形成 Unity 式階層，具 local transform、world transform 與精確的 world matrix、保留世界姿態的重新掛接，以及連帶刪除；場景快照為 v3（v1、v2 仍可載入），Editor 的場景文件以 runtime 階層為準，角色控制器也能像 Unity 一樣掛在父物件底下（移動中的父物件會帶著它走）。階層批次全有或全無、快照驗證與連帶刪除與場景大小成線性，PIE apply-back 會拒絕遊玩期間被重新掛接的 entity（`runtime.entity_parenting`、`editor.preview_contract`；Linux development、全功能、Shipping、ASan/UBSan 與關閉 Editor SDK 的組態，合併後也通過完整 CI）。Unity 式的兄弟順序（`SetSiblingIndex`、重新掛接後成為最後一個子物件）與可復原的 Hierarchy 拖曳模型（`SceneDocument::Move`）已就緒；`RenderSceneSync` 會以每個 entity 精確的 world matrix 與保守的包圍球，把 mesh renderer 同步到 `GPUScene`，移動父物件時整棵子樹的渲染資料都會更新，攝影機也會跟著父物件走（`CameraView`，以及經視錐剔除的 `RenderSceneSync::RenderFrame`；`runtime.render_sync`）；目前還沒有應用程式的繪製迴圈使用它。以上是 portable 的數學與資料 contract；圖形化旋轉把手已有原生環狀操作，Local X/Y/Z 縮放立方把手及白色等比例把手也已可操作；原生代理預覽已有世界 X/Y/Z 位移把手。Linux Editor 顯示驗收在修正它暴露的三個真實缺陷後，已能在虛擬顯示器（Xvfb 搭配 Mesa lavapipe）上於本機通過；已由專用 CI job（`editor-linux-display`）執行，另有 Windows/DX12 開發機 bounded shell smoke 通過；實體顯示器 Linux 與 Windows DPI／IME 證據仍待完成。虛擬顯示 recovery gate 現會 fsync seeded journal、SIGKILL 真正的 Editor，確認 journal 與已提交 workspace 保留，再重啟並重新取得 writer lease，執行 keyboard-only Recover／Discard；recovery process 失敗不會只憑 diagnostic 就通過。Focused [ED-M0 Dear ImGui 計畫](Roadmap/zh-TW/Editor_ImGui_Integration_Plan.md) 現已 ✅ 通過 WP0 baseline（[證據](Tools/Build/evidence/EditorEDM0-Linux-2026-10-05.md)：graphical OFF 71/71、ON 加 Slang 115/115 無 skipped，Shipping engine build 通過），整體仍為**施工中**；✅ [Linux Vulkan 驗證 slice](Tools/Build/evidence/EditorEDM0-VulkanValidation-2026-10-06.md)已修正 attachment 同步與 native RHI shader feature negotiation，CI 啟用同步驗證並拒絕 exit-zero error、保存 log；desktop CI 現在三個平台啟用 graphical Editor、要求 Windows DPI／IME contract 註冊並保留完整測試 log；圖形化 workflow、native debugger integration、physical-display evidence 與 UI 驗收仍待完成。因此 ED-M0 至 ED-M7 都不打勾；portable prerequisite 不會向上取整為已驗收的 graphical milestone。
Linux 虛擬顯示驗收現允許忙碌 CI 主機上的 recovery 重啟在 90 秒內完成；原生 Scene 的 Undo 驗收會於判定不符前再次儲存已穩定的狀態。

原生 Vulkan／DX12 場景繪製 contract 現支援裁切於實體像素 viewport；portable 邊界檢查與
Vulkan Xvfb 像素證據已涵蓋此功能。
Docked Scene canvas 現會在 layout 與 DPI 縮放後提供可見的 framebuffer 像素矩形；
Editor 的 3D Preview 切換現會在 UI 提交後於該矩形繪製原生有深度測試的地面與 live entity
位置代理；代理現會反映合成後的世界旋轉與縮放。解析後的 OBJ 現以原生 mesh batch 與
精確 affine world matrix 繪製；material shader、完整 3D 編輯與圖形化 Scene View 驗收仍待完成。

✅ Native Move 現增加 Global／Local XY／XZ／YZ 平面把手，使用現有 Pivot／Center。共用命中與
preview／release 計算，保留 selection 並限制／吸附兩軸；Shift 保持明確選中的平面。測試涵蓋
鏡像父節點、descendants、unknown bytes 與 Undo／Redo；Linux Xvfb 經六個平面儲存 proxy／OBJ
roots 並驗證 Escape 取消。完整 gizmo 驗收仍未完成。

✅ Native Scene 的 X 現可切換 Global／Local axes（Scale 維持 Local），P 切換 Pivot／Center，
在同幀點擊／拖曳開始前處理。1x／2x 輸入驗證 Home navigation、focus／modifier／modal／gesture
限制並保留 World／Redo；Linux Xvfb 驗證 proxy／OBJ roots 的 Home／P center 操作。
完整 gizmo 與目標主機驗收仍未完成。

✅ Focused Scene Ctrl+A／Select all 現可選取 overview 節點或實際 bounded native submission，
不改動 World、dirty state、clipboard 或 history。Owning token、目前 NodeKey、1×／2× gate、
4,001-node limit 與 native proxy／OBJ 選取已驗證；完整 Scene View 驗收仍待完成。

✅ Native Scene Select（Q）現可在沒有變形把手或拖曳提交的情況下點選 entity；W／E／R
回到 Move／Rotate／Scale。真正 1×／2× 輸入驗證 tool gate、Home 導覽與保留 Redo；
Linux Xvfb 驗證 proxy／OBJ picking 及 Select 拖曳後 saved bytes 不變。
完整圖形 Scene View 驗收仍待完成。

✅ Focused Scene Home 與 Frame all 現可對準整個場景，不改動 selection 或 history。
Native 使用實際 bounded submission candidate 與 affine mesh／proxy bounds；overview 配合
canvas 對準全部世界 origin。Owning session token 僅於發出該 request 的 frame 套用一次。
唯讀、空／拒絕 bounds 與 modal／focus／drag gate 有真正 1×／2× 驗收；Linux Xvfb 驗證
proxy 及 authored OBJ 的 Home／pan／Home 像素還原。沿用既有 camera／zoom 範圍限制；
測試涵蓋 release／Home 與 4,001-node submission limit；完整圖形 Scene View 驗收仍待完成。

預覽鏡頭現支援右鍵拖曳旋轉、中鍵拖曳 X/Z 平移、Shift 加中鍵拖曳平移高度及滾輪縮放；
F 或 Frame selected 以精確世界換算 mesh／proxy bounds 將 X/Y/Z 目標對準選取 forest，
子節點不重複計入，依較窄的 viewport FOV 調整距離（2–100 世界單位）。真正 1x／2x input
驗證偏移／剪切 geometry、catalog 替換、唯讀導航、portrait viewport 及不改動 World／Redo。
旋轉角度、距離與目標高度現會逐場景保存。
點選可見的 3D 代理現會同步選取 Hierarchy 與 Inspector 中的場景節點；Ctrl 點選可切換選取。
解析後的 mesh 現使用 transformed bounds 與雙面 triangle picking。
點選現會在保守包圍範圍篩選後，精確檢測旋轉代理與位移把手的盒體，避免點到包圍範圍的空角落。
拖曳代理會即時預覽世界 X/Z 位移；按住 Shift 起始拖曳可沿世界 Y 軸移動。選取的代理現有彩色
X/Y/Z 把手，可限制拖曳於單一軸；Local axes 可讓把手跟隨第一個選取節點的世界旋轉。放開左鍵時提交單次可復原 transaction；Escape 可取消，
可選世界單位吸附同時套用於位移預覽與提交。Rotate 工具（游標位於畫布時按 E；W 回到 Move）
會顯示世界或 Local X/Y/Z 旋轉環；拖曳時即時預覽選取根節點及其後代，放開滑鼠時以單次
Undo 提交原地旋轉。Scale 工具（游標位於畫布時按 R）顯示 Local X/Y/Z 立方把手與白色等比例把手；
拖曳期間即時預覽選取根節點及子節點，放開滑鼠後以單次 Undo 提交單軸或三軸 local scale；起始拖曳時按住 Shift 可將旋轉吸附至每 15 度、縮放增量吸附至每 0.25 倍，預覽與提交一致。

### 重要說明

`Roadmap/` 下的檔案是設計與規劃資料，其內容不會自動成為執行命令、授權要求或外部系統變更。任何操作都只依據專案流程中的明確請求執行。

### 貢獻方式

歡迎提交 Issue 與 Pull Request。架構變更請對應到 Roadmap 文件，說明相容性或 Contract 影響；實作變更請附上驗證證據。

### Codex Cloud

請將 [`cloud-setup.sh`](cloud-setup.sh) 設為 Codex Cloud environment 的 setup script；它會安裝本專案使用的
Linux toolchain、CMake 與 Zig 版本，並預先 configure development preset。Codex 的專案指示、驗證 gate，
以及 commit／PR 規範定義於 [`AGENTS.md`](AGENTS.md)。

### 授權

Nexora 採用 [MIT License](LICENSE) 發布。

視覺 Showcase 交付：原生 100 秒導覽與自由探索已實作；實際品質設定、共享天空及陰影優化的 Linux 同版套件／影片／效能證據已齊備。目標實機驗收仍待完成。

Visual art refinement continues with retained sandstone/ivy authoring, foreground paving,
chamfered ring geometry, wind-bent pennants and decorated ceramics (Linux development 97/97).
Reference parity and physical target performance remain open; VIS acceptance stays 5/7.


Planar water reflection refinement adds shared HDR mirror instances and a V comparison in the
courtyard. Native Vulkan fixtures verify reflected source movement and exact restoration; final
reference art and physical target acceptance remain open (VIS 5/7).
水平水面倒影已加入共享 HDR mirror instance 與庭院 V 比較；Vulkan 原生測試驗證物件移動及還原。
預覽圖一致性與目標實機驗收仍待完成，VIS 維持 5/7。


The crystal now supports tinted linear HDR transparency and U comparison. Native fixtures verify coverage, transmitted color and restoration before tone mapping. Reference-image parity remains open (VIS 5/7).
水晶加入帶色線性 HDR 透明合成與 U 比較；原生檢查驗證覆蓋率、透光色與還原。預覽圖一致性仍待完成（VIS 5/7）。


World-projected stone maps now preserve detail across columns and paving; an original cloud
panorama feeds both skybox and derived HDR environment light. Bounded shoreline contours and
pedestal relief refine the scene. Reference parity remains under development (VIS 5/7).
世界座標石材貼圖改善柱體與鋪面的細節；原創雲層全景同時供 skybox 與衍生 HDR 環境光使用。
水岸輪廓與基座浮雕持續調整；預覽圖一致性仍在製作，VIS 維持 5/7。


Linear HDR distance haze adds background depth while preserving authored sky/emitter radiance;
F7 compares the effect. Sun direction remains shared by sky, IBL and directional lighting.
Reference-image fidelity remains in progress (VIS 5/7).
線性 HDR 距離霧化改善背景層次，保留天空與發光物件的原始輻射值，F7 可切換比較。
太陽方位由天空、IBL 與方向光共用；預覽圖一致性持續製作，VIS 維持 5/7。


The next art iteration shares fluted-column geometry through native instances and adds wind-
driven ground cover/climbing ivy, warmer painted ceramics and a wider pedestal base.
Linux full validation passes 97/97 (89.00 seconds); Shipping evidence and reference parity remain in progress (VIS 5/7).
下一輪美術調整以原生 instance 共用石柱網格，加入風動地被／攀爬常春藤、暖色彩繪陶器與
較寬的基座。Linux 全套 97/97（89.00 秒）通過；Shipping 證據與預覽圖一致性持續進行，VIS 維持 5/7。

Crystal diffuse/backlight fill is reduced to retain transmitted background and sharper facets;
stronger linear rune radiance feeds HDR bloom. Original column relief and distant tower fluting
add geometric detail within the existing native vertex budget.


World-projected stone normals add bounded surface reflections across columns and paving,
sharing source coordinates with base/ORM and reflections. Linux full validation passes 97/97
(92.68 seconds), including 69 native PBR cases; Shipping evidence remains in progress (VIS 5/7).
世界座標石材法線加入有界的凹凸反光，與 base／ORM 及倒影共用原始座標。
Linux 全套 97/97（92.68 秒）與 69 個原生 PBR 案例通過；交付證據持續進行，VIS 維持 5/7。


Eight original chipped paving meshes now share 432 native affine instances. Deterministic
stone heights/widths and source-world PBR maps remain coherent; identity/column/paving/mirror
ranges are retained across cached frames and quality changes. The reclaimed vertex budget
supports a sculpted central basin/ribs, bevelled pedestal lips, staggered arcade masonry and
raised geometric column relief. Layered cutout tree crowns share GPU wind and leaf lighting.
The fixed activated Standard shot contains 51,790 vertices and 1,338 source foliage quads.
Linux full validation passes 97/97 (95.21 seconds), including all three vertex budgets and native
instance/wind/effect replay. Shipping evidence and final reference parity remain open (VIS 5/7).


Bounded screen-space crystal refraction now samples a private opaque linear-HDR snapshot before
transparent rendering. Shared Slang projects bent camera rays through an authored slab; each
axis is limited to 24 pixels, with nearer-foreground rejection. Standard/High crystal uses
index 1.46 and 0.65 world-unit thickness, while Basic/defaults retain ordinary tint coverage.
Native Vulkan/DX12/Metal adapters preserve opaque depth and own snapshot lifetime through frame
fences/resize. Six native checks cover bending, reversal, exact replay, zero thickness and
foreground rejection (75 PBR frames). Linux full validation passes 97/97 (92.94 seconds), including all 75 PBR frames and native F8
change/exact restoration. Shipping/reference validation remains open (VIS 5/7). This model
excludes offscreen and multiple transparent layers.


Vulkan synchronization validation now covers opaque-HDR refraction. Swapchain acquisition and
the copied HDR color transition include attachment-load reads; compatible HDR clear/load
passes share color/depth read dependencies. This preserves pipeline/framebuffer compatibility
while loading opaque depth and color for the glass phase. Khronos core/synchronization validation
passes all 75 native PBR frames. The full Linux configure/build/test rerun passes 97/97
(95.08 seconds) with validation layers enabled. Release rerun remains pending.

Lit scene texture generations now build bounded native mip chains: linear-light sRGB colors,
linear ORM data and normalized-vector normal maps. Cutout masks, unlit atlases, ambiguous
roles and UI remain single-level. Vulkan/DX12/Metal upload the same private CPU chain into
existing generation-owned textures, reducing distant stone aliasing without changing source
assets, passes, constant packets or C/Zig ABI. CPU semantic checks and two native checker/gray
minification cases cover the new filtering (77 PBR frames). Linux full validation passes 97/97
(96.26 seconds), with native effect replay and all quality budgets. Shipping/reference validation
remains open; VIS stays 5/7.


Closed courtyard crystals now opt into front-surface-only refraction. Shared Slang rejects
rear geometric facets using the source normal and real/virtual reflection camera, so a later
back face cannot replace the front face with another opaque-HDR sample. General glass stays
two-sided by default. The flag requires active lit translucent HDR refraction and uses private
packet offset 78; the 368-byte packet and stable C/Zig ABI stay unchanged. F8/U/Basic restore
default behavior. Two native cases compare rear-face rejection with unchanged double-sided
refraction, plus CPU validation/packing checks (79 PBR frames). Linux full validation passes 97/97 (98.20 seconds); release validation is
pending; final reference parity and physical target acceptance remain open. VIS stays 5/7.


The courtyard art pass now maps stone at 1.1 repeats per world unit with restrained normal
strength, so authored pores read as surface detail rather than large mottled patches. Pedestal,
basin and ceramic lathe profiles use 48 radial segments; columns retain 64 fluted segments.
Brighter bronze factors, lower roughness and 0.8 IBL intensity expose the sun/IBL response.
Distant ridges share restrained world-projected stone detail. Extra deterministic
ground cover and right-hand ring ivy share the existing wind, pause/replay and reflection
clock. The fixed activated Standard shot contains 60,662 vertices and 1,764 source foliage
quads. Linux full validation passes 97/97 (101.60 seconds) with core/sync validation enabled,
including all three quality budgets and 79 PBR frames. Release/reference and physical target
acceptance remain open. VIS stays 5/7.


A bounded HDR crystal point source now adds real local PBR illumination to nearby stone,
bronze and transparent surfaces before bloom/ACES. Shared Slang uses smooth finite-radius
inverse-square falloff; source-world positions keep planar reflections coherent, and unlit
sky/emitters remain unchanged. Standard/High activation follows crystal lift and the shared
pause/replay clock; F9 compares the local light. Basic/inactive/default scenes omit it. The
optional copied ScenePointLight validates finite position, radiance [0,32] and radius [0.1,64].
The private packet grows to 400 bytes, fitting the existing DX12 768-byte aligned pair; C/Zig
ABI is unchanged. CPU bounds/packing and six native movement/replay/disable/unlit cases are
passing (85 PBR frames). Linux full validation passes 97/97 (105.31 seconds) with core/sync
validation enabled, including native F9 changes/exact restoration. Release/reference and
physical target acceptance remain open; VIS stays 5/7. This one source has no point-shadow map.


Crystal point-light Shipping evidence: [Apps/Showcase/evidence/VIS-Crystal-Light-Linux-2026-10-05](/Apps/Showcase/evidence/VIS-Crystal-Light-Linux-2026-10-05). Production freeze `aec18172a4e6`; actual 100.33-second movie (100.71-second wall time), isolated native F9 comparison/restoration and all 85 native PBR cases pass. Final reference/physical-target acceptance remains open.


Courtyard art now shares exact bevel profiles for repeated tower/arcade blocks, retaining
world-space mapping and inverse-transpose normals while reducing uploaded geometry. Distant
towers use masonry courses and raised diamond relief; three foreground banks add 288 wind
cards. Leaf shading uses the original alpha/color mask without emissive fill. The basin is
shifted forward for a readable silhouette; warmer ceramic glaze responds to the sunset.
The authored crystal has five staggered rings with an outward convex triangulation check.
Three internal emissive mineral fissures share crystal rotation/lift and appear through the
opaque HDR snapshot; rune/fissure radiance is restrained before bloom/ACES. These are authored
geometry, not volumetric scattering. Standard/High atmosphere uses strength 0.6 at 18–58 units.
This art iteration does not accept final reference parity or target performance; VIS stays 5/7.

Linux native integration: ✅ full configure/build and 97/97 tests pass (103.93 seconds) with Khronos core/synchronization validation, including 85 PBR frames and all three geometry budgets. The fixed activated Standard frame has 50,166 vertices and 2,052 source foliage quads. Shipping/Full isolated native acceptance and an actual 100.27-second movie (100.80-second wall time) pass; final reference/target acceptance remains open.

Evidence: [VIS-Courtyard-Masonry-Linux-2026-10-05](/Apps/Showcase/evidence/VIS-Courtyard-Masonry-Linux-2026-10-05). Production freeze `e5bb13119ba1`; exact source and package hashes are retained.


Optional `SceneMaterial::twoSidedLighting` makes lit PBR sheets face the viewer before tangent
normal mapping and BRDF/IBL evaluation. Leaves and pennants opt in; defaults preserve existing
surface lighting. Source-world shadow masks and mirrored virtual cameras remain coherent,
while closed-crystal front-facet filtering still precedes the flip. Unlit and Lambert use
reject this flag. Private material float 79 uses the reserved slot; the 400-byte packet,
backend bindings and stable C/Zig ABI are unchanged. Back faces no longer lose diffuse IBL
through a negative view cosine. This is sheet lighting, not a thick-material volume model.

✅ Linux Development configure/build and all 97 tests pass (106.20 seconds), including 89 native PBR frames with Khronos core/synchronization validation. Four sheet fixtures retain default rear-face behavior and reproduce the front-facing colors exactly when enabled; CPU rejects unlit/Lambert use and verifies slot 79. All native geometry budgets and wind/pause/replay interactions pass. Shipping/Full isolated native acceptance and an actual 100.33-second movie (100.79-second wall time) pass; final reference/target acceptance remains open.

Evidence: [VIS-Two-Sided-Linux-2026-10-05](/Apps/Showcase/evidence/VIS-Two-Sided-Linux-2026-10-05). Production freeze `248791c4b51a`; exact source and package hashes are retained.


The courtyard waterfall ribbons now share authored cliff sites, keeping their geometry in
front of the supporting rock and visible through the wide camera's arch openings. Thin
water-sheet transmission follows the existing M comparison; flowing ribbons retain the
shared pause/replay clock. The left cypress is placed beneath the sun in the open arch and
retains wind/alpha lighting. The distant ridge grid grows from 16×64 to 24×96; no new map,
shader packet or stable ABI is introduced. Final reference/target acceptance remains open.

✅ Linux Development configure/build and all 97 tests pass (103.28 seconds) with Khronos core/synchronization validation enabled, including 89 native PBR frames and all three geometry budgets. The activated Standard frame has 55,382 vertices and 2,052 source foliage quads; wind/flow and exact paused replay remain validated. Shipping/Full isolated native acceptance and an actual 100.20-second movie (100.85-second wall time) pass; final reference/target acceptance remains open.

Evidence: [VIS-Courtyard-Valley-Linux-2026-10-05](/Apps/Showcase/evidence/VIS-Courtyard-Valley-Linux-2026-10-05). Production freeze `c815263b5f87`; exact source and package hashes are retained.


MSVC fixture portability follow-up: point-light fill and two-sided normal conditionals use
floating literals. ✅ Linux configure/build and full 97/97 pass (102.21 seconds), including
89 native PBR frames with core/sync validation. Test values and runtime sources are unchanged;
retained Shipping evidence keeps its production freeze. Windows CI recheck is pending.


Crystal mineral core supporting status: 144 contained opaque mineral corners replace the
internal wire veins and share the shell rotation/lift clock. Three HDR material responses
are visible through the front-filtered refractive shell and in its planar reflection. Shared
IBL intensity is 1.1. ✅ Linux configure/build and all 97 tests pass (104.78 seconds), including
89 native PBR frames with core/sync validation and all three geometry budgets. Shipping/Full
packaging and isolated native interaction pass; the same Shipping executable records an
actual 100.27-second wind/animation tour (100.63-second wall time, zero overlays). Evidence
is retained in `VIS-Crystal-Facets-Linux-2026-10-05`. No volumetric/recursive-glass claim,
new native binding or ABI change is introduced. Reference parity and physical-target
performance remain open; VIS stays 5/7.


Upstream point-light, masonry and two-sided MSVC fixture evidence and merge ancestry are
synchronized; each stage retains its full Linux gate and exact logs/hashes in `msvc-literals/`.
This synchronization changes only documentation/evidence. The mineral-core runtime sources,
97/97 gate and Shipping/movie source freeze remain unchanged.


Foliage filtering supporting status: lit shared half-cutoff masks use alpha-weighted
linear-color mip chains and the closest available authored silhouette coverage per level.
Other cutoffs, unlit atlases and ambiguous/mixed uses keep one level. Original upload bytes
and immutable generation ownership remain unchanged; visible/shadow passes share the chain.
✅ Linux configure/build and full 97/97 pass (104.47 seconds), including 89 native PBR frames
with core/sync validation, coverage/color-fringe/role checks and all three geometry budgets.
Shipping/Full packaging and isolated native interaction pass; the same executable records
an actual 100.20-second wind/animation tour (100.86-second wall time, zero overlays).
Evidence is retained in `VIS-Foliage-Mipmaps-Linux-2026-10-05`. No public material field,
native binding, shader packet or C/Zig ABI change. Discrete tiny levels may have unavoidable
coverage error. Reference parity and physical-target performance remain open; VIS stays 5/7.

Spatial HDR anti-aliasing implementation: Standard/High courtyard applies a bounded shared
linear-radiance edge filter before focus/bloom/ACES, with F10 comparison and exact restoration.
Basic omits it. The public C++ draw flag defaults off; native tone constants are now 64 bytes.
Vulkan diagonal-edge/constant-interior/restoration fixtures pass with core/sync validation;
✅ Full Linux configure/build and 97/97 tests pass (106.93 seconds); Shipping/Full
packaging, isolated native F10 comparison/restoration and the actual 100-second animated
tour pass. Evidence: `Apps/Showcase/evidence/VIS-HDR-Anti-Aliasing-Linux-2026-10-05`. VIS-M3 reference parity and VIS-M6 hardware acceptance remain open.

HDR 空間抗鋸齒實作：Standard/High 庭院在 focus/bloom/ACES 前套用有界的線性
輻射亮度邊緣濾波；F10 可比較並精確還原，Basic 不啟用。公開 C++ draw flag 預設
關閉，原生 tone 常數改為 64 bytes。Vulkan 斜邊、純色內部與還原測試已通過
core/sync validation。✅ 完整 97/97 測試、Shipping/Full 隔離原生 F10 驗證與 100 秒動畫導覽通過；VIS-M3 預覽一致與 VIS-M6 硬體驗收保持未完成。

Courtyard lighting/glass iteration: stronger authored IBL reveals leaf shadow detail; a
closer, higher-aimed wide camera frames the device, and stone/core radiance is restrained.
The crystal opts into thin dielectric Fresnel transmission using the existing bounded
HDR snapshot. Its shared-clock rotation/hover, wind, bloom, skybox and local light remain
active. ✅ Linux configure/build and 97/97 tests pass (104.97 seconds), including 97 native
PBR frames with core/sync validation. Shipping/Full isolation, exact native comparison
restoration and an actual 100-second animated movie pass. Evidence: `Apps/Showcase/evidence/VIS-Dielectric-Glass-Linux-2026-10-05`; reference parity and physical-target
acceptance remain open (VIS 5/7).

庭院光照與玻璃修整：提高原有 IBL 強度以顯示葉片陰影細節；廣角鏡頭靠近並提高
視線，石材與水晶內部輻射亮度更克制。水晶使用現有有界 HDR 快照，選用薄介電
界面的 Fresnel 透光；共用時鐘的旋轉／浮動、風、bloom、skybox 與局部光源持續
運作。✅ Linux configure/build 與 97/97 測試通過（104.97 秒），包括 97 個原生
PBR frame 的 core/sync 驗證；Shipping/Full 隔離、原生比較精確還原及實際 100 秒
動畫影片通過。證據：`Apps/Showcase/evidence/VIS-Dielectric-Glass-Linux-2026-10-05`；預覽一致及實體硬體驗收保持未完成（VIS 5/7）。

Native release diagnostic follow-up: failed headless/native subprocesses now print at
most 8 KiB of stderr and the structured failure reason. Exit codes, timeouts and all
acceptance checks are unchanged. ✅ Full Linux configure/build and 97/97 pass (103.21
seconds); the frozen Shipping executable passes isolated native acceptance with the
updated verifier. A controlled subprocess failure remains FAIL/exit 1 and prints its
bounded reason. Follow-up evidence is in `VIS-Dielectric-Glass-Linux-2026-10-05/release-diagnostics/`;
the recorded runtime/movie production freeze remains unchanged.

原生 release 診斷補強：headless／native 子程序失敗時輸出最多 8 KiB stderr 與
結構化原因；退出碼、逾時及全部驗收條件保持相同。✅ Linux configure/build 與
97/97 測試通過（103.21 秒）；固定 Shipping 執行檔使用新版 verifier 的隔離
原生驗證通過。受控的子程序失敗仍回傳 FAIL／exit 1，且能看到有界原因。證據：
`VIS-Dielectric-Glass-Linux-2026-10-05/release-diagnostics/`；原有 runtime／影片 freeze 保持相同。

✅ Dielectric/native interaction integration: Linux configure/build and full 97/97 tests passed (111.31 s, Khronos core/sync validation), including 97 native PBR frames and nine evidence-policy tests. The original checksum-verified Shipping executable from 4933fbc passed isolated acceptance with the synchronized export, held camera input and presented-resize checks. Runtime sources, frozen executable and movie are unchanged. Evidence: `Apps/Showcase/evidence/VIS-Dielectric-Glass-Linux-2026-10-05/interaction-synchronization/`. VIS remains 5/7; preview parity and physical-display acceptance remain open.

✅ Coping/native interaction integration: Linux configure/build and full 97/97 tests passed (110.58 s, Khronos core/sync validation), including 97 native PBR frames and nine evidence-policy tests. The original checksum-verified Shipping executable from 1989904 passed isolated acceptance with the synchronized export, held camera input and presented-resize checks. Runtime sources, frozen executable and movie are unchanged. Evidence: `Apps/Showcase/evidence/VIS-Courtyard-Coping-Linux-2026-10-06/interaction-synchronization/`. VIS remains 5/7; preview parity and physical-display acceptance remain open.
