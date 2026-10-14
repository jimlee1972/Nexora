# ADR-0009：有界的 Editor Scene／Game 同時原生預覽

狀態：已接受有界實作；驗證進行中。日期：2026-10-14。

## 背景

目前每個 acquired frame 只有一次原生 scene submission，顯示 Scene 時 Game 因此回退為檢視圖。
只移除 admission guard 並不安全：第二次 draw 可能覆寫第一次的 instance／material／geometry
upload、改寫 Vulkan descriptor，或釋放尚未提交的 command buffer 引用的 target。

## 決策

新增預設 false 的 `ISurface::SupportsBoundedSceneViews()`，由 `RenderSurface` 轉送。支援的
native adapter 在單次 UI submission 後，最多接受兩個明確指定且不重疊的 direct LDR／無 shadow
viewport；邊界相接可以接受。隱含全畫面、offscreen、HDR、shadow 與 UI 前的流程仍只接受一次
scene submission。Geometry／material／texture 驗證仍必要；拒絕的 descriptor、重疊或第三個 view
不消耗配額。只有 draw 成功錄製後才提交 admission，下一次成功 Acquire 才重設。

Editor 依 capability，在兩個目前的實體像素矩形不重疊時發布兩個 native canvas。不支援或重疊的
排列保留可見的 fallback。兩個 view 都使用當前 owning frame geometry；Game 保留隔離 Play clone
與凍結 CPU asset。Application boundary 不傳遞 backend handle，也不新增第二次 UI pass 或
readback／composite fallback。

## Ownership 與界限

每個 protecting-frame slot 最多擁有兩份獨立 scene upload 與 geometry-prefix key。Vulkan 額外
分開擁有每個 view 的 descriptor、depth／framebuffer 與 optional scene-target state，避免 direct
target 重建銷毀第一次尚未提交的 view。PBR pool 有界為每個 frame slot 兩組既有64材質配額。
DX12／Metal 在依序清除下一個不重疊 view 仍安全時保留既有 depth ownership；可變 scene upload
仍分開。進階 offscreen／HDR／shadow 資源繼續使用互斥路徑。第二份儲存採 lazy allocation，既有
per-view geometry／instance／material 限制仍適用；這不是完整 application／native memory 或
實體 GPU 預算的驗收聲明。

既有 immutable geometry cache 會在兩次 draw 間保留所有已接受 range 的 lease，直到下一個 frame。
第二個 view 錄製時的 eviction 不能讓第一個 view 失效。所有 view 的 upload／key／target 都在
原有 owner／fence 邊界 drain／釋放；resize／recovery 與 abandoned recording 必須涵蓋兩個 slot。
GPU timing 仍是一個包含 UI 與兩個 view 的 command-buffer interval，不推測 per-view 時間。

## 相容性與驗收

Public C++ consumer 因 additive capability 必須重建；預設 false 的 surface 保留既有 fallback。
Stable C／Zig layout、asset／resource identity、source format、shader 與 module dependency 不變。
這是支援性切片，不關閉 ED-M3，也不宣告 HDR、浮動視窗遮擋、任意 viewport 數量或實機
顯示／GPU／DPI 流程已驗收。

必要證據包含 portable admission 邊界／overflow／mode／reset、有意義的舊 guard 與 upload alias
回歸、Vulkan／DX12／Metal 不同的原生 PBR／geometry／UI 像素、重複 frame／resize／drain ownership，
以及真正 Editor Scene／Game 同時顯示的 Play 移動、pause／step／stop、authoring source／Undo
未變動。執行完整 Linux Development／Shipping 與最終來源的全部 hosted CI。Vulkan core／同步
驗證也必須拒絕 pipeline／render-pass 不相容；clear／load pass 除 attachment 相容，還共用
包含 color read 的相同 dependency。目前驗證仍進行中。
