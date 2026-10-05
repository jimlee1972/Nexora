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
