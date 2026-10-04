# Editor Core contract

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

- `ProjectWorkspace` owns its descriptor, stable project UUID, open-document list, and (for
  read-write access) one OS-held writer lease on `.nexora/editor.lock`. A second writer fails with
  the owning process ID while any number of explicit read-only observers may coexist. The lock file
  is metadata, not the lease: the kernel releases the actual lock on normal close or process death.
  Schema-1 descriptors remain readable; a read-write open atomically upgrades them to schema 2 and
  persists their derived UUID, while a read-only open reports `Required` without changing the
  project. A read-write open of a schema-1 project that predates the `.nexora` directory creates it,
  but only for a root that already holds a project descriptor. All project-owned writes reject
  read-only workspaces. Workspace files are atomically replaced (the temporary file is flushed and
  checked before it replaces the old one), a recovery journal is written before the primary
  workspace file, and successful save/recovery removes that journal. The UI may query and explicitly discard a pending journal.
  Versioned Editor layout payloads are persisted separately and never use Dear ImGui's unmanaged
  global ini file. Gameplay library selection is an independent `.nexora/gameplay-library.ini`
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
  Virtual ranges borrow item pointers until the next mutation. Rename, multi-item move, and delete
  validate a complete replacement snapshot before committing and retain one undo snapshot.
- `ProjectContentSession` owns the live browser model, dependency/conflict state, canonical project
  root, project generation, and one recoverable filesystem mutation. The application owns the
  session; UI code borrows it for a frame and never retains `ContentItem` pointers. Rename and move
  use same-volume filesystem renames after validating a candidate model. Delete moves files into a
  unique project-local `.nexora/trash` operation directory, and undo restores both files and model.
  In persistent-identity mode, each source and its `.meta` sidecar are one rollback-capable
  transaction, so rename, move, delete, and undo cannot detach the UUID from the source. Existing
  files and symlinks outside the canonical project root are rejected before mutation.
- Typed asset drag payloads carry the project generation and asset UUID. Reimport results are staged
  and may publish only when their generation and dependency graph remain valid; cancellation,
  staleness, failure, or a cycle preserves the previous artifact. `ProjectContentSession` also keeps
  a newer reimport artifact in the pending undo snapshot, so undo cannot resurrect stale metadata.
