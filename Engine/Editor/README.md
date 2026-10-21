# Editor Core contract

## 2D authoring document

✅ TwoDDocument is compiled only with NEXORA_ENABLE_2D and owns an isolated Runtime World,
SpriteScene, Atlas, clip and Tilemap. It uses Runtime UndoStack with whole-document snapshots,
bounded to 256 transactions and 64 MiB retained bytes; a new transaction resets older history when the count/memory limit is reached. A single
transaction exceeding the memory budget rejects without changing the document. Invalid edits restore the previous snapshot and do not record history.
Reopen validates all codecs, UUID/frame/region references and scene membership in a staging World
before publishing. Read-only documents reject mutations/history/reopen; saved bytes are owning,
and Dirty compares the current canonical snapshot with the last MarkSaved snapshot. Reopen clears
history and marks saved only after acceptance. StartPlay loads a distinct Play World; runtime
changes never mutate the authoring document. Owner-thread only; no disk IO or project lease override.

The [2D Studio](../../Tools/TwoD/README.md) uses this document through the existing EditorImGuiHost.
A main-project host must retain its existing access, recovery, lease and generation checks before
import/save publication. This isolated tool does not complete the main graphical Editor milestones.
Editor optionally depends on TwoD; Runtime/Shipping depend on neither document nor GUI.

### Owning isolated 2D Play context

`TwoDPlayContext::Create` captures a `TwoDDocument` into a distinct Play World, activates its scene and owns its
Sprite/collider metadata, immutable copied Atlas/Clip, copied Tilemap (including unloaded chunks),
fresh InstancePool and canonical GameplayBridge. Source snapshots are capped at 64 MiB; the
pool budget is 1..4096, default 4096. Creation fails atomically without changing source bytes,
Dirty, read-only mode or Undo/Redo. It may inspect read-only documents. No document reference,
source span, filesystem path, project lease or native renderer is retained.

The context is a noncopyable/nonmovable owner-thread object. Its World may be mutated only in
serialized gameplay/owner phases. `GetApi` reuses the canonical size/version, copied output,
wrong-thread and C exception boundaries. Clip lookup matches the full UUID against the frozen
clip; authoring replacement, Undo, document destruction and rendering never change its clock or
assets. Tile/pool changes remain in this session. Pool handles from another context reject.
Removing World entities requires `ForgetEntity` for every destroyed descendant before further
use. The caller must unload all modules and finish callbacks **before destroying the context**;
copied C tables are borrowed and must not be invoked afterward. Stop/Destroy can still use the
services. Context destruction then releases bridge, pool/tiles/assets/metadata before World.
This owner does not load, reload or automatically stop a gameplay module.

`ExtractFrame` collects scene poses, transient instances and loaded visible tiles into one
globally sorted frame, with distinct draw-only tie identities and a shared 100,000-instance
limit. Scene ties preserve existing entity-ID order even when storage order differs; equally
sorted pool inputs follow scene inputs, then tiles, with stable order within each namespace.
Invalid extent/texture, overflow, allocation or extraction failure returns unavailable.
It performs no IO, tick/animation advancement or GPU work. Unknown Atlas/region references use
the existing unavailable accounting. The CPU triangle snapshot remains available for inspection;
`SpriteFrame::Draw` uses the current native 44-byte GPU instance path. Frames retain immutable
Atlas owners after context/source
destruction. The presentation host supplies a nonzero fresh surface-local texture ID for each
Atlas generation, excluding the two reserved largest IDs, and retires native textures after work
completes. A new context can contain changed pixels under the same asset UUID and needs a new ID.

`twod.play_context` exercises 32 actual V3 module lifetimes, source replacement/history,
failed Start, stale handles and copied extraction. `twod.play_context_native` uses X11/Vulkan
pixels for frozen red, animated green, retained green after owner destruction and new-session
blue, with exact native instanced draw/instance/upload-byte counters. These are prerequisite
ownership/renderer contracts; main project Editor routing,
integrated Game UI/preview, full hot reload and physical acceptance remain open.
Project-scoped cooked asset capture is supplied by the separate owner described below. [Evidence](../../Tools/Build/evidence/EditorEDM3-TwoDPlayContext-Linux-2026-10-15.md).


### Frozen cooked 2D Play assets

`TwoDPlayAssets::Capture` owns an immutable CPU Atlas/Clip dependency closure from an explicit,
nonzero `AssetGenerationStore` generation. Clip roots automatically include their Atlas. Duplicate
or zero roots, missing assets, unsupported types, mismatched payload UUIDs, noncanonical dependency
lists and missing frame regions reject the complete candidate. Runtime's store supplies already
hash-verified cooked envelopes; capture additionally checks the schema and 2D codecs. It neither
falls back to the active generation nor reads source files.

The caller serializes capture with every mutation of the borrowed store. Capture temporarily pins
the selected generation and balances each successful Load with Release, including early failure
and C++ exception unwinding. Existing external pins/resident references survive. Successful results
retain no store borrow or pin: decoded immutable payloads survive collection, rollback, store
destruction and later same-UUID asset replacement. Returned shared owners can outlive the snapshot
and serve the existing ClipResolver; surface texture allocation remains the rendering host's job.

At most 4096 unique assets and 64 MiB of combined cooked payload bytes are admitted, including
dependencies. Callers may reduce either bound. Empty closures are valid for an existing generation;
zero budgets and attempts to raise the maximum reject. PayloadBytes counts unique serialized payload
lengths, not allocator capacity, RSS or VRAM. Allocation exceptions propagate after temporary store
ownership unwinds; validation failures return null and an optional diagnostic. No scene, source,
authoring history, native GPU object or existing snapshot is mutated.

This is a cooked-generation ownership prerequisite. The caller must establish project UUID/root,
current Content/source revision, lease/access/recovery and package provenance before using it in
main Editor Play. This API grants no project authority and does not attach `TwoDPlayContext` to the
main Play World, Game UI or reload controls. `twod.play_assets` covers the real importer/cook/bundle/
store path, 4096/4097-asset boundary, exact payload budget, failures with external references,
generation replacement/collection and canonical gameplay consumption after store destruction.

### Typed project 2D source import

✅ With `NEXORA_ENABLE_2D`, indexing and synchronous/queued reimport now carry an owning
immutable `TwoDAssetPayload` through `AssetEntry`, `ContentItem` and `ReimportResult`.
`ImportTwoDAsset` borrows normalized `.nx2atlas`/`.nx2clip` bytes only during the call and uses
the existing TwoD codecs. It performs no IO, GPU work, publication or dependent-file reads.
Payload construction is private and noncopyable; consumers retain shared const owners and can
borrow immutable Atlas/Clip owners independently. An optional expected UUID must match exactly;
zero permits initial embedded-identity discovery. Cancellation is checked before/after bounded
decode, and the source reader also checks between binary chunks. Failed/cancelled reads expose
no typed payload or partial hashes. Allocation/callback exceptions follow the existing C++ boundary.

New valid self-identifying 2D sources use their embedded UUID when creating the first sidecar,
including uppercase filename extensions. `DerivedFromPath` compatibility mode likewise uses the
embedded identity for these optional types, without writing metadata; other types retain derived
identities. Duplicate embedded IDs or a malformed new identity abort indexing and preserve the
previous index. As with the existing sequential sidecar writer, valid earlier sidecars may already
have been created; this is not a multi-file filesystem rollback. The invalid/colliding new sidecar
is not created. Read-only indexing never creates missing metadata. Existing sidecar identities and
importer types remain authoritative across rename; source mismatch/invalid data becomes a failed
entry with no payload/hash. Older generic sidecars whose UUID disagrees are diagnosed, never
silently rewritten or migrated. Source bytes and references are not rewritten.

Each source is capped at 64 MiB before allocation when size is available and during chunk reads.
An indexed typed set and each live/retained Undo Content snapshot admit at most 4096 payloads and
64 MiB of capacity accounting: payload/Atlas/Clip objects, vector capacities and event string
capacities plus terminators. Inline string space is conservatively counted again; shared control
blocks/allocator overhead are excluded. This is not RSS/VRAM or a bound on separately retained
external snapshots. Valid identities beyond the sorted import budget remain failed indexed rows.
Content Reset, Discover and PublishArtifact enforce matching typed UUID/importer and the same
limits. Publication also checks the retained Undo set before changing revision/hash/pointers;
successful reimport updates its payload so path Undo retains the newest admitted version.

Clip Atlas UUID dependencies enter the initial project graph and replace previous dependencies
atomically on successful synchronous/queued reimport. Workers only stage; authoring publication
retains current project/path/artifact/source-size/time/settings/dependency checks, and cancellation,
staleness, invalid input, cycles or budget failure preserve the prior model/graph. Atlas existence,
type and frame-region closure are resolved separately by cooked Play capture; source import does
not load dependencies or claim complete project provenance. Models serialize on the authoring
thread and immutable owners survive model/index destruction. With 2D off, these file types keep
generic streaming/index behavior and no TwoD implementation dependency is introduced.

Public C++ consumers rebuild for appended payload fields and PublishArtifact's new optional
parameter. Existing source/wire/C/Zig codecs, generic/Mesh/Material behavior, project lease/access
checks and optional module dependencies remain unchanged. Main Editor Play/Game routing,
integrated preview/reload remain open; scoped cook/freeze admission is described below. See
[typed source evidence](../../Tools/Build/evidence/EditorEDM1-TypedTwoDContent-Linux-2026-10-16.md).

### Project-scoped cooked 2D Play capture

`ProjectTwoDPlayAssets::Capture` connects admitted project Content to the existing Runtime
importer/cooker/bundle/generation-store path and `TwoDPlayAssets`. It requires explicit host
authorization, an open writer workspace and writable Content session at the same canonical root,
a nonzero matching expected project generation, a nonzero cooked generation and nonempty unique
roots. The current on-disk project descriptor must agree with the workspace UUID/name/schema.
Recovery, external workspace changes, pending reimport and unresolved Content conflicts reject.
Both Pending and Compare remain unresolved, including conflicts outside the selected closure.
The host still supplies current stopped-Play/modal/document policy; this API does not grant it.
Workspace ownership retains its existing writer lease; capture acquires no new lease.

Only the selected Atlas/Clip closure is cooked. Each source must have a persistent matching
schema-1 UUID/importer sidecar, a ready typed immutable Content owner, a matching artifact and
exact source bytes equal to that owner's canonical encoding. Clip dependencies must agree with
the current graph; cooked capture additionally checks Atlas type and frame-region closure.
Authoritative sidecar importer types survive filename-extension changes. Paths stay below Content;
unsafe traversal, symlink components/leaves, nonregular files, hard links and oversized reads reject.
Reads are bounded even if a file grows. No source, sidecar, cache file, Content/history, shared
Runtime store, World or GPU object is modified. Cooking uses a private in-memory cache/store.

Calls serialize with all project/Content owners on the authoring thread. The immutable result
owns project UUID/root, Content generation/revision, exact source/sidecar baselines and the frozen
cooked closure; no workspace/Content/store/file borrow survives. `Matches` repeats current scope,
model/graph and exact file checks immediately before activation. Any Content revision change
invalidates an observation, including unrelated authoring; graph changes are checked separately.
A restored exact file/graph baseline can match again if Content revision is unchanged. Permission
checks are observations, not durable authority or a transaction against hostile concurrent filesystem
replacement. Allocation exceptions propagate and ordinary validation failures return null/false
with an optional diagnostic. Failed candidates never replace a caller's retained snapshot.

Limits may only lower 4096 unique assets and 64 MiB of combined source lengths and, separately, cooked payload lengths.
Exact source baselines retain at most 64 MiB and sidecars at most 4096 bytes per admitted asset;
frozen decoded payloads are separately owned. These are logical lengths, not vector capacity,
allocator/RSS/VRAM or an all-external-snapshot bound. Temporary cooking holds additional copies.
The cooked generation is an explicit host label scoped by project identity/generation, not a
reservation in a global store. After successful activation, reimport/project shutdown cannot
change retained pixels/Clips; Stop/reload policy and native texture retirement remain host duties.

