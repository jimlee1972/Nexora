# Nexora Graphical Editor Roadmap

> Version: v1.3 | Status: AI-executable delivery plan | Updated: 2026-10-02

> **Progress: 0%** (none of ED-M0 through ED-M7 has passed graphical Editor acceptance;
> completed Runtime/Editor SDK prerequisites are not rounded up into an Editor milestone.)

**Completed prerequisites:** ✅ reflection metadata; ✅ command/undo data model;
✅ prefab override/rebase; ✅ isolated PIE session; ✅ dynamic plugin ABI gate; ✅ standalone
process and portable workspace/document core. **Open:** remaining graphical views, authoring
workflows, target-host acceptance, and production hardening.

Platform-only acceptance is deferred when a required host is unavailable. Continue independent
implementation and automated validation; preserve unverified evidence rows and do not mark the
corresponding milestone accepted until its host gate passes.

### Repository completion audit (2026-10-02)

The audit distinguishes a checked implementation foundation from an accepted graphical milestone.
Source and contract tests confirm the checked rows; no ED milestone currently satisfies its complete
automated **and** target-host gate, so overall graphical acceptance remains **0/8 (0%)**.

| Scope | Repository evidence | Accepted |
| --- | --- | :---: |
| ED-M0 shell foundations | Standalone process, optional ImGui host, stable panels, initial docking, input/DPI/IME forwarding, live Hierarchy, recovery modal, retained native GPU rendering, project layout persistence, and recovery failure contracts exist. Linux virtual-display recovery now verifies SIGKILL with a durable seeded journal, unchanged committed workspace, writer-lease reacquisition, and keyboard-only Recover/Discard; physical-display Linux and Windows DPI/IME host evidence remain open; a bounded Windows/DX12 developer-host shell smoke is recorded. | [ ] |
| ED-M1 project/assets | Portable create/open, schema upgrade, single-writer/read-only access, recent-project state, deterministic indexing/search, persistent sidecar UUIDs, virtualized Content Browser state, breadcrumb/selection, transactional mutations, typed generation-safe drag payloads, dependency/cycle inspection, transactional reimport, watcher debounce, and dirty-conflict decisions exist. The native shell exposes project status, provides a graphical create/open/recent selector, binds the real index to a graphical Content panel with recoverable project-local mutations, runs cancellable background import/reimport with bounded progress and structured diagnostics, shows dependency cycles, and presents blocking reload/keep/compare conflict UX; physical-display/Windows workflow acceptance remains open. | [ ] |
| ED-M2 scene authoring | Portable hierarchy/selection, reparent, sibling reorder (undoable Hierarchy drag model), multi-selection, clipboard, transform transaction, undo, and atomic save/reload exist, plus UI-neutral pick-ray, AABB picking, axis-drag, snapping, and viewport-resize-hysteresis math, and Unity-style translate/rotate/scale gizmo math with Global/Local axes, Pivot/Center, parents, negative-scale rules, and multi-selection roots. The graphical Hierarchy now presents a parent-aware expandable tree, filtering, generation-keyed expansion/selection, clipped visible rows, undoable rename, sibling ordering, and cycle-safe reparenting while rejecting stale entity/document generations. A docked Inspector exposes generation-safe position, Euler degrees (quaternion storage), and scale editing for single and mixed-value multi-selection, with atomic Runtime validation and one-step undo. Full authored-mesh Scene View, the complete reflected Inspector, material shader workflows, camera authoring, and missing-plugin restoration remain open. Bounded read-only opaque component inspection and persistence are implemented. The native proxy preview already has Move, Rotate, and Scale handles. | [ ] |
| ED-M3 PIE/debugging | Portable `PlaySession`, structured bounded Console records, owning inspection snapshots, debugger adapter/pause reasons, failure recovery, and deterministic transform conflict rejection exist. The graphical Console shows bounded records and Editor diagnostics; a docked Game panel controls an isolated clone and copied inspection snapshot. Bounded native camera/OBJ Game View is implemented; complete materials/multiple canvases, complete gameplay services/expanded input, complete log routing, and native debugger integration remain open. | [ ] |
| ED-M4 prefab/scenes | Portable override diff/revert/apply, variants, and nested rebase exist. Graphical prefab/multi-scene, migration/recovery, conflict, and source-control workflows remain open. | [ ] |
| ED-M5 specialized tools | Stable capability IDs and honest implemented/read-only/unavailable states exist. No production graphical reference tool has passed edit-preview-save acceptance. | [ ] |
| ED-M6 build/profile/extensions | Portable build manifests/checksums and bounded monotonic profile capture exist. A docked Profiler plots live Editor frame processing time with pause/clear and dropped counts. Build/deploy/log, GPU/memory profiling, export, and plugin-manager workflows remain open. | [ ] |
| ED-M7 hardening | Portable virtual hierarchy, trust/signature policy, and telemetry opt-in tests exist. Graphical scale/soak, migration/corruption, keyboard, and screen-reader audits remain open. | [ ] |

The focused [Dear ImGui plan](Editor_ImGui_Integration_Plan.md) contains the granular checked ED-M0
foundations. Check a row above only when its milestone Definition of Done in §14 is evidenced; do not
derive milestone progress from the number of implemented prerequisites.

## 1. Product vision

Build a standalone `NexoraEditor` with a Unity/Unreal-like workflow—not a claim of feature parity. Its first production path covers Project Browser, Hierarchy, Scene View, Game View, Inspector, Content Browser, Console, Profiler, gizmos, undo/redo, Play-in-Editor (PIE), and import/cook/build. The Editor consumes Runtime APIs and is never required by a shipped game.

## 2. Architecture boundaries

```text
NexoraEditor (tool process)
  UI shell/docking/commands/workspace persistence
  editor documents, selection, and transactions
  Scene/Game surfaces and gizmos
  asset and build/cook/package frontends
          |
          v public Runtime/Renderer/Asset/Editor SDK APIs
  Editor World -- snapshot/clone --> Play World
```

Editor metadata stays out of Shipping components. Selection stores stable IDs, not relocatable pointers. Every mutation is a transaction, including property edits, gizmo drags, reparenting, and multi-edit. PIE clones an isolated Play World and discards changes unless explicitly applied. Engine Core cannot depend on the UI framework; [ADR-0001](ADR-0001-Editor-UI-Framework.md) records that evaluation (docking, IME, accessibility, multi-viewport support, and maintenance) and its outcome. Extensions register panels, commands, importers, and inspectors through the versioned Editor SDK only.

## 3. Milestones

Milestones are delivered in strict order: **ED-M0 → ED-M1 → ED-M2**. The Editor must not begin
production widget implementation before ED-M0 settles its product and UX contracts. In particular,
the shell depends on the public window/swapchain path and must not invent a temporary private
presentation path merely to display UI.

### ED-M0 — Product shell and UX contract

