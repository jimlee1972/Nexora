# Standalone 2D gameplay Player

`NexoraTwoDPlayer` is an engine-owned host for an explicitly selected trusted V3 gameplay library.
It builds with `NEXORA_ENABLE_2D=ON`, independently of the Zig compiler and the Editor. A game can
implement its rules entirely in Zig using the existing public tables. No sample entities, test
Atlas, scripted movement or automatic module reload are injected in project mode.

```sh
NexoraTwoDPlayer --run-2d-project /path/to/game.nx2game
NexoraTwoDPlayer --run-2d-project /path/to/game.nx2game --frames 60
```

The first command presents until native window close. `--frames N` explicitly selects a bounded
headless run (1..100000); add `--native` for actual bounded presentation. Headless runs do not
poll system gamepads; native runs retain physical gamepad input. `--report FILE` retains
JSON observations. Invalid/missing content and callback failures exit nonzero without a success
report. The native-enabled `NexoraProjectPlayer` accepts the same `--run-2d-project` command when
2D is enabled; its verification-only build retains the existing minimal dependency boundary.
Windows uses `wmain` and UTF-16 native paths; the command boundary converts to UTF-8. Package roots,
module source directories, descriptor titles and save directories can contain Unicode. Generated
relative package filenames use ASCII to preserve the existing VFS contract on Windows.

Native runs accept `--backend auto|dx12|vulkan|metal`; `auto` retains the platform default.
An explicit backend requires native presentation (including the default unbounded project mode),
and an unavailable requested backend fails with its surface error instead of falling back.
Vulkan must be enabled in the build. `surface_backend_requested` records the request, not a
claim that `auto` selected a particular API. Reports also retain available native device IDs,
DPI scale range, focus/key/mouse/wheel event counts and unfocused movement frames. These are
observations, not an automatic physical-input, appearance or audibility acceptance.

## Export a game

Build `NexoraTwoDPlayer` using a target-host Monolithic Shipping preset, and build the Zig DLL
separately using the same target architecture and the public V3 binding. Create `game.json` next
to the module and your own assets:

```json
{
  "schema": 1,
  "id": "com.example.mygame",
  "title": "My Game",
  "scene": "Main",
  "module": "bin/gameplay.dll",
  "audio": "optional",
  "tile_atlas": [17, 23],
  "assets": [
    {"kind": "atlas", "path": "Content/World.nx2atlas"},
    {"kind": "clip", "path": "Content/Walk.nx2clip"},
    {"kind": "wave", "id": 1, "path": "Content/Hit.wav"},
    {"kind": "font", "id": 1, "path": "Content/UI.nxfont"}
  ]
}
```

Use your Atlas's actual full `[high, low]` ID for `tile_atlas`, or omit it if no Tilemap is needed.
Without it tile callbacks return `UNSUPPORTED`. Game logic names the configured startup scene via
`load_scene`; this host provides one scene, a 1024-instance pool and copied component access.
Other scene names and rendered/physics spawn descriptors return unsupported. An empty asset list
and UI-only modules are valid. Atlas/Clip identities and dependencies must match their payloads;
all Atlases are admitted before Clips, independently of descriptor order. No source art is bundled
by the engine. The game supplies its own assets and redistribution licenses.

From a Windows x64 MSVC developer shell:

```powershell
python Tools/TwoD/ExportGame.py path/to/game.json `
  --runtime build/windows-showcase-shipping/bin/NexoraTwoDPlayer.exe `
  --compiler build/windows-showcase-shipping/bin/NexoraTwoDAssetCompiler.exe `
  --vc-redist "$env:VCToolsRedistDir/x64/Microsoft.VC143.CRT" `
  --output dist/MyGame
```

With the Visual Studio multi-config generator, the executable directory is
`build/windows-showcase-shipping/bin/Shipping/`. Substitute that directory in the example above;
`bin/` is the single-config layout. See the [tested MSVC commands](../../Tools/TwoD/WindowsAcceptance.md)
for the explicit Visual Studio 2022 configuration and matching CRT selection.

The export validates contained relative source paths, rejects symlinks/traversal, cooks canonical
`.nx2atlas`/`.nx2clip` sources (or copies existing `.nxcook`), and copies the executable, selected
module, WAV and font atlases to a fresh output. It checks dependencies with `dumpbin` on Windows
or `ldd` on Linux, rejects Editor/shared engine dependencies in Shipping, and copies imported
MSVC runtime DLLs from the explicitly supplied redistributable directory. Output is published
only after staging succeeds. `inventory.json` records SHA-256 hashes. Double-click `Run.cmd` to
launch; no Python, compiler, Editor or source project is needed at runtime. OS/graphics/audio
prerequisites and redistribution licensing remain the distributor's responsibility.
`--allow-development` is for automated internal tests only: its inventory marks that fact and
it can copy engine libraries/use the build host CRT. It is not a standalone Shipping package.

