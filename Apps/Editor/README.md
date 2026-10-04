# Nexora Editor

`NexoraEditor` is the standalone process boundary for the Editor Preview. The currently portable
executable opens a versioned project, indexes its `Content` tree, and emits deterministic shell
evidence. `NexoraEditorCore` supplies stable panel IDs and authoring models without introducing a
private presentation path.

```bash
NexoraEditor --project=/path/to/project --report=editor-report.json
```

The default open is read-write and owns the project's single writer lease until process shutdown.
Use `--read-only` for an explicit observer; it validates existing asset identity sidecars and never
upgrades or writes project-owned files. `--recent-projects=PATH` overrides the user-level recent
project store for isolated automation.

This command is a headless workflow/evidence entry point, not the graphical acceptance gate. The
native docking host and its Scene proxy preview use public Window and Presentation contracts;
the complete Scene and Game views remain open.

Configure with `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON` to build the optional
`NexoraEditorImGui` host and its headless draw-data contract test. The option fetches a pinned
Dear ImGui docking release; it remains off by default so the deterministic CLI workflow does not
acquire a graphical dependency.

On a host with a display and native presentation support, launch the shell with:

```bash
NexoraEditor --project=/path/to/project --graphical
NexoraEditor --graphical
```

`--frames=N` supplies a bounded native smoke run for target-host automation. A missing display or
presentation backend is reported as an error rather than silently falling back to the CLI.
When `--project` is omitted, the graphical Project Browser accepts a UTF-8 root and name, supports
create, read-write open, read-only open, and recent-project shortcuts, and keeps actionable
activation errors visible. Its one-shot request is executed by the application on the authoring
thread; a candidate workspace/index/content session replaces the empty state only after every step
succeeds.
The shell tracks the surface's live client extent and DPI scale, rebuilds its bucketed font atlas,
and submits backend-neutral textured/indexed UI draws directly into the acquired native GPU
backbuffer. It also round-trips a versioned project layout and supplies a live `SceneDocument` to
the Hierarchy panel. The same process binds its deterministic `AssetWorkspace` index to a live
Content panel with breadcrumbs, folder navigation, search/type filtering, virtualized UUID-keyed
rows, thumbnail state, selection, typed drag/drop, dependency inspection, and background reimport.
The Scene panel can switch from its editable X/Z overview to a native depth-tested 3D preview.
Vulkan and DX12 draw the ground, resolved OBJ mesh geometry, and proxies for unresolved/unassigned
nodes inside the docked canvas after UI submission, preserving controls outside it. Selection changes
object tint. Each proxy now uses the node's composed world rotation and scale; a conservative pick AABB
filters candidates before a ray test against the rotated proxy or handle box. Thin handles have a
small hit margin so visible edge pixels can be clicked.
Exact sheared world matrices, material shader execution, persistent per-asset GPU caching, and full
Scene View acceptance remain open.
Right drag orbits the preview camera, middle drag pans its X/Z target, the wheel zooms, and F or
Frame selected centers on selected nodes in X/Y/Z. The X/Z target persists with the overview
camera; target height, orbit angle, and distance persist in a separate per-scene camera file on
writable shutdown. Invalid camera files are preserved for inspection.
Left click selects the nearest visible position proxy using a viewport ray against its drawn
box; Ctrl-click toggles it, and an empty click clears selection. Hierarchy and Inspector share
that selection. Resolved authored meshes use transformed local bounds followed by two-sided triangle
picking of the same world TRS geometry submitted to Presentation.
Dragging a selected proxy previews a world X/Z move of selected roots and their descendants, then
commits it when the left button is released as one undoable transform transaction. The Rotate tool
(or E while hovering the canvas) draws X/Y/Z ring handles in world or local space and commits an
in-place rotation of selected roots on release as one Undo step. W returns to Move. Selected roots
and descendants show the prospective rotation while dragging; Escape restores the starting view.
The Scale tool (R over the canvas) shows local X/Y/Z cubes and commits one axis's local scale on
release as an undoable transaction. Selected roots and descendants visibly scale during drag, and Escape cancels the preview. A white camera-facing cube scales all three local components together with a vertical drag; its preview and commit share one Undo step. Hold Shift at drag start to snap rotation to 15-degree steps or local scale changes to 0.25-factor steps; preview and commit use the same snapped value. The shared
Snap movement setting applies the chosen 0.25–4 world-unit step to movement preview and commit.
The X/Z overview retains its axis handles.
`--native-scene-preview` selects this mode on startup for display acceptance.
Project-selector activation now indexes content through an application-owned `AssetImportQueue`;
the selector shows bounded progress and can cancel without activating a partial project. Content
reimport uses the same queue, keeps source/settings hashes and dependency context in its staging
result, and publishes only after the authoring thread revalidates the live asset revision.
Cancelled, failed, or stale jobs leave the previous artifact active and expose structured diagnostic
codes in the Content panel.
Rename/move/delete operate through a recoverable project-content filesystem transaction, with
project-local trash and one-step undo. The real project index creates or validates sibling
`<asset>.meta` identity records; filesystem mutations move those records with their source,
preserving UUID and artifact identity across rename, move, undo, and process reopen. It does not
create a validation-device offscreen target or a full-frame CPU RGBA image. The docked Project panel
shows the stable project UUID, schema and upgrade result, access mode, canonical root, and bounded
recent-project list. Schema-1 projects upgrade atomically to schema 2 only while holding the writer
lease; a read-only legacy open reports that an upgrade is required.

