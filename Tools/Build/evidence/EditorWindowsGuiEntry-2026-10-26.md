# Windows Editor GUI entrypoint regression

The original f327bd86 Editor used PE subsystem 3 (console). Explorer therefore created both the
Editor window and a console, as the user's screenshot showed. Its startup/scene tests did not
check this distinction. That package is superseded by GUI-subsystem distributions.

Windows graphical builds now set WIN32_EXECUTABLE and enter through wWinMain. CommandLineToArgvW
preserves Unicode arguments, the common wmain/Run path handles GUI/CLI commands, and LocalFree
releases the temporary argument array afterward. Console-only and non-Windows builds retain
their existing entrypoints. No console is allocated then hidden. Existing background build
processes already use CREATE_NO_WINDOW.

✅ Local compiled PE reports subsystem 2 (Windows GUI); the packaging reader rejects the original
subsystem-3 executable. Six portable package tests cover dependency admission and valid/invalid
PE subsystem headers. The packager checks before staging and before relocated verification.
Every relocated Editor invocation requires the opt-in console_attached=0 process diagnostic,
while stdout/stderr capture, errors, Unicode creation and script/game build assertions remain.

✅ Actual developer-PC process acceptance uses pythonw.exe as a parent with no console. It starts
the Editor with creation flags 0, without DETACHED_PROCESS or CREATE_NO_WINDOW. The Editor reports
console_attached=0, presents eight frames and exits 0. This is a process-level console check and
native rendering result, not screenshot or general desktop automation. Receipt/logs:
build/editor-no-console-20261026/gui-parent/.

✅ Permanent CTest gates pass locally (2/2, 1.13 s): editor.windows_package_admission runs the six
portable tests, and Windows-only editor.windows_gui_entry checks the actual compiled PE plus
console-free entry, redirected help and Unicode CLI project creation. It sends no UI input and
uses its own temporary project/recents. This catches a future subsystem/entrypoint regression on
the actual target, in addition to synthetic parser coverage.

✅ Recompiled Windows Play/native/UI gate: 10/10, zero failures/skips, 14.51 s.
✅ Windows producer/ABI/CLI gate: 14/14, zero failures/skips, 182.87 s; editor.game_build 171.14 s,
including default-tool script CLI and worker cancellation. These confirm GUI entrypoint changes
preserve redirected command output and the shared builder. Logs retain the latest packaged-play
and phase4-gate prefixes under build/scene-settings/.

Required before final delivery: full exact-head Linux Development, Shipping and applicable CI;
source-matched replacement Editor ZIP; all archive hashes; actual extraction/Unicode relocation,
GUI startup without a console and script/game execution. Preserve original failed evidence.
The package filename records its compiled source commit and has a SHA-256 sidecar. The final
delivery receipt/checksum and CI/merge identifiers are retained in Beads nexora-4u9 and the task's
build/editor-no-console-20261026/ evidence. This replaces the old Windows artifact; the earlier
scene-delivery functional acceptance remains historical and full Editor milestones remain 0/8.

No independent clean-host, captured Windows pixel, arbitrary custom-game-format or complete
graphical milestone acceptance is claimed by this startup repair.