Define supported operating systems, project/workspace formats, stable panel IDs, command and
shortcut routing, docking, theme, DPI, IME, accessibility, and crash recovery. Select the UI
framework through an ADR and focused prototypes; wireframes or an isolated widget demo do not
satisfy this milestone.

- ✅ The standalone `NexoraEditor` process, versioned project/workspace format, stable panel IDs,
  command namespace, atomic workspace replacement, and recovery journal are implemented.
- ✅ UI-framework ADR: [ADR-0001](ADR-0001-Editor-UI-Framework.md) selects Dear ImGui
  (docking/multi-viewport), rendered through `Nexora::RHI` rather than a competing windowing
  stack, and names the accessibility gap ED-M7 still has to scope. The ADR settles the framework
  choice only -- it is not itself graphical docking, theme, DPI, IME, accessibility, or crash UX.
- ✅ The feature-gated Dear ImGui host implements the portable docking, theme/DPI, input/IME,
  live-panel, recovery UX, and accessibility-direction contracts described by
  [Editor_ImGui_Integration_Plan.md](Editor_ImGui_Integration_Plan.md).
- ✅ On Vulkan hosts, the graphical process composites ImGui draw data into the acquired public
  `RenderSurface` swapchain backbuffer; Linux and Windows window events normalize the complete
  Editor key/modifier set.
- Open acceptance: real-display Linux visual/input/recovery evidence and Windows DPI/IME evidence.
  A bounded Windows/DX12 developer-host shell smoke is recorded, but ED-M0 remains open until the
  complete target-host gates pass.

- ✅ Native client-pixel pointer events now convert once through the current frame DPI before UI
  hit testing. Cached positions reproject across scale changes; focus loss clears the cache and DPI
  changes cancel interrupted Scene gestures. Tests cover 100/125/150/175/200%, fractional/negative
  coordinates, event ordering, stationary pointers, invalid scale fallback, and 200% Apply dialog clicks.
  Deferred/zero-extent frames also forward gameplay key releases and focus loss without ticking
  Play or rendering a GUI frame. Target-host physical-display DPI evidence remains open.

### ED-M1 — Project and asset workspace

Create, open, and upgrade projects. Deliver a Content Browser with search/filter, folder/UUID,
drag/drop, import status, dependency inspection, and reimport. Background import must expose
cancellation, progress, and actionable errors, and must produce deterministic artifacts.

- ✅ Typed OBJ reimport now stages immutable geometry and hashes, then atomically publishes through
  the live content model after project/asset/source/settings/dependency and 128 MiB mesh-budget checks.
  Tests verify synchronous/background updates, stable resource identity, owning older snapshots,
  failure/cancellation/staleness/oversize preservation, newer geometry across rename Undo and
  delete/Undo, and budget rollback. A monotonic content revision refreshes the native mesh catalog
  before drawing; rendering/picking perform no source IO.

- ✅ Background workspace imports now retain bounded immutable CPU geometry for triangulated OBJ
  assets, including UVs, explicit/generated normals, indices, and local bounds. Portable tests cover
  malformed/overflowing input, cancellation, vertex limits, UUID/hash stability after move/reopen,
  owning snapshots, and structured worker diagnostics. Persistent per-asset GPU caching and full Scene View acceptance remain open.

- ✅ Project create/open, deterministic content-tree indexing, UUID/path search and filtering,
  cancellation, progress, inspectable errors, and deterministic artifact hashes are implemented.
- ✅ Portable virtualized Content Browser/breadcrumb/selection models, transactional rename/move/
  delete, typed generation-safe drag validation, dependency/cycle inspection, transactional
  reimport, watcher debounce, and explicit dirty-conflict decisions are implemented and tested.
- ✅ The graphical shell now binds the real deterministic index to a docked Content Browser with
  breadcrumbs/folders, search/type filters, virtualized UUID-keyed rows, selection and thumbnail
  states. Generation-tagged drag/drop, dependency inspection, background reimport, and recoverable
  filesystem-backed rename/move/delete/undo route through an authoring-thread
  `ProjectContentSession`; the UI never writes files directly.
- ✅ Versioned sibling `<asset>.meta` records persist UUID and importer type. Read-only indexing
  rejects missing or corrupt identity state; writable indexing creates missing records atomically.
  Rename, move, delete, and undo transact the source and sidecar together, and derived artifacts are
  keyed by UUID plus source bytes, preserving identity across move and process reopen.
- ✅ Project descriptors now have stable UUIDs and atomically upgrade from schema 1 to schema 2
  under an OS-held single-writer lease. Explicit read-only opens cannot upgrade or mutate
  project-owned state, recent projects use a bounded versioned user-level store, and the docked
  Project panel exposes canonical root, schema/upgrade, access, and recent-project status. Core,
  graphical-contract, and Linux real-process tests cover writer rejection and read-only coexistence.
- ✅ The graphical Project Browser emits one-shot create/open requests without owning project state.
  The application transactionally activates the candidate workspace/index/content session and
  keeps errors in the selector. Linux Xvfb acceptance drives keyboard-only create and read-only
  reopen from a launch without `--project`, then verifies the descriptor and active access mode.
- ✅ An Editor-owned `AssetImportQueue` now runs project indexing and reimport on cancellable Core
  jobs. Workers produce generation-tagged staging results plus bounded progress and structured
  diagnostics; the authoring thread alone activates a candidate index or atomically publishes a
  reimport after revision/dependency revalidation. Queued cancellation, stale completion, and
  shutdown preserve the previous index/artifact, and both selector and Content Browser expose
  progress/cancel/failure states.
- ✅ The graphical Content panel now shows dependency cycles and serializes dirty external changes
  through one blocking dialog. Compare exposes both retained hashes without resolving the conflict;
  Reload or Keep records the terminal authoring-thread decision, and no UI path overwrites files.
- Open: physical-display/Windows fresh-project workflow acceptance.

### ED-M2 — Scene authoring core

- ✅ Content now creates/selects one resolved mesh root at the Scene center through Add mesh to
  Scene, with one initialized-entity Undo and stable-ID/name/pose/component Redo. Project-generation
  catalog checks and writable workspace/content/modal gates reject stale, missing, non-mesh or
  multiple selections. Native placement shares the preview's center clamp and target height.
  Real UI clicks, Game geometry preparation and Save/Reload verify the path; source IO stays outside
  the action. Typed mesh placement drag/drop and complete materials remain open.

- ✅ The single-camera Inspector now aligns world position/rotation to the stored Scene 3D view
  with one Undo, retaining lens, local scale and parent. Root-to-parent TRS inversion handles
  sheared/mirrored ancestry; invalid/stale/non-Camera targets reject before mutation. Equivalent
  poses preserve Redo. Real UI clicks, Runtime camera matrices and save/reload verify root/parented
  cameras plus read-only/unavailable view gates. Extreme finite centers use the native preview's
  +/-100,000 clamp before alignment. Full camera-authoring acceptance remains open.

