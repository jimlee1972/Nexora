# V1-M4 through V1-M12 runtime contracts

## Shader artifact admission and replacement

`ShaderArtifactSlot` accepts an RHI `ShaderModuleArtifact` after backend-format and canonical
reflection/layout-hash validation. Its mode comes from the Runtime build configuration, so
Development may stage dynamic compiler output and Shipping accepts cooked artifacts only. The
versioned `NXSHDR` container is loaded with `LoadCookedShaderArtifact` and validates its checksum,
payload bounds, and reflection table before staging; an unsupported version or trailing bytes after
the binding table are rejected with a specific error message. Staging never changes the active artifact;
`Commit(retire_fence, ...)` publishes
the validated candidate and increments its generation, while a failed validation leaves the active
generation untouched. An optional backend callback creates the native module before publication;
creation failure rolls back the transaction and discards the staged candidate, so a retried
`Commit` fails until a new artifact is staged. Calls are serialized on the owning thread, and returned
artifact pointers are borrowed until the next successful commit or slot destruction.
`CollectRetired(completed_fence)` destroys replaced artifacts and native modules only after the
owning GPU fence has completed, keeping hot-reload replacement safe for in-flight command buffers.
Destroying the slot itself destroys the active and all retired native modules immediately without
consulting fences; the owner must ensure the device is idle (or every retire fence has completed)
before the slot is destroyed.

`NexoraRuntime` is the dependency-ordered, platform-neutral baseline for the remaining V1
milestones. It deliberately contains no SDK-specific physics, media, mobile, or editor backend.
Instead, it makes the ownership and safety boundaries executable before those integrations land.

| Milestone | Executable baseline |
| --- | --- |
| M4 | Additive scene lifecycle, stable entity IDs, deferred safe unload, double-precision transforms |
| M5 | Hash-validated asset generations, dependency-cycle rejection, pinning and rollback |
| M6 | Reflection metadata, a real dynamic plugin loader with a stable C ABI gate and service registration, a scene editor built on Create/Modify/Undo, and prefab override/rebase (see below for the full vertical slice; `ExtensionRegistry`/`UndoStack` remain the lighter M6 row exercised by `runtime.v1_m4_m11_contracts`) |
| M7 | Device-neutral input routing, stable touch IDs, virtual-list materialization and locale fallback |
| M8 | CPU-authoritative batched physics queries; motor/controller separation; tiled navigation desired velocity; typed blackboard, compact behavior runtime, and budgeted perception |
| M9 | Skeleton/clip blending and root motion, skinning palettes, audio buses with voice-safe residency, particle SoA, and non-blocking timestamped video presentation |
| M10 | Separate cell/bundle identity, observable RAM/VRAM use, HLOD state and occupied-cell pins |
| M11 | App lifecycle, pressure policy and native WebView pointer ownership |
| M12 | Profile-driven plugin, shader, optional-asset and headless presentation stripping |

The contract tests exercise every row, including localization
fallback, unreachable navigation, animation looping, audio voice limits, media back-pressure and
seek invalidation. Platform SDK adapters and production authoring tools remain future work and
must preserve these interfaces rather than bypassing their lifecycle checks.

## V1-M12 shipping, packaging, and hardening

`Shipping.h` is the platform-neutral delivery contract. `Packager` creates an owning,
deterministically ordered manifest and rejects empty identities/digests, unsafe relative paths, and
output collisions before publishing any result. Plugin and shader allowlists are independent;
Minimal removes optional content, Dedicated removes all shader and presentation artifacts, and SDK
content is opt-in. This makes every strip decision observable without copying files or invoking a
platform signing tool from the runtime.

`BundleUpdater` stages only newer, digest-bearing generations, retains the last installed
generation across activation/restart, and provides explicit confirm or rollback transitions.
`CrashReporter` copies required build/platform/reason metadata and retains a bounded tail of
breadcrumbs. `SoakMonitor` consumes caller-supplied monotonic frame samples and reports peak and
end-to-end growth without owning profiler memory. `DeviceMatrix` records unique startup evidence
for Windows, macOS, Android, and iOS; recording is a testable evidence contract, not a claim that a
device was run by this Linux build.

All objects are synchronous, caller-owned, and perform no background work or filesystem/network
I/O. Returned values own their storage. Configure with `-DNEXORA_ENABLE_SHIPPING=OFF` to omit the
implementation. Shipping builds require compiler IPO/LTO support and use
`NEXORA_SHIPPING_PROFILE=Minimal|Full|Dedicated`; Minimal and Dedicated strip the editor SDK at
configure time, while Dedicated also strips scene rendering and all presentation implementations.
The `runtime.v1_m12_shipping` gate covers a complete package/update/crash flow, invalid and unsafe
inputs, every strip axis, rollback/restart, a four-platform evidence matrix, leak-growth detection,
and 10,000-artifact / 10,000-sample performance baselines. Actual signed installers, store update
transports, native crash dump upload, physical-device startup, and multi-hour sanitizer/device soak
runs remain release-infrastructure gates and are not claimed by this portable foundation.

## V1-M11 mobile platform and native WebView runtime

`Platform.h` is the SDK-neutral contract for app lifecycle, permissions, safe-area insets,
orientation, thermal and memory pressure, haptics, clipboard, deep links, IME composition, native
sharing, platform login, and named Android GPU workarounds. `platform::Runtime` owns copied event
state and synchronously invokes optional platform-service callbacks on the calling thread. Invalid
safe areas, URIs, IME cursors, unavailable services, and terminal lifecycle transitions fail
without partially changing state. Callbacks must not outlive the runtime and are not thread-safe.

`NativeWebViewHost` exclusively owns opaque native view handles created by an SDK adapter and
destroys every live handle on explicit removal or host destruction. Engine-visible IDs never
expose native pointers. Navigation and pointer delivery validate the view before entering the
adapter. `IsNativeOverlay()` explicitly identifies WebViews as compositor-owned native overlays;
they are therefore not render items and never enter the RenderGraph UI pass. Counters expose
creation, destruction, routing, and rejected-operation activity for diagnostics.

Configure with `-DNEXORA_ENABLE_PLATFORM=OFF` to omit the M11 implementation and run its
feature-strip gate. The enabled test covers the complete callback/event vertical slice, lifecycle
background/resume, failure paths, deterministic handle destruction, native pointer routing, and a
10,000-event performance baseline. Android WebView, WebView2, and WKWebView adapters plus physical
iOS/Android device execution remain platform-SDK gates; this portable Linux gate does not claim
those device runs.

## V1-M10 large-world runtime

The V2-M4 portable layer provides deterministic adaptive XZ hierarchy splitting, stable integer-coordinate cell identities and hashes, hierarchical cell groups, fixed-grid/explicit 3D volume policies, quantized double-precision world-origin rebasing, and revision-ordered persistent cell deltas. Builds sort identities before hashing, so unordered input cannot affect output. `HlodV2` keeps full content visible until a selected merged-mesh or impostor artifact reports ready, preventing transition holes; `ImpostorBuilder` produces an order-independent artifact identity. `PersistentDeltaStore::Materialize` overlays deltas on scene defaults without retaining a runtime-memory snapshot. These synchronous, caller-owned contracts perform no filesystem I/O; gameplay keeps absolute identities/coordinates while only render-relative coordinates consume the rebase origin.

