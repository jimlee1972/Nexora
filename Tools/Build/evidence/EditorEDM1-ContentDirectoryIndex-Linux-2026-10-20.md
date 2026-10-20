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
