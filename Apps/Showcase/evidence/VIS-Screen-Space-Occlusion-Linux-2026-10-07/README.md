# Bounded HDR screen-space contact occlusion: Linux evidence

An optional perspective-only descriptor reconstructs geometry from the HDR distance channel. Four neighboring depths estimate a normal and twelve bounded taps shade nearby contacts. The radius is bounded to 32 physical pixels per axis, with depth range and bias rejection. The shared native shader and copied 80-byte tone packet work on Vulkan, DX12 and Metal. High HDR radiance and bloom extraction are preserved; UI follows composition. This is a screen-space composition approximation with no hidden-geometry, temporal or ray-tracing claims.

Native fixtures verify contact darkening, planar stability, unchanged HDR highlights and exact absent/zero restoration. F11 toggles courtyard occlusion; Basic omits it. Full Linux configure/build and 104/104 tests pass with core/sync validation. Frozen Shipping native acceptance, exact wind/reflection/occlusion restoration and a real 100-second tour pass. Source freeze `60e6a54621fe3bcfedf4b6d5b3ccd8e5419b05b3`. Evidence uses the frozen Shipping executable.

Reference parity and physical-display acceptance remain open (VIS 5/7).
