# ED-WF-07 owning Sprite source authoring documents

Date: 2026-11-13 (Asia/Taipei). Base: `cde8e8e5` (PR #770, including PR #771).
Tracking: `nexora-whr.7.2`, supporting [#748](https://github.com/jimlee1972/Nexora/issues/748).
The separately claimed Flip/Atlas-repack task `nexora-whr.6.4` remains independently owned.

## Scope and contracts

✅ ND2D schema 2 adds an owning catalog of existing 96-byte Sprite source metadata after the
four historical sections. Records are length-prefixed, sorted by UUID high/low and unique, with
1..4096 records and the unchanged complete 64 MiB wire budget. Source-free encoding retains exact
schema-1 bytes. Atlas/Clip/source/NX2S wires and every C/Zig table retain their existing layouts.

✅ TwoDDocument owns an immutable catalog, returns retained source aliases, and validates the full
document against its Atlas through ResolveSpriteAsset. Assignment, replacement, removal, history,
Dirty/save/reopen and isolated Play preserve source identity and independent generations. Missing,
duplicate, conflicting, out-of-order or malformed metadata, invalid geometry, dangling bindings and
read-only operations reject. A referenced source cannot be removed. Serialization-budget failure
rolls back before recording history. Existing 256-transaction/64 MiB history bounds remain; external
retained owners are outside history accounting. C++ document/context/decoded-scene/panel layouts
change, so consumers must rebuild.

✅ Studio provides source import requests and new/current Sprite UUID selection, source-aware
preview and source-owned geometry controls. IO stays in the workspace owner; the panel never writes
files. Player admits embedded Atlas/Clip/source closure before module callbacks, preserving existing
registry capacity, retirement and cross-document identity checks. Conflicting same-UUID metadata
rejects startup. Exported scenes need no source paths after relocation.

This supports isolated 2D Studio documents. Main Editor Inspector/project save/reopen/Build, live
source animation/reload and complete ED-WF-07 remain open. Workflow **4/9**, graphical Editor
**0/8**, 2D **9/11** and physical M6/M7 acceptance remain unchanged.

## Executed evidence

`twod.authored_sources` checks byte-identical v1 encoding, canonical v2 round trips, all truncations,
trailing data, duplicate/invalid/count-limited catalogs, exact 4096-entry admission, failed reference
publication, retained aliases, Undo/Redo, read-only rejection and old/new isolated Play geometry after
the document is destroyed. Removing the last unused source returns to schema 1, and Undo restores it.

`twod.graphical_controls` checks real import-button requests and caller/read-only rejection. Its
actual composed preview vertices respond to source Pivot replacement. Existing Undo/Redo and camera
admission assertions remain. These are Studio panel tests, not main Editor Inspector acceptance.

`twod.authored_source_export` and `twod.authored_source_native` use a real V3 C++ module, exported
hashed scene documents and relocated Unicode package paths. All authoring sources are deleted before
execution. Both packages validate the embedded source through public content APIs; native readbacks
require red/blue at x=172/148, y=108 initially and at x=148/172 after Pivot replacement, with empty
samples x=112/208. Both authored Sprites retain their World transforms, including negative X scale.
The source trim is (1,1,1,1) in a 4x4 canvas, PPU 1; Pivot X changes from 2 to 3. Native Player reports
30 presented frames and balanced gameplay allocations. Optional Studio execution reopens the moved
document read-only, presents six native frames and leaves its bytes unchanged.

The export tests also corrupt the authored source and require failed export to preserve every
previous package byte. A package containing two conflicting same-UUID source definitions must reject
before gameplay. Every native attempt retains its actual screenshots, logs and package inventory;
CI uploads `artifacts/authored-source*/`.
The curated [initial](AuthoredSources-initial.png) and [shifted](AuthoredSources-shifted.png)
screenshots are unchanged byte copies from the final Development attempt. The independently
executed Shipping captures have identical PNG hashes and sampled RGB values.

## Validation and retained failures

Focused validation reached 7/7 passing cases before the final serialization rollback and native
Studio checks. Final Development configure/build pass, and its unfiltered two-worker gate passes
**477/477**, zero skips/failures, in **1461.24 seconds**. Full Shipping configure/build, strict
Shipping relocation/native pixel acceptance and the TwoD-OFF NexoraEditorCore build pass. API-M6
validates 145 exports; documentation routing tests pass 16/16. Exact inventories, retained failures,
pixel samples and source/artifact hashes are in the
[machine-readable receipt](ED-WF-07-AuthoredSources-Cloud.json).

The initial new test failed compilation because it compared C arrays instead of their coordinate
values; the corrected test compares both coordinates. The first module fixture omitted the required
Stop callback and was correctly rejected by the unchanged loader. An older Play test executable
also required recompilation after the public C++ document layout changed. A subsequent validation
scheduling error overlapped a rebuild and tests, exposing a temporarily relinking shared library;
that failed run is retained separately. Final validation runs after builds finish, without weakening
loader requirements, library checks, pixel assertions, warning flags or existing deadlines.
An in-progress full build was intentionally restarted after adding the fixture's Windows wide-path
entry; the subsequent complete build/test is the final accepted local execution.

## Reproduction

Reuse the user-owned cloud toolchain/sysroot from PR #771: CMake 4.4.4, Ninja 1.13.2, GCC 14.2,
Zig 0.14.0, Slang 2026.18, clang-format 23.1.5, Xvfb/xdotool and lavapipe. No system installation
or tracked preset override is required. Local execution is Linux; hosted platform CI is recorded
separately in the delivery PR.

```sh
cmake --preset linux-development
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development
cmake --preset linux-shipping -DNEXORA_SHIPPING_PROFILE=Full \
  -DNEXORA_ENABLE_EDITOR_SDK=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_ENABLE_SLANG=ON -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF
cmake --build --preset linux-shipping
python3 Tests/TwoD/AuthoredSourceAcceptance.py \
  --app build/linux-shipping/Apps/TwoDPlayer/NexoraTwoDPlayer \
  --fixture build/linux-development/Tests/TwoD/NexoraAuthoredSourceTests \
  --module build/linux-development/Tests/TwoD/libNexoraAuthoredSourceFixture.so \
  --compiler build/linux-shipping/Tools/TwoD/NexoraTwoDAssetCompiler \
  --evidence-dir build/linux-shipping/artifacts/authored-source-native --shipping --native
cmake --preset linux-development -B build/editor-source-feature-off \
  -DBUILD_TESTING=OFF -DNEXORA_ENABLE_2D=OFF \
  -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=OFF \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=OFF -DNEXORA_ENABLE_SLANG=OFF
cmake --build build/editor-source-feature-off --target NexoraEditorCore
```

Development retains optional Zig/Slang/graphical Editor coverage. Shipping strips the standalone
Editor and tests Runtime/SDK linkage; actual native graphics run in Development and the Shipping
Player. The repository-root README stays unchanged because this supporting slice accepts no major
milestone.
