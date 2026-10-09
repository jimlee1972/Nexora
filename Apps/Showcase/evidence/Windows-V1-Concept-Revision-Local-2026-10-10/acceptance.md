# Windows courtyard concept revision candidate

This candidate responds to the operator's request to revise composition, lighting/color,
crystal, foliage and ground toward `Roadmap/art/V1-Visual-Identity-Concept.png`. It is an actual
native scene revision with the retained original assets, not a rendered concept-image substitute.
Final visual approval and a stable target-hardware frame-time budget remain open; VIS stays 5/7.

The wide camera is lower and farther away. The 100-second visual tour starts and ends with
that same framing. Foreground banks move into the bottom corners, with smaller hanging/rim
leaves and unchanged leaf/vertex budgets. Stone uses finer world-space weathering, restrained
normal amplitude and cooler base factors under the sunset. Bronze roughness/tint is more muted.
The crystal shell is narrower; its contained faceted mineral core fills more of the shell,
with bounded 1.46 refraction, reduced thickness and a stronger cyan transmission tint.
The sun-to-IBL ratio, atmosphere and shadow tint retain sunset depth. The color-grade contrast
is 0.97 to preserve dark foliage detail after ACES instead of clipping it to black.

Both retained 1280x720 fixed views run 600 real native frames on the GTX 960 with static Zig,
paused/activated courtyard, no overlay, no software renderer and no backend fallback. The actual
JSON reports and source hashes are retained with the images. They are previews, not a full
physical interaction/tour acceptance gate or stable FPS proof. The DX12 preview averages 61.07
FPS (p99 18.88 ms); the official repeated quality matrix remains separate.

Two earlier art iterations were inspected and rejected locally: excessive IBL flattened the
sunset; increased contrast clipped leaf shadows. Each earlier iteration has actual complete
100-second native DX12/Vulkan H264 recordings stored under the local
`build/v1-windows-local-2026-10-10/` directory. Those movies belong to different executable
hashes and are not final evidence for this corrected candidate. No large movie is committed.

The first comprehensive revision passed the full Windows Development gate, 122/122 in
229.79 seconds, including convex-shell containment and the adjacent-material native pixel fixture.
The corrected candidate's full configure/build/CTest gate passes 122/122 in 249.24 seconds. Shipping/Monolithic package and isolated
headless verification pass with 51 manifest entries. Exact corrected-candidate final Linux CI,
physical interaction/tour checks, repeated frame-time matrix and final same-version movies
remain required. This document does not claim a Linux Codex Cloud run.

Exact full Windows gate commands:

```powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --config Development --parallel 4
ctest --preset windows-showcase-development --parallel 4 --output-on-failure
```

Normalized complete corrected-candidate logs are retained beside this record. The same source
also passes native DX12 PBR image comparisons, including the split adjacent-material receiver;
no tests are skipped. Documentation link/bilingual validation reports zero errors. Repository
root README remains concise and unchanged by this supporting iteration.
