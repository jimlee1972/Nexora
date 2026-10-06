# Courtyard coping and side-arcade shadows: Linux evidence

The pedestal uses 72 original annular coping stones with joints, bevels, bounded
radial/height variation and inverse-transpose normals under a bend with positive
orientation. Recessed cores support the three courses. Existing native materials,
asset IDs, shader packets and animation ownership are unchanged.

The original 14x14 directional projection clipped two side-arcade crowns. The
24x20 projection covers all four crowns while retaining light orientation, depth
range and native tier resolutions. Independent world-coordinate projection proof
and a RoomSession regression cover the crowns; native F6 comparisons restore
paused pixels exactly. Existing core/synchronization validation runs cover the
97-frame PBR fixture without shader changes.

Full Development and isolated Shipping acceptance logs, native screenshots, an
actual 100-second animation recording and SHA-256 provenance are retained here.
Production source freeze: `19899040d0c70d3ba2053bd918cae03230bed88f`. Workspace artifacts: `NexoraShowcase-Courtyard-Coping-1989904.mp4` and
`NexoraShowcase-Courtyard-Coping-1989904-Linux.zip`. No overlays or retiming.

Software Vulkan/Xvfb does not accept photoreal preview parity or physical target
performance. VIS remains 5/7.
