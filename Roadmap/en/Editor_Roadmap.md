# Nexora Graphical Editor Roadmap

> Version: v1.3 | Status: AI-executable delivery plan | Updated: 2026-10-07

> **Progress: 0%** (none of ED-M0 through ED-M7 has passed graphical Editor acceptance;
> completed Runtime/Editor SDK prerequisites are not rounded up into an Editor milestone.)

**Completed prerequisites:** ✅ reflection metadata; ✅ command/undo data model;
✅ prefab override/rebase; ✅ isolated PIE session; ✅ dynamic plugin ABI gate; ✅ standalone
process and portable workspace/document core. **Open:** remaining graphical views, authoring
workflows, target-host acceptance, and production hardening.

Platform-only acceptance is deferred when a required host is unavailable. Continue independent
implementation and automated validation; preserve unverified evidence rows and do not mark the
corresponding milestone accepted until its host gate passes.

✅ Linux automated Vulkan validation now rejects exit-zero errors and covers attachment
synchronization and native RHI shader features ([evidence](../../Tools/Build/evidence/EditorEDM0-VulkanValidation-2026-10-06.md)).
ED-M0 remains open pending the target-host and workflow gates.

### Repository completion audit (2026-10-02)

The audit distinguishes a checked implementation foundation from an accepted graphical milestone.
Source and contract tests confirm the checked rows; no ED milestone currently satisfies its complete
automated **and** target-host gate, so overall graphical acceptance remains **0/8 (0%)**.

| Scope | Repository evidence | Accepted |
| --- | --- | :---: |
| ED-M0 shell foundations | Standalone process, optional ImGui host, stable panels, initial docking, input/DPI/IME forwarding, live Hierarchy, recovery modal, retained native GPU rendering, project layout persistence, and recovery failure contracts exist. Linux virtual-display recovery now verifies SIGKILL with a durable seeded journal, unchanged committed workspace, writer-lease reacquisition, and keyboard-only Recover/Discard; physical-display Linux and Windows DPI/IME host evidence remain open; a bounded Windows/DX12 developer-host shell smoke is recorded. | [ ] |
| ED-M1 project/assets | Portable create/open, schema upgrade, single-writer/read-only access, recent-project state, deterministic indexing/search, persistent sidecar UUIDs, virtualized Content Browser state, breadcrumb/selection, transactional mutations, typed generation-safe drag payloads, dependency/cycle inspection, transactional reimport, watcher debounce, and dirty-conflict decisions exist. The native shell exposes project status, provides a graphical create/open/recent selector, binds the real index to a graphical Content panel with recoverable project-local mutations, runs cancellable background import/reimport with bounded progress and structured diagnostics, shows dependency cycles, and presents blocking reload/keep/compare conflict UX; physical-display/Windows workflow acceptance remains open. | [ ] |
| ED-M2 scene authoring | Portable hierarchy/selection, reparent, sibling reorder (undoable Hierarchy drag model), multi-selection, clipboard, transform transaction, undo, and atomic save/reload exist, plus UI-neutral pick-ray, AABB picking, axis-drag, snapping, and viewport-resize-hysteresis math, and Unity-style translate/rotate/scale gizmo math with Global/Local axes, Pivot/Center, parents, negative-scale rules, and multi-selection roots. The graphical Hierarchy now presents a parent-aware expandable tree, filtering, generation-keyed expansion/selection, clipped visible rows, undoable rename, sibling ordering, and cycle-safe reparenting while rejecting stale entity/document generations. A docked Inspector exposes generation-safe position, Euler degrees (quaternion storage), and scale editing for single and mixed-value multi-selection, with atomic Runtime validation and one-step undo. Scalar opaque PBR material assets now import/reimport and support single-object Inspector assignment with persistent UUID references, Undo/save/reopen and real native Scene View palettes. Full authored-mesh Scene View, the complete reflected Inspector, complete material/shader workflows, camera authoring, and missing-plugin restoration remain open. Bounded read-only opaque component inspection and persistence are implemented. The native proxy preview already has Move, Rotate, and Scale handles. | [ ] |
| ED-M3 PIE/debugging | Portable `PlaySession`, structured bounded Console records, owning inspection snapshots, debugger adapter/pause reasons, failure recovery, and deterministic transform conflict rejection exist. The graphical Console shows bounded records and Editor diagnostics; a docked Game panel controls an isolated clone and copied inspection snapshot. Bounded native camera/OBJ Game View and frozen scalar PBR materials are implemented; complete materials/multiple canvases, complete gameplay services/expanded input, complete log routing, and native debugger integration remain open. | [ ] |
| ED-M4 prefab/scenes | Portable override diff/revert/apply, variants, and nested rebase exist. Native additive tabs, owning/reference documents, coordinated Save All and named composition reopen are accepted on Linux Xvfb. Graphical prefab, migration/recovery, semantic/provider conflicts and full target-host acceptance remain open. | [ ] |
| ED-M5 specialized tools | Stable capability IDs and honest implemented/read-only/unavailable states exist. No production graphical reference tool has passed edit-preview-save acceptance. | [ ] |
| ED-M6 build/profile/extensions | Portable build manifests/checksums and bounded monotonic profile capture exist. A docked Profiler plots live Editor frame processing time with pause/clear and dropped counts, and shows real current process resident bytes plus observed peak. CSV and schema-1 wall-time JSON export/import are available, alongside separate bounded process-memory JSON traces. Native Vulkan/DX12/Metal command-buffer GPU intervals have a separate bounded live history and schema-1 JSON capture with honest unavailable/software status. Build/deploy/log, physical GPU calibration, arbitrary capture import, and plugin-manager workflows remain open. | [ ] |
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
- ✅ WP0 reproducibility audit: graphical OFF 71/71 and ON with Slang 115/115 passed, no skips;
  Shipping engine built. [Linux evidence](../../Tools/Build/evidence/EditorEDM0-Linux-2026-10-05.md)
  and the focused plan now provide the remaining target-host checklist. ED-M0 remains open.
- Open acceptance: real-display Linux visual/input/recovery evidence and Windows DPI/IME evidence.
  A bounded Windows/DX12 developer-host shell smoke is recorded, but ED-M0 remains open until the
  complete target-host gates pass.

- ✅ Native client-pixel pointer events now convert once through the current frame DPI before UI
  hit testing. Cached positions reproject across scale changes; focus loss clears the cache and DPI
  changes cancel interrupted Scene gestures. Tests cover 100/125/150/175/200%, fractional/negative
  coordinates, event ordering, stationary pointers, invalid scale fallback, and 200% Apply dialog clicks.
  Deferred/zero-extent frames also forward gameplay key releases and focus loss without ticking
  Play or rendering a GUI frame. Target-host physical-display DPI evidence remains open.

- ✅ Native UI atlas acknowledgements are now scoped to a process-local surface resource domain.
  New owners receive the atlas at unchanged DPI, while move/resize retains it; native lifetime tests
  cover both replacement and teardown ([record](../../Tools/Build/evidence/EditorEDM0-SurfaceLifetime-2026-10-06.md)).

- ✅ ImGui context ownership now follows host State across move assignment and construction.
  The allocator-backed `editor.imgui_context_lifetime` gate detects the former 17-allocation leak,
  checks self-move and current-context restoration, and requires zero retained allocations.

- ✅ Public-RHI texture registrations no longer resurrect stale IDs after renderer/device reset;
  repeated reset and stale fallback are covered by `editor.imgui_contract`.

- ✅ Native UI images now use copied, bounded RGBA8 registrations with generation-checked fallback.
  The native lifetime gate checks owner replacement, DPI/resize reuse, limits and over 4096 uploads;
  DX12 recycles replaced descriptors only after GPU completion. Physical visual acceptance remains open.

✅ Final-head automated native UI acceptance passed on Linux/Vulkan (140/140), Windows/DX12
(123/123) and macOS/Metal (122/122); all three backends completed the 4290-upload lifetime gate.
[Immutable hosted evidence](../../Tools/Build/evidence/EditorEDM0-NativeImages-2026-10-06.md)
includes all 18 selected CI jobs. Physical display, installed IME and visual-legibility/glyph
coverage remain open; ED-M0 through ED-M7 remain unchecked.

- ✅ Scene-file acceptance fixtures release the consumed destination reader before atomic
  overwrite, retaining every SaveAs/New continuation assertion. The multi-launch Linux workflow
  now has a 120-second total budget with unchanged 10-second per-step deadlines. Final graphical
  Linux gate passes 178/178; updated hosted Windows verification remains pending and physical-host
  acceptance stays open. [Evidence](../../Tools/Build/evidence/EditorEDM7-AutosaveRecovery-Linux-2026-10-09.md).

