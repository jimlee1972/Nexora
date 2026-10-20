# Content child-folder snapshot reuse — Linux, 2026-10-20

Scope: supporting WP2/ED-M1 and ED-M7 scale work, based on main `3b9fffcc` (PR656).
No ED milestone or physical-host acceptance is added; milestones remain 0/8.

## Contract and implementation

Previously each graphical Content frame called `ChildFolders`: scan every indexed item,
compute relative paths, deduplicate, sort and copy the complete folder list before clipping.
`ContentBrowserModel::ChildFolderSnapshot` now lazily retains one current-folder immutable
shared-owned vector. Warm calls return its owner without scanning items or copying paths.
`ChildFolders` preserves its independent owning-vector API by copying this projection.

Successful Reset (including same-count/same-generation replacement), Discover, Commit-backed
Rename/Move/Delete, Undo and actual folder changes invalidate the local projection. Filter,
selection, artifact publication, same-folder navigation and rejected admission preserve it.
Model copies may share the immutable current projection, but invalidate independently.
Retained snapshots remain valid after model assignment/destruction. No item, document, World,
filesystem iterator or mutable path borrow enters the cache. Queries remain serialized on the
authoring thread. This additive public C++ API/layout change requires rebuilding consumers;
module dependencies, stable C ABI, source/sidecar bytes and persisted formats are unchanged.

The graphical frame retains its snapshot owner. `AcceptAssetDrop` can publish a replacement
ProjectContentSession browser from inside the folder loop, so a non-owning cached span would
have been unsafe. The owning immutable version preserves all current-frame row paths across
that publication; next frame obtains the refreshed projection. Existing fixed-height clipping,
literal scoped IDs, folder/asset order and deferred folder navigation remain intact.

## Regression coverage

`editor.preview_contract` covers projection order and identity reuse, failed Reset/Discover/
Delete/SetFolder, same-count/same-generation Reset, Discover after Move then Undo, Delete/Undo,
Rename, folder and Reveal transitions, independent model copies, mutable owning-query copies,
assignment and destruction. It preserves existing artifact revision semantics.

`editor.content_dependency_reveal` adds real 1x/2x pointer drag/drop into a folder, checks actual
asset-path publication and retention of the previous immutable snapshot, then verifies Undo
restores source and sidecar bytes. Existing 2048-folder 1x/2x writable/read-only pointer/Tab/Enter,
scrolling, breadcrumb and filter/selection/history tests remain unchanged.

## Scoped query measurement

One process, 2048 indexed assets in 2048 immediate folders; 31 samples. Each sample copies an
unqueried model before timing one cold query, then times 1024 warm owner-return calls and checks
every owner matches the retained snapshot. Fixture/model-copy work is outside query timing.
A C++20 `-O2` probe links the actual Development Editor and Runtime libraries. Compiler is Debian
GCC 14.2; no native presentation or GPU is involved. Concurrent Development/Shipping compilation
was active; these observations are not calibrated production budgets or complete frame timings.

| Query | p50 (microseconds) | p95 (microseconds) |
| --- | ---: | ---: |
| Cold derivation/deduplication/sort |23755.8|48830.6|
| Warm immutable owner return |0.0168359|0.01854|

Probe source SHA256: `336dbf760d864a2ac3716ded4cc63ce67cc4b49a7c5d6cb461febdd174fb4b71`.

Warm calls observed 65,011,712 total folder entries (31×1024×2048). This comparison measures cold
versus retained query work, not whole-frame before/after performance. Cold derivation remains
O(n)/O(f log f); activation lookup, visible asset scans and other frame work remain. The current
projection retains owned labels/paths until invalidation or last external owner release;
process-memory, GPU, long soak and physical acceptance remain open.

## Validation

All commands use the repository Linux presets with the task-local CMake/tool sysroot.
Development is Modular with graphical Editor/2D enabled and Slang off. Shipping is Monolithic
with Editor/testing off. Native display/recording availability skips are reported honestly.

- `cmake --preset linux-development` — passed.
- `cmake --build --preset linux-development -j 4` — passed; all consumers rebuilt.
- `CTEST_PARALLEL_LEVEL=4 ctest --preset linux-development --output-on-failure` —
  285 registered, 273 actual passes, 12 existing native environment skips, zero failures,
  290.29 seconds. The unchanged serial lifecycle gate passed in 204.30 seconds; deep hierarchy
  73.15 seconds, Content keyboard shortcuts 26.27 seconds and dependency/reveal/drop 3.57 seconds.
  Both DPI clipping lines report maximum 55 submitted folders out of 2048.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping -j 2` — passed.
- Focused `ctest --test-dir build/linux-development -R
  '^editor\.(preview_contract|content_dependency_reveal)$' -V` — 2/2, zero skips, 2.63 seconds.
- `python3 Tools/Build/TestDocumentationCI.py` — 16/16. Changed Markdown local links, paired
  roadmap routing, full CI selection, touched-file clang-format and `git diff --check` passed.

All 61 roadmap files were searched for affected Content/folder/virtualization requirements;
only the paired Editor roadmap needs a new completion entry. The paired ImGui integration plan
keeps its shell-only scope. Root README remains unchanged; full frame, memory/GPU, physical
workflow and ED milestone acceptance remain open. Hosted validation must pass on the exact
published head before merging; the final PR body records its immutable workflow/job evidence.

Retained local log SHA256:

- `folder-cache-build.log`: `31de2f07e9ce10ce29c14c65c7e8f6da7fb9feeaa14719d7e2cdaf5909fb3dc3`
- `folder-cache-ctest.log`: `941b801be77b502c61cc35cd335c04a05305aa3083b93792052d5a1b91494da3`
- `folder-cache-last-test.log`: `d6298c040a696413078d2255e1bead0a0ba8c747453656e14364ee4c833c2c83`
- `folder-cache-shipping-build.log`: `7102d9e06cc616bc55d3f2fcb1871792611c415687007e5ded631272bf32badc`
- `folder-cache-focused-ctest.log`: `904843d5b3a7c2eeb15d7db51255b476b1de948686958a49cafa478bb394f7e8`
- `folder-cache-probe.log`: `f3d8d88c6a857f06cd52c4f4d0920fdaec3cb5051767e453fb245e4998b55e58`
