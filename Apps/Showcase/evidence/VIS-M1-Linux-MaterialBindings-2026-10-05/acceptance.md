# VIS-M1 opaque material binding evidence

2026-10-05, Linux/X11/Vulkan, Mesa lavapipe software device, Xvfb 1280x720.
This accepts the material binding slice only. VIS-M1 and VIS-M2–M6 remain open.

- `cmake --preset linux-development`: passed.
- `cmake --build --preset linux-development --parallel 4`: passed.
- `ctest --preset linux-development`: 87/87 passed, no skips (32.84 seconds).
- `python Tests/Showcase/LinuxShowcaseInteraction.py build/linux-development/Apps/Showcase/NexoraShowcase`:
  retained independent native captures, all nine rooms, three courtyard cameras,
  UI hide/restore and exact wide-shot replay passed.
- Vulkan scene acceptance: 27 native submissions, independent material colors/textures,
  fallback white, immutable cache reuse, invalid slots/missing textures/alpha rejected without
  consuming the submission, direct/offscreen paths and existing affine/resize/teardown tests passed.
- Courtyard six-slot palette covers every index exactly once; other rooms preserve the legacy path.

Local SDK paths and cache overrides were environment-only. No module/link dependency was added.
`source-hashes.json` records the tested source bytes. `acceptance.json` and native captures record
actual execution. DX12 and Metal source paths and Metal native tests are included, but their
execution is delegated to PR CI and is not claimed as Linux execution. No PBR, IBL, HDR, hardware
frame budget or final art acceptance is implied by these Lambert screenshots.
