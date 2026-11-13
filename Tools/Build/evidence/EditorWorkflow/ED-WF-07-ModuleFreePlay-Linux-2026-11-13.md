# Main-scene Sprite Play without a gameplay module — Linux, 2026-11-13

Task `nexora-whr.7.6`; base main `e7684590` (PR #776). Dates use Asia/Taipei.
This is the main scene-clone consumer of saved source UUID references without a game library.
The initial implementation used that base; integration now includes merged Inspector PR #777
(main `ac15a642`). Both supporting slices are preserved and verified together below.
Workflow **5/9**, graphical Editor **0/8**, 2D **9/11** remain unchanged.

## Contract and boundary

`CapturePlayTwoD` now has an explicit `AuthoredSources` scope. It probes bounded owning reserved
metadata, deduplicates source UUID roots and freezes only their exact source/Atlas closure.
Unrelated unavailable 2D assets do not become roots; source-free capture stays asset-free without
an additional Runtime serialization or Content scan. The default module scope still
captures all project 2D assets. Existing writer, recovery, conflict, generation/revision, path,
content and source provenance checks remain in effect for actual source capture.

Immediately before binding, the graphical host rechecks the exact authoring capture and asset
provenance. `BindAuthoredPreview` requires supported references and a matching active isolated
Play scene, owns the existing private Sprite service and borrows that actual clone. `IsLoaded`
stays false; no library or module callback is involved and native audio remains unopened. Existing
reference validation, separate bounded capture limits and owner-thread serialization apply.
No second World, Runtime component, authoring mutation, ABI/codec or module dependency is added.

The existing Game 3D/2D selector starts in 3D each session. Selecting 2D submits actual Play poses
and immutable source geometry through the same clipped Game draw list and texture owner. Static
sources survive Pause/manual Step/Resume. Unload precedes World Stop, and the existing native
owner retires registrations between acquired frames. Frames retain their own immutable assets.
The real application explicitly reports module absence and the bound clone in test evidence;
that diagnostic adds no product control or alternate activation path.

This covers main scene clone Play when `project.build.json` is absent. Configured package Play,
main Build export, Scene View and animation/reload remain separate; Inspector UI is independently
delivered by PR #777. This is not complete ED-WF-07, physical-device acceptance, allocator/RSS/VRAM profiling,
GPU Sprite instancing or a production-scale performance claim. See the
[application contract](../../../../Apps/Editor/README.md#module-free-main-scene-sprite-play).

## Focused evidence

`editor.play_authored_sprites` now checks 32 module-free owners with no callbacks, exact Runtime
and authoring preservation, Pause/Step, fresh frame texture identities, explicit retirement and
frame lifetime after Unload/Stop. It rejects malformed/missing/foreign/duplicate bindings and
an Editor World. Two entities sharing a source prove root deduplication. A deliberately unavailable
unreferenced source fails complete module capture but permits reference-only capture; a source-free
scene still needs no preview owner. Retained previews also run after catalog/Content/index/project
owners and authoring files are destroyed. The existing module pre-Create and large-scene tests stay.

The actual `NexoraEditor` acceptance fixture saves a wrapped main scene with two source references,
two real imported Atlases, a camera and a mesh. It contains no game library or package config.
Native keycode and mouse controls select 2D, Pause, Step, Resume and Stop over 32 sessions.
The strict red/blue source masks match exactly on every restart, each with **1296 pixels** in the
focused run. Concurrent authoring Scene samples and every source/scene file remain exact.
Stop clears both native Atlas registrations and the bound preview. Vulkan validation errors fail.
The test retains unique attempt directories and full 1280x720 paused/stepped/stopped images.

```bash
cmake --build --preset linux-development --target \
  NexoraPlayAuthoredSpriteTests NexoraGameTwoDPreviewTests NexoraEditor
ctest --preset linux-development \
  -R '^(editor.play_authored_sprites|editor.game_twod_preview|editor.linux_authored_preview)$' \
  --output-on-failure
```

✅ Focused **3/3** pass: authored CPU case 2.34 seconds, existing Game preview case 1.98 seconds,
actual native module-free case 55.91 seconds. The native fixture reports 32 sessions and zero
validation errors. This focused result is retained separately from the required complete gate.

## Retained initial fixture failures

- The first build rejected a missing `ContentItem::artifact_hash` initializer under existing
  `-Werror`. Its test initializer was completed; no warning policy changed.
- The initial CPU preview check assumed an empty retirement queue after an earlier module case.
  Unload correctly queued that case's texture. The fixture now verifies and collects that old
  texture explicitly before checking each new preview's retirement; no production code changed.
- The first native fixture authored one-world-unit squares. The real Game rectangle was 604x180
  and its default camera spans 20 vertical units, so native readback contained exactly 81 red and
  81 blue pixels, below the new test's unchanged >200 predicate. The full failure screenshot is
  retained. The fixture now authors four-world-unit squares at X=-2.5/+2.5 through its source
  pixels-per-unit and Runtime transforms, yielding 36x36 source pixels. Production camera,
  rendering, strict channel predicates, test deadlines and assertions were not relaxed.

Raw compiler/test logs and the first complete native failure capture remain in task-specific
external evidence. Successful later runs do not reclassify these fixture errors as passing runs.

## Required complete verification

```bash
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development
```

✅ Pre-Inspector-integration Development configure/build and **492/492** tests pass, zero failures/skips,
**1405.68 seconds**. The final authored CPU case passes in 1.99 seconds and actual native
module-free case in 54.09 seconds. Its 32-session report retains 1296 strict red and 1296 blue
pixels, exact restart geometry and zero validation errors. The inventory query ran before CTest;
both the full named status log and detailed LastTest log are retained with hashes.

```bash
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_EDITOR_SDK=ON
cmake --build --preset linux-shipping
cmake --preset linux-development -B build/editor-no-tests -DBUILD_TESTING=OFF \
  -DNEXORA_ENABLE_2D=OFF -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --build build/editor-no-tests --target NexoraEditor
cmake --preset linux-development -B /workspace/work/editor-completion/sprite-reference-monolithic \
  -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_EDITOR=ON -DNEXORA_ENABLE_2D=ON \
  -DNEXORA_ENABLE_WINDOW_PRESENTATION=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build /workspace/work/editor-completion/sprite-reference-monolithic \
  --target NexoraPlayAuthoredSpriteTests
ctest --test-dir /workspace/work/editor-completion/sprite-reference-monolithic \
  -R '^editor.play_authored_sprites$' --output-on-failure
python Tools/Build/TestDocumentationCI.py
```

✅ Shipping configures/builds; it strips standalone Editor/EditorCore by contract, so that result
does not execute this application helper. The actual graphical Editor compiles/links with 2D
and testing OFF. The Monolithic actual consumer compiles/links and passes **1/1** in 1.96 seconds
without graphical Editor, including its callback-free preview/rejection/lifetime cases.
Documentation utilities pass **16/16** and nine changed Markdown/language-pair documents pass
link validation. An initial documentation command omitted the established build.env Python path;
its missing markdown_it import is retained separately. Sourcing that existing environment fixes
the invocation without a dependency install or parser/document change.

The [machine receipt](ED-WF-07-ModuleFreePlay-Cloud.json) retains every configured named outcome,
source/log/image hashes and the separate initial failure generations. Full native
[paused](ModuleFreePlay-paused.png), [stepped](ModuleFreePlay-stepped.png) and
[stopped](ModuleFreePlay-stopped.png) images are actual 1280x720 window captures without compositing.
The exact-mask comparison covers the source geometry; no whole-window identity is claimed.
All compilation finishes before graphical acceptance. The repository-root README stays unchanged.
Hosted checks and merge status are tracked against the delivered head in the PR and Beads;
passing local gates alone do not permit merging before all applicable hosted checks succeed.

## Inspector integration verification

PR #777 was merged independently while PR #778 checks were running. The current branch integrates
main `ac15a642`; all six conflicts were pending-work wording in documentation. Both delivered
sections remain, and source assignment plus module-free preview are no longer listed as pending.
The earlier complete 492-case generation and its immutable receipt at head `884f52e7` are retained
separately. Fresh combined focused/full and optional-config verification is pending; older CI
results do not satisfy the updated-head merge gate.
