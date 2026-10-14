# Zig 2D engine-control acceptance — Linux, 2026-10-14

✅ Engine-facing C/Zig control is implemented and executed. Original V3 prefix compatibility
is preserved; TwoD remains optional. This record is separate from the earlier C++ 2D evidence.

| Gate | Observed result |
| --- | --- |
| Linux Development configure/build | `cmake --preset linux-development`; `cmake --build --preset linux-development`: PASS. Cache enables Zig 0.14.0, Slang 2026.18 and graphical Editor. |
| Complete Linux suite | `ctest --preset linux-development`: 267/269 initially passed, 820.90 s; two existing native Editor input cases failed. No tests skipped. |
| Failed-case verification | `ctest --preset linux-development --rerun-failed --output-on-failure`: 2/2 PASS, 29.18 s, with no concurrent build. `editor.linux_build_process_console` missed operation-4 input; `editor.linux_native_scene_center` missed a one-step Undo/save. Their code and assertions were not changed. All 269 cases have passing results; the initial full-run failure is retained explicitly. |
| Final affected regression | The command below passes 16/16, 10.41 s, after final source/header review and rebuild. |
| C/C++/Zig ABI | C11 consumer, original prefix, manifest/immutable V3 baseline plus additive 2D baseline; real Zig compiler checks every 2D record/table field offset and size. PASS. |
| 2D bridge | Copied Sprite/collider/camera, clip switch/pause/events/probe/retry, query snapshot/masks/sweep, Tilemap/chunks, atomic pool batches, stale/foreign/duplicate rejection, thread/reentry and teardown. PASS. |
| Actual Zig consumers | Static and dynamic start/fixed/update/reload/stop/destroy; input and TransformV2, Sprite/animation/query control, tile/chunk access, batch create/read/write/destroy; no duplicated content, allocator balance. PASS. |
| Native input/presentation | `gameplay.zig_twod_native`: 600 actual Linux X11/Vulkan frames, held normalized D key, Zig entity movement, reload and shutdown. [Report](native.json). Software rasterizer; physical display/Windows NOT RUN. |
| Optional-off | Separate Linux Development configure with `NEXORA_ENABLE_2D=OFF`, Zig ON, native backends OFF; build/run `gameplay.zig_abi_smoke`: 1/1 PASS. No TwoD/sample targets remain. |
| Monolithic Shipping | `cmake --preset linux-shipping -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON -DNEXORA_BUILD_SHOWCASE=OFF`; `cmake --build --preset linux-shipping`: PASS. Static Zig executable copied to a fresh staging directory and launched with `LD_LIBRARY_PATH` unset, `--frames 60`: PASS. [Report/hash/scope](shipping.json). Vulkan loader uses the cloud SDK override, so this is not a clean-machine claim. |
| Documentation | Changed Markdown links/bilingual pairs, canonical API metadata and `git diff --check`: PASS. Root README stays concise and unchanged. |

Final affected regression command:

```bash
ctest --preset linux-development -R 'gameplay.zig_|twod\.|api.m6|runtime.gameplay_module_host'
```

MSVC CI exposed aggregate initialization of an array of explicit SpatialIndex constructors.
The bridge now has a normal constructor that directly initializes both indices. After this
portable fix, the complete Linux build and affected 16/16 tests pass again (10.41 s), including
the 600-frame native input gate; Shipping rebuild/staged launch and Clang 19 syntax with
`-Wall -Wextra -Wpedantic -Werror` pass. Windows execution awaits latest-head CI.

The user requested engine control from Zig. This sample supplies no combat, enemy spawning,
progression or game rules. Engine services own worlds/assets/windows/GPU; no native pointer crosses
the ABI. See [contracts](../../README.md) and [usage](../../../../Gameplay/Zig/README.md).

2D milestone acceptance stays 9/11 (P0 6/8). Physical minimum-spec Windows 60 FPS and clean-host
DX12/Vulkan integration remain NOT RUN (`nexora-j4q`). Main Editor project Play attachment is a
separate integration (`nexora-zih`); the standalone consumer proves the delivered engine boundary.
