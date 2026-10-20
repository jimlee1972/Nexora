# ED-M1 graphical Content folder clipping

Date: 2026-10-20 (Asia/Taipei). Task: `nexora-xke.6`.
Baseline: main `110b265de594f30b93f52fd6173faf195b3f36ed` (PR654).
This is a supporting WP2 rendering slice; Editor milestone acceptance remains 0/8.

## Behavior and compatibility

Content asset rows already used a clipper, but every immediate folder still submitted a selectable,
literal text and drop-target handling on every frame. The folder snapshot now uses its own ImGui
clipper with the exact text-line-plus-spacing height of the existing literal row. Logical folder
counts and the full scroll extent are preserved. Visible and navigation-required ranges retain the
same project-root/generation/role/path IDs and deferred folder entry. Frame-local position
observations retain only visible controls. A private test counter distinguishes logical folders
from submitted folder rows; it is reset before testing whether the Content window can draw.

Asset clipping follows the completed folder clipper and still supports explicit endpoint reveal.
Parent breadcrumbs, read-only navigation, pointer double-click, folder Enter ownership and existing
typed drop admission remain unchanged. There is no public API, persisted format, stable C/Zig ABI,
module graph, dependency, linkage boundary or CI/deadline change. No Shipping linkage gate is needed.

This bounds control submission, not all Content work: ChildFolders still scans the item index,
allocates an owning frame snapshot and sorts it, and activation lookup still scans that snapshot.
The fixture's submission threshold is not a hard-coded production row cap or a whole-frame timing
budget. Broader production asset scale and physical workflow acceptance remain open. The rendering
contract and EN/zh-TW Editor roadmaps agree; the concise root README is unchanged. Other roadmap
references to virtualized Content do not add a new milestone claim.

## Regression evidence

The existing real ImGui dependency test now includes 2048 same-marker-suffix folders plus two
current-folder assets at 1x and 2x, in writable and read-only sessions. Synthetic indexed items avoid
creating 2048 unnecessary files; exact existing source/sidecar bytes are still snapshotted and checked.
Assertions cover logical count and total scroll extent, middle scrolling, absent far-row controls,
Tab across the clipped boundary and Enter, owning parent breadcrumbs, asset End reveal below the
folder block, far-folder pointer double-click, filters, selection, revision and pending Rename Undo.

Before clipping, the new regression failed in 0.51 s because all folder rows were submitted.
An initial one-Tab expectation was corrected to Dear ImGui's existing input policy: the first Tab
after mouse input reveals the clicked navigation cursor; the next Tab advances across the boundary.
Both behaviors are asserted, followed by entry into the exact next folder. No navigation assertion,
input guard or production deadline was removed.

The final focused command was:

```bash
ctest --preset linux-development -R '^editor.content_(dependency_reveal|keyboard_shortcuts|diagnostic_display)$' -V
```

All three passed in 32.91 s: diagnostic display 0.04 s, keyboard shortcuts 26.60 s, and dependency/
folder controls 6.26 s. The observed maximum was 55 submitted folder rows out of 2048 logical folders
at each DPI, including the tested navigation range. Existing literal glyph, copied project/generation,
forward/reverse Reveal, input/modal/close and source conservation regressions remain in this target.

## Full local gate

Host: Linux 6.18.55 x86_64, GCC 14.2, CMake 4.4.4 and Ninja 1.13.2. The existing local
Development cache uses Modular linkage with graphical Editor and 2D enabled and Slang disabled.
Pinned vendor source caches and machine-local settings are not committed. Exact required commands:

```bash
cmake --preset linux-development
CMAKE_BUILD_PARALLEL_LEVEL=4 cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development --output-on-failure
```

Configure and full build passed. CTest registered 285 tests: 273 passed, 12 native GPU/display/backend
cases skipped, zero failures, 387.30 s. The actual Content folder/dependency test passed in 4.58 s,
keyboard shortcuts in 27.60 s and graphical diagnostics in 0.04 s. Deep hierarchy passed in 86.69 s;
100k lifecycle passed in 282.93 s under its unchanged 300 s deadline. No compilation ran alongside
CTest. No timing threshold, native assertion or sanitizer setting was relaxed. Skips do not certify
physical or native GPU acceptance; existing scale/timing follow-ups remain open.

Documentation tests passed 16/16; changed-document links, bilingual routing, touched-file formatting
and `git diff --check` passed. All roadmap references to Content virtualization were reviewed; only
the paired Editor roadmap requires this supporting entry. Source/test digests match the validated
files. No production performance budget or milestone was closed.

| Retained log | SHA-256 |
| --- | --- |
| `folder-clip-red.log` | `ee3369135f22715d66879e840b917395b30665a7fd91fe80b64fdf91d70e7cea` |
| `folder-clip-focused-controls.log` | `f635fc6ff24caa823cca7417e94e2cb95379be35ce479e428140fd6bcf8cfa59` |
| `folder-clip-full-configure.log` | `00507a10e3f89a2c730848101943cc5f9ebf06ef96717e51173afec0282d0fa9` |
| `folder-clip-full-build.log` | `584cc8652a2ff9f829fba445f86a91587689db8751cf6a0ecd7ac616bdc68712` |
| `folder-clip-full-ctest.log` | `1e09bbe08e6c83ea6733c997aed36915fae6f2c58a1e079d9dc7b69966991f42` |
