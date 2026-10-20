# ED-M7 lazy Hierarchy request projections — Linux, 2026-10-20

Beads delivery `nexora-gu7.38.4`, base main `8fd501c46d9736ce5295c0ee12991eeca6518e20`.
Parent CI stability, production budgets and physical acceptance remain open.

## Scope

The request pass formerly captured all nodes, projected all rows and built all visible keys on every
frame, before the visible Hierarchy repeated that observation after buttons. The new request pass
captures nodes for expansion pruning, queued selection, sibling reorder or hidden row counts;
full row/key projection is required only for queued selection or hidden counts. Visible rendering
still observes current nodes after buttons. Every borrow remains frame-local; no World/document
snapshot is retained across frames. Current generations and complete row semantics are preserved.

Begin visibility is determined before request processing, which still runs even when hidden.
The existing navigation test executable now also verifies actual hidden ImGui frames at 1x/2x:
queued selection/reparent, one-step Undo/source conservation, full updated row counts without row
widgets, stale reload rejection, no-document reset and visible recovery. Existing assertions and
180/300-second large-hierarchy deadlines are unchanged. There are no persisted format or public
SDK changes; the test executable additionally links the already-used private ImGui test dependency.

## Preliminary verification

GCC14.2/Linux x86_64, managed four-CPU quota. On identical production correctness fixtures with
other build schedulers paused, unmodified DeepHierarchy passed 89.42 s; the initial visible-frame
prototype passed 70.33 s. This is one exploratory pair, not percentiles or production performance.
The final source additionally preserves hidden-window full row statistics. Both final focused CTests
pass: keyboard navigation 21.11 s and new hidden lifecycle 0.11 s, total 21.24 s, with other builds
resumed. The first test compile omitted required explicit selection flags; supplying false/false
fixed the fixture call without relaxing any assertion.

## Final Linux gate

Fresh linux-development configure/build and the complete serial CTest suite pass: 293 registered,
281 actual passes, 12 existing native feature/environment skips, zero failures, 605.86 s. Final-source
DeepHierarchy passes 64.21 s, the 100k lifecycle 230.52 s and hidden lifecycle 0.20 s. Original 180/300 s
deadlines and all prior test assertions remain intact. The final gate ran with one independent
single-worker Content build active; its timings are observations, not an isolated statistical study.
All three source/CMake hashes stayed unchanged through the complete gate. The full LastTest log is
archived outside the repository.

Documentation tests 16/16, local Markdown links/bilingual pairing, touched C++ formatting and diff
checks pass before publication. This private scheduling change introduces no public SDK/module
boundary or persistent format change. Hosted platform/Shipping results are recorded separately on
the PR and must pass before merge. Production performance, root causes of CI variance and physical
acceptance remain open.
