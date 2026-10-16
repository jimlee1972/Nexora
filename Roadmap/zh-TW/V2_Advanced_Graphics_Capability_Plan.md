# V2 進階圖形能力計畫

狀態：V2-GFX-1 施工中；能力檢查基礎已通過 Linux 驗證。五個完整 gate 仍全部待完成。
新增日期：2026-10-16。

本文件為 V2 完整規劃與 AI 施工規劃共同採用的規範性範圍擴充，將尚缺的 DirectX 世代
渲染能力納入 V2 交付。V3 可延伸這些系統，但不再是首次交付目標。本文件規劃後續實作，
不宣稱規劃中的原生路徑已可用；已交付的 contract 變更另記錄如下。

## V2-GFX-1 施工狀態

✅ 能力檢查為第一個已交付的 portable／Linux 實作切片：獨立記錄裝置支援／tier、啟用狀態與後端實作的 snapshot；
有 SDK guard 的 DX12 feature／Shader Model 查詢；Vulkan 核心能力偵測；明確的優先／
fallback 選擇；PSO 建立前檢查；以及納入完整能力要求的 PSO cache identity。未知支援
一律拒絕啟用，數字版本可區分 SM 6.10 與 6.9，native feedback tier 90／100 對應 0.9／1.0。

平行錄製、依 fence 回收的 worker descriptor／allocator、compiler／
artifact 矩陣整合、CPU／GPU 基準，以及 V2-GFX-2～V2-GFX-5 原生執行仍待完成。
能力檢查測試或裝置硬體查詢不構成任何原生進階功能完成的證據。

### 驗收證據 — 2026-10-16

來源 branch：`feat/v2-graphics-capabilities`，基於 `5f3da989`。
Beads：`nexora-45y.1`；剩餘執行 gate 為 `nexora-45y.2`～`nexora-45y.6`。

| 命令／檢查 | 實際結果 |
| --- | --- |
| `cmake --preset linux-development` | 通過；GNU 14.2、CMake 3.31.6、Ninja 1.11.1；本機 cache 指定隔離的 X11 development prefix |
| `cmake --build --preset linux-development --parallel 4` | 通過，包含原生 Vulkan 原始碼及新增能力測試 |
| `ctest --preset linux-development` | 0 失敗：171 通過、3 個依賴顯示環境的 native Presentation 測試略過，共 174 項；75.62 秒 |
| `cmake --preset linux-shipping` 與 `cmake --build --preset linux-shipping --parallel 3` | 通過，Monolithic／Minimal 並啟用 LTO |
| 要求 native 的 `renderer.contracts`、`renderer.graphics_capabilities`、`renderer.v2_gpu_driven` | 3/3 通過，0 略過；Mesa lavapipe 24.0.5、Slang 2026.18；0.19 秒 |
| 變更 Markdown／雙語配對、`git diff --check`、異動檔案 clang-format 18.1.8 檢查 | 通過 |

Production toolchain 測試起初因環境缺少 `clang++` 失敗；準備 Clang 19.1.7 後，完整
development 測試已通過。工具相依安裝／解壓於 repo 外，未調整專案 SDK 基線或 optional
feature 預設值。

首次 Windows CI 編譯發現新 snapshot 型別與 WinGDI 的 `DeviceCapabilities` 巨集撞名。
型別已改為 `GraphicsDeviceCapabilities`，能力回歸測試也會先載入 Windows 標頭再載入
公開 RHI 標頭。此修正保留 consumer 的 SDK 巨集，不採 undef 處理；失敗 enum 也使用
`NoFailure`，避開 X11 的 `None`，Linux 啟用 Window 時會在 Xlib 後編譯能力檢查標頭。
修正後 head 仍須通過最終平台 CI 驗收。

原生測試沿用 Slang 的 `-target spirv -profile glsl_450 -fvk-use-entrypoint-name` contract，
產生 `vertexMain`／`fragmentMain` 與 `computeMain` artifact，設定
`NEXORA_REQUIRE_NATIVE_BACKENDS=1`、兩個 `NEXORA_SLANG_*SPIRV_PATH` 變數與 lavapipe ICD，
執行 `ctest --preset linux-development -R '^renderer\.(contracts|graphics_capabilities|v2_gpu_driven)$' -V`。
保留的能力輸出為 Vulkan backend、software device、vendor 65541、device 0、driver 1；
Compute／Indirect 在 1.0 支援、已啟用且已實作。平行錄製與 Tessellation 回報支援 1.0，
但未實作／啟用；其他進階能力維持 unknown 且不可用。Pipeline 檢查會在配置 native PSO 前
拒絕所有尚未實作的進階路徑；既有完整 GPU-driven 參考比對仍通過。

