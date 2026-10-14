# ED-M3 bounded opaque Play debug lines — Linux, 2026-10-14

Beads: `nexora-rd7.2.8`. Initial main `ffc8932e`, with reviewed PR594 dependency
`77243d47`. PR594 merged as `f637eea5`; its tree `ff28f5cc` equals the reviewed dependency
exactly. The unpublished branch was replayed onto merged main with all 12 source/config hashes
preserved.
This supports ED-M3; all eight Editor milestones remain open and physical-host acceptance
is not established.

## Contract and ownership

The existing V3 callback copies at most 256 opaque `0xRRGGBBAA` requests (alpha255).
Finite coordinates have absolute bound 99,999; length must exceed 0.000001; finite duration
is 0–3600 seconds. Null/malformed requests return InvalidArgument; unbound/full queues return
Lifecycle, preserve admission state and increment the scoped service-error counter.
`debug_lines` counts retained requests rather than visible native lines/pixels.

The owner advances gameplay duration before fixed ticks and Update in each playing frame.
Zero-duration requests retire at the next advance; use positive duration at startup to cross the
first boundary. Paused rendering/Step do not age lines; fixed Step can append a request.
The fixed queue allocates nothing. Stop/Destroy retain access; normal/failed unload clears it.
Frame-owned geometry survives module and World teardown without borrowing request storage.

Game View appends depth-tested camera-facing two-sided 0.06-world-unit ribbons using the existing
neutral Lambert/PBR material slot and one native scene submission. Authored geometry has priority
under existing vertex/index/instance budgets.256 requests add at most 1024 vertices/3072 indices/
256 instances. Ribbon scratch arrays are reused per frame. Debug frames use protecting-frame
uploads, avoiding retention of transient combined meshes in the immutable cache.
No native resource/World/entity borrow, stable C wire/version/capability, source format, shader,
module dependency or backend change is introduced. Private C++ consumers rebuild.

## Regression and native evidence

- Valid old-host availability regression fails `Play debug line service unavailable` (CTest8).
  An initial incomplete test loader was corrected before collecting this expected old-host result.
- A controlled no-expiry fault fails `duration expiry/order/boundary is wrong` (CTest8), then the
  original queue bytes are restored. Seven focused Play/native checks pass **7/7,5.45s**.
- 32 isolated Play lifecycles cover copied wires, finite/alpha/degenerate/duration rejection,
  exact expiry/order/large deltas, invalid-clock preservation,256/full capacity, actual paused
  FixedStep admission, source/Undo, failed-start Destroy access and reset, owning frame geometry,
  maximum render budgets and preservation of an existing 64-slot PBR palette.
- The C-ABI-only dynamic fixture removes the sole cloned mesh. Actual native green pixels must
  originate from debug requests; fixed motion, stable Pause, Step/Stop, copied diagnostics,
  unchanged authoring bytes and Core/gameplay Console records pass (**5.29s** after restoration).
  Stop/Destroy both admit requests. Dense readback avoids sparse sampling of thin ribbons; the
  initial subpixel-width failure was corrected to 0.06-world-unit geometry before this result.

## Validation status

Host: Linux 6.18.44, GCC 14.2.0, CMake 4.4.4, clang-format 23.1.3, Python 3.12.14;
Xvfb/Vulkan lavapipe (`lvp_icd.json`). Graphical shell/SDK/Slang/Vulkan/gameplay simulation/scalar
material features are enabled in Development. The paired JSON records the exact option names.

The pre-review Development build passed (**254 actions**) and the full gate passed **267/267,
838.92s**, no skips. Pre-review Shipping passed (**7 actions**, Minimal/Monolithic,
Editor/ImGui/SDK off; this is not graphical Shipping acceptance). Documentation tests passed 16/16.

Subsequent self-review added a regression with the camera facing along +X and a vertical ribbon:
the default tangent parallels the normal, failing `PBR ribbon tangent is not orthogonal to its
normal` (CTest8). The corrected helper derives a unit tangent along the segment; empty queues also
return before scratch allocation. These changes received a fresh complete Development/
Shipping gate on merged main `f637eea5`; the earlier267-test result is not the final-source gate.
Post-review build passed 281 actions, focused Play/native checks 7/7 in 5.49s (native 5.32s),
and Shipping passed 7 actions. The final-source full gate passed **267/267,845.17s**, no skips (including native debug5.29s).
Nine touched C++ files pass clang-format; documentation passes16/16; all 12 final source/config
hashes, evidence links, final diff and unchanged root README are checked. Hosted final-head CI
still gates merge and branch deletion. Exact 12 source/config
hashes and the separate pre-review hashes are in the paired JSON.

Alpha/unlit HUD/fixed-pixel strokes, combined native canvases, complete gameplay/devices/hot reload,
native debugger, physical GPU/display/DPI/soak and full ED-M3 acceptance remain open. Native evidence
here proves Linux Xvfb/lavapipe, not other desktop GUI or physical-host execution.
