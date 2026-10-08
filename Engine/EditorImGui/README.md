# Editor Dear ImGui host contract

`NexoraEditorImGui` is an optional UI-host module. It owns the Dear ImGui context, translates
public `Nexora::Window` events, applies the Editor theme and DPI scale, creates the root dockspace,
and presents panels using the stable IDs owned by `NexoraEditorCore`. On the first frame it builds
the default workspace with Project and Hierarchy on the left, Inspector on the right, Console and Content along the bottom,
and Scene and Game tabs in the center, with Scene selected initially. Hierarchy and Content are selected
deterministically after their dock nodes settle, so adding a sibling tab cannot hide the primary
authoring views on first launch.

Synthetic contract fixtures explicitly choose Ctrl shortcut semantics and disable event trickling.
The shared undo/redo contract also runs with macOS behaviors and physical Cmd/Super events;
production retains ImGui's native platform defaults.

## Ownership and lifetime

- `EditorImGuiHost` owns one ImGui context and destroys it with the host.
  State ownership also releases the previous context on move assignment, clearing backend/IME
  borrows before destruction and preserving another current context. Move between frames;
  a moved-from host supports only destruction or assignment. Before replacing a destination that
  used the public-RHI renderer, call `ReleaseRenderer` while its borrowed device remains alive.
  `editor.imgui_context_lifetime` verifies move assignment/construction, self-move, current-context
  restoration, and zero outstanding ImGui allocations after all owners are destroyed.
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
  and selection anchor/cursor, rename buffer/modal, and pending one-frame UI requests. It builds
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
  selected forests in X/Y/Z from the union of exact world-transformed CPU mesh bounds and rotated
  proxy bounds, including descendants once. Both inputs use the current clipped framebuffer aspect
  and the narrower horizontal/vertical FOV, with distance clamped to 2–100 world units.
  Generation-checked catalog snapshots are borrowed only during the action; framing does not read
  source files or consume the native upload budget. Source geometry can be framed even when its
  GPU upload falls back to a proxy; submission budgets remain application-owned. Invalid world
  bounds or centers outside +/-100,000 preserve the previous camera. Read-only navigation retains
  scene content, dirty state and history. Close/recovery/gesture gates block the action. Orbit angle,
  distance, and target height are validated state; the target shares the overview's persisted X/Z center. The application saves the orbit angle, distance, and target height per scene through
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
  six instance slots for Move's three axes and three plane handles in the native draw. The Rotate tool (E over the canvas;
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
  Pause display captures one owning snapshot; producers continue admitting/evicting records and
  filters operate on that frozen copy. Resume display releases the copy and reads current ingress.
  Clear view hides all sequences present at its click, including records hidden by filters or
  received while paused. It never removes producer records or resets cumulative drops; newer
  records appear after resume. Visible/captured-or-retained counts distinguish display state from
  ingress. These diagnostic controls do not write projects or consume authoring history.
  Pause/clear scope follows ingress address identity and resets on a different/null binding; detach
  with a null binding before reusing the same ingress storage for a new instance. No record storage
  or ingress pointer is dereferenced between drawing calls. The captured copy is bounded by that
  ingress's configured capacity and is replaced, not appended to, on the next pause.
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
  ImGui frame. Gamepad, pointer motion/look, device-specific rebinding and multiple input users remain open.
  Game uses the existing bounded Lambertian preview and composed TRS, without editor proxies or
  gizmos; material shader execution, exact hierarchy shear, and simultaneous 3D views remain open.
- The Scene panel emits a one-shot save request from its button or Ctrl+S. The application consumes
  it after drawing, checks project write access and scene load state, and calls `SceneDocument::Save`.
  The host retains result text and emits owning file requests; it never writes scene files.
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
- Profiler Import CSV emits an independent one-shot request; the application reads the current
  project's `.nexora/frame-processing.csv` on the authoring thread and transfers a validated owning
  wall-time snapshot to the host. The imported static trace is displayed separately from live
  history; Clear imported changes only that snapshot. Read-only import is allowed, while modal,
  recovery, close and no-project gates block requests. Failed reads/publication preserve the prior
  snapshot. Root/UUID changes or project detachment clear imported data, status and pending requests.
  Publication requires 1-600 ordered nonzero frames, finite nonnegative CPU wall times and zero
  unavailable GPU/memory fields. No retained workspace/sample borrow or live-session mutation is
  introduced. Statistics use an incremental mean so finite large samples do not overflow a sum.
  CSV has no project/device provenance and is labelled accordingly; JSON import verifies project identity.
