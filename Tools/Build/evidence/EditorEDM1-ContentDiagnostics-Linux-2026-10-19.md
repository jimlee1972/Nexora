# ED-M1 bounded Content import diagnostics — Linux, 2026-10-19

Scope: initial asset-index failure text in the main graphical Content Browser. This supporting
slice does not accept ED-M1 or change the overall 0/8 graphical milestone status.

## Contract and compatibility

`ProjectContentSession::Open` carries owning `AssetEntry::error` into the appended
`ContentItem::import_diagnostic` field. Admission accepts empty text or at most 1024 valid UTF-8
bytes without NUL. Invalid project admission reports the relative asset path and preserves the
prior session. Content Reset/Discover reject invalid diagnostics before publication.

Single asset selection displays wrapped literal error text under Asset details, including in
read-only mode. Healthy/multiple selection and closed Content do not retain the previous
observation. Content rename/move/delete history owns its diagnostic copies. Successful Ready
artifact publication clears live and retained Undo diagnostics; failed/cancelled/stale publication
keeps the accepted item and uses existing operation diagnostics. Independent read-only observers
retain their own snapshots. Reindex/reopen reflects the repaired source.

Background retry now explicitly admits an initial failure without a prior artifact through
`ReimportJobRequest::initial_import`. Its default false preserves the ordinary nonempty-baseline
contract; the flag must agree with an empty baseline. ProjectContent opts in only for failed
initial items. Exact baseline/source/generation/settings/dependency publication checks remain.
The graphical regression exposed the previous queue rejection and proves repaired OBJ publication.

The public C++ ContentItem and ReimportJobRequest layouts change and consumers must rebuild. No C ABI, plugin ABI,
project/scene format, Runtime/Shipping dependency or serialized diagnostic storage changes.
Reverting this slice removes the appended fields and display; no migration is required.

## Automated acceptance

- `editor.content_diagnostics`: exact 1024-byte UTF-8 text, limit+1, embedded NUL, malformed and
  truncated UTF-8, atomic Reset/Discover rejection with generation/revision/selection conservation,
  owning rename/Undo, failed publication, successful repair and Undo conservation.
- `editor.content_diagnostic_display`: actual malformed OBJ indexing and synthetic WindowEvent
  pointer clicks through the real ImGui Content panel at 1x/2x DPI; failed/healthy/multiple
  selection, read-only rejection, failed/successful synchronous and queued reimport, ordinary
  missing-baseline/contradictory-recovery rejection, rename Undo, independent
  observer ownership and reindex/reopen. These are CPU graphical contracts, not physical pixels.

## Environment and commands

Debian 13 Linux x86_64, GCC 14.2.0. CMake 4.4.4, Ninja 1.13.2 and clang-format 23.1.3 were
installed in an external task directory. Debian X11 development headers/libraries and Vulkan
development files and Clang 19.1.7 were extracted into an external task sysroot, selected with
`CMAKE_PREFIX_PATH` and task-local PATH/loader settings. Development uses Modular linkage,
graphical shell ON and Slang OFF; the Shipping preset uses Minimal Monolithic with Editor OFF.
No machine-local presets, dependency files or build output are committed.

Final validation:

- Development configure/build: PASS with graphical shell ON.
- Focused diagnostic contracts: PASS 2/2, including 1x/2x pointer input and actual queued repair.
- Complete Development CTest: PASS, 284 registered, 272 passed, 12 skipped, zero failures,
  291.94 seconds. Hierarchy scale/lifecycle passed in 197.61 seconds.
- Shipping configure/build: PASS, Minimal Monolithic, Editor/testing excluded.
- Nine touched C++ files pass clang-format validation. Changed Markdown/bilingual pairing and
  `git diff --check` pass. Root README remains unchanged.

Exact commands (after sourcing the external task tool environment):

```bash
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --preset linux-development
cmake --build --preset linux-development --parallel 4
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping --parallel 2
git diff --check
```

CTest uses `CTEST_PARALLEL_LEVEL=4`. The first full attempt exposed the reset environment's
missing `clang++` in `build.v2_production_toolchain`; after installing real Clang, its unchanged
test passed and the complete gate was repeated. This environment repair changes no repository code.

Windows/macOS, physical displays, native GPU pixels, screen readers and the full ED-M1 target-host
workflow are unverified by this slice. Existing milestone acceptance remains open.
