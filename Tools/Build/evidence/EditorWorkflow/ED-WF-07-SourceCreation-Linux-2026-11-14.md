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
before replacing live in-memory owners. It first reconciles cached paths/rows/imported owners from
the current same-root Content session. Private discovery preflight rejects Undo-reserved names,
ASCII case aliases and retained-budget conflicts before creating a source/sidecar. A later ordinary
Content command can release the original name, which then imports immediately after Rename/Move.
No whole-project rescan or project-generation reset occurs.
Existing Content selection, folder/filter and pending file-move Undo survive; Undo retains the new
asset. Creating an asset does not assign it or dirty Scene/Runtime. Existing-source editing, slicing,
source animation/reload and full ED-WF07/08 remain separate, open work. Independent task claims are
preserved. This is a cooperative local-filesystem operation, without power-loss durability claims.

## Acceptance

Complete Development Modular configure/all-build and unfiltered **514/514** tests pass,
zero failures/skips, **1793.69 seconds**. All 16 frozen source hashes match.
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

Real X11 input creates a source, renames it, verifies no-write rejection of its Undo-reserved name, renames again to release that
name, and recreates/immediately imports a distinct UUID into the vacated path. It assigns the
new UUID to two entities, checks Source details, saves and reopens read-only. The renamed source
bytes remain exact. The source is never patched
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

## Review follow-up and provenance

The initial proposal at `9c17c45c` passed its own full local gate (514/514, 1740.31 s).
[Automated review](https://github.com/jimlee1972/Nexora/pull/789#discussion_r4574356631)
then identified that a cached index could still occupy a path vacated through Content Rename/Move.
The current implementation preflights live/Undo admission before publication and reconciles the
index from current same-root Content. Model regressions cover both Rename and Move, including
ASCII case aliases, no-write Undo reservation rejection, released-path import and preserved Undo.
The extended native flow performs real renames and recreates a distinct UUID in that released path.
The full gate recorded here reruns all product and test changes, including the earlier test-only
canonical-root correction for aliased temporary directories. All 16 current source hashes match.
The previous receipt and raw logs are retained separately; current acceptance does not substitute
old-head CI or a targeted test for this fresh complete gate.

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
