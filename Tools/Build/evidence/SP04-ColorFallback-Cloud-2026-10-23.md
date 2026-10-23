# SP-04 explicit color and 2D fallback — cloud evidence

Base: `f75469c5120d890f4edfe6411d1bfece666cd252` (latest remote main at task start).
Branch: `feat/twod-color-sp04`. Beads: `nexora-ofe`; final art/physical gate: `nexora-ofe.1`.

## Scope

Explicit opt-in sRGB/straight-alpha PNG import into the existing linear RGBA8 Atlas format;
strict standard color-metadata checks and ICC rejection; matching allocation-free Zig image/tint
helpers; native Player sRGB output via a UI-only surface option; separate procedural Zig layered
fallback with Y crossing, roof fade, baked glow/shadow removal and retained hazard outline.
C/Zig ABI and Atlas schema remain unchanged. Native C++ descriptor consumers must rebuild.

## Executed locally in Linux cloud

- Full `cmake --preset linux-development`, `cmake --build --preset linux-development` and
  `ctest --preset linux-development`: **281 registered, 265 passed, 16 existing native-prerequisite
  skips, zero failures**, 218.21 seconds. The new Zig wrapper and Player consumer actually ran.
- Full `linux-shipping` Monolithic build with 2D/Zig enabled succeeded. A separate baseline-CPU
  ReleaseSafe Zig shared module was exported with the Shipping Player and ran at 1920×1080,
  1024×768 and 3441×1081; inventory asserts no Development test copy.
- Import tests check all 256 transfer values against a shared golden, reference swatches, alpha,
  deterministic real packer output/extruded gutters, conflicting profiles, unknown options and
  failed publication. Legacy manifests remain byte-identical. Source conversion happens once.
- Native Vulkan adapter and `NativeTests.cpp` compile using the real module/SDK flags. Local
  Vulkan/Slang runtime prerequisites are unavailable, so local native pixel execution is **not run**.
- API-M6 validates ABI 3 with 114 documented exports. Touched C++ formatting, Zig format, Python
  compilation, module Markdown links/fences, bilingual roadmap changes and `git diff --check` pass.
  Root README remains unchanged; no milestone/physical acceptance counts were increased.

[Exact commands](SP04-ColorFallback-Cloud-2026-10-23/commands.txt),
[JUnit](SP04-ColorFallback-Cloud-2026-10-23/development-junit.xml),
[CTest output](SP04-ColorFallback-Cloud-2026-10-23/development-tests.log),
[Shipping consumer](SP04-ColorFallback-Cloud-2026-10-23/shipping-consumer.log).
CTest text trailing whitespace was normalized for repository checks; JUnit is unchanged.

## Hosted native gates

New platform-specific tests require linear byte 55 to be encoded to sRGB byte 128 (±2), rather
than accepting UNORM output. Linux additionally checks the straight-alpha composite, Y crossing
and foreground fade. Windows DX12 uses GPU staging readback; Metal uses the existing native
readback. These cases are committed for hosted CI; compilation is not evidence they executed.
Record completed current-head hosted job links in the PR and game integration document.

## Remaining acceptance

This is not a production first level. The fallback glow/shadow are ordinary baked alpha sprites,
not dynamic local lighting/occlusion shadows/selective bloom. Final dark gradients must account
for linear UNORM8 quantization. Formal palette, white-edge/transparent filtering, 100/75/50%
readability, low-effects hazards and physical Windows target output/RAM/VRAM/frame time remain
open. Existing SP-01/02/03/05/06/07 and M6/M7 gates are not closed by these diagnostics.
