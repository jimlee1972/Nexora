# Sprite source geometry — Linux verification

Date: 2026-10-28. Base: `2481d6efd9a9b10c67eca906afb9124031e19716` (`main` at checkout).
Scope: S01's first Sprite source-metadata/extraction slice; not the complete Sprite expansion.

## Implemented

- A fixed 96-byte schema-1 `.nx2sprite` metadata codec, stable Sprite/Atlas/region references,
  original canvas, top-left trim, bottom-left pixel Pivot, PPU and bounded border storage.
- Explicit source-to-instance resolution, keeping World transform/Y-sort anchors while composing
  trim, noncentral Pivot, Flip and negative scale through the existing extraction path.
- Atomic importer registration, Atlas dependency, existing Cooker/DDC/bundle/generation reuse.
- PNG grid cell selection with stable sparse IDs, exact margin/spacing validation, deterministic
  output and prior-output retention after invalid input or compiler rejection.
- A source fixture, contract documentation and bilingual 2D roadmap records. No gameplay rules.

## Executed gates

Linux x86-64, GCC 13.3, CMake 4.4.4, Ninja 1.13.2; isolated Clang 18.1.3 for the existing reflection
tool test. No new runtime module or project third-party dependency was introduced. Linux X11
development headers/libraries and pinned task tools were provisioned outside the checkout;
machine paths/cache overrides are not committed. Development is Modular, Shipping is Monolithic.

| Command | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS |
| `cmake --build --preset linux-development --parallel 6` | PASS |
| `ctest --preset linux-development --parallel 4 --output-junit <report>` | 250 registered: 237 PASS, 13 SKIP, 0 FAIL |
| `cmake --preset linux-shipping` | PASS |
| `cmake --build --preset linux-shipping --parallel 2` | PASS |
| `clang-format --dry-run --Werror Engine/TwoD/src/SpriteAsset.cpp Tests/TwoD/SpriteAssetTests.cpp` | PASS |
| `python3 -m py_compile Tools/TwoD/PrepareAssets.py Tests/TwoD/AssetToolTests.py` | PASS |
| `git diff --check` | PASS |

The existing preset defaults keep Slang and Zig compilation disabled. Thirteen native Vulkan/X11
tests are SKIPPED, not accepted: Vulkan scene/PBR/upload/swapchain, X11 held-key/metrics, Vulkan
texture recording, graphics-resource reuse, descriptor isolation, recording GPU timing/resource
reuse and the two recording benchmarks. This cloud sandbox cannot bind the Unix sockets required
by Xvfb. A separately attempted display-enabled run retained one `invalid Xvfb display number`
failure; direct diagnosis reported socket-listener creation failure. That run is not native
acceptance. The final full run uses the supported headless tool environment and the tests'
existing missing-prerequisite skip behavior; no test, preset or native assertion was weakened.

Initial environment failures (missing Clang and nonexecutable generated ELF modes) were repaired;
the final full run above is from the repaired environment. Incomplete generated object files were
rebuilt rather than changing unrelated Runtime package code. These setup repairs are not engine
feature changes. The normal development and Shipping presets finish successfully afterward.

### Focused evidence

`twod.sprite_source` passes independent reference coordinates for two 64x64 source canvases with
different trims, a noncentral mirrored Pivot, negative World scale and projected screen vertices.
It verifies unchanged World/Y-sort anchors, region dimension/identity rejection and source identity
after repacking. It also covers fixed-wire roundtrip, all truncations, trailing bytes, wrong version,
nonfinite/overflow/budget inputs, atomic importer failure and real cooked generation publication.

`twod.asset_tool` invokes the actual Python preparer and C++ compiler. It verifies 4x4/12-frame
selection, reordered/sparse indices, nonzero margin/spacing, selected-ID overflow, `.nx2sprite`
wire values/cooking and failed-output preservation, alongside the unchanged historical color/PNG
and Clip checks. Existing runtime/bridge/authoring/asset/Play and API baseline tests also pass.

The Monolithic Shipping compiler additionally prepared the existing Atlas fixture (344 bytes),
the new Sprite source fixture (96 bytes) and its cooked payload (185 bytes) without Editor libraries.
Commands used `Tools/TwoD/PrepareAssets.py` with `Fixtures/atlas.json` and `Fixtures/sprite.json`,
then `NexoraTwoDAssetCompiler cook actor.nx2sprite actor.nxcook` in a fresh task output directory.

## Acceptance boundary

PR CI found a test-only canonical-path prerequisite on macOS (`/var` aliases `/private/var`).
The direct padding-fixture helper call now resolves its existing root before invocation, matching
`compile_manifest` preparation. Production source-path containment checks remain unchanged.

No Windows, macOS, Android or iOS execution, physical minimum-PC benchmark, fresh GPU pixel
capture, advanced Sprite material/mask or automatic Editor/Player source-reference binding was
accepted here. Existing Atlas/Clip/scene/C/Zig wires, straight-alpha color and module dependencies
are unchanged. Public C++ consumers rebuild for the appended optional local quad. Border metadata
does not implement Sliced rendering. The full Sprite expansion and existing M6/M7 stay open.

Automatic approval review rejected both Beads synchronization and feature-branch publication to
the public GitHub destination, citing absent trusted authorization for that disclosure. The tested
code/commit and task database changes are retained locally. No PR, hosted CI, remote synchronization
or merge is claimed; publication requires resolving that approval block.
