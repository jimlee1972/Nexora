# Nexora Engine API Foundation Roadmap

> Version: v1.0 | Status: planning baseline | Updated: 2026-09-21

## 1. Purpose and gap

Existing modules prove lifecycle, RHI, scene, and runtime contracts, but tested internal types are not yet a durable game-author API. This roadmap adds the math, transform, text, file I/O, time, identity, collection, and diagnostics surface expected from a 3D engine, with explicit C++, stable C ABI, and Zig boundaries.

## 2. Design rules

- Stateless value types belong in `Foundation`, process services in `Core`, and world/entity APIs in `Runtime`.
- C++ may offer typed conveniences; DLL/language boundaries use fixed-width scalars, PODs, handles, and pointer-length pairs only.
- Public coordinates are right-handed and Y-up. Angle names state radians or degrees; matrix layout and multiplication order are contractual.
- STL types, exceptions, RTTI objects, and allocator ownership never cross the C ABI.
- Deterministic and fast math are separate. Gameplay reaches files through VFS, never unrestricted OS paths.
- An API is incomplete until it has documentation, unit tests, ABI tests, and a sample consumer.

## 3. API tracks

### API-M1 — Math and geometry

Deliver `Scalar`, `Vector2/3/4`, `Quaternion`, `Matrix3/4`, `Transform`, `Color`, `Rect`, `Ray`, `Plane`, `Aabb`, `Sphere`, and `Frustum`, including safe normalization, interpolation, TRS composition/decomposition, projection, intersection, and epsilon policy. Gates cover scalar/SIMD tolerance, NaN and zero-length behavior, coordinate golden tests, serialization round trips, and C ABI layout.

### API-M2 — Foundation data types

Deliver UTF-8 `StringView`/engine-owned `String`, formatting/parsing boundaries, `Uuid`, `Name/StringId`, generational handles, `Result/ErrorCode`, `Span`, and `ByteBuffer`. C++ containers remain conveniences; ABI buffers are caller-owned or opaque engine allocations with matching destroy functions. Test malformed UTF-8, embedded NUL, locale-independent parsing, collision diagnostics, and cross-module allocation.

### API-M3 — VFS and file I/O

Expose mount URIs (`content://`, `user://`, `cache://`), canonical paths, small synchronous reads, asynchronous requests, streams, enumeration, metadata, atomic writes, and watches. Specify sandboxing, symlinks, case behavior, cancellation, partial reads, and completion threads. Gates cover memory/temp/bundle backends, traversal attacks, cancellation races, atomic replacement, large offsets, and injected failures.

### API-M4 — Engine services

Expose monotonic/game/fixed time, versioned random streams, structured logs, profiling markers, configuration, task dispatch, and event subscriptions. Wall-clock time cannot drive simulation. Every callback states its thread, lifetime, unsubscribe, and reentrancy contract.

### API-M5 — World/game facade

Use opaque world/entity handles and component type IDs for spawn/destroy, get/set/batch query, deferred commands, scene loading, asset references, input snapshots, and camera/light/renderable/physics/audio facades. Zig never receives movable C++ ECS storage pointers.

### API-M6 — Bindings and versioning

Create a canonical C header and ABI manifest. Generate or validate the C++ facade and Zig thin wrapper from the same declarations. Every export records `since`, ownership, nullability, threading, errors, and determinism. Extensible descriptors carry `struct_size`; semantic removal requires a major ABI.

## 4. Proposed layout

```text
Engine/Foundation/include/Nexora/Math/
Engine/Foundation/include/Nexora/Text/
Engine/Core/include/Nexora/IO/
Engine/Core/include/Nexora/Services/
Engine/Runtime/include/Nexora/Game/
Engine/API/include/nexora/nexora.h
Bindings/Zig/nexora.zig
Tests/API/
```

This is a target layout, not a claim that these paths are implemented today.

## 5. Sequence and definition of done

| Phase | Deliverable | Exit gate |
| --- | --- | --- |
| A | conventions, ownership/error table, ABI manifest schema | architecture review and ABI fixture |
| B | math, geometry, transform | unit/property/golden/layout tests |
| C | text, IDs, buffers, results | fuzz corpus and cross-module allocation test |
| D | VFS, streams, async I/O | backend contract and sandbox suites |
| E | services and world facade | deterministic replay and lifecycle/thread tests |
| F | C/Zig bindings and reference | C/C++/Zig consumer builds and compatibility diff |

