# Main Editor Sprite source reference model — Linux, 2026-11-13

Task: `nexora-whr.7.3`, an independent prerequisite of ED-WF-07. Base: main `6cfd05eb`.
Dates use Asia/Taipei. This is a CPU authoring-model proof; it does not claim a new graphical
workflow or new native pixels. The separately claimed ND2D/Studio work in `nexora-whr.7.2` / PR #773
is outside this change. Workflow **5/9**, graphical Editor **0/8**, 2D **9/11** remain unchanged.

## Delivered behavior

- `SpriteSourceCatalog` retains immutable imported source/Atlas owners. Publication checks typed
  UUIDs and complete source-to-region/trim closure before swapping the candidate; rejected candidates
  retain the last-good version and owners. Each Atlas region index is built once, avoiding repeated
  full Atlas validation per source. Existing Content import enforces the aggregate 4096-asset/64 MiB
  retained-payload limits; the catalog also bounds its participating Atlas/source set.
- Exact project generation plus Content revision protects pending operations against source and
  Atlas reimport in the same project. Selection-only Content changes do not alter this revision.
- The existing scene wrapper preserves `editor.sprite.source`, TypeId `0x4544535052540001`, as a
  17-byte version-1 UUID reference. It owns no source filesystem path or runtime resource.
- Complete-selection assignment and clear validate live keys, writable Content, caller authority
  and reserved-component compatibility before one existing atomic metadata Undo/Redo transaction.
  Unrelated plugin data and Runtime scene bytes are preserved. Missing assets can be cleared;
  unsupported reserved payloads cannot be overwritten. Equal/no-op requests preserve pending Redo.

## Test coverage

`editor.sprite_source_catalog` uses real persistent ProjectWorkspace/AssetWorkspace/Content import
and source/Atlas reimport. It verifies invalid generation, absent dependency, missing region,
trim mismatch, typed identity/duplicate/count admission, 1024 sources sharing an Atlas, retained
owners after project/catalog teardown, initial/mixed multi-selection assignment, replacement,
one-step Undo/Redo, wrapped file save/reopen and stale NodeKeys. It rejects partial selections,
stale entity/project/content versions, read-only/disabled authoring and Atlas IDs used as sources.
Future versions, wrong reserved type/name, zero IDs, truncated/trailing data and oversized valid
prefixes stay protected. Source deletion does not prevent explicit clear and restoration by Undo.

The full preset also retains existing 2D/runtime/API/native source acceptance tests. This model
test adds no fixture-specific native skip, assertion relaxation, deadline increase or production
test hook. New source remains conditional on `NEXORA_ENABLE_2D`.

## Validation

Linux x86_64, GCC 14.2.0, CMake 3.31.6, Ninja 1.13.2. The cached Development configuration has
2D, graphical Editor, Zig and Slang enabled. Builds completed before the two-worker full test run;
no compilation overlapped graphical acceptance. The [machine-readable receipt](ED-WF-07-SourceReferences-Cloud.json)
records all 487 named outcomes, source hashes and raw-log hashes.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development
```

✅ Configure/build and **487/487 tests pass**, zero failures/skips, **1324.18 seconds**. The new
`editor.sprite_source_catalog` passes in 0.07 seconds. Existing composed-native source, Player,
repack, layout restart, scene/prefab, API, SDK and lifetime cases remain included.

```bash
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_EDITOR_SDK=ON
cmake --build --preset linux-shipping
```

✅ Shipping configure/build pass. Shipping deliberately strips standalone Editor and EditorCore,
including with this Full/SDK configuration; it does **not** execute the new Editor API. No new
Shipping native-pixel claim is made. A separate Development Monolithic consumer below validates
the new API's static linkage and behavior.

```bash
cmake --preset linux-development -B /workspace/work/editor-completion/repack-feature-off \
  -DBUILD_TESTING=OFF -DNEXORA_ENABLE_2D=OFF \
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build /workspace/work/editor-completion/repack-feature-off --target NexoraEditorCore

cmake --preset linux-development -B /workspace/work/editor-completion/sprite-reference-monolithic \
  -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_2D=ON \
  -DNEXORA_ENABLE_WINDOW_PRESENTATION=ON \
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build /workspace/work/editor-completion/sprite-reference-monolithic \
  --target NexoraSpriteSourceCatalogTests
ctest --test-dir /workspace/work/editor-completion/sprite-reference-monolithic \
  -R '^editor.sprite_source_catalog$' --output-on-failure
```

✅ 2D-OFF EditorCore configures/builds. Fresh Monolithic EditorCore and its actual reference-model
consumer compile/link; **1/1 passes**, zero failures/skips. No full Monolithic regression is claimed.
`python Tools/Build/TestDocumentationCI.py` passes **16/16**; eight changed Markdown files pass
local-link and language-pair checks. Touched C++ clang-format and `git diff --check` pass. Root
README is unchanged. Hosted CI/merge is tracked by the PR and Beads; local results alone do not
authorize merging before the applicable CI succeeds.

Final review moved the CPU test registration outside the neighboring material test's window
condition, leaving its own 2D condition. The effective full configuration is identical: all 487
test names, commands and properties compare equal before/after, and final Development configure,
full build and the focused test pass. No runtime/test C++ changed after the full run. An additional
attempt with 2D ON and Window Presentation OFF was explicitly rejected by the existing root
dependency check (`NEXORA_ENABLE_2D requires Asset Pipeline and Window Presentation`). That unsupported
combination is not a passing gate; the supported Monolithic configuration retains Window
Presentation ON and graphical shell OFF. Module dependency policy was not changed.

## Initial failures retained

The first configure ran before the new test source had been written and rejected the missing
`SpriteSourceCatalogTests.cpp`; the complete source was subsequently built. The first test assumed
a default browser generation of zero; the fixture now explicitly resets generation to zero.
The next test assumed the final Atlas padding pixel was opaque; it now compares the complete
retained Atlas encoding with the expected valid Atlas. These fixture corrections did not change
production validation, ownership, transaction behavior or test thresholds.

## Remaining scope

Inspector widgets and user selection flows, source-to-runtime Sprite/Scene View/Play conversion,
main Editor Build export, source animation/reload and complete ED-WF-07 acceptance remain open.
The reference helper performs no filesystem provenance revalidation. Hosts must supply current
writer/Play/recovery authorization and explicitly capture/freeze assets before runtime use.
Windows/macOS/mobile and physical-GPU validation are not claimed from this Linux environment.
