# Crystal refraction: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (92.94 seconds, no skips).
All 75 native PBR frames pass, including six neutral/bent/reversed, exact replay, zero-thickness
and foreground-rejection cases. Shipping/Full packaging and checksum-verified isolated native
interaction pass, including F8 pixel changes and exact restoration. The same executable
records an actual 100-second animation tour without overlays.

Production source freeze is `5a82472337ed7d102b314404dab5a2ac7232d33b`; exact source/executable/movie/package
hashes are retained in `release-provenance.json`. Standard/High crystal uses index 1.46 and
0.65 world-unit thickness. Shared Slang samples an opaque linear-HDR snapshot with a 24-pixel
per-axis bound and foreground rejection; native Vulkan/DX12/Metal adapters preserve opaque
depth, own the copy/sample transitions and release snapshots through frame fences/resize.
Defaults/Basic retain ordinary tint transparency. The private packet grows to 368 bytes while
DX12's aligned 768-byte pair and stable C/Zig ABI remain unchanged.

Workspace artifacts: `NexoraShowcase-Crystal-Refraction-5a82472.mp4` and
`NexoraShowcase-Crystal-Refraction-5a82472-Linux.zip`. Software Vulkan/Xvfb evidence does not
accept physical target performance or final reference parity. Offscreen/multiple transparent
layers, full-volume tracing, dispersion and travel-distance absorption remain outside this
bounded slab model. VIS stays 5/7.
