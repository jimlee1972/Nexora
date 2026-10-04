# Editor Dear ImGui host contract

`NexoraEditorImGui` is an optional UI-host module. It owns the Dear ImGui context, translates
public `Nexora::Window` events, applies the Editor theme and DPI scale, creates the root dockspace,
and presents panels using the stable IDs owned by `NexoraEditorCore`. On the first frame it builds
the default workspace with Project and Hierarchy on the left, Inspector on the right, Console and Content along the bottom,
and Scene and Game tabs in the center, with Scene selected initially. Hierarchy and Content are selected
deterministically after their dock nodes settle, so adding a sibling tab cannot hide the primary
authoring views on first launch.

## Ownership and lifetime

- `EditorImGuiHost` owns one ImGui context and destroys it with the host.
- Modular builds expose Dear ImGui as one shared dependency so `NexoraEditorImGui` and each
  host/test executable observe the same process-global context; Monolithic builds keep it as one
  statically linked dependency inside the executable.
- Draw data and frame metrics are valid only for the frame in which `EndFrame` returns them.
- `DrawProjectSelector` owns only editable UTF-8 fields, the displayed error, and at most one
  `ProjectSelectorRequest`. `TakeProjectSelectorRequest` transfers that request once. The host never
  creates directories, acquires a writer lease, indexes content, or replaces the active project;
  the application performs those steps and reports a failed activation back to the selector.
- `ProductShell` and `SceneDocument` remain borrowed Editor Core models and outlive calls that
  present them. The Hierarchy owns only presentation state: its filter, generation-keyed expansion
  and selection anchor, rename buffer/modal, and pending one-frame UI requests. It builds
  parent-aware visible rows and clips their submission with `ImGuiListClipper`; plain/Ctrl/Shift
  selection routes through `SceneDocument::Select`, rename routes through `SceneDocument::Rename`,
  and sibling reorder or drag/drop reparenting routes through `SceneDocument::Move`. Stale entity or
  document generations, cycle rejection, and undo remain in Editor Core. The application owns the
  document and its `World`.
  Create root/child queues a name and optional generation-keyed parent; the authoring thread
  applies the request even if the dock tab is hidden on the next frame. Success selects the new
  entity, expands its parent, and clears a filter that would hide it. Invalid names or stale parents
  leave the document unchanged and show an error. Ctrl+Shift+N queues a root with the current
  create-name field outside text input and recovery.
  Copy and Paste buttons, plus Ctrl+C and Ctrl+V outside text inputs, route to the scene document's
  snapshot clipboard. Paste selects and reveals the new roots; failures appear in the Hierarchy.
  Shortcuts are disabled while a recovery journal awaits a choice.
  Delete selected and the Delete key in the focused Hierarchy or hovered native 3D canvas remove selected subtrees through the
  document. The action is disabled during recovery; Undo restores deleted entities and names.
  Duplicate and Ctrl+D outside text inputs copy the current selection without changing the scene
  clipboard. Paste and Duplicate run before Hierarchy rows borrow node names for the frame.
  The Scene panel has a top-down X/Z overview with a world grid, composed entity positions,
  middle-button pan, wheel zoom, and click selection synchronized with Hierarchy. It is an
  authoring overview; native OBJ geometry submission is application-owned and full Scene View acceptance remains open.
  `SceneCanvasViewport()` exposes its visible, clipped canvas rectangle in framebuffer pixels
  after each frame's dock layout and DPI scale. It is empty when the panel is not drawn and is
  reset at `BeginFrame`. The 3D Preview toggle publishes this rectangle to the application for
  a bounded direct native scene draw after UI; the X/Z overview remains available for editing. Backends
  without native scene geometry support show an unavailable message.
  In 3D Preview, right drag orbits the proxy camera, middle drag pans its X/Z target, Shift+middle
  drag pans target height, and the wheel zooms. F or Frame selected centers the target on
  selected nodes in X/Y/Z and adjusts distance from their conservative proxy bounds, clamped to
  2–100 world units. Orbit angle, distance, and target height are validated state; the target shares the overview's persisted X/Z
  center. The application saves the orbit angle, distance, and target height per scene through
  Editor Core `CameraPersistence`.
  A left click sends a framebuffer-pixel pick request to the application. The application tests
  the same proxy boxes it draws and updates the shared Hierarchy selection; Ctrl-click toggles a
  proxy and an empty click clears the selection.
  Dragging a selected proxy emits start and release positions in framebuffer pixels. The
  application projects both onto the selected node's horizontal plane for X/Z movement, or
  onto its world Y axis when Shift was held at drag start. It draws selected proxies and their
  descendants at the prospective position and commits one undoable world move on release.
  Selected proxies show colored X/Y/Z handles at the first selected node. The Local axes
  checkbox rotates them with that node's world rotation; otherwise they follow world axes. Their
  hit bounds enclose the rendered boxes, and a click captures the same axis for the whole drag. The application reserves
  three instance slots for these handles in the native draw. The Rotate tool (E over the canvas;
  W returns to Move) shows X/Y/Z ring handles and commits a selected-root turn on release as one
  Undo step. Selected roots and descendants visibly rotate during the drag; Escape cancels the
  preview. The Scale tool (R over the canvas) draws local X/Y/Z cubes and commits an axis's scale
  as one Undo step on release. Selected roots and descendants visibly scale during the drag; Escape cancels the preview. A camera-facing white cube previews and commits uniform scaling across all three local components; upward drags enlarge and downward drags shrink. Shift held at drag start snaps rotation to 15-degree steps and scale changes to 0.25-factor steps. Optional 0.25–4
  world-unit snap steps apply to movement preview and commit.
  Ctrl-click toggles a marker in the selection, Shift-click selects a visible range using the
  Hierarchy anchor, and Frame selected or F centers the overview on the selected world bounds.
  Dragging a selected marker previews an X/Z move and commits one document transform transaction
  on release. Red X and blue Z handles constrain a drag to one world axis. Escape cancels the
  preview without changing the scene. Snap movement optionally rounds the whole drag delta to
  0.25, 0.5, 1, 2, or 4 world units; the preview and committed move use the same snapped delta.
  The host exposes the overview center and zoom as a validated, backend-neutral camera state.
  The application loads and saves it per scene through Editor Core `CameraPersistence`; the UI
  never chooses a project file path.
  The Scene panel displays `SceneDocument::Dirty` beside its save controls. The indicator clears
  only when live serializable content matches the last successful save or reload.
