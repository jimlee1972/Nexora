# ED-WF-02: persistent script Build UI and owned game execution

Date: 2026-10-31. Epic [#741](https://github.com/jimlee1972/Nexora/issues/741),
work order [#743](https://github.com/jimlee1972/Nexora/issues/743).
Implementation base: `60f42e90868b06cabc9509d85a4e3ee6d5b6c1cd`.
Status at source publication: implemented with scoped Windows acceptance; Linux/CI and merge pending.
This work order is not yet accepted. Plan progress stays 1/9, full Editor milestones stay 0/8.

## Behavior and contracts

- Save/Reload/reopen retain Build mode, script manifest and output directory. Legacy/default
  configuration retains canonical schema 1; schema 2 includes bounded portable build settings.
  Unknown/duplicate fields, unsupported modes/versions, invalid UTF-8 and unsafe paths reject.
  CLI explicit scene-only/script choices remain authoritative and stdout remains JSON.
- Build & Run retains the existing saved-scene decision continuation, publishes through the same
  guarded producer and directly launches the native Game with no arguments/output-directory CWD.
  The owner rejects replacement builds while that game runs. Stop/project change/shutdown drain
  the owned process group. Application flags do not hide or detach a console executable.
- Worker phases and bounded compiler diagnostics cross a mutex mailbox as owning data. No UI or
  workspace borrow crosses the worker. Script diagnostics map frozen-source paths back to project
  paths. Terminal controls/invalid UTF-8 do not reach the display; output and diagnostic growth are
  bounded. Long status output scrolls inside a bounded region so Build/Cancel remain reachable.
- SDK embedding preserves exact canonical bytes in bounded literals and allocates strings only
  on worker consumption, repairing MSVC 19.44 C2026. The compiler test compares compiled bytes
  with canonical files. Ninja discovery is independent of the Editor's MSBuild/Xcode generator.
- Game-output development records remain ED-WF-03. Stateful reload and Sprite integration remain
  subsequent work orders; this batch does not claim those features or clean-host acceptance.

Contracts: [Editor Core](../../../../Engine/Editor/README.md),
[producer](../../../../Engine/Editor/GameBuild.md),
[application](../../../../Apps/Editor/README.md),
[ImGui host](../../../../Engine/EditorImGui/README.md).

## Local Windows validation

Host scope: Windows/MSVC 19.44, CMake 3.31.6-msvc6, Ninja 1.12.1, Zig 0.14.0,
Visual Studio 2022 generator, Development, native DX12,
Vulkan OFF, graphical Editor/ProjectPlayer ON, canonical Zig 0.14 and independent Ninja.
This is the existing developer host, not an independent clean machine or physical-device milestone.

Configure/build used the installed Visual Studio CMake and explicit tools:

```powershell
$edwfCmake = 'D:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
& $edwfCmake -S . -B build/editor-workflow-native -G 'Visual Studio 17 2022' -A x64 -DCMAKE_CONFIGURATION_TYPES=Development -DNEXORA_ENABLE_VULKAN_BACKEND=OFF -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF
& $edwfCmake -S . -B build/editor-workflow-native -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON '-DNEXORA_ZIG_EXECUTABLE=G:/proj/Nexora/.tools/zig-win-x86_64-0.14.0/zig.exe' '-DNEXORA_EDITOR_GAME_NINJA_EXECUTABLE=D:/Program Files/Microsoft Visual Studio/2022/Community/Common7/IDE/CommonExtensions/Microsoft/CMake/Ninja/ninja.exe'
& $edwfCmake --build build/editor-workflow-native --config Development --target NexoraEditor NexoraProjectPlayer NexoraGameBuildTests NexoraProjectBuildSettingsTests NexoraProjectBuildSettingsUiTests NexoraGameBuildProgressTests NexoraBuildProcessTests NexoraGameBuildSDKTests -j 4 -- /flp:logfile=build-editor-workflow-native.log
```

The existing process fixture and startup splash target were also built explicitly.
CTest used `--test-dir build/editor-workflow-native -C Development --output-on-failure`
with these exact test-name filters (absolute worktree paths were used for retained log files):

| Filter | Result |
| --- | --- |
| `^editor\.(bounded_build_process\|project_build_settings\|project_build_settings_ui\|game_build_progress\|game_build_sdk_bytes)$` | 5/5 PASS, exit 0, 8.37 seconds |
| `^editor\.windows_build_workflow$` | 1/1 PASS, exit 0, 274.90 seconds; unchanged 360-second test bound |
| `^editor\.(game_build\|windows_startup_splash\|windows_startup_integration\|windows_gui_entry)$` | exit 1 overall: producer PASS 231.99 seconds, GUI entry PASS; both screen-pixel tests failed under foreground occlusion |

The GUI workflow sends native pointer/character messages to the actual Editor widgets and verifies
saved files, direct child/window identity, PE GUI subsystem, real game presents/normal close,
repeated Build & Run/Stop, compiler diagnostics, stale-source rejection, process draining and
unchanged prior output/authored sources. It uses Unicode/space project, manifest and output paths.
The scoped [receipt and binary/output hashes](ED-WF-02-Windows.json) omit private temporary paths.

Retained local logs are under `build/editor-workflow-native/`: `build-ui-focused.log`,
`windows-build-workflow.log`, `build-ui-final-regression.log`, MSBuild logs and the
`Tests/Editor/windows-build-workflow/` receipts/inputs/stdout/stderr. These generated files are not
committed. Earlier focused baseline passed 2/2; the initial five-test integration passed 5/5,
including the actual producer in 213.86 seconds.

## Failed evidence and limitations

The initial compiler failure was C2026 in generated SDK data; bounded byte embedding repairs the
cause rather than weakening compiler checks. First native GUI attempt failed on Windows foreground
activation; the fixture now uses the real text widget's triple-click line selection and directed
character messages without global keyboard injection. The second attempt found a real UI defect:
long compiler status pushed Build outside the client. Bounded status rendering and a visible-action
regression repair it. Both failed attempts and projects remain locally retained; the final complete
GUI workflow passed without relaxing its assertions or deadlines.

The additional unchanged startup pixel tests failed locally. The retained screen capture contains
another foreground application's pixels, not the Editor splash. This host therefore cannot supply
that run's splash/main visual evidence. No Logo/startup source or pixel assertion was changed to
force a pass. Hosted Windows must rerun these exact gates before merge. Local captured desktop
images are not committed or published.

Linux development/shipping, full hosted platform CI and exact-head acceptance remain pending.
Mac/native/physical/clean-host claims cannot be inferred from these local Windows results.
The original cloud thread resumed with supported execution settings. Readiness recovery cloned
main and completed apt-get update (exit 0), then dependency installation lost its exec-server
transport; recovery timed out after 25 seconds and the environment reported unavailable. Tool
installation completion is unconfirmed; no cloud implementation gate started or passed. Beads bootstrap/shared-history blockers remain separately
recorded in ED-WF-01; no shared database was replaced.

## Accepted exact-head validation and merge (2026-10-31)

✅ ED-WF-02 accepted in [PR #754](https://github.com/jimlee1972/Nexora/pull/754), squash
`1232574620ba784da1b3a607780986790e831b52`. The earlier pending/recovery/local-failure sections above
are retained historical evidence, superseded by these complete results. Candidate
`2eabfa2b560c72f931f2b44138a822bd6739ffba`, tree `8a80e6681feef714010f5001215a897cecf1c5b7`:

- All 42 CI checks succeeded. Full hosted development: Linux 489 passed/1 conditional skip,
  Windows 432 passed/1 conditional skip, macOS 406 passed/1 conditional skip. The conditional
  test is `twod.portable_font_bake`. Hosted Windows actual GUI workflow passed in 93.85 seconds;
  unchanged splash/integration pixel tests passed in 0.35/18.04 seconds. Original local failures
  remain retained; no Logo/startup assertion or deadline was changed.
- Separately, the new Codex Cloud Linux environment completed Development configure/build/CTest:
  **490/490 passed, 0 skipped, 0 failed**, 2000.47 seconds, final exit 0 for each command. Shipping
  configure/build also final exit 0, Zig ON. Actual GUI script Build and native/ABI gates passed;
  `editor.linux_packaged_play` passed in 45.37 seconds. See the [complete command/exit receipt](ED-WF-02-Cloud.json).
- Initial Cloud Development/Shipping builds exited 1 due to a read-only Zig cache. Task-scoped
  XDG/Zig cache directories repaired the environment; complete incremental build commands and
  full CTest were then run successfully. No source, assertions, deadlines or datasets changed.
  OOM counters were zero. The old recovery build stopped at 529/969 was an instructed interruption,
  not a successful full gate or a compiler failure.
- While Cloud ran, main advanced to `6c5adefc`. Its changes were SDK publication/VM tools and
  evidence/docs, with no compiled Engine/Apps/Bindings/Gameplay/Shaders/CMake source changes.
  Integration tree `659e4c1f9a7acd27131580900470b4663e9fcd52` passed 23 changed Markdown documents,
  SDK promotion 3/3 and real Windows guest-helper hash/UTF-8/pipe regressions. Main's 23 checks
  succeeded, candidate retained 42 successes. Merged tree was checked equal to that integration.
- Complete Cloud logs/JUnit/configuration/archive: `validation-754-evidence.tar.gz`, 439768 bytes,
  SHA-256 `f8f719ec430558c936b93865aa1f3d5768822d0faa3c28ae0ff7fcfe2a421209`.
  Full logs remain with the Cloud validation artifact; the JSON receipt is committed separately.

Beads safely adopted existing remote history into the original external paths; pull/prime and
both graph/evidence pushes succeeded. Epic `nexora-whr`, accepted child `nexora-whr.2` and remaining
`.3` through `.9` retain the approved dependencies. No `bd init`, force or shared-history replacement.
Accepted workflow progress is **2/9**; full Editor milestone count stays **0/8**. ED-WF-03 and later
orders and the new final Windows Editor ZIP are not accepted by this supporting work order.
