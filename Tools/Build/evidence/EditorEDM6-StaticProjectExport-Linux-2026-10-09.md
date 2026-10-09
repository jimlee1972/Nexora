# Editor ED-M6 owning StaticView producer — Linux cloud acceptance

Date: 2026-10-09. Scope: `nexora-62u.1.6`, bounded pure Editor producer, on accepted main
`9b599c5d` including Play scene services. This reconstructs the documented producer against the
current owning capture/codecs; the historical Oct-8 local commit is absent from this cloud checkout.
Historical test results are not substituted for the gates executed here.

## Result and exercised paths

`CookStaticProject` consumes owning captured World text/NodeKeys/opaque records and explicitly
provided owning imported OBJ/scalar PBR values. It generates tangents with the real Renderer,
encodes the shared Runtime types, cooks real NXAB assets and returns an exact validated package
through the production package/BundleBuilder path. Missing assets, full UUID/resource collisions,
invalid scalar reflection, unsupported reserved bindings, nonzero legacy shaders without scalar
override and exceeded logical budgets reject without modifying input or publishing bytes.

Tests create real Editor authoring/parenting/material references, capture, then destroy the original
World/document/import DTOs before cooking. Runtime package decoding proves complete tracked and
untracked entities, mirror hierarchy matrices, owning immutable geometry/materials, derived tangents,
full `UINT64_MAX` shader identity, binary unknown payloads and legacy non-UTF-8 names. Reordered
metadata and extra unused supplied assets produce identical exact dependency closure bytes.

Boundary tests exercise 65,535 mesh vertices, 1,048,575 triangle-aligned indices, geometry overflow,
4096 opaque records, exact 16 MiB aggregate names/payload budget and overflows, plus a functional
100,000-entity captured World. The source scene-local ID deliberately differs from the new loader's
ID. Raw snapshot bytes remain unchanged. These are functional/logical limits, not a peak-RSS or
production frame-time acceptance claim.

Normal and optimized Python tests run the real Editor producer fixture to a Unicode/space path,
then invoke the standalone Runtime-only ProjectPlayer and inspect explicit assertions-independent
checks. Verification preserves package bytes. Independent capture/import runs reproduce the same
bytes. Runtime reports three assets, three entities, two resolved renderers (scalar and neutral),
one inactive unknown component and no native rendering/gameplay execution.

## Executed validation

Linux cloud: GCC 14.2, CMake 3.31.6, Ninja, Slang 2026.18, Zig, Xvfb and Mesa software Vulkan.
The full graphical gate runs serially with other native gates and heavy builds.

```bash
cmake --preset linux-development \
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON \
  -DNEXORA_BUILD_PROJECT_PLAYER=ON
cmake --build --preset linux-development -j 4
ctest --preset linux-development
```

Passed **201/201, zero skips, 399.49 seconds**. The new producer contract passed in 8.00 seconds;
the normal/optimized production CLI checks passed in 15.76/16.67 seconds. Existing native Scene,
Game/materials/input/scene services, Console, recovery, profiler, file save and Showcase gates remain
present and pass. Earlier focused validation before the additional exact-limit cases passed
3/3 in 32.37 seconds; the final full suite above contains the final implementation and limit cases.

Initial hosted Windows Development builds rejected the fixture's generic `std::fill(..., 0)` with
MSVC C4244 under `/WX`: template deduction retained `int` when assigning to `uint8_t`. The fixture
now supplies `std::uint8_t{0}`, preserving the same invalid zero-UUID bytes and every assertion,
without warning suppression or production changes. Focused producer/normal/optimized CLI tests
passed **3/3 in 40.20 seconds**. The final complete graphical gate then passed again:
**201/201, zero skips, 399.40 seconds**. Shipping results below remain applicable to the unchanged
production implementation. Hosted CI for the corrected commit is assessed separately before merge.

```bash
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 4
cmake --preset linux-shipping -B build/linux-static-player-shipping \
  -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_BUILD_PROJECT_PLAYER=ON \
  -DNEXORA_ENABLE_EDITOR_SDK=OFF -DNEXORA_FEATURE_EXAMPLE_PLUGIN=OFF
cmake --build build/linux-static-player-shipping --target NexoraProjectPlayer -j 4
build/linux-development/Tests/Editor/NexoraStaticProjectExportTests \
  --write-package ../production-static.nxproject
build/linux-static-player-shipping/Apps/ProjectPlayer/NexoraProjectPlayer \
  --verify-package ../production-static.nxproject
```

Minimal Monolithic Shipping passed 74 build steps with Editor/SDK/example stripped. Full-profile
Monolithic Runtime-only ProjectPlayer passed 51 build steps; cache confirms Editor, SDK and example
plugin are OFF. The actual generated production artifact, stored in the external task scratch
directory, verified as `VERIFIED_STATIC_VIEW` with
the three-asset/two-renderer result and exact full UUIDs/legacy shader/opaque name bytes. No Editor
objects are linked into this Shipping consumer. Touched C++ clang-format, Python syntax and
`git diff --check` pass. No generated package/build output is committed.

## Remaining boundary

No filesystem/source/catalog lookup or export publication occurs in the producer. A current-state
coordinator must authorize project identity/access/recovery, scene/content freshness, cancellation
and atomic output publication. GUI export, native player rendering, gameplay compilation, signing,
remote deploy/logs, physical hardware acceptance and the complete ED-M6 milestone remain open.
Existing little-endian-host NXAB and non-authenticating FNV integrity are unchanged. Windows/macOS
tests are not claimed to have run in this Linux environment; hosted CI is checked separately.
