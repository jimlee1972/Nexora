# VIS-M1 Linux floating HDR acceptance — 2026-10-05

Linux Development configure/build/full CTest passed: 95/95 without skips (60.17 seconds).
Shipping Monolithic configure/build and NexoraShowcasePackageShippingEvidence passed, including
checksum verification and isolated headless package launch. Native visual evidence is separately
retained below; that headless launch alone does not accept rendered visuals.

The actual native courtyard renders shared PBR/IBL into a protecting-frame RGBA16F target. Main
samples it on the GPU, applies exposure and shared ACES, transfers once into the acquired image,
and UI follows. External RenderGraph declares RGBA16F and ShaderRead; its counters still describe
real successful callbacks. Native RHI triangle allocation explicitly rejects this externally owned
format rather than silently substituting RGBA8. Stable C/Zig and NXAB remain unchanged.

The native Vulkan oracle includes 24 presented submissions: prior material/IBL tests plus radiance
4 versus 1, exposure 1/0.125/0.25, current-frame emission markers, UI color invariance, float target
reuse/resize and rejected direct HDR/nonfinite exposure. Legacy direct and offscreen RGBA8/Lambert
remain covered. Matching DX12 test source uses actual Win32 pixels; Metal reads the GPU-composited
drawable through a private test seam. Windows/Metal execution requires exact-head PR CI.

Interaction retains all nine rooms, fixed shots, P/O comparisons and E exposure changes with exact
restoration. Three sequential 360-frame runs (60 warmup, 300 samples) measured 34.52, 35.23 and
35.81 FPS with own builds/tests idle. Reports identify RGBA16F, exposure 1 and the actual basic
quality/shared PBR IBL path. These software lavapipe results do not accept GTX 960 performance.

The source hashes, native stdout/capture, interaction screenshots/reports and baseline accompany
this acceptance. Geometry remains the engineering blockout. Bloom, directional shadows, vegetation
motion, final art and target-hardware visual/performance acceptance remain open. HDR storage
precision does not establish an HDR10 monitor/swapchain.

MSVC warning corrections rename the Win32 capture buffer to avoid local shadowing and use float light literals. The repeated 95/95 Linux gate (45.77 seconds) and supplemental exact test-source hash are retained as `ctest-linux-development-msvc.log` and `source-hashes-msvc.json`; original benchmark/capture evidence is unchanged.

The first native Metal CI rejected the HDR UI test clip because this fixture is 640×360, whereas its new UI rectangle assumed 640×480. The rectangle and sample now use the actual fixture extent; native rerun is required. `source-hashes-metal-ui.json` retains the corrected test source. Production rendering is unchanged by this correction.

The Win32 visual oracle now captures the composed desktop at ClientToScreen coordinates rather than the window GDI DC, which showed the class background for the flip-model swapchain. Slang mismatch diagnostics retain bounded differing lines for cross-platform investigation. The repeated 95/95 Linux gate and supplemental source hashes are retained as `ctest-linux-development-desktop-capture.log` and `source-hashes-desktop-capture.json`. The native Windows and compiler checks still require corrected-head CI.

The tone pass now consumes an explicit 48-byte fullscreen triangle instead of SV_VertexID. This avoids cross-platform Slang SPIR-V builtin declaration ordering differences while retaining exact generated-artifact checks. Vulkan/DX12 append the vertices to the protecting frame's upload; Metal copies them into its command encoder. The Windows Lambert marker oracle now respects the existing 1.15 ambient/direct coefficient. Linux repeated all 95 tests without skips (46.80 seconds), rebuilt Shipping and passed isolated package evidence; supplemental native and nine-room interaction captures verify this production change. Original performance measurements describe the earlier procedural-vertex implementation; they are not new performance measurements. Windows/Metal exact-head CI remains required.

Corrected-head Windows and Metal Development/native pixel/generated shader checks passed in Build 1428. The Linux retained nine-room interaction reached the existing 45-second CTest limit on a hosted runner; its bounded timeout is now 90 seconds, retaining all pixel, exact-restoration and shutdown assertions. The repeated full local gate with retained screenshots passed 95/95 without skips (44.51 seconds). Exact-head CI is rerun before merge.
