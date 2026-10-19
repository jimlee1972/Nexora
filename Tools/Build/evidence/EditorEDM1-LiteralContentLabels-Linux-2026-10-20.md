# ED-M1 literal Content labels and scoped control identities

Date: 2026-10-20 (Asia/Taipei). Task: `nexora-xke.4`.
Base: `69cb7b25` on `main`. This supports ED-M1; Editor milestone acceptance remains 0/8.

## Problem and resulting behavior

Dear ImGui interprets `##` as a display terminator and `###` as an ID hash reset.
Previously, `[Folder] Foo###same##Content/Foo###same` and
`[Folder] Bar###same##Content/Bar###same` produced the same ID with the same window seed.
Appending a path, or placing that path in a string `PushID`, does not escape these markers.
An asset named `同名##%###same.scene` also lost its visible suffix.

Content now hashes canonical project-root bytes and Content generation, then the control role
and owning UUID/relative-path bytes through `ImHashData`. Fixed hidden widget labels retain
ordinary Selectable/Button navigation, hit, drag/drop and context behavior. Names are drawn
with an explicit begin/end text range. Selected-scene Enter ownership and folder activation
compute the same row ID as rendering. Folder/root/generation changes mark retired navigation
for reinitialization on a subsequent focused Tab; a pointer-only scope transition does not request
navigation reinitialization. Surviving breadcrumb/search controls and focused current-view rows
clear that pending state. IDs are ephemeral ImGui identities, not persisted IDs,
and retain ImGui's ordinary finite-hash collision limits.

## Regression coverage

The existing `editor.content_dependency_reveal` executable now also exercises literal labels:

- Actual emitted glyph vertices/UVs contain full UTF-8 names and `##`, `###`, `%` suffixes;
  assertions search the real Content draw list rather than copied label observations.
- Actual WindowEvent pointer double-clicks and Tab/Enter independently open `Foo###same` and
  `Bar###same`, and traverse `Nested##%###same`; marker breadcrumbs navigate to the right parent.
- Same-basename scene assets retain distinct UUID selection; Tab-focused asset Enter requests its
  owning path, while folder Enter does not also open the selected scene.
- Both 1x and 2x framebuffer scales cover writable and read-only sessions. Generation replacement
  and a copied project with the same relative paths/UUIDs/generation produce distinct live IDs.
- Navigation preserves exact source/sidecar snapshots, Content revision and a pending Rename Undo.
  Existing forward/reverse Reveal, clipped scrolling, input/modal/close guards remain exercised.

## Scope and compatibility

Only EditorImGui rendering/private test access and its existing test executable change.
Private frame-local folder-position observations retain only visible rows, avoiding a second
copy of every indexed folder path in large projects. No new dependency, build/CI setting,
module graph, stable C/Zig ABI or persisted format changes.
Public Editor APIs remain unchanged. No linkage boundary changes require a Shipping gate.
The Editor roadmap EN/zh-TW pair and the rendering contract are synchronized; the concise root
README milestone summary remains unchanged. Native GPU/physical display, semantic screen readers
and complete project/asset workflow acceptance remain separate gates.

## Initial focused failure and correction

The first focused run passed `editor.content_keyboard_shortcuts` (35.48 s) but failed the new
marker traversal assertion: after entering `Foo###same`, repeated Tab retained the previous
folder's no-longer-submitted navigation ID. A diagnostic repeat localized this to entering the
nested folder. An initial correction called `NavInitWindow` after a successful folder-row transition, and the
same marker assertions passed (1.27 s). The first full gate then exposed a parent-traversal
regression in the existing shortcut test: 272 passed, 12 native Vulkan tests skipped, one failed,
347.19 s total; lifecycle passed 231.07 s and deep hierarchy passed 76.51 s. The marker test passed
1.68 s. The first correction had not reset retired navigation IDs on the return route.

A second correction handled folder/root/generation transitions in the shared view-scope update,
reinitializing only when Content owns navigation focus. Both unchanged focused suites then passed:
keyboard shortcuts 27.79 s, marker/dependency controls 0.98 s, 28.78 s total. No assertion, key route,
timeout or fixture requirement was removed. A second full gate then passed shortcuts (32.94 s) and marker/dependency (2.17 s), but failed the existing
read-only diagnostic-selection assertion in `editor.content_diagnostic_display` (0.02s): automatic
navigation reinitialization interfered with the first pointer selection after scope replacement.
This run had 272 passes, 12 native Vulkan skips and one failure in 353.01 s; lifecycle (245.25 s)
and deep hierarchy (107.75 s) passed.

The final correction records retired navigation scope and waits for an actual focused Tab before
calling `NavInitWindow`; a surviving breadcrumb/search control or focused current-view row clears that pending state.
The unchanged diagnostic, shortcut and marker/dependency suites then passed in 0.04 s / 33.69 s / 1.66 s
(35.41 s total). Full configure/build/CTest is repeated again for this final implementation.

The corrected navigation implementation then passed the full gate: 285 registered, 273 passed,
12 native Vulkan skips, zero failures, 432.67 s. Lifecycle passed 260.98 s, deep hierarchy (159.18 s),
shortcuts (88.88 s), markers/dependency (1.89 s) and diagnostics (0.06 s). Review then bounded private
folder-position observations to visible rows to avoid duplicating every indexed folder path.
The complete gate is repeated for that final observation-only refinement as well.

## Final validation

Host: Linux 6.18.55 x86_64, GCC 14.2.0, CMake 4.4.4, Ninja 1.13.2. Development/Modular,
Editor and 2D enabled, Slang disabled in this local cache. Builds completed before CTest;
no concurrent compilation, deadline override or committed CI change was used.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development --output-on-failure
```

Final source: 285 registered, 273 passed, 12 native Vulkan skips, zero failures in 338.69 s.
Lifecycle (226.47 s), deep hierarchy (97.74 s), shortcuts (39.98 s), marker/dependency (1.68 s)
and diagnostics (0.04 s).
The skipped native presentation/recording/material tests require unavailable display/GPU conditions;
these local results do not certify native or physical host acceptance. All default timeouts remain.
The three touched C++ file digests were unchanged throughout the final gate.

`python3 Tools/Build/TestDocumentationCI.py`: 16/16 passed (1.287 s). Changed-document routing/link
validation returned no errors; touched C++ clang-format dry-run and `git diff --check` passed.
Root README was reviewed and remains unchanged. Shipping is not required for this rendering/private
observation change because linkage boundaries are unchanged.

Retained local log SHA-256 digests:

| Log | SHA-256 |
| --- | --- |
| `literal-full-ctest.log` | `ba0fe6436f70411740cd25445d9fc2495fb00bb7ccc587c5b4a42be009e110c1` |
| `literal-full-ctest-accepted.log` | `c6450a8d9238daa8f94d7db4c0d48bfa79b0407e0d42222d19fae9083beba944` |
| `literal-full-ctest-release.log` | `28e117a3aceba528dd70150ef9c89832dbdf09d0b05a853fa774d7805c8fd0eb` |
| `literal-configure-final-verified.log` | `c9be1761cd25f0304868b995d9216989e8f2691bc5ec3205631bbf59addb790e` |
| `literal-build-final-verified.log` | `36f7e86df3e7ea99c6a7b3fda040fd7568f60064fccd394bb548debf048133c9` |
| `literal-full-ctest-final-verified.log` | `432a4312c469b14c1e9dbab364b80dbc6ca0c3b06157138eb716bdfa8b95cc0d` |
