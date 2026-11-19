# ED-WF-07: integrated Editor Sprite source authoring

Work order [#748](https://github.com/jimlee1972/Nexora/issues/748), under
[Epic #741](https://github.com/jimlee1972/Nexora/issues/741). Date: 2026-11-19.
✅ This record accepts integrated Editor Sprite source authoring, bringing workflow acceptance to
**6/9 (66.7%)**. Complete graphical Editor milestones stay **0/8** and TwoD stays **9/11**.
The corresponding PR must pass final-head review and merge guards before issue/Epic acceptance
is updated; complete animation/reload and final Windows delivery remain ED-WF-08/09.

## Same-project native acceptance

The new `editor.linux_sprite_source_workflow` case uses a real graphical Editor, acknowledged native
input and native X11/Vulkan pixels. It opens `Authored.scene` through the actual Open Scene modal;
that exact file is the one enrolled in `project.build.json`. It does not copy another saved scene
over the Build input. The fixture supplies source/Atlas/Clip assets. Assignment, Atlas slicing and
Clip edits use ordinary Editor controls; explicit external source changes exercise real Content
Reimport, including corrupt and missing-file rejection.

| Required behavior | Executed proof and regression coverage |
| --- | --- |
| Project index and Inspector assignment use the common source identity | Actual Scene picking selects First. Clear, assignment and their Undo/Redo use Inspector/history controls. GUI Save must write the exact expected bytes to the enrolled scene; the source UUID and all identity sidecars remain fixed. Existing `editor.sprite_source_catalog`, `editor.sprite_source_inspector` and `editor.linux_sprite_source_inspector` retain malformed/read-only/stale request guards. |
| Save/reopen and isolated Play use the same authored source | The writer closes normally; a new Editor opens the enrolled scene. Four Play/Pause/one-Step/Stop sessions check five independently projected Game pixels each. Configured SceneOnly Play owns the packaged provider; no gameplay module is loaded. Stop retires its textures/commits and preserves exact authoring scene bytes and native Scene pixels. The separate module-free and actual SDK-module cases remain in the full inventory. |
| Atlas slicing and Clip editing survive publication | The real Atlas editor moves region7 from x2 to x10 without changing UUID, dimensions or image bytes; Scene and Play change from red to green. The real Clip editor saves duration/event/reorder changes, then Cancel rejects another draft. Exact saved wire values are decoded independently; the exact edited Clip must appear in the runtime closure. Existing actual 1x/2x draft/history/source-frame tests remain. |
| Reimport publishes one current revision, preserving identity and selection | Six real Content Reimport actions publish red, green and two restorations, and reject corrupt/missing primaries. Success requires exactly one Content revision increase; failures retain the previous revision, authored bytes, sidecars, selection and last-good Scene pixels. Corrupt data fails the worker; a missing primary is rejected before enqueue with the actual actionable error. Project generation stays fixed. |
| The same saved scene/assets reach a relocated independent Player | CLI Build consumes the exact GUI-saved enrollment and edited Atlas/Clip. Build cannot mutate the authoring tree. The runtime directory moves to a Unicode path; the entire authored project is archived and deleted before launching Game from an unrelated working directory. A native Player pixel matches independently computed Game output within one code value. Normal shutdown, native drain, no Vulkan validation failures and unchanged runtime hashes are required. |
| Crop, Pivot, mirrors, negative scales and retained immutable generations | Existing [source detail evidence](ED-WF-07-SourceDetails-Linux-2026-11-14.md), [source editing evidence](ED-WF-07-SourceEditing-Linux-2026-11-15.md), [Scene cutout evidence](ED-WF-07-SceneSpritePreview-Linux-2026-11-14.md) and [source snapshot evidence](ED-WF-07-SourceSnapshots-Linux-2026-11-12.md) map to the unchanged full-suite tests. They cover source/Atlas closure, affine geometry, alpha-aware picking, malformed dependencies and old snapshots after owners are released. The [accepted source/repack Player contract](ED-WF-06.md) remains the common runtime contract. |
| Live configured publication retains old generations until normal retirement | Fresh complete-gate `editor.linux_packaged_source_reimport` passes 32 cycles / 70 native publications with independent source/pool/3D masks, saved reopen, corrupt/missing retention and half-frame clock preservation. `editor.linux_packaged_source_reload` passes 33 actual SDK replacements, seven rejected candidates, obsolete-result rejection, 38 library finalizers, an 80-byte state payload and Stop cancellation. |

### Independent color oracle

The public source texture uses linear RGBA8. Configured Game and the independent Player present
through the sRGB attachment. The test calculates IEC61966-2-1 encoding independently:
`v = channel / 255`; output is `12.92*v` below `.0031308`, otherwise
`1.055*v**(1/2.4)-.055`, rounded to eight bits. Linear `(240,20,20)` must present as
`(248,79,79)`; green presents as `(79,248,79)`, with at most one code value of rounding difference.
Five world probes use the public default camera and fixture bounds. The Scene canvas has a
different composition path; its before/after isolation and failed-import retention checks compare
its own exact native pixels rather than equating that path to Game output.

## Frozen validation inputs

Base main `4c87033615fe1aece699b0e133e1d80e140ec8af`, tree
`936cab1019ae90b8551aed51f0cc39bfe46ec020`. Baseline plus the four frozen test inputs has tree
`0cc55163f3822771949e1c8f039d6a8401922450`; this is a validation-input tree, not a published commit.
The [machine receipt](ED-WF-07-Cloud.json) records source hashes, all 560 results, gate/log hashes,
targeted attempts and fresh full-gate native receipts. It distinguishes the source validation tree
from the later documentation/PR tree; final delivery additionally checks exact merged-tree equality.
Only Python native acceptance helpers and one CTest registration change. There is no production
linkage, ABI, ownership or lifetime-boundary change; a new Shipping build is not required for this
test-only patch. Existing runtime/package, module graph, `.bat`, CLI and SDK paths remain intact.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
LP_NUM_THREADS=2 CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development --output-junit /workspace/work/editor-sprite-workflow/full-junit.xml
```

Configure, full build and unfiltered CTest all exit **0**. CTest passes **560/560 in 5913.08 s**,
with **zero failures, skips or retries**. The final four source hashes remain unchanged.
The same-project native case passes **191.49 s**, actual SDK reload **1684.38 s**, source/Atlas
reimport **383.03 s**, Scene Sprite native regression **103.93 s**, and actual Hierarchy context
actions **4.62 s**. Existing Inspector, authoring/history, source geometry, standalone Player,
configured/module-free Play and native 1x/2x interaction cases all pass in that inventory.
External logs, complete command inventory, JUnit, final exit receipt and original native attempts
remain under `/workspace/work/editor-sprite-workflow/`. Only acceptance documentation and raw
evidence captures are finalized after the gate; implementation/test inputs remain frozen.

Seven raw full-gate native captures preserve the actual windows:
[assignment](ED-WF-07/workflow-assigned.png), [reopen](ED-WF-07/workflow-reopened.png),
[red Play](ED-WF-07/workflow-play-1.png), [green Play](ED-WF-07/workflow-play-4.png),
[corrupt rejection](ED-WF-07/workflow-rejected-corrupt.png),
[missing rejection](ED-WF-07/workflow-rejected-missing.png) and
[source-deleted relocated Player](ED-WF-07/workflow-relocated-player.png).
Their original SHA-256 values match the native result receipt; they are not edited composites.

## Retained investigation history

Native attempts1–7 remain failures with their original logs and result receipts. They exposed test
assumptions or input observation gaps: using the module-free facade for configured Play, comparing
linear texture bytes directly to sRGB presentation, unacknowledged Scene focus, a wrong diagnostic
environment flag, a popup's initial sizing-frame coordinates, and treating missing-file admission
rejection as a queued worker failure. These are not asserted to be production regressions.
No pixel tolerance, native deadline or authored-data requirement was weakened. Stable popup
geometry is observed before one actual click; missing/corrupt paths require their specific errors.
Focused attempts8 and9 pass in **178.84 s** and **174.95 s**. Attempt9 includes exact successful
revision increments, unchanged rejected revisions and retained selected entity2. The final frozen
source additionally compares failed-import Scene pixels exactly to the preceding native sample;
the final-source case passes within the complete full gate in **191.49 s**. It verifies four
Play sessions, six Reimport actions and matching relocated Player pixels, including that last
exact-retention assertion. Earlier targeted passes do not substitute for the final-source gate.

Historical partial Atlas/Clip gates and explicit recoveries remain separate in the
[original authoring record](ED-WF-AtlasClip-Linux-2026-11-16.md) and
[review followup](ED-WF-AtlasClip-Review-Linux-2026-11-16.md). This acceptance does not
retroactively convert those failures into an all-green original run.

## Delivery and limits

Final-source full validation has passed. Documentation unit checks pass **16/16**, and all **664**
repository Markdown documents pass link/bilingual validation. The root README changes only its
existing overall workflow count. Final-head review and exact source/base/tree checks remain
required before the agent's own PR can merge. Routine CI stays manual-only;
no PR/push workflow is dispatched. Work-order/Epic acceptance changes follow the reviewed merge.
ED-WF-05, ED-WF-08 and ED-WF-09, independently owned reload tasks, complete graphical Editor
milestones and physical/clean-host gates remain separate. Native execution here is Linux Xvfb
with software Vulkan; no Windows, macOS, Android, iOS or physical-device execution is claimed.
