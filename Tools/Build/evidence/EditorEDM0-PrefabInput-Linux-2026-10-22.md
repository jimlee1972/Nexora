# Native prefab input and failure evidence investigation

Date: 2026-10-22. Initial base: `0c24ec75330ab5c94721847abca7656c5cb5e272`.
The candidate also incorporates documentation-only main `b0b3f697` before the final gate.
Environment: Linux cloud, Xvfb 1600x1200, Mesa lavapipe software Vulkan; validation layers are
installed, but these local prefab runs do not claim verified runtime layer activation.
This is native virtual-display workflow coverage, not physical display/DPI/IME acceptance.

## Original failure and limits

The preceding recovery-scope audit's complete 364-case Linux gate reported 363 passes and
one failure in `editor.linux_prefab_isolation` at 25.42 seconds: “One native Undo did not
restore all reverted properties/identities.” Its unchanged standalone rerun passed in 92.45
seconds. The original fixture deleted the temporary project in `finally`; CTest retained the
assertion but not the actual failed asset or input acknowledgements. A rerun does not establish
the cause or remediate the failed gate.

Source inspection confirms three ambiguity points:

- Fixed `.2`-second input sleeps acknowledged elapsed time, not GUI processing.
- Undo directly invokes the isolated document's history; the following `prefab action=3` Save
  diagnostic does not prove Undo was invoked or succeeded. Conditional confirmation rows precede
  Undo, changing both its position and admission.
- The two expected edits were not checked in the committed model before Revert. Dirty state
  alone does not establish that both the name and X position were committed.

The X11 backend preserves pointer/button order, and default ImGui trickling separates button
transitions. Inspection did **not** establish that the next Save motion overwrote an Undo release.
The historical failure's exact cause remains unproven; this change does not claim a core history fix.

## Change and preserved assertions

An opt-in, test-build-only observer copies actual GUI focus/key/pointer state, drawn prefab
control positions/admission, actual history attempt/success counters, and the first root's
committed name/X value after EndFrame. It retains no pointers and performs no action.
Frame numbers accompany changed observations. Detaching/replacing a prefab clears drawn control
admission. Normal graphical and Shipping builds do not emit this opt-in output.

The fixture sends each key/pointer phase once and waits for the processed GUI state, resolves
actual enabled Undo/Redo positions, checks both expected committed edits before full Revert,
and requires each history gesture to invoke and apply history exactly once. Original exact
source/identity/opaque-byte, selected-property, source-apply, rebase, save/reopen, scene-placement
and read-only assertions remain. Result/input waits retain an 8-second bound, shutdown bounds
remain 15/10 seconds, and the registered whole-case deadline remains 180 seconds.

Both process streams are drained during input/result waits using binary pipes. Output collection
rejoins raw prefixes before Unicode decoding and validation, including errors already drained
and error markers or Unicode scalars split across reads. Failures retain the project, baseline
and Undo assets, per-launch output, and timestamped input records. Successful runs remove their
owned scratch tree.

## Investigation and validation

- An external harness preserving the unchanged original fixture's scratch tree passed its full
  workflow. The first observed diagnostic fixture also passed in 96.69 seconds.
- A diagnostic run using Python tracing failed before Create: the compact startup shortcut did
  not open the prefab panel, and observations contained no drawn controls. This establishes an
  unsynchronized startup admission failure in that diagnostic run, not the old Undo failure's cause.
- The acknowledged fixture plus portable UI case passed 2/2 in 133.37 seconds, including the
  complete native workflow in 132.90 seconds, under its original 180-second bound.
- An external diagnostic supervisor suspended its own Editor for 1.2 seconds immediately before
  the post-Revert single Undo gesture. The acknowledged full native workflow passed without
  repeating an authoring action. Retained observations show Undo at `(86,239.5)` disabled during
  confirmation, then `(86,199.5)` enabled, followed by exactly one increment from `undo=1,1` to
  `undo=2,2` on release. Diagnostic artifacts are retained externally; this is not a replacement
  for the standard full gate.
- `LinuxDisplayAcceptanceTests.py`: 6 process-level cases passed, including drained errors on
  either stream, standalone `Validation Error`, and split Unicode/error markers in binary reads.
- Final prefab/output focus passed 3/3 in 133.50 seconds (native prefab 132.72 seconds).
- The first complete standard Linux gate completed **FAILED**: 361 passes, 2 failures, 0 skips
  out of 363 in 1219.73 seconds. Configure and all 148 incremental build steps passed; all 5512
  frozen source/asset/configuration hashes matched. Prefab passed in 132.30 seconds. The failures
  were unchanged `editor.linux_native_scene_meshes` (single-Undo baseline, 30.28 seconds) and
  `editor.linux_native_game_twod` (cycle 13, no 2D viewport/two Atlases, 45.23 seconds).
- A retained Scene Meshes diagnostic reproduced its plane-move Undo failure. A subsequent
  before/after-byte diagnostic passed all eight single Undo checks. These are diagnostic results,
  not acceptance of the failed complete gate; actual shortcut admission remains unproven there.

## Confirmed Game View readiness defect

The failed Game2D log advertises option `(1067.5,227.5)`, then `(849.5,237.5)`, then loses the
popup while still reporting zero textures. It does not record the actual input position/time,
so this trace alone cannot establish which coordinate that failed click used.

Independent source inspection establishes an observer defect: ImGui initially submits popup
items during a hidden sizing frame, while hovered-window detection excludes that hidden window.
The Game View observer previously published those temporary item rectangles unconditionally.
The native fixture treats presence of the option as readiness and therefore could click an
uninteractive measurement rectangle. The observer now publishes only nonhidden, visible, enabled
options. It does not change the Selectable action, rendering, model, input backend, or retry input.

The portable preview fixture checks absence immediately after the real popup-opening release
frame, then checks both options in the next visible frame before its existing single selection.
No input or frame was added. Running this exact rebuilt test against the preserved pre-fix shared
library fails with “hidden popup sizing frame published interactive View option coordinates”;
the corrected library passes the same test, including the original atlas/data/Stop checks.

The native Game2D 32-cycle case and portable preview passed 2/2 in 68.24 seconds (native 68.18).

## Final combined standard gate

| Command/check | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS, 1.779 s |
| `cmake --build --preset linux-development` | PASS, 386 incremental steps, 113.947 s |
| `ctest --preset linux-development` | PASS, 363/363, zero failures/skips, 1242.97 s |
| Source/asset/configuration freeze | All 5512 pre/post SHA-256 values match |
| `cmake --preset linux-shipping` | PASS |
| `cmake --build --preset linux-shipping` | PASS; Editor remains excluded by Shipping policy |
| Changed C++ range formatting, Markdown links and bilingual pairing | PASS |

The final complete run also passes Scene Meshes, but that case's intermittent shortcut cause
remains open in the shared investigation. Its input path was not changed here. The preceding
361/363 failed gate remains retained separately and is not reclassified as successful.
The final standard gate uses the normal interpreter and registered test commands; diagnostic
supervisors, forced Editor pauses and the old-library override are absent. This candidate changes
no production input routing, authoring transaction, dependency, or acceptance deadline.

## Acceptance boundary

The existing shared native X11 investigation remains open. This scoped work improves synchronization
and makes future failures attributable; it cannot reconstruct deleted historical assets. ED-M0
physical Linux display, Windows per-monitor DPI/installed IME, and backend visual acceptance remain
open. No Windows/macOS/device execution is claimed from this Linux worker.
