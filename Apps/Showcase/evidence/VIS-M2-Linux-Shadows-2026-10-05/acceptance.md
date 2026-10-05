# VIS-M2 Linux directional shadows — 2026-10-05

Linux Development configure/build/full CTest passed 95/95 without skips (49.58 seconds).
Shipping Monolithic configure/build and NexoraShowcasePackageShippingEvidence passed checksum
verification and isolated headless launch. Native visual acceptance is retained separately.

The actual courtyard records a 1024² directional prepass into protecting-frame R32Float color
and D32Float visibility targets. Indexed geometry and instances generate normalized light-space
depth; main applies shared four-tap PCF and bounded normal/slope bias. Shared ramp and tint
separate warm light and cool shadows; emission remains independent. Native transitions and fences
protect maps across main sampling, frame reuse, resize and shutdown. The private material packet
is 208 bytes and main uses eight sampled maps. No stable C/Zig or persistent schema change.

The native pixel oracle presents 30 frames: existing materials/IBL/HDR plus caster movement in X/Y,
partial PCF edges, shadow resolution/reuse, camera/resize changes, disabled shadows and a styled
light tint. Supplemental native stdout records the real oracle. The retained PPM remains the prior
material-filtering diagnostic frame; courtyard screenshots demonstrate the new shadow route.
Matching DX12/Metal source requires exact-head CI; Linux does not prove their execution.

Nine-room interaction captures retain fixed wide/material/motion framings, F6 shadow and G
styled/neutral comparisons with exact hash restoration, P/O/E comparisons, replay and resize.
Reviewed native wide and shadow-off captures show the new architectural/device shadows. Their
engineering geometry remains provisional, and final physical route review is not established.

Three sequential fixed-wide 360-frame runs (60 warmup, 300 measured samples), with own builds/tests
idle, measured 25.72, 24.37 and 26.79 FPS on software lavapipe. Reports retain frame timing/memory,
actual shadow counters, enabled style/shadow flags, bias, RGBA16F and exposure. This does not accept
the GTX 960 hardware budget. Final art, bloom, animated foliage, visual tour and quality optimization
remain subsequent milestones. VIS-M2 awaits cross-platform CI and route acceptance.

Build 1432 passed native Metal and Windows Vulkan package replay. MSVC rejected three unused shadow-fixture declarations that hid the owning loop fixture; those redundant declarations are removed. The DX12 package's final camera replay capture sampled a previous camera after the fixed delay; it now waits up to five seconds for the actual exact baseline GPU image, retaining equality and failing at the deadline. All earlier shadow/style/material/exposure restoration comparisons passed. Linux repeated the full gate after integrating current Editor main changes; supplemental log/hash records accompany this correction. Corrected-head Windows CI remains required.
