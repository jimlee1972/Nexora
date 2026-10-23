# Gameplay UI and supplied fonts

`GameplayUi` implements the optional `NexoraGameplayUiApiV1` C11 table declared in
`nexora/gameplay_ui.h`; `Bindings/Zig/ui.zig` mirrors it as `nexora.ui.Api`. It uses the existing
Presentation indexed UI pipeline for HUD panels, UTF-8 labels, buttons, card choices and menus.
The application determines layout and gameplay; the bridge owns no game rules or native widgets.

The host constructs one owner per gameplay session, reserves its white texture ID (default
`UINT64_MAX - 2`) and registers immutable rasterized fonts with `RegisterFont`, or lets Zig
register them through `api.register_font` / `api.registerFont`. A supplied
`GameplayUiFont` contains tightly packed RGBA8 pixels, a unique nonzero surface texture ID,
pixel-size and line-height metrics, and Unicode scalar glyph metrics/UV rectangles. Registration
moves/copies those bytes into the owner and validates dimensions, glyph uniqueness, UVs, metrics,
and IDs before publication. At most 32 active fonts, 4096x4096 pixels and 65,536 glyphs per font are
accepted, with at most 16 MiB RGBA bytes per font to fit the native Sprite overlay budget. Texture
IDs must differ from Sprite atlas generations and the two Presentation-reserved largest IDs. The
application handles atlas generation, font licensing and asset packaging; the host handles GPU
retirement. Zig can `@embedFile` cooked RGBA bytes and supply matching `ui.Glyph` records without
any game-specific C++ code. `NexoraUiFontV1` uses pointer/count pairs borrowed only during registration,
requires zero reserved fields, and copies both pixels and metrics before publishing its handle.
The caller must reserve its texture ID outside Sprite atlas IDs and the UI white texture; duplicate
IDs within the UI owner reject. Registration during an open UI frame returns `LIFECYCLE`. Invalid
counts, pointers, dimensions or metrics leave the output handle unchanged.
There is no Editor/ImGui dependency and no generated box pretending to be a Chinese glyph.

Zig enumerates fonts through `get_font` and can query `has_glyph` before choosing text. The application
must supply actual CJK glyphs to draw Chinese. Missing fonts/glyphs return `UNSUPPORTED`; malformed UTF-8,
embedded NUL, surrogate sequences, overlong encodings and invalid scalars return `INVALID_ARGUMENT`.
Text drawing supports Unicode scalar lookup, LF line breaks, supplied advances, size scaling and
rectangle clipping. Complex-script shaping, automatic wrapping, rich text and editable IME widgets
are separate capabilities. Historical registration is append-only by default. The independently negotiated [image UI extension](GameplayUiDraw.md) can explicitly remove fonts; removed opaque handles become unsupported, while owning frame copies remain valid.

## Frame and input ownership

All methods and C callbacks run synchronously on the construction thread. Calls from another
thread return `LIFECYCLE`; direct input/frame host methods throw on lifecycle violations, while
`RegisterFont` returns false. No exception escapes
a C callback. Tables and context tokens borrow the owner until all module callbacks stop.
Descriptors, strings and output pointers borrow only their call. The caller supplies correctly
aligned nonaliasing output storage. Null context/outputs reject. Output values remain unchanged
on ordinary validation errors; allocation failures return `LIFECYCLE` and the host may discard
the frame. There is no transaction across multiple successful calls.

The host calls `BeginInput` once before module update, supplying current logical pointer coordinates,
press/release edges, keyboard/gamepad navigation (-1/0/+1), activation/cancel edges and focus state.
It clears ready draw output, so a module that submits no UI cannot leave a stale HUD. Focus loss
clears pointer capture and selection. The input API/host decides physical mappings and repeat rates.
The module calls `begin_frame(width,height)`, submits panels/text/buttons, then `end_frame`.
Frame nesting and draw calls outside a frame return `LIFECYCLE`. The host must end a started frame
before replacing input, or call the host-only `CancelFrame()` to discard partial commands
after a failed/stopped module callback. Cancellation clears selection and capture but preserves registered fonts/images.
A frame is bounded to 65,536 quads, 4,096 button IDs and 65,536 UTF-8 bytes
per label. Coordinates are top-left logical pixels, dimensions at most 16,384, colors pack red in
the least significant byte using Presentation's existing linear RGBA8 blending contract.

Button IDs must be nonzero and unique per frame. Navigation follows enabled buttons in the previous
frame's submission order with wraparound, allowing keyboard/gamepad operation of menus and choices.
A newly displayed menu focuses its first enabled button. A click activates only after a press and
release inside the same enabled button; release alone does not activate. State bits report hovered,
focused, activated and cancelled. Activation/cancel and pointer edges are consumed by `end_frame`.
Use nonoverlapping buttons; disabled buttons do not receive focus or activation.

After successful `end_frame`, `Frame()` returns an owning snapshot of vertices, 32-bit indices,
commands, uploads and shared immutable font/image owners. `GameplayUiFrame::DrawData()` borrows that
snapshot until it is mutated/destroyed; pass it to the existing Sprite overlay/native UI path.
The snapshot may outlive the UI owner and module reload. Native surfaces copy upload records under
the existing Presentation contract; keep the snapshot alive until Draw returns. Surface teardown
and immutable texture retirement remain host responsibilities. Reloaded modules may reuse the live
UI table/font handles; session teardown must stop modules before destroying the owner.

## Verification

`gameplay.zig_ui_wire_layout` checks C/Zig table, font descriptor and glyph layout with the real Zig
compiler. `twod.gameplay_ui` covers registration pointer/count/descriptor errors, copied registration
buffers, atlas ownership after owner destruction, Chinese UTF-8 scalar lookup,
malformed UTF-8, unavailable glyphs, duplicate IDs, native draw geometry/clips, keyboard/gamepad
navigation, pointer capture/click, focus loss and wrong-thread rejection. Its small deterministic
raster fixture tests ownership/lookup and does not establish Chinese typography or native GPU pixel
acceptance. Physical gamepad, DPI/font appearance and cross-platform product acceptance remain open.

## Player DPI conversion

The generic Player supplies logical input coordinates by dividing native pointer pixels by the published display DPI. Module UI dimensions use display logical extents. Before drawing, it calls `GameplayUiFrame::ToPixels` once on its owning frame copy, scaling vertices and clamping outward-rounded scissors to framebuffer bounds. User UI scale remains game-owned. The input service continues to expose native pixels. See [GameplayDisplay](GameplayDisplay.md).

## Image UI extension

See [GameplayUiDraw](GameplayUiDraw.md) for optional image/nine-slice primitives, style-free interaction, automatic texture generations, reusable font slots and snapshot-aware native retirement. UI V1 wire layout is unchanged.
