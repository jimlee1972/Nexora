# Bounded gameplay asset IO and residency

`NexoraGameplayAssetsApiV1` is an optional, independently negotiated service appended to
services V1 after image UI. Original, SP-01 and SP-02 service prefixes remain complete and
unchanged. Check the returned `struct_size` before reading `get_assets_api`; Zig uses
`nx.getAssets(services)`. Old hosts remain usable without this optional capability. Content V1
and its C/Zig wire are unchanged. Clip payloads additionally admit source schema 2 through the
existing kind 2 path; see [source animation](README.md#source-animation-clips).

## Catalog, requests and ownership

The host freezes an ordered catalog of at most 4096 files. `describe_catalog(index)` returns
kind and file length without reading the payload; an unavailable index returns `UNSUPPORTED`.
Kinds 1/2 are existing cooked Atlas/Clip containers, 3 is opaque game-owned bytes, and 4 is a
cooked Sprite source (`sprite_source`, `.nx2sprite` schema 1 payload). C names these
`NEXORA_ASSET_CATALOG_*`; Zig uses `assets.catalog_*`. Catalog kind 4 is distinct from
content registry kind 3. No existing table, descriptor stride or kind value changes. The standalone
Player exposes only deferred entries, in descriptor order. Preloaded entries are not catalog
indices. Paths never cross this C ABI. The trusted host reader enforces package containment,
rechecks file size and performs a bounded read of exactly the declared length (1..64 MiB).

`request` reserves its full file length and returns a monotonically increasing, nonzero ticket.
At most 64 requests and 64 MiB of default reservations exist simultaneously. A full queue/budget
returns `UNSUPPORTED` without publishing a ticket. Requests progress QUEUED → LOADING → READY or
FAILED. `poll` only copies state; call success does not mean asset success. Check phase/result.
IO completion is nondeterministic. There is one background reader; no gameplay callback runs on
it. The host's owner-thread `Tick` polls without waiting for IO, then synchronously verifies the
cooked hash/type/UUID/dependency metadata and publishes through Content V1 or the separate
Sprite source registration table. Sources require container/payload UUID equality and exactly
one dependency matching their Atlas. Registry admission checks the live Atlas region and trim
dimensions. Decode/hash work
can consume owner-thread time: this is loading-screen/chapter IO, not a guaranteed frame-time
streamer. A legacy Clip or source requires its Atlas to be ready first; a source Clip requires all
its sources first. Source Clips require exact sorted, unique source dependency edges in the cooked
container. No implicit dependency loads occur.
Requesting a source before its Atlas fails with `UNSUPPORTED` and publishes no source. Load the
Atlas, then issue a fresh source request; a failed ticket does not restart itself.

Catalog loads reject a UUID already active in the registry, rather than replacing another
owner's preloaded or manually registered asset. After a successful Atlas/Clip/source request, its UUID
belongs to the registry independently of the ticket. `release_request` forgets a completed
request; it does **not** unload its content. Explicit `remove_asset(high, low)` unloads a UUID.
Removing an Atlas with resident dependent Clips or sources returns `LIFECYCLE` atomically. Remove dependents
before their Atlas. Missing/stale tickets and missing UUIDs return `UNSUPPORTED`. Tickets never
alias later requests within the service lifetime; do not carry them to another service owner.

Releasing queued requests cancels them immediately. Releasing an in-flight request invalidates
its ticket immediately but retains the file reservation until that worker drains; its result
is discarded without publication. A caller cannot create unbounded abandoned readers by
cancel/retry. Failed requests release their reservation but retain copied error state until
`release_request`. Ready opaque bytes retain their reservation and payload until release.
`copy_bytes` copies a READY kind-3 payload; null/zero probes its length, and a short nonnull
buffer returns `UNSUPPORTED`, publishes only required length and preserves buffer contents.
Opaque bytes are not decoded, hash-authenticated or installed into UI/audio automatically.
Zig validates their own format and owns subsequent font/image/audio registrations and cleanup.

## Budgets and native retirement

`GameplayContent::Limits` defaults to 256 active identities, 64 MiB active decoded storage and
128 MiB active-plus-retired storage. Hosts may configure up to 4096 identities, 1 GiB active
and 2 GiB live storage. New data must fit active and live budgets before publication; replacement
reserves the old generation plus the entire new generation. Budget failures preserve the old
registry and outputs. Retired metadata is bounded to 65536 generations between collection calls.
All defaults retain the old 64 MiB active limit; deferred files do not expand it implicitly.

Removing/replacing an entry records its immutable owner weakly. Old resolver results and
extracted frames keep their Atlas/Clip/source alive according to the
[source ownership contract](GameplayContent.md#optional-sprite-source-registry-table). A source
snapshot alone does not own an Atlas. `CollectRetiredTextures` returns each expired
Atlas texture ID exactly once, and drops expired Clip/source metadata. Before native acquire, the
standalone Player forwards IDs to `ReleaseSpriteTextures` and checks its result. Re-entry uses
a fresh process-wide texture generation, never a recycled ID. A retained old frame therefore
cannot accidentally reference a new chapter's pixels. Custom hosts must connect this collection
and native release path too; resource removal alone does not release a GPU cache.

`get_usage` reports active identities, conservative retired generations/bytes (including expired
owners awaiting collection), request count, reservation bytes and the observed high-water decoded
content count. `texture_bytes` is the RGBA payload of potentially resident active/retired Atlases,
not measured device allocation. IO/codec workspace can temporarily coexist with decoded content;
budget up to four times the largest admitted file in addition to content/reservations. This
logical budget excludes allocator overhead, reader stacks, World/pool state, UI, PCM audio and
backend upload buffers. It is **not** total-process RSS or physical VRAM evidence. Staging may be
configured to 1..128 MiB, but each file stays at most 64 MiB and only one reader executes.

Games own transition policy: stop references/animation, preserve gameplay events, despawn or
recycle instances, remove Clips/Atlases, allow snapshots/native retirement to drain, then load
the next chapter. Removal does not silently destroy World entities, pool instances, Tilemap
cells or game events. A configured pool can hold 1..100000 copied instances; default is still
1024. Shared extraction/draw/texture limits remain unchanged, including 128 textures per prepared
packet and the existing codec limits. Larger capacity does not establish performance acceptance.

## Threads, failures and shutdown

All public callbacks and registry mutations run on their construction thread. Wrong-thread calls
return `LIFECYCLE`; C callbacks contain exceptions. Null/malformed arguments return
`INVALID_ARGUMENT`; ordinary rejected outputs remain unchanged except documented length probes.
The caller provides valid aligned, nonaliasing memory; this ABI cannot validate arbitrary native
addresses. Table/context and reader services remain alive through module Stop/Destroy. The host
must stop all modules, call `GameplayAssets::Shutdown` to join/discard pending IO, and only then
shut down borrowed VFS/engine services. Destruction also joins its pending reader. Local IO is not
forcibly interrupted; cancellation/shutdown can wait for the underlying read to return. No worker
can outlive those services or publish after shutdown.

## Verification boundary

`twod.gameplay_assets` checks copied state, original/SP-01/SP-02 canaries, actual background reads,
corrupt cooked admission, opaque probes/copies, wrong threads and cancellation reservations.
It also exercises kind-4 source identity/dependency/type/hash checks, Atlas-first requests,
region/trim rejection, duplicate protection and immutable source/Atlas retention.
`twod.sprite_catalog_export` runs an independent C ABI module from a relocated package after
deleting its authored inputs: canonical source cooking, preloaded source metadata, real deferred
IO, missing dependencies, opaque kind-3 copies, dependency-ordered removal and corrupted package
rejection are checked. `twod.sprite_source_catalog_player` uses the accepted optional C/Zig source
API with actual preloaded files. Its raw initial source/Atlas are removed before Zig compilation;
the relocated Player checks scene/pool metadata, source/Atlas replacement and Linux native pixels.
The original module-embedded fixture remains intact. Main Editor authoring stays separate. See
the [catalog evidence](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-SpriteCatalog-Linux-2026-11-12.md).
`twod.asset_residency_stress` repeats five 2048² Atlases (>80 MiB decoded) through eight complete
load/unload rounds with retained-generation checks and zero final resource counters. It samples
actual process RSS/working set when available and rejects more than 32 MiB growth in the final
four samples above the first four warm-up samples; unavailable readings are labeled, never zero
evidence. That tolerance and short workload are a regression check, not a production RAM budget. The independent
Zig transition module uses a >80 MiB exported package, asynchronous local IO, 1800 pool instances,
dependency-ordered unloading and 30 real Animator events across 15 diagnostic transitions.
The native pixel test additionally checks 2048² upload, retained-frame draw, retirement and fresh
generation re-entry. Platform evidence must identify which tests actually ran; compilation alone
does not accept native GPU behavior. Final game art, target-PC RAM/VRAM/frame-time peaks and complete
combat/pickup/scene-transition acceptance remain separate gates.
