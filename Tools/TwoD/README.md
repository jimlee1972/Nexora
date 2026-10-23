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
preserved as the renderer's linear RGBA8 contract by default (legacy behavior).
For authored sRGB PNGs, explicitly set atlas `"color_space": "sRGB", "alpha_mode": "straight"`.
RGB uses the IEC 61966-2-1 transfer before slicing, packing and gutter extrusion; alpha is unchanged.
`"linear"` is the only other accepted color space. Unknown values and premultiplied alpha reject
before publication. Untagged PNGs use the explicit manifest interpretation. In sRGB mode, standard
sRGB intent/gAMA/cHRM metadata is checked; conflicting gamma/chromaticities, duplicate profile
chunks and all ICC profiles reject. Normalize ICC/Display-P3 assets externally first. The legacy
linear mode continues to ignore ancillary metadata. No automatic profile guessing occurs.
Cooked Atlases remain linear RGBA8, with unchanged wire/ABI and no second decode at sampling.
This quantizes dark gradients: it is a bounded compatibility path, not lossless sRGB storage or
HDR. Inspect final dark art; a future tagged sRGB texture/high-precision path requires separate work.
See [color/fallback contracts](../../Engine/TwoD/ColorAndFallback.md). IDs, layout,
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

Studio checks `CloseRequested()` again after `BeginFrame()` pumps native events. A normal close
leaves the frame loop before validating acquisition status, then drains/destroys the surface;
unrelated acquire/render/present failures still fail. Linux `twod.cloud_native` additionally waits
for a rendered Studio window, sends `WM_DELETE_WINDOW`, and requires exit 0 plus the completed
native/document lifecycle message. This covers the Windows VM close-button exit-1 regression.

Use a dedicated authoring directory. Paths are relative to that directory, canonicalized again
before IO and rejected if they escape it. Existing project lease/recovery policy remains the
responsibility of a host embedding the public panel; this tool does not acquire or override a
main Editor project lease. `--read-only` disables edits/publication; `--smoke --frames 12` runs an
isolated synthetic save/reopen/native fixture and requires a writable workspace. The public panel
emits IO requests; the host checks access and revision before publishing. Atlas previews larger
than 1024 pixels are downsampled to fit the existing bounded Editor texture registry; runtime
assets keep full resolution. Surface texture slots are reused through the existing host lifecycle.

## Mixed stress and standalone Shipping

Native `NexoraTwoDBenchmark` accepts `--backend auto|dx12|vulkan|metal`, matching the standalone
Player. Explicit requests require `--native` and fail if unavailable; no backend fallback is
accepted. The CSV's appended `backend_requested` column records the request (`none` for CPU-only
runs); existing device IDs/name and GPU timing identify the observed device independently.

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

Optional `--frame-trace FRAMES.csv` retains every frame, including explicitly marked warm-up
frames, with its CPU/full interval and eight contiguous stages: reload, BeginFrame, population,
spatial build, query/sweep, extraction, draw and EndFrame/metrics. The ordinary aggregate CSV and
all six workloads remain the same. Stage clocks add measured instrumentation cost; samples use
bounded per-case storage (at most 604) and write only after that case drains. Full intervals retain
their original boundaries through post-Present timing collection, excluding local destruction
and trace output. BeginFrame includes window event pumping and surface acquisition; it is not a
GPU-wait measurement. GPU samples in the aggregate report belong to completed earlier submissions.
Use the same frame's stages to investigate spikes, rather than combining independent maxima.
The trace path must differ from the report (including an existing file alias); open/write failures
exit nonzero. CPU-only runs support the same trace for portable regression checks.

StageValidation copies the monolithic executable and cooked assets to a fresh directory, emits
SHA-256 hashes, removes development library search paths, runs the staged CPU workload and rejects
a deliberately corrupted cook. It checks unresolved/engine shared dependencies on Linux. The
package still needs the target OS runtime and graphics driver; Windows redistributables and a
clean-machine native run remain target-host work. Run the staged executable with `--native` for
native presentation on an equipped host.

