# Zig gameplay and 2D engine control

Zig 0.14.0 is the pinned compiler. C++ owns main, Engine, World, input, asset residency,
fixed/update scheduling, native rendering and shutdown. Zig consumes
[`Bindings/Zig/nexora.zig`](../../Bindings/Zig/nexora.zig), the mirror of the canonical
[`nexora/nexora.h`](../../Engine/API/include/nexora/nexora.h). No C++ ECS storage, GPU/window
object or allocator ownership crosses this boundary.

## Build and run

```bash
cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=OFF
cmake --build --preset linux-development --target NexoraZigTwoD
build/linux-development/Gameplay/Zig/NexoraZigTwoD --frames 60
build/linux-development/Gameplay/Zig/NexoraZigTwoD --frames 60 --module build/linux-development/Gameplay/Zig/libNexoraZigTwoD.so
build/linux-development/Gameplay/Zig/NexoraZigTwoD --interactive
```

Native Linux execution requires Window/Presentation, a working Vulkan backend and compiled UI
shaders (`NEXORA_ENABLE_SLANG=ON`). WASD moves the Sprite, Space pauses animation, Escape exits.
`--interactive --frames 600` gives a bounded native input run; `--native --frames 60` uses scripted
input. `--report path.json` retains a report. Windows/macOS artifact suffixes are `.dll`/`.dylib`;
build with their host toolchains. `NEXORA_ENABLE_2D=OFF` excludes this sample and bridge; the
existing 3D `game_module.zig` and ABI smoke remain available. Dedicated/headless presets strip
TwoD, while this sample's CPU mode extracts geometry without creating a window.

```bash
cmake --preset linux-shipping -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=OFF
cmake --build --preset linux-shipping --target NexoraZigTwoD
build/linux-shipping/Gameplay/Zig/NexoraZigTwoD --frames 60
```

Shipping links the Zig object statically and does not require the Development module library.
The same V3 create/start/fixed/update/stop/destroy contract applies. Keep the bridge and its
borrowed owners alive until module unload. The procedural two-region Atlas/clip in the shared `Apps/TwoDPlayer/TwoDPlayer.cpp` sample path
are bounded validation fixtures; production content still uses the existing cook/generation
pipeline and an embedding-owned Atlas/Clip resolver.

## Authoring engine control in Zig

[`src/twod_module.zig`](src/twod_module.zig) is a complete small consumer. It spawns through the
existing scene API, reads/writes `TransformV2`, flips/tints/orders a Sprite, runs clip animation,
queries collisions, paints a tile/chunk and updates 32 pooled objects in one batch per call.
It creates no combat, enemy spawn, progression, inventory or other game rules.

```zig
const nx = @import("nexora");
const api = nx.getTwoD(host) orelse return -2;
var sprite = nx.TwoDSprite{
    .atlas_high = atlas_high,
    .atlas_low = atlas_low,
    .region = region_id,
    .flags = 8 | 1, // visible, flip X
};
if (api.set_sprite.?(api.context, entity, &sprite) != 0) return -1;
const camera = nx.TwoDCamera{ .vertical_size = 12 };
if (api.set_camera.?(api.context, &camera) != 0) return -1;
```

All 2D table callbacks are present when `getTwoD` succeeds; optional Tilemap/pool/clip services
return `unsupported` when unattached. Handle every result. After writing transforms, rebuild the
chosen query source once and reuse its snapshot. Animation events and query hits support a
count-only probe, then a sufficiently large output buffer. Never consume animation time in both
fixed and variable updates. Pool handles carry owner/index/generation and are distinct from
World IDs. Batch writes/destroys reject stale, foreign or duplicate handles atomically.

The sample serializes versioned scalar state and handles, not function/context pointers. Reload
reacquires the table and validates migrated IDs/handles before reusing them. Host-owned content
survives old-module stop/destroy; the host clears it when the scene/pool is retired. A failed
module start may have made earlier successful calls, so multi-call content rollback belongs to
the host. The bridge itself is synchronous, owner-thread only and rejects reentry.

## GPU-instanced 2D rendering from Zig

The existing `TwoDApiV1` pool calls control CPU-driven GPU-instanced Sprites. A host using
`SpriteFrame::Draw` automatically compacts the visible scene/pool/tile Quads into native instance
records; no per-object draw call or GPU pointer is required in Zig. Set Atlas/region, Transform,
size/pivot, flip, tint and sorting fields through the same copied `TwoDInstance` values:

```zig
var values: [32]nx.TwoDInstance = @splat(.{});
var handles: [32]nx.TwoDInstanceHandle = undefined;
for (&values, 0..) |*value, i| {
    value.sprite = sprite; // A valid cooked Atlas UUID and region resolved by the host.
    value.transform.position.x = @as(f64, @floatFromInt(i));
}
if (api.create_instances.?(api.context, &values, values.len, &handles) != 0) return -1;
// In a later update: change values, then upload the copied batch to the host.
if (api.write_instances.?(api.context, &handles, values.len, &values) != 0) return -1;
// When retired: destroy_instances(context, &handles, handles.len).
```

See the compiler-tested [consumer](src/twod_module.zig) for complete initialization, movement,
read/write/destroy, error handling and reload migration. Counts are bounded by host pool capacity
and 100,000 instances per call/render packet; capacity is not a frame-rate guarantee. Handles are
CPU pool handles, not GPU buffers. The host retains the same pool across a successful reload and
reacquires API pointers; it owns final cleanup and native texture/buffer retirement.

Only adjacent Atlas-compatible Sprites after transparent sorting share an instanced draw. Different
Atlas generations or intervening layers split batches; UI stays ordered after Sprites. Zig still
updates/simulates on CPU. Compute shaders, script-owned GPU buffers, indirect draws and a general
Mesh/Material instancing API are outside this delivered 2D slice.

## Validation

