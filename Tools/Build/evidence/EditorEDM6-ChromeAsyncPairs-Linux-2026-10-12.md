# ED-M6 Chrome nestable async duration import — Linux, 2026-10-12

## Scope and configuration

Beads task: `nexora-62u.2.7`. Base: `b7ce693cf77f24d6a87e087864ac03d6c2b60398`.
Linux Development Modular, GCC 14.2, Dear ImGui enabled, native X11/Vulkan tests under
Xvfb with Mesa lavapipe. This is cloud contract/input evidence, not physical display/GPU
calibration or complete ED-M6 acceptance. Overall Editor graphical acceptance remains 0/8.

The Profiler imports nestable async `b/e` using explicit process-local `id2.local` with
category/scope identity and same-process cross-thread ends. Selection refers to the begin
thread/name. Each identity is LIFO; independent identities can complete in any order.
The importer retains external microsecond starts and inclusive elapsed milliseconds, without
inferring CPU execution, GPU time, RSS, exclusive time or a call tree.

Legacy `id` and `id2.global` denote global IDs, so they are deliberately rejected for recognized
b/e on the selected process. Legacy S/T/F and instant n are validated/skipped, not paired.
Numeric/string local IDs remain distinct; only nonnegative exact numeric IDs through 2^53-1
or nonempty UTF-8 strings up to 256 bytes are supported. Category/scope are optional bounded
strings. Name/process/thread/timestamp are required on both endpoints. Unknown metadata
on existing X/B/E is still validated/skipped, preserving prior admission compatibility.

## Executed contracts

The existing `editor.chrome_cpu_trace` fixture exercises:

- Cross-thread ends, out-of-order independent-ID completion and mixed X/b/e begin retention.
- Same-ID same-name nesting, nonselected name/thread nesting, ID type/category/scope separation,
  Unicode escaped/raw equivalence, zero/fractional elapsed durations and omitted category/scope.
- Missing/unbalanced/wrong-ID/name endpoints, negative/backward timestamps, invalid ID types,
  oversized IDs, absent/malformed/global/mixed identity objects and malformed endpoint fields.
- A pending begin evicted by 605 newer X intervals cannot overwrite the newer ring occupant;
  newest600 sequences and six dropped starts remain correct.
- Exact 128-open async admission plus shared synchronous/async overflow rejection. Existing
  4 MiB/32,768-event/600-sample/JSON-depth/string bounds and synchronous cases remain exercised.
- Actual read-only workspace file import preserves source bytes and prior owning captures;
  recovery, external changes, missing/oversized/nonregular/linked sources still reject.

`editor.chrome_trace_display` clicks actual 1x/2x controls, reads a real selected-project
mixed X/B/E/b/e file with a cross-thread async end and publishes an owning static snapshot.
It verifies inclusive 1.25 ms, zero X and 0.5 ms async intervals. Invalid publication,
live Clear, independent static captures, read-only requests, close modal, project switch
and detached scope preserve or revoke state under the existing contract.

## Validation

Configure and build passed. The first complete CTest run finished **226/227** in 711.58 s:
`editor.linux_scene_files` timed out its final invalid-startup Main edit/save assertion (49.78 s).
No test was skipped; both Chrome tests passed. The unchanged native scene workflow then passed
`ctest --preset linux-development --rerun-failed --output-on-failure` **1/1** in 39.75 s.
This run alone does not establish the original failure's cause. The complete suite is repeated
before merge; its final results and fresh hosted checks are tracked in the PR and Beads. Documentation routing tests passed
16/16, all five changed Markdown/link/bilingual checks and all five touched C++ format checks passed.
No linkage boundary, persisted schema or stable C/Gameplay ABI changed. Root README is unchanged.
Global async IDs, legacy async pairing, other capture formats and physical calibration remain open.

## Source identities

- `Engine/Editor/src/ChromeTrace.cpp`: `ddedabcae8b53fa99b2016b99b7875534e077c0f3b907d6d8fe776b691911fe9`
- `Tests/Editor/ChromeTraceTests.cpp`: `dc6b61a882eeea14dea7e6de01d3ffb01699e58387d71711f9e532dd39d81ebf`
- `Tests/EditorImGui/ChromeTraceDisplayTests.cpp`: `35d3c2c00d8c5f9bd76b200922a8edd840ee2f3ec94a284070a71bdfb3a09366`
