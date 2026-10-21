# Persistent prefab binding comparison — Linux, 2026-10-21

Beads `nexora-gu7.21.1`, a supporting slice of M4-05. Reviewed per-field resolution, validated
merged candidates, atomic publication and complete M4-05/ED-M4/physical acceptance remain open.

## Actual missing information and repair

Production `CompareSceneRevisions` previously ignores persisted PrefabPlacement records. A literal
valid version4 scene with identical Runtime, names and unknown bytes but source revisions7/8/9
therefore compares equal. The actual linked product regression fails in .09 s with
`Different retained prefab revisions compared equal` before the repair. An initial test-only
temporary-string reference warning was corrected with an owning row before that actual RED run.

A bounded projection now adds `prefabs/<instance>/present`, `source`, `revision` and
`nodes/<target>/present`, `source_node`, `scope`. Scope retains the ordered slash-separated UUID
sequence; a present empty root scope differs from a missing mapping. Revisions and target IDs
retain unsigned decimal precision. Parser-validated instance/target identities provide stable
keys; placement/node record order is not semantic. No asset resolution, callback, native load,
source IO, caller document/history/baseline mutation or new format/public layout is introduced.
Every new field uses the existing source/entity/value/snapshot/row/result budgets and output owns
all values after input views expire. Internal projection changes require no new Shipping gate.

## Independent evidence

Literal two-node version4 fixtures verify exact three-way revision conflicts, UINT64_MAX, source
asset and source-node replacements, ordered nested scope conflicts, target mapping removal/addition,
instance identity changes, shared new bindings and removal versus revision editing. Exact unknown
payload `00ff11` remains independently inspectable. Invalid zero revision, nil source, dangling
target and duplicated mapping reject the complete comparison. Node and placement record reorders
produce no differences. A binding-free fixture retains schema3 for its opaque data: schema4
requires a placement, so an initially invalid schema4 deletion fixture was corrected rather than
weakening parsing.

Literal 2048-node shallow and 1024-node maximum-depth32 bindings fit comparison budgets. A valid
2048-node depth32 scene loads successfully in production but exceeds the existing 4 MiB semantic
snapshot budget and rejects with no partial result. Source and document limits are unchanged.

The actual comparison UI fixture now saves a bound scene, edits its local Name and observes a
real disk revision with a conflicting Name and removed binding. Actual 1x/2x widgets display all
seven rows, including six binding deletion fields, and retain existing readonly/cancel/stale,
overwrite-conflict and no-destructive-request checks. An initial fixture assertion expecting
schema3 for an opaque-free source was corrected to the existing writer's schema2 contract.

Corrected focused Core/UI tests pass 2/2 in15.67 s (15.60/.06 s).

The first complete required Linux configure/build/CTest gate includes both pending own source SDK
and graphical source controls plus accepted evidence-tooling main616e7292. Private0634d096,
treef9fd55dd, passes **313 registered,301 actual passes,12 environment skips,zero failures,643.90 s**.
All65 combined source hashes match. Deep/100k pass73.27/214.60 s under original180/300-second limits
and scale1. Product builds include400 feature integration and326 evidence-tooling integration steps.
This is historical combined-tree evidence, not a dependency on the pending graphical source PR.

To publish this finite repair independently, the exact original eight-file feature is applied to
fresh accepted main49b13e22 after source SDK acceptance. Private379355930f447bf86f553a0c48094017db78b5a0,
tree8b656a3e559bf06cb702c05226f19a72b2dc2963, passes a new complete required Linux gate:
**311 registered,299 actual passes,12 environment skips,zero failures,697.98 s**, after406 integration
build steps. All61 frozen current-main/source/fixture/helper hashes match; the four projection/test
sources retain the same contents. Deep/100k pass66.00/242.28 s with original180/300-second limits
and scale1. No production percentile or timing isolation is inferred. The source GUI feature and
its two additional tests are absent from this independent tree.

Initial documentation CI passes16/16 in30.235 s. Final documentation and exact-head hosted checks
remain pending. Internal projection changes add no exported layout/linkage and require no new
Shipping gate. No real GPU, physical DPI/IME or screen-reader acceptance is inferred.

## Hosted Windows portability follow-up

The initial public headbd921b56 fails Windows desktop and mimalloc compilation: MSVC /W4 /WX
reports C4456 at the scope UUID loop because its local `id` shadows the scene identity. Rename
only that loop variable to `scope_id`; emitted values, paths, limits, ABI and behavior are unchanged.
No diagnostic suppression or compiler/deadline relaxation is added. An additional Linux compiler
syntax pass with `-Wshadow -Werror=shadow` succeeds. The complete required configure/build/Linux
gate passes again: **311 registered,299 actual passes,12 environment skips,zero failures,689.26 s**,
with all61 updated source hashes unchanged. Deep/100k pass95.81/208.12 s at the original180/300-second
limits and scale1. The superseded public head must never be merged. New exact-head hosted
Windows/other checks remain required; Linux does not substitute for MSVC validation.
