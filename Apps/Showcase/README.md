# Nexora Zig Showcase

Windows CI naming correction: rename the rivet-loop local to `rivetRadius` to resolve MSVC C4458 without changing numeric values or operations. Native movie/package evidence is frozen before this naming-only change; merge still requires full current-head cross-platform CI.

✅ Linux Development full 97/97 tests passed (111.61 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.69 s wall time). Production freeze `79540805777114ab26d77dadda2258db39d43046`; evidence: `Apps/Showcase/evidence/VIS-Device-Inlays-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Hero detail authoring adds eight original two-ring bronze rivet domes and seventeen raised rune outlines, retaining the 42-material palette and native vertex budgets. The annular bend has negative orientation, so its triangle winding is explicitly reversed. The opaque mineral core retains 144 corners, with bounded deterministic fracture offsets and actual face normals; equal source positions map equally and shell containment is tested. Clearer glass (tint 0.65/0.98/0.94, roughness 0.035), reduced core emission and warm ceramic body/paint factors retain shared-clock motion and comparison controls. This remains the existing single-interface refraction approximation. Linux Development configure/build and full 97/97 tests passed (111.61 s, Khronos core/sync validation); Shipping verification passed; reference parity remains open.

✅ Linux Development full 97/97 tests passed (112.34 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.84 s wall time). Production freeze `d5676eea058ecb999979120119f2ac36c6d5bad9`; evidence: `Apps/Showcase/evidence/VIS-Background-Depth-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Background authoring retains the ridge height function, extent, triangle count and vertex normals while welding its continuous grid from 9,216 to 2,425 vertices. Tower upper stories use real gaps between shared corner piers. Foreground stone material AO is 0.45 and background stone AO is 0.5; this uniform authoring control is not spatial contact occlusion. Neutral lighter background stone and haze (strength 0.35, distance 24–75) retain HDR contrast while improving distant readability. Accepted main Editor changes are integrated and preserved. Linux Development configure/build and full 97/97 tests passed (112.34 s, Khronos core/sync validation); Shipping verification passed; preview parity remains open.

✅ Linux Development full 97/97 tests passed (110.56 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.73 s wall time). Production freeze `5615a2d816db60f099792eb8abe2ba09585427c3`; evidence: `Apps/Showcase/evidence/VIS-Arcade-Stones-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Courtyard arcade authoring uses 20 original wedge prototypes across four arches, with real joints and bevels rather than tubular spans. Raised column relief covers front and courtyard-facing sides. The bend preserves triangle winding with transformed normals; existing materials, shader packets and animation ownership remain stable. Linux Development configure/build and full 97/97 tests passed (110.56 s, Khronos core/sync validation); Shipping verification passed; reference parity remains open.

✅ Linux Development full 97/97 tests passed (110.29 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.81 s wall time). Production freeze `040a3acf7c07351b98b13f2e0c5c89e4d1d3584b`; evidence: `Apps/Showcase/evidence/VIS-Crystal-Proportions-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

Courtyard crystal authoring uses the same cooked mesh at scale (0.6, 0.8, 0.6), centered at Y=3.15. The opaque interior uses scale (0.3, 0.624, 0.3); shell, core and splinters retain correctly transformed normals, shared rotation/hover and pause/replay. Focus and local light follow the new center. Stone uses the existing uniform occlusion scalar at 0.65; ambient foliage lighting stays available. The C comparison menu lists F10 anti-aliasing alongside the existing lighting, material, wind and HDR controls. Source assets, shader packets and stable ABIs remain unchanged. Linux Development configure/build and full 97/97 tests passed (110.29 s, Khronos core/sync validation); Shipping verification passed; final reference parity remains open.

