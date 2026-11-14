# Startup diagnostic follow-up

PR #763 at `0f901cb3` failed Windows package job `125338978442`: the original
35-second no-argument startup deadline expired. The preserved artifact records splash-show
and main-create, no callback errors, and workspace stage 4; there is no prepared-frame or main-show
event. Both test and input desktops match. This is not the previous isolated-desktop failure,
and the root cause remains undetermined. The main HWND stayed hidden and no console attached.

The [receipt](ED-WF-05-Startup-Diagnostic.json) retains the downloaded artifact and local log hashes.
Opt-in startup evidence now records at most 64 first-frame phase checkpoints. A deadline failure
includes the bounded Editor stderr and externally observed events in CTest output. No timeout,
render assertion, input check, required test or visibility policy changed. This adds diagnostics;
it does not establish a fix or waive the failed CI.

The affected NexoraEditor target rebuilt with exit 0. The original native startup harness ran two
real no-argument launches and exited 0, retaining all response, pixel, order and shutdown checks.
Commands are `cmake --build build/editor-workflow-native --config Development --target NexoraEditor
--parallel 2` and `python Tests/Editor/WindowsStartupIntegration.py
build/editor-workflow-native/bin/Development/NexoraEditor.exe
build/editor-workflow-native/ed-wf05-startup-checkpoint-native`, with explicit
`NEXORA_NATIVE_TEST_INPUT_DESKTOP=1` for this test only. This is local Windows evidence; full
immutable-head Cloud and CI remain required. Work-order acceptance stays 4/9 and Editor 0/8.