`LargeWorld.h` defines stable fixed-grid addressing, spatial lookup, streaming demand, room/portal
prefetch, offline HLOD, terrain patches, and instanced vegetation. `StreamingManager` keeps cell,
full-bundle, and HLOD-bundle identities separate; ranks source demand deterministically; applies
unload hysteresis; and exposes RAM/VRAM use and rejected transitions. Occupied cells remain fully
resident for collision continuity even when a source leaves or the budget is temporarily too small.
`SetOccupiedFootprint` atomically replaces each character's intersected adaptive/3D cell set, so both
sides of a boundary remain pinned until the character footprint leaves them.
Far HLOD has independent memory cost and residency and can therefore outlive its full cell.

Terrain uses cullable clipmap patches and vegetation uses species-owned instance arrays, so neither
creates `World` entities. All objects are synchronous and caller-owned; returned views remain valid
only until their owner is mutated. Configure with `-DNEXORA_ENABLE_LARGE_WORLD=OFF` to strip the
implementation. The enabled test covers every M10 gate, failure paths, portal prefetch, LOD/culling,
and a 10,000-item spatial performance baseline.

## V1-M9 presentation runtime

`Presentation.h` is the backend-neutral boundary for Animation, Audio, VFX, and Video. Animation
validates an acyclic parent-before-child skeleton, samples translation tracks, blends graph state
transitions, extracts root motion, and publishes a validated matrix palette for a renderer's GPU
vertex-skinning or baked-animation-texture backend. The graph and palette are synchronous and
caller-owned; production clip compression and GPU upload remain backend responsibilities.

V2-M8's optional [`NexoraPoseSearch`](../PoseSearch/README.md) supplies a translation-only feature
extractor and deterministic weighted pose database/search through a separate Foundation-only
module. Runtime does not link it implicitly. A caller may use its owning clip/time result to select
animation state; the database owns no graph, character motor, transform, or gameplay event authority.

`AnimationGraph::Synchronize(time)` seeks the active looping clip within `[0, duration)` and
cancels any crossfade. Invalid/nonfinite/out-of-range times or non-looping clips return false
without mutation. `Update(0)` then returns the synchronized pose with zero root motion; a later
positive update extracts only displacement from the new clock. Seeking does not produce gameplay
events or character movement. An explicit consumer may use the Foundation-only
[`Animation SyncGroup`](../Animation/README.md) for marker-aligned sample times; Runtime does not
acquire an implicit Animation dependency. Synchronize/Play/Update remain caller-serialized. This
is a visual clock operation, not a motor/controller movement request.

`AudioEngine` enforces a hard voice limit, routes events through named buses, and acquires a
reference in `ResidencyTracker` for every active voice. Stopping one of several voices cannot
unload their shared clip. The `streaming` event bit is preserved for a MiniAudio/platform adapter;
this layer deliberately performs no device I/O.

`ParticleSystem` uses separate position, velocity, age, and lifetime arrays with bounded capacity.
Sprite, mesh, and trail renderer kinds share this CPU simulation contract; GPU simulation and draw
expansion can consume the same spawn data without changing gameplay ownership. `PositionSnapshot()`
returns an owning copy of current live positions in simulation order, so render extraction does not
retain private SoA pointers. Snapshot, Spawn and Update remain serialized on the simulation thread.

Decoded video producers submit texture identities to a bounded `VideoPlayer` queue. `Tick()` only
examines already decoded frames, drops superseded frames against the audio clock, and publishes a
`VideoTexture()` identity usable by UI or materials, so gameplay never waits for decode. Seeking
flushes queued frames and rejects stale timestamps; subtitle selection uses the same clock. A
platform hardware decoder owns its worker and texture allocation outside this synchronous queue,
and video audio is expected to enter `AudioEngine`, making that audio clock the A/V sync authority.

Configure with `-DNEXORA_ENABLE_PRESENTATION=OFF` to omit the complete M9 implementation. The
disabled configuration exposes `NEXORA_PRESENTATION_ENABLED=0` and runs only the feature-strip
gate, supporting dedicated/headless builds without Animation, Audio, VFX, or Media code.

## V1-M8 gameplay simulation

`GameplaySimulation.h` is the public, backend-neutral boundary for Physics → Character → Navigation → AI. `PhysicsWorld` provides authoritative immediate and batch queries without exposing Jolt types. The standard motor owns desired locomotion, gravity, root motion, and external velocity; `CharacterController` owns collision resolution, ground snap, stepping, crouch clearance, and teleport semantics. Every result reports requested and actual motion separately. The portable motor position is a feet origin; ground snap sweeps downward from above the previous/candidate feet to the contact plane, avoiding the zero-distance inside-AABB ray that previously let gravity penetrate the floor. Centered rendering capsules add half their current height to this origin. Objects are synchronous and caller-owned; none are thread-safe.

`Move()`/`Teleport()` also take a caller-supplied `ground_ready`/`destination_ready` readiness flag and report `CharacterGroundState::StreamingPending` when it is false: locomotion, gravity accrual, and ground snap/step evaluation are all suspended and the character holds its current position instead of free-falling through geometry that has not streamed in, per the V1-M10 large-world streaming contract. This header stays independent of `LargeWorld.h` by design (either can be stripped without the other), so the readiness flag is the full extent of the contract here; a caller that wants the M10 `StreamingManager` to drive it is expected to pin the character's cells with `SetOccupied()` and query `Status(cell)->residency == Residency::Full` itself — declaring the character a high-priority streaming source and any automatic bridging between the two systems is gameplay/application-layer wiring this foundation does not provide.

`NavigationWorld` owns streamed tiles and invalidates paths by generation when a tile unloads. It only returns a desired velocity and never receives a `World` or writable `Transform`. The AI foundation uses fixed typed blackboard slots, a compact shared behavior program with per-tick deterministic traces, and a stimulus query with an explicit work/result budget. Configure with `-DNEXORA_ENABLE_GAMEPLAY_SIMULATION=OFF` to strip this implementation and run the feature-strip gate. The enabled test validates batched physics queries, ground/wall resolution, teleport, streaming-pending hold/resume, cross-tile navigation and stale-path invalidation, blackboard typing, behavior execution, and perception budgets.

V2-M7's optional `NexoraAIIntegration::IntentAdapter` is a higher-level module that maps AI desired
XZ direction and speed into Runtime's requested horizontal motion. It rejects non-finite direction
components, invalid speeds, and any projection whose components or planar magnitude overflow, and maps `Disabled` to a zero-motion hold. Runtime retains motor
clamping and character ownership; the adapter runs synchronously and owns no state. It requires both
`NEXORA_ENABLE_AI_RUNTIME_BRIDGE` and `NEXORA_ENABLE_GAMEPLAY_SIMULATION`, and is not linked into
`NexoraRuntime` itself.

## V1-M7 input, UI, and localization runtime

`InputUi.h` is the device-neutral input and UI boundary; unlike M8-M12 it has no feature-strip
switch and always compiles into `NexoraRuntime`. `InputSystem` lets keyboard, mouse, gamepad, and
touch devices contribute to one user in the same frame, keyed by `(device kind, device id)`
ownership, and rejects a duplicate `sequence` so a platform backend can safely redeliver an event
without double-counting it; `PointerId` stays stable across an individual touch's down/move/up
events. `ActionMap` evaluates named actions from bound raw controls without engine code depending
on concrete device layouts.

`UIDocument` composes a parent/anchor `RectTransform` tree and dispatches pointer input through
explicit capture → target → bubble phases; `CapturePointer` gives one element exclusive routing
for a pointer until released, and `UIRouter` orders multiple documents by priority. `VirtualizedListModel`
keeps only the visible window plus overscan realized regardless of total item count.
`LocalizationTable::Resolve` checks the active locale first, then falls back to a configurable
fallback locale (`SetFallbackLocale`, default `"en"`) before returning the raw key, and
`RefreshLocalization` only re-resolves element text when its generation counter has advanced, so a
disabled document can defer localization work until it is shown again.