- ✅ The shared Linux native Showcase gate now observes an animated presented frame before
  queuing Pause, then requires stable changed frames and exact replay. Delayed input/presentation
  regressions cover the original double-toggle race, missing motion and pause cleanup. Existing
  five-second comparisons and 15-second settling remain bounded; the nine-room aggregate receives
  180 seconds. This strengthens the cloud gate without accepting a physical Editor milestone.
  [Evidence](../../Tools/Build/evidence/EditorEDM0-LinuxAnimationHandshake-2026-10-09.md).

### ED-M1 — Project and asset workspace

- ✅ Content folders support real Tab focus and Enter activation; focused Alt+Up returns to the
  preceding breadcrumb and clamps at the Content root. Read-only traversal preserves mutation
  history and filters; keyboard ownership gates protect text, panel/focus and blocking-modal
  states. Folder entry does not also trigger the selected-scene Enter route. 1x/2x and macOS
  modifier coverage is recorded in [Linux evidence](../../Tools/Build/evidence/EditorEDM1-ContentFolders-Linux-2026-10-08.md).
  Screen-reader and physical-host accessibility acceptance remain open.

- ✅ Content Up/Down and Home/End navigate the complete matching asset rows with held-key repeat,
  clamped endpoints and clipper/scroll reveal. Shift keyboard and pointer selection share an
  inclusive anchored visible interval; read-only and existing ownership gates apply. 1x/2x and
  macOS modifier tests cover range shrink/reversal, stale scope and Rename handoff; 100k-row model
  coverage preserves history and rejects hidden endpoints. Full accessibility acceptance
  remains open. [Linux evidence](../../Tools/Build/evidence/EditorEDM1-ContentNavigation-Linux-2026-10-08.md).

- ✅ Focused Content Ctrl/Cmd+A selects the entire current-folder query/type result, including
  clipped rows and read-only inspection. Unmodified Delete uses one recoverable source/sidecar
  batch with one Content Undo. Text, panel/focus, drag and blocking-modal gates are tested at
  1x/2x; model tests cover 100k rows and large batch rejection/delete/Undo without per-ID scans.
  [Linux evidence](../../Tools/Build/evidence/EditorEDM1-ContentKeyboard-Linux-2026-10-08.md).

Create, open, and upgrade projects. Deliver a Content Browser with search/filter, folder/UUID,
drag/drop, import status, dependency inspection, and reimport. Background import must expose
cancellation, progress, and actionable errors, and must produce deterministic artifacts.

- ✅ Recent-project recording now checks the canonical root/name against its reader's 1024-byte
  UTF-8 limits before list mutation or staging. Linux long-root regression coverage proves rejection
  preserves the persisted list, in-memory entries and successful reopen of the last-good store.

- ✅ Workspace saves and recovery now use matching limits of 4096 documents and 1024 UTF-8
  bytes per path. Input is validated before any journal/stage write; a bounded line reader rejects
  oversized/corrupt records without changing the model or committed/recovery files. Missing legacy
  workspaces remain supported; non-regular metadata and file symlinks reject. `editor.workspace_budget`
  verifies maximum-size round trips, CRLF/no-final-LF compatibility, rejected saves and recovery.

- ✅ Project/workspace persistence now shares the scene/asset atomic replacement helper.
  Occupied `.tmp` files/directories/symlinks are preserved and reported with their path; a failed
  rename no longer deletes its destination to retry. Portable tests cover descriptor upgrades,
  gameplay settings, layout, recent-project rollback, committed workspace preservation and
  explicit journal recovery after a staging collision. Physical crash-during-write evidence remains open.

- ✅ Import queue admission now retains at most 64 operations by default, with a configurable
  capacity and retryable full-queue error. Queued jobs and unconsumed completed/failed/cancelled
  results retain their slot until consumption. `editor.preview_contract` covers mixed request
  kinds, queued/failure/cancel paths, 100 rejected retries, 100 readmission cycles and shutdown.
  This bounds operation count; the project index itself is bounded by the next item, and physical
  workflow gates remain open.

- ✅ `AssetWorkspace::ImportTree` now bounds one Content index at 262,144 files (".meta" sidecars
  excluded) and 1024 UTF-8 bytes per project-relative path, adjustable through `AssetIndexLimits`.
  Both limits are enforced while enumerating, before any identity sidecar is written: an oversized
  tree fails with an actionable error, keeps the previous index and creates no partial sidecars,
  and enumeration stops instead of growing memory. `editor.asset_source` covers exact-limit
  acceptance, over-limit rejection with index preservation, and sidecar-free rejection in writable
  mode. Per-entry diagnostic text, thumbnails and physical workflow gates remain open.

- ✅ Ordinary asset indexing and synchronous/background reimport now stream binary source hashes
  through an 8 KiB read chunk instead of retaining whole files. Mid-file cancellation publishes no
  partial hash; empty, embedded-NUL, exact-chunk and multi-chunk fixtures preserve the existing
  source/artifact hash format (`editor.asset_source`). OBJ size preflight/source limits, workspace
  geometry budgets and authoring-thread publication remain enforced. Physical workflow acceptance
  remains open.

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
- ✅ Focused Content F2 and context Rename now share owning UUID/generation/root/path drafts,
  focused/select-all UTF-8 filenames, Enter/Apply and Escape/Cancel. Invalid names are retryable;
  unchanged names clear errors and preserve Content Undo. Focus loss cancels before deferred rendering.
  The modal blocks authoring/File commands and cancels on
  focus/write loss, hidden Content, external modals or stale asset scope. Real 1x/2x Unicode/gate
  tests and Linux Xvfb F2/Enter with scene Save/Undo/restart verify the workflow; physical IME and
  complete graphical acceptance remain open.
- ✅ Linux native project-upgrade acceptance now delivers one keyboard gesture through explicit
  rendered keydown/keyup phases. Three cold runs and the affected 252-test graphical suite retain
  exact upgrade/backup/report, read-only conservation and Vulkan assertions.
  [Input evidence](../../Tools/Build/evidence/EditorEDM0-ProjectUpgradeInput-Linux-2026-10-10.md).
- Open: physical-display/Windows fresh-project workflow acceptance.

### ED-M2 — Scene authoring core

- ✅ Linux native Scene Preview rotation Save acceptance now retains one physical keyboard
  gesture across rendered release/modifier/key phases. Three cold workflows and the affected
  253-test graphical suite preserve existing preview/commit/Undo/scale/reopen assertions and
  deadlines. [Input evidence](../../Tools/Build/evidence/EditorEDM0-ScenePreviewSaveInput-Linux-2026-10-10.md).

- ✅ Scalar opaque PBR `.nmaterial` assets now import/reimport as bounded immutable typed data,
  with 64 KiB sources, 4096-asset/Content Undo budgets and canonical Renderer-schema validation.
  Single-object Inspector assignment uses a versioned owning opaque UUID reference, one Undo and
  save/reopen while preserving legacy shader IDs. Bounded reference inspection, cancellation,
  stale/read-only/missing/unsupported guards, and actual 1x/2x dropdown clicks are covered.
  Scene View submits deduplicated native PBR palettes with Renderer tangents; Vulkan pixels verify
  independent materials, reimport color changes, rejected versions and Undo/reopen. Texture/shader
  editing, persistent GPU caching and full ED-M2 acceptance remain open. Frozen scalar Game
  materials are covered separately by the ED-M3 supporting slice.
  Contract: [ADR-0005](ADR-0005-Editor-Scalar-PBR-Materials.md). Evidence:
  [Linux acceptance](../../Tools/Build/evidence/EditorEDM2-ScalarMaterials-Linux-2026-10-08/acceptance.md).
  [Main integration gates](../../Tools/Build/evidence/EditorEDM2-ScalarMaterials-Linux-2026-10-08/integration.md).

- ✅ Reflected Inspector writes now prepare one owning, bounded `InspectorEditBatch` and invoke
  one transaction callback. Empty/duplicate/oversized targets, stale or ambiguous field metadata,
  read-only properties and nonfinite scalar values reject before mutation. The legacy per-entity
  callback accepts only one target; multi-selection uses `ApplyBatch`, with live generation,
  permission and value-type validation owned by the authoring transaction. A real `SceneDocument`
  test covers one-step multi-target Undo/Redo, failed later-target rejection with Redo preservation,
  opaque-data retention and the 100k-target boundary. Complete reflected widgets, plugin restoration
  and ED-M2 target-host acceptance remain open. Evidence:
  [`EditorEDM2-InspectorAtomicBatch-2026-10-08.md`](../../Tools/Build/evidence/EditorEDM2-InspectorAtomicBatch-2026-10-08.md).

