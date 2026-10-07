# ED Profiler CSV import: Linux evidence

Date: 2026-10-07. Source parent: `e93fbdeb5d207675eefefe1742148ffd2a50c3eb`, merged by
PR #400 as `c51934aa745673709029a65118a6dc4e1dcd47c6`.

ProjectWorkspace now synchronously imports its exported `.nexora/frame-processing.csv` into an
owning FrameProcessingCapture. Reading is permitted for read-only observers and changes no file,
workspace or live ProfileSession. Closed/recovery-pending projects, unsafe metadata/leaf aliases,
nonregular/missing/unreadable files and corrupt/unsupported data return no snapshot plus an error.
Input retains at most 128 KiB, with one over-budget probe; output requires 1-600 ordered nonzero
uint64 frame IDs, finite nonnegative wall times, consistent uint64 drop counts and empty GPU/memory
cells. The exact five-column header and unquoted numeric fields are required; LF/CRLF are accepted.
from_chars consumes complete fields independently of locale, retaining double/uint64 precision.
This is the existing exported CSV subset, not arbitrary CSV or JSON capture support. Serialized
filesystem checks are not a claim of immunity to concurrent filesystem replacement.

The graphical Import CSV control emits an independent one-shot request. The application performs
IO on the authoring thread, then transfers an owning validated snapshot to the host. Imported
history is a separate static trace with its own Clear imported control. Live history/capture/drop
counts and existing export sources remain unchanged. Failed IO/publication preserves the prior
trace. Read-only import is allowed; recovery/modal/close/no-project gates reject actions. Project
root/UUID changes or detachment clear imported data, status and queued requests. CSV has no
device/project provenance and the panel labels this limitation; GPU/memory remain unavailable.
Plot statistics use an incremental mean instead of an overflowing sum for finite large values.
Public additions are C++ methods/one owning value type; existing signatures, class layouts, stable
C SDK, module dependencies, JSON/CSV export formats and CTest registration are unchanged.

The existing editor.profiler_export test covers CSV round trips, read-only/closed/recovery cases,
stale errors, failed-load retry, source/stage preservation, 600/601 rows, exact 128-KiB and overflow
payloads, unsupported headers/quoting/columns, NUL/truncation/blank lines, malformed/partial/nonfinite
numbers, duplicate/out-of-order IDs, inconsistent drop counts, CRLF, directory/leaf aliases and
POSIX metadata-parent aliases. Precision fixtures include IDs above 2^53 and UINT64_MAX, UINT64_MAX
drops, full double precision, minimum normal/subnormal and maximum finite double under comma locale.
Both 1x/2x host tests send actual Window pointer/button events through import/clear controls, check
independent one-shot requests and owning publication after caller mutation, preserve live history,
reject invalid publication, retain old data on failed load, allow read-only/empty-live imports, and
clear stale requests/snapshots on detach. Alias fixtures are POSIX-only; other fixtures run across
configured hosts. This does not claim an OS-driven native App import scenario or human visual gate.

Tools/sysroot/socket/Vulkan setup matches
[recovery presence](EditorEDM7-RecoveryPresence-Linux-2026-10-07.md). Final commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development -R 'editor.profiler_export|editor.profiler_json|editor.workspace_budget'
ctest --preset linux-development
```

Configure/build passed with graphical shell and Slang enabled. Focused tests: **4/4**, **1.67 seconds**.
Full Linux CTest: **123/123 passed**, none skipped, **204.73 seconds**, with lavapipe and Khronos
core/synchronization validation. Changed Markdown/bilingual pairing and git diff --check passed.
No linkage boundary changed; no additional Shipping result claimed. Generated output/tools/logs
remain uncommitted. Hosted checks and review must pass before merge.

Source SHA-256:

- `Engine/Editor/src/ProjectWorkspace.cpp`: `6a99902a40fc277a2e7c5139471e8575e21808b98ee9ac8be0511f1185133eb2`
- `Engine/EditorImGui/src/EditorImGui.cpp`: `51d22832912d9a0e99f0e0870bfffa81baa6421ebbb9e6f58a50aea8937bec7e`
- `Apps/Editor/main.cpp`: `b8aae87ceab069eed89252f996d668a7c6687d0faae42ca8be57c545b910fb87`
- `Tests/EditorImGui/ProfilerExportTests.cpp`: `5947cd80ea2da77128b3bd0545e64b07b72a6c4da107dbb82870678b915b1383`

JSON/arbitrary capture import, build/deploy, GPU/memory instrumentation and remaining ED features
stay open. Full graphical milestone acceptance remains **0/8**.
