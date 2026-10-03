# Windows physical-display acceptance (GTX 960 developer machine)

✅ The Shipping/Full package passed the packaged verifier with `-PhysicalDisplay -CompleteGuidedTour`
on the developer machine's real desktop session (physical monitor, NVIDIA GeForce GTX 960,
driver 32.0.15.8180, Windows NT 10.0.19045). Command (from `build/RUN-PHYSICAL.cmd`):

~~~powershell
powershell -NoProfile -ExecutionPolicy Bypass -File build/windows-showcase-shipping/package/NexoraShowcase-Shipping/accept-v1.ps1 `
  -EvidenceDirectory build/windows-showcase-shipping/artifacts/v1-physical-display `
  -ExpectedBuildId e4a140139189 -PhysicalDisplay -CompleteGuidedTour
~~~

| Field | Observed |
| --- | --- |
| Verifier result | `status=PASS`, no issues |
| `physical_display_verified` / `clean_host_verified` | `true` / `false` (this is not a clean host) |
| Operator attestation flag | `physical_display_operator_attestation=true` (see below) |
| Backend | DX12, `backend_fallback=false`, `software_rasterizer=false` (the verifier rejects a software rasterizer under `-PhysicalDisplay`) |
| Native graph frames / presents | 15649 / 15649 |
| Build ID | `e4a140139189` |
| Checksums verified / screenshots | 14 / 25 |
| Interaction checks | pointer orbit/zoom, F1/F2 toggles, tour replay/pause, 210-second tour, scene modify/undo/play/reload, locale, held input/crouch/teleport, animation blend, lifecycle/pressure |

Retained here: `acceptance.json`, launch reports, Lab exports, tour/reload state JSON, the 25 PNG
captures and the verifier stdout/stderr logs. Rendering and tour-completed captures were reviewed
(real DX12 3D scene, 17.5–18.5 ms frames, tour 210.01/210 s, paused at step 7/7).

## How the attestation was made — read this

- The `-PhysicalDisplay` flag is an operator attestation. It was supplied by Claude on the user's
  instruction to run the remaining acceptance on the local machine; the user did not separately
  watch the run. Claude could not view the live Showcase window (the computer-use layer hides
  windows outside its allowlist), so the visual review is of the verifier's captured PNGs.
- A first attempt failed with `Window client is empty.`: Claude's own mid-run screenshots hid the
  Showcase window. The accepted run is the second one, with no screenshots taken during the run.
- If you need an independent human-witnessed physical-display record, re-run the command above
  while watching the screen.

## Still open

- Vulkan/Metal native-backend parity on their target hosts (the Windows presets build with
  `NEXORA_ENABLE_VULKAN_BACKEND=OFF`; no Vulkan or Metal path was exercised here).
- The per-tag release artifact workflow. Audio/video/WebView remain contract-only/unavailable.
- Note: the Shipping room's on-screen line still reads "CLEAN WINDOWS LAUNCH / PHYSICAL DISPLAY
  ACCEPTANCE PENDING" because it is a built-in string; the app was not rebuilt for this record.

V1 final acceptance remains **PENDING**.

✅ 實機（GTX 960、實體螢幕）以 `-PhysicalDisplay -CompleteGuidedTour` 通過 Shipping/Full 套件驗收：
`status=PASS`、DX12 無 fallback 且非軟體 rasterizer、15649 次 graph/present、25 張截圖。
`-PhysicalDisplay` 聲明由 Claude 依使用者指示提供，使用者並未另外全程目視；首次嘗試因 Claude 自己的
截圖遮蔽視窗而失敗（`Window client is empty.`），本紀錄為第二次執行。Vulkan/Metal parity 與每個 tag 的
release 流程仍待完成，V1 最終驗收尚未完成。
