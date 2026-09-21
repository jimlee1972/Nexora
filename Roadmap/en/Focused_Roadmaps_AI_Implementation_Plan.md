# Engine API, Zig Showcase, and Editor: AI Implementation Technology and System Plan

> Version: v1.0 | Status: planning baseline | Updated: 2026-09-21

## 1. Analysis and critical path

These roadmaps must not be implemented by agents independently generating files. The API contract is upstream, the Showcase is the first external consumer, and the Editor is the high-complexity consumer. The critical path is **API conventions → Math/Text/VFS → C ABI/Zig binding → engine-owned Showcase → Editor shell/authoring → PIE/tools**. AI is useful for bounded implementation, adapters, test matrices, and synchronized documentation. Humans own ABI, UX, dependency, security, and release-status decisions.

## 2. AI construction system

Every work packet contains a roadmap ID, problem/non-goals, writable paths, prerequisite contracts, public API diff, ownership/lifetime/thread/error/determinism rules, acceptance commands, platform matrix, and rollback. Without a settled contract, an agent may produce only an ADR or prototype.

Use four logically separated roles: **Planner** creates a dependency DAG of review-sized changes; **Implementer** supplies code/tests/docs within scope; **Verifier** independently derives boundary, failure, concurrency, and ABI tests; **Reviewer** audits public surface, platform assumptions, security, performance, and status language. One model may time-slice roles, but their context, output, and evidence remain distinct. High-risk gates require a human owner.

Roadmaps/ADRs express intent, headers/schemas are the contract, tests encode executable behavior, and generated bindings must reproduce exactly. CI retains ABI manifests/diffs, reports, benchmark deltas, fuzz corpora, headless Showcase reports, and Editor migration goldens. Documentation cannot claim a platform not executed on its target host.

## 3. Techniques by roadmap

### Engine API

Inventory existing types through AST/schema analysis to prevent competing vectors/transforms. Validate the C header, Zig externs, ABI manifest, and reference docs from one canonical schema; generated files are not hand-edited. Use property/metamorphic tests for math, and fuzz invalid UTF-8, paths, parsers, descriptor sizes, handle generations, async cancellation, and shutdown. An ABI guardian compares symbols, calling convention, layout, enum values, and ownership annotations. Performance agents report measurements and confidence and cannot weaken correctness tolerances.

### Zig Showcase

Pair a C++ fake host with the Zig consumer using shared conformance vectors. A scenario DSL describes scripted input, expected events/counters, and required capabilities; headless assertions establish correctness and image comparison is supplemental. Chaos runs inject callback failures, outstanding jobs, N/N-1 state schemas, device loss, and shutdown races. Visual classification cannot alone certify rendering; GPU evidence and counters are required. Static checks reject Zig access to private platform/RHI headers and binary inspection confirms the C++ entry point.

### Editor

Derive state machines, command/undo models, and wireframes from user stories before widget code. Model-based tests generate long edit/undo/redo/save/reload/PIE traces and compare canonical scene state. Version synthetic small, medium, 100k-entity, corrupt, and old-schema projects. Visual regressions cover DPI/theme/locale, while focus, keyboard, and accessibility trees need structured assertions and human audits. Treat importers/plugins as untrusted inputs with isolation, limits, fuzzing, and human-owned signing/permission policy.

## 4. Waves and parallelism

| Wave | Safe parallel work | Exit/dependency rule |
| --- | --- | --- |
| 0 | inventory, ADRs, contract tables, fixtures | approve ABI conventions before bindings |
| 1 | Math, Text prototypes, VFS harness | merge shared error/allocator contract first |
| 2 | C ABI, Zig wrapper, fake host | pass ABI snapshot and cross-allocator gates |
| 3 | Showcase host, headless scenario, first scene | pass engine-owned lifecycle gate |
| 4 | Editor shell, document model, asset browser | no private Runtime fork |
| 5 | PIE, reload, prefabs, specialized pilots | pass isolation and transaction recovery |
| 6 | platform backends, performance, distribution | update support only from target-host evidence |

One owner controls each public header. Agents may parallelize independent tests/adapters but never invent conflicting ABI surfaces. Integrate at every wave exit rather than accumulating long branches.

## 5. Gates, risk, and human control

Low risk covers private refactors and additive tests/docs. Medium risk includes non-breaking APIs, minor serialization migrations, and UI workflows and needs owner review plus compatibility evidence. High risk includes ABI breaks, allocation/ownership, threads, filesystem sandboxing, importer/plugin execution, data migration, and release signing; it requires an ADR, threat model, two human approvals, and target-host evidence.

Mandatory gates are formatting/static analysis, unit/property/fuzz tests, sanitizers, ABI diff, deterministic serialization, module graph, and headless integration. GUI, GPU, and platform gates are recorded separately. Missing infrastructure is `NOT RUN`, never `PASS`.

## 6. Prompt/review protocol and metrics

Prompts cite the contract, list assumptions, ask for failure tests first, prohibit unapproved dependencies and STL/Zig slices across ABI, require bilingual docs, and report commands, target, results, and untested scope. Reviews ask: who allocates/frees; how long is a borrow valid; which thread; what happens on shutdown/reload/version mismatch; can data deterministically round-trip; and is missing capability explicit?

Measure lead time, first-review acceptance, escaped defects, flake rate, ABI breaks, fuzz findings, benchmark regressions, and AI-patch reverts—not generated lines. Stop automation when failures repeat, contracts churn, oracles are unreliable, or no target platform can validate the work. Return to ADRs, fixtures, or human prototypes; an agent may never relax a gate to pass its own patch.

## 7. Definition of done

The AI system is successful when each milestone is traceable to a contract, independently verified, reproducible, platform-evidenced, and rejectable/rollbackable by a human owner. The shared release gate is a stable public API, a Zig Showcase using only that surface, and an Editor with no private bypass.
