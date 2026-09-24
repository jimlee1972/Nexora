# Nexora Zig Showcase and Engine-owned Entry Point Roadmap

> Version: v1.0 | Status: planning baseline | Updated: 2026-09-21

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

- **ZS-M0 Contract:** host-owned lifecycle, function table, errors/memory/threads; one suite for a C++ fake and Zig smoke.
- **ZS-M1 Bootstrap:** C++ `NexoraShowcase` owns CLI, window/headless mode, discovery; Zig start/update/stop executes.
- **ZS-M2 API scene:** build scene, camera, mesh, input, and diagnostics solely through public C/Zig bindings.
- **ZS-M3 Feature gallery:** bounded physics, presentation, and streaming rooms with capability fallbacks.
- **ZS-M4 Reload/failure:** transactional reload, state migration, bad ABI rejection, and rollback.
- **ZS-M5 Distribution:** dynamic Development and static/packaged Shipping profiles with license/build/API manifests.

## 5. Evidence and completion

The engine must outlive the module. C++ and Zig consumers run identical ABI vectors. A headless scripted tour emits deterministic JSON; a native-window smoke proves real presentation, while screenshots are visual regression evidence rather than correctness oracles. Test missing symbols, version mismatch, callback errors, outstanding jobs, failed restore, device loss, and shutdown. Use host sanitizers, Zig safety checks, and allocation owner tags. Windows/DX12 is the first graphical gate; Linux headless remains the CI gate, and Vulkan/Metal advance only after execution on their hosts.

Done means `NexoraShowcase` starts from C++ `main` and Zig visibly drives the scene through public APIs, with headless, failure, reload, and Shipping evidence. Zig cannot directly access RHI/native windows or own editor UI. This document complements [`V1-Visual-Showcase-Long-Term-Plan.md`](V1-Visual-Showcase-Long-Term-Plan.md) by defining the Zig consumer and engine-owned entry point specifically.