- ✅ Hierarchy Up/Down and Home/End navigate all visible tree/filter rows with repeat, clamped
  endpoints, Shift anchored ranges and clipper reveal. Plain Right expands/enters children;
  Left collapses/returns to the visible parent. Read-only inspection preserves World/history;
  owning generation-keyed cursor/anchor checks reject stale/hidden scope. Real 1x/2x and macOS
  modifier tests cover nested traversal, pointer/keyboard range handoff, reload/detach, Rename,
  text/focus/modal/drag ownership. [Linux evidence](../../Tools/Build/evidence/EditorEDM2-HierarchyKeyboard-Linux-2026-10-08.md).
  Screen-reader and physical-host accessibility acceptance remain open.

- ✅ Native Move now draws/picks XY/XZ/YZ planes in Global/Local axes and Pivot/Center, capturing a
  numeric plane basis for shared preview/release math and per-axis world-unit snapping. Shift keeps
  an explicit plane; Escape cancels. Tests cover invalid rays, shared boxes, mirrored/nonuniform
  parents, selected descendants, opaque bytes, preview/Redo and one-step Undo. Linux Xvfb moves and
  saves proxy/OBJ roots through all six planes after Scale/Rotate checks. Full gizmo acceptance stays open.

- ✅ Native Scene X switches Global/Local axes while Scale retains Local; P switches Pivot/Center
  before same-frame click/drag setup. Home navigation, actual hover/focus, modifier/text/modal,
  active/released gesture and read-only guards have real 1x/2x coverage with retained World/Redo.
  Linux Xvfb verifies Home/P center scale/rotate/move and one-step Undo on proxy/OBJ roots.
  Complete gizmo and target-host acceptance remain open.

- ✅ Scene Ctrl+A and Select all now update generation-keyed selection without changing World,
  dirty state, clipboard or history. Overview selects scene nodes; native uses an owning one-frame
  token and the actual 3,999-bounded draw/pick candidates, omitting hidden/locked records and tails.
  Read-only, empty, panel/text/modal/focus/backend and active/pending-drag gates have real 1x/2x
  coverage. Tests retain unknown bytes, clipboard and Redo, reject stale/malformed/duplicate packets
  and cover 4,001 nodes. Linux Xvfb selects both proxy/OBJ roots. Full Scene View acceptance stays open.

- ✅ Native Scene Select (Q) keeps ordinary/Ctrl picking while hiding transform handles and
  suppressing drag previews/commits. W/E/R restore Move/Rotate/Scale. Real 1x/2x input tests cover
  same-frame Q/click, toolbar parity, Home navigation, active/pending drag gates, read-only,
  panel/focus/modal/text guards and retained Redo. Linux Xvfb verifies proxy/OBJ picking, hidden
  handles and unchanged saved bytes after Select drags before returning to transform tools.
  Complete Scene View and target-hardware acceptance remain open.

- ✅ Scene Home and Frame all navigate without changing selection, World,
  dirty state or history. An owning token applies once using actual 3,999-bounded native submission
  candidates, upload/proxy fallback, exact affine mesh/proxy bounds and
  bounded FOV distance; overview fits all world origins to its logical canvas within zoom limits.
  Real 1x/2x tests verify empty selection/scene, read-only, button/keyboard parity, modal/panel gates,
  rejected/stale/one-shot bounds and retained Redo, including release/Home and 4,001-node limits.
  No extra idle-frame node snapshot is built to enable the button. Linux Xvfb restores native pixels for proxy and
  authored OBJ scenes. Full Scene View and physical-display acceptance remain open.

- ✅ Native F and Frame selected now center selected forests on exact world-transformed CPU mesh
  and rotated proxy bounds, including descendants once. Current clipped framebuffer aspect and the
  narrower viewport FOV set bounded distance; invalid/out-of-range bounds preserve the camera.
  Real 1x/2x keyboard/button tests cover offset mirrored/sheared geometry, portrait viewports,
  catalog replacement/stale fallback, read-only navigation, modal/panel gates and retained World/Redo.
  Framing owns no GPU data and performs no source IO; full Scene View acceptance remains open.

- ✅ Focused Hierarchy F2 now opens Rename with focused/select-all name input; Enter commits one
  metadata Undo and Escape cancels. The dialog blocks other authoring/clipboard/Undo/Save/Play
  shortcuts and queued Hierarchy writes. Real 1x/2x input verifies UTF-8 CJK/supplementary characters,
  retryable empty names, clipboard/history retention, save/reload and cancellation on write loss,
  external modal, focus loss or stale document. ImGui/library consumers share 32-bit Unicode text;
  font coverage and physical Windows IME acceptance remain open.

- ✅ Focused Hierarchy Ctrl+A now selects all filtered/expanded visible rows, including clipped
  rows, through generation-keyed selection. Empty matches clear selection; read-only projects retain
  the action. Keyboard tests verify collapsed descendants, filter order, foreign-panel/text-input
  focus, recovery/close gates and unchanged World/Redo. Large-scene scale/soak acceptance stays open.

- ✅ Typed Content mesh drags now assign the Inspector Mesh field for the displayed selection.
  Hover only previews; release uses one generation-checked batch, retaining existing materials and
  adding missing MeshRenderer components. Real 1x/2x pointer tests cover initialized Undo/Redo,
  save/reload, non-mesh/stale catalog/project rejection, canceled input and workspace/modal gates.
  Rejected drops retain Redo; complete material/reflected Inspector workflows remain open.

- ✅ Hierarchy Cut and Ctrl+X now capture complete selected forests before one atomic deletion.
  Undo restores original IDs and selection; first successful Paste keeps root names with new IDs,
  then retained clipboard data uses Copy naming. Failed Cut/Paste and Duplicate preserve pending
  clipboard state. Real 1x/2x key/pointer, workspace/modal/text-input gates, replay and persistence
  tests cover this lifecycle. Fresh layouts dock Inspector on the right to keep Hierarchy usable.

- ✅ Graphical Copy/Paste/Duplicate now captures complete selected-root forests with owned
  copy-time world root poses, child local transforms, Camera/Light/MeshRenderer payloads, authored
  Euler hints and opaque bytes. Parents map to new stable IDs, and selected descendants copy once.
  One initialized creation Undo removes the whole forest and restores prior selection; Redo retains
  initialized values. Real Ctrl+C/Ctrl+V/Ctrl+D/replay and persistence tests cover copy-time isolation,
  clipboard preservation, forward parents, validation/collision/lifecycle rejection and 1,000 cycles.

- ✅ Multi-selection Delete now removes all selected subtrees as one atomic Runtime/document
  transaction. One Undo restores stable IDs, sibling order, components, names, Euler hints,
  opaque payloads and the full selection. Real Delete/Ctrl+Z/Ctrl+Y tests verify the workflow;
  ID-collision/lifecycle and externally expanded-subtree rejection preserve history, unrelated
  entities survive, orphan roots retain captured world poses and a common merged sibling order even
  after different outside parents disappear, and 1,000 replay cycles keep snapshots.

- ✅ Typed Content asset drags now place resolved meshes on the overview X/Z point or native
  Scene Y=0 ground intersection, with an owning UUID/generation payload frozen at drag start.
  A tooltip previews the point without mutating the World; release creates/selects a named root
  with one initialized Undo. Real pointer tests cover 1x/2x DPI, Undo/Redo, save/reload and access,
  generation, modal, Escape/blur and unsupported-ground rejection without consuming Redo.
  Native placement shares the clamped preview camera and clipping limits. Surface snapping,
  geometry ghost previews, complete materials and target-host acceptance remain open.

- ✅ Content now creates/selects one resolved mesh root at the Scene center through Add mesh to
  Scene, with one initialized-entity Undo and stable-ID/name/pose/component Redo. Project-generation
  catalog checks and writable workspace/content/modal gates reject stale, missing, non-mesh or
  multiple selections. Native placement shares the preview's center clamp and target height.
  Real UI clicks, Game geometry preparation and Save/Reload verify the path; source IO stays outside
  the action. Complete materials remain open.

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

- ✅ Vulkan Scene/Game uploads now reuse bounded capacity in fence-protected frame slots.
  Steady, smaller and Scene-free frames retain allocation; growth stages replacement before retiring
  old storage, and resize/teardown drains GPU work. Native call tracing and pixels verify 100 steady
  frames, maximum descriptor budgets, failed growth and leak-free lifetime. Fresh geometry is still
  copied per draw; persistent per-asset GPU caching and full graphical acceptance remain open.

- ✅ Hierarchy now selects Empty, Camera or Light for Create root / Create child and Ctrl+Shift+N.
  Camera/Light are initialized at identity local TRS as one complete stable-ID Undo/Redo transaction.
  Default names follow type choices while custom names remain. Queued owning requests recheck
  access and scene/parent generations. Portable and 1x/2x real menu/pointer/key tests cover root/child,
  stale scenes/parents, read-only/modal gates, 100-step replay and save/reload. Full reflected component
  creation and target-host acceptance remain open.