Standalone Zig wrapper/layout tests use the normal Zig runner. On macOS, the
[host-link helper](../../Tools/Build/README.md#native-zig-test-linking) uses the configured Apple
compiler for the final link to avoid Zig 0.14's libSystem resolution issue with newer SDKs.

- `gameplay.zig_abi_smoke`: original V3 prefix compatibility and shared lifecycle vectors.
- `gameplay.zig_twod_static` / `gameplay.zig_twod_dynamic`: actual Zig input, transforms,
  animation/query calls, pool batches, reload without duplicate content and allocator balance.
- `gameplay.zig_twod_wire_layout`: real Zig/C++ sizes and every field offset, including a short
  original host prefix that must never read the appended callback.
- `twod.gameplay_bridge`: bridge success/failure and ownership/buffer/query/handle contracts.
- `gameplay.zig_twod_native`: Linux Xvfb/Vulkan, 600 presented frames, D input moves the Zig
  entity, real reload and ordered shutdown. Software Vulkan is a correctness gate only.

See [TwoD ownership/error contracts](../../Engine/TwoD/README.md) and
[2D roadmap](../../Roadmap/en/2D_Roadmap.md). Physical Windows performance, clean-host packaging,
main Editor project/Play attachment and a production game's content remain separate work.

## Optional Play pointer snapshots

`nx.capturePointerInput(host, user, &snapshot)` negotiates the appended pointer callback by its own
field end, ABI version, capability and non-null function. Initialize
`var snapshot = nx.PointerInputSnapshotV1{};` (caller size defaults to the known wire size).
Handle Unsupported on older hosts without reading their absent suffix; the original V3 base stays
compatible. `getTwoD` likewise checks only its own field end, so adding later extensions cannot
reject an otherwise valid older 2D host.

A successful call copies native client-pixel X/Y deltas, horizontal/vertical 120-unit wheel values,
five raw held button bits and the legacy batch sequence. Results own their data; errors preserve
output and larger buffers keep unknown suffixes. Fixed and Update see the same non-consuming batch;
deduplicate displacement by sequence and do not multiply it by seconds. A Fixed-only consumer can
miss batches without a fixed tick; use Update to cover those batches too. Editor focus/owner changes
can revoke an already copied current batch; previously returned values remain owning snapshots.
The provider/context remains valid through Stop/Destroy on the serialized game thread. Other logical
users receive neutral input. See the [Editor contract](../../Apps/Editor/README.md#optional-copied-play-pointer-input)
for bounds, acquisition/release and coordinate-domain resets. No raw mouse lock or native object is
exposed. `gameplay.zig_pointer_wire_layout` runs real Zig/C++ layout and negotiation vectors;
`gameplay.zig_twod_wire_layout` includes a positive exact-size older 2D host case.

## Optional Zig gameplay services

`nx.getServices(host)` negotiates the appended V3 services field by size, version and capability.
Older V3, 2D-only and pointer-only hosts return `null`; their existing features remain usable.
Each service negotiates separately and may return Unsupported. Reacquire every borrowed table
when creating/reloading a module; retain none after host teardown. C++ hosts can compose
`runtime::GameplayServices` from the public service tables; no gameplay rules belong in that owner.

```zig
const services = nx.getServices(host) orelse return -2;
var actions = nx.input.Api{};
if (services.get_input_api.?(services.context, &actions) != 0) return -2;
// Bindings are additive. Clear the action first when replacing all of its controls.
if (actions.clear(0, 1) != 0) return -1;
const binding = nx.input.Binding{ .device = .keyboard, .control = nx.input.Key.space };
if (actions.rebind(0, 1, &binding) != 0) return -1;
var state = nx.input.ActionState{};
if (actions.action(0, 1, &state) != 0) return -1;
// Fixed/Update see the same non-consuming sequence: deduplicate one-shot transitions yourself.
```

The [input contract](../../Engine/Runtime/GameplayInput.md) covers action states, deadzones,
multiple users, active device, focus and disconnect behavior. Native Linux gamepads use physical
joystick controls, which games can rebind; device mappings and physical device acceptance are
host concerns. Keyboard/mouse feeds use Window events. Existing `capture_input` keeps its original
WASD/Space meanings.

`nx.ui.Api` submits HUD panels, UTF-8 labels and stable-ID buttons. `registerFont` copies a cooked
RGBA atlas and `nx.ui.Glyph` metrics supplied by Zig (including `@embedFile` data); font handles
and GPU texture lifetime remain host-owned. Missing glyphs return Unsupported. Chinese text
requires an actual font atlas covering those codepoints. This bridge does not rasterize TTF,
shape complex scripts, supply commercial artwork or invent replacement glyphs. The
[UI contract](../../Engine/TwoD/GameplayUi.md) defines per-frame begin/end, button navigation,
texture ID reservation, budgets and owning native overlay snapshots.

`nx.audio.Api.registerWave(asset_id, @embedFile("sound.wav"))` copies PCM16/float32 WAV data.
`playSound`, `stopVoice`, `pauseVoice`, `setVoiceGain` and `setBusGain` expose music/SFX control;
`outputAvailable` detects actual host output. The [audio contract](../../Engine/Runtime/GameplayAudio.md)
provides Linux ALSA, Windows waveOut output and portable host sinks. Unavailable output returns Unsupported;
CPU mixing tests do not establish physical audio-device acceptance.

```zig
var saves = nx.storage.Api{};
if (services.get_storage_api.?(services.context, &saves) != 0) return -2;
// Serialize gameplay data explicitly; never store pointers, API tables or ephemeral handles.
if (saves.saveSlot("resume", 1, encoded_state) != 0) return -1;
var info = nx.storage.Info{};
if (saves.probe("resume", &info) != 0) return -1;
// Allocate a bounded buffer from info.payload_size, validate schema, then loadSlot.
```

Permanent slots survive module/process lifetime; they are independent of `save_state` reload
migration. Save-and-exit only exits after successful `saveSlot`; resume validates the game's
schema and reconstructs World entities/pool handles. The host chooses a writable directory and
project scope. See the [storage contract](../../Engine/Runtime/README.md#durable-zig-gameplay-slots).
The 2D host enables slots with `--save-directory PATH`; `--audio` explicitly requests device
output. Services are exposed to static and dynamically loaded Zig modules without adding game
mechanics. Asset cooking remains the existing pipeline; a game authors its own content.

`gameplay.zig_services_consumer` runs real Zig negotiation, every service wire field layout,
input transitions, Chinese-codepoint atlas registration, UI command generation, voice controls
and save/reopen across separate storage owners. Per-service C++ contracts and Zig wrapper tests
cover invalid arguments and buffer/lifetime behavior. The fixture glyph is wire acceptance data,
not visual typography acceptance. [ProjectPlayer](../../Apps/ProjectPlayer/README.md#explicit-gameplay-module-zig-v3-abi)
can explicitly load a V3 Zig library for the existing native StaticView adapter; dynamic 2D
content and arbitrary runtime mesh residency remain outside that adapter's contract.

The 2D sample reserves input actions 253/254/255/256 for next/previous/activate/cancel UI navigation.
Their keyboard and raw gamepad bindings are editable through the same Zig input table.
Pointer snapshots expose native client pixels and 120-unit wheel deltas through `input.Api.pointer`;
the legacy optional pointer callback is also wired on this host.

`nx.content.Api` registers existing cooked `.nx2atlas` and `.nx2clip` bytes directly from Zig
(for example `@embedFile`), and describes/lists their UUIDs. The 2D host's Sprite and animation
resolvers use this same registry, so content is no longer restricted to its startup fixture.
Registration copies/decodes bytes before returning and publishes immutable generations atomically;
clips require a registered atlas and valid regions. Reuse the existing asset compiler/cook pipeline,
then set the returned IDs through `nx.TwoDApiV1`. The host reserves texture IDs for loaded atlases;
keep UI font texture IDs outside its documented range. See
[content contracts](../../Engine/TwoD/GameplayContent.md). ProjectPlayer's StaticView host does not
advertise this optional 2D service; `NexoraZigTwoD --module PATH` is the 2D module host.


The 2D sample host polls Linux raw joysticks or Windows XInput through `NativeGameplayGamepad`.
Interactive movement/pause uses rebindable actions 250..252 (WASD/Space and slot-0 left stick/A);
UI navigation/activate/cancel uses 253..256. Left-stick Y is down positive at the adapter and
negated by the sample binding, with a 0.2 stick deadzone. The service handles focus loss and
slot disconnect; unchanged XInput packets are resubmitted after focus recovery. Other Linux
models may need rebinding. These are sample controls, not game rules or a physical-controller
acceptance claim. Noninteractive runs retain their deterministic synthetic movement.
See [native mappings and ownership](../../Engine/Runtime/GameplayInput.md#native-gamepad-adapter-and-evidence).


For the integrated static-sample acceptance flow, pass `--validation-package ROOT` and a dedicated
`--save-directory PATH`; `--audio` requests actual device submission. The package contains cooked
Atlas/Clip and a WAV generated by `Tools/TwoD/StageProductValidation.py`. The host validates identities
and dependency closure, supplies an ASCII fixture font/Save/Open UI, resumes the position slot,
performs immutable cooked/module reload and verifies storage-owner reopen after callbacks stop.
Ordinary source-free procedural sample mode remains available without this flag. This explicit
fixture does not attach main Editor project Play or export arbitrary games. See
[integrated tool commands and acceptance scope](../../Tools/TwoD/README.md#integrated-cooked-service-validation).


## Exporting your own Windows game

Use [NexoraTwoDPlayer](../../Apps/TwoDPlayer/README.md) to load your own separately built V3 Zig
DLL with an explicit game manifest, cooked assets, WAV and rasterized CJK font atlas. The generic
player builds without a Zig compiler, uses per-project user saves and has no fixed test scene or
sample sprite-count requirement. `NexoraProjectPlayer --run-2d-project` provides the same host when
native/2D support is enabled. `ExportGame.py` and `BakeWindowsFont.ps1` document the Windows/MSVC
Shipping workflow. The original `NexoraZigTwoD` still runs its static/dynamic/reload fixtures through
that shared host. Game logic remains Zig; engine executable, native device adapters, resource
ownership and shutdown remain C++ host responsibilities.

For an embedded Windows-baked atlas in Editor Play, `ui.registerBakedFont(texture_id,
@embedFile("UI.nxfont"), scratch_glyphs, &font_handle)` decodes NXFONT1 little-endian metrics and
registers copied buffers through the same UI table. Supply enough caller-owned `ui.Glyph` scratch
entries for the corpus (maximum 65536); the helper allocates nothing and retains no input bytes.
Errors preserve the output handle; scratch is work storage. The existing real Zig services
consumer verifies successful CJK-codepoint decoding plus truncated bytes/undersized scratch.
Standalone project manifests can instead preload that font and use `get_font(index)` directly.

## Display service

`nexora.display` provides independently negotiated display snapshots, native mode requests and pixel/logical/world conversion with orthographic rotation. `getDisplay` safely returns null on historical service hosts while `getServices` preserves their earlier services. The independent `src/display_module.zig` consumer exports through the generic Player; see the [contract and examples](../../Engine/TwoD/GameplayDisplay.md).

## Image UI diagnostic screens (SP-02)

`src/ui_showcase.zig` is an independent Zig-only consumer of Display/UI/image UI. It renders a
nine-slot HUD, choices, loadout, subtitles and settings, with visible focus and 100/150% UI scale.
The baked Noto glyphs are real; colored icon/panel pixels are diagnostic fixtures. Host tests drive
pointer/navigation/cancel at four DPI scales. `NexoraZigUiShowcaseBuild` builds its module; export
it with the general Player just like any separate module. See [the image UI contract](../../Engine/TwoD/GameplayUiDraw.md).

## SP-03 diagnostic transitions

`src/asset_transitions.zig` is a separate V3 module using the optional asset service. It loads
five cooked 2048-square Atlas/Clip pairs sequentially from a >80 MiB exported package, exercises
1800 pooled instances and real animation events, then removes resources in dependency order and
waits for native/snapshot retirement before re-entry. Layout, transition choices and cleanup are
Zig. This is diagnostic data, not production art/combat/pickup acceptance. See
[the service contract](../../Engine/TwoD/GameplayAssets.md).

## SP-04 explicit authoring color and fallback

`nx.color` supplies allocation-free sRGB RGB decoding for raw image registration, float Sprite
tints and packed UI tints; alpha stays linear coverage. Cooked Atlas pixels are already linear
and must not be decoded twice. See [color/fallback contract](../../Engine/TwoD/ColorAndFallback.md).
`src/color_slice.zig` is an independent procedural diagnostic using existing 2D services for
Y-ordered layers, roof fading and baked glow/shadow removal while retaining danger outlines.
It is neither final game art nor dynamic lighting/bloom acceptance.
