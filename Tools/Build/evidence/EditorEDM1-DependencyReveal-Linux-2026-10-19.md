# ED-M1 dependency Reveal: Linux evidence

Date: 2026-10-19. Base: `0f81e2899c6bd00e853f00742bb702b5422cc38e` (PR #650).
Beads: `nexora-xke.3`. This delivers a supporting Content workflow; physical-display acceptance
and ED-M0–ED-M7 milestone acceptance remain open (**0/8**).

## Delivered behavior and contracts

Previously, forward and reverse dependency entries were inert text. Asset details now show a
UUID-scoped Reveal button for each current indexed target. Pointer or keyboard activation reveals
its parent folder, clears query/type filters and selects only that UUID. The following frame includes
the target row in the real clipper and scrolls it into view, including a target beyond 128 other rows.
This is inspection: it does not open a scene, reimport, write source/sidecar files or create Undo.
Unknown references retain an unavailable UUID with a disabled button. Dependency labels use literal
UTF-8 text, so Unicode, percent signs and ImGui marker characters do not become format or ID syntax.

`ContentBrowserModel::RevealAsset` stages breadcrumbs/folder/selection before replacing view state;
missing IDs or paths with no revealable relative parent preserve the previous view. Item snapshots,
content revision, dependency state and pending mutation Undo remain unchanged. The additive C++
method requires rebuilding consumers; no persisted schema, stable C/Zig contract or module edge changes.

The UI consumes its current-frame UUID after borrowed row/details rendering. Only scrolling is
deferred; its owning scope carries root, generation, revision and folder and checks current sole
selection before use. Changed scope/input ownership, a hidden/collapsed Content panel or closed
Content discards it. It retains no content/document pointer. Read-only navigation uses the existing
selection/input gates. Enter activation owned by another widget cannot also trigger the selected
scene's global Open route; ordinary selected-scene Enter remains supported.

## Meaningful automated coverage

The existing `editor.preview_contract` now covers valid cross-folder reveal, both filter resets,
missing and parentless targets, unchanged revision and retention of a prior mutation Undo.

The new `editor.content_dependency_reveal` uses actual WindowEvent pointer/key batches and the
production ImGui controls at 1x/2x framebuffer DPI with a fixed 1600x1200 logical display. The test
undocks the real Content panel to a fixed 1000x900 logical area for reproducible widget visibility.
It verifies:

- Forward and reverse Reveal, exact owning UUID and folder, and a far target inside the actual
  ImGui clip rectangle after scrolling, with all 129 nested rows available.
- Same-basename assets in different folders, with Unicode/`##`/`%` names, plus a missing UUID.
- Read-only keyboard activation with both query/type exclusion and no new history.
- No accidental Open request when Reveal owns Enter; a normal asset-row Enter still requests Open.
- Writer-to-observer generation replacement, focus loss, a blocking external-conflict modal and
  cancellation of deferred scrolling when Content closes.
- Rename/source-and-sidecar Undo survives inspection. A byte-for-byte snapshot of every Content
  source and identity sidecar matches after all navigation, input, conflict and Undo operations.

These are portable ImGui/input contracts, not physical display, glyph-legibility or screen-reader
acceptance. Native target-host workflow acceptance remains open.

## Validation and original failure retained

Host: Linux 6.18.55 x86_64, GCC 14.2, CMake 4.4.4, Ninja 1.13.2, clang-format 23.1.3.
Development: Modular, graphical Editor and 2D ON, Slang OFF, OpenSSL 3.5.7. External task tools
provide X11/Vulkan development files and Clang 19; no Xvfb/xdotool or physical GPU acceptance ran.
Shipping: the existing Minimal Monolithic preset, Editor/testing OFF and 2D ON. This validates
that preset's linkage; it is not a full Shipping Editor acceptance run.

```bash
source /workspace/work/editor-roadmap/tools.env
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
python3 Tools/Build/TestDocumentationCI.py
clang-format --dry-run --Werror \
  Engine/Editor/include/Nexora/Editor/ContentBrowser.h Engine/Editor/src/ContentBrowser.cpp \
  Engine/EditorImGui/src/EditorImGui.cpp Engine/EditorImGui/src/EditorImGuiTestAccess.h \
  Tests/Editor/EditorTests.cpp Tests/EditorImGui/ContentDependencyTests.cpp
git diff --check
```

The targeted model/UI tests passed, then full Development configure/build and Minimal Shipping
configure/build passed. Two full CTest runs used identical source/settings and default hierarchy
scale **1**, with unchanged assertions, samples and deadlines:

| Full run | Registered | Passed | Skipped | Failed | Wall time |
| --- | ---: | ---: | ---: | ---: | ---: |
| Initial | 285 | 272 | 12 | 1 (hierarchy lifecycle timeout) | 431.81s |
| Isolated repeat | 285 | 273 | 12 | 0 | 349.16s |

The initial lifecycle test timed out at **300.02s** after all eight sample loops, with seven JSON
records and the final source-history phase still active. Its unchanged first source-generation
phase took **15036.1ms**; subsequent cases took **269.130–333.024ms**. The initial full run overlapped
the tail of Minimal Shipping linking. This correlation does not establish contention or cold-start
cause. Deep correctness passed **131.76s** and the new Reveal UI passed **0.48s** in that run.

The isolated full repeat started after all builds finished. Lifecycle passed **245.02s**, deep
correctness **75.41s**, and Reveal UI **0.55s**. Its first source generation was **441.567ms**; all
source-generation phases lay in **240.623–441.567ms**. No source, option or deadline was changed
between runs. This successful repeat is not a production performance fix. Follow-up
`nexora-0ua.1.16` remains open; the existing Windows timing investigation also remains open.
The [phase diagnostic contract](EditorEDM7-HierarchyPhases-Linux-2026-10-19.md) supplies interpretation.

Both runs' twelve skips are unavailable native Vulkan tests; none is counted as acceptance. The
initial complete logs are retained outside the checkout as `reveal-ctest-attempt1.log` and
`reveal-lasttest-attempt1.log`. Full CTest console-log SHA-256 values:

- Initial: `6c2511dd1fe6a05f4479b46d456a4e69244e9d34ef42655b8a823fbbbe16888e`.
- Isolated repeat: `dd00db0bf3580ccd075a31f19001c7147f5a8c1c2af53ab34f849a6600bd85cd`.

Documentation tests passed **16/16**; changed Markdown links/bilingual pairs, touched-C++ formatting
and diff checks passed. All roadmaps were reviewed for affected delivered behavior; only the paired
Editor supporting entry changes. The root README remains concise and unchanged.

Windows/macOS did not run locally. Hosted source-head CI results and retained job links are recorded
in the PR and Beads before merge; physical workflows, thumbnails and full milestone acceptance
remain open.

## Source SHA-256

- `Engine/Editor/src/ContentBrowser.cpp`: `8ef69ad959a3eca5078a73ce8fec091e09ab8bf34f9c05a37ec75ebd388ecc2f`.
- `Engine/EditorImGui/src/EditorImGui.cpp`: `3334313e20c39af02c69abb5fc20f936a9aba43715794f4c338404af9937f3eb`.
- `Tests/EditorImGui/ContentDependencyTests.cpp`: `32dbd5a7bb4d90aa4cd31536a4b72a93f6688581deb5d5fe1ccb14317f222c55`.
