# Editor layout restart and test-free build repair

Base: `b1671eaa95b0863571a4f8b4a7b636068fdc02fd`.
Beads: `nexora-6xr`, `nexora-jvk`. Scope is these two repairs; Sprite workflow work is deferred.

## Reproduced defects and changes

The fresh-context layout regression on the original renderer failed with
`restart changed the saved active dock tab` (1.45 s). Loading into an existing context had not
exercised ImGui's initial window focus. The last submitted Project panel took focus as it appeared,
replacing the saved Hierarchy selection. After `LoadLayout`, persistent shell panels use
`NoFocusOnAppearing`, and pending default Scene/Content selection is cleared. New layouts retain
normal appearing-panel focus; explicit pointer, keyboard, menu and Play focus remain enabled.
No persisted layout format changes.

`editor.layout_restart` checks Hierarchy and Project separately at 100/125/150/200% DPI. Each case
destroys its host, recreates three fresh contexts, checks selected and actually visible tabs,
exact floating Content position/size and byte-identical layout saves, then explicitly switches to
the other tab. The initial corrected focused run passed in 14.96 s. The native X11 display gate additionally
selects Hierarchy through Ctrl+4 before shutdown, reopens the real Editor process and compares all
saved dock selections; it passed in 40.68 s. Existing recovery, migration, Play and input checks remain.

The original `BUILD_TESTING=OFF` graphical translation unit failed with four incomplete-type errors
for unguarded `EditorImGuiTestAccess` definitions. Move the existing conditional before those four
methods, retaining all production API definitions and test-enabled observations. The same object
now compiles successfully. Desktop CI requires the layout regression and rebuilds the actual
graphical Editor with testing disabled after normal tests/package verification.

## Cloud execution

Linux Debian 13, GNU 14.2, CMake 3.31.6, Ninja 1.13.2, Zig 0.14.0, Slang 2026.18,
Xvfb and Mesa lavapipe. Tools and Debian packages are staged under
`/workspace/work/editor-completion/`; no system installation or sandbox relaxation was required.
`build.env` sets user-owned XDG state/cache/config/data paths, X11 include/library discovery,
the staged Vulkan ICD and tool paths. Development enables graphical shell, Zig, Slang,
ProjectPlayer/native Player and OpenSSL. This is a software display, not physical-host evidence.

The initial no-project smoke lacked a writable XDG state directory and exited before rendering;
the first corrected invocation used an unsupported `--max-frames` argument. Both logs are retained.
With user-owned XDG paths and the supported `--frames` flag, the Editor presented 8/8 frames,
then 240/240 frames with synchronization validation enabled: 480 UI draws, one upload, zero rejected
textures and no validation diagnostics.

Final scoped-source Development configure/build passes. The final focused CTest selection passes
3/3 in 54.35 s: `editor.layout_restart` 15.45 s, `editor.linux_display_acceptance` 26.97 s and
`editor.scene_file_input` 11.92 s. The native gate independently requires the saved Hierarchy tab
identity `0xB334A44B` before comparing all dock selections across process restart.

Minimal Shipping configure/build passes (114 build steps, zero registered tests by contract).
The complete modular Development graphical Editor with `BUILD_TESTING=OFF` builds (167 steps),
then rebuilds after the final focus correction and presents 8/8 native frames with 16 UI draws,
one upload and zero rejected textures. Dynamic-symbol inspection confirms no
`EditorImGuiTestAccess::` definitions in that test-free library.

The final source-head unfiltered Development CTest and hosted CI receipts are recorded in the
associated delivery PR. These are independent of the earlier failed/interrupted candidate below.

The first full-gate attempt retained an environment failure (missing clang++) and optional font-tool
skip; staging Clang 19 and the pinned font requirements cleared both focused checks. Its native
plugin-manager and Build Console input failures passed unchanged focused rechecks. It also caught
`editor.scene_file_input` failing `Folder Tab focus failed` with the initial overbroad panel flag.
The final implementation limits suppression to loaded layouts, preserving first appearance of
Content in a newly created shell. After 470 completed cases, the obsolete candidate's traversal
was interrupted before the remaining serial soaks; its original log is retained. Final-source
validation is recorded separately rather than relabeling that run. No assertion, deadline or
fixture size was weakened.

No Windows/macOS or physical display/DPI/IME/docking acceptance is inferred. Full Editor acceptance
remains 0/8; ED-WF acceptance remains 4/9. The root GitHub README remains unchanged.
