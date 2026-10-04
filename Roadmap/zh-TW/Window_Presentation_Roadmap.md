# Nexora Window 與 Native Presentation Roadmap

> 版本：v1.0｜狀態：規劃基線｜更新：2026-10-02

> **進度：實作完成**（WP-M0 至 WP-M4 已實作；WP-M1/WP-M2 的 Windows/DX12 驗收已記錄，Linux Showcase Vulkan composition 已通過未跳過的 Xvfb/lavapipe 驗收；physical-display、Windows/Vulkan 與 macOS/Metal 仍須各 target-host runner。）

## 1. 目的與 ownership

本 Roadmap 負責 Zig Showcase、圖形化 Editor 與未來 game executable 共用、目前缺少的
OS window 與 window-system presentation 邊界。RHI 繼續擁有 GPU resource/queue；platform window
module 擁有 native handle/event；presentation adapter 擁有 swapchain，並將 window 變化轉成
backend-neutral surface event。Zig gameplay 不得取得 native window、device、queue 或 swapchain pointer。

## 2. 現有基線

- ✅ X11 修飾鍵事件現回報 transition 後的 flags、保留仍按下的左右配對鍵，並在失焦／銷毀時
  清除追蹤狀態。原生 Xvfb gate 驗證四組修飾鍵。

- ✅ Validation 與 native RHI device 可執行 deterministic offscreen workload。
- ✅ Renderer scene extraction 與 offscreen `Present` state validation 已有測試。
- ✅ `NexoraShowcase` 保留 headless lifecycle，並可擁有可重用的 native render surface。
- 已實作：Win32 window/input translation 與 DX12 presentation 已有 WP-M1/WP-M2 Windows 驗收證據；Showcase 整合仍是獨立的 target-host gate。

## 3. 必要 contract

- Backend-neutral `WindowDescriptor`、`WindowHandle`、`WindowEvent` 與 `SurfaceDescriptor`。
- 明確 ownership：application 擁有 window；presentation surface 不得比 window 長壽；銷毀
  swapchain/window 前必須先 drain GPU work。
- Event pump、resize、fullscreen transition 與 render submission 的 threading 規則。
- Unsupported backend、surface loss、out-of-date swapchain、minimized/zero extent、device loss
  與 display/DPI change 的可恢復錯誤。
- 由具 timestamp 的 window event 產生 input snapshot，不暴露 platform message struct。
- Native type 只存在 private platform/backend translation unit。

## 4. Milestones

### ✅ WP-M0 — Contract 與 module boundary

- 定義公開 window/surface descriptor、event、error、ownership 與 threading contract。
- 加入 feature option 與 module graph 宣告，不得讓 headless build 依賴 window SDK。
- 加入 fake-window/fake-surface lifecycle、resize coalescing、zero extent 與 teardown tests。

交付證據：`Nexora::Window` 與 `Nexora::Presentation` 的公開 type 全為 backend-neutral；module
README 記錄 ownership、lifetime、threading、resize 與 recovery 規則。兩個 module 均受
`NEXORA_ENABLE_WINDOW_PRESENTATION` 控制，dependency 已加入受驗證的 module graph，且
`window_presentation.contracts` 覆蓋 fake lifecycle gate。本 milestone 不宣稱已有 native window
或 swapchain backend。

### ✅ WP-M1 — Win32 window 與 input

- 建立/關閉/顯示/resize Win32 window，支援 DPI-aware client size 與 deterministic event pump。
- 將 keyboard、text/IME、pointer、wheel、focus 與 close event 轉成 backend-neutral snapshot。
- 覆蓋重複 create/destroy、resize storm、minimize/restore 與 event queued 時 shutdown。

### ✅ WP-M2 — DX12 swapchain presentation

- 建立、acquire、render to、resize 與 present DXGI swapchain，不洩漏 DXGI/D3D12 type。
- 定義 backbuffer/fence ownership、frames in flight、vsync/tearing policy、color format 與 present diagnostics。
- 處理 occlusion、zero extent、surface loss、device removal 與 resize failure，不損壞 active generation。

Windows 驗收證據由 `window_presentation.contracts` 產生：測試會建立真實 Win32 視窗、acquire、clear 並 present 四個 DX12 frame、調整 swapchain 大小，且斷言診斷計數器。2026-09-28，本機 x64 `windows-dx12-development` preset 以關閉 Vulkan 的 native D3D12 path 建置並通過此 contract，已記錄 WP-M1/WP-M2 的 target-host evidence。Linux 契約結果或 screenshot 均不足以單獨作為證據。

