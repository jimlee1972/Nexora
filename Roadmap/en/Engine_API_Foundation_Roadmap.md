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
