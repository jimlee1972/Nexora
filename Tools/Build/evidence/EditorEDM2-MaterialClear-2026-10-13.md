# ED-M2 supported material reference clearing

Date: 2026-10-13. Beads: `nexora-owg.1.10`. Supporting Inspector authoring slice; full ED-M2,
texture/shader authoring and physical 0/8 acceptance remain open.

## Behavior and ownership

Unassigned clears the complete selected Mesh Renderer set through bounded document metadata removal.
Requests own entity/document generation keys and project generation. ClearMaterialAssets rechecks
selection, live components, nonzero current generation, writable Content and supported exact 17-byte
schema-1 references on every target. Missing sources/catalogs are allowed; unsupported reserved
versions/names/types/oversized bytes reject the whole selection and disable the dropdown. Unrelated
opaque types, legacy mesh/shader values and source material files remain exact.

RemoveOpaqueComponents validates nonzero type and at most 4096 unique live keys, stages owning nodes,
previous payloads and the remaining complete opaque store before one metadata Undo publication.
Removal decreases active-store records/bytes; Undo history owns removed data for restoration. An
all-absent no-op preserves pending unrelated Redo. Save/reopen persists absence without changing
stable C/Zig/scene schemas, existing object layouts, dependencies or native shader inputs. The C++
methods are additive Editor exports; the generic metadata boundary grants no schema/access rights.

## Regression evidence

- Real document removal preserves every unrelated component byte and Runtime snapshot, supports
  one Undo/Redo and rejects empty/zero-type/duplicate/late-stale inputs. Absent removal preserves
  unrelated Runtime Redo.
- Actual 1x/2x Inspector dropdown input clears two known/missing UUIDs; single-selection input,
  one history transaction, unchanged mesh/shader/opaque data and save/reopen also pass. Real popup
  input remains usable without a catalog. Partial/duplicate/stale keys, project mismatch, focus and
  read-only cancellation preserve metadata/Redo; a later unsupported payload or absent Mesh Renderer
  prevents clearing the first valid target. Material source bytes remain identical.
- The initial UI assertion read the label on the frame before deferred publication was observed.
  Split guards proved removal/other-data conservation; observing the next widget frame corrected
  that test timing. Final targeted tests 2/2 passed in0.14s (catalog0.03s/Inspector0.10s); unsuccessful
  preliminary assertions are excluded from acceptance.

## Validation

The existing task runtime provides GCC 14.2, Slang 2026.18, clang-format 23, X11/Xvfb and software
Vulkan. Development enables graphical Editor/SDK/Slang with headless mode off; cached native runtime
paths remain machine-local and no preset/build output is committed.

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development`: passed (437 actions after targeted preparation).
- `ctest --preset linux-development --output-on-failure`: **248/248**, no failures/skips, **825.20s**.
  Actual document catalog/removal 0.03s, extended 1x/2x material widgets 0.09s, native Scene material
  pixels 0.69s and frozen native Game materials 3.12s; all existing graphical guards remain.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed (7 actions),
  Minimal Monolithic/native Vulkan with Editor disabled. Development above validates the new Editor
  exports; this smoke does not claim a shipping Editor application.
- Final targeted catalog/widget regression: 2/2, 0.14s. Failed preliminary label-timing assertions
  are excluded; deferred publication is observed on the next widget frame.
- Changed C++ `clang-format --dry-run --Werror`, `git diff --check`, changed Markdown relative links,
  synchronized EN/zh-TW roadmap/ADR and `python3 Tools/Build/TestDocumentationCI.py`: passed (16 tests).
  Root README remains concise/unchanged; full milestone acceptance remains 0/8.

Final-head hosted checks are recorded in the PR/Beads before merge. Pending jobs are never counted
as passed. Software Vulkan/Xvfb evidence does not imply physical multi-DPI/GPU acceptance.