CTest `twod.cloud_native` retains CSV, native Studio log, source/cooked assets, saved document and
`acceptance.json` under `build/<preset>/artifacts/twod-cloud/`. `twod.native_pixels` retains its
cross-atlas alpha/HUD capture under `artifacts/twod-native/`. Missing Xvfb is a local skip and a CI
failure; no skipped test constitutes acceptance. [Local physical Windows evidence](../../Engine/TwoD/evidence/Windows-2026-10-14/acceptance.md)
retains older developer-PC DX12 screening. The [rebuilt Windows VM slice](../../Engine/TwoD/evidence/Windows-VirtualBox-Current-2026-10-16/acceptance.md)
passes CPU/cooked/corruption checks, native Microsoft software DX12, Zig GPU instancing and visible
desktop presentation; a human also confirms actual keyboard movement/Escape. It uses the existing
VC++ Runtime prerequisite. Separate Monolithic Development Studio also has human four-tab/text/preview and Save/Open evidence,
independent read-only file checks and a repaired close-button path verified to exit 0. These are
supporting authoring checks. Full M7 remains PARTIAL / NOT ACCEPTED; controller, audible device audio,
integrated product/UI/text/save/shutdown and display/DPI stay open in that VM slice. Vulkan is unavailable there.
Software VM timing is not physical M6 acceptance. See [module contracts](../../Engine/TwoD/README.md) and
[2D roadmap](../../Roadmap/en/2D_Roadmap.md).

## Selected integrated load and sustained native validation

The opt-in `NexoraTwoDBenchmark --integrated --frames 600 --reload-every 300 --report load.csv
--frame-trace frames.csv` runs one integrated fixture; omitting `--integrated` preserves the original
six cases, report schema, workload and 4..600 frame limit. Integrated mode accepts
`--objects 1000|5000|10000` (default 10000); it updates every selected object, builds all
corresponding query shapes, performs circle/sweep queries, animation and extraction,
then draws 256 moving translucent VFX and a real `GameplayUi` overlay in the same native pass.
The two Sprite atlases/batches are followed by 16 panels, eight buttons and eight ten-glyph labels
(120 UI quads/commands). Deterministic navigation/activation validates UI service execution, not
physical input. The supplied readable ASCII font and two-region animation are fixtures, not
production art/fonts. No gameplay, GPU simulation or skipped CPU updates are introduced.

At each reload cadence the full pool is recycled with stale-handle rejection, an 8 MiB allocation
is touched at 4 KiB intervals plus its last byte (volatile stores survive LTCG) and freed through
`TrackingAllocator`, the Atlas codec is decoded again, and both old
GPU atlas generations are retired before new IDs upload. This is in-memory loading churn, not
asynchronous disk IO. Mixed and sparse layouts alternate at these boundaries, preserving the
original spacing, sizes, movement and camera. At the declared 300/600-frame cadence, mixed/sparse visible counts including VFX are
1,256/406 for 1,000 objects, 5,256/451 for 5,000, and 9,956/451 for 10,000. Every selected
ordinary object is updated/queried in either layout. Fixed simulation time is 1/60 s per
rendered frame; it is not a real-time gameplay clock.

`--soak-seconds 1800` runs for at least 30 measured minutes after 1,200 warm-up frames, with no
duration-dependent in-memory sample buffer. It also satisfies the supplied minimum `--frames`.
Duration/cadence accept 1..14400; these options require integrated mode, which requires a distinct
trace path. Native integrated runs reject software rendering and viewport changes. Any failed
update, query, UI command, submission, resource retirement, drain or output write exits nonzero.
The original native APIs/ABI and engine lifetime contracts are unchanged.

Every frame is retained in CSV through one pending record. Trace output of the preceding record,
frame-local destruction and every-60-frame resource polls are included in the full interval;
`cycle_ms` spans frame start to next frame start and includes loop/trace overhead. The final record
uses the loop's final boundary. Nine work stages are contained in the full interval; their sum
excludes trace output, destruction and resource polls. CPU spans simulation through UI building.
Native GPU intervals identify unique earlier completed submissions, excluding warm-up submissions
from measured GPU aggregates; they do not measure same-frame GPU work or display latency.

