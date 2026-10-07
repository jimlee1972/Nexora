# Restrained rune and bronze materials: Linux evidence

Rune reflectance (0.02, 0.16, 0.17), emission (0.012, 0.32, 0.4) and roughness 0.22 preserve readable turquoise detail with restrained brightness. Aged bronze reflectance (0.8, 0.65, 0.4) and roughness 0.4 give the existing collars/rivets a more matte response. Metalness, detail textures, geometry, crystal HDR radiance, lighting, bloom and the shared activation pulse remain active. No materials or shader packets are added.

Full Linux configure/build and 97/97 tests pass with Khronos core/synchronization validation (123.56 s). Shipping isolated native acceptance, exact wind/reflection restoration and an actual 100-second shared-clock recording pass (100.91 s wall time). Standard retains 58,249 vertices, 135,378 indices, 387 batches and 44 materials. Production freeze `5a7c665266fe34d18ad33dc1804a41f600786e68`; native art uses the same Shipping executable. Source/executable/movie/package SHA-256 provenance is retained. Artifacts: `NexoraShowcase-Restrained-Runes-5a7c665.mp4` and `NexoraShowcase-Restrained-Runes-5a7c665-Linux.zip`.

Reference parity and physical-display acceptance remain open (VIS 5/7).
