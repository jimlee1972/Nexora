# Aligned golden-hour solar elevation: Linux evidence

The authored sun direction changes from (-18, 4, -19.2) to (-18, 6.5, -19.2), exposing clearer golden-hour light and shadow across the foreground paving, vessels and pedestal. Hero/sky and floating HDR diffuse/specular IBL are recooked from the same direction used by the native HDR solar disc, directional key and shadow projection. Original PNGs, radiance factors, BRDF LUT, animation clock and geometry budgets remain unchanged.

Full Linux configure/build and 102/102 tests pass with Khronos core/synchronization validation. Shipping isolated native acceptance, exact wind/reflection restoration and a real 100-second tour pass. Source freeze `be4fb04edf382692c5a5903f571c2afad5ae20a1`. Native art, movie and package use the same frozen Shipping executable. Source/executable/movie/package hashes and actual selected-device observations are retained.

Standard: 60865 vertices, 140070 indices, 393 batches, 46 materials. Reference parity and physical-display acceptance remain open (VIS 5/7).
