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

The Linux display acceptance retains completed Editor launches' stdout and stderr in CTest logs
and rejects Vulkan `Validation Error`, `VUID-*`, and `SYNC-HAZARD-*` diagnostics even if the Editor
exits successfully. This includes selector, read-only/writer conflict, layout, close and durable
SIGKILL/recovery launches; cleanup-only waits cannot replace an earlier failure.
`editor.linux_validation_output` verifies this policy using real subprocesses on both streams.
The `editor-linux-display` CI job enables Khronos core and synchronization validation and retains
separate `editor-validation.log` and `native-validation.log` artifacts, so the later native test
cannot overwrite the Editor evidence. These are software Vulkan/Xvfb results; target-host
physical-display and Windows DPI/IME acceptance remain separate.

The Development desktop CI matrix enables the graphical shell on Linux, Windows/DX12, and
macOS/Metal. It requires `editor.imgui_contract` on every host and
`editor.windows_dpi_ime_contract` on Windows, then retains verbose full-suite output in
`development-tests.log`. The Windows test exercises native candidate positioning and frame-scoped
callback lifetime across repeated 1x/1.25x/1.5x/2x round trips; it does not exercise an installed IME's composition or physical monitors.
Feature-off isolation remains covered by the separate build-contract/mimalloc configurations.
The pinned third-party ImGui target retains AppleClang's zero-length text-replacement warning
without promoting it to an error when that diagnostic is supported; engine/Editor targets keep
strict warnings. MSVC's newly exposed local-shadowing errors are fixed with distinct variable names.
Synthetic Editor tests explicitly choose portable Ctrl semantics and disable event trickling;
production retains ImGui's native macOS Cmd mapping. The undo/redo contract also runs with macOS
behavior enabled and physical Super/Cmd events, preventing a passing fixture from masking that path.

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
Vulkan now retains bounded upload capacity in completed frame slots for Scene/Game geometry.
Steady or smaller draws reuse allocation while copying fresh vertices, indices and instances.
Growing an upload commits replacement only after allocation/binding succeeds; resize and shutdown
wait for GPU work before releasing it. Native call-tracing/pixel tests cover reuse and failure paths.
Authored geometry uses exact sheared world matrices; material shader execution, persistent per-asset
GPU caching and full Scene View acceptance remain open.
Right drag orbits the preview camera, middle drag pans its X/Z target, the wheel zooms, and F or
Frame selected centers on selected forests in X/Y/Z, using exact world-transformed mesh bounds
and rotated proxy bounds, including descendants once. The narrower horizontal/vertical viewport
FOV determines distance (2–100 world units); F and the button use the same current framebuffer
rectangle. Framing uses generation-checked CPU snapshots without source IO and does not consume
GPU upload budgets. Read-only navigation preserves scene/history.
Home or Frame all navigates without changing selection. Native input emits an owning session token;
after widgets the application revalidates it and supplies its actual drawing/picking candidates,
including the 3,999-node cap and upload/proxy fallback. Pending drag commits block the request.
The GUI borrows numeric bounds only for the single issuing-frame application, using existing
affine bounds and distance limits. The overview fits all world
origins to its logical canvas within the existing zoom range. Empty scenes and invalid native bounds
retain the view. Modal/focus/text/drag gates apply; native Linux pixel restoration after pan and
real 1x/2x tests verify the workflow. The X/Z target persists with the
overview camera; target height, orbit angle, and distance persist in a separate per-scene camera file on
writable shutdown. Invalid camera files are preserved for inspection.

Native Scene X toggles existing Global/Local axes (Scale keeps Local); P toggles Pivot/Center.
Focused canvas shortcuts resolve before same-frame click/drag setup and wait during held or pending
gestures. Modifier/text/modal/focus gates preserve authoring; read-only mode navigation is available.
Home/P center gestures now share Q/W/E/R's real-hover guard. Real 1x/2x input and Linux Xvfb proxy/OBJ
center gestures verify unchanged history and one-step Undo; full gizmo target-host acceptance stays open.

