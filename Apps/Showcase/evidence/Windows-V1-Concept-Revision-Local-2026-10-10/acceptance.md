# Windows courtyard concept revision candidate

This candidate responds to the operator's request to revise composition, lighting/color,
crystal, foliage and ground toward `Roadmap/art/V1-Visual-Identity-Concept.png`. It is an actual
native scene revision with the retained original assets, not a rendered concept-image substitute.
Final visual approval and a stable target-hardware frame-time budget remain open; VIS stays 5/7.

The wide camera is lower and farther away. The 100-second visual tour starts and ends with
that same framing. Foreground banks move into the bottom corners, with smaller hanging/rim
leaves and unchanged leaf/vertex budgets. Stone uses finer world-space weathering, restrained
normal amplitude and cooler base factors under the sunset. Bronze roughness/tint is more muted.
The crystal shell is narrower; its contained faceted mineral core fills more of the shell,
with bounded 1.46 refraction, reduced thickness and a stronger cyan transmission tint.
The sun-to-IBL ratio, atmosphere and shadow tint retain sunset depth. The color-grade contrast
is 0.97 to preserve dark foliage detail after ACES instead of clipping it to black.

Both retained 1280x720 fixed views run 600 real native frames on the GTX 960 with static Zig,
paused/activated courtyard, no overlay, no software renderer and no backend fallback. The actual
JSON reports and source hashes are retained with the images. They are previews, not a full
physical interaction/tour acceptance gate or stable FPS proof. The DX12 preview averages 61.07
FPS (p99 18.88 ms); the official repeated quality matrix remains separate.

Two earlier art iterations were inspected and rejected locally: excessive IBL flattened the
sunset; increased contrast clipped leaf shadows. Each earlier iteration has actual complete
100-second native DX12/Vulkan H264 recordings stored under the local
`build/v1-windows-local-2026-10-10/` directory. Those movies belong to different executable
hashes and are not final evidence for this corrected candidate. No large movie is committed.

The first comprehensive revision passed the full Windows Development gate, 122/122 in
229.79 seconds, including convex-shell containment and the adjacent-material native pixel fixture.
The corrected candidate's full configure/build/CTest gate passes 122/122 in 249.24 seconds. Shipping/Monolithic package and isolated
headless verification pass with 51 manifest entries. The committed corrected candidate passes the repeated native matrix, physical interaction/tour
gates and exact-head hosted CI described below. Final same-version visual movies remain required. This document does not claim a Linux Codex Cloud run.

Exact full Windows gate commands:

```powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --config Development --parallel 4
ctest --preset windows-showcase-development --parallel 4 --output-on-failure
```

Normalized complete corrected-candidate logs are retained beside this record. The same source
also passes native DX12 PBR image comparisons, including the split adjacent-material receiver;
no tests are skipped. Documentation link/bilingual validation reports zero errors. Repository
root README remains concise and unchanged by this supporting iteration.

## Committed-source throughput matrix

The identified `bb0c96f0c5397819d13577be20371533790d2dad` Shipping artifact has executable SHA-256
`3bb1b43bbae727e50f717e8b3fd28e1b7b25373e377cc12f4a1a833348abc52b`. Its complete 18 raw
matrix reports pass the existing benchmark validator and hash checks. Three independent processes
per tier/backend run 360 actual frames, discarding 60 warmup frames. The native binary is the same
for both backends; no recorder, compiler or CTest process overlaps these measurements.

| Quality | DX12 FPS, all three runs | Vulkan FPS, all three runs |
| --- | --- | --- |
| Basic | 102.55 / 102.19 / 100.93 | 61.53 / 61.78 / 54.16 |
| Standard | 60.65 / 60.73 / 60.20 | 40.11 / 37.75 / 37.51 |
| High | 60.43 / 60.01 / 60.49 | 38.41 / 39.20 / 37.17 |

DX12 Standard p99 is 18.89 / 19.04 / 20.48 ms; Vulkan Standard p99 is 35.15 / 37.18 / 37.66 ms.
This matrix does not reproduce the earlier hundreds-of-milliseconds outliers, which remain in
historical records. Average DX12 throughput around 60 FPS does not establish a stable 16.7 ms
budget. Vulkan remains below that provisional target. Hardware budget approval remains false.

