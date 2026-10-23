# Windows Editor horizontal wheel and first Save All

## Scope and source

Tasks `nexora-3mj`, then `nexora-zey`; continuation of `nexora-nqn.4.2` and `.4.3`.
The original checkout remains on its existing branch. Changes use the existing isolated
`build/nqn-source` worktree and `codex/windows-editor-nqn-acceptance` branch, merged with main
`50c723888b9aff8246047a9bd0f881cdac9c93e0` at `3e4e63d8` before implementation.
Raw evidence is retained in `build/3mj-zey-20261023`, including command stdout/stderr, exits,
source patches, frozen runtime hashes, disposable project bytes, images and capture metadata.
No working project, global display/language setting, timeout, assertion, stress fixture or CI
registration is changed. Earlier packets retain their independent acceptance boundaries.

## Horizontal wheel

Win32 WM_MOUSEHWHEEL and X11 buttons 6/7 use right-positive horizontal deltas. ImGui subtracts
its wheel X from the scroll offset, so passing that delta unchanged reverses movement.
The adapter now negates X; Y stays unchanged. Cocoa normalizes its native horizontal delta at
the Window boundary, preserving its previous net ImGui direction. Native macOS interaction
is not accepted from this Windows run.

The new `editor.imgui_contract` regression submits public Window events over a real ImGui
child with both scrollbars. It checks right from the left edge, left back to zero, down and up,
including no cross-axis displacement. Before the fix it fails the rightward movement assertion
(exit 0xc0000409, CTest exit 8); after the fix it passes without changing that assertion.

Physical Windows 10 Home input uses the supported native `sky.scroll` API and the user-authorized
GDI capture alternative. A one-node, six-read-only-component fixture exposes both Inspector axes.
Frame 004 retains an initial pointer-hover transition that scrolled Hierarchy; it is not treated
as Inspector acceptance. With the pointer settled in Inspector, +600 X (005) advances right;
-600 X (006) restores the origin. +360 Y (007) reaches rows 4/5; -360 Y (008) restores rows 0/1.
Frames 004/006/008 have identical SHA-256. The process exits 0 and scene bytes remain unchanged.
Each image has HWND/PID, UTC, dimensions, method and SHA-256 metadata.

## First Save All and Save and Exit

The fresh schema-1 project upgrades to schema 2 and binds `.nexora/scenes/Main.scene` without
creating its parent directory. Single-scene Save creates parents, but Save All deliberately
requires existing ordinary parents. Frames 009–013 reproduce the rejected Save and Exit,
safe Cancel, then successful single-scene Save. The missing parent is recorded before repair.

The application writer now prepares that one private directory on Save All when an owned scene
uses the default path. Every host Save All route shares that preparation; startup does not write it.
It writes no scene bytes and refuses read-only/recovery/external-change states, nonordinary
metadata parents, occupied files and aliased scene directories. The batch transaction is unchanged:
no safety rejection, stale-baseline check, sibling-stage check or recovery policy is bypassed.
The new regression covers idempotent directory-only preparation, first coordinated save and
independent reload, occupied staging preservation, read-only rejection and occupied storage;
the non-Windows branch additionally rejects a directory symlink.

An earlier startup-preparation version passed focused tests and physical frame 015 shows the
first Save All reporting all files saved and a clean scene.
A separate fresh project, whose scene file does not exist before confirmation, uses Alt+F4 then
Save and Exit (016–017): exit 0 and a valid Main.scene is published. That version was refined to
prepare on demand so pending recovery cannot change startup behavior. Its gate was explicitly
interrupted, with process identities, partial logs and exit -1 retained under `startup-version-*`;
it is not a completed gate. Final runtime verification is recorded separately. All disposable files
are retained.

The final on-demand version is separately frozen as `final-bin`: Editor SHA-256
`abc739d0dfbb369b86d6b9d901a28a39e0ecce56efb87af072dd60d7409db84c`.
Its Window and EditorImGui DLL hashes are identical to the wheel-tested runtime, independently
verified in `evidence.json`; that continuity is limited to those components, not whole-runtime
identity. Before first Save All, neither the scene directory nor scene file exists. Frames 021–022
show first Save All success. A separate fresh project has no directory or scene before Alt+F4 /
Save and Exit (043–044), then publishes Main.scene and exits 0 with no staging or recovery residue.

## Continued physical interaction

Final runtime frames 025–038 exercise the installed Chinese IME through native key events at 100%:
start, update, tone, candidate list, selection and commit in Rename. Saved scene bytes contain exactly
`node 2 0 你`. A second composition is cancelled with Escape, then Rename is confirmed without
additional characters or dirty state; scene SHA-256 remains identical. Candidate placement is
observed adjacent to the field; the separate earlier Home/End caret-follow evidence remains scoped
to its earlier runtime. No second-scale caret/candidate acceptance is inferred.

Frames 039–042 show Content undocking, movement and floating placement restored after restart.
The Content ini section is identical (`Pos=590,620`, `Size=716,180`, no DockId). Full ini byte
identity is **not** accepted: the initial evidence assertion failed because the left dock's selected
tab changes from Hierarchy to Project on restart. Both ini files, the diff and failed check are
retained; follow-up `nexora-6xr` tracks the policy/repair. Redocking attempts only move the floating
panel and do not establish successful docking. Held-input focus loss has no supported across-action
hold API/operator, so no completed key press is substituted. These rows remain blocked.

## Validation and remaining acceptance

