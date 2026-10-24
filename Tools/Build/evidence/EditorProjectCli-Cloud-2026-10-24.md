# Editor project CLI — cloud validation, 2026-10-24 UTC

This is a manually maintained summary; generated build logs/JUnit/objects remain outside Git.
Base main: `3c27da8aa45db109e35f866bb1d9305528d7eb66`. Beads: `nexora-ltc`.
The implementing PR identifies the tested head and subsequent hosted results. No physical
Windows observation is inferred from this Linux host.

## Behavior and compatibility

- `--create-project=PATH --project-name=NAME` creates a workspace through the same
  `ProjectWorkspace::Create` and content-indexing path used by the Editor, then exits headlessly.
  Add `--graphical` to enter the existing native shell after creation.
- Existing `--project`, GUI selector, single-writer lease, schema/UUID and atomic-publication
  contracts remain. Existing descriptors are rejected, never force-replaced. Unrelated existing
  documentation is retained. Creation does not generate a Zig game or attach a gameplay module.
- Argument conflicts, missing/duplicate creation/name options and an unavailable graphical build
  are rejected before creation. Runtime filesystem/indexing/report/display failures return nonzero;
  later indexing/display failure does not claim to roll back an already created workspace.
- Windows uses `wmain` and explicit UTF-8 filesystem conversion. Headless JSON escapes quoted,
  backslash and control-bearing valid names. The public engine C ABI is unchanged.

## Executed locally

- Full `linux-development` configure/build/CTest: **297 registered, 281 passed, 16 existing
  native-prerequisite skips, 0 failures; 211.43 seconds**. `editor.project_cli` ran and passed.
- The actual headless Editor CLI fixture independently ran **8 tests**, passing in both ordinary
  and optimized Python. It covers Unicode/spaces in roots, names, report and recent paths; valid
  JSON escaping; UUID-preserving reopen/read-only access; creation alongside existing docs;
  duplicate creation rejection without descriptor/workspace replacement; argument conflicts;
  invalid names; unusable roots; missing-project open; and graphical-off rejection before creation.
- Full Monolithic `linux-shipping` configure/build passed. That preset disables Editor; this is
  Runtime/Shipping regression evidence, not a claim that a Shipping Editor was launched.
- Windows GNU object cross-compilation of the real headless Editor translation unit passed,
  including `wmain`. This is compile-only, not an MSVC link or native Windows execution.
- Touched C++ clang-format, Python syntax compilation, local Markdown links and `git diff --check`
  passed. Root README was not changed; the relevant English/Traditional Chinese Editor roadmap
  pair records only the scoped capability, leaving ED-M0/ED-M1 and 0/8 milestone status unchanged.

The 16 local skips require unavailable Slang/XTEST/native Vulkan prerequisites. The local build
has the graphical shell disabled. `editor.linux_project_cli` is registered for the native
Xvfb/graphical configuration and creates/reopens a real eight-frame window, retaining both output
streams and rejecting Vulkan validation errors. It was **not run on this local configuration**;
its hosted result must be checked separately. Windows CJK CLI behavior and physical Windows
display/input also require their respective hosted/operator evidence.

## Reproduction

Toolchain: CMake 3.31.10, Ninja 1.13.2, GCC 14.2, Zig 0.14.0, Python 3.12.14. SDK overrides below are
local paths, not committed presets or required game-runtime dependencies.

```sh
source /workspace/scratch/nexora-sp05/build.env
cmake --preset linux-development \
  -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_ZIG_EXECUTABLE=/workspace/scratch/nexora-readiness-20261023/toolchain/ziglang/zig \
  -DX11_X11_INCLUDE_PATH=/workspace/scratch/nexora-readiness-20261023/sdk/usr/include \
  -DX11_X11_LIB=/lib/x86_64-linux-gnu/libX11.so.6 \
  -DNEXORA_VULKAN_LIBRARY=/lib/x86_64-linux-gnu/libvulkan.so.1 \
  -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=/workspace/worktrees/nexora-sp01/build/linux-development/_deps/nexora_vulkan_headers-src \
  -DNEXORA_WINDOW_METRICS_XVFB_EXECUTABLE=/workspace/scratch/nexora-sp01/x11-tools/usr/bin/Xvfb
cmake --build --preset linux-development
CTEST_PARALLEL_LEVEL=2 ctest --preset linux-development --output-on-failure
python3 Tests/Editor/ProjectCliTests.py --editor build/linux-development/Apps/Editor/NexoraEditor --graphical-build=OFF -v
python3 -O Tests/Editor/ProjectCliTests.py --editor build/linux-development/Apps/Editor/NexoraEditor --graphical-build=OFF
cmake --preset linux-shipping -DNEXORA_ENABLE_2D=ON -DNEXORA_ENABLE_ZIG_GAMEPLAY=ON \
  -DNEXORA_ZIG_EXECUTABLE=/workspace/scratch/nexora-readiness-20261023/toolchain/ziglang/zig \
  -DX11_X11_INCLUDE_PATH=/workspace/scratch/nexora-readiness-20261023/sdk/usr/include \
  -DX11_X11_LIB=/lib/x86_64-linux-gnu/libX11.so.6 \
  -DNEXORA_VULKAN_LIBRARY=/lib/x86_64-linux-gnu/libvulkan.so.1 \
  -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=/workspace/worktrees/nexora-sp01/build/linux-development/_deps/nexora_vulkan_headers-src
cmake --build --preset linux-shipping --parallel 2
```

For Windows GNU compile-only validation, use `zig c++ -target x86_64-windows-gnu -std=c++20 -c`
with the actual Editor include paths from `compile_commands.json` and an external output object.
The first attempt lacked the configured writable Zig cache and reported `ReadOnlyFileSystem`;
after sourcing the recorded build environment, normal object compilation succeeded. No source
warning or test requirement was suppressed.