Dirty external changes now block automatic reload and open a Content-panel decision dialog. Compare
shows the retained editor/disk hashes and keeps the conflict open; Reload or Keep records the
terminal authoring-thread choice without direct UI filesystem access. Dependency cycles are also
shown in the Content panel and continue to block artifact publication.

The docked Console now shows bounded structured records with text and severity filters, timestamps,
source, and a dropped-record counter. The Editor records graphical startup and scene open/save
results through the Runtime console; broader gameplay and build log routing remains open.

The docked Game panel now controls an isolated `PlaySession`: F5 starts or stops, F6 pauses or
resumes, and F10 advances one paused fixed tick. The panel inspects copied Play World entity
positions and tick counts. Stop discards the cloned World; the Editor World is not applied back.
An X/Z inspection map draws copied Play World positions, including parented entities, and caps
marker submission at 4096. This is the fallback for unavailable native Game rendering. Native Game
camera/OBJ rendering and optional gameplay callbacks are described below; blank library paths
provide inspection-only Play.

The ED-M2 graphical Hierarchy foundation now renders a parent-aware expandable tree, filters by
entity name, supports plain/Ctrl/Shift selection with a retained generation-keyed anchor, clips
visible-row submission, and routes rename, sibling ordering, and drag/drop reparenting through the
generation-safe, undoable `SceneDocument` contracts. The docked Inspector also edits local position, Euler degrees (quaternion storage), and scale for single or multiple
selections through the same generation-safe document boundary. Mixed fields are explicit, one field
edit applies atomically to the entire selection, and invalid transforms roll back without a partial
write. SceneDocument persists authored Euler hints through save/reload and restores them with undo.
For one selected entity, the Inspector can add/remove a Camera component and edit its field of view
and clipping planes. Invalid values are rejected; Save, Reload, and Undo retain the camera contract.
The single-selection Light section likewise adds/removes a Light and edits nonnegative intensity.
The Hierarchy can create root entities and children of the single selected entity, then selects the
new node. Invalid names and stale parents are rejected, while creation participates in scene Undo.
Ctrl+Shift+N creates a root with the current Hierarchy name outside text input. The Linux Xvfb
acceptance creates one through this shortcut, saves it, restarts the Editor, and checks that both
the starter and created node reload.
The Hierarchy also offers Copy and Paste buttons and Ctrl+C/Ctrl+V outside text inputs. Copies retain
the source's captured world pose even after the source moves; Paste selects the new root entity,
and one Undo removes a single pasted copy.
Delete selected, or press Delete while the Hierarchy is focused or the native 3D canvas is hovered, to remove selected subtrees.
Undo restores the deleted entities and their names; multiple selected roots undo one at a time.
Duplicate or Ctrl+D copies the current selection without replacing an earlier Copy selection.
The central Scene panel also shows a top-down X/Z grid and entity markers at their world positions.
Click a marker to select it, use the wheel to zoom, or drag with the middle button to pan.
Ctrl-click toggles selection, Shift-click selects a visible range, and F or Frame selected centers
the view on the current selection.
Drag a selected marker to move selected objects in X/Z, or drag its red X or blue Z handle for a
single-axis move. The move commits when released, Escape
cancels it, and one Undo restores the previous positions. Snap movement optionally rounds the
whole drag to a selected world-unit step (0.25, 0.5, 1, 2, or 4).
On normal shutdown, writable projects save the overview center and zoom to
`.nexora/scenes/Main.overview.camera`. Reopening restores the view. Invalid camera files are
reported and preserved; read-only projects never write camera state.
The Scene panel marks unsaved changes from live scene content. Undoing an edit back to the saved
content clears the marker; a failed save leaves it visible.
On a native close request with unsaved scene content, the Editor keeps the window alive and offers
Save and Exit, Discard and Exit, or Cancel. A failed or read-only save leaves the dialog open.
External window destruction cannot be canceled and stops rendering.
Scene View, reflected component widgets, the complete graphical save/restart workflow, and the ED-M2 visual exit gate
remain open.