Move now adds paired-color XY/XZ/YZ plane handles in Global or Local axes, at the existing Pivot/Center.
Drawing and picking share numeric boxes; a hit captures both axes and retains selection. Explicit planes
override Shift's free-drag Y mode. Preview/release share bounded ray-plane intersections and per-plane-axis
world-unit snapping, then one generation-checked selected-root translation. Scale/Rotate remain separate;
Escape and existing focus/modal gates cancel without authoring. Numeric helpers own no World/GPU data.
Contract tests cover snapping, malformed rays, mirrored/nonuniform parents, descendants and Redo/Undo;
Linux Xvfb verifies all six planes, Shift, Escape and saved proxy/OBJ root transforms. Full gizmo acceptance remains open.

Focused Scene Ctrl+A and Select all change selection only. Overview uses current scene NodeKeys;
native input emits one owning token consumed after widgets and revalidated against SceneFileSession.
`SelectNativeSceneCandidates` borrows the actual drawing/picking packet during the call, omits hidden
or locked records, validates all remaining current NodeKeys and updates selection once. Empty sets
clear selection; stale, malformed, duplicate or over-budget packets preserve it. The native 3,999
candidate cap excludes invisible tails. Read-only projects work. Text/modal/focus/backend and
active/pending-drag gates protect the action; same-frame native framing waits for a later input.
Real 1x/2x, 4,001-node and native proxy/OBJ inputs verify the workflow without source IO.
Select (Q) removes all transform handles from native drawing and picking and suppresses transform
preview/commit requests, while ordinary and Ctrl-toggle entity picking remain available. W/E/R return
to Move/Rotate/Scale. Tool keys require focused Scene/canvas input and respect text/modal/focus and
active/pending-drag guards; keyboard navigation after Home does not mask the actual canvas hover.
Read-only picking works. Real 1x/2x input and native proxy/OBJ saved-byte tests cover this path.
Left click selects the nearest visible position proxy using a viewport ray against its drawn
box; Ctrl-click toggles it, and an empty click clears selection. Hierarchy and Inspector share
that selection. Resolved authored meshes use transformed local bounds followed by two-sided triangle
picking of the same exact world-matrix geometry submitted to Presentation.
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
Pause display freezes an owning log snapshot while Runtime producers continue; filters still apply
to the frozen records. Resume reads current ingress. Clear view hides all records present at the
click, including live records received during pause, while preserving the source buffer and its
cumulative dropped count. Later logs remain visible. Display counts distinguish visible records
from captured/retained records; controls perform no project writes or scene-history mutations.

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
Hierarchy creation now offers Empty, Camera or Light alongside the name field. Create root,
Create child and Ctrl+Shift+N initialize the selected built-in component as one Undo; child poses
start at the parent's local origin. Default names follow type changes while custom names remain.
Queued requests revalidate access and scene/parent generations before mutation. Save/reload retains the
initialized components, names and hierarchy; full reflected component creation remains open.
Inspector Copy values snapshots one selected entity's committed Transform/Euler, Camera or Light
numeric values; Paste values applies the matching type to the displayed selection as one Undo.
Camera/Light Paste preserves absent components, and Transform Paste preserves authored turns.
Read-only Copy is available; write/modal/focus gates apply to Paste. Both abandon unsubmitted
Inspector drafts and Scene gestures. The owning host clipboard survives source deletion/reload,
is independent of hierarchy Copy/Cut/Paste, and is cleared when the host is destroyed. Scene files
and runtime/stable C/Zig contracts are unchanged; complete reflected component editing remains open.
Reset Transform clears local TRS and visible/stale authored Euler revolutions across the selection
as one Undo.
Reset Camera/Light restores existing components to defaults without adding missing components.
Reset cancels pending Inspector drafts and Scene gestures, preserves unrelated payloads and parents,
and follows the workspace/modal authoring gate. Already-default clicks retain Redo; Save/Reload
stores committed defaults. Real ImGui input tests cover 1x/2x scale and mixed component presence.
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