`twod.project_play_assets` covers actual filesystem/sidecars and Runtime cooking, source and
scope staleness, dependency/region rejection, exact byte/count budgets, alias rejection where the
platform permits creating fixtures, pending publication and canonical gameplay consumption after
all project/cook owners die. The main Editor now consumes this capture at Play start; see the
[host contract](../../Apps/Editor/README.md#optional-2d-services-in-project-play). This capture API itself does not attach the API,
Play World, Game View or reload controls and accepts no enclosing milestone. The main application's
[bounded 2D Game View](../../Apps/Editor/README.md#bounded-2d-game-view) now consumes its frozen data
without adding GUI or native resource ownership to this capture API.

## External Chrome duration import

`ChromeTraceImporter` owns intervals from Chrome `X` complete events, synchronous `B/E` scopes,
nestable async `b/e` and legacy async `S/F` duration events, selected by exact
process/begin-thread/event name. Timestamps keep the external microsecond clock; durations are milliseconds. The importer infers no CPU/GPU/memory metric.
Wrapped `traceEvents` objects and bare arrays retain the existing 4 MiB input, 32,768-event,
600-sample, Unicode/JSON and metadata bounds.

On the selected thread, every B requires a name and timestamp; every E requires a timestamp
and closes the most recent B. An optional end name must match. Nonmatching event names still
participate in pairing so a nested scope cannot close its parent accidentally. At most 128
synchronous plus async scopes can be open; B/E timestamps must be nondecreasing and all scopes must close. Unrelated
threads do not share the stack. Valid zero durations remain zero. Legacy step `T/p`, instant
`n` and other phases remain validated/skipped metadata and are not paired. Recognized B/E records on the selected thread,
previously skipped, now reject the whole import if malformed or unbalanced.

Nestable async `b/e` events require name, process, thread and timestamp, with no `dur`.
Identity is exactly one legacy `id` or an `id2` object containing exactly one `local` or
`global` key. Legacy `id` and `id2.global` share a global namespace and may pair across
processes/threads. `id2.local` belongs to the selected process; unrelated process-local values
remain opaque validated metadata and never participate. Local and global namespaces stay
separate even when their ID/category/scope text matches. Each namespace uses ID type/value
plus `cat` and `scope`; omitted category/scope are empty. Numeric IDs are exact nonnegative
integers through 2^53-1; string IDs are nonempty decoded UTF-8 up to 256 bytes. Numeric 1 and
string "1" are separate. Category/scope must be strings of at most 256 decoded bytes for
participating intervals. Mixed id/id2, empty/ambiguous/unknown id2 members reject.

Only begins on the selected process/thread with the exact selected name produce samples.
All global b/e on all processes and all local b/e on the selected process participate,
including nonselected names/threads/processes, so nesting cannot close a selected parent
accidentally. Each identity is LIFO with matching end names and nondecreasing timestamps
while open; independent IDs can finish in any order. All participating scopes must close,
and the shared 128-open budget includes synchronous scopes. Malformed recognized global
b/e on any process or participating local b/e reject the whole import. Previously skipped
global events on other processes now participate and must pair; ordinary unrelated local
metadata remains skipped.
Legacy async `S/F` uses the same bounded ID/category/scope lanes and begin selection, in a
separate pairing domain from `b/e`. A legacy identity additionally includes the exact event name;
different names may overlap on one ID, but a second live S with the same complete identity rejects
rather than nesting. F must close the matching live S with a nondecreasing timestamp. Completed
identities may be reused. Global S/F may cross processes/threads; process-local S/F only participates
on the selected process. Unclosed or malformed participating S/F now rejects the whole import;
these phases were previously skipped. Legacy steps T/p remain opaque metadata and produce no
intervals. Legacy scopes share the existing 128-open budget with synchronous and nestable scopes.
The supported subset retains typed IDs and category separation; it does not reconstruct legacy
step sub-slices or permissively repair broken traces.

Async durations remain inclusive elapsed latency; their self elapsed time is unavailable.
No CPU execution cost or reconstructed call tree is inferred.

Selected begins reserve their sample sequence and position immediately. End events complete
that owning interval only if the ring slot still holds that sequence; an evicted begin cannot
overwrite a newer sample. Newest retention and loss counts follow begin/start order, including
mixed X/B/E/b/e/S/F records and nested same-name scopes. Pending intervals never escape a failed import.
`duration_milliseconds` remains inclusive elapsed duration. The additive optional
`self_duration_milliseconds` is external elapsed time excluding observed synchronous children.

### Synchronous self elapsed inspection

Successful imports retain bounded selected-thread X/B/E context, including nonmatching names.
An iterative sorted reconstruction subtracts each immediate child's raw inclusive duration once;
grandchildren are not double-counted. It compares relative starts and raw durations instead of
adding tiny durations to large absolute clocks. This preserves sub-clock-ULP durations and rejects
false containment caused by rounded absolute endpoints. Known B/E begin order resolves coincident
paired bounds. Crossing or otherwise ambiguous coincident components, including unresolvable
coverage/precision, keep their inclusive values and leave self unavailable; disjoint valid
components remain available. Zero duration can have available zero self time. Async b/e/S/F never
claims self time and does not subtract from synchronous scopes.

Context sorting never reorders selected samples. Completion checks retained sequence identity
before writing a self value, including equal-start context sorted opposite to begin retention.
At most one fixed-size context record per selected-thread X/completed E and one iterative stack
slot per context are retained during parsing; the existing32768-event/4MiB limits bound this extra
O(events) memory. Reconstruction is O(events log events), without recursive ancestry traversal.
Temporary context is released with the importer. Capture history retains only the existing newest600
owning samples. These are logical bounds, not total allocator/RSS/GPU or frame-budget acceptance.

Validate rejects nonfinite/negative self or values above inclusive duration; unavailable remains
valid. Old supported input acceptance, UTF-8/JSON limits, source/read-only policy and inclusive
semantics are unchanged. C++ callers using three-field interval initialization retain source
compatibility with unavailable self; all binary C++ consumers must rebuild for the appended
optional field. Persisted files, stable C ABI, other capture schemas and linkage dependencies stay
unchanged. This measures external interval coverage, not CPU execution, scheduling or GPU time.

`ProjectWorkspace::ImportChromeTraceJson` supplies the same project-scoped read-only-safe file
admission and actual byte bounds. Failure does not publish or alter source files, prior captures
or live measurements. Persisted Editor schemas, C ABI and product linkage are unchanged.

## Build artifact byte verification

`BuildFrontend::VerifyArtifacts` synchronously verifies caller-captured owning
`BuildArtifactInput` values against an exact nonempty schema-one manifest set. It
requires canonical `sha256:` plus 64 lowercase hexadecimal digits, matching relative
path identities and exact byte counts. A missing/unavailable SHA-256 backend never
accepts. Legacy `Validate`/`Write` retain their serialized manifest compatibility;
writing a manifest alone does not verify its artifacts.

Limits are 64 artifacts, 16 MiB each, 64 MiB total, 1024 UTF-8 bytes per path, 256
bytes each for profile name/target, 128 for configuration and 32 KiB for command text.
Empty/ASCII-control-containing/invalid UTF-8 profile text and unsafe relative paths reject.
All shape and byte budgets are checked before hashing. Inputs and manifest remain
unchanged, and no file IO, publication, native process or permission grant occurs.
Calls serialize with caller-owned captures; results certify only those supplied bytes
at that call, without retained borrows. The caller owns consistent filesystem capture,
expected target completeness, process outcome, reproducible argv/toolchain/features
and future publication/deployment. A command string is descriptive and never executed.
No build-success UI is introduced. Public C++ consumers rebuild; module graph,
existing wire schema and stable C/Gameplay ABI are unchanged.

## Native tool byte callback SDK

The optional public C `EditorToolAbi.h` defines a separately versioned service table
for Inspect/Edit/Preview/Serialize byte transformations. It preserves required engine
plugin ABI and lifecycle schemas. A service registration supplies a readable immutable
table for its admission; native plugins remain trusted and must obey capacities,
call-scoped borrows, cooperative draining and the no-throw contract.

`NativeToolInvoker` performs a fresh qualified `PluginHost::FindService` lookup for
every synchronous owner-thread call. It retains no table, function or native context.
Wrong-thread/reentrant calls (including another invoker on the same owner thread),
malformed prefixes/interface/masks/budgets, revoked or
foreign providers and undeclared operations reject before invocation. The input is
read-only and at most 64 KiB; output storage is host-owned and capped at 64 KiB, also
respecting each table's smaller declared limits. Complete shape validation precedes
allocation/callback work. Future table suffixes are ignored, with a 4096-byte declared
prefix limit. Empty input/output are supported; declared output capacity must be nonzero.

Results own copied bytes, state, diagnostics and optional exact raw callback status;
successful output must explicitly report a size within the supplied capacity. Every failure discards output,
and unexpected exceptions reject without escaping the wrapper or retaining its busy
guard. Owning results survive native unload. Unknown statuses/oversized success never
become success. Consumers release all calls/borrows before host/registry mutation or
unload; this provides no lifetime lease or automatic in-flight tracking.

The byte SDK exposes no SceneDocument, workspace or renderer objects and performs no
file IO, history mutation or publication. Declared byte operations confer no actual
document/IO/GPU permissions. The host separately authorizes native execution, validates
the operation's document type, parses owning results and rechecks current scope/source/
writer/recovery/Play state before actual deferred authoring or IO. Capability metadata,
graphical reference-tool editing/preview/save and missing-backend restoration remain
separate work. Public C++ consumers rebuild; module graph and persisted formats stay
unchanged, and Foundation's optional C header adds no Shipping Editor data.

## Stable revisioned prefab asset foundation

`PrefabAssets` owns exact versioned SceneDocument bytes, an asset UUID/revision, stable node UUIDs
and stable field UUIDs, optional exact base revision and scoped nested instance references. Capture
uses an authoring-thread identity factory only during the call; it copies and validates the scene
before calling that factory. Nil/duplicate identifiers reject. Matching serialized node IDs and
canonical field keys preserve identities across rename, value changes and actual save/reopen. The
caller supplies the previous revision of that same authoring source (or a variant of it), not an
unrelated document with coincidentally matching Runtime IDs. A same-asset capture increments without
wrap; a new-asset variant starts at revision one and references the exact previous asset revision.
It retains a full materialized source snapshot; this foundation does not automatically rebase it.

The canonical `NXPFAB1` binary format uses fixed little-endian integers, exact EOF and validated
counts before iteration. It retains exact unknown opaque bytes and authored Scene metadata. Current
field identity covers name, parent, layer, Transform lanes, whole Camera/Light/Mesh components and
whole opaque components; it does not interpret arbitrary native plugin layouts. Limits are 8 MiB
scene bytes, 16 MiB asset bytes, 4096 nodes, 64 fields/node and 32768 fields/asset. Decoder errors
return no partial asset and never mutate live documents or their history.

Resolve stages an owning unique-asset revision archive, instance scopes and expanded node count.
Each input asset UUID/revision pair identifies one exact source; duplicate pairs reject. Distinct
revisions of the same UUID may coexist in one closure and retain their separate exact references.
Missing/stale sources, duplicate identities, invalid attachments, base/nested cycles or budgets
reject the whole result. Base dependency closure does not become a second live instance; repeated
nested placements have separate instance scopes. Shared base closure is memoized to avoid exponential
traversal, while each actual nested placement consumes its budget. Limits are 64 supplied sources,
64 MiB total encoded input/archive, 128 expanded instances, 4096 expanded nodes and depth 32.
Returned values retain no document, World, caller-source or callback borrows.

Load/Publish use the canonical project path `.nexora/prefabs/<asset-uuid>.nxprefab`. Actual reads
require ordinary nonsymlink, single-link files and remain bounded if the file grows after preflight.
Both operations require resolved workspace/recovery state; only publication requires the writer
lease. Initial publication preserves an occupied identity. Replacement requires the exact canonical
previous bytes, matching UUID and next revision, then uses the common atomic staging helper. Invalid
or externally changed sources and occupied foreign staging files remain untouched. This is the
existing serialized single-writer contract, not exclusion against hostile filesystem races or a
multi-file crash/power-loss journal. No source scene, saved baseline, clipboard or Undo entry changes.
Full graphical isolation, persistent instance metadata, graphical instantiate/override diff/revert/
apply/rebase remain separate work; public C++ consumers rebuild, and stable
C/Gameplay ABI, Runtime prefab compatibility and module dependencies remain unchanged.

`PrefabAssets::SaveDocument` integrates wrapped prefab publication with the live authoring document.
It captures owning previous-asset and document baselines plus project scope before identity callbacks;
current document/project state is rechecked before any publish. Initial saves and new variants use
missing-identity publication, while same-asset updates require the exact previous revision. A new
variant additionally requires its exact base revision to remain the current stored source. An
unchanged confirmed source retains its current revision and does not clear pending Redo.
Only confirmed publication advances the document's saved signature/opaque baseline, moving already
owned preparation strings without Reload or new history allocation. Generation, selection,
clipboard and all existing Undo/Redo stay intact. Failure leaves the old saved baseline; any actual
publication followed by a late source/scope change retains the file but does not acknowledge the
changed document. The serialized host handles that explicit error. This is an authoring save helper,
not a graphical isolation session, nested materializer or multi-file crash journal.

Before replacing the current wrapped source, Publish retains its exact committed previous bytes at
`.nexora/prefabs/revisions/<asset-uuid>/<revision>.nxprefab`. An occupied archive entry must already
equal those bytes; mismatching, corrupt, aliased, multiply linked and foreign staging paths reject
without advancing the current source. Current source bytes and workspace authority are rechecked
after retention. If the final current-file publication fails, the retained old committed revision
is harmless and no uncommitted future version occupies the next revision identity. An unchanged
retained entry requires no replacement write. Initial publication still writes only the current file.

`LoadRevision` returns owning validated archived data or, only when the archive entry/ordinary
parents are absent, the matching current revision. Existing invalid archive evidence never falls
back to a current source. `ResolveProject` gathers at most 64 exact sources / 64 MiB encoded bytes,
then performs the same cycle/depth/instance/node checks as Resolve; inspection-only read-only scopes
work. These are logical data budgets, not a bound on total process memory. History directories are
ordinary nonsymlink directories; each file keeps the existing 16 MiB and single-link read bounds.
Calls serialize with all project publication and owners; returned graphs borrow no files/documents.

History is retained when a committed revision is superseded, so previously overwritten legacy
revisions cannot be reconstructed. No automatic pruning or total disk quota is introduced: retained
sources may still be referenced by variants/nested graphs. The archive/current writes form an
ordered single-writer protocol, not a multi-file power-loss durability guarantee. Graphical
override/rebase, persistent instance metadata, journaling and retention policy remain separate work.

## Transactional nested prefab materialization

`PrefabAssets::Instantiate` resolves the complete exact revision closure, builds an independent
staging World/document, then imports that complete forest once against a caller-owned current
PreparedSave. Base dependencies are validation sources, not duplicate live instances. Every nested
placement keeps its independent instance scope; stable node UUIDs map to fresh live generation keys.
Nested roots attach to the corresponding stable parent node while retaining authored local TRS,
Euler metadata, names, sibling order, components and unavailable opaque bytes.

All result vectors, scope vectors/identities and translation indices are allocated before live
publication. The final import retains its atomic Undo/Redo, clipboard/cut and stale-observation
contracts. No filesystem IO or source asset publication occurs. Callers serialize source/target
owners and supply current authoring authority; project writer/recovery/Play policy belongs to the
host. Returned mappings own their data but their target keys still require document-generation
validation. Existing graph budgets apply; the combined serialized forest also fits 8 MiB.
The staging algorithm's temporary Worlds/history are not a total process-memory budget. Persistent
instance metadata, graphical instancing and override/revert/apply/rebase remain separate work.

## Stable prefab revision comparison

`BuildPrefabRebasePlan` compares exact old-base/local/new-base assets and coalesces rows by stable
node/field UUID into complete property groups. Source-only groups update; local-only overrides
survive. Both sides changing different lanes of the same vector/component still require an explicit
KeepLocal/TakeSource decision. Missing choices return an owning unresolved review without a
candidate; duplicate, unknown or invalid choices reject. Exact node and field identity sets must
match. Structural/identity, nested-reference and changed scene-metadata reconciliation remain open.
Chosen parent/sibling groups pass the actual candidate parser and hierarchy validator together.

`PrefabDocumentSession::ReviewRebase` requires a saved isolated variant and reads its exact retained
current reference plus the newer actual current source. Its private owning result freezes document,
publication, project/owner scope, all three assets and both complete bounded source closures.
`ResolveRebase` and `Rebase` reread actual publication, retained/current sources and closure assets;
stale or tampered dependencies reject. Read-only scopes may inspect and resolve choices, but only
a current writer with resolved recovery/external state and explicit authorization may apply.
Application stages the complete candidate graph, then changes properties and reference in one
atomic Undo/Redo without writing files or acknowledging the baseline. Explicit wrapped Save persists
the new reference and preserves history; local keys, generation, selection, clipboard and original
scene remain intact. Calls serialize on the authoring owner. Logical bounds are not a total
process/history memory quota. Public C++ consumers rebuild; stable C/Gameplay ABI is unchanged.
Graphical review/conflict confirmation and structural/live-instance bindings remain separate work.

An isolated variant document now owns its current exact base UUID/revision independently of
its last published asset. `SceneDocument::ApplyPrefabPropertySnapshot` stages same-identity
properties and that reference in one Undo/Redo transaction; a reference-only change is dirty even
when reusable scene bytes are unchanged. Ordinary property transactions retain the reference.
Prepared-save signatures include it, so changed references invalidate pending reviews and saves.
`PrefabDocumentSession::BaseReference` returns an owning current observation; the published
`SourceBaseline` changes only after explicit wrapped Save. Review follows the current undoable
reference, including after Undo/Redo of a saved reference change.

Wrapped Save validates the retained source closure and complete candidate graph, then increments
the asset revision for changed reference metadata even if scene bytes are identical. Exact old
wrapped revisions remain archived. Open initializes the reference as part of a clean saved baseline;
read-only inspection stays available. Plain scene Save and Save All reject tagged documents before
publication because their format does not persist this asset context. A batch prepared before a
reference change rejects its stale signature. No scene format or stable C/Gameplay ABI changes;
public C++ consumers rebuild. Owner calls remain serialized, and the host supplies project/source
authority. Three-way rebase planning, graphical conflict choices and structural reconciliation are
separate work.

`ComparePrefabRevisions` returns an owning read-only three-way `SceneComparison`, aligned by stable
node and field UUIDs. A null source means absent; invalid present source/identity metadata rejects.
Official scene parsing and bounded field snapshots retain component presence/stored values,
authored Euler, hierarchy/sibling order and exact unknown payload hex. Parent values use node UUIDs
instead of serialized Runtime IDs. Base/nested references, attachment UUIDs and nested ordering
are also compared; the containing asset's own UUID/revision are provenance, not authored diff rows.
Renumbering Runtime IDs with correctly remapped stable metadata creates no false differences.

The existing SceneComparison limits apply: 8 MiB scene input, 4096 nodes, 64 KiB display value,
4 MiB stable snapshot, 131072 rows and 16 MiB result. Expanded stable paths consume those budgets;
opaque payloads must fit the existing hex display bound. Unsafe display control characters reject.
Sources and authoring owners serialize during capture; results borrow no asset/document/World.
The comparison performs no file IO, interpretation of unavailable native components, live mutation,
Undo/baseline change or merged publication. Choice/conflict rows are inspection hints and require
separate structural validation before any future revert/apply/rebase. Graphical bindings and
transactional override publication remain separate work; public C++ consumers rebuild.

## Atomic document property snapshots

`SceneDocument::ApplyPropertySnapshot` parses an owning complete authoring source, checks the
current PreparedSave and requires the exact existing entity/node ID set, scene name and persistence.
It stages names, authored Euler and unavailable opaque data while preserving current generation
keys. The existing Runtime atomic snapshot replacement applies hierarchy/order/TRS and complete
component presence/stored values. It performs no IO, creation/deletion or saved-baseline change.
Unchanged rotation groups retain an existing in-memory Euler hint when both the exact Runtime
quaternion and canonical Euler records are unchanged. This includes latent authored turns whose
saved quaternion currently differs from Runtime and is therefore omitted by PrepareSave. Explicit
quaternion changes or Euler-record insertion/removal use the candidate metadata. Complete history
retains the resulting hints; a no-op preserves Redo. This does not persist a latent hint while its
quaternion differs from Runtime or change the scene codec.
Selection and user clipboard/cut state remain intact. Callers serialize owners and enforce current
project writer, recovery and Play authority; false authorization rejects before mutation.

All document/history storage is prepared before publication. A dedicated immutable owning history
entry replays the complete Runtime and metadata state as one Undo/Redo, checking the expected current
document each time. Rejected replay leaves the cursor/content in place. Ordinary Runtime/rename/
opaque operations retain their independent history cursor. Equivalent snapshots preserve pending
Redo. Limits are 4096 fully tracked nodes, 8 MiB source/prepared/runtime bytes and 1024-byte UTF-8
NUL-free names. These are logical per-transaction bounds, not a total history memory quota.
Successful replacement expires entity and node-name borrows; document pointers and generation
keys remain valid. Structural additions/deletions and prefab field selection/rebase are separate
work. Public C++ consumers rebuild; stable C/Gameplay ABI and module graph are unchanged.

## Identity-remapped prefab property candidates

`BuildPrefabPropertySnapshot` validates both wrapped sources, requires identical stable node UUID
sets and returns an owning complete authoring candidate. Official parsing normalizes supported
schemas first. Every entity/parent, node, authored Euler and opaque owner ID is remapped to the
current serialized identity. Source names, sibling order, TRS and complete stored component/unknown
data survive; the target scene name/persistence are retained. Source base/nested reference metadata
is not a document property and is not changed by this helper.

The complete output is parsed again before return. Sources must be fully tracked and fit the
existing prefab 4096-node / 8 MiB scene bounds, including normalized/remapped output. Results borrow
no files, assets, Worlds or documents and carry no publication authority. The serialized owner
checks its current PreparedSave and project writer/recovery/Play policy before applying a candidate
through the atomic property transaction. Planning performs no IO, live changes, baseline or history
mutation. Structural shape mismatch rejects rather than producing a partial revert. Graphical
revert, targeted field selection, reference rebase and source publication remain separate work.
Public C++ consumers rebuild; stable C/Gameplay ABI and module graph are unchanged.

Shader authoring and diagnostics remain an Editor/tool responsibility above Runtime and RHI.
The UI-independent `ShaderCompileResult` carries file/line/column/severity/backend/variant
diagnostics. `CompileSlang`
invokes the configured `slangc` process (or an injected runner), captures diagnostics, and
validates the requested artifact payload. The default runner launches the compiler directly
(`posix_spawnp` on Linux/macOS, `CreateProcessW` on Windows) with an argument vector and never
through a shell, because request paths, include directories, and defines come from project content;
iOS and Android have no default runner and require an injected one. A launch failure or a non-zero
exit without a parseable error is reported as an error diagnostic rather than silently dropped.
`ParseShaderDiagnostics` understands both the native Slang 2026 layout (`error[E30015]: ...`
followed by ` --> file:line:col`) and the single-line `file:line:col: severity: message` form.
Severity comes only from the explicit severity token, echoed source and gutter lines are ignored,
and out-of-range line or column numbers degrade to 0 instead of throwing.
`ShaderHotReloadController` tracks each compile request (shader, target, variant, includes, and
defines) separately and fingerprints the source plus every declared dependency, so editing an
included file triggers a reload and one variant observing a source edit never suppresses another
variant's rebuild. `ApplyShaderCompileResult` publishes only successful, layout-compatible output through
Runtime's transactional slot. Reload commits may carry a GPU retire fence so old modules remain
alive until in-flight work has completed; a Shipping-configured Runtime rejects the dynamic path.
These integrations preserve the RHI artifact and canonical reflection contract rather than moving
compiler ownership into RHI.
`DevelopmentShaderCache` owns successful results keyed by source, target, profile, variant, entry
points, defines, include directories, declared dependencies, schema, and reflection layout, accounts against
an explicit (not globally fixed) variant budget, and invalidates all consumers when a recorded
source or include dependency changes. Callers should take a `CaptureShaderInputs` snapshot before
compiling and pass it to `Store`, so a save landing mid-compile leaves the entry stale instead of
being stamped as current; `Store` rejects (and does not budget) a result whose snapshot is already stale. Shipping admission remains exclusively in Runtime and never
consults this development cache.

`NexoraEditorCore` is the UI-independent authoring layer used by the standalone `NexoraEditor`
process. It owns project/workspace persistence, deterministic content indexing, stable panel and
command identities, hierarchy metadata, selection, clipboard operations, and scene-document
persistence. The production-support layer also owns explicit specialized-tool capability states,
reproducible build manifests, portable frame samples, virtual hierarchy ranges, signed-extension
policy, and opt-in telemetry state. It composes public Runtime Editor SDK APIs rather than reaching
into renderer or platform internals.

## Ownership and lifetime

Save All recovery retires a strictly empty ordinary journal without replacing any source or
inferring a publication phase. If final directory removal fails, cleanup best-effort restores its
bounded phase manifest for retry; occupied/invalid journals retain their entries. Initial metadata
write failure leaves original scene files and dirty baselines unchanged.

- `ProjectWorkspace` owns its descriptor, stable project UUID, open-document list, and (for
  read-write access) one OS-held writer lease on `.nexora/editor.lock`. A second writer fails with
  the owning process ID while any number of explicit read-only observers may coexist. The lock file
  is metadata, not the lease: the kernel releases the actual lock on normal close or process death.
  Schema-1 descriptors remain readable; a read-write open retains an exact descriptor backup and
  immutable plan report before atomically upgrading to schema 2 and persisting the derived UUID.
  A read-only open reports `Required` without changing the project. A read-write open of a schema-1 project that predates the `.nexora` directory creates it,
  but only for a root that already holds a project descriptor. All project-owned writes reject
  read-only workspaces. Workspace files are atomically replaced (the temporary file is flushed and
  checked before it replaces the old one), a recovery journal is written before the primary
  workspace file, and successful save/recovery removes that journal. The UI may query and explicitly discard a pending journal.
  Workspace, recovery, descriptor upgrade, gameplay settings, layout, profiler export, build
  manifests and recent project saves share the scene/asset atomic replacement helper. A preexisting
  sibling `.tmp`
  file, directory or symlink (including dangling links) is preserved and rejects the write.
  A failed replacement never deletes the destination to retry; Windows uses replace-existing
  `MoveFileExW`, while POSIX uses rename. Only staging created by this call is cleaned up after
  replacement failure. A failed primary workspace publish preserves its committed file and
  in-memory documents, retaining the newly written recovery journal for explicit recovery.
  After the occupied stage is resolved, a retry/recovery can publish normally. This is a
  single-writer replacement contract, not exclusive temporary creation against concurrent actors.
  Workspace and recovery records share a bounded line reader and the same public limits:
  4096 documents, each at most 1024 UTF-8 bytes. Save validates the complete input before writing
  a journal or primary stage, so it cannot publish a workspace its own reader rejects for size.
  CRLF and a final record without LF remain compatible; embedded NUL, overlong records and
  excess document counts reject without changing the current model or retained files/journal.
  Only a missing legacy workspace is treated as empty; unreadable/non-regular metadata and
  file symlinks are rejected. Successful recovery closes its bounded reader before publication.
  Recent-project admission validates the canonical root and name against its reader's 1024-byte
  UTF-8 record limits before changing the retained list or writing a stage. Unsupported project
  paths report an error and preserve the last-good store, instead of publishing an unreadable list.
  Versioned Editor layout payloads are persisted separately and never use Dear ImGui's unmanaged
  global ini file. Layout save/read share a 1 MiB raw-payload limit; schema-0/1 headers
  are bounded, embedded NUL and non-regular/aliased files reject with an error, and only a missing
  file returns no layout without error. CRLF payload normalization is linear. Invalid/oversized
  saves preserve the last-good layout and any unrelated staging path. Gameplay library selection
  is an independent `.nexora/gameplay-library.ini`
  schema-1 payload, capped below 1100 bytes on read. Empty selects inspection-only Play. UTF-8
  relative paths are limited to 1023 bytes; roots, traversal, backslashes, colons, control separators,
  unknown schemas, and extra records are rejected. Missing files return no setting; corrupt files
  return an error without mutation. Saves require the writer lease and use the existing atomic
  replacement path. A saved selection never loads executable code during project open; the Play
  owner separately validates canonical containment/existence when the user starts Play.
- `RecentProjectStore` owns user-level, schema-versioned recent-project state separately from the
  project. Entries are keyed by project UUID, deduplicated by UUID or canonical root, bounded to 12,
  and atomically replaced. The application chooses its storage path; read-only project access does
  not grant writes to project-owned files.
- The graphical project selector is a UI request source, not a project owner. `NexoraEditor` stages
  a candidate `ProjectWorkspace`, submits its content tree to `AssetImportQueue`, and activates the
  resulting `AssetWorkspace` and `ProjectContentSession` only after create/open, identity
  validation, background indexing, and content binding all succeed. Cancellation or failure leaves
  the selector active and releases the candidate writer lease.
- `AssetWorkspace` owns index entries. Pointers returned by `Find` and `Search` are borrowed until
  the next successful `ImportTree` call or destruction. The Editor executable uses
  `PersistentReadWrite`: every source asset has a sibling `<asset>.meta` with schema, UUID, and
  importer type. Existing sidecars are validated before publishing a replacement index; malformed,
  oversized, symlinked, or duplicate-UUID metadata fails without replacing the last good index.
  `PersistentReadOnly` never creates missing sidecars and rejects incomplete identity state.
  `DerivedFromPath` remains an explicitly non-persistent compatibility mode.
  One index admits at most `kMaximumIndexedAssets` (262,144) files and `kMaximumIndexedPathBytes`
  (1024) UTF-8 bytes per project-relative path (`AssetIndexLimits` overrides both). The bounds are
  checked while enumerating, before any sidecar is written; exceeding one fails with an actionable
  error and leaves the previous index and the project files unchanged.
  Triangulated `.obj` entries additionally retain immutable, shared owning CPU `MeshGeometry`
  snapshots (positions, normals, UVs, uint16 indices, and local bounds). `ImportObjMesh` is a
  synchronous parser with no I/O or publication; workspace jobs invoke it off the UI thread.
  Positive and relative negative OBJ indices are supported, missing normals become flat face
  normals, and material/group declarations never open referenced files. Unsupported records,
  non-triangular/degenerate faces, malformed indices, and nonfinite coordinates fail with a line
  diagnostic and no partial geometry. Other asset types retain their existing indexing behavior.
  OBJ reads and parser input are capped at 16 MiB, coordinate/expanded vertex records at 65,535,
  and indices at 1,048,576. Cancellation is checked during chunk reads and at each parser line.
  A workspace retains at most 128 MiB of CPU mesh vector capacity; assets exceeding the sorted
  import budget fail individually. Failed entries remain indexed with their UUID/path/error;
  background workspace jobs emit bounded `asset.import_failed` diagnostics. Typed OBJ reimport
  stages bounded geometry together with hashes; persistent per-asset GPU caching remains separate work. Old shared geometry snapshots survive index replacement/destruction.
- `ContentBrowserModel` owns its sorted item snapshot, breadcrumb and stable-ID selection state.
  Path keys, labels and rename input use UTF-8; filesystem operations retain native paths. Sorting,
  search, discovery, folder navigation and Undo never convert through a system code page.
  Virtual ranges borrow item pointers until the next mutation. Rename, multi-item move, and delete
  validate a complete replacement snapshot before committing and retain one undo snapshot.
  `ChildFolders` derives sorted, deduplicated immediate directories only from strict descendants
  of the current folder. Ancestor/sibling paths, component-prefix siblings and unrelated roots
  cannot manufacture `..` rows. The owning query result is independent of asset filters and
  preserves selection, revision and pending mutation Undo; explicit parent navigation remains
  available through breadcrumbs and Alt+Up in the graphical host.
  `ChildFolderSnapshot` returns an immutable shared-owned projection, lazily reused until successful
  Reset/Discover/path mutation/Undo or a folder change. Selection, filters, artifact publication,
  same-folder navigation and failed admission retain it. Copies may share immutable projections;
  invalidating one model does not invalidate another. Retained snapshots survive assignment and
  model destruction; no item/World/document borrow is cached. Call only on the authoring thread.
  `ChildFolders` retains its independent owning-vector API. Consumers must rebuild for the additive
  C++ API/layout change; persisted formats and the stable C ABI are unchanged.
- Source sorting stages one owning UTF-8 key per item and a source-to-destination permutation,
  then swaps complete `ContentItem` records with statically checked noexcept moves. Reset,
  discovery (including retained Undo) and authoring commits preserve the original byte comparator,
  payload owners, metadata and ordering. The temporary acceleration admits at most 262,144 items
  and 16 MiB of UTF-8 key bytes; these are payload/admission bounds, not total RSS. Over-budget or
  failed cache construction releases staging and takes the existing complete projection-sort path.
  No partial permutation is published. Ordinary fallback allocation behavior is unchanged.
  Owner-thread calls remain serialized; keys never persist across calls or become World borrows.
  The public model layout/SDK and persisted formats are unchanged.
- `NexoraContentDirectoryBenchmark before|after` builds a deterministic 100k-item/1000-folder
  metadata fixture and emits schema-1 JSONL for fixture/reset, first-root, 100 cold-folder and 200
  typed-search timings. The 13,050 checksum, complete results, source count, unchanged revision,
  empty selection and lack of new Undo history are checked. It opens no user project/files or GPU.
  Future incompatible record changes require a new schema; this producer does not import traces.
  Record machine/compiler/load with results; one timing pair is not a production percentile budget.
- `VisibleCount`, `Visible` and `SelectVisible` share one lazy immutable projection of owning item
  indices for the current folder and normalized query/type. Successful Reset/Discover/path edits/
  Undo and actual folder/filter changes revoke it; same-folder navigation, equal normalized filters,
  selection, artifact-only publication and rejected operations retain it. Reveal also revokes it when
  clearing active filters, even within the same folder. Model copies may share indices, never item
  pointers: range results resolve into each model's current item storage, including fresh artifact
  payloads. Returned item pointers keep their existing mutation-bounded borrow contract.
  Queries run serially on the authoring thread, like `ChildFolderSnapshot`; const does not grant
  concurrent access. The first admitted source query builds the directory index; cold projections
  scan current-folder membership with complete linear fallback and retain O(matches) active indices.
  Warm Count is O(1), while Visible visits only the bounded requested range. Range arithmetic
  clamps before addition, including zero counts and SIZE_MAX offsets/counts. Selection/history/
  source semantics stay unchanged. Other lookups, filesystem indexing, complete frame and
  production memory/GPU budgets remain separate. Public C++ consumers must rebuild for the
  private layout change; persisted formats, stable C ABI and module dependencies are unchanged.
- Cold directory queries share a lazy immutable membership/child index for one source publication.
  The first admitted query reads all item paths and unique ancestor prefixes; later folder/filter
  changes scan only the current folder's slots and copy only its immediate children. Exact native
  path/UTF-8 label/byte ordering and original name/type filtering remain intact. Source/order Reset,
  Discover, Commit and Undo invalidate the index explicitly; selection, folder/filter changes and
  artifact-only publication retain it. Copies share only immutable paths/indices; results resolve
  into each model's own items. Public child snapshots copy their children and do not pin the entire
  project index. No World/item-pointer borrow is retained.
  Admission limits are 262,144 source items, 65,536 directory nodes (including ancestors), parent
  depth64 and16 MiB path work (source UTF-8 bytes plus three times new-prefix UTF-8 bytes). These
  bound acceleration payload/work, not total RSS. Native-root/drive-relative special cases, budget
  failure and failed candidate allocation take the complete existing linear path; no source or
  navigation is omitted. The fallback decision is cached until source/order publication. Ordinary
  output allocation retains its existing behavior. Calls remain serialized on the authoring thread;
  observation data never grants file-write authority. C++ consumers must rebuild for the private
  model layout change and release all SDK owners before unloading its library. Formats, stable C
  ABI and module dependencies are unchanged; no index is persisted.
- `ContentBrowserModel::RevealAsset` is an inspection-only UUID operation: it stages the target's
  parent folder/breadcrumbs and sole selection before clearing query/type filters. Missing IDs or
  paths without a revealable relative parent preserve the complete view state. It does not touch
  content revision, item snapshots, pending mutation Undo, dependency state or files; read-only
  sessions may use it. The additive C++ method requires rebuilding consumers; persisted formats
  and stable C/Zig contracts are unchanged.
- `ContentBrowserModel::SelectVisible` atomically replaces selection with the complete matching
  current-folder query/type row set, including clipped rows. It consumes the cached matching indices and preserves
  revision, assets and mutation Undo. No matches clear selection. Batch Delete uses a UUID set to
  reject duplicates/missing IDs before publication and compacts surviving items in one scan;
  ProjectContentSession resolves the complete source/sidecar move list through a frame-local UUID
  index, preserving request order and the existing rollback/Undo transaction.
- `ContentBrowserModel::SelectVisibleRange` replaces selection with an inclusive current-folder
  query/type interval, in either direction. Both UUID endpoints must belong to that visible row
  set; hidden/missing endpoints preserve the previous selection. Linear scans build one owning
  replacement set without per-row Find/Select calls. Revision, asset data and mutation Undo are
  unchanged, and all borrowed row pointers remain local to the call.
- `ProjectContentSession` owns the live browser model, dependency/conflict state, canonical project
  root, project generation, and one recoverable filesystem mutation. The application owns the
  session; UI code borrows it for a frame and never retains `ContentItem` pointers. Rename and move
  use same-volume filesystem renames after validating a candidate model. Delete moves files into a
  unique project-local `.nexora/trash` operation directory, and undo restores both files and model.
  In persistent-identity mode, each source and its `.meta` sidecar are one rollback-capable
  transaction, so rename, move, delete, and undo cannot detach the UUID from the source. Existing
  files and symlinks outside the canonical project root are rejected before mutation.
  Rename to the identical UTF-8 filename validates writable access and existing source/sidecar,
  clears prior errors, and preserves the browser revision and pending Content Undo without IO writes.
- Typed asset drag payloads carry the project generation and asset UUID. Reimport results are staged
  and may publish only when their generation and dependency graph remain valid; cancellation,
  staleness, failure, or a cycle preserves the previous artifact. `ProjectContentSession` also keeps
  a newer reimport artifact in the pending undo snapshot, so undo cannot resurrect stale metadata.
- `AssetImportQueue` owns generation-tagged workspace-import and reimport jobs submitted to an
  application-owned `JobSystem`. Its worker state retains bounded progress and diagnostic histories
  (stable code, severity, message, path, and asset context); overflow is counted explicitly.
  Intake is also bounded to 64 retained operations by default. A four-argument constructor sets
  a custom operation capacity (zero normalizes to one), preserving the existing constructor.
  Queued, running, completed, failed and cancelled records all occupy a slot until `TakeResult`
  consumes them; cancellation alone does not free capacity. A full queue returns operation zero
  and a retryable error, without submitting a job or evicting another result. Admission and result
  consumption share the existing queue mutex; Start/TakeResult/Shutdown remain authoring-thread
  operations. This bounds retained operation count, not the total bytes of arbitrary project indexes.
  Workspace results and reimport artifacts remain staging data until the authoring thread calls
  `TakeResult` or `ProjectContentSession::PollReimport`. Reimport publication revalidates project
  generation, asset path, previous artifact, settings identity, source-file revision/size, and
  dependency revision before committing. The staging result itself carries deterministic
  source/settings hashes. The queue must be destroyed before its `JobSystem`; `Shutdown` stops intake,
  requests cancellation, and waits for every retained job. A content session with a pending reimport
  borrows its queue; destroy the session before the queue, including during exception unwinding.
  Cancellation requests do not release that borrow; PollReimport must consume the result, or the
  session must be destroyed before the queue.
  Workspace indexing and synchronous/background reimport share binary source staging. Ordinary
  assets are hashed incrementally through an 8 KiB read chunk instead of retaining the whole file;
  cancellation is checked between reads and after EOF. Source hashes retain the existing FNV-1a
  seed, and artifact hashes retain the UUID-string prefix, including empty and binary sources.
  Failed/cancelled reads expose no partial hash or geometry. OBJ parsing retains its 16 MiB source
  limit and early size preflight; workspace geometry retains its 128 MiB budget. Workers still only
  stage results, and the authoring thread revalidates before publishing to the live Content model.
- `MeshAssetCatalog` atomically publishes owning CPU geometry snapshots from imported assets on the
  authoring thread. Both UUID and resource lookup require the current nonzero project generation;
  failed imports and missing payloads remain unresolved. Retained snapshots survive replacement
  and unload. No scene mutation, I/O, or GPU residency occurs in this catalog.
  Saved mesh resource IDs derive from the persistent UUID: rotate the high half left by 23 bits,
  XOR the low half, then apply the fixed 64-bit avalanche constants in `MeshResourceId`; a zero
  result maps to 1 and a zero UUID is invalid. The golden ID test freezes this persistence contract.
  Changing it requires migrating saved references. Collisions (including duplicate UUIDs) reject
  the entire candidate publication and preserve the previous catalog; lookup also verifies UUID.
  Paths and source bytes never enter this derivation. GUI assignment is handled by the optional Inspector; the application submits resolved OBJ geometry through bounded native batches. Persistent GPU caching remains open.
- `SceneDocument::CreateCamera` / `CreateLight` validate a nonempty single-line name and live
  scene-owned parent before creating one initialized default component with identity local TRS.
  They return its stable ID and own one matching Runtime/Editor Undo step; Redo restores ID, name,
  parent and defaults. Root/child creation retains existing hierarchy and other entities. The caller
  owns generation/access checks and serializes authoring calls. The additive C++ APIs require
  rebuilding consumers; scene formats and stable C/Zig contracts remain unchanged.
- `SceneDocument` borrows its `World`, which must outlive the document. Entity selection and
  hierarchy use stable IDs, never component or container pointers. The hierarchy itself is the
  runtime's (`Entity::parent`, see the Runtime README's entity hierarchy section); the document keeps
  only node names. `Reparent` is an undoable runtime `SetParent` that keeps the world pose, and
  `Move` is a Hierarchy drag (reparent keeping the world pose, then place at a sibling index) as one
  undo step. `Nodes()` lists nodes in runtime sibling order. `Create`
  with a parent starts the node at the parent's origin (and fails before creating anything if that
  parent is no longer a live entity of the scene, e.g. after its creation was undone), and `Paste`
  places the root copy at the world pose captured by `CopySelection`, even if the source later moves
  or is deleted. Copy captures each selected root's complete subtree, Camera/Light/MeshRenderer
  data, child local poses, Euler hints and opaque payloads in owning storage. Selected descendants
  are copied once, and invalid selection leaves the prior clipboard intact. One Undo removes the
  complete pasted forest and restores the previous selection; Redo retains initialized payloads. Editor scene files still write a parent column in
  each node line, but from world snapshot version 3 on the snapshot is authoritative and that column
  is not validated, so a node whose runtime parent is not itself a node still reloads. A file whose world snapshot is version 1
  or 2 is migrated on `Reload` by applying the node-line parents with the world pose kept, so nothing
  moves; the migration is rehearsed on a scratch `World` first, so a failure leaves no scene loaded.
  After a runtime undo destroys an entity, `SceneDocument::Undo` drops its node metadata and
  selection before a later save, so the scene file cannot name an entity absent from the world
  snapshot.
  `DeleteSelection` validates the selected nodes before deleting, removes each selected subtree
  once, and drops its node metadata and selection immediately. Undo restores the subtree's stable
  IDs, names, Euler hints, opaque payloads and the complete selection as one atomic Undo step,
  including separate selected roots. Rejected deletion leaves Runtime/document history untouched.
  Redo replays Runtime transactions and restores the matching authoring metadata, including
  names, selection, and authored Euler revolutions. A new edit discards the redo branch; Reload
  clears both histories.
  `DuplicateSelection` captures and pastes the current selection while preserving the user's prior
  clipboard. Like Paste, the entire initialized forest is one Undo step.
  `WorldTransform` exposes a live node's composed world pose to Editor views, so children can be
  drawn at their actual world position without exposing mutable Runtime entity storage.
  `WorldPoses` owns a bulk current-scene observation, filters it to tracked document nodes in
  World storage order, and retains the effect of untracked ancestors. It uses no persistent cache
  or retained Runtime pointers, authors nothing and consumes no history. Owner-thread calls remain
  serialized; invalid Runtime hierarchy/affine composition rejects the complete observation.
  `WorldMatrix` returns an owning exact affine matrix for a live document node, including shear;
  missing or foreign nodes return no matrix.
  `TranslateSelectionXZ` and `TranslateSelection` validate generation-keyed targets, filter
  selected descendants, and apply world-space X/Z or X/Y/Z deltas through the portable gizmo
  math and one atomic transform Undo.
  `ApplySelectionGizmo` applies validated world-space rotate, translate, or scale operations to
  those same selection roots with one atomic transform Undo; the native Rotate tool uses it.
  `PreviewSelectionGizmo` returns owning prospective world poses for nodes and descendants using
  the same generation checks, selected-root filtering, and local edits as commit. Iterative cached
  ancestry composition retains exact matrix origins and the Runtime rotation/scale approximation,
  including rotated, mirrored, and nonuniform ancestors. `PreviewSelectionGizmoMatrices` returns
  owning exact affine matrices for those same prospective local edits. Neither snapshot changes
  selection, dirty state, or Undo/Redo history; invalid input returns no preview. The authoring thread consumes the snapshot for the current frame only.
  `SelectionGizmoFrame` uses the first selected root rotation with either that root origin or
  the mean selected-root origin. Selected descendants are excluded from the mean; an empty
  selection has no frame. Native handle placement and Center operations share this frame.
  `SetMeshRenderer`, `SetMeshRenderers` and the owning `MeshRenderer` snapshot validate
  entity/document generations. Nonempty, equal-sized batches require unique live keys and validate
  every target before mutation. Component attachment, mesh/material shader replacement and removal
  commit the whole batch as one Runtime Undo step; equal-value batches preserve Redo.
  Unresolved resource IDs are retained in scene persistence, and Undo can return to the saved clean
  baseline. These synchronous authoring APIs do not perform asset lookup, I/O, residency, or GPU work.
  `PrepareSave` owns immutable serialized scene bytes plus document generation, content signature
  and opaque baseline, bounded by the existing 64 MiB scene-file limit. Preparation performs no IO
  and does not change dirty state, selection or history. `SavePrepared` revalidates generation and
  all serializable content (including opaque bytes and authored Euler turns) before any file IO;
  only successful atomic single-file replacement advances the clean baseline. Ordinary `Save`
  uses the same path. Its optional written-bytes result owns exactly the prepared bytes after
  successful publication and is empty on failure; the owning result is allocated before IO.
  Both calls are serialized by the authoring host; the snapshot can be copied
  or retained without World borrows, but submission still requires the live owning document.
  Callers retain workspace writer/recovery and destination-path responsibilities. These individual
  calls publish one file; coordinated publication uses the separate `SceneSaveBatch` owner below.
  `Dirty` compares the live serializable scene to the last successful Save or Reload. Its signature
  preserves sibling order while ignoring storage order left by a restored subtree, so Undo can
  return to a clean scene. Failed saves keep the previous baseline; external Runtime edits are seen.
