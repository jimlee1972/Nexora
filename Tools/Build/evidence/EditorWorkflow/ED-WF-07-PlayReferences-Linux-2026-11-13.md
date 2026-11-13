# Main Editor authored Sprite references in module Play — Linux, 2026-11-13

Task `nexora-whr.7.4`; base main `82d8c49f` (PR #773, incorporating PR #775). Dates use Asia/Taipei.
This adds the module-backed Play consumer of the main Editor's versioned source references.
The independently merged ND2D/Studio work in `nexora-whr.7.2` / PR #773 remains outside this slice.
Workflow **5/9**, graphical Editor **0/8**, 2D **9/11** remain unchanged.

## Behavior and boundary

- `CapturePlayTwoD` detects source references with bounded owning metadata previews, validates
  every UUID against the frozen project source/Atlas closure, and owns the exact bounded Runtime
  and opaque authoring capture. Missing/unsupported reserved references reject without rewriting
  the document. Empty Content cannot silently discard a present reference.
- The graphical host rechecks `MatchesPlayTwoD` and existing project asset provenance immediately
  before loading a configured module. Reference-only edits, Runtime changes and document reopen
  invalidate authoring activation. The no-module path keeps its previous behavior.
- `PlayTwoDServices::Bind` requires the matching active isolated Play scene and Runtime bytes.
  It decodes references from the immutable owning capture and creates private default styled
  source Sprites before module `Create`; no duplicate World or authored Runtime transform is made.
  Foreign/duplicate entities, missing sources, malformed metadata and mismatched clones reject
  before callbacks. Reference publication failure discards private metadata before input/audio/storage
  setup. Existing per-scene membership checks remain one-time preparation work.
- Frozen scene and asset ownership survive source/project teardown. Stop/destroy callbacks retain
  services until completion; the Sprite owner drains before its borrowed Play World. Capture,
  activation and extraction remain owner-thread serialized. Existing separate Runtime, opaque and
  asset budgets apply; this is not an allocator/RSS budget.

See the [application contract](../../../../Apps/Editor/README.md#authored-source-references-in-module-backed-play)
and [reference model](../../../../Engine/Editor/README.md#main-editor-sprite-source-reference-transactions).
No C/Zig ABI, NX2S/ND2D/asset codec or module dependency changes.

Final review identified a scalability regression in the first reference probe: calling the
single-entity Inspector accessor for every node repeated linear key/metadata searches. The final
probe uses the new generic `SceneDocument::InspectOpaqueComponents(type, reserved_name)` overload,
with one live-entity index and one node/record traversal. It filters by reserved type **or** name,
owns at most 64 preview bytes per matching row and performs no Runtime serialization. The CPU test
now belongs to EditorCore rather than depending on EditorImGui. A 100000-node probe measures
35.589 ms in the initial focused run (diagnostic only, no timing threshold), and tests alias
matching, oversized-preview bounds, stale membership and owners surviving document teardown.
The first large fixture build called a nonexistent direct World deletion method; it was corrected
to the existing WorldCommandBuffer API and its compiler failure retained separately.

## Focused tests and native proof

`editor.play_authored_sprites` uses real ProjectWorkspace/AssetWorkspace/Content imports, main
reference assignment and isolated Play. The module reads both authored source Sprites before its
Create callback writes any Sprite. Runtime bytes and authoring capture remain exact through
load/extraction/unload. Stop/destroy ordering retains the source service.

Rejected fixtures cover disabled authority, malformed/future/oversized/name/type references,
missing source UUIDs, changed Runtime transforms, reference-only drift, document reopen,
foreign/duplicate entity metadata and excessive reference counts. Rejection precedes module
Create and preserves Runtime bytes and document history. Reimport retains old source owners;
reopened references capture correctly, and retained bindings run after source removal. Empty
source-free Content keeps the legacy binding, but a later reference invalidates that activation.

The existing `editor.sprite_source_play` / `editor.sprite_source_native` fixture now runs
legacy/authored/legacy snapshots. The authored phase uses actual `AssignSpriteSources`, and its
module first verifies the bound source UUID and default white style through the public source API.
The callback then styles the Sprite red and creates the existing blue negative-scale pool.
Native composed pixels verify Pivot 2 / Pivot 3 / retained Pivot 2 geometry after source/project
destruction. The legacy phases verify no authored Sprite leaks across teardown. This is not a
new Inspector or no-module visual acceptance.

The initial focused CPU set (reference catalog, legacy Play services, source Play and new authored
Play) passes **4/4**, zero failures/skips. The initial actual native case passes without a skip.
No assertion, timeout, native-loader requirement or test outcome is relaxed.

## Full validation

Main advanced from PR #775 to PR #773 during the first unfiltered run. That obsolete-base run
was intentionally interrupted (CTest exit 130), with its completed outcomes retained separately;
it is not counted as a full pass. Own changes were stashed, the branch fast-forwarded to the
already-merged main and all changes restored. The two Editor roadmap append conflicts preserve
both delivered sections; all own C++/test-registration hashes match their pre-integration values.
The complete gate is rerun against the integrated main, including the new authored-document tests.

Linux x86_64, GCC 14.2.0, CMake 3.31.6, Ninja 1.13.2; cached Development enables graphical Editor,
2D, Zig and Slang. The [machine-readable receipt](ED-WF-07-PlayReferences-Cloud.json) records all
491 configured case outcomes, source/log hashes and actual capture pixels.

```bash
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development
```

Builds finish before graphical acceptance; no compilation overlaps the full test run.
✅ The final implementation configures/builds and passes **491/491**, zero failures/skips,
**1357.85 seconds**. The final 100000-node reference probe measures **32.4834 ms** (diagnostic only),
and its complete CPU case passes in 1.97 seconds. The enhanced actual native source case passes
in 1.36 seconds. The earlier integrated-base run before the bulk correction passed 491/491 in
1362.55 seconds; its status log/source hashes remain separate pre-optimization evidence.
Existing native main Game View, layout restart, C/Zig, pool, SDK, reimport, lifetime and all three
authored-source cases from PR #773 remain in scope.

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

✅ Shipping configures/builds; by contract it strips standalone Editor/EditorCore, so it does not
execute this new application helper. The actual graphical Editor compiles/links with 2D OFF and
testing OFF. The Monolithic consumer compiles/links the new exported bulk API and passes **1/1**
without graphical Editor, including its 100000-node probe and Play lifecycle/rejection checks.
Documentation utilities pass **16/16**, nine changed Markdown documents/language pairs
pass validation, touched C++ formatting and `git diff --check` pass. Root README stays unchanged.
The final inventory query ran before the full gate; both the full named-outcome status log and
detailed `LastTest.log` are retained with hashes. An inventory query during the earlier
pre-optimization run overwrote that run's temporary detailed log; its complete status log and
native harness receipts remain, and it is not used as the final detailed-log proof.

Actual full-window native captures are retained without redaction or compositing:
[legacy](PlayReferences-legacy.png), [authored](PlayReferences-authored.png),
[restored legacy](PlayReferences-restored.png). The authored phase exchanges the red/blue positions
after Pivot reimport; each quad is exactly 24x24 pixels (576 strict red/blue pixels). Legacy and
restored source-only red/blue masks match exactly. A receipt-only initial whole-frame identity
assertion failed: the first ProductShell frame had not finished its dock layout, and 293888
background/UI pixels differ from the restored frame. All three original full images and their
distinct hashes are retained; no whole-frame identity or complete GUI workflow is claimed.
The source-mask check uses the native fixture's unchanged strict channel predicates and supplements
its existing per-position assertions; no production test assertion or deadline was removed.

Hosted CI and merge are recorded by the PR and Beads against the eventual delivery head. Local
passing gates alone do not permit a merge before all applicable CI succeeds.

## Remaining work

Inspector widgets and complete user authoring flow, Scene View conversion, a no-module Play owner,
main Editor Build export and source animation/reload remain open. Production module loading still
requires fresh project/source provenance even though retained matching snapshots can be exercised
without authoring sources by the isolated replay fixture. The full ED-WF-07 workflow is not accepted.
Windows/macOS/mobile and physical-GPU runs are not claimed from this Linux execution environment;
hosted CI results are tracked separately for the delivery head.