- ✅ Scene/Hierarchy authoring now requires a writable attached workspace and no recovery, Play review
  or close modal. Controls, shortcuts and queued create/rename/reparent/reorder edits share the gate;
  read-only selection, Copy and camera navigation remain usable. Real keyboard/pointer tests cover
  preserved Undo/Redo, resumed Paste/Duplicate, readonly picking and canceled overview/native drags
  across access changes. Full graphical scene authoring acceptance remains open.
  Escape now explicitly cancels close confirmation before subsequent Save; Xvfb verifies that path.

Deliver Hierarchy, Scene View, Inspector, camera controls, selection/picking, translate/rotate/scale
gizmos, parenting/reordering, multi-selection, clipboard, undo/redo, and save/reload. Reflection
creates property widgets; unknown components retain raw data instead of being silently discarded.

- ✅ Camera and Light Inspector fields now support multi-selection with mixed presence/value states.
  Enabling a mixed component adds it to missing entities while preserving existing values; Enter applies
  only the edited field as one generation-checked atomic Undo/Redo transaction. Invalid/stale/duplicate
  batches and read-only/recovery writes reject the whole edit. Real keyboard tests cover Camera FOV and
  Light intensity, unchanged fields, repeated Undo/Redo and save/reopen. Full reflected editing remains open.

- ✅ Camera/Light controls now disable and discard drafts/pending requests during read-only, recovery,
  Play review and close confirmation. Real keyboard tests cover Escape, focus loss, deselection/reload,
  Play inspection and Inspector collapse, preventing stale input from reviving when access returns.
  Pending batches recheck current selection and cancel prospective Scene gestures; locale-independent
  numeric formatting preserves full precision. Full reflected editing remains open.

- ✅ Missing-plugin components now have a bounded read-only Inspector showing owning names, full-width
  entity/type IDs, byte counts, and at most 64 preview bytes. Scene format 3 retains opaque data through
  save/reload, clone-by-clipboard, deletion, and independent metadata Undo/Redo; opaque-free scenes retain
  format 2 and readers accept formats 1/2. Generation checks and 1 MiB/component, 16 MiB total payload,
  64/entity, and 4096-record limits reject stale/oversized imports. Corrupt, duplicate, orphan and missing
  entity records reject reload before live mutation. Plugin restoration/execution and full reflected
  editing remain open.

- ✅ Native pick/release authoring commands commit before Save, Save-and-exit, and GPU submission.
  Real Xvfb XYZ drags save immediately after release and verify the completed pose; saves while
  a gesture is held wait for commit or cancellation before serializing.

- ✅ Native Scene preview now draws resolved OBJ vertices/indices through bounded Presentation
  batches, packing shared resources once with absolute 16-bit indices and world TRS instances.
  Transformed bounds and two-sided triangles replace proxy picking for resolved meshes. Portable
  tests cover range rollback, geometry/coordinate budgets, mirrored/rotated picking and silhouette
  misses; Xvfb distinct triangle/quad assets verify picking, Center scale/rotation, preview/release
  pixels, and one-step Undo. Missing/deleted/oversized assets retain references and warn while using
  proxies. Persistent per-asset GPU caching, material shaders, exact shear and
  full Scene View acceptance remain open.

- ✅ The Inspector now assigns imported OBJ mesh assets and removes MeshRenderer for single and
  mixed multi-selection as one atomic generation-safe Undo/Redo operation. Each entity retains its
  material reference; mixed presence adds defaults only where absent. Runtime/document batches reject
  duplicate, missing and stale targets before mutation; no-op batches retain Redo. Contract tests
  drive actual combo/remove clicks, repeated replay, read-only rejection, selection/project/document
  staleness and save/reload. Missing references remain preserved; full Scene View acceptance stays open.

- ✅ MeshAssetCatalog now publishes owning imported geometry with stable UUID-derived 64-bit
  resource IDs and project-generation checks. Tests freeze persisted IDs, preserve references
  across rename/reopen, reject collisions atomically, and retain snapshots across unload.
  Persistent per-asset GPU caching and full Scene View acceptance remain open.

- ✅ SceneDocument now exposes generation-safe MeshRenderer attachment, mesh/material resource
  replacement, removal, and owning reads through Runtime Undo/Redo. Tests retain full 64-bit and
  unresolved IDs across scene save/reload, preserve Redo on no-op edits, and reject stale keys.
  Persistent per-asset GPU caching and full Scene View acceptance remain open.

- ✅ Stable-ID hierarchy/selection, cycle-safe reparenting, multi-selection, clipboard duplication,
  transform transactions, undo, and atomic scene save/reload are implemented in Editor Core.
- ✅ Portable Inspector property adapters and mixed-value multi-selection, opaque unknown-component
  round trips, a cancel-safe gizmo transaction state machine, generation-safe asynchronous picking,
  atomic camera persistence, 1,000-step undo/redo replay, and corrupt-scene state preservation are
  implemented and tested.
- ✅ Native and overview gestures cancel on application focus loss before synthesized input
  releases, on Undo/Redo and Create/Paste/Duplicate shortcuts, document-generation changes, hidden canvas, recovery,
  and preview-mode changes. Contract tests cover overview/native focus loss, Undo, and document
  replacement; a real Xvfb FocusOut during a prospective move preserves saved scene bytes.

- ✅ The public Presentation SceneDrawData boundary supports bounded geometry/instance batches
  in one native depth pass, with compatible whole-mesh defaults, portable range rejection, and
  distinct-geometry Vulkan pixels. [ADR-0002](ADR-0002-Editor-Scene-Mesh-Batches.md) records the
  contract; Editor persistent per-asset GPU caching and full Scene View acceptance remain open.

- ✅ The native 3D gizmo exposes Pivot/Center (P). Center handle placement, rotate/scale preview,
  and commit share the mean selected-root origin and first root local axes. Selected descendants
  do not weight the center twice; gesture-time tool/pivot/camera changes are disabled. Linux Xvfb
  drives two-root selection, snapped Center scale, Center rotation, and atomic Undo.

- ✅ Native Move/Rotate/Scale previews and commit share SceneDocument root edits and Runtime
  hierarchy composition. Owning prospective world-pose snapshots preserve dirty state, selection,
  and Undo/Redo; tests cover rotated, mirrored, nonuniform ancestors, selected descendants,
  stale keys, invalid factors, one-step Undo, and no visual jump on uniform-scale release.

- ✅ The graphical Hierarchy now renders a parent-aware expandable tree, filters by entity name,
  applies plain/Ctrl/Shift selection with a retained generation-keyed anchor, clips visible-row
  submission, and routes rename, sibling ordering, and drag/drop reparenting through generation-safe,
  undoable `SceneDocument` contracts. Stale entity/document generations are rejected.
