# ED-M0 target-host evidence tooling: Linux validation

Date: 2026-10-20 (Asia/Taipei).
Source: parent `b8c31d72437a77cc190ad49eb70cf97b6c23fac6` plus the accompanying evidence-tool patch.
This is WP8 supporting tooling and automated validation, not physical-host acceptance.

## Scope and invariants

The standard-library Python recorder supplies the complete Linux/Windows operator checklist,
retains exact Editor argv, source/configuration/host/window/backend metadata, executable SHA-256,
and copied artifact hashes. Windows observations must name actual 100/125/150/200% scales;
IME candidate observations require two different actual scales. Unknown/duplicate/missing rows,
missing/changed/outside artifacts, failed smoke logs, virtual displays and absent operator consent
cannot produce `COMPLETE_OPERATOR_RECORD`. That status indicates evidence completeness only.
Actual visual/input/IME claims still require human review of the artifacts.

The existing PowerShell interview is explicitly preliminary. Editor runtime, model/persistence,
module dependencies, RHI ownership, C/C++ ABI and graphical feature gating are unchanged.
The recorder does not seed journals or inject crashes. It uses a disposable project and a
bundle-local recent-project list. See [operator instructions](../../Editor/README.md).

## Environment and commands

- Debian GNU/Linux 13, GCC 14.2.0, CMake/CTest 4.4.4, Ninja package 1.13.2.
- Slang 2026.18, Xvfb, xdotool, Mesa lavapipe 25.0.7 and Vulkan headers/loader 1.4.309.
- Clang 19.1.7 is required by the existing production reflection-tool test.
- Dependencies were installed/extracted outside the repository under `/workspace/.tools`;
  no generated outputs, dependency copies or user presets belong to this patch.
- Dear ImGui resolved tag `v1.91.9b-docking`, revision `4806a1924ff6181180bf5e4b8b79ab4394118875`.
- Development is Modular with graphical shell and Slang ON. Shipping uses the unmodified Minimal
  Monolithic preset, `BUILD_TESTING=OFF` and `NEXORA_ENABLE_EDITOR=OFF`.

The session uses `CMAKE_PREFIX_PATH` for the extracted X11 SDK, `LD_LIBRARY_PATH` for its runtime
libraries, Slang and tool directories on `PATH`, and `VK_DRIVER_FILES` pointing to the extracted
lavapipe ICD. Local X11/Vulkan library overrides select the same SDK/loader. These are environment
selectors, not project policy changes. The graphical configure is:

```bash
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON
cmake --build --preset linux-development -j 4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping -j 2
```

## Focused results

- Graphical Development configure and all 783 build steps passed.
- Shipping engine configure and all 102 build steps passed; this is not a Shipping Editor build.
- `build.editor_target_evidence` passed with graphical ON and in a separate OFF configure.
  Its current 13 portable cases cover every required scenario, actual DPI mismatch, equal candidate scales,
  virtual/unattested/incomplete metadata, corrupt/outside artifacts, invalid smoke output, duplicate
  records, forged status, interrupted interviews and preservation of blocked rows/artifacts.
- Separate OFF configure with Slang OFF had no `nexora_imgui` population, `NexoraEditorImGui`
  target, ImGui source entry or ImGui link entry. It reused only the pinned Vulkan headers.
- `window_presentation.vulkan_scene`, `vulkan_pbr` and `vulkan_swapchain` passed 3/3 (15.44 s).
- Documentation regression suite passed 16/16; changed Markdown link/structure checks and
  `git diff --check` passed. No C++ files changed and the root README remains unchanged.

## Complete suite and isolated failures

The full graphical `ctest --preset linux-development` executed all 331 tests with no skips:
326 passed and 5 failed in 1185.73 s. `build.v2_production_toolchain` lacked `clang++`; after
installing Clang 19.1.7, its direct invocation passed. Four unchanged native fixtures also failed:

