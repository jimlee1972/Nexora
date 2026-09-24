# Editor Dear ImGui host contract

`NexoraEditorImGui` is an optional UI-host module. It owns the Dear ImGui context, translates
public `Nexora::Window` events, applies the Editor theme and DPI scale, creates the root dockspace,
and presents panels using the stable IDs owned by `NexoraEditorCore`.

## Ownership and lifetime

- `EditorImGuiHost` owns one ImGui context and destroys it with the host.
- Draw data and frame metrics are valid only for the frame in which `EndFrame` returns them.
- `ProductShell` and `SceneDocument` remain borrowed Editor Core models and outlive calls that
  present them.
- The host does not own a native window or swapchain. The application supplies events exposed by
  `RenderSurface::Events`; `Render` records the generated draw lists through the public RHI, and
  the application retains target ownership.

## Threading and errors

All methods are serialized and run on the Window owner thread. Invalid display dimensions and
delta times are clamped to safe values. The Window abstraction owns native IME candidate-window
positioning; unsupported hosts report that result explicitly. Target-host visual acceptance
remains a release-runner responsibility.

## Accessibility direction

The stable `ProductShell` panel and command IDs are the semantic source for a future secondary
accessibility tree. Widget labels use those stable IDs and never become the data-model identity.
Dear ImGui does not provide a native accessibility tree, so keyboard traversal and screen-reader
bridges remain ED-M7 work; plugins must not inspect the ImGui widget tree to supply semantics.
