# Weathered closed masonry: Linux evidence

Bevelled block corners receive bounded deterministic displacements shared by duplicated UV/normal vertices. Geometric normals are recalculated from the deformed faces. The original 96-vertex/132-index topology remains; tests verify 24 physical boundary points, 66 edges used exactly twice, noncollapsed triangles and outward facing normals. Boxed stone and device details use the same geometry helper. No shader, ABI or original cooked art changes.

Full Linux configure/build and 97/97 tests pass with Khronos core/synchronization validation (109.78 s). Shipping isolated native acceptance, exact wind/reflection restoration and an actual 100-second shared-clock recording pass. Standard: 56,665 vertices, 133,194 indices, 336 batches, 44 materials. Production freeze `b88e22c7b24aa6f63b697783c0c9d64b241b135a`; native art uses the same Shipping executable. Source/executable/movie/package SHA-256 provenance is retained. Artifacts: `NexoraShowcase-Weathered-Blocks-b88e22c.mp4` and `NexoraShowcase-Weathered-Blocks-b88e22c-Linux.zip`.

Reference parity and physical display acceptance remain open (VIS 5/7).