Resource observations are optional fields, never fabricated zero: process RSS on supported hosts;
Windows private commit/handle count and DXGI node-0 local/nonlocal process usage on a unique matching
PCI vendor/device adapter. Local usage/budget is WDDM accounting, not physical VRAM residency or
per-texture allocation. `fixture_alloc_*` covers only the explicit loading allocation, not STL,
driver or total engine allocations. Native buffer reallocation/texture upload/rejection counts
are cumulative engine diagnostics; RSS/commit include drivers. Driver memory identity is retained
as exact integer CSV text. No global allocator hooks or platform security/power settings change.

```bash
python3 Tools/TwoD/RunIntegratedAcceptance.py --benchmark path/to/NexoraTwoDBenchmark.exe --output fresh/evidence --objects 5000 --seconds 1800
```

The runner writes a fixed plan with executable/source hashes before executing two repetitions of
all original six-case screens, two integrated 600-frame screens (reload every 300), then one
continuous soak per backend (reload every 600). DX12/Vulkan run sequentially. It retains every exit,
frame and failure, checks every selected update/native counter and rejects missing samples/wrong devices
or a summary/trace from another load. Runner defaults to 10,000 for historical compatibility;
select 5,000 explicitly for the current-PC M6 gate. 1,000 is functional smoke only. All six original
screen cases retain correctness checks and individual timing verdicts; only cases at or below the
selected object count gate performance, so an extra 10,000-object timing failure does not block
the 5,000-object gate. Original nonzero exits, wrong devices and correctness failures still fail.
No historical report or verdict is rewritten.
Default expected hardware is this developer PC's NVIDIA 4318/5121; explicit IDs select another host.
Full/cycle and each phase must meet p95/p99 <=33.3333 ms and max <=66.6667 ms. Soak resource limits are
512 MiB sampled process RSS/private/local GPU usage, first/last five-minute median growth <=32/32/16 MiB,
<=4096 handles with growth <=8, and <=8 native buffer reallocations without median growth. These
are predeclared current-host fixture budgets; product minimum hardware, full allocator accounting,
production content and M7 device/clean-host acceptance remain independent. Stop local builds/other
native fixtures during measurement; ordinary desktop apps may remain open. Analysis runs only
after all native processes finish. Never retry until passing or omit a measured spike.


## Device audio probe

`NexoraTwoDAudioProbe --frames 48000` submits a quiet 440 Hz tone through the default Linux ALSA
or Windows waveOut output, closes/reopens it, and verifies unavailable playback after close.
It links Runtime only and is included in Monolithic Shipping without Editor libraries. Use
`--expect-unavailable` on a machine/configuration intentionally lacking output; ordinary device
open/write failure exits nonzero. Frame count is 1..480000 at 48 kHz (default one second).
The JSON reports device submission separately from `audible_acceptance: NOT VERIFIED`; a human
must listen on the target device. No OS notification sound or null device counts as audible M7
acceptance. The existing Zig 2D host's `--audio` uses this same backend. Ordinary modules
register/play their WAV through the optional service; the explicit cooked validation mode below
also provisions its supplied WAV in the host fixture.

Linux `twod.audio_device_lifecycle` runs the actual ALSA adapter with a null output and an explicitly
missing device, preserving JSON/stdout/stderr under `artifacts/twod-audio/`. This checks ownership,
reopen and honest failure; it does not accept Windows playback or physical speakers. See the
[device contract](../../Engine/Runtime/GameplayAudio.md).

## Integrated cooked service validation

Build the optional real Zig consumer with `NEXORA_ENABLE_ZIG_GAMEPLAY=ON` and Zig 0.14. Then stage
an Editor-free Monolithic Shipping host and reproducible fixture assets:

```bash
python3 Tools/TwoD/StageProductValidation.py --runtime build/linux-shipping/Gameplay/Zig/NexoraZigTwoD --compiler build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --output work/twod-product
work/twod-product/NexoraZigTwoD --validation-package work/twod-product --save-directory work/twod-product-saves --audio --interactive
```

