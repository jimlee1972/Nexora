# Unity-style Editor input acceptance

Related work: [issue #759](https://github.com/jimlee1972/Nexora/issues/759), Beads `nexora-mhh`.
This supports the workflow epic; the nine-work-order denominator and 0/8 Editor milestones remain
unchanged. Status: implementation and scoped Windows tests pass; complete validation is pending.

## Behavior

Q selects Hand, W/E/R select Move/Rotate/Scale, X toggles axes and Z toggles Pivot/Center. The
explicit Select toolbar remains available; P remains a pivot alias. Native Scene navigation uses
Alt+left orbit, middle or Alt+Ctrl+left view-plane pan, right look, right+WASD/QE fly, Shift faster,
wheel zoom (fly-speed adjustment during right hold), and Alt+right dolly zoom. Right look preserves
the eye; orbit preserves the pivot. F frames, Shift+F follows selected world bounds at fixed
distance, Home frames all. Manual navigation or scope/focus/modal changes cancel following.

Ctrl/Shift-click toggles ordinary entity selection. Native hit testing admits tentative modified
drags only for actual transform handles; ordinary entity clicks cancel the transform request.
Ctrl activates incremental transform snapping; Shift rotation/scale snapping remains compatible.
The existing overview stays X/Z and supports Hand/right/middle panning without authoring.

Unity Play chords (Ctrl+P, Ctrl+Shift+P, Ctrl+Alt+P), Ctrl+1..5 panel focus, Ctrl+Shift+C Console,
Ctrl+R Project refresh, Ctrl+Shift+B Build Settings and Ctrl+B scoped Build & Run join existing
scene/save/history/object shortcuts. F5/F6/F10 remain aliases. Prefab isolation moves to
Ctrl+Alt+Shift+P to remove the actual Step conflict. Game capture keeps reserved Play chords and
filters unrelated authoring input. Text entry, application/window focus and modal admission remain
required. Build requests retain the existing owner's Save/Discard/Cancel and publication gates.

Rect/combined Transform tools, vertex snapping, orientation widget, maximized-view layout, and a
new XY 2D mode are outside this input change; no inactive shortcut pretends to supply them.
See [current controls](../../../../Apps/Editor/README.md#unity-style-editor-input-issue-759).

## Verification

The [Windows receipt](Unity-Input-Windows.json) records exact commands, eight passed cases, elapsed
times, log hashes, thirteen source hashes and six executable/library hashes. Final scoped CTest
exited 0: **8/8 passed, zero skips/failures, 70.19s** after the complete native target build exited
0. Real ImGui input tests exercise 1x/2x Hand, orbit/look/pan/fly/zoom, no accidental picks/history,
modified transform admission/cancellation, tool/axis/pivot guards, Shift+F/Undo/follow cancellation,
Unity Play chords and Game capture, and Build Settings/Build & Run/busy-build admission. Existing
selection, camera alignment and Game camera regression cases also passed. These are native
Windows process tests, not physical clean-host, full GUI presentation or final ZIP acceptance.

Two exploratory failures remain recorded locally: the Select test released after leaving its
button, corrected by real pointer release ordering; Ctrl+Alt+P was intercepted by the existing
Prefab shortcut, corrected by moving that shortcut. Final assertions remain present. Initial
build invocations named two nonexistent test targets; the corrected final build exited 0.

Full Codex Cloud development configure/build/CTest, applicable Shipping linkage, exact-head CI,
native Scene/Prefab interaction and final ZIP extraction/relocation remain pending. The input
changes are separate from immutable PR #758 Runtime validation and must not use its old results
as acceptance of these new controls. No new completion marker is earned yet.
