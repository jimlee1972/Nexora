# ED workspace record budgets: Linux evidence

Date: 2026-10-07 (Asia/Taipei). Parent: `5faacff0e298a0ecc54d633faa356473220048b5`.

Workspace saves previously accepted more than the reader's 4096-document limit, publishing a
workspace that could not reopen. Workspace and recovery parsers used unbounded `std::getline`
before validating the path length. An unavailable primary file was also treated as an absent
legacy workspace. The new `editor.workspace_budget` fixture failed against the parent writer:
`over-budget save changed committed state, journal or staging`.

Workspace/recovery now share a fixed-size line buffer and public limits of 4096 documents and
1024 UTF-8 bytes per path. Save validates the complete input before any journal/stage write.
Only absent legacy workspaces load as empty; unreadable/non-regular metadata and file symlinks
reject. Candidate documents publish only after complete validation. Recovery closes the reader
before writing the primary file and removes the journal only after successful publication.

The portable test covers maximum-size save/reopen, maximum-length CRLF records, final records
without LF, rejected 4097-document saves, 4 MiB corrupt records/headers, embedded NUL, invalid
schema, empty entries, preserved journals/model/files, explicit recovery, absent metadata,
directory metadata and POSIX file aliases. Symlink cases are excluded on Windows because
developer machines may not grant symlink privileges. This preserves the existing single-writer
contract; it does not establish protection against concurrent path substitution.

Environment and isolated dependency setup match
[import admission](EditorEDM1-ImportAdmission-Linux-2026-10-07.md). Final commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development -R 'editor\.(preview_contract|workspace_budget|parser_robustness)$'
ctest --preset linux-development
```

Configure/build passed with the cached X11/Vulkan sysroot, graphical shell and Slang 2026.18
enabled. Focused tests: **3/3 passed**. Full CTest: **120/120 passed**, none skipped, 202.52 seconds.
The full run uses lavapipe and Khronos core/synchronization validation with the same
VK_DRIVER_FILES/VK_LAYER_PATH/VK_INSTANCE_LAYERS/VK_LAYER_ENABLES settings as import admission.
`git diff --check` passed. The test target adds no module dependency; public constants add no
class layout or linkage boundary change. Generated output and local setup remain uncommitted.

Source SHA-256:

- `Engine/Editor/src/ProjectWorkspace.cpp`: `5c42b32f10213c569461066f120270d2ff0b66eb932c0c60eada3f92e2ccaaa1`
- `Engine/Editor/include/Nexora/Editor/EditorWorkspace.h`: `16dd1eb83eecce38066571c6e7a144c05ba7588c2b82bb129b4196753a702a13`
- `Tests/Editor/WorkspaceBudgetTests.cpp`: `602e85cebe0f9044b4b1c701320560da37fc4bc8f5f9f1226708da216dfa0dbd`
- `Tests/Editor/CMakeLists.txt`: `77d0490eff7ae81d3fa9514daaa7af0861844f4be470c2f07eebfbe60acd8982`

Physical-display, Windows/macOS/mobile, installed IME and full graphical milestone acceptance
remain open. Hosted CI results are separate evidence and must pass before merging.
