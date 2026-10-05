# Stone mipmaps: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (99.53 seconds, no skips).
All 77 native PBR frames pass, including minified sRGB checker/reference comparisons, with Khronos core/sync validation.
Shipping/Full packaging and checksum-verified isolated native interaction pass. The same
executable records an actual 100-second animation tour without overlays.

Production source freeze is `0c9513a2a3e7968e9c97e342dddba7a01c8ad3a2`; source, executable, movie and package hashes
are retained in `release-provenance.json`. Immutable scene uploads build sRGB, linear ORM
and normalized normal mip chains in Vulkan, DX12 and Metal. Cutout masks, unlit atlases,
ambiguous texture roles and UI retain their original level. No public API, shader packet or
stable C/Zig ABI change is required.

Workspace artifacts: `NexoraShowcase-Stone-Mipmaps-0c9513a.mp4` and `NexoraShowcase-Stone-Mipmaps-0c9513a-Linux.zip`.
Software Vulkan/Xvfb evidence does not accept physical target performance or final reference
parity. VIS stays 5/7.
