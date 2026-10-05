# VIS-M3 Linux hero art and HDR post-process — 2026-10-05

✅ Linux Development configure/build/full CTest passed 96/96 without skips (53.83 seconds).
Commands: `cmake --preset linux-development`, `cmake --build --preset linux-development`,
`ctest --preset linux-development`. Monolithic Shipping configure/build and
`NexoraShowcasePackageShippingEvidence` passed; isolated headless package proof is separate
from the native visual acceptance retained here.

The courtyard combines adopted CC0 KayKit architecture/palette and Poly Haven HDRI with original
Nexora art: segmented broken stone/bronze ring, diamond runes, a flat-shaded crystal, round plinths,
hollow ceramic profiles and a bounded sky. Source/license/derived hashes for the crystal and six
64×64 stone/bronze maps are cooked into the verified Runtime DAG (119 metadata, 120 crystal,
121–126 surface maps). Procedural architecture, vessels and sky remain CPU-authored scene geometry;
this does not claim they were imported or cooked. Vegetation remains static pending VIS-M4.

Native Main performs thresholded twelve-neighbor bloom in linear HDR before exposure/ACES, then
shared saturation/contrast grading and exactly one display transfer, followed by UI. This bounded
neighborhood filter is not a separable Gaussian or temporal pipeline. All adapters copy the same
32-byte private tone constants. Stable C/Zig/NXAB remain compatible; public C++ clients rebuild.

The native pixel oracle presents 34 frames. Four new fixtures prove bloom halo spreading, threshold
rejection, disable, grayscale grade and unchanged UI; the prior material/IBL/HDR/shadow cases remain.
`native/bloom-0.ppm` is disabled, `bloom-1.ppm` enabled, `bloom-2.ppm` threshold-rejected and
`bloom-3.ppm` grayscale. `native/scene.ppm` remains the older material-filtering diagnostic.
DX12/Metal execution requires exact-head PR CI and is not claimed by this Linux record.

Nine-room captures retain the three reframed courtyard shots and exact K/P/O/E/F6/G restoration,
including bloom and grade comparisons. Reviewed `interaction/courtyard-wide.png` shows the current
ring, crystal, ceramics, architecture, floor details and shadow composition. Resize/present accounting
now separately identifies genuinely recoverable Vulkan results instead of treating them as successful
presents. The verifier still requires acquired = successful + recoverable, rejecting other lost frames.

Benchmark details are retained in `benchmark/baseline.json`: three sequential 360-frame native runs,
60 warmup and 300 measured samples, fixed wide camera, no VSync, with our builds/tests idle.
The software lavapipe runs measured 15.0250992, 15.0581163, 14.9483666 FPS; these timings do not accept GTX 960 performance. GPU timestamps are unavailable and are
not substituted with whole-frame timing. Final target-hardware hero/close-up review remains open;
this accepts the Linux implementation slice, not the entire VIS-M3 milestone. Overall progress: 3/7.
