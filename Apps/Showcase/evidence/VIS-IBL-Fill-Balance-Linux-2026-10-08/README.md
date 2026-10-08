# Courtyard indirect fill balance: Linux evidence

Reduce the Standard/High authored floating HDR environment intensity from 2.4 to 1.8, separating stone shadow and lit faces while preserving their original material and directional lighting. Original source sky PNG, cooked sky atlas, floating diffuse/specular/BRDF payloads, resource headers, geometric HDR sun, key and point lights, crystal/mote emission, bloom, geometry and shared animation clock retain their original values. Basic still omits IBL. Standard remains 62,557 vertices, 142,488 indices, 395 batches, 48 materials and 4,041 source foliage quads.

Full Linux configure/build and 111/111 tests pass with core/sync validation. Frozen Shipping native acceptance, exact wind/reflection/occlusion restoration and a real 100-second tour pass. Source freeze `1dd920094dc339ddc90ce5955543447355d056ef`. Native art, movie and package use the frozen Shipping executable.

Reference parity and physical-display acceptance remain open (VIS 5/7).