- ✅ Inspector Copy values / Paste values now snapshots committed Transform/Euler, Camera or
  Light numeric values from one entity and applies them to multi-selection as one atomic Undo.
  Camera/Light preserve component absence; Transform retains authored turns. The typed owning
  clipboard survives source edits/deletion and reload, independently of hierarchy clipboard.
  Read-only Copy, disabled/mismatched Paste, draft cancellation, no-op Redo and persistence are
  covered by portable and 1x/2x real UI input tests. Full reflected Inspector remains open.

- ✅ Inspector now exposes Reset Transform, Reset Camera and Reset Light for multi-selection.
  Transform reset clears local TRS and visible/stale Euler revolutions; Camera/Light reset preserves missing
  components. Changed batches are atomic single-step Undo/Redo, and no-ops retain Redo. Reset cancels
  unsubmitted drafts and Scene gestures; workspace/modal gates apply. Portable and 1x/2x real UI
  input tests cover metadata-only Undo, mixed presence, unrelated payloads and save/reload.
  Complete reflected Inspector and target-host acceptance remain open.

- ✅ Camera/Light/Mesh presence-edit Undo now restores both the original flag and every stored
  value, including nondefault inactive lens/intensity and full-width mesh/shader references.
  Mixed batches retain exact bytes across 40 Undo/Redo cycles per component; no-op/invalid/stale
  inputs preserve history and failed replay retains its cursor. Full Linux 240/240 and Minimal
  Shipping pass. [Linux evidence](../../Tools/Build/evidence/EditorEDM2-DormantComponentHistory-Linux-2026-10-10.md).

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

- ✅ The graphical Inspector now interprets bounded, explicit reflection metadata for opaque
  components: Boolean, signed/unsigned scalars, enum/flags, vectors/colors, full-width entity/asset
  references, fixed arrays and flattened nested paths. Mixed selection commits one exact-source,
  generation-checked atomic Undo/Redo batch; Flags clicks preserve each target's other bits.
  Unknown bytes survive save/reopen. Project metadata
  reload revokes stale drafts; read-only and corrupt metadata preserve sources. Real controls at
  1x/2x and native Vulkan edit/save/restart are accepted in
  [the Linux evidence](../../Tools/Build/evidence/EditorEDM2-ReflectedInspector-Linux-2026-10-10.md).
  Native pointer delivery retains down/up across slow frames; eight consecutive actual runs pass.
  Presented X11 control/hover/held/released readiness and separate shortcut-key phases also
  pass eight cold, single-thread software-rendered native workflows (134.62s); all 239 graphical Development tests and Shipping build pass.
  The native fixture now retains the initial 1280x720 host size, removing the reproduced
  resize/dock initialization race; five consecutive cold native runs pass (75.21s).
  Dynamic arrays, native plugin restoration and complete ED-M2 acceptance remain open.

- ✅ Native pick/release authoring commands commit before Save, Save-and-exit, and GPU submission.
  Real Xvfb XYZ drags save immediately after release and verify the completed pose; saves while
  a gesture is held wait for commit or cancellation before serializing.

- ✅ Native Scene preview now draws resolved OBJ vertices/indices through bounded Presentation
  batches, packing shared resources once with absolute 16-bit indices and exact affine world instances.
  Transformed bounds and two-sided triangles replace proxy picking for resolved meshes. Portable
  tests cover range rollback, geometry/coordinate budgets, mirrored/rotated picking and silhouette
  misses; Xvfb distinct triangle/quad assets verify picking, Center scale/rotation, preview/release
  pixels, and one-step Undo. Missing/deleted/oversized assets retain references and warn while using
  proxies. Scalar opaque PBR assets are now supported ([ADR-0005](ADR-0005-Editor-Scalar-PBR-Materials.md));
  persistent per-asset GPU caching, complete texture/shader workflows and full Scene View acceptance remain open.

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

- ✅ Authored Scene geometry, conservative bounds and triangle picking now use exact WorldMatrix
  through mirrored/sheared ancestry; previews use owning prospective matrices matching commit.
  Native Game meshes use live post-tick Play matrices, even with older inspection metadata. Tests
  distinguish closed-form affine hits from lossy TRS misses and retain frame matrices through Stop,
  reject unrepresentable conversions per object and preserve frozen assets/clone isolation.
  Proxies/gizmos keep their TRS policy; material workflows, GPU caching and full acceptance remain open.

- ✅ Public Presentation instances now accept exact affine model matrices with common private
  inverse-transpose normal packing for Vulkan/DX12/Metal. Portable and native Vulkan pixel tests
  cover mirrored/sheared matrices and invalid-then-valid draws; Scene/Game authored meshes now
  consume owning exact matrices, while graphical acceptance remains open ([ADR-0003](ADR-0003-Presentation-Affine-Instances.md)).

- ✅ Deep mirrored/sheared hierarchy origins and gizmo position conversion are affine-exact.
  SceneDocument owns exact world and prospective matrix snapshots; translation and Center
  rotate/scale positions match commit, with one Undo/Redo and save/reload coverage in
  `editor.affine_gizmo_contract`. Native authored meshes now consume these exact matrices.

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
- ✅ File New/Open/Save/Save As now manage one active scene with project-relative UTF-8 paths,
  document/project tokens, dirty Save/Discard/Cancel, nested Untitled Save As and explicit Replace.
  Failed Open preserves the document/history/path; failed replacement retains the destination and
  preexisting temporary paths. Read-only permits Open and rejects writes;
  Play/recovery/close gates reject scene replacement. New clears history/clipboard and stays dirty
  until saved. Canonical aliases enforce metadata namespace/type; failed close saves reopen the
  attempted path/error for retry. Content saves stream only their own bounded source/identity,
  retaining unrelated geometry and earlier content Undo;
  per-file CPU camera state survives switches and writable shutdown. Real 1x/2x menu/key/modal tests
  and Linux Xvfb verify New, typed Save As, Open/reopen, source-file retention and read-only bytes.
  ✅ The native Open fixture waits for the existing committed startup association before later
  edit/save/Undo assertions, retaining its original operation deadlines and production routes.
  Content Unicode folder/asset labels, search, rename/move and Undo use UTF-8 and native paths,
  with portable and 1x/2x panel tests; they avoid Windows system code-page conversion.
  ✅ Startup restores the last successfully opened/saved scene with per-file view state, including
  read-only reopen. Bounded project/UTF-8 metadata revalidates managed scope; invalid/aliased,
  foreign or unavailable data falls back to Main and stays protected for the session. New/failed
  operations retain the prior choice, and independent metadata-write failure retains successful
  scene persistence. Portable tests and Linux Xvfb restart/edit/save/fallback verify the workflow.
  Full ED-M4 acceptance remains open; additive tabs are covered below.
- ✅ Content Browser scene activation now supports double-click, context Open scene, the Open scene
  button and focused Enter. Owning paths use the existing deferred scene-file request and dirty
  Save/Discard/Cancel workflow; UI widgets perform no file IO or World replacement. Read-only Open
  is allowed; Play/modal/token gates reject replacement. Real 1x/2x pointer/key tests cover Unicode
  paths, single/double clicks, non-scene/multiple selection rejection, dirty decisions and stale
  requests. Full ED-M4 acceptance remains open; additive tabs are covered below.
