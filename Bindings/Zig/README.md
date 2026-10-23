# Nexora Zig bindings

`nexora.zig` mirrors the public C ABI. Host tables negotiate by `struct_size` and version;
optional services report unsupported without losing older services.

## Optional image UI

`nx.getUiDraw(services)` probes `ui_draw.Api`; an old host remains usable without this suffix.
Use automatic image/font generations, explicit handle removal, `gridCell` and metric-based
`wrapText` to compose UI in Zig. See [the contract](../../Engine/TwoD/GameplayUiDraw.md) and
[the executable diagnostic screens](../../Gameplay/Zig/src/ui_showcase.zig).
