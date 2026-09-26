# Nexora Graphical Editor Roadmap

> Version: v1.2 | Status: AI-executable delivery plan | Updated: 2026-09-24

> **Progress: 0%** (none of ED-M0 through ED-M7 has passed graphical Editor acceptance;
> completed Runtime/Editor SDK prerequisites are not rounded up into an Editor milestone.)

**Completed prerequisites:** ✅ reflection metadata; ✅ command/undo data model;
✅ prefab override/rebase; ✅ isolated PIE session; ✅ dynamic plugin ABI gate; ✅ standalone
process and portable workspace/document core. **Open:** window/docking/UI shell,
graphical views, authoring workflows, and production hardening.

### Repository completion audit (2026-09-24)

The audit distinguishes a checked implementation foundation from an accepted graphical milestone.
Source and contract tests confirm the checked rows; no ED milestone currently satisfies its complete
automated **and** target-host gate, so overall graphical acceptance remains **0/8 (0%)**.

| Scope | Repository evidence | Accepted |
| --- | --- | :---: |
| ED-M0 shell foundations | Standalone process, optional ImGui host, stable panels, initial docking, input/DPI/IME forwarding, live Hierarchy, recovery modal, retained native GPU rendering, project layout persistence, and recovery failure contracts exist. Real-process recovery and Linux/Windows host evidence remain open. | [ ] |
| ED-M1 project/assets | Portable create/open, deterministic indexing/search, virtualized Content Browser state, breadcrumb/selection, transactional mutations, typed generation-safe drag payloads, dependency/cycle inspection, transactional reimport, watcher debounce, and dirty-conflict decisions exist. Graphical workflow acceptance remains open. | [ ] |
| ED-M2 scene authoring | Portable hierarchy/selection, reparent, multi-selection, clipboard, transform transaction, undo, and atomic save/reload exist. Scene View, Inspector, picking, cameras, gizmos, and reflected graphical widgets remain open. | [ ] |
| ED-M3 PIE/debugging | Isolated `PlaySession`, fixed tick, pause/step, focus policy, discard, and explicit transform apply-back exist. Graphical Game View, Console/runtime inspection, and debugger integration remain open. | [ ] |
| ED-M4 prefab/scenes | Portable override diff/revert/apply, variants, and nested rebase exist. Graphical prefab/multi-scene, migration/recovery, conflict, and source-control workflows remain open. | [ ] |
| ED-M5 specialized tools | Stable capability IDs and honest implemented/read-only/unavailable states exist. No production graphical reference tool has passed edit-preview-save acceptance. | [ ] |
| ED-M6 build/profile/extensions | Portable build manifests/checksums and monotonic profile capture exist. Graphical build/deploy/log/profile/plugin-manager workflows remain open. | [ ] |
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
  ED-M0 remains open until those target-host gates pass.

### ED-M1 — Project and asset workspace

Create, open, and upgrade projects. Deliver a Content Browser with search/filter, folder/UUID,
drag/drop, import status, dependency inspection, and reimport. Background import must expose
cancellation, progress, and actionable errors, and must produce deterministic artifacts.

- ✅ Project create/open, deterministic content-tree indexing, UUID/path search and filtering,
  cancellation, progress, inspectable errors, and deterministic artifact hashes are implemented.
- ✅ Portable virtualized Content Browser/breadcrumb/selection models, transactional rename/move/
  delete, typed generation-safe drag validation, dependency/cycle inspection, transactional
  reimport, watcher debounce, and explicit dirty-conflict decisions are implemented and tested.
- Open: graphical Content Browser, drag/drop, dependency inspection, and reimport UX acceptance.

### ED-M2 — Scene authoring core

Deliver Hierarchy, Scene View, Inspector, camera controls, selection/picking, translate/rotate/scale
gizmos, parenting/reordering, multi-selection, clipboard, undo/redo, and save/reload. Reflection
creates property widgets; unknown components retain raw data instead of being silently discarded.

