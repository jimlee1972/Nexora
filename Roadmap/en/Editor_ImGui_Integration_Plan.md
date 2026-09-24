# Editor ED-M0 Dear ImGui Integration Plan

> Version: v1.0 | Status: proposed plan, not yet started | Updated: 2026-09-24 | Relates to:
> `Editor_Roadmap.md` (ED-M0), `ADR-0001-Editor-UI-Framework.md`

## 1. Purpose

[ADR-0001](ADR-0001-Editor-UI-Framework.md) selected Dear ImGui and settled the "UI-framework ADR"
line item of ED-M0. This document plans the remaining ED-M0 scope the ADR explicitly did not
close: graphical docking, theme, DPI, IME wiring, the accessibility direction, and crash-recovery
UX. It is a plan, not a milestone status update: nothing here is implemented yet.

**This plan's first step introduces a new third-party dependency (vendoring Dear ImGui) and
changes the build system (a new CMake module, a new module-graph entry, a new feature option).**
Per this repository's standing rule ("遇到需要裝新相依、改 CI、或動 build 系統的情況，先講清楚再做"),
that specific step -- not the rest of the plan -- is what needs explicit confirmation before any
code is written. Writing this plan document itself does not cross that line.

## 2. Current baseline (verified against source)

- `Apps/Editor/NexoraEditor` (`Apps/Editor/main.cpp`) is a headless CLI today: it opens a project,
  indexes content, and writes a JSON report. It has no window, no rendering, and depends only on
  `Nexora::Editor` (see `Apps/Editor/CMakeLists.txt`).
- `NexoraEditorCore` (`Engine/Editor/`) is UI-toolkit agnostic by design (`Engine/Editor/README.md`):
  workspace, asset indexing, `SceneDocument` (create/select/reparent/undo), specialized-tool
  capability registry, and build-manifest frontend all exist and are portable-tested, but the
  README is explicit that "Docking, DPI/IME/accessibility, viewport rendering, gizmos, and
  native-host visual validation remain UI-host responsibilities" -- i.e. this plan's job, not
  something already done.
- `Nexora::Window` and `Nexora::Presentation` already provide the platform window, input
  translation, and RHI-backed `RenderSurface` that `Window_Presentation_Roadmap.md` WP-M3 names as
  what Editor Scene/Game views must reuse ("usable by Editor Scene/Game views without adding an
  Editor dependency to Runtime"). This plan's ImGui backend renders through that existing surface,
  not a new one.
- `Config/Modules/modules.json` already has the exact feature-gated-module pattern this plan
  needs: `Window`/`Presentation` are gated behind `NEXORA_ENABLE_WINDOW_PRESENTATION`, `Editor`/
  `EditorApp` behind `NEXORA_ENABLE_EDITOR`. A new `EditorImGui` module follows the same shape.
- This session's earlier bug-fix pass already touched code this plan directly depends on:
  `Engine/Window/src/X11Window.cpp`'s wheel-axis fix and `Win32Window.cpp`'s IME
  null/negative-size checks and UTF-8 title transcoding are exactly the input/IME plumbing Phase 2
  below needs to forward into `ImGuiIO`.

## 3. Scope and non-goals

In scope: vendoring Dear ImGui, one renderer/input backend per already-supported RHI/Window
platform combination, docking, DPI, IME forwarding, a first real panel wired through the existing
`NexoraEditorCore` registries, and crash-recovery UX built on the existing recovery-journal data
layer (`ProjectWorkspace::RecoverWorkspace`, already implemented per `Engine/Editor/README.md`).

Out of scope: ED-M1 through ED-M7's remaining graphical work (Content Browser, Scene View gizmos,
PIE Game View, specialized-tool panels, build/profile frontends, production hardening) -- this plan
only gets ED-M0 itself to graphical acceptance. Full accessibility implementation is also out of
scope; per ADR-0001, this plan's Phase 5 only wires the plumbing ImGui exposes (IME positioning),
not the secondary accessibility tree ED-M7 will need to scope separately.

## 4. The dependency decision this plan needs confirmed before coding starts

- **What**: vendor Dear ImGui (docking feature/branch) as source, most likely via CMake
  `FetchContent` pinned to a specific tag (not a live branch tracker), consistent with how this
  repository already pulls Vulkan headers as a build-time fetched dependency
  (`_deps/nexora_vulkan_headers-src` seen in this session's build output) rather than a git
  submodule.
- **License**: MIT, matching this repository's own `LICENSE` -- no friction (already confirmed
  while researching the ADR).
- **New CMake module**: `Engine/EditorImGui` (name open to bikeshedding), gated behind a new
  feature option, e.g. `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL` (default OFF until Phase 1 lands,
  then reconsidered), depending on `Editor`, `Presentation`, `Window`, `RHI` in
  `Config/Modules/modules.json`. `EditorApp`/`Apps/Editor` gains this as a new link dependency only
  when the option is on, so the existing headless CLI path keeps working with the option off.
- **CI impact**: a new Linux CI leg (and eventually Windows/macOS legs) building with the option
  on; until native rendering is actually exercised, an offscreen/headless ImGui smoke test (see
  Phase 1) keeps this testable on the existing Linux gate without a display.

If this shape is not what you want (different vendoring mechanism, different module name/location,
different default), say so before Phase 1 starts -- everything past this section assumes it.

## 5. Phased plan

### Phase 1 -- Vendor ImGui + minimal offscreen smoke test (Linux-verifiable here)