`TextEditBuffer` validates UTF-8 on every mutation and indexes IME composition ranges by
**code-point**, not grapheme-cluster, boundaries — a deliberate scope limit, not an oversight;
composing/committing/undoing text never exposes a byte offset that splits a multi-byte code point.
The enabled test (`runtime.v1_m7_input_ui_localization`) covers simultaneous multi-device input
and duplicate-sequence rejection, capture/target/bubble dispatch and pointer capture exclusivity,
list virtualization at scale, locale switching and fallback-locale resolution, disabled-document
localization refresh, logical-resolution independence, UTF-8 IME composition/undo and invalid-UTF-8
rejection, and a 10,000-event input routing performance baseline.

## V1-M4 scene vertical slice

`World` owns scenes and their entities. Entity identifiers remain stable across scene
serialization, and snapshots use the versioned `NEXORA_SCENE` text schema. Loading validates the
complete snapshot before publishing it; malformed versions, duplicate IDs, invalid transforms,
and IDs already owned by the destination world are rejected without partially adding a scene.
An entity count larger than the snapshot text could possibly hold (an entity record needs at least 25
characters in version 1, 39 in version 2, and 41 in version 3) is rejected before any allocation, and the up-front
reservation is capped at a small constant, so a hostile snapshot cannot make the loader throw or
reserve memory proportional to a claimed count.
Double-precision world transforms provide the large-coordinate foundation.

