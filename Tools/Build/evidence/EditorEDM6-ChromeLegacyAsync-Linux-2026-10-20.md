# ED-M6 legacy Chrome async intervals — Linux, 2026-10-20

Beads: `nexora-gu7.33.1`, under M6-04 `nexora-gu7.33` and execution epic `nexora-gu7`.
Base: `b8c31d72437a77cc190ad49eb70cf97b6c23fac6` (PR #662).

## Delivered behavior

The existing production importer now recognizes legacy async S/F whole intervals. It reuses
bounded typed ID/category/scope decoding, begin process/thread/name selection, global id/id2.global
aliases and selected-process id2.local admission. Legacy identities also include exact names and
live starts cannot nest. Independent names/IDs can finish in any order; legacy and nestable domains
cannot close one another. Global ends can cross processes/threads. Duplicate live starts, unmatched,
backward, unclosed, ambiguous, malformed or over-budget participating records reject the whole
import without changing source/prior capture/live measurements. T/p steps remain opaque metadata.

The bounded subset deliberately preserves the importer's typed numeric/string ID and category
separation rather than coercing IDs or repairing broken traces. For comparison, upstream Catapult
[legacy interval pairing](https://github.com/catapult-project/catapult/blob/master/tracing/tracing/extras/importer/trace_event_importer.html)
uses independent named, non-nestable async intervals, and its
[phase constants](https://github.com/catapult-project/catapult/blob/master/tracing/tracing/model/constants.html)
do not place S/F legacy ids in the process-local phase set. The implementation is original; these
references informed the pairing model, not copied source.

All scopes share 128 live entries; at most 4 MiB/32,768 events enter one import. Newest600 retention
follows selected begin order. Completion checks the sample sequence before writing, so an evicted
begin cannot corrupt a newer interval. Samples retain external microsecond starts and millisecond
inclusive elapsed latency. No CPU execution, self/exclusive time, call tree, GPU timing or memory
measurement is inferred. No persisted schema, public type layout, C ABI, dependencies or linkage
boundary changes. Earlier unsupported S/F records now validate/pair and can reject an invalid trace.
Other phases and existing X/B/E/b/e contracts retain their behavior.

## Verification

The new regression first failed against the old implementation: `editor.chrome_cpu_trace`,
0.20 s, with the legacy cross-process/begin-order assertion. After the implementation, focused
Core/project IO and real 1x/2x ImGui control tests passed 2/2 in 0.50 s (Core 0.43 s, UI 0.06 s).
The final full build includes additional balanced overflow cases added after this focused run.

Coverage includes independent cross-process completion, local/global and legacy/nestable
separation, names, numeric/string IDs, category/scope lanes, decoded Unicode, fractional/zero
durations, completed identity reuse, foreign begin selection and opaque steps. Every truncated
mixed trace rejects. Exact128 accepts; balanced129 legacy, mixed nestable and synchronous
candidates reject, avoiding missing-end false positives. A pending begin evicted by605 X records
preserves all newer durations and six drops.

Actual project files import through a read-only workspace without changing bytes; malformed
imports preserve prior owning captures. Actual Profiler import controls at1x/2x load mixed
X/B/E/b/e/S/F files and retain a five-interval owning static view. A failed unclosed-S reload
preserves that view and live metrics. Existing tests cover modal/read-only/project replacement,
independent static imports, scope revocation and source alias/recovery policies.

Final local acceptance used GCC14.2, Development Modular with graphical shell/Editor enabled,
Slang and Zig gameplay OFF. These exact commands passed:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

The full serial Development suite registered286 tests:274 actually passed,12 existing native
feature/environment skips, zero failures,542.81 s. Final Core/project-IO Chrome tests passed in0.27 s
and actual1x/2x controls in0.03 s. Existing deep hierarchy59.87 s, serial100k lifecycle198.68 s and
overview pose snapshot39.65 s passed. The12 skips are four window Vulkan scene/PBR/upload/swapchain,
seven renderer native Vulkan recording/descriptor/timing/benchmark cases, and material-scene native;
they are not claimed as executed native acceptance. Shipping Monolithic configured/built43 steps
with Editor/testing excluded. Documentation-routing fixtures passed16/16; changed Markdown links,
touched C++ formatting and git diff whitespace checks passed. Source/test digests stayed unchanged
through the full gate. No Windows/macOS/physical-host validation was executed locally.

Physical display/GPU calibration, other capture formats and self/exclusive time are not accepted
by this supporting slice. ED-M0 through ED-M7 milestone acceptance remains0/8.
