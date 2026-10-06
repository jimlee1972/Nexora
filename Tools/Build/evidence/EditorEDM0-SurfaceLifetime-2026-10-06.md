# ED-M0 native UI surface lifetime

Date: 2026-10-06 (Asia/Taipei). This implementation follows the native validation and desktop
parity slices; it does not close ED-M0 or physical/installed-IME acceptance.

## Reproduced defect

One `EditorImGuiHost` rendered eight frames to a native Vulkan surface, destroyed/replaced that
owner at unchanged DPI, then rendered eight frames again. Before the fix, the first owner reported
one atlas upload and no rejected textures; the replacement reported **zero uploads and 56 rejected
commands**. No native UI draws survived because the host reused a font-generation acknowledgement
from the destroyed owner. Pointer/address comparison would not safely cover allocator reuse.

The public surface now supplies a process-local `UiResourceDomain()` token for its UI texture
namespace. New owners have distinct domains; move/resize preserve the domain; teardown/move-out
returns zero. Tokens are neither native handles nor serialized document identities. The host
retains only this value, resets upload acknowledgement on domain change, and rejects dead domains.

## Native regression gate

`editor.native_surface_lifetime` reuses one host across two native owners. It verifies one initial
atlas upload per owner, real native draw counters, zero rejected textures, moved-owner cache reuse,
100/125/150/200% DPI round trips, bounded steady-state vertex/index allocations, swapchain resize,
and rejected rendering after teardown. Resize may legitimately return a recoverable status; the
test waits for eight presented frames with a five-second deadline and never ends an unacquired frame.

Linux runs the executable on a separate Xvfb display and applies the same both-stream validation
error policy as the shell gate. The focused native test passed with Khronos core/synchronization
validation enabled: **1/1, 1.13 seconds**, both new owners reported one upload and zero rejections.
Windows/DX12 and available macOS/Metal hosts register the same executable. A runner with no Metal
surface is reported unsupported/skipped and is not native acceptance evidence.

Final validation:

- `cmake --preset linux-development` and `cmake --build --preset linux-development`: passed.
- `ctest --preset linux-development`: **117/117 passed**, none skipped, 177.28 seconds.
- `ctest --preset linux-development -R '^editor\.(native_surface_lifetime|linux_(validation_output|display_acceptance))$' -V`
  with Khronos core/synchronization validation and loader evidence enabled: **3/3 passed**;
  no validation diagnostics; actual device-layer insertion confirmed.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: passed.
  Shipping testing is OFF by preset.

The host/toolchain are those in the [Linux baseline](EditorEDM0-Linux-2026-10-05.md).
Strict validation uses `VK_INSTANCE_LAYERS=VK_LAYER_KHRONOS_validation` and
`VK_LAYER_ENABLES=VK_VALIDATION_FEATURE_ENABLE_SYNCHRONIZATION_VALIDATION_EXT`.
No physical-display, installed Windows IME, sanitizer, or visual-legibility result is implied.

Follow-up: the original resize case checked identity and upload counts, but not rejected bindings
after resize. The [native image/cache record](EditorEDM0-NativeImages-2026-10-06.md) adds that assertion,
reproduces Vulkan's swapchain cache loss, and records the fix plus a 4290-upload replacement soak.

## Final-head automated acceptance

Source `b2ca94f962329fa14d4629a67f37daff01ba12a9` integrates main `85e94f6`.
[Build run 37408629909](https://github.com/jimlee1972/Nexora/actions/runs/37408629909)
records Linux Development **140/140**, 312.90 s; Windows/DX12 **123/123**, 135.75 s;
macOS/Metal **122/122**, 96.46 s. These are hosted runs, separate from Linux workspace testing.

Exact-source local configure/build passed; full Linux Development **118/118**, no skips,
179.52 s; strict Vulkan **3/3**, 21.97 s, actual Khronos layer insertion and zero validation
errors/VUIDs/synchronization hazards. Engine-only Shipping configure/build and Shipping/Full
isolated native package acceptance passed with Zig 0.14.0.
Full commands appear in [PR #348](https://github.com/jimlee1972/Nexora/pull/348).

The first hosted Linux Shipping attempt failed during native window creation before interaction;
its failure remains recorded. One failed-job rerun was requested after local isolated acceptance
passed. The rerun and CI result succeeded: all 18 selected jobs passed. This overlapping PR was
closed as superseded by merged #354, which carries its lifetime fixes and the stronger native-image
resize follow-up.
Physical-display, installed Windows IME and visual-legibility acceptance remain open.
