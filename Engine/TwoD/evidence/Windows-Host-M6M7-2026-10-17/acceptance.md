# Physical Windows host M6/M7 slice — 2026-10-17

**Result: scoped developer-PC device/UI acceptance; full M6/M7 remain NOT ACCEPTED.**
Tracked by `nexora-j4q.7.8`. Source base is `f347b90773470783a58a9eb28d3fc8c6633f27ba`
plus this PR's Player/benchmark/fixture patch. This is a new run, separate from the older PR #620
and VirtualBox evidence. The original VM, snapshots and old saves were not operated on this run.

## Host and immutable artifact identity

Windows 10 Home x64 19045; AMD FX-6100; 17,134,051,328 bytes RAM; GTX 960, 2 GiB VRAM,
NVIDIA driver 32.0.15.8180. One 1920×1080 monitor; original scale 100%. VS 2022 Community
17.14.35, MSVC 19.44.35228 x64, SDK 10.0.26100.0, pinned Zig 0.14.0.
Both native backends identify vendor 4318 (NVIDIA), device 5121 (GTX 960).

The manual package is Monolithic Shipping, DX12 and Vulkan enabled, graphical Editor off,
with matching Microsoft.VC143.CRT 14.44.35112. [Inventory](manual-inventory.json) identifies
Player `a21d7986547f53c3528f291fc27cdd65c739c26b8a900369149d2b9fb5283515`
and independent module `77841fccf381c68ffb7bac5a4f65c3308e685f2a567ec1fe01686a5dbd79e0c8`.
All listed hashes still match after manual use. The [final rebuilt Shipping inventory](final-inventory.json)
retains that exact Player hash; both final native smokes and real-font headless contracts pass.
[Binary identity](identity.json) also identifies the final benchmark. Binaries, CRT and font raster are not committed.
Local package: `build/windows-host-m67-20261017/manual-01/package/`.
This UI-only fixture has no World sprites and is not a production game or integrated M6 load.

`HostModule.cpp` uses the public C ABI without linking engine DLLs. The fixture's own position,
rebinding and service counters are in stderr; generic Player `entity_x`, `resumed`, `ui_saves`
and `ui_opens` describe its built-in fixture and stay zero/false for this independent module.
The corpus is 114 printable BMP characters extracted from the fixture source; the real atlas
was baked with Microsoft JhengHei. Synthetic solid test glyphs are used only by headless tests.
Font raster redistribution rights and production text/content remain game-project concerns.

## Observed acceptance

| Item | Evidence and result | Boundary |
| --- | --- | --- |
| Explicit hardware backends | ✅ [DX12](smoke-dx12.json) and [Vulkan](smoke-vulkan.json), 3 native frames each, device IDs above, 399 UI draws, balanced allocations | Positive backend selection; no software fallback. Vulkan human appearance/input/audio suite not repeated |
| Probe audible playback | ✅ [Probe](audio-probe-repeat.json): 480,001 submitted frames and reopen; user: “有，清楚聽到” | Second approximately 10-second 440 Hz run; raw JSON alone does not prove hearing |
| Chinese UI and 100/150/175% DPI | ✅ User confirms Chinese, punctuation, numbers and buttons normal; [first report](manual-01-report.json) max 1.5, [second](manual-02-report.json) max 1.75, both initial/min 1.0 | Standard display menu maximum is 175%; 200% NOT RUN. One monitor, so cross-monitor NOT RUN. No screenshots retained |
| Native keyboard/rebind/focus/resize | ✅ User confirms D/A, D→F with old key inactive, SAVE→move→OPEN, held movement Alt-Tab/minimize/restore/resize normal | First report: 183 key presses, 14 focus losses, 0 unfocused movement, 3 resize generations; fixture trace: 46 keyboard movement frames |
| Wheel and UI cancel repeat | ✅ [Third host](manual-03-report.json): 87 wheel events, 16 key presses, 9 mouse presses; [module trace](manual-03-stderr.log): 4 UI cancels | Second session counters were zero, so it was not used to accept these paths. Fixed panel has no scroll widget; wheel delivery is the accepted behavior |
| Independent durable state | ✅ First module: 2 saves/2 opens, final position -0.846032, D. Next process loads same position/D; third process changes to F and both [Vulkan](resume-vulkan.stderr) and [DX12](resume-dx12.stderr) restart with same position/F | Fresh dedicated Unicode test-save path. Final Stop saves latest position, explaining old on-screen SAVED -0.017 versus next process -0.846. No old user save modified |
| Music/SFX and bus control | ✅ User confirms clear looped 660/440 Hz tones, Master/Music/SFX 100/25/0% behavior, pause/resume/stop. First module: 12 bus changes, 5 plays, 1 pause action, 3 stops | Human observation plus real module service calls; not sample-accurate latency or all device formats |
| External speaker reconnect | ✅ User reports external speakers, unplug/replug normal and automatic audible recovery without controls | Connection/endpoint identity unspecified. All manual reports have 1 successful open, 0 pump failures: this did NOT exercise backend device-loss/reopening. USB endpoint removal and default-device switching remain NOT RUN |
| Normal close and restart | ✅ All three manual runs exit 0, final save true, balanced allocations; 14,447 + 1,812 + 3,614 native frames | Short manual developer-host lifetimes, not a declared long-duration soak or Editor Play/Stop |
| XInput | NOT RUN: user has PS4 over USB; OS detects Wireless Controller/HID, XInput slots 0–3 disconnected | Native Windows adapter supports XInput, not DirectInput-only PS HID. No driver/remapper was installed or changed |
| Launcher/PATH isolation | ✅ [Run.cmd](launcher-isolation.json) starts both native backends from an unrelated Unicode CWD with Windows-system-only PATH and developer SDK variables removed; exit0 and immutable hashes | Installed developer PC, not clean-host or human double-click acceptance |
| Real Zig cooked service integration | ✅ [Headless cases](zig-headless.json) reject damaged Cooked, wrong type, missing WAV and corrupt save; [native runs](zig-native-summary.json) pass two 120-frame processes per backend | Each DX12/Vulkan run: 34 visible sprites, 120 instanced draws, 4,080 GPU instances, 2,160 UI draws, one cooked reload, 96,000 audio frames, save/reopen and balanced teardown. Positions 0→2→4 persist; deterministic fixture input, no additional human audibility/input claim |
| Headless service/storage contract | ✅ [Contracts](headless-contracts.json): schema1 12-byte payload, restart restores position/rebind; CRC corruption rejected and bad bytes preserved; package immutable | Synthetic headless font is not appearance/device evidence |

