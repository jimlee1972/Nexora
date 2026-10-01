# Editor Dear ImGui host contract

`NexoraEditorImGui` is an optional UI-host module. It owns the Dear ImGui context, translates
public `Nexora::Window` events, applies the Editor theme and DPI scale, creates the root dockspace,
and presents panels using the stable IDs owned by `NexoraEditorCore`. On the first frame it builds
the default workspace with Project and Hierarchy on the left, Console and Content along the bottom,
and an open center area for the upcoming Scene/Game views. Hierarchy and Content are selected
deterministically after their dock nodes settle, so adding a sibling tab cannot hide the primary
authoring views on first launch.

## Ownership and lifetime

- `EditorImGuiHost` owns one ImGui context and destroys it with the host.
- Draw data and frame metrics are valid only for the frame in which `EndFrame` returns them.
- `DrawProjectSelector` owns only editable UTF-8 fields, the displayed error, and at most one
  `ProjectSelectorRequest`. `TakeProjectSelectorRequest` transfers that request once. The host never
  creates directories, acquires a writer lease, indexes content, or replaces the active project;
  the application performs those steps and reports a failed activation back to the selector.
- `ProductShell` and `SceneDocument` remain borrowed Editor Core models and outlive calls that
  present them. The Hierarchy reads nodes from the supplied live document and writes a clicked
  node back through `SceneDocument::Select`; the application owns that document and its `World`.
- `ProjectContentSession` is also borrowed for each `DrawProductShell` call. The Content panel reads
  virtualized ranges from its UUID-keyed model, emits generation-tagged POD drag payloads, and routes
  rename/move/delete/undo/reimport back through the session. Reimport submits to the borrowed
  `AssetImportQueue`, shows bounded progress and structured diagnostics, offers cancellation, and
  polls authoring-thread publication once per frame. It never writes the filesystem itself.
  Breadcrumb and folder drop targets validate the payload, project generation, destination, and
  write permission before the session mutates anything. Dependency rows resolve IDs only while the
  panel is drawing.
- `ProjectWorkspace` and `RecentProjectStore` are borrowed for the frame. The Project panel exposes
  project name, stable UUID, canonical root, descriptor schema, read-write/read-only access,
  applied/required upgrade state, and the bounded recent-project list. It never acquires a lock,
  upgrades a descriptor, or writes recent state; the application completes those operations before
  drawing.
- The project selector displays background content-index progress and exposes a one-shot cancel
  request. The application owns the candidate workspace and import operation, consumes the staged
  `AssetWorkspace` on the window/authoring thread, and keeps the selector open after cancellation or
  failure.
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
  project `.nexora` directory. The legacy schema 0 payload is read and rewritten as schema 1 on the
  next save; malformed or unsupported payloads are rejected and replaced only by a normal save.
- Recovery is prompted once per discovered journal. Failed recover/discard operations keep the
  modal open and expose the data-layer error instead of silently dismissing it.

## Threading and errors

All methods are serialized and run on the Window owner thread. Project access/upgrade state is a
snapshot borrowed from Editor Core; read-only state disables project-owned writes in the data layer,
not merely in widgets. Content mutation errors remain on the session and are shown in the panel; the
UI does not optimistically update around a failed filesystem transaction. Invalid display
dimensions and delta times are clamped to safe values. The Window abstraction owns native IME
candidate-window positioning; unsupported hosts report that result explicitly. Target-host visual
acceptance remains a release-runner responsibility. Background import progress/cancellation and
external dirty-conflict dialogs remain deferred ED-M1 work. The Xvfb acceptance launches without a
project, drives create and read-only open through keyboard navigation, and verifies the resulting
descriptor and active access mode in a real process.

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