- ✅ The docked graphical Inspector presents local position, Euler degrees (quaternion storage), and
  scale for single or mixed multi-selection. Position/Scale now retain bounded numeric drafts and
  commit only on Enter as one atomic Undo step, preserving current unrelated fields and each entity's
  rotation. Full-precision scientific input and negative scale work; invalid/zero-scale input rejects
  the batch and equal-value Enter preserves Redo. Real keyboard tests cover typing, Escape, focus loss,
  selection/reload, Play inspection, read-only/recovery/close gates and save/reopen. Drafts never save;
  full reflected Inspector and target-host acceptance remain open.
- ✅ The Inspector's single-selection Camera section adds/removes the component and edits validated
  vertical field of view and clipping planes through generation-keyed Undo. Scene save/reload retains
  the values; the complete reflected Inspector remains open.
- ✅ A single-selection Light section adds/removes the component and edits validated nonnegative
  intensity with the same Undo and scene save/reload behavior.
- ✅ Graphical rotation fields use degrees with explicit Z-X-Y composition and finite-value validation.
  Enter submits one atomic multi-selection transaction while preserving each target's other axes,
  position, and scale. Contract tests drive the real text widget with public key/text events.
- ✅ SceneDocument owns Euler hints, preserves authored revolutions across selection/save/reload,
  and restores them with undo even when the quaternion is unchanged. Editor scene format 2 validates
  finite, unique, rotation-matching hints and reads legacy version 1. Atomic same-World reload preserves
  scene ID/state and rejects malformed data or cross-scene ID collisions without changing live state.
  Target-host acceptance and the complete graphical save/restart workflow remain open.
- ✅ The graphical Scene panel now routes Ctrl+S and Save Scene through a one-shot application
  request. The application saves `.nexora/scenes/Main.scene`, reloads it on project open, rejects
  read-only saves, and preserves unreadable scene files instead of overwriting them. This is a
  single-scene persistence slice; the graphical save/restart acceptance workflow remains open.
- ✅ The Scene panel exposes Undo/Redo by button and Ctrl+Z/Ctrl+Y/Ctrl+Shift+Z outside text input.
  Runtime replay restores stable IDs, hierarchy, transforms, and Camera/Light components; document
  replay restores names, selection, and authored Euler revolutions. New edits discard Redo; tests
  cover replay and saving after undone creation. The complete visual workflow remains open.
- ✅ The graphical Hierarchy can create named roots and children through generation-checked
  requests, select the new entity, and reveal it in the tree. Contract coverage includes stale
  parents, Undo, and save/reload; the Scene View and full ED-M2 acceptance remain open.
- ✅ Ctrl+Shift+N creates a Hierarchy root outside text input. Linux Xvfb now exercises a graphical
  create, save, process restart, and scene reload, verifying the authored root survives.
- ✅ Graphical Hierarchy Copy/Paste buttons and Ctrl+C/Ctrl+V now use a world-pose snapshot clipboard,
  select and reveal pasted roots, and leave text inputs' clipboard shortcuts alone. Contract tests
  cover shortcut routing, source movement after copy, and single-copy Undo.
- ✅ The graphical Hierarchy now deletes selected subtrees with its button or a focused Delete key; Delete while hovering the native 3D canvas uses the same undoable document action.
  Node metadata and selection are restored by Undo; selected descendants are not deleted twice.
- ✅ Duplicate and Ctrl+D copy the current graphical selection while retaining the user's previous
  clipboard. Hierarchy row views are built after paste and duplicate actions mutate the document.
- ✅ The central Scene panel now has an interactive top-down X/Z overview: parent-composed entity
  positions, grid, wheel zoom, middle-button pan, and click selection shared with Hierarchy.
  Native OBJ output is now available; full Scene View acceptance remains open.
- ✅ The native Vulkan/DX12 scene draw contract now accepts a bounded physical-pixel viewport.
  Portable bounds checks and Vulkan Xvfb pixel readback cover clipping.
- ✅ The docked Scene canvas now publishes its visible framebuffer-pixel bounds after layout and
  DPI scaling, resetting them each frame.
- ✅ The Scene panel now offers a Vulkan/DX12 native 3D proxy preview inside that canvas, drawing
  depth-tested ground and position proxies for live scene nodes after UI submission. The X/Z
  editing overview remains available. Right drag orbits the preview camera, middle drag pans its
  X/Z target, Shift+middle drag pans target height, and the wheel zooms. F or Frame selected
  centers on selected nodes in X/Y/Z and adjusts orbit distance to their conservative proxy
  bounds (2–100 world units). Orbit angle, distance, and target height persist per scene. Clicking a visible position proxy selects
  its node across Scene, Hierarchy, and Inspector; Ctrl-click toggles. Proxy instances now show
  composed world rotation and scale; a conservative bound filters candidates before an exact
  rotated-box pick, including translation handles. Authored
  material shaders and exact sheared matrices remain open. Selected
  proxies show colored X/Y/Z translation handles in world or local space; Local axes uses
  the first selected node's world rotation, and picking a handle captures its axis for the drag. Dragging previews selected roots and descendants in world X/Z, along world Y with
  Shift-drag, or along the picked handle, then commits one undoable move on release. Escape cancels;
  optional 0.25–4 world-unit steps snap both preview and commit. The Rotate tool (E over the canvas;
  W returns to Move) draws X/Y/Z rings in world or local space and commits an in-place rotation of
  selected roots as one Undo step on release; the native proxy draw previews the turn of selected
  roots and descendants while dragging. The Scale tool (R over the canvas) draws local X/Y/Z
  cubes plus a white uniform cube, previews selected roots and descendants during drag, and commits an axis's or all three local scale components as one Undo step on release. Holding Shift at drag start snaps rotation to 15-degree steps or local scale deltas to 0.25-factor steps in both preview and commit. Full Scene View acceptance
  is still open.
- ✅ The Scene overview now shares Hierarchy's Ctrl/Shift multi-selection anchor and centers on
  selected world positions with F or Frame selected. Input-event tests cover both interactions.
- ✅ Scene overview marker dragging previews a world X/Z move; visible X and Z handles constrain
  the preview to one world axis. Release commits selected roots through one atomic, undoable
  transform transaction. Optional 0.25–4 world-unit steps snap the preview and commit together.
  Escape cancels; tests cover axis constraints, parent scale, subtree roots, and snapped descendants.
  Full 3D gizmo handles and renderer-backed Scene View remain open.
- ✅ The Scene overview center and zoom now use validated per-scene `CameraPersistence` on project
  open and normal writable shutdown. A corrupt camera file is preserved; Linux Xvfb acceptance
  checks that wheel zoom survives a project reopen. The proxy preview now has basic 3D camera
  controls; its orbit angle and distance now persist in a separate per-scene camera file.
- ✅ The graphical Scene panel now shows an exact unsaved-content indicator. The baseline advances
  only after successful save/reload, and Undo back to that content clears the marker; contract tests
  include subtree deletion, failed save, and external Runtime edits.
