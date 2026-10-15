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
borrowed owners alive until module unload. The procedural two-region Atlas/clip in `TwoDHost.cpp`
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
