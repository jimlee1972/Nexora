# V1-M4 through V1-M12 runtime contracts

`NexoraRuntime` is the dependency-ordered, platform-neutral baseline for the remaining V1
milestones. It deliberately contains no SDK-specific physics, media, mobile, or editor backend.
Instead, it makes the ownership and safety boundaries executable before those integrations land.

| Milestone | Executable baseline |
| --- | --- |
| M4 | Additive scene lifecycle, stable entity IDs, deferred safe unload, double-precision transforms |
| M5 | Hash-validated asset generations, dependency-cycle rejection, pinning and rollback |
| M6 | Reflection metadata, a real dynamic plugin loader with a stable C ABI gate and service registration, a scene editor built on Create/Modify/Undo, and prefab override/rebase (see below for the full vertical slice; `ExtensionRegistry`/`UndoStack` remain the lighter M6 row exercised by `runtime.v1_m4_m12_contracts`) |
| M7 | Device-neutral input routing, stable touch IDs, virtual-list materialization and locale fallback |
| M8 | Separate character intent and resolved motion; query-only navigation paths do not write transforms |
| M9 | Animation playback, bounded audio voices, reference-counted residency and a timestamped media queue |
| M10 | Separate cell/bundle identity, observable RAM/VRAM use, HLOD state and occupied-cell pins |
| M11 | App lifecycle, pressure policy and native WebView pointer ownership |
| M12 | Profile-driven plugin, shader, optional-asset and headless presentation stripping |

The contract test `runtime.v1_m4_m12_contracts` exercises every row, including localization
fallback, unreachable navigation, animation looping, audio voice limits, media back-pressure and
seek invalidation. Platform SDK adapters and production authoring tools remain future work and
must preserve these interfaces rather than bypassing their lifecycle checks.

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

`GameplayModuleHost` executes the versioned `NexoraGameModuleV2` C ABI while the original V1 layouts
remain declared for source compatibility. It validates the host and
module structure sizes, ABI version, and required callbacks before initialization. Update, reload,
and unload operations are serialized; a replacement module is initialized before the active module
is shut down, and a rejected replacement leaves the active module running.

The host table exposes size-checked component reads/writes, event subscription, and tick control.
Modules may additionally provide state save/load callbacks. Reload serializes the active state,
initializes and restores the candidate, and only then retires the active module; migration failure
keeps the active module alive. `GetReloadStats()` exposes successful reload count, migrated bytes,
and wall-clock reload duration for profiler integration.

Configure with `-DNEXORA_ENABLE_ZIG_GAMEPLAY=ON` to compile the minimal Zig GameModule and run the
`gameplay.zig_abi_smoke` test. Zig 0.14.0 is the pinned CI toolchain. This is the first executable
toolchain gate. Dynamic-library/editor orchestration, mobile cross-compilation, and device execution
remain required follow-up gates.

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
M4-M12 contract sweep, exercised by `runtime.v1_m4_m12_contracts`) is a lighter descriptor/ABI-number
bookkeeping structure that predates this milestone and does not itself load anything.

`SceneEditor` composes `World`, `WorldCommandBuffer`, and `UndoStack` (from the M4 vertical slice)
into Create/Modify/Undo operations. Undoing a destroyed entity restores both its component data and
stable ID through the editor's privileged access to `World`; older transform and create undo cards
therefore continue to target the same entity. Destroy also validates that the entity belongs to the
supplied scene before mutating the world.

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
