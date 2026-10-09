# Visible sky brightness balance: Linux evidence

Calibrate the visible sky emission multiplier from 1.0 to 0.65 for clearer authored cloud tonal separation. The reflected sky inherits the same multiplier. Original source PNGs, cooked atlas, floating HDR IBL, geometric HDR sun, key light, crystal radiance, geometry, materials and the shared animation clock retain their original values. Standard remains 62,557 vertices, 142,488 indices, 395 batches, 48 materials and 4,041 source foliage quads. The source panorama remains an authored LDR image; the renderer retains RGBA16F HDR, a separate geometric HDR sun and crystal bloom.

Full Linux configure/build and 110/110 tests pass with core/sync validation. Frozen Shipping native acceptance, exact wind/reflection/occlusion restoration and a real 100-second tour pass. Source freeze `f90613ed665fd769be1ea6ed03cdd91d65f7e48b`. Native art, movie and package use the frozen Shipping executable.

Reference parity and physical-display acceptance remain open (VIS 5/7).