The graphical File menu supports New Scene, Open Scene, Save and Save As with Ctrl+N/Ctrl+O/
Ctrl+S/Ctrl+Shift+S. Scene panel Save uses the same managed current path; Untitled Save opens Save As.
Project startup restores the last successfully opened/saved scene and its per-file view state.
Without valid startup state it opens `.nexora/scenes/Main.scene`, creating a starter root only when missing.
New starts empty/dirty, Open adopts a validated project-relative `.scene`, and Save As adopts its
successfully written destination without changing the original file. Dirty New/Open requires
Save/Discard/Cancel; existing different destinations require Replace. Missing/corrupt Open preserves
the document, history, and current path. Read-only projects allow Open and reject writes. A failed
startup load protects its file from ordinary Save; New/Open/Save As can recover explicitly.
Stop Play before New/Open. Untitled Save and Exit collects a path and exits only after successful
save; rejection reopens the path dialog with its attempted filename and error. Canonical aliases and
case variants cannot bypass reserved metadata policy or save a scene over another file type.
Content paths, folder/asset labels, search and rename input use UTF-8 with native filesystem paths;
Unicode scene discovery, folder navigation, rename/move and Undo avoid system code-page conversion.
Focused Content F2 renames one selected asset with focused/select-all UTF-8 filename input;
Enter/Apply commits one Content Undo and Escape/Cancel abandons it. Context Rename shares this
flow. Invalid names are retryable and unchanged names close without a filesystem transaction.
The modal blocks other authoring/File commands and cancels on focus/write loss, external modals,
hidden Content or stale project/asset path, retaining document history. Linux Xvfb uses real F2/
Enter before Save/Undo/restart; physical IME acceptance remains open.

The active Content scene follows its stable UUID through rename/move and Content Undo. Refresh runs
at authoring mutation boundaries and before Save; unchanged frames use a token/path/browser-revision
cache rather than filesystem polling. It retains document history, dirty edits, selection and the
live view state at the new path. Committed Content relocation updates startup selection even with
dirty document edits; it does not save those edits. Discard and Exit then reopens the relocated
committed source. Deleting or losing the tracked asset blocks ordinary Save until
Content Undo restores it or the user explicitly uses New/Open/Save As. A different UUID at the old
path stays protected. Linux Xvfb drives actual context rename/delete, Save and both Undo workflows.

Saving a Content scene streams only that saved scene and its identity metadata, retaining an 8 KiB
read buffer and 64 MiB limit, then publishes the saved source to the live Content browser without clearing earlier content Undo or reimport state. An import failure is
a Console warning after the scene has successfully saved, never a claim that its save rolled back.
Editor view states remain separate from scene content: the legacy Main scene keeps its adjacent
`.overview.camera`/`.preview.camera`; other paths mirror into `.nexora/scenes/views/<relative-path>`
with camera extensions. Switching retains owning CPU view state for previously opened files and
shutdown persists it only for writable projects and valid, readable view metadata. Untitled view state
has no file destination. Startup selection lives in bounded project/UTF-8 metadata at
`.nexora/scene-session.ini`. Successful Open/Save/Save As records a clean associated file, and
committed same-UUID Content relocation updates its existing filename without saving document edits;
New/failed operations/Discard do not replace the previous choice. Read-only startup restores it
without writing. Malformed, foreign, aliased or unavailable startup data is reported and preserved
for the session while the application falls back to Main. A later metadata write failure warns without
undoing a successful scene save or blocking Save and Exit. Workspace recovery blocks recording.
Additive scenes and complete ED-M2/
ED-M4 graphical acceptance remain open.
The Content panel opens a `.scene` by double-click, context Open scene, or its Open scene button/
focused Enter for one selected scene. It uses the same deferred file request, dirty decision and
managed-path/token validation as File Open, including read-only access. Play and modal gates block
replacement. Non-scene and multiple selections do not activate; opening cancels Inspector drafts
and scene gestures. No source IO or World replacement occurs while drawing Content widgets.
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
Authored meshes use owning exact WorldMatrix values, explicitly transposed from column-major
Runtime doubles to row-major Presentation floats; proxies/gizmos retain their TRS policy.
World bounds and two-sided triangle picking use that same exact matrix without proxy offsets.
Prospective authored geometry uses PreviewSelectionGizmoMatrices and shares local edits with commit.
The frame keeps owning geometry snapshots
through submission, and Presentation copies borrowed uploads before returning.

