# Cooked gameplay content registration

`GameplayContent` supplies the optional `NexoraGameplayContentApiV1` C11 table in
`nexora/gameplay_content.h`; `Bindings/Zig/content.zig` mirrors it as `nexora.content.Api`.
The existing gameplay host exposes this original Atlas/Clip table. A Zig module registers cooked
`.nx2atlas` and `.nx2clip` bytes, including `@embedFile` data,
without game-specific C++ code, native paths, renderer pointers or a second content format.
The existing `DecodeAtlas`/`DecodeClip` codecs perform all schema, UTF-8-independent ASCII event,
geometry, count and pixel budget checks. This API consumes the cooked payload, not a bundle container;
the embedding remains responsible for bundle/hash/authenticity checks when acquiring those bytes.
Use the existing PNG preparation/import/cook tools to build production assets.

## Registration and identities

`register_atlas` and `register_clip` borrow a valid byte buffer and two distinct aligned output
pointers for their call. They publish the UUID encoded in the payload only after successful
registration. Null pointers, zero bytes, more than 64 MiB, malformed/truncated/trailing data or
cross-type UUID collisions return `INVALID_ARGUMENT` without changing outputs or the registry.
A legacy Clip's Atlas must already be registered; a schema-2 source Clip requires every referenced
Sprite source to be registered (and thus validated against its Atlas). Unavailable or wrong-typed
dependencies return `UNSUPPORTED`. Every legacy animation region must exist in its Atlas. Atlas replacement checks every resident dependent
clip and rejects removal of a referenced region. Registered identities remain active until explicit removal or owner destruction; there is no
implicit unload on module stop/reload. The optional [asset service](GameplayAssets.md) adds removal,
bounded catalog IO and usage without changing this V1 wire.

Publication is atomic per call. Replacing an Atlas with identical decoded data reuses its immutable
owner and native texture ID. Changed bytes receive a fresh generation with a different texture ID.
Clip replacement copies an immutable owner. Existing extracted frames, clip resolver results and
Atlas bindings retain their prior owners after replacement or registry destruction. Active entries default to 256 identities and 64 MiB of decoded pixels/region/frame/event storage;
a budget failure returns `UNSUPPORTED`. Configurable limits additionally reserve active-plus-retired
generations (default 128 MiB), so retained snapshots apply backpressure rather than unbounded growth.
Hosts collect expired generations and retire native texture IDs; see the asset service contract.

Texture IDs are process-unique generation IDs in the reserved range `[1 << 62, 1 << 63)`.
The host must reserve that range for GameplayContent and keep explicit UI/Sprite resource IDs outside
it. IDs are stable while a decoded Atlas generation is current, including byte-identical reloads,
and are never recycled. A new surface receives uploads again through normal Sprite extraction.
GPU ownership, texture retirement and surface teardown remain host responsibilities.

`describe_asset` returns copied metadata for a known UUID. `list_assets` returns deterministic
Atlas-first, then Clip ordering, with each kind ordered by UUID high/low. Null output with zero
capacity probes count. A short nonnull buffer returns `UNSUPPORTED`, publishes only the required
count and leaves its elements unchanged. A missing description returns `UNSUPPORTED` and leaves
its output unchanged. Atlas metadata reports region count/dimensions and its own UUID; Clip metadata
reports frame count and the dependent Atlas UUID. These APIs expose content identity, not native
texture handles or C++ memory.

## Host integration and lifetime

The owner supplies `GetApi`, `ResolveAtlas(AssetId)` and `ResolveClip(AssetId)`. The host routes
module 2D animation resolution through `ResolveClip`, and native Sprite extraction through
`ResolveAtlas` returning the shared Atlas plus its immutable texture generation. Existing World,
Sprite, pool and Tilemap APIs can reference the returned UUIDs directly. Hosts may fall back to
another resolver for separately provisioned content; UUID ownership must remain unambiguous.

All operations serialize on the construction thread. C callbacks return `LIFECYCLE` for wrong-thread
calls and contain host exceptions; direct resolver methods throw on wrong-thread access. These registration callbacks have no deferred workers or reentry into module code.
The separate optional asset service performs bounded background IO before owner-thread publication. The table/context borrow the
registry owner until every module callback has stopped. Input bytes may expire after registration;
returned metadata is copied and resolver results own immutable data. Caller pointers must refer
to valid aligned, nonaliasing buffers of their declared lengths. The API does not validate arbitrary
native memory addresses. Allocation failure returns `LIFECYCLE` without publishing a partial asset.