- The project selector displays background content-index progress and exposes a one-shot cancel
  request. The application owns the candidate workspace and import operation, consumes the staged
  `AssetWorkspace` on the window/authoring thread, and keeps the selector open after cancellation or
  failure.
- Content labels, selected paths and drag/context labels convert native paths explicitly to UTF-8;
  the host never uses the Windows system code page for asset names.
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
  The host tracks its last native UI resource domain as a value, never as a retained surface pointer.
  Changing owners invalidates the upload acknowledgement and sends the atlas to the new domain even
  at unchanged DPI. Resize/move within a domain retains the upload; dead domains reject rendering.
  `editor.native_surface_lifetime` covers native replacement, move, DPI round trips, steady-buffer
  reuse, resize, and teardown. Linux runs it on Xvfb with strict validation; Windows/DX12 and
  macOS/Metal use the native host when available. An unsupported Metal runner is explicitly skipped
  and cannot supply native/physical acceptance evidence.
- Public-RHI user texture registrations borrow the caller's texture/device. Unregister prevents
  future bindings; wait for the last GPU use before destroying that texture. Release the renderer
  before destroying its device. IDs are scoped to
  the host State, with monotonically assigned 32-bit generations that survive renderer release
  and device replacement. Exhaustion returns zero instead of reusing an old generation. Renderer
  release invalidates every registration; stale IDs keep the diagnostic font fallback and cannot
  unregister a later texture. `editor.imgui_contract` covers release, repeated cache reset,
  device replacement, stale draw fallback, and rejection metrics.
- `RegisterNativeTexture` copies tightly packed linear RGBA8 pixels for the native `RenderSurface`
  renderer, without exposing its device. Each image is at most 1024x1024; the host permits 64 live
  images and 16 MiB of retained pixels. Invalid sizes, exhausted slots/bytes/generations return zero.
  IDs share the host's monotonic generation namespace with public-RHI registrations, but each
  renderer accepts only its own registrations. Stale/foreign IDs use the diagnostic font fallback
  and increment host rejection metrics. Register/unregister between frames on the owner thread.
  Unregister frees the CPU copy immediately and prevents future bindings. A bounded GPU cache slot
  remains until reuse or surface drain; replacing it retires the old resource behind completion.
  One host drives a surface's UI namespace. New domains resend all live images; resize/move and
  font DPI changes preserve immutable image uploads. Uploads are acknowledged only on successful
  recording. Public-RHI renderer release does not invalidate native registrations.
  `editor.native_surface_lifetime` covers copied caller data, stale fallback, owner/DPI/resize,
  byte/slot limits, and over 4096 native uploads with no rejected backend texture bindings.
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

## Hierarchy keyboard navigation

Focused Up/Down and Home/End select the complete visible tree or filtered flat row set, with
held-key repeat and endpoint clamping. Shift extends/shrinks an inclusive range around the
selection anchor; Shift pointer selection shares that anchor and moves the keyboard cursor.
Collapsed descendants are skipped. Without a visible selection, Down/Home start at the first row
and Up/End at the last. Empty rows preserve selection. The clipper includes and scrolls the chosen
endpoint even when it was previously clipped.

In the unfiltered tree, plain Right expands a closed parent, then enters its first child on a
subsequent press. Left collapses an expanded parent, otherwise selects its visible parent; roots
and leaves clamp. Filtering presents a flat list and disables these tree actions. Ctrl/Alt/Super
navigation variants and Shift Left/Right do not change the owning selection through these routes.
Read-only projects retain selection and expansion; neither authors the World or consumes history.

