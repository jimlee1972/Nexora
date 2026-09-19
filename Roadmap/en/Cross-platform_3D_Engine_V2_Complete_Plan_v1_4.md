# Cross-platform 3D Engine — V2 Complete Plan

**English edition:** v1.4

**Source:** [`../zh-TW/跨平台3D_Engine_V2_完整規劃書_v1_4.md`](../zh-TW/跨平台3D_Engine_V2_完整規劃書_v1_4.md)

**Status:** Normative planning baseline

## Purpose and positioning

V2 defines production-scale GPU-driven and connected-world capabilities. It covers GPUScene, GPU culling, temporal upscaling, virtual texturing, adaptive world partitioning, networking, advanced navigation/AI/animation/UI, distributed builds, and LiveOps. The document is an architecture and delivery contract: implementations may evolve internally, but public data, ABI, ownership, threading, serialization, and compatibility rules must remain explicit and testable.

## Unchanging architecture principles

- Keep the C++20 engine core independent from gameplay-language details.
- Expose gameplay through a language-neutral, versioned C ABI; Zig is the primary gameplay language.
- Pass plain data, handles, spans, callbacks, and allocator interfaces across module boundaries. Do not pass STL containers, exceptions, RTTI objects, or ownership-ambiguous pointers.
- Use generational runtime handles and persistent UUIDs for distinct purposes. Stale handles must fail safely.
- Keep authoring objects separate from hot-loop storage and render extraction.
- Make platform backends, optional subsystems, plugins, and third-party SDKs replaceable and removable.
- Treat schemas, manifests, generated metadata, and serialized formats as versioned contracts.
- Require reproducible command-line builds and automated validation before merging.

## Scope

### Runtime and data

The runtime owns lifecycle, jobs, task graphs, time, events, memory, world state, transforms, component storage, reflection, serialization, and resource handles. Structural world changes are deferred through command buffers. Runtime state is never silently treated as authoring state.

### World and streaming

Scenes have explicit load, activation, deactivation, and unload states. Persistent identity survives streaming while runtime identity does not. Cross-scene references, partition cells, HLOD residency, streaming priorities, and failure recovery must be deterministic and observable.

### Rendering

A unified RHI covers DX12, Vulkan, and Metal. RenderGraph declares resource use, lifetime, queue ownership, and barriers. Shader reflection is the source of binding metadata. Materials select explicit shading models and bounded variants. content-addressable DDC, worker isolation, dedicated servers, replication/interest/prediction/replay, and remote diagnostics.

### Content and build pipeline

Assets use stable UUIDs, import settings, dependency graphs, derived-data keys, bundles, manifests, and atomic publication. Cooking is deterministic and platform-aware. Invalid dependencies, schema drift, stale generated data, and unsupported feature combinations fail in CI rather than at runtime.

### Gameplay-facing systems

Input, UI, localization, physics, characters, navigation, AI, animation, audio, media, saves, replay, and diagnostics expose narrow APIs. Simulation-authoritative data is separated from cosmetic data. Expensive work is scheduled, budgeted, and profiled.

### Editor and tools

The editor consumes public engine services instead of bypassing ownership rules. Undo/redo uses transactions. Play mode has an explicit world boundary. Inspectors, profilers, commandlets, device tools, import workers, and build workers must also operate headlessly where required.

### Plugins and optional features

Foundation modules cannot depend on optional high-level modules. Plugins declare capabilities, versions, dependencies, build variants, runtime cost, and trust requirements. Development builds may be modular; shipping builds may be monolithic without changing observable contracts.

## Memory, threading, and errors

- Allocation ownership is explicit at every API and module boundary.
- Frame, scratch, streaming, persistent, GPU, and tool memory are accounted separately.
- Thread affinity is documented; jobs declare reads, writes, dependencies, cancellation, and completion.
- Rendering, IO, audio, networking, and platform callbacks never mutate world state without synchronization or a queued command.
- Expected failures return structured errors. Assertions represent programmer invariants, not recoverable content or device failures.

## Compatibility and serialization

Every persistent format carries a schema version and migration policy. Unknown required fields, invalid numbers, broken references, dependency cycles, and incompatible versions produce actionable diagnostics. Hot reload occurs only at a module-safe barrier and invalidates cached generations safely.

## Security and privacy

Credentials and signing material remain outside the repository. Remote content, plugins, WebView bridges, network input, and downloaded artifacts are authenticated and constrained by explicit trust policies. Telemetry is opt-in/configurable and excludes secrets and unnecessary personal data.

## Delivery milestones

The implementation sequence is: V1 migration; reflection/DDC tooling; GPUScene; GPU-driven rendering; large worlds; networking; AI and animation; runtime features; distributed build and production hardening. Each milestone must identify owners, inputs, outputs, invariants, dependencies, tests, performance budgets, migration impact, and rollback strategy.

## Required validation

- Configure and build all supported profiles from clean presets.
- Run unit, integration, serialization round-trip, migration, fuzz, stress, and platform tests.
- Validate ABI layouts and exported symbols.
- Validate RenderGraph hazards, resource lifetime, shader bindings, and golden images.
- Validate deterministic cooks, package manifests, dependency DAGs, and reproducible outputs.
- Exercise device loss, cancellation, partial downloads, stale handles, corrupt content, and low-memory paths.
- Record CPU, GPU, memory, IO, network, and build-time budgets without unexplained regression.

## Definition of done

V2 is complete only when its advertised profiles build reproducibly, reference projects exercise the contracted scope, CI gates are mandatory, compatibility and migration are documented, unsupported capabilities degrade predictably, and shipping packages contain no undeclared tools, credentials, diagnostics, or optional modules.

## Final principle

Prefer explicit ownership, stable contracts, deterministic pipelines, measurable budgets, replaceable backends, and recoverable failure modes over hidden convenience or speculative coupling.
