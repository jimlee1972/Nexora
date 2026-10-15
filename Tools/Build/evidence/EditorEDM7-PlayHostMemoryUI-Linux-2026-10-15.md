# ED-M7 — Scoped Play host memory GUI, Linux, 2026-10-15

Task: `nexora-0ua.1.14`. Base: `2a3cfd7d2005c8a89d3d228e9f1301c60c2cb43c`.
This is supporting [Editor roadmap](../../../Roadmap/en/Editor_Roadmap.md) work; full production
and physical milestone acceptance remain open. [JSON](EditorEDM7-PlayHostMemoryUI-Linux-2026-10-15.json)
retains implementation/test hashes and machine-readable results.

## Delivered behavior

The Game panel's Host memory button opens a nonmodal read-only window. It shows exact decimal
uint64 live/peak payload and allocation counts, rejected requests, and separately the latest
completed teardown's reclaimed payload. Playing/Paused values remain available; Stopped/failed
Create/Start distinguish a missing module from completed recovery. Missing data is explicit.

The application bridges PR602's provider through owning UUID/root/Content-generation/Play-generation
observations before drawing. Draw rechecks the active scope. A copied project with the same UUID,
root/generation replacement and detachment exclude previous teardown data. Scope replacement closes
the window and cancels a held open-button press. Same-root read-only inspection remains allowed.
No World/module/allocator borrow, IO or native memory query enters widget text rendering. Current
gameplay changes appear through the next owner publication; teardown serials saturate and never wrap.

Opening releases Game capture without a Play/authoring command; the window is not saved to layout.
No new C/Zig wire, module dependency, capture schema, file action or telemetry transport is added.
These bytes exclude allocator/map/native overhead, RSS, GPU and non-host resources, and are not
leak-freedom or total-memory/performance evidence. C++ UI consumers rebuild. See the
[embedding](../../../Apps/Editor/README.md#host-allocation-ownership-observations) and
[UI contract](../../../Engine/EditorImGui/README.md#scoped-play-host-memory-inspection).

## Targeted acceptance

- Final `cmake --preset linux-development`, `cmake --build --preset linux-development`:
  **passed, 565 build actions**. Final `ctest --preset linux-development`: **308/308**, **zero
  skipped**, **1,043.89 s**, on the recorded final 10 source hashes. The enhanced 1x/2x UI test,
  including first failed load after reactivation, passed **0.07 s**; the native memory window test
  passed **0.64 s**, with **9,607 changed RGB components**, exact payload/recovery/rejection values,
  two Console warnings and source bytes preserved. The unchanged native Play soak also passed
  **32 sessions / 128 ordered records, 81.87 s**, and 100k hierarchy lifecycle passed **156.30 s**.
  Final native reports and all raw soak RSS samples are retained separately from preflight in JSON.
  The preflight AppMain hash is retained to distinguish the earlier publication phase.

- `editor.play_host_memory_ui`: **passed, 0.06 s** after final scope/gesture coverage.
  Actual ImGui controls receive owner input at **1x and 2x**. Each scale runs 32 repeated paired,
  retained, failed-Create and failed-Start module cases plus initial live/peak/rejection inspection.
  It checks the displayed `18446744073709551615` rejection count, atomic malformed-value rejection,
  owning copies, Pause/Stop, fresh current counters, prior completed recovery, same-UUID/different-root
  projects, same-root read-only access, stale Play/Content generations, detach/reattach and a held
  button across project replacement. Source scene bytes and dirty state remain unchanged.
- `ctest --preset linux-development -R '^editor.play_host_memory_ui$|^editor.linux_play_host_memory$' -V`:
  **2/2, 0.72 s** before the final extra scope/gesture cases; native case **0.66 s**.
  The independent C-ABI fixture allocates 64 bytes plus 1 MiB, briefly raises the peak by 128 bytes,
  performs one rejected allocation and one wrong-owner release, then deliberately leaves the 1 MiB
  block for fallback reclamation. The real Editor displays **1,048,640 live bytes**, **1,048,768 peak
  bytes**, **1,048,576 reclaimed bytes**, and exact rejection counts **1/1**. Pause/Stop/restart and
  the separation of session 2's current payload from prior teardown pass. Both ended modules verify
  live page sentinels during Destroy and produce exactly **two warnings in the actual Console**.
  Native center-region readback changes **9,607 RGB components** when the memory window opens;
  this is UI pixel evidence, not OCR or a physical-display guarantee. Source scene bytes remain
  unchanged through shutdown. Test-only, opt-in geometry/text evidence comes from drawn controls.
- `python3 Tools/Build/TestDocumentationCI.py`: **16/16**.
- `cmake --preset linux-shipping` and `cmake --build --preset linux-shipping`: **passed**,
  **34 build actions**, Monolithic Minimal profile, Editor and BUILD_TESTING off.

Initial compilation corrected the bridge's const declaration to match the existing non-const
`PlayWorld()` accessor. The first native attempt used pre-Start button coordinates and clicked
Pause; the fixture now waits for the actual displayed current-session publication. Numeric/pixel
assertions and deadlines were retained. No Runtime SDK signature or unrelated production behavior
was changed to resolve either failure.

Review also found that asynchronous project activation can finish after BeginFrame. Publication is
now immediately before DrawProductShell can accept that project's first Play command, with a
first-failed-load-after-reactivation case. The initial full CTest run was interrupted to apply this
owner-phase correction; only the restarted final gate is used for completion evidence.

Linux uses GCC 14.2.0, CMake 4.4.4, real Zig 0.14.0 and Slang 2026.18, with the graphical shell,
TwoD and Showcase enabled in Development. The prescribed Minimal Shipping profile excludes Editor
and all tests by project policy; its build checks profile regression, not an Editor Shipping binary.
Native acceptance here is Xvfb/Vulkan at 1x; the 2x evidence uses actual ImGui controls with synthetic
owner input. Hosted validation and immutable merge provenance are recorded in the delivery PR and
Beads. Full production assets, long-duration, RSS/GPU/total allocator and physical budgets remain open.
