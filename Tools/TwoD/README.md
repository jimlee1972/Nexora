# 2D asset, authoring and validation tools

Build with `NEXORA_ENABLE_2D=ON` (enabled by default with its dependency closure).
`NexoraTwoDStudio` also requires `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON`.
Linux native rendering requires a display, a Vulkan driver and `NEXORA_ENABLE_SLANG=ON`.
The tools contain engine fixtures and authoring controls; no game mechanics are included.

## Import and Cook

```bash
python3 Tools/TwoD/PrepareAssets.py Tools/TwoD/Fixtures/atlas.json --compiler build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --output work/atlas.nx2atlas
python3 Tools/TwoD/PrepareAssets.py Tools/TwoD/Fixtures/clip.json --compiler build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --output work/clip.nx2clip
build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler cook work/atlas.nx2atlas work/atlas.nxcook
build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler cook work/clip.nx2clip work/clip.nxcook
```

Create the output directory first. The repository fixture is procedural RGBA8 artwork.
Atlas manifests declare `type: "atlas"`, `id: [high, low]`, width, optional maximum_height/nearest,
and images. Each image uses a workspace-contained `path` plus either `grid: [width, height]`
with optional first_id, or `regions: [{id, rect: [x, y, width, height]}]`. Coordinates are PNG
pixels from the upper-left. Only bounded noninterlaced RGBA8 PNG is supported; byte values are
preserved as the renderer's linear RGBA8 contract, without ICC/gamma conversion. IDs, layout,
gutters and bytes are independent of manifest input order. The C++ compiler rejects duplicate
IDs, capacity failures and invalid frames with a diagnostic and leaves the previous output intact.

Clip manifests declare `type: "clip"`, id, atlas UUID, optional loop and ordered frames with
region, seconds and optional ASCII event. See [fixtures](Fixtures/) for complete examples.
Preparation uses only Python's standard library; `.nx2atlas`/`.nx2clip` validation and cooking
use existing Runtime import/Cook services. Publication uses Core VFS atomic writes on all desktops.

## 2D Studio

```bash
build/linux-development/Tools/TwoD/NexoraTwoDStudio --workspace work --atlas atlas.nx2atlas --clip clip.nx2clip --document scene.nx2doc
```

Studio uses the existing EditorImGuiHost/native Presentation and TwoDDocument/Runtime UndoStack.
Its Scene tab places sprites and edits position, scale, size, pivot, color/alpha, flips, sorting and
colliders; the orthographic preview draws collision bounds. The Atlas tab previews/selects regions
from sliced imports. The Animation tab edits duration, events, looping and frame order with
pause/playback/event preview. The Tilemap tab paints/erases layered collision cells and unloads
or reloads their 16x16 chunk. Save/Open preserves all authored metadata and assets; Undo/Redo
is bounded to 256 transactions and 64 MiB retained bytes. A new transaction resets older history at that budget boundary; a single transaction exceeding
64 MiB rejects. Reopen clears history after validation.
An empty document can be opened without an atlas, then imported through the configured paths.

Use a dedicated authoring directory. Paths are relative to that directory, canonicalized again
before IO and rejected if they escape it. Existing project lease/recovery policy remains the
responsibility of a host embedding the public panel; this tool does not acquire or override a
main Editor project lease. `--read-only` disables edits/publication; `--smoke --frames 12` runs an
isolated synthetic save/reopen/native fixture and requires a writable workspace. The public panel
emits IO requests; the host checks access and revision before publishing. Atlas previews larger
than 1024 pixels are downsampled to fit the existing bounded Editor texture registry; runtime
assets keep full resolution. Surface texture slots are reused through the existing host lifecycle.

## Mixed stress and standalone Shipping

```bash
build/linux-development/Tools/TwoD/NexoraTwoDBenchmark --native --atlas work/atlas.nxcook --clip work/clip.nxcook --frames 48 --report work/stress.csv
cmake --preset linux-shipping
cmake --build --preset linux-shipping
python3 Tools/TwoD/StageValidation.py --runtime build/linux-shipping/Tools/TwoD/NexoraTwoDBenchmark --compiler build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --atlas work/atlas.nx2atlas --clip work/clip.nx2clip --output work/package
```

Without `--native`, the benchmark runs portable CPU contracts. Optional cooked files load through
Core VFS asynchronous reads and existing hash/schema/bundle-generation admission. It updates
animations, fixed-step pools and circle queries, extracts/batches/culls sprites, recycles full
pools and retires/reloads immutable atlases halfway through each case. Reports include initial
load/reload time, four warm-up frames, CPU median/p95/p99/max, complete frame average/p95/p99/max, unique completed native GPU samples,
GPU p95 when available, Linux process RSS peak (zero means unavailable), visible/culled counts,
Sprite batches and covered AABB ratio. GPU time measures native command buffers, not display
latency; CPU time measures the update/query/extraction section. Native draw counts also include
the background. Whole-process RSS includes drivers; no total allocator count is inferred.

StageValidation copies the monolithic executable and cooked assets to a fresh directory, emits
SHA-256 hashes, removes development library search paths, runs the staged CPU workload and rejects
a deliberately corrupted cook. It checks unresolved/engine shared dependencies on Linux. The
package still needs the target OS runtime and graphics driver; Windows redistributables and a
clean-machine native run remain target-host work. Run the staged executable with `--native` for
native presentation on an equipped host.

CTest `twod.cloud_native` retains CSV, native Studio log, source/cooked assets, saved document and
`acceptance.json` under `build/<preset>/artifacts/twod-cloud/`. `twod.native_pixels` retains its
cross-atlas alpha/HUD capture under `artifacts/twod-native/`. Missing Xvfb is a local skip and a CI
failure; no skipped test constitutes acceptance. Physical PC 60 FPS and clean Windows acceptance
remain NOT RUN. See [module contracts](../../Engine/TwoD/README.md) and
[2D roadmap](../../Roadmap/en/2D_Roadmap.md).
