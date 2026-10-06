# Ceramic arcade inlays: Linux evidence

The twelve raised diamond borders on the six arcade pillars now surround filled ceramic diamonds, using existing material 3. Front and inner faces use outward winding and unit normals. Each inlay shares its four vertices and adds six indices; the stone border retains its original geometry. The native mirror includes the same ceramic batches. No new texture, material, shader packet, stable ABI or animation clock is introduced.

Full Linux configure/build and 97/97 tests pass with Khronos core/synchronization validation (119.52 s). Shipping isolated native acceptance, exact wind/reflection restoration and an actual 100-second shared-clock recording pass (101.22 s wall time). Standard: 56,713 vertices, 133,266 indices, 371 batches, 44 materials. Production freeze `2c7d68a0c8bfe4393f8051e4c94356e34b93cd46`; native art uses the same Shipping executable. Source/executable/movie/package SHA-256 provenance is retained. Artifacts: `NexoraShowcase-Arcade-Inlays-2c7d68a.mp4` and `NexoraShowcase-Arcade-Inlays-2c7d68a-Linux.zip`. The later clean-frame test synchronization change is validated separately and preserves this executable.

Reference parity and physical-display acceptance remain open (VIS 5/7).
