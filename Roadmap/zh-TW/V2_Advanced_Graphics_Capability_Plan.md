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

完整平行錄製、依 fence 回收的 worker descriptor／allocator、compiler／
artifact 矩陣整合、CPU／GPU 基準，以及 V2-GFX-2～V2-GFX-5 原生執行仍待完成。
能力檢查測試或裝置硬體查詢不構成任何原生進階功能完成的證據。

✅ Vulkan Compute Descriptor 隔離為下一個已交付前置項目：以 Command List 自有 Pool
及每次 Dispatch 不可變的綁定快照，取代裝置共用的可變 Set。原生測試於序列與四個 worker
錄製模式比對 320 組不同 workload，涵蓋每份 List 40 次 Dispatch（跨越單一 Pool 的 32 Set
容量）、最後只綁定但不 Dispatch、所有 worker join 後反向提交、確認完成才銷毀，以及
缺少／過期綁定的拒絕。輸出 identity、分類資料及間接繪製前綴均符合 CPU 參考結果。
Snapshot 中平行錄製能力仍未啟用／未實作；完整 RenderGraph 排程與其他後端 worker 所有權尚未完成。

### Metal 完整 Kernel Compute 與 Worker 所有權 — 2026-10-17

✅ Metal 路徑現可選用真正由 Slang 產生的 `GPUDriven.metal`，驗證指定 Artifact 且不以 No-op
代替；四個 Storage Binding 存於各清單，Encoder 邊界後可重播，Worker 使用 Autorelease
Pool，原生物件由 ARC 保留。Shared-buffer 測試讀回會等待完成並檢查範圍。有限且獨立的
Buffer-only Compute 檢查涵蓋 Metal；完整平行 Feature 檢查仍未啟用。

macOS 原生 Gate 現會執行完整 Culling／Distance／LOD／Hi-Z／Compaction／Classification／
Indirect Generation，並要求通過既有 CPU Oracle。`renderer.metal_recording_isolation`
共用 320 組隔離 Dispatch、48 組序列／四 Worker WAW Graph 比對、有時限重疊及失敗批次
Sentinel 保留／重用；另測試 WAW Encoder 邊界的 Binding 重播與 Offset／空範圍／非法／
失效 Buffer 讀回。Native-required 組態缺少真正 Artifact／Device 時失敗；Optional 組態
明確略過。這項 Gate 已超越先前的 No-op Smoke。

來源分支：`feat/v2-metal-compute-recording`；Beads `nexora-45y.2.5`。
本機啟用 Slang 的 `cmake --preset linux-development` 與
`cmake --build --preset linux-development` 通過。Xvfb／Native-required 的
`ctest --preset linux-development` **228/228 通過，零略過／失敗，284.16 秒**，包含真正
Metal Source 產生及原生 Vulkan 執行（Slang 2026.18、Mesa lavapipe 24.0.5、Zig 0.14.0）。
最終平台 Guard 後聚焦執行
`ctest --preset linux-development -R '^(build\.shader_crosscompile|renderer\.(v2_gpu_driven|vulkan_descriptor_isolation|contracts|graphics_capabilities|parallel_graph_recording|metal_source_contract))$' --output-on-failure`：
**7/7 通過，零略過，0.34 秒**。`cmake --preset linux-shipping` 與
`cmake --build --preset linux-shipping` 通過（Monolithic／Minimal、LTO）。本次 C++ Format
及 `git diff --check` 通過。原生 Artifact 缺失拒絕另有專用 macOS 測試。

