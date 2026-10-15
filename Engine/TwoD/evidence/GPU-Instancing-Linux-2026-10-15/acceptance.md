# Zig-controlled 2D GPU instancing — Linux, 2026-10-15

✅ CPU-driven native GPU instancing is implemented for the existing 2D scene/pool/tile extraction.
Zig continues to use the original copied V1 create/read/write/destroy API; no C wire changes.
The C++ host owns window, assets, upload storage, presentation and final cleanup.

| Gate | Observed result |
| --- | --- |
| Development | `cmake --preset linux-development`; `cmake --build --preset linux-development`: PASS. GCC 14, Zig 0.14.0, Slang 2026.18; TwoD, Zig and native Vulkan enabled. |
| Complete Linux suite | `ctest --preset linux-development --output-on-failure`: 284/284 PASS, 895.76 s, no skips. |
| Final refinement regression | After final range/capacity/pixel assertions and reuse of the existing UI fragment shader, Development rebuild and the command below: 30/30 PASS, 22.10 s. |
| Native instancing pixels | `twod.native_pixels`: 256 adjacent opaque instances in one draw, followed by separate green and alpha-red Atlas batches and indexed HUD. The last opaque instance has a different transform and is checked at its own pixel. Eight frames assert 24 instanced draws, 2,064 instances and 90,816 instance payload bytes; expanded Sprite vertices/indices are absent from the native packet. |
| Existing image contracts | Cross-Atlas transparent order, linear blend, HUD placement, four UV flip combinations, linear filtering/gutters, Atlas retirement/reupload and Tilemap chunk changes: PASS. |
| Invalid input | Non-affine Quad rejection, finite attributes, first/count overflow, exact last-record boundary and 100,000-record cap: PASS. Invalid native ranges reject before uploads, without consuming the acquired frame; a valid draw then succeeds in that frame. |
| Real Zig input/reload | 600 presented frames, normalized D-key movement through Zig, host pool updates, reload and balanced allocation: PASS. Final [native report](native.json) records 600 instanced draws, 20,254 instances and 891,176 payload bytes. Counts vary with input timing and culling; tests require bytes = instances × 44. |
| Shipping | `cmake --preset linux-shipping`; `cmake --build --preset linux-shipping`: PASS. Static Zig module, 120 native frames, 120 instanced draws, 4,080 instances and 179,520 payload bytes, reload and balanced allocation. [Report](shipping.json). |
| Shaders | `python Engine/Presentation/shaders/GenerateShaders.py --compiler <glslangValidator-15.1.0> --check`: PASS for existing scene/UI plus new Sprite vertex shader. Runtime needs no shader compiler. |
| Zig example/docs | GPU-instancing README fragment compiled by real Zig 0.14.0; Markdown parse, local links and `git diff --check`: PASS. Bilingual 2D/Zig/API roadmaps and index updated; root README unchanged. |

Final affected regression:

```bash
ctest --preset linux-development \
  -R 'gameplay.zig_|twod\.|api.m6|runtime.gameplay_module_host|window_presentation' \
  --output-on-failure
```

Shipping native invocation (under an isolated Xvfb display):

```bash
build/linux-shipping/Gameplay/Zig/NexoraZigTwoD --native --frames 120 --report shipping.json
```

The executor is Linux/X11/Vulkan with Mesa lavapipe software rasterization. Native API draws and
read-back pixels establish correctness, not physical GPU throughput. Vulkan validation layers were
not requested by the native pixel harness. The Shipping run uses the configured cloud dependency
paths and is not a clean-host packaging or Windows acceptance claim. DX12 and Metal adapters are
implemented; native target-host acceptance remains separate from provider compilation/test checks.

Each Sprite uploads 44 bytes rather than 80 vertex + 24 index bytes on this 32-bit-index path;
background/HUD uploads remain. This is a payload comparison, not a measured speedup. Projection,
CPU triangle snapshots for Editor preview, sorting, culling, animation and Zig simulation still run
on CPU. Only adjacent compatible Atlas batches merge. Buffers follow existing owning frame-slot
fences and surface teardown; no native resource pointer or GPU allocation is exposed to Zig.

M5 gains native instancing without changing the accepted 9/11 milestones or P0 6/8. Physical minimum-
spec Windows 60 FPS and clean Windows Runtime/Shipping gates remain NOT RUN. Main Editor project
Play attachment remains separate. Compute simulation/culling, indirect drawing, script-owned GPU
buffers and a general Mesh/Material instancing API are outside this delivered 2D slice.

See [TwoD contracts](../../README.md), [Presentation](../../../Presentation/README.md) and
[Zig usage](../../../../Gameplay/Zig/README.md#gpu-instanced-2d-rendering-from-zig).
