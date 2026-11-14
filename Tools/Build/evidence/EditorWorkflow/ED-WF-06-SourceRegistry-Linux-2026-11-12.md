# ED-WF-06 Sprite source registry cloud slice

Date: 2026-11-12. Base: `b1671eaa` (`origin/main` at task start).
Tracking: Beads `nexora-whr.6.1`, under `nexora-whr.6` / [issue #747](https://github.com/jimlee1972/Nexora/issues/747).
Scope: independent registry foundation; **not complete ED-WF-06 acceptance**.

## Implementation and acceptance boundary

The separate `NexoraGameplaySpriteContentApiV1` / Zig `content.SpriteApi` registers existing
96-byte cooked Sprite sources into GameplayContent. `GetSpriteApi` checks caller size and writes
only its V1 prefix. The original content table, Host/services, Sprite/Instance descriptors,
Atlas/Clip/scene wires and opaque catalog kind 3 remain unchanged.

Decoded metadata must resolve against the current registered Atlas through `ResolveSpriteAsset`.
Unknown/non-Atlas dependencies, incompatible region/trim dimensions and UUID type collisions reject
without publication. Source dependencies block Atlas removal and incompatible replacement.
Identical source publication reuses its immutable owner. Replacement/removal retains old source
snapshots and extracted Atlas/geometry snapshots, with existing active/live budgets and bounded
retirement accounting. Source retirement creates no texture ID. All access stays on the owner
thread and the table/context borrow the registry owner.

The canonical gameplay Host does not advertise the new table. Capability negotiation, source-bearing
C/Zig Sprite/Instance descriptors, catalog/Player admission, Editor authoring, source animation and
pool migration remain open in ED-WF-06 through ED-WF-09. Content registry kind 3 means Sprite source;
asset catalog kind 3 still means opaque bytes. No physical, Windows, macOS or complete graphical
Editor acceptance is earned. Workflow count stays **4/9 (44.4%)**, Editor **0/8**, 2D **9/11 (82%)**.

## Environment and exact commands

The Linux cloud executor initially lacked CMake, Ninja, Zig and development headers. Tools are
user-owned under `/workspace/work/engine-goal-20261112`: CMake 4.4.4, Ninja 1.13.2, GCC 14.2,
Zig 0.14.0, Clang 19.1.7 and clang-format 23.1.5. Official Debian packages were extracted to a task-local
sysroot; no system or repository build configuration was changed. X11/Vulkan libraries,
Xvfb/xdotool and a software Vulkan driver are available there. Markdown parser versions follow
`Tools/Build/documentation-requirements.txt`.

Commands inherit task-local tool PATH, `CMAKE_PREFIX_PATH` to the sysroot, its library path,
workspace cache/state paths (`XDG_STATE_HOME` included) and the local lavapipe manifest. Development uses four build/test workers;
Shipping uses two build workers. Original test assertions and deadlines are unchanged.
Zig layout tests are explicitly enabled without enabling the optional graphical shell or Showcase:

```sh
cmake --preset linux-development -DNEXORA_ZIG_EXECUTABLE=/workspace/work/engine-goal-20261112/tools/bin/zig
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

✅ Final local gates: Development configure/build/CTest all exit **0**; CTest has **267 passed,
7 skipped, 0 failed / 274 registered**, 131.43 seconds. Shipping configure/build both exit **0**.
The [machine-readable receipt](ED-WF-06-SourceRegistry-Cloud.json) retains both complete test
inventories and source hashes. The first full run exited **8**, with **255 passed, 7 skipped,
12 failed / 274**, 231.55 seconds; environment/SDK-generation repairs are explained below.

Skipped tests are the existing seven Vulkan parallel-recording native gates (texture recording,
graphics resource reuse, descriptor isolation, timing, reuse and two benchmarks). This default
preset does not enable their required shader/recording configuration. No skip was added or changed.
Existing native Vulkan scene/upload/swapchain, linear Sprite packet rendering, X11 input/metrics,
and the source-free authored-scene native export test ran successfully. The new source registry's
specific extraction assertions are CPU checks; this is not new source-registry native-pixel or
complete graphical Editor acceptance. Exact-head hosted CI remains a separate delivery gate.

Additional checks:

- `ctest --preset linux-development -R '^twod.gameplay_content$'`: exit 0 after the fixture fixes
  below. Covers malformed/version/trailing data, missing closure, collisions in both directions,
  table canaries, actual source-aware extraction, Atlas/source replacement, retained frames/source
  owners, removal order, wrong-thread rejection, identity limits and live-generation backpressure.
- `ctest --preset linux-development -R '^gameplay.zig_content_wire_layout$'`: exit 0, including
  C/Zig table size/alignment/offsets and absent/short/wrong-version/valid callback forwarding.
- Compile the exact base commit's `GameplayContentTests.cpp`, `GameplayContent.h` and
  `nexora/gameplay_content.h` with C++20 against the new shared library and execute: exit 0.
  The old consumer retains its original Atlas/Clip assertions and original table definitions.
- `cc -x c -std=c11 -pedantic-errors -Wall -Wextra -Werror -fsyntax-only -IEngine/API/include -`
  with the public header and a content-kind static assertion: exit 0.
- Touched C/C++ files use repository clang-format; touched Zig files pass `zig fmt --check`.

## Retained initial failures and repairs

Initial configuration failed because X11 development files were absent. After extracting the
headers, the cached X11 selection still pointed at a static archive, causing a shared-library
PIC link failure. Extracting the matching runtime and refreshing only `X11_X11_LIB` resolved it;
subsequent normal preset configuration selects the shared library.

The first new test fixture used `UiVertex.y` instead of `position[1]` and did not compile. Its next
run exposed the source-registration validation witness's missing instance identity/affine matrix;
registration rejected a valid source. The witness and extraction fixture now explicitly use a
nonzero identity and `runtime::ToMatrix({})`. Original codec/geometry validation stays intact.
Those failed runs are not counted as successful validation.

Beads adopted the existing `refs/dolt/data` through verified bootstrap, outside the repository.
One subsequent pull returned an upstream HTTP 503; a later pull succeeded before task-graph reads.
The scoped task was created/claimed and pushed to the verified existing Nexora remote.

The first full CTest run also exposed missing `clang++` in the reflection tool test and attempts
by Core/CLI/export fixtures to write diagnostics under the read-only default user-state path.
A task-local Clang 19 installation and `XDG_STATE_HOME` under the workspace fix those environment
prerequisites without changing engine code or test assertions. The embedded SDK byte guard also
correctly rejected a Zig documentation edit made after the initial configure; the final configure
and rebuild refresh its generated bytes. The complete first-run result and final run remain distinct.
