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

Save All recovery retires a strictly empty ordinary journal without replacing any source or
inferring a publication phase. If final directory removal fails, cleanup best-effort restores its
bounded phase manifest for retry; occupied/invalid journals retain their entries. Initial metadata
write failure leaves original scene files and dirty baselines unchanged.

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
- `ContentBrowserModel::SelectVisible` atomically replaces selection with the complete matching
  current-folder query/type row set, including clipped rows. It uses one item scan and preserves
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

`AssignMaterialAsset` requires one selected live Mesh Renderer, writable Content, an editable host,
live entity/document/project generations and the same owning typed payload in catalog and Content.
It commits one `SceneDocument::SetOpaqueComponent` Undo. Identical assignments preserve Redo.
The Editor-owned `editor.material.asset` component stores a versioned UUID independently of the
unchanged legacy shader ID; missing resources and unsupported component versions retain all bytes.
Per-frame reference inspection copies only bounded opaque metadata prefixes and requires exact
17-byte length/version, including when unrelated plugins retain large payloads. Unsupported
versions/type-name collisions reject assignment. Scene container and stable C/Zig
schemas stay unchanged. Application-owned Game material snapshots freeze converted values and
mesh-entity assignments before Play; reimport/deletion/reassignment cannot alter that session.
Material catalogs do not borrow Runtime Worlds or contain native resources. Multi-selection,
reference removal, textures, shader graphs and the shipped-game/cook consumer remain separate work.

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
