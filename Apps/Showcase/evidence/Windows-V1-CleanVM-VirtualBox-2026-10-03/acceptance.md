# Windows clean-VM acceptance (VirtualBox, Windows 10)

✅ The Shipping/Full package passed the packaged verifier `accept-v1.ps1 -CleanHost -CompleteGuidedTour`
inside the independently provisioned clean Windows 10 VM `Nexora-ZS-M5-CleanMachine-Win10`
(Base-NoMedia; created from the supplied Windows ISO, previously used for ZS-M5). Physical-display
attestation was **not** supplied (`-PhysicalDisplay` omitted).

| Field | Observed in guest |
| --- | --- |
| Verifier result | `status=PASS`, exit code 0 |
| `clean_host_verified` / `physical_display_verified` | `True` / `False` |
| Package | `NexoraShowcase-Shipping.zip` SHA-256 `09b0fda20980bf50e50c61991a41b45c3906d7f5e500fb63947b3d8eba1513b4` (matches host value) |
| Checksums verified by the verifier | 14 |
| Screenshots captured by the verifier | 25 |
| Build ID | `e4a140139189` |
| Backend | DX12, `backend_fallback=False`, `software_rasterizer=False` as reported by the app |
| Native graph frames / presents | 3861 / 3861 |
| Issues | none |
| OS | Microsoft Windows NT 10.0.19045.0 |
| Adapter | VirtualBox Graphics Adapter (WDDM) 7.2.20.25154 |
| Dev tools present (cmake, cl, zig, git) | none detected |

Method: the package ZIP was delivered to the guest as a read-only ISO (the guest has no Guest
Additions), copied to `C:\NexoraTest`, extracted, and verified/run by the packaged script. The ISO
helper scripts only copy, hash-check and invoke `accept-v1.ps1`; they do not alter the package.

Evidence in this directory: `zip-checksum-match.jpg`, `live-frame-shipping.jpg` (DX12, rasterizer
hardware as reported, build E4A140139189), `tour-198s.jpg` (guided tour step 7/7 at 198.54/210 s) and
`summary-console.jpg` (verifier summary). The in-guest `acceptance.json` and PNGs were not exported
from the VM; the summary above is transcribed from the guest console screenshot.

## Limits (do not over-read this record)

- The GPU is a **VirtualBox virtual adapter**, not a physical GPU or a physical display. The app
  reports a hardware rasterizer for it, but this is not physical-display or physical-GPU acceptance.
- V1 final acceptance remains **PENDING**: physical-display operator attestation, Vulkan/Metal
  native-backend parity on their target hosts, and the per-tag release artifact workflow.
- Audio/video/WebView adapters remain contract-only/unavailable.

✅ 乾淨 Windows 10 VM（VirtualBox，無開發工具）通過 Shipping/Full 套件驗收：`status=PASS`、
`clean_host_verified=True`、14 個 checksum、25 張截圖、DX12 無 fallback、3861 次 graph/present。
GPU 為 VirtualBox 虛擬顯示卡，**不是**實體顯示器驗收；`physical_display_verified=False`，
V1 最終驗收仍待完成。
