# Windows DX12 completed-slot target reuse

GTX 960 Shipping baseline `5aa60d5c` measures Standard DX12 51.71–55.16 FPS and
completed GPU mean 7.53 ms versus wall mean 18.83 ms. DX12 recreates scene color,
reflection/refraction and shadow color/depth targets on every draw. This revision retains complete
compatible targets in the existing protecting swapchain slot after `Acquire` waits its fence.
Dimensions, HDR format, shadow resolution and reflection/refraction requirements must match.

Private color/reflection/shadow states track recorded composite/pass transitions and return to
RENDER_TARGET before clears. Refraction keeps the existing synchronized opaque-copy transitions.
Views/material bindings refresh from the current submission; every pass, draw, geometry and effect
remains. Invalid input retains validation behavior; partial recording does not publish a reusable
set, and mismatch/direct draw resets the completed slot. Resize/teardown release after the existing
drain. Storage stays bounded to three slots; no extra wait, module dependency or shader change.

Latest main `16c2c980` is integrated at `bba5db85`; source hashes are retained separately from the
preceding repaired Vulkan artifact `5aa60d5c`. Complete Windows Development configure/build/CTest
passes **125/125 in 444.81 seconds**, including both unchanged DX12/Vulkan native PBR fixtures.
Two CTest workers and all original timeout/pixel limits are retained.

```powershell
cmake --preset windows-showcase-development -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_VULKAN_LIBRARY=G:/proj/Nexora/.tools/vulkan-loader-local/vulkan-1.lib
cmake --build --preset windows-showcase-development --config Development --parallel 4
ctest --preset windows-showcase-development --parallel 2 --output-on-failure
```

Commands run with the initialized VsDevCmd/pinned Zig/Ninja task environment. Exact-source hosted
CI and frozen Shipping quality/GPU/immediate/paced/live results are recorded below; final hardware budget remains pending. Full concept-art acceptance and same-version final movies remain open. No Linux Codex
Cloud execution is claimed from this Windows gate.

## Committed Shipping frame performance

Source `7c4534b850c37fc0ae741ebf2198408f20613368`, frozen executable SHA-256
`257c23cb8051ee736d50bbdb1dcba9f4eaaea6036fecdb1446bb12c1a9b0e518`, passes Shipping package/isolated-headless checks and all 18 strict native quality reports. Geometry, effects, draw order and pixel thresholds are retained. Native runs are sequential, with no project build/CTest/Beads synchronization/recorder overlap; background desktop activity is not controlled. All outliers are retained.

| Quality | DX12 FPS, three runs | Vulkan FPS, three runs |
| --- | --- | --- |
| Basic | 157.79 / 158.26 / 137.56 | 148.56 / 146.32 / 135.65 |
| Standard | 134.91 / 131.52 / 133.59 | 125.74 / 125.87 / 122.85 |
| High | 131.88 / 131.90 / 133.07 | 122.39 / 108.73 / 109.38 |

DX12 Standard p99 is 8.78 / 9.41 / 8.82 ms, versus the prior fixed `5aa60d5c` baseline 22.37 / 23.70 / 27.76 ms. Vulkan Standard p99 is 10.04 / 9.94 / 12.48 ms. The High Vulkan second-run 29.42 ms p99 remains in the raw evidence.

| Unpaused Standard with UI, 1,200 native/UI frames | FPS | Wall p99 ms |
| --- | --- | --- |
| dx12, requested vsync off | 133.43 | 9.35 |
| dx12, requested vsync on | 60.03 | 18.04 |
| vulkan, requested vsync off | 126.30 | 9.60 |
| vulkan, requested vsync on | 60.04 | 18.24 |

Opt-in completed GPU comparisons retain 1,138 unique completed GPU intervals and 1,140 wall samples each; default/headless timing remains unavailable/null.
dx12: GPU mean 7.03 ms, GPU p99 7.35 ms; wall mean 7.64 ms, wall p99 10.38 ms.
vulkan: GPU mean 7.79 ms, GPU p99 8.28 ms; wall mean 7.93 ms, wall p99 9.64 ms.

Both paced runs average approximately 60 FPS, but p99 remains 18.04 / 18.24 ms, exceeding the provisional 16.7 ms budget. Overall hardware-budget certification, full concept parity and final same-version movies remain open.

Exact-source GitHub-hosted CI [Build 2105](https://github.com/jimlee1972/Nexora/actions/runs/37978332845) passes all 18 jobs, including full `cmake --preset linux-development`, `cmake --build --preset linux-development`, `ctest --preset linux-development`, Linux Shipping and native Xvfb core/synchronization validation. Vulkan repair source `5aa60d5c` [Build 2092](https://github.com/jimlee1972/Nexora/actions/runs/37974114417) also passes all 18 jobs. These are hosted Actions results; no Codex Cloud execution is claimed. Raw status is retained in `hosted-ci.json`.

## Same-artifact physical-monitor delivery

Both backends pass `accept-v1.ps1 -Backend <backend> -ExpectedBuildId 7c4534b850c3 -PhysicalDisplay -CompleteGuidedTour` using the frozen SHA above. Each backend has 74 captures and 51 verified manifest checksums, no issues/fallback/software renderer, an enabled/paused engineering tour at step 6, and the existing actual-monitor operator attestation. Clean-host acceptance remains false.

- dx12: observed 210.011 seconds; acceptance/launch/progress reports and six selected captures are retained under `committed-dx12-physical/`.
- vulkan: observed 210.012 seconds; acceptance/launch/progress reports and six selected captures are retained under `committed-vulkan-physical/`.

The runnable isolated copy is `build/v1-windows-local-2026-10-10/native-demo-7c4534b8/bin/NexoraShowcase.exe`; sibling `run-dx12-demo-7c4534b8.cmd` and `run-vulkan-demo-7c4534b8.cmd` select Standard, animated/UI, requested vsync on. This is engineering/display acceptance; final concept parity, stable frame-time budget and final movies remain open.

## Stage handoff

At the operator request, this stage stops after retaining the performance/physical results and documentation. The Windows recorder still uses 10 FPS; the prepared 30 FPS cadence draft is not applied or accepted. Final concept parity, paced frame-time budgets, same-version final movies and independent clean-host delivery remain open. See [current stage status](status.zh-TW.md).