`runtime::Transform` is one component holding a position, a rotation stored as a unit quaternion
(`qx, qy, qz, qw`, identity by default), and a per-axis scale (`sx, sy, sz`, one by default), following
the Unity and Unreal convention of a single transform with possibly non-uniform scale. Euler angles
are an Editor presentation and are not stored here. Negative scale mirrors an axis; a zero scale, a
non-finite value, or a zero-length quaternion is invalid (`IsValidTransform`). Every transform that
reaches a `World` is validated and its quaternion normalized (`NormalizedTransform`): a
`WorldCommandBuffer` containing an invalid transform is rejected whole before anything changes,
`GameWorld::SpawnEntity` throws `std::invalid_argument`, and a snapshot with an invalid transform is
rejected. Snapshots are written as `NEXORA_SCENE 3`, whose entity record is the ID, the parent ID,
the position, the rotation, and the scale, followed by the components. `NEXORA_SCENE 2` (no parent)
and `NEXORA_SCENE 1` (position only) remain readable: their entities load as roots, version 1 with
identity rotation and unit scale, and either is upgraded to version 3 the next time it is saved. The
text is written and read in the classic locale. Writers that own only the position (the Zig/C
`write_component` bridge and a character controller's per-tick synchronization) use `WithPosition` and
therefore keep the entity's rotation and scale.

Scenes enter `LoadedInactive`, may transition to `Active`, and unload through `Unloading` before
their entity storage is released by `EndFrame`. Persistent scenes reject unload requests. An editor
world can be copied into an isolated play world without changing stable IDs or active scene state.

`SystemScheduler` executes named systems only after their declared dependencies. Systems enqueue
structural writes in a `WorldCommandBuffer`; validation and application occur after all systems,
so iteration never invalidates entity storage. The scheduler is synchronous and belongs to its
calling thread. `World`, returned entity references, and command buffers are not thread-safe;
entity references remain valid only until that scene's entity vector is structurally changed.

`RenderSceneFrame` performs read-only extraction across all active additive scenes. A frame is
rejected unless it finds a camera, a light, and at least one mesh with a material/shader; otherwise
it submits Shadow, Forward+ light-culling/draw, PostProcess, and Present RenderGraph passes.
`NEXORA_ENABLE_SCENE_RENDERING=OFF` compiles the
same data/scene lifecycle and serialization contracts while stripping presentation submission.
The `runtime.v1_m4_vertical_slice` test covers the complete data-to-render path, malformed input,
dependency cycles, safe unload, deterministic save/load, and a 10,000-entity time/size baseline.

### Entity hierarchy

Entities form a hierarchy following Unity's conventions ([plan](../../Roadmap/en/Entity_Parenting_Plan.md)).
`Entity::parent` is 0 for a root, and `Entity::transform` is **local**: relative to the parent, or
the world pose for a root. A parent and its child are always in the same scene and the hierarchy is
acyclic. Siblings (the children of one parent, or the roots of a scene) are ordered by their scene storage
order, which snapshots keep. `SiblingIndex` reads an entity's position and
`WorldCommandBuffer::SetSiblingIndex` moves it, clamping past the end, like Unity's
Get/SetSiblingIndex. A reparent makes the entity its new parent's last child. Both move the entity
within the scene storage, a structural change that invalidates entity references like creation and
destruction do. A child may still be stored before its parent; only the order among siblings is
meaningful.

- Reads: `World::Parent` (nullopt for a missing entity), `Children` (direct children in storage
  order), `Subtree` (the entity first, every parent before its children), `WorldTransform`, and
  `WorldMatrix`. `WorldTransform` takes its position from the exact matrix origin, including deep
  rotated, mirrored, and nonuniform ancestry. Its composed rotation and component-wise scale
  remain a TRS approximation when the affine linear part cannot be represented by those values;
  it is not a matrix decomposition or a nearest-TRS fit. `WorldMatrix` (column-major 4x4, `ToMatrix`
  per level) is always exact and is what rendering consumes (see "Render sync" below).
  `ComposeTransforms` and `RelativeTransform` are the public TRS building blocks.
- Writes: `WorldCommandBuffer::SetParent(entity, parent, keep_world = true)`, with parent 0 to
  detach. `keep_world` is Unity's `worldPositionStays`: the local transform is recomputed so the
  entity does not move; with `false` the local values are kept and the entity moves with its new
  parent. Self-parenting, a parent that is the entity's own descendant, a missing parent, and a parent
  in another scene are rejected. Under shear, keep-world reparenting uses each ancestor's local
  inverse to preserve the exact origin; rotation/scale retain the existing TRS approximation.
  `runtime.entity_parenting` checks closed-form three-level mirrored origins and save/load.
- Batches are validated against the hierarchy as the earlier commands of the same batch leave it, so
  "detach, then destroy the old parent" is valid while "destroy the parent, then move the child" is
  rejected whole, with nothing applied. A keep-world reparent whose re-expressed local transform is
  not representable (two valid but extreme poses can overflow) also rejects the whole batch: such a
  batch is rehearsed on a scratch copy of the world first and applied only if the rehearsal succeeds,
  so a rejected batch never touches the world's storage and borrowed entity references stay valid.
- `DestroyEntity` destroys the entity and every descendant, as in Unity and Unreal.
  `WorldCommandBuffer::LastDestroyed` lists every entity removed by the last successful `Apply`, so
  owners of per-entity state (such as `GameWorld`'s physics, audio, and character bindings) can release
  it for cascaded descendants too.
- Loading a snapshot rejects a parent outside the snapshot, a self-parent, and any cycle, without
  partially adding the scene. Hierarchy validation, `Subtree`, and cascading destruction are linear in
  the scene size, so one long chain cannot make loading or destroying quadratic.
- `Entity::parent`, like `Entity::transform`, is a plain field: writing it directly through
  `World::CreateEntity`'s reference bypasses all of the validation above and is reserved for code
  that maintains the invariants itself. Traversal stays bounded even on a hierarchy corrupted that
  way: `Subtree` tracks visited entities, `WorldTransform`/`WorldMatrix` walks are step-limited and
  then report failure, and validating a `SetParent` rejects the batch when the new parent's ancestor
  chain is dangling or cyclic.

A character controller may sit under a parent, as Unity's CharacterController on a child does. It
works in world space, using the exact `WorldMatrix` translation, since the world TRS is only
approximate under a sheared hierarchy.

- `SetCharacter` starts the controller at the entity's world position.
- Each `TickCharacter` starts from the transform's current world position, so a moved parent (a
  moving platform) carries the character and keeps its ground contact. A keep-world reparent does
  too.
- When something else changed both the local and the world position, the tick treats it as a
  teleport, with `CharacterController::Teleport` semantics: no ground and the velocity reset.
- The new world position is mapped back through the inverse of the parent's matrix, so only the
  local position changes.
- The tick runs on a copy of the controller state and commits only after the transform is stored;
  a move that cannot be stored changes neither.

### Render sync

`RenderSceneSync` (`RenderSync.h`) is the hierarchy's only path into the renderer: it mirrors the
mesh renderers of a `World`'s active scenes into a `renderer::GPUScene`, one GPU object per entity.

- Each object's transform is the entity's exact `WorldMatrix`, converted by `ToRenderMatrix` to the
  renderer's (row, column) `math::Matrix4`, so shear under non-uniformly scaled parents survives.
  `TransformBounds` bounds what the GPU actually draws: it applies the uploaded float matrix,
  scales the mesh's local sphere by that matrix's spectral norm (its largest stretch), and pads the
  radius by the worst-case float evaluation error of the shader's sums, so the bounds stay
  conservative under shear, coefficient narrowing, and large translations that cancel a far-off
  mesh center. That error bound holds only without overflow, so when an intermediate float sum
  could overflow (even with a finite result) the radius is infinite and the sync rejects the
  object.
- The caller's `RenderResourceResolver` maps a `MeshComponent` to resource indices and local bounds;
  returning nullopt (an asset that is not resident) keeps the entity out of the GPU scene.
- `Sync` creates objects for new mesh renderers, writes only the transform, bounds, or resources that
  changed, and destroys the objects of entities that were destroyed (including cascaded
  descendants), lost their mesh renderer, left an active scene, or became unrepresentable (a pose
  that overflows float, or a parent cycle written directly into `Entity::parent`). Visibility and
  LOD belong to other systems and are never overwritten after creation. An object destroyed behind
  the sync's back is mirrored again.
- World matrices are memoized per call (bit-identical to `WorldMatrix`). A walk up a parent chain
  fails as soon as it revisits an entity (a cycle written directly into `Entity::parent`), and
  entities on a failed walk are remembered as such, so a sync is linear in the entity count even on
  deep chains, one large corrupted cycle, or many small ones. Destruction runs in entity-id order, keeping GPU slot reuse
  deterministic.
- Ownership and threading: the sync owns only the objects it created and hands `retire_fence` to
  `GPUScene::Destroy`. GPU handles carry no scene identity, so while a sync owns objects it is bound
  to the `GPUScene::InstanceId` of the scene holding them: `Sync`, `RenderFrame`, and `Release`
  refuse any other scene (nullopt or `false`, with nothing touched), including a scene rebuilt at
  the same address or the same scene after `Clear()`. Moving the bound scene keeps the binding.
  `Release` destroys every object and unbinds; `Abandon` forgets them without touching any scene,
  for when the bound scene was destroyed or cleared. The sync cannot be copied or move-assigned (either would
  duplicate or silently drop ownership); move construction hands the objects over. Destroying a
  sync that still owns objects leaves them in the scene, so release it first. The sync, the
  `World`, and the `GPUScene` are externally synchronized on one thread, like the `GPUScene` itself.
- `CameraView` builds a camera entity's view the way Unity's Camera does: the position comes from
  its exact world matrix and the orientation from its world rotation (the product of the chain's
  rotations), so a camera under a moving or turning parent follows it while a non-uniformly or
  negatively scaled parent neither skews nor flips the view. Nexora is right-handed, so the camera
  looks down its local -Z. The projection uses `CameraComponent`'s vertical field of view and clip
  planes with `[0, 1]` depth, validated as the floats the renderer uses (a field of view that rounds
  to 180 or a near plane that rounds to 0 is rejected). The far plane culls through the frustum, so
  the view sets no radial distance limit that would cut off the frustum's far corners. Invalid
  camera data or a bad aspect ratio produce no view.
- `RenderSceneSync::RenderFrame` always syncs, then renders the GPU scene through the world's first
  camera: `BuildGPUDrivenCommands` culls by frustum and distance, and only the kept objects reach
  `ExecuteSceneFrame`. It needs a camera and a light, like `RenderSceneFrame`; a frame in which
  everything is culled still clears its targets. Upload extraction and `GPUScene::CommitFrame` stay
  with the caller that owns the GPU buffers. No application uses it yet; the Showcase keeps
  `RenderSceneFrame`, whose evidence counts every mesh renderer.

Limits: `PlaySession` apply-back copies transforms only and reports a conflict for an entity whose
parent changed during play; physics does not consume entity transforms yet (rendering does, through
`RenderSceneSync`).

Gameplay modules reach the hierarchy through component wires in `nexora/nexora.h`, read and written
with `read_component`/`write_component`:
- `"Nexora.Transform"`: three doubles holding the local position. A write keeps rotation and scale.
- `"Nexora.TransformV2"` (`NexoraTransformV2`): the full local transform. A write is validated and
  normalized; an invalid one is rejected and leaves the entity unchanged.
- `"Nexora.WorldTransform"`: the same layout for the world pose. It is read only and lossy under
  shear, like `WorldTransform`.
- `"Nexora.Parent"` (`NexoraParent`): the parent id, 0 for a root. A write reparents through
  `GameWorld::SetParent`, with all of its rejections. `keep_local == 0` keeps the world pose
  (Unity's default); non-zero keeps the local values.

`ReadGameplayComponent`/`WriteGameplayComponent` in `GameplayHostBridge.h` implement every wire
once. The V2 bridge and the V3 Showcase host both call them, so the two cannot drift. The V3 host
used to reset rotation and scale on a position write, a defect the shared path removes.
The same functions also accept a borrowed `runtime::World`, so an embedding Editor can serve
an isolated PlaySession clone without constructing a second GameWorld or duplicating wire decoding.
Calls are serialized on the World's owner thread, reads copy into caller-owned buffers, and writes
use the same atomic WorldCommandBuffer validation as GameWorld. No Entity pointer escapes or
survives a write. Stop/module teardown must end callbacks before the clone is destroyed. The V2
host table and stable C wire layouts are unchanged; a V3 module adapter remains embedding-owned.


## Zig gameplay bridge

`GameplayModuleHost::SupportsFixedUpdate` reports the loaded module's advertised optional fixed
callback without invoking it, under the same mutex as load/unload. Embedders can tick a module
that only has per-frame Update without interpreting an absent fixed callback as a runtime failure.

`GameplayModuleHost` executes the versioned `NexoraGameModuleV3` C ABI while the V1 and V2 layouts
remain declared for source compatibility. V3 separates state creation/destruction from
`on_start`/`on_stop`, makes update failures observable through `NexoraGameplayResult`, adds an
optional fixed-update callback guarded by a capability bit, and exposes host/module capability
masks. It validates the host and
module structure sizes, ABI version, and required callbacks before initialization. Update, reload,
and unload operations are serialized; a replacement module is initialized before the active module
is shut down, and a rejected replacement leaves the active module running.

The host table exposes size-checked component reads/writes, logging, and an explicitly paired host
allocator. Every allocation and matching deallocation carries the same `NexoraAllocationOwner`;
gameplay state uses `NEXORA_ALLOCATION_OWNER_GAMEPLAY_STATE`, while migration scratch storage is
reserved for `NEXORA_ALLOCATION_OWNER_STATE_MIGRATION`. The allocating host owns the allocator and
the module owns the returned block until it returns that exact pointer, size, alignment, and tag.
A module may retain the table only from successful `create` until `destroy`; the table and module
state are invalid immediately after `destroy`. Lifecycle and update calls are serialized but run
on whichever caller thread entered `GameplayModuleHost`; callbacks must not re-enter the same host
or retain borrowed component buffers. The Zig module allocates its state through the host and
returns the exact allocation during `destroy`, so allocation ownership never crosses the C ABI
implicitly and reload candidates have independent state. No exception crosses the C ABI; every
fallible callback reports a `NexoraGameplayResult`, and an update failure does not implicitly
unload the active module. Event delivery remains a future additive capability.
Modules may additionally provide state save/load callbacks. Reload serializes the active state,
creates and restores the candidate before calling its `on_start`, and only then retires the active
module. This ordering lets `on_start` reuse restored scene and entity handles instead of spawning
duplicate content. Load, descriptor, start, and migration failures destroy the candidate while
keeping the active module alive; `on_stop` runs only for a module that started successfully.
The path overloads discover platform-named modules and own each loaded library as one monotonically
numbered generation. Before retiring a generation, the host invokes the configured quiescence
barrier while lifecycle serialization is held; the embedding must wait there for every job and
deferred callback that can enter that generation. Only after the barrier, `on_stop`, and `destroy`
does the host release the old `LoadLibrary`/`dlopen` handle. Unload follows the same ordering, so
shutdown racing a reload is serialized and cannot release callable code. Static function-pointer
loads remain supported for monolithic/Shipping consumers, but a dynamically owned generation may
only be replaced by another dynamically owned generation. `Generation()` and `GetReloadStats()`
expose the active generation, successful reload count, migrated bytes, and wall-clock reload
duration for diagnostics and profiler integration. Dynamic path reloads first require consecutive stable file-size and write-time samples, so a linker copy cannot be opened halfway through publication. `GetFailureState()` records the callback kind, ABI result, and generation for the latest variable/fixed-update failure; failures remain observable without implicitly unloading state and a successful transactional reload clears the prior generation's failure.

`Tests/Gameplay/GameplayConformanceVectors.h` is the single lifecycle/update vector set used by the
C++ fake and Zig consumer. The Runtime negative suite rejects a missing loader symbol, ABI and
structure-size mismatches, absent required callbacks, and callback failures. Linux sanitizer gates
are directly runnable with `linux-sanitizers` (AddressSanitizer plus UndefinedBehaviorSanitizer)
and `linux-thread-sanitizer`; TSan is separate because it cannot be combined with ASan.

Configure with `-DNEXORA_ENABLE_ZIG_GAMEPLAY=ON` to compile the minimal Zig GameModule and run the
`gameplay.zig_abi_smoke` test. Zig 0.14.0 is the pinned CI toolchain. This is the first executable
toolchain gate. The Runtime dynamic-generation suite uses real native libraries and covers
discovery, repeated reload, failed restore rollback, job quiescence, and shutdown during a barrier.
The Zig Showcase executable is wired to a Development shared-library artifact; mobile
cross-compilation, and device execution remain required follow-up gates.

Desktop CI builds the same module on Linux, Windows, and macOS. A separate CI smoke matrix also
runs `zig build-obj` for `aarch64-linux-android` and `aarch64-ios`; these checks validate object
generation only and do not claim Android NDK or iOS SDK linking, packaging, or runtime execution.

## V1-M5 asset, cooker, bundle, and residency pipeline

`AssetPipeline.h` is the public, platform-neutral content contract. Authoring identities are
non-zero 128-bit UUIDs, references remain UUID-based after a source file moves, and importers are
selected by normalized source extension. Importers publish a complete `CanonicalAsset` or fail
without modifying the derived-data cache or any active runtime generation.

The cooker keys local derived data by UUID, type, platform, settings, dependencies, and canonical
payload. It emits the versioned `NXAB` runtime blob; deserialization validates the schema, bounds,
and payload content hash before publishing data. Runtime generations consume only these blobs and
bundle metadata—there is no source-path or source-format loading entry point in the runtime store.
The in-memory DDC is intentionally a replaceable local-cache baseline; cache loss never changes
runtime correctness.

`BundleBuilder` lays validated runtime blobs into a contiguous bundle and records UUID, offset,
size, and hash in its manifest. Verification rejects corrupt bytes, overlapping/non-contiguous
entries, duplicate UUIDs, and unsupported blobs. Bundle dependency validation requires every named
dependency to exist, rejects cycles before staging, and can return the complete closed cycle path.
Staging builds an isolated candidate generation and only publishes it on `ActivateStaged()`, so a
failed import, cook, verification, or dependency gate leaves the prior good generation active.
Rollback swaps the active and previous verified generations.

Generation pins are reference-counted. A pin keeps a generation available across activation;
asset residency is separately reference-counted per UUID and generation so old and new bytes can
coexist safely. `ResidentBytes()` exposes the current streaming-memory baseline. Collection never
reclaims the active, previous, staged, pinned, or resident generation. The store is synchronous
and owned by its calling thread; returned blob pointers remain valid until their residency is
released and the generation becomes collectible. No pipeline object is thread-safe.

`DataTable` provides the M5 typed-table foundation: immutable rows after successful build, unique
string primary keys, constant-time indexed lookup, and atomic rejection that retains the previous
good table. Rich schema types and JSON authoring adapters can be layered over this runtime
container without introducing a JSON DOM into gameplay hot loops.

Configure with `-DNEXORA_ENABLE_ASSET_PIPELINE=OFF` to omit the importer, cooker, DDC, bundle,
generation, and DataTable implementation from `NexoraRuntime`. The enabled test covers the full
source-to-import-to-cook-to-bundle-to-runtime path, corrupt data and dependency-cycle failures,
DDC reuse, rollback, generation pinning, residency accounting, DataTable atomicity, and a 10,000
asset performance baseline. The disabled configuration runs a dedicated feature-strip test.

`BundleMount.h` (also gated by `NEXORA_ENABLE_ASSET_PIPELINE`) is the API-M3 bundle-backend
deliverable: `MountBundle(vfs, name, bundle)` verifies the bundle (`BundleBuilder::Verify`), then
writes each asset's raw serialized bytes into a fresh `core::VirtualFileSystem` memory mount at
`<name>://<uuid>.blob`. A caller reads an asset back with the VFS's own `Read`, then
`AssetCooker::Deserialize` -- there is no bundle-specific read API, since the whole point is that a
bundle becomes ordinary, browsable VFS content. It refuses (and leaves no partial mount behind) a
bundle that fails verification, an already-mounted name, or a write failure partway through.

## V1-M6 reflection, plugin host, scene editor, and prefab foundation

`EditorSdk.h` is the public, platform-neutral contract for the tooling half of V1-M6.
`ReflectionRegistry` stores hashed-name `TypeDescriptor`/`FieldDescriptor` metadata and rejects a
duplicate type name or ID; it is deliberately independent of any specific reflected type, rather
than hand-annotating `World`'s existing structs.

`PluginHost` is a real cross-platform dynamic loader (`dlopen`/`dlsym` on Linux and macOS,
`LoadLibrary`/`GetProcAddress` on Windows), not an in-process descriptor comparison. The required
plugin contract is the single exported C symbol `NexoraPluginAbiVersion()` (see
`Plugins/Example/ExamplePlugin.cpp`, which only includes the public `Nexora/Foundation/BuildInfo.h`
and `Nexora/Foundation/PluginAbi.h` headers). `Load()` resolves that symbol and closes the library
immediately, before resolving or calling anything else, if the symbol is missing or its reported
ABI does not match the host's -- an ABI mismatch never reaches a second engine call, including the
registration step below. Adding a new plugin requires only a manifest and this one exported symbol:
no Engine source changes and no private Engine header.

`ServiceRegistry` is a name-keyed lookup for typed services that plugins and engine subsystems
publish to each other, and `PluginHost` actually connects a loaded plugin to one: a plugin that
additionally exports the optional `NexoraPluginRegister` symbol (also declared in `PluginAbi.h`, as
a plain C function-pointer signature -- the plugin side of the ABI never needs a `Nexora::Runtime`
C++ type such as `ServiceRegistry` itself) gets it called once, after the ABI check passes, with a
callback the plugin uses to publish services into the `ServiceRegistry` passed to `Load()`.
`Plugins/Example/ExamplePlugin.cpp` exports this and registers a real string service, so
`runtime.v1_m6_editor_sdk` proves the full loop -- ABI gate, dynamic load, and registration --
against a built artifact, not a mock. Registration is optional: `Load()` without a `ServiceRegistry`
argument (or a plugin that omits `NexoraPluginRegister`) only proves ABI compatibility, which is a
legitimate plugin on its own. This `ServiceRegistry`/`PluginHost` pair is the real, dynamically-loaded
extension mechanism; the pre-existing in-process `ExtensionRegistry` (`Runtime.h`, from the earlier
M4-M11 contract sweep, exercised by `runtime.v1_m4_m11_contracts`) is a lighter descriptor/ABI-number
bookkeeping structure that predates this milestone and does not itself load anything.

`SceneEditor` composes `World`, `WorldCommandBuffer`, and `UndoStack` (from the M4 vertical slice)
into Create/Modify/Undo operations. `SetTransforms` validates a non-empty unique-ID batch, applies
every transform or none, and records the entire multi-selection edit as one undo operation. Undoing a destroyed entity restores both its component data and
stable ID through the editor's privileged access to `World`; older transform and create undo cards
therefore continue to target the same entity. Destroy cascades to descendants, and undoing it restores
the whole subtree with its parents and local transforms (only when none of those IDs exists again);
if the subtree root's outside parent no longer exists by then, the root is restored as a root at the
world pose it had.
The same history now replays Redo for create, transform, Camera/Light/MeshRenderer, hierarchy edits, and subtree
delete. A new operation discards the undone branch; failed callbacks keep the history cursor in
place. Creation records its parent in the transaction so Redo restores the stable ID and hierarchy.
`CreateMeshEntity` creates one root with a validated/normalized transform and nonzero mesh reference.
Initialization uses one World command buffer before recording creation, so Undo removes the whole
entity and Redo restores the same stable ID, pose, component presence and full-width material ID.
Invalid poses, missing/unloading/unloaded scenes and zero mesh references reject before allocation
or history changes. `CreateEntity` shares the initializer and keeps identity-local child semantics.
Existing mesh component edits still allow zero/unresolved IDs as authoring data; this nonzero check
is specific to initialized mesh creation. Neither method resolves assets or manages GPU residency.
`SetCamera` validates finite field of view and clipping planes, then records the previous component
presence and values for Undo. Invalid edits leave the World untouched.
`SetLight` applies the same undo ownership to finite, nonnegative light intensity.
`SetMeshRenderer` delegates to `SetMeshRenderers`, which requires nonempty equal-sized spans
and unique live entity IDs. The batch validates all targets, then records component presence and
owning copies of mesh/material IDs as one Undo operation. Replay builds fresh World commands from
immutable owning values. Missing/duplicate entities reject without mutation or history. Zero and
currently unavailable resource IDs remain valid authoring data; asset resolution and GPU residency
are separate concerns.
Removal, replacement, and replay use the same public World command boundary and stable entity IDs.
`SetParent` is undoable and restores the previous parent and the exact previous local transform.
Destroy also validates that the entity belongs to the supplied scene before mutating the world. `CreateEntity` returns the new entity's stable `Id`, not a
reference into `World`'s storage: unlike `World::CreateEntity` (consumed immediately, within this
file, per the rule above), `SceneEditor` is the public data-model layer external callers such as a
future editor UI are meant to drive, so it cannot assume a caller consumes the reference before some
other call reallocates the owning scene's entity storage underneath it.

`Prefab` is a tree of named nodes with string properties, giving nested prefab composition for
free. `PrefabInstance` resolves a property by checking its own per-instance overrides before
falling back to the prefab tree, so an override on a missing path is rejected up front.
`Rebase()` retargets an instance at a new template and drops any override whose path the new
template no longer has, rather than leaving it silently dangling. `PrefabVariant` instead bakes a
fixed override set into a new, fully materialized `Prefab` tree that can itself be nested or
instanced downstream. `ProjectSettings` is a typed key/value store with an explicit-fallback
getter.

Configure with `-DNEXORA_ENABLE_EDITOR_SDK=OFF` to omit this file from `NexoraRuntime`; the disabled
configuration runs a dedicated feature-strip test. The enabled test additionally loads
`NexoraExamplePlugin`'s real built shared library through `PluginHost` when
`NEXORA_FEATURE_EXAMPLE_PLUGIN` is on, proving both the ABI gate and service registration against an
artifact built from nothing but the public plugin contract, not a mock.

**Scope note:** Runtime implements the data and command contracts, without graphical dependencies.
The optional `Apps/Editor` and `Engine/EditorImGui` front end drives these contracts for its
Hierarchy, Scene/Game views, Inspector, Content Browser, gizmos, and Profiler. Graphical milestone
acceptance is tracked separately in the Editor roadmap; the Runtime tests validate portable
`SceneEditor`, `PrefabInstance`, and Play contracts.

`PlaySession` is the portable PIE ownership contract. It owns an isolated `WorldKind::Play` clone;
`Tick` runs only while playing, `Step` runs exactly one fixed update while paused, and input focus
starts released until explicitly granted by editor policy. Stopping discards runtime mutations by
default. The only supported apply-back policy copies changed transforms for stable entity IDs;
runtime-created entities and all other component mutations remain isolated and are discarded.
Transform apply-back is a deterministic stable-ID diff against the source transform snapshot. It
considers only entities that play changed: a changed transform, or a parent different from the one
at play start. If such an entity's Editor transform changed concurrently, or it was reparented in
either world (even with unchanged local values, since those would mean a different pose under another
parent), the entire apply is rejected without
partial mutation and the conflict remains visible through `LastApplyBackStatus`. The Play World and update callback are released
before `Stop` returns, including after conflicts and contained update failures.

`RuntimeConsole` is a bounded, mutex-protected multi-producer ingress for owning structured records
(sequence, severity, category, timestamp, source, and message). Old records are evicted in sequence
order and the cumulative dropped count is observable. `PlaySession::Inspect` similarly returns an
owning, stable-ID-sorted entity/component snapshot with both local and world poses rather than
pointers into relocatable World storage. It includes copied parent/scene state and optional
Camera/Light/Mesh payloads; these remain valid after component mutation/removal and Stop.
`ReportRuntimeFailure` lets embedding per-frame callbacks use the same failure policy as fixed ticks:
pause the owned clone, release input, record `RuntimeFailure`, and increment the crash count once per
reported failure. It rejects reports without an active clone and does not perform World rollback.
Failed fixed updates pause the session, revoke input, and expose `RuntimeFailure`, allowing the editor
to inspect, resume, or stop the still-owned Play World. User, step-complete, debugger-break, and failure
pause reasons are distinct. Native IDE/debugger integration stays behind the caller-owned
`DebuggerAdapter`; attach/detach and polling occur synchronously on the caller thread, and only copied
location/diagnostic state crosses the boundary.

`PrefabInstance` exposes its override diff as a read-only span. Individual entries or the full diff
can be reverted, while `ApplyOverrides` creates a new immutable prefab revision and clears the
instance diff. Rebase keeps only overrides whose stable node paths survive in the new revision;
these operations never mutate a shared source prefab in place.

## API-M5/M6 Game facade and gameplay host bridge

`Nexora/Game/GameWorld.h` is the API-M5 boundary over `World`: `World::FindEntity`/`FindScene`
return a pointer into `World`'s own `std::vector<Entity>`/`std::vector<Scene>` storage, which
`CreateEntity`/`LoadScene` can reallocate at any time, so nothing outside this file is meant to
call them directly. `GameWorld` wraps the same operations behind by-value `EntitySnapshot`/
`EntitySpawnDescriptor` and plain `nexora::runtime::Id` -- never a pointer -- satisfying the
roadmap's explicit rule that a Zig module must never receive a movable C++ ECS storage pointer.
`Id` itself is not re-wrapped in a generational handle: it is already a monotonically increasing,
never-reused identifier, which the V1 Complete Plan's ABI rules list as its own accepted
C-ABI-crossing category ("EntityID"), separate from an index+generation Opaque Handle.

`SpawnEntity` attaches camera/light/mesh-renderer at creation time via `EntitySpawnDescriptor`
(they are plain fields on `Entity`). Component setters attach, update, or remove those components;
`DeferredCommands` exposes atomic mutation batches and rejects a full batch if an entity is stale.
`SetParent`, `GetParent`, and `GetWorldTransform` expose the entity hierarchy; `EntitySnapshot::transform`
is the local transform.
The immediate setters and `DestroyEntity` use the same `WorldCommandBuffer` internally, reusing
the M4 command-buffer contract rather than adding new `World` friend access.
`Query(scene, mask)` is an OR-mask batch query over a scene's entities.
`CaptureInput` wraps `InputSystem::Consume` into a by-value `InputSnapshot`, and `AssetRef` is a
named re-export of the already-ABI-appropriate `AssetUuid` (API-M2). The facade owns its portable
`PhysicsWorld` and `AudioMixer`, binds bodies and voices to the owning entity ID, removes those
bindings on entity destruction (including descendants destroyed by the cascade), resolves ray hits back to entity IDs, and synchronizes an attached
`CharacterController`'s position to the entity Transform after each motor tick (its rotation and
scale are left untouched). Audio resource IDs
are unique within a `GameWorld`, making entity stop/destruction deterministic with `AudioMixer`'s
resource-based stop contract. Physics and character methods are omitted when the optional gameplay
simulation feature is stripped; the rest of the API-M5 facade remains available. All facade calls
are synchronous, caller-thread-only, and retain no caller-owned spans or references.