| Test | First failure | Serial rerun |
| --- | --- | --- |
| `build.v2_production_toolchain` | Missing Clang executable | Passed, 0.57 s |
| `editor.linux_scene_files` | Save-and-Exit rejected-path retry | Passed, 40.26 s |
| `editor.linux_native_scene_preview` | First-axis drag Undo bytes | Passed, 30.55 s |
| `editor.linux_native_scene_meshes` | Center gesture Undo bytes | Passed, 24.27 s |
| `editor.linux_prefab_placement_source_rebase` | Review event timeout | Passed, 14.09 s |

`ctest --preset linux-development --rerun-failed` subsequently passed 5/5 in 109.74 s. A separate
unchanged SceneFiles invocation while the full suite ran failed at an earlier Open/Undo assertion.
These native sources match the fetched main commit and were not edited for this patch. The rerun
establishes successful coverage at this source, not a root-cause fix or a clean first-attempt suite.
The intermittent native-input/control risk remains open; no deadline, Undo count, saved-byte,
review-event or acceptance assertion was relaxed. The full log and failed-test rerun are retained
in the session's `ctest.log` and `ctest-rerun.log`. Investigation is tracked as Beads
`nexora-nqn.15`; this patch does not claim to repair it.

## Activated Vulkan synchronization validation

After the full suite and isolated rerun, configure/build were repeated with the same cached
Development settings (`ninja: no work to do`). The native Editor gates ran with the actual layer:

```bash
VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation \
VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT \
VK_LOADER_DEBUG=layer \
ctest --preset linux-development \
  -R '^editor\.(linux_(display_acceptance|validation_output)|native_surface_lifetime)$' -V
```

All 3 passed in 44.84 s. Loader output contained 15 device-layer insertion records naming
`VK_LAYER_KHRONOS_validation`. Scanning the full log found no `Validation Error`, `VUID-` or
`SYNC-HAZARD-` records. This includes the actual native display/recovery and retained-resource
lifetime gates; it is explicitly Xvfb/lavapipe automation, not physical GPU/display validation.

## Actual recorder integration

The new recorder launched the real graphical Editor on an isolated Xvfb display with a disposable
project and EOF on operator input. At 8 frames, native diagnostics reported acquired=8,
presented=8, ui_draws=59, ui_uploads=1 and ui_rejected=0. Native smoke passed; the process-level
recorder returned exit 1, `INCOMPLETE`, `display_kind=virtual`, and all manual rows remained blocked.
Standalone validation also returned exit 1. Both raw output and copied hashed artifacts were retained
in the session evidence bundle. This verifies that successful native rendering cannot accidentally
satisfy physical-display/operator acceptance. The Mesa cache warning reflects read-only home storage,
not a native validation result.

## Hosted fixture correction and semantic handoff

The initial source `8d899641` reached macOS 26.6.2 / Python 3.15 in hosted CI. The new interrupted-
interview test made its final existing-directory assertion after leaving the simulated Linux host
scope, so the real Darwin host correctly raised unsupported-platform `ValueError` instead of the
fixture's expected `FileExistsError`. The assertion now stays inside the simulated host scope.
A thirteenth case explicitly verifies that an unsupported host creates no bundle. Linux execution
and an outer Darwin-host simulation both passed all 13 cases; this simulation is not a native macOS
execution result. Production recording/platform policy is unchanged. Fresh final-source hosted
checks are required; the failed original-source check is not relabeled as passed.

The paired [accessibility handoff](../../../Roadmap/en/Editor_Accessibility_Handoff.md) inventories
actual ProductShell panels and scene commands, recovery's host-local identity/display labels,
missing semantic roles/actions/values, owned scope/action lifetime, focus/error/live regions and
UIA/AT-SPI/AX candidates. Proposed recovery action IDs and future snapshot/bridge contracts are
explicitly distinguished from current APIs. Source and Markdown review verify documentation only;
no accessibility tree, adapter, dependency or physical-reader acceptance is introduced.

## Remaining acceptance

Physical Linux/X11 readability, real input/docking/recovery, Windows four-scale/per-monitor DPI,
installed-IME composition/candidate positioning, and supported-host glyph/visual parity are still
unaccepted. No Windows PowerShell/native/IME or macOS execution is claimed from this Linux worker.
Historical evidence must not be relabeled as new target-host acceptance. The milestone remains open.
