# Two-sided thin-sheet lighting: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (106.20 seconds, no skips).
All 89 native PBR frames pass with Khronos core/synchronization validation enabled.
Four sheet fixtures preserve default back-face behavior and reproduce front-facing colors
exactly when enabled; CPU rejects unlit/Lambert use and verifies reserved slot 79.
Shipping/Full packaging and checksum-verified isolated native interaction pass. The same
executable records an actual 100-second wind/animation tour without overlays or retiming.

Production source freeze is `248791c4b51a7b0329a2798e8dae12129e87a618`; source, executable, movie and package hashes
are retained in `release-provenance.json`. Optional twoSidedLighting orients the geometric
normal before tangent normal mapping, BRDF and IBL. Leaves and pennants opt in; source-world
shadows and virtual mirror cameras retain their behavior. Closed-glass front-facet filtering
precedes the normal flip. This is sheet lighting, not volumetric scattering. The native
400-byte packet, backend bindings and stable C/Zig ABI remain unchanged.

Workspace artifacts: `NexoraShowcase-Two-Sided-248791c.mp4` and `NexoraShowcase-Two-Sided-248791c-Linux.zip`.
Software Vulkan/Xvfb evidence does not accept physical target performance or final reference
parity. VIS stays 5/7.
