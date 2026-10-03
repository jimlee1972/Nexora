# Editor Dear ImGui host contract

`NexoraEditorImGui` is an optional UI-host module. It owns the Dear ImGui context, translates
public `Nexora::Window` events, applies the Editor theme and DPI scale, creates the root dockspace,
and presents panels using the stable IDs owned by `NexoraEditorCore`. On the first frame it builds
the default workspace with Project and Hierarchy on the left, Console and Content along the bottom,
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
  Delete selected and the Delete key in the focused Hierarchy remove selected subtrees through the
  document. The action is disabled during recovery; Undo restores deleted entities and names.
  Duplicate and Ctrl+D outside text inputs copy the current selection without changing the scene
  clipboard. Paste and Duplicate run before Hierarchy rows borrow node names for the frame.
  The Scene panel has a top-down X/Z overview with a world grid, composed entity positions,
  middle-button pan, wheel zoom, and click selection synchronized with Hierarchy. It is an
  authoring overview; authored 3D mesh output and full 3D gizmos remain open.
  `SceneCanvasViewport()` exposes its visible, clipped canvas rectangle in framebuffer pixels
  after each frame's dock layout and DPI scale. It is empty when the panel is not drawn and is
  reset at `BeginFrame`. The 3D Preview toggle publishes this rectangle to the application for
  a bounded direct native scene draw after UI; the X/Z overview remains available for editing. Backends
  without native scene geometry support show an unavailable message.
  In 3D Preview, right drag orbits the proxy camera, middle drag pans its X/Z target, the wheel
  zooms, and F or Frame selected centers the target on selected nodes in X/Y/Z. Orbit angle,
  distance, and target height are validated state; the target shares the overview's persisted X/Z
  center. The application saves the orbit angle, distance, and target height per scene through
  Editor Core `CameraPersistence`.
  A left click sends a framebuffer-pixel pick request to the application. The application tests
  the same proxy boxes it draws and updates the shared Hierarchy selection; Ctrl-click toggles a
  proxy and an empty click clears the selection.
  Dragging a selected proxy emits start and release positions in framebuffer pixels. The
  application projects both onto the selected node's horizontal plane for X/Z movement, or
  onto its world Y axis when Shift was held at drag start. It draws selected proxies and their
  descendants at the prospective position and commits one undoable world move on release. Escape cancels the drag; optional 0.25–4 world-unit snap steps apply to
  both preview and commit. Full 3D gizmo handles remain open.
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
  preview, not a rendered Game View.
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

## Camera Inspector

The single-selection Camera section toggles component presence and edits vertical field of view,
near plane, and far plane on Enter. Requests use the selected document generation and go through
`SceneDocument::SetCamera`; rejected values show an error without changing the scene.
The Light section uses the same one-shot document path for component presence and finite,
nonnegative intensity, with Undo and scene persistence.

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
