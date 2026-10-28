# Sprite source assets (S01)

This additive source-geometry slice extends the existing TwoD module. Atlas/Clip formats,
linear straight-alpha color, native Sprite instance records and C/Zig service prefixes remain
unchanged. Public C++ consumers rebuild for the appended optional `Sprite::source` identity and
`SpriteInstance` local quad. Source-bearing scenes use the additive NX2S schema 3 below; scenes
without sources retain byte-for-byte schema-2 encoding. The
[full 2D roadmap](../../Roadmap/en/2D_Roadmap.md) retains its existing platform verdicts; this supporting work does not accept M6/M7 or the complete Sprite expansion.

## Identity and wire

`SpriteAsset` owns values only: its UUID, Atlas UUID, stable region ID, untrimmed canvas size,
trim rectangle, pixel Pivot, PixelsPerUnit and four borders. Renaming source files or repacking an
Atlas does not change those references. Region dimensions must match trim dimensions when resolved;
missing/foreign/mismatched regions reject instead of silently selecting another frame. Region IDs
must still be retained by the authoring source; this slice does not infer identity after arbitrary
manual recutting.

The independent `.nx2sprite` schema 1 is exactly 96 bytes, little-endian:

| Field | Encoding |
| --- | --- |
| Magic, version | U32 `0x5032444e`, U32 `1` |
| Sprite UUID, Atlas UUID | Four U64: Sprite high/low, Atlas high/low |
| Region, source width/height, trim x/y/width/height | Seven U32 |
| Pivot x/y, PPU | Three IEEE-754 float32 |
| Border left/right/top/bottom | Four U32 |

Both UUIDs and the region are nonzero; source UUID cannot equal its Atlas dependency. Canvas
dimensions are 1..4096 with at most 4 Mi pixels, following existing image budgets. Trim is nonempty
and contained in the canvas; checked subtraction/widened sums reject integer overflow. PPU is
positive and finite. Pivots may be outside the source canvas, but all computed float32 local
corners must be finite, nondegenerate and within +/-1,000,000, and each Sprite size must be at most
1,000,000 world units. Border sums cannot exceed source dimensions; any nonzero border requires
an untrimmed source. Borders are stored for future sliced rendering, not rendered as nine-slice
by this slice. Invalid metadata encodes to an empty buffer; malformed/versioned/trailing data
decodes to `nullopt` with no published asset.

`RegisterImporters` atomically adds `.nx2sprite` together with its previous two importers. The
canonical type is `sprite_source`, with the Atlas UUID as its dependency. It uses the existing
Cooker, DDC, bundle hashing and generation store; it introduces no new module or asset manager.
Cook can validate metadata alone; region dimensions require the resolved Atlas and are checked
by `ResolveSpriteAsset`. A complete consumer must validate that closure before displaying it.

## Coordinates and extraction

Trim uses top-left source pixels; Pivot uses bottom-left source pixels. For canvas height H,
trim `(x,y,w,h)` has bottom-left `(x,H-y-h)`. Local geometry is:

```text
min = ((x - pivotX), (H - y - h - pivotY)) / PPU
max = ((x + w - pivotX), (H - y - pivotY)) / PPU
```

For a 64x64 canvas, trim `(16,16,32,48)`, Pivot `(32,0)` and PPU 32, this produces
`min=(-0.5,0), max=(0.5,1.5)`. A next frame trimmed to `(8,8,48,56)` produces
`min=(-0.75,0), max=(0.75,1.75)`. Both retain the same foot/pivot and world Y-sort anchor.

`ResolveSpriteAsset(asset, atlas, styledInstance)` copies its inputs, replaces the instance's
Atlas/region/size/normalized pivot and creates a pivot-relative local quad. Its world matrix,
identity, tint, visibility and sorting configuration remain unchanged. No pointer/span is retained,
no IO/upload/World mutation occurs, and no native handle is created. Callers serialize asset
publication and hold immutable Atlas data during the call, as with other TwoD services.

The prepared instance reaches the existing `ExtractSprites` path. Its optional local quad affects
projection/culling/CPU preview vertices and therefore the same native GPU instancing conversion;
it does not move the matrix used for sorting. Existing instances without a local quad use their
historical size/pivot/UV behavior. Flip in the new resolver mirrors local geometry about the source
Pivot and enables existing UV flips. Negative scale composes through the original world matrix.
Resolve again after changing source geometry, Atlas generation or flip state; no hidden per-frame
resolver/cache is introduced. Atlas validation is preparation work, not a recommended per-Sprite
per-frame operation. Retained extracted frames keep the existing Atlas ownership rules.

The direct resolver remains a transient adapter: it clears `sprite.source` on the returned
instance so the source-derived quad can enter the historical three-argument `ExtractSprites`.
Retain source identity in the authored Sprite rather than storing this resolved copy.

## Source references in scenes and extraction

`Sprite::source` optionally identifies a `SpriteAsset`. A present UUID must be nonzero; Atlas and
region may be omitted because the source supplies them. Size/pivot retain their historical finite
bounds even when overridden; tint, flip, visibility and layer/order/Y-sort stay authored. A source
never silently falls back to the old Atlas/region fields.

```cpp
Sprite sprite;
sprite.source = sourceId;
sprite.sortY = true;
scene.Set(entity, sprite);
auto frame = scene.Extract(camera, atlasResolver, spriteAssetResolver);
```