`Nexora/Game/GameplayHostBridge.h` is the API-M6 piece: it wires the `NexoraGameplayHostV2` C ABI
(`Nexora/Foundation/GameplayABI.h`, the "Zig gameplay bridge" contract above) to a real `GameWorld`
instead of the `read_component`/`write_component`/`log`/`subscribe_event`/`set_tick_enabled` slots
only ever being filled by a test-scoped stand-in (`Gameplay/Zig/ZigGameplayTests.cpp`'s `HostState`
is exactly that: a fake host with its own private value, unrelated to any real `World`). `MakeHost`
builds a real `NexoraGameplayHostV2` whose `read_component`/`write_component` actually read and
write a live entity's Transform, camera, light, and mesh-renderer state (the Transform wire is a
position only -- the local position under a parent -- so a write changes the position and keeps the
entity's rotation and scale). Stable component IDs are
derived from their `Nexora.*` names, and explicit wire structures keep internal C++ layouts out of
the ABI. `log` forwards to a
real `core::AsyncLogService` when `GameplayHostContext::log` is set (category `"Gameplay"`,
level validated against `LogLevel`'s own range before the `uint32_t` -> enum cast, message built
from the `(pointer, length)` pair rather than assumed NUL-terminated); it stays a silent no-op when
that field is left null, exactly as it always was. `GameplayHostContext` grew a field to carry it --
like every other Runtime C++ facade type (`GameWorld`, `EntitySpawnDescriptor`, ...), it has no
`struct_size` of its own and is not part of the stable, versioned C ABI surface
(`NexoraGameplayHostV2`/`NexoraGameModuleV2`, which do carry one and which this repo's ABI gate
checks); callers must be rebuilt against the current header when it changes, same as for any of
those sibling types. Giving it its own binary-compatibility guarantee would only make sense as part
of a real ABI surface for the whole Game-namespace C++ facade, not a single struct in passing.
`subscribe_event` and `set_tick_enabled` delegate to optional embedding-owned callbacks in
`GameplayHostContext`. A missing subscription hook returns `NEXORA_GAMEPLAY_ERROR_UNSUPPORTED`;
a missing tick hook is a safe no-op. The embedding owns event delivery and update scheduling, so
the bridge never retains callback state past the context lifetime and invokes both hooks
synchronously on the calling game thread.