Use a fresh package output and dedicated writable save directory. Windows uses the corresponding
Shipping EXE and requires `dumpbin` in the staging developer shell to check imports; the staged
application does not require that tool. Linux staging checks `ldd`. Development copies with engine
libraries are permitted only by the internal test helper and explicitly labeled as such. No Editor
library may be admitted. The staging tool supports Linux and Windows; it preserves target OS runtime
and driver prerequisites and does not claim a clean Windows run.

`--validation-package` is an explicit static-sample fixture mode. Atlas/Clip IDs are fixed to
`[1,2]`/`[3,4]`, with `step` animation events. Assets pass real PNG preparation, import/Cook and
hash/type/identity/dependency checks before registration; the host reads Atlas, Clip and WAV through
Core VFS asynchronous IO. Missing/invalid data fails instead of falling back to the startup fixture.
The mode adds readable 5x7 ASCII fixture text, native SAVE/OPEN buttons and optional WAV device
playback. It does not supply production fonts/art or CJK typography. `--audio` explicitly requests
output; omitting it still validates WAV decoding without claiming device submission.

Interactive movement uses the existing keyboard/gamepad action bridge. SAVE records the current
fixture entity position; OPEN restores it and rebuilds the query snapshot. Startup resumes a valid
existing slot. Orderly exit saves, stops module callbacks and audio voices/output, destroys the old
storage owner, verifies a fresh-owner reopen, then drains the native surface before engine shutdown.
The app's schema-1 save stores two finite bounded little-endian float64 X/Y values, never entity
IDs, pool handles or pointers. This is fixture state, not a generic project export or complete scene
save. Corrupt/foreign save data rejects without silent replacement. Immutable byte-identical cooked
reload and module migration keep resource owners/IDs stable.

`twod.cooked_product_services` stages a Development test copy, runs two separate processes and
checks save/resume plus corrupt Cook, wrong asset type, missing asynchronous WAV and corrupt save
rejection. Linux also uses ALSA null output. `twod.cooked_product_native` additionally checks actual
Vulkan text pixels, native mouse SAVE/OPEN, keyboard movement, 900 UI/Sprite frames, window resize,
audio submission and teardown. Reports/screenshots live under `artifacts/twod-product*/`. An
isolated Shipping test uses `ProductAcceptance.py --shipping --native` with the same options.
The native driver waits for the presented glyph, then uses bounded physical XTEST key pulses
and mouse SAVE clicks until a checksum-valid schema-1 slot contains positive X movement. Only
then does it release the key and click OPEN; final position must match the acknowledged SAVE.
Missing movement or publication still fails, and host stderr is retained on acknowledgement failure.
This replaces the assumption that one key press plus a fixed delay proves a simulation update.
Software Vulkan, fixture text and null audio do not accept physical Windows devices, DPI/font
appearance, minimum-PC performance or the full M7 target-host gate.


The existing Windows Shipping CI job also builds/stages this host, runs CPU cooked-service checks
without device/audio claims, and uploads the `twod-windows-shipping` artifact. Its clean
`NexoraTwoD-Shipping/` package includes `README.txt` with the native interactive command and controls;
acceptance logs/test copies are stored separately. The target host still needs OS runtime/drivers,
physical sound/controller checks and DPI/clean-machine acceptance. CI does not mark M7 accepted.

## General game export and Windows fonts

The independent [2D Player](../../Apps/TwoDPlayer/README.md) now accepts an explicit game descriptor,
separately built trusted Zig/V3 module and project-owned cooked Atlas/Clip, WAV and font atlases.
`ExportGame.py` stages a fresh Monolithic Shipping directory with dependency checks, matching MSVC
runtime copies, a launcher and hash inventory. `BakeWindowsFont.ps1` uses an explicitly selected
Windows GDI font/corpus to produce a bounded real glyph atlas; missing glyphs fail. Neither path
requires the fixed service-validation artwork. See the guide for commands, format/lifetime/limits
and the [Windows physical acceptance list](WindowsAcceptance.md) for target-host work.

