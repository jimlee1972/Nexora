# Sunset alignment: Linux evidence

The HDR sun and key direction are (-18, 4, -19.2), aligned with the panorama azimuth. The sun center is planned near pixel (208, 175) in the paused wide camera; native screenshots verify visibility. The left arcade and all attached stems/leaves move from X=-7.5 to X=-9 to clear the stone-column occlusion. The first direction-only prototype remained occluded and was not adopted. Solar/key radiance remain 12/8/3 and 5.5/3.6/1.8; the float diffuse/specular IBL is rebaked consistently, while the BRDF LUT and original image sources remain unchanged. Wind roots, animation clock, camera, bloom and native shadow settings remain active.

Full Linux configure/build and 97/97 tests pass with Khronos core/synchronization validation (121.07 s). Shipping isolated native acceptance, exact wind/reflection restoration and an actual 100-second shared-clock recording pass (101.23 s wall time). Standard retains 58,249 vertices, 135,378 indices, 387 batches and 44 materials. Production freeze `899958d93d53819bc169a1c04520e77b43a8774f`; native art uses the same Shipping executable. Source/executable/movie/package SHA-256 provenance is retained. Artifacts: `NexoraShowcase-Sunset-Alignment-899958d.mp4` and `NexoraShowcase-Sunset-Alignment-899958d-Linux.zip`.

Reference parity and physical-display acceptance remain open (VIS 5/7).
