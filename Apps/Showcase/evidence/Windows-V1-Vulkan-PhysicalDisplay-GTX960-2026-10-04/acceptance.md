# Windows Vulkan physical-display acceptance (GTX 960 developer machine)

✅ The Shipping/Full package built **with the Vulkan backend ON** passed the packaged verifier with
`-Backend vulkan -PhysicalDisplay -CompleteGuidedTour` on the developer machine's real desktop session
(physical monitor, NVIDIA GeForce GTX 960, driver 32.0.15.8180, Windows NT 10.0.19045).

The package came from CI job `showcase-windows-vulkan-package` (run 37132535617, commit `ecac94d8f9ca`),
not from a local build. Windows presets still default to `NEXORA_ENABLE_VULKAN_BACKEND=OFF`; the job
overrides it with `-DNEXORA_ENABLE_VULKAN_BACKEND=ON`.

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File <package>/accept-v1.ps1 `
  -PackageRoot <package> -EvidenceDirectory <evidence> -Backend vulkan `
  -ExpectedBuildId ecac94d8f9ca -PhysicalDisplay -CompleteGuidedTour
~~~

| Field | Observed |
| --- | --- |
| Verifier result | `status=PASS`, no issues, process exit 0 |
| `physical_display_verified` / `clean_host_verified` | `true` / `false` |
| Operator attestation flag | `physical_display_operator_attestation=true` (see below) |
| Backend | `vulkan`, `backend_fallback=false`, `software_rasterizer=false` |
| Native graph frames / presents | 15794 / 15794 (Offscreen, Main, UI, Present) |
| Build ID | `ecac94d8f9ca` |
| Checksums verified / screenshots | 14 / 25 |
| Interaction checks | all 9 passed (orbit/zoom, F1/F2, tour replay/pause, 210-second tour, scene modify/undo/play/reload, locale, held input/crouch/teleport, animation blend, lifecycle/pressure) |

Rendering and tour-completed captures were reviewed: the overlay reads "LIVE FRAME / VULKAN",
"RASTERIZER HARDWARE", ~16.5 ms frames, tour 210.01/210 s paused at step 7/7.

## What it took (two real defects found by this run)

1. **Illegal instruction.** The first CI-built package crashed with `0xC000001D` even in headless
   mode. Zig defaulted to the build machine's native CPU. Fixed by `-mcpu=baseline` (PR #229).
   This also affected any CI-built package, including the release workflow's.
2. **Present-time `out_of_date`.** The second package completed the whole tour and then aborted with
   `native graph Present failed: out_of_date`. Vulkan `Present` now schedules swapchain replacement
   and the Showcase drops the frame (PR #231). The accepted run is the third attempt overall
   (build `ecac94d8f9ca`); the earlier failures are described here, not retained as evidence.
3. The win32 Vulkan surface also did not compile before this work (`windows.h` must precede
   `vulkan_win32.h`; the `vulkan-1` import library lookup) — PR #225.

## How the attestation was made — read this

- `-PhysicalDisplay` is an operator attestation, supplied by Claude on the user's instruction; the
  user did not watch the run. Claude could not view the live window (the computer-use layer hides
  windows outside its allowlist), so the visual review is of the verifier's captured PNGs.
- Hosted CI has no GPU: it proves build/link/package only. This record is one machine, one driver.

## Still open

- Metal native-backend parity (needs a Mac), and Vulkan on other GPUs/drivers.
- Windows presets still build Vulkan OFF by default.
- The per-tag release workflow has had a dry run only, not a real `v*` tag run.

V1 final acceptance remains **PENDING**.

✅ 實機（GTX 960、實體螢幕）以 `-Backend vulkan -PhysicalDisplay -CompleteGuidedTour` 通過開啟 Vulkan 後端的
Shipping/Full 套件驗收：`status=PASS`、Vulkan 無 fallback 且非軟體 rasterizer、15794 次 graph/present、
25 張截圖、9 項互動檢查全過。套件來自 CI（commit `ecac94d8f9ca`）。過程中修了兩個真實缺陷：
Zig 原生 CPU 造成的非法指令（`-mcpu=baseline`），以及 Vulkan present 時 `out_of_date` 造成中止。
`-PhysicalDisplay` 聲明由 Claude 依使用者指示提供，使用者並未全程目視。Metal parity、其他 GPU 驅動、
首次真實 tag release 仍待完成，V1 最終驗收尚未完成。
