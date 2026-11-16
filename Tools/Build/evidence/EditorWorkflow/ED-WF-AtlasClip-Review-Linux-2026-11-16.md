# Atlas/Clip PR review followup

Status: focused and related regressions and native authoring matrix pass. Full Linux and
exact-head hosted CI remain mandatory delivery gates before merge.
This follows [the baseline acceptance](ED-WF-AtlasClip-Linux-2026-11-16.md) for
`12470be031fb204c3fe1bd7fc005bd7a6a8c6f85` in PR #798. Its original full-run failures and explicit
native recoveries remain recorded without relabeling them as a single green invocation.

## Review corrections

- [Repeated validation](https://github.com/jimlee1972/Nexora/pull/798#discussion_r4583224656):
  a legal1024-region Atlas and4096-frame source Clip took6.497s in the original boundary fixture.
  Each frame re-entered quadratic Atlas validation. Saves now trust standalone validation of
  the exact immutable imported payload and memoize matching Ready region/trim closure per source
  UUID. The same fixture completes in0.0508s, retaining all4096 frames and8 distinct dependencies.
  No platform-specific wall-clock assertion or relaxed validity condition was added.
- [Unrelated broken sources](https://github.com/jimlee1972/Nexora/pull/798#discussion_r4583224662):
  the strict catalog correctly rejects incomplete aggregate closure, but was unsuitable for a
  repair selector. `PublishAvailableContent` skips unavailable or individually incompatible
  sources while preserving admitted identity/budget checks and owning snapshots. The original
  strict method keeps its contract. Tests include missing Atlas, mismatched region dimensions,
  failed source and loading Atlas states; actual1x/2x controls repair a broken Clip using a valid
  source while unrelated broken entries remain present.
- [Slice selection](https://github.com/jimlee1972/Nexora/pull/798#discussion_r4583224671):
  each digit of a rename7-to91 arrives on a separate GUI frame. Sorting now follows the original
  entry through its permutation, including duplicate IDs, and draft Undo/Redo retains selection.
  Subsequent X editing changes the intended slice, preserves region8 and returns to region7 after
  Undo. The original widget regression failed before this correction.

The focused catalog/model/actual UI selection passes3/3 in3.09s. The maximum-closure fixture,
strict catalog behavior, retained-owner/version isolation and real widgets are exercised without
Scene/Runtime mutation. Normal formats, C/Zig ABI and module dependencies remain unchanged.

The [18-source followup receipt](ED-WF-AtlasClip-Review-Linux-2026-11-16.json) records hashes and
exact native results. Development configure/full build passed. Related catalog/creation/editing/
Inspector/authored-Play regressions pass8/8 in11.23s. Actual native Editor editing, read-only reopen,
Build, source deletion, Unicode relocation and Player rendering pass again with Development
(46.29s), graphical Monolithic (4 model/UI/catalog/native cases,45.18s) and Shipping Player.
All three retain the same authored Atlas/Clip hashes, unchanged Scene/sidecars/read-only bytes and
normally drained rendering. The full Linux gate and all applicable hosted checks must pass for
the final committed head before merge; these scoped results are not a new full-suite claim.

Runtime canonical reload and reimport code is unchanged. The original Development/Monolithic
33-replacement and32-cycle/70-publication receipts remain explicitly tied to their baseline
snapshot. Native finalizer records establish execution and stable independent live probes;
quarantined teardown callback success is not mislabeled as a universal denial status.
