# ADR-0002：原生 Scene mesh 批次

狀態：接受實作。日期：2026-10-04。

## 背景

Editor proxy 預覽目前提交一個 mesh 與多個 transform instance。實際場景需要在同一個有深度測試
的 viewport 繪製多種幾何。RenderSurface 擁有原生 presentation pass、上傳及完成 fence；Editor
不得取得 backend handle 或另建私有 pass。

## 決策

SceneDrawData 增加可選、借用的 SceneMeshBatch span。每筆記錄選擇共用 uint16 index 上傳中的
範圍，以及共用 instance 上傳中的範圍。Index 指向完整的共用 vertex 上傳。空 batches 保持原有
完整 mesh／全部 instance 繪製；空 instance span 仍表示一個 identity instance。

Vulkan 與 DX12 在配置或錄製前驗證所有範圍，只上傳一次陣列，並在既有 scene／depth pass
執行各 indexed draw。Batch 與 instance 最多 4,096，vertex 最多 65,535，index 最多
1,048,576。零 count、不完整三角形、未對齊三角形的起點、越界及溢位均拒絕，而且不消耗該幀
的 scene 提交機會。允許重疊範圍以支援 submesh 及重複幾何。MVP、光照、texture 與 base
material 為整次提交共用；transform 與 tint 維持每個 instance 獨立。

Span 只在 DrawScene 呼叫期間借用；返回後由 frame fence 保護已複製的 GPU 上傳。Resize、復原、offscreen
合成、裁切、UI 次序及每幀一次 scene 提交規則維持原約定。sceneDrawCalls 計算成功提交次數，
sceneInstances 計算上傳的 instance 記錄數量。

## 影響及驗證

既有呼叫端原始碼相容，但所有使用者須重新編譯 C++ descriptor。本變更不新增 serialized
scene 格式、stable C ABI 欄位、模組相依或 shader input。不支援的 backend 繼續回報
Unsupported。實際 mesh 的資產 residency 與每材質批次屬於後續功能，本擴充不代表 ED-M2 完成。

Portable contract 驗證範圍邊界與 uint32 溢位；Vulkan X11 pixel gate 使用不同 geometry／instance
範圍，並驗證多次錯誤 descriptor 後仍可成功繪製。執行完整 Linux Development 與 Shipping gate；
Windows CI 編譯 DX12。雲端執行不代表實體顯示器或實體 GPU 驗收。

Vulkan 會保留各 fence-protected frame slot 的有界 upload 容量；相同或較小提交會複製
最新 bytes 而不重新配置。放大時先完成新配置才釋放舊資源；resize／teardown 會 drain 並
釋放所有 slot。這不新增 per-asset residency，也不改變 source span 生命週期。
Native call-tracing／pixel 測試涵蓋配置失敗及 ownership。
