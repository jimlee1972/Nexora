# ED-M6 owning Runtime scene capture — Linux, 2026-10-08

## Delivered boundary

`World::SaveScene(scene, max_bytes)` uses the existing schema-3 serializer with a checked owning output buffer. It preserves classic locale, 17-digit precision, quoted names and full-width IDs; the legacy overload delegates with an unlimited logical cap. Escaped scene-name size is checked before `std::quoted` can allocate a formatter temporary. Failure returns no partial snapshot and leaves World state untouched.

`SceneDocument::CaptureRuntimeScene` captures the complete live Editor Runtime scene plus the tracked NodeKey subset and full, type-sorted unknown metadata. Existing untracked Runtime entities remain in the Runtime snapshot without invented authoring nodes. All returned bytes own their storage and survive editing, Undo/Redo, New/Reload and source destruction. Names and authored Euler hints are authoring-only and excluded.

Entity count is admitted before indexing/serialization: 100,000 maximum. Runtime bytes are capped at 64 MiB. Unknown metadata admits 4096 records, 64/entity, 1 MiB/payload, 256 raw bytes/name and 16 MiB aggregate names plus payload. Legacy non-UTF-8 names retain exact bytes; CR/LF/NUL names reject. These logical bounds do not cap allocator capacity or total process memory. Calls require serialized authoring access.

Document/NodeKey generations identify objects, not revisions. Capture grants no write/recovery/publication permission and does not establish current content, cooked semantic validity or an atomic export. The project export coordinator, cook/package, native player, graphical build/cancel/log flow and full ED-M6 remain separate work. No full milestone box is marked complete.

## Validation

Tested base: main `7b4e62f656a4347a2cf2741e0bda25074d8e3875`.
Linux cloud, GCC 14.2, CMake 3.31.6, Ninja 1.11.1; default graphical shell/Slang OFF.
External tools, Vulkan header cache and Mesa/Xvfb dependencies remain outside tracked product files.

```sh
cmake --preset linux-development -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=/workspace/Nexora/build/linux-development/_deps/nexora_vulkan_headers-src
cmake --build --preset linux-development
ctest --preset linux-development -R 'runtime.bounded_scene_save|editor.scene_runtime_capture'
ctest --preset linux-development
cmake --preset linux-shipping -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=/workspace/Nexora/build/linux-development/_deps/nexora_vulkan_headers-src
cmake --build --preset linux-shipping
```

- Development configure and 247-step build passed.
- Focused actual World/SceneDocument tests: 2/2 passed, zero skips, 5.09 seconds.
- Full Development suite: 90/90 passed on its first run, zero skips, 27.50 seconds.
- Shipping configure and 72-step Monolithic Minimal build passed with Editor/SDK stripped.
- Source formatting and diff checks passed; owning README and bilingual roadmap contracts are updated.
- No local Windows/macOS/mobile or physical desktop acceptance is claimed.

Tests use actual production serializers, metadata and history. They cover historical golden wire bytes, every under-cap boundary, exact-cap acceptance, locale/precision, escaped UTF-8 names, missing/unloading scenes, large names and a deep hierarchy; ownership and unchanged state; 100,000/100,001 entities; duplicate/zero/missing identities; tracked subsets; opaque count/individual/aggregate boundaries including raw names; real save/reload, New, destruction and Undo/Redo. Capture errors return no partial value and clear on successful retry.

Logs are task-local under `/tmp/nexora-ed-beads-p1p7a0jw/scene-capture-{configure,build,focused,tests,shipping-configure,shipping-build}.log`; reproducible commands and exact source hashes are retained here. Beads: `nexora-62u.1.3`, `nexora-62u.1.4`.

## Source hashes (SHA-256)

- `Engine/Runtime/include/Nexora/Runtime/Runtime.h`: `ddd3e068d3e87fd193ace1ae1efbcc9e87a3c0b9363a78612585fe5d61e1f8b4`
- `Engine/Runtime/src/Runtime.cpp`: `93efca4f77e2f7f3485f228f5a13f629e3b9d8d342bbb8b59a55743fa449ae65`
- `Engine/Editor/include/Nexora/Editor/EditorWorkspace.h`: `5b6b1917f7bbaa8a9926a18f086ac83890df7efec7ef0af7e84697c139fa0938`
- `Engine/Editor/src/EditorWorkspace.cpp`: `36ded07cc706d051a95a8a4fc54bea10c0f52dbd42bbd9d9d0eb7ed522586d47`
- `Tests/Runtime/BoundedSceneSaveTests.cpp`: `b35dffcc1359af6e29ee289525576a9e74de5b356f67f67b84ff2b2e61fa66e9`
- `Tests/Editor/SceneRuntimeCaptureTests.cpp`: `64e9f20fac36ef1565dd90ebd2827530c47024bb3cf471c95e7ae5cd7a27f691`
- `Tests/Runtime/CMakeLists.txt`: `d30fe168039d3cdae4d551afb46c547c1ad86cd052fbc98c86777629dba90505`
- `Tests/Editor/CMakeLists.txt`: `b7e6732edc6c93497aeddc250cf68f5b10d5b00f7f2be5414f365f7b1d3d589a`

## Integration with scalar material assets

Integrated main `8c4ee4e4397588104e2c11f777433aa226811885` in commit `0752f5f3`.
The real conflict was only adjacent appended Editor test registrations; both targets and all material tests were preserved. Capture metadata retains the reserved material binding as complete opaque bytes. Bounded serializer/capture method semantics and tests were unchanged.

Repeated required Development configure, 123-step build and full suite: **92/92 passed on the first integration run, zero skips, 28.66 seconds**. Shipping configure and 5-step incremental Monolithic build passed. Logs use `scene-capture-integration-{configure,build,tests,shipping-configure,shipping-build}.log` under the same external task directory. Local default graphical shell/Slang remain OFF; material PR owns its distinct real GUI/native evidence.

Integrated exact source SHA-256 (prior hashes describe the original tested base):

- `Engine/Runtime/include/Nexora/Runtime/Runtime.h`: `ddd3e068d3e87fd193ace1ae1efbcc9e87a3c0b9363a78612585fe5d61e1f8b4`
- `Engine/Runtime/src/Runtime.cpp`: `93efca4f77e2f7f3485f228f5a13f629e3b9d8d342bbb8b59a55743fa449ae65`
- `Engine/Editor/include/Nexora/Editor/EditorWorkspace.h`: `50649225760ef552fad5f114d10a80d2039226699dd668f1a6047e6517954835`
- `Engine/Editor/src/EditorWorkspace.cpp`: `1ed9a0141105fe403a779ed34c145ed753de21d23300ddc3d412a44dbc64e081`
- `Tests/Runtime/BoundedSceneSaveTests.cpp`: `b35dffcc1359af6e29ee289525576a9e74de5b356f67f67b84ff2b2e61fa66e9`
- `Tests/Editor/SceneRuntimeCaptureTests.cpp`: `64e9f20fac36ef1565dd90ebd2827530c47024bb3cf471c95e7ae5cd7a27f691`
- `Tests/Runtime/CMakeLists.txt`: `d30fe168039d3cdae4d551afb46c547c1ad86cd052fbc98c86777629dba90505`
- `Tests/Editor/CMakeLists.txt`: `df96cdee6ee776219c78937a064b9f9e9a453b44b915fad465df461d33cf333e`
