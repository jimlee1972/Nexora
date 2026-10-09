# Windows GTX 960 performance baseline

The operator reported slideshow-like motion and rejected the courtyard candidate's art,
requesting all visual aspects to approach `Roadmap/art/V1-Visual-Identity-Concept.png`.
These measurements establish the starting point and the isolated compiler-optimization effect;
they do not accept VIS-M3 final art or the VIS-M6 hardware budget.

Each backend ran Basic, Standard and High sequentially, three independent processes per tier,
360 acquired frames per process, discarding 60 warm-up frames. The scene is activated, paused
at zero, fixed wide camera, 1280x720, VSync off, clean view, Shipping/Full/Monolithic and static
Zig gameplay. No movie recorder, other native acceptance, compiler or CTest ran concurrently.
GPU timestamps and refresh rate remain unavailable. CPU values cover the whole process.

Original runtime source freeze: `5ddd9baac289e602821e4cc06df2e1e1ef1b28b3`.
Original package executable SHA-256: `419838a435908f56555959b3c123aece0532fe3e7932a4b99ca177c9462da64a`.
The optimization candidate uses HEAD `e3eb383b4df7dfaadd1bab8071987aa74cf4144d` plus the
recorded uncommitted `CMake/Modules/NexoraBuildConfig.cmake` change. Runtime geometry, assets,
shaders and rendering settings are unchanged. The original compiler command contains `/GL`
but no `/O` speed optimization; the candidate specifies `/O2` and generated MSVC `MaxSpeed`.
No `NDEBUG`, fast-math, disabled validation, quality downgrade or rendering-effect removal is added.

| Backend | Quality | Original FPS range | Optimized FPS range |
| --- | --- | --- | --- |
| dx12 | basic | 11.99–20.95 | 39.96–52.02 |
| dx12 | standard | 10.70–15.76 | 34.55–36.93 |
| dx12 | high | 12.23–16.75 | 35.71–37.71 |
| vulkan | basic | 6.54–8.18 | 32.66–35.80 |
| vulkan | standard | 8.25–14.18 | 24.95–26.61 |
| vulkan | high | 9.30–13.33 | 23.04–26.63 |

Native collection commands (one after the other):

```powershell
python -X utf8 Tools/Package/BenchmarkShowcase.py <package-executable> --output <dx12-directory> --backend dx12
python -X utf8 Tools/Package/BenchmarkShowcase.py <package-executable> --output <vulkan-directory> --backend vulkan
```

Both complete matrices identify NVIDIA GeForce GTX 960, prohibit fallback/software rasterization,
retain each raw report and validate a stable executable SHA-256 across repeats. Consult each
`benchmark.json` for exact executable hashes, build identity, driver, settings, commands and samples.
The candidate improves throughput but still misses the provisional 60 FPS / 16.7 ms target.
Further profiling, art revision, new same-version physical acceptance and full final CI are pending.

## Exact finite-value validation candidate

A temporary local stage probe, subsequently removed from both main.cpp and Dx12Surface.cpp,
measured approximately 7.4 ms in PBR validation and 1.3 ms in subsequent duplicated UV validation.
The second candidate retains Shipping optimization, replaces per-component CRT finite classification
with exact IEEE binary32 exponent classification, and removes only scans already covered by the
shared PBR validator. Lambert checks remain. Geometry, shader bytes, texture content, resolution,
all effects and draw order are unchanged. No temporary stage probe is present in the packaged binary.

The candidate retains all 18 fresh runs, including severe outliers. Results must not be reduced to
only the fastest repeats or interpreted as stable 60 FPS acceptance.

| Backend | Quality | FPS range across all three repeats |
| --- | --- | --- |
| dx12 | basic | 65.94–91.55 |
| dx12 | standard | 52.52–56.93 |
| dx12 | high | 18.56–54.91 |
| vulkan | basic | 53.24–57.60 |
| vulkan | standard | 13.32–32.75 |
| vulkan | high | 32.69–33.80 |

The raw reports retain mean/P95/P99/CPU times and the artifact's exact hash. Standard DX12 improves
to 52.52–56.93 FPS. Vulkan Standard still drops to 13.32–14.25 FPS in two repeats, and DX12 High
has an 18.56 FPS outlier. These are unresolved observations, not accepted hardware performance.
Full Windows Development configure/build and 113/113 CTest pass (211.00 s), including the expanded
binary32/NaN component contracts and actual native DX12 PBR. Observed-completion Vulkan physical
acceptance passes; final
Linux validation, command-recording optimization and requested comprehensive art revision remain open.

Full Windows gate commands:

```powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --config Development --parallel 4
ctest --preset windows-showcase-development --parallel 4 --output-on-failure
```

The native Shipping package target also passed its isolated headless verification and 51-checksum
manifest. This record is a local Windows gate; it does not claim a Linux Codex Cloud run or final CI.

## Observed native Vulkan acceptance

The same finite-validation Shipping candidate passes stock Windows PowerShell 5 isolated-copy
acceptance on the physical GTX 960 display, with the operator's explicit attestation. Status is
PASS, 51 manifest entries verified, 74 local screenshots, no issues and no clean-host claim.
All room/input/resize/effect/replay checks run; the changed synchronization observes actual tour
JSON progress and completion rather than relying on a fixed wall-clock sleep. Engineering tour
completion is 210.017 seconds, step 6, enabled and paused. Original gates remain unchanged.

Selected fixed art, occlusion comparison and completion PNGs are retained in
`vulkan-observed-native/`, with reports. The complete 74-image local run remains at
`build/v1-windows-local-2026-10-09/finite-validation-vulkan-observed/`.
This gate validates real native display and interactions; it does not accept the rejected art
or the insufficient/variable frame-time budget. The matching DX12 physical run is in progress.

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File <package>/accept-v1.ps1 -EvidenceDirectory <evidence> -Backend vulkan -PhysicalDisplay -CompleteGuidedTour -ExpectedBuildId e3eb383b4df7
```