- `SceneSaveBatch` borrows one writer workspace and its current named scene-file sessions. The
  workspace, sessions and documents must outlive the batch. Prepare/publish/cancel are serialized
  on the authoring thread; drain background readers before publication. Up to 16 unique documents
  capture owning `PreparedSave` outputs and exact disk baselines, with 128 MiB aggregate original
  and output bytes and the existing 64 MiB per-file limit. This bounds serialized payload, not
  total resident memory. Read-only, stale, externally changed, recovery-blocked, unnamed,
  duplicate, ASCII case-colliding, aliased or unsafe inputs reject before publication. Destination parents must exist
  as ordinary directories; occupied sibling staging paths are preserved.
  `.nexora/scene-save-all.recovery` retains a bounded project-UUID/version/checksum manifest and
  exact originals and outputs. Preparing authorizes no source writes; prepared authorizes
  sequential native replacements after all copies/stages validate; committed acknowledges every
  document baseline only after all files verify. Rollback records a separate completed phase so
  partial cleanup can be retried without requiring copies already removed. Metadata uses the
  existing atomic-write policy; file replacement uses POSIX rename or Windows replace/write-through,
  without deleting a destination before retry. Independent readers do not observe a filesystem-wide
  atomic snapshot, and power-loss/fsync durability is not promised.
  Before restoration, recovery validates every retained payload and all current destinations.
  Conflicting external changes, malformed metadata, aliases and unknown retained entries block
  recovery and preserve inspection data. `RecoveryRequired` leaves documents dirty and originals
  retained; `PublishedRecoveryRequired` acknowledges the complete batch while gating authoring
  until cleanup finishes. `ProjectWorkspace` recovery status and controls recognize both journals.
  Discarding an uncommitted Save All restores its originals; discarding a committed batch finishes
  cleanup and retains its acknowledged outputs. Baseline acknowledgement preserves identity,
  selection, opaque metadata, Euler turns and Undo/Redo. Graphical additive tabs and composition
  are separate host workflows. This rebuild-required C++ API does not change scene or gameplay C ABI.
  [Linux acceptance](../../Tools/Build/evidence/EditorEDM4-SceneSaveBatch-Linux-2026-10-09.md).
- Linux ownership/lifecycle acceptance is recorded in
  [the additive session evidence](../../Tools/Build/evidence/EditorEDM4-AdditiveSceneSession-Linux-2026-10-09.md).
- `AdditiveSceneComposition` borrows the session and persists at most sixteen named paths, roles,
  prior-row dependencies and active selection in a project-bound 32 KiB UTF-8 record. Start from
  `BootstrapScene`, attach one clean primary, then Restore after Play/readers drain. All candidate
  sources/dependencies plus final exact source/metadata revisions verify before membership changes.
  Named source payloads have a 128 MiB aggregate logical limit, not a total RSS bound. Restore
  counts actual loaded baselines after each admission and again before publication, since source
  files can grow after size preflight. Save and final Restore require existing source revisions and
  recheck all destinations for aliases, including byte-identical hard links introduced after admission.
  A rejected restore
  preserves the primary and drops only newly admitted records; invalid/foreign metadata, source ID
  collisions and external changes preserve their bytes. Save writes only metadata and requires its
  observed baseline, all named/live/source-resolved documents and writer access. Dirty scene content
  remains dirty; unnamed/unresolved sets retain the previous composition. No source IDs or opaque
  payloads are rewritten, and no power-loss/hostile-filesystem durability is promised. See the
  [Linux acceptance](../../Tools/Build/evidence/EditorEDM4-SceneComposition-Linux-2026-10-09.md).
- `AdditiveSceneSession` borrows one current project and Editor World and owns up to sixteen
  coexisting documents/file sessions; it can attach an externally owned primary document. Admission
  and active switches retain each document's selection, generation, opaque metadata and Undo/Redo.
  Owning snapshots follow deterministic dependency order; missing/cyclic dependencies and removal
  of required documents reject atomically. Paths are distinct, including ASCII case collisions and
  existing filesystem aliases. The owner Save As entry rechecks collisions against every other
  open document/reference before writing; trusted hosts use it instead of bypassing membership
  through borrowed file sessions. Unloading/unloaded scenes reject attachment and stale access.
  Existing global entity IDs must not collide: rejection preserves
  other scenes and unknown payloads without silent ID rewriting. Inspection-only references expose
  const document/file views and are excluded from Save All. Mutable borrows require owned documents,
  a current writer and resolved recovery/external workspace state; these are cooperative host checks,
  not a native-code sandbox. Unnamed owned documents require Save As before coordinated publication.
  The host serializes calls, stops Play and drains readers before membership changes or Save All.
  Document/file borrows survive other admission/switches but expire on removal/destruction. Owned
  owners destroy histories/file sessions before releasing their Editor scene records, preserving the
  monotonic ID watermark. Detaching a borrowed primary changes membership only; its original owner
  remains responsible for document/World lifecycle. Graphical tabs and persisted composition require
  separate host integration. This C++ API requires a rebuild and changes no scene/gameplay schema.
- `AdditiveSceneGraph` owns scene descriptors and dependency edges, distinguishes owned documents
  from references, and rejects cycles or unsafe removal atomically. Initial dependencies must refer
  to already admitted scenes; zero, self, missing dependencies and duplicate scene IDs reject before
  mutation. Initial and replacement dependencies are sorted and deduplicated, preserving deterministic
  load order and reverse-order removal after a rejected edit. Descriptors do not open documents or
  publish files; additive tabs and coordinated multi-document save remain separate host workflows.
  This validation changes no serialization schema, class layout or module linkage. Migration
  dry-runs never mutate source text; autosave writers and readers share a 64 MiB payload limit.
  Oversized writes are rejected before filesystem mutation, occupied temporary paths are preserved, and failed
  writes/replacements clean only this attempt's temporary file while retaining the destination.
  The schema-1 writer uses the classic locale regardless of the process locale. Recovery reads
  at most 127 header bytes and requires the writer's `NEXORA_AUTOSAVE 1 <revision> <size>\n`
  syntax with unsigned decimal fields. It rejects non-regular files and file symlinks, checks the
  total file budget and exact declared payload length before allocation, and still verifies EOF
  after reading. Failures preserve source bytes and the caller's revision; success clears a prior
  error. This is a serialized-file preflight, not protection against concurrent file substitution. Calls are
  serialized by the authoring host; concurrent writers are not supported. Bounded autosave
  recovery rejects corruption without changing the caller's revision; stable-path three-way
  records retain unresolved base/local/remote values without coupling conflicts to a source-control provider.
- Inspector adapters borrow reflection metadata and expose differing multi-selection values as an
  explicit mixed state. `InspectorPropertyAdapter::ApplyBatch` prepares one owning request for up to
  100,000 unique nonzero entity IDs and calls one transaction writer. Empty/duplicate/oversized
  selections, read-only or stale component/field/type metadata, ambiguous fields and nonfinite
  scalar values reject before the writer runs. The authoring owner revalidates live entity/document
  generations, workspace permissions and component-specific value types, then commits all targets
  as one Undo transaction or returns false without changing state/history. Deferred consumers copy
  the request and repeat those live checks at commit; the adapter does not own the World or undo
  stack. Legacy `Apply` remains a single-target callback API; multi-target calls now reject instead
  of allowing a later failure to leave earlier targets modified. C++ consumers must use `ApplyBatch`
  for multi-selection; this adds no stable C/Zig wire or persistence-schema change.
  Unknown component stores own opaque bytes and replace their state only
  after a complete payload validates, so unavailable plugins do not silently discard authoring
  data.
- `ProfileSession` retains a bounded, monotonic frame history. Invalid or out-of-order samples are
  rejected; capacity evictions increment a dropped count. Capture can be paused and cleared without
  changing project files. The graphical host currently supplies Editor frame processing wall time
  after BeginFrame and before Present; completed native GPU timing is ingested separately.
  `SampleProcessMemory` is a separate application-thread observation, throttled to one real Core
  OS read per 250 ms using a monotonic clock. `ProcessMemory()` returns a copied optional current
  process RSS / working-set byte count, observed peak since Clear, and attempt/success counters.
  A failed read publishes unavailable for the latest value while preserving the historical peak;
  no previous success masquerades as a fresh measurement. Capture pauses both wall-frame ingestion
  and memory reads while preserving observations. Clear resets both histories and the sample timer,
  retains the pause state, and allows an immediate observation once capturing resumes. Clock
  rollback/same-timestamp calls do not sample. Counters saturate and the timer handles the clock's
  upper boundary. Memory observations are process-wide across project changes and detachment,
  including shared resident pages, rather than scene/GPU/allocator usage or an OS lifetime peak.
  Widgets copy the observation and never call the reader. The optional injected function-pointer
  reader supports deterministic owner-contract tests; the application uses the real default.
  `FrameSample` and schema-1 wall-time CSV/JSON capture remain separate and contain no RSS samples.
  `ProjectWorkspace::ImportEditorFrameProcessingCsv` reads the project's exported CSV synchronously
  into an owning `FrameProcessingCapture`, without changing files, workspace state or a live
  session. Read-only observers may import; closed/recovery-pending projects, aliased metadata/leaf
  paths, nonregular/unreadable/missing files and corrupt data reject with an error. Input is bounded
  to 128 KiB and 1-600 ordered, nonzero frame IDs. The fixed header, five unquoted columns, finite
  nonnegative wall times, consistent uint64 drop counts and empty GPU/memory cells are required.
  LF/CRLF are accepted; integer/double parsing is locale-independent and consumes entire fields.
  Snapshots retain numeric precision and mark GPU/memory unavailable. CSV contains no project/device
  provenance; arbitrary CSV formats remain deferred.
  `ImportEditorFrameProcessingJson` applies the same read/access/size/sample policy to schema-1
  `.nexora/frame-processing.json`. It requires the current project UUID, exact source/metric/scope/unit,
  matching sample count, lossless decimal-string uint64 IDs/drop counts, false availability flags and
  null GPU/memory samples. Duplicate/missing/unknown fields, trailing data and invalid JSON numbers
  reject without mutation. Fields may be reordered; ASCII schema strings accept equivalent JSON
  escapes. This bounded nonrecursive reader supports only the exported schema, not arbitrary captures. Filesystem checks
  retain the serialized authoring-thread contract, not a concurrent filesystem adversary guarantee.
