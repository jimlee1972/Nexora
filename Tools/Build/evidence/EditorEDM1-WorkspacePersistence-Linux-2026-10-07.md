# ED workspace persistence: Linux evidence

Date: 2026-10-07 (Asia/Taipei). Parent: `bbb505d469174d237817f76ac05f17ddb234ee59`.

ProjectWorkspace used a separate writer that truncated occupied `.tmp` paths and removed a
failed rename destination before retrying. A public-API probe reproduced both an overwritten
staging file and deletion of an empty destination directory. The new regression fixture failed
against that writer at the occupied-stage assertion.

Workspace-owned writes now delegate to the existing scene/asset replacement helper after the
existing parent-directory validation. Occupied regular files, directories, symlinks and dangling
symlinks are preserved and rejected, with the stage path in the diagnostic. Replacement failure
preserves the destination and cleans only this call's stage. The helper uses POSIX rename or
Windows MoveFileExW replace-existing behavior; the latter is implemented but not locally run.

Tests cover settings, layout destination failure and retry, workspace file/model preservation,
explicit recovery from the retained journal, recent-project entry rollback, and descriptor upgrade
preservation/retry. POSIX symlink cases are excluded on Windows, which may lack symlink privilege.
This retains the single-writer contract and does not claim exclusive temporary-file creation or
crash-during-write injection.

Environment and local dependency setup match
[import admission](EditorEDM1-ImportAdmission-Linux-2026-10-07.md).
Final commands and results:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development
```

Configure/build passed with the cached isolated X11/Vulkan sysroot, graphical shell and
Slang 2026.18 enabled. CTest: **119/119 passed**, none skipped, 202.31 seconds.
The full run configured lavapipe and Khronos core/synchronization validation using the same
VK_DRIVER_FILES/VK_LAYER_PATH/VK_INSTANCE_LAYERS/VK_LAYER_ENABLES settings as import admission.
`git diff --check` passed. No linkage boundary changes or generated files are committed.

Public-API probe before/after:

```text
--- after ---
occupied_stage_save=0 stage_preserved=1 setting=Content/original.so
directory_destination_save=0 directory_preserved=1
```

Source SHA-256:

- `Engine/Editor/src/AtomicFile.h`: `b265df2b05bf81ac02098c550674fea8baa7ce323c71dab7b50c683e8f646079`
- `Engine/Editor/src/ProjectWorkspace.cpp`: `f08a24a650bb9dea11bc5e4e4f2478626dab17b6d79b72d7a3fa03276851ff9d`
- `Tests/Editor/EditorTests.cpp`: `010de53fc26d46c73e95573ebc964add9f9cb2332fde3ea9ce14bb4da5b788a4`

Physical-display, Windows/macOS/mobile, installed IME and full milestone acceptance remain open.