- `AssetImportQueue` owns generation-tagged workspace-import and reimport jobs submitted to an
  application-owned `JobSystem`. Its worker state retains bounded progress and diagnostic histories
  (stable code, severity, message, path, and asset context); overflow is counted explicitly.
  Workspace results and reimport artifacts remain staging data until the authoring thread calls
  `TakeResult` or `ProjectContentSession::PollReimport`. Reimport publication revalidates project
  generation, asset path, previous artifact, settings identity, source-file revision/size, and
  dependency revision before committing. The staging result itself carries deterministic
  source/settings hashes. The queue must be destroyed before its `JobSystem`; `Shutdown` stops intake,
  requests cancellation, and waits for every retained job. A content session with a pending reimport
  borrows its queue; destroy the session before the queue, including during exception unwinding.
  Cancellation requests do not release that borrow; PollReimport must consume the result, or the
  session must be destroyed before the queue.
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
- `SceneDocument` borrows its `World`, which must outlive the document. Entity selection and
  hierarchy use stable IDs, never component or container pointers. The hierarchy itself is the
  runtime's (`Entity::parent`, see the Runtime README's entity hierarchy section); the document keeps
  only node names. `Reparent` is an undoable runtime `SetParent` that keeps the world pose, and
  `Move` is a Hierarchy drag (reparent keeping the world pose, then place at a sibling index) as one
  undo step. `Nodes()` lists nodes in runtime sibling order. `Create`
  with a parent starts the node at the parent's origin (and fails before creating anything if that
  parent is no longer a live entity of the scene, e.g. after its creation was undone), and `Paste`
  places the root copy at the world pose captured by `CopySelection`, even if the source later moves
  or is deleted. An invalid selection leaves the previous clipboard intact; one Undo removes a
  single pasted entity with its copied pose. Editor scene files still write a parent column in
  each node line, but from world snapshot version 3 on the snapshot is authoritative and that column
  is not validated, so a node whose runtime parent is not itself a node still reloads. A file whose world snapshot is version 1
  or 2 is migrated on `Reload` by applying the node-line parents with the world pose kept, so nothing
  moves; the migration is rehearsed on a scratch `World` first, so a failure leaves no scene loaded.
  After a runtime undo destroys an entity, `SceneDocument::Undo` drops its node metadata and
  selection before a later save, so the scene file cannot name an entity absent from the world
  snapshot.
  `DeleteSelection` validates the selected nodes before deleting, removes each selected subtree
  once, and drops its node metadata and selection immediately. Undo restores the subtree's stable
  IDs, names, Euler hints, and selection; separate selected roots are separate Undo steps.
  Redo replays Runtime transactions and restores the matching authoring metadata, including
  names, selection, and authored Euler revolutions. A new edit discards the redo branch; Reload
  clears both histories.
  `DuplicateSelection` captures and pastes the current selection while preserving the user's prior
  clipboard. Like Paste, each created root has its own Undo step.
  `WorldTransform` exposes a live node's composed world pose to Editor views, so children can be
  drawn at their actual world position without exposing mutable Runtime entity storage.
  `TranslateSelectionXZ` and `TranslateSelection` validate generation-keyed targets, filter
  selected descendants, and apply world-space X/Z or X/Y/Z deltas through the portable gizmo
  math and one atomic transform Undo.
  `ApplySelectionGizmo` applies validated world-space rotate, translate, or scale operations to
  those same selection roots with one atomic transform Undo; the native Rotate tool uses it.
  `PreviewSelectionGizmo` returns owning prospective world poses for nodes and descendants using
  the same generation checks, selected-root filtering, and local edits as commit. Iterative cached
  ancestry composition matches Runtime TRS semantics, including rotated, mirrored, and nonuniform
  ancestors. It changes no selection, dirty state, or Undo/Redo history; invalid input returns no
  preview. The authoring thread consumes the snapshot for the current frame only.
  `SelectionGizmoFrame` uses the first selected root rotation with either that root origin or
  the mean selected-root origin. Selected descendants are excluded from the mean; an empty
  selection has no frame. Native handle placement and Center operations share this frame.
  `SetMeshRenderer`, `SetMeshRenderers` and the owning `MeshRenderer` snapshot validate
  entity/document generations. Nonempty, equal-sized batches require unique live keys and validate
  every target before mutation. Component attachment, mesh/material shader replacement and removal
  commit the whole batch as one Runtime Undo step; equal-value batches preserve Redo.
  Unresolved resource IDs are retained in scene persistence, and Undo can return to the saved clean
  baseline. These synchronous authoring APIs do not perform asset lookup, I/O, residency, or GPU work.
  `Dirty` compares the live serializable scene to the last successful Save or Reload. Its signature
  preserves sibling order while ignoring storage order left by a restored subtree, so Undo can
  return to a clean scene. Failed saves keep the previous baseline; external Runtime edits are seen.
- `AdditiveSceneGraph` owns scene descriptors and dependency edges, distinguishes owned documents
  from references, and rejects cycles or unsafe removal atomically. Migration dry-runs never mutate
  source text; bounded autosave journals reject corruption; stable-path three-way records retain
  unresolved base/local/remote values without coupling conflicts to a source-control provider.
- Inspector adapters borrow reflection metadata and expose differing multi-selection values as an
  explicit mixed state. Unknown component stores own opaque bytes and replace their state only
  after a complete payload validates, so unavailable plugins do not silently discard authoring
  data.
- `ProfileSession` retains a bounded, monotonic frame history. Invalid or out-of-order samples are
  rejected; capacity evictions increment a dropped count. Capture can be paused and cleared without
  changing project files. The graphical host currently supplies Editor frame processing wall time
  after BeginFrame and before Present; GPU timing and process memory are not instrumented.
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
- Build manifests own copied profile/artifact data and are atomically replaced. A successful
  manifest always records its target, configuration, reproducible command, artifact sizes, and
  checksums.

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

