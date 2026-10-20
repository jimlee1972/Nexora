# ED-M1 bounded UTF-8 path sort staging — Linux, 2026-10-20

Beads `nexora-gu7.2.3`; main baseline `8fd501c46d9736ce5295c0ee12991eeca6518e20`.
Published Git-provider branch/PR670 stays unchanged; its local build worktree is reused on the
separate unpublished content-sort branch. This delivery does not include the directory index.

## Contract

Reset/Discover (both current and retained Undo records)/Commit formerly reconstructed UTF-8 path
keys for every sort comparison. Each admitted sort now captures a single owning key per item,
sorts keys and allocates a source-to-destination permutation before changing any source record.
The permutation swaps complete owning ContentItem values; a static noexcept check guards the
post-staging step. UTF-8 byte ordering, unknown metadata and geometry/material owners are retained.

The private acceleration admits 262,144 items and 16 MiB total UTF-8 key bytes. Container/allocator
overhead and total process RSS are separate. A budget or cache-allocation failure releases all
staging and uses the old complete comparator; data is never truncated or rejected for this cache.
The fallback retains its previous ordinary result/allocation behavior. Calls remain serialized on
the authoring thread with no new asynchronous work or retained borrowed keys. Public model layout,
persisted formats and module dependencies are unchanged. Private DLL test probes hold POD counters,
not callbacks/allocators; all SDK owners retain their usual library-lifetime obligation.

## Correctness and initial measurements

Tests compare every owning field against the original projection-sort oracle, including actual
triangle geometry/canonical scalar material owners, Unicode composition, case, supplementary-vs-BMP
UTF-8 order and POSIX raw bytes. Rename/Discover/Undo/Move/Delete and failed admissions preserve
source/selection/authority. Partial key-cache allocation fault falls back completely and recovers;
exact/over item and key-byte bounds retain all identities/paths/records.

The initial focused CTest passed33.15s. A subsequent source-only fixture update adds supplementary
Unicode ordering, followed by a fresh configure/build (2 steps) and Shipping integration (9 steps);
the final complete gate includes those additional cases and remains pending at this recording.

The shared schema-1 benchmark contains100,000 metadata records in1000 folders,100 records/folder.
No source files or production mesh/GPU payloads are loaded. Both actual old-library and new-library
runs check checksum13050, unchanged source/history and complete typed results. GCC14.2/Linux,
Development Modular, managed4-CPU quota; other build/CTest work was active. This is one observation
pair, not isolated calibration or production percentiles. Reset improves; cold-folder/search paths
remain unchanged in this delivery and their observed variance is not claimed as fixed.

Before (actual baseline Core library, matching baseline headers):

```json
{"schema":1,"dataset":"content-directory-metadata","label":"before","items":100000,"folders":1000,"fixture_ms":373.494272,"reset_ms":15324.485094,"first_root_query_ms":418.353154,"cold_folder_queries":100,"cold_folder_ms":13724.368062,"search_queries":200,"search_ms":5516.627345,"checksum":13050}
```

After (matching rebuilt Core library and headers):

```json
{"schema":1,"dataset":"content-directory-metadata","label":"after","items":100000,"folders":1000,"fixture_ms":452.936078,"reset_ms":1999.798060,"first_root_query_ms":486.076977,"cold_folder_queries":100,"cold_folder_ms":15953.363775,"search_queries":200,"search_ms":9448.319416,"checksum":13050}
```

Complete final Linux/Shipping/documentation results and exact-head hosted CI are recorded before
publication/merge. Directory indexing, complete ED-M1 and production assets/RSS/GPU acceptance stay
open.

## Complete delivery gate

Final source including supplementary/BMP byte-order cases passes fresh linux-development
configure/build (2 final steps), Minimal Monolithic Shipping configure/build (9 metadata integration
steps on the existing complete Shipping build) and the entire serial Linux CTest suite:293 registered,
281 actual passes,12 existing native feature/environment skips,0 failures,717.71s. Final sort test
passes32.82s; original100k lifecycle passes246.94s at timeout scale1. All five source/CMake hashes
remain unchanged through the gate; full LastTest is archived outside the repository.

Shipping strips Editor by policy; public/private Editor boundaries are validated by Development and
applicable hosted checks. Documentation16/16, local Markdown links/bilingual pairing, touched C++
formatting and diff checks pass before publication. Hosted platforms/Shipping remain separately
reported on the PR and must succeed before squash merge. No global allocation-failure guarantee,
production assets/RSS/GPU budget or complete milestone acceptance is claimed.
