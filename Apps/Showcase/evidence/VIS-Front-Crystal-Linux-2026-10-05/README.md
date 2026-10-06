# Front crystal: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (97.13 seconds, no skips).
All 79 native PBR frames pass, including rear-facet rejection/default two-sided comparisons with Khronos core/sync validation.
Shipping/Full packaging and checksum-verified isolated native interaction pass. The same
executable records an actual 100-second animation tour without overlays.

Production source freeze is `572225c5fdb3a49c97ac05fd86d13c9378a6b563`; source, executable, movie and package hashes
are retained in `release-provenance.json`. Closed crystal meshes opt into front-surface-only refraction. Geometric normal/camera
checks reject rear facets; mirrored crystals use the virtual camera. Default panes stay
two-sided. The opt-in flag uses reserved packet offset 78; the 368-byte packet and stable
C/Zig ABI remain unchanged.

Workspace artifacts: `NexoraShowcase-Front-Crystal-572225c.mp4` and `NexoraShowcase-Front-Crystal-572225c-Linux.zip`.
Software Vulkan/Xvfb evidence does not accept physical target performance or final reference
parity. VIS stays 5/7.
