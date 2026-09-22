# Nexora Core runtime contract (V1-M1)

`NexoraCore` owns process-level runtime services. It depends only on `NexoraFoundation`; higher-level world, rendering, asset, editor, and gameplay modules must depend on Core rather than the reverse.

## Public I/O and services (API-M3/M4)

`VirtualFileSystem` accepts both `mount/path` and public `mount://path` URIs, including a mount's
own root with nothing after the scheme (`"mount://"`) -- that resolves to the empty relative path,
not `InvalidPath`, so `Enumerate` can list everything a mount contains without a caller having to
know or guess a subpath first. It rejects traversal, absolute paths, backslashes, and symlink
escapes -- this path-containment check is always on, for every mount, in every build configuration;
it is not gated behind a Shipping-only privilege tier (see "Not yet implemented" below). Reads and
metadata are snapshots; enumeration is lexically sorted. `WriteAtomic` writes a sibling temporary
file then renames it (directory backend, creating any missing parent directory first) or replaces a
map entry under a mutex (memory backend, which never had a notion of a missing parent to begin
with), so success makes the complete replacement visible and both backends agree on the exact same
virtual path. Async completion runs on a job worker and the returned handle owns its result state;
the cancellation token is checked once, before the read starts, not partway through a large read
already in flight.

Mount names are caller-chosen, not fixed by this file: the master plan's canonical logical roots
(`engine:// project:// bundle:// cache:// user:// temp://`, see the V1 Complete Plan's "P. Virtual
File System") are a naming *convention* for callers to follow, not something `VirtualFileSystem`
enforces. Only `content` is currently wired up, by `Engine::Initialize` below -- the other five
roots are not yet mounted by any Core or Runtime code.

Two backend kinds share the same `Mount`-table/`Read`/`WriteAtomic`/`Metadata`/`Enumerate` surface:
a directory backend (`Mount`, backed by a real host directory) and a memory backend (`MountMemory`,
a flat key/value store scoped to the `VirtualFileSystem` instance's lifetime, never touching the
host filesystem). `Tests/API/ApiCoreContractTests.cpp` runs the identical read/write/metadata/
enumerate/error-injection sequence against both to keep them behaviorally equivalent, not just
individually correct.

`Services.h` exposes steady-clock nanoseconds, deterministic versioned PCG random streams,
thread-safe configuration, and a `ProfilingMarker` scoped timer alongside the existing fixed game
clock, logging, jobs, and event bus (task dispatch is `JobSystem`; event subscription is
`EventBus`; structured logging is `AsyncLogService`/`LogRecord`; game/fixed time is
`FixedTickClock`/`WorldTimeState` -- see `Time.h` and `Log.h`). Simulation must use
`FixedTickClock`, not wall time. Event callbacks run synchronously on the publishing thread;
unsubscribing prevents future calls but does not cancel a callback already copied for dispatch.
`ProfilingMarker::SetSink` registers a plain-function-pointer emission hook (name, elapsed
nanoseconds) called from every marker's destructor; the default (no sink) makes it a zero-overhead
scoped timer. It is an emission hook, not a profiler: aggregation, a HUD, or forwarding onto
`EventBus` belongs in the function you register, not in this class. Not thread-safe to change
concurrently with steady-state marker traffic -- set it once at startup.

A bundle backend exists at the Runtime layer, not here: `Nexora::Runtime::MountBundle`
(`Engine/Runtime/include/Nexora/Runtime/BundleMount.h`) projects a verified V1-M5 `Bundle`'s assets
onto a `MountMemory` mount, one file per asset. It could not live in `VirtualFileSystem` itself --
`Bundle` is a Runtime type, and Core must not depend on Runtime -- so it is a Runtime-side function
that takes a `VirtualFileSystem&` the caller already owns, not a third `Mount*` method here.

**Not yet implemented** (tracked against the V1 Complete Plan's "P. Virtual File System" and the
Engine API Foundation roadmap, not silently treated as done): a `PlatformPackageMount` backend; the
full async IO scheduler (request merge/coalescing, priority preemption, aligned reads, streaming
deadline hints -- `ReadAsync` here is a single job per request with no such scheduling);
memory-mapped file access; and call-site privilege gating for who may call `Mount`/`MountMemory`
with which paths in a Shipping build (today `Mount` is just a regular Core API -- any caller with a
`VirtualFileSystem&` can mount any host directory it can see; only path traversal *within* an
already-mounted root is rejected).

## Lifecycle

`Engine::Initialize` starts logging, workers, and the `content` VFS mount. `Engine::Shutdown` drains jobs before destroying VFS state, then drains logging. Both shutdown and destruction are idempotent. `EngineServices` is a non-owning view valid only between initialization and shutdown.

## Threading and lifetime

- `JobSystem`, `AsyncLogService`, `TrackingAllocator`, and `VirtualFileSystem` support concurrent producers.
- `EventBus` snapshots callbacks before invoking them, so callbacks run without its lock held.
- `FrameArena` is single-owner. Its allocations are invalid after `Reset`; callers must complete dependent jobs before `Engine::BeginFrame`.
- `AsyncReadHandle::Get` is a snapshot and never blocks. `Pending` means the caller should poll or schedule later work.

## Memory and errors

The tracking allocator's aligned-allocate/deallocate calls are the replaceable backend seam: by default they go through `::operator new`/`::operator delete`, and with `-DNEXORA_ENABLE_MIMALLOC=ON` they go through `mi_malloc_aligned`/`mi_free` instead (mimalloc is fetched via CMake `FetchContent`, off by default, built as a static library and never exposed in a public Core header). Either way, live/peak bytes and per-tag totals are tracked the same; allocator objects and exceptions never cross a C ABI. Frame arena exhaustion reports `std::bad_alloc`. Invalid lifecycle, paths, time values, and job descriptors fail explicitly.

## Platform

`Nexora::Core::platform` wraps the OS-specific primitives the rest of Core needs: hardware concurrency for sizing the job pool, and best-effort OS thread naming for profilers/debuggers. `JobSystem` names each worker `Nexora.WorkerN` through it. Naming is diagnostic only — a platform that can't honor it never fails the caller.

## Deferred work

This slice establishes the M1 contracts and smoke gates. A work-stealing scheduler, recurring system-graph caching, and C ABI wrappers remain follow-up M1 work rather than being represented by placeholder APIs. The opt-in mimalloc backend has only been exercised on Linux in this environment; Windows/macOS/Android/iOS builds with `NEXORA_ENABLE_MIMALLOC=ON` are unverified here.