✅ Linux Development full 97/97 tests passed (109.50 s, Khronos core/sync validation). Shipping/Full isolated native acceptance and an actual 100-second shared-clock tour passed (100.89 s wall time). Production freeze `19899040d0c70d3ba2053bd918cae03230bed88f`; evidence: `Apps/Showcase/evidence/VIS-Courtyard-Coping-Linux-2026-10-06/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

The three courtyard pedestal rims now use 72 original annular coping stones with physical joints, bevelled edges, bounded radial/height variation and correctly transformed normals. Recessed lathed cores support the courses. Directional shadows use a 24×20 world-unit projection to include both side-arcade crowns, retaining light orientation, 40-unit far plane, tier resolution and F6 comparison. This geometry change retains existing materials, texture IDs and animation ownership; final reference parity remains open.

✅ Linux configure/build and full 97/97 tests passed (108.23 s, Khronos core/sync validation), including nine evidence-policy tests and 600 actual native camera frames. Isolated original Shipping acceptance passed. Interaction synchronization now waits for complete Lab JSON/Markdown export, holds D through the fixed 600-frame camera run, and requires stable foreground pixels at the resized viewport scale before shutdown. The resize-generation, camera-movement, pixel-restoration, frame-count and timeout gates remain in force. Runtime/movie freeze remains c815263b; evidence: `Apps/Showcase/evidence/VIS-Courtyard-Valley-Linux-2026-10-05/interaction-synchronization/`. VIS remains 5/7; reference parity and physical-display acceptance remain open.

MSVC compiler follow-up: foreground sprigs use `sprigRadius` to avoid camera-member
shadowing under /WX. Exact source comparison after identifier normalization is unchanged.
✅ Linux configure/build and full 97/97 pass (102.71 seconds), including 85 native PBR
frames with core/sync validation. Evidence: `VIS-Courtyard-Masonry-Linux-2026-10-05/msvc-member-shadowing/`.
Existing Shipping/movie keep their recorded production freeze; Windows CI recheck is pending.


`NexoraShowcase` is the first local, engine-owned Zig Showcase verification
slice. The process entry point, `Engine`, `GameWorld`, fixed/update loop,
offscreen renderer, reload, and shutdown are owned by C++; the Zig object only
uses the V3 gameplay ABI callbacks. The Zig module moves the primary cube's
Transform through the public component wire contract, so the report proves a
real C++ world mutation rather than only an isolated counter. Its state is
created and destroyed through the paired host allocator callbacks, including
both sides of a transactional reload, rather than relying on Zig global state.

Development builds a dynamic Zig module; Shipping links it statically. Interactive startup now
opens the Hub by default (`--mode=interactive --scene=hub --backend=auto`). Linux/Vulkan and the
existing Windows/DX12 geometry path use `DrawScene` for indexed, lit, depth-tested room geometry,
then `RenderUi` for the original bitmap font and panels. UI shaders are embedded independently of
the graphical Editor feature. Metal scene geometry, sampled materials, instances and GPU copy are now implemented in source;
Hosted macOS native graph and pixel/input/depth/lifecycle gates pass; physical Mac visuals remain a separate acceptance task.

The eight room controls are `1`–`8`; mouse drag orbits, wheel zooms, and held WASD controls the camera
or gameplay character. F1/F2/F3 toggle overview/profiler/matrix; F5 reloads an owned scene snapshot
after ending Play and undo history. Scene room E/U/P performs Modify/Undo/isolated Play; Gameplay C/G
crouches/teleports; L changes locale; T/Space/R starts, pauses or replays a 210-second guided tour.
`--mode=tour --tour=v1` starts that tour directly. `--vsync=off` requests immediate presentation.

`RoomSession` owns a separate demonstration Editor World and public Runtime subsystems. Its scalar
state and returned report/mesh/UI snapshots are owned by the Showcase, serialized on the application
thread and destroyed before Engine shutdown. Borrowed geometry/UI spans last until the next
corresponding build call. Input focus loss clears held keys and pointer drag; pointer coordinates map
to the logical UI viewport. The source mesh uses `nexora.showcase.mesh.v1`, is imported/cooked/bundled,
loaded from the active generation, and contributes visible geometry to the Scene room.

`runtime_rooms.integration_probes` records live public-API checks separately from the original
`validation_lab` CTest mapping. PASS here certifies integration scope only. Native/clean-host visual
acceptance remains PARTIAL; unknown contract gates and unexecuted failure-injection metadata remain
NOT_RUN. `--probe=v1.M8 --markdown=probe.md --report=probe.json` reruns and exports one integration
result. M9 audio/video and M11 WebView explicitly show contract-only/unavailable adapter state.
Skeleton translations and a weighted procedural column visualize the public skin palette through CPU
deformation; live particles use owning Runtime position snapshots. J blends two clips. GPU skinning
and native media playback remain separate capabilities. Gameplay uses a capsule mesh, collision-aligned
AABB stair ramp and tubes for actual navigation/query results. Streaming meshes use terrain LOD and
public cell residency, with authored coarse HLOD proxies and vegetation geometry. `visual_complete` is not inferred from these views.

`showcase.linux_vulkan_virtual_display` verifies four genuine GPU frames and resize under Xvfb.
`showcase.linux_vulkan_interaction` uses xdotool to exercise all eight rooms, editing/undo/play,
reload, locale, character movement, pointer orbit, tour and resizing. It retains screenshots and
JSON/Markdown evidence when `NEXORA_SHOWCASE_EVIDENCE_DIR` is set. Both fail in CI if their native
prerequisites are missing; local missing-tool runs skip with code 77. `showcase.runtime_rooms`
checks pause/replay timing, world isolation, reloaded editor lifetime, input and geometry ownership.
The interaction gate allows up to 25 seconds for the first software Vulkan frame and sends
`WM_DELETE_WINDOW` for graceful shutdown under Xvfb without a window manager.


## Visual identity courtyard greybox

`--scene=courtyard` (or `9` from any room) opens the first VIS-M0 engineering blockout:
paving, broken architecture, a central stone-ring mechanism and ceramic/vegetation placeholders.
`B` cycles reproducible wide, material-close-up and motion-finale camera framings; mouse/WASD
orbit and wheel zoom remain available. Re-entering the courtyard resets the wide camera.
`F4` toggles a completely diagnostic-free screenshot view in any room; the RenderGraph UI stage
still completes, but submits no UI draw while hidden. Press `F4` again to restore the controls.
The eight-room V1 tour remains available through `T`.

The courtyard now adopts three CC0 KayKit meshes (decorated pillars, broken paving and rubble)
and its gradient atlas. Their pinned originals, license, hashes and inventory ship under
`Content/Showcase/Courtyard`; `PrepareCourtyardAssets.py --check` verifies deterministic bounded
conversion. Converted authoring payloads are imported, cooked, bundled and loaded from an owning
Runtime generation before their geometry/UVs/texels are submitted. The 64x64 area-filtered palette
atlas is a baseline texture, not detailed PBR maps.

The crystal placeholder uses the original mesh read from the active Import → Cook → Bundle →
Runtime generation when the asset pipeline is enabled; disabled builds use an explicit procedural
cube fallback and report `representative_asset_loaded=false`. Reports retain the fixed-shot index,
UI visibility and representative asset hash. Geometry/report snapshots retain existing ownership.

Geometry remains an engineering blockout. Six material slots now use shared PBR, generated tangents,
normal/ORM/emission bindings and hardware sRGB filtering. `P` switches to the Lambert comparison;
`O` toggles IBL. The CC0 Forest Slope HDRI, deterministic irradiance/GGX prefilter/BRDF LUT,
licenses and source/derived hashes ship under `Courtyard/Environment`. Cooked float resources depend
on cooked source/license/conversion metadata in the same verified Runtime bundle. Full builds load
that bundle before submitting their bytes; asset-disabled builds explicitly retain direct light.

Three reproducible camera framings and comparison restoration have native tests. Floating-point HDR
composition and directional shadows are implemented below; wind and finished art remain pending. See the [visual roadmap](../../Roadmap/en/V1-Visual-Identity-Roadmap.md) and
[blockout inventory](content/Courtyard-Greybox.md). Native baseline capture and VIS-M0 acceptance
are tracked separately from final target-hardware visual/performance acceptance.

## Feature gallery

`--scene=tour` emits the complete gallery capability matrix. Individual `math`, `scene`,
`gameplay`, `presentation`, and `streaming` room IDs can also be selected. Every room is reported as
`IMPLEMENTED`, `CONTRACT ONLY`, or `UNAVAILABLE`, with an explicit fallback description; selecting
a room never pretends that an absent backend exists. The portable slice implements the Math and
Scene rooms, conditionally implements the Gameplay physics query when simulation is enabled, and
reports Presentation and Streaming as implemented when their runtime feature modules are enabled.
Disabled modules retain explicit `CONTRACT ONLY` or `UNAVAILABLE` fallbacks.
Native interaction uses WASD to move the gallery camera; Zig performs the public raycast used for selection and the report records camera, selection, raycast, and recovery overlays. `--capabilities=minimal` forces the deterministic fallback matrix used by CTest without pretending disabled feature modules are present.

```bash
NexoraShowcase --headless --scene=tour --frames=4 --report=showcase-gallery.json
```

## Windows build

The repository pins Zig 0.14.0 for the gameplay object. If it is not already
on `PATH`, use the ignored local tool installed at
`.tools/zig-win-x86_64-0.14.0/zig.exe`:

```powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --target NexoraShowcase
```

The executable and its modular DLLs are written to:

```text
build/windows-showcase-development/bin/Development/NexoraShowcase.exe
```

Run the verification report:

```powershell
& .\build\windows-showcase-development\bin\Development\NexoraShowcase.exe `
  --headless --validate-v1 --frames=4 `
  --report=.\build\windows-showcase-development\showcase-v1.json
