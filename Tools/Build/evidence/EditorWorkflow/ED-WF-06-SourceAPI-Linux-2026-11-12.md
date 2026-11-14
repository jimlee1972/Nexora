# ED-WF-06 compatible source APIs and Player extraction

Date: 2026-11-12. Base: `120193793460fb497752cb6d2d0339e70ee098d4` (PR #767).
Tracking: Beads `nexora-whr.6.2`, under `nexora-whr.6` / [issue #747](https://github.com/jimlee1972/Nexora/issues/747).
Scope: source descriptor, capability negotiation and module-embedded Player content slice.
**ED-WF-06 remains unaccepted**: file catalog closure and the Editor source workflow remain open.

## Contracts and executed acceptance

✅ Independent C/Zig source Sprite/Instance wires have fixed 80/160-byte strides on supported
64-bit targets. Historical wires remain 64/144 bytes. Optional TwoD and services suffixes negotiate
complete supported prefixes; every intermediate caller size and caller-tail canary is exercised.
Old hosts remain usable without the suffix. Returned tables require size/version/callback checks.
Source identity is separate from authored style/transform; zero UUID means absence. Batches stage
before publication/output, preserving existing owner/index/generation handles and legacy destruction.
Wrong-thread operations reject. Source-bound legacy reads and animation reject without output loss;
explicit legacy writes clear a source. Runtime adds no TwoD implementation dependency.

✅ The canonical Player offers source-content registration and source-aware extraction. A real Zig
module uses the shared CMake/game SDK and public tables to register Atlas/source bytes, then create
one scene Sprite and one negatively scaled pooled instance. Missing Atlas registration rejects
without changing outputs. At frame 10 it replaces the Atlas red→blue and changes source Pivot,
retaining the instance handle and authored transforms. It validates source identity every frame,
destroys through the old pool API and removes source before Atlas on Stop.

The export is relocated into a Unicode path; original scripts and assets are deleted. Headless and
native Player runs use an unrelated working directory and leave the runtime-only inventory intact.
Native Xvfb/lavapipe readback checks both scene and pooled pixels before/after replacement:
red at x=124/244, blue at x=76/292, with old/new locations empty in the opposite phase (y=120).
The fixture pauses at capture markers to retain an already submitted frame; production rendering
has no fixture branches. Captures are actual window readbacks: [red](SourceAPI-red.png),
[blue](SourceAPI-blue.png). This is Linux software Vulkan correctness, not physical-PC performance.

`twod.sprite_source_player` always runs headless when Zig is found. Full Linux graphical/native
Vulkan configurations also select native readback; the explicit local native command below covers
the default preset's graphical-shell-OFF configuration. Missing native prerequisites fail when
native readback is selected; no new skip is added.

Main Editor Play offers the shared source instance table but still owns frozen Atlas/Clip content,
with no source-content registration or source authoring/capture. Its empty source resolver makes
unresolved source instances unavailable without rejecting ordinary Sprites. File catalog kind 3
remains opaque bytes; content kind 3 is a Sprite source. No `.nx2sprite` catalog admission, source
animation/reload transaction, Editor workflow or physical/Windows/macOS acceptance is claimed.
Counts remain ED-WF **4/9**, Editor **0/8**, 2D **9/11**; M6/M7 acceptance is unchanged.

## Reproduction and validation

The task reuses the previous slice's user-owned CMake 4.4.4, Ninja 1.13.2, GCC 14.2, Zig 0.14.0,
Clang 19.1.7, clang-format 23.1.5, extracted Debian X11/Vulkan/Xvfb/xdotool sysroot and lavapipe.
PATH, CMAKE_PREFIX_PATH, library/cache/state paths and VK_DRIVER_FILES point at the task workspace;
no system installation or tracked preset override is required. Build/test workers: Development 4,
Shipping 2. Slang and the graphical shell stay OFF locally, with `NEXORA_ZIG_EXECUTABLE` cached
to the task-local Zig. The original gate has optional Zig gameplay OFF; after hosted CI exposed
an omitted optional-layout expectation, both Development and Shipping also run with
`NEXORA_ENABLE_ZIG_GAMEPLAY=ON`. Final configure refreshes embedded SDK bytes.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
python3 Tests/TwoD/SpriteSourcePlayerAcceptance.py \
  --app /workspace/Nexora/build/linux-development/Apps/TwoDPlayer/NexoraTwoDPlayer \
  --zig /workspace/work/engine-goal-20261112/tools/bin/zig \
  --cmake /workspace/work/engine-goal-20261112/tools/bin/cmake \
  --evidence-dir /workspace/Nexora/build/linux-development/artifacts/source-player-final --native
```

✅ Final Development configure/build/CTest exit **0**: **271 passed, 7 skipped, 0 failed /
278 registered**, 158.83 seconds. Shipping configure/build both exit **0**. The seven
skips are the unchanged conditional Vulkan parallel-recording gates whose shader configuration is
OFF in this preset; no new test skips. The final direct native Player run passes all 30 frames
and both actual pixel phases with balanced allocations.

Final gate results and immutable source/artifact hashes are retained in the
[machine-readable receipt](ED-WF-06-SourceAPI-Cloud.json).

✅ Final expanded Zig Development configure/build/CTest exits **0**: **303 passed, 7 existing
conditional Vulkan recording skips, 0 failed / 310 registered**, 245.08 seconds. This includes
the repaired original wire enumerator, the new historical-host-prefix assertion, real typed Zig
consumers/reload and native HD-2D. Zig Shipping configure/build also exits **0**. The initial
expanded gate passed 303/310 before the additional historical-host assertion; both results remain
distinct in the receipt. The old consumer is rerun successfully against the final expanded build.

The expanded Zig gate and Shipping Player native run are retained separately in the receipt.
The monolithic Shipping Player repeats the relocated/source-deleted fixture and both pixel phases;
its red/blue PNG hashes match the curated Development captures exactly. The final fixture also
retains the export inventory and a SHA-256 map of every runtime-only package file. Export uses
`--allow-development` for this internal fixture even with the Shipping host; this is not production
package publication acceptance. CI now uploads every
`artifacts/source-player/` attempt, including logs and captures from failed runs.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON
cmake --build --preset linux-shipping
# Repeat the native command above with --app build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer
# and --evidence-dir build/linux-shipping/artifacts/source-player-final.
```

Additional checks include the API-M6 manifest validator and three mutation tests rejecting baseline
removal, C/Zig field reorder and missing metadata for any of the eight new callback contracts.
The exact base commit's original bridge test, bridge declaration and public C headers are compiled
against the new shared libraries and executed unchanged. A strict C11 public-header consumer
checks frozen strides and the optional helper. Touched C/C++ and Zig files are formatted; changed
Markdown links and bilingual roadmap pairs are validated. Root README remains unchanged.

## Retained failures

The first targeted wire test failed because Zig cannot translate the C `offsetof`-based macro
through `@cImport`. The test now compares the imported C field offset directly; the C macro itself
passes the strict C11 consumer. The original failed run remains separate from the passing rerun.
No runtime assertion or test deadline was relaxed.

The first full Development run exited 8: 270 passed, 7 skipped, 1 failed / 278 registered
(252.56 seconds). `twod.play_context` still treated new `sizeof(table)-1` as unsupported even
though it now contains the complete supported historical prefix. Its boundary assertion now uses
`NEXORA_2D_API_V1_BASE_SIZE-1`, and an additional assertion checks successful original-prefix
acquisition with no optional getter copied. After rebuilding, the entire gate is rerun; this
initial failed inventory remains in the receipt.

The external old-consumer harness initially lacked adjacent C headers, explicit transitive link
libraries, and their runtime search paths. Supplying all exact-base public headers and the same
library list as CMake resolves those harness setup failures; the original test is unchanged.
Beads claim synchronization initially saw a non-fast-forward push; pulling shared history and
pushing again succeeded without forcing or replacing it. Neither issue changes engine behavior.

The first immutable hosted head `06de103c` failed macOS push Build
[43188204579](https://github.com/jimlee1972/Nexora/actions/runs/43188204579), job `131080850111`:
417 passed, 1 optional font skip, 1 failed / 419 (1299.00 seconds). The existing
`gameplay.zig_twod_wire_layout` C++ list omitted the new `get_source_api` offset even though Zig
correctly enumerated it, so the final sentinel assertion ran one index early. The missing offset
assertion is added; the sentinel remains. A second interoperability assertion verifies current Zig
accepts a host returning only the original TwoD table prefix. The default local configuration did
not enable that optional target; expanded Zig Development/Shipping gates now cover it before the
corrected head is published. The initial failed CI and its repaired-head evidence remain distinct.