When the executable or a module imports `vulkan-1.dll`, also pass
`--vulkan-loader path/to/redistributable/vulkan-1.dll` with the matching architecture. The Vulkan
loader is not a Windows OS DLL: a static import requires it before `main`, including headless
or DX12 execution. Export copies only this explicitly supplied loader, recursively checks its
imports and includes its hash in the inventory. Missing/misnamed loaders and unresolved transitive
imports fail before output publication, including development test copies. A Vulkan-disabled
DX12-only binary needs no loader argument. Host fixture and Zig project acceptance scripts accept
the same option. Supplying the loader does not install a Vulkan ICD/GPU driver or establish
clean-host acceptance; publish a newly hashed package and rerun that host gate. Preserve the
redistribution license obligations of the selected loader along with the existing CRT/assets.
The exporter currently targets Windows and Linux. It never changes existing output directories.

## Text and fonts

Bake a bounded UTF-8 corpus containing all UI strings on Windows with a licensed installed font
or an explicitly supplied licensed font file:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File Tools/TwoD/BakeWindowsFont.ps1 `
  -TextFile Content/strings.txt -Output Content/UI.nxfont -FontName "Microsoft JhengHei"
```

`-FontFile` supports a local TrueType/OpenType collection through private GDI registration; its
first family is selected. `-PixelSize` is 4..96 and `-Columns` is 1..32. The baker checks actual
font glyph coverage, rejects missing glyphs and supplementary-plane characters, and publishes
atomically to a new file. It adds space and ignores control characters. It does not silently
substitute tofu or ship a system font file. This is a BMP atlas baker for HUD/menu CJK text,
not general shaping, dynamic OS font lookup or IME composition. Missing runtime glyphs remain
explicit UI errors. Zig modules can also embed the same file with `ui.registerBakedFont` and
caller-owned glyph scratch storage, including in Editor Play. Use a suitable font and retain its license for redistributed raster output.

NXFONT1 is little-endian: eight bytes `NXFONT1\0`; uint32 width/height; float32 pixel size/line
height; uint32 glyph count; `count` records of uint32 codepoint and nine float32 values
(advance, x, y, width, height, u0, v0, u1, v1); then tightly packed RGBA8. Dimensions are at most
4096, RGBA at most 16 MiB, and all metrics/codepoints/UVs pass the existing UI admission. Runtime
font handles are enumerated with `get_font(index)` in descriptor order; descriptor font IDs
1..32 reserve immutable texture identities and are not gameplay font handles.

## Input, audio and durable state

The existing services provide rebindable keyboard/mouse/gamepad actions, pointer/buttons/wheel,
active device observation, focus reset, immediate UI, WAV voices/buses and permanent storage.
Compatibility movement actions are 250/251, fire 252; UI next/previous/activate/cancel are
253..256. Games can clear/rebind these or define their own actions 1..249. Keyboard arrows/Tab,
Enter/Escape and conventional gamepad navigation bindings are supplied. Native window close exits;
Escape remains available to the game's cancel/menu action in project mode.

`audio` is `off`, `optional` (default) or `required`. Required output or explicit `--audio` fails
startup if unavailable. Optional output reports failure and retries once per second. A failed
pump closes the output and retries with bounded timing, preserving registered assets/voice state
and discarding device backlog. This is host recovery, not an audio-thread/device-notification API.
Successful submission does not establish that a person heard the sound.
Reports include `audio_open_attempts`, `audio_open_successes` and `audio_pump_failures`
to distinguish initial output, failed pumps and reopening during a device test. These counters
do not establish audibility or automatic default-device migration.

Default storage is `%LOCALAPPDATA%/Nexora/Games/game-<id>/saves` on Windows and
`$XDG_DATA_HOME/Nexora/Games/game-<id>/saves` (or `$HOME/.local/share/...`) on Linux.
`--save-directory PATH` overrides it. IDs are 1..128 lower-case ASCII letters/digits/dot/underscore/
hyphen, with alphanumeric ends. Prefixing the directory avoids Windows reserved device names.
Use one dedicated directory/writer per project; simultaneous instances are not coordinated.
The game chooses its save slots/schema and when to commit, including before exit. The host never
serializes live entity IDs, pool handles or hot-reload memory as a permanent game save. Existing
storage checks scope/schema envelope/integrity and atomically commits; corrupt state is not
silently replaced by the host. Error handling/migrations belong to the game.

All callbacks, input, mixing, module updates and rendering serialize on the host thread. Stop and
Destroy run while World and services still exist; then voices/output stop, the GPU drains and
Engine shuts down. Frame extraction globally sorts scene sprites, pooled instances and configured
tiles. Module-provided UI follows sprites in the same native submission. Assets load through VFS
with a combined 64 MiB payload budget; unknown/missing/corrupt/type/dependency failures reject.

