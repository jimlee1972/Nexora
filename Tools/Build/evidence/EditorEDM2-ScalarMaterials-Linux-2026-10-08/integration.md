# Integration after main PR #443

Date: 2026-10-08. This supplements the [original scalar-material acceptance](acceptance.md);
it does not replace its 142-test result or original capture/source provenance.

- Feature commit: `6f9c935fa07dde0770baca24a7d84e7d8c243422`, tested originally against main `06b11369`.
- Integrated main: `b966f895a30cf71485051edaad9fb80faa68c14f` (measured live process resident memory, PR #443).
- Tested merge commit: `faae87ec9f468ef8fde0170dc372c8b6e830724e`.
- [Integration source and external-log SHA-256 values](integration-source-hashes.json) cover 34 affected
  source/CMake/module-graph files, including upstream memory sources. Original source hashes match
  the immutable feature commit; original red/green and blue/green captures are unchanged.

The merge was clean. Material, process-memory and atomic Inspector targets are all retained.
No implementation or test changes were made between integration and final validation. Memory UI
semantics and its separate source blocks are unchanged from main. Touched C++ passes clang-format
19.1.7 dry-run checks; final diff whitespace checks pass.

## Required gates

The same external tools/dependency cache as the original acceptance were used. Development explicitly
configured graphical shell and Slang ON, with Modular linkage and Development mode:

```bash
cmake --preset linux-development -DNEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL=ON -DNEXORA_ENABLE_SLANG=ON
cmake --build --preset linux-development -j 2
ctest --preset linux-development
```

✅ Configure succeeded and all **206 incremental build steps passed**. The final full suite passed
**146/146, zero skipped, 211.90 seconds**, including the four new upstream process-memory tests,
material import/catalog/Inspector/native tests and atomic Inspector batch coverage.

The first integrated full-suite attempt passed 145/146 in 229.39 seconds: the existing
`editor.linux_scene_files` timed out while checking that Open adopted the live document/save path.
Its original failure log is retained. With all other team builds/native gates paused, the unchanged
case was rerun in isolation:

```bash
ctest --preset linux-development -R '^editor.linux_scene_files$' -V
```

✅ **1/1 passed, 37.73 seconds**. The subsequent complete serialized suite produced the 146/146
result above, with the same scene-files test passing in 37.83 seconds. These results do not establish
the cause of the original timeout, and no timing, assertion or acceptance condition was weakened.

Shipping was then configured/built sequentially:

```bash
cmake --preset linux-shipping -DFETCHCONTENT_SOURCE_DIR_NEXORA_VULKAN_HEADERS=/workspace/Nexora/build/linux-development/_deps/nexora_vulkan_headers-src
cmake --build --preset linux-shipping -j 2
```

✅ All **7 incremental build steps passed**, including the upstream Core change. This remains
Shipping/Monolithic/Minimal with Editor, Editor SDK, graphical shell, Slang and testing OFF.
It verifies Editor-independent engine linkage; it does not validate a shipped material consumer.

Finally, the material native gate ran with Khronos core and synchronization validation:

```bash
export VK_LAYER_PATH=/tmp/nexora-ed-beads-p1p7a0jw/sysroot/usr/share/vulkan/explicit_layer.d
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
export VK_LOADER_DEBUG=layer,driver
export MESA_SHADER_CACHE_DIR=/tmp/nexora-ed-materials-mesa-cache
ctest --preset linux-development -R '^editor.material_scene_native$' -V
```

✅ **1/1 passed, 0.30 seconds**. Loader stderr attests insertion of
`VK_LAYER_KHRONOS_validation` and the software `llvmpipe (LLVM 19.1.7, 256 bits)` device using
`libvulkan_lvp.so`. A post-run check found zero `Validation Error`, `VUID-` or `SYNC-HAZARD-` records.
Real source import, independent PBR colors, reimport pixels, rejected versions and Undo/reopen
remain covered. Physical GPU/display and other target-host evidence remain open.

## Latest-main compatibility

After these gates completed, main advanced to `f21620387e5b27dfb9cce9b341100bb516cc8708`
(bounded Console admission, PR #444). A read-only `git merge-tree --write-tree HEAD origin/main`
compatibility check returned exit 0 without conflicts. That newer main was not merged into this
locally tested source; the 146-test result above belongs to the explicit tested merge commit.
The PR merge CI remains responsible for validating the combined latest-main tree.

## Retained external logs

All paths below are managed-cloud validation artifacts, not generated files committed to the repository:

- `/tmp/nexora-ed-materials-integration-configure.log`
- `/tmp/nexora-ed-materials-integration-build.log`
- `/tmp/nexora-ed-materials-integration-ctest.log` (original failed attempt)
- `/tmp/nexora-ed-materials-integration-scene-files-isolated.log`
- `/tmp/nexora-ed-materials-integration-ctest-serial.log` (final full pass)
- `/tmp/nexora-ed-materials-integration-shipping-configure.log`
- `/tmp/nexora-ed-materials-integration-shipping-build.log`
- `/tmp/nexora-ed-materials-integration-validation.log`
- `/workspace/Nexora-ed-m2-materials/build/linux-development/artifacts/editor-material-scene-native/stdout.log`
- `/workspace/Nexora-ed-m2-materials/build/linux-development/artifacts/editor-material-scene-native/stderr.log`

All ED milestone exit boxes remain unchecked. This integration adds no material features and does
not claim full ED-M2, persistent GPU residency, texture/shader editing or Game View materials.
