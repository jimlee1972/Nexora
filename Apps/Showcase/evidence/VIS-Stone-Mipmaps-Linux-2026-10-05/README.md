# Stone mipmaps: Linux native evidence

✅ Linux Development configure/build and all 97 tests pass (96.26 seconds, no skips).
All 77 native PBR frames pass, including minified sRGB checker/reference comparisons.
Shipping/Full packaging and checksum-verified isolated native interaction pass. The same
executable records an actual 100-second animation tour without overlays.

Production source freeze is `174b822f4df8cab7c1f34508371bec5a6b0f2d2c`; source, executable, movie and package hashes
are retained in `release-provenance.json`. Immutable scene uploads build sRGB, linear ORM
and normalized normal mip chains in Vulkan, DX12 and Metal. Cutout masks, unlit atlases,
ambiguous texture roles and UI retain their original level. No public API, shader packet or
stable C/Zig ABI change is required.

Workspace artifacts: `NexoraShowcase-Stone-Mipmaps-174b822.mp4` and `NexoraShowcase-Stone-Mipmaps-174b822-Linux.zip`.
Software Vulkan/Xvfb evidence does not accept physical target performance or final reference
parity. VIS stays 5/7.