- Gizmo transactions own their stable-ID and initial-transform snapshots until commit or cancel.
  If an update callback rejects a target after earlier targets were applied, the transaction uses
  that same callback to restore those earlier targets from the initial snapshot before reporting
  failure, rather than leaving a partially previewed gesture.
  Picking results are accepted only for the latest request and matching scene/viewport generations.
  Scene camera files are atomically replaced, while undo/redo history owns its replay callbacks.
- `PlaySession` remains the Runtime-owned PIE boundary. Play worlds are isolated and discarded by
  default; explicit apply-back is required and rejects concurrent Editor transform changes and
  entities reparented during play atomically.
  Console records and inspection/debugger state cross as owning snapshots, never live World pointers.
- Specialized tools are registrations, not implied backends: a tool must report `Implemented`,
  `ReadOnly`, or `Unavailable`, and every non-implemented state carries a reason.
  Schema/interface version one now owns a provider ID, declared operation permissions,
  document/contribution IDs and document/preview/pending-operation budgets. Stable IDs use lowercase
  ASCII letters, digits, dot, hyphen and underscore, starting with a letter; displayed titles and
  diagnostics are bounded UTF-8 without C0, DEL or Unicode C1 control characters. Ordinary
  non-ASCII continuation bytes retain their code-point meaning. Discovery is limited to 128 tools,
  128-byte IDs, 256-byte titles, 1024-byte reasons, 16 document IDs and 16 contribution IDs, and
  4096 total text bytes per descriptor. Document/preview declarations are positive and at most
  16 MiB/128 MiB; pending operations are 1..64. Invalid versions, states, permissions, duplicate IDs
  or exceeded limits reject before registry mutation. Removal releases discovery capacity;
  `Snapshot()` owns copied metadata across provider removal. `Find()`/`Tools()` are borrowed only
  until the next mutation. Calls are serialized on the authoring thread.
  These declarations grant no access and enforce no native allocation policy: the implementing
  host must separately check permissions, access, document generation, actual resource use and
  preview lifetime. Empty contribution lists preserve the original four-field aggregate discovery
  API; they do not certify a production tool. Registry removal neither unloads a native library nor
  edits source payload. The production reference plugin and graphical fallback remain separate
  acceptance work. Public C++ consumers must rebuild; the C/Zig gameplay ABI is unchanged.
- Build manifests own copied profile/artifact data and are atomically replaced. A successful
  manifest always records its target, configuration, reproducible command, artifact sizes, and
  checksums. Manifest JSON numbers and control-character escapes use the classic locale, including
  under a digit-grouping global locale. Publication streams directly into a native-path sibling
  stage through the shared atomic publisher: occupied files/directories/leaf aliases reject without
  truncation, failed replacement preserves the destination and cleans owned staging, and successful
  validation/write clears a prior error. Serialization callbacks run synchronously and retain no
  manifest borrows; callback exceptions close staging and attempt owned-stage cleanup before
  propagating. Schema-1
  fields and artifact byte-count number types are unchanged. The writer records supplied metadata;
  it does not execute the build command or independently verify artifact checksums. All profile
  and artifact strings must be valid UTF-8 before any directory/stage creation. Artifact paths
  use normalized project-relative forward-slash syntax: roots, drive/stream colon syntax, backslashes,
  dot traversal, component-ending dots/spaces, repeated/trailing separators, ASCII controls and
  DEL reject on every host. UTF-8
  paths are parsed as native UTF-8 instead of a system code page. Valid escaped control text remains
  supported in profile/checksum metadata; existing exact duplicate-artifact checks remain in force.

## Owning Runtime scene capture

The owning capture is consumed by the pure `CookStaticProject` producer, which resolves explicitly
supplied owning OBJ/scalar-PBR inputs into an exact Runtime StaticView dependency closure and
returns deterministic package bytes. [Producer contract](StaticProjectExport.md) specifies identity,
bounds, reserved references, lifetime/threading and the separate publication boundary. The separate
StaticProjectExportJob and graphical Build menu now provide owning background cooking/verification,
cooperative cancellation and current-content-checked single-package publication; see the coordinator
section of that contract. Full executable Build/deploy workflows remain open.

`SceneDocument::CaptureRuntimeScene` synchronously returns an owning `RuntimeSceneCapture` from a
live, non-unloading Editor World scene. `runtime_snapshot` contains every Runtime entity and its
components, including entities without an Editor node, and preserves the exact bytes from
`World::SaveScene(scene, max_bytes)`. `nodes` contains only the document's tracked NodeKeys and their
complete opaque type names/payloads; tracked nodes with no opaque data still retain their keys.
Nodes follow Runtime entity storage order and each node's opaque records sort by unique type ID.
Editor node names and authored Euler hints are excluded. The constructor does not adopt existing
Runtime entities or invent Editor metadata.

The capture owns all strings/vectors and survives later edits, New/Reload, and destruction of the
document and World. Scene ID, document generation and NodeKey generations identify the captured
objects; they are not an authoring revision and do not prove that captured content is still current.
Default equality compares the complete owning data. Consumers must perform their own current-state
comparison and access/recovery checks before publishing a result.

Capture checks the 100,000 Runtime entity limit before serialization, then preflights nonzero/unique
identities, tracked-node membership in this scene and opaque metadata before copying payloads.
Runtime output is capped at 64 MiB. Opaque limits are 4096 records total, 64 per entity, 1 MiB per
payload, 256 bytes per nonempty type name and 16 MiB total raw type-name bytes plus payload bytes.
Names containing CR, LF or NUL reject; legacy non-UTF-8 names retain their original bytes. Invalid
identities, missing/foreign nodes, lifecycle/kind failures and exceeded budgets return no partial
capture and an actionable optional error. These are logical data/output limits, not allocator or
process-RSS bounds.

Calls are serialized with all World/document authoring access on the owning thread; the operation
is not thread-safe. Capture performs no IO and does not change selection, clipboard, Undo/Redo,
dirty state or the saved baseline. It does not authorize publication, workspace write access or
recovery decisions. The owning result may be transferred to a worker without retaining World
borrows, but that worker gains no authority to mutate or publish to the source document.

## Threading, errors, and deferred work

Project create/open/upgrade, recent-project mutation, workspace/layout writes, and content-model
publication remain serialized on the authoring thread. `AssetImportQueue` workers only read source
snapshots, create deterministic staging results, and append bounded progress/diagnostic events; they
never mutate a live `AssetWorkspace`, `ProjectContentSession`, or UI model. Cancellation is checked
before and between enumerate/read/stage/publish phases. A queued or in-flight cancellation, worker
failure, stale completion, or dependency-cycle rejection discards staging and preserves the active
index/artifact. The writer lease serializes cooperating Editor processes; read-only observers cannot
upgrade, recover, save layout, or open writable content. A multi-file rename failure rolls
already-moved files back before returning an actionable error. The synchronous `ImportTree` and
`Reimport` entry points remain compatibility paths for headless callers; the graphical shell uses
the background queue. The optional graphical host presents `DirtyConflictModel` decisions, while
the core remains the authoring-thread source of truth and never reloads over unsaved state.
Functions report expected failures with `false`, optional values, stable diagnostic codes, or
per-entry error text; filesystem exceptions are converted to error results where applicable.

Initial indexing failures retain their owning `AssetEntry::error` in
`ContentItem::import_diagnostic`. Content Reset/Discover and project-session admission require
at most 1024 valid UTF-8 bytes without NUL per diagnostic; invalid admission preserves the prior
model, selection, history and generation. Project admission identifies the offending relative
asset path. This is an in-memory field, not a scene/project schema or persisted error log;
public C++ consumers rebuild for the appended ContentItem field. Rename/move/delete Undo copies
retain the diagnostic. Successful Ready artifact publication clears it in both the live item and
retained Content Undo snapshot, so Undo cannot resurrect a resolved failure. Failed/cancelled/stale
reimport preserves the last accepted item; current operation diagnostics remain in the separate
bounded reimport status. Read-only observers retain independent snapshots.

`ReimportJobRequest::initial_import` explicitly admits recovery with no previous artifact;
it defaults to false and must agree with an empty `previous_artifact`. Ordinary reimports still
require a nonempty baseline. ProjectContent sets the flag only for failed items without an
artifact. Publication still revalidates the exact captured empty baseline, project generation,
path, source revision, settings and dependencies; a worker gains no publication authority.
This appended C++ request field also requires consumers to rebuild; the plugin C ABI is unchanged.

The `.meta` filename suffix is reserved for asset identity sidecars and is excluded from the source
asset index. Artifact hashes use the persistent UUID plus source bytes rather than the current path,
so an Editor move followed by reopen does not invalidate identity or derived-data addressing.
Profiling samples require strictly increasing frame IDs. Telemetry drops every event until the user
explicitly opts in. Its in-memory queue retains at most 1,024 events of at most 1,024 UTF-8 bytes
apiece; empty, embedded-NUL, invalid UTF-8, oversized and over-capacity events return false without
changing accepted records. Reaffirming consent preserves the queue. `Set(false)` releases all event
strings, including after repeated revocation; later opt-in starts with no previous records. Events()
borrows a span until the next queue/consent mutation. Calls are serialized by the owner; this
primitive provides no persistence, transmission, secure memory wiping or content redaction.
Extension policy rejects untrusted publishers and, by default, invalid or missing signatures.

Watcher events are path-coalesced after a caller-supplied debounce interval and known self-writes are
discarded. A disk change never overwrites dirty authoring state: `DirtyConflictModel` retains both
hashes until the authoring thread explicitly chooses reload, keep, or compare. Compare is
non-terminal and keeps the conflict actionable; reload and keep are terminal decisions retained for
the document/import owner to consume without the UI touching the filesystem.

The core deliberately does not depend on a UI toolkit. The optional `NexoraEditorImGui` owner
provides docking, theme/DPI scaling, input/text forwarding, stable-panel presentation, and recovery
choice UX. Its Hierarchy filter, generation-keyed expansion/selection anchor, rename buffer, and
row clipping are presentation state; selection, rename, and hierarchy edits still enter the core
only through `SceneDocument::Select`, `SceneDocument::Rename`, and the undoable, cycle-safe
`SceneDocument::Move`. Its initial Inspector reads a selected node's borrowed local transform and
routes position, Euler-degree rotation, and scale changes through generation-keyed `SceneDocument` calls.
`SceneDocument::CreateMesh` creates a named mesh root with initialized pose/component as one Undo.
Metadata and Runtime history retain the same stable ID, name, mesh/material values and pose across
Undo/Redo; Save/Reload uses the existing scene schema. Empty/newline names and invalid poses reject
before creation; missing/unloading scenes and zero mesh references reject in the Runtime initializer.
The caller owns workspace permissions and asset-generation/residency checks. The graphical Content
Browser resolves exactly one selected mesh against its project-generation CPU catalog, places the
root at the Scene center, and selects it; it performs no source reads during the action.
The single-selection Camera component toggle and field edits use the same generation key and undo
boundary; invalid clipping and stale keys leave the scene unchanged. Camera values persist in the
runtime scene snapshot, so Save and Reload retain them.
`SceneDocument::AlignCameraToWorldPose` sets one generation-checked Camera's world position and
rotation as one transform Undo step. It preserves lens values, local scale, parent, selection and
other components. Each ancestor's local TRS is inverted from root to immediate parent, retaining
exact camera translation under sheared and mirrored chains instead of using lossy world scale.
Invalid/stale/non-Camera targets or non-finite local results reject before mutation. Equivalent
world positions within 1e-10 and equivalent quaternion rotations keep Undo/Redo unchanged.
The graphical Camera Inspector exposes this as Use Scene view pose when one Camera is selected and
native Scene 3D is enabled/available; workspace/modal gates apply and existing drafts are canceled.
The single-selection Light toggle and intensity field follow the same generation and undo rules;
intensity must be finite and nonnegative.
`SceneDocument::ResetTransforms`, `ResetCameras` and `ResetLights` synchronously borrow a nonempty
batch of generation-checked keys on the authoring thread. Stale or duplicate keys reject the entire
batch. Transform reset restores local identity TRS and zero authored Euler hints, preserving parent,
selection and unrelated components. Camera/Light reset restores defaults only where that component
already exists; absent components remain absent. Each changed batch owns one Undo step. Metadata-only
Euler resets clear visible and stale hints so old revolutions cannot revive on a later rotation,
and still record matching Runtime history; already-default batches preserve Undo/Redo.
Save/Reload retains committed defaults through the existing schema. These additive Editor C++ APIs
require consumers to rebuild; stable C/Zig contracts and scene formats do not change. Workspace
write permissions remain the caller's responsibility.
`SceneDocument::SetTransformValues` synchronously borrows target keys and Euler values on the
serialized authoring thread, validates a nonempty unique live batch and a finite normalized TRS
whose rotation matches the authored Z-X-Y degrees, then replaces local TRS and hints together as
one Undo. Invalid keys, nonfinite/zero-scale poses or mismatched Euler values reject before mutation.
Metadata-only edits also record Runtime history; equal normalized values retain Redo. Hidden stale
hints are replaced so old turns cannot revive. Parents, names, selection and other components remain
unchanged. Workspace access is caller-owned; this additive C++ API requires rebuilding consumers,
without changing scene schemas or stable C/Zig wires. Reset Transform uses this same transaction.
The optional Inspector's component-value clipboard owns committed numeric Transform/Euler,
Camera lens or Light intensity values, independent of the SceneDocument entity-forest clipboard.
No source key, World reference or asset payload is retained. Camera/Light value Paste uses existing
batch setters and preserves absence; the clipboard is not scene persistence or an OS text clipboard.
The graphical Camera/Light host also supports mixed multi-selection and rechecks current selection
and access before each batch. Its canceled drafts never enter SceneDocument or scene persistence;
returning from read-only/recovery, application focus loss or Play inspection cannot revive them.
The host now applies workspace write access to Scene/Hierarchy controls, shortcuts and pending
mutation requests as well. Read-only selection, Copy and view navigation remain available; access
changes cancel prospective gestures before release. SceneDocument remains independent of workspace
permissions; its caller owns this access policy.
Multi-selection fields display mixed state and apply one changed field to every selected entity as a
single all-or-nothing Runtime transaction and undo step; malformed transforms roll back without a
partial write. `SceneDocument` owns authored Euler hints and restores them with undo, even when a changed angle
has the same quaternion. Scene format version 2 persists finite, rotation-matching hints; version 1
loads with canonical angles. Duplicate, orphaned, non-finite, or mismatched hints reject reload before
live state changes. Reload atomically replaces the existing Editor scene, preserves its ID and state,
advances document/entity generations, and clears selection and undo. Failed reload preserves them.
External rotation changes invalidate hint display and serialization. Reflected component widgets remain open. Native renderer submission,
platform IME candidate positioning,
accessibility, viewport rendering, gizmos, and target-host visual validation remain UI-host
responsibilities.
The portable gizmo state machine and picking validator define transaction and asynchronous-result
policy only; they do not claim graphical manipulation or renderer-backed picking acceptance.
`Nexora/Editor/ViewportMath.h` adds the UI-neutral Scene View math those hosts consume, in double
precision to match `runtime::Transform`: `ViewportPickRay` (pixel to world ray, origin top-left),
`PickNearest` (AABB slab picking that skips hidden, locked, and malformed candidates, hits at
distance 0 from inside a box, and breaks distance ties by lowest entity id), `AxisDragDistance`
(closest-point projection of a drag ray onto a gizmo axis; undefined for a parallel ray),
`SnapToStep` (half-away-from-zero grid snapping that never launders NaN), and
`ViewportResizeFilter` (dead-band plus stable-frame hysteresis so panel jitter does not reallocate
render targets). These are CPU-only and covered by `editor.viewport_math`.

The same header carries the gizmo math, following Unity's tools. A gesture captures `GizmoTargets`
(each root's local transform, its parent's world transform, and owning root-to-parent local poses)
and, for Center, `SelectionCenter` once at Begin. Every frame it applies the whole delta since Begin with `ApplyGizmo` and passes the
resulting local transforms to `GizmoTransaction::Update`. Frames therefore never accumulate rounding,
and Cancel restores the start exactly. The decision table:

| Rule | Behavior |
| --- | --- |
| Global / Local (`GizmoAxes`) | World axes, or the axes of the entity's world rotation. A mirrored axis is shown unmirrored, as in Unity. |
| Translate | World-space offset; only local positions change. |
| Rotate | About a world axis, applied after the existing world rotation. Pivot turns each entity in place; Center also swings positions about the centre. Angle from `RotationDragAngle`, signed by the right-hand rule, in (-pi, pi]; sum per-frame angles for longer turns. |
| Scale | Factors multiply each entity's local scale, as in Unity's scale tool: exact for the uniform handle and for entities aligned with the gizmo axes. Center also scales the offsets from the centre along the gizmo axes. |
| Negative scale | `ScaleDragFactor` never crosses zero (minimum `kMinGizmoScaleFactor`), so a drag can neither create nor remove a mirror. An existing mirror is kept, since factors are positive. |
| Parents | Captured ancestor matrices give exact origins, and individual local inverses convert translated or Center-operated positions back to local space, including shear and mirrors. Rotation and scale retain composed TRS semantics. Manually constructed targets without `parent_chain` retain the `parent_world` path. |
| Multi-selection | `GizmoRoots` drops duplicates, missing entities, and any entity whose ancestor is also selected; that entity moves with its ancestor. Center is the mean of the selected world positions; the portable core has no bounds. |
| Invalid frames | A malformed operation or an unrepresentable result makes `ApplyGizmo` return nullopt. The caller keeps the previous frame. |

Snapping is the caller's choice with `SnapToStep` on the distance, angle (in degrees), or factor.
Renderer-backed ID-buffer picking remains open. The
Editor native proxy preview now draws and picks X/Y/Z translation bars, rotation rings, or local
scale cubes and a camera-facing white uniform cube. Shift held at drag start snaps rotation in 15-degree steps and scale deltas in 0.25-factor steps; preview and commit use the same result. The local basis uses the first selected node's world rotation without mirroring axes.
`PickOrientedBox` tests a viewport ray against a rotated proxy or handle box after a conservative
AABB filter, so empty corners of a rotated bound do not select the object.

`editor.parser_robustness` mutation-tests the parsers that read persisted or external data (trace and
metric decoding, replay log, unknown-component store, scene snapshot, runtime blob, NXSHDR, shader
diagnostics, asset UUID, autosave, camera, and project/workspace/layout/recent-project files) with
fixed seeds. It
requires every parser to return normally on corrupted input; under the ASan/UBSan presets memory and
undefined-behavior errors fail it too. Set `NEXORA_PARSER_ROBUSTNESS_ITERATIONS` for a longer local soak.
Workspace mutations restore valid descriptor/workspace/layout metadata for each input; recovery uses
a real schema-1 journal and writable owner, checking failed recovery preserves committed bytes,
current documents and the journal. Autosave mutations also check source and caller-revision
preservation. It is a robustness check, not a proof that no malformed input can fail.

### Bounded asynchronous mesh thumbnail snapshots

✅ `BuildMeshThumbnail` rasterizes actual imported geometry into an owning 64×64 opaque
RGBA8 image. A fixed isometric camera derives framing from referenced positions; two-sided
flat neutral shading and a depth buffer preserve geometry/occlusion. Cached bounds, materials,
textures and imported normals do not determine this shape preview. Every position must be finite,
every index must address the payload, and indices must form triangles. At most 16,384 vertices
and 24,576 indices (8,192 triangles) are admitted; there is no truncation. Zero projected extent
or no covered samples returns an explicit failure. Cancellation is checked throughout validation
and at every triangle/scanline. Allocation/callback exceptions propagate from this pure function.

`MeshThumbnailCache` uses the existing Core JobSystem at Low priority. Each request copies the
bounded geometry before submission; workers retain no Content/document/UI/owner pointers and
perform no IO, GPU access or authoring mutation. Requests capture nonzero project generation,
nonzero asset UUID, an artifact revision of 1..128 bytes and the immutable geometry owner.
The revision identifies the imported artifact, not a path or global Content revision. `Poll`
rechecks current generation, asset existence, exact revision and geometry ownership before
publishing. Even completed work exposes no pixels until that owner-thread check. A superseded
operation can never publish into the replacement's entry. Ready lookups perform the same checks;
changed/deleted sources report Stale without exposing old pixels.

All cache calls/destruction run on one serialized non-worker owner thread. The Content model and
running JobSystem outlive the cache. One cache belongs to one project session. Call `Clear` on
project replacement/reopen even if its generation is reused; generation changes also clear
on Request/Poll, while Get already rejects their old images. Operation IDs do not reset on Clear, preventing same-generation reopen ABA.
Cancel immediately reports Cancelled; collection still waits for job completion and cancellation
wins over unpublished success. `Shutdown` stops intake, cancels and drains only owned jobs before
releasing their captured inputs. It is idempotent. A host must call Poll regularly to collect work.

The LRU holds 1..64 entries (default 16), including failed/cancelled observations. The pending-work
limit is 1..4 (default 4), including cancelled/superseded jobs awaiting collection. A full pending
budget rejects new work without evicting a current hit. Successful intake evicts the least recently
requested/read entry as needed; in-flight victims are cancelled. Same-key requests deduplicate
both successful and failed/cancelled entries; `Invalidate` explicitly enables retry. A Stale
observation reschedules when Undo restores its source. Validation
or submission failure returns a diagnostic; worker failure is retained as a Failed observation.
No automatic retry, placeholder success or reuse of another revision occurs.

At the maximum there are 64 published 16-KiB images (1 MiB), plus at most four pending results,
bounded input copies and per-worker raster scratch. Ready entries release input geometry after
collection. These are logical payload/work bounds, not allocator-capacity, RSS or VRAM claims.
Returned snapshots share immutable pixels and may outlive replacement, eviction and cache
shutdown; external retained images fall outside the cache's bound.

`editor.mesh_thumbnails` checks actual coverage/shading/depth, extreme finite positions, malformed
input, exact input/cache/work bounds, scanline cancellation, deferred publication, late cancellation,
source replacement/deletion, project generation changes, explicit same-generation reopen, LRU,
visible failure/deduplication, failed submission and retained pixel lifetime. The CPU cache is an
M1-01 prerequisite. The additive C++ API changes no stable C/Zig ABI, asset format or module
dependency. Content image upload/drawing, other asset types, representative project/UI
performance, physical DPI and full ED-M1 acceptance remain open. See the
[retained evidence](../../Tools/Build/evidence/EditorEDM1-MeshThumbnailJobs-Linux-2026-10-21.md).

### Mesh reimport publication

ContentItem and ReimportResult own immutable shared mesh payloads. Synchronous and background OBJ
reimport share the bounded chunk reader/parser; importer type survives rename and is carried in
the job request. A worker only stages geometry. ProjectContentSession checks project/asset/source/
settings/dependency revisions, validates a candidate ContentBrowserModel including its 128 MiB
live mesh capacity budget, then commits dependency/hash/geometry publication. Failed, cancelled,
stale or oversized results preserve the previous payload, artifact and model revision. New payloads
also update the pending rename/move Undo snapshot, so undo cannot resurrect stale geometry.

