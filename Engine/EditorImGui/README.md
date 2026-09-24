# Editor Dear ImGui host contract

`NexoraEditorImGui` is an optional UI-host module. It owns the Dear ImGui context, translates
public `Nexora::Window` events, applies the Editor theme and DPI scale, creates the root dockspace,
and presents panels using the stable IDs owned by `NexoraEditorCore`. On the first frame it builds
the default workspace with Hierarchy on the left, Console along the bottom, and an open center area
for the upcoming Scene/Game views.

## Ownership and lifetime

- `EditorImGuiHost` owns one ImGui context and destroys it with the host.
- Draw data and frame metrics are valid only for the frame in which `EndFrame` returns them.
- `ProductShell` and `SceneDocument` remain borrowed Editor Core models and outlive calls that
  present them. The Hierarchy reads nodes from the supplied live document and writes a clicked
  node back through `SceneDocument::Select`; the application owns that document and its `World`.
- The host does not own a native window or swapchain. The application supplies events exposed by
  `RenderSurface::Events`; the native `Render` overload rasterizes the generated draw lists and
  composites the resulting RGBA8 frame into the surface's currently acquired swapchain backbuffer.
  The application retains target ownership. The public-RHI overload is retained for deterministic
  headless draw-contract validation and is not the graphical application's presentation path.
- `Render` uploads each draw list to transient public-RHI vertex and index buffers, applies its
  framebuffer-scaled clip rectangles, binds the font texture slot, and preserves ImGui index and
  vertex offsets in indexed draws. The submitted command list owns the ordering; transient
  resources are released only after `Submit` returns.
- Recovery is prompted once per discovered journal. Failed recover/discard operations keep the
  modal open and expose the data-layer error instead of silently dismissing it. The modal accepts
  both its visible buttons and the `R` (recover) / `D` (discard) keys so display-backed workflow
  automation exercises the same UI path as a user.

## Threading and errors

All methods are serialized and run on the Window owner thread. Invalid display dimensions and
delta times are clamped to safe values. The Window abstraction owns native IME candidate-window
positioning; unsupported hosts report that result explicitly. Target-host visual acceptance
remains a release-runner responsibility.

Window backends normalize navigation, editing, punctuation, keypad, function, alphanumeric, and
left/right modifier keys before events reach the host. Each key event carries the complete
Control/Shift/Alt/Super snapshot, which the host publishes through Dear ImGui's modifier events.

## Accessibility direction

The stable `ProductShell` panel and command IDs are the semantic source for a future secondary
accessibility tree. Widget labels use those stable IDs and never become the data-model identity.
Dear ImGui does not provide a native accessibility tree, so keyboard traversal and screen-reader
bridges remain ED-M7 work; plugins must not inspect the ImGui widget tree to supply semantics.

## Display-backed recovery gate

When `Xvfb` and `xdotool` are available at configure time,
`editor.crash_recovery_workflow` launches the real graphical Editor on X11, kills it with a pending
journal, relaunches it, and waits until the recovery modal is visibly open. It then drives both the
recover and discard choices through native keyboard events and verifies the resulting files. This
is automated Linux display evidence; physical-display inspection remains part of ED-M0's broader
human acceptance gate.