- `ProjectContentSession` is also borrowed for each `DrawProductShell` call. The Content panel reads
  virtualized ranges from its UUID-keyed model, emits generation-tagged POD drag payloads, and routes
  rename/move/delete/undo/reimport back through the session. Reimport submits to the borrowed
  `AssetImportQueue`, shows bounded progress and structured diagnostics, offers cancellation, and
  polls authoring-thread publication once per frame. It never writes the filesystem itself.
  Breadcrumb and folder drop targets validate the payload, project generation, destination, and
  write permission before the session mutates anything. Dependency rows and cycle diagnostics
  resolve IDs only while the panel is drawing. Dirty external changes open one blocking conflict
  dialog at a time: Compare exposes the retained editor/disk hashes without resolving the conflict,
  while Reload or Keep records the terminal authoring-thread decision. The host never reads or
  overwrites the source file while presenting that choice.
- `ProjectWorkspace` and `RecentProjectStore` are borrowed for the frame. The Project panel exposes
  project name, stable UUID, canonical root, descriptor schema, read-write/read-only access,
  applied/required upgrade state, and the bounded recent-project list. It never acquires a lock,
  upgrades a descriptor, or writes recent state; the application completes those operations before
  drawing.
- `RuntimeConsole` is borrowed for the frame. The Console panel takes an owning, bounded snapshot,
  filters severity and text, clips visible rows, and reports the producer's dropped-record count.
  It does not retain record references after drawing. The application owns ingress and timestamps.
