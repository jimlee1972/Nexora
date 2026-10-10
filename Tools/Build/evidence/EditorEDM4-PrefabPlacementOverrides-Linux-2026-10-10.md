# Owning live prefab instance property review — Linux, 2026-10-10

Beads `nexora-pmb.1.23`; base is owning source inspection PR509 `bcb4829e`.
This review carries observation only, never authoring authority or source writes.

Exact retained nested sources materialize in an isolated temporary document.
Scoped source identities translate baseline node/parent IDs to live keys before
semantic comparison, so existing unrelated nodes and repeated nested references
do not fabricate differences. Rows own retained source revision, scope, stable
node/field UUID and semantic field values. New local opaque component fields
retain their serialized type identity without inventing a source property UUID.
Authored Euler revolutions and unavailable opaque names/bytes remain visible.
Parent changes are explicitly structural; scene-global and absolute sibling
index fields are outside this property review.

Actual mixed retained revisions1/2 fixtures cover clean translated IDs, local
name/Euler/opaque changes and component additions, read-only preparation,
deterministic rows across real Undo/Redo, valid same-revision source replacement,
missing retained archive, stale/unbound keys, hierarchy differences and an
over-budget 40000-byte opaque payload. Preparation, failure and source matching
preserve every project/source file, prepared live scene and user history.
Existing subsequent clipboard and bound Scene Save/reopen tests remain intact.

The review repeats owning source/target/project scope after preparation and on
Matches. Missing/corrupt/incompatible sources or semantic budgets return no
partial report. Whole-scene semantic input is limited to 8 MiB/4096 entities,
snapshot to 4 MiB, each value to 64 KiB; opaque payload hex doubles its byte size.
Reports retain at most32768 rows/16 MiB logical field/scope/value data. These are
logical data limits, not total temporary World/process memory or performance claims.

Graphical Linux Development configure/build passed (211 steps); focused owner,
atomic property, nested materialization and semantic comparison **4/4 passed,
14.52s**. Full `ctest --preset linux-development` **243/243 passed,608.78s**,
zero failures or skips. Graphical shell, Cryptography/OpenSSL, Slang, Zig,
Showcase and native ProjectPlayer were enabled. Minimal Monolithic
`linux-shipping` configure/build passed (five actual steps).

Graphical live instance diff and atomic revert/apply/rebase remain open; complete
Editor milestones stay **0/8**. Linux execution does not claim other OS/physical-host runs.
