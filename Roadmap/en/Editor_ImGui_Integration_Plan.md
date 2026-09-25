# Editor ED-M0 Dear ImGui Integration Plan

> Version: v1.3 | Status: implementation in progress; target-host evidence pending |
> Updated: 2026-09-25 | Relates to: `Editor_Roadmap.md` (ED-M0),
> `ADR-0001-Editor-UI-Framework.md`, `Window_Presentation_Roadmap.md`

> **Repository audit (2026-09-25):** implementation is **in progress**. The checked foundations
> below are present in source and contract tests, but **none of WP0–WP8 has passed its exit gate**.
> Retained GPU resources, direct rendering to the borrowed presentation target, project-owned
> layout persistence, DPI font-atlas rebuilding, and recovery failure contracts are implemented.
> Automated X11 coverage now includes startup, resize, close, corrupt-layout replacement, legacy-layout
> migration, and crash/relaunch recovery for both recover and destructive-discard choices. Portable
> coverage also exercises stale texture generations, deferred font-atlas retirement, every DPI bucket,
> and a bounded 512-frame docking/layout soak. Physical-display and Windows target-host evidence
> remain open, so these foundations must not be interpreted as ED-M0 acceptance.

## 1. Goal, acceptance boundary, and current truth

ADR-0001 selected Dear ImGui with docking. This plan is the execution specification an AI agent
must follow to finish ED-M0 without creating a second window, presentation, or editor-data model.
ED-M0 is accepted only when the Editor opens through the public `RenderSurface`, renders a usable
GPU-backed docked shell, consumes real input, handles DPI and Windows IME, exposes recovery before
normal editing, and has reproducible automated and target-host evidence.

The repository contains a feature-gated `NexoraEditorImGui`, a pinned Dear ImGui docking dependency,
context ownership, input translation, stable-ID docking, a live Hierarchy, recovery modal,
DPI/theme policy, generation-checked textures, completion-tracked resource rings, and native
Vulkan/DX12/Metal draw recording through a backend-neutral presentation contract. These remain
foundations rather than final renderer acceptance because real-display Linux evidence and Windows
DPI/IME evidence are still absent. Therefore ED-M0 remains open.

### Verified implementation checklist

- ✅ The graphical shell is optional and isolated in `NexoraEditorImGui`; Editor Core has no
  Dear ImGui dependency.
- ✅ Dear ImGui is pinned to `v1.91.9b-docking`, docking is enabled, and unmanaged `imgui.ini`
  persistence is disabled.
- ✅ `NexoraEditor --graphical` creates one public `RenderSurface` and consumes its `WindowEvent`
  stream and live `FrameInfo` extent/DPI state.
- ✅ The host owns one `ImGuiContext`, presents stable-ID Hierarchy/Console panels, builds the
  initial dock layout, and round-trips Hierarchy selection through `SceneDocument`.
- ✅ The portable RHI draw-contract overload uploads vertices/indices, applies scaled scissors,
  preserves index/vertex offsets, and is exercised on the validation device.
- ✅ Key/modifier, pointer, wheel, focus, Unicode text, DPI, and IME candidate callbacks have
  implementation foundations.
- ✅ Recovery UI calls only `ProjectWorkspace` recover/discard operations, preserves failures,
  and exposes exactly-once result consumption.
- ✅ The production surface overload emits backend-neutral textured/indexed `UiDrawData`; Vulkan,
  DX12, and Metal implementations record native GPU draws without `CompositeRgba8`.
- ✅ Pipeline, sampler, generation-checked textures, bounded upload rings, and
  completion-protected retirement are implemented for the production surface and validation paths.
- ✅ Project-owned layout persistence, DPI font-atlas rebuilding, and recovery
  failure/exactly-once contract coverage exist.
- [ ] Physical-display Linux graphical validation and Windows DPI/IME target-host acceptance
  evidence are recorded and passing. Automated X11 rendering and kill/relaunch recovery are
  available in the feature-on Linux gate.

### Definition of "done"

All of the following must be true at the same commit:

1. `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF` preserves the headless Editor and does not fetch or
   link Dear ImGui.
2. With the feature ON, `NexoraEditor --graphical --project=<path>` uses exactly one
   `RenderSurface`; no native graphics API or OS window handle escapes into Editor Core.
