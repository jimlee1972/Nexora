# Main Editor interleaved source publication and SDK replacement

Supporting task `nexora-whr.8.3.9`, based on accepted ED-WF-07 main
`21be880c49d53f25cde4d0ea69062c714af91382`, tree
`d6279e8bdf6d805f6e7026d57411b1cdd4859907`. ✅ Final-source supporting acceptance passes
the complete 561-case Linux gate. This does not accept ED-WF-08 or its parent
8.3 dependency, change the independent 8.2/ED-WF-05 claims, or raise workflow **6/9**, graphical
Editor **0/8** or TwoD **9/11**. The root README remains unchanged.

## Verified gap and executed scenario

Existing configured main Game source-reload and source-reimport native modes are mutually
exclusive. Each already validates its own transaction, but their separate sessions do not establish
that live source publication composes with an in-flight SDK result. The host prioritizes a script
result at the native frame boundary and stages source publication on a following frame; both retain
the existing Play World and pool. No production behavior changes in this supporting patch.

The new `editor.linux_packaged_source_interleaved` case uses the same real Editor, canonical SDK,
saved Zig fixture, Content commands and independent native source/pool/3D oracle. It runs one
writable configured Play session without Stop/Play between updates:

1. Start the real SDK module and acquire/release ordinary Game input. A source-free native Editor
   supplies the independent central 3D baseline. The shared fixture independently predicts source
   trim/Pivot/mirror/negative-scale/pool color masks from the public vertical20 camera.
2. For four successive saved script generations, wait for actual compiler progress, then publish
   Atlas and Sprite-source variants through real Content Reimport. Source output must match the
   new independent mask before candidate admission. Actual module Update tick logs must advance
   in the old module, and the admission journal must remain unchanged during this interval.
3. Require actual Runtime publication and the first dispatched Update of each new module, exact
   increasing reload counters and ticks beyond the last old-module tick observed after source
   publication while admission was pending, unchanged scene/entity/pool owner/index/generation, independent Game
   pixels and unchanged authoring pixels across the module replacement. Candidate-hook logs alone
   cannot satisfy the live-module oracle.
4. During the next real compilation, corrupt the Atlas primary and require worker failure with
   retained native Game/authoring pixels. Restore and publish it. The compiled candidate mutates a
   hidden pool entry to an unresolved source; it must reject at live generation5, never dispatch,
   and leave the old module's Update ticking with the last-good native output.
5. During valid recovery compilation, remove the Sprite-source primary and require the ordinary
   missing-file admission error with retained native output. Restore it, publish new Atlas/source
   variants while admission is still pending, then accept code6 as the fifth successful replacement.
   Restore original source bytes through two further real Reimports: **13 publications** total.
6. Observe actual finalizers for retired/rejected code0–5. Continue through all five native
   animation/Additive/unload/Replace phases using the same pool handle. The first remaining
   half-frame advance must select the independently expected next source frame, exposing clock
   resets across the combined updates. Ordinary Stop/Destroy and native shutdown must drain.

The fixture adds periodic tick logging to its temporary authored Zig source to observe actual
Update dispatch. It does not add a product admission hook, pause the compiler, bypass Content,
change the source asset codecs or count a provisional candidate as live. Saved script/source
edits have explicit expected hashes; unrelated scenes, settings, sidecars and authored data stay
exact. Restored asset primaries match their initial bytes. Existing separate 33-replacement and
70-publication scenarios retain their original commands, assertions and deadlines.

## Frozen inputs and focused result

Baseline plus the three frozen test inputs has validation-input tree
`c126c7e3255e1849333c83d13fc648e95e95a6a3`. This is not a published commit/tree for later docs.
The [machine receipt](ED-WF-08-Interleaved-Linux.json) records all three SHA-256 values, the native
receipt and raw capture hashes. Changed inputs are CTest registration, the existing native driver
and the new independent interleaving helper. No production linkage/ABI/lifetime boundary changes;
this test-only patch does not require a new Shipping build.

