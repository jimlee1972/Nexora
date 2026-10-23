# Gameplay display service (SP-01)

`NexoraGameplayDisplayApiV1` is an optional independently negotiated service, exposed by the
standalone 2D Player. Games use the public C table or `Bindings/Zig/display.zig`; native window
objects, STL and platform types never cross this boundary. Runtime's service aggregate does not
own or depend on the Window/Presentation/TwoD implementation.

## Ownership, negotiation and frame order

The host owns the service and callback through module Stop/Destroy/unload. Calls are serialized
on its owner thread. Wrong-thread calls return `LIFECYCLE`; null/invalid values return
`INVALID_ARGUMENT`; missing capability or incompatible descriptors return `UNSUPPORTED`.
Rejected getters leave outputs unchanged. Operational native failures return `LIFECYCLE`.
The display snapshot is initialized before module creation/start.

The services V1 table appends `get_display_api`. Its original prefix ends at
`NEXORA_GAMEPLAY_SERVICES_V1_BASE_SIZE`; the aggregate accepts that historical size and copies
only the supported whole prefix. New consumers inspect the returned size before reading the
optional suffix. New Zig consumers retain older input/UI/audio/storage/content on old hosts;
`getDisplay` returns null when absent. Frozen historical ABI baselines remain unchanged; the
separate display baseline and layout tests cover this extension. C++ consumers must rebuild.

Before gameplay callbacks the Player processes window events, publishes one display snapshot,
and converts native pointer pixels to logical UI coordinates. All fixed/variable updates and
render extraction for that iteration use the same snapshot. `set_aspect` changes the policy for
the next published frame; it cannot mutate a snapshot halfway through module callbacks.
`revision` changes when published extent, DPI, mode or viewport changes, not on every frame.
A present invalidated by a mid-frame window request is not counted as delivered; the Player
accepts recoverable out-of-date/occluded/zero-extent presentation and processes resize next frame.
`request_window` returns admission of the native operation, not a promise that the requested
extent will fit. Observe the next snapshot for actual dimensions. Calls are not a transaction
across several successful native operations. Unsupported headless requests report `UNSUPPORTED`.

## Dimensions, modes and projection

`pixel_width/height` are actual client framebuffer pixels. `logical_width/height` and
`viewport_logical` are pixel dimensions divided by the positive `dpi_scale`. Input tables keep
their existing native pixel contract. Display UI coordinates are independent of user-selected UI
scale: a game scales its own positions/font sizes (for example 1.0 or 1.5); it does not apply DPI
twice. The Player scales owning UI geometry/clips to framebuffer pixels once before rendering.

`set_aspect(0,0)` selects the whole surface. Otherwise both finite bounds must be positive and
minimum <= maximum. The Player centers the largest integer-pixel viewport within those bounds,
with floor rounding and at most one-pixel asymmetric margins; tiny float representation errors
at exact integer boundaries are tolerated. Sprites, Tilemap visibility, camera projection and
scissors share its dimensions. The host offsets the extracted world frame to the viewport and
uses a black background when bars are present. UI uses full-surface logical coordinates; the
game positions HUD inside `viewport_logical`. UI is not forcibly clipped to world bars, allowing
full-window menus. World picking rejects points in bars (right/bottom bounds are exclusive).

Zig `pixelToLogical`, `logicalToPixel`, `pixelToWorld` and `worldToPixel` support viewport offsets
and a rotated orthographic camera with Y-up world coordinates. Empty extents yield no mapping.
The module chooses aspect policy, camera position/span and UI scale. No game-specific 16:9–21:9
policy, 24/28-unit camera or progression rule is built into the host.

`request_window` admits dimensions 1..16384, mode 0 (windowed) or 1 (borderless), and resizable
0/1. The standalone Player opts into `fitToWorkArea`; other Window/RenderSurface hosts retain
their previous sizing by default. On Windows this fits the decorated outer rectangle to its monitor work area without enlarging the
requested client size, preserves aspect within integer rounding, and centers the fitted window.
Borderless uses the monitor desktop rectangle, with no exclusive display-mode change. Returning
to windowed mode restores the saved rectangle/style; that transition prioritizes restoration
over requested width/height. A later windowed request can resize it. Mode requests can update
resizability. A minimized Windows window reports zero extent and the Player skips gameplay/presentation
until a drawable frame returns. Broader focus/power/save policy remains SP-06.

Windows supplies initial real client extent and DPI, including before the first DPI-change event.
X11 supplies actual client pixels with scale 1 and uses EWMH fullscreen (requiring a cooperating
window manager). Cocoa's Spaces fullscreen is not this borderless contract: native mode requests
return unsupported. Other hosts may leave the optional service absent; Editor Play is not wired
by this change. No cross-platform physical acceptance is implied.

## Project settings and Zig use

Add this optional field to the schema-1 `game.json` exported by `Tools/TwoD/ExportGame.py`:

```json
"display": {"width": 1920, "height": 1080, "mode": "windowed", "resizable": true}
```

All four fields are required when `display` is present. Unknown/duplicate fields, null, fractional
or boolean dimensions and dimensions outside 1..16384 reject. Export emits one optional
`display 1920 1080 windowed 1` record in `NEXORA_2D_GAME 1`. Old descriptors omit it and retain
640x480/resizable/windowed defaults. Packages using the new record require the updated Player.
No persistent settings file or game settings menu is created by the host.

```zig
const services = nx.getServices(host) orelse return -2;
const display = nx.getDisplay(services) orelse return -2;
if (display.aspect(16.0 / 9, 21.0 / 9) != 0) return -3;
// In update, after the host has published the policy:
const state = display.state() orelse return -3;
// ui.begin_frame uses state.logical_width/state.logical_height.
// Native input pointer pixels can be converted with nx.display.pixelToWorld.
```

## Validation boundaries

`twod.gameplay_display` covers snapshot stability, aspect/DPI matrices, clipping/geometry,
historical service prefix/canary, missing services, thread/error behavior and fitted client sizes.
`gameplay.zig_display_wire_layout` compares all C/Zig fields; the Zig wrapper checks rotated
world/pixel and DPI round trips, bars and zero extents. `gameplay.zig_display_player` exports and
runs an independent Zig library through the generic Player at 1080p, 4:3 and odd ultrawide sizes,
checking viewport observations, marker/UI generation and strict descriptor admission.

The Zig module also requests borderless/windowed modes when the host supports them; headless
runs exercise explicit unsupported behavior. Its button toggles 100/150% UI size and logs native
activation for manual verification. See [Windows acceptance procedure](../../Tools/TwoD/WindowsDisplayAcceptance.md).
Linux tests are not Windows rendering, hardware, click accuracy or native mode acceptance.