ContentBrowserModel Revision advances on successful Reset, file-model edits, Undo and artifact/
geometry publication; failed publications do not advance it. Reopening ProjectContentSession keeps
this revision monotonic while clearing its navigation/filter state. MeshAssetCatalog PublishContent
retains owning snapshots and validates stable identities. The application refreshes the catalog
from the live content revision before native drawing; delete removes geometry from live resolution
and Undo restores it. Source IO stays in explicit import/reimport work, outside rendering/picking.

The initial Profiler Export CSV action saves `.nexora/frame-processing.csv` through ProjectWorkspace's
writer lease and atomic replacement. The application passes completed Editor frame samples after UI
commands and before adding the current frame; the call borrows them synchronously, retaining only
serialized CSV data. `cpu_ms` at this call is Editor frame processing wall time after BeginFrame and
before Present, not whole-frame CPU utilization. CSV uses locale-independent full double precision,
columns `frame,frame_processing_wall_ms,older_frames_dropped,gpu_ms,memory_bytes`, and empty GPU/memory
cells because this wall-time format excludes live RSS observations. Export requires 1-600 strictly increasing nonzero
frame IDs with finite nonnegative wall times. Empty/invalid/read-only/recovery exports fail without
replacing the last good file. UI emits a one-shot request, disables export without samples/write
access or during recovery/close confirmation, and shows the application's result; UI never writes a
file itself. Arbitrary capture import remains open; native GPU and process-memory traces use
separate schemas.

Profiler Export JSON writes the companion `.nexora/frame-processing.json` under the same writer,
sample-validation, recovery and atomic-replacement rules. Schema 1 records source `NexoraEditor`,
metric `editor_frame_processing_wall_ms`, scope `after_begin_frame_before_present`, unit
`milliseconds`, project UUID and sample count. Frame IDs and `older_frames_dropped` are decimal
strings to preserve uint64 precision in JavaScript consumers. Wall times use locale-independent
full double precision; availability flags are false and each GPU/memory value is JSON null even
when a caller's FrameSample contains those fields. Export borrows samples only for the synchronous
call and leaves CSV unchanged. `gpu_timing_available` and `memory_measurement_available` are false;
each `samples` entry has `frame`, `frame_processing_wall_ms`, `gpu_ms` and `memory_bytes`.
Measured completed native GPU intervals and process RSS use independent bounded histories and
schema-1 JSON export/import; they are not fields in this wall-time format. See
[native GPU capture evidence](../../Tools/Build/evidence/EditorEDM6-GpuTimingCapture-Linux-2026-10-09.md)
and the contracts below. Physical GPU calibration, per-pass attribution and arbitrary capture
adapters remain open.

Game Apply Changes is an explicit transform-only review. Opening it emits Pause when needed and
releases Game input. The modal owns original/Editor/Play transforms and session/document/entity
generations, clips visible diff rows, blocks authoring/Play shortcuts, and supports Cancel/Escape
while retaining paused Play. Confirm emits an owning one-shot request. Application write/recovery
checks precede `ApplyReviewedPlayTransforms`, which requires paused Play and exact current review
identity/values. Reparenting, Editor edits, reload, new Play sessions, missing/recreated entities,
and entities outside the current SceneDocument reject the entire batch. Success is one atomic,
undoable `SceneDocument::SetTransforms` operation, preserving metadata/selection. Only then does the
application unload the gameplay module before Stop(Discard); component and entity changes remain
isolated. Failure leaves Play paused and reports the reason. Default Stop/F5 always discards.
Portable PlaySession's own Stop(Transforms)/applied_transforms counters are a separate Runtime path;
graphical apply uses document Undo ownership and Console diagnostics instead.

### Missing-plugin component ownership

SceneDocument owns opaque components in authoring node metadata; Runtime scenes and Play clones do
not execute or decode these plugin bytes. SetOpaqueComponent validates the full prospective store,
checks document/entity generations, and records a separate metadata Undo entry without consuming
Runtime SceneEditor history. Equal values are a no-op. Delete/Undo and clipboard/duplicate/create
Undo/Redo retain exact bytes; failed paste capacity checks occur before any entities are created.
SetOpaqueComponents validates a nonempty batch of at most 4096 unique live generation keys and the
complete prospective store before adding/replacing a shared payload. It stages owning node and
previous-payload storage before one metadata Undo publication; a late capacity/key rejection leaves
all earlier targets and Redo intact. Equal batches preserve unrelated Runtime Redo. This generic
boundary grants no access or schema authority: material assignment separately requires exact current
selection, writable Content, current catalog payload identity and valid supported references on every
Mesh Renderer. Inspection of those references copies only bounded metadata, including exact byte count.
RemoveOpaqueComponents validates a nonzero type and at most 4096 unique live keys, stages owning
nodes/history and validates the remaining complete store before one metadata removal publication.
Absent references preserve Redo; unrelated types remain exact, while Undo history owns the removed
payloads for restoration. The generic boundary grants no schema/access authority. ClearMaterialAssets
only removes supported exact schema-1 references after validating the complete current selection,
live Mesh Renderers, nonzero project generation and writable Content. Source/catalog availability is
irrelevant to clearing stored supported references; any unsupported reserved payload rejects every
target. Legacy mesh/shader values and material files remain unchanged.
OpaqueComponents returns owning complete payloads. InspectOpaqueComponents returns owning names,
full-width type/entity IDs, byte counts and only the first 64 bytes, avoiding full-payload copies each
GUI frame. Inspection does not grant project write permission.

UnknownComponentStore accepts nonzero IDs and bounded nonempty single-line names without NUL;
limits are 256-byte names, 1 MiB/component, 64/entity, 4096 records and 16 MiB of bytes plus names.
Replacement and failed deserialize retain previous state. Deterministic classic-locale serialization
sorts entity/type IDs and hex-encodes binary bytes (empty payload is `-`). Serialized stores are
bounded to 34 MiB. Move transfers ownership/budget counters and leaves a reusable empty source.

Scene format 3 adds `opaque <entity> <type> "name" <hex-or-dash>` metadata before `world`; scenes
without opaque records still write format 2; persistent prefab placements use format 4. Reload
accepts formats 1/2/3/4, bounds the file to 64 MiB,
stages all records and world validation, and rejects duplicate/malformed/oversized/orphan records
before replacing live state. Every opaque owner must be both an authoring node and a staged Runtime
entity. Successful reload invalidates old keys and clears history; failed reload preserves live data,
selection, generations, history and the saved baseline. All tracked authoring node IDs must exist
in the staged Runtime snapshot even when there are no hints/opaque/prefab records. The parser
matches unique Runtime IDs against authoring IDs in one scan before live replacement; Runtime-only
entities and scenes with no tracked nodes remain supported. No format/API/ownership change is
introduced. Fixed LF golden sources cover Editor versions 1–4 and Runtime versions 1–3, independent
legacy world/local pose expectations, Euler turns, unknown bytes, bindings and deterministic upgraded
save/reopen. Required-field prefixes, corrupt versions/duplicates/orphan nodes and occupied staging
preserve last-good data, dirty/saved baseline, keys/selection, clipboard, Undo/Redo and unrelated
scenes. Syntactically valid final whitespace omission stays accepted. These checks do not establish
interrupted publication recovery or complete migration/physical acceptance.
Opaque metadata participates in Dirty and
atomic scene writes. Its exact saved comparison is cached between authoring mutations, avoiding
full payload serialization on every GUI frame; Undo/Redo invalidate that cache. Both saves and
chunked reads enforce the 64 MiB scene-file bound. Runtime snapshot and stable gameplay C ABI formats do not change.

SceneDocument SetCameras/SetLights check every document/entity generation and duplicate ID before
committing one Runtime batch and one document Undo entry. Invalid batches preserve all components,
selection and history; equal batches add no undo step. Presence and each entity's unrelated fields
are retained in Undo/Redo and scene persistence. Single-entity setters delegate to the same boundary.
Camera/Light edit access is independent of Content Browser resource access; project read-only and
recovery/modal guards reject pending component requests. Mesh assignment separately requires a
writable content session and matching project generation.

## Clipboard forest creation

CopySelection captures a complete owned forest at copy time: roots use their world TRS while
children retain local transforms and remapped internal parents. Root names gain ` Copy`; descendants
retain their names. All Camera/Light/MeshRenderer payloads, full-width unresolved resource IDs and
opaque bytes survive. Euler hints retain authored revolutions when their rotation matches the copied
pose; detaching a root with a differently rotated outside parent uses its world orientation.
Paste prevalidates the complete opaque budget/names, invokes one Runtime CloneEntityForest and
publishes one metadata Undo entry. One Undo removes all created entities and restores prior selection;
Redo restores initialized pose/component data, stable IDs, names and metadata. Duplicate preserves
the prior clipboard even on failure. The serialized authoring thread retains no live World borrow in
clipboard/history, and no source IO or GPU residency work occurs during Copy/Cut/Paste/Duplicate.

Paste now stages the complete replacement node metadata, root selection and next history capacity
before Runtime clone publication. After clone succeeds, assignment and swaps publish without a new
metadata/history allocation. Exhausted entity-generation space rejects instead of wrapping.

`SceneDocument::ImportForestBytes` accepts at most 8 MiB and 4096 source authoring nodes, using the
same versioned parser/migration and unknown-payload validation as Reload. It requires caller-provided
current authoring permission and an exact matching owning PreparedSave for the target document.
The serialized host remains responsible for writer lease, recovery, Play and modal authority.
Invalid/foreign-generation/stale/over-budget inputs return no partial result and retain source,
selection, Undo/Redo and the user clipboard. Parsing uses a separate World/document before mutation.

The complete source World storage order determines the imported sibling order. All internal parents
are remapped to fresh entities through one existing CloneEntityForest transaction; root names,
initialized components, authored Euler revolutions and exact opaque bytes remain unchanged.
The returned owning source-ID/current-generation target-key map has no source World/document borrows.
One Undo removes the entire import and restores prior selection; Redo restores the same initialized
identities and metadata. A scoped restore keeps previous Copy and pending-Cut clipboard behavior,
including failure/exception unwinding. Result capacity is allocated before publication.
This operation performs no file IO, GPU admission, native loading, persistent prefab-instance binding,
existing-parent attachment or arbitrary opaque entity-reference interpretation. Those remain host/
prefab integrations. Public C++ consumers rebuild; stable C/Gameplay ABI and module graph are unchanged.


## Cut clipboard lifecycle

`CutSelection` first captures the complete owning forest, then invokes atomic `DeleteSelection`.
A rejected capture or deletion restores the previous clipboard and its pending-cut state. One Undo
restores the original IDs, payloads, metadata and selection. The next successful Paste creates new
IDs at the captured poses while preserving root names; subsequent Paste uses normal ` Copy` names.
Rejected Paste does not consume this pending state. Copy replaces it, Duplicate preserves it, and
Reload clears it. Clipboard state is transient and independent of document Undo/Redo: undoing Cut
or Paste does not reverse the clipboard's last successful action. All operations stay on the
serialized authoring thread; no scene format or C ABI changes are required.

`editor.affine_gizmo_contract` checks independent closed-form world translation, Center rotation
and Center scale through three-level mirrored/sheared ancestry, owning pose/matrix previews,
selected-descendant filtering, one-step Undo/Redo, stale/invalid rejection and scene save/reload.
The application now consumes owning exact world/preview matrices for authored Scene meshes and
bounds/triangle picking, with live post-tick matrices for Game meshes; proxies/gizmos retain TRS.
Editor C++ consumers rebuild for the added owning target field and matrix getters;
stable C/Zig wire layouts and scene formats are unchanged.

## Managed scene files

`SceneFileSession` borrows one workspace and document for its lifetime; calls run serially on the
application authoring thread. Returned paths, tokens, and diagnostics own their values. New/Open
advance the document generation and clear selection, entity clipboard, and Undo/Redo. New keeps the
Runtime scene ID/name/activation/persistence, starts empty and dirty, and is a document boundary.
Open stages the complete file before replacement; a malformed/missing file preserves the current
World, authoring metadata, generation, dirty state, history, and managed path. It is allowed in a
read-only workspace. The caller owns the Stop Play policy and any asset-index publication.

Save/Save As require current project UUID/root/document token and write access. Paths must be bounded
UTF-8 project-relative `.scene` filenames without traversal, nonportable punctuation, controls, or
canonical parent/symlink escapes. Lexical and resolved paths both enforce the case-insensitive
`.nexora` namespace restriction to `scenes`; file aliases to other extensions reject. Successful
association adopts the canonical relative path so aliases cannot bypass Content publication. Ordinary
Save needs an associated path; Save As adopts it only after successful persistence. Existing different
destinations and destinations protected after a failed bootstrap load need explicit replacement.
New/Open first return `NeedsUnsavedChoice` for dirty content; the application saves or supplies an
explicit discard choice. Missing path, unsaved choice, overwrite confirmation, and rejection are
separate results; none consume history or modify files. Successful Save retains document Undo/Redo.
The scene writer preserves native Unicode temporary paths, rejects preexisting temporary paths
without truncating/removing them, and replaces with native Windows replace or POSIX rename; failure
never deletes the original destination to retry. This checks paths at operation time; it does not
lock against concurrent external filesystem edits.

`SceneDocument::Save(path, written_bytes)` clears a supplied output string before serialization and
IO. Only after successful atomic replacement does it move the exact serialized bytes, including
opaque components and authored Euler hints, into that caller-owned string. Failure leaves it empty
and preserves document dirty state, selection, generation and history. The original `Save(path)`
delegates to the same serializer without requesting bytes. All calls remain serialized on the
authoring thread; the returned bytes describe the Editor's publication even if an external writer
subsequently changes the file.

Ordinary Save and unconfirmed Save As to the current association compare the actual source bytes
against an owning disk baseline. File size or restored modification time cannot hide a changed
revision. Bind/Open/successful Save establish the baseline; failed Bind/Open leave the live good
association and document intact. Bind is an explicit bootstrap association of a document the caller
has already loaded or created: a missing-at-bind destination permits its first Save, but deleting an
existing associated source rejects ordinary Save. New clears the baseline. Content relocation through
the same asset UUID keeps it, so a renamed externally edited source still conflicts at its new path.
Source bytes are read only during file operations, never during frame polling or Content
synchronization; existing path-scope and regular-file metadata checks remain in those operations.

`NeedsOverwrite` carries an owning scalar `SceneOverwriteToken`; the session retains the exact
reviewed bytes, project/document token and resolved destination. Interactive replacement must pass
that token to Save or Save As. A second disk revision returns a fresh confirmation without writing;
old/replayed tokens, another destination/session, Bind/New/Open and relocation cannot reuse approval.
Read-only access and pending workspace recovery are rechecked before Save, including confirmed Save.
Existing `SaveAs(..., true)` without a confirmation remains an explicit caller-owned destructive
replacement policy; it is not suitable for delayed interactive approval. Cancel does not discard
either version. Unreadable/nonregular/oversized destinations fail closed. A confirmed source that
disappears also rejects. Every successful scene write establishes its baseline directly from the
exact bytes published by `SceneDocument::Save`, never from a post-save reread that could adopt
another writer's revision.
An external edit after successful Save therefore requires review on the next ordinary Save.

Each retained baseline and pending replacement owns at most 64 MiB; both together retain at most
128 MiB of source bytes. One bounded comparison/read snapshot or published output adds at most
64 MiB (192 MiB total source-byte peak, excluding string allocator overhead and SceneDocument
serialization/World data).
Comparison scratch is released before serialization; the published output string moves into the
baseline after pending bytes are released, with no additional copy or reread.
The session is noncopyable; all operations remain serialized on its owning authoring thread. Exact
comparison is an operation-time check, not a file lock or guarantee against a concurrent change
between check and replacement. C++ Editor/EditorImGui consumers rebuild for the new optional
confirmation arguments and result/request fields; stable C/Zig and scene formats are unchanged.

`SceneFileSession::SynchronizeContent` borrows the current same-root Content session for one
authoring-thread call. Bind before Content mutations; refresh after them. A loaded Content scene
tracks its asset UUID and project generation through rename, move and Content Undo. Only an existing
regular managed `.scene` destination can replace its path; document generations, World, selection,
dirty state and Undo/Redo stay unchanged. Deleted, missing, unsafe or stale tracked assets protect
ordinary Save and startup recording. Restoring the same UUID unblocks Save; another UUID at the
old filename cannot adopt the document. New/Open or adopting a Save As destination resets the
association; ordinary Save retains it. Read-only
sessions may track paths but still cannot write. Bootstrap load protection remains independent.

`ContentBrowserModel::Discover` publishes one already-saved owning item without recording a content
edit. It rejects duplicate ID/path, including collisions in the retained Undo snapshot, preserves
folder/filter/selection, and carries the new item into that snapshot so an earlier content Undo cannot
hide it. Application scene saves compose `AssetWorkspace::ImportSavedScene` with discovery/artifact
publication. That operation reads only the saved `.scene` and its bounded identity sidecar, hashes
source bytes with an 8 KiB streaming buffer and a 64 MiB admission limit, and validates identity
against the current in-memory index. It never enumerates directories or rereads/parses unrelated
sources/OBJ files, and retains their geometry ownership. It requires an initialized writable
persistent index and updates only that scene entry after successful import. Index paths own UTF-8
with portable separators; filesystem consumers convert explicitly to native paths. Importing a saved
source remains separate from committing the scene document; a later import failure does not roll
back a successful save.
After a Content move, a saved scene may retarget its same-UUID stale index entry only if both old
source and sidecar are absent and its importer type is `.scene`. Existing sources/aliases/sidecars
and other importer types remain collisions. Publication replaces stale ID/path entries atomically
in memory and keeps other geometry; no directory scan or unrelated source read is introduced.

`SceneFileSession::RestoreStartup` bootstraps before `RememberCurrent`, returning `NeedsPath` for
absent metadata and using normal atomic Open for an associated scene. It never discards a dirty
document. The bounded 1100-byte binary metadata contains schema, project UUID and a UTF-8 managed
scene path; reading revalidates canonical scope and rejects metadata directory/file aliases.
Rejected settings or source loads protect the original record for the session. The caller may
fall back to Main without replacing it. Recording requires the live token, initialized startup state,
write access, no recovery journal, and an unblocked associated file. Ordinary dirty documents cannot
be recorded; a validated same-UUID Content relocation may update its committed filename while
retaining dirty World/history. The relocation exception is consumed only after successful metadata
commit and resets on document/path adoption. It never records Untitled,
New or failed scene operations. A record is a separate atomic commit with the same original/temp
preservation as scene writes; its failure leaves the World, scene save, path and Undo/Redo intact.
The application warns after an independent recording failure and still permits successful Save and
Exit. Read-only startup only reads; per-file camera loading follows the restored association.

Recovery presence means any occupied or uninspectable `.nexora/workspace.recovery` or
`.nexora/scene-save-all.recovery` path, including directories and valid/dangling leaf symlinks.
Only verified absence permits existing recovery-gated authoring/export/shutdown actions.
Recovery rejects unsafe inputs without replacing source files. Explicit writer discard of the
workspace journal removes one directory entry (never recursively); alias targets remain untouched,
and a nonempty directory remains pending after discard fails. Save All discard instead validates
and restores an uncommitted batch or finishes committed cleanup; it never recursively erases the
only retained originals. If both journals are present, resolve the workspace journal first, then
Save All. Read-only observers cannot recover or discard either journal.

## Play input binding values

`PlayInputBindings` is an owning, fixed-size platform-neutral profile for four movement directions
and five copied button bits, with two control slots per action. Its finite control set covers
letters, arrows, Space, left/right Shift/Ctrl and left/right mouse; None unbinds a slot. Validation
rejects unknown controls, duplicate concrete controls (including within one action) and mouse
movement bindings. None may repeat and actions may be fully unbound. The value type depends on no
Window/native key codes and changes no gameplay C ABI. Host policy owns editing, project scope,
lifetime and publication; this value type performs no persistence or transmission.

## Project Play input settings

`ProjectWorkspace::SavePlayInputBindings` explicitly publishes `.nexora/play-input.ini` under the
writer lease with resolved recovery and a verified metadata directory/file. The schema-1 ASCII
format has one header and nine named action records, each containing two stable control names.
`MouseMiddle`, `MouseBack` and `MouseForward` extend the admitted names without changing the
schema or existing names/defaults. Like left/right mouse, they bind only to button actions;
movement remains keyboard-only. Older readers reject unsupported names and preserve the file.
Serialization is deterministic; reads permit reordered action records, CRLF and no final LF, but
reject unknown/missing/duplicate fields, extra records, NUL, invalid controls, duplicate concrete
controls and mouse-axis mappings. `kMaximumPlayInputSettingsBytes` is 1,024; a fixed 1,025-byte read
independently bounds consumption. Only verified missing legacy state yields no value without error.

Reads return owning profiles and permit read-only access. Uninspectable/non-regular/leaf or metadata
aliases and pending recovery reject. Invalid saves touch no staging; occupied temporary files,
directories and aliases preserve unrelated data and the last-good destination through the shared
atomic replacement helper. Explicit valid saves may replace corrupt settings, while reads and
ordinary shutdown never rewrite them. Settings are independent from scene content and journals;
these additive APIs change no existing class layout, module dependencies or gameplay C ABI.

## Scalar PBR material assets

`MaterialToolDocument` owns bounded schema-one material state and history on its construction
thread. Project UUID, asset UUID, nonzero generation and a nonwrapping serial bind observations.
Open retains the exact original source (at most 64 KiB) as the save baseline and uses production
import/export to store canonical state (at most 1024 bytes). Snapshots copy complete material and
source values; no document, workspace, source or native callback borrow survives.

Apply parses and validates owning callback bytes before staging one Undo entry. Equal canonical
results preserve serial, dirty state and pending Redo. Read-only, invalid/future/oversized sources,
wrong scope/serial/thread and failed operations conserve existing history and baseline. At most
64 transitions are retained across Undo/Redo. Dirty replace/close requires explicit discard;
failed replacement still preserves the old owner. Reopen/close invalidate old serials.

`AcknowledgeSave` checks the exact previous raw source and caller-confirmed published bytes whose
canonical material equals the current document. It advances only the saved baseline and serial,
retaining Undo/Redo. It performs no IO and does not prove filesystem publication or writer authority.
The host checks current project/document/asset/source, recovery, Play state and writer lease before
deferred authoring or publication. File capture/publication belongs to the source session below;
graphical controls and native Preview remain separate work. This owner grants no plugin permissions. Existing file/C/Gameplay schemas,
module dependencies and class layouts remain unchanged; public C++ consumers rebuild.


`MaterialToolSourceSession` borrows a workspace and Content session that outlive it, while
privately owning its material document. Open copies an ordinary bounded `.nmaterial` source and
binds project/root, Content generation and exact asset/path. A nonwrapping process-local owner
identity supplies document-scope generation, preventing observations from crossing owners with
otherwise equal project/asset/serial values; exhaustion rejects. Snapshots remain owning inspection
values after scope loss. Authoring and Save check construction thread before mutable context,
current identity/path, writer access, resolved recovery/external state, no pending reimport, the
host's explicit authoring/Play gate and exact saved raw source. No mutable document borrow escapes.

