# Nexora Core runtime contract (V1-M1)

`NexoraCore` owns process-level runtime services. It depends only on `NexoraFoundation`; higher-level world, rendering, asset, editor, and gameplay modules must depend on Core rather than the reverse.

## Public I/O and services (API-M3/M4)

`VirtualFileSystem` accepts `mount/path` and `mount://path` URIs. Canonicalization rejects absolute paths, backslashes, `..`, and symlink escapes; mount-root enumeration is supported while root writes are rejected. Directory, memory, read-only package-adapter, and Runtime bundle backends share reads, metadata, lexically sorted enumeration, and a consistent virtual namespace. `WriteAtomic` creates parents and publishes a complete replacement; package mounts reject writes.

API-M3 exposes whole and 64-bit ranged reads, positional `FileStream` reads, and `MappedFile`. Host files use the operating system's read-only mapping facility; memory and package mounts return an owned immutable snapshot. Async requests accept priority, deadline, offset, size, and alignment options. They run on a `JobSystem` worker, check cancellation before and after I/O, and publish an owned snapshot through `AsyncReadHandle`; identical in-flight requests share one underlying read while per-handle cancellation remains isolated. `Watch` records a metadata snapshot and `PollWatches` synchronously emits created/modified/removed callbacks on the polling thread. `Unwatch` prevents later polls, but does not cancel a callback already copied by a concurrent poll.

In Shipping, public `Mount` rejects arbitrary host roots. Engine bootstrap alone uses the private trusted host-mount path for configured `content://` and the standard-library `temp://` directory. `MountMemory` and validated read-only `MountPackage` remain available because they cannot escape to arbitrary host paths. Platform adapters populate `MountPackage`; Runtime's `MountBundle` validates a bundle and populates a memory mount. Mount names are caller-chosen.

`Services.h` exposes steady-clock nanoseconds, deterministic versioned PCG random streams, thread-safe configuration, and a `ProfilingMarker` scoped timer alongside fixed game time, structured logging, jobs, and events. `kEngineServicesApiVersion` versions this C++ service surface; random checkpoints also carry their algorithm version and reject incompatible restores. Simulation uses `FixedTickClock`, not wall time. Event callbacks execute synchronously on the publishing thread; deferred callbacks execute on the thread calling `DispatchDeferred`. Callbacks may publish or unsubscribe reentrantly because the callback list is snapshotted before invocation. An unsubscribe cannot revoke a callback already copied by a concurrent publish, so captured state must outlive joined publishers. Profiling callbacks execute synchronously in the marker destructor; marker names and sink-owned data need only remain valid for that call. Profiling sinks are configured at startup and are not changed concurrently with marker traffic.

Typical success paths are `RandomStream replay; replay.Restore(stream.Save())`, checking the optional returned by `Configuration::Get`, waiting for a submitted `JobHandle`, and retaining an `EventBus` subscription until publishers have joined. Explicit failure paths include `RandomStream::Restore` returning `false` for a different algorithm version, `Configuration::Set` returning `false` for an empty key, a job reaching `Failed` and rethrowing its captured callback exception from `Wait`, and `Unsubscribe` returning `false` for a stale handle. Jobs own their callback captures through terminal status, run on worker threads, capture exceptions, and observe cancellation before execution. `AsyncLogService` takes ownership of submitted strings; `Flush` waits for already accepted records and `Stop` drains them.

## Bounded asynchronous log observation

`AsyncLogService` owns compact, NUL-free valid UTF-8 category/message strings. Category and message
limits are 256 bytes and 16 KiB, inclusive; empty strings remain valid. Pending and crash-ring
capacities clamp to 4,096 records each. The ring must be nonzero; pending zero disables admission.
Invalid levels/text, full or disabled pending ingress, stopped traffic and exhausted sequence space
reject without evicting accepted pending records, and increment a saturating rejection count.
Embeddings use `ReportRejected` for raw wire data rejected before allocating an owning record.
`Write` accepts owning strings, so callers budget the allocation before its admission. Accepted
strings release excessive producer reserve. The maximum pending/ring text payload is bounded by
both capacities; caller-owned snapshot copies have their own lifetime and memory cost.

Accepted records receive monotonic sequence IDs starting at one. UINT64_MAX is accepted once and
IDs are never reused, including across Start/Stop. `SnapshotSince(cursor)` copies consumed retained
records newer than the cursor, plus consumed/rejected watermarks, without flushing or waiting for
new producer traffic. A gap between an observer cursor and the next retained accepted sequence
identifies unread ring eviction; eviction of already-observed records is not additional observer
loss. `CrashRingSnapshot` retains its owning compatibility view. The record/header additions require
C++ consumers to rebuild; stable C/Gameplay ABI layouts stay unchanged.

