# ED build manifest admission: Linux evidence

Date: 2026-10-07. Parent: `4a4610fbc286f160b758058bd91752d2060c3099` (PR #398).

BuildFrontend now validates every profile/artifact string as UTF-8 before path parsing or IO.
Artifact paths use project-relative forward-slash syntax consistently across hosts: empty/rooted
paths, Windows drive/stream colons, backslashes, component-ending dots/spaces (including dot
components), repeated/trailing separators, ASCII controls and DEL reject. Native parsing uses
UTF-8 rather than the system code page. Valid
control text in profile/checksum metadata still uses JSON escaping; exact duplicate paths reject.
This is a syntax policy, not Windows reserved-name/case-folding/Unicode-normalization validation.
Commands/checksums remain supplied metadata, not executed builds or verified artifact content.

The existing `editor.preview_contract` test covers a Windows-target manifest produced on Linux
with CJK/supplementary UTF-8 artifact text and zero bytes, 27 invalid paths, five malformed UTF-8
classes in each of six fields, duplicate artifacts, last-good/occupied-stage preservation, no
directory creation on invalid admission, stale-error clearing and valid byte-identical retry.
Drive rejection now runs on every host. No public signature/layout, module dependency, stable C
SDK contract or CTest registration changes. The Foundation UTF-8 helper is an existing dependency.

Tools/sysroot and Vulkan/socket setup match
[recovery presence](EditorEDM7-RecoveryPresence-Linux-2026-10-07.md). Final commands:

```bash
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development -R 'editor.preview_contract|editor.workspace_budget|editor.profiler_export'
ctest --preset linux-development -R '^editor.linux_native_scene_center$'
ctest --preset linux-development
```

Configure/build passed with graphical shell and Slang enabled. Final focused contract tests:
**3/3**, **1.39 seconds**. Final full CTest:
**123/123 passed**, none skipped, **204.00 seconds**, with lavapipe and Khronos core/synchronization
validation. `git diff --check` passed. No linkage boundary changed; no new Shipping result claimed.

Earlier full validation failed only Scene Center window discovery; an immediate focused retry
failed its saved-scene rotation assertion. The PR #398 EditorProduction source was temporarily
rebuilt as a baseline: Scene Center passed 1/1 in 24.59 seconds. Restoring that candidate's source,
rebuilding, then rerunning focused Scene Center (1/1, 25.40 seconds) and the full suite
(123/123, 202.56 seconds) passed without test changes, skips or weakened assertions.
Exact-head review subsequently identified Win32 trimming aliases, including `game.exe.` and
`dir/.. /evil.dll`. The final component-ending dot/space rejection and eight regression cases were
added, then configure/build, focused contracts and the full gate above passed again. Final
Scene Center passed within the full suite in 25.79 seconds. The cause of the earlier native UI
failures is not established;
these passes do not claim that an intermittent harness/runtime problem has been fixed.
The graphical app has no BuildFrontend caller, and this patch changes no graphical input path.

Source SHA-256:

- `Engine/Editor/src/EditorProduction.cpp`: `a505980df0efd6c3176e88b70ddbdc8779e3236b39d73bc663e76725ecd17c79`
- `Tests/Editor/EditorTests.cpp`: `3e88545eea32cbfcfafe8a45ea484a0e4e430712de301ff9f074b21aa4caac49`

Local tools, logs and generated output remain uncommitted. Hosted checks must pass before merge.
Graphical build/deploy workflows remain open; full graphical milestone acceptance remains **0/8**.
