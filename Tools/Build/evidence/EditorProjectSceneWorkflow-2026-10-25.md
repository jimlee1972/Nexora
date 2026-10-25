# Main Editor project and scene workflow

Base: `e6533cb9` (latest remote main at task start). Beads: `nexora-m10`.

## Behavior and boundaries

- The graphical executable opens its Project Browser without `--graphical` or a project path.
  Explicit project/create/report CLI commands retain their automation semantics.
- The Project panel displays a root-relative file tree, including non-assets and empty folders.
  One background reader supplies owning snapshots; no UI/document/workspace object enters it.
  Symbolic links are listed without traversal or scene activation. The 100000-entry, 64-level,
  32 MiB path budgets produce visible partial-list diagnostics rather than claiming a full tree.
- The main application retains one Scene. Its New/Open paths reuse existing generation tokens,
  save-before-switch, overwrite and Save/Discard/Cancel protection. A failed save or canceled
  decision preserves the current document. Old composition metadata is left untouched.
- Left double-click opens a different scene from Project; a single click selects. The separate
  Content browser and supporting additive Core/widget APIs remain available.

## Validation record

Local packet: `build/editor-workflow-20261025/`. Initial MSVC build passed. Three existing UI
tests passed in the restricted run; two scene fixtures could not save their initial files there.
The same scene-tab and scene-file executables passed 2/2 (23.94 s) with normal filesystem access,
including actual 1x/2x Project rows, Unicode scene paths, single/double click and dirty Cancel.
This first result precedes the final Discard, Window shortcut and native-test additions.

Final MSVC configure/build passed. Five selected CTests passed 5/5, zero failures/skips (62.81 s):
`editor.imgui_contract`, `editor.project_upgrade_ui`, `editor.project_picker_ui`,
`editor.scene_tabs`, and `editor.scene_file_input`. The latter includes the Project tree's
Unicode scene path, single/double click, dirty Cancel/Discard, stale-request rejection at 1x/2x,
and reader entry-budget rejection before further directory enumeration.
Existing scene-file tests retain failed-save, Save As, overwrite and save-before-switch coverage.
The Project-tree fixture subsequently uses the workspace's canonical root, matching the UI
contract even when a platform's temporary directory is an alias. The affected executable was
rebuilt and `editor.scene_file_input` passed again 1/1 (20.50 s); production binaries are unchanged.
Path identities additionally use the existing raw-byte identity helper. Actual rendered IDs for
`First###same.scene` and `Second###same.scene` are distinct at 1x/2x; the affected executable passed
again 1/1 (18.43 s). The native direct-host/relocation/project gates were repeated with this DLL.

The initial no-mode-argument native package probe reproduced exit 2 because the interactive
default was incorrectly chosen inside option parsing. It is now selected after all options are
parsed. The final direct-host probe passed 8/8 presented frames, followed by the relocated GUI
entry with no `--graphical` injection (120/120 frames, exit 0), CLI creation under a Unicode path,
and project-open smoke (120/120 frames, exit 0, one scene, zero references). Both full native
smokes reported zero rejected UI textures. The failed candidate remains in the packet's `release/`
directory; the final deliverable/evidence is under `delivery/`. Native probes use isolated
recent-project state. The reader counts pending and visited entries against one global budget.

The Editor-only ZIP is 4,102,849 bytes, SHA-256
`1fe8598234b3e555e6fe382b430a88e029acb8d949a9dde224c2574725f72b18`. It contains only the
GUI entry/host, required DLLs and license notices. No Player, Studio, compiler, examples or source.
Documentation checks passed 16/16; package dependency tests passed 4/4.

The first hosted head `cd429d56` stopped during Linux compilation: GCC rejected a zero-length
printf format in the tree node. The call now uses a typed `%s` format without weakening warnings.
The Project tree also retains the previous export progress/result/verification display through
the same shared renderer used by the metadata-only fallback. Local and relocated native gates
above were repeated with both corrections; the earlier failed hosted runs remain visible.
The follow-up Linux display build passed compilation and exposed a stale inventory assertion
requiring the removed native multi-tab name. The workflow now requires three registered tests:
the retained supporting tab UI contract, scene-file controls and the new native single-scene gate.
The Core additive/save/composition inventory requirements remain unchanged.

Exact-head hosted Linux Development/Shipping results are recorded in the PR and local CI receipt
after completion. The native Xvfb gate now
tests default startup, actual Project-tree double-click, single-scene save destination and retained
legacy metadata; prior multi-tab application acceptance is historical. Core additive tests remain.

No independent clean-host, Windows native chooser manual interaction, physical DPI/IME or full
Editor milestone acceptance is claimed by these tests. Root README remains reader-facing and
overall Editor milestone progress remains 0/8.
