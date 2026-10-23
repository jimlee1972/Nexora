# Image UI, reusable font resources and Zig composition

The standalone Player negotiates `NexoraGameplayUiDrawApiV1` through the optional
`get_ui_draw_api` services suffix. UI V1 and the original and SP-01 services prefixes are unchanged.
A new client on an old host keeps its historical services and observes this extension as unsupported.
An old SP-01-sized caller on a new host still receives its display getter. Check each returned
`struct_size` before reading a suffix. `Bindings/Zig/nexora.zig` performs these bounds checks.
Runtime only copies the wires; it owns no Presentation resources, layouts or game rules.

## Images and fonts

`register_image` copies tightly packed RGBA8 bytes before publishing an opaque nonzero handle.
Images are at most 4096x4096 and 16 MiB each, with 128 active images and 64 MiB active image pixels
per UI owner. At most 65,536 image handle generations and 65,536 font handle generations can be
issued per owner. Registration bounds are not the total retained-snapshot or GPU memory budget.
`register_font` accepts the existing copied `NexoraUiFontV1` descriptor. There are at most 32 active
fonts, 16 MiB and 65,536 glyphs per font. Removal recovers the active slot; stale handles never alias
another registration. Font enumeration uses the dense list of current active fonts and returns
opaque handles, which are not enumeration indices. Historical UI registration continues to work.

Use texture ID zero with this extension to allocate an immutable host generation. The upper
65,536 surface texture IDs are reserved for UI automatic generations, excluding the two
Presentation IDs and the UI white texture. Sprite/custom texture producers must keep their IDs
outside that range. Explicit nonzero IDs must remain distinct from all Sprite/font/image generations;
the UI owner rejects reused UI IDs even after removal. A UI owner and its native surfaces must share a texture namespace lifetime. Before replacing an owner on a reused surface, stop callbacks, drop old frames and release its textures, or recreate the surface; do not overlap two owners with automatic IDs in the same surface namespace. The bounded automatic namespace can exhaust
before the individual image/font generation caps; exhaustion reports `UNSUPPORTED`. IDs may be
consumed on allocation failure; output handles remain unchanged on rejection.

`remove_image` and `remove_font` invalidate the handle immediately, outside an open UI frame.
Old owning `GameplayUiFrame` copies retain the immutable bytes and may outlive the module/UI owner.
The host calls `CollectRetiredTextures` after those copies expire, then batches
`RenderSurface::ReleaseSpriteTextures` **before acquiring the next frame**. The standalone Player
implements that queue and propagates release errors. Surface release drains in-flight GPU work.
Stopping/unloading a module must release its handles or keep them deliberately for the next module;
shared-frame lifetime and GPU retirement are separate steps. An external host must implement this
queue too. Surface destruction retires any remaining allocations. Active/retired snapshots retained
by the application remain its memory responsibility. This does not implement SP-03 general asset IO.

All service operations use the UI construction thread; wrong-thread or open-frame registration/removal
returns `LIFECYCLE`. Buffers borrow only their call, table/context borrow the host lifetime through
module Stop/Destroy, and ordinary validation errors preserve outputs. Allocation failure can discard
a partial frame and requires the host's existing cancellation/recovery path. No exceptions escape C.

## Drawing and input

`image` takes source pixels, destination logical pixels, a logical clip rectangle and packed linear
RGBA tint. `nine_slice` divides the source using source-pixel insets, and the destination using
independent logical insets. If destination borders exceed the destination size they shrink
proportionally without negative geometry; zero-sized patches emit no quad. Invalid/nonfinite rectangles,
out-of-bounds source regions, negative insets and overlapping source borders reject before drawing.
The existing 65,536-quad per-frame budget includes images and all nine-slice patches.

The existing UI DPI conversion applies once to every image/text vertex and clip. Layout should use
Display's safe logical rectangle. These image primitives preserve the existing linear RGBA8/straight
alpha UI contract; they do not resolve SP-04 sRGB import/sampling or certify final art colors.

`interact` participates in the same unique-ID set, pointer capture, enabled navigation order and
activation/cancel edges as historical UI buttons, without painting. Zig composes its selected,
hovered, disabled and focus visuals around those flags. It must draw visible focus feedback, avoid
intersecting hit targets and stop submitting inactive-screen controls. Keyboard and gamepad use the
same host navigation contract; synthetic navigation is not physical gamepad acceptance.

## Zig and fonts

`nx.getUiDraw(services)` probes the extension. `registerImage(0,...)` and `registerBakedFont(...)`
select automatic generations. `gridCell` lays out bounded slots inside a supplied safe frame;
`wrapText` reads actual `get_glyph` advances, preserves UTF-8, prefers spaces and splits overlong words
at scalar boundaries. Width, font pixel-size, requested size, scratch and missing-glyph errors are
explicit. The caller controls font/line-height and layout policy. Complex shaping, language-specific
line-start/end punctuation, rich text and IME editing are not implemented. Final Japanese/Traditional
Chinese typography must review these limits and may supply prepared line breaks in Zig content.

`Tools/TwoD/BakeFont.py` uses the pinned Pillow/FreeType and fontTools dependencies. It checks every
UTF-8 scalar in every supplied text file against the selected font's cmap before rasterizing. No
fallback or synthetic box counts as coverage. It emits NXFONT1 and JSON with font/input hashes,
missing glyphs, atlas dimensions, bytes and slot requirements; an unsupported corpus or atlas
budget fails without publishing a font. Audit mode emits only coverage/budget JSON. Pass the final
localized strings and Japanese names; planning documents alone do not establish final-script coverage.
Preserve the source font's license with exported atlases. See the [tool instructions](../../Tools/TwoD/README.md).

The Zig-only `Gameplay/Zig/src/ui_showcase.zig` demonstrates nine HUD slots (4/2/3), three choices,
loadout, wrapped subtitles and settings with 100/150% UI scale. Its real Noto raster subset has
source, corpus, hash and OFL attribution in `src/fixtures`. Its colored icon/9-slice atlas is explicitly
diagnostic, not approved skill/portrait/paper art. All screen composition stays in Zig.

## Evidence and remaining product acceptance

See [SP-02 cloud evidence](../../Tools/Build/evidence/SP02-ImageUi-Cloud-2026-10-23.md).
The cloud gates cover wire layout, original/SP-01 prefixes, resource lifetimes, nine-slice UV/clips,
DPI geometry, glyph metrics, stale handles, synthetic menu operation, full-corpus baking and an
independent exported Zig consumer. The native Linux pixel test checks image borders/center/clips
and GPU retirement when Vulkan/Xvfb prerequisites are available. Final game assets, final scripts,
physical Windows DPI/typography/input and gamepad acceptance remain separate, open gates.