- ✅ A native close request now opens an unsaved-scene confirmation with Save and Exit, Discard and
  Exit, and Cancel. Failed or read-only saves keep the dialog open. External native-window destruction
  still stops rendering; graphical close/reopen acceptance remains open.
- Open: graphical Scene View, the complete reflected Inspector, renderer-backed picking, camera
  controls, gizmos, reflected widgets,
  and unknown-component visual workflows. ED-M2 exit still requires UI
  select/edit/undo/save/restart acceptance and visual evidence.

- **ED-M3 — PIE/debugging:** Game View, play/pause/step, fixed ticks, input focus, isolated worlds, apply policy, Console, runtime inspection, debugger boundary. The engine loads Zig gameplay; the Editor is not Zig `main`.
  - ✅ Portable `PlaySession` prerequisite covers isolated Play World ownership, fixed tick,
    play/pause/step, input-focus policy, discard-by-default, and explicit transform apply-back.
  - ✅ Portable debugging prerequisite adds structured bounded Console records, owning runtime
    inspection snapshots, debugger boundary/pause reasons, contained update recovery, and deterministic
    all-or-nothing transform conflict detection.
  - ✅ A docked Console now displays bounded Runtime records with text/severity filtering, source,
    timestamps, and dropped-record count; the Editor feeds startup and scene open/save diagnostics.
  - ✅ A docked Game panel now controls an isolated PlaySession through Play/Stop, Pause/Resume,
    and one fixed Step, with a copied entity inspection list, bounded X/Z world-pose preview, and
    bounded tick scheduling. F5/F6/F10 are keyboard routes; Linux Xvfb exercises the sequence. An optional gameplay library supplies callbacks and Stop discards the clone.
  - ✅ A bounded native Game View draws imported OBJ geometry through the isolated Play World's
    first valid active camera. Assets are frozen at Start and frames own their uploads after fixed
    ticks. Tests cover shared geometry, inactive scenes, reimport isolation, Pause/Step/Stop, and
    Xvfb/lavapipe camera pixels with an unchanged editor scene. Scene and Game share one native 3D
    submission per window; simultaneous visible canvases fall back to the Game inspection map.
  - ✅ Game View now offers a clipped Preview camera chooser with Automatic as the default and
    renderable active-camera choices. Selection is Play-session scoped and preview-only; read-only
    projects retain it, modal prompts disable it, and Stop/new Start discard it. Real UI clicks and
    Runtime tests cover removal, invalid native-float projection, unloading scenes, deterministic
    fallback and unchanged Editor/Play data and Undo. Frame preparation revalidates after ticks.
  - ✅ Runtime component wire read/write now accepts the isolated PlaySession World directly,
    sharing decoding and atomic command validation with GameWorld. Fixed/Step callback tests
    prove Editor isolation, parented world poses, component payloads, and failed-write rollback;
    the initial graphical input adapter is now implemented.
  - ✅ The Game panel now accepts an optional project-relative V3 gameplay library. Start loads it
    against the isolated clone; optional FixedUpdate runs on ticks/Step and Update once per playing
    frame. Bounded messages reach Console, failures reject Start/pause Play, and Stop/window shutdown
    unload before clone destruction. Static lifecycle tests and a real Xvfb dynamic library verify
    mesh movement, Pause/Step/Stop, and an unchanged authored scene. This initial component-oriented
    host does not advertise scene/physics services; expanded input devices and hot reload remain open.
  - ✅ Clicking the playing Game canvas now routes user-zero held WASD/arrow movement and
    Space/mouse/Shift/Ctrl buttons through copied gameplay input snapshots. Escape, pointer exit,
    hidden Game, Pause/Stop, prompts, and native blur clear capture and held state. Captured keys
    cannot trigger authoring shortcuts; F5/F6/F10 remain controls. Tests cover state transitions,
    owning snapshots, callback delivery, and native Xvfb input-only mesh movement/release.
    Gamepad, pointer look, rebinding, and multiple users remain open.
  - ✅ Selecting a Game entity now opens a read-only Play Inspector with copied local/world poses,
    parent and scene state, Camera/Light values, and full-width mesh/shader IDs. The Game panel shows
    pause reasons and callback failure counts; per-frame and fixed callback failures both release input.
    Snapshots remain valid after component removal and Stop; Editor selection and authoring stay separate.
  - ✅ Gameplay library selection now persists in bounded schema-1 project settings, independently
    of scenes and recovery journals. Read-only access never writes; invalid settings are preserved until
    an explicit replacement, and an unresolved recovery journal blocks shutdown saves. Explicit CLI
    paths (including empty) override the saved selection. Xvfb reopens without a CLI path and verifies
    that the saved library still drives the Play mesh; reopening alone does not load a module.
  - ✅ Game Apply Changes now pauses for an owning transform-diff review. Confirm rechecks Play,
    document and entity generations plus every original/Editor/Play value, then applies one atomic
    SceneDocument transaction and stops with clone discard. Conflicts/reparenting/unsupported scenes
    reject the complete batch. Undo restores all applied values; component/create/delete changes are
    never copied. Modal input blocks authoring/Play shortcuts; default Stop still discards.
  - Open: complete Game View materials/multiple native canvases, complete gameplay services and expanded input routing, complete
    runtime/build log routing, and native debugger/IDE integration.
- **ED-M4 — Prefabs/scenes/collaboration safety:** variants, override diff/revert/apply, nested rebase, additive scenes, migrations, autosave/recovery, external-change detection, and readable diff/merge. Safe source control precedes live collaboration.
  - ✅ Portable prefab prerequisite covers inspectable override diffs, targeted/full revert,
    immutable apply, variants, and nested-path rebase.
  - ✅ Portable additive-scene ownership/dependency ordering, migration dry-run, atomic bounded
    autosave/corrupt recovery, and stable-path three-way conflict records are implemented and tested.
  - Open: graphical workflows and source-control-provider UI integration.
- **ED-M5 — Specialized tools:** material/shader graph, animation, particles/VFX, audio, navigation/physics debug, terrain/vegetation, localization. Each is a capability plugin with honest read-only/unavailable states.
  - ✅ Portable capability registry enforces stable tool IDs and honest implemented/read-only/
    unavailable states with fallback reasons.
  - Open: graphical specialized tools and capability plugins backed by each production subsystem.
- **ED-M6 — Build/profile/extensibility:** profiles, cook/package, target/device matrix, remote logs, CPU/GPU/memory/frame tools, plugin manager, and API docs. Build success includes a target manifest and reproducible command.
  - ✅ Portable build frontend validates and atomically writes target/configuration/command and
    checksummed artifact manifests; bounded monotonic CPU/GPU/memory frame capture is implemented.
  - ✅ The graphical Profiler shows a live, bounded Editor frame processing wall-time trace with
    pause/clear, latest/average/peak, and evicted-frame count. GPU time and memory are labelled
    unavailable until instrumented.
  - ✅ The Profiler now exports retained Editor frame-processing wall times to project CSV with
    full double precision and an evicted-frame count. GPU/memory cells stay empty. The synchronous
    writer validates 1-600 ordered finite samples, rejects read-only/recovery writes, and atomically
    preserves the previous file on validation failure; real UI clicks emit one-shot requests.
  - Open: graphical build frontend, remote deployment/logs, GPU/memory profiling, versioned
    export, and plugin manager.
