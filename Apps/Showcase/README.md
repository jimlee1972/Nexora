# Nexora Zig Showcase

`NexoraShowcase` is the first local, engine-owned Zig Showcase verification
slice. The process entry point, `Engine`, `GameWorld`, fixed/update loop,
offscreen renderer, reload, and shutdown are owned by C++; the Zig object only
uses the V3 gameplay ABI callbacks. The Zig module moves the primary cube's
Transform through the public component wire contract, so the report proves a
real C++ world mutation rather than only an isolated counter. Its state is
created and destroyed through the paired host allocator callbacks, including
both sides of a transactional reload, rather than relying on Zig global state.

Development builds a `NexoraZigGameplay` shared library and `NexoraShowcase` selects it by default; `--gameplay-module=static` retains the statically linked ABI path used by Shipping, while `dynamic` makes discovery failure explicit. The deterministic headless slice continues to use the validation RHI. On Linux, `--mode=interactive --backend=vulkan` owns an X11 window and Vulkan swapchain; on Windows, `--backend=dx12` uses Win32/DX12. Both paths display a clear-color background, software-rasterized triangle, and diagnostics panel through the existing presentation composition boundary. They pump normalized input and acquire/compose/present until the window closes. Supplying `--frames=N` bounds an interactive verification run. `--backend=auto` may fall back to the validation path on an unsupported host, but prints and records `fallback_reason`; explicit native requests fail rather than silently switching backends.

The report deliberately separates `headless_evidence` from `windowed_evidence`. Linux CTest also registers `showcase.linux_vulkan_virtual_display`: it starts an isolated Xvfb server, runs four Vulkan frames, requests a 960x540 resize, and verifies startup/composition/present/shutdown evidence. A developer machine without Xvfb skips with code 77, while CI treats a missing or non-starting Xvfb server as a failure so the Linux native evidence cannot silently disappear.
The embedded `validation_lab` uses the versioned `nexora.showcase.validation.v1` schema. Its M0-M12 registry maps cards to CTest contract names without executing those tests. JSON and Markdown share one five-state model; portable tests cover invalid-asset, dependency-cycle, plugin-ABI-mismatch, and rollback injections. M7-M10 room records expose headless evidence only and keep `visual_complete: false`.
The 3D Hub now builds a presentation-only view from those results: thirteen color-coded cards retain their CTest identity and carry stable room and world-object associations. The native software composition draws the same model without running correctness logic, and the report records the associations plus visible invalid-asset, dependency-cycle, plugin-ABI-mismatch, and rollback states. Card data is copied into the view, owned by the Showcase, and discarded during ordered process shutdown; drawing is synchronous on the presentation thread. `NOT_RUN` remains honest until an external authority supplies evidence, and unsupported native scope remains `UNSUPPORTED`.


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
executable, dynamic Zig gameplay module, and required engine DLLs on Windows.
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
