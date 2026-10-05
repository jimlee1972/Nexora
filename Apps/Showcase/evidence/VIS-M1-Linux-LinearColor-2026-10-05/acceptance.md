# VIS-M1 hardware sRGB color filtering evidence

2026-10-05, Linux/X11/Vulkan, Mesa lavapipe software device, Xvfb.
This accepts Linux linear color filtering only; VIS-M1 and VIS-M2–M6 remain open.

- `cmake --preset linux-development`: passed with the Slang-enabled cache.
- `cmake --build --preset linux-development`: passed.
- `ctest --preset linux-development`: 92/92 passed, no skips; see the retained full log.
- Native Vulkan PBR pixels: black/white midpoint base color and emission match linear 0.5 factors;
  ORM remains linear and legacy Lambert retains UNORM filtering. Existing normal, mirrored model,
  ACES/display-transfer, invalid descriptor, frame reuse and resize cases still pass.
- Native room interaction: all nine rooms, three courtyard cameras, UI hiding, PBR/Lambert comparison
  and exact camera/comparison restoration pass under the full gate.

The shared shader now consumes hardware-decoded linear colors. Vulkan mutable images, DX12 typeless
resources and Metal pixel-format views select sRGB only for PBR base/emission. Paired views share
immutable generation and fence lifetimes. Vulkan texture candidates allocate completely before
recording copy commands, and failure releases candidate storage without publishing the generation.
The retained midpoint capture contains the two equal gray material patches.

Windows/Metal execution requires the PR CI; local execution was Linux only. Floating-point HDR,
IBL, final art, physical display acceptance and GTX 960 performance acceptance remain pending.
Three sequential 360-frame native benchmark runs (60 warm-up, 300 retained samples) report
48.50, 47.29, 48.23 FPS. Own builds/tests were idle during capture. These are
software-driver results and do not establish the GTX 960 hardware budget. Shader and production
sources were unchanged between benchmark capture and the final test-oracle correction.

The Vulkan pixel oracle requires a unique calibrated emission marker for each submitted frame
before comparing material patches. This prevents accepting stale X11 presentation pixels;
all 13 frames passed, including midpoint base/emission and linear ORM/legacy checks.
The final full gate took 38.90 seconds.
