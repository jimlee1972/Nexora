# 2D cloud acceptance — 2026-10-14

Scope: portable C++ contracts and Linux/X11/Vulkan functional acceptance. Software device:
llvmpipe (LLVM 19.1.7, 256 bits), vendor 65541, observed Vulkan raw driver version 1.
The exact device/workload observations are retained in [stress.csv](stress.csv).
Physical minimum-spec PC 60 FPS and Windows clean-machine Shipping are **NOT RUN**.
Their open target-host task is `nexora-j4q`; hardware and spike budgets remain TBD.

## Reproduction and evidence

```bash
cmake --preset linux-development -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_ENABLE_SLANG=ON
cmake --build --preset linux-shipping
```

Linux display gates require Xvfb, Vulkan/llvmpipe and Slang. The cloud session used workspace-local
CMake 3.31.6, Ninja 1.11.1, GCC 14, Slang 2026.18, Xvfb 21.1.16 and Mesa 25.0.7;
no machine-specific preset/tool path is committed. Full Development gate: **262/262 passed**,
806.93 seconds; no skips. Subsequent boundary/refinement regressions: **7/7 passed**, with the
X11 asynchronous readback correction additionally passing **five consecutive native pixel runs**.
Shipping configure/build and staged package acceptance pass. Equivalent CI uses its desktop
toolchains; it cannot substitute for physical PC or clean-machine tests.

✅ `twod.runtime_contracts`: canonical Atlas/clip codecs, deterministic import/Cook/bundle generation,
invalid data/publication, entered-frame events/switch/pause/loop/large steps, independent collision
oracle, grid rebuild/filter/sweep/large query, World affine/pivot/flip/culling/snapshot/Play, transient
pool stale/foreign/reentrant lifecycle and fixed-step partition/remainder/backlog behavior.
✅ `twod.authoring_contracts`: whole-document Undo/Redo/save/reopen, reference validation,
read-only and truncated/invalid edits, isolated Play, layered Tilemap painting/collision,
negative chunk coordinates/loading/culling and history preservation.
✅ `twod.asset_tool`: real RGBA8 PNG rectangle/grid slicing, repeated/reordered packing and clip
bytes, malformed PNG/path rejection and failed publication preserves prior output.
✅ `twod.graphical_controls`: actual ImGui Undo/Redo button events, permission rejection, all four
Studio tabs and animation-preview state.
✅ `twod.native_pixels`: actual Vulkan textured/indexed triangles; cross-atlas order, source texture
alpha plus tint alpha, HUD, all four UV flips with noncentral pivot, nearest/linear sampling/gutters,
immutable reuse/retire/reload/alias rejection, and negative Tilemap chunk render/query boundaries.
The retained [capture](sprite-alpha-hud.png) shows opaque red → half-alpha green texture →
half-alpha red tint (linear 0.75/0.25/0, sRGB encoded) plus blue HUD.
✅ `twod.cloud_native`: cooked PNG/clip assets load asynchronously, mixed update/query/animation/
recycle/reload runs on native Vulkan; existing EditorImGuiHost draws Scene/Atlas/Animation/Tilemap
Studio tabs and saves/reopens the isolated document. Retained [metadata](scope.json) names the
scope and sample counts; generated per-run logs/assets/documents stay in build artifacts.

The workload contains 100/1,000/5,000/10,000 objects, 1,000 overlapping translucent sprites and
10,000 sparse objects. Each case uses four warm-up frames and 24 measured frames. It records
CPU update/query/extraction intervals, full frame average/p95/p99/max, unique completed GPU
command-buffer intervals, load/reload time, process RSS, batches, visible/culled and covered-AABB
ratio. GPU completion lag explains fewer retained GPU intervals than measured submissions; no
calibration, physical display latency, allocator-total or fragment-invocation count is inferred.
These short software-GPU runs establish repeatability/correctness, without a 60 FPS claim.

✅ Optional OFF: EditorCore builds with `NEXORA_ENABLE_2D=OFF`; headless configure forces TwoD OFF.
✅ Monolithic Shipping: [StageValidation.py](../../../../Tools/TwoD/StageValidation.py) stages the
runtime executable and cooked Atlas/clip, verifies SHA-256 files, removes development library
search paths, runs all six CPU workloads and rejects a corrupted cooked file. No Editor or
Nexora shared engine dependency is loaded. [package-scope.json](package-scope.json) retains
file hashes and scope without local paths. Target OS runtimes/graphics drivers remain required;
Windows redistributables and native clean-machine operation are unverified.

## Remaining target-host acceptance

M6: declare minimum CPU/GPU/RAM/Windows/backend/driver/resolution/frame pacing and acceptable
16.67 ms frame/spike budgets, then run sustained mixed stress on that physical PC.
M7: clean Windows DX12 and separately offered Vulkan, keyboard/controller, text/UI, actual audio,
async load completion/failure, reload, save/reopen, display/DPI, runtime prerequisites and shutdown.
Main Editor project lease/recovery integration and full ED milestones remain separate.

Cloud capability acceptance is M0–M5/M8–M10 (9/11, P0 6/8). M6/M7 remain unaccepted;
this does not change V1 foundation or main Editor acceptance percentages.