```

The process exits with code `0` only when the C++-owned lifecycle, Zig
fixed/update callbacks, public Transform read/write, scene extraction,
offscreen `Shadow -> Forward+ -> PostProcess -> Present` path, validation
diagnostics, and transactional state migration all pass. `ctest` runs the
same executable together with the ABI smoke test:

```powershell
ctest --preset windows-showcase-development -R `
  "gameplay\.zig_abi_smoke|showcase\.zig_headless" --output-on-failure
```

## Visual Studio Code

Open `Engine.code-workspace`, select the `windows-showcase-development` CMake preset,
and select `NexoraShowcase` as the launch target. The
`Nexora Zig Showcase (Windows)` `cppvsdbg` configuration retains the older Zig ABI
headless command from the Run and Debug panel. CMake Tools invokes MSVC; VS
Code is the editor and task/debugger frontend, not a separate compiler.

## ZS-M2 public scene boundary

The headless scene is authored by Zig through append-only `NexoraGameplayHostV3` callbacks. Zig
loads and activates the scene, resolves opaque asset handles, spawns the camera/light/physics-backed
mesh entities, exercises despawn, consumes an input snapshot, raycasts, submits high-level debug
lines, and reads frame diagnostics. C++ continues to own `GameWorld`, input/backend state, physics,
render extraction, and all returned handles; Zig receives no RHI, device, queue, swapchain, native
window, or movable ECS pointer.

All callbacks execute synchronously on the serialized game thread. Input and diagnostics are copied
snapshots, spawn/debug descriptors are borrowed only for the call, asset/entity/scene values are
opaque non-owning IDs, and debug requests are copied by the host. Invalid pointers, sizes, handles,
UUIDs, or lifecycle state return `NexoraGameplayResult` without partial publication. The Zig module
retains only opaque IDs and its host-allocated state between callbacks; those IDs become invalid
when their host-owned world is destroyed.

## Reproducible packages

The `NexoraShowcasePackageDevelopment` target creates a Development/Modular package containing the
executable, dynamic Zig gameplay module, required engine DLLs, and the MSVC CRT redistributable DLLs
on Windows, so a clean target does not need a separately installed Visual C++ runtime.
`NexoraShowcasePackageShipping` creates the static Shipping/Monolithic layout and is available only
from a Monolithic build. Both use the same deterministic
packaging command and emit `build.json`, the public API manifest, a content manifest with SHA-256
digests, `SHA256SUMS`, and the repository license beneath `build/<preset>/package`.

```bash
cmake --build --preset linux-development --target NexoraShowcasePackageDevelopment
cmake --build --preset linux-shipping --target NexoraShowcasePackageShipping
```

The generated `build.json` records the exact relocatable launch command, including the packaged Zig
library path. On Linux, the evidence target verifies every packaged checksum, copies the package to
a fresh temporary directory, launches only from that copy, and retains the embedded Showcase report
plus command and exit status in `launch-evidence-linux.json`:

```bash
cmake --build --preset linux-development --target NexoraShowcasePackageDevelopmentEvidence
```

On Windows, the same evidence launcher verifies the package checksums, stages a fresh isolated copy,
executes the recorded command, and retains `launch-evidence-windows.json`:

```powershell
cmake --build --preset windows-zig-showcase --config Development --target NexoraShowcasePackageDevelopmentEvidence
cmake --preset windows-zig-showcase-shipping
cmake --build --preset windows-zig-showcase-shipping --target NexoraShowcasePackageShippingEvidence
```