The canonical language boundary now lives in `Engine/API/include/nexora/nexora.h`;
`Nexora/Foundation/GameplayABI.h` is a compatibility include rather than a duplicate declaration.
`Engine/API/abi_manifest.json` records since, ownership, nullability, threading, error, and
determinism metadata for every callback export. `abi_baseline_v3.json` plus
`api.m6_manifest_compatibility` reject field removal/reordering without a major bump, while the
C11 consumer and `Bindings/Zig/nexora.zig` gate both supported language views.

The append-only V3 scene API callbacks expose only copied wire descriptors and opaque scalar IDs. Scene and entity lifetime remain host-owned; asset handles are non-owning UUID-derived tokens; input and diagnostics are by-value snapshots; raycast results are copied; and debug lines are high-level requests copied synchronously by the host. Every callback runs on the serialized game thread, borrows pointer arguments only for that call, returns `NexoraGameplayResult`, and publishes no partial object on failure. No RHI, device, queue, native-window, or swapchain pointer crosses this boundary.

`Gameplay/Zig/src/game_module.zig` and its ABI test now build with the repository-local Zig 0.14.0
toolchain and are covered by `gameplay.zig_abi_smoke`. `Apps/Showcase/NexoraShowcase` uses a
separate V3 host adapter, which serves the component wires through the same shared read/write path,
to map the stable Transform wire to a live `GameWorld` entity and emits
headless render/reload evidence; the local Windows `windows-zig-showcase` preset covers it with
`showcase.zig_headless`. `GameplayHostBridge` remains a V2 C++ facade covered by
`Tests/Runtime/GameplayHostBridgeTests.cpp`; its generic V2 `MakeHost()` entry is intentionally not
silently substituted for the V3 Showcase table. Native window/swapchain execution and dynamic
module discovery remain open.

