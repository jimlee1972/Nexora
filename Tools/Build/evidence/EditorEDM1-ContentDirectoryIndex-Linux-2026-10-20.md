# ED-M1 cold Content directory index — Linux, 2026-10-20

Beads `nexora-gu7.2.2`, initial main baseline `8fd501c46d9736ce5295c0ee12991eeca6518e20`.
Separate `nexora-gu7.2.3` publishes sort-key acceleration and the shared phase benchmark. Reconcile
that shared benchmark on current main before publishing this independently scoped directory PR.

## Contract

A lazy owning immutable index groups direct asset slots and sorted immediate child directories.
First admission traverses source paths/unique ancestor prefixes; subsequent folder/filter queries
use current-folder slots and child copies. Source/order Reset/Discover/Commit/Undo revoke it;
selection, filters, folder navigation and artifact publication retain it. Model copies share only
immutable paths/indices and resolve rows into their own item storage. Retained public child vectors
own only their children, avoiding a whole-project index lifetime extension. No index is persisted.

Admission caps:262144 items,65536 directory nodes including ancestors,64 parent components,16 MiB
UTF-8 path work (source bytes plus3x newly indexed prefix bytes). Counts/work are not total RSS.
Native-root/drive-relative special cases, budget exhaustion and candidate allocation failure keep
the complete original linear behavior. The decision persists until source/order replacement.
No source record/navigation is discarded; ordinary output allocation behavior is unchanged.
Queries serialize on the authoring thread. Observations do not grant filesystem authority.
Matching C++ consumers rebuild for the private model layout; SDK owners are released before Core
library unload. Stable C ABI, project/asset formats and module dependencies are unchanged.

## Validation in progress

Strict GCC14.2 syntax passes for the actual provider and new test/benchmark consumers. Independent
complete product build is in progress. Tests use the original complete linear semantics as an
oracle for native paths/UTF-8 labels/order, filters, clipped ranges and selection; include component
prefix siblings, unrelated roots, Unicode/raw POSIX names, file/directory-name collision and repeated
separators. Rename/Discover/Undo/Move/Delete/artifact publication, same-count replacement, stale drag
authority, model copies and retained snapshots exercise invalidation and ownership. Partial candidate
allocation, exact/over item/folder/depth/path-work admission preserve complete sources/navigation.

The shared baseline uses100k metadata records in1000 folders,100 records per folder; it creates no
source files or production mesh/GPU payloads. Actual old Core library plus matching headers passes
checksum13050. Fixture373.49ms,Reset15324.49ms,first root418.35ms,100 cold folders13724.37ms and200
searches5516.63ms. Sort-only results belong to the separate sort delivery. Final directory timings
will record the exact integrated baseline, machine/compiler/load and all unchanged-result guards.
One measurement pair is not a production percentile/RSS/GPU budget.

Focused/full Linux/Shipping, final measurements, documentation and hosted results remain pending.
Production asset scale and complete ED-M1/physical acceptance remain open.

## Independent complete baseline gate

Final six source/CMake hashes remain unchanged. Independent Development Modular configure/build
passes773 initial steps; Minimal Monolithic Shipping configure/build passes102 steps (Editor is
stripped by profile policy). Initial Shipping dependency population could not reach the default
proxy; the retry uses the exact already-pinned local Vulkan-header checkout, with the original log
retained externally. No product source or correctness condition changed for that environment fix.

Fresh Linux preset configure/build and complete serial CTest pass290 registered:278 actual passes,
12 existing native feature/environment skips,zero failures,727.58s. Directory regression72.35s,
original100k lifecycle276.08s at scale1. Earlier focused1/1 passes77.87s. Full LastTest is archived
outside the repository. Documentation16/links/bilingual/format/diff checks pass on this baseline.
Latest-main/shared-sort integration and all hosted checks are still required before publication.

Actual directory-only metadata benchmark after this baseline:

```json
{"schema":1,"dataset":"content-directory-metadata","label":"after","items":100000,"folders":1000,"fixture_ms":358.843512,"reset_ms":18410.436464,"first_root_query_ms":374.704445,"cold_folder_queries":100,"cold_folder_ms":4.356365,"search_queries":200,"search_ms":49.464271,"checksum":13050}
```

The same checksum13050 and unchanged-source/history guards pass. Cold folder/search results improve
for this dataset; Reset still uses the original comparator and its timing variance is recorded.
Both caches' combined/current-main results will be recorded after the independent sort prerequisite.
These are single observations with other build/CTest load, not production percentiles/assets/GPU.

