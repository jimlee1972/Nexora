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
The initial local validation above establishes no target-host result. Hosted desktop results
are recorded separately below. Physical text/DPI screenshots, installed-IME composition and
sanitizer acceptance remain open; ED-M0 remains open.

## CI queue follow-up

The native implementation is commit `b20973f5456415426824bf5d0970a101e9d47f74`.
A workflow-only follow-up shares concurrency groups between ED-M0 push/PR events and performs
best-effort cancellation of older commits on four explicitly named ED-M0 branches. It preserves
current heads, its own run, completed runs, main and fork repositories; a ref advancing during
cleanup stops that branch's cancellation. Other branches keep independent concurrency groups.
Only the documentation job receives Actions write permission; fork cleanup is disabled.
Local validation: 16/16 documentation routing tests, workflow YAML parsing, and a mocked JavaScript
guard audit all passed. Hosted cleanup execution and final cross-platform CI are still pending.
## Hosted desktop and fixture follow-up

At immutable source `6884c7c08e2bf45b1384b8b3d3a20c4c729faece`,
[Build run 37371576645](https://github.com/jimlee1972/Nexora/actions/runs/37371576645)
records these distinct results:

- [macOS/Metal Development](https://github.com/jimlee1972/Nexora/actions/runs/37371576645/job/111995856982):
  **122/122 passed**, 84.97 seconds. The native lifetime gate passed in 7.24 seconds and
  reports copied pixels, stale fallback, owner/DPI/resize behavior, and **4290 uploads**.
- [Windows/DX12 Development](https://github.com/jimlee1972/Nexora/actions/runs/37371576645/job/111995856950):
  the context lifetime, DPI/IME callback contract, and native surface/image gate passed;
  the latter completed in 7.64 seconds and reports **4290 uploads**. The full suite failed
  11 unrelated fixture cases, so this is native image/descriptor-soak evidence rather than
  whole-job acceptance.
- Linux Development passed 139/140; its display fixture did not reliably focus the Scene canvas
  before the Hierarchy creation shortcut. The failure remains recorded rather than retried away.

Repair commit `92b3293` moves temporary-directory cleanup after all file/workspace owners,
uses binary canonical scene writes instead of Windows CRLF conversion, and explicitly focuses
Scene before the Linux shortcut. Existing behavioral assertions remain required.
After synchronizing main `f5a1e6ae9167b9487c45644b3a673d13b714da7b`, the repaired native image
source passed full Linux Development **118/118** without skips (187.37 seconds), strict Vulkan
**4/4** (24.44 seconds, actual layer insertion and no error diagnostics), and Shipping engine
configure/build. Refreshed final-head hosted CI is still required before merging.

## Final-head automated acceptance

Source `75e10f02addf47959401c0c15743a25b8f78a26a` integrates main `85e94f6`.
[Build run 37408634636](https://github.com/jimlee1972/Nexora/actions/runs/37408634636)
passed all 18 selected jobs, including CI result, with no failed or skipped selected jobs.

| Hosted Development gate | Result | Duration |
| --- | --- | --- |
| Linux/Vulkan | 140/140 passed | 287.52 s |
| Windows/DX12 | 123/123 passed | 135.44 s |
| macOS/Metal | 122/122 passed | 66.37 s |

Each desktop native lifetime gate reported **4290 uploads**, copied pixels, stale fallback,
owner/DPI/resize reuse and bounded slots/bytes. The Windows full-suite fixture failures above
are historical; the final head passed. Sanitizer/build-contract, all mimalloc jobs, dedicated
Linux display/synchronization validation and isolated Shipping package jobs also passed.
These are hosted results, not Windows/macOS runs in the Linux development workspace.

Exact-source local validation: Linux Development configure/build and **118/118**, no skips,
182.02 s; strict Vulkan **4/4**, 25.38 s, actual Khronos layer insertion and zero validation
errors/VUIDs/synchronization hazards; engine-only Shipping configure/build passed.
Shipping/Full package configure/build and `Tools/Package/VerifyShowcaseRelease.py` passed
with Zig 0.14.0 and isolated native acceptance PASS. Full commands are recorded in
[PR #354](https://github.com/jimlee1972/Nexora/pull/354).

The shared fixtures release workspace/file owners before temporary-directory cleanup, use
binary canonical scene writes and hold native shortcuts across GUI frames. Upstream Lab-export,
presented-resize and held-input synchronization remains required. No behavioral/pixel assertions
were removed. Physical Linux display, Windows monitor-DPI/installed-IME composition and visual
legibility/glyph coverage remain open; **ED-M0 and graphical milestones remain 0/8**.
