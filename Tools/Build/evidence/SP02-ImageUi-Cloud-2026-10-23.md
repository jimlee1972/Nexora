# SP-02 cloud implementation evidence — 2026-10-23

**Cloud engine implementation and Zig diagnostic screens delivered; final game UI acceptance remains
open.** Based on Nexora main `3a8b6be364dec258abebe8cf5cbd4ae205acaa82`. No M6/M7 count advances.
Beads `nexora-0v7` tracks this feature; `nexora-0v7.1` retains final-asset/script/physical acceptance.

This report records the local cloud gates. Completed hosted platform/native results and final
merge status are recorded in [PR #703](https://github.com/jimlee1972/Nexora/pull/703).

## Scope

The independently negotiated C/Zig UI draw extension adds copied image registration, images,
clipped nine-slice, style-free button interaction, glyph metrics, automatic immutable texture
IDs and removable font/image handles. Owning frames preserve bytes after removal; the general
Player drains expired native resources before acquire. Original/UI V1/SP-01 wire prefixes remain
usable. Zig composes layout, focus visuals, wrapped labels and the five diagnostic screens.

The implementation does not supply approved skills, portraits, paper frames, item/progression rules,
final localized scripts, SP-03 general asset IO or SP-04 color conversion. Diagnostic icons do not
close the four-skill/two-companion/three-charm production HUD criterion. The Noto font pixels are real.

## Verification

| Gate | Result / boundary |
| --- | --- |
| Full Linux Development configure/build/CTest | 274 registered; **258 passed, 16 prerequisite skips, 0 failed** |
| C/Zig/host compatibility | Every new field size/alignment/offset, original/SP-01 prefix canaries, SP-01 display getter retained without UI draw, absent extension behavior |
| Resource ownership | Copied image bytes, stale handles, immutable IDs, 300 image register/remove cycles, 40 font register/remove cycles, retained frames delaying release, native-thread rejection |
| UI and text | Nine-slice source UV/border scaling/clips; DPI 100/125/150/175%; UTF-8 advances/wrapping, unavailable glyphs, bounded scratch; synthetic pointer/nav/cancel, disabled controls and focus |
| Real Zig screens | HUD 4/2/3 slots, choices, loadout, subtitles and settings; UI 100/150%; stop/recreate with the same host and resource-slot recovery |
| Linux Shipping | Full Monolithic build with 2D/Zig passed |
| Separate Shipping consumer | ReleaseSafe Zig library exported/relocated/launched by the general Shipping Player at 1920x1080, 1024x768 and odd ultrawide 3441x1081; inventory not a Development copy; balanced module allocations |
| Native image fixture | Compiled with the Development target's flags and X11 headers; local runtime not registered because Slang/native GPU prerequisites are unavailable. Hosted Vulkan/Xvfb execution evidence is recorded in PR #703 |
| Portable font tool | Real FreeType raster and strict cmap admission, non-BMP scalar fixture, missing glyph/invalid UTF-8/overwrite rejection and audit-only publication passed |
| Product acceptance | Final art, final script/font selection, physical Windows DPI/typography/mouse/keyboard/gamepad: **NOT ACCEPTED** |

Raw [CTest output](SP02-ImageUi-Cloud-2026-10-23/development-tests.log),
[JUnit](SP02-ImageUi-Cloud-2026-10-23/development-junit.xml),
[Shipping consumer](SP02-ImageUi-Cloud-2026-10-23/shipping-ui-tests.log) and
[commands](SP02-ImageUi-Cloud-2026-10-23/commands.txt) accompany this report.
Skips are existing native GPU/input prerequisites, not portable image UI or font cases.
The native pixel fixture extends the existing `twod.native_pixels` gate with actual image-border,
center/UV/clip readback and GPU retirement; a successful compile alone is not a pixel result.

## Full planning-document font inventory

A strict bake used the complete public game design/system/art documents, not a few sample Chinese
characters: **1,605 unique scalars, zero missing**, Noto Sans CJK TC Regular Sans2.004 at 32 px.
The atlas is **2048x1024 RGBA (8 MiB)** with 1,605 glyphs, using one of 32 active font slots and
remaining within the 16 MiB per-font bound. No engine font limit was raised.

[The inventory](SP02-ImageUi-Cloud-2026-10-23/document-corpus-font.json) records every source-file
hash, font SHA-256, missing-glyph list and output hash. Source font:
https://github.com/notofonts/noto-cjk/tree/Sans2.004 . The small committed raster subset includes
[OFL attribution](../../../Gameplay/Zig/src/fixtures/NOTICE.md). Pillow 12.3.0/fontTools 4.61.1 were used.
These are planning-document inputs, not the final localization/subtitle corpus; repeat this audit
on the final strings and review Japanese names, line breaks, clipping and Windows appearance.
Complex shaping and locale punctuation rules are not implemented by scalar wrapping.

## Acceptance procedure

Build the `NexoraZigUiShowcaseBuild` module, export it with the general Player, and launch natively
at 1080p. Its confirmation button cycles the five diagnostic screens. Use keyboard/gamepad
navigation and pointer press/release, activate choices, toggle 100/150% in settings, cancel back,
resize and repeat at DPI 100/125/150/175%. Inspect glyph edges, wrapping, focus, clips and safe-frame
alignment. Replace diagnostic atlas regions with the approved final artwork, drive real inventory,
choice and subtitle data from Zig, and repeat with final localized strings. Record physical device
and rendered evidence. Do not mark `nexora-0v7.1`, SP-01 Windows acceptance or SP-07 performance done
from the cloud checks in this report.

## Hosted build follow-up

The first hosted macOS Development build found an empty Zig shared-library command: its argument
list intentionally exists only on non-Apple platforms, because Zig 0.14 cannot resolve libSystem
with the hosted SDK. The image UI sample now follows the repository's existing Apple path: build
the PIC Zig object, then link the dylib with Apple Clang. Linux retains its existing build-lib
path; the full local Development gate and Shipping consumer were rechecked. The corrected-head hosted run in PR #703 is authoritative for actual Apple build/execution evidence.

The native image oracle checks all RGB channels of its border/center and the actual RenderUi
clear color. For the clear color it accepts the documented linear UNORM bytes or their sRGB
attachment encoding within two byte values, matching the existing native alpha test's format
contract. This is not a change to asset color conversion or SP-04 acceptance.
