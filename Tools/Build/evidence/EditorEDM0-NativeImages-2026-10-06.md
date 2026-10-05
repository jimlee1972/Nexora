# ED-M0 native UI image registration and cache retirement

Date: 2026-10-06 (Asia/Taipei). Source: `codex/ed-m0-native-images`, based on
`51996e312121681446fb9b5e7da7fa8b905663c9`; this record is committed with the implementation.
This is automated Linux/Vulkan evidence, not physical-display or installed-IME acceptance.

## Changes and reproduced gaps

The native `Render(RenderSurface&)` path previously uploaded only the font atlas and forwarded
arbitrary ImGui texture IDs without a native registration entry point. The public-RHI registry
cannot supply an image to a surface's private device. `RegisterNativeTexture` now copies tightly
packed linear RGBA8 input, limits each image to 1024x1024, and permits 64 live images / 16 MiB.
Registration shares the host's monotonic public generation namespace; zero means invalid input
or exhausted capacity. Stale/public-RHI IDs receive the diagnostic font fallback before native
binding. Native backend keys are bounded cache slots, rather than every historical generation.
CPU copies live until unregister/host destruction; a GPU cache slot lives until replacement/drain.

DX12's 4096-entry descriptor heap previously consumed a new index permanently on every upload.
Replaced UNORM and distinct sRGB indices now retire with the old resource and return to the free
list only after the frame fence completes. Reserved HDR indices never enter that free list.
Vulkan's fixed pool is 512 sets to fit live UI slots and protected replacement generations.

A stronger per-frame rejection assertion exposed another defect: Vulkan swapchain recreation
cleared sampled images/descriptors while `UiResourceDomain()` stayed unchanged. The host correctly
retained its upload acknowledgement, so subsequent frames silently skipped missing textures.
Resize now preserves the texture table, descriptor pool/layout and samplers after device idle;
only swapchain-dependent pipelines, passes and frame resources are rebuilt. Scene texture reuse
also survives resize. The existing native pixel test now requires four lifetime uploads instead
of seven and proves that the same red/green material pixels remain correct after resize.

## Native regression evidence

`editor.native_surface_lifetime` verifies copied caller pixels, stale-ID rejection/fallback,
owner replacement, DPI/font changes, resize, unchanged steady-state upload counts, and both
slot and byte limits. It registers 64 images and replaces the whole batch 66 times, issuing
**4290 texture uploads** to the replacement native surface. Every recorded frame must report
zero backend texture rejections. This exceeds DX12's heap capacity without growing historical
cache identities; Windows/DX12 execution still needs its final hosted CI result.

The original [surface lifetime record](EditorEDM0-SurfaceLifetime-2026-10-06.md) checked resize
identity/upload counts but did not assert texture rejection after resize. This follow-up closes
that coverage gap and fixes the cache loss it exposed.

## Validation

Environment: Debian 13.6, GCC 14.2, CMake 4.4.4, Slang 2026.18, Xvfb 21.1.16,
Mesa lavapipe 25.0.7 / LLVM 19.1.7 and Khronos validation layers 1.4.309.

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development`: passed.
- `ctest --preset linux-development`: **118/118 passed**, none skipped, 178.61 seconds.
- Strict validation command below: **4/4 passed, 24.18 seconds**. Logs show actual Khronos
  instance/device-layer insertion; no validation/synchronization error diagnostics occurred.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed.
  This is an engine build; Shipping policy disables the Editor and tests.
- Clang 19 `-Wshadow -Werror` syntax audit for the native test: passed.
- Changed Markdown links and bilingual documents validated; `git diff --check` clean.

```bash
export VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation
export VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT
export VK_LOADER_DEBUG=layer
ctest --preset linux-development -R '^(editor\.(linux_(display_acceptance|validation_output)|native_surface_lifetime)|window_presentation\.vulkan_scene)$' -V
```

An initial full run caught the obsolete scene upload-count expectation and one Editor close/cancel
save failure. The counter expectation was strengthened to require resize reuse; the Editor save
scenario passed in the isolated strict gate and final full suite without changing its assertions.
No Windows/DX12 descriptor-soak result, macOS/Metal result, physical text/DPI screenshot,
installed-IME composition or sanitizer result is claimed by this record. ED-M0 remains open.