## V2-M9 Timeline, UI, Audio, and Media portable foundation

`PresentationV2.h` builds on the existing V1 UI and presentation contracts. `Timeline` owns
deterministic keyed tracks with clamped scrub/seek and interpolation; `CameraRig` resolves weighted
layers at the highest active priority. Flex and grid layout return owning `LayoutBox` values using
the existing logical `Rect` coordinate system. `RichText` resolves localization-key spans through
the existing `LocalizationTable`, and `StyleSheet` plus `Theme` provide deterministic class-order
cascade and inline overrides without introducing a second localization or shaping authority.

`AccessibilityTree` validates stable element IDs, parent existence, cycles, and required accessible
names, then exposes deterministic reading order. `ProjectSurfacePointer` maps a world-space ray
through triangle barycentrics to UV and then to the existing UI pointer coordinates. `RoomAudioGraph`
finds the strongest portal-transmission route without requiring a per-source physics raycast.

`AdaptiveMediaStream` models local, HLS, and DASH segment timelines but deliberately forwards seek
and decoded-frame publication through the existing V1 `VideoPlayer`; the local-player contract
therefore remains authoritative. DRM and capture/encoder are optional provider interfaces compiled
only with `NEXORA_ENABLE_MEDIA_DRM` and `NEXORA_ENABLE_CAPTURE_ENCODER`, both OFF by default and
fully strippable. The portable gate is `runtime.v2_m9_timeline_ui_audio_media`. Production text
shaping/rendering, audio device/spatialization backends, HLS/DASH transport/ABR, DRM systems, and
capture codecs remain backend gates.