Text input, another panel, application blur, Game capture, active/pending scene gestures, a held
pointer, drag payloads, Rename and blocking modals prevent these commands. Selection changes
cancel abandoned Inspector drafts. State owns generation-keyed values, never retained node/name
borrows. Document/filter changes clear cursor/anchor; every operation rechecks visible membership
and the cursor's current selection. Invalid/hidden anchors rebase to the initial visible cursor.
Clearing selection or Ctrl/Cmd+A clears the cursor. Expansion updates the same frame's row snapshot.
Real 1x/2x and macOS-modifier event tests cover ranges, clipped reveal, nested traversal, repeat,
read-only history, filter/reload/detach boundaries, Rename and ownership gates, including a real
Hierarchy drag and a same-frame native Scene release. Screen-reader bridging and physical-host accessibility acceptance remain open.

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

## Inspector component values and reset

The Editor Inspector exposes Copy values / Paste values for Transform, Camera and Light. Copy
requires one live selected entity with that component and snapshots committed numeric values,
including authored Euler turns; it discards unsubmitted drafts and works in read-only workspaces.
One host-owned typed payload survives selection, source edits/deletion and document/workspace
changes until replaced by another successful Copy or host destruction. It retains no source keys
or borrowed World storage, and does not replace entity Copy/Cut/Paste or the OS text clipboard.
Paste requires matching copied type, normal authoring interaction and workspace write access.
Transform Paste applies local TRS/Euler to the whole selection; Camera/Light Paste updates existing
components only. An all-absent selection disables Paste. Every changed batch is one atomic Undo;
no-ops retain Redo. Copy and Paste cancel Inspector drafts and prospective Scene gestures and clear
active input before acting, so Enter cannot submit abandoned text. Multi-selection/absent Copy is
disabled and leaves the previous payload intact. Play inspection has no component-value controls.
The host does not persist this clipboard or perform source IO while copying/pasting values.

Reset Transform restores local identity position/rotation/scale and zero authored Euler revolutions
for the displayed selection, including hidden stale hints. Reset Camera and Reset Light restore existing components to their
Runtime defaults without adding missing components; controls disable when all targets lack that
component. Each changed selection resets atomically as one Undo/Redo step; already-default clicks
retain Redo. Parent, selection, names, opaque payloads and unrelated components remain intact.
Reset abandons active/pending Inspector drafts and prospective Scene gestures before mutation.
Read-only/recovery/Play-review/close gates apply. Committed defaults survive Save/Reload; 1x/2x real
pointer/keyboard tests cover multi-selection, mixed presence, abandoned drafts, Undo and access gates.
Complete reflected component editing remains open.

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
file itself. Arbitrary capture import, GPU timing and memory instrumentation remain open.

Export JSON emits an independent one-shot request consumed through `TakeProfileJsonExportRequest`;
the CSV request API retains its behavior. Both buttons share empty-sample, write-access and modal
gates. The application writes schema-1 `.nexora/frame-processing.json` through ProjectWorkspace and
reports success/failure in the same status area. JSON identifies measurement source/scope, project,
units and sample count; uint64 frame/drop values are decimal strings and unmeasured GPU/memory are
explicitly unavailable/null. The UI does not serialize/write files or retain sample borrows.

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

## Scene Select all

Focused Scene Ctrl+A and Select all change selection without changing World, dirty state,
clipboard or authoring history. Overview selects its scene nodes through current NodeKeys in
node order; empty scenes clear selection. Native input emits one owning project/document token.
`TakeNativeSceneSelectAllRequest` consumes it once in its issuing GUI frame and rejects changed
scope, unavailable preview, focus loss and newly blocking dialogs. BeginFrame and gesture
cancellation discard unapplied requests. No source IO or GPU ownership is added.

After widgets the application revalidates the current SceneFileSession token and selects the same
bounded candidates used by drawing/picking, including mesh upload/proxy fallback. It borrows those
numeric records only during the synchronous selection call. Hidden/locked candidates are omitted;
current NodeKeys validate every remaining target before one selection update. Stale, malformed,
duplicate or oversized packets preserve selection. An empty eligible set clears it. The existing
3,999-candidate budget means invisible submission tails are not selected by native Ctrl+A.

Read-only selection works. Other panels/text input, blocking modals, unavailable native preview,
held mouse buttons and active/pending drag commits cannot invoke the action. Native same-frame
framing waits for a later input rather than using pre-selection bounds. Buttons do not poll Nodes()
every idle frame. Real 1x/2x tests cover native/overview and button/key parity, one-shot/stale packets,
4,001-node limits, unknown bytes, clipboard and Redo retention. Linux Xvfb verifies both submitted
proxy/OBJ roots become selected after Ctrl+A and remain pickable. Full Scene View and target-host
acceptance remain open.

