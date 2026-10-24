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