- ✅ Stable-ID hierarchy/selection, cycle-safe reparenting, multi-selection, clipboard duplication,
  transform transactions, undo, and atomic scene save/reload are implemented in Editor Core.
- ✅ Portable Inspector property adapters and mixed-value multi-selection, opaque unknown-component
  round trips, a cancel-safe gizmo transaction state machine, generation-safe asynchronous picking,
  atomic camera persistence, 1,000-step undo/redo replay, and corrupt-scene state preservation are
  implemented and tested.
- Open: graphical Hierarchy/Scene/Inspector, renderer-backed picking, camera controls, gizmos,
  reflected widgets, and unknown-component visual workflows. ED-M2 exit still requires UI
  select/edit/undo/save/restart acceptance and visual evidence.

- **ED-M3 — PIE/debugging:** Game View, play/pause/step, fixed ticks, input focus, isolated worlds, apply policy, Console, runtime inspection, debugger boundary. The engine loads Zig gameplay; the Editor is not Zig `main`.
  - ✅ Portable `PlaySession` prerequisite covers isolated Play World ownership, fixed tick,
    play/pause/step, input-focus policy, discard-by-default, and explicit transform apply-back.
  - Open: graphical Game View, Console/runtime inspection, and debugger integration.
- **ED-M4 — Prefabs/scenes/collaboration safety:** variants, override diff/revert/apply, nested rebase, additive scenes, migrations, autosave/recovery, external-change detection, and readable diff/merge. Safe source control precedes live collaboration.
  - ✅ Portable prefab prerequisite covers inspectable override diffs, targeted/full revert,
    immutable apply, variants, and nested-path rebase.
  - Open: graphical workflows, additive scene tooling, migrations, recovery, and source-control diff/merge.
- **ED-M5 — Specialized tools:** material/shader graph, animation, particles/VFX, audio, navigation/physics debug, terrain/vegetation, localization. Each is a capability plugin with honest read-only/unavailable states.
  - ✅ Portable capability registry enforces stable tool IDs and honest implemented/read-only/
    unavailable states with fallback reasons.
  - Open: graphical specialized tools and capability plugins backed by each production subsystem.
- **ED-M6 — Build/profile/extensibility:** profiles, cook/package, target/device matrix, remote logs, CPU/GPU/memory/frame tools, plugin manager, and API docs. Build success includes a target manifest and reproducible command.
  - ✅ Portable build frontend validates and atomically writes target/configuration/command and
    checksummed artifact manifests; monotonic CPU/GPU/memory frame capture is implemented.
  - Open: graphical frontend, remote deployment/logs, live profiler integration, and plugin manager.
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

1. Finish create/open/upgrade validation: canonical roots, schema compatibility, lock/read-only mode,
   recent projects, and errors, without changing process working directory.
2. Bind the deterministic index to a virtualized Content Browser keyed by asset UUID. Add breadcrumbs,
   search/filter, selection, transactional rename/move/delete, and loading/error thumbnail states.
3. Use typed drag payloads containing project generation and asset UUID; validate type, target,
   permissions, and staleness before mutation.
4. Make import/reimport cancellable jobs with source/settings hashes, dependency edges, staged output,
   atomic publish, bounded progress, and structured diagnostics. Cancellation/failure preserves the old
   artifact.
5. Show forward/reverse dependencies and cycles. Debounce file events and require reload/keep/compare
   for dirty conflicts instead of overwriting.

**Tests/gate:** golden deterministic index/artifacts, upgrade/corruption, cancellation at every phase,
stale completion, watcher burst, and drag validation. A fresh project must import, find, inspect, move,
reimport, and recover an asset entirely in the UI without a destructive failure path.

### WP3 — ED-M2 scene-authoring vertical slice

**Depends on:** WP2 and production serialization/reflection APIs.

1. Key virtualized Hierarchy rows, expansion, selection anchor, filtering, rename, reorder, and cycle-safe
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
