# Nexora Window 與 Native Presentation Roadmap

> 版本：v1.0｜狀態：規劃基線｜更新：2026-09-23

> **進度：60%**（WP-M0 至 WP-M2 已實作；WP-M1/WP-M2 必須通過 Windows/DX12 runner 才能驗收；WP-M3 與 WP-M4 尚未完成。）

## 1. 目的與 ownership

本 Roadmap 負責 Zig Showcase、圖形化 Editor 與未來 game executable 共用、目前缺少的
OS window 與 window-system presentation 邊界。RHI 繼續擁有 GPU resource/queue；platform window
module 擁有 native handle/event；presentation adapter 擁有 swapchain，並將 window 變化轉成
backend-neutral surface event。Zig gameplay 不得取得 native window、device、queue 或 swapchain pointer。

## 2. 現有基線

- ✅ Validation 與 native RHI device 可執行 deterministic offscreen workload。
- ✅ Renderer scene extraction 與 offscreen `Present` state validation 已有測試。
- ✅ `NexoraShowcase` 已有 headless lifecycle，並將 native presentation 標為 `CONTRACT ONLY`。
- 已實作、待 Windows 驗收：Win32 window/event translation 與 DX12 window-system swapchain；Showcase/Editor 整合仍未完成。

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

### WP-M1 — Win32 window 與 input

- 建立/關閉/顯示/resize Win32 window，支援 DPI-aware client size 與 deterministic event pump。
- 將 keyboard、text/IME、pointer、wheel、focus 與 close event 轉成 backend-neutral snapshot。
- 覆蓋重複 create/destroy、resize storm、minimize/restore 與 event queued 時 shutdown。

### WP-M2 — DX12 swapchain presentation

- 建立、acquire、render to、resize 與 present DXGI swapchain，不洩漏 DXGI/D3D12 type。
- 定義 backbuffer/fence ownership、frames in flight、vsync/tearing policy、color format 與 present diagnostics。
- 處理 occlusion、zero extent、surface loss、device removal 與 resize failure，不損壞 active generation。

Windows 驗收證據由 `window_presentation.contracts` 產生：測試會建立真實 Win32 視窗、acquire、clear 並 present 四個 DX12 frame、調整 swapchain 大小，且斷言診斷計數器。WP-M1/WP-M2 必須等 Windows/DX12 runner 綠燈後才能標為驗收；Linux 契約結果或 screenshot 均不足以單獨作為證據。

### WP-M3 — Showcase 與 Editor 整合

- 讓 `NexoraShowcase --mode=interactive --backend=dx12` 擁有可見輸出與 input。
- 提供 Scene/Game view 可重用的 render surface，不讓 Runtime 依賴 Editor。
- 保留 deterministic Linux headless path，fallback 必須顯示原因而不得靜默發生。

### WP-M4 — 其他平台與 hardening

- 在支援的 Linux/Windows host 加入 Vulkan window-system surface，並在 macOS 加入 Metal presentation。
- 驗證 multi-window/multi-surface lifetime、HDR/color-space negotiation、fullscreen、hot-plug
  與長時間 resize/device-loss stress。
- 分開記錄 target-host evidence；cross-compilation 不等於 runtime validation。

## 5. 驗證與 Definition of Done

- Unit/contract tests 覆蓋 ownership、event ordering、resize、zero extent 與 failure injection。
- Windows/DX12 runner 證明真正 acquire/render/present 並擷取 diagnostics；screenshot 只是
  補充視覺證據，不是 correctness oracle。
- Linux headless configure/build/test 不依賴 desktop display。
- Windowed shutdown 後不可留下 GPU resource、queued callback 或 native handle。
- Showcase 與 Editor 只使用 public window/presentation contract。