Reads reject symlink/hardlink/parent aliases and source growth beyond 64 KiB. Save preallocates saved
baselines, rechecks the exact source, uses the common occupied-staging-preserving atomic helper,
confirms actual bytes and refreshes Content's owning material before acknowledging the baseline.
Undo/Redo survive confirmed Save; history never writes a source implicitly. A late confirmation or
Content refresh failure explicitly reports that publication occurred, retaining the dirty owner
and old baseline; retrying that baseline rejects until explicit inspection/discard/reopen. This is
the serialized ordinary-file contract, not hostile filesystem race exclusion or a power-loss
journal. Close can release a stale owner without IO; dirty close/replacement still requires discard.
The source owner invokes no native callbacks, grants no plugin permissions and adds no graphical
controls or GPU Preview. Existing module/file/C/Gameplay schemas and class layouts stay unchanged;
C++ consumers rebuild. Actual file/reopen and disabled-tool acceptance is recorded in
[Linux evidence](../../Tools/Build/evidence/EditorEDM5-MaterialToolSourceSession-Linux-2026-10-11.md).

`ExportMaterial` validates the complete canonical scalar/Renderer schema and produces
at most 1024 bytes of schema-1 source using the classic locale and float `max_digits10`.
It performs no IO/publication and rejects divergent reflection or invalid scalar data.
The optional [scalar material native tool](../../Plugins/ScalarMaterial/README.md) uses
this public codec and `ImportMaterial` for actual bounded Inspect/Edit/Serialize byte
transformations. Edit's public little-endian wire contract is in `ScalarMaterialTool.h`.
GPU Preview remains unavailable. Document history and confirmed source Save/reopen use the
owners above; graphical signed-tool selection and its deferred native application remain open.

`.nmaterial` schema 1 imports opaque linear base color, metallic, roughness, occlusion and emission
from bounded fixed-order tokens; see [ADR-0005](../../Roadmap/en/ADR-0005-Editor-Scalar-PBR-Materials.md)
for the source grammar, numeric bounds and persistent UUID-reference bytes. Indexing and reimport
own immutable `MaterialAsset` snapshots. Workers only stage; live publication rechecks source,
project and dependency revisions. Cancellation, invalid/unsupported source and stale results keep
the previous artifact. Content Undo retains the latest successful material reimport, just as it
retains mesh reimports. Sources are limited to 64 KiB and workspaces to 4096 typed materials.

`MaterialAssetCatalog` is externally serialized on the authoring thread and publishes atomically.
Canonical scalar fields and their exact derived Renderer schema are validated together, rejecting
unsupported shader/profile/features and contradictory reflected parameters. It borrows Content only
for the call and returns owning snapshots; every lookup checks the current
nonzero project generation. Import/catalog operations perform no native GPU work. Its direct
Renderer dependency uses the shared material-validation policy; application drawing uses Renderer
tangent generation and existing Presentation PBR bindings.

AssignMaterialAssets requires the complete selected live Mesh Renderer set, writable Content, an
editable host, live entity/document/project generations and the same owning typed payload in catalog
and Content. It commits one bounded atomic SetOpaqueComponents metadata Undo; the single-object
wrapper follows that same boundary. Identical assignments preserve Redo. ClearMaterialAssets removes
supported references in one metadata transaction, including missing-source/catalog cases, while
protecting unsupported payloads and retaining unrelated data.
The Editor-owned `editor.material.asset` component stores a versioned UUID independently of the
unchanged legacy shader ID; missing resources and unsupported component versions retain all bytes.
Per-frame reference inspection copies only bounded opaque metadata prefixes and requires exact
17-byte length/version, including when unrelated plugins retain large payloads. Unsupported
versions/type-name collisions reject assignment. Scene container and stable C/Zig
schemas stay unchanged. Application-owned Game material snapshots freeze converted values and
mesh-entity assignments before Play; reimport/deletion/reassignment cannot alter that session.
Material catalogs do not borrow Runtime Worlds or contain native resources. Texture/shader graphs
and the shipped-game/cook consumer remain separate work.

## Process-memory capture persistence

`ProfileSession::MemorySamples` borrows an authoring-thread-only bounded history of real process
RSS / working-set observations. Capacity is `min(frame_capacity, 600)`; capacity zero disables
history while retaining latest/peak observations. Fixed storage keeps the noexcept OS sampler free
of allocations. Each attempt records its increasing sequence, elapsed milliseconds from the first
observation since Clear, and optional resident bytes. Failed reads record unavailable, zero remains
a valid byte count, and pause gaps remain in elapsed time. Evictions increment a saturating separate
memory dropped count. Clear resets both histories and origins without resuming Capture. Process
history survives project switches and detachment. UI drawing consumes spans immediately and holds
no borrow between calls.

`ExportProcessMemoryJson` atomically writes `.nexora/process-memory.json` under the project writer
lease. Its independent schema 1 identifies source `NexoraEditor`, metric `process_resident_memory`,
scope `current_process_including_shared_resident_pages`, byte units, millisecond time units and
`first_observation_since_clear` origin. `export_project_uuid` identifies the destination project,
not ownership of process allocations or where each observation was taken. Sample count is numeric;
sequence, resident byte and dropped counts use lossless decimal strings. Unavailable resident values
are JSON null. No OS absolute timestamp, path, GPU data or allocator ownership is persisted.

Export requires 1–600 ordered nonzero sequence IDs and strictly increasing finite nonnegative
elapsed times. Empty/invalid histories, closed/read-only/recovery projects, unsafe metadata or
nonregular/aliased destinations reject before publication. Occupied staging and failed replacement
preserve the previous file and unrelated staging. `ImportProcessMemoryJson` synchronously reads
at most 128 KiB into a separate owning `ProcessMemoryCapture`; read-only observers may import.
The bounded nonrecursive reader requires this exact schema, metadata, current export-project UUID,
count and sample order. Unknown/duplicate/missing fields, corruption, overflow and trailing data
reject without changing source files, a live session or the caller's previous capture. Equivalent
ASCII JSON escapes and reordered fields are accepted. Filesystem checks follow the serialized
host-thread model; concurrent filesystem substitution is outside this contract.

The optional GUI supplies independent Export memory / Import memory / Clear memory import controls.
The application performs all IO and transfers validated owning data. Imported process history stays
separate from live memory and wall-time imports; failed publication preserves it, live Clear does
not erase it, and root/UUID changes or detachment clear it and pending requests. The plot uses elapsed
time horizontally, breaks lines at unavailable attempts and shows MiB as 1,048,576 bytes. Graphical
controls follow project access and modal/recovery/close gates. Existing schema-1 wall-time CSV/JSON
readers/writers are unchanged; Arbitrary captures and physical-host acceptance remain open; GPU traces use a separate schema.

## Retained native mesh memory observations

ProfileSession::ObserveMeshMemory receives copied Presentation diagnostics on the application owner
thread after successful Present: a nonzero process-local surface domain, steady time, actual retained
native allocation bytes and CPU comparison-key payload bytes. Both values are available/unavailable
together; zero is valid. No native handles, GPU reads or module dependency enter Editor core. The
metrics describe the current surface's immutable geometry cache, across project changes, excluding
textures, frame uploads, heap overhead and total device-local VRAM. They do not belong to a project
or replace process RSS/working-set measurements.

Each capturing stream samples at most once per 250 ms with strictly increasing steady time. New
owner domains reset this stream's latest/peak/history/drop/clock state, including while paused;
malformed/zero domains and partial metric pairs reject without disturbing prior state. Unavailable
observations create gaps while retaining observed peaks. Fixed storage keeps min(frame_capacity,600)
records, with saturating independent eviction counts; capacity zero retains latest/peak only. Clear
resets observations and sampling origin without resuming Capture or freeing renderer resources. UI
copies latest values and consumes history spans synchronously; no borrow survives a widget call.
The two-series plot identifies byte units and elapsed time, preserving pause gaps and missing data.

The graphical channel supports independent explicit file actions and owning static views; core
interchange is described below. Existing
wall-time CSV/JSON, process-memory and GPU timing schemas, imports and export controls remain unchanged; their missing fields are not inferred from this scope.
The public C++ ProfileSession layout grows by fixed bounded storage and consumers must rebuild.
Stable C/Zig/scene schemas and module dependencies are unchanged. Whole-device memory instrumentation and physical acceptance remain separate work.

## Retained mesh memory capture interchange

`MeshMemoryCapture` owns up to 600 ordered observations of one native surface stream and its
independent dropped count. `ExportMeshMemoryJson` explicitly writes `.nexora/mesh-memory.json`
under the project writer lease and resolved recovery; `ImportMeshMemoryJson` reads at most 192 KiB
into an owning capture, including for read-only observers. Neither API accesses native resources.
Schema 1 identifies `NexoraEditor`, `retained_mesh_memory`, current native surface retained geometry,
owned CPU comparison-key payload excluding allocator overhead, byte units and elapsed milliseconds
since the first observation after Clear/domain change. The destination project UUID identifies the
export destination only: this retained surface history can span projects. No surface domain,
absolute clock, native handle, device identity, texture/frame-upload bytes, RSS or total VRAM is
serialized or inferred. Existing wall/RSS/GPU capture files remain independent.

Sequences, both byte fields and drops use lossless decimal strings; elapsed time retains finite
double precision. Zero is measured, paired null values are unavailable. Shared validation rejects
empty/over-600 histories, partial availability, zero/nonincreasing sequences and nonfinite,
negative or nonincreasing times. The bounded nonrecursive schema-token reader accepts reordered
fields and equivalent ASCII escapes; wrong project/source/scope/unit, unknown/duplicate/missing
fields, invalid samples and trailing data reject. Occupied staging, unsafe metadata/destination,
failed replacement and recovery preserve previous files and unrelated staging. Failed import
returns no replacement and never mutates previously returned captures. Closed workspaces reject IO;
read-only projects may import but cannot export. All IO is externally serialized on the owner
thread; concurrent filesystem substitution is outside this contract. Sample spans are synchronous
borrows, imports own their storage, and no borrow/native owner survives these calls. Additive C++
APIs require consumer rebuild without changing stable C/Zig/scene schemas or module dependencies.
The graphical host binds scoped explicit file actions and owning static publication to these APIs;
whole-device metrics and physical calibration remain open.

## Completed native GPU profile history

`ProfileSession::ObserveGpuFrame` consumes copied native completion values on the application thread:
a process-local surface domain, explicit Vulkan/DX12/Metal source, software-device flag, increasing
submission ID and optional milliseconds. Domains are neither persisted nor native handles. New
surface/source/software domains clear GPU history rather than merging different streams. Invalid
domains/sources/nonfinite/negative values and duplicate/backward IDs reject. Unsupported sources
remain unavailable. A missing result records unavailable instead of reusing an old success, while
preserving the observed peak. The fixed history retains `min(frame_capacity, 600)` records and
saturates its independent eviction count; zero capacity keeps latest/peak only.

Capture pauses GPU ingestion while native backend instrumentation continues. The owner still
advances the paused completion watermark, so Resume cannot replay that completion. Clear resets
GPU history/latest/peak/drop counts but retains the watermark and pause state, requiring a newly
completed ID before a value returns. The application consumes results after successful Acquire and
before drawing Clear/Capture. GPU history is surface/process-wide across project changes; it is
separate from wall-time frames and process RSS, and the GUI retains only copied scalar observations.
The plot's horizontal axis is native submission ID and missing timings break its line. Native
sources, software status, delayed completion, milliseconds and observed peak are explicit. Existing
CSV/JSON wall-time captures remain schema-compatible and still contain no native GPU values.
GPU trace persistence/import uses its own schema; per-pass tools and physical timing calibration
remain open.

## Native GPU capture interchange

`GpuTimingCapture` owns one ordered native surface stream with its Vulkan timestamp, DX12 timestamp
or Metal command-buffer source, software-device flag, up to 600 copied samples and independent
dropped count. `ValidateGpuTimingSamples` is shared by export, the reader and static GUI publication.
It rejects unavailable/unknown sources, empty/oversized histories, zero/nonincreasing submission
IDs and negative/nonfinite durations. A known source may contain only unavailable results; measured
zero is distinct from null. Source/domain changes start new live histories, so files do not mix
devices. Native surface domain IDs and platform handles are never serialized.

`ExportGpuTimingJson` explicitly writes `.nexora/gpu-timing.json` under the project writer lease
and resolved recovery. Schema 1 identifies `NexoraEditor`, `completed_gpu_timing`, native
command-buffer scope, milliseconds, source/software status and the native completed-submission
axis. `export_project_uuid` identifies only the export destination: the retained process-surface
history may include observations from other projects. Submission/drop IDs use lossless decimal
strings; optional timings are finite JSON numbers or null. No CPU time, RSS, absolute timestamp,
path, display latency or per-pass attribution is inferred. Locale-independent double precision is
preserved. Invalid input, occupied staging, unsafe metadata/destinations and replacement failures
preserve the previous file and unrelated staging.

`ImportGpuTimingJson` synchronously reads at most 128 KiB into an owning capture; read-only observers
may import, while closed/recovery projects and nonregular/aliased paths reject. The shared bounded
ASCII schema-token reader has no recursion, DOM or unknown-value skipping. All three profiler JSON
schemas retain their distinct field contracts; reordered fields and equivalent ASCII escapes are
accepted, but unknown/duplicate/missing fields, wrong source/scope/unit/project, invalid samples and
trailing data reject without mutating files or caller state. IO remains externally serialized on the
host thread, with concurrent filesystem substitution outside the contract.

The GUI's GPU actions are independent one-shot requests consumed by the application owner. Static
import success/failure and Clear GPU import leave live GPU/CPU/RSS and other static imports intact;
live Clear leaves static captures intact. Project root/UUID changes or detachment clear static GPU
state and pending requests. Plots use native submission IDs, break at unavailable samples and label
source/software, scope, units and retained peak. Read-only/modal/recovery/close gates match existing
profiler actions. No capture is saved implicitly. These additive rebuild-required C++ APIs change
no module dependencies or stable C/Gameplay ABI. Physical GPU calibration, per-pass analysis and
third-party capture adapters remain open.

## Semantic scene revision comparison

`CompareSceneRevisions` accepts synchronous base/local/remote byte views and returns owning changed
rows keyed by stable entity IDs and fields. An absent whole source is `nullopt`, distinct from an
empty present file or an existing empty field. `SceneDocument::ReloadBytes` and ordinary file Reload
share one versioned parser and legacy migration; comparison uses only isolated temporary Worlds.
Comparison caps authoring nodes during ingestion before legacy validation/migration, and legacy
hierarchy validation visits each node/edge once. Quaternion signs use one canonical hemisphere;
equivalent q/-q rotations remain equal while independent authored Euler turns stay inspectable.
Runtime entity presence, parent/sibling order, local TRS, camera/light/mesh fields, tracked names,
authored Euler turns and opaque type names/exact hexadecimal payloads remain inspectable. Generation
IDs, selection and Undo history are not persistent source fields. Unknown payloads are never loaded
as plugins or interpreted as a future schema.

Persistent prefab bindings also participate. Additive stable paths `prefabs/<instance>/present`,
`source`, `revision` and `nodes/<target>/present`, `source_node`, `scope` retain complete instance,
asset/revision and source-node-to-target observations. Scope is an ordered slash-separated UUID
sequence; an empty root scope is a present empty string, distinct from an absent mapping. Revisions
and target IDs use exact unsigned decimal strings. Node/placement record order has no semantic
effect. Binding removal versus a changed revision remains unresolved; comparison performs no
source asset resolution, plugin loading, binding rebase or merge publication. Existing parser
limits and aggregate snapshot/value/row/result budgets apply to these fields too; overflow rejects
the complete result. All borrows expire within the serialized call and output owns every value.
This extends the stable-path namespace without changing the C++ layout, ABI or stored scene schema.

Each source is limited to 8 MiB and 4,096 entities; a value to 64 KiB (32 KiB opaque binary), a
semantic snapshot to 4 MiB, and the final result to 131,072 rows/16 MiB. Limits measure logical
source/field bytes, not process RSS or the parser's bounded temporary allocations. Invalid, unsafe
display text, unsupported or over-budget revisions reject without partial output, IO or caller
document/baseline/history mutation. Output sorts stable paths and owns every optional value after
the input views expire. Callers supply project/revision provenance and serialize borrowed access.

Shared/local/remote choices are per-field inspection hints. Deletion versus editing remains an
explicit unresolved conflict. They do not certify a structurally valid merged scene and do not
publish source data. Graphical conflict/provider and reviewed merge publication remain separate
acceptance work. ReloadBytes is a rebuild-required C++ API with the same successful replacement,
generation/history and failed-reload preservation contract as file Reload; file Reload moves its
existing owning buffer into the parser, avoiding an extra full source copy.

Actual contract, capacity, legacy and history preservation results are recorded in
[Linux comparison evidence](../../Tools/Build/evidence/EditorEDM4-SemanticSceneComparison-Linux-2026-10-09.md).
## Owning semantic merge candidates

`SceneMerge::Prepare` copies exact optional base/local/remote bytes, validates their supported
scene formats in isolated Worlds and retains an immutable comparison. Raw absent sources differ
from empty/corrupt present files. `SceneMergeReview` owns raw sources and projected fields; its
comparison and byte views are borrows valid while that review remains alive. Copies share immutable
ownership. Moved-from reviews expose identity0/empty observations and cannot resolve requests.

Schema1 `SceneMergeRequest` owns row choices and the exact nonzero process-local review identity.
Row indices belong only to that immutable comparison; replacing a review invalidates old choices
even for byte-identical inputs. Unknown schemas/sides, stale identities and duplicate/absent rows
reject. Shared/local/remote nonconflict hints apply by default; every unresolved row requires an
explicit Local/Remote choice. No arbitrary manual string value is admitted. Identity saturation
fails closed. Review identities are ephemeral and must never be persisted or treated as write access.

`Resolve` produces actual owning scene bytes or explicit absent-whole-scene intent. Complete
rotation/Euler and each prefab instance binding must match one captured source group, avoiding
hybrid orientation hints or source/version/mapping combinations. Other independently selected
fields must form a complete valid hierarchy with contiguous sibling positions and valid active
Camera/Light data. The builder uses iterative canonical preorder, preserves stored dormant values,
exact unsigned references, names, authored turns and unknown type/name/payload bytes, then loads the
complete candidate and reprojects it. Every selected semantic field must match exactly; incomplete,
cyclic, out-of-budget, normalized-away or discarded fields reject rather than partially succeeding.

Candidate serialization uses existing Runtime3 and Editor2/3/4 canonical writers/parsers. Legacy
inputs use their existing migration; original bytes remain owned for later exact stale-file checks.
No new persisted format, stable C/Gameplay ABI or module dependency is introduced. The new exported
C++ API requires consumers to rebuild. The comparison projection/classification implementation is
shared with the existing read-only API; its source behavior is unchanged.

Inherited logical limits are8 MiB per raw source,4096 entities,64 KiB per value,4 MiB per source
snapshot and16 MiB/131072 rows per comparison. Three owned raw sources and projected source maps
can each reach their respective limits; the selected temporary union can approach12 MiB before
the final4 MiB snapshot check. Final bytes must fit8 MiB. These limits exclude container/allocator
overhead, temporary Worlds and process RSS. Allocation exceptions propagate, with all inputs and
caller Worlds/history unchanged; a private failure seam exercises a live staged-World unwind.

Calls are synchronous serialized snapshot work and can run on an owning worker, with no live
authoring pointer, IO, source publication, native service callback, plugin loading or permission
grant. Drawing/previewing does not commit. A future publisher must independently validate current
project/document/file scopes, writer/owned role, Play/modal/recovery policy and asset/reference
compatibility before staged atomic publication; deletion intent requires its own explicit action.
Cancel/stale jobs discard the entire owning result. Drain calls and destroy reviews, borrowed views
and queued requests before unloading Editor Core. Native plugin unload is independent because no
plugin object/function/table is retained. Full graphical reviewed publication remains open.

## Graphical additive document host

The native application consumes owning tab requests through `AdditiveSceneSession`, independently
revalidating workspace access, source/target generations, reference policy, stopped Play, recovery
and drained exports. Save All uses the accepted immutable batch and publishes every owned saved
Content scene after coordinated success. Close considers every owned dirty document, including
inactive owners behind a clean reference, and preserves unnamed Save As retry continuations.
Composition metadata is saved only after host readers and Play are stopped; failed full restore
retains the bootstrap and freezes source mutation while all foreign data remains intact. Initial
metadata begins with multiple documents and later records a single remaining owner; ordinary
single-scene startup/fallback remains authoritative until a composition exists. Legacy startup
observation initializes before bootstrap. Stable entity identity/opaque references are not rewritten.
The Scene canvas/Hierarchy display the active document. See
[Linux graphical evidence](../../Tools/Build/evidence/EditorEDM4-GraphicalSceneTabs-Linux-2026-10-09.md)
for actual controls, source isolation, reference policy, failure/reopen and legacy compatibility.

## Supported project upgrade inspection and retained originals

`ProjectWorkspace::PreviewUpgrade` synchronously reads supported schema-1/2 descriptor and bounded
workspace data into an owning plan without acquiring a writer lease, creating directories or
writing files. Its copied original retains exact CRLF/UTF-8 bytes; legacy identity derives from the
canonical root/name exactly as ordinary Open. Proposed bytes describe a plan, not publication
authority. Current-schema preview preserves original bytes. Unsupported/corrupt inputs reject.
Descriptor reading retains at most 4 KiB and checks actual read bytes, so post-stat growth cannot
cause an unbounded line allocation. Workspace limits remain 4,096 paths of at most 1,024 bytes.

Read-write Open reinspects under its existing OS-held cooperating-writer lease. Before a legacy
descriptor replacement, `.nexora/project-upgrade.schema1.backup` retains exact original bytes and
`.nexora/project-upgrade.schema1.report` records the immutable schema/UUID/byte-count plan. The
report explicitly says plan-only and is not evidence that publication succeeded. Preexisting
matching dedicated regular evidence supports retry; foreign/corrupt, symlink/hard-link aliases and
occupied staging reject without deletion or replacement. Source bytes revalidate before/between
evidence writes and before descriptor publication. Preparation or replacement failure retains the
source and any successfully prepared evidence; repairing occupied staging permits retry. Successful
reopen retains backup/report and stable UUID without rewriting current-schema descriptors.

Preview and Open calls remain synchronous owner operations; plans survive caller/file changes but
never grant later write authority. Failed Open preserves prior in-memory state and its writer
lease. Source scenes and workspace/layout bytes are not migrated by this descriptor slice. The
cooperating-writer policy does not promise hostile concurrent-filesystem isolation, power-loss
durability, all-file atomic replacement or cancellation of a synchronous call. Broader graphical
migration/recovery remains separate work. Public C++ consumers rebuild; stable C/Gameplay ABI and
module dependencies are unchanged.

## Scoped graphical source-conflict inspection

