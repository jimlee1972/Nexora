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
