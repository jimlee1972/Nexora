# Nexora 2D contracts

`Nexora::TwoD` is the optional `NEXORA_ENABLE_2D` module. It depends on Runtime and
Presentation, requires Asset Pipeline and Window Presentation, and is removed from headless
and Dedicated builds. Runtime remains independent of TwoD, Presentation and Editor. Public C++
consumers must rebuild; the stable C/Zig ABI and schema-3 World format are unchanged.

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

`DrawSprites` validates/rebases Sprite triangles plus optional HUD data into one native RenderUi
pass, with an opaque background. Do not issue another RenderUi in that acquired frame. The existing
Vulkan/DX12/Metal native unlit triangle pipeline handles RGBA alpha and per-texture nearest/linear
sampling. It does not allocate backend objects in public headers or implement a private renderer.
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

These establish portable/Linux correctness. Physical minimum-PC 60 FPS and clean Windows
DX12/Vulkan integration (keyboard/controller, text/audio, loading/save, display/DPI and shutdown)
remain **NOT RUN**. Software Vulkan timings are not a PC performance guarantee. Follow the
[bilingual 2D roadmap](../../Roadmap/en/2D_Roadmap.md) and its linked acceptance evidence.
