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
  `RenderSurface::Events`; the native `Render` overload flattens ImGui draw lists into the public
  backend-neutral `UiDrawData` contract. `RenderSurface` records those indexed draws directly into
  its acquired native GPU image. The application retains target ownership. The public-RHI overload
  remains the deterministic headless draw-contract path.
- The public-RHI `Render` path lazily retains one pipeline, an atlas-sized validation font-texture
  allocation, and geometrically grown vertex/index upload buffers. Repeated frames reuse those
  resources.
  `ReleaseRenderer` waits for the device before destroying them and must run before that device is
  destroyed. The path applies framebuffer-scaled clip rectangles and preserves ImGui index and
  vertex offsets. The native `RenderSurface` path owns an equivalent completion-protected cache.
- DPI is quantized to 100%, 125%, 150%, or 200%. Crossing a bucket rebuilds the font atlas at that
  pixel density, publishes the framebuffer scale, and derives the theme anew rather than
  cumulatively scaling an existing style.
- Dear ImGui's global ini file remains disabled. `SaveLayout` and `LoadLayout` provide an explicit
  in-memory round trip. `ProjectWorkspace` stores that payload with an explicit schema under the
  project `.nexora` directory; malformed or unsupported payloads are rejected.
- Recovery is prompted once per discovered journal. Failed recover/discard operations keep the
  modal open and expose the data-layer error instead of silently dismissing it.

## Threading and errors

All methods are serialized and run on the Window owner thread. Invalid display dimensions and
delta times are clamped to safe values. The Window abstraction owns native IME candidate-window
positioning; unsupported hosts report that result explicitly. Target-host visual acceptance
remains a release-runner responsibility.

Window backends normalize navigation, editing, punctuation, keypad, function, alphanumeric, and
left/right modifier keys before events reach the host. Each key event carries the complete
Control/Shift/Alt/Super snapshot, which the host publishes through Dear ImGui's modifier events.
Dear ImGui keyboard navigation is enabled so the recovery modal and shell remain operable without
a pointer; OS-level multi-viewport creation stays disabled.

## Accessibility direction

The stable `ProductShell` panel and command IDs are the semantic source for a future secondary
accessibility tree. Widget labels use those stable IDs and never become the data-model identity.
Dear ImGui does not provide a native accessibility tree, so keyboard traversal and screen-reader
bridges remain ED-M7 work; plugins must not inspect the ImGui widget tree to supply semantics.
