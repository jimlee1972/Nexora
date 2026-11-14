# Editor target-host evidence

`EditorTargetEvidence.py` records ED-M0 observations on Linux/X11 and Windows Development hosts.
It requires Python 3.10 or newer and only uses the standard library. Use a disposable project copy,
never a working project: the manual recovery scenarios include explicit termination and discard.
The tool itself does not seed journals, inject crashes, perform manual scenarios or publish evidence.

Build the graphical Editor at the stated source commit with the normal native backend and shader
options. Record the actual backend/device/driver from native diagnostics, not the requested CMake
option. The binary SHA-256 is captured alongside the operator-supplied full source SHA. This ties
observations to a binary; it does not independently prove that binary was built from that source.

Run on the target host (use `python` on Windows if that is its installed command):

```bash
python3 Tools/Editor/EditorTargetEvidence.py record \
  --editor /path/to/NexoraEditor --project /path/to/disposable-project \
  --output /path/to/new-evidence-bundle --commit FULL_40_CHARACTER_SOURCE_SHA \
  --backend vulkan --gpu-driver 'actual GPU / driver version' \
  --monitors 'monitor models and observed scales' --display-kind physical
```

Use `--backend dx12` only when the Windows surface actually selected DX12. The output directory
must be new and outside the repository. The recorder first saves all rows as blocked, runs a
bounded 240-frame graphical smoke (60-second deadline), and preserves its stdout/stderr. A bundle-local recent-project list isolates the drill from the
operator's normal recent-project settings. Smoke
requires matching acquired/presented counts, nonzero native UI draws/uploads, no rejected textures,
and no recognized native validation diagnostics. It then prints the exact manual Editor argv.
Launch that binary separately and perform each prompted scenario. Press Enter for any unperformed
scenario. Record the observed DPI/IME, limitations, and the path to a screenshot, recording or log.
Artifacts are copied into the bundle under SHA-256 names; preserve the entire directory when sharing.
On Windows, the equivalent PowerShell invocation is:

```powershell
python Tools/Editor/EditorTargetEvidence.py record `
  --editor 'C:\Nexora\build\NexoraEditor.exe' --project 'C:\DisposableProject' `
  --output 'C:\Evidence\new-ed-m0-bundle' --commit FULL_40_CHARACTER_SOURCE_SHA `
  --backend dx12 --gpu-driver 'actual GPU / driver version' `
  --monitors 'monitor models and observed scales' --display-kind physical
```

Scenario notes should explain what the artifact proves. A screenshot alone cannot prove focus/input,
exactly-once text, resource lifetime, or recovery data preservation; use a recording and/or logs.

