# Local Windows 2D host verification — 2026-10-14–15

Scope: physical developer-PC correctness and performance screening, not completed M6/M7 acceptance.
Runtime source is `ed698e7ed36db3a3763cc4fcd019924ff6a37e07`. This work changes only test
fixtures, progress diagnostics and local deadlines; [scope.json](scope.json) identifies that patch. Binaries and build products remain outside Git.

Host: AMD FX-6100, NVIDIA GTX 960, 16 GiB installed RAM (15.96 GiB OS-visible), Windows 10
19045, NVIDIA driver 32.0.15.8180. DX12 is selected by the existing Windows Automatic surface
backend; Vulkan is disabled for this build. The physical monitor is 1920x1080; workload client
size is 640x480. [Pre-run host/workload plan](host-plan.json) fixes the screening budget before
measurement: frame p95/p99 <=16.67 ms, max <=33.34 ms. This is a diagnostic budget, not a
reviewed product minimum-spec declaration.

## Functional validation

- ✅ Complete Windows Development configure/build and CTest: 237 cases, zero failures;
  skipped cases: 0; total 2065.07 s. [Raw final summary](validation-summary.txt) retains the
  observed results. The suite uses /O2 Development with assertions, parser deadline 900 s,
  hierarchy deadline scale 3 and capture generator deadline 90 s.
  Native DX12 window/resize/text-event/DPI-message contracts and
  2D runtime, bridge, assets, authoring and ImGui controls are included. Injected window messages
  establish automated contracts, not physical keyboard, controller or real multi-monitor DPI use.
- ✅ Native Studio smoke renders Scene/Atlas/Animation/Tilemap tabs, writes and reopens a document,
  validates native texture resolution and drains its surface. This is automated lifecycle/UI
  rendering evidence; no human pixel review or screenshot is claimed.
- ✅ Shipping Monolithic benchmark/asset compiler and Zig 0.14 consumer build. The native Zig
  consumer presents 600 DX12 frames, reloads without duplicated content and balances state
  allocation. [Raw report](zig-shipping-native.json) is retained unchanged. Its built-in
  `physical_windows: NOT RUN` label describes the generic sample; host metadata here supplements
  it. Its 600 movement frames are synthetic direction input, not physical keyboard acceptance.
- ✅ [Staged package](package-manifest.json): copied executable plus Cooked Atlas/Clip, SHA-256
  verification, Core async-load success, corrupt-cook rejection and all six portable workloads.
  [PE dependencies](dependencies.txt) contain Windows/DX12 and VC++ Runtime imports, with no
  Nexora or Editor DLL. VC++ redistributable/graphics-driver availability on a clean host remains unverified.
- ✅ Staged native DX12 mixed workloads complete, including animation/fixed updates, indexed
  collision queries/sweeps, Sprite extraction/culling, bulk recycle and GPU Atlas retire/reload.

Original full-build attempts failed on MSVC C4127 constant conditions, followed by C4702 after
an initial constexpr-only rewrite. The final mutually exclusive constexpr/else branches preserve
all 14 fixture modes, their events and worker joining without disabling `/W4 /WX`. The final full
build and runtime plugin lifecycle/ownership tests pass. Original logs remain in local build output.

The first unoptimized full-suite attempt also lacked MSVC/Ninja environment for two profile
subconfigurations and timed out in parser robustness (300 s) and the 100,000-node hierarchy
case (180 s). It was stopped before all 237 cases completed. The final gate loads the installed
Visual Studio Developer Shell/Ninja and uses `/O2` Development without `NDEBUG`/fast-math.
The parser's positive timeout option is 900 seconds locally; CI retains 300 seconds. Every seed,
default 2,000 mutations per parser and assertion is unchanged. A second optimized attempt passed
the parser in 221.09 s but exceeded 30-second Mesh JSON generator and 180-second deep-hierarchy
deadlines; it too was stopped before all cases ran. The final run selects bounded hierarchy scale
3 (deep/overview 540 s; Windows scale lifecycle 1,800 s) and capture generator 90 s. CI retains
scale 1 and capture 30 s. No fixture count, benchmark sample, schema check or assertion is removed. Per-parser progress identifies
work completed rather than inferring success from a longer deadline.

## Physical stress observations

Six cases, each four warm-up plus 600 measured frames. No build or test ran concurrently with
this measurement. [Raw CSV](shipping-native-stress.csv) retains counts, load/reload timings,
CPU update/query/extraction distributions, whole-frame distributions, completed GPU intervals,
coverage proxy and device identity. GPU timings measure command buffers, not display latency.
Default presentation/pacing is unchanged; fixed simulation delta 1/60 is not a 60 FPS observation.
The benchmark contains no integrated audio, UI/VFX load or complete allocator trace.