- ✅ Active Content scenes follow stable UUIDs through rename/move and Content Undo, retaining
  document generation, dirty content, selection, history and live view state. Committed relocation
  also updates startup filename without saving dirty World state, so Discard
  and Exit/restart reloads the relocated committed source. Missing/deleted,
  unsafe or stale tracked assets block ordinary Save; restoration resumes it, while a different UUID
  at the old filename cannot replace the association. Saved-scene indexing retargets stale moved
  entries only when old source and sidecar are absent. Portable Unicode/read-only/failure contracts
  and Linux Xvfb context rename/delete/Save/Undo verify the workflow; full ED-M4 remains open.
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
  centers on selected forests in X/Y/Z using exact world-transformed CPU mesh and rotated proxy
  bounds, including descendants once. The narrower viewport FOV adjusts orbit distance (2–100 world units). Orbit angle, distance, and target height persist per scene. Clicking a visible position proxy selects
  its node across Scene, Hierarchy, and Inspector; Ctrl-click toggles. Proxy instances now show
  composed world rotation and scale; a conservative bound filters candidates before an exact
  rotated-box pick, including translation handles. Authored meshes now use exact sheared matrices;
  scalar opaque PBR assets now execute in Scene View ([ADR-0005](ADR-0005-Editor-Scalar-PBR-Materials.md)).
  Complete texture/shader workflows remain open. Selected
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
  - ✅ Project input bindings now persist through explicit Apply and save owning UUID/root/profile
    requests. Schema-1 named records use a bounded 1 KiB reader and atomic writer, with corruption,
    duplicate/control validation, read-only/recovery/alias/staging preservation and compatible
    ordering/line-ending tests. Activation restores saved profiles, and startup recovery defers/retries
    loading after resolution. Linux Xvfb proves remapped B movement, old D rejection and read-only
    process reopen without changing settings/scene bytes. Device profiles and physical-host acceptance
    remain open. [Linux evidence](../../Tools/Build/evidence/EditorEDM3-ProjectInputBindings-Linux-2026-10-08.md).
  - ✅ Stopped Game input bindings now edit two finite keyboard/mouse slots per movement/button
    action and atomically apply a validated session profile. Duplicate/unknown/mouse-axis candidates
    reject; None unbinds. Cancel/Reset/read-only/project scope and modal/focus/Play gates have real
    1x/2x and macOS input coverage. Replacement clears held input; deferred batches and copied
    gameplay callbacks use the selected mapping without changing the C ABI. Expanded devices/users
    and physical-host acceptance remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM3-GameInputBindings-Linux-2026-10-08.md).
  - ✅ Portable `PlaySession` prerequisite covers isolated Play World ownership, fixed tick,
    play/pause/step, input-focus policy, discard-by-default, and explicit transform apply-back.
  - ✅ Portable debugging prerequisite adds structured bounded Console records, owning runtime
    inspection snapshots, debugger boundary/pause reasons, contained update recovery, and deterministic
    all-or-nothing transform conflict detection.
  - ✅ Runtime Console admission now clamps capacity to 4,096 records and enforces 256 B category,
    1 KiB source and 16 KiB message budgets with valid NUL-free UTF-8 and known severity. Rejections
    preserve accepted history/sequences; drops saturate and sequence exhaustion cannot wrap.
    Owning compaction releases oversized producer reserves. Exact-limit/Unicode/malformed input,
    counter exhaustion and four-producer snapshot tests cover the portable boundary; complete log
    routing and native debugger acceptance remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM3-ConsoleAdmission-Linux-2026-10-08.md).
  - ✅ Console Pause display retains an owning snapshot while producers continue admission/eviction;
    Clear view hides every current sequence without deleting ingress or resetting cumulative drops.
    Resume shows newer retained logs, and filters still work while paused. Window pointer/button tests
    at 1x/2x DPI cover background-producer eviction, source/null rebinding and 32 control cycles.
    Broader runtime/build log routing and full ED-M3 target acceptance remain open.
  - ✅ The graphical Console now receives owning sequenced observations from a real bounded Core
    async producer, including Editor and V3 Play logs. Pending/ring/text budgets, UTF-8/raw-wire
    rejection, monotonic restart-safe cursors and exactly-once upstream loss accounting keep
    worker traffic independent of UI Pause/filter/Clear. Four-producer, teardown, byte/sequence
    boundary and actual 1x/2x control tests cover the path; native Game fixtures verify Core and
    gameplay retained records at shutdown. Full Runtime/build producers and debugger/IDE remain
    open. [Linux evidence](../../Tools/Build/evidence/EditorEDM3-CoreConsole-Linux-2026-10-09.md).
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
    host now also provides the bounded Scene API and optional CPU collider queries below;
    rigid-body integration, expanded input devices and hot reload remain open.
  - ✅ The actual Play V3 host now loads/activates empty in-memory scenes and spawns/despawns
    validated Camera/Light/Mesh entities in the isolated clone. UTF-8 names, descriptor values and
    lifetime admission quotas are bounded; outputs and World data survive rejected operations.
    Atomic deletion cascades, Stop/Destroy callbacks retain service access, and new bindings reset
    quotas. World allocation rejects ID exhaustion without wrapping. Portable and actual dynamic
    Xvfb fixtures preserve Editor bytes across repeated Play lifecycles. Optional CPU Physics
    queries are described below; other optional callbacks remain unavailable.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM3-PlaySceneServices-Linux-2026-10-09.md).
  - ✅ Optional Play Physics spawn now owns at most 256 live local AABB bindings and exposes
    copied real CPU PhysicsWorld raycast results. Eight current affine-transformed corners preserve
    inherited rotation/mirror/shear, inactive/stale/unloading entries are excluded, and equal hits
    select the lower entity ID. Despawn cascades prune bindings; lifetime spawn quotas remain
    consumed. Invalid/miss/overflow failures preserve outputs, including Stop/Destroy lifetime.
    Actual V3, simulation-OFF and dynamic native Game fixtures cover queries and unchanged Editor
    data. [Linux evidence](../../Tools/Build/evidence/EditorEDM3-PlayPhysicsServices-Linux-2026-10-09.md).
    Rigid-body/backend stepping, authored collider import and full gameplay/input remain open.
  - ✅ Clicking the playing Game canvas now routes user-zero held WASD/arrow movement and
    Space/mouse/Shift/Ctrl buttons through copied gameplay input snapshots. Escape, pointer exit,
    hidden Game, Pause/Stop, prompts, and native blur clear capture and held state. Captured keys
    cannot trigger authoring shortcuts; F5/F6/F10 remain controls. Tests cover state transitions,
    owning snapshots, callback delivery, and native Xvfb input-only mesh movement/release.
    Gamepad, pointer look, device-specific rebinding and multiple users remain open.
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
  - ✅ Native Game View now freezes validated scalar PBR values and mesh-entity UUID assignments
    before Play. Native palettes use deduplicated bounded slots, neutral missing/budget fallbacks,
    Renderer tangents and copied post-tick camera/affine data. Actual catalog reimport/deletion and
    reassignment retain Play values; Pause/Step/Stop preserve Editor isolation and prepared frames
    own their data after Stop. Xvfb/lavapipe verifies red Game pixels and a fresh read-only green
    reopen with unchanged scene/source bytes. Texture/shader graphs and dynamic references remain
    open. [Linux evidence](../../Tools/Build/evidence/EditorEDM3-GameMaterials-Linux-2026-10-09.md).
  - Open: complete Game View materials/multiple native canvases, complete gameplay services and expanded input routing, complete
    runtime/build log routing, and native debugger/IDE integration.
