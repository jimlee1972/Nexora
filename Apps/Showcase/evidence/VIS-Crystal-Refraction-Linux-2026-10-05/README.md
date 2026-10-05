# Crystal refraction: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (95.08 seconds, no skips).
All 75 native PBR frames pass, including six neutral/bent/reversed, exact replay, zero-thickness
and foreground-rejection cases. Shipping/Full packaging and checksum-verified isolated native
interaction pass, including F8 pixel changes and exact restoration. The same executable
records an actual 100-second animation tour without overlays.

Production source freeze is `7de378f24240b4665d443154be292eb8b6f3ed8d`; exact source/executable/movie/package
hashes are retained in `release-provenance.json`. Standard/High crystal uses index 1.46 and
0.65 world-unit thickness. Shared Slang samples an opaque linear-HDR snapshot with a 24-pixel
per-axis bound and foreground rejection; native Vulkan/DX12/Metal adapters preserve opaque
depth, own the copy/sample transitions and release snapshots through frame fences/resize.
Khronos core/synchronization validation passes all 75 native PBR frames. Compatible HDR
clear/load dependencies and swapchain/copy attachment-read barriers preserve valid loads.
The freeze also preserves accepted Editor Vulkan validation changes on main.
Defaults/Basic retain ordinary tint transparency. The private packet grows to 368 bytes while
DX12's aligned 768-byte pair and stable C/Zig ABI remain unchanged.

Workspace artifacts: `NexoraShowcase-Crystal-Refraction-7de378f.mp4` and
`NexoraShowcase-Crystal-Refraction-7de378f-Linux.zip`. Software Vulkan/Xvfb evidence does not
accept physical target performance or final reference parity. Offscreen/multiple transparent
layers, full-volume tracing, dispersion and travel-distance absorption remain outside this
bounded slab model. VIS stays 5/7.