[Human observations](human-observations.json) retain the actual replies and clarification of the
175% ceiling. Original host JSON retains `audible_acceptance: NOT VERIFIED` and
`physical_windows: NOT RUN`; these generic markers are supplemented, never rewritten.
Computer-use initialization failed twice with `helper_unknown_error: setup refresh had errors`.
The human operated all native UI; no synthetic UI automation or screenshot evidence is claimed.

## Reproduction and validation

Use the VS x64 developer shell, bundled CMake and separate build configurations:

```powershell
cmake --preset windows-showcase-shipping -G "Visual Studio 17 2022" -A x64 `
  -DCMAKE_CONFIGURATION_TYPES=Shipping -DNEXORA_BUILD_SHOWCASE=OFF `
  -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF `
  -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON `
  -DNEXORA_VULKAN_LIBRARY=<local-vulkan-loader-import-library> `
  -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=<pinned-v1.3.296-headers> `
  -DNEXORA_ZIG_EXECUTABLE=<zig-0.14.0-executable>
cmake --build --preset windows-showcase-shipping --target NexoraTwoDPlayer `
  NexoraTwoDAssetCompiler NexoraTwoDAudioProbe NexoraTwoDBenchmark NexoraZigTwoD `
  NexoraZigTwoDSharedBuild --parallel 2
cmake --preset windows-development -G "Visual Studio 17 2022" `
  -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_VULKAN_BACKEND=OFF -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF
