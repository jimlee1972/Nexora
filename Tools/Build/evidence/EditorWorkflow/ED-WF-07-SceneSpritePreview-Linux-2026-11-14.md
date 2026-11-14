# Native Scene Sprite preview — Linux, 2026-11-14

Task `nexora-whr.7.9`; initial base main `cc9a4bae4a026b7b2f5e95b97615d6bdb19c8a39`.
Before final verification the feature branch fast-forwards to main `e4b02c88267ad09a5ff37349d87c1e0814817ec7`
(PR #784); both bilingual roadmap additions are preserved. The earlier old-base full gate is
interrupted in its first hierarchy lifecycle case, with zero completed tests, and is not acceptance.
Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** remain unchanged.

## Delivered boundary

The native 3D Scene View uses owning imported source/Atlas snapshots, exact trim/Pivot/PPU
geometry and hierarchical affine matrices, source/mesh union bounds and cutout-aware picking.
The existing linear unlit PBR sampler/transfer and alpha cutoff0.5 apply; partial blending,
pixel-perfect nearest2D, a new XY authoring camera and production animation/reload remain separate.
See [application contract](../../../../Apps/Editor/README.md#native-scene-sprite-sources),
[owning images](../../../../Engine/EditorImGui/README.md#owning-authoring-scene-images),
[typed Scene composition](../../../../Engine/Presentation/SceneUiComposition.md) and
[bounded concurrent views](../../../../Roadmap/en/ADR-0009-Editor-Concurrent-Canvases.md).

The surface supports two independently fenced LDR sampled images before one GUI packet, preserving
original single-sampled/direct behavior. Existing Scene/Game overlap suppression remains. Explicit
between-frame Scene texture retirement frees both native color-space views/descriptors after a GPU
drain without touching independent UI textures or other Scene owners. No shader, stable C/Zig,
asset codec or module graph changes; public C++ consumers rebuild.

## Retained failures and correction

- The initial retirement fixture omitted the explicit empty UiDrawData upload initializer; the
  unchanged warning policy rejected it. The CPU fixture initially combined differently typed auto
  declarations. Both compile failures and a resulting not-run CTest invocation remain retained.
- First main integration passed module-free authored preview but genuinely broke configured Play:
  the lower surface admitted only one sampled Scene reference. Its source generation, log and raw
  failure image remain retained outside the repository. The bounded two-view primitive now keeps
  both references valid, transitions/binds each sampled target and gives DX12/Metal a separate
  fenced secondary LDR color target. The original twelve single-sampled/direct frames remain;
  twelve additional dual frames use the same orientation/color/occlusion oracle. No admission,
  warning, pixel assertion or timeout was weakened.
- The new native Scene fixture initially had an unbound Python nonlocal declaration. Its first
  pixel attempt read the pre-caption canvas geometry; it now consumes the actual settled presented
  rectangle. Its expanded writable phase initially compared against geometry before the ordinary
  Save-status row appeared. Establishing Save before exact pixel baselines preserves the same
  byte/pixel identity assertions. A subsequent final Undo/Save chord raced the per-frame command
  consumer; ordinary key delivery now allows a processed frame between successive commands. The
  first reimport loop compared X11 pixels immediately after EndFrame queued presentation and saw
  the preceding generation. The same bounded wait now requires the independent physical-window
  color oracle to pass after native admission; channel predicates/timeouts remain unchanged.
  All original attempts and captures remain separate.

## Full-gate native input observation correction

The first integrated full gate is stopped after339 passes and one failed new native case. The saved
scene remained in its undone state when the Redo byte/pixel comparison timed out; the original
observer did not prove processed Y delivery. A first S/Z/Y observation refinement then exposed a
separate unobserved Ctrl+3 focus chord in the occlusion phase. Both generations, original native
captures, scripts and full logs remain retained. Neither is accepted as a completed gate.

The private raw preview observer now copies all twelve relevant keys and pointer/button phases.
The fixture presses each physical key once and waits for processed IO acknowledgement of every
press/release; cleanup sends releases only.
it never retries a command or mutates the model through test access. Content popup centers must
settle before input, and the ordinary Inspector title collapse control avoids double-click timing.
The same complete104.14-second native case passes with exact existing geometry/color/file/pixel
oracles and unchanged180-second deadline. Its processed Ctrl+Y frame records the restored pose.
The final required/optional gates restart from a new frozen source generation.

## Verification

Final integrated Development Modular configure/all-build and unfiltered **510/510** tests
pass, **zero failures/skips**, **1669.58 seconds**. All frozen source
hashes remain exact. [Machine receipt](ED-WF-07-SceneSpritePreview-Cloud.json) records every configured
test, native observations, source/log/image hashes, complete gate commands and retained failures.
The final main Scene native case proves independent perspective color/trim/Pivot/mirror and cutout
picks, exact Frame All source/mesh union,32 Play sessions, floating Inspector occlusion/reveal,
resize, prospective Move then commit, exact Undo/Redo bytes/pixels,32 actual Content context Atlas
reimports with monotonic native IDs, and corrupt Atlas refusal preserving the last-good owner.
Read-only preview/navigation/Play preserves all project hashes. The CPU fixture additionally checks
full mirrored/sheared hierarchy and same-node mesh/source union, immutable reimport/lifetime and
unsupported/missing/stale-reference handling without changing World/history.

Unedited1280x720 native captures: [dual paused](SceneSpritePreview-dual-paused.png),
[Frame All](SceneSpritePreview-frame-all.png), [Inspector occlusion](SceneSpritePreview-occluded.png)
and [reimported](SceneSpritePreview-reimported.png). These functional Xvfb/lavapipe observations do
not certify physical displays, GPUs or performance.

The original eight-case focus passes114.28 seconds after the genuine dual-view fix; the final six
CPU/native cases pass5.61 seconds. Dedicated dual and retirement tests pass with proven actual
Khronos instance/device layer insertion, requested synchronization validation and zero VUID/SYNC
diagnostics. Both original twelve-frame single/direct and new twelve-frame dual pixel oracles are
preserved, including orientation/occlusion, resize and third/stale/foreign rejection. The standalone
retirement fixture submits80 RGBA+linear replacement generations/160frames while preserving another
Scene owner and an independent same-integer UI texture.

Shipping Full/Monolithic builds. Actual graphical TwoD-OFF/testing-OFF Editor and native Player
build. Actual TwoD-ON graphical Monolithic Editor passes source CPU/composition/retirement and the
same complete native Scene fixture. Shipping strips Editor, so its result is linkage validation.
No local Windows/macOS/Android/iOS execution is claimed. Every applicable exact-head hosted check
must pass before delivery; results belong in the PR/Beads. Parent ED-WF-07/08 and **5/9**, **0/8**,
**9/11**, physical/clean-host and production animation/reload acceptance remain open/unchanged.

Exact commands (source the established cloud build environment first):

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development --show-only=json-v1
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full -DNEXORA_ENABLE_EDITOR_SDK=ON
cmake --build --preset linux-shipping
cmake --preset linux-development -B build/editor-no-tests -DBUILD_TESTING=OFF -DNEXORA_ENABLE_2D=OFF -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_BUILD_PROJECT_PLAYER=ON -DNEXORA_ENABLE_PROJECT_PLAYER_NATIVE=ON
cmake --build build/editor-no-tests --target NexoraEditor NexoraProjectPlayer
cmake --preset linux-development -B /workspace/work/editor-completion/packaged-preview-monolithic -DNEXORA_LINK_MODE=Monolithic -DNEXORA_ENABLE_EDITOR=ON -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_WINDOW_PRESENTATION=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build /workspace/work/editor-completion/packaged-preview-monolithic --target NexoraEditor NexoraSceneSpritePreviewTests NexoraSceneUiCompositionTests NexoraSceneTextureRetirementTests
ctest --test-dir /workspace/work/editor-completion/packaged-preview-monolithic -R '^(editor.scene_sprite_preview|window_presentation.scene_ui_composition|window_presentation.scene_texture_retirement|editor.linux_scene_sprites)$' --output-on-failure
python Tools/Build/TestDocumentationCI.py
```

Documentation utilities pass16/16. Changed Markdown/local links, bilingual roadmap pairs,
touched C++ formatting and final diff checks are required again after this evidence update.
Repository-root README remains unchanged.
