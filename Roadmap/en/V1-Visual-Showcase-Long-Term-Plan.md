# Nexora V1 Visual Showcase Demo Long-Term Plan

> **Progress: 10%** (as of 2026-09-23; weighted across the Phase A-E acceptance items;
> completed items use ✅; a headless contract does not complete a windowed phase.)

## 0. Current-state audit

- ✅ C++-owned `NexoraShowcase` entry point, CLI, and ordered Engine/module shutdown exist.
- ✅ A deterministic headless scene, validation RHI, scene extraction, and JSON evidence report exist.
- ✅ The Zig static consumer provides fixed/update, Transform read/write, and transactional state migration.
- Open: native window/input/swapchain are owned by the Window and Native Presentation Roadmap.
- Open: real 3D Hub and Rendering/Scene/Gameplay/Presentation/Large World/Platform/Shipping rooms.
- Open: M0-M12 probe registry, interactive/guided tour, error injection, and visual status UI.
- Open: clean package launch, manifest/checksum, windowed smoke, and versioned screenshot evidence.

> Document version: v1.0
>
> Document status: planning baseline (Draft)
>
> Updated: 2026-09-22

## 1. Purpose

This document plans a long-term-maintainable NexoraShowcase, giving Nexora both:

1. A directly runnable Windows `NexoraShowcase.exe` that demonstrates 3D rendering, scenes, input, gameplay, assets, and Runtime capabilities.
2. A repeatable Demo Probe and headless smoke that verifies whether V1-M0 through V1-M12 actually compose and run together in one Runtime.

The Demo does not replace CTest. CTest owns deterministic, headless, error-path, and performance-baseline coverage; NexoraShowcase owns observable cross-module integration, visual results, and manual demonstration.

## 2. Current state and necessary boundaries

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

| Milestone | Existing/expected contract gate | Showcase view | First-phase status |
| --- | --- | --- | --- |
| M0 | CMake preset, module graph, build/CTest, Host startup | Build ID, module list, startup diagnostics | Contract exists; visual entry point pending |
| M1 | `core.runtime`, Foundation/Gameplay ABI | Frame time, job graph, allocator/log/VFS counters | Contract exists; shown via diagnostics |
| M2 | Shader reflection, validation device, renderer contracts | Shader/pass/resource overlay | Offscreen verifiable; window path pending |
| M3 | Native DX12/Vulkan/Metal offscreen path | Backend badge, native present counters, 3D frame | Native offscreen exists; swapchain pending |
| M4 | `runtime.v1_m4_vertical_slice`, scene snapshot/lifecycle | Operable scene, entity, undo, play/editor world | Runtime foundation exists; content and UI pending |
| M5 | `runtime.v1_m5_asset_pipeline` | Import/cook/bundle/progress/reload/rollback | Contract exists; showcase assets pending |
| M6 | `runtime.v1_m6_editor_sdk`, plugin ABI/prefab | Reflection inspector, Undo, prefab rebase, plugin status | Editor SDK exists; graphical editor not in current state |
| M7 | `runtime.v1_m7_input_ui_localization` | Key binding, UI widgets, locale switch, fallback | Contract exists; window input/UI pending |
| M8 | `runtime.v1_m8_gameplay_simulation` | Character, collision, nav, AI trace | Contract exists; 3D gameplay scene pending |
| M9 | `runtime.v1_m9_presentation` | Animation, particles, audio, video queue | Portable foundation; native media adapters pending |
| M10 | `runtime.v1_m10_large_world` | Streaming map, HLOD, RAM/VRAM budget | Contract exists; visual world pending |
| M11 | `runtime.v1_m11_platform` | Lifecycle/pressure/WebView ownership panel | Portable contract; platform adapter pending |
| M12 | `runtime.v1_m12_shipping` | Package/profile/rollback/crash/device evidence | Contract exists; distributable exe pending |

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

### Phase A -- Windowed App Shell

Goal: produce a first `NexoraShowcase.exe` that can open a window, close it, resize, and show a clear color plus a diagnostics overlay.

- Create the `Apps/Showcase` target and command-line parsing.
- Create the backend-neutral WindowSurface contract.
- Implement Win32 window and DX12 swapchain on Windows.
- Keep the existing offscreen device/test path unchanged.
- Add `showcase.startup`, `showcase.resize`, `showcase.shutdown` smoke tests.

### Phase B -- First 3D Vertical Slice

Goal: the Hub scene shows a genuinely interactive 3D frame.

- Add the minimum vertex/index/uniform/depth/texture RHI contract needed.
- Establish camera, mesh, material, light, and frame-resource ownership.
- Expand `FramePipeline` from a fixed triangle to the minimum path that can submit a scene frame.
- Build procedural mesh/material content so the first version does not depend on large external assets.
- Implement the Rendering Room and frame diagnostics.

### Phase C -- Probe and V1 Validation Lab

Goal: the Demo can trigger each M0-M12 probe individually and emit a JSON/Markdown report.

- Build the ShowcaseProbe registry, status model, and report schema.
- Map existing CTest contracts onto showcase cards, without duplicating their correctness implementation.
- Implement the M4/M5/M6 scene/asset/editor foundation demos.
- Add error injection: invalid assets, dependency cycles, plugin ABI mismatch, rollback.

### Phase D -- Gameplay / Presentation / World Rooms

Goal: connect the M7-M10 Runtime contracts into the same 3D scene loop.

- Input/UI/localization overlay.
- Character/physics/navigation/AI visualization.
- Capability-aware animation/particle/audio/video demos.
- Streaming cell/HLOD/procedural terrain/vegetation demo.

### Phase E -- Platform / Shipping / Distribution

Goal: double-clicking a clean package's exe demonstrates the product, and proves the package's content and profile are correct.

- M11 lifecycle/pressure/WebView adapter status.
- M12 Full showcase package, manifest, update/rollback, crash breadcrumbs.
- `NexoraShowcase.exe --headless --validate-v1` package smoke.
- Windows artifact ZIP and SHA-256 manifest.

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

## 13. Suggested first build ticket

The next implementation milestone should be named **V1-Showcase-M0 Windowed Demo Shell**, scoped to only:

1. Add `NEXORA_BUILD_SHOWCASE` and `Apps/Showcase/CMakeLists.txt`.
2. Add `NexoraShowcase.exe`, able to parse mode, scene, backend, and headless.
3. Add Windows Win32 window lifecycle and a DX12 swapchain/surface adapter.
4. Add clear color, a triangle, and a diagnostics overlay; do not introduce large content assets yet.
5. Keep NexoraHost, the offscreen renderer, and existing CTest unchanged.
6. Add startup, resize, shutdown, and headless smoke tests.
7. Update the Windows VS Code/CMake usage instructions and this document's implementation-status section.

Only once this ticket is complete should work move on to the 3D scene, probe registry, and each V1 showcase room -- so a genuinely runnable exe baseline is established first, and V1 capabilities are attached to it incrementally, rather than using not-yet-existing visual features to paper over an unfinished RHI/window boundary.
