# Shared 2D path optimization and physical revalidation — 2026-10-18

**Functional: four native processes exit 0; Windows related contracts 20/20 pass. Performance: 21/24 rows meet the unchanged screen. Full M6 remains NOT ACCEPTED.**
Tracked by `nexora-j4q.2.2`; delivery/review is [PR #644](https://github.com/jimlee1972/Nexora/pull/644).

## Shared engine change

`SpatialIndex::Build` still rebuilds every supplied shape every call and publishes atomically.
It now sorts contiguous cell/shape references and uses independent reusable staging vectors,
removing per-shape cell-vector and per-cell tree-node allocations. Shape/cell-reference limits,
conservative bounds, sorted/deduplicated queries, sweep geometry and failure rollback are retained.
The published and staging snapshots retain bounded high-water storage; this trades persistent
scratch memory for less allocation churn. The private C++ class layout changes and requires
consumer rebuilds; public functions and C/Zig wire ABI do not change.

`ExtractSprites` validates IDs in contiguous storage and sorts compact references instead of
moving matrix snapshots during sorting. Already ordered IDs/draw order avoid redundant sorting.
Invisible duplicate IDs still reject before callbacks; matrices/IDs remain snapshotted before
resolver callbacks. Owning CPU geometry, Atlas references, UV/pivot/flips, culling and transparent
order remain available to all existing native and Editor consumers. GPU instancing was already
enabled and is unchanged. This is a shared engine change, with no benchmark-specific branch.

## Frozen scope and unchanged workload

Same physical FX-6100/GTX 960 2 GiB/16 GiB Windows 10 19045 developer PC; NVIDIA 32.0.15.8180.
Native x64 MSVC Monolithic Shipping, 640×480 requested VSync, 1920×1080/60 Hz desktop.
DX12/Vulkan enabled; graphical Editor/Slang/mimalloc off. [Host](host.json), [build log](build.log).
Development complete build and the final incremental runtime-contract target both succeeded.
Shipping benchmark/compiler rebuilt successfully. No builds or other Nexora fixtures ran during timing;
normal desktop applications remained open. Hardware inventory was captured before timing.

The [advance plan](plan.json) retains the exact previous budgets: p95/p99 ≤ 1000/30 ms,
maximum ≤ 2000/30 ms. Two repeats per backend, six cases each, four warm-up plus 600 measured
frames per case. Simulation step remains 1/60 second. No count, resolution, workload, timing limit
or backend selection was changed. Animation/movement, complete index rebuild/query/sweep, sprite
extraction/sorting/draw and midpoint full pool recycle/atlas retirement/reload remain enabled.

[Load equivalence](load-equivalence.json) compares all 24 rows to the earlier
[30 FPS baseline](../Windows-M6-30FPS-2026-10-18/acceptance.md): visible/culled counts, batches,
AABB coverage, query hits, sample counts and device/backend identity match exactly.
`Tools/TwoD/Benchmark.cpp` is unchanged. The earlier baseline and all failing results are retained.

## Timing results

DX12 passes 6/6 and 6/6; Vulkan passes 4/6 and 5/6.
Each process exits 0, identifies NVIDIA vendor 4318/device 5121, and completes native teardown.
Each case has 600 measured frames and 598 completed GPU samples. Exit 0 is not a timing PASS.

| Run | Workload | CPU p95 ms | GPU p95 ms | Frame p95 ms | Frame p99 ms | Frame max ms | Budget |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [dx12-1](dx12-1.csv) | 100 mixed | 0.408 | 0.477 | 17.564 | 18.007 | 36.041 | PASS |
| [dx12-1](dx12-1.csv) | 1000 mixed | 1.718 | 0.500 | 17.578 | 18.409 | 35.136 | PASS |
| [dx12-1](dx12-1.csv) | 5000 mixed | 8.947 | 0.583 | 18.075 | 19.492 | 35.282 | PASS |
| [dx12-1](dx12-1.csv) | 10000 mixed | 17.855 | 0.562 | 24.099 | 26.794 | 28.065 | PASS |
| [dx12-1](dx12-1.csv) | 1000 overlap | 2.808 | 0.759 | 17.688 | 18.521 | 34.927 | PASS |
| [dx12-1](dx12-1.csv) | 10000 sparse | 18.868 | 0.493 | 19.791 | 24.234 | 35.282 | PASS |
| [vulkan-1](vulkan-1.csv) | 100 mixed | 0.348 | 0.077 | 33.276 | 33.761 | 35.732 | FAIL |
| [vulkan-1](vulkan-1.csv) | 1000 mixed | 1.802 | 0.090 | 31.924 | 32.461 | 35.629 | PASS |
| [vulkan-1](vulkan-1.csv) | 5000 mixed | 10.129 | 0.157 | 17.951 | 19.270 | 34.998 | PASS |
| [vulkan-1](vulkan-1.csv) | 10000 mixed | 18.443 | 0.285 | 24.378 | 27.567 | 31.570 | PASS |
| [vulkan-1](vulkan-1.csv) | 1000 overlap | 2.716 | 0.296 | 31.154 | 31.958 | 35.656 | PASS |
| [vulkan-1](vulkan-1.csv) | 10000 sparse | 21.189 | 0.096 | 21.711 | 35.826 | 46.578 | FAIL |
| [dx12-2](dx12-2.csv) | 100 mixed | 0.376 | 0.491 | 17.544 | 18.069 | 34.789 | PASS |
| [dx12-2](dx12-2.csv) | 1000 mixed | 1.781 | 0.514 | 17.631 | 18.214 | 35.066 | PASS |
| [dx12-2](dx12-2.csv) | 5000 mixed | 8.799 | 0.577 | 18.302 | 19.910 | 35.224 | PASS |
| [dx12-2](dx12-2.csv) | 10000 mixed | 18.706 | 0.555 | 25.556 | 27.648 | 35.246 | PASS |
| [dx12-2](dx12-2.csv) | 1000 overlap | 2.823 | 0.856 | 17.570 | 18.424 | 34.620 | PASS |
| [dx12-2](dx12-2.csv) | 10000 sparse | 19.280 | 0.490 | 20.404 | 22.645 | 35.690 | PASS |
| [vulkan-2](vulkan-2.csv) | 100 mixed | 0.372 | 0.076 | 33.280 | 33.702 | 35.900 | FAIL |
| [vulkan-2](vulkan-2.csv) | 1000 mixed | 1.816 | 0.089 | 31.758 | 32.414 | 35.441 | PASS |
| [vulkan-2](vulkan-2.csv) | 5000 mixed | 9.383 | 0.146 | 18.036 | 19.758 | 34.692 | PASS |
| [vulkan-2](vulkan-2.csv) | 10000 mixed | 18.589 | 0.286 | 24.986 | 28.340 | 36.041 | PASS |
| [vulkan-2](vulkan-2.csv) | 1000 overlap | 2.702 | 0.293 | 31.146 | 31.593 | 35.215 | PASS |
| [vulkan-2](vulkan-2.csv) | 10000 sparse | 19.332 | 0.080 | 20.038 | 23.084 | 26.677 | PASS |

The table below shows the range of **two per-run percentiles**, not a pooled percentile or
statistical confidence interval. These are normal-desktop observations, not a controlled lab study.

| Backend / 10k case | Baseline CPU p95 range ms | New CPU p95 range ms | Baseline frame p99 range ms | New frame p99 range ms |
| --- | --- | --- | --- | --- |
| dx12 / mixed | 26.262–29.472 | 17.855–18.706 | 39.954–40.079 | 26.794–27.648 |
| dx12 / sparse | 35.291–45.750 | 18.868–19.280 | 47.295–60.557 | 22.645–24.234 |
| vulkan / mixed | 24.919–25.038 | 18.443–18.589 | 35.290–35.672 | 27.567–28.340 |
| vulkan / sparse | 37.800–47.018 | 19.332–21.189 | 44.068–56.893 | 23.084–35.826 |

CPU costs improve without reducing the workload. The remaining failing rows are listed above
without relaxing the screen. Whole-frame intervals include acquire/draw/present; aggregates do
not isolate individual hitches or prove a presentation/driver root cause. GPU timing measures
command buffers, not display latency; independent stage percentiles must not be summed.

## Memory and provenance

[Summary](summary.json) retains commands, exit codes, durations and each budget decision.
`GetProcessMemoryInfo` is sampled approximately every 100 ms; [DX12 1](dx12-1-memory.json),
[DX12 2](dx12-2-memory.json), [Vulkan 1](vulkan-1-memory.json), [Vulkan 2](vulkan-2-memory.json).
These are whole-process working-set/private-commit observations, not engine allocator or VRAM
accounting. Built-in Windows CSV RSS zero means unavailable. Short runs do not establish a leak/soak result.

[Identity](identity.json) records unchanged executable hash before/after and hashes of the plan/CSVs.
The plan identifies the prior committed base plus exact working-tree source-byte hashes.
[Implementation patch](implementation.patch) reconstructs that implementation from the source base;
a reverse-apply check and all five recorded source hashes passed after measurement.
The unchanged benchmark stdout still mentions historical 60 FPS acceptance NOT RUN; the plan
and this report provide the revised 30 FPS interpretation without rewriting raw output.

## Verification and reproduction

[Windows CTest log](ctest.log), [JUnit](ctest.xml) and [final incremental build/test log](test-build.log):
**20/20 PASS, no skipped/failed tests**, including the new independent repeated-grid/analytic-sweep
oracles, failed-build rollback and sorted extraction/snapshot invariants. Existing bridge, Play,
assets, UI/content, Zig layout, host storage/export/font/backend/manifest contracts also pass.

```powershell
cmake --build build/windows-development --config Development --parallel 2
cmake --build build/windows-development --config Development --target NexoraTwoDTests --parallel 2
ctest --preset windows-development -C Development -R "^twod\.|^gameplay\..*(twod|ui|content)" --output-on-failure
cmake --build build/windows-showcase-shipping --config Shipping --target NexoraTwoDBenchmark NexoraTwoDAssetCompiler --parallel 2
build/windows-showcase-shipping/bin/Shipping/NexoraTwoDBenchmark.exe --native --backend dx12 --frames 600 --report <fresh-dx12.csv>
build/windows-showcase-shipping/bin/Shipping/NexoraTwoDBenchmark.exe --native --backend vulkan --frames 600 --report <fresh-vulkan.csv>
```

Repeat native commands sequentially a second time. [Helper snapshot](run_acceptance.py) contains
the exact memory sampling and screen checks. It ran from `build/windows-2d-opt-20261018/`;
this copy is archival. Copy it back to that build directory and choose a new `OUT` before rerunning.
Full Linux Development/Shipping and native pixel checks are tracked separately on the final PR head;
their results are not inferred from Windows build success. No Linux gate ran locally on this Windows host.
`python Tools/Build/TestDocumentationCI.py` passes 16/16 with the existing pinned parser on
`PYTHONPATH=.tools/documentation-python`; changed Markdown links/bilingual pairs and
`git diff --check` pass. Roadmap review finds only the bilingual 2D roadmap affected by this
supporting performance change. The concise root README and overall milestone summary are unchanged.

## Remaining milestone boundary

Full **M6 NOT ACCEPTED; 9/11 (82%), P0 6/8 unchanged**. This is the existing developer-PC
synthetic workload, not a declared product minimum specification or integrated UI/VFX budget.
Per-frame trace, allocator/VRAM accounting, integrated product load and sustained acceptance remain open.
M7 clean-host/controller/audio/DPI/product gates were not run.
