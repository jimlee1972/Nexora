# Main Editor Sprite source creation — Linux, 2026-11-14

Task `nexora-whr.7.11`; base main `1a057c16ce34b6d5c06d19ca52c3a35cb017efbd`.
Workflow **5/9**, graphical Editor **0/8**, TwoD **9/11** remain unchanged.

## Delivered behavior

✅ An imported Atlas's Content context menu opens an owning source-creation modal. Original
size, top-left trim, bottom-left pixel Pivot and PPU produce a new canonical 96-byte source with
an independently generated UUID; trim size follows the selected region and borders start at zero.
One explicit action creates and imports only that saved file. GUI rendering performs no new file IO,
retains no model/pixel borrow and creates no GPU resources. Current scope/version, writer/recovery,
focus, stopped Play, busy state, names, geometry, asset/payload/path capacity and destination/identity
collisions guard creation. Existing assets are never replaced. Created-file/import-failure and
uncertain-publication outcomes remain explicit. See [file/import contract](../../../../Engine/Editor/README.md#new-main-project-sprite-source-files)
and [GUI contract](../../../../Engine/EditorImGui/README.md#main-sprite-source-creation).

The serialized application stages single saved-source import, browser Discover and dependencies
before replacing live in-memory owners. No whole-project rescan or project-generation reset occurs.
Existing Content selection, folder/filter and pending file-move Undo survive; Undo retains the new
asset. Creating an asset does not assign it or dirty Scene/Runtime. Existing-source editing, slicing,
source animation/reload and full ED-WF07/08 remain separate, open work. Independent task claims are
preserved. This is a cooperative local-filesystem operation, without power-loss durability claims.

## Acceptance

Complete Development Modular configure/all-build and unfiltered **514/514** tests pass,
zero failures/skips, **1740.31 seconds**. The full gate records all 15 frozen source hashes.
Shipping Full, actual TwoD-OFF/testing-OFF graphical Editor/native Player, graphical Monolithic
TwoD-ON model/control tests and the actual native creation flow pass. Exact-head hosted CI is
required before actual delivery; PR/Beads record its final result. No local Windows/macOS/mobile
execution, physical display/device or clean-host acceptance is claimed.

Model checks verify exclusive file publication, canonical asymmetric metadata, read-only/recovery,
foreign/stale scope, symlink destinations (POSIX), unsafe/device/case-alias names, absent regions,
invalid geometry, 4096 typed assets, occupied staging, wrong embedded identity, single-file import,
preserved file-move Undo and read-only identity reopening. Actual 1×/2× widgets verify numeric/text
input, owning one-shot requests, rejection/retry, focus, stale content, writer/Play/build guards,
modal F5 blocking, success dismissal and unchanged Scene/Runtime/files. The 2× fixture uses Atlas
UUID1:1 to ensure the validation witness cannot collide with its dependency.

Real X11 input creates the successful file, immediately assigns it to two selected entities,
checks Source details, saves the generated UUID and reopens read-only. The source is never patched
by the test. Its independent oracle reads the existing ND2P schema-1 layout: original19×23,
trim(3,5;7×9), Pivot(4.5,8.25), PPU2, Atlas region7 at(3,4;7×9). Every pre-existing Content file
remains exact. After read-only operations and shutdown, all project files match the saved snapshot.
The opt-in observer copies actual widget geometry and displayed text only. Native screenshots are
additional presentation evidence, without OCR, physical-DPI or glyph-quality claims.

Lossless, unedited1280×720 captures: [draft](SpriteSourceCreation-draft.png),
[assigned details](SpriteSourceCreation-assigned.png), [read-only reopen](SpriteSourceCreation-readonly.png).
[Machine receipt](ED-WF-07-SourceCreation-Cloud.json) contains source/log/image hashes, every configured
result, native UUID/file hashes and retained development failures. Initial compile/test fixture
errors were corrected before this frozen acceptance gate; no admission guard or deadline was relaxed.
The initial native oracle expected a different magic, although the product created canonical bytes;
the oracle now checks the existing codec's exact magic/schema and complete metadata.

## Test-only canonical-root follow-up

The full gate above ran at `9c17c45c`. A subsequent one-line UI-test correction compares the
request root with `ProjectWorkspace.Root()` rather than the temporary input path. macOS temporary
directories can use aliases; the production owner correctly emits the canonical workspace root.
All other 14 frozen files, including every product implementation/build file, remain byte identical.
The updated actual 1×/2× UI test passes both ordinary TMPDIR (**1.79 s**) and a real symlink TMPDIR
(**1.85 s**) on Linux. This is targeted portability validation, not a new local full-gate or local
macOS claim. The machine receipt preserves the original full-gate hash and the current test hash;
updated-head hosted CI remains required. No runtime behavior, admission or deadline changed.

## Exact commands

Source the established cloud SDK environment, then:

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
cmake --build /workspace/work/editor-completion/packaged-preview-monolithic --target NexoraEditor NexoraSpriteSourceCreationTests NexoraSpriteSourceCreationUiTests
ctest --test-dir /workspace/work/editor-completion/packaged-preview-monolithic -R '^editor.sprite_source_creation(_ui)?$' --output-on-failure
python Tests/EditorImGui/LinuxSpriteSourceCreation.py --editor /workspace/work/editor-completion/packaged-preview-monolithic/Apps/Editor/NexoraEditor --fixture /workspace/work/editor-completion/packaged-preview-monolithic/Tests/EditorImGui/NexoraSpriteSourceCreationUiTests --xvfb /workspace/work/editor-completion/sysroot/usr/bin/Xvfb --xdotool /workspace/work/editor-completion/sysroot/usr/bin/xdotool
python Tools/Build/TestDocumentationCI.py
```

Documentation routing utilities pass16/16. Final Markdown links, bilingual roadmap pairs, touched
C++ formatting, immutable source hashes and diff checks are recorded after evidence generation.
The repository-root README and overall milestone counts are unchanged.