`SceneComparisonJob` captures owning bounded saved-baseline and prepared-local bytes on the
authoring owner, reads a bounded disk revision and compares isolated Worlds on the referenced
`JobSystem`. Jobs must outlive the owner; shutdown cancels and drains before document destruction.
Workers retain no workspace/document borrows and perform no source writes. One operation blocks
replacement until its terminal worker is consumed. Cancellation, changed document/path/project,
changed prepared local content, changed saved baseline or a replacement file-session identity
reject publication. Ready owns an immutable captured comparison, not continuing disk authority.

The same real worker now prepares a `SceneMergeReview` rather than discarding captured sources
after deriving hints. `SceneComparisonSnapshot::review` retains exact raw base/local/disk versions
and their semantic maps. Its `result` aliases that immutable review's comparison through shared
ownership; there is no second complete comparison projection. Both are published only after the
existing current owner checks pass. Non-ready, cancelled, stale and failed operations expose neither
review nor comparison. Shutdown drains before clearing its current observation; copies already
retained by a caller remain valid captured data. Owners and requests stay on the serialized authoring
thread, and EditorCore must remain loaded until all retained reviews and resolution calls are gone.
Stop/join the JobSystem before unloading EditorCore to cover worker descriptor cleanup after completion.

A retained review can resolve explicit choices after the actual source changes or the worker owner
stops. That result describes captured versions and grants no current file/document authority.
Consumers must validate the complete candidate and recheck live project/access/reference/document/
baseline/source scope separately at publication. The worker retains bounded raw sources and maps
under the existing candidate limits; this intentionally retains more logical data than a hint-only
snapshot and is not a total RSS guarantee. Cancel requests are checked before and after synchronous
bounded parsing; Shutdown waits for that work rather than abandoning borrowed callbacks. The
snapshot's additive C++ layout requires matching consumer rebuilds; persisted formats and the stable
C ABI are unchanged. Graphical field choices and atomic publication remain separate work.

The application polls on its authoring owner, then transfers only owning observations to ImGui.
Reference and read-only documents may inspect through their const file session; no writable
document accessor is required. Stopped Play, live scope and recovery/modal gates revalidate
requests. Neither comparison nor its per-field hints alter selection, history, dirty baselines,
sources or replacement confirmation. Inspectable differences do not certify a valid merged tree.
[Linux graphical evidence](../../Tools/Build/evidence/EditorEDM4-GraphicalSceneConflicts-Linux-2026-10-10.md)
covers actual controls and source isolation. Reviewed merge publication and provider integration
remain open. These additive C++ APIs require rebuild and change no stable C/Gameplay ABI or module
dependency.

## Asynchronous captured merge candidates

`SceneMergeCandidateJob` owns one bounded copied schema1 request and a shared immutable
`SceneMergeReview` on the existing JobSystem. Start/Poll/Cancel/Busy/Snapshot/Shutdown are serialized
owner calls. Workers borrow no document, workspace, file session, module callback or plugin object;
they perform no IO. Review/request capacity is checked before submission, and complete semantic
validation still runs in isolated Worlds. Start allocation failures may propagate; malformed or
failed work never exposes a candidate. A failed idle Start replaces its current observation with
Failed, while an active operation cannot be replaced. Earlier copied completed results stay valid.

The worker checks cancellation before and after bounded synchronous parsing. Poll accepts the
embedding owner's current immutable review identity; mismatch produces Stale and no result. Ready
owns canonical candidate bytes, or an explicit absent whole-scene candidate, and grants no write or
delete authority. A mutex protects copied observations; no UI/authoring callback runs on the worker.
Shutdown cancels/drains before clearing current output, including consumed work. The JobSystem and
EditorCore must outlive the drain and all retained reviews/results; in-process unload is not a
sandbox. Stop/join the JobSystem before unloading EditorCore, since worker descriptor cleanup can
follow its completion notification. Public C++ consumers rebuild for these additive classes; stable C/Gameplay ABI and all
stored formats remain unchanged.

The graphical application binds requests to file token/path, comparison operation, immutable review
and monotonically advancing UI draft. It checks current local bytes at Start and successful
completion, and current stopped Play/project/recovery/export/modal scope independently. Read-only
and reference inspection can validate captured data without obtaining authoring permission. A
captured candidate may later become stale relative to disk; publication must separately recheck
exact raw source observations, writer/reference permissions and current project/document/file scope.
This API provides no publisher, source overwrite, baseline acknowledgement or live document edit.
Existing source/snapshot/output limits measure logical data, not total RSS. Copied choice count
cannot exceed retained row count; Resolve separately enforces uniqueness, index and side validity.

## Bounded scoped build processes

The native Editor forwards copied current-scope process output into owning Runtime Console
records with explicit session scope/operation and receipt time, bounded ASCII escaping and
separate raw-byte loss/record-drop units. The adapter is documented in Apps/Editor; it adds no
runner callbacks, module dependency, persistence, artifact verification or document authority.

`BuildProcess` owns one direct executable/working-directory/UTF-8 argv request on its referenced
Core JobSystem. Owner calls are serialized; the JobSystem outlives the runner and shutdown
cancels/drains before destruction. Workers retain no document/workspace borrows. Absolute native
paths, at most 256 arguments and 32 KiB executable/argv bytes reject invalid admission before
changing the previous observation. There is no shell expansion. The child inherits the host
environment; the caller authorizes its executable and project scripts. This is not a sandbox.

Cancel admits only queued/running work while synchronized with worker outcome publication.
After a terminal outcome is published, Cancel returns false even before owner Poll; completed
exit codes and launch failures remain observable. Shutdown ends the owner scope without
rewriting an already completed failure as cancellation.

POSIX output pipes are close-on-exec at creation on Linux. The built-in build and shader
launchers share a creation/spawn mutex for older Darwin pipe creation. Linux requires glibc
2.34 close-from spawn actions; macOS uses CLOEXEC_DEFAULT and whitelists duplicated standard
streams. Unrelated inheritable host descriptors are absent in the build child. Other POSIX
backends without this isolation report Unsupported. Embedding applications must apply the same
pipe/spawn isolation policy to their own concurrent launchers.

POSIX creates a new managed process group through posix_spawn; hosts must retain ordinary SIGCHLD
wait policy and must not externally reap runner-owned children or change that policy while active.
The direct child remains unreaped until cleanup, reserving process-group identity. Windows starts
suspended, assigns a kill-on-close Job Object before execution and restricts inherited handles to
its output pipe and null stdin. Cancellation, exceptional cleanup and shutdown stop managed
descendants and join the direct child/worker. Deliberately detached POSIX groups are outside this
cooperative policy. Mobile platforms without a backend explicitly report Unsupported.

Merged stdout/stderr retains a raw byte tail, default 64 KiB and maximum 256 KiB, with exact dropped
byte accounting; it is neither UTF-8 display text nor a redacted log. Callers sanitize/redact before
display or persistence. The API persists no commands, output or environment. Failed launch has no
fabricated exit code; native nonzero exits, signals, incomplete output, cancellation and stale
owner scope cannot publish success. Exited means only actual code zero; independently verified
artifacts, target manifest, checksums and reproducible build publication remain host obligations.
The API changes no stable C/Gameplay ABI or module graph; public C++ consumers must rebuild.
## Bounded Git status observations

`GitStatusProvider` uses the existing managed `BuildProcess` and Core JobSystem to query a
canonical working-tree root with a `.git` directory or gitfile. The owner supplies an absolute
trusted Git executable and authorizes repository configuration and the inherited environment;
this is a cooperating local query, not a configuration/process sandbox. Calls are serialized on
one owner thread. Workers retain process inputs only, never project/document/provider borrows.
The JobSystem outlives the provider and every call; Shutdown cancels/drains before destruction.
Release retained captures and all C++ SDK objects before unloading Editor/Core libraries;
Shutdown is not a library lifetime lease.

Refresh launches direct fixed argv without shell expansion. It explicitly selects `.git` and the
current working tree, disables optional index locks, fsmonitor and untracked-cache updates, and
requests porcelain-v1 `-z`, rename detection, normal untracked entries and no submodule recursion.
It performs no fetch/push/checkout/merge/index mutation command. Normal untracked directories may
be collapsed; ignored paths and recursive submodule status are outside this query projection.
Git 2.15 or newer is required by the command flags. Missing tools/repositories reject explicitly;
there is no automatic tool discovery, query on project open or fabricated clean-tree fallback.

