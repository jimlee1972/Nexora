# Bounded main Editor 2D Game View — Linux, 2026-10-16

Beads: `nexora-zih.6`, supporting `nexora-zih`. Base: `be3b53a7b3b46ef931d0a0b3f7c2627fb8a5c6d9` (after PR618 Vulkan descriptors/native input acknowledgement).

The actual project Editor offers explicit 3D/2D Game view selection, preserving the 3D default.
The same Play clone/scene/pool supplies an owning canonical Sprite frame with frozen multi-Atlas
bindings. Geometry is appended after Play mutation to the current Game window's clipped GUI draw
list. Frame/thread/geometry/texture checks reject invalid packets before partial append. Rendering
advances no gameplay time. Disjoint native 3D Scene pixels remain independent; overlapping native
Scene submission is suppressed. Source/CBI schemas are unchanged.

Focused evidence before the complete gate:

- New/existing CPU tests: 3/3, 0.13 seconds; additional overlap/expired packet/foreign owner checks
  passed 1/1, 0.03 seconds. Actual View clicks, 2x DPI, Step/Stop, multi-Atlas pool order, shared GUI
  slot pressure and malformed packet atomicity are exercised. The existing 32-cycle host test
  additionally checks exact4096/4097 preview admission and wrong-thread extraction.
- Actual `editor.linux_native_game_twod`: 32 cycles, 37.10 seconds. Native blended Atlas/pool
  pixels, real 2D selection, Pause/Step/input/Stop, unchanged authoring files and concurrent 3D
  source pixels pass; zero Vulkan validation errors. Both sampled phases contain 2592 opaque
  base, 2592 blended and 2592 pool-only pixels. Swapchain encoding is observed as sRGB.
- Initial compilation used an ordered map for `AssetUuid`, which has no comparison operator;
  the cache now uses its existing hash. The fixture initially assumed separate scene/entity ID
  spaces; corrected to the actual mesh entity ID. The initial native palette assumed UNORM;
  actual captured pixels matched the existing supported sRGB attachment path. Acceptance now
  selects one coherent palette from the known canvas background and checks exact linear/sRGB
  colors with a two-code-value tolerance; it does not accept arbitrary colors or skip pixels.

The original 335-case full gate was interrupted during Hierarchy lifecycle after main merged
PR615's RHI capability/pipeline changes. The application changes were preserved, main was
fast-forwarded and the new RHI contract/diff reviewed. Legacy requirements default to empty, but
the C++ Device interface requires a complete rebuild. All gates are repeated on this combined
source; the aborted old-base run is not counted as a successful complete validation.

Final validation on the source hashes in the [machine-readable record](EditorEDM3-GameTwoDPreview-Linux-2026-10-16.json):

- `cmake --preset linux-development`, `cmake --build --preset linux-development`,
  `ctest --preset linux-development`: **337/337 passed**, zero skips, **1132.84 seconds**.
- Combined-source focused gate: **5/5**, **67.31 seconds**, including the actual
  Vulkan 320-dispatch serial/four-worker oracle and native movement/SAVE/OPEN acknowledgement,
  plus the three Editor CPU/native regressions. This includes main PR618's integration.
- Final native 2D acceptance: **32 cycles**, **58.929 seconds**, zero Vulkan
  validation errors, unchanged source bytes/pixels. Manual Step changes the opaque Clip region
  from green to red; native palette is sRGB.
  All 32 sessions start in 3D and require actual explicit 2D clicks.
- The Editor regressions include full-cache image/font uploads at DPI rebuild and surface-domain
  change; the CPU test additionally replaces the immutable Play owner.
  Native Metal adapter coverage checks 65 uploads accepted/66 rejected; Linux cannot run that
  adapter, so its runtime result belongs to the applicable macOS gate.
- Existing native 3D Play soak: **32 cycles**, **128 lifecycle records**, unchanged source bytes/pixels,
  zero validation errors. Warmed peak RSS growth **3,690,496 bytes**
  is within the **33,554,432-byte** regression guard; no leak-freedom claim.
- `cmake --preset linux-shipping`, `cmake --build --preset linux-shipping`: passed.
  This Minimal Monolithic configuration excludes Editor/testing.
- Actual graphical feature-OFF build at `/tmp/nexora-editor-20261016/2d-off` with
  `NEXORA_ENABLE_2D=OFF`, `NEXORA_ENABLE_ZIG_GAMEPLAY=OFF`: built `NexoraEditor` and
  `NexoraPlayGameplayModuleTests`, `NexoraPlayDiagnosticsTests`, `NexoraPlaySceneServicesTests`,
  `NexoraGameCameraTests`. Four tests passed, **0.21 seconds**; no 2D tests were registered.
  Editor, EditorCore and EditorImGui dynamic dependency tables contain no TwoD dependency.
- Documentation tooling: **16/16** tests; **ten** changed Markdown files pass validation.
  Thirteen touched C++/Objective-C++ files pass clang-format. Root README is unchanged.

No enclosing Editor/2D milestone or physical-device acceptance is claimed.

The initial PR619 head `7b2659c1` passed the complete local 336-case gate in 1119.64 seconds,
but Windows hosted compilation found MSVC C4456: the new Sprite snapshot local `frame` shadowed
the existing outer surface frame. Renamed it to `sprite_frame` without changing behavior and
then fixed two valid P2 findings: reset 2D selection/input focus for a new session/immutable Play
owner and admit Metal's 64 image uploads plus the font. Added the lifecycle/full-cache regressions
above and repeated the complete Development, Shipping and actual graphical feature-OFF gates.
Initial-head results are preserved separately in the JSON and do not substitute for corrected-head CI.

Corrected head `bc0040c9` passed the complete local 336-case gate in 1138.27 seconds and all
18 push CI jobs, including Linux 343/343. Its first PR Linux run passed 342/343 and failed only
the unchanged standalone native keyboard acceptance; three local repeats passed, and one complete
failed-job retry was accepted. Main then merged PR618's Vulkan descriptor isolation and the native
input/SAVE acknowledgement fix at `be3b53a7`. Integrated that main cleanly, kept all 16 Editor
source hashes unchanged, and repeated the combined-source 337-case gate and supporting checks.
The old-base retry is superseded and is not final combined-source acceptance.

The preview admits up to4096 scene/pool inputs, 100,000 poses and 16,384-pixel extents. It shares
the GUI's 64 live images, 1024x1024-image and 16MiB CPU-copy limits. Cached Atlas registrations
remain scoped to immutable Play ownership and clear before the next BeginFrame after Stop/scope
change. Native slots remain in the existing bounded cache until reuse/drain, with fence-owned
retirement; immediate GPU deallocation or an RSS/VRAM bound at Stop is not claimed.

Authored Sprite persistence, Tilemap binding, seamless DLL reload and physical acceptance remain
open. ED0/8, 2D9/11 and P0 6/8 milestone counts are unchanged.
