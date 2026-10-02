# Linux native sampled material acceptance

Scope: Linux/X11/Vulkan, Xvfb/lavapipe, Khronos synchronization validation, 2026-10-03 Asia/Taipei.
Build IDs identify the pre-commit base; source-provenance.json records the exact tested source tree.

- PASS: `cmake --preset linux-development`
- PASS: `cmake --build --preset linux-development -j4`
- PASS: `ctest --preset linux-development --output-on-failure`: 75/75, no skips; all five native gates.
- PASS: `cmake --preset linux-shipping`; `cmake --build --preset linux-shipping -j4`
- PASS: `cmake --preset linux-showcase-shipping`; `cmake --build --preset linux-showcase-shipping --target NexoraShowcasePackageShippingEvidence -j4`

Hub/Rendering sample original procedural checker texels through GPU UV/sampler/descriptor bindings.
The Vulkan pixel gate verifies red versus green selection from one two-texel texture, no repeated
upload for the same immutable ID, and texture resubmission after swapchain resize. Malformed pitch,
unknown ID, oversized extent and nonfinite UV are rejected. It retains instance/depth/lifetime checks:
16 accepted draws, 17 instances, three resizes, five scene texture uploads. Scene/UI texture ID tables
are separate; scene uploads have their own diagnostics. The screenshots retain the composed native
scene/UI output. Cache/upload sizes are bounded and staging is protected by its owning frame fence.

DX12 has matching material bindings but this Linux evidence does not certify DX12 target-host
execution, physical displays or complete V1. Native RenderGraph scene binding remains pending.

Follow-up CI fixes: close the Editor test's scene-reading handle before Windows atomic replacement.
Run 37045590037's native TSan report was within the system Mesa driver mutex teardown; CI now runs
portable engine concurrency with native backends disabled for TSan. Development and ASan/UBSan keep
native Vulkan acceptance. The corrected Windows/TSan CI results remain pending.