- **ED-M4 — Prefabs/scenes/collaboration safety:** variants, override diff/revert/apply, nested rebase, additive scenes, migrations, autosave/recovery, external-change detection, and readable diff/merge. Safe source control precedes live collaboration.
  - ✅ Graphical prefab isolation is validated: create/open/variant/save/reopen use a separate
    native document, with hierarchy, name/position edits, Undo/Redo and dirty-close protection.
    Read-only inspection preserves project files. Accepted-owner integration passes full Linux
    242/242 and Minimal Shipping; phased native shortcuts retain exact additive ownership assertions.
    Nested materialization and override/rebase remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-GraphicalPrefabIsolation-Linux-2026-10-10.md).
  - ✅ Prefab isolation owner is validated: project-scoped create/open/variant/save/reopen use a
    separate World and document, retaining unknown bytes and original scene history. Dirty replacement,
    read-only writes, stale scope/revision and reentrant identity callbacks reject. Graphical binding,
    nested materialization, historical revision access and override/rebase remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-PrefabIsolation-Linux-2026-10-10.md).

  - ✅ Prefab revision retention is validated: superseded committed sources remain available by
    exact UUID/revision, and bounded project closure can resolve multiple revisions of one asset.
    Archive conflicts preserve current sources; failed current writes do not occupy future revisions.
    Graphical rebase/nested materialization and power-loss journaling remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-PrefabRevisionHistory-Linux-2026-10-10.md).
  - ✅ Prefab document save integration is validated: confirmed wrapped publication advances
    only the owning saved baseline while preserving generation, selection, clipboard and Undo/Redo.
    Current source/project and expected asset revision are rechecked; graphical isolation remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-PrefabDocumentSave-Linux-2026-10-10.md).

  - ✅ Stable prefab three-way comparison is validated: node/field UUID paths, stable parent
    references, unknown payloads and nested metadata remain independent of serialized Runtime IDs.
    Graphical override/revert/apply/rebase and structurally validated publication remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-StablePrefabComparison-Linux-2026-10-10.md).

  - ✅ Atomic same-identity document property snapshots are validated: complete mixed metadata,
    hierarchy/TRS and stored components use one guarded Undo/Redo, preserving generations, selection,
    clipboard and baseline. Latest Main integration passes full Linux 241/241 and Minimal Shipping.
    Structural changes and graphical prefab revert/rebase remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-AtomicPropertySnapshot-Linux-2026-10-10.md).
  - ✅ Prefab asset foundation now owns exact scene/unknown bytes, stable asset/node/field identities,
    revisioned codec, writer/expected-source publication and bounded exact base/nested closure.
    Actual rename/Undo/save/reopen, scoped repeated placements, cycle/stale/budget rejection and
    readonly/recovery/external-change/staging protection pass. Graphical isolation, instance
    materialization and override diff/revert/apply/rebase remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-StablePrefabAssets-Linux-2026-10-10.md).

  - ✅ Scene forest import now uses official bounded parsing, owning source-to-generation identity
    maps and one document-safe Undo/Redo with exact names, sibling order, components and retained
    Copy/Cut clipboard. Actual stale/corrupt/permission/budget rejection and save/reopen pass.
    Persistent prefab binding and graphical workflows remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-AtomicForestImport-Linux-2026-10-10.md).
  - ✅ The native graphical Editor now edits independent owned/reference documents through bounded
    scene tabs, dirty-close decisions, coordinated Save All and aggregate window-close protection.
    Persisted named roles/load order/active selection reopen in both access modes; failed complete
    restore freezes authoring and retains every original. Actual 1x/2x controls and X11 independent
    Undo, Save All, reference, corrupt/repair and close-to-single restart workflows pass. Legacy
    startup/relocation/fallback remains accepted. Combined multi-scene canvas/Hierarchy, graphical
    dependency editing, prefab/migration/provider workflows and full ED-M4 remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-GraphicalSceneTabs-Linux-2026-10-09.md).
  - ✅ The authoring SDK now persists a bounded, project-bound named scene composition with
    ownership/reference roles, deterministic dependency load order and active selection. Restore
    stages the complete candidate and rechecks source/metadata revisions before replacing membership;
    invalid, missing, colliding, externally changed or interrupted input preserves the primary and
    every original. Genuine sixteen-document, read-only, late-change and aggregate-budget tests pass,
    including missing baselines, byte-identical hard-link replacement and growth after size preflight.
    Graphical reopen integration is covered by the supporting slice; full ED-M4 remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-SceneComposition-Linux-2026-10-09.md).

  - ✅ Read-only semantic scene comparison now owns stable object/field differences across actual
    base/local/remote revisions using the production parser/migration. Missing/empty values, sibling
    order, known Runtime fields, authored Euler turns and exact unknown payloads remain distinct;
    unresolved conflicts and resource rejection preserve live data/history. Real legacy, component,
    source/opaque/aggregate bounds and 4,096-entity tests pass; equivalent quaternion signs and
    pre-ingestion authoring-node bounds also pass. Per-field choices are inspection hints;
    graphical provider/conflict presentation and valid merge publication remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-SemanticSceneComparison-Linux-2026-10-09.md).
  - ✅ The authoring SDK now owns bounded coexisting scene document/file sessions and releases
    their Editor World records after readers/owners drain. Active switches preserve identity,
    history and opaque data; references stay inspection-only and are excluded from Save All.
    Duplicate destinations, stale scopes, dependency cycles and unloading/unloaded publication
    reject safely. Real sixteen-document, rejected-admission lifecycle and independent reopen
    tests pass. Graphical tabs and persisted composition are covered by the supporting slices; full ED-M4 remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-AdditiveSceneSession-Linux-2026-10-09.md).
  - ✅ The authoring SDK now stages/revalidates a bounded immutable Save All batch, publishes all
    named scene files and acknowledges baselines only after complete verification. Interrupted
    publication restores exact originals or retains gated recovery data; foreign/corrupt inputs
    are preserved. Real multi-document/16-document, rollback, restart, initial metadata write
    failure and final directory cleanup retry tests pass.
    Graphical additive tabs and persisted composition are covered by the supporting slices; full ED-M4 remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-SceneSaveBatch-Linux-2026-10-09.md).
  - ✅ Scene saves now prepare an owning immutable byte/content/generation snapshot without IO,
    dirty-baseline or history changes. Deferred single-file publication revalidates live generation
    and serializable content, including opaque bytes and authored Euler turns, before IO; only a
    successful replacement marks the snapshot clean. Ordinary Save shares this path. Tests cover
    stale rejection, Undo/Redo, staging failure, ownership and save/reopen. Coordinated publication and additive tabs are covered below; full ED-M4 remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-PreparedSceneSave-Linux-2026-10-08.md).

  - ✅ Initial additive-scene dependencies now reject zero, self and missing IDs before graph
    mutation and normalize repeated edges consistently with dependency replacement. Dedicated
    portable tests preserve owned/reference descriptors and deterministic load order after rejected
    admission, verify cycle rollback and safe reverse-order removal. Graphical tabs/Save All are covered above; full ED-M4 acceptance remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-AdditiveSceneDependencies-Linux-2026-10-08.md).
  - ✅ Autosave writes enforce the same 64 MiB payload budget as recovery before touching files,
    retain last-good journals and occupied temporary paths, and clean failed replacement staging.
    Portable tests cover the exact limit, oversized rejection, binary/empty payloads, locale-independent
    headers, corrupt recovery, and failure/retry preservation. Graphical crash/recovery acceptance remains open.
  - ✅ Portable prefab prerequisite covers inspectable override diffs, targeted/full revert,
    immutable apply, variants, and nested-path rebase.
  - ✅ Portable additive-scene ownership/dependency ordering, migration dry-run, atomic bounded
    autosave/corrupt recovery, and stable-path three-way conflict records are implemented and tested.
  - Open: graphical prefab/migration/recovery and semantic source-control-provider UI integration.
  - ✅ Managed ordinary scene Save compares a bounded owning exact-byte disk baseline, including
    same-size edits with restored modification times. External changes require Replace/Cancel with
    a session/path/document-bound confirmation that rechecks disk revisions before writing. Ctrl+S,
    Save before New/Open and Save and Exit share this graphical workflow; failures retain scene,
    history and disk versions. Portable 64 MiB/lifecycle/Content relocation tests and 1x/2x ImGui
    control tests cover the supporting slice. Full prefab/migration/crash and source-control
    acceptance remain open; see [external-save evidence](../../Tools/Build/evidence/EditorEDM4-ExternalSceneSave-Linux-2026-10-08.md).
  - ✅ Graphical semantic source inspection now captures owning base/local/disk revisions and
    publishes bounded background comparisons only to the original live file/document scope.
    A read-only field table integrates with the retained external-save Replace/Cancel decision;
    references may inspect without writable access. Real 1x/2x controls and Linux native writer,
    read-only, corrupt-disk and additive-reference tests preserve sources and independent history.
    Cancellation, changed content/baseline/session and stale generations reject publication.
    Reviewed merge publication, provider integration and physical-host acceptance remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-GraphicalSceneConflicts-Linux-2026-10-10.md).
- **ED-M5 — Specialized tools:** material/shader graph, animation, particles/VFX, audio, navigation/physics debug, terrain/vegetation, localization. Each is a capability plugin with honest read-only/unavailable states.
  - ✅ Qualified native service lookup now checks the exact loaded plugin admission and owned
    provider identity, including copied registries, same-name reload and same numeric IDs across
    hosts. Manual replacements and revoked/pending/restart-required providers reject. Results
    remain owner-thread borrows; consumers release them before mutation/unload. This grants no
    operation authority or lifetime lease; graphical callbacks and reference-tool acceptance remain
    open. [Linux evidence](../../Tools/Build/evidence/EditorEDM5-QualifiedPluginServices-Linux-2026-10-10.md).
  - ✅ UI-neutral specialized-tool metadata now owns schema/interface versions, provider IDs,
    declared permissions, document/contribution identities and bounded resource declarations.
    Invalid/oversized admission preserves existing registry state; owning snapshots survive removal.
    These declarations do not grant permissions, enforce plugin allocations or prove a production
    graphical workflow. The reference plugin and edit/preview/save/unload acceptance remain open.
    [Linux contract evidence](../../Tools/Build/evidence/EditorEDM5-ToolCapabilities-Linux-2026-10-09.md).
  - ✅ Portable capability registry enforces stable tool IDs and honest implemented/read-only/
    unavailable states with fallback reasons.
  - Open: graphical specialized tools and capability plugins backed by each production subsystem.
