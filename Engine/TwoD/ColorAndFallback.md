# 2D color and layered fallback

## Color contract

All existing Atlas, UI image and tint APIs consume **linear RGB with straight alpha**. Their C/Zig
ABI, Atlas schema 1, resource ownership, owner-thread requirements and copied snapshots are unchanged.
The compositor blends in linear attachment space. The native 2D Player requests the new explicit
`srgbUiOutput` surface mode: DX12 uses an sRGB RTV/PSO, Metal an sRGB drawable/UI pipeline, and Vulkan
requires an sRGB swapchain format. Hardware decodes the destination for blending and encodes the
final result once. A missing required format fails surface creation; there is no UNORM fallback.
Tests require encoded gray/composite pixels; a successful headless run is not output-color acceptance.

The option defaults false for historical mixed 3D surfaces. A true surface is UI/Sprite only:
`DrawScene` returns `Unsupported` and scene residency/bounded-view capability queries return false.
`CompositeRgba8` retains its separate byte-copy semantics (caller supplies output-encoded pixels).
C++ surface descriptor layout changes require rebuilding native consumers; no C/Zig ABI changes.
Render-thread, frame acquisition, error propagation and lifetime rules are unchanged. Resize and
surface recreation preserve the requested output mode.

`PrepareAssets.py` now accepts explicit atlas `color_space: "sRGB"` and `alpha_mode: "straight"`.
The default remains `linear`/`straight` for old manifests. sRGB RGB is decoded once by the standard
piecewise transfer, rounded to nearest linear UNORM8, then sliced/extruded/cooked. Alpha is never
gamma transformed or premultiplied; even RGB under zero alpha is decoded independently. Standard
PNG sRGB/gAMA/cHRM metadata is checked in this explicit mode; conflicting/duplicate profiles and
ICC reject before the existing atomic output publication. Untagged pixels follow the manifest.
Normalize other profiles externally. See the [tool instructions](../../Tools/TwoD/README.md).

For example, sRGB `(128,128,128,128)` becomes linear bytes `(55,55,55,128)`. Do not decode that
cooked texture again. Do not select sRGB for a normal/occlusion/mask texture that stores linear data.
The tool does not infer color from filenames or invent missing emission masks.

`Bindings/Zig/color.zig` has caller-owned, allocation-free helpers:

```zig
try nx.color.decodeRgba8(raw_srgb_pixels); // before registerImage; never on cooked bytes
const tint = nx.color.spriteTint(.{128, 128, 128, 128}); // float linear RGB, linear coverage
const ui_tint = nx.color.uiTint(.{128, 128, 128, 128}); // existing packed UI format
```

Incomplete RGBA rejects before mutation. Buffer lifetime and image-copy behavior remain those of
the existing image registration API. Helpers perform no PNG/ICC parsing and require no host API.
Float Sprite tints retain more precision than byte image/UI storage. Linear UNORM8 loses dark
shades; final palette/gradient acceptance remains open. This change does not add tagged sRGB GPU
textures, HDR or wide gamut. Historical mixed 3D UNORM surfaces retain their existing output policy.

## Zig diagnostic fallback

`Gameplay/Zig/src/color_slice.zig` composes a fixed-roll, 24 m orthographic view using existing
Sprite/instance APIs. A procedural imported Atlas supplies ground, actor, foreground, independent
baked glow and soft-shadow masks, a danger outline and gray swatch. Zig changes foot-point Y,
foreground opacity, and decoration visibility; reduced effects retains the opaque danger shape.
The gray UI swatch toggles effects after the first four diagnostic frames. No gameplay rules or
camera policy were added to C++. The generated fixture is not production art.

The glow is an ordinary straight-alpha layer, **not additive emission or selective bloom**.
The shadow is a baked sprite, **not dynamic light occlusion**. This is the allowed pure-2D fallback,
not a claim that the 3D scene lighting path is connected to the 2D Player. Proper dynamic local
light, soft shadow and selective bloom APIs remain deferred. The camera is never changed to 45°.

## Acceptance boundaries

Portable tests verify source transfer/profile rejection, deterministic packing, untouched alpha,
legacy defaults, Zig helpers and relocated Player exports at 1080p, 4:3 and odd ultrawide sizes.
The native Linux test checks a gray swatch and straight-alpha composite with one consistent output
encoding, then crosses Y and fades the foreground. Local compilation is not native pixel evidence;
record actual hosted runs separately.

A final-art 1080p first level, dark gradients, white-edge/transparent filtering, 100/75/50% readability,
low-effects hazards, target Windows output, target RAM/VRAM/frame budget and physical input remain
open in Beads `nexora-ofe.1`. No test fixture substitutes for that gate or existing M6/M7 acceptance.
