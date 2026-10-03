# Nexora V1 Visual Showcase Demo Long-Term Plan

> **Progress: Linux, Windows CI and local developer-GPU visual slices verified; complete V1 acceptance remains open.**
> The previous percentage had no reproducible weighting ledger and is superseded by the acceptance evidence below.

## 0. Current-state audit

- ✅ Linux/X11/Vulkan native indexed, lit, depth-tested 3D drawing and readable GPU UI run under Xvfb/lavapipe; resize, ordered shutdown, and real keyboard/mouse interaction execute without skips.
- ✅ Eight navigable rooms, F1/F2/F3 overlays, F5 snapshot reload, mouse orbit/zoom, input routing, locale switching/fallback, and a pausable/replayable 210-second guided tour are implemented.
- ✅ Live public-API integration demonstrations cover scene snapshots, Editor/Play isolation, Modify/Undo, prefab override/rebase, procedural mesh import/cook/bundle/load and rollback, character/collision/navigation/AI, animation/skin occupancy, particle capacity, audio/video contracts, cell/HLOD budgets, and lifecycle/shipping simulations.
- ✅ Integration probe scope is explicit: `runtime_rooms.integration_probes` does not certify CTest or clean-host visual acceptance. The original CTest mapping remains a separate authority; synthetic error-injection metadata stays `NOT_RUN` until observed.
- ✅ Development/Modular packaging includes the seven required Linux engine libraries. Isolated-copy launch evidence rejects a dependency resolved outside the package; Full/Monolithic and Minimal builds remain separate.
- ✅ Full-profile package presets, original content catalog, interactive launch scripts, deterministic ZIP and SHA-256 output, and versioned Linux screenshots are available.
- ✅ Windows DX12 local developer machine (GTX 960): the stock PowerShell 5 verifier passed the current Development/Full package (22 checksums, 24 screenshots, 2528 native graph/copy/present frames) and Shipping/Full package (14 checksums, 25 screenshots, 15231 native graph/copy/present frames, complete 210-second tour). Windows Development CTest 68/68; Linux Development CTest 77/77. Evidence: [`Windows-V1-DX12-Local-2026-10-03`](../../Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md).
- ✅ Windows hosted-CI Full Shipping/DX12 isolated-copy graphical acceptance passes ([CI 37053279518](https://github.com/jimlee1972/Nexora/actions/runs/37053279518); evidence: [`Windows-V1-Native-Graph-CI-2026-10-03`](../../Apps/Showcase/evidence/Windows-V1-Native-Graph-CI-2026-10-03/acceptance.md)).
- ✅ Clean Windows 10 VM (VirtualBox, no dev tools) passed the Shipping/Full package verifier with `-CleanHost -CompleteGuidedTour`: `status=PASS`, 14 checksums, 25 screenshots, DX12 with no fallback, 3861 native graph/present frames ([record](../../Apps/Showcase/evidence/Windows-V1-CleanVM-VirtualBox-2026-10-03/acceptance.md)). The adapter is a VirtualBox virtual GPU, so this is not physical-display acceptance.
- ✅ Physical-display acceptance on the GTX 960 developer machine: the Shipping/Full package passed `accept-v1.ps1 -PhysicalDisplay -CompleteGuidedTour` (`status=PASS`, `physical_display_verified=true`, DX12 without fallback or software rasterizer, 15649 native graph/present frames, 25 screenshots; [record](../../Apps/Showcase/evidence/Windows-V1-PhysicalDisplay-GTX960-2026-10-03/acceptance.md)). The operator attestation flag was supplied by Claude on the user's instruction and the user did not separately watch the run; see the record.
- Windows Vulkan physical-display acceptance passed on the GTX 960 (CI-built package, build `ecac94d8f9ca`; it exposed and fixed a baseline-CPU crash and a present-time out-of-date abort). Open (final V1 acceptance remains PENDING): Metal native-backend parity (needs a Mac), Vulkan on other GPUs, Windows presets still default Vulkan OFF (the per-tag release workflow `.github/workflows/release.yml` now exists and its workflow_dispatch dry run passed on hosted CI; it has not yet run on a real `v*` tag). Audio/video/WebView adapters remain explicitly contract-only/unavailable.

Evidence and exact validation results: [`Linux-Vulkan-Visual-Slice-2026-10-03`](../../Apps/Showcase/evidence/Linux-Vulkan-Visual-Slice-2026-10-03/acceptance.md).

> Document version: v1.2
>
> Document status: Linux, Windows developer-machine, clean-VM and physical-display slices implemented and verified; Windows Vulkan GTX 960 verified; Metal parity and final V1 acceptance pending; per-tag release workflow added (dry run verified, first real tag run pending)
>
> Updated: 2026-10-03

## 1. Purpose

This document plans a long-term-maintainable NexoraShowcase, giving Nexora both:

1. A directly runnable Windows `NexoraShowcase.exe` that demonstrates 3D rendering, scenes, input, gameplay, assets, and Runtime capabilities.
2. A repeatable Demo Probe and headless smoke that verifies whether V1-M0 through V1-M12 actually compose and run together in one Runtime.

The Demo does not replace CTest. CTest owns deterministic, headless, error-path, and performance-baseline coverage; NexoraShowcase owns observable cross-module integration, visual results, and manual demonstration.

## 2. Current state and necessary boundaries

> This section is the original baseline audit made when the plan was created (historical). Current implementation status is in §0 and §14.

The repository already has V1 Runtime, RHI, Renderer, and several milestone contracts, but is not yet a complete windowed 3D engine:

| Current state | Evidence | Impact on the Demo |
| --- | --- | --- |
| NexoraHost can already run an Offscreen -> Main -> Present RenderGraph workload | `Apps/Host/main.cpp`, `Engine/Renderer/src/FramePipeline.cpp` | Reusable as a headless/render-contract baseline, but not a visible window |
| V1-M3's native-backend Present verification checks resource state | `Engine/RHI/README.md` | A window surface/swapchain contract must be added; offscreen Present cannot be claimed as visible output |
| `NEXORA_ENABLE_EDITOR_SDK` includes reflection, plugin, scene editor, and prefab foundations | `Engine/Runtime/src/EditorSdk.cpp`, `Engine/Runtime/README.md` | A Runtime-driven Inspector/Undo/Prefab demo can come first; a graphical Editor UI is follow-up work |
| M4-M12 are currently a platform-neutral executable baseline | `Engine/Runtime/README.md` | The Demo must show state, counters, and visualized results without misrepresenting a portable contract as a completed third-party SDK |
| Existing test targets are spread across Foundation, Core, Renderer, and Runtime | `Tests/*/CMakeLists.txt` | The Demo should share the Runtime API but must not treat a test executable itself as the demonstration program |

The goal for the first showcase version is therefore a "runnable V1 Showcase vertical slice," not a one-shot claim that every platform, SDK, and production editor is already done.

## 3. Target product definition

### 3.1 Product and executable

- CMake target: `NexoraShowcase`
- Windows executable: `NexoraShowcase.exe`
- Windows phase-one backend: Win32 window + DX12 surface/swapchain
- Runtime modes: Development and Shipping / Full
- Default startup screen: Showcase Hub
- Default resolution: 1280x720, resizable
- Default launch arguments:

~~~text
NexoraShowcase.exe --mode=interactive --scene=hub --backend=dx12
~~~

### 3.2 Capabilities the Demo must provide

- A genuinely visible 3D camera, mesh, material, depth, lighting, and RenderGraph frame.
- Keyboard, mouse, and extensible gamepad input.
- A Scene/Feature menu that enters each V1 showcase room.
- Every showcase room shows its feature name, current status, probe results, error messages, and key counters.
- F1 shows an overview UI; F2 shows the profiler/diagnostics; F3 shows the V1 matrix; F5 reloads the current scene.
- A `--headless --validate-v1` mode runs the portable probe suite without opening a window, for CI and local smoke use.
- Demo startup, scene switches, errors, and performance data are logged with an attached build ID.

### 3.3 What the Demo is not

- Not a complete game product, and not all of V1's content assets.
- Not a static-image simulation of 3D rendering.
- Not every CTest case turned into a manually-operated UI test.
- Not, in its first phase, a complete cross-platform graphical editor, WebView SDK, hardware video decoder, or store installer.

## 4. Usage modes

### 4.1 Interactive Showcase

For developers, contributors, and external demonstrations. Users can freely move the camera, switch rooms, trigger feature buttons, and observe counters and error state.

### 4.2 Guided Tour

A fixed 3-5 minute flow, in order:

1. Engine startup and BuildInfo
2. 3D scene and RenderGraph
3. Asset import/cook and scene reload
4. Character/physics/navigation/AI
5. Animation/audio/VFX/video
6. Large-world streaming/HLOD
7. Shipping profile, manifest, and diagnostic results

The Guided Tour must be pausable and replayable, and must leave a readable probe result at every step.

### 4.3 V1 Validation Lab

Lets an engineer pick a single milestone from the UI, re-run its probe, inspect its input and output, and export the result as a JSON/Markdown artifact. This is integration validation, not a replacement for that milestone's own CTest.

### 4.4 Headless CI Smoke

~~~text
NexoraShowcase.exe --headless --validate-v1 --report=artifacts/showcase-v1.json
~~~

Headless mode must not depend on a GPU window, mouse, or manual key presses; when a visible frame is actually needed, that is verified separately by a Windows native smoke or a manual demonstration flow.

## 5. Proposed architecture

### 5.1 Target dependency graph

~~~text
NexoraShowcase
    |
    +-- ShowcaseApp       window / input / UI / scene routing / CLI
    +-- ShowcaseProbes    V1 capability probes and report serialization
    +-- ShowcaseContent   procedural demo content and authored bundles
    |
    +-- NexoraRuntime
          +-- NexoraRenderer
                +-- NexoraRHI
                      +-- Win32 + DX12 WindowSurface (new)
~~~

The existing `NexoraRHI::Device` offscreen contract must be preserved. Windowing capability should add an independent WindowSurface/swapchain boundary rather than putting Win32 types into the public backend-neutral header.

### 5.2 Proposed layout

> Actual layout: `Apps/Showcase/` contains `CMakeLists.txt`, `main.cpp`, `ShowcaseProbes.cpp/.h`, `ShowcaseRooms.cpp/.h` and `evidence/` (versioned acceptance evidence). The tree below is the original proposal.

The following is a target structure, marked as proposed -- it does not represent what already exists at the time this document was written:

~~~text
Apps/Showcase/
  CMakeLists.txt
  main.cpp
  ShowcaseApp.cpp/.h
  ShowcaseCommandLine.cpp/.h
  ShowcaseSceneCatalog.cpp/.h
  ShowcaseUi.cpp/.h

Engine/Window/
  include/Nexora/Window/Surface.h
  src/Win32Surface.cpp
  src/Dx12Swapchain.cpp

Showcase/
  Probes/
  Scenes/
  Reporting/
  Content/

Tests/Showcase/
  ShowcaseStartupTests.cpp
  ShowcaseProbeTests.cpp

Content/Showcase/
  Scenes/
  Materials/
  Meshes/
  Textures/
  Audio/
  Video/
  Localization/
~~~

### 5.3 CMake options

Proposed additions:

~~~cmake
option(NEXORA_BUILD_SHOWCASE "Build the long-lived visual V1 showcase" ON)
set(NEXORA_SHOWCASE_BACKEND "Auto" CACHE STRING "Showcase window backend")
set_property(CACHE NEXORA_SHOWCASE_BACKEND PROPERTY STRINGS Auto Win32D3D12)
~~~

`NexoraShowcase` may only consume the Engine public API through `target_link_libraries`. Demo UI, content, and probes must not depend back on a test executable, nor bypass the Runtime to mutate its internal state directly.

## 6. Showcase scene design

The Demo should not build a mutually isolated test window per milestone; instead it should build one navigable Showcase Hub plus several reusable showcase rooms.

### 6.1 Showcase Hub

- A central 3D display stand, the Nexora logo, current build ID, backend, resolution, and frame time.
- Eight portals or UI cards: Core, Rendering, World, Gameplay, Presentation, Large World, Platform, Shipping.
- Each card shows PASS, PARTIAL, CONTRACT ONLY, or UNAVAILABLE; an unintegrated item must never be shown green as if complete.

### 6.2 Rendering Room

- Procedural triangle/quad/cube/instanced meshes.
- Camera orbit, depth, material, light, shadow placeholder, and a RenderGraph pass overlay.
- Shows the Offscreen -> Main -> Present pass graph, plus the window surface's acquire/present counters.
- When native backend selection fails, clearly shows the fallback and its reason, rather than silently falling back to a fake frame.

### 6.3 Scene / Asset / Editor Room

- M4's editor-world/play-world isolation, entity lifecycle, deferred commands, and scene snapshots.
- M5's asset source -> import -> cook -> bundle -> load flow, plus hash, generation, residency, and rollback.
- M6's Create/Modify/Undo, reflection property panel, plugin ABI gate, and prefab override/rebase.
- This room is a demonstration of the Runtime-driven editor foundation; it does not claim an independent graphical editor is complete.

### 6.4 Gameplay Room

- A controllable capsule/character, ground, ramps, obstacles, and a visualized collision query.
- Character motor, ground snap, step/crouch/teleport results.
- Navigation tiles, desired velocity, AI blackboard, behavior trace, and perception budget.
- Any real third-party physics/navigation adapter must show its adapter status on the card.

### 6.5 Presentation Room

- Visualization of skeleton/clip blending, root motion, and the skin palette.
- Particle emitter, particle count, bounded capacity, and dropped-spawn counter.
- Audio bus, voice limit, and residency panel.
- Video queue, timestamp, seek invalidation, and back-pressure panel; uses an explicit contract-only mode when no decoder SDK is present.

### 6.6 Large World Room

- Multiple world cells, portal prefetch, occupied-cell pins, HLOD proxies, and streaming budget.
- Shows cell identity, bundle identity, RAM/VRAM estimates, and load/unload reasons.
- Uses procedural terrain and vegetation to verify streaming orchestration first, so the first version is not blocked on large art assets.

### 6.7 Platform / Shipping Room

- App lifecycle, safe-area, memory/thermal pressure, and an input-focus simulator.
- When native WebView has no platform adapter, shows "adapter unavailable" and the ownership contract, rather than a fake WebView that looks usable.
- M12 profile preview: Minimal, Full, and Dedicated plugin/shader/asset/presentation strip results.
- Package manifest, update staging, rollback, crash breadcrumbs, and a build/device evidence summary.

## 7. V1-M0 through M12 verification matrix

Every row needs two results: the Contract Gate is automated evidence, and the Showcase View is the human-visible integration result. If either is missing, that item cannot be marked as a complete showcase.

| Milestone | Existing/expected contract gate | Showcase view | First-phase status | Owner |
| --- | --- | --- | --- | --- |
| M0 | CMake preset, module graph, build/CTest, Host startup | Build ID, module list, startup diagnostics | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Build |
| M1 | `core.runtime`, Foundation/Gameplay ABI | Frame time, job graph, allocator/log/VFS counters | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Core |
| M2 | Shader reflection, validation device, renderer contracts | Shader/pass/resource overlay | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Renderer |
| M3 | Native DX12/Vulkan/Metal offscreen path | Backend badge, native present counters, 3D frame | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Presentation |
| M4 | `runtime.v1_m4_vertical_slice`, scene snapshot/lifecycle | Operable scene, entity, undo, play/editor world | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Scene |
| M5 | `runtime.v1_m5_asset_pipeline` | Import/cook/bundle/progress/reload/rollback | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Assets |
| M6 | `runtime.v1_m6_editor_sdk`, plugin ABI/prefab | Reflection inspector, Undo, prefab rebase, plugin status | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Editor SDK |
| M7 | `runtime.v1_m7_input_ui_localization` | Key binding, UI widgets, locale switch, fallback | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Input/UI |
| M8 | `runtime.v1_m8_gameplay_simulation` | Character, collision, nav, AI trace | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Gameplay |
| M9 | `runtime.v1_m9_presentation` | Animation, particles, audio, video queue | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Presentation |
| M10 | `runtime.v1_m10_large_world` | Streaming map, HLOD, RAM/VRAM budget | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Large World |
| M11 | `runtime.v1_m11_platform` | Lifecycle/pressure/WebView ownership panel | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Platform |
| M12 | `runtime.v1_m12_shipping` | Package/profile/rollback/crash/device evidence | PARTIAL — Linux/Windows developer views verified; clean Windows 10 VM and GTX 960 physical-display verified; Windows Vulkan GTX 960 verified; Metal parity pending | Runtime Shipping |

### 7.1 Unified probe interface

Every probe should return a serializable result, with no direct UI dependency:

~~~cpp
struct ShowcaseProbeResult {
  std::string id;
  std::string milestone;
  ProbeStatus status;
  std::string summary;
  std::vector<ProbeMetric> metrics;
  std::vector<ProbeIssue> issues;
};
~~~

The UI, headless report, CTest adapter, and Guided Tour all consume the same result type. A probe may only use public contracts -- it cannot read a test's private data, nor use a screenshot to judge correctness.

## 8. Phased build order

### Phase A -- ✅ Linux Windowed App Shell

- ✅ Reuse `NexoraShowcase`, Window and RenderSurface; `NEXORA_BUILD_SHOWCASE` is an explicit feature gate requiring Zig gameplay and Window Presentation.
- ✅ Native startup, resize, shutdown, clear color, indexed triangles and readable diagnostics execute under Xvfb/lavapipe.
- ✅ `showcase.linux_vulkan_virtual_display` and `showcase.linux_vulkan_interaction` run without skips. CI provisions Xvfb/xdotool and fails on missing native evidence.
- ✅ Separate headless and windowed report objects remain intact. Close requests stop acquisition before another frame is opened.

This accepts the Linux virtual-display slice only; physical-display and Windows/macOS target-host acceptance are separate.

### Phase B -- First 3D Vertical Slice

- ✅ Portable vertex/index/uniform/depth/sample-texture and frame-resource contracts remain covered by existing Renderer tests.
- ✅ Linux Vulkan indexed geometry, directional lighting, depth and UI composition are native GPU work with per-frame fence ownership.
- ✅ Hub and Rendering room procedural geometry and an interactive camera are visible; diagnostics show actual native counters and sample timing.
- ✅ Linux native triangle/quad/instanced-cube content and native-owner RenderGraph scene binding are verified. The newest Windows/native and local target-host gates remain separate.

### Phase C -- Probe and V1 Validation Lab

- ✅ Stable M0-M12 registry and versioned JSON/Markdown exports, with separate CTest and runtime-integration authorities.
- ✅ F3 matrix, Tab selection, R rerun, `--probe=v1.MN`, and `--markdown=PATH` provide live integration results.
- ✅ Scene snapshot, Modify/Undo, isolated Play World, prefab override/rebase and a real cooked procedural mesh affect the displayed scene.
- ✅ Empty asset, dependency cycle, asset-generation rollback and shipping-update rollback use public Runtime APIs.
- ✅ Validation Lab shows sampled input/output metrics and issues, scrolls details, cycles error cases, and exports owning JSON/Markdown reports. Empty asset, dependency cycle, asset/update rollback and real dynamic-library ABI rejection execute through public Runtime APIs. Missing plugin files produce FAIL; unsupported case/milestone pairs remain UNSUPPORTED.
- Schema-fixture injection metadata remains separate from these observed runtime runs.

### Phase D -- Gameplay / Presentation / World Rooms

- ✅ Normalized held-key and pointer input, Runtime UI hit testing, locale/fallback demonstration, character motion/crouch/teleport, collision hits, navigation points, AI/perception counters.
- ✅ Animation translations, skin palette count, particle occupancy/drop count and audio/video queue/bus contracts update in the same application loop. Native audio/video playback is explicitly unavailable.
- ✅ Streaming cells, occupied pins, portal prefetch, HLOD residency, procedural terrain/vegetation and RAM/VRAM budgets have visible state.
- ✅ Seven tour steps run for 210 seconds with pause and replay, leaving readable results.
- ✅ Native indexed capsule geometry and collision-aligned AABB stair ramp, actual collision-ray/path tubes, a CPU-weighted public-palette skin column, two-clip blend and live particle position meshes execute in the application.
- ✅ Streamed Full terrain subdivision meshes, authored coarse HLOD proxies and vegetation geometry follow public cell residency. Memory counters are authored streaming estimates. Production GPU skinning, smooth-ramp physics adapters and high-resolution art remain beyond this procedural demonstration.

### Phase E -- Platform / Shipping / Distribution

- ✅ Lifecycle/pressure simulation and WebView ownership/unavailable status.
- ✅ Minimal/Full/Dedicated Packager previews, staged-update rollback and bounded crash breadcrumbs.
- ✅ Full profile presets, packaged content manifests, interactive scripts, deterministic ZIP/SHA-256 and isolated-copy headless package smoke.
- ✅ Linux screenshot and native interaction artifacts are versioned; CI retains screenshots, packages and CTest logs.
- Open: version-tag evidence (the Full Windows executable now also passes on a clean Windows 10 VM and on the GTX 960 physical display).

## 9. Build, run, and packaging specification

### 9.1 Development build

Once the target exists, the standard Windows flow should be:

~~~powershell
cmake --preset windows-development
cmake --build --preset windows-development --target NexoraShowcase
ctest --preset windows-development -R "showcase|runtime|renderer"
~~~

For the Visual Studio generator's multi-config build:

~~~powershell
cmake --build build\windows-development --config Development --target NexoraShowcase --parallel 4
~~~

Presets actually used on Windows: `windows-showcase-development` (Development/Full) and `windows-showcase-shipping` (Shipping/Full); the packaged verifier is `accept-v1.ps1` (`-CompleteGuidedTour` checks the full 210 seconds). Exact commands: [`Windows-V1-DX12-Local-2026-10-03`](../../Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md).

### 9.2 Launch arguments

~~~text
NexoraShowcase.exe --mode=interactive --scene=hub --backend=auto
NexoraShowcase.exe --mode=tour --scene=hub --tour=v1
NexoraShowcase.exe --headless --validate-v1 --report=showcase-v1.json
NexoraShowcase.exe --scene=rendering --backend=dx12 --vsync=off
~~~

Every argument must be written back into the startup log and the report, so a screenshot, bug report, or CI artifact can be traced back to the same build.

### 9.3 Release package

~~~text
dist/NexoraShowcase/<version>/
  NexoraShowcase.exe
  Nexora*.dll
  Plugins/
  Content/Showcase/
  manifest.json
  checksums.sha256
  README.txt
  run-showcase.ps1
~~~

The package must be produced or verified by the M12 Packager/manifest contract; manually copying DLLs is never an acceptable release flow. The showcase build should use Shipping + `NEXORA_SHIPPING_PROFILE=Full`; Minimal and Dedicated are for strip/headless verification, not a complete visual showcase package.

## 10. Automation and CI

Required gates:

- `showcase.configure`: the CMake option, module graph, and content manifest are all configurable.
- `showcase.build`: Windows Development can produce `NexoraShowcase.exe`.
- `showcase.headless_startup`: can initialize in windowless mode, load the probe registry, and exit cleanly.
- `showcase.v1_probe`: each probe's status, schema, and error path are stable.
- `showcase.package_launch`: launches from a clean artifact directory and produces a report.
- `showcase.windowed_smoke`: runs on a Windows runner with a GPU; must not block general CI when no GPU is present.
- All existing Foundation/Core/Renderer/Runtime CTest cases must keep passing.

### 10.1 Result classification

| Status | Meaning |
| --- | --- |
| PASS | Contract, integration probe, and required visuals are all complete |
| PARTIAL | Contract passes, but a platform/native adapter or the visual is still missing |
| CONTRACT_ONLY | Only the headless contract, with a clear reason it cannot yet be visualized |
| UNAVAILABLE | A local dependency or backend is missing, with a diagnosable error left behind |
| FAIL | An already-implemented capability's probe fails |

## 11. Long-term maintenance rules

1. Every new V1 capability must add or update its public contract, CTest, Demo Probe, showcase card, and documentation matrix together.
2. Showcase content should prefer procedural or freely redistributable assets; third-party assets must record license, source, hash, and a replacement plan.
3. The Demo must never expose a debug-only internal API as an Engine public API.
4. Scenes and bundles must have a version, schema, and migration policy; an old Demo package must be able to show why it is incompatible.
5. Every on-screen counter must state its data source and sample time, to avoid showing stale data.
6. Every release keeps the Windows artifact, `showcase-v1.json`, CTest log, BuildInfo, Git commit, and screenshots.
7. Performance budget and visual-quality budget are managed separately; 60 FPS is not correctness evidence, and a screenshot is not contract evidence.
8. If the Demo depends on an SDK that is not installed, it must gracefully degrade and show UNAVAILABLE, not crash outright at startup.

## 12. Definition of Done

### First demonstrable version

- `NexoraShowcase.exe` can open a window on a clean Windows environment.
- The Hub has a genuine 3D camera, mesh, material, and visible render output.
- At least the Rendering Room, Scene/Asset/Editor Room, and Gameplay Room are complete.
- The F1/F2/F3 overlays can show build, backend, frame time, and probe status.
- `--headless --validate-v1` can produce a parseable JSON report.
- Existing CTest does not regress, and `showcase` startup, probe, and package gates are added.

### Complete V1 Showcase version

- Every row of M0-M12 has an explicit Contract Gate, Showcase View, Status, and owner.
- M2/M3's offscreen and windowed rendering results are reported separately.
- M6 clearly distinguishes the Editor SDK from the graphical editor; if the graphical editor is not complete, the UI must not mislabel it as complete.
- M9/M11 can still run a contract-only demonstration when a third-party adapter is missing.
- The Shipping / Full package can launch on a clean Windows environment, with its content verified by the manifest.
- Every tag can produce an executable, package manifest, probe report, CTest report, and versioned screenshots.

### 12.1 Current status

| Item | Status | Basis |
| --- | --- | --- |
| Hub has a real 3D camera, mesh, material and visible output | ✅ | Linux Vulkan pixel acceptance; Windows DX12 local screenshots |
| Eight rooms (including Rendering, Scene/Asset/Editor, Gameplay) | ✅ | Room switching and screenshot acceptance |
| F1/F2/F3 overlays, F5 reload, 210-second tour | ✅ | Linux/Windows interaction acceptance; Shipping tour 210.002 s |
| `--headless --validate-v1` parseable JSON, Validation Lab export | ✅ | Showcase probe tests and exported JSON/Markdown |
| Existing CTest not regressed, showcase gates added | ✅ | Linux Development 77/77; Windows Development 68/68 |
| Every M0-M12 row has Contract Gate / Showcase View / Status / Owner | ✅ | §7 matrix (statuses are PARTIAL, not PASS) |
| M6 separates Editor SDK from graphical editor | ✅ | §6.3 scope statement |
| M9/M11 run contract-only when adapters are missing | ✅ | Audio/video/WebView marked contract-only/unavailable |
| Shipping/Full package launches on a clean Windows host | ✅ (VM) | Clean Windows 10 VirtualBox VM, `-CleanHost`, PASS ([record](../../Apps/Showcase/evidence/Windows-V1-CleanVM-VirtualBox-2026-10-03/acceptance.md)); virtual GPU, not physical |
| Physical-display acceptance | ✅ | GTX 960, `-PhysicalDisplay` PASS ([record](../../Apps/Showcase/evidence/Windows-V1-PhysicalDisplay-GTX960-2026-10-03/acceptance.md)); attestation flag supplied by Claude on the user's instruction |
| Every tag produces executable/manifest/report/screenshots | Pending | Versioned evidence directories exist; tag workflow not declared complete |

## 13. Suggested first build ticket

> ✅ This ticket's scope is complete and has been superseded by Phases A-E (windowed Linux and Windows execution, 3D, probes, rooms and packaging are implemented); kept below as historical planning.

The next implementation milestone should be named **V1-Showcase-M0 Windowed Demo Shell**, scoped to only:

1. Add `NEXORA_BUILD_SHOWCASE` and `Apps/Showcase/CMakeLists.txt`.
2. Add `NexoraShowcase.exe`, able to parse mode, scene, backend, and headless.
3. Add Windows Win32 window lifecycle and a DX12 swapchain/surface adapter.
4. Add clear color, a triangle, and a diagnostics overlay; do not introduce large content assets yet.
5. Keep NexoraHost, the offscreen renderer, and existing CTest unchanged.
6. Add startup, resize, shutdown, and headless smoke tests.
7. Update the Windows VS Code/CMake usage instructions and this document's implementation-status section.

Only once this ticket is complete should work move on to the 3D scene, probe registry, and each V1 showcase room -- so a genuinely runnable exe baseline is established first, and V1 capabilities are attached to it incrementally, rather than using not-yet-existing visual features to paper over an unfinished RHI/window boundary.

## 14. Implementation follow-ups and acceptance records

Per-slice acceptance records are kept below in chronological order; the latest status is in §0 and §12.1.

Validation Lab follow-up: `Tab` selects M0-M12; `I` cycles None/Empty asset/Cycle/Plugin ABI/Rollback; `R` runs; PageUp/PageDown scroll results; `X` exports `showcase-lab.json` and `showcase-lab.md`. The example plugin is optional and packaged beside the executable when enabled. `--plugin-library=PATH` selects a real local library for the M6 gate. All records include the sample tick and exact input case.

✅ Validation Lab/geometry follow-up and feet-origin ground-contact regression acceptance: [Linux-V1-Lab-Geometry-2026-10-03](../../Apps/Showcase/evidence/Linux-V1-Lab-Geometry-2026-10-03/acceptance.md), full Linux Development gate 75/75 with all five native gates executed under Khronos/synchronization validation.

Windows CI run 37043085582 built and tested Linux/macOS successfully, but Windows failed on MSVC `/WX` conversion and member-shadowing diagnostics in `ShowcaseRooms.cpp`. Explicit index/coordinate conversions and distinct geometry radius names address the reported diagnostics; Linux Development remains 75/75. The corrected Windows CI and clean-host acceptance remain pending.

✅ Native hardware instance submission now reuses one 24-vertex cube for the Rendering room floor and three differently positioned/scaled/tinted cubes. Vulkan pixel acceptance checks two instances in one draw; DX12 implements the same input contract and awaits target-host execution. Local target-host acceptance remains open.

✅ [Linux native instancing acceptance](../../Apps/Showcase/evidence/Linux-V1-Instancing-2026-10-03/acceptance.md).

Windows CI run 37044292214 passed Full Shipping packaging after the geometry diagnostic fixes. Development still failed on upstream Editor test variable shadowing, now addressed by distinct local names. Clean-host graphical acceptance remains pending.

✅ Native Hub/Rendering sampled RGBA8 checker material now uses UV, lighting/base color and instance tint. Linux pixels verify texture selection, immutable cache reuse and resize reupload; DX12 provides matching source bindings and awaits target-host execution. Expanded Windows/physical-display acceptance remains open.

✅ [Linux sampled-material acceptance](../../Apps/Showcase/evidence/Linux-V1-Textures-2026-10-03/acceptance.md).

✅ Linux native-owner RenderGraph now schedules Offscreen -> Main -> UI -> Present against actual fence-owned scene color, GPU copy and swapchain/UI operations. Requested logical transitions and actual completed callback order are reported separately from RHI offscreen contracts. Rendering P cycles instanced cubes, quad and triangle. The packaged accept-v1.ps1 verifier captures Windows isolated-copy native screenshots/JSON; physical-display and clean-host attestations are delegated to the user locally and remain unaccepted until recorded.

✅ Baseline sampled-material commit `36a178b6ec3d8406d5099952bdfc44a39840d5ce` passed all jobs in CI run [37047161660](https://github.com/jimlee1972/Nexora/actions/runs/37047161660), including Windows Development/Full Shipping, macOS/Linux and TSan/ASan contracts. This certifies the earlier baseline; the native-graph/local-verifier revision has separate CI and target-host evidence.

✅ [Linux native-owner RenderGraph acceptance](../../Apps/Showcase/evidence/Linux-V1-Native-Graph-2026-10-03/acceptance.md).

Windows native follow-up: CI run 37052188209 executed DX12 graph work and all room captures, with the application reporting PASS (357 acquired/offscreen/copied/presented frames). The stock-PowerShell wrapper failed while reading the process exit code. It now retains the process handle before exit, logs the observed exit code, and preserves launch reports on failure. Corrected wrapper CI and local clean-host/physical-display acceptance remain separate pending gates.

The Windows verifier fits and raises its native window before capture and avoids opening a covering console. Captured images must be reviewed as part of target-host acceptance.

✅ Expanded Windows Full Shipping/DX12 isolated-copy graphical acceptance passes in [CI run 37053279518](https://github.com/jimlee1972/Nexora/actions/runs/37053279518): 14 checksums, 12 visible screenshots, sampled asset/plugin rejection, 339 native graph/copy/present frames and exit code 0. [Versioned Windows CI evidence](../../Apps/Showcase/evidence/Windows-V1-Native-Graph-CI-2026-10-03/acceptance.md). The hosted VM does not accept physical-display or independently provisioned clean-host gates; these remain the user’s local work.

✅ All jobs in Build run 37053279518 pass for source revision `ecf2277f07bc51ef09112c652a0ccbc0511d0a99`, including Windows/Linux/macOS Development, native Full Windows acceptance, mimalloc and sanitizer contracts.

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

✅ Clean Windows 10 VM acceptance: the Shipping/Full package (SHA-256 `09b0fda2…13b4`, build `e4a140139189`) passed `accept-v1.ps1 -CleanHost -CompleteGuidedTour` in the independently provisioned VirtualBox guest `Nexora-ZS-M5-CleanMachine-Win10`: 14 checksums, 25 screenshots, DX12 without fallback, 3861 graph/present frames, no issues. `physical_display_verified` stays false because the adapter is a VirtualBox virtual GPU; V1 final acceptance remains pending. [Record](../../Apps/Showcase/evidence/Windows-V1-CleanVM-VirtualBox-2026-10-03/acceptance.md).

✅ Physical-display acceptance: on the GTX 960 developer machine the Shipping/Full package (build `e4a140139189`) passed `accept-v1.ps1 -PhysicalDisplay -CompleteGuidedTour`: `status=PASS`, `physical_display_verified=true`, DX12 without fallback or software rasterizer, 15649 graph/present frames, 25 screenshots, 210-second tour completed. The attestation flag was supplied by Claude on the user's instruction (a first attempt failed because Claude's own screenshots hid the window). Vulkan/Metal parity and the per-tag release workflow remain open, so V1 final acceptance is still pending. [Record](../../Apps/Showcase/evidence/Windows-V1-PhysicalDisplay-GTX960-2026-10-03/acceptance.md).

✅ Windows Vulkan physical-display acceptance: the CI-built Shipping/Full package with the Vulkan backend ON (build `ecac94d8f9ca`) passed `accept-v1.ps1 -Backend vulkan -PhysicalDisplay -CompleteGuidedTour` on the GTX 960: `status=PASS`, Vulkan with no fallback and no software rasterizer, 15794 graph/presents, 25 screenshots, 9/9 interaction checks, full 210-second tour. The attestation flag was supplied by Claude on the user's instruction. Two defects were found and fixed on the way (baseline CPU for Zig, PR #229; present-time out-of-date recovery, PR #231). Metal parity, other GPUs and the first real-tag release run remain open; V1 final acceptance is not complete. [Record](../../Apps/Showcase/evidence/Windows-V1-Vulkan-PhysicalDisplay-GTX960-2026-10-04/acceptance.md)
