# Wind-driven canopy and puddle sky masking: Linux evidence

Original foliage expands to 3,072 leaf cards: denser hanging arcade ivy, climbing ivy on six columns, layered cypress crowns, device vines and foreground banks. Existing cutout texture/coverage mips, two-sided transmission, root UVs and shared-clock wind remain in use. The activated Standard scene reports 61,655 vertices, 121,842 indices, 299 batches and 42 materials, within the native 16-bit geometry budget.

Reframed regional puddles remain native affine planar reflections. The direct sky joins the receiver mask alongside the floor; its depth can no longer occlude the Fresnel-attenuated mirrored sky behind nearer reflected objects. Fixed-camera prototype comparison changed the empty wide puddle pixel (520,687) from RGB (222,183,147) to (115,99,96); that comparison is a local diagnostic, not a reference-parity score. Source/native captures retain the corrected rendering. No shaders, public packets, cooked assets or stable ABIs change.

Full 97/97 Development tests pass with Khronos core/synchronization validation (110.96 s). Isolated Shipping acceptance, an actual 100-second shared-clock animation recording and source/executable/movie/package SHA-256 provenance are retained here. Production freeze: `22df5bbb911907aa8bcd495e6da99532d75b9d79`. Artifacts: `NexoraShowcase-Canopy-Puddles-22df5bb.mp4` and `NexoraShowcase-Canopy-Puddles-22df5bb-Linux.zip`. No overlays or retiming.

Reference parity and physical-display performance remain unaccepted. VIS remains 5/7.

Windows CI follow-up: run 37412368918, job 112103646786 reported MSVC C4458 because the rivet-loop local `radius` hid the camera member. Rename only that loop local to `rivetRadius`, retaining all numeric values and operations. The frozen native executable/movie evidence above predates this naming-only change. Current-head CI must rebuild and validate every supported platform before merge. The retained log is a diagnostic excerpt, not the complete CI log.
