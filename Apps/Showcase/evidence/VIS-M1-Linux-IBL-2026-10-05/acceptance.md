# VIS-M1 IBL resources and native reflection evidence

2026-10-05, Linux/X11/Vulkan, Mesa lavapipe, Xvfb 1280x720.
This accepts the Linux IBL slice; VIS-M1 requires floating HDR composition and remains open.

- `cmake --preset linux-development`: passed, Slang-enabled cache.
- `cmake --build --preset linux-development`: passed.
- `ctest --preset linux-development`: 94/94 passed without skips (41.64 seconds).
- `cmake --preset linux-shipping`: passed, Monolithic.
- `cmake --build --preset linux-shipping`: passed.
- `cmake --build --preset linux-shipping --target NexoraShowcasePackageShippingEvidence`: passed;
  checksums and isolated launch evidence are retained. This isolated package launch is headless.

The pinned CC0 Forest Slope HDRI is preserved under Content/Showcase/Courtyard/Environment.
Conversion verifies source, license and attribution hashes; a bounded Radiance decoder produces
linear RGBA16F diffuse irradiance, seven GGX specular levels and a split-sum BRDF LUT using
128 Hammersley samples. Generation checks and analytical constant-energy/orientation, malformed
RGBE and BRDF-bound tests pass. Source/license/converter/derived metadata is itself cooked into a
bundle asset and is an explicit dependency of each cooked environment resource. The verified
Runtime generation loads those bytes before native upload; render time loads no source files.

Native Vulkan pixels use radiance 4 fixtures and verify diffuse/metal separation, roughness mip
selection, rotation, view-dependent reflection and U-seam filtering, IBL disable, frame reuse,
resize/re-upload and rejected missing references, mip mismatch and RGBA8/linear ID aliases. The
per-frame emission marker ensures the oracle reads the current presented submission. Existing
material normal/ORM/sRGB/gamma and mirrored tangent cases remain included. Equivalent Metal cases
are source-complete and require native CI; local execution was Linux only.

All nine room controls, three courtyard fixed cameras, clean view, PBR/Lambert and IBL/direct-light
comparisons, exact restoration and camera replay pass with retained native captures.
The gold ring now has environment reflections; geometry remains the accepted engineering blockout.
Targets remain RGBA8. Floating HDR scene storage, final art, shadows/wind, physical visuals and
GTX 960 performance remain pending. Three sequential 360-frame runs with 60 warm-up and
300 retained samples each measured 37.16, 35.63, 36.46 FPS. Own builds/tests were
idle during capture. These lavapipe measurements do not accept the physical GPU budget.

## Cross-platform reproducibility correction

The initial exact-head CI found Windows checkout line-ending changes in pinned text hashes and ARM/x86 libm variation at binary16 ties. Pinned provenance and converter sources now require LF; binary32 canonical intermediates precede half conversion. The original captures and source hashes above document the initial Linux generation. The corrected source/payload hashes and repeated 94/94 Linux gate are retained in `source-hashes-canonical.json` and `ctest-linux-development-canonical.log`. Cross-platform acceptance requires the corrected PR head CI.
