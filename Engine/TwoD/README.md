# Nexora 2D contracts

The optional Editor's `TwoDPlayAssets` freezes validated cooked Atlas/Clip closures from one
explicit Runtime asset generation without retaining the store. It resolves Clip Atlas dependencies,
validates frame regions, bounds unique payload/count totals and preserves external residency/pins.
See the [Editor ownership contract](../Editor/README.md#frozen-cooked-2d-play-assets).
Project provenance and main Editor Play/Game attachment remain separate work.

`Nexora::TwoD` is the optional `NEXORA_ENABLE_2D` module. It depends on Runtime and
Presentation, requires Asset Pipeline and Window Presentation, and is removed from headless
and Dedicated builds. Runtime remains independent of TwoD, Presentation and Editor. Public C++
consumers must rebuild; the original C/Zig V3 prefix and schema-3 World format are preserved.
The optional append-only 2D ABI extension is described below.

## Ownership and execution

All mutable services are owner-thread objects. Serialize scene extraction, queries and snapshots
with World mutations. `SpriteScene` borrows a live Runtime World/scene; destroy it before that
World. Find/FindCollider pointers borrow metadata and must not survive metadata mutation, Restore
or owner destruction. It stores Sprite/collider metadata, uses the existing transform hierarchy, and extracts
one owning World pose snapshot per frame. InstancePool instead owns independent transient
instances; it does not copy or cache entities from a World. `ExtractSprites` consumes their
validated affine matrices through the same extraction and native rendering path.

Each visible Atlas UUID resolves once per extraction, including unavailable results, preventing
duplicate load requests while retaining per-object unavailable counts. An AtlasResolver supplies a shared immutable Atlas and a nonzero surface-local texture generation
ID. A frame retains those owners; upload spans borrow their pixels until Draw returns. Never
mutate a published Atlas or reuse a resident ID for different bytes. IDs must be distinct for
atlas generations, surface recreation and HUD resources; the two largest IDs are reserved.
`ReleaseSpriteTextures` between frames waits for native work before reclaiming images/descriptors.
The caller retires superseded IDs; there is no unbounded automatic generation cache. Resize keeps
resident atlases. A new surface requires uploads again. UI fonts remain mutable by default.

`SpriteFrame::Draw` requests native GPU instancing through `DrawSprites`: each visible Sprite
becomes one 44-byte affine/UV/tint record; native vertex shaders generate six Quad corners.
Vulkan, DX12 and Metal issue instanced draws. The existing extraction retains CPU triangle geometry
for Editor ImGui preview and inspection; projection, sorting, culling, animation and script updates
still run on CPU. GPU submission does not upload that expanded Sprite geometry. This is CPU-driven
GPU instancing, not GPU simulation, compute culling or indirect drawing. The borrowed overlay uses
the indexed UI pipeline in the same native pass, after the instanced Sprites and background.
Atlas lifetime/sampling and linear blending remain shared with Presentation.
Sprite batching merges only adjacent matching atlas IDs after `(layer, order, descending world Y,
entity ID)` sorting; it never groups translucent geometry across intervening layers. A rotated
orthographic camera projects affine-transformed Sprite corners and culls offscreen bounds.
Covered AABB pixels are an overlap diagnostic, not measured GPU overdraw or fragment invocations.
RGBA8 pixels/tints follow the shared UI linear-color contract; an sRGB swapchain encodes the final
linear blend. PNG preparation preserves byte values and does not perform color-profile conversion.

## Assets, animation and queries

Atlas packing uses canonical region-ID order, bounded shelf layout and one-pixel extruded gutters.
PNG rectangle/grid slicing is provided by [PrepareAssets.py](../../Tools/TwoD/PrepareAssets.py);
packing and validation use the C++ module. `.nx2atlas` and `.nx2clip` register with the existing
ImporterRegistry/AssetCooker/BundleBuilder/AssetGenerationStore. Stable UUID references survive
repeat imports; cooked data passes existing hash/schema checks before generation publication.
The little-endian codecs reject malformed, truncated, trailing and out-of-budget data. Limits:
1,024 regions, dimensions up to 4,096, 4 Mi pixels/16 MiB per Atlas; 4,096 frames and ASCII event
names up to 256 bytes per clip; containers are bounded to 64 MiB. Unsupported PNG formats fail
before publishing an output. See [tools](../../Tools/TwoD/README.md) for formats and commands.

Animator owns a validated clip, switches atomically, pauses/resumes without resetting elapsed
clock, and emits each entered-frame event in order. Play emits no initial-frame event. Nonlooping
clips finish on their last frame. Negative/nonfinite deltas and more than 65,536 transitions per
Advance reject without advancing state; large valid steps emit every event rather than dropping
updates. A relative 1e-12 frame-duration tolerance stabilizes decimal duration boundaries across
equivalent delta partitions. It is independent of skeletal animation and application behavior.

SpatialIndex atomically rebuilds a sorted-ID grid of circles/AABBs. Overlap uses symmetric masks;
Query excludes the query's nonzero ID. Sweep is an exact point/disc sweep against circles and
rounded AABBs, sorted by fraction/ID, including initial overlap. Very large queries use a bounded
complete scan. Invalid queries return no hits; invalid builds preserve the prior index. Limits:
100,000 shapes, 4,096 cells per shape, 1 Mi total references. Scene collider offsets follow the
World affine transform. Circles use the largest XY singular value; rotated/sheared boxes become
conservative world AABBs. These are overlap/query services, without rigid-body integration or Jolt
changes. Returned observations own their data.

InstancePool preallocates at most 100,000 slots, uses owner/index/generation handles and validates
entire create/destroy batches before publishing. Stale, foreign and duplicate handles reject.
Exhausted slots retire rather than wrap IDs. ForEach permits value edits but rejects structural
reentry; exceptions release its guard and preserve already-performed value edits. Validate edited
values before extraction. Borrows expire when a slot is destroyed or the pool is destroyed.
FixedStepper returns monotonic ordered fixed-step ranges and retained remainder. Excess backlog
rejects without silently dropping time; applications decide recovery and execute the returned
steps. It does not replace the Core scheduler or dispatch gameplay callbacks.

## Scene and Tilemap persistence

NX2S schema 2 wraps the unchanged schema-3 World text plus Sprite/collider metadata. Parse,
identity, membership and value validation occur in a staging World before Editor replacement.
Play Worlds reject Restore; loading into a separate Play World preserves authoring isolation.
Deleted World entities must have their metadata removed before Save; stale metadata rejects
snapshots. Scene services do not silently resurrect/unload entities or write project files.

Tilemap supports 100,000 cells with layers, collision flags, grid painting and 16x16 chunks.
Paint batches are atomic; zero region erases and duplicate coordinates reject. Negative chunk
coordinates use floor division. A rebuilt chunk index culls whole chunks conservatively against
rotated camera bounds, followed by exact per-cell culling. Explicit unloaded chunks disappear
from both rendering and collision queries. Cells share Atlas regions and the same Sprite pipeline;
collision cells become AABBs with layer-specific stable IDs. Tilemap IDs and World IDs occupy
separate query spaces unless the application remaps them before combining. Its canonical schema-1
codec preserves layers, painting and chunk state. It has no disk streaming worker or gameplay map.

## Verification and remaining host gates

✅ Portable runtime, independent collision oracle, asset-tool, authoring/Undo/Play and graphical
control contracts are in [Tests/TwoD](../../Tests/TwoD/).
✅ Linux X11/Vulkan captures check cross-atlas alpha ordering, HUD, nearest/linear sampling,
gutters, UV/pivot/flips and negative chunk render/query boundaries. Mixed native stress loads
cooked PNG/clip assets asynchronously, animates/query-updates/recycles objects, reloads GPU
generations, and records 100/1,000/5,000/10,000, overlap and sparse cases.
✅ Monolithic Linux Shipping is staged with cooked assets, SHA-256 manifest and corruption
rejection, then runs without Editor or development engine libraries.

These establish portable/Linux correctness. The [local Windows record](evidence/Windows-2026-10-14/acceptance.md)
also verifies MSVC contracts, physical DX12 mixed stress, Studio save/reopen and staged Monolithic
Shipping with Cooked loading/corruption rejection, plus 600 native Zig frames using synthetic input.
Physical minimum-PC 60 FPS remains **NOT ACCEPTED**. The separate
[Windows VM slice](evidence/Windows-VirtualBox-2026-10-15/acceptance.md) passes prerequisite/CPU checks
after Runtime installation, but native DX12 fails and Vulkan is unavailable; M7 remains
**PARTIAL / NOT ACCEPTED**. Actual keyboard/controller, audible playback and display/DPI checks
remain open. Both frozen Windows executables predate GPU instancing; neither validates
current-main rendering performance. Local developer-PC execution is not clean-host evidence.
Software Vulkan timings are not a PC performance guarantee. Follow the
[bilingual 2D roadmap](../../Roadmap/en/2D_Roadmap.md) and its linked acceptance evidence.

## C ABI and Zig gameplay control

`GameplayBridge` supplies the canonical `NexoraTwoDApiV1` table from
[`nexora.h`](../API/include/nexora/nexora.h). The host appends `get_twod_api` to its
`NexoraGameplayHostV3` and advertises `NEXORA_GAMEPLAY_CAPABILITY_2D_API` only when the live
bridge is attached. Check host structure size **before** reading the appended callback, then
check capability and the returned table's version/size. `nexora.getTwoD` performs those checks
and rejects missing callbacks. The original V3 prefix is still sufficient for other modules;
TwoD-off hosts leave the callback null and the capability unset. Runtime has no dependency on
TwoD. The original ABI baseline is retained alongside an additive 2D V1 wire baseline.

The bridge borrows the supplied World, its matching SpriteScene and optional Tilemap/InstancePool;
reject a mismatched World/scene at construction. ClipResolver returns immutable host-owned clips;
Play validates their UUID/Atlas identity and copies a clip into its animator. Unavailable clips,
Tilemap or pool return `UNSUPPORTED`. Loading/cooking/asset generations remain host responsibilities;
the module cannot read native paths, upload textures, or hold renderer pointers through this table.
The host retains the table/context and services until all modules and callbacks have stopped.
The module may retain the copied table until destroy. Destroy the bridge before its borrowed owners.

All callbacks execute synchronously on the construction thread. Wrong-thread or bridge reentry
returns `LIFECYCLE`; resolver exceptions are contained at the C boundary. Never run a module
callback concurrently with extraction, pool ForEach, World mutation, asset publication or teardown.
Every descriptor/buffer is borrowed only during its call. Outputs are copied and may be retained.
The caller must provide correctly aligned, valid, nonaliasing buffers for their advertised capacity.
No native object, C++ pointer or exception crosses the API; its context is an opaque borrowed token.

| Operation | Contract |
| --- | --- |
| Sprite/collider set/get | Existing members of the bound scene only; null setter removes (idempotently). Unknown flags/kinds, invalid geometry or missing entities reject. Sprite fields cover Atlas UUID/region, size/pivot, tint, layer/order/Y sort, visibility and flips. |
| Camera set/get | Center, orthographic vertical size and radians; finite/bounded, height at least 1e-5. Viewport dimensions remain host-owned and come from `RenderCamera` for the current surface. |
| Animation Play/Pause/Advance | UUID resolves through the host, speed/loop/event rules reuse Animator. Play resets region and clock; pause preserves time. Removing/changing the Sprite Atlas discards its playback. Explicit Advance controls time; rendering does not advance animation again. |
| Animation buffers | Null events with capacity zero probes the result without advancing. Short nonnull buffers return `UNSUPPORTED` with required event count/result and consume no time/events. Retry the same delta after allocating enough capacity. Success with a nonnull buffer commits even when event count is zero. At most 65,536 events; each name is 0..256 ASCII bytes with explicit length and no required terminator. Invalid input leaves outputs unchanged. |
| Query rebuild/overlap/sweep | Rebuild once after World/transform or collider mutations, then query the immutable snapshot many times. Metadata changes through the bridge invalidate it; external World changes require an explicit rebuild. Missing snapshot returns `LIFECYCLE`. Scene source 0 and Tilemap source 1 keep separate ID namespaces. Overlaps sort by ID; sweeps by fraction/ID. |
| Query buffers | Null hits/capacity zero probes count. Short nonnull buffers return `UNSUPPORTED` and publish only required count, no partial hits. Invalid input leaves count/hits unchanged. At most 100,000 hits; caller interprets identities within the selected namespace. |
| Tilemap | Atomic paint batches, region zero erases, collision 0/1; read cell and load/unload chunks. Uses existing Tilemap limits. `Collect` returns owning loaded visible-chunk inputs for one globally sorted extraction with scene/pool instances; unloaded chunks never appear. |
| Transient pool batches | Create/read/write/destroy up to 100,000 copied instances per call using host-budgeted InstancePool. Handles carry owner/index/generation, never World IDs. Stale/foreign/duplicate writes or destroys, invalid transforms/Sprites and exhausted capacity reject the whole batch. Buffers remain unchanged on rejection. Transform writes normalize quaternions. Read may repeat a valid handle. No entity state is duplicated into the pool. |

`INVALID_ARGUMENT` covers rejected descriptors/IDs/geometry; `UNSUPPORTED` covers absent optional
services, unavailable assets and insufficient output/table capacity; `LIFECYCLE` covers execution
violations and caught host exceptions. A failed rebuild preserves the previous query snapshot.
There is no blanket rollback across several successful API calls made by a module.
After entity destruction the host calls `ForgetEntity` for every `LastDestroyed()` ID, including
descendants, before saving or retiring the scene. Unloading a module does not destroy host-owned
entities, animation state, tiles or pool instances. Reload can reuse migrated IDs/handles; the
host reclaims content at scene/session teardown. If new content must be rolled back after a
failed module start, the embedding owns that transaction.

The optional Editor's `TwoDPlayContext` supplies an owning isolated document snapshot with
frozen Atlas/Clip, copied Tilemap and a fresh 1..4096-slot pool around this bridge. It combines
scene/pool/loaded tiles through the existing extraction path with a shared 100,000-input cap.
Modules must stop before the context is destroyed; owning extracted frames retain Atlas pixels
afterward. Source history, source assets and native texture ownership remain separate. See the
[Editor contract](../Editor/README.md#owning-isolated-2d-play-context). This prerequisite does
not attach the bridge to the main project Editor or complete its Game View/reload acceptance.

The [Zig sample and usage guide](../../Gameplay/Zig/README.md) exercise static/dynamic loading,
fixed input/Transform/Sprite/animation/query control, bulk pool updates, Tilemap and migration.
`twod.gameplay_bridge` tests failures, buffer retries, query snapshots, lifetime cleanup,
thread/reentry, atomic batches and stale handles. The real Zig compiler checks every wire size
and field offset against C++ in `gameplay.zig_twod_wire_layout`; C11 and metadata gates also run.
Linux/Xvfb/Vulkan `gameplay.zig_twod_native` holds a normalized D key through 600 presented frames
and verifies Zig movement, reload and balanced allocation. This is engine control evidence,
not game rules or a physical-PC performance result.
