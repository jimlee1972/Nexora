# Linear emission companions and selective bloom

Emission is explicit Sprite configuration, never inferred from a bright color. The existing
C/Zig Sprite/Atlas/Clip layouts remain unchanged. The optional effects Sprite identifies a
separate mask Atlas/region and linear RGB/intensity. Mask **red** is linear data; mask alpha is
ignored. Actual Sprite texture alpha times tint alpha supplies coverage. Mask sampling is linear;
nearest-mask Atlases reject explicitly. Base Sprite sampling may remain nearest. HUD is never
part of scene/emission targets.

## Companion authoring and validation

[PrepareEmission.py](../../Tools/TwoD/PrepareEmission.py) consumes a separate manifest, inherits
source rectangles, grid margins/spacing and selected cell identities from the base Atlas manifest,
and requires matching original mask/source canvas dimensions. It never sRGB-decodes mask bytes,
even when the base is sRGB-authored. Both use the existing canonical packer and extruded padding;
a different mask packing width is supported. Extraction maps base UV to the matched packed mask
rectangle with scale/bias, preserving Sprite flips and source trimming. Region dimensions must
match. A bad/missing region or mismatched sampling contract rejects instead of stretching a mask.

The separate little-endian `.nx2emission` metadata has magic `ND2E` (0x4532444e), schema 1,
metadata/base/mask UUIDs and 1..1024 base-to-mask region pairs. UUIDs are nonzero and distinct;
base region IDs are unique. Length, schema, identities, region dimensions and dependencies are
validated by the native compiler. Encoding sorts pairs by base ID. Atlas/Clip codecs do not change.

The tool publishes immutable content-addressed mask/links files before atomically replacing the
small JSON catalog. A failed build retains the previous catalog and its referenced bytes. A late
catalog publication failure may leave an unreferenced new file, never a partially replaced old
generation. Do not rewrite a content-addressed file; conflicting existing bytes reject.

## Runtime ownership, loading and export

Cook/export the mask as an ordinary Atlas; export links as an existing deferred **blob**. This
reuses the existing asset catalog/inventory/request/reservation limits without extending its wire
or adding an Editor authoring tool. Explicit descriptor asset entries include both dependencies;
there is no hidden filename lookup or automatic chapter transition. See [tool usage](../../Tools/TwoD/README.md).

Zig reads copied READY blob bytes and calls `nx.twod_effects.EmissionAsset.parse(bytes)`; the parsed
view **borrows those bytes**. Keep them alive while using the view. `maskFor` and `apply` configure
an existing effects Sprite; load/register the referenced mask Atlas before submit. Release the
blob request after copying, and own/free that copied buffer through the normal game lifetime.
The C++ decoder instead returns owning metadata. Neither parser allocates or owns a GPU resource
through the Gameplay ABI. Submit still validates/copies complete effects and preserves the last
valid snapshot on rejection.

Player resolves current identities and mask dependencies after updates. Extraction retains the
exact immutable base/mask Atlas generations and copies per-command styles before sorting/batching.
Only adjacent equal Atlas/effect styles merge; scene and emission write together in that order.
Ordinary opaque foregrounds erase previous emission; translucent foregrounds attenuate it by
coverage. Old frames keep masks after registry removal. The normal expired-generation collector
then retires native texture IDs after GPU completion. Re-entry obtains a fresh ID. Links have the
existing blob reservation/copy/release lifetime; they are not implicitly installed in the Atlas
registry. Games stop references and remove chapter assets explicitly.

## Composition, strength and diffusion

Scene receives explicit emission times decorative strength. The separate emission MRT stores
unscaled emission with the same ordered alpha blend. Bloom samples only that target, so ordinary
white surfaces do not glow. Bloom intensity times frame strength is applied **once** at composition;
it is not multiplied by the already scaled scene emission. Ambient light remains independent.
Zig owns effect levels and critical hazard outlines; 100/75/50% scales decoration, off selects the
legacy Player path.

Bloom starts at ceil(width/2), ceil(height/2), at least one pixel. At most five down levels and
four up levels use four-tap downsampling and normalized nine-tap tent upsampling with equal-weight
detail accumulation. Diffusion [0,64] apportions tap displacement across passes in output pixels;
multiresolution reconstruction also has its inherent sampling footprint. This is an artistic
spread control, not an optical light radius. All inputs/outputs are linear RGBA16F. Final fixed
SDR mapping and one sRGB encoding precede unchanged HUD rendering.

Targets and descriptors belong to completion-protected frame slots. Resize/configuration changes
and disabling bloom release unused slots after their fence/completion, and drain releases all.
Zero framebuffer extent suspends drawing. Capability/pipeline/allocation failures are explicit;
no 3D bloom entry is called. `twoDBloomPasses`, `twoDBloomTargetBytes` and `twoDEmissionDraws` count
recorded work/current allocations, not resident VRAM or measured GPU duration. Timing and the real
Zig interactive diagnostic belong to HD2D-06. [Evidence](evidence/HD2D-05-Linux-2026-10-31.md).


When the Player disables the effects pipeline, ordinary legacy drawing releases the effects
resources belonging to that already completed slot. After each in-flight slot has a legacy frame,
scene/emission/bloom/shadow/upload owners are gone. No extra global wait is added. A flag persists
from a linear submission through Present: its HUD, including separately appended UI, cannot
retire targets referenced by commands recorded in the same frame. Re-enable creates fresh targets;
bounded compiled pipeline objects remain cached until surface destruction.
