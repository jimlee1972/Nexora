# VIS-M5 Linux native tour evidence

Development configure/build/full CTest: 97/97 PASS, 39.33 seconds, no skips. Shipping configure/build/package verification passes. Native interaction verifies actual free-camera translation and exact fixed-camera restoration, in addition to living-effect comparisons and pause/replay. XImage bulk decoding is checked against independent native XGetPixel samples.

The retained 100-second video captures the actual Vulkan window at 1280×720, 15 fps, with no overlay or mouse cursor. `recording.json` identifies executable/video hashes and application build identity. The app was built from base 270d46ff with the changes identified in source-hashes.json; that base build identity alone does not describe uncommitted source.

Scope: Xvfb / lavapipe software GPU, not physical Windows target-hardware acceptance. No GPU timing or GTX 960 performance claim. Offline ffmpeg is an evidence tool only.

Living effects PR #327 passed all 18 jobs in run 37288122626 at head 9040f0c7644db448f5c0e819dc7d1fc15f315f3d and merged as 38a02b28707e2c4727084f557372961b4037b43b.