3. The native path submits vertex/index data, projection constants, scissor rectangles, font and
   user textures, alpha blending, and resource transitions to the acquired backbuffer. The CPU
   compositor is removed from production use (it may remain only as an explicitly named test
   oracle if justified).
4. Resize, minimize, out-of-date/suboptimal surface status, DPI change, focus loss, and shutdown
   follow the `RenderSurface` recovery state machine without leaking or using in-flight resources.
5. Hierarchy selection round-trips through `SceneDocument`; recovery failures remain actionable;
   stable panel IDs, rather than visible labels or ImGui IDs, are persisted.
6. Linux automated gates pass, Linux/X11 real-display evidence covers render/input/resize/recovery,
   and Windows target-host evidence covers per-monitor DPI and IME composition/candidate position.
   Unsupported platforms are recorded as unverified, never inferred from compilation.

## 2. Fixed architecture decisions

These decisions are closed for ED-M0. An implementation may change one only by updating
ADR-0001 (or adding a superseding ADR), this document in both languages, and the affected contract
README before code is merged.

| Topic | Decision | Reason / consequence |
| --- | --- | --- |
| UI framework | Dear ImGui docking, pinned to `v1.91.9b-docking`. | Immediate-mode UI fits a custom RHI; do not track a moving branch. Record the exact tag and MIT license. |
| Acquisition | CMake `FetchContent`, reachable only when `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON`. | Keeps the dependency optional. Configure-off must work without network/cache. Do not add a submodule or system-package requirement in this milestone. |
| Module boundary | `NexoraEditorImGui` depends on public `Editor`, `Presentation`, `Window`, and `RHI`; Editor Core never depends on ImGui. | UI ownership stays outside the portable document/transaction model. Keep `Config/Modules/modules.json` authoritative. |
| Window/presentation | The application owns one `RenderSurface`; the UI host borrows it during a frame. | No GLFW/SDL, second swapchain, private native handles, or backend-specific window creation. |
| Renderer | One backend expressed against public RHI, not separate ImGui Vulkan/DX12/Metal copies. Platform-specific code belongs below RHI/Presentation. | Avoids three drifting renderers and makes validation-device tests possible. If public RHI cannot express a need, extend RHI generically and document its contract rather than downcasting. |
| Multi-viewport | Docking is enabled; OS-level ImGui multi-viewport is deferred. | Extra platform windows require a multi-surface ownership design. ED-M0 needs a docked main window, not detached native windows. Keep `ViewportsEnable` off. |
| Context | One `EditorImGuiHost` owns exactly one `ImGuiContext`; all calls occur serially on the window-owner thread. | Draw data is borrowed only until the next `BeginFrame` or context destruction. No background thread may call ImGui. |
| IDs/persistence | `ProductShell` panel/command IDs are semantic identity; `###stable.id` separates visible labels. | Renaming/localizing labels must not break layout or future accessibility semantics. Persist a versioned workspace layout through Editor Core, not an unmanaged global `imgui.ini`. |
| Input | `WindowEvent` is the only input source. Key events carry a full modifier snapshot; text events carry Unicode scalar values. | Do not poll Win32/X11/Cocoa directly. Text input is distinct from key shortcuts. |
| DPI/fonts | Coordinates are logical UI units; framebuffer scale and surface extent determine pixels. Rebuild font resources when the chosen DPI bucket changes. | `FontGlobalScale` alone is a temporary scaffold and is not sufficient for crisp final output. Style sizes must be derived from an immutable base to prevent cumulative scaling. |
| IME | Text arrives through `WindowEventType::Text`; candidate placement goes through `RenderSurface::SetImeCandidatePosition`. | No native window handle is exposed to ImGui. Windows target-host proof is mandatory. |
| Textures | `ImTextureID` maps to a generation-checked Editor UI texture registry; the font atlas occupies a reserved entry. Unknown/stale IDs use a diagnostic fallback and report an error. | Never reinterpret arbitrary pointers or raw RHI handles. This is required before ED-M1 thumbnails can reuse the backend. |
| Recovery | The data layer owns journal operations; the UI presents recover/discard before normal editing and keeps the modal open on failure. | UI code must not delete or rewrite journals itself. A choice is consumed once. |
| Accessibility | Stable panel/command IDs are the semantic seed; native accessibility remains ED-M7. | Dear ImGui has no accessibility tree. ED-M0 records the handoff and supplies keyboard-operable recovery/basic shell; it must not claim screen-reader completion. |
| Failure policy | Expected surface states skip/recreate a frame; invalid contracts return a typed status/error; programmer invariants assert in development. | No silent fallback to a private renderer, no exception across module boundaries, and no busy loop while minimized. |

