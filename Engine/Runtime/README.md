# V1-M4 through V1-M12 runtime contracts

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

`AudioEngine` enforces a hard voice limit, routes events through named buses, and acquires a
reference in `ResidencyTracker` for every active voice. Stopping one of several voices cannot
unload their shared clip. The `streaming` event bit is preserved for a MiniAudio/platform adapter;
this layer deliberately performs no device I/O.

`ParticleSystem` uses separate position, velocity, age, and lifetime arrays with bounded capacity.
Sprite, mesh, and trail renderer kinds share this CPU simulation contract; GPU simulation and draw
expansion can consume the same spawn data without changing gameplay ownership.

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

`GameplaySimulation.h` is the public, backend-neutral boundary for Physics → Character → Navigation → AI. `PhysicsWorld` provides authoritative immediate and batch queries without exposing Jolt types. The standard motor owns desired locomotion, gravity, root motion, and external velocity; `CharacterController` owns collision resolution, ground snap, stepping, crouch clearance, and teleport semantics. Every result reports requested and actual motion separately. Objects are synchronous and caller-owned; none are thread-safe.

`Move()`/`Teleport()` also take a caller-supplied `ground_ready`/`destination_ready` readiness flag and report `CharacterGroundState::StreamingPending` when it is false: locomotion, gravity accrual, and ground snap/step evaluation are all suspended and the character holds its current position instead of free-falling through geometry that has not streamed in, per the V1-M10 large-world streaming contract. This header stays independent of `LargeWorld.h` by design (either can be stripped without the other), so the readiness flag is the full extent of the contract here; a caller that wants the M10 `StreamingManager` to drive it is expected to pin the character's cells with `SetOccupied()` and query `Status(cell)->residency == Residency::Full` itself — declaring the character a high-priority streaming source and any automatic bridging between the two systems is gameplay/application-layer wiring this foundation does not provide.

`NavigationWorld` owns streamed tiles and invalidates paths by generation when a tile unloads. It only returns a desired velocity and never receives a `World` or writable `Transform`. The AI foundation uses fixed typed blackboard slots, a compact shared behavior program with per-tick deterministic traces, and a stimulus query with an explicit work/result budget. Configure with `-DNEXORA_ENABLE_GAMEPLAY_SIMULATION=OFF` to strip this implementation and run the feature-strip gate. The enabled test validates batched physics queries, ground/wall resolution, teleport, streaming-pending hold/resume, cross-tile navigation and stale-path invalidation, blackboard typing, behavior execution, and perception budgets.

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
serialization, and snapshots use the versioned `NEXORA_SCENE 1` text schema. Loading validates the
complete snapshot before publishing it; malformed versions, duplicate IDs, non-finite transforms,
and IDs already owned by the destination world are rejected without partially adding a scene.
Double-precision world transforms provide the large-coordinate foundation.

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

## Zig gameplay bridge

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
initializes and restores the candidate, and only then retires the active module; load, descriptor,
start, and migration failures stop and destroy the candidate while keeping the active module alive.
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
into Create/Modify/Undo operations. Undoing a destroyed entity restores both its component data and
stable ID through the editor's privileged access to `World`; older transform and create undo cards
therefore continue to target the same entity. Destroy also validates that the entity belongs to the
supplied scene before mutating the world. `CreateEntity` returns the new entity's stable `Id`, not a
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

**Scope note:** this milestone's graphical surfaces -- Hierarchy, Scene View, Game View, Inspector,
Property Drawer, Asset Browser, Gizmo, and Profiler UI -- are not implemented here. They need a
windowing/rendering front end this repository does not have yet (`Apps/Host/NexoraHost` is a
headless CLI); `SceneEditor` and `PrefabInstance` are the data-model and command layer such a
front end would eventually drive, exercised here through CTest rather than through any UI.

`PlaySession` is the portable PIE ownership contract. It owns an isolated `WorldKind::Play` clone;
`Tick` runs only while playing, `Step` runs exactly one fixed update while paused, and input focus
starts released until explicitly granted by editor policy. Stopping discards runtime mutations by
default. The only supported apply-back policy copies changed transforms for stable entity IDs;
runtime-created entities and all other component mutations remain isolated and are discarded.
Transform apply-back is a deterministic stable-ID diff against the source transform snapshot. If an
Editor transform changed concurrently, the entire apply is rejected without partial mutation and the
conflict remains visible through `LastApplyBackStatus`. The Play World and update callback are released
before `Stop` returns, including after conflicts and contained update failures.

`RuntimeConsole` is a bounded, mutex-protected multi-producer ingress for owning structured records
(sequence, severity, category, timestamp, source, and message). Old records are evicted in sequence
order and the cumulative dropped count is observable. `PlaySession::Inspect` similarly returns an
owning, stable-ID-sorted entity/component snapshot rather than pointers into relocatable World storage.
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
The immediate setters and `DestroyEntity` use the same `WorldCommandBuffer` internally, reusing
the M4 command-buffer contract rather than adding new `World` friend access.
`Query(scene, mask)` is an OR-mask batch query over a scene's entities.
`CaptureInput` wraps `InputSystem::Consume` into a by-value `InputSnapshot`, and `AssetRef` is a
named re-export of the already-ABI-appropriate `AssetUuid` (API-M2). The facade owns its portable
`PhysicsWorld` and `AudioMixer`, binds bodies and voices to the owning entity ID, removes those
bindings on entity destruction, resolves ray hits back to entity IDs, and synchronizes an attached
`CharacterController`'s position to the entity Transform after each motor tick. Audio resource IDs
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
write a live entity's Transform, camera, light, and mesh-renderer state. Stable component IDs are
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
separate V3 host adapter to map the stable Transform wire to a live `GameWorld` entity and emits
headless render/reload evidence; the local Windows `windows-zig-showcase` preset covers it with
`showcase.zig_headless`. `GameplayHostBridge` remains a V2 C++ facade covered by
`Tests/Runtime/GameplayHostBridgeTests.cpp`; its generic V2 `MakeHost()` entry is intentionally not
silently substituted for the V3 Showcase table. Native window/swapchain execution and dynamic
module discovery remain open.