- **ED-M7 — Production hardening:** incremental indexing, virtualized UI, 100k-entity hierarchy, soak, workspace migration, corrupt recovery, signed-extension policy, opt-in telemetry/privacy, keyboard and screen-reader audit.
  - ✅ 100k-item virtual hierarchy ranges, trusted-publisher/signature policy, and telemetry that
    drops events until explicit opt-in are covered by portable tests.
  - Open: graphical performance/soak acceptance, workspace migrations, corrupt-document recovery,
    and keyboard/screen-reader audits.

## 4. Persistence and transaction contract

Scene, Prefab, and Project formats are versioned, deterministic, and atomically written. Commands contain stable targets, reversible before/after state, merge keys, and authoring timestamps that do not affect simulation. Long operations stage then commit; recovery journals preserve the last good file. Schema migrations support dry runs, backups, and reports; major versions never silently downgrade.

## 5. Acceptance matrix

| Area | Automated gate | Human/visual gate |
| --- | --- | --- |
| Documents | golden round trip, migration, corruption/fuzz | recovery workflow |
| Transactions | property tests and 1,000-step replay | gizmo and multi-edit behavior |
| Scene View | picking/gizmo math and render contracts | DPI, viewport, resize |
| PIE | isolation, start/stop stress, state diff | focus and pause/step workflow |
| Assets | deterministic import, cancellation/rollback, cycles | browser/status usability |
| Extensions | ABI/version/capability rejection | install/disable/recovery |
| Performance | startup/index/frame/interaction baselines | representative large project |

## 6. Release slices, dependencies, and non-goals

**Editor Preview** requires all of ED-M0, ED-M1, and ED-M2; none of those milestones alone qualifies.
**Creator Alpha** adds M3/M4, and **Production Beta** adds selected specialized tools, build/profile,
and hardening. Labels follow acceptance evidence, not panel existence or prerequisite groundwork.
Dependencies include Engine API M1–M5, window/swapchain, reflection, the asset pipeline, scene
snapshots, and the Editor SDK. Completed reflection, undo-model, or prefab foundations therefore do
not make the graphical Editor complete. Full visual scripting, a marketplace, cloud collaboration,
cinematic tooling, and every remote platform are separate future roadmaps.

## 7. Fixed delivery decisions

These are implementation inputs. Changing one requires an ADR, compatibility note, and synchronized
updates to both language editions.

| Topic | Decision and consequence |
| --- | --- |
| Product boundary | `NexoraEditor` is a standalone client of public engine APIs. Shipping never links `Editor`, `EditorImGui`, or editor metadata. |
| UI and modules | Dear ImGui docking is the first shell behind `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL`. Product-neutral models stay in `Editor`; widgets stay in `EditorImGui`; dependencies remain declared in `Config/Modules/modules.json`. |
| Presentation | All Editor windows use public `Window`/`Presentation`/`RHI`; private swapchains, backend downcasts, and a second event pump are forbidden. |
| Identity | Projects, assets, entities, components, documents, panels, commands, and tools use typed stable serialized IDs. Pointers, indices, paths, and labels are not identity. |
| Mutation | Authoring writes are transactions on the authoring thread. Workers return immutable, revision-tagged results for validation and commit. |
| Documents | Formats are versioned, deterministic, atomically replaced, and preserve unknown fields/components. Migration supports inspect, dry-run, backup, and report. |
| Async work | Import, index, thumbnail, build, and source-control jobs are cancellable. They never retain document pointers or mutate UI state. |
| PIE | Play owns a cloned world and separate input domain. Stop discards by default; apply-back is an explicit diff transaction. |
| Extensions | Extensions use the versioned Editor SDK capability registry and pass ABI, permission, dependency, and trust policy before registration. |
| Accessibility | Keyboard use, semantic mirrors, contrast, and screen-reader bridging are ED-M7 release gates; visible widgets alone are not evidence. |
| Evidence | Automated tests prove contracts and target-host captures prove graphical behavior. A mock, screenshot, or compile-only result cannot replace another named gate. |

## 8. Ownership, threading, error, and shutdown contract

```text
EditorApplication
  +-- WindowSystem / PresentationDevice / RenderSurface(s)
  +-- EditorUiHost (graphical feature only)
  +-- ProjectSession (zero or one)
      +-- AssetIndex + JobCoordinator
      +-- DocumentManager
      |   +-- SceneDocument(s) / PrefabDocument(s)
      |   +-- SelectionModel + TransactionHistory per document
      +-- PlaySession (zero or one cloned Play World)
      +-- ExtensionManager / CapabilityRegistry
```

Owners use RAII and destroy children in reverse order. Project close stops intake and jobs before
closing documents; documents close before Runtime services; GPU resources retire only after their
completion values. UI stores stable IDs or scoped weak handles, never owning pointers into ECS, asset,
plugin, or GPU storage. Every async result carries project generation, target ID, input revision/hash,
and operation ID, so completion after close, reload, undo, or reimport fails closed.

The authoring thread owns events, ImGui, documents, selection, transactions, extension callbacks, and
result commits. Render submission consumes an immutable frame packet. Workers read snapshots, write
staging output, and report through bounded queues with cancellation. File watchers only emit normalized
events; conflict decisions occur on the authoring thread. Shutdown stops intake, cancels work, drains
callbacks without committing, waits for required jobs/GPU values, writes recovery state, then destroys
services.

Expected failures use typed results with stable codes, actionable context, and an operation ID;
assertions are for internal invariants. A capability failure disables that capability but preserves
open documents. Failed writes retain the last good file, failed imports retain the previous artifact,
and device loss preserves CPU authoring state while device-owned resources are rebuilt.

## 9. Ordered AI construction plan

A package starts only when its dependencies and exit gates pass. Within each package, implement the
contract/model, automated tests, graphical binding, failure states, and target evidence in that order.
Portable prerequisites alone never close a graphical milestone.

### WP0 — Baseline audit and evidence ledger

**Depends on:** none. **Affects:** all milestones.

1. Map every requirement to source, tests, owning module, and one honest state: `absent`,
   `contract-only`, `portable`, `graphical`, or `target-accepted`.
2. Record support tier, compiler/backend, feature flags, unavailable SDKs, baseline results, and exact
   commands. Distinguish pre-existing failures; do not commit generated evidence.
3. Convert every open bullet into a gate with owner, dependencies, automated check, manual evidence,
   and rollback condition. Select the smallest end-to-end next slice.

**Exit gate:** inventory and source/tests agree, no open work is called complete, and the next slice has
an explicit acceptance test.