The shared preview upload is limited to 65,535 vertices and 1,048,576 indices, including the initial
24-vertex/36-index cube. Local positions are limited to +/-100,000; Scene retains its bounded
world-pose eligibility policy and validates affine float conversion through Presentation. Up to 3,999
scene nodes leave room for ground and 96 Rotate handle cubes
within the 4,096-instance contract. Missing/deleted assets, over-budget meshes and unrepresentable
affine transforms use proxies, retain saved references, and emit a Console warning when the
unavailable count changes.
No source IO occurs in rendering or picking. Portable append/picking tests and an Xvfb fixture with
distinct triangle/quad OBJ assets verify geometry ranges, actual silhouette selection, Center scale/
rotation, preview/release pixel equality, and one-step Undo. Closed-form three-level mirrored/sheared
fixtures distinguish exact silhouette hits from lossy TRS misses and check owning matrix conversion.
Material shaders and persistent per-resource GPU caching remain open.

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

Profiler Import CSV reads `.nexora/frame-processing.csv` through the workspace owner and displays a
separate static wall-time snapshot. The file is bounded to 128 KiB/600 ordered samples; malformed,
unsafe, unsupported or recovery-pending input rejects without changing the prior imported trace.
Read-only projects may import. Status and Console report the result; Clear imported changes only
the UI snapshot, while live capture and its export commands retain their behavior. Switching project
root/UUID or detaching clears imported history and pending requests. CSV lacks device/project
provenance and the UI says so; GPU/memory remain unavailable. IO and publication run synchronously
on the authoring thread with owning snapshots and no retained workspace/sample borrow.

Profiler Export JSON saves `.nexora/frame-processing.json` through the same synchronous owner and
atomic writer. Its independent UI request is consumed before adding the current frame, and errors
reach Profiler status and Console. Schema 1 identifies source/scope, milliseconds, project UUID,
sample count and evicted-frame count. uint64 frame/drop values are lossless decimal strings;
GPU/memory availability is false and sample values are null. CSV behavior and file remain unchanged.
Arbitrary import and instrumented GPU/memory traces are still unavailable.

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

The graphical loop sets logical display size and current frame DPI before forwarding native events,
including for deferred and zero-extent frames that still have input to process. Each native batch
also reaches the owning gameplay input snapshot once, even without an ImGui frame or Play tick,
so key releases and focus loss cannot leave held controls behind while rendering is suspended.
EditorImGui converts native client pixels to logical coordinates once and reprojects cached positions across DPI transitions.
Scene gestures cancel on scale changes. The tests drive client-pixel clicks at 200% DPI; native
Windows/physical-display DPI and IME acceptance remain separate target-host gates.

Missing-plugin opaque metadata stays in SceneDocument, participates in the normal dirty/save/close
workflow, and is exposed only as bounded read-only Inspector rows. Opening a version 3 scene does
not load plugin code or transfer opaque authoring bytes into the gameplay clone. Project writer and
recovery checks remain application-owned; format 1/2 scenes remain readable and opaque-free saves
retain format 2. Plugin rehydration is not implemented by this fallback.

The Inspector edits Camera/Light multi-selection through generation-checked atomic SceneDocument
batches. Mixed values/presence are explicit, Enter commits one field, and one Undo restores all prior
per-entity presence/values. Project read-only and blocking recovery/review/close states reject pending
edits. Mesh resource assignment retains its additional writable-content/project-generation checks.

The Mesh Renderer Inspector supports mixed multi-selection. Assigning an imported OBJ preserves each
entity's material reference; removing components affects the selection in one Undo step. Stale
selection/project/document requests and read-only/recovery edits reject the complete operation.

