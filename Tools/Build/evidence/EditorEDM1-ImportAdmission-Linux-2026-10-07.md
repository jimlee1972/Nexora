# ED-M1 import admission: Linux evidence

Date: 2026-10-07 (Asia/Taipei). Base: `91d30c9a5bfa9e569f638516557f088f76a42977`.

The queue defaults to 64 retained operations. A new four-argument constructor permits a custom
capacity, normalizes zero to one, and preserves the existing exported three-argument constructor.
Both workspace and reimport admission reject a full queue before job submission or ID assignment.
Queued/running/unconsumed completed/failed/cancelled results retain capacity until `TakeResult`.
Tests cover mixed requests, retained queued/failed/cancelled results, 100 rejected retries,
100 readmission cycles, one-shot consumption, and shutdown. This bounds operation count, not
arbitrary workspace-index bytes or all application memory.

Validation environment: Linux x86-64, GCC 14.2.0, CMake 4.4.4, Ninja 1.13.2,
Slang 2026.18, Xvfb and Mesa lavapipe 25.0.7. Graphical shell and Slang enabled.
X11/Vulkan/Clang tools were extracted into an isolated local sysroot; no local tools/build outputs
are committed. The original constructor and new overload are present in the shared-library exports.

Final results:

- Development configure/build: passed.
- `ctest --preset linux-development`: **119/119 passed**, none skipped, 203.88 seconds.
- Shipping configure/build: passed (Minimal Monolithic, testing OFF by preset).
- `git diff --check`: passed.

Exact commands, after putting CMake/Ninja/Slang/Xvfb/xdotool/clang++ on PATH and their libraries
on LD_LIBRARY_PATH:

```bash
cmake --preset linux-development -DCMAKE_PREFIX_PATH=/workspace/Nexora/work/sysroot/usr -DCMAKE_CXX_FLAGS=-I/workspace/Nexora/work/sysroot/usr/include -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON
cmake --build --preset linux-development --parallel 4
VK_DRIVER_FILES=/workspace/Nexora/work/sysroot/usr/share/vulkan/icd.d/lvp_icd.json VK_LAYER_PATH=/workspace/Nexora/work/sysroot/usr/share/vulkan/explicit_layer.d VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT ctest --preset linux-development
cmake --preset linux-shipping -DCMAKE_PREFIX_PATH=/workspace/Nexora/work/sysroot/usr -DCMAKE_CXX_FLAGS=-I/workspace/Nexora/work/sysroot/usr/include
cmake --build --preset linux-shipping --parallel 3
```

An initial full run failed because clang++ was missing and the existing native Scene Open/Save
input test timed out. Rebuilding the unchanged import implementation from the base also reproduced
the same Open/Save timeout. A later diagnostic run passed without source changes, and the final
full run after installing clang++ and completing background builds passed every test. The initial
failures are not reported as successful validation.

No physical-display, Windows/macOS/mobile, installed IME, sanitizer or full graphical milestone
acceptance is claimed. ED-M0 through ED-M7 remain open.

Source SHA-256:

- `Engine/Editor/include/Nexora/Editor/AssetImport.h`: `029f6b441403a7beb4d260e94548ffd9af8c2c6c66f393e316d0b6437eccb131`
- `Engine/Editor/src/AssetImport.cpp`: `d14328f8baa20874445796bf1cb663a9726fb5a720303cd668438c87b3de1d4f`
- `Tests/Editor/EditorTests.cpp`: `86875bd1a1d0a5e4b308322996fe6ba7aa50b0638a96cc57cae26f8664fd3146`