- Graphical Windows Development configure/build: exit 0, DX12 enabled, Vulkan disabled.
- Final focused CTest: `window_presentation.contracts`, `editor.additive_scene_session`,
  `editor.scene_save_batch`, `editor.scene_file_contract`, `editor.imgui_contract`,
  `editor.windows_dpi_ime_contract`: 6/6 passed, exit 0 (126.43 seconds).
- Final frozen runtime `--graphical --frames=240`: exit 0, acquired=presented=240,
  ui_draws=2631, ui_uploads=1, ui_rejected=0; GPU timing source=2, software=0.
- Full user-revised Windows local baseline: 310 selected, 304 passed / 6 skipped / 0 failed,
  exit 0, 1662.25 seconds. Full inventory: 313; selected names match the inventory minus exactly
  the three unchanged stress cases, independently checked in `baseline-inventory-verification.json`.
  Command: `ctest --preset windows-development --output-on-failure -E '^(editor.deep_hierarchy|editor.hierarchy_scale_lifecycle|editor.overview_pose_snapshot)$'`.
  Exactly the unchanged
  `editor.deep_hierarchy`, `editor.hierarchy_scale_lifecycle`, `editor.overview_pose_snapshot`
  remain in their separately blocked local 100k stress tier (`nexora-wo1`). Full CI registration
  remains unchanged.
- The six skipped local cases are `renderer.d3d12_texture_recording`,
  `renderer.d3d12_graphics_enhanced_barriers`, `renderer.d3d12_recording_isolation`,
  `renderer.d3d12_recording_gpu_timing`, `renderer.d3d12_recording_enhanced_barriers`,
  `renderer.d3d12_recording_benchmark`; they are not accepted from the GUI smoke.
- Linux configure/build/full CTest requires exact-head CI; Linux was not run on this Windows host.
  No linkage boundary changes; a separate Linux Shipping gate is not required.
- Documentation tests: 16/16 and changed-document/bilingual-link checks pass. Two initial subprocess
  launches failed with WinError 1920 because `python3` resolved to the WindowsApps alias. The rerun
  uses a task-local copy of the installed Python executable under that name, identical SHA-256,
  with process-only PATH/PYTHONHOME/PYTHONPATH; tests and system settings are unchanged. Both logs
  are retained. Root README remains unchanged and concise.

Interactive input pauses while native-window gate tests run, avoiding foreground contention.
The initial interaction capture used an invalid HWND and the PID guard rejected it before capture;
the corrected HWND was selected from the live window inventory. This is a harness error.
Additional DPI/cross-monitor, successful redocking, held-input focus loss and complete operator
records remain unaccepted. Prior keyboard recovery and layout evidence stays scoped to its prior
frozen runtime. Original WGC capture remains open as `nexora-nqn.4.6`; GDI does not repair it.
No clean-host, Linux physical-display, screen-reader or ED-M0 milestone acceptance is claimed.

## Integrated main revalidation

Main advanced to `8169c012dc570eb5ce0e6819ec41ef5ce12a0902` (font coverage and native input
reliability). The existing feature branch merged it at `13534509d9748f32734653b94a614c9fb49d2738`;
all upstream changes are preserved. The earlier results above retain their original source scope.
New evidence is in `build/3mj-zey-integration-20261023`, using a separately frozen runtime:

- `NexoraEditor.exe`: `dbf0247ea895e2e02d3d1e2afd03abe960c57e6077bffbb0e614156575591683`.
- `NexoraEditorImGui.dll`: `74ede5d04b4e1a7675631e547f695f3ba1dabf65872b46cca2e803bc6b0b9956`.
- `NexoraWindow.dll`: `15fb2b6a3829c986a509bc86f2e96e3f40af70de352bb90413ba1112ac8518f3`.

Integrated configure/build exit 0. The six focused cases listed above plus `editor.font_glyphs`
pass (7/7, 163.10 seconds). Native Inspector right/left and down/up round trips pass;
settled frames 004/006/008 are byte-identical and the fixture scene remains unchanged.
Fresh first Save All and a separate fresh first Save and Exit succeed without a pre-existing
scenes directory. Native IME commits `你` once, cancellation preserves the saved scene SHA-256
`26375e1286299b804efa6610e01a0df6241d1a6aa6db7330600b4a2f16329a11`, and reopening
shows that saved name. Every interactive session exits normally with code 0. The 240-frame
DX12 smoke exits 0 with acquired=presented=240, ui_draws=1914, ui_uploads=1, ui_rejected=0,
GPU timing source=2, software=0.

The initial Escape-guard stop remains recorded in `interaction-stopped.json`; renewed user
authorization in a subsequent turn allowed the supported native interface to resume. The old
stop does not describe the completed sessions. GDI remains the user-approved capture alternative.
Integrated full local baseline passes: 314 registered, 311 selected, **305 passed / 6 skipped /
0 failed**, exit 0, 1865.39 seconds. The command is the unchanged local-baseline command above.
The inventory check proves that only the same three separately blocked stress cases are excluded;
the six skips are exactly the same native recording cases listed above. The integrated documentation
unit suite passes 16/16, and changed-document/bilingual-link checks pass. Verification preserves
all 1,231 previously manifested files, validates all 20 frozen binaries and source hashes, and
indexes 31 integrated capture frames with metadata. No Editor or test process remains after the gate.
Final exact-head Linux/Windows/macOS and other applicable CI must pass before PR #701 can merge;
the final CI receipts and expected-head merge record are retained with the packet and PR.
Prior recovery/layout evidence is not promoted to this runtime; `nexora-6xr`, redocking,
held-input focus loss and additional DPI/monitor/operator gates remain open.
