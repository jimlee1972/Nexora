# ED recent-project record admission: Linux evidence

Date: 2026-10-07 (Asia/Taipei). Parent: `6a1ffb7a1d36ebfd0023dc198f237ee80e275600`.

RecentProjectStore's reader validates UTF-8 root/name fields with a 1024-byte limit. Record
previously accepted a valid project with a longer canonical root and replaced the last-good
store with a record the reader could not reopen. Admission now uses the same validation before
changing retained entries or writing a stage. Unsupported fields produce an error and preserve
both persisted and in-memory state.

The Linux extension to `editor.workspace_budget` creates an actual project root longer than
1024 bytes. It verifies rejection, unchanged store bytes and entries, absent staging, and
successful reopening of the original list. The same fixture against the parent Editor library
fails: `unsupported recent-project root replaced the last-good store or model`.
Other hosts may reject long paths at their filesystem boundary, so the long-root fixture is
Linux-only; existing recent-store portable tests still cover ordinary record/reopen behavior.

Environment matches [import admission](EditorEDM1-ImportAdmission-Linux-2026-10-07.md).
Final commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development -R 'editor\.(workspace_budget|preview_contract)$'
ctest --preset linux-development
```

Configure/build passed with the cached isolated X11/Vulkan sysroot, graphical shell and
Slang 2026.18 enabled. Focused tests: **2/2 passed**. Full CTest: **120/120 passed**, none skipped,
204.48 seconds. Lavapipe and Khronos core/synchronization validation were enabled using the same
VK_DRIVER_FILES/VK_LAYER_PATH/VK_INSTANCE_LAYERS/VK_LAYER_ENABLES settings as import admission.
`git diff --check` passed. No dependencies, public API/layout or linkage boundaries changed.

Source SHA-256:

- `Engine/Editor/src/ProjectWorkspace.cpp`: `8fb967afdc89be402f611e11b0ae4b72e9c9df98a3aeac73a33296b86ce8a1c6`
- `Tests/Editor/WorkspaceBudgetTests.cpp`: `45c1eddb7ab91aea78da10e969e2674a5b4d08875162a331a52eafb580f46490`

Physical-display, installed IME and complete graphical milestone acceptance remain open.
Hosted target validation is separate evidence and must pass before merge.