## Native Move planes

Move draws paired-color XY, XZ and YZ plane handles beside the existing single-axis handles.
`SceneMovePlanes.h` builds owning numeric boxes for the same native drawing and exact/padded picking.
Global planes follow world axes; Local planes follow the first selected node's world rotation.
Pivot/Center uses the existing selection frame. A plane hit keeps selection and captures its two
orthonormal axes as numeric values; Ctrl picking cannot start a plane gesture. Shift on an explicit
plane keeps that plane rather than switching the free drag to Y. Scale and Rotate keep their own handles.

Start/current rays intersect the captured plane at the gizmo origin; both preview and release call
the same `MovePlaneDelta`, snapping each plane coordinate in world-unit steps before forming one
world delta. Nonfinite/degenerate/parallel/behind/distant rays reject the move. The existing bounded
candidate submission, one selected-root transaction, parent conversion and descendant preview stay
in use. Escape/focus/modal cancellation leaves committed World untouched. The private helper retains
no World, document, plugin or GPU borrow and performs no IO. No Runtime or public GUI ABI changes.

Contract tests cover every Global/rotated Local plane, shared drawn/pick boxes, snapping, invalid rays,
nonuniform/mirrored parents, selected descendants, opaque bytes, preview/Redo retention and one-step Undo.
Linux Xvfb drives all six plane gestures on proxy/OBJ roots, including Shift, Escape and saved transforms,
after Scale/Rotate regression checks. Complete gizmo and physical target-host acceptance remain open.

## Native gizmo mode shortcuts

X toggles the native Scene's existing Global/Local axes; Scale always retains Local axes. P toggles
Pivot/Center. These host-owned settings require focused Scene input, actual canvas hover or active
canvas, application focus, no text input and no blocking modal. Ctrl/Alt/Super-modified X/P do not
toggle modes. Both shortcuts resolve before same-frame click/drag setup and remain blocked during
held or pending native gestures. The P path now uses the same hover/navigation guard as Q/W/E/R,
including Home followed by P without moving the pointer. Read-only projects retain mode navigation.
World, selection, dirty state and Undo/Redo are unchanged; no IO, plugin or GPU contract changes.
Real 1x/2x input tests cover same-frame mode/click, modifier/focus/text/modal gates, Scale constraints
and retained Redo. Linux Xvfb verifies Home/P followed by center scale/rotate/move on proxy and OBJ
roots, with one-step Undo and saved transforms. Complete gizmo and target-host acceptance remain open.

## Native Scene selection tool

Select (Q) retains ordinary and Ctrl-toggle native picking without creating a transform drag,
preview or release request. Move/Rotate/Scale remain W/E/R; existing enum values are retained and
Select is appended. The application submits and picks no transform handles for Select, so handles
cannot intercept an entity hit. Tool changes only affect the host-owned view state, preserving
selection, World, dirty state and authoring history. Read-only projects can choose tools and pick.

Keyboard tool selection requires a focused Scene panel, a real canvas hover or active canvas,
application focus, no text input and no blocking modal. It uses mouse hover without ImGui's navigation
focus override, so Home followed by Q/W/E/R still works without moving the pointer. Keyboard requests
resolve before click/drag setup in the same frame. Active and pending native drag commits block tool
changes; toolbar buttons share these guards. No plugin callbacks, IO or GPU ownership is added.
Real 1x/2x input tests cover Q/click batching, Ctrl picking, toolbar parity, Home/navigation, active
and released gestures, read-only, panel/focus/modal/text gates and retained Redo. Linux Xvfb covers
proxy and authored OBJ picking, hidden handles, unchanged saved bytes after a Select drag, and
switching back to transform gestures. Complete Scene View and target-hardware acceptance remain open.

## Frame all Scene view

