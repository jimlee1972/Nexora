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
