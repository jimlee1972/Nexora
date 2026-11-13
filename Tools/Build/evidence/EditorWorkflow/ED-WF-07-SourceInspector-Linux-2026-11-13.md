# Main Editor Sprite source Inspector

Date: 2026-11-13 (Asia/Taipei). Base: `82d8c49f` (PR #773, including #775).
Tracking: `nexora-whr.7.5`, supporting ED-WF-07 / GitHub #748.
Independent `nexora-whr.7.4` / PR #776 owns main Play reference conversion; its work is preserved.

## Scope and contracts

✅ Actual main Inspector source combo/Clear consume the existing bounded source catalog and
complete-selection metadata transactions. They support None/Mixed/missing/unsupported display,
one batch Undo/Redo, wrapped scene save/reopen and independent unrelated opaque bytes. Valid
source references leave the unavailable-plugin presentation; malformed reserved components stay
visible there and cannot be overwritten or cleared by this section.

✅ The popup owns metadata/Atlas snapshots, display paths, complete NodeKeys, Content generation
and revision, and an owning root/address identity. It publishes the full catalog once per open,
checks consecutive visible frames and current context before each use, and releases on close,
scope change or Inspector draft cancellation. Assignment rechecks exact imported owners in the
model. Idle reference presentation reuses existing owning Inspector component previews and indexes
typed Content rows once per frame when supported references exist. It adds no per-target linear
document/Content scans or repeated complete Atlas validation. Oversized selections skip this
section's reference parsing/indexing.

✅ Current project writer, writable Content, stopped Play, focus and ordinary scene/modal/recovery
permission guard actions. Missing supported sources/Atlas can clear without a resolvable catalog.
Unsupported type/name/version/data reject the complete selection without changing bytes/history.
No widget file IO, Runtime components/transform changes or C/Zig/scene/asset wire changes occur.
2D feature guards retain the ordinary opaque wrapper when this Inspector is unavailable.

This accepts reference authoring only. Scene View drawing, no-module Play, Build export,
source animation/reload and complete ED-WF-07 remain separate. Workflow **5/9**, graphical Editor
**0/8**, TwoD **9/11** and physical device/clean-host acceptance do not change. Root README stays
unchanged because this supporting task accepts no overall milestone.

## Executed UI evidence

`editor.sprite_source_inspector` drives actual controls at 1× and 2×. It verifies complete-selection
assignment/Clear, exact saved-baseline Undo/Redo and equal-assignment Redo retention, mixed and
missing references, missing Atlas regions, unsupported version preservation, writer/read-only,
stopped Play, focus and recovery admission, stale source reimport/project/selection popups and
wrapped save/reopen. Runtime scene bytes remain unchanged by the initial metadata assignment.

`editor.linux_sprite_source_inspector` starts the actual graphical Editor on Xvfb/lavapipe, observes
stable presented control centers and sends real X11 pointer/key events. Two entities receive the
same 17-byte source UUID reference; one Undo restores exact original scene bytes, Redo restores
exact assigned bytes, Clear returns to the original and its Undo restores the assignment. Actual
read-only process restart displays persisted references; clicks and Save leave every project file
unchanged. Unrelated plugin bytes `07090b` remain exact. No synthetic authoring call drives this
native test; private observations only copy geometry and processed input/state.

Every native attempt retains full 1280×720 PPM screenshots, actual scene snapshots, input/logs,
project files and a receipt under `artifacts/editor-sprite-sources/inspector-*`, already covered by
the existing desktop/display CI artifact paths. Screenshot inspection confirms the actual source
path, two selected entities, preserved unrelated component and authoring transforms. Full-frame
equality is not asserted because UI focus/layout can differ; exact scene/project bytes are the
independent mutation result. This Linux software-GPU evidence makes no Windows/macOS/device claim.

The base-82 indexed, heartbeat-enabled focused run passes both tests: control case 2.75 seconds and
native case 32.23 seconds, total 35.00 seconds. Full gates and source/log/capture inventories are recorded separately
in the [machine-readable receipt](ED-WF-07-SourceInspector-Cloud.json).

## Retained initial failure

The first native fixture consumed the first visible control point before initial docking settled:
it clicked `(81,174)`, while later observations placed Select All at `(363.875,193.375)`. Selection
remained zero and the two-target assertion correctly failed after 10.62 seconds. Its input/log and
failed receipt remain in the original nested test artifact directory. The corrected observer waits
for unchanged geometry for 0.4 seconds across at least two subsequent observed frames, then sends
one gesture. It does not
retry edits, weaken selection/history/pixel/file assertions, or increase action deadlines. A fresh
strict native test then passes, followed by both final focused tests above.

Review strengthened the readiness observer further: elapsed time alone could retain an old point
while initial rendering stalls, so it also requires new observed frames at that same position.
The first full regression was intentionally interrupted with exit 130 after 21 completed passing
cases and zero test failures. Its full build/test/source snapshot and CTest log are retained
separately; it is not counted as a complete run. Only this native readiness helper changed, with
all C++ and test-registration hashes unchanged. A fresh focused run and full gate validate the
final observer before delivery.

The initial fresh-frame probe correctly timed out because the existing geometry stream emits
only changed observations: an unchanged control has no new frame record. That failed focused run
and helper version are retained. A separate test-only opt-in frame heartbeat now supplies fresh
frame identity without changing the existing geometry stream or any product/input operation.
The readiness condition still requires both 0.4 seconds and two subsequent frames; no deadline
or result assertion is weakened. Final validation uses that heartbeat-enabled native fixture.

The pre-index full run was intentionally interrupted after 31 passing completed cases, zero test
failures, exit 130. Review confirmed that Content lookup is linear and the first presentation
repeated both Content and document lookups per selected source. The final section instead consumes
the ordinary Inspector's existing owning previews and builds a frame-local typed-row index once.
The additional actual-control fixture has 32,771 Content rows and 32 selected references to one
source. Its observed whole-shell CPU frames are 84.0496/90.2364 ms at 1×/2×, including large-catalog
view/cache setup. These are diagnostic samples, not isolated source timings or portable frame
budgets; no timing threshold is asserted. Exact source bytes and the real label remain unchanged.
The interrupted log/source snapshots stay separate; final full validation uses the indexed code.

## Reproduction

Reuse the existing cloud task-local CMake/Ninja/GCC/Zig/Slang/Clang/sysroot and writable XDG caches.
No new system packages or tracked preset override are required.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-on-failure
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full \
  -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF
cmake --build --preset linux-shipping
cmake --preset linux-development -B build/inspector-monolithic \
  -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build build/inspector-monolithic --target NexoraSpriteSourceInspectorTests
ctest --test-dir build/inspector-monolithic -R '^editor.sprite_source_inspector$' --output-on-failure
cmake --preset linux-development -B build/inspector-feature-off \
  -DNEXORA_ENABLE_2D=OFF -DBUILD_TESTING=OFF -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build build/inspector-feature-off --target NexoraEditor
```

## Validated implementation checkpoint on base 82d8c49f

Complete Linux Development configure/build and unfiltered two-worker CTest pass **506/506**, zero
failures/skips, in **1476.61 seconds**. Full Shipping configure/build pass. A fresh Monolithic build
links the actual graphical Inspector consumer and its control test passes 1/1 in 2.72 seconds.
A separate fresh `NEXORA_ENABLE_2D=OFF`, testing-OFF graphical Editor configure/build passes.
API-M6 remains 145 exports; documentation routing passes 16/16 and changed Markdown/language pairs
pass. Seven final source hashes match the built worktree. Both interrupted full executions and
native setup failures remain separate from this complete result.

Main independently advanced to PR #776 (`e7684590`), delivering module-backed authored source Play.
This implementation checkpoint is retained before integrating its overlapping contract paragraphs;
combined validation and the final delivery head are recorded as a separate generation.

## Integration of independently delivered module Play

Main PR #776 is merged at `e7684590`; its authored-reference freeze/bind, generic linear bulk
inspection API, native snapshots, tests and evidence are retained unchanged. Both overlapping
Editor contract paragraphs are resolved by keeping the graphical Inspector consumer and the new
bulk inspection/Play contract. Module-backed Play is now independently delivered; Scene View,
no-module Play, Build, animation/reload and full ED-WF-07 remain open. Counts stay 5/9, 0/8, 9/11.
The source/registration changes belonging to this Inspector implementation remain unchanged;
combined source hashes and complete local/hosted gates are verified before the integration commit.

The first integrated full execution retains one native Inspector failure at Redo/save (30.05s):
assignment and exact Undo/save succeeded, but the raw `xdotool key ctrl+y` step left the file at
the initial bytes. The native records contain no acknowledged Y-down/up phases, so the cause is
not declared a product Redo defect. The fixture now uses the existing documented Ctrl+Shift+Z
Redo shortcut through `NativeSceneInput.chord`, acknowledging each modifier/key down and release
before Save. This changes the physical Redo gesture and strengthens input observation; the exact
assigned/initial scene bytes, history semantics, project hashes and action deadlines stay intact.
The failed execution, input stream, actual scene and screenshots remain separate from recovery.

That complete integrated execution reports 506 passes, one failure, zero skips out of 507, in
1491.66 seconds and exits 8; it is not a passing gate. The final phased-Redo focused recovery
passes 3/3: unchanged main Play case 2.60s, Inspector controls 2.65s and actual native window
35.22s, total 40.49s. Exact assignment, Undo/Redo, Clear/Undo and read-only project byte predicates
remain unchanged. A fresh complete integrated gate is required after this focused recovery.

## Final combined local gates

The final integrated Development configure/build and unfiltered two-worker CTest pass **507/507**,
zero failures/skips, in **1471.98 seconds**, including the independent main authored-Play probe and
both actual Inspector control/native cases. Integrated Full Shipping configure/build passes;
Monolithic actual graphical Inspector test passes 1/1 in 2.67s; the testing-OFF, 2D-OFF graphical
Editor configure/build passes. API145 exports, documentation16/16, changed Markdown/language
pairs and edited C++ formatting pass. Final own/inherited source hashes, logs, actual captures and
project byte inventories are in the receipt. Its complete base82 generation and the failed first
integrated run remain distinct; neither is relabeled as this clean final execution.