Linux Slang Cross-compilation 僅提供 Artifact 證據；macOS 執行驗收須通過 Hosted CI。
完整 V2-M3 Parity、GPU Validation、Graphics／Texture／共用讀取並行、非同步 Queue／
重用回收、Compiler／Artifact 就緒矩陣及同條件實體 GPU 效能仍待完成。
Shader／SDK 基線及 Stable C ABI 不變。詳見
[Metal 所有權 Contract](../../Engine/RHI/README.md#metal-gpu-driven-compute-and-worker-ownership-v2-gfx-1-slice)。

### D3D12 隔離 Buffer 錄製 — 2026-10-17

✅ D3D12 的 Buffer 上傳／狀態準備現由各命令清單擁有；錄製與放棄清單不會修改裝置已提交的
狀態。提交會先檢查每個 Buffer 的 Generation／Revision，再執行原生清單；Queue 接受後才
提交已消耗的上傳及 `COMMON` Decay。上傳、已接受的提交與非空讀回會使舊快照過期，遭拒
清單必須重新錄製。Texture 狀態準備仍須序列執行，未取得 Rollback 語意。

有限的 `independent_compute_recording` 檢查現涵蓋 D3D12 獨立且僅使用 Buffer 的 Compute
Pass；完整平行 Feature 仍未啟用／未實作。Windows 專用的
`renderer.d3d12_recording_isolation` 共用既有 Vulkan Workload：320 組獨立 Dispatch 比對、
48 組序列／四 Worker Graph 比對、強制 Callback 重疊、放棄批次 Sentinel 與成功重用。
另驗證放棄初次上傳／狀態準備、上傳／提交／讀回／失效 Handle 的快照在執行前遭拒，
並進行六次 CPU 精確比對。

來源分支：`feat/v2-d3d12-recording-isolation`，基底 `8a12a0e2cf40ab524c659d868bfa447a908655e3`。
Beads：`nexora-45y.2.4`；父任務 `nexora-45y.2` 與五個完整 Gate 均維持待完成。
本機驗證：`cmake --preset linux-development` 與 `cmake --build --preset linux-development`
通過；Xvfb／Native-required 的 `ctest --preset linux-development` **194/194 通過，零略過／失敗，
97.62 秒**，使用 Slang 2026.18 SPIR-V、Mesa lavapipe 24.0.5 與 Zig 0.14.0。聚焦執行
`ctest --preset linux-development -R '^renderer\.(parallel_graph_recording|graph_buffer_dependencies|vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven)$' --output-on-failure`
**6/6 通過，零略過，0.28 秒**。`cmake --preset linux-shipping` 與
`cmake --build --preset linux-shipping` 通過（Monolithic／Minimal、LTO）。本次 C++ 的
`clang-format --dry-run --Werror` 及 `git diff --check` 通過。

原生 Windows 執行須由 Hosted CI 證明，Linux 不認證 D3D12。未宣稱實體 GPU 計時、
GPU Validation Layer 結果、共用讀取／Texture 並行、Metal Worker 所有權、非同步 Queue
分離或 Allocator 重用回收。Compiler／Artifact 就緒矩陣與同條件效能 Gate 仍待完成。
Shader／SDK 基線與 Stable C ABI 不變。詳見
[RHI 快照 Contract](../../Engine/RHI/README.md#d3d12-buffer-recording-snapshots-v2-gfx-1-slice)。

### 有界 Graph Compute 錄製 — 2026-10-17

✅ Graph 排程現可明確 opt-in，使用既有 JobSystem，以有界批次平行錄製獨立且僅使用
Buffer 的 Compute Pass。Validation／Vulkan 提供獨立的有限範圍檢查，完整
`ParallelCommandRecording` Feature 仍未啟用。未標記的 Callback、Texture、其他 Queue／
後端、任何共用 Buffer（含讀／讀）、明確相依，以及缺少、停止或僅一個 Worker 的 Pool，
均保留序列執行。原有 `Execute(device)` 維持序列。

Coordinator 建立並擁有 Allocator／List，等待所有已啟動錄製工作結束，依編譯順序提交，
再等待各完成值後才釋放 List。錄製失敗的批次完全不提交，依 Graph 順序選擇 Callback
錯誤並停止後續 Pass。提交／等待失敗時先 Drain，再釋放 List。此為等待完成後回收，
尚非非同步 Worker 重用。

來源 branch：`feat/v2-parallel-graph-recording`，基底 `63bb5f7a49e5ec9df0b285494e79ee046c61a6f7`。
Beads：`nexora-45y.2.3`；父任務 `nexora-45y.2` 與完整 V2-GFX-1 維持待完成。

- `cmake --preset linux-development` 與 `cmake --build --preset linux-development`：通過。
- Xvfb 下要求 Native 的 `ctest --preset linux-development`：**194/194 通過、零略過／
  失敗、102.26 秒**，使用 Slang 2026.18 SPIR-V、Mesa lavapipe 24.0.5、Zig 0.14.0。
- `ctest --preset linux-development -R '^renderer\.(parallel_graph_recording|graph_buffer_dependencies|vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven)$' --output-on-failure`：
  **6/6 通過、零略過、0.27 秒**。
- `cmake --preset linux-shipping` 與 `cmake --build --preset linux-shipping`：通過，
  Monolithic／Minimal 並啟用 LTO，包含最終完成值等待實作。
- 本次 C++ 的 `clang-format --dry-run --Werror` 與 `git diff --check`：通過。

Portable 測試強制 Callback 同時執行及逆序完成，驗證固定提交與有界分配、相依批次、
資源／Queue／Texture／明確相依的序列 Fallback、既有序列介面、停止／缺少／單 Worker
Pool、確定性的錄製錯誤、失敗批次不提交，以及模擬延後完成裝置上的完成後回收。
提交失敗注入模擬原生 Queue 已接受工作，要求序列／平行兩條路徑都在 Allocator 釋放前
先 Drain。排程例外的設計也會等待每個已啟動 Job；未注入 JobSystem 內部的分配失敗。

原生 Vulkan Descriptor 測試保留 320 組隔離 Dispatch 比對，另以 24 組不同 Workload
執行兩輪具 WAW 相依的 Graph Pass，於序列／四 Worker 模式比對 48 次 CPU 參考結果。
有時限的 Rendezvous 要求四個 Callback 實際重疊執行。注入錄製失敗後，所有輸出 Sentinel
保持原值，沒有提交／讀回，所有 Worker 結束後可成功重用相同借用 Buffer。正式路徑
仍不讀回。此僅證明 Linux 軟體裝置正確性，不構成 CPU 開銷下降、實體 GPU 計時、
一般 Graphics／Texture 錄製、共用讀取並行、D3D12／Metal Worker 安全、取消或非同步
多 Queue 回收的證據。Compiler／Artifact 矩陣、同條件 CPU Median／p95、GPU 時間／
記憶體預算及所有完整 Gate 維持待完成。Shader、SDK、Feature 預設值及 Stable C ABI
均不變。詳見 [執行 Contract](../../Engine/Renderer/README.md#bounded-opt-in-parallel-compute-recording)。

### Buffer 相依與編譯後 Barrier 計畫 — 2026-10-17

✅ 下一個 V2-GFX-1 前置項目已加入借用 Buffer 的明確讀寫宣告、Texture／Buffer 的
RAW／WAR／WAW 邊、跨 Queue 或不同狀態／存取範圍的讀／讀排序，以及由 `Compile`
建立的每個 Pass 不可變 Barrier 計畫。全 Buffer 相依對應真正的 Vulkan Memory Barrier、
D3D12 狀態追蹤／UAV Barrier 與 Metal 追蹤式 Encoder 邊界。重複匯入別名會被拒絕，Buffer
仍由呼叫者擁有；同狀態連續 Storage 寫入仍會產生 WAW Barrier。

來源 branch：`feat/v2-graph-buffer-dependencies`，基底 `ee8b37d64c9342702b8ec176c6792f3148d79982`。
Beads：`nexora-45y.2.2`；父任務 `nexora-45y.2` 與完整 V2-GFX-1 gate 維持待完成。

- `cmake --preset linux-development` 與 `cmake --build --preset linux-development`：通過。
- 在 Xvfb 下執行 `ctest --preset linux-development`，要求 native Slang 2026.18 SPIR-V，
  使用 Mesa lavapipe 24.0.5 與 Zig 0.14.0：**191/191 通過，零失敗／略過，105.43 秒**。
- 聚焦命令 `ctest --preset linux-development -R '^renderer\.(graph_buffer_dependencies|contracts|v2_gpu_driven)$' --output-on-failure`：
  **3/3 通過，零略過，0.18 秒**。
- `cmake --preset linux-shipping` 與 `cmake --build --preset linux-shipping`：通過，
  Monolithic／Minimal 並啟用 LTO；因公開 C++ Command List 介面改變而要求此驗證。
- 對本次 C++ 檔案執行 `clang-format --dry-run --Werror`，以及 `git diff --check`：通過。

聚焦 Contract 在 Callback 前擷取精確 Handle／存取範圍／Queue，檢查重複重播計畫、
同狀態 WAW、等待所有先前讀者的交接、循環、無效／重複宣告、過期匯入、Callback 失敗
隔離與 Transient 初次使用的所有權。原生 GPU-driven 驗收已移除假 Texture 相依與手動
Compute→Draw 邊，宣告四個真實 Buffer，執行兩次具 WAW 相依的 Compute 寫入，再由
Indirect Read 推導相依，並要求 CPU／GPU 結果完全一致。正式錄製仍不讀回。
原生 Smoke 已接入各平台要求的 `renderer.contracts`；本機僅證明 Vulkan 軟體裝置
正確性。Windows／D3D12 與 macOS／Metal 結果須由 Hosted／目標主機執行取得；本機
未執行 GPU Validation Layer，也不宣稱實體 GPU 效能。

錄製／提交仍為序列，邏輯 Queue 仍對應單一同步原生 Queue。非同步 Queue Family 交接、
Worker 生命週期／回收、同條件效能、更多 Buffer 存取類別、Transient Buffer 分配及完整
平行排程仍待完成。未啟用任何進階 Feature Capability，未更改 Shader／Toolchain 基線，
Stable C ABI 不變。詳見 [Renderer Contract](../../Engine/Renderer/README.md#v2-graph-buffer-dependencies-and-compiled-synchronization)
與 [RHI 對應](../../Engine/RHI/README.md#whole-buffer-access-dependencies-v2-gfx-1-prerequisite)。

### Descriptor 隔離證據 — 2026-10-16

來源 branch：`feat/v2-parallel-command-recording`，基於已合併的能力檢查基礎 `4972c14b`。
Beads：`nexora-45y.2.1`；父任務 `nexora-45y.2` 維持待完成。
要求 native 的 Linux 命令為
`ctest --preset linux-development -R '^renderer\.(vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven)$' --output-on-failure`，
使用下方相同的 Slang 2026.18 artifact 與 Mesa lavapipe 24.0.5 環境。
四項測試全數通過且零略過（0.82 秒）；Descriptor 隔離測試耗時 0.27 秒。
完整 `cmake --preset linux-development`、`cmake --build --preset linux-development --parallel 4`
與 `ctest --preset linux-development --output-on-failure` gate 通過：171 項通過、4 項明確略過
（三項顯示測試與未提供 artifact 的本 native 測試），共 175 項，77.42 秒。
上述獨立要求 native 的執行已實際執行隔離測試且未略過。
此為軟體裝置正確性證據，不構成 CPU 開銷下降或實體 GPU 效能宣稱。
本機未安裝 Vulkan Validation Layer；GPU validation 與 target-host gate 維持待完成。
未調整 SDK／compiler 基線或 feature 預設值。Pool 目前隨單次使用的 List，在同步 native fence
完成後回收；非同步 worker 重用仍屬獨立 gate。

初次 hosted Linux push gate 發現獨立的 2D 原生輸入時序假設：兩次皆為 341/342 通過，
本次新增 Descriptor 測試均成功；相同 PR head 的 Linux gate 則通過。Product driver 現在
先確認真實位移及 checksum 正確的 SAVE，才執行 OPEN，不再從固定延遲推定位移。
修正後五次原生驗收、第一次輸入未送達的恢復、所有輸入未送達的拒絕，以及本機啟用 Zig 的
**190/190 完整 gate（無略過／失敗、135.96 秒）**均通過；保留原有 native 斷言並新增
精確還原比對。詳見 [2D 驗收修正](../../Engine/TwoD/evidence/Cooked-Services-Linux-2026-10-16/acceptance.md#movement-and-save-acknowledgement-follow-up)。
PR [#618](https://github.com/jimlee1972/Nexora/pull/618) 後續已於兩組 Hosted Workflow 成功後合併。

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
PR [#615](https://github.com/jimlee1972/Nexora/pull/615) 後續已於全部 36 項 Hosted 檢查成功後合併。

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