- Add the `FetchContent` declaration and the new `EditorImGui` module skeleton, module-graph entry,
  and feature option, all defaulted OFF so no existing build is affected.
- Write the Vulkan renderer backend first (the only native RHI backend this cloud session can
  actually run): translate ImGui draw data into `Nexora::RHI` texture/pipeline/command-list calls,
  targeting an offscreen render target the same way `ExecuteTriangleFrame` already does for the
  existing renderer contract tests -- no real window or display required.
- Gate: a new contract test renders one ImGui frame (even just `ImGui::ShowDemoWindow`) to an
  offscreen `Nexora::RHI` validation-device and Vulkan-device texture and asserts no validation
  errors and a non-zero draw-call count, mirroring the pattern `window_presentation.contracts`
  already uses for fake/offscreen surface lifecycle testing.

### Phase 2 -- Real window + input translation

- Wire the Vulkan backend to a real `Nexora::Presentation::RenderSurface` (per WP-M3, the same
  reusable surface owner Showcase already uses) instead of an offscreen texture.
- Translate `Nexora::Window` events into `ImGuiIO`: pointer position, mouse buttons, the
  now-correctly-axis-mapped wheel delta (X11 fix already landed), keyboard, and text/composition
  events into `ImGuiIO::AddInputCharacter`.
- Gate: target-host evidence only (a real window, real input) -- this is the same category as
  WP-M1's Windows-runner requirement in `Window_Presentation_Roadmap.md`; Linux/X11 can be
  developed and smoke-tested here, but full interactive verification needs a real display, which
  this cloud session does not have.

### Phase 3 -- Docking + first real panel

- Enable ImGui's docking branch feature set; define the initial dock layout and stable panel IDs,
  reusing `ProductShell::Panels()`/`IsStablePanelId` (`Engine/Editor/README.md`) rather than
  inventing a parallel panel-identity scheme.
- Wire one real panel end-to-end against live `NexoraEditorCore` state -- the Console or a minimal
  Hierarchy view are the smallest correct choices, since both already have a portable data source
  (`SceneDocument`'s node list) needing no new Editor Core work.
- Gate: the panel reflects live `SceneDocument` state and round-trips a user action (e.g. selecting
  a node) back into `SceneDocument::Select`.

### Phase 4 -- Theme / DPI

- Theme: a single first-class theme is sufficient for ED-M0 acceptance; per-user theming is not
  required by the milestone's gate.
- DPI: forward the DPI-aware sizing `Engine/Window`'s Win32 backend already computes
  (`AdjustWindowRectExForDpi`, seen in this session's Win32Window.cpp review) into ImGui's font
  atlas scale and style scale.

### Phase 5 -- IME wiring

- Forward `Engine/Window`'s composed-text events (the exact plumbing this session's Win32 IME
  null/negative-size fix hardened) into `ImGuiIO::AddInputCharacter`, and implement
  `io.SetPlatformImeDataFn` to position the native IME candidate window at ImGui's input cursor.
- Gate: a text field accepts IME composition input on Windows (target-host only) and does not
  regress the existing `Engine/Window` IME contract on Linux (no IME concept to wire there beyond
  what X11Window already handles).

### Phase 6 -- Crash-recovery UX

- `ProjectWorkspace::RecoverWorkspace` and the recovery journal already exist at the data layer
  (`Engine/Editor/README.md`). This phase adds only the UI: on startup, if a recovery journal is
  present, show a dialog offering recover-or-discard before the normal shell renders.
- Gate: killing the process mid-session and relaunching offers recovery; declining discards the
  journal exactly as the existing data-layer contract already guarantees.

### Phase 7 -- Accessibility direction (plumbing only, not implementation)

- Per ADR-0001, this phase is scoping work, not delivery: confirm the panel/command registry
  (`ProductShell::Panels()`, stable panel/command IDs) that Phase 3 already routes through is
  sufficient to drive a future secondary accessibility tree, and record any gaps found for ED-M7
  to pick up. No accessibility tree is built here.

## 6. Validation and Definition of Done

- Phases 1 and 3 (offscreen render, docking/panel logic) are verifiable on this repository's
  existing Linux gate.
- Phases 2, 4 (DPI), 5 (IME), and 6 (crash UX) need real-window target-host evidence on at least
  one platform each; Linux/X11 gets what coverage a display-having Linux host can provide, but
  Windows/macOS acceptance is explicitly out of this cloud session's reach, matching this
  repository's standing rule against claiming platform coverage that was not actually run.
- ED-M0 as a whole milestone is not marked accepted until every item in `Editor_Roadmap.md`'s
  ED-M0 gate (not just this plan's phases) has passing evidence -- this plan does not by itself
  authorize updating that milestone's status.

## 7. Risks

- Docking-branch maintenance: ImGui's docking feature has historically lived on a separate branch
  from mainline releases; pinning a specific tag (per §4) and re-evaluating on each upgrade is
  cheaper than tracking a moving branch.
- Font atlas + descriptor-set lifetime on Vulkan is a common source of validation errors when a
  window resizes; Phase 1's offscreen-first approach isolates this from window-resize interactions
  until Phase 2.
- Scope creep risk: it is tempting to build more than ED-M0 needs once a real window exists (e.g.
  starting ED-M1's Content Browser early). This plan's phase gates exist specifically to keep the
  work bounded to ED-M0's own acceptance criteria.
