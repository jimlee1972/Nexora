# ED-M3 bounded concurrent native canvases — Linux, 2026-10-14

Beads: `nexora-rd7.1.2`. Initial base: merged PR595/main `4548168c`; latest integrated base: PR596/main `82f02460`.
This supports ED-M3; all eight Editor milestones and physical-host acceptance remain open.

## Boundary and ownership

The additive default-false C++ capability admits at most two explicit disjoint direct LDR/non-shadow
viewports after one UI submission. Touching edges are allowed. Full-surface implicit, offscreen,
HDR, shadow and before-UI modes remain exclusive. All geometry/material/texture validation remains;
rejected descriptors, overlap and third views do not consume successful-recording admission.

Each protecting-frame/view owns a lazy mutable upload and geometry-prefix key. Vulkan additionally
owns each view's complete scene targets/bindings and bounded 64-material descriptor group, avoiding
release or rewrite of first-view state. DX12/Metal preserve serial shared depth ownership and use
separate mutable uploads. Existing immutable mesh leases cover both unsubmitted draws. Owner/fence
resize/recovery/drain includes both slots. GPU timing remains one command-buffer interval.

Editor publishes two current clipped physical rectangles when disjoint, and suppresses both native
draws with UI fallback when overlapping. Game retains its isolated clone, frozen catalogs, camera
choice and debug ribbons. Source/Undo and input/pause/step/stop ownership are unchanged.
No stable C/Zig wire, serialized/resource identity, shader or module dependency changes; public
C++ consumers rebuild. See [ADR-0009](../../../Roadmap/en/ADR-0009-Editor-Concurrent-Canvases.md).

## Meaningful regressions and native proof

- The actual old Vulkan guard rejects the second draw: `second bounded Scene/Game viewport is
  unavailable` (CTest8,0.80s).
- A controlled fault performs the normal second upload then overwrites only first-view vertex
  bytes, preserving valid resource/index lifetime. The native pixel oracle fails `concurrent scene
  uploads aliased or lost UI pixels` (CTest8,3.78s). Source is restored byte-for-byte.
- Six restored renderer/UI/native checks pass **6/6,17.08s**, including existing Vulkan PBR and upload
  tracing. Native legacy pixels distinguish first red geometry, shifted second green geometry and
  blue UI outside both rectangles; overlap/third rejection and 29 draws/28 frames remain exact.
- Shared native PBR fixtures rewrite the same CPU arrays between calls. Seven pairs cover separate
  instance/geometry/palette differences, 64 slots per view across all frame slots, legacy/resident
  paths, malformed second requests, overlap/third rejection and zero-extent recovery. Actual Vulkan
  PBR pixels pass **7.59s**; the same fixtures are wired into hosted DX12/Windows-Vulkan and Metal.
  Those hosted results are still pending; no local non-Linux native execution is claimed.
- At 1x/2x, 32 Play lifecycles per scale verify concurrent rectangles, unsupported fallback, overlap
  suppression/restoration, paused Step, Stop, and unchanged source/Undo. Real Xvfb native Editor
  acceptance passes **5.47s**: both previews rasterize, Play motion leaves Scene pixels unchanged,
  Pause stable, real floating-window overlap/restoration preserves paused Game/Scene pixels,
  Step/Stop, copied diagnostics, Console ingress and unchanged authored file pass.
  Scene uses its existing blue geometry oracle; Game retains the warm OBJ oracle.
  A stronger native drag check additionally verifies pixels actually change during overlap, then
  restore exactly; real mouse-down/move frames are separated. This passes **6.06s**. The script
  refinement was applied before any full-gate native Game script case ran; production C++ and
  earlier full-gate cases remained unchanged. Paired JSON records both script hashes.

## Validation status

Linux 6.18.44 / GCC 14.2.0 / CMake 4.4.4 / clang-format 23.1.3, Xvfb/Vulkan lavapipe.
Graphical shell/SDK/Slang/Vulkan/gameplay simulation/scalar material are enabled; paired JSON records
exact option names and 19 source/config hashes. Development build passed 333 actions, Shipping
passed 17 actions (including Presentation/Vulkan), documentation passed 16/16 and 16 touched
C++/ObjC++ files pass formatting. The pre-upstream complete Development gate passed **270/270,836.16s**, no skips; all 270 individual
Passed results are present in the captured full-gate output. Native concurrent Editor 5.82s,
Vulkan PBR 7.57s and UI 0.21s pass. All 19 final source/config hashes and 273 local documentation
targets match; root README is unchanged. Final-head hosted CI still gates merge/branch deletion. Shipping Minimal/
Monolithic with Editor/ImGui/SDK off is not graphical Shipping acceptance.

HDR/shadow multi-view, arbitrary counts, general floating-window occlusion, full material/gameplay
and physical GPU/display/DPI/soak acceptance remain separate. Scoped slot bounds are not total
native/application memory or physical performance budgets.

## Latest-main integration

Before publication, another contributor merged PR596 (`82f02460`): the 2D C/Zig gameplay bridge
and compatible V3 host suffix. All 37 incoming paths were disjoint from this task; ordinary
fast-forward preserved all 19 owned source/config hashes. The full Development/Shipping gates
were repeated on this new canonical-header/runtime baseline. The 270-test result above
validates the previous base; the final integrated Linux gate passed **271/271,836.25s**, no skips.
Development 315 actions and Shipping 16 actions pass, including the upstream gameplay bridge.
Vulkan PBR 7.20s, UI 0.20s and native concurrent Editor 5.82s pass. Other contributors'
claims, ABI additions and source are preserved.

## Metal qualification review

After the Linux gate, review corrected the Metal-only upload accessor with a const overload used
by `MeshBuffer() const`. The file is excluded from Linux compile commands; the other 18 owned
code/config hashes are unchanged. Reconfigure/build of both Linux presets reports `ninja: no work
to do.`, establishing unchanged Linux build inputs. The paired JSON records pre/post Metal hashes.
The final Metal source requires actual final-head hosted macOS compilation and pixels before merge;
no local macOS execution is claimed. All 19 final file hashes, documentation/format and native-host
CI still gate delivery.

## Khronos validation regression

Initial published head `aeddd64` failed the push Xvfb job's native core/synchronization validation:
`VUID-vkCmdDrawIndexed-renderPass-02684`. Pixel oracles passed, but the clear render pass used to
create shared pipelines omitted the overlay/load pass's color-read destination access. Different
subpass dependencies made these render passes incompatible. This is a source defect, not a flaky
test; the failing head must not merge. Reproduction with the installed local Khronos layer failed
both Vulkan Scene/PBR tests in **9.05s** (CTest exit8).

The fix preserves color-read synchronization in both render passes and removes the divergent
overlay dependency. The existing pixel fixture now doubles as the permanent regression under
the existing CI validation step; no validation filter or workflow was weakened. With the same
layer and synchronization validation enabled, Vulkan Scene/PBR and both Renderer checks pass
**4/4,10.39s**, with verified layer loading and no `Validation Error`, `VUID-` or `SYNC-HAZARD-`.
The final complete Development gate passes **271/271,840.60s**, no skips. The fix rebuild
completed274 actions; the subsequent configure/build reported no work. Shipping passes9 actions.
Native concurrent Editor5.75s, Vulkan PBR7.49s and portable UI0.19s pass. Docs16, formatting16,
source/config hashes19 and local links273 pass. All new-head hosted CI still gates merge; earlier
hosted results establish the previous source only.
