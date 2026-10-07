# Solar elevation and accepted-main integration: Linux evidence

Accepted main deab1c965704 is preserved, including all 18 PoseSearch/build/documentation paths; owned Showcase sources and the original be4fb04 evidence remain unchanged. General linux-shipping Full/Monolithic configure/build also pass. The authored sun direction changes from (-18, 4, -19.2) to (-18, 6.5, -19.2), exposing clearer golden-hour light and shadow across the foreground paving, vessels and pedestal. Hero/sky and floating HDR diffuse/specular IBL are recooked from the same direction used by the native HDR solar disc, directional key and shadow projection. Original PNGs, radiance factors, BRDF LUT, animation clock and geometry budgets remain unchanged.

Full Linux configure/build and 104/104 tests pass with Khronos core/synchronization validation. Shipping isolated native acceptance, exact wind/reflection restoration and a real 100-second tour pass. Source freeze `5ee0a10628bc2d694d7417442a69242adc02f258`. Native art, movie and package use the same frozen Shipping executable. Source/executable/movie/package hashes and actual selected-device observations are retained.

Standard: 60865 vertices, 140070 indices, 393 batches, 46 materials. Reference parity and physical-display acceptance remain open (VIS 5/7).