cmake --build --preset windows-development --config Development --parallel 2
ctest --preset windows-development -C Development
```

The existing Development generator had no explicit platform field; do not add `-A x64` to that
cache. A build without `--config Development` selects Debug under Visual Studio and is not the
Development gate. Initial configuration selection was corrected before the recorded final gate.

Compile the independent fixture with MSVC `/LD /EHsc /std:c++20 /utf-8 /W4 /WX` and public API
include path. Bake its corpus with Windows PowerShell 5's documented per-process
`-ExecutionPolicy Bypass -File Tools/TwoD/BakeWindowsFont.ps1`; no persistent policy was changed.
PowerShell 7's Add-Type reference set did not compile this GDI helper. Stage with
`HostAcceptance.py --shipping --real-font ... --redist ... --output <fresh> --stage-only`.
Run `--backend dx12` unbounded for human interaction and `--native --frames 3 --backend vulkan`
for the separate smoke/restart. Saves use a dedicated directory outside the package.

The full MSVC build exposed C4127/C2220 in two constant backend guards in
`Tests/Renderer/VulkanDescriptorIsolationTests.cpp` (including current remote main). A first `if constexpr` early return exposed C4702 unreachable-body warnings. Definitions and
call sites now use the existing backend test macros, compiling the DX12/Metal-only cases only
for their matching target while retaining `/W4 /WX`. This is a
compile-time test repair, not additional GPU recording acceptance.

`ProductAcceptance.py --shipping` additionally ran against the real `NexoraZigTwoD` executable.
The four native invocations use `--validation-package <staged> --native --backend dx12|vulkan
--audio --frames 120 --save-directory <per-backend-test-root> --report <fresh>`.
The [Zig manifest](zig-manifest.json) remains unchanged. This staging tool lists system CRT/driver
prerequisites; it is not the general exported package or an independent clean-host test.

The final full MSVC Development build exits 0. [Full selected-configuration CTest summary](windows-validation.json):
**197 tests, 191 passed, six skipped, zero failed**, exit0, 1,471.59 seconds. The six DX12 native
recording tests report `native graphics unavailable` or `D3D12 artifacts/device unavailable`;
this cache has Slang off, so they are not GPU-recording acceptance. Native 2D Shipping uses
its own embedded shaders and is separately exercised above. Development also has graphical
Editor and Zig gameplay off, but retains pinned Zig 0.14.0 for the ABI tests. Full richer Linux/
Windows/macOS CI results will be recorded with the PR. The unmodified full local log/JUnit remain
under `build/windows-host-m67-20261017/`.

Local VM transport 12 tests, GDI corpus/font contracts, backend preflight/headless provenance,
independent storage/corruption checks and 16 documentation tests pass. Changed Markdown links,
bilingual pairs and `git diff --check` pass. Root README remains concise and unchanged; only the
2D roadmap is affected by this supporting work. Performance results follow below.

## M6 current-PC screening result

**Correctness/teardown: PASS for both six-case processes. Performance acceptance: NOT ACCEPTED;
all 12 rows exceed the predeclared screening budget.** The plan was written before measurement;
p95/p99 ≤16.67 ms, max ≤33.34 ms, VSync 640×480, four warm-up plus 600 measured frames per case.
No build, CTest, Zig compiler or other Nexora native fixture was running during timing.
Other normal desktop applications remained open. This is one sequential run per backend,
not a distribution across repeated runs or a clean lab/minimum-spec machine.

Run the final Shipping `NexoraTwoDBenchmark --native --backend dx12|vulkan --frames 600
--report <fresh.csv>` separately per backend. [DX12 CSV](performance-dx12.csv),
[Vulkan CSV](performance-vulkan.csv), [summary](performance-summary.json) and the predeclared
[plan](performance-plan.json) retain identity, driver/device, sample count, loading/reload,
CPU/GPU/frame timing, visibility/query/batch observations and budget comparisons.

| Backend | Workload | CPU p95 ms | GPU p95 ms | Frame p95 ms | Frame p99 ms | Frame max ms |
| --- | --- | --- | --- | --- | --- | --- |
| dx12 | 100 mixed | 0.55 | 0.42 | 17.43 | 17.73 | 35.01 |
| dx12 | 1000 mixed | 3.15 | 0.45 | 18.49 | 32.85 | 53.57 |
| dx12 | 5000 mixed | 15.17 | 0.53 | 19.56 | 21.55 | 25.09 |
| dx12 | 10000 mixed | 26.11 | 0.62 | 31.97 | 36.64 | 42.01 |
| dx12 | 1000 overlap | 2.99 | 0.72 | 17.77 | 18.75 | 35.80 |
| dx12 | 10000 sparse | 39.23 | 0.28 | 40.33 | 48.78 | 412.54 |
| vulkan | 100 mixed | 0.51 | 0.10 | 33.20 | 33.64 | 35.51 |
| vulkan | 1000 mixed | 2.22 | 0.09 | 31.65 | 32.27 | 35.14 |
| vulkan | 5000 mixed | 13.70 | 0.15 | 19.37 | 29.92 | 39.45 |
| vulkan | 10000 mixed | 25.46 | 0.41 | 30.99 | 35.76 | 37.50 |
| vulkan | 1000 overlap | 2.85 | 0.30 | 31.28 | 31.77 | 35.47 |
| vulkan | 10000 sparse | 48.59 | 0.14 | 49.06 | 59.97 | 78.28 |

The 10k mixed and sparse rows clearly exceed a 60 FPS frame budget. The lower-load whole-frame
percentiles also fail the strict screen, even where their CPU/GPU work is small; VSync/presentation
wait is included in whole-frame time. These aggregate results do not identify a permanent root
cause for individual hitches. No budget is relaxed and no optimization/minimum-PC pass is claimed.
The benchmark includes a full pool recycle/atlas retirement halfway through each measured case.

External `GetProcessMemoryInfo` sampling every approximately 100 ms records whole-process peak
working sets: DX12 **90,865,664 bytes**, Vulkan **147,972,096 bytes** (905/829 samples).
The benchmark's Windows `peak_process_rss_bytes=0` means unavailable, not zero memory. External
working set includes driver mappings; it is not allocator/VRAM accounting or a memory-soak trend.
GPU timing has 598 completed samples per case; command-buffer timing is not display latency.

## Instancing and CPU-stage diagnostic

The measured sprite path already uses GPU instancing: a single compatible sprite batch issues
DX12 `DrawInstanced(6, instanceCount, ...)` or Vulkan `vkCmdDraw(6, instanceCount, ...)`.
The 10k mixed row has 9,700 visible instances; the sparse row has only 195. These are not 10,000
per-object draw calls. The mixed benchmark also rebuilds the spatial index, queries/sweeps,
extracts sprites and prepares their GPU instance data every frame.

A temporary instrumented copy of `Tools/TwoD/Benchmark.cpp`, linked to the same Shipping libraries,
ran DX12 with four warm-up and **60 measured frames** per case. It timed pool-to-vector population,
`index.Build`, Query+Sweep, `ExtractSprites`, extracted Draw, and EndFrame separately using
`steady_clock`. This is a coarse diagnostic experiment, not the frozen 600-frame acceptance or
a render-only benchmark. No tracked renderer/benchmark implementation changed. Independent stage
p95 values must not be summed as the exact frame p95.

| Workload | Populate p95 ms | Index build p95 ms | Query/sweep p95 ms | Extract p95 ms | Draw CPU p95 ms | EndFrame p95 ms |
| --- | --- | --- | --- | --- | --- | --- |
| 10k mixed | 1.9795 | 6.3234 | 0.3023 | 16.4837 | 5.4747 | 0.7960 |
| 10k sparse | 1.7366 | 27.1584 | 0.0298 | 11.8509 | 0.2096 | 0.6493 |

The useful next investigation is CPU extraction/index construction plus a separate render-only
baseline. Draw CPU includes preparation/upload/command recording; this does not isolate a single
conversion as the cause. GPU-driven culling or a direct instance extraction path is not implemented
or accepted here. The original benchmark executable SHA-256 remains
`8b6a75eb101b4a56590a63d4e1a28c2cf9a00eaea659ed1507cd80297cd42fa6`.

## Linux CI headless-input regression

The first Linux mimalloc job failed the initial zero-position assertion. A diagnostic-only follow-up
at `e42b91e7` retained the actual state in
[the failure excerpt](linux-headless-input-failure.txt): startup position was zero, followed by
three gamepad movement frames, left axes -1 and final position -0.05. The Player polled native
gamepads even in headless mode. This is observed input contamination, not evidence of an allocator
or save corruption defect; the physical identity behind the runner's input is not established.

The fix confines native gamepad polling to native presentation. The headless fixture now also
requires zero keyboard/gamepad movement and zero sampled axes on both first start and resume.
Native input routing is retained. Human/native evidence above keeps its original frozen executable
identity; the headless isolation follow-up is validated separately and does not retroactively
change those observations. All applicable CI must pass on the final PR head before merge.

After this fix, both MSVC Player builds pass; the Development `ctest --preset windows-development
-C Development -R '^twod\.' --output-on-failure` gate passes **18/18**, including first/resume
zero-input checks. A fresh Shipping real-font/CRT package passes the same storage/corruption
[contracts](headless-isolation-contracts.json), followed by three native frames and balanced
teardown on each hardware backend. The [follow-up summary](headless-isolation-summary.json)
records its distinct Player hash; original manual/600-frame binaries remain untouched.

## Remaining full milestone gates

M6: declared minimum PC/product workload, integrated UI/VFX and allocation/VRAM/long-duration
acceptance remain open. [Predeclared performance plan](performance-plan.json) is developer-PC
screening only; budgets will not be relaxed after measurement.
M7: actual XInput hardware, 200%/cross-monitor display, real endpoint loss/reopen and default
output switching, main Editor Game View acquisition/Play/Stop, production game schema/content
and an independent clean Windows hardware machine without developer tools remain open.
The physical fixture does not replace those gates. **M6/M7 NOT ACCEPTED; 9/11 (82%), P0 6/8**.
