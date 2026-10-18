# DX12 VSync admission and retained spike investigation — 2026-10-18

The current-PC screen with unchanged workloads/budgets passes **24/24** rows. This is a physical developer-host
screen, not full 2D-M6 acceptance or a product minimum specification.

## Change and retained uncertainty

Shared DX12 VSync waits for DXGI presentation capacity at Acquire, including the first frame.
One swapchain-owned handle bounds queued frames to the existing two/three frame slots; separate
GPU fences still protect allocators, uploads, timestamps and retirement. Resize retains the flags
and handle, zero extent suspends before waiting, failed waits reject acquisition, and drain closes
the handle once. Immediate retains its asynchronous admission. Public headers and C/Zig ABI are
unchanged by this DX12 change. This affects shared engine consumers, not a benchmark-only branch.
Microsoft documents the [waitable handle contract](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-getframelatencywaitableobject)
and [queue capacity](https://learn.microsoft.com/en-us/windows/win32/api/dxgi1_3/nf-dxgi1_3-idxgiswapchain2-setmaximumframelatency).

The previous [72.3556 ms DX12 sparse failure](../Windows-M6-VulkanPacing-2026-10-18/acceptance.md)
remains retained. Its aggregate CSV cannot identify the same-frame cause, and fixed diagnostic
repeats did not reproduce that event. No scheduler/driver attribution or proof of eliminating its
unique root cause is claimed. Current Windows policy prevents ETW CPU capture; exact error codes
and separate native/stage clocks, QPC values and thread-cycle observations are retained.

Diagnostics do establish queued GPU-fence waits around low-load reloads. A single queued-frame
trial reduced reload waits but regressed 10k mixed/sparse p99, so it was rejected. Both failed
repeats and their source/binary identities remain. The final policy retains CPU/GPU overlap with
queue capacity equal to frame slots. See [same-frame reload comparison](diagnostics/reload-comparison.json)
and complete compressed traces/source snapshots in [the diagnostic manifest](diagnostics/manifest.json).
Diagnostics are separate from the formal benchmark's optional checked-in stage trace below.

## Fixed physical screen

- Same FX-6100, 16 GiB, GTX 960 2 GiB, Windows 10 host; native identity is preserved in every CSV.
- 640x480 windowed VSync; same six cases, including 10,000 mixed and 10,000 sparse objects.
- 600 measured + four warm-up frames; two repeats per backend, ordered DX12/Vulkan/DX12/Vulkan.
- All animation, movement, complete spatial rebuild/query/sweep, extraction/culling/order,
  instanced drawing, full pool recycle and immutable Atlas retirement/reload remain.
- Unchanged predeclared p95/p99 <= 1000/30 ms and maximum <= 2000/30 ms. No failed row is dropped.
- Builds/other Nexora fixtures finish before timing; normal desktop applications remain open.

The benchmark adds optional buffered per-frame clocks; original CPU/render work and aggregate
CSV fields stay intact. Every formal process enables `--frame-trace`, including its extra clocks
inside the original full-frame interval. Per-case samples write after drain, outside that interval.
[Frame summaries](frame-summary.json) retain the same frame's CPU and all eight contiguous stages;
raw CSV has all 604 frames/case, including explicit warm-up flags. Aggregate p95/p99/max are
recomputed from the trace as a consistency check. [Load equivalence](load-equivalence.json)
compares every row's counts, coverage, queries, GPU samples and native identity. The
[advance plan](plan.json), [all process results](summary.json), [binary/CSV hashes](identity.json)
and [source snapshots](source/manifest.json) identify the measured candidate. Process memory
samples are working set/private commit, not total VRAM/allocator accounting or soak evidence.

| Run | Case | p95 ms | p99 ms | Maximum ms | Result |
| --- | --- | ---: | ---: | ---: | --- |
| dx12 1 | 100 mixed | 17.3699 | 17.7401 | 20.3369 | PASS |
| dx12 1 | 1000 mixed | 17.3928 | 18.0578 | 18.8186 | PASS |
| dx12 1 | 5000 mixed | 18.1100 | 19.2038 | 21.0314 | PASS |
| dx12 1 | 10000 mixed | 23.9445 | 27.5683 | 36.5129 | PASS |
| dx12 1 | 1000 overlap | 17.5016 | 18.4603 | 21.6173 | PASS |
| dx12 1 | 10000 sparse | 19.5810 | 23.5412 | 31.1217 | PASS |
| vulkan 1 | 100 mixed | 17.4126 | 17.7480 | 23.5796 | PASS |
| vulkan 1 | 1000 mixed | 17.3404 | 17.9284 | 25.4592 | PASS |
| vulkan 1 | 5000 mixed | 18.2368 | 19.3680 | 20.4513 | PASS |
| vulkan 1 | 10000 mixed | 23.1767 | 26.0826 | 27.4641 | PASS |
| vulkan 1 | 1000 overlap | 17.4110 | 18.2400 | 19.4009 | PASS |
| vulkan 1 | 10000 sparse | 19.1611 | 20.9990 | 41.6129 | PASS |
| dx12 2 | 100 mixed | 17.3882 | 17.7958 | 19.3934 | PASS |
| dx12 2 | 1000 mixed | 17.3858 | 17.7200 | 19.8763 | PASS |
| dx12 2 | 5000 mixed | 17.8164 | 19.5818 | 21.7911 | PASS |
| dx12 2 | 10000 mixed | 23.9527 | 26.7453 | 33.1596 | PASS |
| dx12 2 | 1000 overlap | 17.4742 | 17.9422 | 19.2705 | PASS |
| dx12 2 | 10000 sparse | 19.6277 | 23.1757 | 34.6080 | PASS |
| vulkan 2 | 100 mixed | 17.3633 | 17.8955 | 22.9418 | PASS |
| vulkan 2 | 1000 mixed | 17.5502 | 18.2149 | 20.7311 | PASS |
| vulkan 2 | 5000 mixed | 18.2574 | 19.6174 | 21.2519 | PASS |
| vulkan 2 | 10000 mixed | 24.2327 | 27.0561 | 29.6598 | PASS |
| vulkan 2 | 1000 overlap | 17.5157 | 18.1344 | 24.7840 | PASS |
| vulkan 2 | 10000 sparse | 21.1331 | 26.4405 | 52.2651 | PASS |

## Validation and boundaries

The full local MSVC Development build and canonical Monolithic Shipping benchmark/compiler build
pass. Related CTest is **33/33**, zero failures/skips, including actual DX12 VSync/Immediate,
two/three slots, wait failure, resource drain, resize, suspension and handle cleanup. The same
new regression fails the old adapter on missing first-frame admission, with exit 1 as expected.
Existing DX12/Vulkan native PBR and 2D contracts remain in this gate. The portable trace
test verifies all six cases, warm-up/sample identity, same-frame stage partitions, CPU/full
percentile parity, unchanged ordinary CSV schema and rejection of aliased/invalid output.

Exact commands and output are retained in [validation artifacts](validation/product-build.cmd.gz),
[CTest log](ctest.log), [JUnit](ctest.xml) and [the old-adapter counterexample](baseline-regression.log).
The final PR head's full hosted Linux Development configure/build/CTest, Linux Shipping and other
platform jobs are evaluated on [PR #644](https://github.com/jimlee1972/Nexora/pull/644) before merge.
Physical Windows results do not establish Linux/macOS results, and hosted results do not replace
this physical screen. Public source is base `6d113e80144f18574a1afda440c40922f4a0ac29` plus the retained implementation
patch and exact working-source hashes; no executable or generated build tree is committed.

The original isolated 72.3556 ms cause remains untraced. Product minimum specification,
integrated UI/VFX, allocator/VRAM, long-duration operation and clean-host/device acceptance remain
open. **M6 NOT ACCEPTED; roadmap 9/11 (82%), P0 6/8 unchanged.**