- `PlaySession` is borrowed for the frame. The docked Game panel reads an owning inspection
  snapshot and emits one-shot Start, Pause, Resume, Step, or Stop commands. F5 toggles Start/Stop,
  F6 toggles Pause/Resume, and F10 steps a paused session. The application owns the cloned World,
  fixed tick schedule, and discard policy; the UI never mutates the Play World directly. A bounded
  top-down X/Z map draws copied world poses, including entities under parents. It is an inspection
  preview. While Play is running, the Game tab publishes `NativeGameViewport()` in clipped
  framebuffer pixels for a camera-driven native OBJ draw owned by the application. Start focuses
  the Game tab. The application builds owning geometry/instance/matrix data after Play commands and
  fixed ticks, using the preview camera choice (Automatic selects Runtime's first valid active camera)
  and a CPU mesh catalog frozen at Start; editor reimport/deletion cannot change Play assets. Stop releases that catalog and discards the
  clone. No World borrow survives frame preparation. Missing cameras/geometry show an actionable
  status. Unsupported backends retain the X/Z map. One native 3D draw is available per window/frame:
  a simultaneously visible native Scene canvas takes precedence and Game falls back to its map.
  The Game panel's optional UTF-8 gameplay-library field emits a project-relative path and is
  disabled during Play. The application restores the project setting with its generation; otherwise
  project generation changes reset the field. The application
  validates the canonical library remains inside the project, loads its V3 module into the clone,
  runs optional FixedUpdate on ticks/Step and Update once per playing frame, and routes bounded
  module messages to Console. Failure pauses Play or rejects Start with status. It unloads before
  destroying the clone, including on window shutdown. Blank paths provide inspection-only Play.
  The initial component-oriented host supports shared component wires and bounded owner-checked
  allocation, without advertising scene/physics services.
  Clicking a playing Game canvas captures keyboard input; Escape, pointer exit, hiding Game,
  Pause/Stop, recovery/close prompts, and native focus loss release it. Captured keys/text do not
  reach authoring shortcuts; F5/F6/F10 remain Editor controls. Acquisition discards that frame's
  input batch and clears ImGui keyboard state, preventing the capture click or old held keys from
  becoming gameplay actions. The application applies the UI focus snapshot to PlaySession, then
  publishes an owning, held `NexoraInputSnapshot` before fixed ticks. User 0 maps WASD/arrows to
  [-1,1] move_x/move_y and buttons to Space=1, left mouse=2, right mouse=4, Shift=8, Ctrl=16;
  sequence advances once per processed frame and reserved is zero. Repeated capture_input calls
  return the same copied frame value. Pause/hide/blur clears held controls, including before Step.
  The application forwards release/focus events into the owning gameplay snapshot even when
  rendering is deferred or the client extent is zero; these batches do not tick Play or need an
  ImGui frame. Gamepad, pointer motion/look, rebinding, and multiple input users remain open.
  Game uses the existing bounded Lambertian preview and composed TRS, without editor proxies or
  gizmos; material shader execution, exact hierarchy shear, and simultaneous 3D views remain open.
- The Scene panel emits a one-shot save request from its button or Ctrl+S. The application consumes
  it after drawing, checks project write access and scene load state, and calls `SceneDocument::Save`.
  The host retains only the result text; it never chooses the path or writes the scene file.
- `RequestCloseConfirmation` opens one modal for an unsaved scene after a cancelable native close.
  `TakeCloseChoice` transfers Save and Exit, Discard and Exit, or Cancel once to the application.
  The application owns the final save and exit decision; a failed save leaves the modal visible.
- The Scene panel's Undo button and Ctrl+Z call `SceneDocument::Undo` on the authoring thread.
  Redo, Ctrl+Y, and Ctrl+Shift+Z call `SceneDocument::Redo`. Shortcuts leave an active text input's
  own history alone. A successful document replay clears the retained Hierarchy selection anchor;
  empty histories are reported in the Scene panel. Save and history actions are disabled while a
  recovery journal awaits a choice.
- The docked Profiler reads an application-owned bounded `ProfileSession`. It can pause and clear
  capture, plots retained Editor frame processing times, and reports the latest, average, peak, and
  evicted-frame count. GPU time and memory remain explicitly unavailable until instrumented.
- The project selector displays background content-index progress and exposes a one-shot cancel
  request. The application owns the candidate workspace and import operation, consumes the staged
  `AssetWorkspace` on the window/authoring thread, and keeps the selector open after cancellation or
  failure.
- The host does not own a native window or swapchain. The application supplies events exposed by
  `RenderSurface::Events`; the native `Render` overload flattens ImGui draw lists into the public
  backend-neutral `UiDrawData` contract. `RenderSurface` records those indexed draws directly into
  its acquired native GPU image. The application retains target ownership. The public-RHI overload
  remains the deterministic headless draw-contract path.
- The public-RHI `Render` path lazily retains one pipeline, an atlas-sized validation font-texture
  allocation, and geometrically grown vertex/index upload buffers. Repeated frames reuse those
  resources.
  `ReleaseRenderer` waits for the device before destroying them and must run before that device is
  destroyed. The path applies framebuffer-scaled clip rectangles and preserves ImGui index and
  vertex offsets. The native `RenderSurface` path owns an equivalent completion-protected cache.
- DPI is quantized to 100%, 125%, 150%, or 200%. Crossing a bucket rebuilds the font atlas at that
  pixel density, publishes the framebuffer scale, and derives the theme anew rather than
  cumulatively scaling an existing style.
- `UpdateImeCandidate` borrows the active `RenderSurface` only for the current ImGui frame;
  `EndFrame` clears that borrow. The platform callback converts logical cursor coordinates to
  rounded client pixels with the current DPI scale and ignores hidden candidates.
- Dear ImGui's global ini file remains disabled. `SaveLayout` and `LoadLayout` provide an explicit
  in-memory round trip. `ProjectWorkspace` stores that payload with an explicit schema under the
  project `.nexora` directory. The legacy schema 0 payload is read and rewritten as schema 1 on the
  next save; malformed or unsupported payloads are rejected and replaced only by a normal save.
- Recovery is prompted once per discovered journal. Failed recover/discard operations keep the
  modal open and expose the data-layer error instead of silently dismissing it.

## Threading and errors

All methods are serialized and run on the Window owner thread. Project access/upgrade state is a
snapshot borrowed from Editor Core; read-only state disables project-owned writes in the data layer,
not merely in widgets. Content mutation errors remain on the session and are shown in the panel; the
UI does not optimistically update around a failed filesystem transaction. Invalid display
dimensions and delta times are clamped to safe values. The Window abstraction owns native IME
candidate-window positioning; unsupported hosts report that result explicitly. Target-host visual
acceptance remains a release-runner responsibility. The Xvfb acceptance launches without a project,
drives create and read-only open through keyboard navigation, and verifies the resulting descriptor
and active access mode in a real process. Physical-display and Windows workflow acceptance remain
ED-M1 work.

Window backends normalize navigation, editing, punctuation, keypad, function, alphanumeric, and
left/right modifier keys before events reach the host. Each key event carries the complete
Control/Shift/Alt/Super snapshot, which the host publishes through Dear ImGui's modifier events.
Dear ImGui keyboard navigation is enabled so the recovery modal and shell remain operable without
a pointer; OS-level multi-viewport creation stays disabled.

## Accessibility direction

The stable `ProductShell` panel and command IDs are the semantic source for a future secondary
accessibility tree. Widget labels use those stable IDs and never become the data-model identity.
Dear ImGui does not provide a native accessibility tree, so keyboard traversal and screen-reader
bridges remain ED-M7 work; plugins must not inspect the ImGui widget tree to supply semantics.

## Scene and Hierarchy authoring access

An attached workspace must be writable before Scene/Hierarchy authoring. Read-only projects retain
filtering, selection, Copy, camera navigation and isolated Play; create, rename, reparent/reorder,
delete, Cut/Paste/Duplicate, Undo/Redo, Save requests and gizmo/overview drag commits are blocked.
Recovery, Play review and close confirmation share the same mutation gate. Queued Hierarchy writes
and rename targets are discarded while blocked; interrupted overview/native gestures cancel before
synthetic or physical release and cannot revive when access returns. The Scene panel identifies
read-only projects. Hosts without an attached workspace keep their portable authoring behavior.
Escape explicitly reports Cancel and closes the unsaved-scene modal; subsequent editing and Save
resume through the normal gate instead of bypassing an open modal.

## Camera and Light Inspector

Camera and Light sections support single and mixed multi-selection. Camera edits vertical field of
view and clipping planes; Light edits finite, nonnegative intensity. Enter commits through one
`SceneDocument::SetCameras` or `SetLights` batch; rejected input leaves the scene unchanged.
Controls disable during read-only, recovery, Play review and close confirmation. Both active drafts
and pending requests are abandoned on these gates, application focus loss, empty selection, Inspector
collapse and Play inspection. Widget IDs include a shared cancellation generation, so returning to
the same selection cannot revive ImGui's old input buffer. Requests recheck the current selection and
cancel prospective Scene gestures before mutation. Numeric formatting is locale independent; mixed
values, Undo/Redo and committed-only scene persistence retain the existing contract.

## Inspector Position and Scale input

Position/Scale fields keep bounded text drafts; typing never writes SceneDocument or Undo history.
Mixed values show an empty field with a Mixed hint. Enter parses one finite double and applies only
that field to fresh per-entity transforms in one generation-checked `SetTransforms` transaction.
Rotation and unrelated fields, including changes made while typing, remain intact. Zero scale rejects
the batch; negative scale is valid. Full-precision, locale-independent formatting and scientific input
avoid silent decimal truncation. Equal-value Enter produces no request and preserves Redo.

Escape or leaving the field abandons unsubmitted input. Selection/document changes, application focus
loss, leaving Editor inspection for Play, and hiding the Inspector invalidate drafts; a later Enter
cannot revive them. Read-only/recovery/Play-review/close confirmation disable Position/Scale/Euler
controls and discard pending transform/rotation requests. Close-modal state is queried in the root
scope that opens it, so its later frames retain the same gate. Accepted Transform/Euler requests check
current selection and access, then cancel prospective Scene gestures before mutation. UI drafts never
participate in scene persistence; Save stores committed values.

## Inspector rotation

Local rotation is presented in degrees using extrinsic Z-X-Y composition (`qY * qX * qZ`).
Typing does not mutate the document; Enter validates a finite angle and commits one generation-safe
`SceneDocument::SetEulerField` transaction. Multi-selection applies only that axis, retaining each
target's other angles, position and scale. Invalid or stale input rejects the complete batch.

Euler hints belong to `SceneDocument`; the host retains only current-selection display samples and
active text buffers. Hints retain authored revolutions (for example, 450 degrees), treat `q` and `-q`
as the same rotation, survive selection changes and document save/reload, and participate in undo
including edits that produce the same quaternion. External rotation changes use canonical angles
and stale hints are omitted from saves. Scene format version 2 stores hints in the Editor layer;
version 1 remains readable. At gimbal lock the
canonical display sets Z to zero and folds the combined rotation into Y. Partial numeric text is
retained only while its field is active; Escape or leaving the field abandons unsubmitted input.

The application now obtains prospective world poses from `SceneDocument::PreviewSelectionGizmo`
for Move, Rotate, and Scale. Preview and release share the validated root edits and Runtime hierarchy
composition, so transformed descendants match the committed poses. Escape drops the snapshot without
writing the document or changing Undo/Redo history.

The native canvas offers Center pivot (P). Pivot rotates/scales each selected root in place;
Center places handles at the mean selected-root origin and rotates/scales root offsets around that
point. Selected descendants do not weight the center twice, and local axes follow the first selected
root rotation. Tool, axes, pivot, and camera controls stay fixed during a left-button gesture.

Scene gestures cancel before focus loss synthesizes input releases, before Undo/Redo, root creation,
Paste or Duplicate shortcuts, and when the document generation changes, the canvas is hidden,
recovery is active, or preview mode changes. These cancellation paths drop prospective state; a later
mouse release cannot commit the abandoned gesture. The X/Z overview shares the focus/history
cancellation behavior. Save during a drag waits for the gesture to commit or cancel, then serializes committed scene content.
This retains the request when ImGui processes a queued release in a later frame than Ctrl+S.

The Xvfb workflows issue Undo once per gesture and retry only Save until the expected committed
scene bytes appear, with a bounded deadline. This accommodates queued event delivery on a loaded
host while retaining the one-step Undo assertion; failures report both actual and expected bytes.

### Mesh asset assignment

The Inspector offers imported OBJ assets from the current project in its Mesh Renderer selector
for single or multiple selections and can remove components from the whole selection. Different mesh
IDs or mixed component presence show Mixed; differing materials do not mix the mesh label. It borrows
the application-owned MeshAssetCatalog and ProjectContentSession for the frame; a queued edit owns
all entity/document keys, the asset UUID and project generation. Selection changes reject abandoned requests; every target validates before
a single atomic Undo transaction. Publication checks both the live content item and catalog, preserving
each existing material reference when replacing meshes and adding a default component only where
absent. Read-only projects and recovery disable edits.
Missing/unresolved mesh references are retained and displayed honestly. Each accepted edit uses
SceneDocument Undo/Redo and cancels prospective scene gestures before mutation. The application
publishes the CPU catalog after project activation. The application now submits resolved OBJ
geometry through native Presentation batches; OBJ geometry reimport publishes atomically through the live content model.

OBJ reimport now publishes an owning geometry payload together with the artifact hash, after its
source/project/dependency revisions and live geometry budget pass validation. The application
refreshes its CPU catalog when the live Content Browser revision changes, before drawing native
geometry. Failed/cancelled/stale results keep the prior mesh; rename/move Undo keeps a newer
published geometry and delete/Undo removes/restores live resolution. UI borrows snapshots for the
frame and performs no mesh source IO while rendering or picking.

Play inspection uses one owning Runtime snapshot per UI frame. Selecting a Game entity switches
Inspector to read-only Play mode: local/world transforms, parent/scene state, Camera/Light payloads,
and full 64-bit mesh/material shader IDs are displayed as text. Editor mode remains available;
Play selection never changes the scene document's selection. Missing entities clear the inspected ID,
and Stop returns Inspector to Editor mode. Pause reasons and callback failure counts are visible in
Game. Fixed and per-frame gameplay callback failures share RuntimeFailure pause/input-release policy;
native debugger attachment and generic reflected component inspection remain open.

The application loads the bounded project gameplay setting without loading the module. Explicit
`--gameplay-library` values, including an empty value, override it. Normal shutdown saves only for a
writable project without a recovery journal; unchanged invalid settings are preserved. The UI setter
accepts the project generation so a restored setting survives that generation's first draw.

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

The Play review modal constrains its dimensions to the logical main viewport and scales its child
region to available space, keeping confirmation reachable in a 640x360 logical viewport at 200% DPI.
Pose values use full double precision with horizontal scrolling. UI tests exercise layout and
client-pixel button routing; target-host native DPI evidence remains a separate acceptance gate.

Pointer events arrive in native client pixels. `SetDisplay` supplies logical dimensions and the
actual frame scale before `ProcessEvents`; conversion uses that scale once, independently of the
font DPI bucket. The host retains an owning integer coordinate pair and reprojects it when scale
changes, even without a mouse-move event. Focus loss clears the pair. Scale changes cancel pending
Scene gestures before synthetic pointer motion can commit a source edit. Invalid nonfinite scales
fall back to 100%, finite scales clamp to the existing 25% minimum. DpiChanged events update the
same path in event order. ImGui floors its consumed logical mouse position during NewFrame.
No scene input normalization changes Window/RenderSurface's native coordinate snapshots.

The Inspector groups missing-plugin components in a clipped read-only child region with horizontal
scrolling. Each row names its entity, component name, full-width type ID, exact byte count and up to
64 hex preview bytes; empty/truncated payloads are explicit. It never interprets bytes as editable
properties and exposes no remove/write controls. Multi-selection shows per-entity rows; snapshots
are owning and bounded, cleared when selection/Inspector mode changes. Missing plugins preserve data
through SceneDocument save/reload and Undo. Restoring/loading a plugin and reflected editing remain
separate work; the UI does not execute preserved bytes.

Camera and Light Inspector controls support mixed multi-selection presence and values. The first
click on mixed presence enables the component for all selected entities, preserving present values
and adding defaults only where missing; disabling removes all in one undoable transaction. Fields
require presence on the whole selection. Camera FOV/near/far and Light intensity use generation-keyed
owning text drafts and commit only on Enter. Typing, Escape, focus loss or selection changes do not
write a partially parsed value. The entered field applies to every selected entity while retaining
its unrelated fields. Finite/clip/intensity and stale/duplicate validation is all-or-nothing; UI
requests also check project write/recovery access. InputText drafts avoid unsupported EnterReturnsTrue
flags on ImGui numeric widgets. Full generic reflected component editors remain separate work.

## Game preview camera selection

Game camera selection is preview-only and stores an entity ID scoped to the PlaySession generation.
The clipped chooser offers Automatic and renderable cameras in active scenes, using owning snapshot
IDs and a temporary Runtime CameraView validation on the serialized authoring thread. Read-only
projects can choose previews; recovery/Play-review/close modals disable the chooser. It does not
change World components, SceneDocument selection, Undo history or project persistence. Missing,
inactive or unrenderable choices return to Automatic, and Stop/new Start clear the choice and popup.
`GameCameraSelection()` returns zero for Automatic. The application passes that value into frame
preparation after commands/ticks, where current scene lifecycle and CameraView are rechecked; stale
snapshots cannot retain a removed camera or draw an unloading scene. Automatic remains ordered by
entity ID. No borrowed World data survives frame preparation.

## Camera alignment from Scene view

With one enabled Camera selected, Use Scene view pose copies the stored native Scene orbit's eye
and right-handed -Z orientation (using the same +/-100,000 clamped X/Z center as the native preview) through `SceneDocument::AlignCameraToWorldPose`. Scene 3D must be
enabled and available. Lens/FOV/clipping, local scale and parent are retained; this is pose alignment,
so a different camera FOV still produces a different framing. Draft Inspector inputs and Scene
motions cancel before the one-step Undo transaction. Read-only/recovery/review/close gates disable
it, and multiple selections require choosing one camera. The core inverts every ancestor's local
TRS for exact positions under shear/mirrors; it rejects invalid/stale targets and preserves Redo
for equivalent world poses. Real UI clicks, Runtime camera matrices and save/reload verify the path.

## Add a content mesh to the Scene

Add mesh to Scene resolves exactly one selected content asset against the current project-generation
CPU mesh catalog. A writable workspace/content session with no recovery, review or close modal is
required. Missing/unresolved assets, non-mesh/multiple selections and stale catalogs disable the
operation. The action cancels Scene gestures/Inspector drafts, creates a named root at the overview
X/Z center (Y=0) or the enabled/available Scene 3D target (the same +/-100,000 X/Z clamp as preview), and selects it. It changes the Editor Scene,
not the isolated Play clone. `SceneDocument::CreateMesh` and Runtime's initialized creation record
one Undo, retaining the pose, component and stable identity on Redo. No source IO or GPU allocation
occurs inside the UI action. Save/Reload retains the existing mesh resource reference and metadata.

## Content mesh drag placement

Content rows publish an owning typed UUID/project-generation payload once at drag start; switching
projects cannot refresh an old drag into a new operation. Only the Scene canvas accepts placement.
The overview maps the logical pointer to X/Z with Y=0. Native Scene converts the pointer once to
its published physical viewport, builds a ray from the preview's clamped float orbit camera, and
intersects Y=0. Parallel/behind-camera, out-of-budget and out-of-projection ground points reject.
A tooltip reports the prospective point; hover performs no document mutation or source IO.
Delivery rechecks the current browser/catalog, workspace/content access and modal state, then shares
Add mesh to Scene's initialized creation/selection and one-step Undo path. Escape, focus loss,
blocking prompts or write-access loss discard the asset drag and active source, so a later held-button
release cannot revive it. Rejected deliveries retain Redo. Native geometry ghosts and mesh-surface
placement are deferred; the tooltip is the current placement preview.

## Multi-selection deletion

Hierarchy Delete and the hovered native Scene shortcut use the document's atomic selected-subtree
batch. All selected roots are one Undo entry, with descendant filtering, exact component/metadata
retention and prior selection restoration. Existing workspace/modal gates and gesture cancellation
apply to the whole selection; the UI retains no Runtime entity borrow across deletion or replay.

## Complete clipboard forests

The Hierarchy Copy/Paste/Duplicate controls and Ctrl+C/Ctrl+V/Ctrl+D shortcuts now share owning
complete-subtree capture and one initialized creation transaction. Copies retain component data,
child local poses, Euler hints and opaque bytes; one Undo restores prior selection, and Redo retains
initialized values and stable IDs. Existing read-only/modal/input-focus gates apply. The UI never
borrows live entity data across the operation, and Duplicate leaves the previous clipboard intact.


## Graphical Cut

The Hierarchy Cut button and Ctrl+X outside text input share the owning CutSelection workflow.
Writable workspace and recovery/Play-review/close-modal gates apply before clipboard mutation.
Cut cancels Scene gestures and Inspector drafts, clears selection, and shows a Paste/Undo status.
Paste and Duplicate also cancel pending drafts and show the Editor Inspector after successful
creation. One Cut Undo restores all selected subtrees; first successful Cut Paste keeps root names
with new IDs, and later Paste uses Copy naming. Clipboard state itself is not document history.
Fresh layouts dock Inspector on the right so a default floating Inspector cannot cover Hierarchy
buttons. Existing saved layouts retain their placement. Real key/pointer tests cover 1x/2x input,
workspace/modal/text-input rejection, replay, retained clipboard state and save/reload.

Game Apply Changes wraps to another row when its button does not fit, keeping the Play review
action reachable in a narrow dock after a DPI/extent change.


## Content mesh assignment by drag

The closed Inspector Mesh field accepts the owning Content UUID/project-generation payload.
Hover only shows an assignment tooltip; release captures the currently displayed generation-keyed
selection into the existing owning mesh request. The authoring thread rechecks selection, writable
workspace/content, modal/focus gates, browser membership and the current CPU mesh catalog before
one SetMeshRenderers batch. Existing material shader references survive; missing components are
added with default material data. Invalid/non-mesh/stale/canceled drops preserve history. Successful
delivery cancels Inspector drafts and Scene gestures. No source IO or GPU upload occurs in the drop
handler. The asset chooser remains available alongside drag assignment.


## Hierarchy Select All

Ctrl+A in the focused Hierarchy, outside text input and recovery/Play-review/close modals, selects
its complete visible row set through generation-keyed SceneDocument selection. The row set includes
clipped rows, follows the current filter and expansion, and excludes collapsed descendants when
unfiltered. An empty result clears selection. Read-only projects retain the action. It cancels
prospective Scene gestures and resets the range anchor to the first selected row; no World edit,
clipboard replacement or Undo/Redo consumption occurs. Inspector/text-input and other-panel focus
keep their own Ctrl+A behavior. Large-scene scale/soak acceptance remains open.