Every Linux record requires backend validation, glyph readability, Hierarchy selection, docking
and restart, typing/shortcuts, both wheel axes, held-input focus loss, resize/minimize/restore/close,
keyboard-only Recover and Discard after durable-journal termination, and failure/retry observations.
Windows additionally requires separate 100/125/150/200% observations, repeated moves between
monitors with different DPI, installed-IME start/update/commit, cancel, caret movement and candidate
placement at two explicitly recorded different scales. Follow the detailed procedure in
[the integration plan](../../Roadmap/en/Editor_ImGui_Integration_Plan.md#6-validation-commands-and-evidence-format).
The operator must build Development and explicitly attest to having performed the passed rows.

Validate a retained bundle with:

```bash
python3 Tools/Editor/EditorTargetEvidence.py validate /path/to/bundle/evidence.json
```

Validation recomputes status, scenario coverage, artifact hashes and smoke results; it does not
trust a saved `status`. Failed observations return `FAIL`. Missing/duplicate/unknown rows, missing
metadata/artifacts, changed files, cyclic artifact symlinks, virtual-display records and absent
attestation remain `INCOMPLETE`.
Both return exit 1. A complete operator record returns `COMPLETE_OPERATOR_RECORD` and exit 0.
This is evidence completeness, not independent verification of visual claims or milestone approval.
A reviewer must inspect the actual observations/artifacts against the plan before accepting ED-M0.
The tool cannot detect whether a person incorrectly labels a virtual display as physical.

`RecordEditorWindowsEvidence.ps1` remains a legacy bounded Windows smoke and preliminary interview.
Its v1 `PASS` does not cover the full DPI/scenario matrix and is not accepted by the new validator.
Re-record target-host acceptance with the Python recorder. Historical records must not be relabeled.

`build.editor_target_evidence` runs portable admission regressions without a window or GPU. Passing
it proves neither actual Windows IME/DPI behavior nor physical Linux rendering. Linux Xvfb tests
remain separate automation. No target-host acceptance is inferred from cloud execution.

## Windows Editor-only download

The packager requires a Windows GUI PE subsystem and rejects old console-subsystem Editors.
Relocated startup/CLI verification explicitly checks console_attached=0 through an opt-in process
diagnostic while still validating redirected output, exit codes and Unicode project arguments.

The **Windows Editor package** Actions workflow builds the current Development Editor with DX12.
Download its `NexoraEditor-Windows-x64-<full source SHA>` artifact from a successful run. The artifact
contains a ZIP, its SHA-256 sidecar and hosted Windows verification logs. Extract the ZIP into a new
folder; keep the EXE, DLLs and licenses together. This replaces neither an existing package nor a
project. `build-info.json` records the source commit; `inventory.json` hashes every shipped file
except itself. Verify the ZIP with PowerShell `Get-FileHash -Algorithm SHA256` against the sidecar.

Double-click `NexoraEditor.exe` for the project browser; no arguments are required. `Run.cmd`
also forwards arguments unchanged. Explicit project/create CLI commands remain available:

```powershell
.\NexoraEditor.exe --help
.\NexoraEditor.exe --create-project=G:\proj\NewOnmyoji "--project-name=Onmyoji Survivors" --graphical
.\NexoraEditor.exe --project=G:\proj\OnmyojiSurvivors --graphical
```

Creation requires a new project descriptor and rejects an existing one. Open an existing Nexora
project with `--project`; do not recreate it. This package contains no Player, AssetCompiler,
Studio, source or Zig toolchain. Separately compiled gameplay modules and their requirements are
outside this Editor startup package.

`PackageWindowsEditor.py` requires Windows, MSVC `dumpbin`, an explicitly selected x64 CRT redist
folder and the fetched Dear ImGui license. OpenSSL-linked builds also require the matching explicit
OpenSSL installation and its license. It copies the recursive import closure, rejects missing
or unclassified dependencies, and refuses an existing output directory. Before producing a ZIP,
it relocates the package to a Unicode path, restricts PATH to Windows system directories, checks
CLI help/create/reopen/duplicate rejection and runs 120-frame open plus eight-frame creation DX12
smokes. Logs survive failure; a failed verification does not produce a delivery ZIP. Its portable
synthetic tests run with `python -m unittest discover -s Tools/Editor/tests -v`.

Hosted Windows rendering checks do not establish physical DPI/IME, game integration or clean
Windows installation acceptance. Existing Editor milestone acceptance requirements remain open.

The packaging task cleans up superseded `Build` runs only on its exact
`build/editor-windows-package` branch and only for the repository owner's runs. Both events at the
current branch head remain required; other branches and actors are excluded.

Windows camera persistence retains native filesystem paths when creating its temporary file,
so opening and closing an Editor project under a Unicode directory does not narrow its camera
metadata path through the active ANSI code page. The relocated graphical package check exercises
this shutdown path as well as initial rendering.


The packaging command optionally accepts `--game-runtime-profile`, `--cmake-root`, `--ninja`,
`--zig-root` and `--ninja-license` together to include game-building support. This stages the
native game host and flat dependency profile privately under Runtime, and executable-relative
CMake/Ninja/Zig under Tools. Compiler standard libraries and CMake modules are required tool data;
no Nexora or game source checkout, examples, or extra 2D applications are copied. Each distribution's
licenses remain included. The Windows delivery workflow supplies all five paths, pins Zig 0.14.0,
CMake 3.31.6 and Ninja 1.12.1, and builds the native ProjectPlayer. Editor-only mode remains an
optional packaging utility; it is not the complete Editor delivery contract.
The graphical and CLI build paths resolve the same installed profile/tools; a CLI build may omit
all runtime/tool options to use that installation. ✅ Actual relocated Editor/script-game
verification, ZIP extraction and all 41 source-head CI checks pass in PR #721. See the
[acceptance record](../Build/evidence/ProjectSceneDelivery-Acceptance-2026-10-26.md);
this does not claim independent clean-host or complete Editor milestone acceptance.

A packaged `build-info.json` marker makes game-build dependency lookup installation-only: a missing
private Runtime/profile or tool cannot silently use a build-PC path. Unmarked development Editors
may use CMake's actual configured Player profile path, including POSIX per-target output layouts.

Complete delivery additionally extracts the actual ZIP, admits every inventory hash and the single
public Editor executable, then repeats relocated startup and shipped-tool script Build/native game
verification. That extracted Editor enters the same Play command with the newly built module for
1,200 presented frames, three scene commits, native draws and unchanged authored-source hashes.
Failure removes the archive instead of publishing a partially verified delivery. These hosted DX12
process receipts do not claim physical input, Windows pixel readback or clean-host acceptance.

Native Windows game admission additionally rejects console PE subsystem for the private player
and exported Game.exe, verifies their identical bytes and Unicode/redirected CLI, and launches
Game.exe directly with no arguments and ordinary flags. It monitors one responsive native window,
no console or child process, waits for real successful native presents, sends WM_CLOSE, then checks
actual frames/gameplay updates, drain and exit 0. Staged and extracted installations repeat this
check alongside the unchanged 100-frame/three-commit game and 1,200-frame Editor Play admissions.

## Extracted Editor source-edit reload gate

`PackageWindowsEditor.py --script-reload-cycles=32` adds the actual Windows Editor source-edit
workflow after existing real-ZIP extraction/relocation admission. It requires the complete private
SDK/toolchain. The Windows Editor package workflow enables this gate automatically on the active
`build/editor-logo-splash` implementation branch and for explicit workflow_dispatch runs (90-minute
job limit). Other push packaging retains its existing35-minute job limit and passes0 cycles. The full gate must be run and pass before accepting the script-reload delivery.

The native harness uses the real Ctrl+P command, preserves source timestamps while changing Zig
bytes and requires32 consecutive accepted replacements in the same Play session. Counter and
Scene/entity handles must survive; the old code keeps dispatching during compilation. Stop occurs
while an observed owned compiler process is active; retained process handles and Windows Job
accounting prove the tree drained. The harness owns a kill-on-close Job for only its Editor/SDK
children, including abnormal helper termination. Ordinary Editor close/input behavior is unchanged.

The per-candidate120-second, startup35-second, responsiveness500-ms and normal-close10-second
limits remain. An aggregate one-hour harness bound leaves room for cleanup before its70-minute
packager bound. The packager rejects a nonzero/timeout child, incomplete32-cycle receipt, reset
state/handles or residual owned processes, and verifies installation inventory again afterward.
Any failure removes the ZIP rather than publishing it. Wrapper stdout/stderr and native logs
remain under `verification/extracted/script-reload*`. Hosted Windows evidence remains separate
from Linux gates and from physical-device/clean-host acceptance.

Startup pixel tests use an owned small foreground launcher as a controlled interactive-parent
precondition. It is not an Editor HWND, has no taskbar entry and is never counted as Editor pixel/input
acceptance. The real Editor child retains its PID/class, hidden-first-frame, pixels, response and
shutdown checks; the splash constructor/preparation must still avoid stealing focus. Foreign
application content/topmost state and desktop/display configuration are not changed. The hidden
Editor itself now fits its outer bounds to the monitor work area before its first real draw.

The extracted-ZIP script gate also requires four production failures (compile, state restore, candidate initialization, and candidate start after a private World write). Each rejection must retain the active module generation, strictly advancing counter and Scene/entity handles; old updates check World state before writing. A subsequent valid source must commit and dispatch without reset, before the existing compilation-cancellation and process-drain checks. Partial checkpoints survive runner interruption but cannot admit a package or replace the final PASS receipt.

The existing input-desktop selector is shared by startup and script acceptance. Isolated native commands require explicit `NEXORA_NATIVE_TEST_INPUT_DESKTOP=1`; this changes only the calling thread/children, never the user's desktop. Each actual Editor process starts immediately from its owned foreground launcher, after any fixture CLI preparation. Foreground ownership is checked before every key chord; absent ownership fails without sending keys to other applications.

The Windows launcher uses a32×32 Button inside the primary work area, with thread-local physical
coordinates. It pumps a bounded foreground acknowledgement. If direct activation is denied, it
clicks only its hit-tested owned interior, after rejecting capture/menu modes or held mouse buttons.
It acknowledges the actual owned mouse-up before restoring the pointer. Product focus/topmost
behavior and Editor deadlines are unchanged. The workflow runs three native launcher-only trials
after the original startup gate and retains their records separately from Editor acceptance.
See the Windows [foreground contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-setforegroundwindow)
and [point ownership contract](https://learn.microsoft.com/en-us/windows/win32/api/winuser/nf-winuser-windowfrompoint).
