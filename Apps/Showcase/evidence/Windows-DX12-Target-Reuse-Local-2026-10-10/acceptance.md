# Windows DX12 completed-slot target reuse

GTX 960 Shipping baseline `5aa60d5c` measures Standard DX12 51.71–55.16 FPS and
completed GPU mean 7.53 ms versus wall mean 18.83 ms. DX12 recreates scene color,
reflection/refraction and shadow color/depth targets on every draw. This revision retains complete
compatible targets in the existing protecting swapchain slot after `Acquire` waits its fence.
Dimensions, HDR format, shadow resolution and reflection/refraction requirements must match.

Private color/reflection/shadow states track recorded composite/pass transitions and return to
RENDER_TARGET before clears. Refraction keeps the existing synchronized opaque-copy transitions.
Views/material bindings refresh from the current submission; every pass, draw, geometry and effect
remains. Invalid input retains validation behavior; partial recording does not publish a reusable
set, and mismatch/direct draw resets the completed slot. Resize/teardown release after the existing
drain. Storage stays bounded to three slots; no extra wait, module dependency or shader change.

Latest main `16c2c980` is integrated at `bba5db85`; source hashes are retained separately from the
preceding repaired Vulkan artifact `5aa60d5c`. Complete Windows Development configure/build/CTest
passes **125/125 in 444.81 seconds**, including both unchanged DX12/Vulkan native PBR fixtures.
Two CTest workers and all original timeout/pixel limits are retained.

```powershell
cmake --preset windows-showcase-development -DNEXORA_ENABLE_VULKAN_BACKEND=ON -DNEXORA_VULKAN_LIBRARY=G:/proj/Nexora/.tools/vulkan-loader-local/vulkan-1.lib
cmake --build --preset windows-showcase-development --config Development --parallel 4
ctest --preset windows-showcase-development --parallel 2 --output-on-failure
```

Commands run with the initialized VsDevCmd/pinned Zig/Ninja task environment. Exact-source hosted
CI, frozen Shipping quality/GPU/immediate/paced/live comparison and final hardware budget are
pending. Full concept-art acceptance and same-version final movies remain open. No Linux Codex
Cloud execution is claimed from this Windows gate.
