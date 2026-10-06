# Coverage-aware foliage mipmaps: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (104.47 seconds, no skips).
All 89 native PBR frames pass with Khronos core/synchronization validation enabled;
all three native geometry budgets pass. Shipping/Full packaging and checksum-verified
isolated native interaction pass. The same executable records an actual 100-second
wind/animation tour without overlays or retiming.

Production source freeze is `e7fb2f5e24d980d5ff53caf65dfec82cd8208fa4`; source, executable, movie and package hashes
are retained in `release-provenance.json`. Lit half-cutoff foliage receives alpha-weighted linear-color mip chains and
closest available authored coverage at each derived level. Transparent RGB cannot pollute
leaf colors. Unlit atlases, other cutoffs and ambiguous/mixed roles keep one level; tiny
levels may have unavoidable discrete coverage error. Visible and shadow passes share native
resources and unchanged immutable-generation ownership. CPU checks verify coverage, original
level preservation, no magenta fringes, supported cutoff/unlit rejection and semantic conflicts.
No public material field, native binding, shader packet or stable ABI is introduced.

Workspace artifacts: `NexoraShowcase-Foliage-Mipmaps-e7fb2f5.mp4` and `NexoraShowcase-Foliage-Mipmaps-e7fb2f5-Linux.zip`.
Software Vulkan/Xvfb evidence does not accept physical target performance or final reference
parity. VIS stays 5/7.
