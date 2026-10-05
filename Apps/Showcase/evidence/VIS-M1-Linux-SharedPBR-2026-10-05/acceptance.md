# VIS-M1 shared direct-light PBR evidence

2026-10-05, Linux/X11/Vulkan, Mesa lavapipe software device, Xvfb 1280x720.
This accepts the direct-light PBR slice only; VIS-M1 and VIS-M2–M6 remain open.

- `cmake --preset linux-development` (Slang enabled cache): passed.
- `cmake --build --preset linux-development --parallel 4`: passed.
- `ctest --preset linux-development`: 91/91 passed, no skips (36.97 seconds).
- `cmake --preset linux-shipping`: passed.
- `cmake --build --preset linux-shipping --parallel 4`: passed, Monolithic linkage.
- `cmake --build --preset linux-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4`:
  passed, checksums and isolated Shipping launch; this package launch uses headless scope.
- `python Tests/Showcase/LinuxShowcaseInteraction.py build/linux-development/Apps/Showcase/NexoraShowcase`:
  retained three courtyard shots, PBR/Lambert comparison, exact PBR restoration, exact fixed-camera
  replay, overlay toggling and all nine rooms passed.
- `python Tests/Showcase/LinuxCourtyardBenchmark.py build/linux-development/Apps/Showcase/NexoraShowcase --output Apps/Showcase/evidence/VIS-M1-Linux-SharedPBR-2026-10-05/benchmark`:
  three sequential 360-frame native runs, 60 warm-up and 300 retained samples each; no concurrent
  own builds/tests. The reports record actual shading, present mode, CPU, driver selection,
  process-wide CPU time and peak RSS. GPU timing and refresh remain unavailable.

Native PBR pixels verify emission-map colors and calibrated ACES/sRGB output, normal-map lighting,
ORM changes, single color-map decoding, white/flat missing-map defaults, mirrored tangent handedness,
invalid descriptors, frame-slot reuse, direct/offscreen paths and resize/re-upload. CPU tangent
cases verify UV axes, mirrored islands, degenerate UV fallback and invalid geometry. Exact affine
CPU determinant signs avoid rounded GPU cancellation. Pinned shader regeneration passes for
SPIR-V/HLSL/MSL, with no runtime Slang dependency and one Vulkan descriptor set per material.

The three software runs averaged 46.051, 45.008, 45.201 FPS.
They do not establish GTX 960 or physical display acceptance. Tested source hashes are retained.
Windows courtyard PBR/comparison captures and Metal native PBR cases are included for CI; their
execution is not claimed as local Linux execution. No module dependency was added. Public C++
consumers rebuild; Runtime mesh serialization and stable C/Zig ABI are unchanged.

RGBA8 targets and manual color decoding after sampling are the current boundary. Hardware sRGB
views/linear filtering, IBL, floating-point HDR, final reflections/art, shadows, motion, tour and
final performance/visual acceptance remain open.

MSVC portability follow-up: Windows CI rejected three implicit int-to-float ternaries under
`/WX`. Explicit float constants preserve the exact values and byte packing. The full Linux
Development gate passed 92/92 without skips after this correction (36.90 seconds); see
`ctest-msvc-portability.log` and the two updated header hashes in `msvc-portability-hashes.json`.
Original captures and benchmark artifacts retain their original source hashes.