Portable 測試另覆蓋獨立 tier 編碼（含 feedback 90／100 與 DXR 1.2）、未知／不支援／
停用／未實作的拒絕結果、明確 fallback 耗盡、SM 6.10 比較，以及已快取的基線 PSO
不能放行進階要求。本 Linux 環境未產生 Windows／Apple native 執行結果或實體 GPU 效能
證據，既有平台 CI／target-host gate 仍須完成。環境原有 SwiftShader 因缺少既有 Vulkan
基線能力而被拒絕，未為取得通過結果而放寬要求。

## 基線與範圍

`5201d9f6e02d2a24406004174a50f61ad9cebf9b` 的原始碼查核已確認原生 DX12 Queue、
Command List、Descriptor Heap、傳統 Resource Barrier、Graphics／Compute PSO、Compute
Dispatch 與 Indirect Draw。沿用現有實作，不將其重複計為新增工作。Windows DXIL 建置使用
SM 6.6 與 SM 6.0 profile；原生 Presentation 也使用 SM 5.0。這些事實不代表已通過
DX12 Ultimate 或更新 Shader 功能的驗收。

可程式化 Shader 已存在；統一 Shader 架構屬於硬體特性。本次不新增 DX9／DX10／DX11 API
後端，而是納入其中尚缺的渲染能力 Tessellation。降低 CPU 開銷必須與現有序列提交路徑
實測比較，不能只憑 API 版本宣稱達成。

## 交付 Gate

所有完整 gate 仍為**待完成**，包含依能力啟用的路徑。Runtime fallback 能讓裝置繼續執行，
但不能取代加速路徑的實作或 target-host 驗收。

| Gate | V2 預定工作 | 相依 | 驗收與 Fallback |
| --- | --- | --- | --- |
| V2-GFX-1 | Capability／SDK／compiler matrix、多執行緒 Command Recording、CPU 開銷基準 | 既有 V2-M3 contract | 獨立查詢 feature tier 與 shader target；每個錄製 worker 獨占 allocator／list，依完成 fence 回收 allocator、descriptor 與資源。RenderGraph 管理相依順序與提交。比對序列／平行輸出，執行 validation、生命週期壓力及 CPU／GPU 計時；保留可選的序列錄製路徑。 |
| V2-GFX-2 | Tessellation、Enhanced Barriers | V2-GFX-1 能力與生命週期 contract | 新增 patch topology、Hull／Domain stage、reflection 與 pipeline／cook key；以有界參考場景驗證細分輸出，保留預先細分 mesh fallback。將 graph access／sync／layout 對應 Enhanced Barriers，涵蓋 UAV、copy、aliasing 與 queue handoff；以 GPU validation 比較傳統 barrier 路徑，兩條路徑均保留。 |
| V2-GFX-3 | DXR 基礎至 DXR 1.2、可選 Shader Execution Reordering（SER） | V2-GFX-1、V2-GFX-2 的相容 barrier 實作 | 實作 BLAS／TLAS 建置、更新、壓縮與生命週期，RT pipeline／shader table 及原生 ray dispatch；以確定性場景驗證 shadow／reflection／AO。查詢實際 RT tier，記錄需求中 DXR 1.2 與正式 SDK 的對應，並驗證其差異功能。SER 另設能力查詢與 compiler gate，比對重排／未重排結果；保留 raster lighting。 |
| V2-GFX-4 | Mesh Shader、Variable Rate Shading（VRS）、Sampler Feedback、DX12 Ultimate 覆蓋 | V2-GFX-1、既有 V2-M3 Compute／Indirect 基線 | 實作原生 Amplification／Mesh stage 與 meshlet 提交、依 VRS tier 啟用 rate／image 控制，以及 feedback map 寫入、resolve 與 streaming 消費。對照 indexed-indirect fallback 驗證剔除與幾何；驗證 VRS 品質／誤差界線並保留 full-rate fallback；與停用 feedback 的 streaming 比對 residency／mip 正確性。分別回報 DXR、Mesh Shader、VRS、Sampler Feedback tier；四條原生路徑均通過才宣稱 Ultimate 覆蓋。 |
| V2-GFX-5 | Work Graphs、Shader Model 6.9、Shader Model 6.10 Preview | V2-GFX-1 至 V2-GFX-4、固定版本的相容工具鏈 | 實作 Work Graph state object／program 選擇、backing memory 生命週期、node record ABI 與原生 graph dispatch；以有界 node fan-out／輸出對照 Compute＋Indirect 排程。於支援裝置編譯並執行 SM 6.9 專項案例，以及需求中 SM 6.10 Preview 的線性代數、可變 Group Shared Memory 案例。記錄正式發布規格與 feature 名稱；不支援或尚不可用的 preview 功能維持待完成。保留 Compute＋Indirect 與較低 SM shader variant。 |

