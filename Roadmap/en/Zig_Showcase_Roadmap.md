# Nexora Zig Showcase and Engine-owned Entry Point Roadmap

> Version: v1.0 | Status: planning baseline | Updated: 2026-09-21

> **Progress: 90%** (as of 2026-09-24; weighted acceptance checklist across the six milestones
> in section 4; completed items are marked with ✅ and the result is rounded down to 10%.)

**Implementation status (2026-09-24):** ZS-M0 through ZS-M4 are complete. ABI V3 now defines
explicit result and capability values, separate create/start/stop/destroy phases, fallible variable/fixed updates, and
transactional state migration in the C++ host. The in-tree C++ fake module exercises this contract.
The Zig module now owns independently allocated state obtained and released through paired V3 host
allocator callbacks, including reload candidates. The fake and Zig module now consume one vector
set; allocation tags, negative descriptor/callback coverage, and Linux sanitizer presets complete
ZS-M0. Runtime dynamic discovery and generation lifecycle are implemented; the Showcase builds and
selects a Zig Development shared library, while Shipping retains the same statically linked ABI.

**Local implementation status (2026-09-23):** Deterministic headless/static and
Development-dynamic ZS-M1 verification is now available as `NexoraShowcase.exe`. C++ owns `main`, engine lifecycle, a small
`GameWorld`, fixed/update scheduling, offscreen scene rendering, reload, and shutdown; the Zig
consumer mutates the primary entity Transform through the public ABI and the executable emits a
JSON evidence report. Dynamic module discovery is exercised by CTest. Native Win32/DX12 presentation
is implemented through the Window & Presentation contracts, but still requires target-host Windows acceptance.

## 1. Architectural decision

The Showcase `main`, platform window, engine lifecycle, render loop, and shutdown are owned by the C++ Host/Engine. Zig is a loaded gameplay module, never the process owner. It calls the stable C ABI to create content, consume tick/input, manipulate entities/components, and update presentation state. This makes Zig more than an ABI smoke while retaining that smoke gate.

```text
OS -> C++ NexoraShowcase main
   -> engine bootstrap/window/RHI/runtime
   -> load Zig gameplay module
   -> nx_game_query_api / nx_game_create
   -> on_start -> fixed_update/update -> on_stop
   -> engine render/present and final shutdown
```

## 2. Module contract

The Zig dynamic/static module exports fixed C symbols and a descriptor containing ABI version, structure size, capabilities, and callbacks. The engine supplies a versioned `NxEngineApi` table and opaque context. It owns worlds, assets, window, GPU, thread pool, and callback invocation. Zig owns and destroys its gameplay state.

No exception crosses the ABI; callbacks return `NxResult`. API pointers live only for the module lifetime and borrowed frame data cannot be cached. Reload first quiesces jobs, then migrates a versioned state blob transactionally and rolls back on failure. Zig submits high-level draw requests, never native device pointers. Shipping may statically link while preserving the same contract; Development favors dynamic reload.

## 3. Showcase content

The first playable slice is a small 3D gallery. Zig spawns a camera, rotating cubes, lights, and materials; consumes input snapshots; raycasts to select objects; and displays transform/entity/asset/frame diagnostics. Bounded physics/character and large-world areas follow. Rendering and swapchain creation remain engine responsibilities.

| Room | Public APIs called by Zig | Visible evidence |
| --- | --- | --- |
| Math Lab | vectors, quaternion, transform, ray/AABB | gizmos, values, property-test state |
| Scene Lab | entity/component, scene, asset | spawn/despawn, reload, generation |
| Gameplay Lab | input, fixed tick, physics/navigation | controllable actor and query trace |
| Presentation Lab | animation/audio/VFX facade | counters and backend/fallback badge |
| Streaming Lab | cell requests and residency queries | cell/HLOD overlay |

UI distinguishes `IMPLEMENTED`, `CONTRACT ONLY`, and `UNAVAILABLE`; placeholders never impersonate backends.

## 4. Milestones

