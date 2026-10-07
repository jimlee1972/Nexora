# ED build manifest publication: Linux evidence

Date: 2026-10-07. Parent: `135f8e3df707af70b1478355f57de34544f58bb8`.

BuildFrontend previously truncated a narrow-string sibling `.tmp`, used a separate replacement
helper, left staging after failed publication, and formatted JSON through the global locale.
It now streams schema-1 records through the shared native-path atomic publisher. Occupied stage
files/directories/valid or dangling leaf aliases reject without truncation or removal. A failed
replacement preserves the destination and removes this call's owned staging. Successful validation
and write clear stale error text. Record fields and numeric uint64 artifact byte-count types are
unchanged; command/checksum fields remain caller-supplied metadata, not execution or verification.

The private publisher accepts a synchronous stream callback; the existing string-view wrapper
retains its interface/behavior. Build JSON is not assembled into an additional full manifest buffer.
Both numeric output and control-character hex escapes use the classic locale. Callback exceptions
close staging and attempt owned-stage cleanup before propagating. Public C++ signatures/object
layouts, stable C SDK contracts, module dependencies and test registration are unchanged. This
retains the documented single-writer contract, not exclusive creation against concurrent actors.

The existing `editor.preview_contract` test now exercises a native UTF-8 destination, a global
one-digit grouping locale, UINT64_MAX bytes, Unicode/quote/backslash/control/newline profile text,
occupied file/directory/valid/dangling stages, failed destination-directory replacement, last-good
preservation, invalid-schema admission, stale errors, retry and a relative destination. Cross-host
alias fixtures remain POSIX-only; native-path/directory/locale cases run on all configured hosts.

Isolated tools/sysroot and Vulkan/socket setup match
[recovery presence](EditorEDM7-RecoveryPresence-Linux-2026-10-07.md). Final commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development -R 'editor.preview_contract|editor.workspace_budget|editor.profiler_export'
ctest --preset linux-development
```

Configure/build passed with graphical shell and Slang enabled. Focused tests: **3/3 passed**,
1.39 seconds. Full CTest: **123/123 passed**, none skipped, **203.60 seconds**, with lavapipe and
Khronos core/synchronization validation. `git diff --check` passed. No linkage boundary changed;
no additional Shipping build is claimed. Generated output and local tool/probe files are uncommitted.

An independent API-only C++ probe linked the built NexoraEditorCore and wrote the same Unicode,
control-text and UINT64_MAX fixture under one-digit grouping. Python's strict JSON consumer rejected
duplicate keys/nonfinite constants and verified the complete decoded object against independent
expected values, including the original Unicode/control text and integer `18446744073709551615`.
This supplemental local consumer check passed; it is not a newly registered CTest or hosted check.

Source SHA-256:

- `Engine/Editor/src/AtomicFile.h`: `ccc5b1fc29288d72c47da73896e9e4693f6b24b5ac473cce039416363022dc17`
- `Engine/Editor/src/EditorProduction.cpp`: `40fcf1996eb43dfdcc5f8bc2e504ffd017820b1f106e641a0b572957a1eedc01`
- `Tests/Editor/EditorTests.cpp`: `ef778b2587ba5be08cd2777fea57470dd3d1182d6985f724886fed8f6995c6f9`

Graphical build/deploy workflows and full ED-M6 acceptance remain open. Full graphical milestone
acceptance remains **0/8**. Hosted checks must pass before merge.
