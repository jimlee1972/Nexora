# Nexora Zig Showcase

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
V1 final acceptance is open (Metal parity).

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
