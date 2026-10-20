# ED-M2 counted reflected-array Core authoring — Linux cloud evidence

Date:2026-10-20. Beads `nexora-gu7.5.1`, parent M2-03 remains open.
Initial main baseline:`43d6bb967e41c9a5d572285056d2da7e0e168f0c`.

## Contract and compatibility

Version2 reflection metadata explicitly declares bounded little-endian u64 counts in an owning
fixed prefix and schema-ordered variable typed tails. Version1 fixed layouts remain readable
unchanged; unknown component bytes are never guessed/migrated/reinterpreted as native vectors.
Multiple tails use each source's actual lengths. See Editor README for exact grammar, primitive
encodings, ownership, serialized authoring-thread access, complete admission and unload rules.
C++ records grow and consumers rebuild; stable C/Gameplay ABI, Scene formats and dependencies stay.
Scene opaque persistence retains the existing staged/atomic publication path.

Insert/Append/Remove/Move/Clear and element/flags editing validate complete current catalog,
generations, exact sources, current selection and explicit owner authority. All replacements stage
before one authoring transaction. Unknown prefix/other components survive. Corrupt, invalid,
read-only, stale, over-budget and allocation-rejected edits preserve source/history. The old opaque
batch history admission cleared Redo before allocating its Undo slot; admission now owns the Undo
entry before clearing Redo/live node swap. Private one-shot replacement/history faults verify this
failure path without retaining callbacks/allocators. No-op Move/Clear preserve pending Redo.

## Focused correctness

Fresh product build and `ctest --preset linux-development -R
'^editor.reflected_(dynamic_arrays|inspector)$'` pass2/2 in0.18s before final freeze. Core assertions
cover mixed lengths, multiple following-tail offsets, every existing primitive/vector/color/reference
kind, enum/flags validation, exact32/over32 capacity, full-target output budget rejection, unknown
prefix/unavailable components, whole-batch Undo/Redo and fresh World Unicode-path save/reopen.
Malformed/truncated/extra counts and invalid elements outside the common multi-selection intersection
reject without dropping payload. Metadata1/2 compatibility, unsupported version3, bad counts/offsets,
collisions and stale source/catalog/selection/reload generation reject. Allocation faults cover
before/after the first replacement and Undo admission, retaining pending Redo; successful retry works.

Complete final Linux/Shipping/documentation/hosted gates remain pending. This Core foundation does
not close structural array widgets, native UI authoring or the full M2-03/ED-M2 milestone. Cloud
headless tests cannot certify physical DPI/IME/display or production performance/RSS.

## Complete initial main baseline gate

The seven final source/CMake hashes are unchanged through fresh Development Modular configure/
build (207 final steps), focused2/2 in0.18s and full serial Linux CTest:296 registered,284 actual
passes,12 existing native/environment skips,zero failures,647.23s. Dynamic arrays pass0.20s;
unchanged Deep76.61s/100k266.83s retain the original180/300s deadlines at scale1. Full LastTest
is archived externally. A preliminary stopped build and a briefly overlapping replacement scheduler
were both excluded; affected own generated objects were removed before the single final frozen
rebuild and full gate. No interrupted result is reported passed.

Minimal Monolithic Shipping configure/build passes22 integration steps; the profile strips Editor
and disables tests, so Shipping CTest reports no tests. It is not Shipping Editor execution.
Documentation routing tests16/16, local Markdown links/bilingual counterparts and diff checks pass.
Actual main has since advanced through the independent Content sort delivery; normal integration
and a fresh complete gate follow before publication. Structural array widgets remain a separate
child delivery and cannot be closed by these Core results.

## Complete latest-main integration gate

Normal merge of main319823b92e77803c1b77d565d203e29cf1bf412b preserves all six Core implementation/
test hashes; the test CMake changes only by adding the independent sort registration/benchmark.
Fresh Development configure/build and full serial Linux CTest pass297 registered,285 actual passes,
12 existing native feature/environment skips,zero failures,739.25s. Dynamic arrays0.15s,
Deep79.69s and100k271.24s retain all original assertions and180/300s deadlines (scale1).
Full LastTest is archived externally. The initial and latest-main gates are distinct recorded runs.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF
cmake --build --preset linux-shipping
ctest --preset linux-shipping
```

The final Minimal Shipping metadata integration and documentation16/links/bilingual/format checks
are repeated before publication. This profile strips Editor and has no registered tests; Development
and exact-head hosted checks cover the actual Editor boundary. Hosted checks must all succeed before
squash merge. Structural widgets remain the separate dependent child; no full M2-03 or ED-M2/
physical/native-window acceptance is claimed by this Core delivery.