## 3. Ownership, lifetime, and frame contract

The required ownership graph is:

```text
NexoraEditor process
  ProjectWorkspace / World / SceneDocument / ProductShell (owned by application)
  RenderSurface (owned and drained by application)
  EditorImGuiHost (owns ImGuiContext and renderer cache)
    font atlas + UI texture registry + per-frame upload allocations
    borrowed WindowEvent span, models, and acquired frame target
```

The frame order is normative:

1. `RenderSurface::BeginFrame()` pumps events and acquires or describes the next target.
2. Interpret `SurfaceStatus` with `RecoveryAction`. Abort exits; skip does not begin ImGui render
   submission; recreate/resize is completed before resources for the new frame are recorded.
3. Feed the borrowed event span exactly once. Apply focus, pointer, button, wheel, key/modifier,
   text, close, resize, and DPI events in queue order.
4. Read `FrameInfo()` after `BeginFrame`; call `SetDisplay` with current logical extent, DPI, and
   framebuffer scale. A zero extent means suspended/minimized: wait for events and do not allocate.
5. Call `BeginFrame(delta)`, build dockspace/panels/modals, then `EndFrame()` exactly once.
6. Convert `ImDrawData` to RHI commands: upload buffers, bind projection/pipeline/texture, clamp
   scissors after applying `DisplayPos` and `FramebufferScale`, execute callbacks according to the
   callback policy, and draw with `IdxOffset` plus `VtxOffset`.
7. Transition to presentation state, submit, then call `RenderSurface::EndFrame()` once.
8. Retire upload/font/descriptor resources only after the completion value that protects their
   last use. On exit, wait/drain, destroy the host, then destroy the surface.

Borrowed spans, `ImDrawData`, and acquired image handles must never be cached across frames.
`ProjectWorkspace`, `SceneDocument`, and `ProductShell` must outlive the `DrawProductShell` call.
Any change to these rules requires synchronized updates to `Engine/EditorImGui/README.md` and the
owning RHI/Presentation README.

## 4. Ordered construction plan

An AI agent must execute the work packages in order. Each package ends with its own focused test
and a review of `git diff`; do not batch all packages into one unreviewable change. Check boxes
report repository truth, not intent.

### WP0 — Baseline and reproducibility audit

**Status: partially complete.**

1. Read `CLAUDE.md`, the Editor, EditorImGui, RHI, Window, and Presentation READMEs, ADR-0001, and
   the two related roadmaps. Record any contract conflict before editing code.
2. Configure/build/test `linux-development`; also run `linux-shipping` for any RHI ABI, module
   dependency, exported header, or link-boundary change.
3. Configure once with the graphical feature OFF and once ON. Verify OFF does not populate
   `nexora_imgui`; verify ON resolves the pinned tag. Capture the resolved revision in validation
   notes. Do not commit `_deps`, build trees, or `CMakeUserPresets.json`.
4. Inventory current tests (`editor.imgui_contract`, presentation contracts, module graph) and map
   every later acceptance criterion to a test or target-host checklist entry.

**Exit gate:** clean baseline results and an explicit gap list. If configure needs network and no
cache is available, report an environment limitation; never silently disable the feature.

### WP1 — Make the public RHI sufficient for ImGui

**Status: implemented in source and validation contracts; native target-host validation remains.**

1. Compare `ImDrawVert`/`ImDrawIdx` and every `ImDrawCmd` field with public RHI capabilities.
   Required semantics are dynamic vertex/index upload, orthographic constants, alpha blending,
   no depth test/write, cull-none, scissor, sampled RGBA texture, and indexed base-vertex drawing.
