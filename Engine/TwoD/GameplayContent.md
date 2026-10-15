# Cooked gameplay content registration

`GameplayContent` supplies the optional `NexoraGameplayContentApiV1` C11 table in
`nexora/gameplay_content.h`; `Bindings/Zig/content.zig` mirrors it as `nexora.content.Api`.
A Zig module registers cooked `.nx2atlas` and `.nx2clip` bytes, including `@embedFile` data,
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
A clip's Atlas must already be registered; unavailable dependencies return `UNSUPPORTED`.
Every animation region must exist in that Atlas. Atlas replacement checks every resident dependent
clip and rejects removal of a referenced region. Registered identities remain active for the session;
there is no implicit unload on module stop/reload and no removal API.

Publication is atomic per call. Replacing an Atlas with identical decoded data reuses its immutable
owner and native texture ID. Changed bytes receive a fresh generation with a different texture ID.
Clip replacement copies an immutable owner. Existing extracted frames, clip resolver results and
Atlas bindings retain their prior owners after replacement or registry destruction. Active entries
are bounded to 256 identities and 64 MiB of decoded pixels/region/frame/event storage; a budget failure
returns `UNSUPPORTED`. Externally retained old generations are intentionally outside that active
registry budget; the host bounds its in-flight frame retention and retires old native texture IDs.

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
calls and contain host exceptions; direct resolver methods throw on wrong-thread access. There are
no deferred workers or callbacks and no reentry into module code. The table/context borrow the
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
