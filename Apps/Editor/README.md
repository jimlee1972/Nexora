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

Windows target-host evidence for ED-M0 is recorded with `Tools/Editor/RecordEditorWindowsEvidence.ps1`.
It runs a bounded real-window smoke automatically, then asks the person at the machine for the
per-monitor DPI, IME composition/candidate and keyboard-only recovery rows. Unanswered or
not-performed rows are recorded as `blocked`, and the overall status is `PASS` only when every row
passes on a machine with at least two monitors running at different scales. The script itself has not been run on a Windows host
yet, so it is not evidence; only a recorded `evidence.json` from a real run is.

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
When `--project` is omitted, the Windows graphical Project Browser shows Open project and Create
project without a raw Project root field or standalone upgrade-preview action. Open selects an
existing `project.nexora` using the Windows file chooser; its containing directory is the project
directory. Create first chooses the actual destination folder, then asks for a display name.
The name is not appended to the selected folder. Cancellation emits no workspace request; chooser
errors stay in the browser. Read-only Open and recent-project shortcuts remain available.
The Windows adapter owns its synchronous COM dialog and releases its result on the authoring
thread, without changing the process working directory. It does not open or create the workspace.
Hosts without a location-picker adapter retain the portable text-path browser and inspection UI.
Existing project validation, writer leases and schema-upgrade backups remain in the owning Core
workspace operation. The one-shot request is executed by the application on the authoring
thread; a candidate workspace/index/content session replaces the empty state only after every step
succeeds.
The shell loads bounded scalable host fonts, optionally configured by
`NEXORA_EDITOR_FONT_PATHS`, and reports coverage/timings to stderr when
`NEXORA_EDITOR_FONT_DIAGNOSTICS=1`. Missing glyphs remain explicit: the measured Linux defaults
rendered 中 and U+1F600, but U+1F642 remains absent; a bitmap-only emoji font does not establish
scalable coverage. An explicitly configured official monochrome Noto Emoji source supplied
U+1F642 in the baseline-corrected diagnostic repeat; no font is bundled.
The [font contract](../../Engine/EditorImGui/README.md#bounded-host-font-coverage)
and [validation record](../../Tools/Build/evidence/EditorEDM0-GlyphCoverage-Linux-2026-10-22.md)
record the passing portable pipeline test, complete Linux suite (366/366, zero skips, 1570.09 s),
static-linkage font test and native preview within its unchanged 45 s bound. Windows/macOS behavior,
physical readability, multi-monitor DPI and installed IME remain separate acceptance gates.

The shell tracks the surface's live client extent and DPI scale, rebuilds its bucketed font atlas,
and submits backend-neutral textured/indexed UI draws directly into the acquired native GPU
backbuffer. It also round-trips a versioned project layout and supplies a live `SceneDocument` to
the Hierarchy panel. The same process binds its deterministic `AssetWorkspace` index to a live
Content panel with breadcrumbs, folder navigation, search/type filtering, virtualized UUID-keyed
rows, thumbnail state, selection, typed drag/drop, dependency inspection, and background reimport.
The [bounded mesh thumbnail worker](../../Engine/Editor/README.md#bounded-asynchronous-mesh-thumbnail-snapshots)
now supplies revision-checked owning CPU shape previews for future Content image presentation.
Native image upload/drawing in these rows and other asset thumbnail types remain open.
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
Authored geometry uses exact sheared world matrices. [Scalar opaque PBR materials](#authored-scalar-pbr-scene-materials)
now execute in Scene View; complete texture/shader workflows, persistent per-asset GPU caching and
full Scene View acceptance remain open.
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
results through a real bounded Core AsyncLogService. V3 gameplay records use the same producer;
Core Trace/Debug map to Console Trace, and Info/Warning/Error/Fatal preserve their severity.
Broader Runtime/build log producers and native debugger routing remain open.
Pause display freezes an owning log snapshot while Runtime producers continue; filters still apply
to the frozen records. Resume reads current ingress. Clear view hides all records present at the
click, including live records received during pause, while preserving the source buffer and its
cumulative dropped count. Later logs remain visible. Display counts distinguish visible records
from captured/retained records; controls perform no project writes or scene-history mutations.

### Core producer-to-Console ingress

The application owns a bounded Core async log producer and a noncopyable polling adapter for this
one graphical session. The adapter binds one producer/Console pair whose lifetimes outlive it.
Every loop polls only copied already-consumed records; it never flushes, joins or invokes UI from
a producer thread. Source text is validated bounded UTF-8 and compacted. Monotonic cursors avoid
duplicate delivery; unread Core-ring sequence gaps and rejected producer/wire counts enter the
Console's saturating visible drop counter exactly once, independently of Console capacity evictions.
Producer records already observed before Core-ring eviction are not counted again as source loss.

V3 Play logging rejects unknown levels, messages over 4 KiB, missing nonempty pointers, embedded
NUL and malformed UTF-8 before copying; it never truncates Unicode or silently converts an unknown
level. Null/zero messages are empty records. A rejection callback accounts lost wire data without
allocating its payload. The underlying Runtime V2 bridge uses the same Core policy with its 16-KiB
message budget. Module unload/Stop records reach the producer before clone destruction; shutdown
then drains Core and polls its final owning records before Console teardown. Graphical shutdown
prints bounded producer/retained/drop/gameplay-retained counts for native acceptance fixtures.
This routes actual Core/Play producers, without claiming complete engine/build/device log routing
or debugger/IDE integration. [Linux evidence](../../Tools/Build/evidence/EditorEDM3-CoreConsole-Linux-2026-10-09.md).

### Logical Play input ownership

The Game panel's session-only User0–3 selector assigns its one keyboard/mouse source to one logical
input owner. Selection changes release UI capture and clear the host's held keys/buttons; the
existing acquisition handshake discards the next focused batch. Project activation resets the UI
selection to0; module unload resets the host to0. Stop/Pause still revoke held state; the UI session
choice can remain until the project changes. Existing project input profiles stay shared and retain
their schema; selecting a user emits no settings/save request or authoring command.

The existing `capture_input(user,snapshot)` callback accepts0–3. The selected user receives copied
held values; every other user receives a neutral owning value with the same frame sequence and zero
reserved field. Repeated capture does not consume input. Out-of-range/null capture rejects before
writing output; invalid owner forwarding revokes focus while retaining the selected identity. The
default remains0. The logical-user selector preserves the legacy input wire and module dependencies;
public C++ consumers rebuild. This routes one source among logical lanes, without simultaneous physical
users, gamepad, relative-pointer or pointer-look claims.

Focus loss or a pressed Escape revokes `PlayInputState` before discarding the acquisition batch.
Release wins over all other events in that batch, regardless of ordering. Subsequent batches stay
neutral until the owner explicitly reacquires focus; reacquisition still suppresses its first batch.
The sequence advances for neutral batches and previously copied snapshots remain unchanged.

Runtime `PlaySession` releases its own focus on Pause and accepts focus grants only while Playing.
Resume requires explicit owner reacquisition. The native forwarding adapter also clears its separate
held input state when the session is paused, so manual Step never reuses a captured key batch.

### Scoped build output-to-Console ingress

The native owner forwards its current BuildProcess retained merged stdout/stderr to the same
Runtime Console. Numeric project scope/operation labels accompany owning ASCII-escaped records;
all control/non-ASCII bytes are represented exactly as hex escapes. Timestamps identify wall-clock
receipt time, not producer execution time or stdout/stderr ordering. Output chunks carry Info,
not inferred severity. Per-frame processing consumes at most 16 KiB raw bytes in four 4096-byte chunks,
each within the 16 KiB Console message budget; producer loss adds at most one Warning record.

Cumulative retained-byte offsets avoid duplicate forwarding. Missing raw bytes enter a separate
saturating byte counter and an explicit warning; Console drops remain record counts. New operations
reset only their cursor. Foreign, detached and stale scope observations are ignored; regressed,
oversized or overflowing snapshots reject before changing history. Console admission failure
consumes that chunk once and uses the existing Console rejection counter. Allocation failure leaves
the current chunk retryable. Shutdown cancels/joins the runner, then drains its at-most 256 KiB
retained snapshot in bounded passes before Console teardown. Output can contain child-produced
private data and remains session-only, just like the existing local Build Console; this adapter
adds no persistence/transmission, command/environment capture, authoring write or artifact claim.

The docked Game panel now controls an isolated `PlaySession`: F5 starts or stops, F6 pauses or
resumes, and F10 advances one paused fixed tick. The panel inspects copied Play World entity
positions and tick counts. Stop discards the cloned World; the Editor World is not applied back.
An X/Z inspection map draws copied Play World positions, including parented entities, and caps
marker submission at 4096. This is the fallback for unavailable native Game rendering. Native Game
camera/OBJ rendering and optional gameplay callbacks are described below; blank library paths
provide inspection-only Play.

### Optional 2D services in project Play

`EditorApp` declares its direct dependency on the optional `TwoD` module in the module graph;
the graphical executable links `Nexora::TwoD` explicitly only when 2D is enabled.

With `NEXORA_ENABLE_2D`, selecting a gameplay library in a project containing typed Atlas/Clip
Content now prepares `PlayTwoDBinding` at Play start. `CapturePlayTwoD` selects all recognized
2D sources, capped at 4096, including recognized sources with unavailable typed payloads so they
fail admission. It captures the active document's bounded Runtime scene identity and delegates
writer/lease, project UUID/root, source/sidecar, conflict, generation and dependency validation to
`ProjectTwoDPlayAssets`. The project Content generation is also the private cooked-generation
label; it does not reserve a global Runtime generation. Capture errors are shown through the
existing gameplay status/Console. Immediately after cloning, the host repeats `Matches` with
current authoring authorization before module activation. No library or no 2D sources preserves
the existing inspection/3D path. A project with 2D sources must pass capture even for a 3D library.

`PlayTwoDServices` borrows that existing Play World and its selected active scene, retains the
immutable cooked Atlas/Clip closure, and owns fresh Sprite/collider metadata, a 4096-instance pool
and the canonical `GameplayBridge`. It creates no second World and copies no authoring Sprite
metadata. Clip resolution survives project/index/store destruction. API entity operations are
limited to the bound scene; scenes created through the generic scene service are not automatically
2D scenes. Tilemap configuration is not supplied, so tile callbacks return `UNSUPPORTED`.
Animation advances only when gameplay calls the 2D animation API; the host adds no second clock.

`PlayGameplayModule` constructs the V3 host table after binding, advertising the 2D capability and
getter only for an attached owner. Module load/replacement failures clear the candidate owner;
Stop/Destroy callbacks still have their World, frozen assets and pool. The module is destroyed
before the 2D services, which are destroyed before `PlaySession::Stop` releases the clone. Every
ID in a successful generic despawn's `LastDestroyed`, including descendants, is forgotten by the
2D bridge before subsequent queries. Other direct World destruction paths must still explicitly
notify the bridge. All host calls serialize on the Editor owner thread. A copied 2D table expires
when its module is destroyed; it must never be called after unloading or across sessions. Existing
pool owner/generation validation rejects handles reused with a fresh session. Feature-OFF builds
retain the ordinary host and do not include/link these optional services.

`editor.play_twod_services` exercises actual project source/cook capture and the same host used by
`NexoraEditor`, with 32 clone lifetimes, failed Create/Start, full module replacement, frozen
animation after external owners die, descendant cleanup, fresh pool handles, wrong-thread bridge
rejection, source/history conservation and capability-free 3D loads. This is CPU integration
acceptance on its own; bounded native main Game View acceptance is described below.
Authored Sprite persistence, Tilemap binding, seamless hot reload and full physical acceptance
remain open. ED/2D milestone counts do not change.
[Linux validation](../../Tools/Build/evidence/EditorEDM3-MainPlayTwoDServices-Linux-2026-10-16.md).

### Bounded 2D Game View

With an attached 2D gameplay bridge, the Game panel offers explicit 3D/2D view selection.
3D is the default for every new Play session, including mixed projects with incidental Atlas/Clip
assets. Stop or a change of immutable Play owner resets the selection and releases 2D input focus.
The 2D view
uses the gameplay camera and the same cloned World, Sprite metadata and pool as the canonical
API. Changing views mutates no World or authoring history. A disjoint native 3D Scene can remain
visible even without support for multiple native 3D views. If the physical rectangles overlap,
native Scene drawing is suppressed so it cannot overwrite the GUI canvas.

`PlayTwoDServices::ExtractFrame` takes one owning CPU snapshot after Play commands/fixed ticks/
Update and before GUI EndFrame. It sorts World poses by entity ID, appends pool slots in their
stable iteration order, assigns unique draw identities and uses canonical Sprite sorting/extraction.
The frozen multi-Atlas resolver retains immutable owners in the returned packet. Render does not
advance animation. Pool and scene inputs share a 4096-Sprite budget; the pose snapshot is capped
at 100,000, and nonzero framebuffer extents at 16,384 per axis. Invalid poses, budgets or thread
ownership return no packet. Missing/invalid Atlas references or unavailable GUI registrations
contribute to the displayed unavailable count; valid sprites can still draw.

`GameTwoDTextures` owns registrations scoped to the exact immutable Play assets owner, never a
borrowed project model or mutable UUID lookup. At most 64 Atlas registrations are retained per
Play asset scope, sharing the GUI host's 64 slots and 16 MiB of CPU RGBA copies. Each image is at
most 1024x1024; unsupported larger images are reported unavailable without changing source data.
The native UI packet can include the font alongside all 64 images; Metal admits 65 uploads,
including a full resend after a DPI rebuild or surface domain change.
Already registered Atlases are reused; dynamically changing to additional Atlases can exhaust the
session budget until Stop. Shared GUI texture pressure rejects allocation without publishing a
zero/stale handle. Native registration may occur during frame assembly before Render; scope change,
Stop or project switch clears CPU registrations before the next BeginFrame. No former Play quads
are appended after Stop. On shutdown, Clear runs after drawing ends. The existing bounded native
cache retains slots until reuse or surface drain, retiring replaced resources behind completion;
this does not claim immediate GPU deallocation or an RSS/VRAM bound at Stop.

`DrawGameTwoDSprites` copies at most 4096 uniform-tint indexed quads into the current Game window's
own ImGui draw list after Play mutation and before EndFrame. It admits one packet per frame and
validates all ranges, live native texture generations, finite coordinates, aggregate quad counts
and physical clip rectangles before any geometry is appended. No upload spans, World/asset borrows
or caller geometry survive the call. Physical positions and clips convert through the current DPI
scale, retaining ordinary GUI window stacking and panel clipping. Source Sprite IDs/pool handles
and stable C/Zig ABI layouts are unchanged. This preview uses the existing indexed GUI path;
it does not accept the standalone instanced stress/performance or physical target-host gates.

`editor.game_twod_preview` covers actual view clicks, multi-Atlas/pool order, Step, shared texture
pressure, foreign/stale owners, malformed packet atomicity, expired frame rejection, 2x DPI,
concurrent/overlapping canvases and Stop/source conservation. `editor.linux_native_game_twod`
runs the actual project Editor and V3 library for 32 native cycles, validating blended Atlas/pool
pixels, Pause/Step/input/Stop, concurrent 3D source pixels and unchanged authoring files.
[Evidence](../../Tools/Build/evidence/EditorEDM3-GameTwoDPreview-Linux-2026-10-16.md).
Authored Sprite persistence, Tilemap binding, seamless DLL reload and physical acceptance remain
open; enclosing ED/2D milestone counts are unchanged.

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
On Save All or Save and Exit, the writer prepares the ordinary `.nexora/scenes` directory when
an owned scene uses that default path, without saving scene bytes. Startup does not create this directory.
Read-only, recovery-blocked and externally changed workspaces cannot
prepare storage; occupied files or aliased directories are preserved and rejected. This allows
the first Save All or Save and Exit to use the unchanged batch requirement for existing ordinary
parents, exact baselines and unoccupied sibling staging paths.
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
Selecting one failed asset shows its initial import reason under Asset details, with wrapped
literal text. The owning diagnostic is limited to 1024 UTF-8 bytes and remains available in
read-only projects. Changing/clearing selection, hiding the panel or closing Content clears the
displayed observation. Successful reimport clears the resolved failure from the item and its
Content Undo snapshot; failed reimport preserves the item and reports the current operation
separately. The existing Reimport action can retry an initial failure without an old artifact,
using explicit recovery admission and the same publication revision checks. These diagnostics
are reconstructed from indexing rather than persisted in scenes.
Asset details offer UUID-scoped Reveal buttons for both Dependencies and Referenced by. A live
indexed target opens its Content folder, clears search/type filters and becomes the sole selection;
clipped targets are scrolled into view on the next frame. This also works in read-only inspection
and does not open a scene, reimport, write source/sidecars, change the content revision or replace
pending Content Undo. Missing targets retain a literal unavailable UUID and disabled action.
Input ownership/modal gates apply; deferred scrolling checks current project root, generation,
revision, folder and selection, and is discarded when Content closes or its panel is hidden.
Names are printed literally, including Unicode and ImGui marker characters.
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
and plots separately measured native GPU intervals. After a successful Present, the application separately calls
`ProfileSession::SampleProcessMemory` to observe real current process RSS / working-set bytes at
most once per 250 ms. The Profiler displays a copied latest optional value and observed peak since
Clear, in bytes, including shared resident pages. A read failure makes latest unavailable and
preserves the historical peak. Capture pauses memory observations as well as wall-frame ingestion;
Clear resets observations and sampling time without resuming paused capture. The next capturing
frame may sample immediately. Scope remains process-wide across project switches/detachment and
does not measure GPU memory or allocator ownership. The bounded shutdown diagnostic reports
scope/unit, attempt/success counts and latest/peak, without paths or project data.

A separate live Profiler channel copies Presentation's actual retained native mesh allocation bytes
and CPU comparison-key payload bytes after successful Present. It identifies the current native
surface's geometry cache and samples at most every 250 ms into a fixed 600-record history. Missing
observations break plotted lines, zero is valid, domain changes separate owners and Capture/Clear
control observations without releasing resources. Native byte counts include adapter allocation
requirements; CPU values exclude allocator overhead. The channel excludes textures/frame uploads,
process RSS and total device-local VRAM. Explicit mesh export/import uses its separate schema-1
file and owning static display; existing exports retain their own wall/process/GPU scope. C++ ProfileSession consumers must rebuild; stable C/scene formats do not change.

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
Material shaders and broader per-resource residency policy remain open.

Vulkan, DX12 and Metal Scene/Game preview now request optional immutable mesh geometry residency through the
surface capability. Exact-content keys reuse unchanged meshes when the combined frame layout or
instance/material data changes; frozen Play geometry can coexist with reimported Editor versions.
Current CPU catalog payloads are still required, so unavailable assets follow existing proxy/omission
rules. The adapters support 4,096 batches while bounding distinct-range admissions/entries to 64
and resident native allocations to 32 MiB, with a
bounded staged candidate; unsupported/large/failed admissions retain legacy uploads. Resize retains
immutable entries, and completion-safe eviction/shutdown release ownership. CPU/native cache
diagnostics are not VRAM measurements. Full material/shader authoring, broader cache policy and ED-M2 acceptance remain open; see
[ADR-0008](../../Roadmap/en/ADR-0008-Editor-Mesh-Geometry-Residency.md).

Frame geometry append prepares both bounded vector capacities before publishing any vertices or
indices. Allocation failure returns the existing unavailable-mesh fallback without changing element
contents, sizes or prior ranges; reserved capacity may grow. Growth remains geometric within the
upload limits, and a fitting append performs no allocation. This covers only geometry vector growth,
not every allocation in the frame builder. An isolated executable faults each real vector allocation
and verifies unchanged attributes/ranges and successful retry with correctly rebased indices.

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
and message sizes, supports the shared component wire set, and advertises implemented Scene API
and host allocator capabilities. Play diagnostics, frozen mesh UUID resolution and bounded opaque
debug lines are available as described below. Gameplay simulation additionally enables bounded
CPU AABB Physics spawn and copied raycast. Click a playing
Game canvas to capture input; Escape, pointer exit, hiding Game, Pause/Stop,
recovery/close prompts, and window blur release it and clear held controls. F5/F6/F10 remain Play
controls; other captured keys/text do not reach authoring shortcuts. The default user-zero input
snapshot maps WASD/arrows to movement axes and Space/left mouse/right mouse/Shift/Ctrl to button
bits 1/2/4/8/16, with one frame sequence and no borrowed input data. Gamepad, pointer motion/look,
device-specific rebinding, multiple users, and module hot reload remain open.

### Concurrent native Scene and Game previews

The render owner supplies `SupportsBoundedSceneViews()` before each UI frame. When both native
panels are visible and their clipped physical-pixel rectangles do not overlap, Scene renders the
current Editor World and Game independently renders the isolated post-tick Play World. They share
one acquired frame/UI submission with separate scene uploads. The Play camera chooser, frozen
mesh/material snapshots, debug ribbons, input focus, Pause/Step/Stop and source/Undo isolation keep
their existing behavior. A view with no renderable geometry does not consume scene admission.

Unsupported backends retain Scene/map precedence. Overlapping native rectangles suppress both native
draws and show an actionable UI/inspection fallback; moving the windows apart restores the pair.
This avoids fatal overlap descriptors and native draws covering the other panel's fallback. The
bounded path admits only two explicit direct LDR/non-shadow views after UI. Full-surface, offscreen,
HDR and shadow workflows stay exclusive. General floating-window occlusion, advanced multi-view
materials/HDR and physical-host acceptance remain open. No source format or stable C ABI changes.

See [ADR-0009](../../Roadmap/en/ADR-0009-Editor-Concurrent-Canvases.md) and
[Linux evidence](../../Tools/Build/evidence/EditorEDM3-ConcurrentCanvases-Linux-2026-10-14.md).

### Frozen Play mesh resolution

Before cloning Play, `FreezeGameMeshes` validates the current project generation and copies the
published CPU mesh catalog into one immutable owning instance. The module binding and native
Game View share that instance, including during module Create/Start. Copy failure rejects Start
before changing the World. Authoring reimport/delete/replacement and later project publication
cannot change an existing binding. This is the existing Play catalog copy shared by two consumers,
not a growing per-request cache. Geometry is owning CPU data; no filename, World/entity or native
resource borrow crosses the callback.

The existing V3 `resolve_asset(high,low,out)` callback resolves imported mesh UUIDs only and returns
the exact persistent 64-bit `MeshResourceId` used by Play mesh spawn/render. The catalog verifies
the full UUID after resource lookup, so colliding folded identities cannot alias another asset.
Zero/unknown/non-mesh UUIDs or null output return InvalidArgument; an unbound World returns
Lifecycle, and an embedding without a mesh provider returns Unsupported. Failed calls leave output
unchanged. A supplied snapshot must be immutable and have nonzero generation; the native application
uses `FreezeGameMeshes`, which also checks the current project generation. Static and dynamic loads
retain the catalog through Stop/Destroy, then release it before clone destruction. A failed load
also releases it. Calls are serialized on the owner thread and perform no source IO or asset import.

The application publishes the same snapshot to the renderer only after successful module startup.
Inspection-only Play uses the same freeze boundary. Existing geometry/source budgets and native
residency ownership stay unchanged; this is not a total memory/GPU/physical-soak claim. Stable C
wires/capability bits, UUID/resource derivation, scene/project formats and module dependencies are
unchanged. Matching private C++ application consumers rebuild. Material/texture/audio resolution,
dynamic materials and full gameplay/expanded devices remain open.

### Bounded Play debug lines

The existing V3 `debug_draw_line` copies an opaque world-space request on the serialized owner
thread into a fixed 256-entry queue. `rgba` is 0xRRGGBBAA with alpha 255; RGB bytes become linear
instance tint factors. Reject nonfinite coordinates, absolute coordinates above 99,999, segments
of length at most 0.000001, nonopaque alpha and durations outside finite [0,3600] seconds with
InvalidArgument. Null input also returns InvalidArgument; an unbound clone or a full queue returns
Lifecycle. Rejection preserves the queue and contributes to `api_errors`. This callback introduces
no allocation, source IO, World/entity borrow or stable ABI/capability change.

The application advances the queue once before fixed ticks and Update in each playing frame,
using its admitted gameplay delta. Positive durations expire at the exact remaining-time boundary;
zero-duration lines survive until the next advance. Use a positive duration for startup lines that
must survive the first playing boundary. Paused rendering and fixed Step never age requests; Step
callbacks may append lines. Invalid deltas leave the queue intact. Stop/Destroy retain access, then
Unload and failed startup clear all entries. Queue capacity is per current binding, not historical.
`DebugLines()` borrows only until the next queue mutation; frame preparation copies all geometry.

Game View appends camera-facing, two-sided, depth-tested 0.06-world-unit ribbons to the Game
view's existing native scene submission. A camera with no meshes can display them. Ribbons use ordinary
neutral-slot Lambert/PBR lighting and opaque tint; they are not an unlit HUD, alpha blend, fixed
pixel-width stroke or occlusion override. Native Scene canvas precedence and unsupported-backend
fallback remain unchanged. Existing 65,535-vertex/1,048,576-index/4096-instance budgets apply;
authored meshes take priority, and omitted debug requests are reported. The full 256-line queue
adds at most 1024 vertices/3072 indices/256 instances. Temporary ribbon arrays are reused within
frame preparation; empty queues return before scratch allocation. Segment-direction tangents remain
orthogonal to camera-facing normals for PBR. Frames containing debug geometry use fence-owned uploads instead of retaining
transient combined meshes in the immutable geometry cache; other native ownership is unchanged.

[Linux evidence](../../Tools/Build/evidence/EditorEDM3-PlayDebugLines-Linux-2026-10-14.md).
Complete gameplay, expanded devices, hot reload and physical-host acceptance remain open.

### Play scene/entity services

The V3 embedding binds only a Play-kind World; static and relative dynamic loads reject an Editor
World. `load_scene` creates an empty in-memory scene, with an owning NUL-free UTF-8 name of 1–256
bytes and persistent flag 0/1. It performs no project IO or scene-file interpretation. `activate_scene`
uses the loaded-inactive lifecycle, and duplicate/missing/unloading activation fails. `spawn_entity`
validates the admitted descriptor prefix, known flags, reserved zero, finite root position, enabled
Camera FOV (0–180 exclusive), nonnegative finite Light and nonzero Mesh ID before publication.
Camera clipping defaults to 0.1/1000. Material zero is neutral; full uint64 mesh/shader IDs are values,
with resolution remaining separate. Unused component fields/future descriptor suffixes are ignored.
Physics bounds use the optional query binding described below; feature-off requests return
Unsupported. No partial entity or output is published. Missing/unloading
scenes and malformed descriptors return InvalidArgument; unbound, exhausted quotas/IDs or allocation
failure return Lifecycle. All output arguments retain their prior values on failure.

One module binding admits at most 32 scenes and 4096 spawns over its lifetime, counting successful
admission. Despawn uses atomic World commands and removes descendants; it does not replenish quotas.
Authored cloned entities can be despawned in Play, while Editor data/Undo remain isolated. Callback
calls are synchronous on the serialized game thread, borrow wire pointers only for the call and
return scalar IDs. Module Stop/Destroy still have access; the owner clears binding/quotas after
Unload and before clone destruction. Stop discards created/deleted entities; transform Apply Changes
continues excluding creation/deletion. Debug line admission is described below;
raycast is null when gameplay simulation is disabled. Modules must check each optional
callback. Stable C/Zig layout and scene formats are
unchanged. [Linux evidence](../../Tools/Build/evidence/EditorEDM3-PlaySceneServices-Linux-2026-10-09.md).

### Play copied diagnostics

The V3 `get_diagnostics` slot returns an owning `NexoraFrameDiagnostics` on the serialized owner
thread. `frame` counts admitted finite/nonnegative Update invocations of this module binding,
advancing before the module callback, including a callback that reports failure. Create/Start see
zero; fixed ticks and paused Steps do not advance it. This is a gameplay Update sequence, not a
render/presentation, fixed-tick, wall-clock or GPU timestamp. Nonloaded/invalid Update attempts do
not advance it.

`scene_entities` sums current entity-vector sizes in active scenes of the bound Play clone.
Inactive/unloading/unloaded scenes are excluded immediately; spawn/despawn and activation are
visible on the next query. The query allocates nothing, traverses scene records only and retains
no World/entity borrow. `debug_lines` counts currently retained debug requests, including requests
omitted by camera/clipping/render budgets; it is not a native draw/pixel count.
`api_errors` counts non-OK results returned by the host's integer-result service callbacks,
including rejected diagnostic requests. It excludes void log/deallocate operations, pointer-result
allocation, gameplay callback failures and all external/process/native validation errors. Those
existing diagnostics keep their separate channels. Successful observations do not change counters.

Both counters saturate at UINT64_MAX and reset per binding. Create/Start and Stop/Destroy may query
the bound World; state clears after module destruction and before clone teardown. A null output
returns InvalidArgument; an unbound host returns Lifecycle without changing output bytes. No module
host mutex is queried recursively from the callback. The caller must stop using copied callback
pointers before the embedding object dies. Stable C wires/capability bits, source formats and
Editor authoring/history remain unchanged. Matching C++ consumers rebuild for the additive Runtime
count query; native debugger/IDE and expanded device services remain separate work.

### Play CPU collider queries

When `NEXORA_ENABLE_GAMEPLAY_SIMULATION` is ON, `NEXORA_SPAWN_PHYSICS` admits a finite, ordered
local AABB into a fixed owning 256-entry entity/scene binding array. Degenerate bounds, including
planes, follow the existing `PhysicsWorld` contract. Initial root translation must leave both bound
corners finite. Validation and live quota checks precede entity creation; subsequent scalar
assignments cannot throw. Unused bounds remain ignored for non-Physics descriptors. Despawn removes
bindings for the full atomic deletion cascade. Queries and later spawns prune externally deleted or
unloading/unloaded entries. Despawn releases the live collider budget, while the existing 4096
lifetime spawn quota is still consumed. Bind resets both; Clear follows module destruction.

The nonnull V3 `raycast` callback uses a fresh real CPU `PhysicsWorld` for each serialized query.
Each active-scene collider's eight local corners pass through its current exact World matrix,
including inherited rotation, nonuniform scale, reflection and shear, to build a conservative
world AABB. This is an AABB query, not oriented-shape collision. Inactive scenes are excluded.
It does not retain a stale proxy, move bodies, step rigid-body simulation or import authored
Physics components. Authored Editor World and Undo remain isolated.

Finite nonzero direction is normalized with maximum-component scaling before the real Runtime
query, including very large and subnormal directions. Finite distance must be nonnegative. Copied
hits contain entity ID, distance and point; equal-distance hits select the lower body/entity ID
independently of unordered insertion. Invalid/null requests and misses return InvalidArgument;
unbound, allocation failures and overflow/invalid live collider matrices return Lifecycle.
The whole query fails on an invalid live collider rather than returning a partial hit. Outputs
remain unchanged on every failure. Stop/Destroy retain access; Unload clears before clone teardown.
Feature OFF leaves the host slot null and direct valid queries/Physics spawn return Unsupported.
Stable C/Zig layouts and capability bits are unchanged; no additional capability is advertised.

Actual V3 and Linux dynamic/native Game fixtures verify copied hit distance/point/entity,
despawn misses, ownership, transform/shear updates, tie resolution, quotas, unload, Stop/Destroy,
unchanged Editor bytes and real normal Game movement. Enabled and simulation-OFF tests are recorded
in [Linux evidence](../../Tools/Build/evidence/EditorEDM3-PlayPhysicsServices-Linux-2026-10-09.md).
Rigid-body/backend integration, authored collider import, non-mesh asset resolution,
native debugger and complete gameplay/input acceptance remain open.

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
cells because this wall-time format excludes live RSS observations. Export requires 1-600 strictly increasing nonzero
frame IDs with finite nonnegative wall times. Empty/invalid/read-only/recovery exports fail without
replacing the last good file. UI emits a one-shot request, disables export without samples/write
access or during recovery/close confirmation, and shows the application's result; UI never writes a
file itself. Arbitrary capture import remains open; native GPU and process-memory traces use separate schemas.

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
Arbitrary import remains open; measured native GPU and process-memory traces use separate files.

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

Content Up/Down and Home/End now select the current matching asset rows, repeat while held, clamp
at either endpoint and scroll clipped targets into view. Shift navigation and Shift-click extend
or shrink one inclusive anchored interval. These are selection-only, including read-only access;
the same focus/text/drag/modal/Game-input gates apply. Cursor/anchor are owning UUID state checked
against current visible membership and invalidated by scope changes. Keyboard Rename and batch
Delete use that selection. Folder-row keyboard traversal and full accessibility acceptance remain
open.

## Session Play input rebinding

The stopped Game panel now edits two control slots for each of four movement directions and five
button actions. Apply publishes one validated session profile; duplicate controls reject the
complete candidate, None unbinds actions, and Cancel/Reset preserve the applied profile until
explicit Apply. Read-only inspection permits these settings. Switching/detaching projects resets
session state before project settings are restored; new Play sessions within the same project retain it. The finite keyboard/mouse control
set excludes the reserved Escape/F5/F6/F10 Editor controls.

All five common pointer buttons can bind to the existing five button actions: left, right,
middle, back and forward. Movement remains keyboard-only. Defaults are unchanged, so the three
additional buttons remain unbound until explicitly selected. The same acquisition, focus, user
switch, Pause/Stop and profile-replacement reset rules apply to every held button. Pointer look,
gamepad and simultaneous independent device owners remain separate work.

`ForwardPlayInput` sends the copied profile through `PlayGameplayModule` before processing every
native input batch, including deferred rendering. `PlayInputState` translates platform-independent
Window keys/buttons to profile controls and publishes the same selected-user `NexoraInputSnapshot`
wire shape. Successful replacement clears held keys/buttons and output, then discards one acquired
batch while focused; an unchanged profile preserves held input, and invalid profiles preserve the
last-good mapping/output. Sequence remains monotonic and reserved remains zero. Module callbacks
observe one copied frame, including release events without a GUI frame. The UI never writes a
profile file; device-specific persistence, gamepad, pointer look and multiple users remain open.

## Persisted project input bindings

Apply and save sends an owning UUID/root/profile request to the application. Before IO it rechecks
the active project and stopped Play, then delegates writer/recovery/profile/path validation and
atomic publication to `ProjectWorkspace::SavePlayInputBindings`. Failure leaves the last-good file
and applied session profile intact and displays an error. Session Apply writes no file.

Both startup and project activation load `.nexora/play-input.ini` through the bounded workspace
reader, including read-only projects. Missing legacy settings use defaults; corrupt/unsafe settings
are preserved with an error until explicit replacement. Startup recovery defers this load until
Recover/Discard succeeds. The copied scoped setter prevents the first UI frame from discarding a
restored profile, and native/deferred input forwarding uses it through the existing copied ABI.
There is no automatic shutdown save or gameplay-library load caused by reading bindings. Linux
Xvfb checks actual remapped B movement, rejected old D input and read-only process reopen with
unchanged settings/scene bytes. Expanded devices/users and physical-host acceptance remain open.

### Authored scalar PBR Scene materials

The application publishes a generation-scoped material catalog alongside its mesh catalog after
activation and successful Content changes. Inspector single/multiple-object assignment persists
versioned UUID references through one atomic document Undo/Redo and save/reopen, preserving legacy
shader IDs. Mixed references/presence show Mixed; missing Mesh Renderers or unsupported references
reject the entire batch.
Unassigned clears supported references for the complete selection in one Undo/Redo, including
missing-source/catalog cases, while protecting unknown versions and retaining unrelated opaque data.
Saved/reopened references remain absent; mesh/shader values and source material files stay unchanged.
Valid `.nmaterial` scalar assets produce actual per-batch Presentation PBR slots in Scene View. A palette
reserves neutral fallback slot zero and at most 63 unique authored UUIDs; unresolved/over-budget
references remain stored. Authored slots use white instance tint so selection does not recolor the
material; ground, proxies and gizmos retain their existing tint. With no resolved materials the
existing Lambert preview stays active. Geometry tangents come from Renderer, including OBJ assets
with no usable UVs, while the submitted world matrix retains shear/mirroring.

Every draw owns its temporary palette and converted geometry until `DrawScene` returns; native
Presentation fences retain submitted storage. CPU geometry conversion remains per frame; optional
immutable native mesh residency reuses exact geometry independently of frame layout (see
[ADR-0008](../../Roadmap/en/ADR-0008-Editor-Mesh-Geometry-Residency.md)). This is scalar opaque Scene material assignment, not
texture/shader-graph editing, shipping asset cooking or full physical/multi-DPI
Scene View acceptance. See [ADR-0005](../../Roadmap/en/ADR-0005-Editor-Scalar-PBR-Materials.md).

### Frozen scalar PBR Game materials

Before cloning Play, the application freezes validated scalar material values and the current
SceneDocument's mesh-entity UUID assignments into an owning palette. Catalog generation mismatch
rejects Start before clone creation. The palette reserves neutral slot zero and at most 63 distinct
resolved UUIDs; missing/budget-rejected assignments retain their source bytes and report fallback
shading for visible admitted meshes. Unsupported opaque versions remain unresolved. New entities
created by gameplay receive neutral shading; runtime material-reference authoring is separate work.

Game frame preparation reads the post-tick isolated World, copies the selected camera's world
position, generates Renderer tangents (including degenerate UV fallback), and owns its native PBR
palette, geometry and exact affine instances until DrawScene returns. A tangent conversion failure
preserves the geometry through the legacy Lambert path. Frames with no resolved authored material
also retain Lambert shading. Reimport, deletion and authoring reassignment cannot change the frozen
Play palette; Stop/apply-and-stop releases it and the next Start captures current values. Prepared
frames remain valid after Stop or project/catalog destruction. No material assignment or value is
applied back to the Editor World.

Portable tests exercise actual reimport/deletion/reassignment, dedup/budget/missing fallbacks,
legacy full-width shader IDs, camera/tangent values and Pause/Step/Stop isolation. Linux Xvfb tests
acknowledge red native Vulkan Game pixels, preserve them through a paused source edit, then reopen
read-only and acknowledge fresh green pixels with unchanged authored bytes. This does not accept
texture/shader graphs, dynamic material references or physical-host output. Bounded concurrent
Scene/Game views are described below. See [Linux evidence](../../Tools/Build/evidence/EditorEDM3-GameMaterials-Linux-2026-10-09.md).

The Profiler memory controls save/load a separate `.nexora/process-memory.json` trace through the
current ProjectWorkspace on the application thread. Both requests are consumed independently of
wall-time controls and results enter the bounded Console. Export includes the process-wide history
across project switches; the stored UUID denotes only the export destination. Import publishes a
validated owning static snapshot and preserves live capture on failure/success. The UI plots elapsed
time with pause gaps and unavailable reads, and reports memory-history evictions separately. No
capture is written automatically; wall-time CSV/JSON files and GPU availability remain unchanged.

The graphical Editor opts into native GPU timing when creating its RenderSurface. After successful
BeginFrame/Acquire and before UI Capture/Clear, it maps copied completed Presentation timing into
`ProfileSession::ObserveGpuFrame` with the stable surface domain and software-device flag. Recording
never borrows a native command buffer or substitutes CPU frame time. GPU samples include delayed
completion and are independent of CPU wall-time/RSS; no project file is written implicitly. Shutdown
reports bounded source/scope/unit, completed ID, retained/dropped counts and optional latest duration.
Software Vulkan evidence does not establish physical GPU timing accuracy or performance budgets.

The application consumes independent GPU export/import requests through the current workspace.
Export synchronously borrows the retained live samples with the current native source/software and
dropped count, writing `.nexora/gpu-timing.json`. Import transfers a validated owning static snapshot
to the GUI; source/scope/unit/project validation and failures preserve live capture and prior static
state. Results enter Profiler status and the bounded Console independently of wall/RSS requests.
The project UUID denotes the export destination, not GPU resource ownership. No file is written
automatically, and existing wall-time/RSS capture bytes remain compatible. Physical calibration,
per-pass analysis and broader external capture formats remain open.

Build > Export StaticView package captures the current managed Content scene, including unsaved
authoring changes, and cooks/verifies the data package on the application JobSystem. Save an untitled
scene first. Build > Cancel StaticView export requests cooperative cancellation. The Project panel
shows bounded copied status and, only after current-state-checked publication, the package path,
actual byte count/checksum and reproducible verification command. Cancellation, failure, content
changes, project switches and recovery/write-access loss preserve the previous package.

The application owns publication after widget/scene commands and rechecks the request's scene token,
current managed asset UUID, scene/access/Play state and the coordinator's complete freshness proof.
UI code retains no worker or filesystem borrow; changed document scope hides prior success.
Export never saves the scene or changes its Undo/Redo baseline. Cooking cannot be interrupted within
its synchronous producer/loader call; cancellation is checked between phases and 8 KiB IO chunks.
Shutdown cancels and drains the job while the JobSystem remains alive. See the
[owning coordinator contract](../../Engine/Editor/StaticProjectExport.md#graphical-staticview-export-coordinator).
This is verified StaticView data export; executable compilation, target/device/remote deployment,
signing and the complete Build workflow remain open.


## Scoped retained mesh memory file actions

The Profiler's Export mesh memory/Import mesh memory requests own project UUID/root copies.
The authoring owner consumes each once, verifies the current project again, then calls bounded core
`.nexora/mesh-memory.json` IO and publishes the owning static result against that same scope. There
are no widget-side filesystem/native reads or asynchronous native borrows. Failed IO/publication
reports an error while preserving prior static imports and live observations; explicit Clear mesh
import affects only that static channel. Writer/read-only/recovery/focus/modal/close/project changes
follow the GUI/core gates; no capture is written implicitly. Source scope excludes RSS, total VRAM,
textures/frame uploads and allocator overhead, and export destination is not allocation ownership.
The existing core schema1 and stable C/Zig/scene formats/dependencies are unchanged; additive host
C++ APIs require a matching rebuild. Physical device-memory calibration remains separate work.

## Optional copied Play pointer input

The V3 host appends `capture_pointer_input` after `get_twod_api`, advertised by
`NEXORA_GAMEPLAY_CAPABILITY_POINTER_INPUT`. Check the end of this optional field before reading it,
then ABI version, capability and callback; `NexoraCapturePointerInputV1` performs that negotiation.
The original V3 required prefix and `NexoraInputSnapshot` remain unchanged. Old 2D hosts continue
to negotiate using the end of their own callback. C++ host consumers rebuild.

Callers provide aligned `NexoraPointerInputSnapshotV1` storage with `struct_size` set. Success copies
the known 56-byte V1 prefix and leaves larger-buffer suffixes intact; errors preserve all output.
The embedding owns the context through Stop/Destroy, serializes calls on its game thread, and
revokes input before teardown. No native window, event buffer, ownership transfer or deferred work
crosses the boundary. An unbound owner rejects with Lifecycle; null/user>=4 rejects InvalidArgument;
a short output rejects Unsupported. Missing providers return Unsupported through the helper.

The selected user0–3 gets native client-pixel deltas (right/down positive), horizontal/vertical
Window wheel units (120 per notch), and raw held left/right/middle/back/forward bits0–4, independent
of profile action bits. Other users get neutral copies with the same legacy batch sequence. Reads
are non-consuming: apply displacement once per sequence across Fixed/Update, without multiplying
by tick duration. Consume in Update as well if using Fixed: some batches have no fixed tick.
An empty batch clears deltas/wheel while retaining held buttons. Owner focus or
admission mutations may neutralize the current snapshot without advancing its batch sequence.
Motion saturates at +/-65,536 pixels/axis and wheel at +/-122,880 units/axis.

Acquisition suppresses input and may seed the pointer anchor; otherwise the first sample anchors.
Blur/Escape wins the whole batch. Focus/user/profile replacement, Pause and Stop reset impulses and
anchor. Resize/DPI/display changes suppress that batch's motion/wheel and invalidate the anchor at
the domain event; only later pointer samples can establish a new anchor. Normal key/button release
processing continues. Resume requires reacquisition. This adds bounded native motion, not cursor
locking, raw/unbounded relative input, gamepads or complete pointer-look acceptance. See
[ADR-0010](../../Roadmap/en/ADR-0010-Play-Pointer-Input.md).

## Scoped native Play lifecycle gate

### Host allocation ownership observations

The Game panel's **Host memory** button opens a read-only window with live/peak payload bytes and
allocation counts, rejected calls, and the latest completed teardown's recovered payload. Playing
and Paused retain live values; Stopped/failed Create/Start show no loaded module and the completed
teardown separately. Unavailable values are explicit. Counts use lossless decimal uint64 text;
the view does not present these bytes as process RSS, allocator overhead or GPU usage.

The application publishes owning `PlayHostMemoryObservation` copies before drawing, through
`PlayHostMemoryPresenter`. Bind the new project scope before accepting Play commands. UUID,
canonical root, Content generation and Play generation are rechecked by the UI; no World/module
borrow survives publication. Project/root/generation replacement or detachment closes the window
and excludes previous teardown data, including a copied project with the same UUID. A same-root,
same-UUID read-only workspace may inspect the same scope. A monotonically saturating teardown
sequence distinguishes completed attempts; the presenter hides teardown at sequence exhaustion
rather than wrapping or relabelling old content. Widget rendering does no allocator/OS/IO queries.
Current-frame gameplay changes become visible through the next owner publication.

The window is session-only and omitted from saved layout state; opening it releases Game capture
without issuing a Play command or changing authoring/history. No file action or telemetry transport
is added. Test builds support opt-in `NEXORA_PLAY_MEMORY_UI_TEST_EVIDENCE` for bounded geometry/text
observations of the actual drawn controls, following the existing native UI acceptance mechanism.
`editor.play_host_memory_ui` exercises real 1x/2x controls and scoped provider lifecycles;
`editor.linux_play_host_memory` checks native pixels, exact presented values, Pause/Stop/restart,
balanced fallback recovery and unchanged source bytes through an independent C-ABI fixture.

`PlayGameplayModule::Allocations()` returns an owning, owner-thread snapshot of the current
session's live allocation count/payload bytes, observed peaks, and saturating rejected allocation
and deallocation counts. The existing per-request 16 MiB, session 64 MiB and alignment limits stay
in force. Only a successful tracked allocation or an exact pointer/owner/size/alignment release
changes live totals; a null, unknown or mismatched release is rejected. These bytes exclude map
nodes, allocator overhead, native allocations, RSS and GPU memory. No C/Zig wire fields change.

Stop/Destroy retain access to the World and allocator. After those callbacks finish, Unload
reclaims any remaining tracked payload, revokes the World and allocator services, and retains
`LastTeardownAllocations()` with zero live totals plus the exact reclaimed count/bytes and session
peaks/rejections. Failed Create/Start loads receive the same fallback cleanup. New bound sessions
start with zero current counters; the last teardown snapshot remains until another bound session
ends. Repeated Unload preserves that report and produces no duplicate warning. Calls retained
after revocation cannot allocate and do not change the completed session's report; callback
contexts must never outlive their owning host. Serialize observation and callbacks with teardown.

If fallback reclamation was needed, one bounded Warning enters the existing Gameplay/Core Console
producer after cleanup. A throwing log sink cannot interrupt reclamation. This distinguishes
module-paired releases from host recovery; it does not prove that a module freed non-host resources
or that the process is leak-free. The observer does not authorize callback or log-sink reentry.
`editor.play_host_allocations` covers 32 paired/retained/failed-load sessions, exact ownership and
quota failures, copied snapshots, Console severity, repeated cleanup and a failing sink.

### Native process residency

`editor.linux_native_play_soak` runs 32 Play sessions in one Linux Editor process with disjoint
Scene/Game canvases, a real C-ABI library and a two-entity/one-triangle project. Each module touches
4 MiB of host-owned allocation. Fresh clone poses, capture/motion pixels, Escape release, Pause,
exactly one neutral manual Fixed step, Resume, held-button Stop and ordered neutral Stop/Destroy
callbacks are observed; native authoring Scene pixels and source scene/mesh bytes remain unchanged. Startup waits for the actual
native Scene viewport before issuing controls. No authoring/apply-back operation is requested.

The test samples `/proc/<pid>/status` VmRSS after each Stop and 100 ms of event processing. Four warmup
cycles precede the retained envelope: its baseline is the larger of cycles 3/4, and the peak of
cycles 5–32 must remain within 32 MiB above that baseline. All 32 raw samples are reported. This is
current-process resident-set growth for this small dataset, including shared/native mappings; it
is not live allocation ownership, VRAM, a proof of all leak freedom or a production-scale budget.
The 240-second whole-case deadline is a CI liveness guard, while individual observed transitions
have bounded deadlines. Fixture output is bounded to 4 MiB per stream and Vulkan validation errors
fail acceptance. Project/document/plugin/device-loss, large content, long-duration and physical
host soak remain separate gates.


### 2D Game View and optional Play gameplay services

For a bound 2D owner, the same V3 host now advertises services for rebindable action/pointer input,
immediate UI/font registration, native WAV audio controls and project-UUID-scoped permanent slots.
Frozen Atlas/Clip assets remain read-only through the 2D bridge; a mutable content registration
service is not advertised. Game modules may register their own embedded font/WAV buffers; Zig `ui.registerBakedFont`
decodes the Windows baker format using caller-owned scratch without a game-side C++ parser. No
production font or artwork is invented. Storage uses the per-user directory helper, not the project
source; save schema/commit/error handling belongs to gameplay. Static-loader CPU tests never open
an audio device. Real library loads try the native output, report availability through the audio
API and retry output after failure at one-second intervals. Stop/Destroy precede service release.

Only event batches admitted by the existing Play capture handshake reach the action/UI service.
Blur/Escape, paused capture, user switch and discarded acquisition batches revoke held state;
acquisition clicks do not activate UI. Native gamepads poll only during accepted capture. Pointer
snapshots retain native client units; UI hit testing subtracts the Game viewport origin. Navigation
actions 253..256 supply next/previous/activate/cancel defaults and can be rebound. Escape retains
its Editor release-capture behavior. Game UI uses Game content pixels and supplies its own layout;
it does not receive arbitrary Editor modal input.

The explicit 3D/2D selection described above controls Game rendering. The 2D canvas resolves
frozen Atlas owners and extracts the actual Play World poses plus pooled sprites in one sorted
frame. Gameplay UI follows those sprites, clipped/offset to the same physical Game rectangle.
The HUD submission contains no scene sprites, so translucent sprites draw only once. Switching
back to 3D suppresses this HUD. Paused redraws
retain completed UI without advancing module/animation/audio clocks. HUD quads join the same
Game draw list, preserving Editor popup/window ordering, with a combined 4096-quad admission
budget and shared GUI native-image limits. This Editor path makes no GPU
Sprite-instancing performance claim. The standalone Player retains native Sprite instancing.

Surface-local immutable texture IDs use fresh GUI reservations for every Play owner; module UI
IDs are remapped away from Editor/frozen Sprite IDs. Frame snapshots retain Atlas/font owners
through submission. The texture owner releases Sprite and HUD registrations together between
acquired frames before the next BeginFrame. Canvas admission failures show an unavailable status;
native surface failures stop the loop and final drain releases remaining GPU resources. All preparation/callbacks are serialized
on the Editor owner thread and C++ consumers rebuild after the optional Render overload change.

`editor.play_twod_services` covers extraction, bounded clips, retained owners, fresh identities,
retirement, action acquisition/revocation and unchanged authoring history across 32 sessions.
`editor.play_twod_native` uses the same host/ImGui composition on Linux Vulkan, checks actual red
Sprite/UI pixels and viewport clipping, and retires textures across those sessions. This is native
host integration evidence; Windows physical Game View/DPI/audio/controller acceptance remains
in the [target-host list](../../Tools/TwoD/WindowsAcceptance.md).


## Native project and asset workflow acceptance

`editor.linux_project_asset_workflow` drives the real project selector and Content widgets
with X11 pointer/key events: create, asynchronous initial OBJ import, filtering and selection,
authored mesh creation/native Scene submission, folder drag/drop, external-source reimport,
invalid-source rejection and retry, Content Undo, Save, process restart and read-only inspection.
It checks UUID/last-good geometry/artifact retention and exact project-file hashes after the
read-only launch. Conditional registration requires Xvfb/xdotool; hosted display CI requires
that registration explicitly. This software-GPU workflow does not replace physical display,
Windows DPI/IME, or GPU-quality acceptance.

The opt-in `NEXORA_PROJECT_ASSET_WORKFLOW_EVIDENCE` observer exists only in test-enabled
graphical builds. On the authoring thread after EndFrame it formats an owning observation of
existing workspace/model state and widget rectangles. It retains no model/native pointers,
executes no action or IO, and has no gameplay/plugin callback. Private UI rectangle inspection
ends with the host lifetime. Native tests wait for stable rendered coordinates and keep normal
input routing, access checks, import worker publication and filesystem transactions intact.

The opt-in `NEXORA_PREFAB_WORKFLOW_EVIDENCE` observer is also restricted to test-enabled
graphical builds. It copies actual drawn prefab control centers/admission, processed GUI
focus/key/pointer state, history attempt/success counters, and the first root's committed name
(hex UTF-8) and X position. It runs after EndFrame on the owner thread, retains no pointers,
and cannot perform an authoring action. Only changed observations are printed.
`editor.linux_prefab_isolation` acknowledges prefab control pointer/key-chord phases and resolves actual history
control positions before a single Undo/Redo gesture; both expected edits must be committed
before the full-property revert. The original exact asset/identity/opaque-byte and read-only
checks and 180-second whole-test deadline remain required. Both output pipes are drained during
input/result waits. Failures retain the temporary project, per-launch output, and timestamped
input records at the path printed by CTest; successful runs clean their owned temporary tree.
The older aggregate Undo failure did not retain its assets, so this instrumentation must not
be presented as proof of that historical failure's root cause.

The opt-in `NEXORA_SCENE_MERGE_UI_TEST_GEOMETRY` stream also copies processed scene shortcut
keys, pointer/button state, text-input state, the actual comparison/history admission predicates
and scene Undo attempt/success counts. Input observations and control geometry are published
after EndFrame; frame numbers are appended outside change deduplication. The observer grants
no input, authoring or filesystem authority. Counters describe real history calls during this
host lifetime, including the existing Scene toolbar Undo.
Native comparison/publication and center/mesh fixtures acknowledge one physical key phase at a
time using `NativeSceneInput.py`, and retain input/output records on failure. Comparison waits
start before the gesture so an acknowledged phase cannot consume and hide an already-finished
result. Center tests retain their original pointer waits and 80 ms key-phase minimum; no blanket
wait is added to pointer operations. The original 45/90/120-second whole-case and five-second
saved-result deadlines remain required, as do exact source, provenance/archive, read-only,
corrupt-source, geometry and pixel assertions. This narrows input uncertainty; it does not
retroactively identify the cause of historical failures lacking processed input records.

`NEXORA_SOURCE_MARKER_TEST_GEOMETRY` is another opt-in test observer: it copies the first
actually drawn source marker's identity/physical center and document selection, after EndFrame,
without selecting or activating anything. The signed-package native fixture checks the original
10x10 pixel/color predicate at this actual center, clicks once and observes that same source
selected before opening Extensions. It retains both launches' output/input/project on failure.
The existing Ctrl+Alt+E gesture still uses 100 ms native transitions; E has no new GUI-phase
acknowledgement. `ExtensionControls` accepts an optional shared pump so a binary stream has one
reader; existing clients keep their original pump. Package signatures, permissions, lifecycle,
read-only and source-integrity checks remain mandatory.

The Project Asset workflow observer copies the actual scene row, visible/enabled Content
Rename/Delete menu controls and rename field/Apply/Cancel controls, including the current
rename text as UTF-8 hex. Hidden popup sizing frames publish no actionable menu/modal points.
The native scene-file fixture preserves its F2+Enter and context-menu+Apply paths, verifies the
actual typed text and dirty node-count precondition, and retains failed fixtures and output.
The same scene input stream copies the active native tool, actual scale-axis hit and preview
state. Native preview coverage waits for the original physical press/move/release to reach
those states before checking the unchanged scale, saved bytes, Undo and pixel assertions.

Native button observations cover left/right/middle; wheel gestures retain their original
native event path. `click()` and combined move/click gestures share one eight-second input deadline.
A retained final input observation may acknowledge a normal exit; a successful Save and Exit
Enter gesture is instead checked by its original saved-file and zero-exit assertions because
the application exits on Enter-down. No GUI Enter-up acknowledgement is claimed after exit.

The same opt-in scene stream has separate F5/F6/F10 processed bits, the exact original Play
shortcut admission predicate, a keyboard-request sequence/last command and the actual owner
Runtime Play state. It preserves the nine scene-key bits and scene Undo contract. A request
only proves that the UI issued that command: snapshot, Start and module loading remain owner
operations, and native Game viewport/pixel/runtime/source assertions still verify their results.
The native Game fixture acknowledges only those reserved Play keys; movement/input-routing
keys retain their original runtime path because game capture intentionally bypasses ImGui.
Each start gesture and its Game viewport result share the original twelve-second deadline.
Both binary streams have one reader across every reopen, failures retain project/input/output,
and short four-frame checks validate complete output with their original shutdown bounds.

The 32-cycle native Play soak now establishes its authoring Scene baseline before any Start,
using one readback for its existing blue predicate and exact baseline, stable with the same
viewport for 0.3 seconds within the original three-second readiness bound. Focus, neutral
pointer and released buttons are observed by the existing opt-in stream and sole pump. The
original center-region/stride-3 equality remains exact, including an additional check after
Start before motion. First Game readiness also uses one readback. Failed runs retain RGB
reference/actual samples, viewport/difference bounds, source bytes and both streams/input;
32 cycles, RSS/lifecycle/input assertions and output/whole-case budgets remain required.

The same opt-in scene stream copies a separate O key, the exact Open shortcut gate and request
count, the actual scene-file dialog, rendered Path control/focus and current UTF-8 path bytes as
hex. Native Open uses one phased Ctrl+O gesture; Open/Save As waits for the actual modal, field
focus and exact typed destination before a single Return. These phases share ten seconds;
file adoption, source identity, history and read-only checks remain independent result oracles.

Game capture observations copy the actual canvas hover/click, original admission gate, focused
state and admitted-click count. The soak sends one down/up and a separately observed seed before
its measured motion. Acquisition, seed, motion delivery and the original applied/pixel result
share three seconds; the original 32-cycle lifecycle/RSS/source assertions remain required.
These UI copies do not prove provider delivery. Pointer impulses remain per native batch; modules
must consume them in Update as well as Fixed when some batches have no fixed tick.

The Build console fixture uses actual rendered control positions and copied executable/cwd/first
argument text before its single Run request, retaining separate argv and exact operation/exit
checks. Configuration bytes are emitted only in the explicit test-build geometry stream; normal
console behavior and diagnostics remain unchanged. Source inspection likewise selects the actual
Scene marker and clicks the current Inspect source control once, retaining exact retained/current
revision, archive availability and read-only/source-byte assertions. Both fixtures keep one binary
collector and failed inputs/output/projects.

The pointer-button fixture continuously drains output during its original held/release intervals.
An opt-in observer copies the actual single-instance native draw pose after successful DrawScene
and EndFrame queue admission. Fresh gameplay-neutral input and consecutive equal admitted poses
precede the exact released-pixel baseline. Queue admission does not establish physical scanout;
the original two 150 ms intervals and pixel equality remain required, with failed RGB retained.

Scene marker test positions exclude inactive, hidden/sizing, skipped and unsubmitted windows;
the getter leaves marker rendering and selection routing unchanged. Native source selection and
inspection recheck the current named position after pointer processing, with at most three
position-only moves before a single click. Source rebase uses the same bounded positioning and
actual Review/conflict/Apply/consent controls, plus separate Escape/Y phases preserving its
original cancel and Ctrl+Y paths. Exact event identities, consent, source files, read-only and
single history operations remain the result checks. All original result/case limits remain.
