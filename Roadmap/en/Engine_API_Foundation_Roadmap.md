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

- **API-M1 Math**: `Engine/Foundation/include/Nexora/Math/Math.h` now has dot/cross/normalize-safe/lerp/slerp, TRS compose/decompose, `Matrix3`/`Matrix4` inverse (epsilon-fallback policy), `LookAt`, `Orthographic`/`PerspectiveRadians`, `ExtractFrustum`, and `Intersects(Frustum, Aabb/Sphere)`. `Tests/API/ApiFoundationTests.cpp` provides the `sizeof`/`alignof`/`offsetof` ABI layout gate, a byte-level round trip, and a Compose/Decompose inverse check. `Vector4`'s `Dot`/`Length`/`NormalizeSafe`/`Lerp` share their names with `Vector3`'s overloads of the same operation but are function templates constrained to exactly `Vector4` (`template <typename T> requires std::is_same_v<T, Vector4>`) rather than plain overloads: a plain overload would be ambiguous for a bare brace-init-list call such as `Dot({1, 2, 3}, {4, 5, 6})` (both types are aggregates, so a 3-of-4-members initializer list converts equally well to either) -- a real regression a review caught after a same-named plain-overload version briefly merged. Because template argument deduction is never attempted from a bare braced-init-list against a plain type-template parameter, the `Vector4` templates simply are not viable candidates for such a call, so the ambiguity never arises; called with an already-typed `Vector4`, they bind normally. `Dot` now has an SSE2 SIMD path (enabled via `NEXORA_MATH_HAS_SSE2` when the target is x86/x64 with SSE2, falling back to scalar otherwise), cross-checked against the scalar reference by a 10,000-iteration fuzz test whose tolerance is scaled to the sum of `|component product|` magnitudes rather than the final dot value, so catastrophic cancellation near zero doesn't produce a falsely tight bound. **Not done**: the SIMD path covers only `Dot(Vector4, Vector4)` -- `Vector3`'s dot/cross, `Matrix3`/`Matrix4` multiplication, and every other operation in this file remain scalar-only, and the SSE2 path is verified on x86/x64 only; on ARM/NEON it correctly falls back to scalar via the same guard but that fallback itself is untested (no ARM target in this sandbox). Coordinate golden tests against an external reference engine (only internal self-consistency is checked) are also still not done. `Quaternion::NormalizeSafe` and `Vector3`'s one-argument `NormalizeSafe(Vector3)` (default fallback) are now constrained templates too, for the same reason and by the same fix as `Vector4`'s: a bare `NormalizeSafe({1, 2, 3})` was separately already ambiguous between them (`Quaternion::w` defaults to `1.0F`, so a 3-element list aggregate-inits either type), a pre-existing hazard that predated this SIMD work; every real call site passes an already-typed argument, so this closes the hazard with no behavioral change. `Vector3`'s **two**-argument `NormalizeSafe(Vector3, Vector3)` deliberately stays a plain overload -- that arity was never ambiguous with `Quaternion`'s one-argument form (arity alone rules it out), and a first pass that templated the whole two-argument overload broke bare-brace two-argument calls like `NormalizeSafe({3, 4, 0}, {0, 1, 0})` for no benefit, a regression a review caught and this fix corrects by splitting the two arities apart. `Dot4`/`Length4`/`NormalizeSafe4`/`Lerp4` also still exist as thin non-template forwarding wrappers over the templated names above, kept for any caller that adopted that spelling during the brief window it was the only one available.
- **API-M2 Foundation types**: `Engine/Foundation/include/Nexora/Foundation/Types.h` has UTF-8 validation, `StringView`/`String`/`ByteBuffer`/`Span`, `Uuid` (with `Parse`/`ToString`), `Name`, `Result<T>`, and locale-independent `ParseNumber`. The generational-handle deliverable is satisfied by the existing `nexora::core::Handle<Tag>`/`HandlePool<Tag>` (`Engine/Core/include/Nexora/Core/Handle.h`), deliberately not duplicated in Foundation. **Not done**: the "ABI uses caller buffer or engine-owned opaque buffer + destroy function" variant this section calls for -- nothing currently needs `String`/`ByteBuffer`/`Span` themselves (as opposed to a handle or POD) to cross the C ABI, so that mechanism has not been built ahead of a real use.
- **API-M3 VFS**: `Engine/Core/include/Nexora/Core/Vfs.h`'s `VirtualFileSystem` now has two backends: `Mount` (directory) and `MountMemory` (in-memory, same Read/WriteAtomic/Metadata/Enumerate surface), both agreeing that `WriteAtomic` creates a missing parent directory rather than failing. A mount's own root (`"mount://"`, nothing after the scheme) resolves to the empty relative path instead of `InvalidPath`, so `Enumerate` can list a mount's full top-level contents without a caller guessing a subpath first. `Tests/API/ApiCoreContractTests.cpp` runs the identical contract suite (read/write/metadata/enumerate-including-root/traversal rejection/error injection/a multi-MiB round trip) against both backends. `Nexora::Runtime::MountBundle` (`Engine/Runtime/include/Nexora/Runtime/BundleMount.h`) is the bundle-backend deliverable: it verifies a V1-M5 `Bundle` (`BundleBuilder::Verify`) and projects each asset onto a `MountMemory` mount at `<name>://<uuid>.blob`, so reading a cooked asset out of a shipped bundle is `vfs.Read(...)` then `AssetCooker::Deserialize`, not a separate bundle-specific API; it could not live in `VirtualFileSystem` itself since `Bundle` is a Runtime type and Core must not depend on Runtime. Mount names remain caller-chosen; `Engine::Initialize` now wires up `content` and `temp` (via `std::filesystem::temp_directory_path()`, portable and needing no new platform code); `engine`, `project`, `user`, and `cache` remain unmounted since a real per-OS user-data/cache/install directory query doesn't exist here yet, and `bundle` isn't a single static mount at all -- it's the mount *type* `MountBundle` creates per bundle at runtime. **Not done**: `PlatformPackageMount`, the full async IO scheduler (priority preemption, request merge/coalescing, aligned reads, streaming deadline hints), memory mapping, Shipping-mode call-site privilege gating (`Mount` treats every caller equally today; only in-mount path traversal is rejected), and a true multi-GB/offset boundary test (today's test is a multi-MiB smoke test, not a huge-file test).
- **API-M4 Engine services**: `Engine/Core/include/Nexora/Core/Services.h`'s `MonotonicNanoseconds`/`RandomStream` (PCG32, versioned)/`Configuration`, together with the pre-existing `FixedTickClock` (game/fixed time), `AsyncLogService` (structured logging), `JobSystem` (task dispatch), and `EventBus` (event subscription), now cover all nine listed deliverables. `ProfilingMarker` gained `SetSink`, a plain-function-pointer emission hook (previously it was a bare timer with no output mechanism at all). `Tests/API/ApiCoreContractTests.cpp` covers `MonotonicNanoseconds` monotonicity and the `ProfilingMarker` sink call.

**Sample**: `Samples/Api/ApiFoundationSample.cpp` (`NEXORA_FEATURE_API_SAMPLES`, default ON) is a real, compiled-and-run minimal sample covering Math/Types/VFS/Services, wired into CTest as `samples.api_foundation`. It is not the windowed `NexoraShowcase` from [`V1-Visual-Showcase-Long-Term-Plan.md`](V1-Visual-Showcase-Long-Term-Plan.md) (a separate V1-M0 through M12 effort requiring Windows/DX12).

All of the above was verified only on the Linux `linux-development` preset (plus one manual ASan/UBSan build of the new tests and sample, with no memory errors, UB, or leaks). Windows/macOS/Android/iOS remain unverified here.

## 8. API-M5 through M6 status (2026-09-22)

- **API-M5 World/Game facade**: added `Engine/Runtime/include/Nexora/Game/GameWorld.h`. `World::FindEntity`/`FindScene` return a pointer into `World`'s own `std::vector<Entity>`/`std::vector<Scene>` storage, which `CreateEntity`/`LoadScene` can invalidate at any time; `GameWorld` wraps spawn/destroy, get/set, batch query (OR-mask), and scene load so every operation returns only a plain `nexora::runtime::Id` or a by-value `EntitySnapshot`, satisfying the roadmap's explicit rule that Zig must never receive a movable C++ ECS storage pointer. `Id` is **not** additionally wrapped in a generational handle: it is already a monotonically increasing, never-reused identifier, which the V1 Complete Plan's own ABI rules list as its own accepted C-ABI-crossing category ("EntityID"), distinct from an index+generation Opaque Handle -- stacking both would add nothing. Camera/light/mesh-renderer are entity-integrated via `EntitySpawnDescriptor`/`EntitySnapshot`; the asset reference deliverable re-exports the existing `AssetUuid` (API-M2); the input snapshot deliverable wraps `InputSystem::Consume`. **Not done**: `PhysicsWorld`/`CharacterController`/`AudioMixer` remain standalone systems with their own `SimulationId`/resource-id space, not bound to an entity -- that is a larger design this pass does not attempt.
- **API-M6 bindings**: added `Engine/Runtime/include/Nexora/Game/GameplayHostBridge.h`, wiring the existing `NexoraGameplayHostV2` C ABI (`Nexora/Foundation/GameplayABI.h`, the "Zig gameplay bridge" contract) to a real `GameWorld`. Before this, the `read_component`/`write_component` function-pointer slots were only ever filled by a test-scoped stand-in in `Gameplay/Zig/ZigGameplayTests.cpp` (`HostState`, which stores its own private value and has no connection to any real `World`). `MakeHost()` produces a host whose `read_component`/`write_component` actually read and write a live entity's `Transform`, keyed by `TransformComponentType()` (a stable FNV-1a hash of `"Nexora.Transform"` via `nexora::foundation::Name`, not a magic number either side has to agree on by convention). Only the Transform component type is wired in this pass. `log` now forwards to a real `core::AsyncLogService` when `GameplayHostContext::log` is set (previously a pure no-op regardless), validating the level against `LogLevel`'s own range before casting and building the message from the `(pointer, length)` pair rather than assuming NUL termination; it stays a no-op when that field is left null. `subscribe_event`/`set_tick_enabled` honestly report "not implemented" (failure/no-op), and for a reason beyond missing wiring: the ABI has no callback slot for the host to invoke the module when a subscribed event fires, and gating `GameplayModuleHost::Update` needs a real cross-namespace design decision this pass does not make. **Not verified**: `Gameplay/Zig/src/game_module.zig` and its test were not touched at all -- no Zig toolchain is available in this environment (`which zig` fails), so a `.zig` change could not be rebuilt or verified locally; only the `gameplay.zig_abi_smoke` CI runners have Zig. `GameplayHostBridge` is instead covered by a C++-only test (`Tests/Runtime/GameplayHostBridgeTests.cpp`) that calls the built `NexoraGameplayHostV2` function pointers directly. Having the Zig sample actually call through this bridge, replacing its own private fake host, remains open follow-up work.

Verified: `linux-development` 23/23, `linux-shipping` (Monolithic, since this adds to Runtime's link surface), plus one manual ASan/UBSan build -- all clean. Windows/macOS/Android/iOS and the Zig-side integration remain unverified here.