```bash
ctest --preset linux-development -R '^editor.linux_packaged_source_interleaved$' --output-on-failure
```

With `LP_NUM_THREADS=2 CTEST_PARALLEL_LEVEL=1`, the first focused run passes **349.45 s**, exit0:
five actual successful SDK replacements, one hidden-pool rejection, 13 native source publications,
corrupt/missing retention and valid recovery, six observed library finalizers, five exact native
scene phases, stable handle, unchanged 3D and controlled authoring hashes, normal Stop/Destroy.
There are no failed or retried focused native attempts. That run precedes the last counter
assertion: a read-only supplement confirms its recorded first-Update counters exceed the last
old-module ticks for all five replacements, including recovery 4676 versus the last old tick 4672.
The final helper additionally enforces and records this relationship in the native test itself;
only its complete gate qualifies that strengthened source.

The first complete-gate attempt was deliberately interrupted after 54 completed cases, before
the interleaved case, to strengthen that counter assertion. Its exit 130, logs, partial test log
and exact original three-file snapshot remain under `interrupted-before-late-counter-check/`.
It is not a complete-suite success or a failed-case retry. The replacement gate starts from the
beginning with the final frozen inputs; no deadline or oracle is weakened.

## Complete gate and delivery

The final frozen inputs pass the unfiltered gate:

```bash
cmake --preset linux-development
cmake --build --preset linux-development
LP_NUM_THREADS=2 CTEST_PARALLEL_LEVEL=1 ctest --preset linux-development --output-junit /workspace/work/editor-interleaved-source-updates/full-junit.xml
```

Configure, full build and CTest all exit **0**. The final complete run passes **561/561 in
6562.32 s**, with **zero failures, skips or failed-case retries**. All three frozen source hashes
remain exact. The fresh interleaved native case passes **367.14 s**, with five actual replacements,
13 source publications, two rejected source updates, one rejected hidden-pool candidate, actual
continued old-module dispatch, six finalizers and all five scene/animation phases. The existing
separate SDK reload case passes **1706.19 s** (33 replacements/seven rejected candidates), and
source reimport passes **398.61 s** (70 publications, half-clock, Build and saved reopen).

The final native receipt enforces the counter relationship for every published replacement:

| Saved fixture code | Last observed old-module tick while admission was pending | First dispatched new-module tick |
| --- | --- | --- |
| 1 | 160 | 798 |
| 2 | 960 | 1628 |
| 3 | 1744 | 2333 |
| 4 | 2480 | 2996 |
| 6 (recovery after rejected code5) | 3920 | 4497 |

The source-free [baseline](Interleaved-Source-Updates/legacy.png),
[initial SDK frame](Interleaved-Source-Updates/phase-0.png),
[first interleaved update](Interleaved-Source-Updates/interleaved-1.png),
[fourth update](Interleaved-Source-Updates/interleaved-4.png),
[recovered frame](Interleaved-Source-Updates/interleaved-recovered.png),
[remaining half-frame advance](Interleaved-Source-Updates/phase-1.png) and
[final scene replacement](Interleaved-Source-Updates/phase-4.png) are raw native full-run PNGs.
Their SHA-256 values match the original native receipt. No image editing or composite is used.

Logs, complete inventory/JUnit, original focused and interrupted-run records, full native receipts,
source hashes and final exit receipt remain under `/workspace/work/editor-interleaved-source-updates/`.
Only evidence/documentation is finalized after the gate; test implementation remains frozen.
Documentation checks pass **16/16** unit tests and all **665** repository Markdown documents.
Final-head Codex review, all applicable checks and exact base/head/tree/merge guards remain
required for the agent's own PR. Routine CI stays manual-only,
with no push/PR workflow dispatch. Root README and milestone counts remain unchanged.

Execution is Linux Xvfb/software Vulkan. Standalone TwoD Player strict reload, complete ED-WF-08,
physical-device/clean-host and Windows/macOS acceptance remain separately scoped. Prior source/
reload failures and historical supporting records remain intact.
