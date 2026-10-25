# Project build-scene settings candidate — Windows 2026-10-25

Scope: phase 1 of [scene delivery](../../../Roadmap/en/Project_Scene_Delivery_Plan.md).
This candidate supplies project-owned startup/build-scene settings and graphical editing.
It does not implement multi-scene cooking, packaged startup/transitions, Editor Play selection,
source-free game building or the final Editor ZIP. Full goal/Editor milestone acceptance stays open.

## Implementation and checks

- A pure owning schema-1 `project.build.json` codec validates startup inclusion, full UUIDs,
  duplicate/case-alias paths, portable relative paths, strict JSON keys and Unicode escapes.
  Limits: 256 KiB, 256 scenes and 1024 UTF-8 bytes per path.
- An owner-thread session borrows the workspace. Missing legacy settings do not write files;
  read-only/recovery/stale workspace, corrupt settings, external byte changes and occupied
  staging reject publication. Content renames preserve UUIDs; project-root scenes use explicit
  stored identities. Settings never rewrite `project.nexora` or the Editor last-scene metadata.
- Build > Project Build Settings emits scoped owning requests. Actual input regressions at
  1x/2x select Level01 as startup, exclude Title, save through the production session, protect the
  startup checkbox, request Reload/Add Current and reject read-only writes. This is real ImGui
  draw/input validation, not a screenshot or physical native-window interaction acceptance.

## Windows developer-host evidence

Source was synchronized into the existing isolated checkout at
`build/developer-preview-20261024/source`; MSVC configuration/build used `windows-development`.
New public C++ files and changed ranges were formatted with the installed repository formatter.
No optional-feature definition or module dependency was changed.

Final local candidate commands:

```powershell
cmake --preset windows-development
cmake --build --preset windows-development --target NexoraProjectBuildSettingsTests NexoraProjectBuildSettingsUiTests NexoraStaticProjectExportUiTests NexoraSceneFileInputTests NexoraEditor --parallel 3
ctest --preset windows-development --output-on-failure -R '^editor.(project_build_settings(_ui)?|static_project_export_ui|scene_file_input)$'
```

All four selected tests passed (24.49 s): settings core 0.37 s, existing scene-file input 15.19 s,
existing StaticView UI 4.46 s and new 1x/2x settings UI 4.37 s. The core additionally checks exact
byte admission, corrupt/unsupported JSON, external replacement, occupied staging, missing enabled
versus disabled scenes, actual Unicode files, Content rename/conflict and recovery conservation.
Windows could not create the symlink fixture because the current account lacks that privilege;
that internal fixture was explicitly reported unavailable. It is not claimed as a Windows pass.
Linux must exercise the alias fixture and the complete required gate.

The built host was launched from a foreign CWD using only `--frames=8` (no mode/project argument).
It returned 0 with acquired=8, presented=8, ui_draws=16, ui_rejected=0 and no project selected.
Host SHA256: `f3141c2c261a7500a0cbbe99a2d953bc68f8eb82874d8bf38e7e9a137960217d`.
EditorCore DLL SHA256: `591b5be5beaa162f7a16676d172a9dac74a4bfbd09b5fcc0104eff8da639fe75`.
This is a bounded native startup smoke, not a no-argument infinite interactive run or a clean-host gate.

Raw local logs: `build/scene-settings/20261025-233555-{configure,build,test}.log` and corresponding
stderr files; `build/scene-settings/startup-final.{json,stdout.log,stderr.log}` retains startup evidence.
Earlier compile diagnostics (a missing forward declaration, repaired without weakening warnings)
are retained at `build/scene-settings-build.log`; earlier successful narrower runs remain in
`build/scene-settings/`. Initial documentation-test failures in the planning task were environment
access/dependency failures and are not substituted for implementation validation.

## Open gates

Full Linux Development configure/build/CTest, Shipping, exact-head CI, feature-off admission,
native settings-to-game integration, packaged A → B → A, source-free relocation and final
Editor-only ZIP are not accepted by these four local tests. Preserve every phase as pending until
its required evidence exists. Subsequent evidence must identify its actual tested source.
