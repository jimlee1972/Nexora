# Editor Dear ImGui host contract

`NexoraEditorImGui` is an optional UI-host module. It owns the Dear ImGui context, translates
public `Nexora::Window` events, applies the Editor theme and DPI scale, creates the root dockspace,
and presents panels using the stable IDs owned by `NexoraEditorCore`.

## Ownership and lifetime

- `EditorImGuiHost` owns one ImGui context and destroys it with the host.
- Draw data and frame metrics are valid only for the frame in which `EndFrame` returns them.
- `ProductShell` and `SceneDocument` remain borrowed Editor Core models and outlive calls that
  present them.
- The host does not own a native window or swapchain; the application supplies Window events and
  submits the resulting draw data through the public Presentation/RHI path.

## Threading and errors

All methods are serialized and run on the Window owner thread. Invalid display dimensions and
delta times are clamped to safe values. Native renderer submission, IME candidate-window
positioning, and target-host visual acceptance remain platform adapter responsibilities.
