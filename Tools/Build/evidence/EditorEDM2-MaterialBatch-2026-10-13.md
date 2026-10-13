# ED-M2 atomic Inspector material multi-selection

Date: 2026-10-13. Beads: `nexora-owg.1.9`. Supporting authoring slice; full ED-M2 and physical
multi-DPI acceptance remain open. The Linux cloud's native runtime uses Xvfb/software Vulkan;
Windows/macOS claims require actual hosted logs separately.

## Behavior and compatibility

The existing single-object material dropdown now accepts the complete selected Mesh Renderer set.
Mixed UUID/presence is visible as Mixed; any absent Mesh Renderer disables assignment. Requests own
entity/document keys, asset UUID and project generation. The authoring owner checks the complete
current selection, writable Content, live generation and exact current catalog/source payload
identity. Every existing reserved material reference must have a supported exact 17-byte payload;
wrong reserved names/types/versions or oversized payloads reject the entire request without
overwriting bytes. Initially absent references can be added; unrelated plugin payloads are preserved.

`SceneDocument::SetOpaqueComponents` stages add/replace for one shared payload across at most 4096
unique live keys. The complete prospective store must obey its existing 64/entity, 4096-record,
1 MiB/component and 16 MiB bytes-plus-names limits. Staged nodes and previous payloads publish once
through the existing metadata batch history. Late key/capacity rejection and equal-value no-ops
preserve pending Redo. Runtime mesh/shader values, source material files and stable C/Zig/scene
schemas and existing object layouts are unchanged. The C++ methods are additive exports through
the existing Editor boundary; no dependencies or native shader interfaces change. Allocator/driver
OOM injection is not claimed.

## Regression evidence

- `editor.material_catalog`: actual document metadata add/replace, one Undo/Redo, duplicate/empty/
  stale keys, late 64-component capacity rejection, exact previous payload/Runtime conservation and
  unrelated Runtime Redo preservation.
- `editor.material_inspector`: actual imported material dropdown clicks at 1x/2x for two targets,
  mixed/equal/unassigned labels, initial add plus existing reference preservation, one Undo/Redo,
  save/reopen and full-width mesh/shader values. Deferred partial/duplicate/stale-key/project/
  changed-selection/focus requests reject and retain Redo. A later unsupported payload or missing
  Mesh Renderer rejects without assigning the first target. Existing single-selection read-only,
  modal/hidden/focus, reimport/stale catalog, deletion and unknown-version checks remain.
- Existing native Scene/Game material fixtures and complete graphical suite remain required.

## Validation

The task environment provides GCC 14.2, Slang 2026.18, clang-format 23, X11/Xvfb and software Vulkan.
Development enables the graphical Editor, Editor SDK and Slang with headless mode off; cached native
library paths point to the existing cloud runtime. No machine-specific preset or build output is
committed.

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development`: passed (542 actions after the initial targeted build).
- `ctest --preset linux-development --output-on-failure`: **248/248**, no failures/skips, **840.33s**.
  Actual document catalog/batch tests 0.03s; 1x/2x material widgets 0.05s; native Scene material
  pixels 0.71s and frozen native Game materials 3.13s. All original graphical guards remain.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed (74 initial
  build actions, plus 5 BuildInfo/link actions after advancing to the identical merged base),
  Minimal Monolithic/native Vulkan with Editor disabled. Development above exercises
  the new Editor exports; this Shipping smoke does not claim a shipping Editor application.
- Initial targeted catalog/widget tests: 2/2, 0.11s; the final gate additionally includes the later
  explicit missing-MeshRenderer batch rejection.
- Changed C++ `clang-format --dry-run --Werror`, `git diff --check`, changed Markdown relative links,
  synchronized EN/zh-TW roadmap/ADR and `python3 Tools/Build/TestDocumentationCI.py`: passed (16 tests).
  Root README remains concise and unchanged; milestone acceptance remains 0/8.

Hosted Editor/native results are reviewed on the final PR head and recorded in the PR/Beads before
merge. Pending jobs are never counted as passed.