Position/Scale typing is now a cancelable draft. Enter commits one validated field for the current
selection as one Undo step, preserving per-entity rotation and other values; invalid input rejects the
batch. Read-only/recovery/close and Editor-to-Play inspection transitions abandon uncommitted input.
The application continues saving only committed SceneDocument content.

Camera/Light drafts now use the same cancellation lifecycle and disabled controls. Pending component
requests must still match the current selection; read-only, recovery, close confirmation, focus loss,
Inspector collapse and Play inspection discard both typing and queued requests before access returns.

Read-only projects now retain Scene/Hierarchy inspection, picking, Copy and camera navigation while
disabling authoring controls, write shortcuts and drag commits. Recovery, Play review and close
confirmation also discard queued Hierarchy writes; interrupted gestures never commit on later release.

The Game panel's Preview camera chooser selects a renderable active Play camera or Automatic
(first valid camera in entity-ID order). Selection is temporary, resets on Stop/new Play, and leaves
Editor/Play components, scene selection, Undo and saved project settings unchanged. Invalid or
removed cameras fall back to Automatic; frame preparation rechecks the live Runtime camera after
commands and fixed ticks. Read-only projects can choose previews, while modal prompts disable the
chooser. Real UI clicks and Runtime fallback tests cover removal, unloading, invalid native-float
projection and restart with an open chooser.

To place an authored Camera from the Scene 3D view, select one enabled Camera and click Use Scene
view pose in its Inspector. It updates only position/rotation, retaining FOV/clipping, local scale
and parent. One Undo restores the previous pose; Save/Reload retains it. Parent-aware inversion
matches Runtime camera matrices even under sheared/mirrored ancestor transforms. The operation
requires a writable workspace with no modal prompt and cancels Inspector drafts/Scene gestures.

Select one resolved mesh in Content and use Add mesh to Scene to create/select a named mesh root at
the Scene view center. Overview placement uses Y=0; enabled/available Scene 3D uses its target height and the native
+/-100,000 X/Z center clamp.
The action requires writable workspace/content access and no modal prompt. It checks the asset UUID
and project generation against the published CPU catalog, cancels pending view/Inspector gestures,
and records initialized creation as one Undo. Redo retains stable ID, name, transform and component;
Save/Reload retains the resource reference. Existing Scene/Game frame preparation resolves the
created mesh into real geometry. Creation targets the Editor Scene and does not change an active
isolated Play clone; unavailable/stale/non-mesh or multiple selections cannot create placeholders.

Drag a resolved mesh row from Content onto Scene to place it at the pointer's overview X/Z point
or the Scene 3D ray's Y=0 ground intersection. A tooltip shows the prospective placement; only
release authors/selects the named root, with one Undo and stable-ID Redo/Save/Reload. The source UUID
and project generation remain fixed for the drag, and delivery rechecks the current CPU catalog.
Read-only workspace/content, modal prompts, Escape and focus loss discard the drag. Native ground
points outside the current projection or preview position budget reject without consuming history.
This places on the ground plane; mesh-surface snapping and geometry ghost previews remain open.

Deleting a multi-selection now commits all selected subtrees as one transaction. One Undo restores
all roots/descendants, sibling order, component data, names, authored Euler hints, opaque payloads
and the complete prior selection; one Redo removes them again. Selected descendants are collapsed,
and invalid/rejected deletion or replay leaves history intact. The Hierarchy button, focused Delete
key and hovered native Scene Delete shortcut share this document action.

Copy/Paste and Duplicate now retain selected roots with their complete child hierarchies and
Camera/Light/MeshRenderer data. Pasted roots use the captured world pose, children keep local poses
and names, and internal parents map to new stable IDs. Euler revolutions and opaque payloads survive.
All pasted roots are selected; one Undo removes the entire forest and restores prior selection, and
Redo restores initialized values. Duplicate retains the previous clipboard. Source changes after
Copy cannot change the snapshot, and save/reload retains the created forest and resource references.