The Windows Shipping preset uses a separate Monolithic build and static Zig gameplay. On a separately
provisioned target machine, copy one complete package directory, verify `manifests/SHA256SUMS`, run
the exact command in `manifests/build.json` from the package root, and retain `launch-report.json`
alongside the command, exit status, and host details. CI and developer-machine isolated-copy evidence do
not establish this final clean-machine acceptance gate.

## Full visual distribution

`NEXORA_BUILD_SHOWCASE` controls the application independently of the Zig ABI sample; enabling it
requires Zig gameplay and Window Presentation. Default configuration preserves the existing
feature-off build; enable Zig to enable Showcase, or explicitly disable Showcase to build only Zig.

```bash
cmake --preset linux-showcase-shipping
cmake --build --preset linux-showcase-shipping --target NexoraShowcasePackageShippingEvidence
```

Windows uses `windows-showcase-shipping` for Shipping/Full with DX12 enabled. The older
`windows-zig-showcase-shipping` Minimal preset remains a headless strip gate. Packages now include
`Content/Showcase/catalog.json`, `run-showcase.ps1`, `run-showcase.sh`, interactive/headless launch
commands, deterministic ZIP and its SHA-256 sidecar. Linux Development includes all seven engine
shared libraries; the evidence launcher puts the isolated `bin` first and checks that every linked
Nexora library resolves inside it. Platform Vulkan/graphics drivers remain host dependencies.

