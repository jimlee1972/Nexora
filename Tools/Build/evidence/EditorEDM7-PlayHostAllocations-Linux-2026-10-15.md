# ED-M7 — Play host allocation ownership observations, Linux, 2026-10-15

Supporting task: `nexora-0ua.1.13`. Base: `54ad4387741e031df6e0d4ac1c5e03fd2bf68dae`.
This supports the [Editor roadmap](../../../Roadmap/en/Editor_Roadmap.md); it does not complete ED-M7.
Machine-readable source hashes, native samples and results are in the
[companion JSON](EditorEDM7-PlayHostAllocations-Linux-2026-10-15.json).

## Product behavior and scope

Previously, Play silently freed allocations left after module callbacks. The embedding now exposes
owning current-session and last-teardown payload observations and emits one bounded Gameplay Warning
when fallback reclamation is required. Stop/Destroy can still use their live World and host blocks.
The warning runs after reclamation and service revocation; repeated Unload preserves the report
without repeating the warning. Failed Create/Start loads use the same fallback cleanup.

Live allocation count/bytes change only on successful tracked allocation and an exact
pointer/owner/size/alignment release. Peaks and saturating rejected-call counts survive in the last
teardown report, whose live totals are zero. New sessions reset current statistics. The original
16 MiB request/64 MiB session payload budgets remain unchanged; unbound allocation is rejected.
No log is added per allocation, and no C/Zig wire, module dependency or authoring format changes.
See the [owning contract](../../../Apps/Editor/README.md#host-allocation-ownership-observations).

These values exclude map nodes, allocator/native overhead, RSS, GPU and resources allocated outside
this host. Fallback recovery is distinguished from module-paired release, not presented as leak
freedom. Full production budgets, large assets, long-duration, plugin/project/device-loss and
physical-host acceptance remain open. The root README and milestone acceptance are unchanged.

## Verification

- `cmake --preset linux-development` (real Zig compiler and Showcase enabled),
  `cmake --build --preset linux-development`: **passed**, 566 build actions after restoring the
  executor. Exact configure overrides are retained in JSON. `ctest --preset linux-development`:
  **306/306**, **zero skipped**, **1,037.37 s**. This includes the new allocation test, the unchanged
  native Play soak (**81.60 s**), 100k hierarchy scale/lifecycle (**159.37 s**) and existing native
  gameplay/input/Scene/Game/prefab/Console regressions. Implementation/test hashes remained unchanged
  throughout the final gate. No linkage boundary changes require a new Shipping gate for this slice.

- `ctest --preset linux-development -R '^editor.play_host_allocations$' -V`: **1/1**, 0.01 s.
  The same owner runs 32 normal, retained, failed-Create and failed-Start sessions. All source
  scene bytes remain unchanged. Exactly 24 fallback warnings reach the real Core asynchronous
  producer and bounded Console ingress, with Warning severity and Gameplay category; normal
  paired releases produce none. A throwing sink cannot interrupt cleanup.
- Independent expected payload values verify ownership/size/alignment refusal, exact release,
  copied snapshots, quota admission, rejection counts and peaks: 3 × 16 MiB + 64 bytes live at
  the quota probe, five rejected allocations and four rejected deallocations. Repeated Unload
  neither clears the last report nor duplicates warnings; revoked callbacks cannot allocate.
- After executor recovery, the toolchain/Vulkan/native Play preflight passed **3/3**, 82.72 s.
  The unchanged native soak passed **32 sessions / 128 ordered callback records**, 81.38 s;
  source pixels/bytes were unchanged. Warmed RSS baseline 161,837,056 bytes, peak 162,639,872,
  end 158,953,472: growth **802,816 bytes**, within the existing 33,554,432-byte envelope.
  Raw samples are retained in JSON. This native regression run measures process residency,
  separately from the new host-payload test; it does not measure native Console warning pixels.
- `python3 Tools/Build/TestDocumentationCI.py`: **16/16**.
- `clang-format --dry-run --Werror Apps/Editor/PlayGameplayModule.h Tests/EditorImGui/PlayAllocationTests.cpp`:
  both touched C++ files pass. `git diff --check` passes.

The reconstructed executor initially lacked `clang++` and a Vulkan ICD. Its first full run was
interrupted after those environment failures. Task-local Clang 19, Mesa lavapipe, Xvfb and shader/
Zig tools were restored, and the preflight above passed. No assertions, thresholds or timeouts were
weakened. The environment uses GCC 14.2.0, CMake 4.4.4, Zig 0.14.0 and Slang 2026.18;
Development has real Zig gameplay, Showcase, TwoD and the graphical shell enabled.

Hosted checks and immutable merge provenance are recorded in the delivery PR and Beads after
completion. Linux virtual-display results do not establish physical Windows/macOS/GPU acceptance.
