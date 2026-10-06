# Olive foliage reflectance: Linux evidence

The courtyard leaf material uses olive reflectance factors (0.42, 0.52, 0.30), preserving existing ambient light, roughness, texture/alpha, two-sided lighting, wind and warm transmission. Geometry, materials, texture bytes, shader packets and animation clocks remain unchanged. The earlier over-dark ambient-occlusion prototype is excluded from this source freeze.

Full Linux configure/build and 97/97 tests pass with Khronos core/synchronization validation (119.89 s). Shipping isolated native acceptance, exact wind/reflection restoration and an actual 100-second shared-clock recording pass (100.93 s wall time). Standard retains 57,961 vertices, 134,982 indices, 384 batches and 44 materials. Production freeze `db4d3f38de133ec6a22e4d142cac7ea6e100ac5b`; native art uses the same Shipping executable. Source/executable/movie/package SHA-256 provenance is retained. Artifacts: `NexoraShowcase-Olive-Foliage-db4d3f3.mp4` and `NexoraShowcase-Olive-Foliage-db4d3f3-Linux.zip`.

Reference parity and physical-display acceptance remain open (VIS 5/7).