The retained read-only `nvidia-smi` telemetry samples clocks/load at 200 ms intervals: 713 samples,
30–50 Celsius, graphics clocks 135–1354 MHz, memory clocks 405–3505 MHz, GPU utilization 0–70%,
power 14.95–78.38 W and device memory 389–540 MiB. These ranges include process startup and idle
intervals. They are device-wide observations with host-local Asia/Taipei timestamps, not GPU
command-buffer durations or proof of a thermal/power bottleneck. Actual completed native GPU
interval diagnosis is separate follow-up work; no CPU time is reported as GPU time.

## Optional diagnostic implementation scope

Follow-up source adds `--gpu-timing` over the existing optional Presentation timestamps, with
independent deduplicated completed-submission samples and bounded storage. Those changes are
not part of the `bb0c96f0` artifact/matrix above. The diagnostic candidate passes full Windows
Development configure/build/CTest (122/122, 234.54 seconds), Shipping package/isolated-headless
verification and the native/default/headless comparisons below; its exact-head CI remains pending. They do not change the accepted descriptor checks, geometry or draw ordering.

## Committed physical display and hosted CI

The same identified `bb0c96f0c539` artifact passes both physical GTX 960 display gates: 51 manifest
checksums, 74 captures per backend, all interaction/effect comparisons and no software renderer or
backend fallback. Completion is observed from native tour state: DX12 210.007 seconds and Vulkan
210.006 seconds, both enabled/paused at step 6. The selected images and original acceptance,
launch and progress reports are retained in `dx12-physical/` and `vulkan-physical/`. This is the
operator-attested local physical display gate; `clean_host_verified` remains false. It does not
constitute final concept-art acceptance or a stable frame-time budget.

[Build 2069](https://github.com/jimlee1972/Nexora/actions/runs/37959306735) passes all 18 jobs at
exact commit `bb0c96f0c5397819d13577be20371533790d2dad`, including full Linux Development
configure/build/CTest, Linux Shipping build-contract/sanitizers, Windows Development Editor,
DX12 Shipping and hosted software-Vulkan acceptance. These are GitHub-hosted CI results,
separate from the physical GTX 960 measurements and from Linux Codex Cloud.

## Completed native GPU diagnostic results

The separately identified diagnostic executable SHA-256 is
`91e6ff779435ef2f761b6d835875df42627fb0416fafdb86f16dd2ae2c2a3294`; its source hashes are retained
in `gpu-diagnostic/source-provenance.json` over base `bb0c96f0`. Each run renders 1,200 actual frames,
with 1,140 wall/CPU samples and, when enabled, 1,138 unique completed GPU samples. The surface
identifies real GTX 960 native timestamps. Neither backend uses fallback or software rendering.
Defaults report unavailable GPU timing with no completed submissions; headless opt-in also remains
unavailable. These are sequential diagnostic comparisons, not the default quality matrix.

| Backend / GPU opt-in | FPS | Wall mean ms | Process CPU mean ms | GPU mean / p99 ms | Wall p99 ms |
| --- | --- | --- | --- | --- | --- |
| DX12 / off | 58.34 | 17.14 | 9.64 | unavailable | 20.87 |
| DX12 / on | 33.77 | 29.61 | 11.32 | 7.97 / 16.03 | 272.02 |
| Vulkan / off | 36.92 | 27.09 | 18.94 | unavailable | 38.47 |
| Vulkan / on | 37.25 | 26.85 | 17.87 | 9.24 / 9.65 | 37.13 |

The DX12 opt-in run retains an actual 272.02 ms wall p99 outlier; do not discard it or interpret
this single comparison as proven timestamp overhead. GPU intervals exclude CPU waits/display
latency, process CPU includes all threads, and their sample windows are independent. They cannot
be summed as a critical-path breakdown. The Vulkan interval being much shorter than its wall time
supports investigating host/driver work. Inspection finds per-frame Vulkan target destruction and
allocation; fence-owned compatible-target reuse is a separate follow-up, not delivered here.
Hardware frame-budget acceptance and final concept parity remain open.