The graphical shell now saves the active scene with Ctrl+S or the Scene panel's Save Scene button
to `.nexora/scenes/Main.scene` under the project root. It reloads that scene when the project
opens and creates a starter root only when no saved scene exists. Read-only projects reject saves;
a corrupt or unreadable saved scene remains untouched and blocks saving until repaired. The Scene
panel reports save and load failures. This is one-scene persistence, not the complete Scene View or
the ED-M2 save/restart acceptance workflow.
The Scene panel exposes Undo (Ctrl+Z) and Redo (Ctrl+Y or Ctrl+Shift+Z) outside text inputs.
Undoing entity creation removes stale node metadata and selection; Redo restores them with the
stable entity ID. New scene edits discard the redo branch.
The docked Profiler shows a bounded history of Editor frame processing wall time measured after
BeginFrame and before Present. Capture can be paused or cleared; the panel reports evicted frames
and labels GPU timing and process memory as unavailable.

This is an ED-M1 graphical foundation, not ED-M1 acceptance. Physical-display and Windows
fresh-project workflow acceptance remain open.

The Linux virtual-display acceptance fsyncs a seeded workspace journal and its directory, launches
the graphical process, and sends SIGKILL before a recovery choice. It verifies unchanged journal
and committed workspace bytes, then relaunches with the writer lease and exercises keyboard-only
Recover and Discard independently. Recovery runs must exit successfully as well as report native
UI rendering. This covers abrupt termination with an existing journal; crash injection during a
workspace write and physical-display/Windows acceptance remain separate gates.

Move, Rotate, and Scale previews now use the same SceneDocument root-edit calculation and Runtime
hierarchy composition as commit. Rotated and nonuniform ancestors therefore produce matching
prospective and committed proxy poses, including unselected descendants; Escape only discards the
preview snapshot.

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
Native pick and mouse-release authoring commands run before Save/Save-and-exit and GPU submission.
Releasing a drag and saving in the same frame therefore persists that completed edit, rather than
the preceding pose. Xvfb XYZ workflows issue Save immediately after release to check this ordering.

### Mesh asset assignment

The single-selection Inspector offers imported OBJ assets from the current project in its Mesh
Renderer selector and can remove the component. It borrows the application-owned MeshAssetCatalog
and ProjectContentSession for the frame; a queued edit carries the entity/document key, asset UUID
and project generation. Publication checks both the live content item and catalog, preserving an
existing material reference when replacing the mesh. Read-only projects and recovery disable edits.
Missing/unresolved mesh references are retained and displayed honestly. Each accepted edit uses
SceneDocument Undo/Redo and cancels prospective scene gestures before mutation. The application
publishes the CPU catalog after project activation. Resolved meshes now replace proxy geometry in
the native preview; OBJ geometry reimport publishes atomically through the live content model.

### Native OBJ geometry submission

