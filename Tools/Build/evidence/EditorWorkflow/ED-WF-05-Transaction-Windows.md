# ED-WF-05 main Editor generation transaction

Status: local native slice validated; full immutable-head Cloud/Shipping/CI and complete
Editor automatic-compilation workflow remain pending. This does not accept work order05 or
change the4/9 accepted count. [Machine-readable receipt](ED-WF-05-Transaction-Windows.json).

The main Editor now uses ReloadablePlayGameplayModule, with generation-owned host/service
proxies, allocation ledgers and strict Runtime capture/begin/validate/retire/commit/rollback hooks.
Registered work drains before state capture. Old callbacks/finalizers retain quarantined World
and resource owners; candidates use unpublished copies. Configured project Play also copies its
complete GameWorld and GameSceneSession, preserving scene mappings, pending requests, audio,
physics and character state. Native audio/device and texture ownership transfer without reopening
or invalidating handles. Capture/external-write attempts and unfinished candidate UI frames reject.
Closed reloaded-hook UI snapshots publish without advancing gameplay. Session memory peaks and
counters survive module replacement; only final session teardown advances its observation serial.

## Local commands and results

Fourteen affected native targets, including the actual GUI Editor, build successfully (exit0).
The23-case scoped CTest cohort passes (exit0,zero failures/skips,58.31s), covering Runtime
C++/real Zig reload, configured-scene SDK modules, compiler coordination, 2D resource/input/audio,
actual main facade and existing Play/memory/scene regressions. The main C++ transaction fixture
performs96 successful replacements (32 plain,32 configured project,32 2D),576 expected rejections,
451 creates/451 destroys and99 stops, with actual work leases, stable handles, private cleanup,
queued scene requests, storage rejection and immediate closed UI publication. It is not32 actual
Editor source edits and does not establish physical audio/GPU acceptance.

The real built NexoraEditor.exe passes two no-argument native launches: responsive splash,
main hidden until prepared, no white reveal/console, actual foreground input, exit0 on normal
WM_CLOSE and no orphan windows. Existing35s startup/500ms response/10s shutdown limits and all
pixel/event/order assertions remain. First local screen captures failed because command and
input desktops differed; original logs remain retained, and unrelated desktop imagery is not
published. The harness now verifies owned-window visibility and normal Z-order before capture,
refuses input to occluded foreign windows, and supports explicit opt-in selection of the existing
input desktop for its own thread/children without switching the user desktop. The passing run
records matching thread/input desktops and no desktop switch.

## Remaining delivery

Connect latest project/source/Play watcher/compiler admission and cancellation/drain to real
Editor source edits, verify32 actual Zig changes with native responsive gameplay and resources,
then run the full exact-head Linux Development/Shipping and applicable CI. Sprite source work
orders06-08 and final native/extracted/relocated Editor ZIP09 remain pending. Keep all original
failed compiler/XKB/lifecycle/startup records and the merged foundation receipts.