Focused Scene Home and Frame all navigate without changing selection, World, dirty state, clipboard
or document history. Native input emits one owning project/document token. The application consumes
`TakeNativeSceneFrameAllRequest` after widgets, revalidates its current session, and supplies the same
bounded candidates used by native drawing/picking to `ApplyNativeSceneFrameAll`. Drawing and framing
share the 3,999-candidate budget, transform eligibility and actual upload/proxy fallback. Invisible
budget tails cannot pull the camera away. The GUI borrows only numeric bounds during that call;
application geometry remains CPU-owned. Intent applies once in its issuing GUI frame and is discarded
on BeginFrame, focus loss or gesture cancellation. Stale scope, empty/oversized/malformed packets and
invalid/out-of-range world bounds preserve every camera field. No source IO or GPU publication occurs.
The current clipped framebuffer aspect and narrower FOV retain the 2–100 distance limits. Overview
centers all entity world origins and fits the logical canvas with 24-pixel margins and 4–256 zoom.
Read-only navigation works. Other panels/text input, blocking modals and active or pending native drag
commits cannot invoke framing. The empty-scene button safely no-ops without polling `Nodes()` merely
to enable it. Real 1x/2x tests cover affine unions, empty selection/scene, parity, gates, release/Home,
retained World/Redo and one-shot/stale packets. A 4,001-node fixture excludes an invalid transform and
an invisible distant tail using the shared submission builder. Linux Xvfb verifies Home/pan/Home pixel
restoration for proxy and authored OBJ scenes. Full graphical Scene View acceptance remains open.

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


## Keyboard Rename and Unicode text

F2 in the focused Hierarchy starts Rename for one current selected entity outside text input. The
Rename button and double click share the same generation-keyed action. Opening cancels Scene
and Inspector drafts, focuses/selects the name, and routes editor.scene.rename. Enter commits one
metadata Undo; Escape cancels without consuming history. Rename is a blocking authoring modal:
Scene/Inspector writes, clipboard/Undo/Save/Play shortcuts and queued Hierarchy writes wait or are
discarded until it closes. Its own input/submit remains writable; external recovery/Play-review/close
modals and workspace write loss clear the target. Focus loss or stale entity/document generation
also cancels the draft, which cannot revive on restored access. Empty names remain retryable.

Dear ImGui and direct consumers share the PUBLIC IMGUI_USE_WCHAR32 definition. WindowEvent text
contains Unicode scalar values, so InputText must retain supplementary characters instead of
replacing values above U+FFFF. The host asserts the 32-bit text profile; UTF-8 Rename tests retain
CJK and U+1F642 through real input, metadata Undo/Redo and save/reload. Default font glyph coverage
and physical Windows IME acceptance are separate requirements and remain open. No public Window,
Editor or plugin wire ABI changes.

## Hierarchy Camera/Light creation

The Hierarchy's name row now selects Empty, Camera or Light. Create root / Create child and
Ctrl+Shift+N capture the selected type and name in an owning request; the next frame rechecks
workspace/modal gates and the original scene generation / parent NodeKey before creating anything. Child creation starts at
identity local TRS under the current single selection; the new entity is selected and its parent
expanded. One Undo removes the fully initialized entity, and Redo retains its stable ID/name/defaults.
Selecting a type does not mutate the scene or history. Default names follow type changes; custom
names are preserved. Read-only/recovery/close and stale-parent requests cannot create entities.
Requests borrow no World/entity storage and perform no source IO. Complete reflected component
creation and target-host graphical acceptance remain open.

## Content asset Rename

Focused Content F2 opens Rename for one selected asset outside text input. Context Rename uses the
same owning UUID/project-generation/root/path draft, focuses/selects the complete UTF-8 filename,
and accepts Enter or Apply. Escape/Cancel abandons it; unchanged names close without consuming
Content Undo. Invalid names retain the modal, show the error and restore input focus for retry.
The 1024-byte buffer retains supplementary Unicode input without truncating a portable basename.
Rename blocks Scene/Inspector, File/Undo/clipboard/Play commands even with an inactive name field.
Application focus loss, write loss, hidden Content, external blocking modals, stale project generation,
root, missing asset or changed captured path cancels the draft. Access restoration cannot revive it.
Focus loss cancels immediately in event processing, including loss/regain without a renderable frame;
submitting the unchanged filename clears prior errors while preserving Content Undo.
Successful `ProjectContentSession::Rename` transacts source and sidecar with one Content Undo and
retains document history. Widgets keep no ContentItem/World borrows across frames. Real 1x/2x tests
cover Unicode, retry, cancellation, unchanged names, File/authoring gates and stale scopes; Linux
Xvfb drives F2/Enter followed by scene Save/Undo and restart. Physical IME/font coverage remains open.

