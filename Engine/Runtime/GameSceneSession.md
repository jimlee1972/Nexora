# Packaged game scene session

`GameSceneSession` is an owner-thread C++ service over a borrowed Play `GameWorld` and an
owning immutable `LoadedGameProject`. The host supplies the configured startup UUID. This
service does not change Editor document selection, read authoring paths or replace the legacy
V3 logical `load_scene` callback. It requires the optional Asset Pipeline feature.

## Ownership and frame boundary

Keep the session, World, registered services and their contexts alive through module Stop and
Destroy. Queue requests during gameplay callbacks; call `World::EndFrame`, then `Process`
outside every module callback. No callback-time request mutates the live World. `Stop` cancels
pending work and rejects new requests; status/info queries remain available during teardown.
Destroy the module before destroying its borrowed session. All service calls are owner-thread
only. `View`, `Stats` and render capture have that same C++ caller precondition.

One request may be pending. A second request reports Busy without changing its output. Request
IDs are monotonic and never reused in the session. Scene/entity IDs come from the live World's
monotonic allocator; authored IDs and parents are remapped even when source scenes overlap.
Candidate IDs are private until publication. Failed preparation can discard those unpublished IDs.
Status retains the current request and the last completed request while another is queued;
older overwritten request IDs report NotFound. Transition state and result are separate values.

## Transaction and presentation

Processing copies a candidate World, prunes retired scene records, creates/remaps the requested
scene and computes the primary scene/camera. All allocation, catalog validation and presentation
preparation happen before publication. A missing scene, capacity error, exception, rejected
presentation or Stop during preparation preserves the live World, camera, render owners and
audio/physics/character bindings. Diagnostics are bounded UTF-8; C callbacks do not propagate
exceptions. Failed API calls preserve output arguments.

An optional host `Prepare` receives a borrowed candidate view and returns an owning
`GameScenePresentation`. It must not modify the live World or expose candidate IDs, and must
retain native resources privately. Its allocation-free `Commit() noexcept` publishes after
World publication. The native host must drain outstanding GPU work before retiring prior native
owners; session CPU tests with presentation markers do not prove that native requirement.
An empty Prepare is permitted for CPU hosts. The session never calls native graphics APIs.

Successful publication removes the retired entities' audio, physics and character bindings,
then moves the candidate World and swaps instance ownership without allocation. Persistent
scenes and their live modifications survive Replace. Unloaded scene records do not accumulate.
The live budget is 256 scene records and 100,000 entities, including retained logical scenes.
Templates and assets remain shared CPU owners; frames can retain those owners after a transition.

## Replace, Additive and unload

- Replace removes nonpersistent scenes, adds the requested authored scene and makes it primary.
- Additive retains scenes and the existing primary/camera. Loading an already instantiated asset
  additively is invalid. Replace may create another instance of an asset retained persistently.
- Unload accepts only an active session-owned scene. Persistent scenes and the last session-owned
  scene cannot be unloaded. If the primary is unloaded, the first retained instance becomes primary.
- The primary's first authored camera is active; zero means the host default camera. Additional
  scenes do not steal the camera. Stale/external/logical scene handles report NotFound.
- External unloading of a session-owned scene is a lifecycle violation detected before processing.
  The host must route authored scene ownership through this service.

`CaptureGameSceneRenderItems` returns owning resource pointers and current hierarchy transforms
for remapped authored mesh entities. Runtime destruction or renderer removal omits the item.
Replacing its resource binding is unsupported and rejects frame preparation. This helper does
not introduce a general renderer for newly spawned logical mesh entities or unsupported behavior.

## Optional C and Zig service

`get_scene_api` is an appended services V1 suffix. Old complete services prefixes retain their
sizes and bytes; an unregistered or short scene table reports Unsupported. The frozen V3 ABI
and StaticView format stay unchanged. See `nexora/gameplay_scene.h`, the scene extension baseline
and `Bindings/Zig/scene.zig` for exact wire fields and errors.

The Zig SDK exposes `try context.scenes()`, then `loadPath("Title.scene", .replace, false)`
or `load(id, .additive, false)`. Paths resolve only exact UTF-8 catalog locators; no filesystem
search occurs. Save the returned request and poll `status(request)` until Applied, Failed or
Cancelled. `active`, `info` and `unload` use session instance handles. Modules must handle an
absent scene service and must not assume a request has applied inside the requesting callback.

Portable coverage includes candidate rejection/exception/Stop conservation, hierarchy remapping,
shared CPU lifetime, persistent retention, binding cleanup, 120 alternating transitions, historical
services prefixes, C/Zig field layout and an actual SDK-built Zig module requesting Level01 →
Title → Level01. ✅ The scoped native pixel/lifetime, Editor Play/build and relocated game/Editor
gates are accepted by the [delivery plan](../../Roadmap/en/Project_Scene_Delivery_Plan.md).
See the [final record](../../Tools/Build/evidence/ProjectSceneDelivery-Acceptance-2026-10-26.md)
for counters, source/exit conservation and the remaining independent clean-host/full-milestone boundaries.