| Objects | Case | Frame average ms | p95 ms | p99 ms | Max ms | GPU p95 ms | Screening budget |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 100 | normal | 16.6684 | 30.6794 | 38.7323 | 53.0901 | 0.411296 | Exceeded |
| 1000 | normal | 16.8593 | 31.4456 | 39.7287 | 134.609 | 0.426528 | Exceeded |
| 5000 | normal | 16.8233 | 28.3256 | 39.3565 | 49.0094 | 0.48688 | Exceeded |
| 10000 | normal | 34.2998 | 53.6789 | 67.7445 | 106.811 | 0.475296 | Exceeded |
| 1000 | overlap | 16.8359 | 28.9648 | 42.0964 | 110.023 | 0.761536 | Exceeded |
| 10000 | sparse | 42.0048 | 60.9977 | 80.9242 | 118.577 | 0.260416 | Exceeded |

[Screening results](screening-result.json) retain the independent budget comparison and an
OS lifetime peak process working set of 87,240,704 bytes (83.20 MiB), observed every 100 ms
(702 samples); monitoring starts 17.88 seconds after process launch.
[Raw memory observation](memory.json) retains that delay and scope. The benchmark's CSV RSS column
is zero on Windows because its internal implementation is Linux-only; zero is unavailable, not
zero memory use. The OS peak is a process-level observation, not per-frame RSS, GPU memory,
allocator totals or guaranteed capture of every spike. AABB coverage is not measured fragment overdraw.

M6 remains **NOT ACCEPTED**: every workload exceeded the predeclared strict screening budget;
10,000 normal/sparse objects report frame p95 53.6789/60.9977 ms, while GPU p95 stays below
0.8 ms. The 10,000-object CPU update/query/extraction p50 is 27.2007/38.1224 ms;
this CPU section already exceeds 16.67 ms. Whole-frame intervals also include VSync/driver
waits and OS scheduling; the smaller workloads do not establish a CPU saturation claim. Minimum product hardware and integrated UI/VFX/allocator workload budgets are not declared, and screening results do not establish sustained minimum-PC acceptance.
M7 clean Windows remains **NOT RUN**. Actual keyboard/controller, audible playback, display/DPI
transitions, clean-host prerequisites and separately supported Windows Vulkan need further evidence.
Computer Use initialization crashed repeatedly, including after a kernel reset; no UI automation
or physical-input result is fabricated. The user explicitly limited this run to available checks.
Roadmap progress remains **9/11 (82%), P0 6/8**. Parent Beads `nexora-j4q` stays open; the
available local slice is tracked by `nexora-j4q.1`; physical CPU/pacing follow-up is `nexora-j4q.2`.

## Reproduction

```powershell
cmake --preset windows-showcase-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_BUILD_SHOWCASE=OFF -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_VULKAN_BACKEND=OFF -DCMAKE_CXX_FLAGS_DEVELOPMENT=/O2 -DCMAKE_C_FLAGS_DEVELOPMENT=/O2 -DNEXORA_PARSER_ROBUSTNESS_TIMEOUT=900 -DNEXORA_EDITOR_HIERARCHY_TIMEOUT_SCALE=3
cmake --build --preset windows-showcase-development --parallel 4
$env:NEXORA_PARSER_ROBUSTNESS_ITERATIONS = '2000'
$env:NEXORA_CAPTURE_TIMEOUT_SECONDS = '90'
ctest --preset windows-showcase-development --output-on-failure
cmake --preset windows-showcase-shipping -G "Visual Studio 17 2022" -A x64 -DNEXORA_BUILD_SHOWCASE=OFF -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_EDITOR=OFF -DNEXORA_ENABLE_VULKAN_BACKEND=OFF
cmake --build --preset windows-showcase-shipping --target NexoraTwoDBenchmark NexoraZigTwoD NexoraTwoDAssetCompiler --parallel 2
```

Before CTest, import the installed `Microsoft.VisualStudio.DevShell.dll`, call `Enter-VsDevShell`
with the installed VS path, `-Arch amd64 -HostArch amd64 -SkipAutomaticLocation`, and add the
bundled CMake/Ninja directory to the process PATH. This is process-local environment setup,
not a machine/global configuration change.

The Development cache explicitly had Vulkan OFF and Slang OFF; Zig 0.14 uses the existing local
compiler. No local SDK override is required by or added to repository presets. Configure/build
results are exact-host Windows evidence. Linux preset configure/build/CTest are required by PR CI,
not executed on this Windows host. No linkage boundary is changed by the fixture-only repair.

Use PrepareAssets.py for both checked-in fixture manifests, cook with NexoraTwoDAssetCompiler,
then StageValidation.py with a fresh output directory, as described in [tools](../../../../Tools/TwoD/README.md).
Run the staged benchmark from its package directory:

```powershell
./NexoraTwoDBenchmark.exe --native --frames 600 --atlas assets/atlas.nxcook --clip assets/clip.nxcook --report shipping-native-stress.csv
```

For Studio, use a fresh workspace with the source Atlas/Clip and `--smoke --frames 120`.
The Shipping Zig command is `NexoraZigTwoD.exe --native --frames 600 --report zig-shipping-native.json`.
Detailed local commands/logs and executable package remain under
`build/2d-windows-host-2026-10-14/`; curated evidence here contains no executable or development libraries.
