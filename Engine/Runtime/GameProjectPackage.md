# Multi-scene package catalog contract

`GameProjectPackage.h` is the Runtime-only consumer for a versioned, immutable catalog of authored
scenes and their shared mesh/scalar-PBR dependency closure. It performs no code loading, gameplay,
native resource creation, scene transition or source lookup. The catalog is a prerequisite for the
[scene delivery goal](../../Roadmap/en/Project_Scene_Delivery_Plan.md), not complete game acceptance.
The existing [StaticView schema 1](ProjectPackage.md) remains a distinct format/capability.

## Wire and bounds

Schema 1 starts with eight bytes `NXGAME\0\1`, little-endian u32 schema=1 and capability=2,
project UUID high/low u64, startup UUID high/low u64 and u32 scene count. Each scene record contains
its full UUID high/low u64, u32 UTF-8 locator byte count and those locator bytes. Records sort by full
UUID `(high,low)`. A u32 asset count follows, then u64 byte length plus schema-1 NXAB bytes for each
asset, sorted by full UUID. Dependency UUIDs sort canonically inside each NXAB record. A final u64
FNV-1a of all preceding bytes uses basis 1469598103934665603 and multiplier 1099511628211.
FNV detects accidental corruption, not authenticity or authorization to execute code.

Limits are 256 scenes, 1024 UTF-8 bytes per locator, 4096 unique assets, 256 MiB serialized package,
96 MiB per NXAB blob, 64 MiB aggregate Runtime scene snapshot text, 100000 aggregate entities and
128 MiB aggregate decoded geometry. Existing stricter per-asset codecs still apply. NXAB currently
requires a little-endian host. Counts/lengths are checked before copying variable-length wire data;
aggregate content admission rejects the candidate before it can replace a retained package.
These are admission bounds, not peak process memory or performance guarantees.

Startup must name an included nonzero scene UUID. Scene locators are unique portable relative
`.scene` spellings, not source-file access authority. Invalid UTF-8/control/alias spellings,
duplicate UUIDs/case-alias locators, wrong versions/capabilities, truncation, trailing data and
checksum failures reject. Catalog and asset order are canonical; changing input vector order
does not change encoded output. Project identity is separate from asset identity.

## Content and ownership

The package stores each scene once and each shared cooked resource once by full UUID. Every
listed scene must have one matching scene blob. No extra/unreferenced asset, missing dependency,
cross-scene serialized entity reference, unknown asset type or global mesh-resource collision is
accepted. Each scene uses the production StaticView resolver as its bounded data-validation
authority: actual World snapshot/hierarchy/components, exact binding dependencies, material
reference metadata, raw NXAB hash and BundleBuilder integrity checks are preserved.

Unlike StaticView inspection, this capability rejects opaque records whose required behavior
would remain inactive. Known scalar-material metadata is admitted with the existing exact
version/binding checks. No claim is made for arbitrary 2D, physics or plugin asset formats.

`LoadedGameProject` owns its sorted catalog and shared immutable scene templates. Templates own
isolated Play-kind Worlds and authored scene snapshots; matching UUIDs share decoded mesh/PBR
owners across templates. Resolution by UUID or exact catalog locator returns shared ownership;
it remains valid after catalog destruction. A failed decode does not mutate an older catalog.
Borrowed `Scenes()` expires when its owner is moved/destroyed. Ordinary allocation exceptions
propagate. Concurrent const resolution is allowed only without concurrent owner mutation/destruction;
all authoring/session publication remains externally serialized.

Template entity IDs are authoring-local and may overlap across different scenes. They must be
remapped before instances enter the same live World; a template alone does not implement additive
loading, persistent-scene policy, GPU retirement or module lifecycle. Those remain Runtime session work.
Resolution retains no Editor/workspace/file borrow and performs no further filesystem access.

## File/CLI consumer and compatibility

`ReadGameProjectPackage` shares the bounded explicit-regular-file reader and NXAB preflight helpers
with StaticView. POSIX walks descriptor-relative parents with O_NOFOLLOW and verifies the opened
regular-file descriptor. Windows checks path components, rejects symbolic links/reparse points
and bounds streaming reads. This cooperative file access contract does not claim protection from
hostile concurrent filesystem mutation. Existing StaticView wire bytes/opaque inspection semantics
are unchanged. C++ consumers rebuild; the stable C/Zig ABI has not changed in this package slice.

`NexoraProjectPlayer --verify-game-package PATH` reports schema/capability, startup, scene/asset
counts and explicit native_rendering=false/gameplay_loaded=false. It consumes no authored source
files. Legacy `--verify-package` continues to reject this separate format and accepts StaticView.
Native startup/transitions and zero-argument built-game delivery are subsequent integration work.
