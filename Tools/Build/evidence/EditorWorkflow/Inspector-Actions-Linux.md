# Compact Inspector component actions: Linux acceptance

Scope: an independent ED-M0 presentation slice on `0c3f6c47a8674e452080590bf01a1f3fa6196044`.
Transform, Camera and Light each expose one gear menu for Copy values, Paste values and Reset.
Copied values highlight the matching gear and appear in contextual feedback instead of a permanent
Inspector row. Opening/dismissing menus preserves committed values and Undo/Redo while discarding
unsubmitted drafts and prospective Scene gestures. Selection-owned popup IDs remain stable across
that cancellation; focus/modal gates close an open popup without reviving it afterward.

The existing typed owning clipboard, authored Euler turns, source deletion/reload, absent-component
preservation, read-only Copy, write access, mixed selection and atomic history contracts remain.
No scene format, runtime API, C ABI, Zig API or production module dependency changes. Only the
component-value test directly links the existing Dear ImGui target to inspect real layout/text.
Reflected component/provider restoration and physical-host acceptance remain separate work.
Workflow5/9, graphical Editor0/8 and TwoD9/11 counts remain unchanged.

## Widget and native evidence

- The updated actual-pointer tests first failed on the previous UI with
  `Inspector component action menu absent` in both Reset and Values cases.
- Final targeted Reset/Values tests pass at 1x/2x. They retain the original value/history assertions
  and cover 150-unit Inspector panels, reachable menu actions, contextual copied-value feedback,
  non-mutating open/dismiss, abandoned input, no-op Redo, Save/Reload, all-absent/mixed presence,
  independent hierarchy clipboard, read-only writes, and open-popup focus/modal cancellation.
- A first popup driver erroneously sent Escape even when a close modal blocked the menu, dismissing
  that modal before the next attempted Paste. The driver now dismisses only an actual component
  popup; the original production modal/data assertions remain intact and pass.
- Native Linux Xvfb/Vulkan Editor: select Camera, type FOV 75 and Enter, Copy through its actual
  gear menu, Reset to 60, Undo to 75, Undo to the original clean 60, then normal close. Raw screenshots:
  [copied-value menu](Inspector-Actions/native-menu.png),
  [Reset 60](Inspector-Actions/native-reset.png),
  [Undo 75](Inspector-Actions/native-undo.png).
- Native evidence: 4570 presented frames, zero rejected UI submissions, normal exit and byte-identical
  original Scene file SHA-256 `6edc8adc2cf767b7197b7739edf5852c14f7f1250731d6183ce87ea2058dfd1d`.
  These screenshots are actual rendered output, not mockups.

## Complete gate

The frozen implementation passed the required unfiltered Linux development gate:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
LP_NUM_THREADS=2 CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

Configure and build pass. CTest passes **558/558 in 6089.93 seconds**, with zero failures, skips or
retries. Inspector Reset passes in 6.02 seconds and component Values in 11.93 seconds. The complete
suite includes the existing native Scene/Game/Hierarchy, keyboard/modal, authoring reopen and
1x/2x Transform layout cases.

- Packaged native source reload passes in 1760.97 seconds: 33 actual replacements, seven rejected
  candidates, discarded obsolete results, retired-library finalizers, 80-byte state payload and
  Stop cancellation. Receipt reports `passed`, `authoring_unchanged` and
  `sdk_compiled_from_saved_settings` as true.
- Packaged source reimport passes in 386.71 seconds: 32 cycles, 70 published updates, verified SDK
  Build and saved reopen, corrupt/missing-source last-good retention and preserved half-frame clock.
  Receipt reports `passed` and `authoring_unchanged` as true.
- Documentation unit tests pass 16/16; final repository Markdown/bilingual/link validation passes
  all 662 documents with zero errors.
- The implementation/test/CMake SHA-256 receipt matches every frozen file after the complete gate.
  `EditorImGui.cpp` SHA-256 is
  `44b1916e12f386e377a4b1f29d64963aa5db5fed38fac948494be3999cc877c1`.
  Full configure/build/CTest/LastTest logs and source/native receipts are retained outside the
  checkout under `/workspace/work/editor-inspector-actions/`.

Only validation documentation is finalized after that gate; no implementation or test changes are
made. The repository-root README stays unchanged because this slice does not change a milestone.

No routine CI is dispatched, following the repository's manual-only policy. Final-head PR review and
merge/base/source guards are required before publication completes. This Linux cloud slice does not
claim Windows/macOS/mobile or physical-host acceptance.