### ✅ WP-M3 — Showcase 與 Editor 整合

- 讓 `NexoraShowcase --mode=interactive --backend=dx12` 擁有可見輸出與 input。
- 提供 Scene/Game view 可重用的 render surface，不讓 Runtime 依賴 Editor。
- 保留 deterministic Linux headless path，fallback 必須顯示原因而不得靜默發生。

交付證據：`NexoraShowcase --mode=interactive --backend=dx12` 會建立由 Presentation 擁有、可重用的
`RenderSurface`，pump input 並持續 present 至關閉；自動化可使用有 frame 上限的執行方式。Editor
Scene/Game view 可使用相同 public owner，不必讓 Runtime 新增 Editor dependency。Auto fallback 會輸出
並記錄原因，明確指定 DX12 時失敗不會 fallback，non-Windows contract test 則保留 deterministic
headless gate。Showcase command 的實際 Windows/DX12 執行仍屬 target-host 驗收證據，不宣稱已在 Linux cloud 驗證。

### ✅ WP-M4 — 其他平台與 hardening

- 在支援的 Linux/Windows host 加入 Vulkan window-system surface，並在 macOS 加入 Metal presentation。
- 驗證 multi-window/multi-surface lifetime、HDR/color-space negotiation、fullscreen、hot-plug
  與長時間 resize/device-loss stress。
- 分開記錄 target-host evidence；cross-compilation 不等於 runtime validation。

交付證據：Linux 使用 X11 window implementation 與 Vulkan WSI swapchain；Windows 可在 DX12 之外選擇 Vulkan；macOS 使用 Cocoa window 與 `CAMetalLayer`。Backend negotiation 會記錄選定的 present mode 與 color space；portable contract gate 覆蓋 fullscreen、multi-surface lifetime、2,048-cycle resize stress、zero extent、out-of-date、surface-loss 與 device-loss path。這些 source 與跨平台 contract 完成 implementation scope；WP-M1/WP-M2 Windows/DX12 runtime 驗收已記錄，✅ Linux Showcase Vulkan composition 在 2026-10-02 通過未跳過的 Xvfb/lavapipe 驗收（Development 67/67）；physical-display 與其他平台 runtime 驗收仍為獨立 target-host gate。參見[驗收紀錄](../../Apps/Showcase/evidence/V1-Phase-A-Linux-Vulkan-2026-10-02/acceptance.md)。

## 5. 驗證與 Definition of Done

- Unit/contract tests 覆蓋 ownership、event ordering、resize、zero extent 與 failure injection。
- Windows/DX12 runner 證明真正 acquire/render/present 並擷取 diagnostics；screenshot 只是
  補充視覺證據，不是 correctness oracle。
- Linux headless configure/build/test 不依賴 desktop display。
- Windowed shutdown 後不可留下 GPU resource、queued callback 或 native handle。
- Showcase 與 Editor 只使用 public window/presentation contract。

✅ Linux/Vulkan scene boundary 也通過 Xvfb/lavapipe 的 native indexed/depth/light/transform pixel 驗收與 600-frame normalized camera-input integration（Development 70/70）。參見 [Phase B 驗收紀錄](../../Apps/Showcase/evidence/V1-Phase-B-Linux-Vulkan-2026-10-02/acceptance.md)。Physical-display 與其他平台 gate 仍為獨立驗收。

## V1 Visual Showcase 後續整合（2026-10-03）

✅ Linux/Vulkan application 已在 Xvfb/lavapipe 呈現八個 live Runtime room、可讀原生 GPU UI、held keyboard／pointer control 與 210 秒導覽。Full package tooling 包含 content、可重現 ZIP／SHA-256 與 Linux shared-library isolated resolution。此為 V1 Visual Showcase 整合證據，不取代本 roadmap 既有平台 gate，也不代表新版 Windows clean-machine 圖形驗收。

✅ Vulkan/DX12 已實作原生 SceneDrawData hardware instance 與有界 fence-owned upload；Linux Vulkan 像素驗收確認獨立 translation/scale/tint，DX12 目標主機執行仍待驗收。

