# Vulkan FIFO pacing and physical 2D revalidation - 2026-10-18

**Physical screening: 23/24 rows meet the unchanged limits. Functional: all four native processes exit 0; 31/31 related local contracts pass. Full M6 remains NOT ACCEPTED.**
Tracked by `nexora-j4q.2.3`; delivery/review is [PR #644](https://github.com/jimlee1972/Nexora/pull/644).

## Shared engine correction

Vulkan FIFO now waits for the acquired image to become available before admitting CPU frame work. One reusable swapchain-owned fence provides this host boundary; the existing acquire semaphore remains in the GPU submission dependency. This removes queued CPU bursts behind future WSI availability after a resource drain. Immediate presentation keeps asynchronous acquisition. No sleeps, timer-resolution changes or benchmark-specific behavior are introduced. CPU run-ahead is reduced in FIFO; input-to-display latency and arbitrary GPU-heavy throughput are not measured here.

Presentation wait semaphores are now owned per swapchain image rather than per upload slot. Reacquiring the image governs reuse; a submit fence alone does not prove a presentation wait has retired. Frame buffers honor the descriptor's two/three slots, capped by actual images. Default two-slot/two-image behavior on this GTX 960 is retained. Pending extent starts from the descriptor, preserving initial SUBOPTIMAL recovery before any resize event. Partial allocations, recovery and final drain release both synchronization domains. Public C++ layout and C/Zig ABI are unchanged by this Presentation correction.

See the [Presentation contract](../../../Presentation/README.md) and [Khronos semaphore rule](https://docs.vulkan.org/guide/latest/swapchain_semaphore_reuse.html). The earlier shared grid/extraction optimization remains present and unchanged.

## Frozen same-host screen

Physical AMD FX-6100, 16 GiB RAM, GTX 960 2 GiB, Windows 10 19045, NVIDIA 32.0.15.8180; [fresh inventory](host.json). x64 MSVC Monolithic Shipping; 640x480 client on a 1920x1080/60 Hz desktop, requested FIFO VSync. Two repeats per DX12/Vulkan backend, all six cases, four warm-up plus 600 measured frames, fixed simulation step 1/60.

The [advance plan](plan.json) retains p95/p99 <= 1000/30 ms and maximum <= 2000/30 ms. `Tools/TwoD/Benchmark.cpp` is byte-identical to the preceding same-host screen. Animation, movement, all-object spatial rebuild/query/sweep, extraction/culling/sorting, instanced draws, and midpoint full pool recycle/immutable atlas retirement/reload remain active. No builds or other Nexora native fixtures ran during measured acceptance; ordinary desktop applications remained open.

[All 24 rows match the preceding load](load-equivalence.json): visible/culled counts, batches, AABB coverage, query hits, completed GPU samples, frames and observed hardware identity. Every case retains 598 completed GPU samples, three texture uploads and normal native teardown. Instrumented/render-only diagnostics are separate from this unmodified benchmark screen.

## Formal results

| Run | Workload | CPU p95 ms | GPU p95 ms | Frame p95 ms | Frame p99 ms | Frame max ms | Budget |
| --- | --- | --- | --- | --- | --- | --- | --- |
| [dx12-1](dx12-1.csv) | 100 mixed | 0.371 | 0.646 | 17.458 | 18.117 | 34.391 | PASS |
| [dx12-1](dx12-1.csv) | 1000 mixed | 1.817 | 0.664 | 17.660 | 18.314 | 35.933 | PASS |
| [dx12-1](dx12-1.csv) | 5000 mixed | 9.420 | 0.693 | 17.901 | 18.962 | 35.293 | PASS |
| [dx12-1](dx12-1.csv) | 10000 mixed | 15.652 | 0.478 | 21.319 | 24.917 | 30.346 | PASS |
| [dx12-1](dx12-1.csv) | 1000 overlap | 2.885 | 0.941 | 17.838 | 18.819 | 34.526 | PASS |
| [dx12-1](dx12-1.csv) | 10000 sparse | 17.495 | 0.623 | 19.037 | 20.602 | 36.284 | PASS |
| [vulkan-1](vulkan-1.csv) | 100 mixed | 0.380 | 0.067 | 17.352 | 17.739 | 26.050 | PASS |
| [vulkan-1](vulkan-1.csv) | 1000 mixed | 2.090 | 0.077 | 17.653 | 22.465 | 37.580 | PASS |
| [vulkan-1](vulkan-1.csv) | 5000 mixed | 11.230 | 0.138 | 18.823 | 30.762 | 41.279 | PASS |
| [vulkan-1](vulkan-1.csv) | 10000 mixed | 17.872 | 0.285 | 23.973 | 27.688 | 33.859 | PASS |
| [vulkan-1](vulkan-1.csv) | 1000 overlap | 2.831 | 0.337 | 17.449 | 18.315 | 19.645 | PASS |
| [vulkan-1](vulkan-1.csv) | 10000 sparse | 18.402 | 0.073 | 19.152 | 21.412 | 29.530 | PASS |
| [dx12-2](dx12-2.csv) | 100 mixed | 0.346 | 0.642 | 17.617 | 18.154 | 36.018 | PASS |
| [dx12-2](dx12-2.csv) | 1000 mixed | 1.888 | 0.671 | 17.629 | 18.461 | 35.914 | PASS |
| [dx12-2](dx12-2.csv) | 5000 mixed | 8.987 | 0.704 | 17.942 | 19.385 | 34.554 | PASS |
| [dx12-2](dx12-2.csv) | 10000 mixed | 17.479 | 0.530 | 23.450 | 26.471 | 30.645 | PASS |
| [dx12-2](dx12-2.csv) | 1000 overlap | 2.779 | 0.921 | 17.832 | 18.801 | 35.093 | PASS |
| [dx12-2](dx12-2.csv) | 10000 sparse | 17.704 | 0.657 | 19.024 | 21.546 | 72.356 | FAIL |
| [vulkan-2](vulkan-2.csv) | 100 mixed | 0.414 | 0.076 | 17.319 | 17.529 | 18.380 | PASS |
| [vulkan-2](vulkan-2.csv) | 1000 mixed | 1.762 | 0.080 | 17.382 | 17.791 | 19.850 | PASS |
| [vulkan-2](vulkan-2.csv) | 5000 mixed | 9.538 | 0.136 | 18.093 | 19.296 | 30.426 | PASS |
| [vulkan-2](vulkan-2.csv) | 10000 mixed | 18.535 | 0.283 | 23.905 | 25.791 | 27.114 | PASS |
| [vulkan-2](vulkan-2.csv) | 1000 overlap | 3.084 | 0.291 | 17.580 | 18.186 | 20.007 | PASS |
| [vulkan-2](vulkan-2.csv) | 10000 sparse | 18.393 | 0.072 | 19.047 | 20.273 | 24.911 | PASS |

The prior three failed p99 rows are retained in the [preceding report](../Windows-M6-SharedPath-2026-10-18/acceptance.md). The complete new run, including any failures, is in [summary.json](summary.json).

Vulkan now passes all six cases in both repeats. DX12 passes 6/6 then 5/6: the second 10k sparse row has p99 21.5456 ms but maximum 72.3556 ms, above the original 66.6667 ms spike limit. Its aggregate reload is 19.4447 ms and CPU maximum 28.4105 ms, which cannot identify the stage or frame causing the whole-frame maximum. This single failed row remains a failure and prevents all-backend acceptance/merge; no threshold was relaxed.

## Diagnostic evidence and limits

Gzip traces in [diagnostics](diagnostics/manifest.json) preserve 604 frames per case (including warm-up) for every retained experiment. TRACE columns: objects, sparse, overlap, frame, reload, BeginFrame, populate, index, query/sweep, extraction, draw, EndFrame, vkWaitForFences, acquire wrapper, vkQueueSubmit, vkQueuePresentKHR, image index, actual images, requested images, native present-mode value. RENDER columns: objects, sparse, overlap, frame, BeginFrame, draw, EndFrame, fence wait, acquire, submit, present, images, mode. The prototype acquire-wrapper interval includes its extra host wait and transient fence allocation/cleanup. All intervals are milliseconds. Timings are same-frame observations; independent stage percentiles must not be summed.

`diagnostic-before`: original 100-object p99 is about 33.7 ms, with alternating approximately 32-33 ms submit-fence waits after midpoint resource drain. Merely increasing image/slot count to three increases those tails toward 50 ms. Render-only, which deliberately omits CPU updates and reload, stays around 18 ms p99. These diagnostics establish a presentation/queue-pacing interaction; they do not prove every historical sparse outlier has the same cause.

`diagnostic-sync`: image semaphore ownership and extent/slot corrections pass native contracts, but alone do not remove the timing tail. `diagnostic-drain`: a diagnostic-only single image-readiness wait after a drain is insufficient. `diagnostic-image-ready`: waiting for readiness on each acquire brings the two-image 100-object p99 to about 17.7 ms, 10k mixed to 27.1 ms, and 10k sparse to 22.5 ms. Three images are unnecessary for this correction. The formal results above use the production reusable-fence implementation, not the diagnostic prototype that temporarily creates a fence per acquire.

`diagnostic-dx12-tail`: two fixed additional instrumented 600-frame mixed runs did not reproduce the 72 ms maximum. Sparse maxima are 33.7362/35.7843 ms, both at midpoint reload; all six stage-profile cases stay below the original limits in both runs. Vulkan API fields are zero for these DX12 runs. These diagnostic repeats neither replace the failed formal result nor establish that its cause is external scheduling. Capturing that rare tail with same-frame/native scheduling evidence remains open.

Source/EXE hashes are recorded in each diagnostic identity. Gzip source snapshots preserve exact code bytes. [source-map.json](diagnostics/source-map.json) verifies every recorded source hash, including Windows newline conversion where necessary. The recorded wrapper paths refer to their original ignored build locations: `VulkanProfile.base.cpp.gz` corresponds to before/sync, `VulkanProfile.probe.cpp.gz` to drain/image-ready, and `BenchmarkProfile.probe.cpp.gz` to all experiments. `VulkanSurface.before.cpp.gz` is Git LF source; `VulkanSurface.sync.cpp.gz` retains the intermediate working source bytes. Decompress to the original ignored paths and recreate temporary targets from the archived prepare script only in a disposable checkout/build directory; no diagnostic target is delivered in product CMake.

## Validation and provenance

- Full MSVC Development build: [log](development-build.log). Related CTest 31/31, no failures/skips: [log](ctest.log), [JUnit](ctest.xml).
- MSVC Monolithic Shipping benchmark/compiler and private adapter regression: [build](shipping-build.log), [native result](shipping-sync-regression.log). Injected allocation-error messages are expected; the process exits 0.
- An early ownership-only regression version fails before the semaphore correction: [retained diagnostic](early-sync-regression-before.log). Final native tests additionally verify FIFO readiness, Immediate opt-out, bounded fences, two/three slots, drain/recovery, zero extent, acquire failure and partial allocation cleanup.
- Documentation tooling: [16/16 tests](documentation-tests.log). Full Linux configure/build/CTest and remaining hosted gates are recorded by the final PR CI; Windows evidence must not be relabeled Linux.

Exact local commands:

```text
cmake -S . -B build/windows-development -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_VULKAN_LIBRARY=G:/proj/Nexora/.tools/vulkan-loader-local/vulkan-1.lib
cmake --build build/windows-development --config Development --parallel 2
ctest --preset windows-development -C Development -R "^twod\.|^gameplay\..*(twod|ui|content)|^window_presentation\." --output-on-failure
cmake --build build/windows-showcase-shipping --config Shipping --target NexoraTwoDBenchmark NexoraTwoDAssetCompiler --parallel 2
NexoraVulkanSwapchainTests.exe
python build/windows-vulkan-tail-20261018/run_acceptance.py
```

[Executable identity](identity.json) records SHA-256 before/after every formal run. The source base plus [implementation.patch](implementation.patch) and per-file plan hashes identify the exact measured implementation. The helper snapshot assumes it is copied back under `build/windows-vulkan-tail-20261018/` and requires a fresh output directory; it refuses to replace an existing run. Generated build trees and executables remain untracked.

External 100 ms memory samples are process working set/private commit only; they do not measure engine allocations or VRAM. A 600-frame screen does not establish a sustained soak, production UI/VFX integration, 1080p performance or a declared product minimum specification. Full M6 stays NOT ACCEPTED, 9/11 (82%), P0 6/8 unchanged. M7 gates are not exercised here.