## Verification

`twod.gameplay_content` round-trips actual existing Atlas/Clip cooked codecs and checks missing
Atlas dependencies, region mismatches, malformed/truncated/trailing bytes, UUID type collisions,
atomic replacement, identical-generation reuse, changed-generation ownership, registry count budget,
list probes/retries/order, description and wrong-thread rejection. Snapshots remain valid after
owner destruction. `gameplay.zig_content_wire_layout` compares the real Zig table/metadata layouts
with the C header. These are reusable loading and ownership checks; they do not generate formal
artwork or establish graphical product or target-platform acceptance.

## Optional Sprite source registry table

`GameplayContent::GetSpriteApi` supplies the independent
`NexoraGameplaySpriteContentApiV1` table; Zig mirrors it as `content.SpriteApi`.
Initialize `struct_size` before acquisition and check returned size/version/callback before use.
A short table returns `UNSUPPORTED` unchanged; an oversized caller receives only the V1 prefix.
The original Content table and Sprite/Instance descriptors keep their layouts. The canonical Player
now advertises this table through the optional services suffix `get_sprite_content_api`; Zig
`getSpriteContent` checks the aggregate size/version and the returned table. Historical services
prefixes remain supported. Main Editor Play resolves frozen project source snapshots, but runtime
source registration through this services suffix remains `UNSUPPORTED`. Inspector authoring,
Build capture and source-animation reload remain open.
The [asset catalog](GameplayAssets.md) now admits cooked sources as catalog kind 4, and the Player
accepts preloaded `sprite` and deferred `deferred_sprite` files. Kind 3 remains opaque game-owned
bytes in the catalog; it identifies a Sprite source only in the separate content registry.

`register_sprite_source` decodes the existing 96-byte `.nx2sprite` schema 1 and returns its UUID.
The Atlas must already be registered. Missing/non-Atlas dependencies return `UNSUPPORTED`; malformed
metadata, UUID type collisions, missing regions and trim/region dimension mismatches return
`INVALID_ARGUMENT`. Rejection preserves outputs and published content. Atlas replacement validates
all registered Clip and Sprite dependencies through the existing codecs/resolver; incompatible
replacement preserves the previous generation. Removing a referenced Atlas returns `LIFECYCLE`;
remove dependent Clips and sources first. Active source Clips and retained retired Clip owners
prevent removal of their sources with `LIFECYCLE`. Source playback retains its exact Clip owner,
so replacing/removing a Clip cannot invalidate its remaining frame dependencies. Other source
removal does not mutate World/pool instances. Source Clip descriptions retain content kind 2 and
frame count; their single-Atlas metadata is zero because frames may use multiple Atlases.

`ResolveSprite` returns an immutable owning source snapshot for the existing source-aware
`ExtractSprites` resolver. Identical publication reuses that owner even at the live-byte ceiling;
changed metadata receives a new owner. Retained source snapshots survive replacement, removal and
registry destruction. Extracted frames retain their Atlas generation and copied geometry independently
of the source metadata. Source snapshots alone do not retain an Atlas. All callbacks, acquisition,
resolution and removal follow the same owner-thread, exception and borrowed-table lifetime rules
as the original registry, with no IO, worker or module callback added.

Sources share the existing identity/active/live limits and bounded retirement queue. Their decoded
storage charge is `sizeof(SpriteAsset)`; retired sources remain charged until their owners expire
and `CollectRetiredTextures` collects them, without producing a native texture ID. Counts and bytes
include sources. `list_assets` sorts Atlas, Clip, then source, each by UUID; source descriptions use
content kind `NEXORA_CONTENT_SPRITE_SOURCE` (3), count 1, untrimmed canvas width/height and the Atlas
UUID. **Content kinds and asset catalog kinds are separate namespaces**: catalog kind 3 remains
opaque game-owned bytes; source files use catalog kind 4 through the separate asset service.

`twod.gameplay_content` additionally checks real source decoding/extraction, both-direction type
collisions, dependency-preserving rejection, short/extended-table canaries, source/frame retention,
owner-thread rejection and active/live budget backpressure. `gameplay.zig_content_wire_layout`
checks both C/Zig tables and optional-table admission/forwarding. See the
[cloud evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-SourceRegistry-Linux-2026-11-12.md).