Hierarchy Cut and Ctrl+X now capture selected forests before one atomic deletion. Undo restores
original IDs and selection; the first successful Paste preserves root names with new IDs, then the
retained snapshot behaves as Copy. Failed operations preserve the previous clipboard; Duplicate
preserves pending Cut state. Workspace/modal/text-input gates apply to authoring shortcuts, and
Cut/Paste/Duplicate cancel Scene gestures and Inspector drafts. Fresh layouts dock Inspector on
the right so it cannot cover the Hierarchy's authoring controls.

Game Apply Changes wraps to another row when its button does not fit, keeping the Play review
action reachable in a narrow dock after a DPI/extent change.


Content mesh drags can also assign the Inspector Mesh field for the current selection. Hover leaves
the World unchanged; delivery resolves CPU catalog data and commits one atomic mesh batch, preserving
existing material references and adding missing MeshRenderer components. UUID/project and target
generations, browser membership, writable workspace/content, focus and modal gates reject stale or
blocked deliveries. Undo/Redo and save/reload retain the resulting mesh references.


Hierarchy Ctrl+A now selects its complete filtered/expanded visible row set, including clipped
rows, while excluding unfiltered collapsed descendants. It works in read-only projects and clears
selection when no row matches. Text inputs and other focused panels retain Ctrl+A; recovery, Play
review and close confirmation block the command. Selection leaves scene content and history intact.


Focused Hierarchy F2 now opens a generation-keyed Rename dialog for one selected entity, focuses
its name and commits on Enter as one metadata Undo; Escape cancels. Rename blocks other authoring,
clipboard/Undo/Save/Play shortcuts and queued Hierarchy writes. External modals, write-access loss,
focus loss and stale entity/document generations cancel the draft. Empty names remain retryable.
Dear ImGui uses a shared 32-bit Unicode text profile so supplementary characters survive InputText,
Undo/Redo and scene persistence; this does not establish default-font glyph or native IME acceptance.

Native Game meshes now use exact live Play WorldMatrix values after fixed ticks, including when
inspection metadata is older. Frame-owned affine instances survive Stop/clone destruction and asset
reimport; no World borrow escapes. Representable small affine scales remain visible to the renderer,
while nonfinite/singular/overflowing float conversions increment unavailable and omit that object
without rejecting the entire frame. `editor.game_view_preview` checks mirrored/sheared ancestry,
stale-snapshot post-tick matrices, clone isolation, frozen geometry and frame ownership.

Project-owned layout save/read is capped at 1 MiB of raw payload. Missing layout permits the default
dock arrangement; corrupt, oversized or aliased layout reports an error without rewriting the file.
Schema 0/1 and CRLF remain supported. Invalid saves preserve the last-good layout and occupied staging.

Recovery presence means any occupied or uninspectable `.nexora/workspace.recovery` path, including
directories and valid/dangling leaf symlinks. Only verified absence permits existing recovery-gated
authoring/export/shutdown actions. Recovery rejects unsafe inputs without mutation. Explicit writer
discard removes one directory entry (never recursively); alias targets remain untouched, and a
nonempty directory remains pending after discard fails. Read-only observers cannot discard.

Profiler Import JSON reads `.nexora/frame-processing.json` through the workspace owner into the same
separate static plot as CSV. Schema 1 requires the current project UUID and exported wall-time scope,
with lossless uint64/double values and explicitly unavailable GPU/memory. The 128-KiB/600-sample
reader rejects unsupported/corrupt/unsafe or recovery-pending input without replacing the prior plot.
Read-only projects may import; live capture and source files stay unchanged. Project detach/change
clears pending JSON requests and imported data. Status and Console expose success/failure.

Focused Content Ctrl+A (Cmd+A with macOS behavior) selects the complete current-folder query/type
results, including clipped rows; an empty result clears selection. Read-only projects may select.
Unmodified Delete submits the selected UUID batch to the existing project-local trash transaction,
with one Content Undo restoring sources, sidecars and selection. Prospective Scene/Inspector drafts
are cancelled before deletion. Text inputs, another panel, focus loss, drags, context/rename/file/
recovery/Play-review/close modals and Game capture block keyboard commands. Write controls, including
context Delete/Reimport and Content Undo, also respect workspace write/modal gates.