`windows-showcase-vulkan-shipping` is the same Shipping/Full package with the Vulkan backend ON (it needs the Vulkan SDK's `vulkan-1` import library; set `VULKAN_SDK`). Run the packaged verifier with `-Backend vulkan` on a machine with a Vulkan GPU. The tag-triggered Release workflow attaches this package next to the DX12 one; hosted runners only prove it builds, links and packages.

The native Linux evidence is versioned at
[`Linux-Vulkan-Visual-Slice-2026-10-03`](evidence/Linux-Vulkan-Visual-Slice-2026-10-03/acceptance.md).
Windows rooms, clean-VM launch, GTX 960 physical-display evidence, procedural texture/
instancing/skin/particle content and Lab ABI rejection are recorded below. Metal and additional GPU
acceptance remain open.

## Live Validation Lab

F3 opens the matrix. Tab selects M0-M12, I cycles the error input (None, Empty asset,
Dependency cycle, Plugin ABI, Rollback), R runs, and PageUp/PageDown scroll copied metrics.
Every run records `input.milestone`, `input.error_case`, `sample_tick`, output and issues.
M5 handles empty import/cyclic dependencies/generation rollback; M6 uses an actual dynamic
library with a mismatched host ABI, verifying zero loaded handles and zero registrations;
M12 stages and rolls back a fresh update. Each injection uses isolated Showcase-owned objects.
Other case/milestone pairs remain UNSUPPORTED, and a provided missing plugin file is FAIL.
The optional example plugin is placed beside the app and included in Development/Full packages.
`--plugin-library=PATH` overrides it; disabled plugin builds do not fabricate ABI evidence.

X writes `showcase-lab.json` and `showcase-lab.md` in the current directory and reports write
failure on screen. JSON contains owning room/probe snapshots; Markdown includes all metrics
and issues. These are runtime integration results and retain `contract_gate=NOT_RUN`.
The Linux interaction gate operates these controls, checks real ABI rejection and captures
`validation-lab.png`, `lab-export.json` and `lab-export.md`.

Current lab/geometry and ground-contact acceptance: [Linux-V1-Lab-Geometry-2026-10-03](evidence/Linux-V1-Lab-Geometry-2026-10-03/acceptance.md).

Rendering now submits one shared indexed cube with four native instances (floor and three cubes),
using independent translation, axis scale and tint. Vulkan and DX12 use hardware instance input;
Vulkan instance pixels have target-host acceptance; DX12 also has Windows CI and local developer-GPU evidence below.
The profiler and `native_scene_instances` report cumulative accepted instance counts independently
of native scene draw counts. Hub and Rendering use an original 8x8 checker sampled by native Vulkan/DX12 material pipelines. The native owner RenderGraph executes Offscreen -> Main -> UI -> Present; private scene color is copied on the GPU into the acquired image. P in Rendering cycles instanced cubes, quad and triangle.

## Windows local visual acceptance

Build the Full package and run its stock-PowerShell verifier on a visible, unlocked Windows desktop:

```powershell
cmake --preset windows-showcase-shipping
cmake --build --preset windows-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4
& ./build/windows-showcase-shipping/package/NexoraShowcase-Shipping/accept-v1.ps1 `
  -EvidenceDirectory ./showcase-windows-v1-evidence
```

Use `-PhysicalDisplay` only on the physical display/GPU under test and `-CleanHost` only on an
independently provisioned target. These are recorded operator attestations, not inferred from an
isolated copy. Add `-ExpectedBuildId <12-character-build-id>` to pin the version. The script verifies
all package checksums in a temporary isolated copy, captures eight rooms, quad/triangle/Lab/resize,
checks native graph/texture/lifecycle counters and Engine module locations, and preserves JSON/Markdown
and logs. Keep the Showcase visible during capture. Review the images before versioning evidence.
No Python or SDK is needed to run this verifier on the target; the built package's Windows runtime
prerequisites still apply. CI attempts the same native script without physical/clean-host attestations;
missing interactive desktops produce an explicit UNSUPPORTED artifact (77), never native PASS.
Clean-host acceptance passed on a clean Windows 10 VirtualBox VM (virtual GPU) and a `-PhysicalDisplay` run
passed on the GTX 960 developer machine; a Windows Vulkan `-Backend vulkan -PhysicalDisplay` run also passed on the GTX 960 with a CI-built package; Metal parity remains open; the per-tag release workflow ran on real test tags `v0.0.0-rc.1`/`rc.2` and produced draft releases (never auto-published). Audio/video/WebView adapters retain their stated unavailable/contract scope.

The Windows verifier retains the launched process handle before exit for Windows PowerShell 5
exit-code reliability and preserves launch JSON/Markdown even when an acceptance check fails.
The verifier shares its launcher console, fits the outer window to the desktop and raises the
Showcase before each screen capture, so a newly opened console cannot obscure visual evidence.

✅ Windows Full/DX12 hosted-CI native acceptance and unobscured screenshots: [versioned evidence](evidence/Windows-V1-Native-Graph-CI-2026-10-03/acceptance.md). On an independently provisioned physical target, add `-PhysicalDisplay -CleanHost` to record those operator attestations.

### Development/Full and complete tour

The explicit `windows-showcase-development` preset enables DX12 and Full Runtime capabilities,
using VS 2022 x64. Debug and Development are available; build/test presets default to Development.
The older `windows-zig-showcase` preset remains available for ABI work.

~~~powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --parallel 4
ctest --preset windows-showcase-development
cmake --build --preset windows-showcase-development --target NexoraShowcasePackageDevelopmentEvidence --parallel 4
& ./build/windows-showcase-development/package/NexoraShowcase-Development/accept-v1.ps1 -EvidenceDirectory ./build/windows-showcase-development/artifacts/v1-local
~~~

For Shipping from a regular PowerShell session with Visual Studio installed:

~~~powershell
cmake --preset windows-showcase-shipping -G "Visual Studio 17 2022" -A x64 -DCMAKE_CONFIGURATION_TYPES=Shipping
cmake --build --preset windows-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4
& ./build/windows-showcase-shipping/package/NexoraShowcase-Shipping/accept-v1.ps1 -EvidenceDirectory ./build/windows-showcase-shipping/artifacts/v1-local-full-tour -CompleteGuidedTour
~~~

The verifier also captures pointer orbit/zoom, F1/F2, Modify/Undo/Play/reloaded Play, locale,
character movement/crouch/teleport, clip blend, lifecycle/pressure and tour pause/replay.
`-CompleteGuidedTour` waits for all seven real-time steps and checks the exported 210-second,
final-step, paused Runtime state. Fresh scene-reload, pre-replay, replayed-tour and final-tour
JSON/Markdown snapshots are retained. F5 must preserve the edited transform and clear Undo;
R must reset elapsed time and tour step. Each export clears previous files, and the final Lab
checks require the sampled M5 empty-asset and M6 ABI-rejection inputs and outputs.
`interaction_checks` records the scripted controls and captures; these do not replace pixel tests,
image review or CTest. Windows PowerShell 5 now resolves the default package root in the script
body, so invoking the packaged script without `-PackageRoot` works.

Local developer-machine evidence: [Windows-V1-DX12-Local-2026-10-03](evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md).
Clean Windows 10 VM evidence: [Windows-V1-CleanVM-VirtualBox-2026-10-03](evidence/Windows-V1-CleanVM-VirtualBox-2026-10-03/acceptance.md) (virtual GPU).
Physical-display evidence: [Windows-V1-PhysicalDisplay-GTX960-2026-10-03](evidence/Windows-V1-PhysicalDisplay-GTX960-2026-10-03/acceptance.md).
**Mac remains incomplete and is deferred at the user's request.** Full real-input/screenshots, the guided tour and physical/clean-host Mac acceptance remain open. V1 final acceptance is open.

## POSIX release completion work

Linux and macOS now have Shipping/Full release jobs, with distinct archive names, checksum sidecars,
commit/run provenance and separate headless/native evidence. Pull-request Build jobs exercise the
same package gates before a tag. The release still creates a draft. Linux executes the existing
Xvfb interaction/screenshot gate from a verified isolated package; macOS performs an eight-room
Metal graph/report smoke when its device and display are available, otherwise preserving an explicit
UNSUPPORTED artifact. The Mac smoke does not claim screenshots or operator physical-display review.

```bash
cmake --preset macos-showcase-shipping
cmake --build --preset macos-showcase-shipping --target NexoraShowcasePackageShippingEvidence
python3 Tools/Package/VerifyShowcaseRelease.py \
  --package build/macos-showcase-shipping/package/NexoraShowcase-Shipping \
  --evidence-directory build/macos-showcase-shipping/artifacts/release-native
```

`macos-development` also provides `window_presentation.metal_scene`, retaining a GPU pixel PPM.
Development packages include all seven Engine dylibs. The packager rewrites Engine install names to
`@loader_path` within packaged binaries, applies ad-hoc signatures after relocation, then computes
checksums. Isolated launch rejects Engine dependencies resolving outside the package, including
plugin/gameplay dylibs. This is a runnable developer distribution, without Developer ID notarization.
Use `sh run-showcase.sh` on the target. Drivers and desktop services remain host dependencies.

`windows-showcase-vulkan-development` complements the Vulkan Shipping preset for SDK-equipped
Windows developers; the DX12-only Showcase presets remain available without the Vulkan SDK.
✅ macOS hosted Shipping/Full eight-room native graph/resize smoke (96 frames) and native
pixel/input/depth/lifecycle CTest (Development 73/73; mimalloc 63/63) pass; see
[the hosted Metal record](evidence/V1-Metal-Hosted-CI-2026-10-04/acceptance.md). Full real-input/screenshots,
clean-host deployment remain separate Mac acceptance tasks. ✅ The expanded desktop tag workflow
passes `v0.0.0-rc.3`, retaining four ZIPs, native archives and three CTest logs in an unpublished
16-attachment draft; [release record](evidence/V1-Desktop-Tag-RC3-2026-10-04/acceptance.md).

## Windows Vulkan hosted software-driver gate

The Build and Release workflows now provision Mesa lavapipe 26.2.4 from a pinned, SHA-256-verified
`mesa-dist-win` archive. The ICD/DLL remain host dependencies outside the isolated Showcase package;
a temporary ICD registry entry is created only on disposable GitHub-hosted Windows runners and
removed in an always-run cleanup step. The setup script rejects local and self-hosted execution;
no driver bytes are redistributed with the package. Native Win32 Vulkan acceptance
exercises eight rooms, interaction/resize, screenshots and the full 210-second guided tour.
`-ExpectedVulkanDriverLibrary` requires the process to load the exact selected DLL and records its
SHA-256, alongside archive provenance. The gate requires `software_rasterizer=true`, with physical
display and clean-host attestations false. A missing desktop or failed native run fails this gate.
✅ [Hosted execution acceptance](evidence/V1-Windows-Vulkan-Lavapipe-CI-2026-10-04/acceptance.md)
passes 14 checksums, 25 screenshots, nine interaction checks and a 210.006-second tour. The rc.4
tag Build/Release pass and the unpublished draft has 17 assets. Additional physical-GPU coverage remains open. Release bundles retain Vulkan evidence separately in `windows-vulkan-acceptance.tar.gz`.

## Native performance baseline

`--clean-view` starts with every diagnostic UI draw hidden; `F4` can restore it. Native reports
include `render_settings` and `performance`: actual viewport/present mode, 60 warm-up frames, a
bounded window of the latest 18,000 samples, average FPS and nearest-rank P95/P99 frame time.
Frame durations include acquisition/presentation and pacing. Separate process-wide user+kernel
CPU deltas include all threads (including software GPU work); peak resident/working-set bytes are
host observations. GPU timestamp data and refresh-rate metadata remain explicitly unavailable.
Headless/short runs report INSUFFICIENT_SAMPLES rather than inventing native FPS.

Run `Tests/Showcase/LinuxCourtyardBenchmark.py EXECUTABLE --output EVIDENCE_DIR` for three
360-frame fixed-wide-camera Vulkan runs at 1280x720 with immediate presentation and hidden UI.
Keep other tests/builds idle during measurement. The native backend/software flag and BuildInfo
are retained with each run; Xvfb/lavapipe results do not establish the proposed GTX 960 budget.

The courtyard's shared PBR path now renders into RGBA16F and composites through a GPU Main pass
before UI. `E` toggles exposure 1/0.25 to inspect retained highlight detail and restores the fixed shot
exactly; `O` compares IBL/direct light and `P` selects Lambert. Reports identify the actual scene color
format and exposure. HDR precision is independent of monitor HDR10; bloom and final art remain later
VIS-M3 work. Native tests cover emission above one, exposure, UI invariance, frame reuse and resize.

## Courtyard directional lighting

The PBR courtyard uses a frame-owned 1024² directional shadow map, four-tap shared PCF and
shared stylized light/shadow tint. F6 toggles shadow comparison, G toggles stylized/neutral light,
and brackets halve/double bounded normal bias; slope bias follows at twice normal bias.
P still selects the legacy Lambert comparison. Reports retain actual shadow/style flags and bias;
window evidence counts recorded native shadow passes and instances. These controls and fixed-camera
restoration have native tests. Final art, physical route visuals and target-hardware performance
remain separate acceptance tasks.

## Hero art and restrained post processing

The courtyard replaces the crystal cube with an original faceted crystal from the verified
Runtime generation, shapes a broken carved stone ring with aged bronze fittings and diamond
runes, adds round stepped plinths, original hollow ceramic vessels and a distant sky mesh.
Original 64² sandstone/bronze base/normal/ORM authoring maps ship with deterministic converter,
source/license hashes and metadata dependencies under `Courtyard/Hero`. These maps are original
repository art; they are not attributed to a downloaded PBR texture service. The three adopted
CC0 architectural meshes, palette and HDRI remain independently identified.

K toggles bounded GPU HDR bloom; G also enables/disables shared saturation/contrast grading
along with stylized lighting. Reports retain bloom, hero loading and derived hashes. Reframed
wide/close/finale cameras preserve the crystal silhouette. Final target-hardware visual review
and performance acceptance remain open; this is a native implementation, not final physical art
approval. Animated cutout foliage/activation and the visual tour are subsequent slices.

The living courtyard replaces static vegetation tubes with 64 original cutout leaf quads.
Shared GPU wind and matching cutout shadows animate them continuously. `N` compares wind and
`M` thin-leaf back lighting; `Enter` activates a pulsing crystal/runes and 48 bounded cutout
emissive motes. `Space` pauses the courtyard clock and `R` resets time/camera for exact replay.
With the validation matrix visible, R retains its probe-rerun meaning. F4 only hides UI; it does
not implicitly pause animation. Reports retain effect flags, animation time, particle and foliage
counts; these are real submitted geometry counts, not GPU timings or blended transparency proof.

Native interactive startup defaults to the courtyard; `--scene=hub` opens the engineering rooms.
`--tour=visual` selects the 100-second courtyard camera route and stops after its finale. It uses
actual update time on native surfaces; headless tests use deterministic fixed updates and provide
no visual acceptance. `--tour=v1` preserves the existing 210-second engineering tour. T starts
the visual tour in the courtyard, Space pauses camera/effects together, and R restarts it.
C enters free exploration: WASD translates the eye, dragging looks, arrows move vertically;
B or R returns to a fixed shot. H opens named effect comparisons. Default courtyard UI is a short
control strip; F1–F3 explicitly open engineering panels and F4 hides every overlay.
`Tools/Package/RecordVisualTour.py` records the actual Xvfb Vulkan executable with existing FFmpeg,
extracts matching video frames and retains application/video hashes, report, timing and software
GPU scope. FFmpeg is evidence tooling, not an engine/package dependency.

### Visual showcase quality and release evidence

`--quality=basic|standard|high` and courtyard Q select actual shared scene workloads. Basic
uses a 512 shadow map, 32 foliage quads and 24 active motes, disabling IBL and bloom. Standard
uses 1024 / 64 / 48, IBL and 0.15 bloom at radius 12. High uses 2048 / 128 / 96, IBL and
0.18 bloom at radius 20. HDR/PBR, cutout and color transfer remain correct at every tier.
Changing quality rebuilds cached foliage geometry between borrowed draw calls, retaining immutable
Runtime asset generations. Reports identify the effective tier after interaction, not just startup.
The original sky is one Runtime-loaded sRGB emission texture and one non-casting unlit batch;
active motes also skip the lighting/shadow path. Shadow counters now count actual submitted
batch instances.

`--pause-animation --activate-device` enables repeatable fixed-time active-device benchmarking.
`LinuxCourtyardBenchmark.py EXE --quality=standard --output=DIR` retains three independent
360-frame runs, discarding 60 warm-up frames and measuring 300. Run tiers sequentially with
builds/tests idle. CPU wall/frame and resident-memory observations are not GPU timestamps or
physical GTX 960 acceptance. The final package, native video, source/build hashes and reports
are retained together before VIS-M6 acceptance; Windows DX12/Vulkan target-host visuals and
the confirmed hardware budget remain required.

Packaged interactive launchers also open the compact courtyard; use `--scene=hub` explicitly for the engineering portal.

## Golden-hour environment and reference matching

The courtyard now includes original bevelled/fluted stone architecture, upper ruins and stairs,
a continuous mountain ridge, cypresses, hanging vines, two rippling reflective puddles and two
animated distant falls. A camera-centred six-face skybox replaces the gradient sphere. Its
384x256 atlas contains original cloud/sky authoring; the separate solar disc emits linear
radiance (12,8,3), aligned with the low-angle key light. The same directional sky drives the
floating RGBA16F IBL bake (90% original sky / 10% bounded pinned CC0 environment), so lighting
and reflections agree with the visible sky. Original authoring, converters and generated assets
retain reproducible hashes and their separate licenses. Detail maps are now 256x256; masks
remain 64x64. Larger byte payloads are checked against the active asset generation before upload.

`J` compares depth-aware focus; `K` compares bloom. Both are off in Basic quality and enabled
in Standard/High. HDR ACES/transfer remain enabled in every tier. Focus follows the device from
the current camera position, with bounded pixel radii; it never blurs the UI. Crystal rotation,
hovering splinters, cutout vine wind, motes, water ripples and fall ribbons share the replayable
courtyard time. Hanging UV root weights fix the upper attachment. Pausing freezes the actual
geometry/shader time; replay reconstructs from the immutable static cache.

The approved concept in `Roadmap/art/V1-Visual-Identity-Concept.png` is the visual target. These
are actual engine assets and native renders, but reference art parity is still under review:
procedural stone/crystal/environment detail is not yet identical to the concept. Reflective
puddles use PBR direct/IBL reflections, not planar scene reflection/refraction; falls and thin
crystal back lighting are bounded approximations. Do not mark full visual acceptance based on
feature switches, software Vulkan, or an attractive generated concept image.

Native courtyard animation consumes wall-clock delta up to the RoomSession one-second bound,
so software rendering below ten FPS does not stretch the 100-second tour. Engineering rooms
retain their prior 0.1-second cap; the engine/gameplay fixed-update loop is unchanged. The
recording tool still requires an actual native 90–120-second capture and never retimes video.

## Reference material and silhouette iteration

Retained image-assisted sandstone albedo/height and ivy PNG authoring now feeds the deterministic
Hero cook; source briefs, license and SHA-256 values are retained beside the originals. The stone
and ivy maps use 256x256 payloads; the mote remains 64x64 and sky atlas 384x256. Integer area
filtering preserves transparent ivy edges without adding a Python or runtime image dependency.
Original bevelled paving extends beyond the foreground, curved ring wedges have chamfers and an
upper-left break, and the wide camera places the larger device to the right of center.
Subdivided teal pennants and gold motifs share the wind clock, with fixed top anchors; ceramic
vessels now include handles, rings and geometric paint. Cloth is bounded shader bending, not a
cloth simulation. Crystal refraction and scene-reflected puddles remain separate work, and these
changes do not establish reference-image parity or physical target performance.


The shared linear HDR distance atmosphere adds far-background depth in Standard/High. F7
compares haze using the same paused camera; Basic omits it. Sky/unlit emitters retain authored
radiance. Six analytical native PBR cases verify disabled/half/full haze, restoration, unlit
exclusion and near-field clarity (65 total). The private material packet grows to 352 bytes,
with unchanged borrowed scene/frame ownership and stable C/Zig contracts. The effect precedes
transparency, focus, bloom and ACES; it is distance haze, not volumetric scattering.


Four fluted columns share one immutable mesh and four native instances; the identity instance
serves world-authored geometry, and the mirror instance is appended after column instances.
The saved vertex budget supports additional wind-driven ground cover and climbing ivy using
the retained original leaf mask. Cached geometry retains its instance layout across frames and
quality changes. Ceramic colors and the widened pedestal base refine the reference palette.

Crystal diffuse/backlight fill is reduced to retain transmitted background and sharper facets;
stronger linear rune radiance feeds HDR bloom. Original column relief and distant tower fluting
add geometric detail within the existing native vertex budget.


Stone base/normal/ORM maps now share source-world projections. Bounded normal-map gradients
perturb the geometric normal without UV streaking across columns, paving or reflected stone.
Stone strengths use 0.35/0.2 for hero/background surfaces. Existing linear HDR lighting, shadows,
focus and bloom consume the perturbed normals; four native fixtures verify projection strength
and exact restoration (69 PBR frames).


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


Crystal point-light Shipping evidence: [Apps/Showcase/evidence/VIS-Crystal-Light-Linux-2026-10-05](evidence/VIS-Crystal-Light-Linux-2026-10-05). Production freeze `aec18172a4e6`; actual 100.33-second movie (100.71-second wall time), isolated native F9 comparison/restoration and all 85 native PBR cases pass. Final reference/physical-target acceptance remains open.


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

Evidence: [VIS-Courtyard-Masonry-Linux-2026-10-05](evidence/VIS-Courtyard-Masonry-Linux-2026-10-05). Production freeze `e5bb13119ba1`; exact source and package hashes are retained.


Optional `SceneMaterial::twoSidedLighting` makes lit PBR sheets face the viewer before tangent
normal mapping and BRDF/IBL evaluation. Leaves and pennants opt in; defaults preserve existing
surface lighting. Source-world shadow masks and mirrored virtual cameras remain coherent,
while closed-crystal front-facet filtering still precedes the flip. Unlit and Lambert use
reject this flag. Private material float 79 uses the reserved slot; the 400-byte packet,
backend bindings and stable C/Zig ABI are unchanged. Back faces no longer lose diffuse IBL
through a negative view cosine. This is sheet lighting, not a thick-material volume model.

✅ Linux Development configure/build and all 97 tests pass (106.20 seconds), including 89 native PBR frames with Khronos core/synchronization validation. Four sheet fixtures retain default rear-face behavior and reproduce the front-facing colors exactly when enabled; CPU rejects unlit/Lambert use and verifies slot 79. All native geometry budgets and wind/pause/replay interactions pass. Shipping/Full isolated native acceptance and an actual 100.33-second movie (100.79-second wall time) pass; final reference/target acceptance remains open.

Evidence: [VIS-Two-Sided-Linux-2026-10-05](evidence/VIS-Two-Sided-Linux-2026-10-05). Production freeze `248791c4b51a`; exact source and package hashes are retained.


The courtyard waterfall ribbons now share authored cliff sites, keeping their geometry in
front of the supporting rock and visible through the wide camera's arch openings. Thin
water-sheet transmission follows the existing M comparison; flowing ribbons retain the
shared pause/replay clock. The left cypress is placed beneath the sun in the open arch and
retains wind/alpha lighting. The distant ridge grid grows from 16×64 to 24×96; no new map,
shader packet or stable ABI is introduced. Final reference/target acceptance remains open.

✅ Linux Development configure/build and all 97 tests pass (103.28 seconds) with Khronos core/synchronization validation enabled, including 89 native PBR frames and all three geometry budgets. The activated Standard frame has 55,382 vertices and 2,052 source foliage quads; wind/flow and exact paused replay remain validated. Shipping/Full isolated native acceptance and an actual 100.20-second movie (100.85-second wall time) pass; final reference/target acceptance remains open.

Evidence: [VIS-Courtyard-Valley-Linux-2026-10-05](evidence/VIS-Courtyard-Valley-Linux-2026-10-05). Production freeze `c815263b5f87`; exact source and package hashes are retained.


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

Courtyard lighting/glass iteration: stronger authored IBL reveals leaf shadow detail; a
closer, higher-aimed wide camera frames the device, and stone/core radiance is restrained.
The crystal opts into thin dielectric Fresnel transmission using the existing bounded
HDR snapshot. Its shared-clock rotation/hover, wind, bloom, skybox and local light remain
active. ✅ Linux configure/build and 97/97 tests pass (104.97 seconds), including 97 native
PBR frames with core/sync validation. Shipping/Full isolation, exact native comparison
restoration and an actual 100-second animated movie pass. Evidence: `Apps/Showcase/evidence/VIS-Dielectric-Glass-Linux-2026-10-05`; reference parity and physical-target
acceptance remain open (VIS 5/7).

Native release diagnostic follow-up: failed headless/native subprocesses now print at
most 8 KiB of stderr and the structured failure reason. Exit codes, timeouts and all
acceptance checks are unchanged. ✅ Full Linux configure/build and 97/97 pass (103.21
seconds); the frozen Shipping executable passes isolated native acceptance with the
updated verifier. A controlled subprocess failure remains FAIL/exit 1 and prints its
bounded reason. Follow-up evidence is in `VIS-Dielectric-Glass-Linux-2026-10-05/release-diagnostics/`;
the recorded runtime/movie production freeze remains unchanged.

✅ Dielectric/native interaction integration: Linux configure/build and full 97/97 tests passed (111.31 s, Khronos core/sync validation), including 97 native PBR frames and nine evidence-policy tests. The original checksum-verified Shipping executable from 4933fbc passed isolated acceptance with the synchronized export, held camera input and presented-resize checks. Runtime sources, frozen executable and movie are unchanged. Evidence: `Apps/Showcase/evidence/VIS-Dielectric-Glass-Linux-2026-10-05/interaction-synchronization/`. VIS remains 5/7; preview parity and physical-display acceptance remain open.

✅ Coping/native interaction integration: Linux configure/build and full 97/97 tests passed (110.58 s, Khronos core/sync validation), including 97 native PBR frames and nine evidence-policy tests. The original checksum-verified Shipping executable from 1989904 passed isolated acceptance with the synchronized export, held camera input and presented-resize checks. Runtime sources, frozen executable and movie are unchanged. Evidence: `Apps/Showcase/evidence/VIS-Courtyard-Coping-Linux-2026-10-06/interaction-synchronization/`. VIS remains 5/7; preview parity and physical-display acceptance remain open.
