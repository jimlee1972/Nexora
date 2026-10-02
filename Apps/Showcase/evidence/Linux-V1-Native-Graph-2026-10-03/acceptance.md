# Linux native-owner RenderGraph acceptance

Scope: Linux/X11/Vulkan, Xvfb/lavapipe, Khronos synchronization validation.
Source hashes identify the tested working tree; the build ID identifies its pre-commit base.

- PASS: `cmake --preset linux-development`
- PASS: `cmake --build --preset linux-development`
- PASS: `ctest --preset linux-development`: 75/75, no skips, all five native gates.
- PASS: `cmake --preset linux-shipping`; `cmake --build --preset linux-shipping`
- PASS: `cmake --build --preset linux-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4`

The external-owner graph executes real Offscreen, Main, UI, Present callbacks in compiled order.
Scene color is fence-owned native storage; Main performs a GPU image/resource copy, UI draws into
the acquired backbuffer and Present submits it. Logical transition requests are reported separately
from native barriers; no fabricated RHI handle represents native-owned images. Callback failure
stops successors. Contract tests cover ownership mismatch, order, state requests and failure counts.

Vulkan pixel checks retain depth/light/transform/instance/texture checks and verify three offscreen
copies. Duplicate acquire/copy and Present/UI before pending copy are rejected. Rendering P cycles
instanced cubes, quad and triangle; the interaction test captures each distinct view and all rooms.
The profiler and JSON explicitly identify lavapipe as a software rasterizer.

The Full package includes a stock-PowerShell Windows isolated-copy verifier, checksums and launch
instructions. Its Windows execution is subject to CI/target-host evidence, not these Linux results.
Physical-display and clean-host acceptance belong to the user's local Windows run and remain open.
