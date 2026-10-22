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

The fixture sends each prefab control key-chord/pointer phase once and waits for the processed GUI state, resolves
actual enabled Undo/Redo positions, checks both expected committed edits before full Revert,
and requires each history gesture to invoke and apply history exactly once. Later scene-placement
shortcuts retain their existing phase waits and exact saved-byte result checks. Original exact
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

## Final standard gate before recovery-scope integration

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

## Integration with recovery-scope delivery

PR #698 subsequently incorporates main `49ee3d6c` (PR #696), including the recovery-scope
regressions and finite-input normalization. The 363-case table above describes the earlier
`557d0800` candidate on documentation-only base `b0b3f697`; it does not certify this integration.
The integrated candidate adds the registered `editor.recovery_scope` case for 364 total tests.
Its exact full-gate outcome and final-head hosted checks are tracked in
[PR #698](https://github.com/jimlee1972/Nexora/pull/698) and the shared investigation tasks.
Neither a prior-head pass nor an unfinished integrated run authorizes the final merge.

## Acceptance boundary

The existing shared native X11 investigation remains open. This scoped work improves synchronization
and makes future failures attributable; it cannot reconstruct deleted historical assets. ED-M0
physical Linux display, Windows per-monitor DPI/installed IME, and backend visual acceptance remain
open. No Windows/macOS/device execution is claimed from this Linux worker.

## Scene shortcut integration follow-up

Latest main `af14e4d6` adds explicit graphical merge publication (PR #693). Integrating it
in `aa69cd61` resolves a README-only conflict by retaining both contract sections. Independent
review found no overwritten application publication flow or private test-access declaration.
The subsequent complete local gate fails 3/365 (362 pass, zero skip, 1289.87 s), with all
5513 frozen hashes matching. The retained failures are candidate comparison (no new request
started after read-only candidate validation), publication (no first comparison request
started), and Scene Meshes (one rotation Undo did not restore the baseline). Retained scene
comparison assets contain the expected external/corrupt revisions. These observations locate
the missing expected event, but do not prove which input/gate failed. The same head's hosted
display job separately fails the unchanged plugin-manager fixture before rendered source
control; that failure is not relabeled as a scene comparison failure or a passing gate.

A private processed-input observer now records nine scene shortcut bits, pointer/button,
text-input state, exact handler admission and real scene Undo invocation/success counts.
The shared native driver sends a physical phase once and waits for its actual GUI state;
Ctrl+Z additionally requires exactly one invocation and one application. Original core routing
and authoring calls remain required. Both binary streams are drained and retained, including
partial reads. Timestamped argv and changed GUI records with frame numbers survive failures.
Control geometry and output result offsets are captured before their corresponding actions.

Early development of this driver exposed an offset bug: input acknowledgement already drained
a successful comparison result before the old default result offset was captured. Moving
the offset before the original gesture preserves rather than retries that success. An initial
blanket pointer delay also exceeded the existing center-case deadline; removing this added
pointer delay preserves the original waits and 80 ms key-phase minimum. Save acknowledgements
share the original five-second result deadline instead of starting a new eight-second budget;
late successful bytes cannot satisfy the expired bound. Initial failed diagnostic runs remain
retained separately from final validation. Whole-case timeouts remain 45/90/120 seconds.

This supporting synchronization/evidence work does not reconstruct deleted historical input
or close the broader X11 or physical-host acceptance gates. Final integrated validation and
exact-head CI/merge status are recorded in [PR #698](https://github.com/jimlee1972/Nexora/pull/698).

### Hosted source-marker readiness follow-up

The `aa69cd61` hosted display failure occurs before Ctrl+Alt+E, `ExtensionControls`, or any
signed-package operation. Its empty events file is expected at that stage and cannot prove a
plugin runtime failure or missing shortcut. The old fixed startup rectangle cannot distinguish
a missing render from a marker drawn elsewhere. The revised opt-in observer reuses the existing
private copied `SceneMarkerPosition`, node identity and selection. The fixture keeps its original
10x10 RGB predicate at the actual drawn center, sends one native click and requires the same
source selected before continuing. Startup waits, twelve-second operation bounds, signature,
permission, enable/disable/revoke, read-only and source-integrity assertions remain required.
Ctrl+Alt+E retains its original 100 ms gesture; the nine-key observer does not include E.

Both binary process streams have one drain owner shared with `ExtensionControls`; logs, input
records and project survive failures. Successful launches still validate complete output. The
intermediate `scene-input-standard` gate's configure/build pass, but its CTest was deliberately
stopped to include this independently observed hosted gap. That incomplete run is not acceptance.
No original failure is reclassified; final complete-gate and fresh-head hosted results remain
required. The historical exact cause of the missing source pixels remains unproven.

The focused signed-package run passes all four selected checks (including the complete native
flow in 32.22 s). Its retained `source-marker-focused-LastTest.log` copies source node 2 at
`(634,381.5)` on frame 2, settling to `(641,381.5)` on frame 3; exactly one click subsequently
observes selected=1/selection_count=1. Read-only restart settles at `(641,390)` and likewise
selects that same node once. These actual drawn coordinates differ from the old hard-coded
`(523,182)` / `(523,199)` targets. This proves the old fixed points are not the actual marker
centers in this run; it does not reconstruct the earlier hosted pixel failure. The original
pixel predicate and signed-code/source assertions pass at the observed centers.

### Content and native scale diagnostic follow-up

The next frozen integration gate (`scene-input-final-standard`) passes configure/build and
363/365 tests with zero skips in 1310.84 s; all 5,514 frozen source/asset/configuration hashes
remain unchanged. Scene-file dirty rename and native preview axis-2 scaling fail. The former
fixture did not separate its copy/renamed/startup conjuncts; both former fixtures deleted the
failed assets. Their precise historical causes cannot be reconstructed from these failures.

The scene-file fixture now copies the actual first scene row, visible/enabled Rename/Delete
menu points, rename field/Apply/Cancel points and typed UTF-8 text. Hidden popup sizing frames
publish no actionable controls. Existing F2+Enter and context-menu+Apply paths remain distinct;
the dirty relocation first proves a real extra node and dirty document. Failure output separates
each filesystem fact and retains project bytes and both launch streams. Portable 1x/2x tests
prove hidden/visible popup behavior and an actual UUID-preserving rename.

The native preview fixture uses the same single-owner binary input driver and copies the actual
native scale-axis hit and drag-preview state. The original press/move/release must be observed
before the existing exact axis-factor, unchanged-on-disk preview, Undo and pixel checks. The
original three-/five-second result and 45-second whole-case bounds remain required.

An initial focused run passes both portable tests, then exposes two defects in the new driver:
a normal Discard exit is rejected before its retained final release observation is checked;
and a wheel click is incorrectly mapped to one of the three held mouse buttons. The driver now
checks retained/remaining observations before rejecting exit and keeps wheel gestures on their
original native path. Combined pointer move/down/up shares one eight-second input deadline.
Successful Save and Exit terminates on Enter-down, so its final single native Return gesture is
acknowledged by the original saved-file and exit-zero checks, without claiming GUI Enter-up.
These are concrete diagnostic-run defects, not evidence of the older deleted-fixture causes.

The subsequent focused scene-file/preview run passes 2/2 (58.52/41.12 s), and portable
comparison/scene-file tests pass (0.24/0.61 s). The next frozen full standard gate,
`native-input-complete-standard`, passes configure and all 143 build steps, but remains
**FAILED: 364/365 pass, zero skips, 1358.81 s** (wrapper 1358.840 s). All 5,514 source/asset/
configuration hashes match. SceneFiles 58.91 s and native preview 42.99 s pass unchanged
bounds; signed packages 31.20 s, prefab 132.27 s, comparison and publication also pass.
The only failure is `editor.linux_native_play_debug_lines` (15.91 s): the third writer reopen
has no native Game viewport after a blind 0.3-second wait and compact F5 gesture. The old
fixture deletes its project/output and records no processed Play input, so the historical
failure does not establish event loss or runtime Start failure. A scoped Play-input/evidence
follow-up is required; this failed complete gate is retained and cannot authorize merging.

### Play reopen input/evidence follow-up

A separate F5/F6/F10 observation preserves the nine scene-key wire. It copies processed key
phases, the exact original Play shortcut gate, the actual keyboard-request sequence/last
command and owner Runtime Play state. Portable 1x/2x coverage passes in 0.04 s after the six-step
focused build, including stopped/missing-owner/text-modal rejection, release stability and
one UI request without implicitly executing Runtime operations.

All Game fixture launches share one binary drain/collector, including the two short four-frame
checks. Only the reserved Play keys use processed-phase/request acknowledgements; movement and
pointer gestures retain their existing runtime path. Each Start and native viewport result
share the original twelve-second bound; registered whole-case limits remain 45 seconds
(60 for materials). Original geometry/pixels, Pause/Step/Stop, module callbacks, source bytes,
persisted settings, corrupt-file and read-only assertions are retained. Failure retains each
launch's complete output, timestamped native inputs and project; success cleans its scratch.

The twelve registered Game modes pass 12/12 in 66.20 s, including debug-lines in 6.21 s.
This confirms the revised fixture and its runtime assertions in that run; it does not identify
the original third-reopen failure's cause. Another complete frozen gate remains required.

The next complete frozen standard gate (`play-input-complete-standard`) passes configure/build
and 364/365 tests, zero skips, in 1280.58 s (wrapper 1280.612 s); all 5,514 hashes match.
The revised debug-lines workflow passes in 6.26 s, as do every other revised native fixture.
The gate remains **FAILED** solely on `editor.linux_native_play_soak`, cycle 1 at 2.03 s:
"Play motion changed native authoring Scene pixels". The old soak fixture deletes its failed
project and image data. No image evidence reconstructs whether the difference was presentation,
viewport, camera/selection, or authoring isolation, so none is asserted as the historical cause.

Static inspection identifies a separate readback readiness defect: the blue predicate combines
R/G/B channels from three independent screenshots, then captures a fourth baseline without
checking its predicate or stability. The native viewport log is also emitted before surface
presentation. The compared pixels are the existing center-region samples (about 180x200 at
stride 3), not the entire viewport. A supporting fix must preserve a baseline before any Play
Start, exact subsequent source equality, 32 cycles, original lifecycle/neutral input/RSS limits,
four-MiB stream budgets and three-/240-second result/whole-case bounds. It must retain reference
and actual RGB, viewport/difference geometry, source bytes, input and complete output on failure.

The revised registered soak passes all 32 cycles in 87.56 s (87.57 s CTest total).
It retains the pre-Start baseline, adds an exact source check after Start before any motion,
and keeps original exact motion/Stop source checks. Warm RSS baseline is 131,657,728 bytes,
peak/end 132,034,560 bytes (growth 376,832 bytes against the unchanged 33,554,432-byte limit),
with four warmup cycles, unchanged source bytes/samples and zero observed validation errors.
The retained `soak-readback-focused-LastTest.log` contains the full report. This supporting
readback repair does not reconstruct the earlier deleted failed pixels or accept a failed
complete gate; final integrated verification is still required.


### Retained integrated failures and remaining native acknowledgements

The next frozen complete gate, `soak-input-complete-standard`, remains **FAILED: 360/365 pass,
zero skips, 1273.17 s** (wrapper 1273.189 s). Configure/build pass and all 5,514 source/asset/config
hashes match. It exposes five failures: Soak 18.24 s, SceneFiles 46.35 s, Build Console 18.86 s,
pointer buttons 3.40 s and prefab source inspection 12.05 s. Logs are retained unchanged; all six
surviving synthetic project/state directories are also archived under its `-failures` directory.
The old Build/PrefabSource cleanup deleted their failure fixtures, so those typed bytes and click
coordinates cannot be reconstructed.

Soak fails cycle 6 on missing copied motion, while its retained exact source-pixel difference is
zero and viewport unchanged. Existing GUI buttons and geometric containment do not establish
actual capture. Its module reads pointer displacement in Update every Playing render frame;
the documented per-batch/empty-batch contract does not prove a missing-fixed-tick defect. New
private observations copy the original actual hover/click/admission/focus and admitted-click
counter. One acquisition down/up, separate seed and original measured motion share the original
three-second applied/pixel deadline; no click or motion retry is added. Runtime/provider and all
source/RSS/lifecycle results remain independent required oracles.

SceneFiles fails to adopt Main after compact Ctrl+O: no modal/text-focus record appears and startup
metadata remains Content/Empty.scene. A distinct O bit preserves the existing nine scene-key and
three Play-key formats. The true Open branch increments a copied request count. Native Open now
acknowledges one physical chord and actual modal/Path field/focus/text before Return. Open and
Save As phases share ten seconds; original subsequent adoption and source/history/readonly checks
remain required. Portable 1x/2x tests exercise the copied dialog and single-request observations.

Build Console operation 3 actually exits 2, the fixture's missing/unknown-mode result, rather than
merely losing an exit-zero diagnostic. Its old cleanup prevents reconstruction of the submitted
argv. The revised binary collector observes the real controls and exact executable/cwd/argv text
before one Run; the Run and exact operation result share the original twelve seconds. Cancellation,
close under 3.5 seconds, source bytes, output ownership and artifact-unverified checks remain.
Command/path hex is test-only opt-in output, absent from normal product diagnostics.

The pointer-button log actually contains button 9's held mask 8 followed by neutral 0; the original
failure is not evidence that release failed to reach gameplay. A narrow opt-in application copy
records the actual single-instance DrawScene pose after successful EndFrame queue admission.
The fixture drains both streams during its unchanged held/release intervals, checks fresh neutral
consumer state and consecutive equal admitted draw poses, then retains the original exact pixel
comparison across two 150 ms intervals. Queue admission does not prove physical scanout. Failure
retains reference/actual RGB and JSON; no release retry or pixel tolerance is introduced.

PrefabSource replaces a fixed Scene click and blue-control ordinal with the actual Scene marker,
selected identity and Inspect source control. Each inspection is sent once and must produce exactly
one scoped report within the original ten seconds, with unchanged retained/current revision,
missing/restored archive and read-only/source-file assertions. The first new focused attempt fails
before selection because root initially publishes a hidden Hierarchy row instead of the original
Scene marker; that diagnostic attempt is retained as failed. Correcting the observer to the existing
SceneMarkerPosition preserves the original selection route without changing docking or deadlines.

Focused validation after the eight-step application/portable build and two-step SceneFileInput
build: Build console portable 1.03 s, Play input 0.04 s; native Soak 32 cycles 80.28 s, SceneFiles
59.54 s, Build Console 10.74 s, portable scene-file input 0.64 s. The corrected PrefabSource and
pointer-button run passes 2/2 in 12.12 s (5.87/6.24 s). These focused passes do not replace the
failed complete gate or establish the historical unobserved failure causes. A new frozen complete
configure/build/CTest gate and fresh-head CI are still required before merging.

### Integrated marker-layout failure and source-rebase follow-up

`capture-workflow-complete-standard` remains **FAILED: 363/365 pass, zero skips,
1379.72 s** (wrapper 1379.753 s). Configure/build pass and all 5,514 frozen hashes match.
Soak 87.68 s, prefab isolation 136.39 s, SceneFiles 59.43 s, Build Console 11.04 s,
preview 43.48 s and pointer buttons 5.97 s pass. Failures are legacy source rebase
12.28 s and revised source inspection 10.31 s; original reports remain retained.

The retained source-inspection trace now establishes a specific stale-coordinate defect:
first marker `(16,386)`, then `(634,381.5)` when the pointer is processed, then `(641,381.5)`.
The sole mouse-down/up stays at `(16,386)` and selection remains empty. Selection now uses
the same bounded named-coordinate refresh as inspection: at most three pointer moves,
then one down/up, sharing the original ten seconds. No selection/inspection retry is added.

A separate private getter defect is reproduced by a portable 1x/2x visibility test:
its first hidden sizing frame publishes a marker (`hidden=1 published=1`). The original
library fails the test in 0.05 s. A getter-only current-window/visibility/frame guard makes
that same test ELF pass in 1.72 s (CTest total 1.73 s). The unchanged ELF SHA256 is
`3023583dc5fc415b77cc29cf6cfd2088da75e8f2d676e747814f26534e8f9109`.
Visible, forced sizing, skipped hidden and re-visible cases retain exact document bytes,
dirty state and empty selection. Marker rendering, hit testing and production selection
routing are unchanged. The guard does not by itself establish that later layout stays fixed;
the native named-coordinate refresh remains necessary.

The old Rebase fixture reports a blue ordinal control at y243, then times out waiting for
its exact review event. Cleanup deletes that failed project/input, so its actual selection
and clicked control cannot be reconstructed. Its replacement copies actual Review/Keep
local/Apply/confirmation controls, keeps the original Escape and Ctrl+Y paths through a
separate two-key observation, and uses one binary collector. Each control refreshes only
pointer positioning before one click. Exact retained/published/conflict/choice identities,
consent/cancel, Save, single Undo/Redo, source archives and read-only/file preservation all
remain required, with original ten-second results and ninety-second whole-case bound.

After the four-step application/library build, native signed packages/source rebase/source
inspection pass 3/3 in 50.89 s (30.84/13.84/6.20 s). The original failed gates and the
negative old-library regression remain failed evidence. A final integrated gate is still
required; these focused passes do not authorize publishing or accepting physical hosts.


### Final integrated input/workflow gate

`cmake --preset linux-development`, `cmake --build --preset linux-development` and
`ctest --preset linux-development` all pass on the final integrated input candidate.
The complete standard suite is **365/365 pass, zero failures/skips, 1323.58 s**
(wrapper 1323.612 s). Configure takes 1.825 s
and all 150 build steps complete in 85.886 s. All
5,514 frozen source/asset/configuration hashes match after the suite;
manifest SHA256 is `4e060dc371308551b06302fa9de0b6d91ead6109d256d58deccdf18ed3911218`.
The configured cache uses `CMAKE_BUILD_TYPE=Development`, `NEXORA_LINK_MODE=Modular`,
`BUILD_TESTING=ON`, and graphical Editor, native backends, Vulkan and 2D enabled.
A fresh build must explicitly pass `-DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON`;
the standard configure command above preserves this existing feature-on cache.
The immutable local archive prefix is `marker-rebase-complete-standard`. Earlier failed gates and the
wrong-Hierarchy-observer focused attempt remain failed, separately retained evidence.

This accepts the scoped input/readiness/observation and failure-retention implementation in
this Linux run. It does not reconstruct historical causes lacking the required artifacts,
accept physical scanout/DPI/IME, or close the broader X11 investigation. Glyph goal 23 is
excluded and remains a separate candidate. Fresh exact-head CI and verified own-PR merge
remain required before delivery; ED-M0 target-host acceptance stays 0/8.
