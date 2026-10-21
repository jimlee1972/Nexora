# Bounded cold Content filter keys

Finite cloud child `nexora-gu7.2.4`, parent M1-02 remains open.

Changing a filename/type filter previously repeated parent-path construction, filename UTF-8
conversion and case conversion for every candidate. The new owner-thread cache stages complete
owning folded names/types for indexed folders of at least1024 rows. Its16MiB logical admission
accounts for object/row storage and string capacities; actual allocator overhead/RSS is separate.
Only complete snapshots publish. Folder/source/reveal/Undo changes revoke interpretation;
artifact publication preserves names while returned rows resolve current owning records.
Cold queries compare the current256-byte C folding map before reuse. Read-only views, complete
order/selection, drag generation/authority, source records, reimport and mutation history retain
their existing contracts. Allocation, conversion or budget failure retains complete legacy results.
Small folders avoid cache construction; unavailable directory indexing retains full linear behavior.
C++ SDK consumers rebuild. The stable C SDK/capability tables and project formats are unchanged.

## Metadata measurement

One deterministic100k-record flat folder, no production asset payloads,48 changing queries.
Wall milliseconds include SetFilter plus complete VisibleCount. The47 post-first samples use
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

Every result count is checked against the deterministic fixture expectation. Raw schema1 CSV:
[baseline](data/ContentSearch-baseline-Linux-2026-10-21.csv),
[accelerated](data/ContentSearch-accelerated-Linux-2026-10-21.csv).
Historical schema1 rows remain immutable evidence; consumers select their explicit schema.
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
Full Linux, Shipping, fresh no-testing SDK and final documentation checks are pending.
Production assets/RSS, remaining indexing work, physical acceptance and complete M1 stay open.