`Flush` waits for the accepted watermark present at its call, rather than for ongoing producer
traffic to stop. Stop drains accepted records, joins the worker, publishes stopped state under the
same mutex as Write and releases pending deque storage. The bounded crash ring and sequence/rejection
watermarks survive restart. Owner-thread lifecycle calls are serialized; Write, ReportRejected,
Flush and snapshots support concurrent producers/readers. All callers finish before destruction.
No UI callbacks, Runtime/Editor references or higher-module dependencies are stored in Core.

## Lifecycle

`Engine::Initialize` starts logging, workers, and the `content` and `temp` VFS mounts. `Engine::Shutdown` drains jobs before destroying VFS state, then drains logging. Both shutdown and destruction are idempotent. `EngineServices` is a non-owning view valid only between initialization and shutdown.

## Threading and lifetime

- `JobSystem`, `AsyncLogService`, `TrackingAllocator`, and `VirtualFileSystem` support concurrent producers.
- `JobSystem::Stop` joins every worker before clearing thread objects and publishes its stopped
  lifecycle state under the scheduler mutex. Worker entry points capture the stable implementation
  allocation rather than the public wrapper. After draining and joining, `Stop` releases the worker
  closures and the queue's retained storage instead of deferring those allocations to wrapper
  destruction. Repeated start/stop therefore cannot retain worker-lifecycle state past shutdown.
- `EventBus` snapshots callbacks before invoking them, so callbacks run without its lock held.
- `FrameArena` is single-owner. Its allocations are invalid after `Reset`; callers must complete dependent jobs before `Engine::BeginFrame`.
- `AsyncReadHandle::Get` is a snapshot and never blocks. `Pending` means the caller should poll or schedule later work.

## Memory and errors

The tracking allocator's aligned-allocate/deallocate calls are the replaceable backend seam: by default they go through `::operator new`/`::operator delete`, and with `-DNEXORA_ENABLE_MIMALLOC=ON` they go through `mi_malloc_aligned`/`mi_free` instead (mimalloc is fetched via CMake `FetchContent`, off by default, built as a static library and never exposed in a public Core header). Either way, live/peak bytes and per-tag totals are tracked the same; allocator objects and exceptions never cross a C ABI. Frame arena exhaustion reports `std::bad_alloc`. Invalid lifecycle, paths, time values, and job descriptors fail explicitly.

## Platform

`CurrentProcessResidentBytes()` in `ProcessMemory.h` synchronously returns an optional current
process resident-set / working-set byte count. Linux reads at most 256 bytes from `/proc/self/statm`,
parses complete decimal page counts, and checks multiplication by the native page size for overflow.
Windows uses `K32GetProcessMemoryInfo().WorkingSetSize`; macOS uses
`task_info(MACH_TASK_BASIC_INFO).resident_size`. Unreadable/invalid OS data and unsupported hosts
return unavailable. This process-wide observation includes shared resident pages; it is neither
GPU memory, TrackingAllocator accounting, nor an operating-system lifetime peak. The value owns no
OS handle or memory borrow. Calls hold no shared state, and the caller controls sampling frequency.
Linux native allocation/touch evidence is covered by `core.process_memory`; Windows/macOS API
branches require their own CI/host validation and were not run in the Linux cloud.

`Nexora::Core::platform` wraps the OS-specific primitives the rest of Core needs: hardware concurrency for sizing the job pool, and best-effort OS thread naming for profilers/debuggers. `JobSystem` names each worker `Nexora.WorkerN` through it. Naming is diagnostic only — a platform that can't honor it never fails the caller.

## Deferred work

This slice establishes the M1 contracts and smoke gates. A work-stealing scheduler, recurring system-graph caching, and C ABI wrappers remain follow-up M1 work rather than being represented by placeholder APIs. The opt-in mimalloc backend has only been exercised on Linux in this environment; Windows/macOS/Android/iOS builds with `NEXORA_ENABLE_MIMALLOC=ON` are unverified here.

## V2-M11 remote diagnostics portable foundation

`RemoteDiagnostics.h` defines a backend-neutral, versioned binary wire contract in Core so desktop,
server, Android, and iOS adapters can share one protocol without introducing Renderer or Runtime
dependencies. A `TraceId` flows through CPU, IO, cook-artifact, GPU-upload, network, and memory
records. Trace and metric decoders reject bad magic/version, truncation, invalid span timing, and
trailing bytes before publishing an owning value.

`TraceAggregator` is synchronous and caller-owned. It isolates trace IDs, rejects duplicate span
IDs, orders events deterministically, correlates an IO → cook artifact → GPU upload chain only when
the resource identity is consistent, and attributes span time, bytes, GPU time, resident memory,
IO, and network bytes to `PluginID`. The `core.v2_m11_remote_diagnostics` headless gate covers
wire round-trips, malformed packets, correlation, resource isolation, and plugin-cost attribution.
Remote inspector/profiler transports, authentication, device discovery, and Android/iOS/Desktop/
Server host adapters remain target-host gates; this Linux portable layer does not claim remote
observation on those devices.