## Scene file requests

The File menu and Ctrl+N/Ctrl+O/Ctrl+Shift+S drive New Scene/Open Scene/Save As outside text input;
Ctrl+S retains the ordinary save request. `SetSceneFileContext` copies a project/document token and
optional current relative path. Filename and dirty status appear in the menu bar. One modal collects
a project-relative UTF-8 path, Save/Discard/Cancel for unsaved content, or explicit Replace for an
existing destination. Untitled Save before New/Open collects a nested Save As path while retaining
the original intent. Untitled Save and Exit carries `close_after_save`; the application exits only
after successful persistence. A failed close save reopens Save As with the attempted path and error;
replacement approval resets when retrying a different filename. Bounded dialog width keeps wrapped
diagnostics and the path field usable on reopen. Cancel closes the file modal without mutating the
document.

Requests own all values and are consumed once with `TakeSceneFileRequest`; the application calls
`SceneFileSession` and may return a request through `RequestSceneOverwrite` or
`RequestSceneUnsavedChoice`. Opening the workflow cancels authoring gestures and uncommitted
Inspector drafts. File modals block authoring, clipboard/history, Play commands, and Game input.
Project/document token changes cancel both pending dialogs and emitted requests. Read-only projects
permit Open but disable New/Save/Save As; running Play disables New/Open. Recovery/close/apply dialogs
block new file actions. The application independently rechecks policy and token before I/O. These
are single-active-document controls; additive scene tabs and a native OS picker remain open.

The Content panel's Open scene button, focused Enter, scene-row double-click and context Open scene
copy the native asset path into that same owning file request. A single click only selects. Button/
Enter require one selected `.scene`; non-scene or multiple selections do not activate. Content Open
permits read-only access and follows the project/document, focus, Play and modal gates. It cancels
drafts/gestures and retains the selected destination through Save/Discard/Cancel without requesting
path text again. UI code performs no source IO or World replacement; the application consumes the
request after all panels and revalidates it through `SceneFileSession`.

Recovery presence means any occupied or uninspectable `.nexora/workspace.recovery` path, including
directories and valid/dangling leaf symlinks. Only verified absence permits existing recovery-gated
authoring/export/shutdown actions. Recovery rejects unsafe inputs without mutation. Explicit writer
discard removes one directory entry (never recursively); alias targets remain untouched, and a
nonempty directory remains pending after discard fails. Read-only observers cannot discard.

Profiler Import JSON emits `TakeProfileJsonImportRequest` independently of CSV and export requests.
The application owns synchronous workspace IO and publishes a validated owning static snapshot.
CSV/JSON share the imported plot and Clear imported; failed loads preserve it and live capture.
Project change/detach clears both import requests and the previous snapshot. Read-only projects may
import; recovery/close/modal/no-project states disable both controls. JSON validates current project
identity before publication; CSV carries no project provenance. GPU/memory remain unavailable.

Focused Content Ctrl+A (Cmd+A with macOS behavior) selects the complete current-folder query/type
results, including clipped rows; an empty result clears selection. Read-only projects may select.
Unmodified Delete submits the selected UUID batch to the existing project-local trash transaction,
with one Content Undo restoring sources, sidecars and selection. Prospective Scene/Inspector drafts
are cancelled before deletion. Text inputs, another panel, focus loss, drags, context/rename/file/
recovery/Play-review/close modals and Game capture block keyboard commands. Write controls, including
context Delete/Reimport and Content Undo, also respect workspace write/modal gates.

Focused Content Up/Down and Home/End select matching assets in sorted row order, with held-key
repeat and endpoint clamping. Shift extends or shrinks the inclusive interval around the retained
anchor; Shift-click shares the same model operation. Without a visible selection, Down/Home start
at the first row and Up/End at the last. Empty results leave selection unchanged. Navigation asks
the clipper to submit its endpoint and scrolls it into view. Read-only projects retain these
selection-only operations. Ctrl/Alt/Super variants are not claimed by these routes. Existing text,
focus, drag, Game capture and modal gates apply. Plain selection resets the anchor; Ctrl+A clears
cursor/anchor, and project/root/folder/revision or UI-filter changes invalidate retained navigation
state. Every use rechecks visible membership, including externally changed filters/selections.
State retains owning UUIDs and scope values; no ContentItem borrow crosses frames. Rename and
Delete continue to act on the resulting owning selection.

