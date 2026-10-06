# Detailed sky: Linux evidence

The original golden-hour panorama is cooked into six 256-square faces (768 by 512 RGBA8 atlas), replacing 128-square faces. Sky UVs, uploads and byte-size guards derive from the recipe face size. Original panorama and float HDR IBL payloads are unchanged; the LDR source is not measured HDR. Native HDR sun, key light, bloom and shared-clock animation remain active. Geometry and draw counts are unchanged.

The initial prototype exposed old fixed-size importer guards and failed; the retained failure and corrected room test document the repair. Full Linux configure/build and 97/97 tests pass with Khronos core/synchronization validation (122.80 s). Shipping isolated native acceptance, exact wind/reflection restoration and an actual 100-second shared-clock recording pass (101.06 s wall time). Standard retains 57,961 vertices, 134,982 indices, 384 batches and 44 materials. Production freeze `2b2be9d670c9c691e2f333ce0efa2821c9539f78`; native art uses the same Shipping executable. Source/executable/movie/package SHA-256 provenance is retained. Artifacts: `NexoraShowcase-Detailed-Sky-2b2be9d.mp4` and `NexoraShowcase-Detailed-Sky-2b2be9d-Linux.zip`.

Reference parity and physical-display acceptance remain open (VIS 5/7).