## Prospective current-main plus sort integration

An unpublished validation branch combines this directory feature, latest main `43d6bb96` and the
unchanged sort PR673 head `b4da80cd`. Conflict resolution retains both independent helpers,
invalidation paths, test registrations and exactly one shared benchmark target. Fresh Development
configure/build passes 326 integration steps. Full combined correctness/Shipping/hosted gates remain
pending; the directory PR waits for the independent sort prerequisite to reach main.

Actual combined-library 100k metadata benchmark (same schema, checksum/source/history assertions):

```json
{"schema":1,"dataset":"content-directory-metadata","label":"after","items":100000,"folders":1000,"fixture_ms":478.837823,"reset_ms":1957.883825,"first_root_query_ms":330.132122,"cold_folder_queries":100,"cold_folder_ms":4.292734,"search_queries":200,"search_ms":32.593601,"checksum":13050}
```

This single observation runs on the same GCC14.2 managed4-CPU Linux environment with an independent
CTest and Shipping build active. Reset is1957.88ms; 100 cold folders4.29ms; 200 searches32.59ms.
These phase measurements preserve complete results and illustrate the two independent optimizations;
they are not production-assets/RSS/GPU percentiles or isolated performance calibration.

## Complete combined delivery gate

The unchanged eight combined source/CMake hashes pass fresh Linux preset configure/build and full
serial CTest:294 registered,282 actual passes,12 existing native feature/environment skips,zero
failures,737.74s. Directory regression41.18s, sort37.64s, Deep97.57s and100k286.00s retain all
original correctness assertions and180/300s hierarchy deadlines (scale1). Complete LastTest is
archived externally. A separate Minimal Monolithic Shipping configure/build passes the9-step
integration over its earlier complete102-step build; that profile strips Editor by policy.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_ENABLE_SLANG=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF
cmake --build --preset linux-shipping
```

Independent sort PR673 has now merged as main `319823b92e77803c1b77d565d203e29cf1bf412b`, whose
exact tree matches the separately fully tested main43d6-plus-sort candidate. The directory branch
integrates that committed main before publication and verifies all combined code hashes unchanged.
Documentation/links/bilingual/format checks and exact-head hosted CI remain separate publication/
merge requirements. The source dataset and all results remain complete; production assets/RSS/GPU,
general timing variance and full ED-M1/physical acceptance remain open.

## Windows strict-build oracle correction in progress

Published head71c68c59 has two real hosted Windows compile failures (jobs118919543392 and
118918633313). The new directory test oracle's lowercase transform returns std::tolower's int
into a char output; MSVC reports C4244 under the existing /W4 /WX policy. The oracle now makes the
same byte conversion explicit with static_cast<char>, retaining unsigned-char input to tolower.
No production index behavior, expected result, assertion, warning level or timeout is changed.
Original failures remain evidence; no blind hosted retry is used. A fresh full Linux gate and
applicable checks precede a normal update of PR675. Windows success remains pending new CI.

## Corrected oracle complete Linux gate

The explicit-char oracle correction passes fresh configure/build (428 integration steps), focused
directory1/1 in36.24s and full serial Linux CTest:297 registered,285 actual passes,12 existing
native/environment skips,zero failures,652.84s. All eight corrected source/CMake hashes remain
unchanged through the gate; the full LastTest is archived externally. Minimal Shipping configure/
build passes metadata integration; Editor is stripped and no Shipping tests are registered.

The original Windows mimalloc job118919543582 independently reports the same C4244 at the same
oracle transform; all three original Windows failures remain retained. No product model/index
behavior, assertion, warning level or timeout changed. Main474 adds authoring documentation/tests
only, with no runtime delta; normal integration and those affected tests follow before normal
PR675 push. Fresh Windows hosted success remains required before squash.

## Authoring-only current-main integration

The corrected directory branch normally integrates main4744c746. Its delta from319823b9 is only
seven authoring documentation/test/CMake files, with no runtime change. All eight corrected index/
matching-test hashes remain unchanged. Rebuilt affected directory/authoring product targets and
all three directory plus actual1x/2x authoring-reopen checks pass39.20s. The corrected full297-test
runtime gate above remains unchanged. Documentation16, links/bilingual pairing, formatting and
diff checks are repeated before normal PR675 update. Original Windows failures remain evidence;
all new exact-head hosted checks must succeed before squash/closure.
