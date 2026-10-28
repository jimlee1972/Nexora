# Sprite source scene references — Linux verification

Date: 2026-10-28. Base: `2e98820c` (`main`, Sprite source foundation PR #726).
Beads: `nexora-2q5`. Scope: C++ scene persistence and source-aware CPU extraction.

## Delivered behavior

- Optional stable `Sprite::source` UUIDs survive save/reopen and isolated Play. NX2S schema 3
  appends a checked presence marker/UUID per Sprite; source-free scenes retain canonical schema 2.
  Staging parse, membership validation and authoring-only Restore preserve existing live state.
- Source-aware scene/general extraction freezes input values before callbacks, resolves shared
  sources/Atlases once per call, and prepares trim/Pivot/flip geometry without changing World/Y-sort
  anchors or authored styles. Missing dependencies are unavailable; invalid closure rejects.
- Repacking changes the next frame's UVs without editing source identity or previous frames.
  Retained frames own their Atlas generation. No persistent source cache or new asset manager.

## Executed gates

Linux x86-64, GCC 14.2, CMake 4.4.4, Ninja 1.13.2 and Clang/Clang-format 19.1.7/23.1.3.
Task-local CMake/Ninja/Clang, X11 headers and Vulkan loader were provisioned outside the checkout.
CMake cache points to those prerequisites and the installed shared X11 library; no preset/build
system/dependency change is committed. Shipping reuses the same fetched Vulkan headers.

| Command | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS |
| `cmake --build --preset linux-development --parallel 6` | PASS |
| `ctest --preset linux-development -R twod.sprite_source --output-on-failure` | 1/1 PASS |
| `ctest --preset linux-development --parallel 4 --output-junit <task-path>/ctest.xml` | 250 registered: 237 PASS, 13 SKIP, 0 FAIL; 26.22 seconds |
| `cmake --preset linux-shipping` | PASS |
| `cmake --build --preset linux-shipping --parallel 4` | PASS, Monolithic |
| `clang-format --dry-run --Werror Engine/TwoD/include/Nexora/TwoD/Scene.h Engine/TwoD/src/Scene.cpp Engine/TwoD/src/SpriteAsset.cpp Tests/TwoD/SpriteAssetTests.cpp` | PASS |
| `git diff --check` | PASS |

`twod.sprite_source` retains the initial source wire/geometry/cook tests and adds source-only and
mixed legacy/source scene roundtrips; every truncated scene; unknown version/presence marker,
zero source UUID and trailing bytes; invalid Set/Restore conservation; isolated Play; independent
screen-coordinate/tint/flip checks; source/Atlas absence and identity/dimension mismatch; shared
source/Atlas resolution; native texture alias rejection; invalid IDs/matrices/ambiguous geometry
rejection before callbacks; callback mutation isolation; hidden-source behavior; and repack/frame
ownership/retirement. Other runtime, bridge, tools, authoring, Player, API and Editor gates passed.

The 13 existing prerequisite skips are Vulkan scene/PBR/upload/swapchain, X11 held-key/display
metrics, Vulkan texture recording/resource reuse/descriptor isolation/GPU timing/recording reuse,
and two recording benchmarks. Slang/Zig remain disabled under the preset defaults. No fresh
native pixels, Windows/macOS/mobile execution, hardware performance or device acceptance is claimed.
Initial setup attempts exposed missing CMake/Clang/X11/Vulkan prerequisites and a non-PIC X11 static
library selection; the final runs above use the corrected local tool setup and shared X11 library.
No test assertion, skip policy or performance budget changed.

## Acceptance limits

Public C++ consumers rebuild for the appended Sprite field. Old readers reject source-bearing
schema-3 scenes; source-free files keep schema 2. Atlas/Clip wires, C/Zig tables and native draw
records remain unchanged. Main Editor source indexing/Inspector/Play capture, Player catalog
admission, source animation/pool publication and the broader Sprite expansion remain separate.
Reviewed the roadmap set for affected delivered functionality; only the bilingual 2D supporting
record changes. Editor M3-08 and physical M6/M7 remain open; 9/11 (82%), P0 6/8 are unchanged.
The root README's milestone summary requires no update for this supporting slice.