- **✅ ZS-M0 Contract:** host-owned lifecycle, function table, errors/memory/threads; one suite for a C++ fake and Zig smoke.
  - ✅ ABI V3 lifecycle, result/capability values, paired allocator, and transactional state migration.
  - ✅ C++ fake-module lifecycle and reload contract coverage.
  - ✅ One shared conformance-vector set for the C++ fake and Zig consumer.
  - ✅ Owner tags, missing-symbol/version/structure-size/callback failures, and Linux sanitizer gates.
- **✅ ZS-M1 Bootstrap:** C++ `NexoraShowcase` owns CLI, window/headless mode, discovery; Zig start/update/stop executes.
  - ✅ C++-owned `main`, Engine/World lifetime, fixed/update scheduling, and ordered shutdown.
  - ✅ Deterministic headless validation backend, JSON evidence, and static Zig-object consumer.
  - ✅ Runtime dynamic-library discovery, generation ownership, job quiescence, and real library unload/rollback.
  - ✅ Build and select the Zig Development shared-library artifact from `NexoraShowcase`.
  - ✅ Consume native window/input/swapchain through the Window & Presentation boundary.
- **✅ ZS-M2 API scene:** build scene, camera, mesh, input, and diagnostics solely through public C/Zig bindings.
  - ✅ Zig creates the scene, camera, light, and cubes through the public host table; C++ retains engine/world/render ownership.
  - ✅ Append-only spawn/despawn, scene, input snapshot, opaque asset handle, raycast, high-level debug draw, and diagnostics callbacks.
  - ✅ C header, ABI manifest/baseline, Zig binding, ownership/thread/error contract, C++ ABI gates, Zig smoke, and deterministic headless evidence are synchronized.
- **✅ ZS-M3 Feature gallery:** bounded physics, presentation, and streaming rooms with capability fallbacks.
  - ✅ Scriptable room selection and deterministic capability/fallback evidence for the Math, Scene,
    Gameplay, Presentation, and Streaming rooms.
  - ✅ Native interaction adds WASD camera movement while the public Zig scene callback performs selection raycasts and high-level query traces.
  - ✅ Physics/navigation, animation/audio/VFX, and cell/HLOD room overlays derive their state from compiled runtime capabilities.
  - ✅ Every room exposes `IMPLEMENTED` / `CONTRACT ONLY` / `UNAVAILABLE`, and the minimal-capability CTest locks the fallback matrix.
- **✅ ZS-M4 Reload/failure:** transactional reload, state migration, bad ABI rejection, and rollback.
  - ✅ Job drain, restore failure, old-generation rollback, shutdown-during-reload, and repeated-reload stress for real dynamic generations.
  - ✅ Dynamic library replacement waits for stable size/write-time samples before loading. The host records generation-scoped update/fixed-update failures, and Presentation maps device loss to explicit device recreation rather than surface retry.
- **ZS-M5 Distribution:** dynamic Development and static/packaged Shipping profiles with license/build/API manifests.
  - ✅ Reproducible Development-dynamic and Shipping-static package targets emit license,
    build/API/content manifests, per-artifact SHA-256 digests, and `SHA256SUMS`.
  - Open: retain launch evidence from the generated command on a clean target machine. Package
    construction in Linux CI does not substitute for target-host acceptance.
  - ✅ Linux clean-package evidence verifies all checksums, stages a fresh isolated copy, launches
    the relocatable dynamic package from that copy, and retains its report/exit status. Other target
    hosts and independently provisioned-machine acceptance remain open.

## 5. Evidence and completion

The engine must outlive the module. C++ and Zig consumers run identical ABI vectors. A headless scripted tour emits deterministic JSON; a native-window smoke proves real presentation, while screenshots are visual regression evidence rather than correctness oracles. Test missing symbols, version mismatch, callback errors, outstanding jobs, failed restore, device loss, and shutdown. Use host sanitizers, Zig safety checks, and allocation owner tags. Windows/DX12 is the first graphical gate; Linux headless remains the CI gate, and Vulkan/Metal advance only after execution on their hosts.

Done means `NexoraShowcase` starts from C++ `main` and Zig visibly drives the scene through public APIs, with headless, failure, reload, and Shipping evidence. Zig cannot directly access RHI/native windows or own editor UI. This document complements [`V1-Visual-Showcase-Long-Term-Plan.md`](V1-Visual-Showcase-Long-Term-Plan.md) by defining the Zig consumer and engine-owned entry point specifically.
