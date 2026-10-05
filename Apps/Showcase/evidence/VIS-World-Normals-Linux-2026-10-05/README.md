# World normal projection: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (92.68 seconds, no skips).
All 69 native PBR frames pass, including flat/projected normal brightness, zero-strength
geometry restoration and exact projected-normal replay. Immutable flat/tilted maps use
separate generation IDs. Shipping/Full packaging and checksum-verified isolated native
interaction pass; the same executable records a real 100-second tour without overlays.

Production source freeze is `0eec7525b7ec220283a2ef608629b1638e85ca9a`; source/executable/movie/package hashes are retained
in `release-provenance.json`. Stone normal slopes share original world coordinates with
base/ORM and reflected geometry. Surface gradients are bounded, no extra texture/pass/packet
is allocated, and default mesh-UV behavior is preserved. Standard stone normal strengths are
0.35 for focal geometry and 0.2 for background/ground surfaces.

Workspace artifacts: `NexoraShowcase-World-Normals-0eec752.mp4` and
`NexoraShowcase-World-Normals-0eec752-Linux.zip`. Software Vulkan/Xvfb evidence does not accept
physical target performance, geometric displacement/refraction or final reference parity.
VIS remains 5/7.
