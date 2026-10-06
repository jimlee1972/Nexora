# Thin dielectric crystal and courtyard light: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (104.97 seconds, no skips).
All 97 native PBR frames pass with Khronos core/synchronization validation enabled.
Against one constant HDR background, legacy/normal/grazing/restored glass samples are
115/204/190/115 display codes; restoration also matches the scene-region hash exactly.
Shipping/Full packaging and isolated native comparison/restoration pass. The same
executable records an actual 100-second wind/animation tour without overlays or retiming.

Production source freeze is `4933fbc047c1ee224f46081fcd65a68f6a6bc427`; source, executable, movie and package hashes
are retained in `release-provenance.json`. The optional thin dielectric mode uses the
existing bounded HDR snapshot, IOR-based Schlick transmission and shared approximate
specular BRDF. It suppresses diffuse albedo/leaf transmission; uncovered pixels retain
the original unshifted background. Default legacy composition remains unchanged.
Public C++ clients rebuild for the new boolean; the copied flag uses material scalar 99,
retaining the 400-byte PBR packet, stable C/Zig ABI and persistent asset schemas.
This single-interface approximation has no volume transport, absorption distance,
internal reflections, recursive tracing or extra target.

Stronger existing IBL reveals foliage shadows. A closer/higher-aimed wide camera frames
the device, while stone and mineral-core radiance are restrained. HDR sun, local light,
skybox, bloom, shared-clock wind, rotation and hover remain active.

Workspace artifacts: `NexoraShowcase-Dielectric-Glass-4933fbc.mp4` and `NexoraShowcase-Dielectric-Glass-4933fbc-Linux.zip`.
Software Vulkan/Xvfb evidence does not accept physical target performance or final reference
parity. VIS stays 5/7.
