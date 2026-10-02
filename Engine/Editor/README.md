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
  global ini file.
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
  requests cancellation, and waits for every retained job.
- `SceneDocument` borrows its `World`, which must outlive the document. Entity selection and
  hierarchy use stable IDs, never component or container pointers. The hierarchy itself is the
  runtime's (`Entity::parent`, see the Runtime README's entity hierarchy section); the document keeps
  only node names. `Reparent` is an undoable runtime `SetParent` that keeps the world pose, and
  `Move` is a Hierarchy drag (reparent keeping the world pose, then place at a sibling index) as one
  undo step. `Nodes()` lists nodes in runtime sibling order. `Create`
  with a parent starts the node at the parent's origin (and fails before creating anything if that
  parent is no longer a live entity of the scene, e.g. after its creation was undone), and `Paste`
  places the root copy at the source's world pose. Editor scene files still write a parent column in
  each node line, but from world snapshot version 3 on the snapshot is authoritative and that column
  is not validated, so a node whose runtime parent is not itself a node still reloads. A file whose world snapshot is version 1
  or 2 is migrated on `Reload` by applying the node-line parents with the world pose kept, so nothing
  moves; the migration is rehearsed on a scratch `World` first, so a failure leaves no scene loaded.
- `AdditiveSceneGraph` owns scene descriptors and dependency edges, distinguishes owned documents
  from references, and rejects cycles or unsafe removal atomically. Migration dry-runs never mutate
  source text; bounded autosave journals reject corruption; stable-path three-way records retain
  unresolved base/local/remote values without coupling conflicts to a source-control provider.
- Inspector adapters borrow reflection metadata and expose differing multi-selection values as an
  explicit mixed state. Unknown component stores own opaque bytes and replace their state only
  after a complete payload validates, so unavailable plugins do not silently discard authoring
  data.
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
Multi-selection fields display mixed state and apply one changed field to every selected entity as a
single all-or-nothing Runtime transaction and undo step; malformed transforms roll back without a
partial write. Reflected component widgets and serialized Euler hints remain open. Native renderer submission,
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
Renderer-backed ID-buffer picking and the graphical gizmo handles remain open.

`editor.parser_robustness` mutation-tests the parsers that read persisted or external data (trace and
metric decoding, replay log, unknown-component store, scene snapshot, runtime blob, NXSHDR, shader
diagnostics, asset UUID, autosave, camera, and project/workspace/layout/recent-project files) with
fixed seeds. It
requires every parser to return normally on corrupted input; under the ASan/UBSan presets memory and
undefined-behavior errors fail it too. Set `NEXORA_PARSER_ROBUSTNESS_ITERATIONS` for a longer local soak.
It is a robustness check, not a proof that no malformed input can fail.
