# Supported scene goldens and orphan-node rejection — Linux, 2026-10-21

Beads `nexora-gu7.39.1`, supporting M7-04. Broader migration reports, interruption/crash recovery
and physical acceptance remain open.

## Failure and repair

The actual product test first builds 132 target steps and fails with
`editor1-runtime3.scene: accepted orphan authoring node`. The previous parser rehearses only
migration/hint/opaque/prefab paths; a modern snapshot with ordinary tracked nodes can therefore
publish authoring metadata for an entity absent from Runtime. A separate probe linked against the
unchanged accepted product library reproduces the same error. An initial clipboard assertion
incorrectly expected both ancestor/descendant selections after Paste; it was corrected to the
existing root-only selection contract before recording the actual orphan-node regression.

Every nonempty authoring node set now participates in staged World validation. A single scan of
already unique Runtime IDs counts membership in the unique authoring ID set; missing nodes reject
before live replacement. Additional Runtime-only entities and documents with no authoring nodes
remain valid and have positive save/reopen tests. No public interface, format, plugin/linkage,
permission, history-budget or deadline change is made. The added validation performs a rehearsal
parse even for plain modern documents; complete existing scale gates must verify this cost.

## Fixed independent data and conservation

Six checked-in LF fixtures cover Editor1/Runtime1, Editor1/Runtime2, Editor1/Runtime3,
Editor2/Runtime3, Editor3/Runtime3 and Editor4/Runtime3. They are literal data rather than output
from the writer under test. Independent expectations verify complete local/world poses, legacy
world-space hierarchy conversion, negative scale, names, authored Euler turns, exact unknown
component identity/raw bytes and persistent prefab source/revision/scoped node binding.
Upgraded save/reopen bytes are deterministic and source goldens remain unchanged. Scoped Git
attributes retain exact LF parser bytes on Windows as well.

For every required-record truncation prefix and explicit trailing garbage/unsupported version/
duplicate/orphan candidate, file and memory loaders must reject while retaining a live dirty
document, stable keys/generation, selection, complete prepared bytes, saved baseline, Undo/Redo,
clipboard, unrelated scene and last-good output. Occupied foreign staging is conserved on failed
SavePrepared. Final whitespace omission is accepted as legal syntax, not detectable corruption.
This does not claim detection of arbitrary mutations that remain valid scene data.

Corrected actual product focused test passes in 0.53 s. It checks 168/197/201/218/261/445 rejected
inputs per respective fixture (1490 total), each through both loading paths.

The initial complete required configure/build/CTest gate on main `5f926931` passes: 660 remaining
product build steps after focused target builds; **304 registered, 292 actual passes, 12 environment
skips, zero failures, 647.76 seconds**. All ten frozen code/CMake/fixture/attribute hashes match.
Original Deep/100k pass 63.53/224.98 s under unchanged 180/300-second limits and scale1.

Main advanced through accepted Git diff SDK PR #679 during the first gate. A normal merge of
actual main `95d0b3e5be070fd014f178d91e9f88f0058bc22d` preserves the repair and fixtures. The complete
combined required gate then passes again:

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

GCC 14.2/CMake 4.4.4, graphical shell and ProjectPlayer ON, local Slang/Zig OFF. Build passes 329
integration steps. Full CTest passes **305 registered, 293 actual passes, 12 environment skips,
zero failures, 599.50 seconds**. All 45 combined code/test/CMake/fixture/helper/attribute hashes match
before/after. Golden test passes 0.55 s, original Deep/100k 65.36/201.02 s with unchanged 180/300-second
limits and scale1. These observations do not certify production-percentile or physical performance.
No exported/linkage boundary changes require a new Shipping gate for this internal parser repair.

Bilingual roadmap and contract evidence cover only this finite cloud slice. Applicable exact-head
hosted checks must succeed before squash merge; broader migration/recovery and physical results
remain open.

Final documentation tests pass 16/16 in 1.400 s; changed Markdown/bilingual links, touched C++
format, diff, unchanged root README and all 45 source hashes pass.