See [the Windows acceptance list](../../Tools/TwoD/WindowsAcceptance.md) for the remaining physical
checks. Linux software Vulkan and hosted Windows CI do not establish physical PC acceptance.

## Display, DPI and safe viewport

Projects may provide optional schema-1 `display` settings for a 1920x1080 window or borderless desktop mode. Omitted settings preserve 640x480. The Player exposes the optional display service before module start, publishes real framebuffer/logical extent and DPI, and uses a game-selected safe viewport consistently for extraction and picking. UI geometry and pointer input convert DPI once. See the [display contract](../../Engine/TwoD/GameplayDisplay.md) and [Windows acceptance procedure](../../Tools/TwoD/WindowsDisplayAcceptance.md).

## Image UI service

The Player exposes the optional [image UI table](../../Engine/TwoD/GameplayUiDraw.md) alongside
unchanged UI V1 and Display. Zig composes images/nine-slice and custom buttons. Removed
image/font handles retire only after owning snapshots expire; the Player releases their immutable
native textures before the next acquire. Native release failures propagate as errors. Upper
65,536 texture IDs are reserved for automatic UI generations.

## Deferred chapter assets (SP-03)

Atlas/Clip entries may specify `"preload": false`. Deferred entries alone form the asset-service
catalog in descriptor order. A `"kind": "blob"` entry is always deferred and supplies opaque bytes
for Zig-owned parsers and existing font/image/audio registration APIs; deferred `wave`/`font`
entries are rejected rather than silently installing those services. Legacy omitted-preload
entries are still admitted at startup within the aggregate 64 MiB preload budget.

```json
{
  "capacity": {
    "active_assets": 256,
    "active_bytes": 100663296,
    "live_bytes": 134217728,
    "staging_bytes": 20971520,
    "pool_instances": 2400
  },
  "assets": [
    {"kind": "atlas", "path": "Content/Chapter.nx2atlas", "preload": false},
    {"kind": "clip", "path": "Content/Walk.nx2clip", "preload": false},
    {"kind": "blob", "path": "Content/UI.nxfont"}
  ]
}
```

This is a fragment of `game.json`; retain its normal required fields. The optional `capacity`
object requires exactly these five integer fields. Limits are 1..4096 identities, 1 byte..1 GiB
active, active..2 GiB live, 1 byte..128 MiB staging and 1..100000 pool instances. Defaults are
256/64 MiB/128 MiB/64 MiB/1024. Each file remains at most 64 MiB; only preloaded files count
toward the aggregate startup budget. Packages may therefore exceed 64 MiB without being loaded
all at once. Cooked identity/hash/dependency and contained-path checks remain in place.

The exported schema-1 descriptor appends `capacity A B C D E` and uses `deferred_atlas`,
`deferred_clip` and `deferred_blob` lines. A configured `tile_atlas` may be initially unavailable
when a deferred Atlas is present; Zig must load the matching UUID before painting/rendering its
cells. No implicit map/dependency load occurs. Old Players reject these new optional directives
explicitly; export and run with matching versions.

The [asset contract](../../Engine/TwoD/GameplayAssets.md) defines request transfer, cancellation,
clip-before-atlas removal, logical RAM/texture budgets and mandatory reader shutdown before VFS
teardown. It does not promise a fixed frame-time streamer or physical RAM/VRAM acceptance.

## Offline voice and decoded PCM capacity

Optional `audio_capacity` requires exactly `assets`, `active_bytes`, `live_bytes` integer fields;
defaults are 256/64 MiB/128 MiB. Live includes old samples held by playing/paused voices. These
are separate from asset staging/content budgets. Use deferred `blob` entries for local WAVs and
`nx.dialogue.Line` to request/register/play/release one line; do not preload a complete voice pack.
The optional AudioState service reports PCM residency and exposes an independent Voice bus.
Audio V1 and original/SP-01/SP-02/SP-03 services prefixes remain supported. See
[voice integration and limits](../../Engine/Runtime/GameplayVoice.md). This is per-line/chapter
PCM admission, not compressed streaming or verified speaker output. Missing declared install
files still fail startup validation; unmapped dialogue and runtime IO/output failures can use subtitles.

## Optional lifecycle and close confirmation

Projects can negotiate Lifecycle V1 and explicitly enable a close guard from Zig. The Player then
keeps the native window alive until Zig resolves the current close token; saving and error/retry UI
remain game policy. Modules that do not enable the guard retain immediate close. Minimized/occluded
frames still call gameplay to observe transitions and save, without drawing. Observe lifecycle before
each fixed tick to prevent background combat advancement. XInput rumble expires and is cleared on
interruption/close/Stop; unsupported devices report capability honestly. See
[the lifecycle contract and Zig diagnostic](../../Engine/Runtime/GameplayLifecycle.md).
