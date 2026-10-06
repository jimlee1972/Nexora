# Spatial HDR anti-aliasing: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (106.93 seconds, no skips).
All 93 native PBR frames pass with Khronos core/synchronization validation enabled.
The diagonal emissive fixture changes from 0 to 191 intermediate edge pixels; disabling
restores the original scene-region hash, and a constant white interior remains unchanged.
Shipping/Full packaging and isolated native F10 changes/exact restoration pass. The same
executable records an actual 100-second wind/animation tour without overlays or retiming.

Production source freeze is `ba94ca58b1aa31b767eed90f6896073b0c70f898`; source, executable, movie and package hashes
are retained in `release-provenance.json`. The shared spatial filter adds no temporal
history, target, descriptor or production readback. Public C++ consumers rebuild for the
new opt-in boolean and 64-byte tone packet; stable C/Zig and persistent asset schemas
are unchanged. Basic omits the filter. Native fixture pixels and paused native art shots
are retained separately from the moving tour.

Workspace artifacts: `NexoraShowcase-HDR-Anti-Aliasing-ba94ca5.mp4` and `NexoraShowcase-HDR-Anti-Aliasing-ba94ca5-Linux.zip`.
Software Vulkan/Xvfb evidence does not accept physical target performance or final reference
parity. VIS stays 5/7.