- **ED-M6 — Build/profile/extensibility:** profiles, cook/package, target/device matrix, remote logs, CPU/GPU/memory/frame tools, plugin manager, and API docs. Build success includes a target manifest and reproducible command.
  - ✅ The graphical Build Console edits an absolute executable, working directory and discrete
    owning arguments, with explicit Run/Cancel and Ctrl+Enter/Ctrl+Shift+Enter. Live merged output
    is a bounded 16 KiB raw tail rendered with byte escapes, exact dropped counts and native exit
    status; exit zero explicitly leaves artifacts unverified. Scope/authority changes cancel work
    and clear old display, while native close drains a real long-running child. Actual 1x/2x controls,
    binary output, Unicode/empty/metacharacter arguments, failures and cancellation pass; existing
    export controls retain acceptance. Target profiles, verified build manifests and remote deploy
    remain open. [Linux evidence](../../Tools/Build/evidence/EditorEDM6-BuildConsole-Linux-2026-10-10.md).
  - ✅ The build prerequisite now runs actual scoped owning processes with direct native argv,
    bounded merged output/dropped counts and exact launch/exit/cancellation states. Real child
    fixtures cover Unicode/metacharacters, nonzero/signal failure, flood limits, queued/running
    cancellation, managed descendants, shutdown and stale scopes. Completed worker outcomes reject
    late cancellation; actual child tests verify unrelated host descriptors are not inherited.
    A zero exit does not certify
    artifacts or build success; graphical toolchains, verified manifests and deploy/log integration
    remain open. Raw output requires caller sanitization/redaction before display or persistence.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-BuildProcess-Linux-2026-10-10.md).
  - ✅ Optional native ProjectPlayer now draws actual owning StaticView package assets through
    bounded public NativePBR geometry/material/affine admission, without Editor, SDK, source content
    or Showcase. Authored camera/light selection, resized projection and native close/drain are
    explicit; unsupported/budget failures never draw a truncated subset or report success.
    Real CPU fixtures prove exact instance/palette/shared-geometry budgets and mirror/shear ownership;
    Linux Xvfb verifies red/green material pixels, resize, close and exactly four successful presents.
    Verification-only/feature-off builds remain supported. Gameplay, build/deploy/publication UI,
    physical display and other-platform native pixel acceptance remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-NativeProjectPlayer-Linux-2026-10-09.md).
  - ✅ The real plugin host now supports optional schema-one cooperative shutdown/quiescence,
    atomic bounded service registration and revocation in registry copies before native unload.
    Owning rows retain actual ABI/lifecycle diagnostics; legacy, rejected and pending work require
    restart instead of forced unload. Windows loads UTF-8 paths through native wide paths. The
    ExamplePlugin, fourteen real compiled modules and a C header consumer verify worker lifetime,
    Unicode paths, rejection/rollback and exact budgets. Consumers drain borrowed calls first;
    graphical PluginManager, installation/trust policy and native crash isolation remain open.
    [ADR-0007](ADR-0007-Cooperative-Plugin-Lifecycle.md) and
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-PluginLifecycle-Linux-2026-10-09.md).
  - ✅ Build export prerequisites now provide a bounded owning Runtime scene capture. The shared
    schema-3 writer caps output before appending; the Editor captures all Runtime entities plus
    tracked NodeKeys and full unknown metadata without IO or history changes. Entity/output and
    opaque name/payload budgets, exact wire compatibility, lifetime, tracked subsets and real
    Undo/Redo boundaries are tested. Generations identify objects rather than authoring revisions;
    export publication, native player and graphical build flows remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-OwningSceneCapture-Linux-2026-10-08.md).

  - ✅ Runtime-owned schema-1 mesh/scalar PBR/scene codecs and a bounded StaticView package now
    resolve real cooked assets into an isolated World without Editor or source content. The optional
    `NexoraProjectPlayer --verify-package` consumes actual files and reports inactive components.
    Full UUIDs, legacy shader IDs, opaque bytes and exact hierarchy matrices are preserved;
    corruption, unsupported schemas, unresolved dependencies and resource collisions reject.
    [ADR-0006](ADR-0006-Cooked-Static-Projects.md) records the compatibility/ownership decision.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-CookedStaticProject-Linux-2026-10-08.md)
    covers Development, Monolithic Shipping and real CLI consumption. Graphical Editor export, native player
    rendering, gameplay compilation and Build/deploy/log workflows remain open.
  - ✅ The pure Editor `CookStaticProject` producer now consumes an owning Runtime scene capture
    and explicitly supplied owning imported OBJ/scalar PBR values. It cooks the exact full-UUID
    dependency closure through shared Runtime codecs/AssetCooker/package validation, preserves
    snapshot/legacy shader/opaque bytes and rejects resource collisions or unsupported reserved
    bindings. Owning lifetime, deterministic independent captures, real 100k entities, exact
    geometry/opaque bounds and standalone ProjectPlayer consumption are covered.
    [Producer contract](../../Engine/Editor/StaticProjectExport.md) and
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-StaticProjectExport-Linux-2026-10-09.md).
    The separate coordinator below adds current-state/cancellation-checked graphical publication;
    native player rendering and full Build/deploy/log acceptance are separate slices.
  - ✅ The graphical Build menu now exports an owning StaticView package through a single background
    Core job. Real cooking/Runtime verification and chunked staging/readback precede authoring-thread
    project/document/content/catalog/recovery/access/cancellation checks and atomic publication.
    Ready status includes actual bytes/checksum and a reproducible ProjectPlayer verification command.
    Cancellation, stale scenes/projects/assets, failed cooking and unsafe/occupied output preserve
    the last package and authoring Undo/baseline. Actual core and 1x/2x UI-to-worker-to-Runtime tests
    cover this data-package slice; executable compilation, manifests, remote deploy/signing and full
    ED-M6 remain open. [Coordinator contract](../../Engine/Editor/StaticProjectExport.md#graphical-staticview-export-coordinator).
  - ✅ The Editor now opts into real completed native GPU timing: Vulkan/DX12 timestamp queries
    and Metal command-buffer timings publish copied source/submission/optional milliseconds through
    existing completion points. A separate bounded live Profiler history identifies software devices,
    source and scope, rejects stale/nonfinite input and breaks plots at unavailable records. Capture,
    Clear, domain changes and 1x/2x controls are covered; real Linux query allocation/read failure,
    fencing, resize, abandoned recording, teardown and default opt-out preserve rendering/lifetime.
    Physical calibration and per-pass profiling remain open; GPU captures use their own schema below.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-NativeGpuTiming-Linux-2026-10-09.md).
  - ✅ Native GPU timing has a separate schema-1 JSON capture with explicit backend/software,
    command-buffer scope, milliseconds, lossless completed submission/drop IDs and unavailable
    values. Synchronous writer-gated export and bounded read-only import preserve last-good files
    and owning static snapshots. Independent 1x/2x Export GPU / Import GPU / Clear GPU import
    controls preserve live CPU/GPU/RSS and other imports; project changes clear only static state.
    The shared nonrecursive JSON grammar also preserves existing wall/RSS capture contracts.
    Physical timing calibration, per-pass tooling and third-party capture formats remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-GpuTimingCapture-Linux-2026-10-09.md).
  - ✅ Profiler process-memory traces now retain at most 600 real RSS/working-set attempts with
    elapsed time, unavailable reads and independent eviction counts. Separate schema-1 project JSON
    export/import preserves uint64 precision, source/scope/units and pause gaps; import is owning and
    static, and failed reads/publication preserve prior state. Bounded corruption/limit/access tests
    and independent 1x/2x modal/recovery/read-only controls pass on Linux. GPU/allocator profiling and
    physical-host acceptance remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-ProcessMemoryCapture-Linux-2026-10-09.md).
  - ✅ Profiler Import JSON reads the exported schema-1 wall-time capture into an owning static
    trace, checking project UUID, source/scope/unit, ordered lossless frame/drop values and unavailable
    GPU/memory. The bounded nonrecursive reader rejects duplicate/unknown/missing fields, corruption,
    trailing data, unsafe paths and recovery without replacing the previous trace or changing live
    capture. Read-only and 1x/2x independent one-shot UI controls are covered.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-ProfilerJsonImport-Linux-2026-10-08.md).
  - ✅ Profiler Import CSV now reads the project's bounded exported wall-time capture into a
    separate static trace, with independent clear and read-only import. Live capture is preserved;
    corrupt/unsafe input, modal/recovery gating, locale-independent numeric precision, 600-frame/
    128-KiB budgets and 1x/2x pointer ownership are covered. Arbitrary capture import remains open.
  - ✅ Build manifest admission now validates all metadata as UTF-8 and enforces host-independent
    relative artifact syntax, including Windows drive/stream rejection on Linux. Malformed text,
    traversal, component-ending dots/spaces and separator/control aliases reject before any IO,
    preserving last-good manifests
    and unrelated staging; cross-target CJK/supplementary paths and valid retry are tested.
  - ✅ Build manifest publication now streams locale-independent schema-1 JSON through the shared
    native-path atomic publisher. Occupied file/directory/valid/dangling stages and failed replacement
    preserve unrelated data; UTF-8 destinations, uint64 byte counts, escaped controls, error clearing
    and retry are covered by cloud tests. The graphical build/deploy frontend remains open.
  - ✅ Portable build frontend validates and atomically writes target/configuration/command and
    checksummed artifact manifests; bounded monotonic CPU/GPU/memory frame capture is implemented.
  - ✅ The graphical Profiler shows a live, bounded Editor frame processing wall-time trace with
    pause/clear, latest/average/peak, and evicted-frame count. Native GPU intervals now use a separate history.
  - ✅ The application now samples real current process RSS / working-set bytes at most once per
    250 ms, and the Profiler reads copied optional latest/observed-peak values. Capture pauses OS
    observations, Clear resets peak/timer without resuming pause, and failed reads show unavailable
    while retaining the observed peak. Process-wide scope includes shared resident pages across
    project changes; it is neither GPU nor allocator usage. Linux native allocation/touch, actual
    24-frame graphical process, owner throttle/failure, and 1x/2x pointer/ownership/reset tests are
    covered. Schema-1 wall-time captures remain separate; Windows/macOS host validation remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM6-LiveProcessMemory-Linux-2026-10-08.md).
  - ✅ Profiler Export JSON now writes a schema-1 companion with source/scope/unit/project metadata,
    sample count, full double precision and lossless decimal-string uint64 frame/drop values.
    Unmeasured GPU/memory remain explicitly unavailable/null. Independent one-shot CSV/JSON controls
    share write/modal gates; 1x/2x pointer tests and independent normal/optimized Python JSON parsing
    cover precision, limits, failed replacement and preservation. Arbitrary capture import remains open.
  - ✅ The Profiler now exports retained Editor frame-processing wall times to project CSV with
    full double precision and an evicted-frame count. GPU/memory cells stay empty. The synchronous
    writer validates 1-600 ordered finite samples, rejects read-only/recovery writes, and atomically
    preserves the previous file on validation failure; real UI clicks emit one-shot requests.
  - Open: graphical build frontend, remote deployment/logs, physical GPU calibration, arbitrary capture import,
    and plugin manager.
