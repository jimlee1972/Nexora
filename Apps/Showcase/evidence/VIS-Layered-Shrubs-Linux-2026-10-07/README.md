# Layered olive shrubs: Linux evidence

Vessel-side shrubs now use four small original olive leaf cards per stack instead of two oversized cards, supported by narrow woody stems in the existing wood material slot. Only the two foreground stem index ranges are mirrored with their leaves; distant wood is excluded by a geometry-based room regression check. Leaf reflectance is (0.8, 0.8, 0.6), retaining the authored olive source texture, zero emission, alpha cutout, two-sided PBR, wind, transmission, shadows and planar reflection. Source PNGs, HDR radiance, skybox, IBL, shader packets and animation clocks are unchanged.

Full Linux configure/build and 102/102 tests pass with Khronos core/synchronization validation. Shipping isolated native acceptance, exact wind/reflection restoration and a real 100-second tour pass. Source freeze `d95b337dd41ffe42f736bc3bb91428f0f211cd1d`. Native art, movie and package use the same frozen Shipping executable. Logs, source/executable/movie/package hashes and actual selected-device observations are retained.

Standard: 60865 vertices, 140070 indices, 393 batches, 46 materials. Reference parity and physical-display acceptance remain open (VIS 5/7).