`DecodePorcelainV1` follows [Git's version-one format](https://git-scm.com/docs/git-status#_porcelain_format_version_1).
It preserves both status lanes, conflict states, rename/copy destination-then-source order and
owning unquoted filename bytes, including spaces, newlines and non-UTF8 names. UI consumers must
escape names for display and key identities with scope plus original path bytes. These observations
are not filesystem authority, file locks or source-version admission for a later write operation.

Complete output is capped at 256 KiB, 4,096 entries and 4,096 bytes per path. Any dropped output,
malformed/truncated record, duplicate destination, invalid relative Git path, budget or allocation
failure rejects the entire candidate. Empty complete output is an available clean result.
Same-scope last-good captures survive refresh failure/cancellation; accepted results replace one
immutable owning snapshot. Query describes the latest attempt; retained last-good captures keep
their older operation IDs and do not represent a successful new refresh. `Capture(current_scope)` cannot return another scope's data, and
retained snapshots survive refresh, provider shutdown and source-directory removal.

Project/provider replacement requires a fresh nonzero scope; reusing one scope for another root
rejects. Provider-local operation IDs never wrap. Polling a different scope cancels/drains work
and permanently invalidates that operation, even if the caller switches back before completion.
Cancel follows the runner's queued/running admission rule; every accepted query still needs Poll
or Shutdown to drain. Query exposes bounded typed phase/error/exit/loss metadata, not raw merged
stderr for direct UI logging. Rejected intake preserves state. Accepted process intake is followed
only by nonallocating identity publication; later decode/snapshot allocation failures retain
last-good and report failure. New exports are additive C++ APIs; consumers rebuild. Stable
C/Gameplay ABI, module dependencies and persisted project/scene formats are unchanged.

The executable `editor.git_porcelain_parser` and `editor.git_status_provider` tests cover actual
Git in disposable repositories, renamed/Unicode/modified/unmerged files, linked worktrees,
unchanged index/source bytes, failed intake/commands, malformed/flooding native fault fixtures,
queued/running cancellation, stale scope, teardown and retained snapshots. Private one-shot
allocation faults cover intake, outcome copy, partial decode and capture construction with
unchanged last-good, drained jobs and successful retry; they retain no callback/allocator pointer. Missing Git leaves the
native integration test explicitly skipped/unavailable. The native Editor now owns an explicit
project-scoped graphical status view; its UI lifetime contract is documented in the EditorImGui
README. Graphical diff/merge, nested repository discovery, broader external-change routing and
production-scale provider acceptance remain open.

## Bounded read-only Git diff observations

`GitDiffProvider` uses the same managed process/JobSystem ownership boundary as the status
provider. Its serialized owner supplies a trusted absolute Git executable, canonical working-tree
root (including linked-worktree gitfiles), and nonzero project/provider scope. The caller must use
a fresh scope when replacing a root; reuse of the current scope for a different canonical root
rejects. Workers own process inputs only. Shutdown cancels/drains before JobSystem destruction;
release captures and SDK objects before unloading the Editor/Core libraries.

`Compare` accepts one literal repository-relative UTF-8 file path, or two distinct old/new rename
paths, each at most 4096 bytes. Empty, absolute, trailing-slash, empty/dot/parent components, NUL,
invalid UTF-8 and unsupported target values reject before intake. Existing status captures preserve
raw non-UTF8 names, but this UTF-8 process boundary cannot query those names. Missing/untracked
paths can legitimately produce empty output: this is the result of that exact Git request, not a
statement that the repository is clean. The caller separately authorizes any UI selection.

Fixed direct argv selects `.git`/working tree and staged (`--cached`) or working-tree diff, enables
`--literal-pathspecs`, disables optional index locks, fsmonitor/untracked-cache updates, pager,
external diff, textconv, color, rename detection and submodule recursion. A rename pair produces
separate deletion/addition output. No shell, network, index-mutation or patch-application command
is launched. Repository configuration, inherited environment and other Git helpers such as
attribute clean filters remain explicitly trusted; this API is not a process/configuration sandbox.

An owning capture carries the exact request, canonical root, scope and nonwrapping operation ID.
It retains the complete raw merged stdout/stderr (up to 256 KiB), including warnings, binary or
combined-conflict notices. Bytes need not be UTF-8 and can contain NUL. This is not a parsed or
validated patch, a redacted log, an atomic filesystem snapshot or a semantic-merge candidate.
External changes during Git execution are subject to Git's own observation semantics. Consumers
must escape/redact before display or persistence; the service does neither IO publication nor
automatic storage. No diff UI or apply permission is introduced by this SDK slice.

Rejected intake preserves state. Busy/stopped/unavailable/exhausted IDs, nonzero/failed processes,
overflow, cancellation, stale scopes and internal allocation failures are distinct typed outcomes.
Any dropped output rejects the whole capture; an exact 256 KiB result is allowed. Same-scope
failure/cancellation retains last-good with its original request identity, so callers must not label
it as the failed request. A scope mismatch permanently invalidates the running operation even if
the owner switches back before completion. Retained captures own their bytes after later queries
and teardown. New C++ exports require consumer rebuilds; stable C/Gameplay ABI, module dependencies
and persisted formats remain unchanged. The real temporary-repository executable
`editor.git_diff_provider` exercises these contracts; missing Git is explicitly unavailable/skipped.
Linux validation and limits are recorded in the
[Git diff evidence](../../Tools/Build/evidence/EditorEDM4-GitDiff-Linux-2026-10-21.md).

## Reflected opaque-property authoring

`ReflectedInspector` owns a validated catalog and Runtime reflection descriptors. It interprets
explicit portable wire layouts in retained opaque components, never offsets into native plugin
objects. Catalogs allow 64 types, 64 fields/type, 32 fixed elements/field and 32 enum/flag choices.
Names/paths/labels are bounded UTF-8, ranges cannot overlap, expanded property paths are unique,
and schema text is capped at 256 KiB. Boolean uses one byte; integers, enums, flags, entity IDs
and finite binary64 lanes use little-endian eight-byte values. Asset UUIDs use two little-endian
64-bit words; colors have four finite lanes in [0,1]. Unknown padding and other components remain
unchanged. Nested structs use flattened paths; version-one arrays have fixed declared lengths.

`Inspect` returns an owning observation of metadata, generation keys, exact source component
bytes and mixed values for at most 256 targets/1 MiB. `Apply` checks current catalog revision,
selection, complete source bytes, validated values and caller-supplied current authoring authority
before publishing. The owner serializes access; observations retain no World, document or plugin
borrows. `SceneDocument::ApplyOpaqueComponents` stages and validates all replacements together,
then records one atomic Undo/Redo entry. Rejections and no-op edits preserve source/history.
Application/UI callers continue to enforce project writer, recovery and Play authority.
`ApplyFlag` sets or clears only a declared checkbox bit in each target's original mask;
mixed values never copy unrelated bits from the first selection. It shares the same complete
observation revalidation and atomic Undo/Redo path as ordinary property replacement.

Optional project metadata is read from `.nexora/inspector.reflection`; missing means an empty
catalog. Files must be regular, non-symlink and single-linked; actual reads stop at 256 KiB + 1.
The LF header is `NXEDITORREFLECTION 1`, followed by `type <id> "name" <bytes> <field-count>`,
`field "path" <kind> <offset> <elements> <choice-count>` and, where declared,
`choice <uint64-value> "label"` records. Kinds are bool/int64/uint64/double/enum/flags,
vector2/vector3/vector4/color/entity_reference/asset_reference. Unsigned counts reject signed
tokens/overflow; malformed versions, trailing records and invalid layouts reject the whole catalog.
This is cooperating-project metadata validation, not hostile filesystem race isolation.

Project activation and explicit metadata reload replace interpretation; failures clear the active
catalog and preserve scene bytes. The host never rewrites metadata. Dynamic structural controls,
arbitrary native plugin object restoration and runtime execution remain separate work. Public C++
consumers rebuild; stable C/Gameplay ABI and module dependencies remain unchanged.

### Version-two counted reflected arrays

`NXEDITORREFLECTION 2` keeps version-one fixed-field records and reads
`type <id> "name" <prefix-bytes> <fixed-field-count> <array-count>`. After the fixed fields,
`array "path" <kind> <count-offset> <maximum-elements> <choice-count>` and its declared choices
specify each dynamic tail. Counts are little-endian unsigned64 in nonoverlapping eight-byte
prefix slots. Prefix padding stays opaque. Tail elements use the existing explicit primitive/
vector/color/reference encodings and follow the fixed prefix in schema order, with no native
container header, implicit alignment or trailing padding. Multiple tails may have different
lengths in each selected component; all following offsets are derived per source. Malformed,
extra/truncated, oversized or invalid-valued tails remain entirely opaque and cannot be edited.

Version1 remains readable unchanged, with no dynamic declarations or guessed conversion.
Version2 is an explicit opt-in schema/wire contract; the host never migrates unknown component
payloads by guessing an old layout or writes a metadata file. A provider/user supplies matching
version2 data and schema. Existing Scene opaque serialization persists all bytes through its
existing atomic save path; no Scene format, native object ABI or module dependency changes.
Unsupported metadata versions fail closed without altering scene payloads. New C++ record members
and APIs require rebuilding consumers; release owning catalogs/observations before SDK unload.

The combined fixed-field/array count is64 per type, each array admits at most32 typed elements,
and existing64-type/256KiB schema limits apply. Each complete observation and each replacement
batch admit at most256 targets/1MiB source payload; individual components remain within the
existing1MiB unknown-component limit. These logical data bounds are not total process/RSS quotas.
`Inspect` owns complete exact sources and count/mixed observations, validates even elements outside
multi-selection's common index intersection, and exposes only common editable element properties.
Value/flags edits calculate each target's actual offset and retain all other masks/data.

`ApplyArray` supports explicit valued Append/Insert, Remove, Move and Clear. Indices must be valid
in every target; Append/Clear use each target's own length. The current catalog, complete source,
selection, generation and explicit authoring authority are rechecked before staging every complete
replacement and one `ApplyOpaqueComponents` publication. Invalid values/indices, stale observations,
read-only authorization, corrupt data, budgets or allocation failure preserve bytes/history.
No-op operations preserve pending Redo. Opaque history admission now owns the new Undo entry before
clearing Redo or swapping live nodes, so an allocation failure cannot erase the previous Redo.
Private thread-local one-shot faults cover pre/partial replacement and history admission without
retaining callbacks/allocators. All calls remain serialized on the authoring thread; this synchronous
foundation introduces no worker, native plugin execution or retained document pointer.

The public-header [counted reflection SDK sample](../../Samples/Editor/README.md) demonstrates real
append/edit/reorder, stale/authority rejection, one-step Undo/Redo and scoped Save/fresh read-only
reopen, preserving reserved bytes and an unavailable provider's payload. It creates and cleans
only its own temporary project; it is available without the graphical Shell when the Editor module
exists. Its feature/module dependency and Shipping/headless stripping are explicit.

Core tests cover every supported element kind, mixed lengths and following-tail offsets, exact32
capacity, invalid/outside-common values, malformed metadata/counts, stale source/catalog/selection/
reload, allocation rejection with preserved Redo, unknown prefix/components and fresh-World file
reopen. Structural Add/Remove/reorder widgets, native UI workflow and full M2-03 remain open.

## Cryptographic extension verification

`ExtensionTrust` owns at most 64 host-configured public publisher keys. IDs are bounded
case-sensitive ASCII identifiers; manifests must never enroll their own keys into this
trusted registry. Set/rotation/revocation update a nonwrapping revision; repeated identical
configuration is idempotent. Rejected input preserves previous keys. Snapshot owns a copy.
All registry calls are serialized by the caller. Zero/identity key shapes reject; hosts
select authentic RFC8032-generated publisher keys through their separate trust workflow.

Verify uses the declared Cryptography module to verify the actual signature over the
exact supplied artifact bytes, then computes an owning SHA256 digest only after success.
No caller-supplied signature-valid flag is accepted. Unknown publishers, changed keys/data,
invalid signatures, over-budget inputs and unavailable/failing backends cannot verify.
The result includes the captured trust revision; it is an observation, not code-load
authority. Later loading must independently validate manifest/ABI/permissions/dependencies,
recheck trust revision and bind verified immutable staged bytes to the native load. This
slice installs no files, persists no private keys, makes no network calls and loads no
plugin. The older ExtensionPolicy boolean predicate remains a portable policy primitive;
it is not the cryptographic verifier or a production load gate. Graphical installation,
trusted staging and pre-load enforcement remain separate unfinished work.

## Scoped prefab property review and revert

`PrefabDocumentSession::Review` returns an owning comparison against the exact retained base
revision for a variant, or the last published revision for a regular prefab. Read-only inspection
is permitted. Unsaved new node/field identities require explicit Save before review; no temporary
stable identities are fabricated. Candidate restoration requires identical stable node sets and
retains target scene/entity identity. Base/nested metadata rows are visible but never rewritten
by a document-property revert. A clean draft without a published source cannot authorize review.

The opaque review records project UUID/root, session generation, asset/previous publication,
current PreparedSave and exact immutable source. Revert rechecks each observation, writer authority,
recovery/external-change state and actual retained source before one complete-property transaction.
Unauthorized, stale, replaced, corrupt/changed source or incompatible shape rejects without live
mutation or IO. The original scene and stored prefab files remain unchanged until explicit Save.
One Undo/Redo restores complete mixed properties, including unavailable bytes; generation, keys,
selection, clipboard and saved baseline survive. Review grants no write authority. Calls serialize
with all owners/project publication; the native host also requires stopped Play and no conflicting
modal/export/close operation. Public C++ consumers rebuild.

The graphical isolation panel exposes explicit Review changes, owning clipped source/edit rows,
and a separate confirmation before full property revert. Scope changes and successful local edits
revoke displayed review/confirmation. Read-only scopes retain Review but disable Revert. Existing
create/open/variant/save/close and dirty-close protection remain. Targeted field revert, apply to
source, structural reconciliation, persistent live instance bindings and reference rebase remain
separate work. These bounded owning snapshots are not a total history/process memory quota.

## Combined prefab property history

Ordinary property snapshots retain the current isolated prefab base reference. Isolated-reference
and live-placement snapshot APIs share one staged transaction helper: the former changes the base
reference, while the latter changes only the selected placement revision and owned properties.
Undo/Redo restores both metadata families together; a no-op property snapshot preserves pending
placement Redo. Unrelated placements, stable IDs, generation keys, hierarchy order, latent authored
Euler and opaque metadata remain intact. Source files and saved baselines change only through the
existing explicit publication owners.

`PluginManager::SelectTool` returns owning package/scope/configuration identity, not a service
pointer or permission grant. `InvokeTool` rechecks the manager instance, project writer, enabled
package and current permissions before synchronous owning-byte invocation. Owner-thread and
reentrancy checks precede manager inspection. Disable, reconfigure or detach invalidates retained
selections; callers independently validate document/Play/consent policy before authoring.

## Targeted stable-field prefab property candidates

The selection overload of `BuildPrefabPropertySnapshot` accepts bounded stable node/field UUID
pairs. Both complete sources and exact node sets are validated first. A selected identity must be
known in at least one source; shared property names require matching field identities. Duplicate,
unknown, mismatched, unsupported reserved metadata or incompatible resulting hierarchy rejects
the entire owning candidate. Empty selection returns a canonical current-source no-op.

Names, position/rotation/scale groups, authored Euler, parent/sibling groups, complete Camera/Light/
Mesh presence and stored data, and exact unavailable component records are independently selectable.
Selected absent-source unknown components are removed; unselected unknown bytes remain intact.
Inactive component stored values are retained through official canonical Runtime property groups,
rather than reset through component-disable setters. Typed staged hierarchy commands retain local
TRS, validate the complete selected parent graph and then apply sibling positions. Unselected
parent/TRS/component/name data retain their current values; selected moves naturally affect sibling
indices. Source rotations retain their authored Euler records, while other selections preserve the
current rotation/Euler. Target scene identity and existing serialized entity IDs remain unchanged.

Supported canonical Runtime version-three record groups are parsed from official serialization;
changed/unknown record framing fails closed. Document metadata stays owning and opaque records are
never interpreted as native objects. Final output is officially parsed/prepared again. Existing
4096-node,32768-selection and8MiB scene/output budgets apply; these are logical budgets, not total
process/history memory quotas. Planning performs no IO, live edits or history/baseline changes and
grants no authority. The owner still rechecks its current document/project/source before atomic
application. Graphical selected-field binding, source apply, structural identity reconciliation and
reference rebase remain separate work. Public C++ consumers rebuild; stable C/Gameplay ABI is unchanged.

## Graphical selected prefab property restoration

`SelectReview` derives an owning targeted candidate from the unchanged private review, explicit
stable node/field identities and the exact retained source. Unknown, empty, duplicate, stale,
changed/corrupt source, replaced scope, recovery/external-change or unsupported selections reject.
Read-only inspection can prepare a candidate but never apply it. Application rechecks writer
access and all original observations before the existing one-step property transaction.

The isolation comparison exposes field-group checkboxes; component/transform rows sharing one
field UUID select the complete group. Metadata references remain inspection only. Changing a
checkbox revokes prior full/selected revert consent. Review selected prepares a new candidate,
followed by separate selected-revert confirmation. Scope changes, local edits and Undo/Redo revoke
review/confirmation. Unselected values, unavailable bytes, generation, keys, clipboard and original
scene/source files survive; explicit Save remains the publication boundary. Source apply, reference
rebase, structural reconciliation and persistent live instance bindings remain separate work.

## Explicit variant property application to source

`PrefabPropertyReview::CanApplyToSource` is an inspection hint for compatible saved variants.
`ApplyToSource` independently rechecks writer authority, root/UUID/owner/current document, actual
variant publication and the exact current source. Regular prefabs use Save; unsaved variants cannot
apply because advancing their base would invalidate first-save preconditions. Stale source or
variant publication, invalid shape/identities, recovery, external change, exhausted revisions and
unsafe/staged publication reject without changing current source or isolated document.

Full/selected source candidates invert the stable UUID remapping while preserving source scene/
entity/node/field identity, references and all unselected values. Publication advances one source
revision and retains the previously committed source. Failed current publication may retain a
harmless old archive, never an uncommitted future revision. Local variant content, baseline, dirty
state, keys, selection, clipboard and history stay unchanged; the variant keeps its retained base
until explicit Rebase. Source publication has its own revision history and is not a local document
Undo entry. Original scene files stay unchanged. No structural reconciliation is claimed.


## Graphical three-way prefab rebase integration

The graphical host retains the private owning rebase review independently of property revert/source
apply. Review/Prepare use exact old/current closures; conflict choices never grant authority. Explicit
confirmation applies one properties/reference transaction, while wrapped Save remains the sole file
publication boundary. Project or retained-reference scope changes and local editing revoke consent;
blocked authoring cannot revive a previous confirmation. Read-only latest-source inspection remains
available without write controls. Actual 1x/2x controls and X11/Vulkan conflict choices, Save/Undo/Redo/
reopen preserve exact archived versions and original source/unknown bytes. Structural identity and
persistent nested/live-instance reconciliation remain separate work.

## Persistent scene prefab placement bindings

`PrefabAssets::InstantiateBound` retains the same complete graph/staging/one-import contract and
adds one owning Editor placement UUID, exact root source UUID/revision and scoped stable-source-node
to serialized-target-node mappings. The immutable binding snapshot and imported Runtime forest share
one Undo/Redo. Every result/metadata/index allocation precedes the live import; IDs are translated
without allocation afterward. Existing document keys, selection/clipboard policy and unknown bytes
retain the forest-import contract. Project/source authority belongs to the serialized authoring owner.

Scene format4 serializes nonempty bindings before `world`; legacy1/2/3 remain readable and ordinary
unbound output keeps2/3. The format rejects nil/repeated placement identities, missing/duplicate live
node targets, repeated scoped source keys, signed/overflowing counts/revisions, corrupt ordering and
excess scope/node/byte budgets before replacing the live document. Limits are128 placements,4096
mapped nodes total,32 scope levels and4MiB binding records; existing64MiB scene bounds also apply.
These are logical data bounds, not total process/history memory quotas. Source revisions are owning
references, not read-time IO or write authority; the caller resolves/rechecks actual source closure.

Rename/TRS/component edits and same-identity property snapshots preserve exact bindings. Deleting
any mapped node detaches the affected whole placement before publication, with Undo restoring the
complete binding; remaining nodes become ordinary nodes. Paste/Duplicate create detached copies.
Scene Save/Save All include bindings, saved signatures detect metadata changes and clean reopen uses
fresh generation keys. NewScene clears bindings. Runtime scene capture excludes all binding records.
Prefab asset capture/validation rejects bound scene bytes until explicit nested identity reconciliation
exists, avoiding silently copied live binding context. Graphical instancing and structural/nested
reconciliation remain separate work; public C++ consumers rebuild, stable C/Gameplay ABI unchanged.

`ProjectPrefabPlacement::Prepare` owns an immutable observation of the open project root/UUID,
current published root asset, exact resolved transitive closure and target prepared content. It may
run for a read-only project and grants no write authority. `Instantiate` rechecks current project,
recovery/external-change state, target content, root publication and every retained exact source
before requesting one bound import. A valid same-revision source replacement invalidates the review.
The host must serialize authoring owners and repeat Play/modal/target-document role policy at the
explicit action. A nonnil placement UUID and current writable project are required. Rejection
preserves target history/clipboard/files; success writes nothing until ordinary Scene Save. This
is a synchronous source observation, not a hostile-filesystem lease or multi-file atomic IO promise.

## Reviewed live prefab source rebase

`PrefabPlacementRebase::Prepare` explicitly reads the exact retained and newer current source
closures and captures an immutable owning three-way review. Read-only projects can inspect.
Stable scoped node/property identity, dependency references and parent structure must remain
unchanged. Structural additions/deletions, changed nested/base references and incompatible sources
reject the entire review. Rendering must not call preparation or source IO implicitly.

Rows compare retained, local and published values in complete name, position, rotation with authored
Euler presence, scale, Camera/Light/Mesh presence and dormant values, or opaque-type name/bytes
groups. Source-only changes apply automatically; local-only changes and identical edits survive.
Different local and source edits to one group require an explicit KeepLocal or TakeSource choice.
Different lanes of the same group still conflict. Unknown locally added components survive without
fabricated stable source UUIDs. Repeated nested node UUIDs retain their distinct exact scope.

`Resolve` is a pure computation against an owning immutable review. Indices identify actual conflict
rows only; duplicate, out-of-range, nonconflict and invalid decisions reject. Unresolved choices
produce no applicable candidate. Row borrows expire with the last owning review. A resolved candidate
does not authorize a write. `Apply` repeats current writer, project, target and both source-closure
observations before one `ApplyPrefabPlacementSnapshot` transaction. Stale or replaced publications,
missing/tampered archives, recovery/external changes and unauthorized/read-only inputs reject.
The caller serializes authoring owners and repeats current selection, Play and modal consent policy.

One Undo/Redo restores complete properties and retained revision together. Stable live generation
keys, selection, clipboard, unbound values and saved baseline survive. No source or scene file is
written; explicit Scene Save persists the result. Source snapshots/candidates fit 8 MiB/4096 nodes;
semantic snapshots fit 4 MiB with 64 KiB individual values; reports fit 32768 groups/16 MiB logical
data. These limits do not certify total process memory or production-scale performance. Graphical
conflict choices, structural reconciliation and apply-to-source remain separate work. Public C++
consumers rebuild; stable C/Gameplay ABI and module dependencies remain unchanged.


## Owning live prefab instance property review

`PrefabPlacementOverrides::Prepare` owns a validated placement inspection and explicitly
materializes its complete exact retained nested closure in a temporary World/document. It translates
scoped source node identities to live generation keys before comparing semantic name, TRS, authored
Euler, component presence/stored values and opaque name/bytes. Retained source property UUIDs accompany
individual semantic lanes. Newly added local opaque fields have no fabricated retained property UUID.
Parent differences are explicitly structural; absolute sibling indices and scene-global fields are
excluded. This read-only review does not authorize structural reconciliation or writes.
Drawing must not call it implicitly; apply and rebase remain separate operations.

Read-only scopes can review. Missing/corrupt/incompatible retained sources, stale node/target/project,
recovery/external changes and budgets return no partial review. Exact owning source/target scope is
rechecked after preparation; `Matches` repeats it before consuming a captured review. Caller serializes
authoring owners and repeats current selection/Play/modal policy. No file publication, saved-baseline,
selection, clipboard, Undo or Redo mutation occurs. Row span borrows expire with their owning review.

Whole-scene semantic comparison budgets apply: each input fits 8 MiB/4096 entities, snapshot fits
4 MiB, and individual value fits 64 KiB (opaque payload hex consumes twice its byte size). Reports
retain at most 32768 changed rows/16 MiB of logical field/scope/value data. Rejection preserves large
uninterpreted data, rather than silently omitting it. These logical budgets do not limit total
temporary World/container/process memory or claim large-scene review performance. Module dependencies
and stable C/Gameplay ABI remain unchanged; public C++ consumers rebuild.

`PrefabPlacementOverrides::Revert` consumes that immutable current review with an explicit host
authoring authorization and the current project writer. It restores the entire placement's
supported properties from its exact retained nested closure, even when a newer root is published.
Any structural parent difference rejects the entire operation. Source materialization and target
reconstruction happen in temporary Worlds/documents. Authoring names, Euler hints and exact opaque
bytes come from the retained source; canonical version-3 Runtime property records transfer TRS,
presence flags and every stored Camera/Light/Mesh value, including dormant nondefault values.
These records consume only bounded `World::SaveScene` output, not arbitrary external input; schema
changes reject. Live IDs, parent relationships, sibling order, generations and placement bindings
remain target-owned. Source and target snapshots each fit the existing 8 MiB/4096-entity budget.

Immediately before one `ApplyPropertySnapshot` publication, the writer, exact retained/current
sources and complete target observation are rechecked. Failure publishes no partial live state.
One Undo/Redo restores the entire transaction; selection, clipboard and the scene's saved baseline
remain intact. A semantic no-op preserves pending Redo. Neither source nor scene files are written;
ordinary explicit Scene Save persists the result. The caller serializes authoring operations and
repeats current Play, modal and selected-document policy when supplying authorization. Read-only,
foreign/stale, recovery/external-change, missing/changed source and unsupported structural inputs
reject. These Core operations do not implement graphical confirmation or structural
reconciliation, apply-to-source or source rebase.

`RevertSelected` borrows row indices only during its serialized call. Indices identify rows in
the same immutable owning review; duplicates, out-of-range indices and unsupported/structural
review state reject atomically. Selected semantic lanes expand to complete scoped canonical
property groups: name, position, rotation including Euler hints, scale, complete Camera/Light/Mesh
presence and stored values, or an opaque type's name/bytes. Distinct selected rows in the same
property group coalesce; selecting an added local opaque field removes that whole source-absent
component without inventing a retained property UUID. Source-to-live scoped identity is resolved
from the captured complete closure. Unselected groups and other nodes retain their local values.
The generated canonical Runtime3 record bridge requires exactly19 property tokens and rejects
schema changes. Writer/source/target validation, byte/entity budgets, one Undo/Redo, no file writes
and saved-baseline guarantees are the same as whole-instance Revert. An empty valid selection is
a no-op that preserves Redo. Targeted graphical selection remains separate.

## Logical Play keyboard/mouse user routing

`PlayInputBindings::kMaximumUsers` bounds the native Editor's logical selection to four users.
The profile remains one shared owning keyboard/mouse mapping with unchanged persisted schema.
The native owner and Game panel route one source to a session-only selected lane; callback scope,
focus revocation, neutral unselected snapshots and reset behavior are documented in Apps/Editor.
No device enumeration, simultaneous-device ownership or stable ABI extension is implied.


## Validated hierarchy snapshot metadata indices

`SceneDocument::Nodes()` reuses an ID-to-owned-metadata-slot index after validating current size,
IDs and slot order. Same-count replacement/permutation triggers a fresh-map rebuild; stale keys/
capacity are released on publication. Cache entries own only integers, with no metadata, Runtime,
native or returned-view borrow. Returned vectors/scalars follow current Runtime storage/parent
order and current names/entity/document generations. Names remain document string-view borrows;
consume them synchronously before relevant document mutation/destruction. Untracked Runtime entities
remain omitted, and snapshot observation changes no authoring data/history/baseline. Calls remain
serialized on the authoring owner thread, including const observations of the private memo.

Storage scales with the last validated metadata input until rebuild; fresh-map failure preserves
old memo/document/World. O(n) validation/output traversal and other frame work remain; this reduces
repeated hash-node allocation, not total process memory or a constant-work production frame budget.
The private SceneDocument C++ layout grows and consumers must rebuild. Stable C/Zig/scene formats
and module dependencies are unchanged. Scoped CPU/100k ImGui evidence does not certify native GPU,
production asset scale, total memory or physical/long-duration soak budgets.

### Fixed local diagnostic admission

`TelemetryConsent` is a serialized, session-owned local queue with no IO, persistence, network
transport or plugin callback. `Record(LocalDiagnosticEvent::FramePresented)` retains only the
engine-owned `frame.presented` label. Unknown enum values reject. The existing `Record(std::string)`
signature remains available but now accepts only that exact label: arbitrary UTF-8 text, paths,
commands, names, source snippets, case/whitespace variants and embedded NUL reject before retention.
Existing callers that recorded arbitrary strings must migrate to supported typed events; this is
an intentional admission-policy restriction. The compatibility call still constructs its by-value
argument at the caller; typed intake carries no caller text. Existing C++ symbols and class layout
are preserved; the overload adds a C++ export and matching consumers rebuild. Stable C/Zig ABI,
data formats and module dependencies are unchanged.

Default-off consent, 1,024-record capacity and no-eviction behavior remain. The former 1,024-byte
constant remains source-compatible, but no arbitrary payload size is admissible. Accepted labels
are copied into owning strings; allocation failure in typed intake returns false with the prior
queue intact. `Events()` borrows until mutation/destruction, so callers must copy for longer
lifetimes. Opt-out clears all logical records; re-enable starts empty. Clear/project/close/restart
retain existing UI lifecycle rules. This is data minimization, not pattern-based secret detection
or secure erasure of allocator memory. Complete extension trust and physical privacy acceptance
remain separate goals.


## Selected live prefab source publication

`PrefabPlacementOverrides::ApplySelectedToSource` synchronously publishes selected property groups
from an owning immutable live-instance review to its exact retained/current root source. Calls run
on the serialized authoring thread. The project writer, target generations/content, complete source
closure and selected stable node/field identities are rechecked before publication. The host repeats
owned-document/selection, Play/modal/recovery and explicit confirmation policy through `authorized`.
Read-only or foreign/stale requests convey no write authority.

This operation supports one non-nested source, including a variant whose retained base remains
unchanged. Selected semantic lanes coalesce to complete name, position, rotation plus authored Euler,
scale, Camera/Light/Mesh presence and dormant values, or existing opaque-type name/bytes groups.
Source scene/entity/node/field identity, parent/sibling order, references and unselected values remain
source-owned. New opaque fields requiring new stable identities, nested/multiple source scopes,
structural parent differences, exhausted revisions, empty/invalid/duplicate selections and no-change
candidates reject. Selected opaque removal deliberately removes that field from the new source;
its exact old bytes remain archived. Such field-schema deletion still requires future structural
reconciliation before existing property-only rebase can advance an old instance; it is not silently
accepted by rebase.

Complete candidate/result storage precedes source IO. Capture and official validation cover the
whole candidate; publication reuses `PrefabAssets::Publish` with the exact expected source and its
old committed revision archive. Admission/staging failures preserve current source and live state;
an already committed old revision may remain archived after a failed current-file replacement.
Candidate allocation exceptions (including the private one-shot seam) precede source IO. The shared
publisher verifies the installed file after replacement: failed readback or an allocation exception
during that confirmation can leave the new current revision installed without a confirmed result.
The old committed archive remains the recovery source. Callers must inspect current/archive state
before retrying; an empty result is not proof that replacement never happened. Other IO failures
follow the existing prefab publication contract.
No callback/provider/native pointer is retained. Returned assets and reviews own their data and must
be released before Editor/Core library unload; they grant no lifetime lease or future authority.

The live scene, binding revision, generation keys, selection, clipboard, dirty/saved baseline and
Undo/Redo are untouched. Source publication has its own revision history, not a local document Undo
entry. A subsequent explicit existing property-compatible rebase advances the binding as a separate
one-step Undo transaction. Scene files change only through ordinary explicit Scene Save. This is
one-source publication, not a multi-source/document atomic transaction or hostile-filesystem lease.

Existing logical limits apply: at most 32768 selected/report rows, 16 MiB report data, 8 MiB/4096-node
source and target scenes, 64 KiB individual semantic values and bounded prefab codec output. Temporary
Worlds, old/new captures, containers and allocator overhead coexist; these are not total process
memory quotas. New C++ exports require consumer rebuilds; stable C/Gameplay ABI, module dependencies
and persisted formats are unchanged.

The real-project test covers plain/variant selected groups, dormant component storage, unknown-byte
edits/removal and exact archives, explicit rebase/Undo, read-only/foreign/stale/structural/nested/max
revision rejection, invalid/bounded selections, recovery, same-revision replacement, occupied staging,
allocation failure, source reopen and live/file/history conservation. Final focused product test
passes in 0.18 s. The strengthened complete Linux gate at main `8bf8b411` passes 307 registered/295 actual passes/12
environment skips with zero failures (659.01 s); all ten current code/header/test/CMake/helper hashes
remain unchanged. Direct positive cases cover component presence, Light values and U64 mesh/shader
references alongside dormant values. The earlier 305-case gate remains historical.
Minimal Shipping builds nine integration steps and registers no tests. Normal integration of accepted main `8bf8b411` preserves all six source hashes; the source
operation plus old/new authoring widgets pass 5/5 in 17.76 s against that combined product. Final
bilingual evidence is included; applicable exact-head hosted acceptance remains pending; graphical confirmation, nested
publication, structural rebase and full M4-01/ED-M4 remain open.

### Scoped file-only semantic merge publication

`SceneMergePublisher::Prepare` re-resolves a schema-1 review/request and returns an opaque owning
one-shot publication capability for an existing named `SceneFileSession`. It captures exact raw
base/local/disk/candidate bytes, project UUID/root, file-session identity/token, managed path and
prepared live document. It retains no workspace/document/module callback or pointer. All calls,
including shared copies of a capability, are serialized by one authoring owner. The caller must
supply current owned-document, stopped-Play/modal/recovery authority on Prepare and Publish;
writer lease, workspace recovery/external change, blocked path, baseline, document and disk bytes
are independently rechecked. Reference-document sessions remain const at the host boundary.
Only supported present candidates and existing regular nonaliased sources are admitted; scene
creation/deletion, project-wide reference validation and graphical confirmation are separate work.

Publish consumes every shared copy, even on rejection/cancellation. It reserves an exclusive
content-addressed directory under `.nexora/scene-merge-history/<SHA256(manifest)>`, writes and flushes
exact `base.scene` (when present), `local.scene`, `disk.scene`, `candidate.scene` and `manifest.bin`,
then rereads and revalidates the entire archive before staging source bytes. Existing archives are
reused only after exact provenance equality; occupied/incomplete/corrupt archive or temporary paths
reject without overwriting them. No automatic cleanup or blind retry removes crash residue.

Archive schema 1 is a bounded little-endian binary manifest: `NXMERGE1`, uint64 schema/project-high/
project-low, uint64-length-prefixed UTF-8 relative path, four presence/length/SHA256 records in the
order above, choice count, stable-path/Local-or-Remote records, then a 64-character SHA256 of the
preceding manifest bytes. Every number is uint64 little endian; each digest is 64 lowercase ASCII
hex characters and each source digest record is length-prefixed. The directory name hashes the
complete manifest including its trailing digest. Limits are 8 MiB per raw source, 16 MiB manifest,
131072 choices and 48 MiB aggregate logical archive bytes (not total process memory). An unavailable
cryptography provider explicitly rejects. SHA256 verifies integrity, not author identity or trust.
`Inspect` is read-only, including in read-only projects: it validates identity/version/lengths/
checksums/regular files, remaps stable choice paths and resolves the candidate again in isolated
Worlds. Unsupported versions, corrupt bytes and incomplete archives remain untouched; no automatic
migration interprets a future version as schema 1. A future writer needs a new version and explicit
migration policy.

Source staging uses native exclusive create, bounded chunks, flush/close and exact reread, followed
by current-scope/raw-byte rechecks and cancellation immediately before one native replacement.
POSIX uses rename plus directory fsync; Windows uses MoveFileExW with WRITE_THROUGH (there is no
portable Windows directory-fsync guarantee). There are no allocations or callbacks after the
native commit starts. Rejected/Cancelled mean replacement was not attempted; Published confirms
replacement, PublishedRecoveryRequired means replacement succeeded but directory durability was
not confirmed, and PublicationUncertain means native replacement did not confirm success. Inspect
archive and source before recovery/retry in either latter case. These are cooperative-writer
operation-time checks, not filesystem compare-and-swap, an adversarial sandbox or a promise that
blocking OS IO/fsync can be interrupted. Cancellation is observed between bounded IO chunks and
before commit; successful replacement is never relabeled as cancellation.

Publication deliberately leaves the live World/document, selection, Undo/Redo, dirty state and
file-session save baseline unchanged. Ordinary Save still detects the resulting external change;
explicit reload/adoption and integrated graphical publication remain pending. Caller sessions must
outlive synchronous calls; EditorCore stays loaded until all capability copies are destroyed and
all calls drain (their shared control-block deleters are compiled in Core). Owning preparations
retain no session or native-provider borrow; plain archive data owns its values. Moved-from
capabilities return empty paths/bytes, report consumed and reject publication.
Allocation/IO validation errors before replacement may leave owned staging residue for inspection,
but preserve the original source and last-good archive. The private precommit scheduling gate is
compiled only with BUILD_TESTING and retains no native callback in production.
