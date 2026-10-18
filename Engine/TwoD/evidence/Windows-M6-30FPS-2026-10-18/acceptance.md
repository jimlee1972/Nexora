# Physical Windows 2D M6 30 FPS revalidation — 2026-10-18

**Performance: NOT ACCEPTED. Functional completion: all four processes exit 0.**
Tracked by `nexora-j4q.8`; source `9e3adfdd73a52286690d8018fa2ba8970f5138ad` (PR #643).
This is a fresh physical-PC measurement against the revised 30 FPS target, not a reinterpretation
of the older 60 FPS screen. No runtime, renderer, benchmark implementation or workload was changed.

## Frozen configuration and advance declaration

AMD FX-6100, 16 GiB RAM, GTX 960 2 GiB, NVIDIA 32.0.15.8180, Windows 10 x64 19045.
Desktop 1920×1080 at 60 Hz; benchmark viewport 640×480, requested VSync. MSVC 19.44 x64
Monolithic Shipping, native DX12/Vulkan enabled, Slang/mimalloc/graphical Editor disabled.
The existing VS cache was regenerated and both `NexoraTwoDBenchmark` and
`NexoraTwoDAssetCompiler` rebuilt successfully; see [build log](build.log) and [host configuration](host.json).

[Plan](plan.json) was written before the first native process: two sequential repeats per backend,
six unchanged cases each, four warm-up plus 600 measured frames per case. The new screen requires
**frame p95 and p99 ≤ 1000/30 ms, maximum ≤ 2000/30 ms** (33.3333/66.6667 ms).
The maximum permits one additional 30 FPS interval. These are predeclared screening limits,
not a retroactively approved product spike policy. Every case in both repeats must pass.
The simulation remains fixed at 1/60 second per update; the render performance target is 30 FPS.
Each case updates animation/movement, rebuilds/query/sweeps the spatial index, extracts/culls/sorts
sprites, submits GPU instances, and recycles the complete pool/retires and reloads its atlas halfway.
No build or other Nexora fixture ran during timing; normal desktop applications remained open.
A one-time hardware inventory was captured during the first run, so this is a desktop screen, not an isolated lab run.

## Native results

Both backends report NVIDIA vendor 4318/device 5121 (GTX 960); no software fallback is accepted.
Each row has 600 measured frames and 598 completed GPU samples. All native lifecycle assertions
and ordered surface teardown passed. Exit 0 proves functional completion, not the timing budget.

| Backend / repeat | Workload | CPU p95 ms | GPU p95 ms | Frame p95 ms | Frame p99 ms | Frame max ms | Budget |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [dx12-1](dx12-1.csv) | 100 mixed | 0.549 | 0.403 | 17.637 | 17.999 | 34.732 | PASS |
| [dx12-1](dx12-1.csv) | 1000 mixed | 3.116 | 0.441 | 18.398 | 27.197 | 47.790 | PASS |
| [dx12-1](dx12-1.csv) | 5000 mixed | 16.115 | 0.542 | 21.493 | 31.844 | 44.423 | PASS |
| [dx12-1](dx12-1.csv) | 10000 mixed | 29.472 | 0.634 | 36.309 | 40.079 | 50.825 | FAIL |
| [dx12-1](dx12-1.csv) | 1000 overlap | 2.951 | 0.678 | 17.704 | 18.439 | 34.658 | PASS |
| [dx12-1](dx12-1.csv) | 10000 sparse | 45.750 | 0.288 | 46.447 | 60.557 | 72.036 | FAIL |
| [vulkan-1](vulkan-1.csv) | 100 mixed | 0.542 | 0.088 | 33.104 | 33.589 | 35.250 | FAIL |
| [vulkan-1](vulkan-1.csv) | 1000 mixed | 2.878 | 0.112 | 31.360 | 32.176 | 35.324 | PASS |
| [vulkan-1](vulkan-1.csv) | 5000 mixed | 12.899 | 0.167 | 18.402 | 20.598 | 28.345 | PASS |
| [vulkan-1](vulkan-1.csv) | 10000 mixed | 25.038 | 0.342 | 30.533 | 35.672 | 45.670 | FAIL |
| [vulkan-1](vulkan-1.csv) | 1000 overlap | 3.298 | 0.403 | 31.018 | 31.932 | 44.349 | PASS |
| [vulkan-1](vulkan-1.csv) | 10000 sparse | 47.018 | 0.141 | 47.530 | 56.893 | 82.289 | FAIL |
| [dx12-2](dx12-2.csv) | 100 mixed | 0.532 | 0.401 | 17.422 | 17.632 | 35.075 | PASS |
| [dx12-2](dx12-2.csv) | 1000 mixed | 2.456 | 0.452 | 17.788 | 18.512 | 35.201 | PASS |
| [dx12-2](dx12-2.csv) | 5000 mixed | 12.205 | 0.532 | 18.559 | 21.045 | 34.006 | PASS |
| [dx12-2](dx12-2.csv) | 10000 mixed | 26.262 | 0.618 | 32.557 | 39.954 | 44.906 | FAIL |
| [dx12-2](dx12-2.csv) | 1000 overlap | 2.885 | 0.714 | 17.571 | 18.544 | 34.357 | PASS |
| [dx12-2](dx12-2.csv) | 10000 sparse | 35.291 | 0.295 | 36.450 | 47.295 | 55.684 | FAIL |
| [vulkan-2](vulkan-2.csv) | 100 mixed | 0.479 | 0.095 | 33.105 | 33.468 | 37.552 | FAIL |
| [vulkan-2](vulkan-2.csv) | 1000 mixed | 2.238 | 0.109 | 31.684 | 32.209 | 35.790 | PASS |
| [vulkan-2](vulkan-2.csv) | 5000 mixed | 12.166 | 0.159 | 18.339 | 20.500 | 34.903 | PASS |
| [vulkan-2](vulkan-2.csv) | 10000 mixed | 24.919 | 0.408 | 30.150 | 35.290 | 43.573 | FAIL |
| [vulkan-2](vulkan-2.csv) | 1000 overlap | 2.929 | 0.321 | 30.950 | 31.801 | 34.680 | PASS |
| [vulkan-2](vulkan-2.csv) | 10000 sparse | 37.800 | 0.140 | 38.393 | 44.068 | 47.129 | FAIL |

[Machine-readable checks](summary.json) retain all 24 rows, command lines, exit codes and durations.
The 10k mixed and 10k sparse cases fail in both repeats on both backends. Low GPU intervals
alongside high CPU update/query/extraction intervals are consistent with the prior CPU-cost diagnosis;
these aggregates do not isolate a new root cause or establish which optimization will fix it.
Whole-frame time includes acquisition/presentation waits. Independent CPU/GPU percentiles must not be summed.

## Memory and identity

External `GetProcessMemoryInfo` sampling at approximately 100 ms records process working set
and private committed bytes. These are process observations, not engine allocator or VRAM accounting.
Windows CSV `peak_process_rss_bytes=0` means unavailable. Peak memory is not a leak/soak result.

| Run | Memory samples | Peak working set bytes | Peak private commit bytes |
| --- | --- | --- | --- |
| [dx12-1](dx12-1-memory.json) | 939 | 87490560 | 108404736 |
| [vulkan-1](vulkan-1-memory.json) | 826 | 111464448 | 158904320 |
| [dx12-2](dx12-2-memory.json) | 888 | 88952832 | 109588480 |
| [vulkan-2](vulkan-2-memory.json) | 810 | 110395392 | 158736384 |

[Identity](identity.json) retains SHA-256 for the executable before/after, the advance plan and all CSVs.
The benchmark still prints the historical literal `Physical minimum-PC 60 FPS acceptance: NOT RUN`.
That unchanged stdout is preserved; it is not the revised threshold or a PASS declaration.
This report and the predeclared plan provide the actual 30 FPS screen interpretation.

## Reproduction and validation

From the repository root in the VS 2022 x64 developer shell:

```powershell
cmake --build build/windows-showcase-shipping --config Shipping --target NexoraTwoDBenchmark NexoraTwoDAssetCompiler --parallel 2
build/windows-showcase-shipping/bin/Shipping/NexoraTwoDBenchmark.exe --native --backend dx12 --frames 600 --report <fresh-dx12.csv>
build/windows-showcase-shipping/bin/Shipping/NexoraTwoDBenchmark.exe --native --backend vulkan --frames 600 --report <fresh-vulkan.csv>
```

Repeat the two native commands sequentially a second time, using new output names.
[Measurement helper snapshot](run_acceptance.py) records the exact sampling/check logic used.
It was run from `build/windows-2d-m6-30fps-20261018/run_acceptance.py`; this evidence copy is archival.
To rerun it, copy it to that original build location and choose a fresh `OUT` directory first.
It refuses an existing output directory and never relaxes or hides over-budget rows.
Full Linux implementation gates were not run in this Windows session: this change adds evidence
and bilingual status only. No engine implementation or linkage boundary changed.
`python Tools/Build/TestDocumentationCI.py` passes 16/16 using the existing pinned Markdown
parser on `PYTHONPATH=.tools/documentation-python`; changed Markdown links/bilingual pairs
and `git diff --check` pass. All roadmap documents were reviewed for affected functionality;
only the bilingual 2D roadmap needs this evidence addition. The concise root README is unchanged.

## Acceptance boundary

**M6 remains NOT ACCEPTED; 9/11 (82%), P0 6/8 remain unchanged.**
The 30 FPS target change alone does not accept the current PC. Product minimum specification,
integrated UI/VFX/material/atlas workload, sustained operation, allocator/VRAM budgets and raw
per-frame traces remain open. The unmodified benchmark emits aggregate CSV, not a per-frame trace.
No reduced object count is promoted as full M6 acceptance. Previous 60 FPS evidence remains intact.
M7 clean-host/device/product gates were not exercised in this run.
