# ED-M6 external synchronous self elapsed — Linux, 2026-10-20

Beads: `nexora-gu7.33.2`, under M6-04 `nexora-gu7.33` and execution epic `nexora-gu7`.
Delivery base: main `6bed775d80bab8a4766293eed8b9db9fb52d835d`, tree
`3043f7902bff2657f04eafb97cd20dfcb858d9b5`. Prerequisite PR665 merged as
`43a0b289e66867c67b9ba406af43355a3223a8bf`; PR666 merged as this delivery base.
Hosted self-time CI remains pending; full local product results follow.

## Contract

An optional C++ self-duration field represents external elapsed interval time outside observed
selected-thread synchronous children. The importer includes nonselected X/B/E names as context,
subtracts each immediate child's raw duration once and reconstructs iteratively. Relative starts
and raw durations avoid rounding tiny durations away at large absolute timestamps. B/E source
begin order disambiguates coincident paired bounds. Async records, crossing/ambiguous coincident
components and invalid coverage keep inclusive values and leave self unavailable. Independent
components remain available. Available zero is distinct from unavailable.

Context sorting never changes begin-ordered selected samples, newest600 loss counts or sequence
identity checks when writing a retained ring slot. The32768-event/4MiB input bounds also bound the
additional O(events) records/iterative stack; there is no recursive ancestry traversal. Context is
temporary. No whole-frame, total allocator/RSS/GPU or physical budget is implied.

Public C++ consumers rebuild for the appended optional field. Old three-field interval
initialization stays source-compatible and defaults to unavailable. Stable C/Gameplay ABI,
persisted files, input JSON schemas, other profiler capture schemas and module dependencies stay
unchanged. No CPU execution, scheduling, call tree or GPU timing is inferred. Previously supported
inclusive input acceptance is preserved, including ambiguous/overlapping traces.

## Verification

The candidate Core importer was compiled directly with GCC14.2/C++20 against unchanged Foundation;
the full existing Core tests and new self-time cases passed. Project IO was deliberately excluded
from this standalone link because the product DLL's older interval layout must not be mixed with
the new C++ capture ABI. This is preliminary Core acceptance only.

New cases cover unselected children, mixed B/E/X with grandchildren, fractional units, very large
clocks/tiny durations, precision-hidden crossing, unordered nonselected context, process/thread
isolation, ambiguous X coincidence, known B/E coincidence in both selection directions, crossing
components with independent valid components, async exclusion and available zero. An independent
integer tick-union oracle covers sibling/deep coverage, plus40 deterministic generated trees.
4096 implicit X nesting levels verify iterative traversal/newest600.606 equal-start nested records
reverse sorting versus retention order; an evicted100us inner sample cannot overwrite a retained
3us slot. Optional bounds reject negative/nonfinite/greater-than-inclusive values.

The combined PR665/PR666 baseline was separately validated as tree
`3043f7902bff2657f04eafb97cd20dfcb858d9b5`: Development475 steps, full286 registered/274 actual passes/
12 existing native feature/environment skips/zero failures341.56 s, Shipping and documentation16/16.
That baseline proof does not include this new self-time code.

The self-time candidate then rebuilt42 focused product steps on that combined baseline. Actual
`editor.chrome_cpu_trace` (including real read-only project IO) passed0.33 s and
`editor.chrome_trace_display` passed0.05 s,2/2 in0.39 s. Real1x/2x controls validate owning captures,
invalid replacement preservation and actual ImGui text output. The rendered parent shows inclusive
1.250000 ms versus self1.150000 ms, with available zero and unavailable async rows. The logging
inspection uses one in-memory test frame and performs no clipboard or file logging.

The full Development consumer rebuild passed273 steps. The first full CTest attempt registered286
cases:272 actual passes,12 existing native feature/environment skips and two timeouts, total464.81 s.
`editor.prefab_nested_materialization` timed out59.64 s and `editor.hierarchy_scale_lifecycle`
timed out307.96 s. Chrome Core/UI passed0.56/0.05 s. Source hashes remained unchanged.
Shipping configure/build and all16 documentation tests plus changed Markdown links/pairing passed.
The failed full-run log is preserved; these timeouts are not counted as successful acceptance.
An unchanged-timeout serial failed-test diagnostic passed2/2 in306.73 s: prefab10.24 s and
hierarchy296.48 s. A subsequent complete single-worker gate ran the original configure/build/CTest
presets and passed286 registered/274 actual/12 existing skips/zero failures in535.91 s. Core/UI
passed0.32/0.03 s, prefab10.41 s and hierarchy213.97 s. The timeout multiplier stayed1 throughout;
no test assertion, correctness condition, source or timeout was changed between attempts.
The original failures and near-deadline diagnostic remain recorded; runtime variance is not fixed.

Actual main after PR666 merge exactly matched the separately validated combined baseline tree.
The new unpublished delivery branch moved to that main without rewriting tested source; all six
self-time source/test SHA256 digests remain identical and the integration index matches main.
Fresh configure/build against that delivery baseline is recorded before publication; no pending
foreign changes are included. Hosted cross-platform results are not claimed from this Linux gate.

No ED-M0 through ED-M7 milestone or physical acceptance is closed by this supporting evidence.
