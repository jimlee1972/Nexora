# Configured packaged Sprite Play — Linux, 2026-11-14

Task `nexora-whr.7.8`; base main `1d8eabb0c388c9ee6634b5681b505c35e2172c36`.
Dates use Asia/Taipei. Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** remain unchanged.

## Ownership and scope

See the [application contract](../../../../Apps/Editor/README.md#configured-packaged-sprite-play),
[ImGui ownership](../../../../Engine/EditorImGui/README.md#owning-game-scene-images-and-source-quads)
and [native primitive](../../../../Engine/Presentation/SceneUiComposition.md).
The actual configured package owner admits typed immutable Sprite/Atlas closure, all GUI native
registrations and the complete current-frame Scene/Sprite/GUI submission before candidate World
publication or initial module callbacks. Source files/sidecars are rechecked before Play borrowing.
The source-free direct path and optional TwoD-OFF refusal remain. No C/Zig wire, codec or module
boundary changes. Empty/hidden canvases complete texture admission without a fake visible image.
This is static source composition, not source animation/live reimport/reload, new cameras,
depth-aware Sprites, GPU instancing or a physical-device/RSS/VRAM/performance claim.
Native Linux checks use Xvfb and Vulkan lavapipe for functional virtual-display acceptance.

## Focused functional evidence

Initial CPU focus passes the existing actual-SDK packaged Play and indexed2D UI tests plus the
new 32-owner source test. It covers unpublished World conservation, typed malformed payload,
native callback rejection with retained old World, Additive/Replace remapping and registration
capacity cleanup. Review added exact immutable Atlas-owner rejection for a foreign same-UUID frame
and final source-baseline revalidation; the final source is exercised by the gates below.

The initial corrected native fixture passes in 55.60 seconds:32 Play/Pause/Step/Resume/Stop sessions,
162 native pixels for each of four source colors, exact full-canvas trim/Pivot/mirror masks and 36
exact interior3D pixels against an independent source-free Editor. A real SDK Zig module drives
2/4/2/2 visible sources over four commits (Additive/Replace/return). Normal Ctrl+3 panel focus and
Inspector collapse verify occlusion/reveal;1200x680 then1280x720 verify fresh Scene references.
All read-only project hashes and concurrent authoring pixels stay exact; Vulkan/texture errors fail.
This initial generation is retained separately from final-source complete validation.

## Retained initial failures

- An intermediate diagnostic refactor omitted a return and its visible-count assignment; unchanged
  compiler warnings rejected it. Both were completed before subsequent execution.
- The first CPU fixture passed Editor uint8 metadata directly into Runtime byte records; explicit
  byte-view copying corrected the fixture under unchanged types/warning rules.
- The first native fixture sent F5 immediately after finding a window. It now waits for copied
  processed focus/shortcut admission and the real native Scene marker before that one key.
- The first pixel reader was the older center-sampling Scene helper, while the new oracle covers
  the full 604x180 canvas. The raw failure image independently contains 162 pixels of each color.
  A complete-canvas reader fixes the test; strict channel predicates and geometry remain intact.
- Lifecycle messages were first sought in stderr; the Editor writes them to its ordinary Core
  log. The fixture retains that log through the existing NEXORA_LOG_DIR override and checks the
  same Start/three-applied/Stop messages there.
- The first occlusion probe clicked an Inspector still behind Game (Play focuses Game). The normal
  Ctrl+3 command brings it forward before the same occlusion/reveal predicate. No product input
  behavior, assertion, warning policy or timeout was relaxed. All failed logs/receipts and existing
  full native failure captures remain retained.

## Default-layout native rejection and correction

The first final-source build completes 623 targets, but focused verification is **7/8 pass,
1 failure**,248.66 seconds. The real existing Build/Player test's default-layout Editor exits after
5 acquisitions/4 presents. The Game window's first sizing-only hidden frame supplied a native
viewport/draw-list range even though ImGui excluded that window from submitted draw data. The
new owning image-range validation correctly rejected the orphaned Scene image.

`NativeGameViewport` now excludes hidden first-use sizing windows. Such frames use ordinary GUI
and complete Atlas upload admission without publishing a fake on-screen Scene reference. The
renderer reference/range checks remain intact. The same default-layout probe now passes 39.14
seconds and retains12 actual acquisitions/presents, running Play and exact source hashes. The
complete earlier source hashes/patch and all failed logs remain separate. The unfiltered gate had
not started in that failed generation. Fresh configure/build/focused/full gates run after this
production visibility correction; its native default-layout proof is not substituted with a
persisted layout or a relaxed assertion.

## Reviewed GUI allocation-exception path

After the visible-viewport correction, all eight focused cases pass 250.47 seconds. The following
unfiltered gate is intentionally stopped after **25 completed passes, zero completed failures,
exit 130**: review found the GUI submission status was assigned only after `EndFrame`/`Render`.
An allocation exception after closing the GUI could therefore leave the replay guard unset.
The host now marks a conservative failed attempt before either fallible operation; candidate
recovery never ends/submits that GUI again. CPU source candidates additionally inject a callback
allocation exception and verify old World/palette conservation. The original source hashes,
successful eight-case focus and partial full logs are retained separately. Final verification
repeats the three affected source cases and the complete unfiltered gate; the interrupted run
is not counted as complete acceptance.

## Actual TwoD-OFF declaration failure

The first complete Linux gate passes **504/504**, zero failures/skips,1552.04 seconds. The following
actual graphical TwoD-OFF/testing-OFF Editor build then rejects `Presentation::ToString`: this
application header used its declaration without directly including `RenderSurface.h`; an enabled
TwoD dependency supplied it transitively. The defining header is now included directly. No runtime
logic, diagnostics or feature guard is changed. The successful full gate and failed OFF logs/source
hashes are retained separately. Final configure/build/focused/full and optional gates run after this
header-only correction; the earlier complete gate is not substituted for the final source.

## Complete and optional verification

The final source passes the complete Linux Development Modular configure/all-build and unfiltered
**504/504** configured tests, **zero failures/skips**, **1542.80 seconds**.
The three affected source cases also pass before that full gate. All 11 frozen source/registration
hashes remain exact. The inventory is captured before CTest; the detailed log is copied only after
its successful exit. The earlier complete504 pass, interrupted25-pass run and original failures
are separate evidence generations, not substituted results.

The final native case checks 32 actual Play lifecycles,162 pixels of each source color,36 exact
underlying 3D pixels,2/4/2/2 visible sources across actual SDK scene commits, floating Inspector
occlusion/reveal, resize and stale-source refusal before callbacks. Authoring pixels and read-only
project hashes remain unchanged. Four unedited 1280x720 native captures are retained:
[paused](ConfiguredSpritePlay-paused.png), [Additive](ConfiguredSpritePlay-additive.png),
[occluded](ConfiguredSpritePlay-occluded.png) and [restored](ConfiguredSpritePlay-restored.png).
The [machine receipt](ED-WF-07-ConfiguredSpritePlay-Cloud.json) includes every configured test,
source/log/image hashes, actual native observations and original failure classifications.

The final optional runner exits 0, serially, before the final required gate:

- Shipping Full/Monolithic configure/build passes. This is linkage validation; Shipping strips
  the standalone Editor and does not execute the new Development application path.
- Actual graphical Editor and native ProjectPlayer build with TwoD OFF and BUILD_TESTING OFF.
- A fresh graphical TwoD-ON Monolithic Editor build passes three related CPU tests in 1.79 seconds
  and the complete same 32-session native fixture, including exact source/3D masks, SDK scene
  changes, occlusion/resize and stale-source refusal. The canonical C ABI fixture module comes
  from the validated Development SDK build; this does not claim a new Monolithic Zig SDK build.

Exact commands (the established cloud build environment is sourced first):

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development -R '^(editor.packaged_sprite_play|editor.linux_packaged_sprites|editor.linux_game_sprite_build)$' --output-on-failure
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_EDITOR_SDK=ON
cmake --build --preset linux-shipping
cmake --preset linux-development -B build/editor-no-tests -DBUILD_TESTING=OFF -DNEXORA_ENABLE_2D=OFF -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build build/editor-no-tests --target NexoraEditor NexoraProjectPlayer
cmake --preset linux-development -B /workspace/work/editor-completion/packaged-preview-monolithic -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_EDITOR=ON -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_WINDOW_PRESENTATION=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build /workspace/work/editor-completion/packaged-preview-monolithic --target NexoraEditor NexoraPackagedSpritePlayTests NexoraGameSpriteBuildTests NexoraGameTwoDPreviewTests
ctest --test-dir /workspace/work/editor-completion/packaged-preview-monolithic -R '^(editor.packaged_sprite_play|editor.game_sprite_build|editor.game_twod_preview)$' --output-on-failure
python Tests/EditorImGui/LinuxPackagedSprites.py --editor /workspace/work/editor-completion/packaged-preview-monolithic/Apps/Editor/NexoraEditor --fixture /workspace/work/editor-completion/packaged-preview-monolithic/Tests/Editor/NexoraGameSpriteBuildTests --module /workspace/Nexora/build/linux-development/Tests/EditorImGui/packaged-sprite-script/lib/libPackagedSpriteFixture.so --xvfb /workspace/work/editor-completion/sysroot/usr/bin/Xvfb --xdotool /workspace/work/editor-completion/sysroot/usr/bin/xdotool --evidence-dir /workspace/work/editor-completion/packaged-preview-monolithic-native
python Tools/Build/TestDocumentationCI.py
```

Documentation utilities pass16/16. Changed Markdown/local links and bilingual roadmap pairs
pass12 documents with zero errors. Touched C++ formatting and final diff checks pass.
Hosted checks must pass on the exact delivery head before merging; their final results belong
in the PR and Beads. Linux cloud results do not claim local Windows/macOS/Android/iOS execution.
The repository-root README and parent milestone acceptance/counts stay unchanged.
