# V1 distribution completion work — Linux evidence, 2026-10-04

The date uses Asia/Taipei. Base commit: `c4c17bd34df9662b8c140ce13eeafc0be93da0f6`.
`source-provenance.json` records working-tree source hashes and the exact Shipping/Full ZIP digest.
This record accepts Linux distribution and regression scope only. Metal source, Mac packages,
new CI jobs and a new multi-platform tag release still need their respective target-host/run evidence.

| Command | Result |
| --- | --- |
| `cmake --preset linux-development` | PASS |
| `cmake --build --preset linux-development --parallel 4` | PASS |
| `ctest --preset linux-development` | 80/80 PASS, no skips; five Vulkan/Xvfb gates executed |
| `cmake --preset linux-shipping` | PASS, Minimal/Monolithic |
| `cmake --build --preset linux-shipping --parallel 4` | PASS |
| `cmake --preset linux-showcase-shipping` | PASS, Full/Monolithic |
| `cmake --build --preset linux-showcase-shipping --target NexoraShowcasePackageShippingEvidence --parallel 4` | PASS; 11 checksums, isolated-copy headless launch |
| `python3 Tools/Package/VerifyShowcaseRelease.py --package build/linux-showcase-shipping/package/NexoraShowcase-Shipping --evidence-directory build/linux-showcase-shipping/artifacts/release-native` | PASS; checksum-verified isolated native copy |
| `cmake --build --preset linux-development --target NexoraShowcasePackageDevelopmentEvidence` | PASS; seven Engine libraries confined to the isolated copy |
| `python3 Tools/Package/TestPackageShowcase.py` | PASS |
| `python3 Tools/Package/TestLaunchShowcasePackage.py` | PASS; ELF fixtures and portable Mach-O parser fixtures, no Mac execution claim |

Local tooling resides under `/workspace`; caches enable Zig gameplay and Slang and use unpacked X11,
Vulkan and build tools. Shipping configuration uses a local Vulkan-Headers source override. The
first sandboxed CTest attempt skipped Xvfb because local display sockets were denied; the recorded
full gate reran with local networking available and executed all native tests. Windows/macOS builds
were not run in this Linux environment.

The Full package executes eight room controls, camera/character input, Modify/Undo/Play/reload,
locale, clip blend, Validation Lab error rejection/export and resize. Its report records **964**
native scene/offscreen/copy/UI/present operations, **1156** instances, three native scene texture
uploads, one resize generation and no backend fallback. Mesa lavapipe is explicitly a software
rasterizer; `physical_display_verified` remains false. Rendering and Lab captures are retained beside
raw JSON. The 210-second tour was not rerun here; existing Windows evidence remains its authority.

The new source supplies Metal indexed scene/instances/texture/depth/copy, Cocoa input and Retina
coordinates, Mac package relocation/ad-hoc signing, a GPU pixel CTest and POSIX release jobs.
These additions do not themselves certify Metal parity or a completed V1 Showcase. Audio/video/WebView
remain contract-only/unavailable. New tag assets also include desktop CTest logs from the required
passing Build run; that expanded workflow still needs a real run.
