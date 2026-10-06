# Courtyard foreground camera framing: Linux evidence

The wide preset uses radius 9/pitch 0.30 instead of radius 8/pitch 0.20, raising the absolute orbit eye and revealing more actual paving, stairs and puddles. Secondary presets use pitch 0.28/0.25; guided-tour start/end use 0.30 and the 65-second point uses 0.28. The existing orbit target, deterministic B/R replay, mouse orbit, free camera, shared animation clock and all native rendering features remain intact. Geometry, cooked assets, shaders and stable ABIs do not change.

Full 97/97 Development tests pass with Khronos core/synchronization validation (110.65 s), including the inherited MSVC rivet-loop naming correction. Native isolated Shipping acceptance proves fixed-shot/replay, wind/planar-reflection comparisons and exact pixel restoration. An actual 100-second shared-clock tour, screenshots, full logs and source/executable/movie/package SHA-256 provenance are retained here. Production freeze: `47675e7db75b669398368c902928c52866820a28`. Artifacts: `NexoraShowcase-Camera-Framing-47675e7.mp4` and `NexoraShowcase-Camera-Framing-47675e7-Linux.zip`. This fresh release includes the corrected rivetRadius identifier. No overlays or retiming.

Reference parity and physical-display performance remain unaccepted. VIS remains 5/7.
