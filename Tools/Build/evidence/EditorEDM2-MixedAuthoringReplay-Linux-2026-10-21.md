# Mixed authoring history replay — Linux, 2026-10-21

Beads `nexora-owg.3.2`; a finite cloud regression supporting M2-07. The existing generic 1000-step
history counter and `editor.selection_delete_transaction` 1000 batch cycles remain unchanged.
The physical/camera/picking/focus/recovery parent acceptance and ED-M2 remain open.

## Scope and independent oracle

Each 1x/2x case creates a normal writable project, then uses production Open to clear fixture-seed
history. Actual Hierarchy pointer/Ctrl multi-selection and Inspector typing/Enter create the first
transaction. A deterministic log adds 999 ordinary authoring-thread `SceneDocument` commands:
667 Runtime transform transactions total and 333 opaque-metadata batches, each affecting both nodes.
This does not claim that all 1000 commands were created by widget clicks.

A separately computed value sequence checks both complete poses, original negative scales, exact
original unknown component identity/raw bytes and expected controlled opaque payload at every step.
No expected state is copied from World serialization. Actual keyboard Ctrl+Z/Ctrl+Y replay all 1000
transactions in each direction, checking every intermediate state and the retained multi-selection.
An extra empty-history action must preserve the baseline/redo chain. Source bytes remain at the
original baseline until explicit Save.

The Save shortcut publishes through the production scoped `SceneFileSession`. Post-save Undo/Redo
check dirty versus saved state without another source write. A separate process acquires a fresh
writer/World/UI, opens the saved file, verifies every final pose/opaque value and actual mixed-state
Inspector controls, then verifies empty history cannot change saved data or source bytes.

Only test/contract/evidence files change. Formats, runtime APIs, history budgets and existing
short-workflow timeouts remain unchanged. Dedicated replay subprocesses use a 120-second bound;
each new CTest case uses 300 seconds and `RUN_SERIAL` to avoid performance-gate CPU contention.

## Complete Linux validation

The branch starts at actual main `5f9269318b6950703e829f5c924df7f7aa2ae586`. Configured product
object compilation and standalone 1x/2x probes pass; 2x takes 8.865 s. The probe links already-tested
root libraries whose authoring/Runtime source matches this base; the GUI has an unrelated
reflected-array delta. This is preliminary evidence, not a substitute for this branch's full
configure/build/CTest or hosted acceptance. Four test/CMake source hashes are frozen.

The actual branch product passes the required gate:

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development
```

GCC 14.2/CMake 4.4.4, graphical shell and ProjectPlayer ON, local Slang/Zig OFF. The complete
501-step build passes. Focused old/new authoring cases pass 4/4 in 25.27 s. Full CTest passes
**305 registered, 293 actual passes, 12 environment skips, zero failures, 681.78 seconds**.
New replay cases pass at 1x/2x in 28.80/7.01 s; both old shorter workflows pass unchanged.
All four frozen source/test/CMake hashes match before/after. Original Deep/100k tests pass
64.23/204.05 s with original 180/300-second limits and scale1. No timing/percentile or isolated
production-performance acceptance is inferred. No runtime/linkage boundary changes require an
additional Shipping gate for this test-only delivery.

Final documentation/bilingual evidence is included. Applicable exact-head hosted CI must pass
before squash merge; the parent retains its unfulfilled native/physical acceptance criteria.
These are headless actual widget/process checks, not native OS-window, physical DPI/IME/glyph,
screen-reader, GPU-quality or production performance acceptance.