Every API needs a public header, contract, success/failure examples, tests, version metadata, and a Showcase use site. Performance claims require measured benchmark baselines.

## 6. Non-goals, risks, and dependencies

This plan does not reinvent the STL, Unicode shaping, a general scripting VM, or expose all renderer internals. Principal risks are ABI drift, dangling views, allocator mismatch, non-uniform transform scale, and callbacks after shutdown; ABI snapshots, sanitizers, shutdown stress, and owner-tag diagnostics gate them. API-M1 through M4 precede the Showcase and Editor; M5/M6 keep both on supported public paths. The Showcase is the first external consumer and the Editor is the stress consumer—neither may build private bypasses.

## 7. API-M1 through M4 status (2026-09-22)

Checked against section 2's "documentation, unit tests, ABI tests, and a sample" bar, item by item, and not claimed as fully done:

- **API-M1 Math (portable implementation complete)**: `Engine/Foundation/include/Nexora/Math/Math.h` delivers the full value surface, safe normalization, interpolation, TRS compose/decompose (including reflected scale), inverse/projection/look-at, geometry intersection, and the documented epsilon policy. `Tests/API/ApiFoundationTests.cpp` gates every POD's ABI layout and byte round trip, zero/non-finite behavior, geometry boundaries, and transform round trips. `Vector4::Dot` and `Matrix4` multiplication have SSE2 and ARM NEON implementations plus always-available scalar references; deterministic fuzz tests compare the selected path against the scalar result with reduction-order-aware tolerances. Coordinate golden constants were independently generated from DirectXMath's right-handed look-at and perspective functions and converted from its row-vector convention, so handedness/layout/depth errors cannot self-pass through Nexora-derived expectations. The public sample exercises transform, camera, projection, and frustum use. Linux x86-64 validates the scalar and SSE2 paths in cloud CI; the NEON path is implemented and compile-selected on ARM, but ARM runtime evidence must be collected on an ARM target and is not claimed by this Linux run.
- **API-M2 Foundation types**: `Engine/Foundation/include/Nexora/Foundation/Types.h` has UTF-8 validation, `StringView`/`String`/`ByteBuffer`/`Span`, `Uuid` (with `Parse`/`ToString`), `Name`, `Result<T>`, and locale-independent `ParseNumber`. The generational-handle deliverable is satisfied by the existing `nexora::core::Handle<Tag>`/`HandlePool<Tag>` (`Engine/Core/include/Nexora/Core/Handle.h`), deliberately not duplicated in Foundation. **Not done**: the "ABI uses caller buffer or engine-owned opaque buffer + destroy function" variant this section calls for -- nothing currently needs `String`/`ByteBuffer`/`Span` themselves (as opposed to a handle or POD) to cross the C ABI, so that mechanism has not been built ahead of a real use.
- **API-M3 VFS**: `Engine/Core/include/Nexora/Core/Vfs.h`'s `VirtualFileSystem` now has two backends: `Mount` (directory) and `MountMemory` (in-memory, same Read/WriteAtomic/Metadata/Enumerate surface), both agreeing that `WriteAtomic` creates a missing parent directory rather than failing. A mount's own root (`"mount://"`, nothing after the scheme) resolves to the empty relative path instead of `InvalidPath`, so `Enumerate` can list a mount's full top-level contents without a caller guessing a subpath first. `Tests/API/ApiCoreContractTests.cpp` runs the identical contract suite (read/write/metadata/enumerate-including-root/traversal rejection/error injection/a multi-MiB round trip) against both backends. `Nexora::Runtime::MountBundle` (`Engine/Runtime/include/Nexora/Runtime/BundleMount.h`) is the bundle-backend deliverable: it verifies a V1-M5 `Bundle` (`BundleBuilder::Verify`) and projects each asset onto a `MountMemory` mount at `<name>://<uuid>.blob`, so reading a cooked asset out of a shipped bundle is `vfs.Read(...)` then `AssetCooker::Deserialize`, not a separate bundle-specific API; it could not live in `VirtualFileSystem` itself since `Bundle` is a Runtime type and Core must not depend on Runtime. Mount names remain caller-chosen; `Engine::Initialize` now wires up `content` and `temp` (via `std::filesystem::temp_directory_path()`, portable and needing no new platform code); `engine`, `project`, `user`, and `cache` remain unmounted since a real per-OS user-data/cache/install directory query doesn't exist here yet, and `bundle` isn't a single static mount at all -- it's the mount *type* `MountBundle` creates per bundle at runtime. **Not done**: `PlatformPackageMount`, the full async IO scheduler (priority preemption, request merge/coalescing, aligned reads, streaming deadline hints), memory mapping, Shipping-mode call-site privilege gating (`Mount` treats every caller equally today; only in-mount path traversal is rejected), and a true multi-GB/offset boundary test (today's test is a multi-MiB smoke test, not a huge-file test).
- **API-M4 Engine services**: `Engine/Core/include/Nexora/Core/Services.h`'s `MonotonicNanoseconds`/`RandomStream` (PCG32, versioned)/`Configuration`, together with the pre-existing `FixedTickClock` (game/fixed time), `AsyncLogService` (structured logging), `JobSystem` (task dispatch), and `EventBus` (event subscription), now cover all nine listed deliverables. `ProfilingMarker` gained `SetSink`, a plain-function-pointer emission hook (previously it was a bare timer with no output mechanism at all). `Tests/API/ApiCoreContractTests.cpp` covers `MonotonicNanoseconds` monotonicity and the `ProfilingMarker` sink call.

**Sample**: `Samples/Api/ApiFoundationSample.cpp` (`NEXORA_FEATURE_API_SAMPLES`, default ON) is a real, compiled-and-run minimal sample covering Math/Types/VFS/Services, wired into CTest as `samples.api_foundation`. It is not the windowed `NexoraShowcase` from `Roadmap/V1-Visual-Showcase-Long-Term-Plan.md` (a separate V1-M0 through M12 effort requiring Windows/DX12).

All of the above was verified only on the Linux `linux-development` preset (plus one manual ASan/UBSan build of the new tests and sample, with no memory errors, UB, or leaks). Windows/macOS/Android/iOS remain unverified here.

## 8. API-M5 through M6 status (2026-09-22)

- **API-M5 World/Game facade**: added `Engine/Runtime/include/Nexora/Game/GameWorld.h`. `World::FindEntity`/`FindScene` return a pointer into `World`'s own `std::vector<Entity>`/`std::vector<Scene>` storage, which `CreateEntity`/`LoadScene` can invalidate at any time; `GameWorld` wraps spawn/destroy, get/set, batch query (OR-mask), and scene load so every operation returns only a plain `nexora::runtime::Id` or a by-value `EntitySnapshot`, satisfying the roadmap's explicit rule that Zig must never receive a movable C++ ECS storage pointer. `Id` is **not** additionally wrapped in a generational handle: it is already a monotonically increasing, never-reused identifier, which the V1 Complete Plan's own ABI rules list as its own accepted C-ABI-crossing category ("EntityID"), distinct from an index+generation Opaque Handle -- stacking both would add nothing. Camera/light/mesh-renderer are entity-integrated via `EntitySpawnDescriptor`/`EntitySnapshot`; the asset reference deliverable re-exports the existing `AssetUuid` (API-M2); the input snapshot deliverable wraps `InputSystem::Consume`. **Not done**: `PhysicsWorld`/`CharacterController`/`AudioMixer` remain standalone systems with their own `SimulationId`/resource-id space, not bound to an entity -- that is a larger design this pass does not attempt.
`GameplayHostBridge` remains covered by the C++-only test (`Tests/Runtime/GameplayHostBridgeTests.cpp`) that calls the built `NexoraGameplayHostV2` function pointers directly. The V3 Zig Showcase uses a separate V3 host table rather than silently substituting the V2 `MakeHost()` facade. In the local Windows `windows-zig-showcase` preset, `Gameplay/Zig/src/game_module.zig` now builds with repository-local Zig 0.14.0; `NexoraShowcase` drives a live `GameWorld` Transform through the V3 ABI and produces a passing headless render/reload report. This local result does not claim the Linux gate or the windowed native backend.

The historical Linux verification above remains clean for the API-M5/M6 changes. The 2026-09-23
Windows run additionally passed the full `windows-zig-showcase` CTest suite (26/26), including the
Zig ABI smoke test and the headless Showcase validation. Linux/macOS/Android/iOS and the windowed
native backend remain outside that local Windows result.
