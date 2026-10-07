# Crystal mineral core gradient: Linux evidence

Existing inner mineral triangles are grouped by their original authored centroid height: lower faces use the brightest HDR material, middle faces the medium material, and upper faces dark teal. Each shade retains sixteen triangles. Vertex positions, normals, UVs, shell refraction, radiance constants, animation clock and material count are preserved. Lower radiance remains 1.8/1.35 before the existing shared pulse and bloom/ACES, with a darker upper silhouette.

Full Linux configure/build and 97/97 tests pass with Khronos core/synchronization validation (121.63 s). Shipping isolated native acceptance, exact wind/reflection restoration and an actual 100-second shared-clock recording pass (100.91 s wall time). Standard retains 58,249 vertices, 135,378 indices, 387 batches and 44 materials. Production freeze `5f9104f71677df263f45078e0b894a8e045170ad`; native art uses the same Shipping executable. Source/executable/movie/package SHA-256 provenance is retained. Artifacts: `NexoraShowcase-Crystal-Core-Gradient-5f9104f.mp4` and `NexoraShowcase-Crystal-Core-Gradient-5f9104f-Linux.zip`.

Reference parity and physical-display acceptance remain open (VIS 5/7).