施工順序為 V2-GFX-1、V2-GFX-2、V2-GFX-3／V2-GFX-4，最後 V2-GFX-5。
V2-M3 仍須通過原有 native parity gate 才能完成。V2-GFX 擴充 V2 渲染驗收並納入
V2-M12 hardening，不重編 V2-M0～V2-M12，也不重新開啟已驗收的里程碑。V2 最終驗收
另須通過本次新增 gate。既有 6/13 里程碑比例未計入本次擴充，不代表擴充後的整體百分比。

## 待實作的能力與架構 Contract

- Native API 維持 private。RHI 提供 backend-neutral 的能力可用性、tier、shader stage／
  pipeline descriptor 與明確 unsupported 結果；由 Renderer 選擇路徑。
- 區分硬體支援、OS／driver 就緒、SDK／compiler 支援、已建置 artifact、後端已實作與
  target-host 已驗收。不能由整體 DirectX 版本或 shader 編譯成功推定功能完成。
- 進階路徑與 preview 工具鏈採獨立 opt-in feature／profile，不得暗中提高基線 Windows SDK、
  Shader Model 或裝置需求。固定 Windows／Agility SDK、DXC、Slang、driver、OS、GPU
  與相關規格版本。
- Shader cook／cache key 納入 target profile、feature requirement、stage、compiler version、
  reflection／layout identity 與 variant generation；提交前拒絕不相容 artifact。
- RenderGraph 維持 GPU 資源相依與同步決策的唯一擁有者。平行錄製不得改變可觀察的相依
  順序或共用可變錄製狀態；啟用前先定義 per-worker descriptor 配置與 fence 回收機制。
- Vulkan／Metal 僅在支援且已實作時使用對等 native feature，否則選用並測試明確 fallback；
  不保證 API 名稱或進階 tier 完全對等。
- 保持既有 stable C ABI 相容；Gameplay／Editor 控制採 backend-neutral descriptor。
  Plugin 停用時不得引入 shipping dependency 或成本。

## 完成前必須具備的證據

每個 V2-GFX gate 都必須保留受測 commit、命令、工具鏈／裝置矩陣、能力查詢、實際選用
路徑、native API 執行證據、確定性參考比對、validation 輸出、fallback 結果與已知限制，
存入相關 test evidence。負向案例需覆蓋不支援 tier、缺少 artifact、停用 plugin，以及
stale／in-flight 資源。略過原生測試仍屬待完成；portable mock 只能作為 foundation 證據。

平行錄製基準需在相同 workload、裝置、build 與 warm-up 規則下，回報 CPU 錄製／提交
median 與 p95、draw／dispatch 數量、GPU frame time 及記憶體。效能驗收前先定義可量測
目標與允許退步預算；小型 workload 若平行錄製較慢，仍可使用序列路徑。不得推定 CPU
開銷已降低。

實作變更須通過完整 Linux development gate 及相關 Windows 硬體／SDK gate；影響 linkage
時另跑 Linux Shipping。Linux 驗證不能認證 DX12 功能。隨各路徑交付，補上 Windows GPU
validation 與 fallback 的 CI／target-host 證據。Preview 驗收以正式發布的工具鏈與可執行
target 為前提；尚不可用的功能不能靠編譯 stub 或僅 fallback 測試標記完成。

實作與驗收證據具備後才能使用綠色完成標記，並同步更新雙語文件、V2 規劃與 Roadmap 索引。

## 來源參考

- [V2 完整規劃](跨平台3D_Engine_V2_完整規劃書_v1_4.md)
- [V2 AI 施工規劃](跨平台3D_Engine_V2_AI施工技術與系統規劃_v1_2.md)
- [既有 V2-M3 原生執行計畫](V2-M3_GPU_Driven_Native_Execution_Plan.md)
- [RHI Contract](../../Engine/RHI/README.md)
- [Renderer Contract](../../Engine/Renderer/README.md)
- [Shader Library 與原生 Presentation 範圍](../../Shaders/README.md)
