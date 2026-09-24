# Nexora Zig Showcase

`NexoraShowcase` is the first local, engine-owned Zig Showcase verification
slice. The process entry point, `Engine`, `GameWorld`, fixed/update loop,
offscreen renderer, reload, and shutdown are owned by C++; the Zig object only
uses the V3 gameplay ABI callbacks. The Zig module moves the primary cube's
Transform through the public component wire contract, so the report proves a
real C++ world mutation rather than only an isolated counter. Its state is
created and destroyed through the paired host allocator callbacks, including
both sides of a transactional reload, rather than relying on Zig global state.

Development builds a `NexoraZigGameplay` shared library and `NexoraShowcase` selects it by default; `--gameplay-module=static` retains the statically linked ABI path used by Shipping, while `dynamic` makes discovery failure explicit. The deterministic headless slice continues to use the validation RHI. On Windows,
`--mode=interactive --backend=dx12` additionally owns a visible Win32 window, pumps normalized
input, and acquires/clears/presents through the DX12 swapchain until the window closes. Supplying
`--frames=N` bounds an interactive verification run. `--backend=auto` may fall back to the
validation path on an unsupported host, but prints and records `fallback_reason`; explicit `dx12`
requests fail rather than silently switching backends.

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

The `NexoraShowcasePackageDevelopment` target creates a Development package containing the
executable and dynamic Zig gameplay module. `NexoraShowcasePackageShipping` creates the static
Shipping layout and must be invoked from a Shipping/monolithic build. Both use the same deterministic
packaging command and emit `build.json`, the public API manifest, a content manifest with SHA-256
digests, `SHA256SUMS`, and the repository license beneath `build/<preset>/package`.

```bash
cmake --build --preset linux-development --target NexoraShowcasePackageDevelopment
cmake --build --preset linux-shipping --target NexoraShowcasePackageShipping
```

The generated `build.json` records the exact clean-machine launch command. Package creation is not
host acceptance: execute that command after copying the directory to a clean target machine and
retain its report as distribution evidence.
