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
the graphical Editor feature. Metal scene geometry remains unsupported and is not claimed as verified.

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
Skeleton translations and particle occupancy visualize public counters; they are not full GPU
skinning or native media playback. `visual_complete` is not inferred from these views.

`showcase.linux_vulkan_virtual_display` verifies four genuine GPU frames and resize under Xvfb.
`showcase.linux_vulkan_interaction` uses xdotool to exercise all eight rooms, editing/undo/play,
reload, locale, character movement, pointer orbit, tour and resizing. It retains screenshots and
JSON/Markdown evidence when `NEXORA_SHOWCASE_EVIDENCE_DIR` is set. Both fail in CI if their native
prerequisites are missing; local missing-tool runs skip with code 77. `showcase.runtime_rooms`
checks pause/replay timing, world isolation, reloaded editor lifetime, input and geometry ownership.


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
cmake --preset windows-zig-showcase
cmake --build --preset windows-zig-showcase --config Development --target NexoraShowcase
```

The executable and its modular DLLs are written to:

```text
build/windows-zig-showcase/bin/Development/NexoraShowcase.exe
```

Run the verification report:

```powershell
& .\build\windows-zig-showcase\bin\Development\NexoraShowcase.exe `
  --headless --validate-v1 --frames=4 `
  --report=.\build\windows-zig-showcase\showcase-v1.json
```

The process exits with code `0` only when the C++-owned lifecycle, Zig
fixed/update callbacks, public Transform read/write, scene extraction,
offscreen `Shadow -> Forward+ -> PostProcess -> Present` path, validation
diagnostics, and transactional state migration all pass. `ctest` runs the
same executable together with the ABI smoke test:

```powershell
ctest --preset windows-zig-showcase -C Development -R `
  "gameplay\.zig_abi_smoke|showcase\.zig_headless" --output-on-failure
```

## Visual Studio Code

Open `Engine.code-workspace`, select the `windows-zig-showcase` CMake preset,
and select `NexoraShowcase` as the launch target. The
`Nexora Zig Showcase (Windows)` `cppvsdbg` configuration runs the same
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

Linux Development packaging discovers and includes all transitive `libNexora*.so` dependencies.
Build-tree RPATHs are relative and include `$ORIGIN`, so bundled Engine libraries load from `bin/`.
The evidence launcher verifies the staged ELF dependency closure and rejects any Engine library
resolved outside that copy; system libraries and GPU drivers remain host requirements. A copied
executable alone previously passed by resolving Engine DSOs from the original build tree, so that
older checksum-only evidence did not establish Engine-library isolation.

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

The native Linux evidence is versioned at
[`Linux-Vulkan-Visual-Slice-2026-10-03`](evidence/Linux-Vulkan-Visual-Slice-2026-10-03/acceptance.md).
Expanded Windows rooms, clean-machine graphical launch, physical-display evidence, full texture/
instancing/skin/particle content and plugin ABI injection remain open acceptance items.
