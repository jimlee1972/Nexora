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

The **Windows Editor package** Actions workflow builds the current Development Editor with DX12.
Download its `NexoraEditor-Windows-x64-<full source SHA>` artifact from a successful run. The artifact
contains a ZIP, its SHA-256 sidecar and hosted Windows verification logs. Extract the ZIP into a new
folder; keep the EXE, DLLs and licenses together. This replaces neither an existing package nor a
project. `build-info.json` records the source commit; `inventory.json` hashes every shipped file
except itself. Verify the ZIP with PowerShell `Get-FileHash -Algorithm SHA256` against the sidecar.

Double-click `Run.cmd` for the project browser, or use the executable directly:

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
folder, the matching OpenSSL installation (including its license), and the fetched Dear ImGui license. It copies the recursive import closure, rejects missing
or unclassified dependencies, and refuses an existing output directory. Before producing a ZIP,
it relocates the package to a Unicode path, restricts PATH to Windows system directories, checks
CLI help/create/reopen/duplicate rejection and runs 120-frame open plus eight-frame creation DX12
smokes. Logs survive failure; a failed verification does not produce a delivery ZIP. Its portable
synthetic tests run with `python -m unittest discover -s Tools/Editor/tests -v`.

Hosted Windows rendering checks do not establish physical DPI/IME, game integration or clean
Windows installation acceptance. Existing Editor milestone acceptance requirements remain open.