### WP1 — ED-M0 graphical shell acceptance

**Depends on:** WP0 and public Window/Presentation contracts.

Execute WP0–WP8 in the [Dear ImGui integration plan](Editor_ImGui_Integration_Plan.md). Prove a public
`RenderSurface` path, full event normalization, stable docking, DPI font rebuild, Unicode/IME candidate
placement, recovery UX, and device/surface recovery. Keep multi-viewport disabled until every platform
window follows identical ownership and presentation rules. Capture real-display Linux visual/input/
recovery evidence and Windows DPI/IME evidence.

**Exit gate:** all focused-plan gates pass with the feature on and off; viewport/project/application
closure leaves no callback or GPU resource referring to a destroyed owner.

### WP2 — ED-M1 project and content vertical slice

**Depends on:** WP1.

1. ✅ Finish create/open/upgrade validation: canonical roots, schema compatibility, lock/read-only mode,
   recent projects, and errors, without changing process working directory.
2. Bind the deterministic index to a virtualized Content Browser keyed by asset UUID. Add breadcrumbs,
   search/filter, selection, transactional rename/move/delete, and loading/error thumbnail states.
3. Use typed drag payloads containing project generation and asset UUID; validate type, target,
   permissions, and staleness before mutation.
4. ✅ Make import/reimport cancellable jobs with source/settings hashes, dependency edges, staged output,
   atomic publish, bounded progress, and structured diagnostics. Cancellation/failure preserves the old
   artifact.
5. ✅ Show forward/reverse dependencies and cycles. Debounce file events and require reload/keep/compare
   for dirty conflicts instead of overwriting.

**Tests/gate:** golden deterministic index/artifacts, upgrade/corruption, cancellation at every phase,
stale completion, watcher burst, and drag validation. A fresh project must import, find, inspect, move,
reimport, and recover an asset entirely in the UI without a destructive failure path.

### WP3 — ED-M2 scene-authoring vertical slice

**Depends on:** WP2 and production serialization/reflection APIs.

1. ✅ Key virtualized Hierarchy rows, expansion, selection anchor, filtering, rename, reorder, and cycle-safe
   reparent by entity/document generations.
2. Render Scene View to an Editor-owned RHI-neutral texture token. Resize with hysteresis, retire old GPU
   resources by completion value, and persist camera per document.
3. Tag async picking by frame, document, viewport, and entity generations; discard stale results and
   define empty, hidden, locked, and overlapping-object behavior.
4. Provide reflected adapters for scalar, enum/flags, vector, color, asset/entity reference, arrays, and
   nested structs. Display mixed values; preserve unknown components and opaque bytes.
5. Treat each gizmo gesture as one `begin/update/commit|cancel` transaction. Specify world/local,
   pivot/center, snapping, parents, negative scale, and multi-selection before polish.
6. Complete clipboard, duplicate/delete, dirty prompts, save/reload, and 1,000-step undo/redo replay.

**Exit gate:** users can select, inspect, edit, undo, save, restart, and visually verify a scene with
stable deterministic output and no unknown-data loss.

### WP4 — ED-M3 PIE and debugging vertical slice

**Depends on:** WP3 and Runtime snapshot/clone support.

1. Freeze the source revision and clone an isolated Play World; allocate separate Game View, camera,
   audio, and input focus. Start failure cannot mutate the Editor World.
2. Implement `Stopped -> Starting -> Playing <-> Paused -> Stopping -> Stopped`; reject invalid
   transitions, make stop idempotent, and make step exactly one fixed tick.
3. Route device input and shortcuts by explicit focus/capture; emergency stop and focus recovery remain
   available.
4. Feed Console through a bounded thread-safe buffer with severity/category/time/source and visible
   dropped-record count. Inspect snapshots, never relocatable live pointers.
5. Default stop to discard. Apply-back presents supported-field diffs and creates one transaction only
   if source revisions still match; otherwise invoke conflict resolution.
6. Keep debugger attach/detach/pause/location/diagnostics behind an adapter; Editor is never gameplay
   `main`.

**Exit gate:** repeated start/pause/step/stop leaks no world or focus state, isolation holds, and discard/
apply conflict behavior is explicit and tested.

### WP5 — ED-M4 prefab, multi-scene, and collaboration safety

**Depends on:** WP4 and stable scene/prefab schemas.

Implement prefab isolation, variants, nested stable-property override trees, diff, targeted/full revert,
apply, and rebase. Add additive-scene ownership, load order, cross-scene reference policy, save-all, and
dirty indicators. Add dry-run migrations with backup/report, recovery snapshots that never replace the
good file, external-change conflict UX, and canonical semantic source-control diff. Multi-document
operations stage every file and either commit the coordinated set or keep every original.

**Exit gate:** golden projects survive nested edit/rebase, additive save, migrate, crash, and reopen;
external changes cannot silently erase local edits and diffs name stable objects/fields.

### WP6 — ED-M5 specialized-tool capability platform

**Depends on:** WP5 and one production subsystem with an Editor-safe public API.

Extend capabilities with version, implemented/read-only/unavailable state, reason, permissions,
document types, and contributions. First deliver one thin reference plugin proving edit-preview-save-
reopen, undo, unload, and missing-backend behavior without private engine access. Then add material/
shader, animation, VFX, audio, navigation/physics, terrain/vegetation, and localization independently;
each owns a schema, preview lifetime, diagnostics, undo boundary, and budget. Missing plugins preserve
payloads and provide read-only fallback rather than discarding data or faking compilation.

**Exit gate:** the reference tool safely loads/unloads and survives absent or failed backends.

### WP7 — ED-M6 build, profile, and extension operations

**Depends on:** WP5 and the WP6 reference extension contract.

Serialize build target, configuration, features, cook roots, output policy, and toolchain ID. Preview an
escaped reproducible command, execute off the UI thread, support cancellation, bound streamed logs, and
require a checksummed artifact manifest in addition to zero exit. Remote adapters use authenticated
explicit deploy/run/stop/log states and never store secrets in projects. Profiler ingestion uses
monotonic time, bounded memory, dropped-sample counters, stable IDs, and versioned export. Plugin
installation validates SDK/ABI, manifest, permission, trust/signature, dependencies, and restart needs
before code loads.

**Exit gate:** equivalent manifest inputs reproduce outputs; cancelled/failed/incomplete builds cannot
report success; profiler/plugin failure leaves authoring operational.

### WP8 — ED-M7 hardening and release acceptance

**Depends on:** WP1–WP7 selected for the release.

1. Set startup, indexing, memory, frame, and interaction budgets with dataset, machine, build, sample
   window, and percentiles. Test 100k entities and production asset scale; remove unbounded per-frame
   work and queues first.
2. Soak repeated project/document/PIE/plugin/device-loss cycles with leak, deadlock, queue-growth, and
   recovery assertions.
3. Migrate all supported versions and fuzz corrupt/truncated inputs while preserving the last good file
   and a recovery report.