## V2-M12 hardening and reference-project portable foundation

`HardeningV2.h` defines the five V2 reference-project identities—Massive Outdoor, Indoor Portal
Dungeon, Network Arena, Crowd City, and Mobile Stress—and the capabilities each project must cover.
The portable hardening helpers add deterministic save-image corruption detection, a thermal
throttle policy/evidence validator, and an explicit V1-like footprint-growth budget. These are
evidence contracts, not substitutes for physical-device or long-duration runs.

`runtime.v2_m12_hardening_reference_projects` combines those contracts with the existing
`SoakMonitor`, `BundleUpdater`, and Network entity maps. Its fast gate checks 100,000 bounded
streaming-style resource samples, 2,000 disconnect/reconnect mapping resets, rollback to a
known-good generation, corrupted-save rejection, thermal-throttle evidence, and disabled-feature
footprint tolerance. It does **not** claim the V2-M12 shipping gate: five complete reference
projects, 24h+ streaming/network soaks, and real mobile thermal behavior still require target-host
execution and release-lab evidence.

## Editor snapshot replacement

`World::ReplaceSceneSnapshot` is an Editor-only synchronous transaction. It validates a snapshot in
a scratch World and rejects entity IDs owned by other scenes before replacing the target scene.
Success preserves the target scene ID and lifecycle state, keeps ID allocation monotonic, and expires
its entity/component borrows. Other scenes and their borrows remain valid. Gameplay Worlds and
unloading/unloaded targets reject replacement. Malformed snapshots or cross-scene collisions leave
the live World unchanged. The caller must clear authoring undo and invalidate document keys after
success; this API does not own renderer resources or asynchronous work.

`PlaySession::Generation()` starts at zero and advances to a nonzero value on each successful Start;
Stop retains it. It distinguishes owning reviews across new clones even when entity IDs/poses are
identical. Runtime's direct Stop(Transforms) remains separate from the graphical Editor's reviewed,
undoable SceneDocument transaction followed by Stop(Discard); applied_transforms counts only the
direct Runtime path. Editor's owning review also revalidates document/entity generations, supported
scene membership, and all copied original/Editor/Play values before mutation.

SceneEditor SetCameras/SetLights validate nonempty, equally-sized unique-ID batches and every proposed
presence/value before mutation, then apply all entries through one WorldCommandBuffer and one Undo
operation. Camera clipping and finite nonnegative Light intensity rules match their single-entity
wrappers. Undo/Redo own immutable command templates and replay fresh copies because Apply consumes
a command buffer; failed replay keeps the history cursor. Inputs are borrowed only for the call,
no Entity pointer survives a mutation, and methods remain serialized on the World authoring thread.
These C++ Editor operations do not alter the stable gameplay C ABI or scene snapshot format.

## Atomic selected-subtree deletion

`SceneEditor::DestroyEntities(scene, ids)` validates a unique nonempty selection in one live scene,
collapses selected descendants, and applies the selected root deletions through one World command
batch and one Undo entry. `DestroyEntity` delegates to it. The entry owns removed entity payloads,
original order, root sibling indexes and fallback world poses. Undo rejects ID collisions or expired
scene lifecycle before touching the live World, rehearses restoration/sibling placement in a scratch
World, then publishes only the target scene's entity storage. Existing unrelated entities survive;
normal replay retains original serialized order. Missing outside parents restore roots at their
captured world poses, as in the single-subtree contract. Roots from different missing parents
join a common sibling group in merged storage order, preserving existing unrelated roots. Redo requires the current removed-ID set to match the recorded subtrees; external expansion or
contraction rejects without deleting unrecorded entities. Failed initial edits or replay preserve history.
Calls remain synchronous on the serialized authoring thread; no borrowed entity/scene storage is
retained in history, and successful restoration invalidates target-scene entity borrows.

## Initialized forest cloning

`SceneEditor::CloneEntityForest(scene, prototypes)` treats prototype IDs as source identities and
remaps every internal parent to a new entity ID. The owning prototypes may include forward parent
references; unique nonzero IDs, a rooted acyclic forest, normalized valid local poses, enabled Camera
clipping and nonnegative Light values are validated before publication. Invalid inputs preserve the
World, allocation counter and history. Returned IDs follow prototype order. Creation and Undo/Redo
share one owning initialized payload batch; replay retains transform/component data and stable IDs.
Undo rejects a changed descendant-ID set, while Redo rejects collisions and expired lifecycle without
partial publication. Unrelated entities survive. Calls are synchronous on the authoring thread;
prototype spans are borrowed only for the call, successful publication invalidates target entity
borrows, and this API performs no IO or asset resolution.

## Initialized Camera/Light creation

`SceneEditor::CreateCameraEntity(scene, parent)` and `CreateLightEntity` synchronously initialize
one default component at identity local TRS and attach it to a live parent in the same scene
(zero selects a root), then record the complete entity in a single Undo transaction. Missing,
unloading/unloaded scenes and foreign/missing parents reject before creating anything. Initialization
uses the existing command-buffer transaction; rejected application discards the provisional entity.
Undo removes the entity; Redo restores the same stable ID, parent, pose and component defaults.
The caller serializes all calls on the World authoring thread, retains no Entity borrow across a
mutation and owns workspace write policy. These additive Editor-SDK C++ methods require rebuilding
consumers, without changing stable gameplay C/Zig wires or scene snapshot formats.

The Showcase courtyard's `courtyard-ibl-rgba16f-v1` assets use the same generic NXAB pipeline:
bounded offline HDRI conversion produces canonical linear RGBA16F payloads, each depending on a
`courtyard-ibl-metadata-v1` asset containing source/distribution identity, CC0 license evidence,
source/derived/converter hashes and integration parameters. The verified bundle closes those
dependencies before activation. Showcase owns decoded bytes and adapts them to borrowed native
Presentation uploads; Runtime stores no native handles and reads no source HDRI at render time.
This adds application-owned payload types without changing NXAB schema 1 or stable gameplay ABI.

The original courtyard hero crystal and six surface-detail maps are application-owned payload
types in the same NXAB pipeline. Each depends on an art metadata asset containing repository
license/source/converter and derived hashes. The owning activated generation supplies the mesh
and map bytes before Presentation submission. Procedural architectural/vessel/sky geometry remains
Showcase-authored CPU geometry; this does not claim that every procedural shape is a Runtime asset.
The original maps are distinct from downloaded CC0 KayKit/Poly Haven resources. No schema change.

Courtyard original leaf and mote masks extend that same verified art generation with payloads
127/128 depending on metadata 119. The converter retains both RGBA/hash entries; Showcase owns
eight surface payloads (six detail maps and two masks), borrowing them for native submission.
Wind timing and deterministic particle positions are application state, not persistent NXAB fields.
