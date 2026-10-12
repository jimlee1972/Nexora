# ED-M6 Chrome synchronous duration import and Editor CI integration

Date: 2026-10-12. Base: accepted main `cd7588a3` (PR534).
Beads: `nexora-62u.2.6` (duration import), `nexora-nqn.11` (CI admission/compatibility).
This evidence accepts supporting implementation, not ED-M0/ED-M6 or physical-host workflows.

## Imported behavior

The project-scoped graphical Profiler now accepts synchronous Chrome B/E durations alongside X
complete events. Explicit process/thread/event selection, external microsecond start clock and
millisecond duration remain unchanged; no CPU/GPU/memory or exclusive/self-time metric is inferred.
For example, B at 100 microseconds followed by E at 1350 produces 1.25 milliseconds. The same owning
static capture is published through the existing controls, independently of live measurements.

The selected thread uses a LIFO stack capped at 128 open scopes, including nonmatching names.
Nested same-name begins reserve samples in start order and close in reverse order. Optional named
ends must match the current begin. Backward B/E timestamps, unmatched/unclosed ends/begins, missing
fields and capacity overflow reject the whole import. Unrelated threads share no stack. Valid zero
and fractional durations remain exact within the existing double representation. Async phases
remain validated/skipped metadata. Recognized B/E records on the selected thread previously skipped
as metadata now must form complete supported scopes.

The existing 4 MiB / 32,768-event / 600-sample and Unicode/JSON limits remain. Newest retention is
by selected begin/start order. An end checks its reserved sequence before updating the ring slot,
so an evicted begin cannot overwrite a newer occupant. No partially open interval escapes success.
The stack is local to one read-only import and owns its names; no document/World/host borrow reaches it.

Regression coverage includes mixed X/B/E, nested matching/nonmatching names, unrelated threads,
named/unnamed ends, zero/fractional duration, decoded Unicode identity, exact 128-scope admission,
overflow, 605 completed pairs and an evicted pending outer begin. Invalid imports preserve source
and prior owning captures. Project IO covers a real read-only paired import and unclosed-source
failure. Actual 1x/2x Profiler controls emit the scoped request, read the paired project file and
publish copied intervals while retaining independent wall/GPU/memory capture and scope revocation.

## CI baseline repaired

PR534's immutable [Linux display job](https://github.com/jimlee1972/Nexora/actions/runs/38210974620/job/114686612509)
failed before executing the prefab gate because only two of its required three tests were registered.
`PrefabIsolationTests.cpp` was present but its existing target/registration had been dropped from
`Tests/EditorImGui/CMakeLists.txt`. Restore that exact target and its 60-second timeout, retaining all
source assertions. CTest now enumerates document, UI and native isolation as three tests.

The same run's [MSVC desktop job](https://github.com/jimlee1972/Nexora/actions/runs/38210974620/job/114686612699)
failed on C4996/C2220 for `std::getenv` in Editor test-geometry hooks at main.cpp:1472 and 1555.
The Editor app now uses the existing targeted `_CRT_SECURE_NO_WARNINGS` convention under MSVC,
preserving `/WX` and unrelated warning settings/targets. Runtime diagnostic behavior is unchanged.
This source fix is not a claim of Windows execution in the Linux environment.

## Validation configuration

Host: Linux x86_64, GCC 14.2.0, CMake 4.4.4, Ninja 1.13.2, Slang 2026.18,
clang-format 23.1.3, local Xvfb and Mesa lavapipe 25.0.7. Development is Modular,
graphical shell / Slang ON, Zig OFF. Task-local tools and machine-specific paths remain external.

```bash
source /tmp/nexora-editor-20261012/env
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development
ctest --preset linux-development -N -R '^editor\.(prefab_document_isolation|prefab_isolation_ui|linux_prefab_isolation)$'
python3 Tools/Build/TestDocumentationCI.py
git diff --check
```

The final configure/build passed. Full Development passed **227/227**, zero failures/skips,
in **692.12 seconds**. Chrome parser passed in 0.13 seconds, graphical Chrome import in 0.02 seconds,
and the restored prefab UI fixture in 0.09 seconds. The mandatory three-test prefab admission is
present and all three tests passed in the full gate. Documentation tests passed **16/16**;
five changed Markdown documents passed link/bilingual checks and all five touched C++ files passed
clang-format validation. No product module/dependency/linkage boundary, persisted Editor
schema or stable C ABI changed; Shipping validation is not required. Native GPU calibration,
physical display/IME and Windows/macOS local execution remain outside this Linux evidence.

Source SHA-256:

- ChromeTrace.cpp: `43d90c6487274f9ee8e014d1c315775639f0d39a2a5a9e923b129ca98a293826`
- ChromeTraceTests.cpp: `ed2c33c7be4c79f552ce67c4710426bc3fa94369f15eb57c62e13a0dfa53c925`
- ChromeTraceDisplayTests.cpp: `04df5df9144f8f98478f9668466ab4354f05177d8fd446c42783548004abf215`
- Apps/Editor/CMakeLists.txt: `817911b8e34cddca89b1b7ec5e6da559296de84dad982e8421536569c13bfad4`
- Tests/EditorImGui/CMakeLists.txt: `5f0616173730f4c2324252e5a5b876934a25a768f9199cf75cfb0fc02bbceae6`
