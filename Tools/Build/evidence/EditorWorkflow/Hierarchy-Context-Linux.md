# Hierarchy object context actions: Linux acceptance

Scope: an independent Editor UX support slice on main
`56439f6997b981646768abeef49921af6a25810c` (PR806's accepted Inspector presentation).
Right-click an entity row for existing Copy/Cut/Paste/Duplicate/Rename/Delete actions. An unselected
row becomes the selection; a selected row retains its group. The popup owns current entity/document
keys and rechecks the complete selection, generations, focus and modal access before presenting
commands. Read-only Copy stays available; writes retain the authoring gate. Opening discards
Inspector drafts and prospective Scene gestures; dismissal retains committed values and Redo.
Open popups own Delete input, preventing the underlying Hierarchy shortcut from firing through them.

Commands execute after tree rows and root drag targets finish borrowing NodeViews. Rename uses the
existing modal; clipboard, component payloads, complete subtree history, scene files, runtime APIs
and production linkage retain their existing contracts. One test target links existing EditorImGui
and Dear ImGui dependencies for actual UI layout. Workflow5/9, Editor0/8, TwoD9/11 and other actors'
claims remain unchanged. Physical target-host acceptance remains separate.

## Actual widget and native proof

- New actual-pointer tests fail on the previous UI: right-clicking First leaves Second selected.
- Final `editor.hierarchy_context_actions` passes in 4.89 seconds: 1x/2x, regular/150-unit windows,
  target selection, retained group selection, copied subtree Camera/opaque payload, Cut/Paste/
  Duplicate/Delete complete Undo, disabled group Rename and normal single Rename, read-only Copy
  and rejected writes, stale selection/deletion/document reload, focus/modal cancellation, abandoned
  Inspector text, retained Redo and non-penetrating Delete input.
- Five related cases pass: Inspector Reset/Values, builtin creation, keyboard rename lifecycle and
  Hierarchy keyboard navigation (65.42 seconds total). Documentation unit tests pass 16/16.
- Actual final-implementation Xvfb/Vulkan Editor: right-click the previously unselected Camera,
  use Duplicate to create Camera Copy, Undo to the original two nodes and clean state, normal close.
  [Object menu](Hierarchy-Context/native-menu.png),
  [Camera Copy](Hierarchy-Context/native-duplicate.png),
  [Undo](Hierarchy-Context/native-undo.png) are raw native captures.
- Native receipt: 1576 presented frames, zero UI rejections, normal exit and unchanged authored Scene
  SHA-256 `6edc8adc2cf767b7197b7739edf5852c14f7f1250731d6183ce87ea2058dfd1d`.

## Complete gate

The frozen implementation passes the full unfiltered Linux development gate:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
LP_NUM_THREADS=2 CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

Configure/build pass; CTest passes **559/559 in 6129.24 seconds**, exit 0, zero failures, skips or
retries. The new actual context-actions case passes in 4.77 seconds within that complete gate.
Existing Inspector, Hierarchy navigation, rename, lifecycle, native Scene/Game and 1x/2x authoring
reopen/replay/layout cases all pass.

- Packaged source reload: **1804.81 seconds**, 33 actual native replacements, seven rejected
  candidates, discarded obsolete results, retired-library finalizers, 80-byte state payload and
  Stop cancellation; SDK compile and unchanged authored bytes are verified by its passing receipt.
- Packaged source reimport: **382.85 seconds**, 32 cycles and 70 publications, verified SDK Build
  and saved reopen, corrupt/missing-source last-good retention and preserved half-frame clock;
  receipt reports passing and unchanged authored inputs.
- Final documentation checks: 16/16 unit tests and all 663 repository Markdown documents pass
  bilingual/link validation. The root README remains unchanged because no milestone changed.
- Every frozen implementation/test/CMake SHA-256 matches after the full gate. `EditorImGui.cpp`:
  `8e5fd8a34ec69a3c733d16925f7f8c8c7ee40a82ed9aeb4bc0c8106c78a68f90`.
  Configure/build/CTest/LastTest logs, exit receipt, source hashes and native/reload/reimport
  receipts are retained under `/workspace/work/editor-hierarchy-context/`.

Only validation documentation is finalized after that gate; implementation and tests stay unchanged.
Routine CI remains manual-only. Final-head review and exact source/base/tree guards apply before
own PR delivery; this slice does not certify other operating systems or physical-host workflows.
