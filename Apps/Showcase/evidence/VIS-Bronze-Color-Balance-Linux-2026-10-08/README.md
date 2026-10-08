# Bronze color balance: Linux evidence

The authored bronze material changes its base-color multiplier from (0.8,0.65,0.4) to (1.0,0.95,0.85), exposing the source texture gold and patina to native metallic lighting. Metallic=1, roughness=0.4, source textures, normals, HDR key/IBL, emission, geometry and mirror inheritance remain intact.

Full Linux configure/build and 109/109 tests pass with core/sync validation. Frozen Shipping native acceptance, exact wind/reflection/occlusion restoration and a real 100-second tour pass. Source freeze `9d7298bed7ccaff24609ae2184753f574948960b`. Native art, movie and package use the frozen Shipping executable.

Reference parity and physical-display acceptance remain open (VIS 5/7).
