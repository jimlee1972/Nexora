# Editor Core contract

`NexoraEditorCore` is the UI-independent authoring layer used by the standalone `NexoraEditor`
process. It owns project/workspace persistence, deterministic content indexing, stable panel and
command identities, hierarchy metadata, selection, clipboard operations, and scene-document
persistence. The production-support layer also owns explicit specialized-tool capability states,
reproducible build manifests, portable frame samples, virtual hierarchy ranges, signed-extension
policy, and opt-in telemetry state. It composes public Runtime Editor SDK APIs rather than reaching
into renderer or platform internals.

## Ownership and lifetime

- `ProjectWorkspace` owns its descriptor and open-document list; files are atomically replaced, a
  recovery journal is written before the primary workspace file, and successful save/recovery
  removes that journal. The UI may query and explicitly discard a pending journal. Versioned Editor
  layout payloads are persisted separately and never use Dear ImGui's unmanaged global ini file.
- `AssetWorkspace` owns index entries. Pointers returned by `Find` and `Search` are borrowed until
  the next `ImportTree` call or destruction.
- `ContentBrowserModel` owns its sorted item snapshot, breadcrumb and stable-ID selection state.
  Virtual ranges borrow item pointers until the next mutation. Rename, multi-item move, and delete
  validate a complete replacement snapshot before committing and retain one undo snapshot.
- Typed asset drag payloads carry the project generation and asset UUID. Reimport results are staged
  and may publish only when their generation and dependency graph remain valid; cancellation,
  staleness, failure, or a cycle preserves the previous artifact.
- `SceneDocument` borrows its `World`, which must outlive the document. Entity selection and
  hierarchy use stable IDs, never component or container pointers.
- Inspector adapters borrow reflection metadata and expose differing multi-selection values as an
  explicit mixed state. Unknown component stores own opaque bytes and replace their state only
  after a complete payload validates, so unavailable plugins do not silently discard authoring
  data.
- Gizmo transactions own their stable-ID and initial-transform snapshots until commit or cancel.
  Picking results are accepted only for the latest request and matching scene/viewport generations.
  Scene camera files are atomically replaced, while undo/redo history owns its replay callbacks.
- `PlaySession` remains the Runtime-owned PIE boundary. Play worlds are isolated and discarded by
  default; explicit apply-back is required and rejects concurrent Editor transform changes atomically.
  Console records and inspection/debugger state cross as owning snapshots, never live World pointers.
- Specialized tools are registrations, not implied backends: a tool must report `Implemented`,
  `ReadOnly`, or `Unavailable`, and every non-implemented state carries a reason.
- Build manifests own copied profile/artifact data and are atomically replaced. A successful
  manifest always records its target, configuration, reproducible command, artifact sizes, and
  checksums.

## Threading, errors, and deferred work

The current API is serialized and synchronous. Callers may run content indexing on a worker, but
must not call the same workspace concurrently. Long imports report progress and observe a
cancellation callback between files. Failed/cancelled entries remain inspectable and never replace
an existing artifact implicitly. Functions report expected failures with `false`, optional values,
or per-entry error text; filesystem exceptions are converted to error results where applicable.
Profiling samples require strictly increasing frame IDs. Telemetry drops every event until the user
explicitly opts in; extension policy rejects untrusted publishers and, by default, invalid or
missing signatures.

Watcher events are path-coalesced after a caller-supplied debounce interval and known self-writes are
discarded. A disk change never overwrites dirty authoring state: `DirtyConflictModel` retains both
hashes until the authoring thread explicitly chooses reload, keep, or compare.

The core deliberately does not depend on a UI toolkit. The optional `NexoraEditorImGui` owner
provides docking, theme/DPI scaling, input/text forwarding, stable-panel presentation, and recovery
choice UX. Native renderer submission, platform IME candidate positioning, accessibility,
viewport rendering, gizmos, and target-host visual validation remain UI-host responsibilities.
The portable gizmo state machine and picking validator define transaction and asynchronous-result
policy only; they do not claim graphical manipulation or renderer-backed picking acceptance.