2. Add only missing generic primitives to RHI. Do not add types named after ImGui. Define texture
   upload row pitch, sampler/filter/address mode, descriptor lifetime, shader visibility, and
   completion/fence ownership. Update `Engine/RHI/README.md` when these contracts change.
3. Implement and contract-test the generic primitive in the validation device first, then Vulkan.
   DX12/Metal implementations must compile on their target hosts before the shared API is treated
   as portable; leave their acceptance unchecked until actually run.
4. Add deterministic shader sources/reflection through the existing shader pipeline. Do not embed
   backend-only ad-hoc bytecode in `EditorImGui.cpp`.

**Exit gate:** validation-device test proves state transitions, bindings, scissor, indexed offsets,
and resource retirement; Vulkan validation reports no errors for an offscreen frame.

### WP2 — Implement the retained GPU renderer resources

**Status: implemented in source and validation contracts; native target-host validation remains.**

1. Introduce a renderer-owned state object beneath `EditorImGuiHost`: pipeline, sampler, font
   texture/view, descriptor bindings, and a bounded ring of per-frame vertex/index upload buffers.
   Create stable resources lazily after device/format is known; do not create/destroy pipeline and
   font texture for each frame.
2. Upload the real RGBA32 font atlas, assign its registry ID to `io.Fonts->TexID`, and rebuild it on
   context/font/DPI generation changes. Retire the old generation only after GPU completion.
3. Flatten or stream all draw lists without losing per-list base offsets. Compute the projection
   from `DisplayPos` and `DisplaySize`; support 16- and 32-bit `ImDrawIdx`.
4. For every command: honor reset-render-state callbacks, define a policy for application
   callbacks, transform/clamp the clip rectangle, skip empty/out-of-bounds clips, bind the
   generation-checked texture, and issue `DrawIndexed(ElemCount, ..., IdxOffset, VtxOffset)`.
5. Set premultiplied/non-premultiplied blending consistently with shader output and surface format;
   explicitly handle sRGB versus UNORM. Add pixel-readback golden cases for color, alpha, font UV,
   overlapping clips, non-zero display origin, and non-zero vertex offset.
6. Keep allocations bounded. Grow upload capacity geometrically when necessary, reuse it after a
   completion value, and expose metrics for vertices, indices, draw calls, reallocations, and
   rejected textures.

**Exit gate:** repeated offscreen frames produce stable pixels and allocation counts; resize and
font rebuild do not leak; sanitizer/validation runs find no stale-handle or out-of-bounds access.

### WP3 — Connect the renderer to the acquired presentation image

**Status: implemented in source for Vulkan, DX12, and Metal; target-host validation remains.**

1. Add the smallest public `RenderSurface` frame-target access needed by a renderer, preferably a
   callback/encoder or borrowed RHI target descriptor valid only between `BeginFrame` and
   `EndFrame`. Do not expose `VkImage`, `ID3D12Resource`, `MTLTexture`, or a permanent image handle.
2. Specify initial/final state, format, extent, generation, and invalidation on resize/recreate.
   Presentation remains the owner of acquisition, synchronization, and present.
3. Replace the production `Render(RenderSurface&)` CPU loop with shared WP2 command recording into
   that borrowed target. Keep at most one implementation of draw-list traversal.
4. Handle `OutOfDate`, `Suboptimal`, `Occluded`, `Suspended`, device loss, and close according to
   `RecoveryAction`. Do not spin while minimized and do not call `EndFrame` for a frame that was
   never acquired.
5. Update `Engine/Presentation/README.md`, `Engine/EditorImGui/README.md`, module dependencies, and
   exported API tests in the same change.

**Exit gate:** the graphical executable presents GPU-rendered ImGui through the public surface;
no per-frame full-screen CPU RGBA buffer or readback/upload round trip remains.

### WP4 — Input, docking, persistence, and command routing hardening

**Status: portable core and versioned project-owned layout round-trip are present; target-host proof
remains.**

1. Retain the full key mapping (navigation/editing, punctuation, keypad, F1-F12, alphanumeric, and
   left/right modifiers). Add table-driven tests for press/release and modifier snapshots.
2. Test pointer leave/focus loss so stuck buttons/keys cannot survive deactivation. Wheel values
   remain horizontal=`value0`, vertical=`value1`, normalized once and only once.
