# Project tree indentation repair (2026-10-25)

Base: `b631c1423a8b2b946435e57dde3788116c166232`. Beads: `nexora-ipa`.
The user screenshot showed child folders to the left of their parents. Data remains a sorted
depth-first snapshot; the visual bug is ImGui interpreting `Indent(0)` and `Unindent(0)` as its
default spacing. Root rows therefore received default spacing while first-level children used
16 logical pixels. The renderer now omits both calls at depth zero and preserves the existing
16-pixel per-descendant-level offset. Snapshot ordering, IDs, access diagnostics and file actions
are unchanged. No module, linkage or ownership boundary changes.

## Validation

Packet: `build/editor-tree-indent-20261025/`. The new rendered-geometry regression failed on
the old renderer (`editor.scene_file_input`, 11.39 s; retained red log). The first corrected run
proved increasing X/Y coordinates but its reopen check sent four consecutive clicks, which
ImGui counts as one multi-click sequence. The fixture now advances its existing frame clock
by0.5 s between independent double-click gestures; production timing/behavior is unchanged.

Final Windows MSVC build passed. `ctest --preset windows-development --output-on-failure -R
'^editor.scene_file_input$'` passed1/1, zero skips (17.67 s; total17.84 s). The executable retains
the existing Scene file tests and adds actual parent/folder/child row positions, collapse hiding
only descendants, reopen, Unicode paths and distinct legal path identities at1x/2x. Both DPI
cases reported these logical row centers:

| Row | X | Y |
| --- | --- | --- |
| Parent `.nexora` |175.5|92.5|
| Folder `.nexora/scenes` |183.5|126.5|
| Child `TreeFixture.txt` |191.5|143.5|

Centers move8 px per depth because rows span the available width; their left edges/labels move
16 logical pixels per depth. Parent-before-descendant ordering and positive indentation are
asserted on rendered controls, not inferred only from paths. Collapse/reopen additionally checks
that ancestor/sibling controls remain visible and no Scene Open request is emitted for folders.

Native package probes, final ZIP hash and exact-head full Linux CI receipt are recorded after
completion in this packet and the PR. No Windows native pointer/chooser, clean-host, physical
DPI/IME or overall Editor milestone acceptance is implied; overall Editor progress stays0/8.
The repository root README stays unchanged and reader-facing.

## Local package result

Documentation unit checks passed16/16; all four changed Markdown files validated. The Editor-only
ZIP is4,103,015 bytes, SHA-256
`7d60714a756a936f758107ea0e46c6aa2e19497fc60d9eb0d8bc3dee8501e4ba`.
Direct host startup without a graphical-mode flag presented8/8 frames. The GUI entry in a
relocated Unicode/space directory presented120/120 frames and exited0. CLI project creation
under a Unicode path and project-open smoke passed (120/120, one Scene, zero references,
zero rejected UI textures). The package contains only Editor entry/host, required DLLs and
license notices, with no Player, Studio, compiler, tools, examples or source.
Exact-head Linux full-gate results are retained in the PR and local CI receipt after completion.
