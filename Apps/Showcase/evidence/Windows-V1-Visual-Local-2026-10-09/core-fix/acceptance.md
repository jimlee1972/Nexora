# Windows local build and native PBR repair

Date: 2026-10-09. Device: NVIDIA GeForce GTX 960, Windows driver 32.0.15.8180.
Source base: `720dd5304ee3800f85609cf3cbde2754ccfcb6ab`; canonical changed-source hashes are in `provenance.json`.

The initial full Windows build reproduced MSVC C2026 in the generated Slang-to-HLSL fragment.
The generator now emits bounded adjacent raw literals. The three assembled shader entries are
byte-identical to their previous source (6,579 / 34,411 / 3,757 bytes). The fragment uses 17 literals.
No Slang authoring, shading math, stable ABI or module linkage changes.

Commands used the Visual Studio 2022 bundled CMake/CTest (3.31.6-msvc6):

```powershell
cmake --preset windows-showcase-development
cmake --build --preset windows-showcase-development --parallel 4
ctest --preset windows-showcase-development
python Tests/WindowPresentation/EmbeddedHlslTests.py
python -O Tests/WindowPresentation/EmbeddedHlslTests.py
ctest --preset windows-showcase-development -R 'showcase.courtyard_hero_generation|build.pose_search_profiles|build.animation_profiles'
ctest --preset windows-showcase-development -R '^window_presentation.dx12_pbr$'
```

The first complete CTest run finished 95/99 in 610.02 seconds. All four failures subsequently
passed after correction: one existing LICENSE CRLF was restored to the canonical Git LF bytes;
the profile checks used VS x64 developer environment plus bundled Ninja; native GDI pixel capture
now raises the test client above other apps without changing pixel/phase-marker assertions.
The three environment/asset rechecks passed 3/3 in 213.49 seconds, and the native PBR CTest
passed 1/1 in 4.70 seconds. Embedding regressions pass 3/3 in normal and optimized Python.
This is an initial full run plus focused rechecks, not a fresh all-pass full run.

Full Monolithic Shipping with DX12 and Vulkan also built and produced a checksum-verified isolated
headless launch (51 checksums, exit 0). The missing local Vulkan SDK import library was generated
from the installed system `vulkan-1.dll` exports with MSVC dumpbin/lib; this is a local toolchain
input, not a committed engine dependency.

Linux configure/build/CTest and final art/performance acceptance remain pending. Physical Windows
native package evidence is collected separately; these compiler/pixel results do not certify art.
