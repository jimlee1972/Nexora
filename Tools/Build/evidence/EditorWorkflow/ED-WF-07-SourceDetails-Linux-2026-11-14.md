# Main Inspector Sprite source details — Linux, 2026-11-14

Task `nexora-whr.7.10`; base main `edb5e17ba85209b93d307d6bd8cc0932fca7017d`. Independent Clip catalog PR #786 is preserved.
Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** remain unchanged.

## Behavior and ownership

✅ The initially collapsed **Source details** section shows source/Atlas UUIDs, original pixel
size, top-left trim/Atlas region, bottom-left Pivot, PPU, L/R/T/B borders and source-local bounds
before entity transforms. The shared TwoD resolver supplies geometry; no duplicated Pivot math.
The current frame borrows imported immutable metadata and owns only bounded display text. There
is no file IO, asset or Runtime mutation, retained model pointer, thumbnail cache or GPU registration.
Read-only projects and Play can inspect. Mixed/absent/missing/unsupported references hide data;
assignment/Clear suppress that frame's older owning component preview. Current selection/content
and generation are resolved again on the next draw. Collapsed idle cost stays unchanged.
See [UI contract](../../../../Engine/EditorImGui/README.md#main-sprite-source-inspector) and
[native observer](../../../../Apps/Editor/README.md#native-project-and-asset-workflow-acceptance).

## Verification

Complete Development Modular configure/all-build and unfiltered **510/510** tests pass,
zero failures/skips, **1671.38 seconds**. All six frozen source hashes
remain exact. [Machine receipt](ED-WF-07-SourceDetails-Cloud.json) retains every configured result,
full gate commands, source/log/image hashes, native checks and the original fixture failure.
Shipping Full, actual TwoD-OFF/testing-OFF graphical Editor/native Player, and actual graphical
TwoD-ON Monolithic Editor/control/native Inspector gates pass. No local Windows/macOS/mobile
execution or physical hardware/display acceptance is claimed. Exact-head hosted CI remains a
required delivery gate; its results belong in PR/Beads.

Actual 1×/2× widgets verify explicit expansion, source switches, Clear, exact history, mixed,
missing/unsupported data, reimported Pivot and a changed Atlas/source geometry, read-only and Play,
context removal and a 32768-row unrelated catalog. Inspecting preserves exact wrapped scene,
Runtime and source bytes and empty history after reopen. Native X11 input assigns two entities,
expands details in writable/read-only launches, checks independent expected asymmetric values,
exact Undo/Redo/Clear/save/reopen and complete read-only project-file hashes.
The independent fixture is original19×23, trim(3,5;7×9), Pivot(4.5,8.25), PPU2,
Atlas16×16 region(3,4;7×9), local bounds(-0.75,0.375) to(2.75,4.875).
The observer copies current displayed text only under the explicit test flag; pixel captures are
additional presentation evidence, not OCR, physical DPI or glyph-quality certification.

Unedited1280×720 captures, converted losslessly from raw PPM with byte-identical RGB pixels:
[writable details](SpriteSourceDetails-writable.png), [read-only details](SpriteSourceDetails-readonly.png).

The first native fixture failed before assignment because only one of its two Sprite sources had
been updated to the asymmetric Atlas dimensions. The complete catalog correctly refused its
inconsistent closure. Both fixture sources now match. Original source hashes, native artifacts,
failure log and full output remain retained; no product admission, oracle or deadline was relaxed.

Exact commands (source the established cloud SDK environment first):

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
cmake --build /workspace/work/editor-completion/packaged-preview-monolithic --target NexoraEditor NexoraSpriteSourceInspectorTests
ctest --test-dir /workspace/work/editor-completion/packaged-preview-monolithic -R '^editor.sprite_source_inspector$' --output-on-failure
python Tests/EditorImGui/LinuxSpriteSourceInspector.py --editor /workspace/work/editor-completion/packaged-preview-monolithic/Apps/Editor/NexoraEditor --fixture /workspace/work/editor-completion/packaged-preview-monolithic/Tests/EditorImGui/NexoraSpriteSourceInspectorTests --xvfb /workspace/work/editor-completion/sysroot/usr/bin/Xvfb --xdotool /workspace/work/editor-completion/sysroot/usr/bin/xdotool
python Tools/Build/TestDocumentationCI.py
```

Documentation routing utilities pass16/16. Changed Markdown/local links, bilingual pairs,
touched C++ formatting and final diff checks pass after evidence generation.
Root README, full ED-WF-07/08 and physical/production animation/reload acceptance stay unchanged.
