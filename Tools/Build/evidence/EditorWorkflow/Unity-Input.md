# Unity-style Editor input acceptance

Related work: [issue #759](https://github.com/jimlee1972/Nexora/issues/759), Beads `nexora-mhh`.
This supports the workflow epic; the nine-work-order denominator and 0/8 Editor milestones remain
unchanged. Status: ✅ supported input implementation accepted and merged in
[PR #760](https://github.com/jimlee1972/Nexora/pull/760). The final nine-order Editor delivery
remains open; these controls do not complete the broader workflow epic.

## Behavior

Q selects Hand, W/E/R select Move/Rotate/Scale, X toggles axes and Z toggles Pivot/Center. The
explicit Select toolbar remains available; P remains a pivot alias. Native Scene navigation uses
Alt+left orbit, middle, Space+left or Alt+Ctrl+left view-plane pan, right look, right+WASD/QE fly, Shift faster,
wheel zoom (fly-speed adjustment during right hold), and Alt+right dolly zoom. Right look preserves
the eye; orbit preserves the pivot. F frames, Shift+F follows selected world bounds at fixed
distance, Home frames all. Manual navigation or scope/focus/modal changes cancel following.

Ctrl/Shift-click toggles ordinary entity selection. Native hit testing admits tentative modified
drags only for actual transform handles; ordinary entity clicks cancel the transform request.
Ctrl activates incremental transform snapping; Shift rotation/scale snapping remains compatible.
The existing overview stays X/Z and supports Hand/right/middle panning without authoring.

Tools and X/Z shortcuts work across Editor panels after same-frame text/item ownership is resolved.
Ordinary tool keys resolve at EndFrame, after auxiliary windows also establish ownership; only an
owning per-frame admission bit is retained, never a borrowed Scene pointer. Actual auxiliary InputText
activation and character delivery are tested at 1x/2x. Scene initial-click selection retains its
pre-pick ordering.
The Scene initial-click path still resolves tool selection before authoring starts. Normal no-project
interactive startup prefers native3D, with one-time Unsupported fallback; explicit native/CLI choices
keep their existing admission. The final candidate passed native and package startup acceptance below.

Unity Play chords (Ctrl+P, Ctrl+Shift+P, Ctrl+Alt+P), Ctrl+1..5 panel focus, Ctrl+Shift+C Console,
Ctrl+R Project refresh, Ctrl+Shift+B Build Settings and Ctrl+B scoped Build & Run join existing
scene/save/history/object shortcuts. F5/F6/F10 remain aliases. Prefab isolation moves to
Ctrl+Alt+Shift+P to remove the actual Step conflict. Game capture keeps reserved Play chords and
filters unrelated authoring input. Text entry, application/window focus and modal admission remain
required. Build requests retain the existing owner's Save/Discard/Cancel and publication gates.
Native macOS uses the existing Command-primary policy. Both Windows and macOS policies are tested
at 1x/2x, including modifier release while P remains held. Reserved Ctrl/Command+P is consumed by
Editor Play input until P is released, so it cannot also fire a P-bound game action or leak through
autorepeat after releasing its modifier.

Rect/combined Transform tools, vertex snapping, orientation widget, maximized-view layout, and a
new XY 2D mode are outside this input change; no inactive shortcut pretends to supply them.
See [current controls](../../../../Apps/Editor/README.md#unity-style-editor-input-issue-759).

## Verification

The [Windows receipt](Unity-Input-Windows.json) records exact commands, ten passed cases, elapsed
times, log hashes, twenty source hashes and eight executable/library hashes. Final scoped CTest
exited 0: **10/10 passed, zero skips/failures, 76.65s** after the complete native target build exited
0. Real ImGui input tests exercise 1x/2x Hand, orbit/look/pan/fly/zoom, no accidental picks/history,
modified transform admission/cancellation, tool/axis/pivot guards, Shift+F/Undo/follow cancellation,
Unity Play chords and Game capture, and Build Settings/Build & Run/busy-build admission. Existing
selection, camera alignment, Game camera, real 2D services and packaged Play regression cases also passed. These are native
Windows process tests, not physical clean-host, full GUI presentation or final ZIP acceptance.

Three exploratory failures remain recorded locally: the Select test released after leaving its
button, corrected by real pointer release ordering; Ctrl+Alt+P was intercepted by the existing
Prefab shortcut, corrected by moving that shortcut; the global tool follow-up initially reached an
Inspector field before same-frame text ownership existed, corrected by resolving ordinary tool keys
after widgets. The original text assertion passes at both DPIs. Overview Hand/Alt+RMB and global
Hierarchy tools/Space clutch are also exercised. Final assertions remain present. Initial
build invocations named two nonexistent test targets; the corrected final build exited 0.

## Final immutable acceptance

Validated commit `c9f1717085078b216913cd1096427c5aa42c6946`, tree
`84b3867ea2e9014bead151dec733d210f6bfc8ef`, merged by squash
`92ae6bc7447ce35a9abc8f3887b07d57db55b197`; the merged tree equals the validated tree.
[Cloud receipt](Unity-Input-Cloud.json) records exact argv, exit codes, inventory and log hashes.
Development configure/build/full CTest all exited 0: **495/495 passed, zero skips/failures**,
1890.31s. Supported Shipping Runtime/Player configure/build also exited 0; Shipping disables
Editor and testing, and is not Editor acceptance. No test retries or inventory changes occurred.
Postflight found zero residual validation processes and no OOM kill; older evidence archives stayed unchanged.

[CI receipt](Unity-Input-CI.json) records **42/42 successful checks** at that same head. All four
workflows completed successfully before merge, including both shader-preview jobs. Native Windows
ran 437 registered cases: 436 passed, one existing optional portable-font dependency skip, zero
failures (1455.99s). Native macOS ran 411: 410 passed, the same optional skip, zero failures
(928.55s). The skip requests `Tools/TwoD/font-requirements.txt`; it is not an input-test skip.
Linux Xvfb Editor acceptance ran **246/246**, zero skips/failures (1447.68s), including real Scene,
Prefab, Play memory, rendering, scene-byte isolation and save/reopen interactions. These native
results are separate from the Cloud full gate and from historical PR #758 results.

The Windows package job passed **5/5** startup/branding/GUI-entry tests (28.50s), including actual
extracted/relocated Editor and Player, two no-argument startup cycles and no attached console.
The inner ZIP `NexoraEditor-Windows-x64-c9f171708507.zip` is 115761152 bytes, SHA256
`0133f51f51c0470aa235e1457c81d7103dd8203a58b4650cf305bcf92d1eb1ae`.
All 22697 inventory files passed hash/CRC/path verification; no missing or foreign files were found.
Only `NexoraEditor.exe` is at the root; required private Runtime/SDK tools remain. No engine/game
source checkout is included. This is the accepted input candidate, **not the final nine-order ZIP**.
Physical clean-host/device acceptance remains NOT_RUN. Historical failures below remain preserved;
their statements about pending reruns describe those superseded candidates, not this final result.

## First immutable candidate failures and repairs

Commit `99db277558cb2616850aca9503819e88f99060ff`, tree
`567096d379e786011e5893055d835ee08507ce03`, is unaccepted. Cloud ran all 495 cases:
492 passed / 3 failed / zero skipped, CTest exit8, 1756.448s. Configure/build exited0.
Original archive `/workspace/validation-759-evidence.tar.gz` is retained unchanged, 414063 bytes,
SHA256 `2fe20c077a83033779ad09dc47f358186e1657952ca4af5aaec939b69b2f01d0`.

- Prefab isolation requested the new Shift chord but its physical key mapping lacked Shift.
  The repair adds real Shift down/up acknowledgement via opt-in observation, preserving the
  original fixture, source isolation, save/reopen assertions and deadlines.
- Native Scene's final gesture still used old RMB orbit / Shift-MMB vertical pan semantics and
  required zero X/Z movement. It now performs Alt-LMB orbit and MMB view-plane pan and requires
  finite, changed pitch/height/distance and nonzero horizontal target movement, followed by exact
  camera persistence across reopen. Existing actual handle/pixel/Undo/scene-byte checks remain.
- Play memory did not observe the fixture starting after a short unacknowledged F5 press. The
  repair observes actual focused Play admission and one physical key's processed down/up phases,
  without retrying Play or extending the existing result deadlines. Runtime allocation, pixels,
  pause/stop/restart and source-byte assertions remain the result oracles. Full Linux rerun is pending.

The same candidate's native macOS `editor.play_input_state` failed because captured input dropped
Command+P. Job `124756081826` exited8 overall; raw decoded log is retained locally as
`build/editor-workflow-native/unity-input-macos-99db-failure.log`, 792584 bytes, SHA256
`9672360c418e6847ec11a1940e262c636cc86dfcb09032c9dd4a7f9fd669affa`.
The repair and expanded Windows/macOS-profile tests do not substitute for new native macOS CI.

The requested first Shipping command incorrectly enabled graphical Editor, despite the existing
Shipping contract forcibly disabling Editor. Configure/build each exited1, before Editor linkage.
Keep this failure; do not change product ownership to accommodate a validation mistake. The next
run uses the supported Shipping Runtime/Player boundary (Zig/Slang/Vulkan/ProjectPlayer/native ON,
graphical Editor OFF). Development continues to compile/link the actual graphical Editor and run
its complete native suite. Shipping has testing OFF and does not validate an Editor executable.

## Services Input admission follow-up

The next candidate `07afb671f61a0aaf5b52f8045bc716c765feef40` passed the existing scoped Windows
8/8 gate and native Windows package startup/branding/entry checks (5/5, exit0,19.45s), including
two actual no-argument startup/shutdown cycles after ZIP extraction and relocation. Its downloaded
inner ZIP is 115761001 bytes, SHA256
`5bfdb6ebb47fac900906a7e319184e8bfb3bba306b7a5973da4df8adc8d0cdb6`.
All 22697 inventory-listed files were hash/CRC/path checked locally, with zero missing/foreign files.
The only root EXE is NexoraEditor.exe; private Runtime Player and build tools are required Editor
dependencies. No engine/game source checkout is included; compiler/CMake resource sources stay
inside the required private Tools tree. Physical clean-host acceptance remains NOT_RUN.

That source is still **unaccepted**: a new actual host `get_services_api` action-binding regression
reproduced Ctrl/Command+P firing a P-bound 2D game action. The original reproduction is retained as
`build/editor-workflow-native/unity-input-twod-repro-test.log`, exit8,0.36s. Legacy PlayInputState
already consumed the chord, but PlayTwoDServices received the original raw events. The repair shares
one Editor-only, allocation-free filter across both providers, preserves reserved-P consumption
through acquisition and modifier-release repeats, and reserves F5/F6/F10 aliases. The same real
services test must prove ordinary P recovery, focus revocation and nonreserved F2 delivery across
all 32 clone lifetimes. Existing animation, pool, native frame, source/scene isolation and cleanup
assertions remain. Shipping Runtime Input behavior and public C/Zig ABI are unchanged.

The candidate's unrelated `modern-shaders-preview` job `124765137921` failed before compile while
PrepareD3D12Preview downloaded an SDK (WinError10060). Its raw log is retained, SHA256
`76622f271f7ade4d971d347a6955edca78d23eca842d442493add1a61226772d`,57362 bytes. GitHub rejected
an immediate per-job retry with HTTP403 because run41278360329 was still running. This does not
waive the check: the repaired source requires a new complete Cloud/CI run and successful final-head
shader validation. Do not retry/merge the superseded source as final acceptance.
