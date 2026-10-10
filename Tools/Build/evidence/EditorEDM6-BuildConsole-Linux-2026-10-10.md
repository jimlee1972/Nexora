# ED-M6 graphical Build Console — Linux, 2026-10-10

Beads: nexora-62u.1.9. Prerequisite: bounded native BuildProcess (nexora-62u.1.8).
This support feature does not accept the full ED-M6 milestone.

## Validation

Full Linux Development with graphical Editor, Slang, Zig gameplay, Showcase and native
ProjectPlayer enabled: configure/build succeeded; `ctest --preset linux-development` passed
225/225 in 546.51 seconds, zero skips. Minimal `linux-shipping` configure/build passed, 74
build steps. Shipping disables Editor and does not execute this graphical panel.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build --preset linux-development -j4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j4
```

The cloud reused independently verified unchanged pinned ImGui/Vulkan-Headers source trees
through FetchContent source-directory overrides after public dependency fetch was unavailable.
No dependency pin or source changed.

Focused acceptance passed 4/4 in 14.71 seconds: editor.build_process_console (1.03s),
editor.linux_build_process_console (7.43s), editor.linux_static_project_export (6.21s), and
editor.static_project_export_ui (0.04s). These execute actual 1x/2x controls and native host
input; no mocked child process replaces the real runner. The fixture emits NUL, invalid UTF-8
and terminal escape bytes; the test requires escaped display while retaining exact raw bytes.

The native host explicitly runs/cancels a 20-second child, reports failure exit 7, and reports
exit zero with artifact_verified=0. Closing during another running child drains within 3.5 seconds;
source bytes remain unchanged and Vulkan validation errors fail acceptance. Existing export
menu/keyboard controls retain their own actual acceptance.

## Contract and limits

Run starts only with a nonzero writable project scope, stopped Play and no blocked recovery/exit.
Inputs are discrete executable/cwd/argv fields, never a shell command. Configuration and snapshots
own their memory; a frame borrows the serialized runner only for the call. Raw output is bounded
and escaped before ImGui; commands, child output and environment are not persisted or added to
native host diagnostics. Scope/authority changes cancel and remove old display. JobSystem outlives
the runner; shutdown cancels before joining workers. This is not a process sandbox.

No compiler/toolchain discovery, target matrix, build artifact verification, reproducible manifest,
remote deployment, debugger or extension installation is accepted here. Hosted checks at the final
published head are independently required before merge. Root roadmap milestones remain 0/8.