3. Keep the initial dock layout deterministic: Hierarchy left, Console bottom, center reserved.
   Build it only for a new workspace/layout schema; after that restore a versioned Editor-owned
   layout. Unknown/missing panel IDs are ignored with diagnostics, not crashes.
4. Route shortcuts through `ProductShell` command IDs before panel-specific behavior. Respect
   ImGui capture flags when deciding whether gameplay/scene tools receive pointer or keyboard
   input; do not use visible labels as command identity.
5. Keep `ViewportsEnable` disabled and add an assertion/test so an ImGui upgrade cannot silently
   create native platform windows.

**Exit gate:** automated synthetic-event tests pass, layout round-trips, and a real X11 session
proves typing, shortcuts, drag docking, wheel axes, focus loss, and close behavior.

### WP5 — DPI, fonts, and theme

**Status: live extent/DPI forwarding, bucketed font rebuild, and production GPU atlas upload exist;
Windows proof remains.**

1. Define a small DPI bucket policy (for example, nearest supported scale with hysteresis) and an
   immutable base style. Recompute style from base whenever the bucket changes; never repeatedly
   scale the already-scaled style.
2. Rebuild the atlas at bucket pixel density while keeping logical widget sizes stable. Atomically
   switch font texture generation at a frame boundary and defer old GPU resource destruction.
3. Use `FrameInfo` after acquisition as the source of truth. Test same-frame resize+DPI events,
   monitor moves, minimize/restore, fractional scale, and rapid changes.
4. Ship one first-class high-contrast-enough dark theme. Per-user theme editing is not ED-M0 scope.

**Exit gate:** screenshot/checklist evidence at 100%, 125%, 150%, and 200% on Windows shows crisp
text, correct hit targets, no cumulative scaling, and no one-frame stale extent.

### WP6 — IME and Unicode

**Status: event forwarding and candidate callback exist; target-host proof remains.**

1. Validate Unicode scalar handling, including supplementary-plane characters; reject invalid
   scalar values before `AddInputCharacter`. Key events must never duplicate text events.
2. Refresh the borrowed surface used by `Platform_SetImeDataFn` each active frame and clear it when
   the frame/surface ends so callbacks cannot dereference a destroyed surface.
3. Convert ImGui logical cursor coordinates through viewport origin and DPI to the client-pixel
   coordinate contract expected by `SetImeCandidatePosition`.
4. On Windows, test Microsoft Pinyin or another installed IME: start/update/commit composition,
   cancel composition, move the input cursor, switch DPI/monitor, and verify candidate placement.
   Unsupported X11 behavior must be reported honestly, not treated as Windows evidence.

**Exit gate:** Windows recording/screenshots plus a checklist demonstrate committed text exactly
once and correctly positioned candidates at more than one DPI.

### WP7 — Recovery UX and basic keyboard accessibility

**Status: modal/data-layer calls and automated X11 kill/relaunch coverage for both recover and
destructive discard exist; physical-display evidence remains.**

1. Detect a journal before normal editing becomes interactive. Recovery modal takes focus, traps
   keyboard navigation, and offers explicit Recover and Discard actions. Discard requires clear
   destructive wording; neither action runs merely because a default button receives focus.
2. Call only `ProjectWorkspace::RecoverWorkspace`/`DiscardRecovery`. While an operation runs,
   prevent duplicate submission. On failure retain the journal and modal, show the actionable
   error, and allow retry or safe exit.
3. Add data-driven tests for no journal, successful recover, successful discard, recover failure,
   discard failure, and exactly-once `TakeRecoveryChoice` semantics. Add a process-level test that
   kills after a journal is durable, relaunches, and verifies the choice.
4. Verify keyboard traversal and visible focus for the shell and modal. Record the exact semantic
   data still missing for ED-M7's secondary accessibility tree; do not inspect the ImGui widget
   tree from plugins.

**Exit gate:** automated failure paths pass and a real-display kill/relaunch session proves both
recover and discard without data loss outside the selected policy.

### WP8 — Target-host matrix, evidence, cleanup, and milestone update

**Status: automated Linux X11 smoke/recovery implemented; physical-display Linux and Windows
evidence remain.**

1. Run the full Linux gate listed in §6 with a clean tree. Because WP1/WP3 alter linkage/API
   boundaries, run `linux-shipping` too.