`SpriteAssetResolver` returns an immutable `shared_ptr<const SpriteAsset>`. Source-aware
`SpriteScene::Extract` and the four-argument `ExtractSprites` resolve each visible source and its
Atlas once per call. Input identities, matrices, styles and explicit quads validate before any
callback, then a copied snapshot prevents callbacks from changing the caller's geometry/styles.
Atlas closure validation is cached per UUID; per-source geometry preparation does not rescan all
Atlas regions per instance. No cache survives the call. The returned frame retains Atlas owners;
its copied geometry does not require source metadata after extraction. Methods and callbacks
serialize with asset/World publication; modifying objects exposed as immutable is unsupported.

Missing source/Atlas data counts each affected nonhidden instance as `unavailable`. Invalid source
identity/metadata, a mismatched region size, invalid Atlas/texture, duplicate native texture identity
or more than 64 resolved Atlases rejects the complete frame. Hidden/zero-alpha sources trigger no
lookups. Source resolution precedes camera culling, since the correct bounds require source
geometry. The existing no-source path retains its historical extraction. Three-argument general
`ExtractSprites` rejects unresolved source instances explicitly; two-argument scene extraction
uses an empty source resolver and reports them unavailable. A source plus an explicit `localQuad`
is ambiguous and rejects; `ResolveSpriteAsset` can explicitly replace that quad.

NX2S schema 3 preserves the existing World text, Sprite records and collider records. Each Sprite
record adds a U32 presence marker (0/1), then two U64 UUID halves only when present. Decoder versions
2 and 3 use the same existing bounds, membership checks and staging World. Invalid/truncated/trailing
or unsupported versions never replace the live World or metadata. Writers select version 3 only
when at least one source exists; after the last source is removed, version 2 is canonical again.
Old readers explicitly reject version 3 rather than dropping references. No source/Atlas payload is
embedded or rewritten during scene save; repacking/reloading refreshes geometry and UVs at the
next extraction while existing frames retain their prior Atlas generation. Isolated Play can load
these scenes and preserves the existing authoring-only Restore rule.

This completes portable C++ scene/general extraction source binding. Main Editor project indexing,
Inspector/Play capture and Player catalogs still need explicit source admission; C/Zig descriptors,
automatic source-asset animation and pool publication remain separate work. Passing only a resolved
legacy `Sprite` to `SpriteScene::Set` still discards its transient quad; store `source` instead.

## Manifest and CLI

```json
{
  "type": "sprite",
  "id": [11, 12],
  "atlas": [1, 2],
  "region": 7,
  "source_size": [64, 64],
  "trim_rect": [16, 16, 32, 48],
  "pivot_pixels": [32, 0],
  "pixels_per_unit": 32,
  "border": [0, 0, 0, 0]
}
```

`trim_rect` defaults to the entire canvas, Pivot defaults to its center, PPU to 100 and borders
to zero. Source metadata is explicit; there is no implicit alpha-trim or destructive source edit.

```sh
python3 Tools/TwoD/PrepareAssets.py actor.json --compiler build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler --output work/actor.nx2sprite
build/linux-development/Tools/TwoD/NexoraTwoDAssetCompiler cook work/actor.nx2sprite work/actor.nxcook
```

The repository's `Tools/TwoD/Fixtures/sprite.json` resolves region 1 of the existing Atlas
fixture; build `Fixtures/atlas.json` first. It is deliberately a source-metadata fixture, not a
new game asset or an automatically loaded Player catalog entry.

The compiler validates with `sprite` before publishing; the manifest helper stages and atomically
replaces outputs. Validation/compiler errors preserve previous outputs. Neither tool claims that
the main Editor content browser or Player catalog already admits the new source type.

## Selected grid cells

Existing Atlas image manifests can select explicit zero-based row-major cells:

```json
{
  "path": "monster.png",
  "grid": [64, 64],
  "first_id": 101,
  "cells": [0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]
}
```

On a 256x256 source this imports 12 of 16 cells. Identity is `first_id + originalCellIndex`, so
selecting `[3,7,15]` gives IDs `[104,108,116]`; selecting or reordering cells does not renumber them.
The packer's canonical ID order makes equivalent selections byte-identical. Existing manifests
without `cells` still select every cell with their previous IDs/output. An animation's frame order
is explicit in its Clip manifest, independent of Atlas packing/selection order.

Optional `margin: [left,top,right,bottom]` and `spacing: [x,y]` default to zero and must leave an
exact integral grid. Negative, duplicate, empty, noninteger, out-of-range cells and ID overflow
reject. Grid-only settings on a manual rectangle import reject instead of being ignored.

## Verification boundary

`twod.sprite_source` checks malformed wires, geometric budgets, hand-calculated trim/Pivot/flip
and negative-scale projection, unchanged sort anchors, region mismatch, repacking identity,
atomic importer registration and generation-store cooking. `twod.asset_tool` runs the real PNG
preparer/compiler, including 4x4/12-frame selection, sparse IDs, padding, deterministic output,
Sprite metadata cooking and preservation after failed publication. These are portable CPU/tool
checks; they are not fresh native pixel, minimum-PC or Windows/Mac/mobile acceptance.

See the [source foundation evidence](evidence/SpriteSource-Linux-2026-10-28.md) and
[scene binding evidence](evidence/SpriteSourceScenes-Linux-2026-10-28.md) for executed commands.
