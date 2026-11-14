# ED-WF-07 immutable Editor Sprite source snapshots

Date: 2026-11-12 (Asia/Taipei). Base: `69de02dfd05f336d5ac4f4b0762e57448c096bee` (PR #769).
Tracking: Beads `nexora-whr.7.1`, supporting [ED-WF-07 / #748](https://github.com/jimlee1972/Nexora/issues/748).
Catalog/package admission is independently owned by `nexora-whr.6.3` / PR #770; this slice does not
modify its implementation or claim complete ED-WF-06/07 acceptance.

## Delivered contracts

✅ Optional typed project indexing and synchronous/queued reimport admit the existing 96-byte
`.nx2sprite` schema 1. The private, noncopyable `TwoDAssetPayload` owns shared const Sprite metadata,
reports its stable embedded UUID/type and sole Atlas dependency, and charges its object size under
the unchanged 4096-asset/64 MiB workspace limits. Sidecars remain authoritative across rename;
expected UUID, cancellation, source read, hash and authoring-publication rules are unchanged.

✅ `TwoDPlayAssets` freezes source/Atlas closure from an explicitly pinned cooked generation.
It validates decoded UUID, envelope dependency list, Atlas type, region and trim geometry through
`ResolveSpriteAsset`, then releases every temporary store reference/pin. Project capture additionally
checks current writer/project/Content scope, sidecar importer/identity, graph and exact canonical
source bytes. Rejected candidates preserve existing snapshots. Atlas/Source owners and extracted
geometry survive replacement, collection and destruction of project/store owners.

✅ Main Editor Play captures indexed sources and uses the frozen source resolver for gameplay
scene and pooled instances. C/Zig descriptors and all Atlas/Clip/source/scene formats retain their
layouts. Public C++ Editor payload/snapshot layouts change; consumers rebuild. The embedding keeps
source-content registration unsupported: its source assets are immutable captures.

Inspector source assignment, authoring persistence/reopen, Build export, live source animation and
reload publication remain separate gates. Workflow progress stays **4/9**, complete graphical
Editor **0/8**, 2D **9/11**; physical M6/M7 acceptance is unchanged.

## Executed evidence

`twod.editor_sprite_sources` exercises real import/cook/bundle/store and project paths: typed source
ownership, malformed/type/UUID/cancellation failures, exact/short closure budgets, missing or corrupt
Atlas closure, retained generations, uppercase sidecar discovery, renamed importer identity,
synchronous and actual queued reimport, stale provenance and graph drift. Existing Atlas/Clip
source-free content/capture/Play tests remain unchanged and run alongside it.

`editor.sprite_source_play` uses the actual main Editor `PlayGameplayModule` binding, a real Play
clone and public C gameplay tables. The module assigns a red source Sprite and a blue source pool
instance with negative X scale. Both use a captured white Atlas and source trim `(1,1,1,1)` in a
4x4 canvas, PPU 1. Captures with Pivot X=2 and X=3 retain the same UUID. Three phases use old/new/old
snapshots after project/store/source owners are gone, checking geometry, legacy read rejection,
callback teardown, pool destruction and unchanged authoring World.

`editor.sprite_source_native` runs that same path in real composed EditorImGui/Vulkan presentation
on Xvfb/lavapipe. With viewport `(40,30,320,240)` and camera height 10, actual window readbacks at
y=137 require red/blue at x=212/188 initially, reversed at x=188/212 after Pivot replacement, then
restored for the retained old snapshot. Samples x=152/236 must contain neither color. Every native
attempt has a fresh directory with actual PPM readbacks, including the last failing frame. CI retains
those attempts. Curated PNGs preserve the raw PPM RGB bytes exactly: [initial](SourceSnapshots-initial.png),
[reimported](SourceSnapshots-shifted.png), [retained old snapshot](SourceSnapshots-retained.png).
These are frozen-snapshot rendering/lifetime checks; production activation still
requires the current `Matches` observation and its host Play/modal authorization.

The focused eight-test group passes after the fixture repair below. Full configure/build/test and
Shipping results, exact source hashes and artifact identities are recorded in the
[machine-readable receipt](ED-WF-07-SourceSnapshots-Cloud.json).

## Environment and reproduction

Tools remain user-owned under `/workspace/work/engine-goal-20261112`: CMake 4.4.4, Ninja 1.13.2,
GCC 14.2, Zig 0.14.0, Clang 19.1.7 and clang-format 23.1.5. This slice adds official Slang 2026.18
in that same workspace and enables the graphical Editor/Slang configuration. The existing extracted
X11/Vulkan sysroot, Xvfb/xdotool and software Vulkan driver are reused. PATH, prefix/library paths,
Vulkan manifest and cache/state paths point at that workspace. No system configuration or tracked
preset override is changed. Linux execution is distinct from hosted Windows/macOS CI.

```sh
cmake --preset linux-development -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full \
  -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF
cmake --build --preset linux-shipping
```

The new tests are feature-gated with TwoD/Editor; the native variant uses the existing isolated
Xvfb harness and Linux/Slang/Vulkan prerequisites. No test assertions, production validators,
resource budgets or existing deadlines are relaxed.

The full Development configure/build succeeded. The unfiltered 472-test execution used four
CTest workers on a four-CPU cloud quota and overlapped the separate feature-OFF build. It recorded
465 passes, zero skips and seven existing native UI failures (CTest exit 8). A subsequent quiet
`ctest --preset linux-development --rerun-failed --output-on-failure -j 1` passed all seven cases
in 321.57 seconds (exit 0), retaining the same code, driver, assertions and deadlines. Thus all
472 registered cases passed across the full run and recovery, with zero unresolved failures.
The complete recovery result is recorded separately in the receipt;
the initial failed run is never described as a clean single execution. Full Shipping and the
`NEXORA_ENABLE_2D=OFF` `NexoraEditorCore` build both passed. API-M6 validates 145 exports;
documentation routing tests pass 16/16, changed Markdown/bilingual roadmaps and touched C++
formatting pass. The repository-root README is unchanged.

## Retained failures and scope reconciliation

The first eight-test focused run had 6 passing cases and 2 failures. Both new Play variants failed
because the fixture indexed `SpriteFrame.vertices` after the existing composition path had moved
that buffer into UI draw data. It now checks the actual draw-data vertex/index counts and compares
those positions. The source pixel phase had already produced its actual readback. All eight cases
then passed; initial logs/readbacks remain separate from successful attempts. Engine rendering
behavior was not changed to accommodate the fixture.

Initial shared task synchronization rejected a non-fast-forward push. Pulling shared history and
pushing again succeeded without force or replacement. Before implementation, the already claimed
catalog task/PR #770 was identified and preserved; this independently scoped Editor prerequisite
was created and claimed instead. No other contributor's PR is merged by this task.

An initial Shipping configure requested the graphical shell and correctly failed: Shipping always
strips the standalone Editor. The final Full Shipping override retains Editor SDK compilation and
keeps the graphical shell OFF; the existing guard is unchanged. Native graphics run in Development.