The `.meta` filename suffix is reserved for asset identity sidecars and is excluded from the source
asset index. Artifact hashes use the persistent UUID plus source bytes rather than the current path,
so an Editor move followed by reopen does not invalidate identity or derived-data addressing.
Profiling samples require strictly increasing frame IDs. Telemetry drops every event until the user
explicitly opts in; extension policy rejects untrusted publishers and, by default, invalid or
missing signatures.

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
The single-selection Camera component toggle and field edits use the same generation key and undo
boundary; invalid clipping and stale keys leave the scene unchanged. Camera values persist in the
runtime scene snapshot, so Save and Reload retain them.
The single-selection Light toggle and intensity field follow the same generation and undo rules;
intensity must be finite and nonnegative.
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
(each root's local transform and its parent's world transform) and, for Center, `SelectionCenter`
once at Begin. Every frame it applies the whole delta since Begin with `ApplyGizmo` and passes the
resulting local transforms to `GizmoTransaction::Update`. Frames therefore never accumulate rounding,
and Cancel restores the start exactly. The decision table:

| Rule | Behavior |
| --- | --- |
| Global / Local (`GizmoAxes`) | World axes, or the axes of the entity's world rotation. A mirrored axis is shown unmirrored, as in Unity. |
| Translate | World-space offset; only local positions change. |
| Rotate | About a world axis, applied after the existing world rotation. Pivot turns each entity in place; Center also swings positions about the centre. Angle from `RotationDragAngle`, signed by the right-hand rule, in (-pi, pi]; sum per-frame angles for longer turns. |
| Scale | Factors multiply each entity's local scale, as in Unity's scale tool: exact for the uniform handle and for entities aligned with the gizmo axes. Center also scales the offsets from the centre along the gizmo axes. |
| Negative scale | `ScaleDragFactor` never crosses zero (minimum `kMinGizmoScaleFactor`), so a drag can neither create nor remove a mirror. An existing mirror is kept, since factors are positive. |
| Parents | Results go back through each target's own parent, so a child under a rotated or scaled parent moves as dragged in world space. Under a non-uniformly scaled ancestor with rotation, the world pose is the lossy TRS, as with Unity's `lossyScale`. |
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
It is a robustness check, not a proof that no malformed input can fail.

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
cells because those measurements are unavailable. Export requires 1-600 strictly increasing nonzero
frame IDs with finite nonnegative wall times. Empty/invalid/read-only/recovery exports fail without
replacing the last good file. UI emits a one-shot request, disables export without samples/write
access or during recovery/close confirmation, and shows the application's result; UI never writes a
file itself. Comprehensive versioned capture/import, GPU timing and memory instrumentation remain open.

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
OpaqueComponents returns owning complete payloads. InspectOpaqueComponents returns owning names,
full-width type/entity IDs, byte counts and only the first 64 bytes, avoiding full-payload copies each
GUI frame. Inspection does not grant project write permission.

UnknownComponentStore accepts nonzero IDs and bounded nonempty single-line names without NUL;
limits are 256-byte names, 1 MiB/component, 64/entity, 4096 records and 16 MiB of bytes plus names.
Replacement and failed deserialize retain previous state. Deterministic classic-locale serialization
sorts entity/type IDs and hex-encodes binary bytes (empty payload is `-`). Serialized stores are
bounded to 34 MiB. Move transfers ownership/budget counters and leaves a reusable empty source.

Scene format 3 adds `opaque <entity> <type> "name" <hex-or-dash>` metadata before `world`; scenes
without opaque records still write format 2. Reload accepts formats 1/2/3, bounds the file to 64 MiB,
stages all records and world validation, and rejects duplicate/malformed/oversized/orphan records
before replacing live state. Every opaque owner must be both an authoring node and a staged Runtime
entity. Successful reload invalidates old keys and clears history; failed reload preserves live data,
selection, generations, history and the saved baseline. Opaque metadata participates in Dirty and
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