4. Audit keyboard traversal, visible focus, shortcut conflicts, contrast/scaling, semantic mirror, and
   screen readers. Track platform gaps as blockers or approved, owned, dated exceptions.
5. Verify signature and privacy policy: no disallowed plugin load, no telemetry before opt-in, redaction
   before persistence/transmission, inspectable queue, opt-out deletion, and offline behavior.

**Exit gate:** every claimed release gate passes on its supported target hosts, reviewed evidence is
archived, exceptions are explicit, and roadmap status is updated without rounding prerequisites up.

## 10. Cross-cutting implementation contracts

- **Identity:** distinct typed IDs are resolved through their owner. ImGui IDs derive from stable object/
  property identity plus document generation; labels remain localizable. Reuse increments generation.
- **Transactions:** use `begin -> preview/update -> validate -> commit|cancel`; record target IDs, source
  revision, reversible state, merge key, and description. Commit increments revision once; cancel
  restores exactly; undo invalidates stale jobs.
- **Persistence:** write a sibling stage, flush, validate/read back where needed, atomically replace, then
  update journals. Never edit the only good copy. Migration chains declare versions, preflight, backup,
  deterministic transform, validation, report, and no silent downgrade.
- **Jobs:** carry operation/project/target/revision identity, cancellation, progress, bounded diagnostics,
  staging location, and status. Commit only after revalidation. Normalize/debounce file events and
  correlate self-writes.
- **Rendering:** preview tokens are RHI-neutral and generation-safe. Allocate replacements before GPU-
  completion retirement. Empty/loading/error/suspended/minimized/occluded/out-of-date/device-lost are
  explicit states; Editor code never casts backend handles or assumes frames in flight.
- **Extensions:** SDK structs validate size/version and define ownership. Unload revokes registrations,
  cancels jobs, closes/fallbacks documents, drains calls, then releases the library. Safe mode records
  crashes; this plan does not claim native plugins are sandboxed.

## 11. Technical problem and solution register

| Problem | Required solution | Forbidden shortcut / verification |
| --- | --- | --- |
| Portable work is mistaken for graphical completion | Track the five WP0 states separately. | Panel stubs/test count do not close graphical gates. |
| Late job edits a reopened document | Revalidate project generation, target ID, revision/hash. | Workers never capture model pointers. |
| Drag creates hundreds of undo entries | Preview and commit one mergeable transaction; exact cancel. | No command per mouse move. |
| Watcher races Editor writes | Correlate self-write, debounce/hash, prompt on dirty conflicts. | No auto-reload over unsaved edits. |
| Cancelled import corrupts output | Content-addressed staging then validated atomic publish. | Never overwrite active artifacts directly. |
| ECS relocation breaks selection | Resolve stable typed IDs with generation checks at use. | No component address retained across frames. |
| Pick result is from an old frame | Validate frame/document/viewport/entity generations. | Never accept latest completion blindly. |
| Mixed Inspector values are lost | Explicit mixed state and field-only multi-target transaction. | Never copy first selection over all targets. |
| Component/plugin is unknown | Preserve opaque payload and show read-only diagnostics. | Never discard unknown data on save. |
| PIE leaks into authoring | Clone world; revision-checked explicit diff/apply. | Never simulate in Editor World. |
| Multi-file save partially commits | Stage/validate all plus recovery manifest; coordinated commit. | Never report subset success. |
| Resize/device loss frees live GPU data | Generation registry and completion-based retirement/recovery. | CPU frame age is insufficient. |
| Console/profiler grows forever | Bounded batching/backpressure and visible drop counters. | No unlimited history. |
| Plugin unload leaves callbacks | Revoke, cancel, fallback/close, drain, unload. | No reachable function pointer at unload. |
| Build falsely succeeds | Require exit success and validated checksummed artifacts. | Exit code alone is insufficient. |
| Accessibility inferred from pixels | Semantic bridge plus keyboard/screen-reader host audit. | Screenshot/widget tests are insufficient. |
| Benchmark is irreproducible | Record dataset, machine, build, window, and percentile. | No claim from one unlabeled run. |

## 12. Validation and evidence

Every implementation PR runs the Linux development gate. Run Shipping configure/build when module
boundaries, exports, optional features, plugin loading, or Shipping exclusion may change. Target-only
gates run on that host; Linux cloud output is not Windows/macOS/mobile evidence.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development

# Additionally for linkage or Shipping-boundary work:
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

| Gate | Automated evidence | Manual/target-host evidence |
| --- | --- | --- |
| ED-M0 | feature on/off, input, persistence, RHI lifetime/recovery | Linux display; Windows DPI/IME; docking/recovery |
| ED-M1 | deterministic import/index, cancel, corruption, stale completion | import/reimport/dependency/conflict workflow |
| ED-M2 | hierarchy/property/gizmo/pick math, serialization, replay | Scene/Inspector edit-save-reopen at multiple DPI |
| ED-M3 | isolation, state machine, fixed-step, apply conflict, stress | focus, pause/step, Console and Game View |
| ED-M4 | rebase, multi-scene atomicity, migration/recovery goldens | external-edit conflict and crash drill |
| ED-M5 | SDK/capability/unload/unknown-data tests | reference tool and missing-backend modes |
| ED-M6 | reproducibility, cancel/fail, bounded profiler, policy | build/deploy/log/profile/plugin recovery |
| ED-M7 | scale, soak, fuzz, migration, privacy | keyboard, screen-reader, contrast, large project |

Manual records include commit/configuration, OS, GPU/driver, display scale, locale/IME, flags, exact
steps, expected/actual, logs/captures, and reviewer. Redact secrets and private/telemetry data. Evidence
from a materially different commit or configuration is invalid.

## 13. AI change protocol and Definition of Done

For each slice: read this roadmap, focused plans/ADRs, relevant README, module graph, and tests; reconcile
source truth; list affected contracts, compatibility, failure modes, rollback, and evidence; add contract
tests; implement without downcasts/globals/sleep synchronization/raw persisted pointers/silent fallback;
format only touched C++; run exact gates; inspect the diff; synchronize both roadmap languages and
contract READMEs when behavior changes. Never call an unrun check passed. Prefer one reviewable vertical
slice per PR; schema/SDK changes include compatibility and migration, and architectural changes get an
ADR before widget code.

A milestone is done only when its model, graphical workflow, degraded/failure states, persistence, and
undo boundaries work; automated and required host gates pass; compatibility/migration is documented;
shutdown/cancellation/stale completion/recovery has stress coverage; errors are actionable; documents
and evidence match reality; and no build output, local preset, credential, private project, or sensitive
capture is committed. `Editor Preview` requires ED-M0–M2, `Creator Alpha` also requires M3–M4, and
`Production Beta` requires the selected M5 tools plus M6–M7. Preview/read-only capability never raises
the enclosing milestone percentage or waives a target-host gate.
