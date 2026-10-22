# Bounded cold Content filter keys

Finite cloud child `nexora-gu7.2.4`, parent M1-02 remains open.

Changing a filename/type filter previously repeated parent-path construction, filename UTF-8
conversion and case conversion for every candidate. The new owner-thread cache stages complete
owning folded names/types for indexed folders of at least 1024 rows. Its 16 MiB logical admission
accounts for object/row storage and string capacities; actual allocator overhead/RSS is separate.
Only complete snapshots publish. Folder/source/reveal/Undo changes revoke interpretation;
artifact publication preserves names while returned rows resolve current owning records.
Cold queries compare the current 256-byte C folding map before reuse. Read-only views, complete
order/selection, drag generation/authority, source records, reimport and mutation history retain
their existing contracts. Allocation, conversion or budget failure retains complete legacy results.
Small folders avoid cache construction; unavailable directory indexing retains full linear behavior.
C++ SDK consumers rebuild. The stable C SDK/capability tables and project formats are unchanged.

## Metadata measurement

One deterministic 100k-record flat folder, no production asset payloads,48 changing queries.
Wall milliseconds include SetFilter plus complete VisibleCount. The 47 post-first samples use
median p50 and sorted floor(0.95*(n-1)) p95. Both drivers use matching compiler flags from the
Development compile database. The baseline uses a frozen original-layout header and pre-change
module whose ContentBrowser source/header match the accepted parent; copied module loads first.
The accelerated process loads the new module. Another bounded build ran during portions of the
pair. These are shared cloud metadata observations, not isolated or production performance gates.

| Phase | Baseline ms | Accelerated ms |
|---|---:|---:|
| Reset |989.412|1003.23|
| Directory/unfiltered |289.686|254.967|
| First filtered query |390.915|286.989|
| Changing filters p50 |401.388|8.69926|
| Changing filters p95 |581.074|11.301|

Every result count is checked against the deterministic fixture expectation. Raw schema 1 CSV:
[baseline](data/ContentSearch-baseline-Linux-2026-10-21.csv),
[accelerated](data/ContentSearch-accelerated-Linux-2026-10-21.csv).
Historical schema 1 rows remain immutable evidence; consumers select their explicit schema.
Reproduce the current measurement with:

```bash
cmake --build --preset linux-development --target NexoraContentSearchBenchmark -j1
build/linux-development/Tests/Editor/NexoraContentSearchBenchmark
```

## Validation status

Focused independent legacy-oracle tests pass (1/1,2.03s): exact count/order/clipping, complete
filter combinations, small folders, owning copies after original teardown, Rename/Move/Delete,
Undo preserving discovery/current reimport, select-all/range, valid/read-only/stale drag,
allocation failures at0/1/1023 staged rows, over-budget types, unavailable directory indexing and
available locale switches. Locale mapping changes depend on installed platform locales.
Test-only fault injection is compiled only when BUILD_TESTING is enabled and retains no callback.
Accepted implementation `86d71a50` records main `3c11efbb`; all production/feature code is
unchanged from private `c183af29` after the history-only accepted-main merge.

```bash
cmake --preset linux-development
cmake --build --preset linux-development -j1
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

The accepted complete gate passes318 registered tests:311 actual passes,7 environment skips,
zero failures,850.21s. All554 frozen compiled-source/header/inl/CMake hashes match afterward.
It selects lavapipe25.0.7 and explicit local-socket access; Scene/PBR/upload/swapchain/material
software-Vulkan cases execute. The earlier restricted-socket run was interrupted before acceptance
because it skipped available GPU cases; its partial output is excluded. Deep85.20s,100k258.23s
and overview retain original scale1/deadlines. These are shared cloud functional observations.

All consumers rebuild after the public C++ model layout change. A fresh Native-disabled,
Shell-disabled SDK with BUILD_TESTING=OFF builds the real135-step EditorCore target, exports
public VisibleCount and excludes ContentSearchKeysTestAccess and the publisher test gate;
zero tests are registered. Minimal Shipping configure/build passes9 integration steps with
Editor/SDK stripped and zero tests. Final acceptance changes documentation only and receives
current-version metadata/key checks and documentation routing before publication.
Production assets/RSS, remaining indexing work, physical acceptance and complete M1 stay open.

## Accepted-main workflow integration

The final candidate integrates accepted main `8e404390` (including PR692's real native asset
workflow). All consumers rebuild (383 steps); current-version/key tests pass2/2 and documentation
routing passes16/16. The exact integration source is frozen across5522 tracked/new files.
The complete gate replay passes318 registered tests,311 actual passes,7 explicit recording skips,
zero failures in834.03s. Persistent pre-hash, CTest, log-copy and post-hash exit statuses are all0.
An earlier complete invocation reported all tests passing in787.36s but its wrapper returned1
without a diagnosed failing component; that invocation is excluded from accepted gate evidence.
The replay records independent exit statuses and JUnit (318 tests,0 failures,7 skipped).

The rebuilt native application additionally passes the actual project-create/import/search/inspect/
move/reimport-failure/retry/Undo/save/read-only-reopen workflow against this integrated source,
including exact identity/sidecar retention, current mesh geometry and unrelated binary source bytes.
This is a software-Vulkan/Xvfb cloud check; physical display, production assets/RSS and full M1
acceptance remain open. Final evidence updates change documentation only.