- **ED-M7 — Production hardening:** incremental indexing, virtualized UI, 100k-entity hierarchy, soak, workspace migration, corrupt recovery, signed-extension policy, opt-in telemetry/privacy, keyboard and screen-reader audit.
  - ✅ Current session CI cleanup adds exact published branch and actor checks; 14 ownership/race
    and 16 documentation-routing tests, full Linux 239/239 and Minimal Shipping pass.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM7-CurrentCIQueueCleanup-Linux-2026-10-10.md).
  - ✅ The native project browser now offers an owning no-write upgrade preview in both access
    modes, showing captured schema/UUID, source/workspace counts and recovery evidence locations.
    Busy and typed-root changes clear stale intake/results; explicit Open reinspects ordinary
    writer access and source state. Real 1x/2x widgets and Linux native legacy/current/unsupported
    flows preserve source/backup bytes and existing Create/close guards. Broader migration/recovery
    and physical-host acceptance remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM4-ProjectUpgradePreview-Linux-2026-10-10.md).


  - ✅ SignedExtensionHost now verifies canonical signed identity/version/ABI/target/permission/
    dependency manifests and the actual artifact SHA-256 before native loading. Current trust/policy
    revisions are rechecked; Linux uses exact owning sealed memfd images with unique live identities,
    bounded admissions and cooperative revoke/unload. Real constructor fixtures prove denied
    admissions do not initialize; two distinct images, mutation rejection and lifetime limits pass.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM7-SignedNativeAdmission-Linux-2026-10-10.md)
    documents trusted in-process limits and post-admission dishonest-binary ABI detection.
    Graphical installation/recovery, native crash isolation and other-platform staging remain open.
  - ✅ ExtensionTrust now verifies actual pure Ed25519 signatures over bounded immutable artifact
    bytes through an optional Cryptography module backed by vetted OpenSSL >=3.0, then returns an
    owning SHA-256 digest and trust revision. Unknown publishers, tampering, malformed/over-budget
    inputs, unavailable providers and failures cannot verify. Bounded owning key configuration,
    rotation/revocation, independent RFC8032/SHA-256 vectors, exact 64 MiB input and explicit NONE
    backend tests pass. Native AUTO can select OpenSSL; cross-compiling requires an explicit target
    package/backend and otherwise rejects verification. No keys are downloaded or enrolled by an
    artifact. This verifier remains a prerequisite; the separate SignedExtensionHost slice above
    supplies canonical manifest/immutable native admission. Graphical installation remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM7-ExtensionSignature-Linux-2026-10-10.md).
  - ✅ Linux native center-gesture acceptance now repeats only Save while observing committed bytes
    inside the original deadline, retaining exact transform/saved-byte and one-step Undo assertions.
    Physical input/display and general synthetic-input stability remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM7-NativeSaveObservation-Linux-2026-10-09.md).
  - ✅ Graphical hierarchy traversal now uses explicit work storage rather than native recursion,
    preserving parent-first/sibling order for deep expanded scenes. Expansion pruning indexes current
    generation keys once, and name-only saved-state metadata avoids repeated World lookup.
    Real 100,000-node ImGui fixtures cover deep/flat order, clipping, navigation, collapse, filter
    and replacement at 1x/2x in the 3D-preview layout; overview pose traversal is a separate gate.
    These are bounded-by-input correctness foundations; complete frame/memory budgets, production
    asset scale, soak and physical-host acceptance remain open.

  - ✅ Top-down Scene overview now resolves owning bulk world poses with iterative indexed ancestry,
    preserving exact affine origins and the existing TRS approximation. Marker traversal, Frame all
    and prospective drag ancestry avoid repeated parent-chain lookup. Real 100,000-node Runtime and
    deep/flat ImGui fixtures cover reversed storage, 1x/2x, untracked ancestors, reload and corruption
    rejection. Full frame/memory budgets, production asset scale and soak remain open.
    [Linux contract evidence](../../Tools/Build/evidence/EditorEDM7-OverviewWorldPoses-Linux-2026-10-09.md).
  - ✅ Supported project descriptor upgrades now offer owning no-write schema-1/2 dry-runs.
    Read-write Open retains exact original bytes and an immutable plan-only report before source
    replacement, rejects foreign/corrupt/aliased evidence and occupied staging, and revalidates
    source revisions. Actual Unicode/CRLF, read-only, failure/retry/reopen and prior-live-writer
    fixtures preserve stable UUIDs, workspace bytes and last-good backups. Descriptor reads cap
    actual bytes before line parsing, including post-stat growth. Graphical/scene migration and
    broader crash/cancellation acceptance remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM7-ProjectUpgrade-Linux-2026-10-10.md).
  - ✅ Autosave recovery now bounds schema-1 headers before token parsing, rejects signed/overflow
    fields and non-regular/aliased files, and checks the exact file/payload length before allocation.
    Empty/binary/64 MiB/max-revision round trips, every-byte truncation and rejection preservation
    are covered. Mutation fixtures now seed a real workspace recovery journal, reset valid project
    metadata per input and exercise writable recovery; rejected journals preserve committed bytes,
    live documents and source data. Full migration/crash/target-host acceptance remains open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM7-AutosaveRecovery-Linux-2026-10-09.md).
  - ✅ Portable telemetry consent now releases all retained events on opt-out and bounds the
    in-memory queue to 1,024 events of 1,024 UTF-8 bytes each. Invalid/oversized/overflow events
    preserve accepted records; repeated revoke/enable cannot resurrect old events. Contract tests
    cover exact limits, malformed text, saturation and consent transitions. Persistence/transmission,
    redaction and broader graphical privacy acceptance remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM7-TelemetryConsent-Linux-2026-10-08.md).
  - ✅ The native Editor now provides session-local diagnostic privacy settings (Ctrl+Alt+T):
    default-off explicit consent, bounded inspection, clear, immediate opt-out deletion and
    project/close/restart reset. The built-in producer records only a fixed frame-presented label;
    unknown event text is excluded from display. Actual 1x/2x widget and Xvfb keyboard workflows
    preserve read-only source bytes. No diagnostic storage or network transport is installed;
    graphical signed-package management and persistence/transmission policy remain open.
    [Linux evidence](../../Tools/Build/evidence/EditorEDM7-DiagnosticPrivacy-Linux-2026-10-10.md).
  - ✅ Cloud documentation-routing Git fixtures now disable local automatic maintenance/GC before
    commits, preventing detached housekeeping from racing strict temporary-directory cleanup.
    Routing semantics and product/global Git settings remain unchanged.
  - ✅ Recovery presence now detects occupied/uninspectable paths, including directories and
    dangling aliases, preserving existing authoring/export/shutdown gates until explicit resolution.
    Tests cover rejected recovery, nonrecursive/read-only discard and actual 2x Profiler modal gating.
  - ✅ Project layout save/read share a 1 MiB raw-payload budget, bounded schema headers and linear
    CRLF normalization. Corrupt/NUL/over-budget or non-regular/aliased files reject without mutation;
    exact-limit schema-0/1 round trips and failed-save preservation are covered by cloud tests.
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
