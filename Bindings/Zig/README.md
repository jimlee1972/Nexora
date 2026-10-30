# Nexora Zig bindings

`nexora.zig` mirrors the public C ABI. Host tables negotiate by `struct_size` and version;
optional services report unsupported without losing older services.

## Optional image UI

`nx.getUiDraw(services)` probes `ui_draw.Api`; an old host remains usable without this suffix.
Use automatic image/font generations, explicit handle removal, `gridCell` and metric-based
`wrapText` to compose UI in Zig. See [the contract](../../Engine/TwoD/GameplayUiDraw.md) and
[the executable diagnostic screens](../../Gameplay/Zig/src/ui_showcase.zig).

## Deferred assets

`nx.getAssets(services)` probes `assets.Api` without losing older services. Request a frozen
catalog index, poll phase/result, then release the ticket. Ready Atlas/Clip resources remain
registered until explicit UUID removal; remove dependent Clips before Atlases. Opaque-byte
requests may be copied into Zig-owned buffers for existing image/font/audio APIs. See
[the lifetime and budget contract](../../Engine/TwoD/GameplayAssets.md) and
[the diagnostic transitions](../../Gameplay/Zig/src/asset_transitions.zig).

## Lifecycle, close and rumble

`nx.getLifecycle(services)` probes `lifecycle.Api`; a pre-SP-06 host returns null. Explicitly opt into
`guardClose(true)`, query before **each fixed tick**, retain pending game/UI state, and use the real
Storage result before resolving the pending close token. Cancel/retry remain available on failure.
`lifecycle.PauseGate` is a caller-owned pause/countdown helper; confirm only after transactions are
resolved, and never use background elapsed time for battle catch-up. Games also pause their owned
music/voice handles (`dialogue.Line.pause`) and retain pending cards themselves.

`vibrate(slot, low, high, duration_ms)` accepts finite gains [0,1] and 1..5000ms, or zero motors to
stop. Map the user's 0..100 setting to gain/100; send zero immediately when disabled. Connected
DirectInput/DS4 and Linux pads currently lack rumble support, indicated by the copied `rumble_mask`
and -2 return. See [contract and diagnostic](../../Engine/Runtime/GameplayLifecycle.md).

## Script-only game SDK

The typed lifecycle facade and engine-owned build/entry are documented in [Tools/Zig](../../Tools/Zig/README.md). Authored projects contain gameplay scripts and a manifest; they do not maintain a build.zig or V3 ABI export.

## Optional 2D effects contract

`nx.getTwoDEffects(services)` / `try context.effects()` negotiate the HD2D service. Query native
feature flags before enabling effects; HD2D-01 supplies the contract/snapshot owner only and the
Player has not registered a renderer. Submit copied complete batches with `api.submit(options,
lights, occluders, sprites)`; success is acceptance for the next frame, not immediate presentation.
See [ownership, limits and compatibility](../../Engine/TwoD/GameplayEffects.md).

The native 2D Player now negotiates flat local lighting through `context.effects()` when supported.
Keep gameplay and effect configuration in Zig; the default pipeline remains disabled. For example:

```zig
const effects = try context.effects();
const pooled_style = try effects.poolSprite(handles[0]); // Complete owner/index/generation.
try effects.submit(.{ .enabled = 1, .ambient = .{ 0.2, 0.2, 0.25 } },
    &.{.{ .id = 1, .position = .{ .x = 1, .y = 2 },
        .color = .{ 1, 0.4, 0.1 }, .intensity = 2, .radius = 4 }},
    &.{}, &.{pooled_style});
```

Check `(try effects.capabilities()).features` first. At this stage LIGHTING is available on capable native
surfaces; SHADOWS and BLOOM are separate later gates. `poolSprite` resolves an opaque effect
identity, preserving all fields of the existing pool handle; never truncate a handle or use a
sorted Sprite index. Tokens are service-local runtime data and must not be saved. The optional
table tail negotiates independently: old hosts retain original effects operations while pool
identity lookup reports `error.Unsupported`. Failed requests leave the previous valid snapshot.


Emission companion blobs use `twod_effects.EmissionAsset.parse(copied_bytes)`. This borrowed view
resolves base-region to mask-region and `apply` fills an existing effects Sprite while preserving
its entity/pool identity. Keep the copied bytes alive, register/load the mask Atlas first, then
submit normally. Parsing is allocation-free and rejects malformed/truncated/duplicate links;
original effects table and Sprite layouts do not change. [Contract](../../Engine/TwoD/EmissionAssets.md).
