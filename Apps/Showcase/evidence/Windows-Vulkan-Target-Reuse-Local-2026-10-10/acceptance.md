# Windows Vulkan completed-slot target reuse

The GTX 960 Standard diagnostic before this change measured 26.85 ms wall, 17.87 ms process CPU
and 9.24 ms completed GPU time. Vulkan destroyed and rebuilt scene/depth/shadow/reflection/refraction
targets each frame. Compatible offscreen targets now stay in their existing protecting frame slot
and are reused only after its fence completes. Dimensions, HDR format, shadow resolution and
reflection/refraction requirements must match. Mismatches/direct draws/resize/teardown release
through the existing paths. Material/tone bindings remain transient; every requested draw, clear,
opaque snapshot and descriptor/geometry check is retained. Allocation remains bounded to three slots.

The shared 105-frame native PBR pixel fixture now also runs on Windows Vulkan when native/Vulkan
options are enabled. Its Lambert expected color uses the actual adapter; Windows UI retains its
UNORM byte-color expectation. The Vulkan desktop test runs serially to prevent capture occlusion.
Both native fixtures pass with all original pixel thresholds and counters: 105 scene draws,
95 composites, 11 shadow passes and 32 shadow instances.

## Full Windows validation

Full configure/build/CTest with both native backends passes **123/123, 213.17 seconds**, without
skips or timeout changes. One earlier run concurrent with background Git synchronization retained
three timeouts (the 300-second parser test and two 30-second memory JSON child runs); that complete
failed log is preserved separately. After synchronization ended, the full gate passed under the
same limits. A host CPU observation during the contended run reported 100% utilization and active
Defender/Git work; it is not a timestamp-aligned proof of the earlier Showcase outlier's cause.

```powershell
cmake --preset windows-showcase-development -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_VULKAN_LIBRARY=G:/proj/Nexora/.tools/vulkan-loader-local/vulkan-1.lib
cmake --build --preset windows-showcase-development --config Development --parallel 4
ctest --preset windows-showcase-development --parallel 4 --output-on-failure
cmake --preset windows-showcase-vulkan-shipping -G "Visual Studio 17 2022" -A x64 -DNEXORA_VULKAN_LIBRARY=G:/proj/Nexora/.tools/vulkan-loader-local/vulkan-1.lib
cmake --build --preset windows-showcase-vulkan-shipping --config Shipping --target NexoraShowcasePackageShippingEvidence --parallel 4
```

Shipping package and isolated-headless verification pass. Executable SHA-256 is
`8e8c2917fe11ffc11544393bdf49ff2764c37671f2411cc99ddf0849f2ae880c`, over base `5f59f93446af`
plus the source hashes retained in `source-provenance.json`. It is separate from the committed
bb0 art matrix and the earlier 91e6 diagnostic binary. Exact committed target-reuse CI remains
pending. The preceding diagnostic commit 5f59f934 passes all 18 hosted Build 2074 jobs, including
full Linux Development configure/build/CTest and Linux Shipping/build-contract/sanitizers;
[that run](https://github.com/jimlee1972/Nexora/actions/runs/37963907498) does not cover this source
patch. No Linux Codex Cloud run is claimed.

## Complete native quality matrix

All 18 reports validate: three separate 360-frame processes per backend/tier, 60 warmup and
300 measured frames each, 1280x720, native GTX 960, paused/activated, static Zig, immediate
presentation, GPU timing disabled and no fallback/software renderer. No compiler, CTest, recorder
or Beads synchronization overlaps this matrix. All original reports are retained.

| Quality | DX12 FPS, all three runs | Vulkan FPS, all three runs |
| --- | --- | --- |
| Basic | 95.88 / 104.11 / 104.61 | 173.39 / 169.78 / 174.00 |
| Standard | 60.56 / 60.07 / 59.87 | 126.19 / 126.15 / 125.99 |
| High | 59.44 / 59.17 / 59.71 | 123.91 / 123.30 / 123.92 |

Standard p99: DX12 17.96 / 19.15 / 19.96 ms; Vulkan 9.50 / 8.96 / 10.17 ms. Vulkan improves from
37.51–40.11 FPS at committed bb0 to 125.99–126.19 FPS with unchanged art/geometry/effect budgets.
This supports the target-allocation diagnosis. DX12 retains its prior allocation path and roughly
60 FPS throughput; its p99 still exceeds the provisional 16.7 ms target. All benchmark records
continue reporting `hardware_budget_accepted: false`; final overall budget and concept art remain open.

## Longer completed GPU comparison

Each default/opt-in comparison runs 1,200 real native frames, producing 1,140 wall/CPU samples
and 1,138 unique GPU samples when enabled. The native device/source and null default/headless
behavior are validated. These are separate diagnostic runs, not default benchmark certification.

| Backend / GPU opt-in | FPS | Wall mean ms | Process CPU mean ms | GPU mean / p99 ms | Wall p99 ms |
| --- | --- | --- | --- | --- | --- |
| DX12 / off | 57.53 | 17.38 | 10.44 | unavailable | 20.86 |
| DX12 / on | 60.79 | 16.45 | 9.20 | 7.58 / 7.86 | 18.68 |
| Vulkan / off | 125.12 | 7.99 | 10.06 | unavailable | 9.68 |
| Vulkan / on | 125.07 | 8.00 | 6.28 | 7.88 / 8.25 | 9.66 |

The Vulkan opt-in process CPU mean drops from 17.87 to 6.28 ms compared with the retained earlier
candidate. GPU intervals exclude CPU waits/display latency; process CPU includes all threads and
can exceed wall time. Their windows are independent, so they cannot be summed as critical-path
components. This longer DX12 comparison does not reproduce the earlier 272.02 ms p99 outlier;
the earlier result remains retained and is not invalidated. Final same-version physical acceptance,
visual movies, concept parity and overall hardware budget remain open.