✅ 原生 Vulkan／DX12 SceneDrawData 現可指定範圍內的實體像素 viewport，供 docked Scene View 使用。Portable 邊界檢查與 Linux Vulkan 像素讀回驗證裁切；Editor 整合及 DX12 目標主機的 viewport 證據仍待完成。

✅ Vulkan/DX12 的 Scene UV/RGBA8 材質已提供有界且不可變的 scene-only texture ID、white fallback 與 fence-protected upload lifetime。Linux 像素驗收確認 sampler selection、快取重用與 resize 重傳；DX12 目標主機材質執行仍待驗收。

✅ 原生 owner 的 Offscreen → Main → UI → Present 已使用 Vulkan/DX12 fence 持有的 scene color 與 GPU copy，並拒絕重複 acquire 及未完成 copy 的錯誤順序。Linux Vulkan 像素、互動及同步驗證 gate 共 75/75 通過。套件內 Windows `accept-v1.ps1` 記錄隔離副本截圖、checksum 與原生計數器；新版實體顯示／乾淨主機驗收由使用者本地執行。

✅ 新版 Windows Full Shipping／DX12 隔離副本圖形驗收已在 [CI 37053279518](https://github.com/jimlee1972/Nexora/actions/runs/37053279518) 通過：14 筆 checksum、12 張可見截圖、asset／plugin 拒絕案例、339 次原生 graph／copy／present 及退出碼 0。[版本化 Windows CI 證據](../../Apps/Showcase/evidence/Windows-V1-Native-Graph-CI-2026-10-03/acceptance.md)。此 hosted VM 不代表實體顯示或獨立乾淨主機驗收；兩項仍由使用者本地完成。

✅ Windows DX12 本地 Development/Full 與 Shipping/Full 原生執行已記錄於
[Windows-V1-DX12-Local-2026-10-03](../../Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md)。
GTX 960 開發主機驗收涵蓋八個房間、primitive/texture/instance rendering、
原生 offscreen graph/UI/present、輸入／編輯／reload／animation 截圖、Lab asset/plugin 拒絕
以及完整 210 秒 guided tour。明確的 Development preset 與 stock PowerShell 5 預設路徑修正
讓本地文件流程可重現。乾淨主機與實體顯示操作聲明仍待驗收；V1 最終驗收尚未完成。
✅ Windows 本地驗收腳本現要求當次匯出、已修改場景的成功 snapshot round trip、
導覽重播歸零與 M5／M6 指定拒絕案例的實際輸入／輸出。Source／package hash 採固定 LF 位元組。
基底 e4a140139189 通過 Linux Development 77/77 與 Shipping package CI；
實體顯示與乾淨主機最終 gate 仍待驗收。

✅ SceneDrawData 現支援在單次 Vulkan／DX12 depth pass 中繪製有界 index／instance mesh 批次。
空批次維持既有繪製行為。Portable 溢位／範圍檢查與不同幾何的 Vulkan 像素驗證涵蓋 offset 及
錯誤後仍可成功提交；DX12 執行仍屬目標主機 gate。

2026-10-04 補齊 Metal Showcase scene／instance／材質／depth／copy 與 Cocoa 控制／Retina 原始碼，✅ macOS hosted Shipping/Full 隔離封裝已以 Metal scene／copy／UI／present 與 resize 執行八房間（96 幀；[紀錄](../../Apps/Showcase/evidence/V1-Metal-Hosted-CI-2026-10-04/acceptance.md)）；✅ 原生像素／輸入／depth／lifecycle CTest 已通過 macOS Development（73/73）與 mimalloc（63/63）；實體 Mac 畫面仍待驗收。Linux 發佈套件回歸已通過 80/80，原生 gate 無 skip。見[發佈證據](../../Apps/Showcase/evidence/V1-Distribution-Linux-2026-10-04/acceptance.md)。

✅ Optional row-major affine SceneInstance matrix 與共用 private model／inverse-transpose packing
已實作於 Vulkan／DX12／Metal（[ADR-0003](ADR-0003-Presentation-Affine-Instances.md)）。Portable test
驗證精確 point、normal、override 語意、determinant 相消的精確判定與無效輸入；Vulkan pixel
比較鏡像／剪切 instance 與獨立
烘焙的 geometry／normal。Legacy TRS、empty identity、batch budget 與 fence ownership 維持相容；
Editor Scene／Game 現透過 CPU gate 使用精確 matrix；physical-display／GPU 驗收仍為獨立項目。