Windows exports that import `vulkan-1.dll` require explicit
`--vulkan-loader path/to/redistributable/vulkan-1.dll`. The loader is copied, its imports checked
recursively and its hash inventoried; it is never assumed to be a Windows OS component or
borrowed from PATH. Missing/misnamed/unhandled transitive dependencies reject publication.
`HostAcceptance.py`, `ProjectAcceptance.py` and `ZigProjectAcceptance.py` forward the same option.
Vulkan-disabled binaries need no loader. This distribution repair does not change old frozen
packages or establish clean Windows startup; regenerate and verify the new package on that host.

`WindowsVmAcceptance.py` drives an existing, normally powered-off VirtualBox Windows VM through
two cold boots of the exported UI/independent-Zig Shipping fixtures. See the
[VM procedure](WindowsAcceptance.md#既有-virtualbox-vm-的可重複驗收) for its exact ZIP layout and command.
The host owns one OS lock per VM UUID and captures VBoxManage output as bytes; guest operations are
serial, bounded, and never retried after an uncertain timeout. Each run uses a unique directory,
strict UTF-8/NUL rejection, hash verification, flushed staging and immutable reports. Child stdout
and stderr drain concurrently, and actual exit codes are required. Only fresh corruption copies
are modified; existing saves, saved states and snapshots are preserved. A failed cleanup never
forces poweroff. PASS requires every case and an observed normal poweroff after each boot, and
does not accept physical devices, a pristine OS or product performance. The tool neither builds
nor substitutes the supplied Player binaries; record their frozen source/hash separately.
Native cases wait for Guest Additions desktop run level 3, then use a unique InteractiveToken
scheduled task in the already logged-in user's desktop. Guest Control's Session 0 cannot establish
native window acceptance. The task stores no password, requires its real completion result and a
positive desktop session, and is removed after completion. The tool does not configure auto-login.
The close case waits for two known ProjectModule panel pixels before requesting native close;
window creation or a fixed delay alone does not prove that DX12 presented a frame.
Cleanup failure reports an unknown final state and its last observed state; a prior cycle's
poweroff must never stand in for the current failed boot/shutdown.

## Portable corpus font baking

Install `python3 -m pip install -r Tools/TwoD/font-requirements.txt`, then run:

```sh
python3 Tools/TwoD/BakeFont.py --font /path/to/licensed-font.otf \
  --text /path/to/ui.txt /path/to/subtitles.txt /path/to/japanese-names.txt \
  --pixels 32 --width 2048 --output /path/to/new-ui.nxfont --report /path/to/new-coverage.json
```

Outputs must be new files. `--audit-only` emits coverage/packing evidence without a font.
All UTF-8 scalars (excluding LF/CR/tab) must exist in the font's cmap; malformed UTF-8, missing
glyphs, unsupported controls, atlas overflow and budget excess fail explicitly. No fallback
fonts or synthetic glyphs are substituted. The source font license remains the application's
responsibility; distribute it with baked fonts. This tool emits one bounded atlas and reports
whether the corpus fits 16 MiB/65,536 glyphs, using one of 32 active host font slots. Larger
corpora require explicit partitioning/font selection; the tool does not silently omit text.
Registration/removal and Zig composition are described in [GameplayUiDraw](../../Engine/TwoD/GameplayUiDraw.md).
The existing Windows GDI baker remains available; cloud FreeType raster output is not a claim
of identical GDI output or accepted Windows typography.

### SP-03 deferred export

`ExportGame.py` preserves optional capacity settings and deferred Atlas/Clip/opaque-byte entries.
It checks each source/cooked file against 64 MiB, but applies the aggregate 64 MiB limit only to
preloaded files. It stages/copies/hashes every packaged file without holding all payloads resident.
See [the Player descriptor](../../Apps/TwoDPlayer/README.md#deferred-chapter-assets-sp-03) and
[the asset lifecycle contract](../../Engine/TwoD/GameplayAssets.md).
