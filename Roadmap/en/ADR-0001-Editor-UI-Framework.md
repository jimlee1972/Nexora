# ADR-0001: Editor UI Framework

> Status: Accepted | Date: 2026-09-24 | Relates to: `Editor_Roadmap.md` (ED-M0)

## Context

`Editor_Roadmap.md`'s ED-M0 milestone requires the UI framework to be "selected through an ADR
and focused prototypes"; the shell must not begin production widget implementation before that
decision is recorded. Until now the choice existed only as an informal design note in
`Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md` ("Editor UI: Dear ImGui Docking /
Multi-Viewport"), never written up as a decision record with alternatives and consequences. This
ADR closes that specific gap. It does not itself satisfy the rest of ED-M0: graphical docking,
theme, DPI, IME, accessibility, and crash-recovery UX remain open implementation work regardless
of which framework this ADR names.

Two architectural constraints, both already established elsewhere in the repository, bound the
choice:

- `Engine/Editor/README.md`: `NexoraEditorCore` is UI-independent and composes public Runtime
  Editor SDK APIs; it must not reach into renderer or platform internals.
- `Editor_Roadmap.md` §3: "the shell depends on the public window/swapchain path and must not
  invent a temporary private presentation path merely to display UI" -- the Editor's Scene/Game
  views are `Nexora::Presentation`-owned `RenderSurface`s (see `Window_Presentation_Roadmap.md`
  WP-M3), not a separate rendering stack the UI framework brings with it.

In other words, whatever framework is chosen must render *through* Nexora's own RHI and Window
modules, not bring a competing windowing/rendering stack that the Scene/Game viewports would then
have to be awkwardly embedded into (or, worse, duplicate).

## Decision

Adopt **Dear ImGui**, using its docking/multi-viewport feature set, as the Editor's UI framework.

A single project-owned backend layer (`engine::editor::imgui` / `engine::ui::imgui`, matching the
namespacing already sketched in the V1 Complete Plan) renders ImGui draw data through
`Nexora::RHI` and translates `Nexora::Window` input events into `ImGuiIO`, mirroring the pattern
of ImGui's own official Vulkan/DX12/Win32/Cocoa backends but written against Nexora's existing
abstractions instead of the native APIs directly. No second window-system or swapchain path is
introduced. `NexoraEditorCore`'s panel/command/selection/document model continues to be UI-toolkit
agnostic (per the V1 Complete Plan's own note that "the Editor Architecture is not tightly coupled
to the ImGui widget tree"): panels register through stable IDs and drive their contents from
`EditorObjectAdapter`/`InspectorRegistry`-style lookups, so a future framework swap would replace
only the rendering/widget layer, not the data model this ADR does not touch.

## Alternatives considered

- **Qt (Widgets or Quick).** Strong native docking, theming, DPI, IME, and accessibility support
  out of the box. Rejected because it brings its own windowing and rendering stack that the
  Scene/Game viewports would have to be embedded into or bypassed for, duplicating work
  `Nexora::Window`/`Nexora::RHI` already do; its GPL/commercial dual license is also a worse fit
  for an MIT-licensed engine than a permissively-licensed, render-agnostic library.
- **wxWidgets / GTK.** Same fundamental issue as Qt (an owned windowing/rendering stack competing
  with `Nexora::Window`/`Nexora::RHI`) without Qt's stronger docking/accessibility story to offset
  it.
- **A custom retained-mode UI toolkit.** Already the documented choice for *runtime*, game-facing
  UI (`Custom Retained Mode Runtime UI; Dear ImGui only for Editor/Debug`, V1 Complete Plan §title
  "Custom Retained Mode Runtime UI"). Building a second, editor-grade one (docking, property
  grids, undo-aware widgets, accessibility) from scratch is strictly more work than adopting a
  library purpose-built for exactly this role, for no offsetting benefit.
- **A web-based shell (Chromium/CEF + a JS framework).** Excellent docking/theming/accessibility
  via the browser engine, but pulls in a Chromium-sized dependency and process-boundary complexity
  that is disproportionate for a lean, permissively-licensed open-source engine, and still needs a
  custom bridge to present `Nexora::RHI` viewport output inside a web view.

Dear ImGui is also already the de facto standard for exactly this role among comparable
custom-RHI/in-house engines, so the integration pattern (immediate-mode UI backend written against
one's own RHI abstraction, rather than the native graphics APIs ImGui ships reference backends
for) is well-trodden.

## Consequences

- **Docking / multi-viewport**: satisfied by ImGui's docking branch feature set; no additional
  third-party dependency beyond ImGui itself.
- **IME**: ImGui exposes `io.SetPlatformImeDataFn` / `ImGuiPlatformImeData` for platform IME
  positioning, and `Engine/Window`'s Win32/X11 backends already surface composed-text events
  (`WindowEventType::Text`, wired through `WM_IME_COMPOSITION` on Win32) that the ImGui backend
  layer can forward into `ImGuiIO::AddInputCharacter`. No new IME plumbing is required in
  `Engine/Window`; the ImGui backend only needs to consume what it already exposes.
- **Accessibility (screen readers)**: the one deliberate, known gap this decision accepts. Dear
  ImGui has no built-in accessibility tree; native toolkits like Qt would not have this gap. ED-M7
  explicitly requires a "keyboard and screen-reader audit" before production hardening is
  considered done, so this ADR does not close that item -- it only names the direction (likely a
  secondary accessibility tree built from the same stable panel/command registry ImGui panels
  already register through, in the spirit of projects like AccessKit that pair an immediate-mode
  UI with a parallel platform accessibility bridge) as work ED-M7 planning still has to scope in
  full. Marking ED-M7 accessibility as done on the strength of this ADR alone would misrepresent
  progress and is explicitly out of scope here.
- **License**: Dear ImGui is MIT-licensed, matching this repository's own MIT license; no
  dual-licensing or copyleft friction.
- **Per-backend integration work**: one ImGui renderer/input backend must be written per
  `Nexora::RHI` backend (Vulkan, DX12, Metal) and per `Nexora::Window` platform backend (Win32,
  X11, Cocoa) -- new implementation work this ADR authorizes but does not itself deliver.

## Status of ED-M0

This ADR satisfies the "UI-framework ADR" line item of ED-M0. The remaining ED-M0 scope --
graphical docking, theme, DPI, IME wiring, the accessibility direction above, and crash-recovery
UX -- stays open, tracked in `Editor_Roadmap.md` as before.
