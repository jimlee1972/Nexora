# Sprite source scene/pool bindings — Linux evidence (2026-10-28)

This supporting slice adds persistent C++ source UUIDs and extraction-time closure resolution.
It does not complete source-aware animation, Editor/Player catalogs or full Sprite acceptance.

## Executed cloud gates

After sourcing the task's external tool/SDK environment:

```sh
cmake --preset linux-development
cmake --build --preset linux-development --parallel 6
ctest --preset linux-development --parallel 4
cmake --preset linux-shipping
cmake --build --preset linux-shipping --parallel 2
```

Development configure/build succeeded. Final full CTest: **237 passed, 13 native-prerequisite
skips, 0 failed** (250 registered). Shipping Monolithic configure/build succeeded. The focused
`twod.sprite_source`, `twod.runtime_contracts`, `twod.gameplay_bridge` and
`twod.authoring_contracts` checks also passed 4/4. `git diff --check` passed.

The first full run could not start two unrelated Editor executables because these generated ELF
files lacked executable permission. Their build-output permissions were repaired and the complete
suite rerun successfully; no tests, product source or assertions were disabled to fix that issue.

## Verified contracts

- Mixed source/legacy NX2S schema-3 roundtrip, source-free schema-2 byte preservation, backward
  restore and source clearing; every truncated snapshot and trailing/unknown-version data reject
  atomically without changing the original scene.
- Hand-calculated trim/Pivot vertices through loaded scene and pooled extraction, per-instance
  Flip/negative scale, unchanged authored scene/pool fields and stable identities.
- Source and Atlas positive/negative results resolve once per UUID per frame, shared across mixed
  source/legacy inputs. Invisible/zero-alpha inputs do not resolve. Invalid identities and
  source-plus-explicit-quad ambiguity reject before callbacks.
- Wrong source UUID, malformed metadata, missing region and mismatched trim dimensions reject.
  Missing source/resolver/Atlas reports unavailable without legacy fallback.
- New immutable source/Atlas data under the same UUID changes the next extraction while an older
  frame retains its previous Atlas pixels. Caches do not survive extraction.
- Legacy C Sprite/pool reads preserve outputs and return `UNSUPPORTED` for source-bound values;
  legacy region animation cannot play on a source-bound Sprite. Existing legacy checks still pass.
- Editor documents without source catalogs reject Add/Edit source bindings without changing their
  prior saved state; no unsupported document is accepted for Undo/Play.

## Native boundary

`Tests/TwoD/NativeTests.cpp` adds snapshot/restore plus five trim/Pivot/Flip/negative-scale pixel
cases through the actual compact GPU instancing packet and Vulkan surface. It passed standalone
C++20 syntax compilation with the configured test target's strict warning/include flags.

The cloud cache has Slang disabled and no usable native display, so this native target was **not
built or executed here**. The new cases run in existing `twod.native_pixels` CI configurations with
Slang/X11/Vulkan prerequisites. No fresh native pixel, Windows/macOS/mobile, target-PC performance,
M6/M7 or source-aware Editor/Player acceptance is claimed by this local evidence.
