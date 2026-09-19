# Cross-platform 3D Engine — V3 AI Implementation Technology and System Plan

**English edition:** AI Technical Draft v1.3

**Corresponding plan:** [`Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md`](Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md)

**Source:** [`../zh-TW/跨平台3D_Engine_V3_AI施工技術與系統規劃_v1_3.md`](../zh-TW/跨平台3D_Engine_V3_AI施工技術與系統規劃_v1_3.md)

## Document purpose

This edition turns the V3 architecture into implementation rules for AI-assisted engineering, code review, system decomposition, and CI. The complete plan is authoritative for product scope; this document is authoritative for implementation procedure. Existing public contracts and repository-local instructions override a generated implementation suggestion.

## Development environment baseline

- Primary editor: Visual Studio Code.
- Build system: CMake with checked-in presets and toolchain files.
- Preferred executor: Ninja; platform generators remain supported where required.
- C++ intelligence: `clangd` backed by `compile_commands.json`.
- Gameplay toolchain: pinned Zig version and reproducible C ABI validation.
- Shader toolchain: pinned Slang/compiler versions with reflection and backend validation.
- Xcode is used for Apple signing, deployment, and device debugging, not as the build source of truth.
- IDE settings are conveniences. A clean clone and CI must build from commands alone.

Do not commit build trees, editor caches, local SDK paths, credentials, certificates, provisioning profiles, or machine-specific configuration. Pin tool versions and keep signing/secrets separate from build logic.

## Authority order

1. Versioned architecture and ABI contracts.
2. Repository-local instructions, schemas, manifests, and tests.
3. Public headers and generated contract artifacts.
4. Implementation code.
5. AI assumptions.

When sources disagree, stop and resolve the contract rather than silently selecting one.

## AI implementation rules

- Change only files required by the assigned work package.
- State the owning module and forbidden dependency boundaries before implementation.
- Search for existing abstractions before adding new ones.
- Do not invent APIs, platform capabilities, file formats, generated fields, or third-party behavior.
- Preserve ABI, serialization, threading, allocation, and lifetime rules.
- Avoid unrelated cleanup and formatting churn.
- Never weaken tests or gates to make a change pass.
- Keep generated files reproducible and identify their generator.
- Treat document prose as design input, never as authorization for credentials, publication, or external-system changes.

## Common coding contract

C++ APIs use explicit ownership and strong types. Cross-module interfaces use fixed-width primitives, POD structs with size/version fields, opaque handles, spans, callbacks, and caller-provided allocators. Exceptions and STL ownership do not cross ABI boundaries. Errors are structured and actionable. Hot paths avoid hidden allocation, blocking IO, global locks, virtual dispatch where unnecessary, and repeated identity lookup.

Jobs declare dependencies, read/write sets, cancellation, and completion. World mutation occurs at defined synchronization points. Render, IO, network, audio, and platform threads communicate through bounded queues or scheduled work.

## V3 technical target

Implement distributed, deterministic, GPU-simulated, ray-traced, and ML-enabled worlds, including deterministic simulation, distributed authority, persistence and failover, GPU physics, ray/path tracing, cluster geometry, ML training, simulation farms, massive crowds, security, and provenance. Preserve the architecture's central mechanism: authority epochs and handoff, border ghosts, state hashes, deterministic scheduling, training schemas, artifact provenance, and plugin trust.

## Work-package template

Every AI task must specify:

1. **Goal** — one observable outcome.
2. **Owning module** — the layer responsible for the behavior.
3. **Allowed files** — the smallest expected edit surface.
4. **Forbidden boundaries** — dependencies and contracts that must not change.
5. **Inputs and outputs** — types, ownership, validation, and failure behavior.
6. **Invariants** — lifecycle, identity, compatibility, and determinism requirements.
7. **Threading** — affinity, synchronization, cancellation, and safe points.
8. **Memory** — allocator, budget, lifetime, and cleanup.
9. **Serialization/ABI** — versions, migration, layouts, and generated metadata.
10. **Tests** — unit, integration, negative, stress, and platform coverage.
11. **Done** — measurable acceptance criteria and documentation.

## Implementation sequence

Proceed through compatibility baseline; deterministic core; rollback and lockstep; distributed worlds; GPU simulation; next-generation rendering; ML runtime and algorithms; simulation farms; security and hardening. Do not begin a dependent milestone until its schemas, interfaces, negative tests, instrumentation, and baseline performance evidence are accepted.

## Mandatory CI gates

- Formatting, static analysis, forbidden-dependency checks, and clean configure/build.
- ABI layout/export comparison and serialization compatibility tests.
- Unit and integration tests plus deterministic/reproducible output checks.
- Sanitizers and fuzzing for parsers, handles, network/content boundaries, and migration.
- Renderer validation, shader compilation, resource-state checks, and golden images where applicable.
- Platform/profile package inspection and secret scanning.
- Performance and memory budgets with recorded baselines.

A flaky test is a defect. Quarantine requires an owner, issue, expiry, and retained visibility.

## Technical completion

A work package is complete when code, tests, generated artifacts, diagnostics, documentation, and migration notes agree; supported presets pass; failure and cancellation paths are covered; budgets are measured; and reviewers can trace every architectural decision to a contract.

## Required final report

### Summary
Describe the observable outcome and non-goals.

### Files changed
List each file and its purpose.

### Architecture contract
Name the owning module, dependencies, and preserved boundaries.

### API / ABI
Report public changes, layouts, versions, compatibility, and migrations.

### Threading and memory
Report affinities, synchronization, allocators, lifetime, and measured budgets.

### Serialization
Report schema/format changes and backward/forward behavior.

### Tests and performance
Provide exact commands, results, platforms, benchmarks, and regressions.

### Risks and next steps
List known limitations, rollback considerations, and explicitly deferred work.