Content folder rows participate in Dear ImGui Tab navigation. Enter activates the focused folder,
while a single pointer click retains the existing inspection behavior and double-click opens it.
Focused Alt+Up moves to the preceding breadcrumb once per press, stopping at the Content root.
These operations work read-only, preserve asset selection/filter and mutation history, and reset
scroll on navigation. Folder entry suppresses the competing selected-scene Enter route. Keyboard
folder activation and parent navigation share text/focus/drag/modal/Game-capture gates; the folder
list is an owning frame snapshot and navigation is deferred until its iteration completes.
Real Tab/Enter and Alt+Up tests cover 1x/2x, macOS modifier behavior, root clamping, read-only,
text input, another panel, blur and a blocking close modal. A semantic screen-reader bridge and
full accessibility acceptance remain open.

## Game input binding drafts

The stopped Game panel opens an Editor-session input binding modal. Each action exposes Primary
and Alternate choices from the portable profile; movement choices exclude mouse buttons. Apply
validates and publishes the entire owning value, while duplicate controls retain the draft with
an error and preserve the applied profile. Cancel/Escape discards edits; Reset defaults edits only
the draft until Apply. Read-only projects permit these session-only settings. Changing/detaching
the project root/UUID resets the applied profile before the owner restores saved settings; scene
replacement and new Play sessions retain it.

The modal blocks authoring, File and Play requests and cancels Scene gestures/Inspector drafts.
Native blur, recovery, close/file/rename/Play-review prompts and externally started Play cancel
the draft; loss/regain without an intervening frame cannot revive it. Playing and Paused disable
editing. Fixed modal bounds and equal-width columns retain stable hit geometry at 1x/2x.
`GameInputBindings()` returns a copied profile; the application publishes it alongside each native
input batch, including deferred/zero-extent frames. No widgets read/write project settings or
retain World/input borrows. There is no OS key-capture dialog, device-specific persistence, gamepad/pointer
look/multiple-user rebinding or full target-host accessibility acceptance in this slice.

## Project input save requests

Apply remains session-only. Apply and save validates/publishes the draft as the current session
profile, then emits one owning `GameInputBindingsSaveRequest` with project UUID, native root and
profile; `TakeGameInputBindingsSaveRequest` consumes it once. Save is disabled without a writable
project or during recovery. Pending requests cancel on scope/focus/close/conflicting gates. A
failed owner save retains the applied session profile and reports its error; only explicit save
retries can replace project settings. Session Apply clears a previous save-success message.

`SetGameInputBindings` accepts an already validated owning profile and copies workspace scope
during activation, cancelling old drafts/requests. The application loads project settings, uses
defaults for missing state and reports preserved corrupt settings. If startup recovery blocks
loading, it retries once after recovery resolution. `SetGameInputBindingsStatus` reports save/load
results. Widgets perform no IO and do not persist during ordinary shutdown.

## Scalar material assignment

`DrawProductShell` borrows an application-owned `MaterialAssetCatalog` for the current frame.
A single selected Mesh Renderer offers valid typed `.nmaterial` assets; multiple selections state
that assignment requires one object. UI requests own the complete entity key, asset UUID and
project generation and pass through `AssignMaterialAsset` on the authoring thread. Live permissions,
selection, component presence, generations and Content/catalog payload identity are rechecked
before one Undo. The host does not perform filesystem/GPU work or retain borrowed asset pointers.
Missing or unsupported UUID references remain visible and saved; unknown versions cannot be
replaced by this host. Focus loss, hidden Inspector, read-only/modal gates discard queued material
drafts; reopening a gate cannot revive an abandoned request. Inspector reads owning bounded opaque
metadata instead of copying unrelated plugin payloads. Valid Editor-owned schema-1 references are excluded from missing-plugin
inspection, while unknown payloads remain inspectable. Legacy shader IDs stay unchanged. Texture,
graph, multi-selection and Game View material editing remain open.