The application builds frame-owned shared vertices/indices from immutable catalog snapshots,
packing each resource once and rebasing its 16-bit indices. Ground/proxies/gizmos share the initial
cube range; authored objects use bounded Presentation batches in the same native depth pass.
Authored mesh vertices use the world TRS directly, without the proxy's fixed offset/size. Prospective
and committed geometry share SceneDocument gizmo math. The frame keeps owning geometry snapshots
through submission, and Presentation copies borrowed uploads before returning.

The shared preview upload is limited to 65,535 vertices and 1,048,576 indices, including the initial
24-vertex/36-index cube. Local positions are limited to +/-100,000 to keep bounded world TRS inputs
representable by the GPU. Up to 3,999 scene nodes leave room for ground and 96 Rotate handle cubes
within the 4,096-instance contract. Missing/deleted assets and meshes exceeding preview limits use
proxies, retain saved references, and emit a Console warning when the unavailable count changes.
No source IO occurs in rendering or picking. Portable append/picking tests and an Xvfb fixture with
distinct triangle/quad OBJ assets verify geometry ranges, actual silhouette selection, Center scale/
rotation, preview/release pixel equality, and one-step Undo. Material shaders, exact sheared poses,
and persistent per-resource GPU caching remain open.

OBJ reimport now publishes an owning geometry payload together with the artifact hash, after its
source/project/dependency revisions and live geometry budget pass validation. The application
refreshes its CPU catalog when the live Content Browser revision changes, before drawing native
geometry. Failed/cancelled/stale results keep the prior mesh; rename/move Undo keeps a newer
published geometry and delete/Undo removes/restores live resolution. UI borrows snapshots for the
frame and performs no mesh source IO while rendering or picking.

The Game panel accepts an optional UTF-8 gameplay-library path relative to the project root;
`--gameplay-library=Content/libGame.so` overrides the saved project setting for automation. Start loads
its V3 library only after cloning the World. Component wires read/write the clone, FixedUpdate is
optional and runs on fixed ticks/manual Step, and Update runs once per playing frame. Blank paths
retain inspection-only Play. Canonical paths outside the project and failed ABI/lifecycle loads
are rejected visibly. Module logs enter the bounded Console; failed callbacks pause Play.
Stop/window shutdown unload the module before destroying the clone. The host bounds allocation
and message sizes, supports the shared component wire set, and advertises no scene/physics
capability. Click a playing Game canvas to capture input; Escape, pointer exit, hiding Game, Pause/Stop,
recovery/close prompts, and window blur release it and clear held controls. F5/F6/F10 remain Play
controls; other captured keys/text do not reach authoring shortcuts. The initial user-zero input
snapshot maps WASD/arrows to movement axes and Space/left mouse/right mouse/Shift/Ctrl to button
bits 1/2/4/8/16, with one frame sequence and no borrowed input data. Gamepad, pointer motion/look,
rebinding, multiple users, and module hot reload remain open.

Play inspection uses one owning Runtime snapshot per UI frame. Selecting a Game entity switches
Inspector to read-only Play mode: local/world transforms, parent/scene state, Camera/Light payloads,
and full 64-bit mesh/material shader IDs are displayed as text. Editor mode remains available;
Play selection never changes the scene document's selection. Missing entities clear the inspected ID,
and Stop returns Inspector to Editor mode. Pause reasons and callback failure counts are visible in
Game. Fixed and per-frame gameplay callback failures share RuntimeFailure pause/input-release policy;
native debugger attachment and generic reflected component inspection remain open.

Gameplay library selection persists separately in `.nexora/gameplay-library.ini` (schema 1), using
ProjectWorkspace's writer lease and atomic replacement. Project open reads the setting; Start loads
the module. An explicit `--gameplay-library` (including empty for inspection-only Play) overrides the
saved path. Shutdown saves only for writable projects without pending recovery; unchanged invalid
settings are preserved. Missing settings select inspection-only Play. Invalid/oversized schemas or
relative paths report diagnostics, and actual Start still validates canonical containment/existence.
Linux Xvfb closes and reopens without the CLI path, then verifies the saved module moves native
Game pixels while the authored scene stays unchanged.
