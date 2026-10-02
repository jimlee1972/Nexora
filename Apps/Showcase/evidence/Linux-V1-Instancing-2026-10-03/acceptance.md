# Linux native hardware instance acceptance

Scope: Linux/X11/Vulkan under Xvfb/lavapipe with Khronos synchronization validation.
The build ID refers to the pre-commit base; source-provenance.json records the tested working tree.

- PASS: `cmake --preset linux-development`
- PASS: `cmake --build --preset linux-development -j4`
- PASS: `ctest --preset linux-development --output-on-failure`: 75/75, no skips; five native gates.
- PASS: `cmake --preset linux-shipping`; `cmake --build --preset linux-shipping -j4`
- PASS: `cmake --preset linux-showcase-shipping`; `cmake --build --preset linux-showcase-shipping --target NexoraShowcasePackageShippingEvidence -j4`

The Vulkan scene pixel gate draws one triangle with two hardware instances, verifies independent
positions and red/green tints, rejects zero/nonfinite scale/translation and more than 4,096 records,
and retains identity-instance depth-order, lighting, translation, resize and teardown acceptance.
The Rendering room uses one 24-vertex/36-index cube upload for four differently transformed/tinted
instances. Native instance counts are separate from draw counts in the profiler and JSON report.
Vertex/instance bytes remain owned by the protecting frame fence; borrowed application spans do not
escape the call. The screenshot shows the native room output, and interactive.json records the
same tested application's scene and instance counters.

DX12 has the matching hardware instance layout but is not target-host certified by these Linux
results. Textured material, native RenderGraph binding and complete V1 acceptance remain open.
An earlier full run had one X11 BadDrawable interaction failure; the subsequent complete run passed.

Windows CI run 37044292214 passed Full Shipping packaging after the geometry warning fix.
Development then exposed pre-existing Editor test variable shadowing; this patch renames those locals.