2. Under a real X11 display, run a scripted/manual checklist covering launch, visible font/text,
   Hierarchy selection, docking, all input classes, resize/minimize/restore, and recovery. Capture
   command, commit, backend/device, result, and artifact location.
3. On Windows, build Development and Shipping with the graphical feature, then execute the DPI and
   IME checklists. Run DX12 only if the surface selects DX12; do not claim it from Vulkan results.
4. macOS/Metal is a required supported-backend parity item only when Editor support is declared for
   macOS. Until a macOS target host runs it, list it as unverified rather than passed.
5. Remove the production CPU compositor and dead scaffold code, update contract READMEs, synchronize
   both roadmap languages, inspect the final diff, and only then update ED-M0 status. Panel
   existence alone is not acceptance.

**Exit gate:** every required evidence row has a link/result and no required row says "assumed".

## 5. Technical problem and solution register

| Problem | Required solution | Forbidden shortcut | Verification |
| --- | --- | --- | --- |
| RHI lacks a renderer operation | Add a backend-neutral RHI contract, validation implementation, then native implementations. | Include Vulkan/DX12/Metal headers in EditorImGui or downcast the device. | Validation trace plus native validation. |
| Swapchain image is Presentation-owned | Borrow a frame-scoped RHI target/encoder with generation and state contract. | Create another swapchain or expose permanent native image handles. | Resize/recreate stress and stale-generation rejection. |
| GPU/CPU lifetime mismatch | Completion-tracked ring buffers and deferred destruction. | Destroy upload/font resources immediately after `Submit` unless the contract guarantees completion. | Multi-frame stress with validation/sanitizers. |
| Font atlas currently placeholder-sized | Upload actual atlas pixels and retain a stable texture registration per generation. | Bind a 1x1 texture or recreate it every draw. | Font pixel golden and stable allocation metric. |
| Arbitrary `ImTextureID` | Generation-checked registry with fallback/error. | Cast pointers/raw handles. | Valid, stale, unknown, and destroyed texture tests. |
| Clip/offset bugs | Apply display origin and framebuffer scale, clamp, preserve both offsets and index width. | Assume origin zero or 16-bit indices. | Synthetic multi-list draw golden. |
| DPI blur/cumulative growth | Bucketed atlas rebuild; derive style from immutable base. | Rely only on `FontGlobalScale` or call `ScaleAllSizes` repeatedly. | Multi-DPI screenshot and numeric style tests. |
| Duplicate shortcuts/text | Keys route commands; text events alone add characters; respect capture/focus. | Derive text from virtual keys. | Unicode and shortcut collision tests. |
| IME use-after-free/wrong position | Frame-scoped surface binding and logical-to-client-pixel conversion. | Cache native handles or raw surface indefinitely. | Destroy/recreate and multi-DPI IME tests. |
| Corrupt recovery journal | Preserve journal, surface error, allow retry/discard under data-layer policy. | Auto-delete, auto-recover, or hide error. | Injected I/O/corruption tests. |
| Network unavailable during configure | OFF build remains independent; ON build fails clearly or uses an approved pre-populated cache. | Fetch an unpinned branch or silently compile a stub. | Clean configure OFF/ON. |
| Scope creep | Limit panels to shell/Hierarchy/Console/recovery; defer Content Browser, viewport gizmos, PIE, and profiler. | Mark later Editor milestones complete because a dock window exists. | Roadmap review. |
| Font atlas never built at the default 1.0 DPI bucket (`dpi_bucket` defaulted to `1.0F`, equal to `DpiBucket(1.0F)`, so the first `SetDisplay()` call was a no-op and `NewFrame()` later hit ImGui's `IsBuilt()` assertion) | Initialize `dpi_bucket` to a sentinel (`0.0F`) below every real bucket so the first `SetDisplay()` call always builds the atlas. | Special-case the first call, or build in the constructor without a real DPI scale. | `editor.imgui_contract` (fixed 2026-09-25); was previously masked because the test's first two `SetDisplay()` calls happened to cross a bucket boundary. |
| `~EditorImGuiHost()` never cleared `io.BackendPlatformUserData` before `DestroyContext`, tripping ImGui's `Shutdown()` "forgot to shutdown platform backend" assertion | Clear `BackendPlatformUserData` (after `Activate`) immediately before `DestroyContext`. | Suppress the assertion via a build define instead of clearing backend state. | `editor.imgui_contract`; unreached before the fonts/shortcut fixes below because the test aborted earlier. |
| `ImGui::Shortcut(..., ImGuiInputFlags_RouteGlobal)` never fired on the single frame it was first registered (Dear ImGui's routing arbitration is one frame deferred: `SetShortcutRouting()` writes `RoutingNext`, and `RoutingCurr` only adopts it at the *next* frame's `NewFrame()`), and a single `ProcessEvents()` batch mixing pointer/text/key events needed several frames to fully apply under `ConfigInputTrickleEventQueue` (Dear ImGui applies at most one input-type transition per frame by design) | Draw one warm-up frame with no key event to prime shortcut routing before simulating a keypress; disable `ConfigInputTrickleEventQueue` in tests that replay a synthetic event batch as a single deterministic unit rather than live per-tick input. | Switch the shortcut to `ImGuiInputFlags_RouteAlways` to dodge routing arbitration (masks real multi-window shortcut conflicts), or leave trickling on and hope a mixed-type batch drains in one frame. | `editor.imgui_contract` Ctrl+S routing assertion (fixed 2026-09-25). |

## 6. Validation commands and evidence schema

Run focused tests while implementing, then the complete gate before delivery:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development

cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

Also configure an explicit feature-off build if the preset enables the graphical shell, and run the
focused tests by name where available:

```bash
ctest --preset linux-development -R 'editor.imgui_contract|window_presentation.contracts|build.module_graph' --output-on-failure
```

Do not claim Windows/macOS validation from Linux. Each manual evidence record must contain:

```text
commit: <sha>
host/os: <exact version>
window backend / RHI backend / GPU / driver: <values>
configuration and command: <values>
scenario: <acceptance checklist id>
result: pass | fail | blocked
artifacts: <log/screenshot/video path>
notes: <validation messages or limitation>
```

Required automated coverage includes feature OFF/ON configure, module graph, context lifetime,
event/key tables, dock/layout IDs, recovery outcomes, draw-list conversion, texture generations,
clip/offset/index-width goldens, resize/recreate, resource retirement, and repeated frames with
bounded allocation. Required human evidence includes legible output, actual interaction, docking,
focus, DPI, IME, and crash recovery; screenshots alone cannot prove input or lifetime behavior.

## 7. AI execution and change discipline

- Work on one WP and one architectural boundary at a time. Before editing, state the invariant,
  files, expected test, and rollback point in the change notes.
- Search for the existing public abstraction before adding one. Never infer an API from a roadmap;
  verify headers and tests. Do not edit generated build output or fetched dependency sources.
- Keep patches reviewable: RHI contract, backend implementation, Editor integration, and evidence
  updates should be separate conventional commits unless atomic compilation requires otherwise.
- Format only touched C++ files with the repository `.clang-format`. Keep English and Traditional
  Chinese roadmaps synchronized in the same commit.
- When ownership, lifetime, threading, error, or deferred-work behavior changes, update the owning
  README in the same patch. When dependencies change, update `Config/Modules/modules.json` and its
  tests.
- A failed gate is not completion. Record the exact failure and preserve the last buildable commit;
  revert the current WP rather than adding a private bypass.
- Do not check a status box based on source inspection alone. "Implemented" needs an automated
  result; "accepted" additionally needs the target-host evidence named above.

## 8. Explicit non-goals and follow-up handoff

ED-M0 does not deliver Content Browser/thumbnails, Scene View, gizmos, Game View/PIE, inspector
widgets, specialized tools, build/profile UI, detached OS-level ImGui viewports, user-authored
themes, or a screen-reader accessibility tree. WP2's texture registry is only the backend contract
those future panels can use; it does not authorize implementing them early.

The ED-M7 handoff must list stable semantic IDs, labels, roles/actions/value gaps, focus order,
keyboard-only failures, live-region needs, and candidate platform bridge options. That document may
recommend a secondary accessibility library, but dependency adoption requires its own decision and
approval. Until then the honest claim is "basic keyboard-operable ED-M0 shell; screen-reader
support not implemented."
