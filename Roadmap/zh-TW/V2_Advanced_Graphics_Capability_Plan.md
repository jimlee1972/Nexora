# V2 進階圖形能力計畫

狀態：V2-GFX-1 與有界 V2-GFX-2 實作施工中；五個完整驗收 gate 仍全部待完成。
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

### 已回收 Vulkan Descriptor Pool 儲存空間 — 2026-10-17

✅ 選用配置快取擴充為每份閒置項目最多兩個已 Reset 的原生 Descriptor Pool
（八份 Command 配置、十六個閒置 Descriptor Pool）。存活清單獨占配置，先依序用完暖機 Pool
再擴充；每次 Dispatch 仍配置全新不可變 Set。Command Pool Reset 先於 Descriptor Reset／轉移，
Reset 失敗、額外 Pool 或快取溢出皆銷毀儲存空間。建構失敗與關閉時完整回收所有取得的 Pool。
Query Pool 維持每清單獨立，不新增等待、SDK／模組相依、穩定 C ABI 或非同步 Queue 路徑。

原生 Oracle 新增96份唯一工作跨三個 Pool、放棄／只保留兩份、重新錄製／溢出、
保留已完成清單的獨占權及192份 CPU 比對。既有原生串行／四執行緒及失敗／取消重試覆蓋持續執行。
JSON 回報原生 Command／Descriptor 建立、取得與閒置數，快速檢查預設全新配置及選用暖機重用。
Beads `nexora-45y.2.14`；來源 `feat/v2-vulkan-descriptor-pool-reuse`。
Slang Development configure/build 與 Xvfb／強制原生完整 CTest **237/237 通過、零略過／失敗、
253.15 秒**（Showcase 開啟、Graphical Shell 關閉）。聚焦計時／原生重用／報表 **7/7 通過、1.14 秒**；
Shipping Monolithic／Minimal LTO 通過。Khronos1.3.275 物件／同步驗證 **10/10 通過、16.13 秒**，
確認25次 Layer 插入，Validation Error／VUID／SYNC-HAZARD 為零，格式及 Diff 檢查通過。
擴充 Compute Oracle 建立13／重用196份 Command 配置，建立11／取得368個 Descriptor Pool，
最後閒置 Command／Descriptor 各8份；保留清單、三 Pool 溢出及額外192份 CPU 比對通過。
獨立21x40執行，每模式／政策驗證3,360份輸出且21份原生 GPU 樣本皆可用；量測期間 Descriptor
建立／取得由 **168/0** 變成 **0/168**，暖機四清單工作保留8個閒置 Pool。
未宣稱實體效能或記憶體預算驗收；Hosted 平台／ABI 與原生 Vulkan 閘門仍須通過。
先前 `.2.13` 證據僅涵蓋最初 Command 快取；本項擴充已回收原生儲存空間，不共享快照。
完整非同步／平行／效能驗收與實體記憶體預算仍待完成。

### 計時、量測與有界重用整合 — 2026-10-17

後續改動在 PR628 前置基礎上整合 `.2.11` 原生 Timestamp、`.2.12` 配對錄製量測與
`.2.13` 有界 Vulkan Allocator 重用。新增 Vulkan `--benchmark-reuse` 證據，回報明確快取准入與
各模式量測期間原生 Pool 建立／取得數，使用相同工作比較預設全新配置。
原生報表快速檢查確認暖機後只取得快取、不建立新 Pool；一般模式保持不用快取且建立全新 Pool。
✅ 整合 Slang Development configure/build 與 Xvfb／強制原生完整 CTest
**213/213 通過、零略過／失敗、126.86 秒**（Showcase／Graphical Shell 關閉，Slang／Zig／原生 Vulkan 開啟）。
六項計時／量測／重用聚焦目標 **6/6 通過、1.09 秒**；Shipping configure/build 通過
（Monolithic／Minimal、LTO）。Khronos1.3.275 物件／同步驗證 **10/10 通過、10.32 秒**，
確認25次 Layer 插入，Validation Error／VUID／SYNC-HAZARD 為零。格式及 Diff 檢查通過，未新增外部／模組相依。

獨立21x40預設／重用執行，每模式／政策驗證3,360份輸出、21份 GPU 樣本皆可用。
預設串行／平行 CPU 設定加錄製 median/p95 為 **0.241745/0.318109**、**0.606625/0.666376 ms**；
重用為 **0.241805/0.306012**、**0.637702/0.778514 ms**。各模式量測期間原生 Pool 建立／取得
由 **84/0** 變成 **0/84**，重用結束快取四份。預設 GPU 加總 median/p95 為
**4.773421/5.834963**、**4.682682/5.984547 ms**；重用為
**4.714421/5.534808**、**4.657415/5.955893 ms**。
暖機／最大抽樣 RSS：預設51,847,168/51,912,704 bytes，重用51,830,784/51,892,224 bytes。
以上軟體主機觀察不能證明 CPU 開銷改善、記憶體預算或實體效能。仍須 Hosted Windows／Metal／圖形回歸。
本項為量測與同步重用基礎，五個完整驗收閘門皆維持未完成。

### 選用有界 Vulkan Command Allocator 重用 — 2026-10-17

✅ 預設關閉的明確 Device 選項提供固定八格原生 Command Pool／Primary Buffer 快取。
存活清單即使完成提交仍保有獨占權；只有未提交或個別 Fence 已成功完成的清單可 Reset 後歸還，
已接受但後續失敗的提交不具重用資格。Reset 先清除錄製參照，再銷毀 Descriptor／Query／Framebuffer；
Reset 失敗或快取溢出則銷毀配置。診斷回報建立、快取取得及目前閒置數量，裝置關閉時排空並銷毀。
本項不快取 Query／Descriptor、不新增等待、不變更 SDK／穩定 C ABI，也不提供非同步 Submit。
所有 C++ 模組需重建；其他後端維持停用。原生 Compute／Texture Oracle 啟用計時並新增保留清單、
放棄、容量／溢出及重複輸出／生命週期檢查。Beads `nexora-45y.2.13`；來源
`feat/v2-vulkan-recording-resource-reuse`。
Slang Development configure/build 與 Xvfb／強制原生 CTest **211/211 通過、零略過／失敗、121.09 秒**；
Shipping Monolithic／Minimal LTO 通過。聚焦原生重用 **2/2 通過、2.81 秒**。
Khronos1.3.275 物件／同步驗證 **8/8 通過、3.12 秒**，確認21次 Layer 插入，
Validation Error／VUID／SYNC-HAZARD 為零。Compute 建立13份、重用193次、最後快取8份；
圖形建立4份、重用608次、最後快取4份，包含512次全新 Target／Query 原生渲染／讀回像素比對。
既有320份 Dispatch、48份 Graph、64份共用輸入 CPU 比對及失敗／取消重試仍通過。
Null 請求保持停用。Windows／Metal ABI 回歸與 Hosted Vulkan 驗證仍由 CI 把關。
完整平行、非同步 Descriptor／Allocator 重用及實體效能驗收仍待完成。參見 [RHI 契約](../../Engine/RHI/README.md#optional-bounded-vulkan-command-allocator-reuse)。

### 原生串行／平行錄製配對量測 — 2026-10-17

✅ 受限的 `--benchmark [samples dispatches-per-list]` 模式共用原生 GPUDriven compute oracle，
串行／四工作執行緒使用相同工作、四份清單與依序同步提交。兩組暖機後交替每組的先測模式。
JSON 分別回報 CPU 設定、錄製／排程／等待工作、設定加錄製 median/p95、Submit/Wait median/p95、
可選的已完成 GPU 區間加總、編譯器／裝置 ID 與 Linux RSS 抽樣。每次量測前重設輸出哨兵，
計時結束後以 CPU oracle 檢查每份輸出。缺少的指標保持 null；軟體裝置旗標不能證明實體硬體。
不宣稱加速、實體驗收或目標預算。共用既有原生測試程式與 CI 驗證步驟，不新增相依套件或工作。
Beads `nexora-45y.2.12`；來源 `feat/v2-recording-benchmark`。
整合原生 Timestamp 與 PR628 前置改動後，Slang Development configure/build 與
Xvfb／強制原生完整 CTest **234/234 通過、零略過／失敗、245.30 秒**（GCC14.2、Slang2026.18、
Zig0.14、Mesa lavapipe24.0.5）。聚焦計時／量測 **3/3 通過、4.72 秒**；最後報表驗證重查
**1/1 通過、0.53 秒**。Shipping Monolithic／Minimal LTO 通過。
Khronos1.3.275 物件／同步驗證 **7/7 通過、2.14 秒**，確認19次 Layer 插入，
Validation Error／VUID／SYNC-HAZARD 為零，不新增外部或模組相依。

另行獨立執行 `--benchmark 21 40`，每模式驗證 **3,360 份量測輸出**，另含暖機。
串行／四工作執行緒 CPU 設定加錄製 median/p95 為 **0.271299/0.608567 ms** 與
**0.573014/0.789110 ms**；GPU 區間加總 median/p95 為 **5.910813/9.522575 ms** 與
**6.099738/8.569123 ms**，每模式21份 GPU 樣本皆可用。暖機 RSS51,412,992 bytes，
最大抽樣 RSS51,630,080 bytes。此軟體主機平行錄製較慢；這是工具正確性觀察，並非加速或實體驗收。
報表保留在 Repo 外暫存證據，CI Log 保留受限快速檢查 JSON。Windows／Metal 執行仍須 Hosted 原生驗證。
完整閘門與非同步 allocator／descriptor 重用仍待完成。參見[量測契約](../../Engine/Renderer/README.md#matched-native-command-recording-measurement)。

### 選用的原生 GPU 提交完成計時 — 2026-10-17

✅ RHI 計時須由 `CreateDeviceWithOptions` 明確要求；一般建立維持關閉。建構時的支援／
啟用旗標，以及自有 Source／Completion／選用毫秒觀察值，可區分不可用與零。Vulkan 清單
擁有選用 Timestamp Pair，D3D12 清單擁有 Query／Readback Storage 及 Queue Frequency，
Metal 讀取已完成 Command Buffer 的 GPU 時間。收集只使用既有完成機制，不新增 Query WAIT
或 Callback。預設清單不分配／錄製計時資源，也不發布計時完成串流；取消／未提交工作不發布。

Presentation 已驗證的 Counter Width／Period／CPU 上界換算共用至 RHI Detail Header，
不改變其 Surface 觀察值或 Slot 生命週期。非法／Query／分配結果維持不可用，不以 CPU 時間
替代 GPU 時間。各清單 Query 資源在完成或放棄 Buffer 清理後回收；D3D12 拒絕 Device-removed
Fence Sentinel。未修改 SDK／Shader Profile／Stable C ABI／Module 關係，須重建 C++ Module。

來源分支：`feat/v2-native-submission-timestamps`；Beads `nexora-45y.2.11`。
啟用 Slang 的 Development Configure／Build 與 Xvfb／Native-required 完整 CTest
**232/232 通過，零略過／失敗，263.88 秒**（Slang 2026.18、Mesa lavapipe 24.0.5、Zig 0.14.0）。
`ctest --preset linux-development -R '^(window_presentation\.gpu_timing|renderer\.(submission_gpu_timing|vulkan_recording_gpu_timing|vulkan_texture_recording|vulkan_descriptor_isolation|parallel_graph_recording|dxil_artifact_readiness))$' -V`：
**7/7 通過，零略過，0.40 秒**。Shipping Configure／Build 通過（Monolithic／Minimal、LTO）。
原生 Object／Synchronization Validation 使用 Khronos Layer 1.3.275 並移除繼承 Shader 路徑：
`ctest --preset linux-development -R '^renderer\.(submission_gpu_timing|vulkan_recording_gpu_timing|vulkan_texture_recording|contracts|v2_gpu_driven|vulkan_descriptor_isolation)$' -V`
**6/6 通過，零略過，7.46 秒**，Loader 證明載入，沒有 Validation Error／VUID／SYNC-HAZARD。
圖形取得 **4/4 可用且為正的 Vulkan 觀察值**，Compute 計時亦可用且既有 CPU 比對均通過。
預設關閉、Validation 不可用、觀察值 Copy 生命週期、反向提交、取消 Query 與精確像素通過；
既有 Presentation Wrap／非法 Counter 測試驗證共用換算。現有 CI Validation Step 加入新
計時 Target，沒有新增相依或 Job。

Windows／Metal 執行須通過 Hosted 原生 Gate；Software Vulkan 僅證明 API／生命週期正確。
同條件 CPU 錄製／提交 Median／P95、GPU／Memory 預算及實體 GPU 校正 Benchmark 仍待完成，
非同步重用與完整平行 Feature 亦維持待完成。詳見
[RHI Contract](../../Engine/RHI/README.md#opt-in-completed-submission-gpu-timing)。

### 共用輸入與 DXIL 檢查整合 — 2026-10-17

✅ 後續整合不變共用 Buffer 讀取與所選 DXIL Stage／Profile 檢查，包含指定錯誤 Stage 的
建構拒絕與 Fence Event 清理。整合後啟用 Slang 的 Development Configure／Build 與
Xvfb／Native-required CTest **206/206 通過，零略過／失敗，119.65 秒**。包含
`renderer.dxil_artifact_readiness` 的六項聚焦 Renderer 指令 **6/6 通過，零略過，0.29 秒**。
Shipping Configure／Build 通過（Monolithic／Minimal、LTO）。既有四項原生 Vulkan
Object／Synchronization Validation **4/4 通過，零略過，1.18 秒**，Loader 證明 Layer
載入，沒有 Validation Error／VUID／SYNC-HAZARD 訊息。Windows 真正 DXIL／PSO／
純讀者提交，以及 Metal 共用讀取仍須 Hosted Gate 證明。Beads `nexora-45y.2.9` 與 `.2.10`
共用後續整合；完整 Toolchain、平行／效能與 Preview Gate 維持待完成。

PR626 合併後已 Rebase 至 main `bb39a646`，保留上游 Windows VM 工具／證據更新。
完整 Slang Development Configure／Build 與 Xvfb／Native-required CTest 再次通過：
**207/207，零略過／失敗，120.22 秒**。六項聚焦 Renderer 指令 **6/6 通過，零略過，0.26 秒**；
Shipping Monolithic／Minimal LTO Configure／Build 通過。

### DXIL Entry Stage 與 Artifact Profile 檢查 — 2026-10-17

✅ D3D12 指定 Entry Artifact 現會接受 Portable、有界的 DXBC／DXIL Program Metadata
檢查。Reader 驗證 Container／版本／大小、Part 與 Bitcode 範圍、無重疊、單一執行 Part
及 LLVM Magic；解碼已實作的 Vertex／Fragment／Compute Stage 與數值 Shader Model。
階段不符、畸形及過舊 Profile 會在原生 PSO 分配前遭拒。Graphics 會檢查兩個所選圖形階段，
與 Compute 分開。裝置能力仍為獨立檢查；硬體支援 SM6.9／6.10 不能讓既有 SM6.6 圖形或
SM6.0 Compute Bytecode 滿足較新最低要求。沒有 DXIL 最低要求時保留選用 FXC 基線。
未啟用任何原生進階功能，也未提高 Profile 基線。

Portable 測試涵蓋 Stage／Profile 解碼、6.9／6.10 數值順序、每個截斷 Prefix、未排序
Offset、畸形／溢位／重疊 Part 與重複 Program。Windows 啟用 Slang 的原生 CI 另讀取
真正產生的 Artifact，檢查 SM6.6 圖形／SM6.0 Compute，建立基線 PSO，並在裝置支援時
拒絕較新的 Artifact 要求。指定錯誤 Stage 的 Compute Artifact 也會在裝置建構時遭拒；Raw Fence Event 改於最後
建立，避免建構驗證失敗時洩漏 Handle。
Metadata Reader 不驗證 LLVM、Checksum／簽章、Compiler
來源或 Preview Opt-in，不能證明 SM6.9／6.10 執行，也未完成完整 Toolchain 矩陣 Gate。

來源分支：`feat/v2-dxil-artifact-admission`；Beads `nexora-45y.2.9`。
啟用 Slang 的 `cmake --preset linux-development` 與
`cmake --build --preset linux-development` 通過。Xvfb／Native-required 的
`ctest --preset linux-development` **205/205 通過，零略過／失敗，122.74 秒**
（Slang 2026.18、Mesa lavapipe 24.0.5、Zig 0.14.0）。聚焦執行
`ctest --preset linux-development -R '^renderer\.(dxil_artifact_readiness|graphics_capabilities|contracts)$' --output-on-failure`：
**3/3 通過，零略過，0.07 秒**。Shipping Configure／Build 通過（Monolithic／Minimal、LTO）。
Linux Metadata-only Probe 另成功解析真正 vkd3d-proton DXIL Fixture：Compute SM6.6
（2660 Bytes）、Fragment SM6.1（2472 Bytes）、Vertex SM6.0（1988 Bytes）。Fixture／
Probe 留在 Repo 外；這是解析證據，不是 GPU 執行。Windows 執行／PSO 驗收須由 Hosted CI
證明。新增匯出 C++ Helper 後須重建 Module；Stable C ABI 及 SDK／Compiler 基線不變。
完整 V2 圖形、Compiler／Artifact 就緒矩陣與實體 GPU 效能 Gate 仍待完成。詳見
[RHI Contract](../../Engine/RHI/README.md#dxil-artifact-stage-and-profile-admission)。

### 不變的共用 Buffer 輸入 — 2026-10-17

✅ 獨立且預設關閉的 `shared_buffer_read_recording` 能力，允許有界 Compute 與 Color-target
Cohort 共用同 Queue／Access、相容的 Read/Read Buffer。可寫 Buffer 仍須分開；共用 Texture、
全部寫入衝突與 Access／Queue 相依仍排除。明確選用 Callback、Caller 擁有且不變的資源、
全部結束後提交、有序同步完成、取消與預設序列執行維持原契約。

D3D12 快照現區分可能的 UAV 寫入與不變的 SRV／Indirect 讀取。已接受的可能寫入、CPU 上傳
及非空讀回會推進資料 Revision；相容讀取保留 Revision。初始平行讀者可複製相同、持續保留
的 Staging Revision，於每次獨立 Execute 邊界使用 COMMON Promotion／Decay。變更仍使舊
讀者失效；未提交的上傳複製維持私有。已驗證的空讀取不使快照過期。

共用原生 Oracle 新增 64 組 Compute CPU 比對（先平行且輸入仍待上傳、序列與失敗／取消
復原）、強制四 Worker 重疊、分開的輸出 Sentinel 與成功重用。兩個圖形 Worker 共用新建
Indirect 參數、寫入不同 Color Target，兩張圖均精確符合序列直接繪圖。D3D12 另驗證純讀者
反向提交、上傳／讀回／GPU Writer 使快照失效、空讀取相容性及復原。Portable 測試保留自訂
後端 Fallback 與 Write/Read、Read/Write、Write/Write、Access Hazard 順序。完整平行
Feature 仍維持未啟用／未實作。

來源分支：`feat/v2-shared-buffer-recording`；Beads `nexora-45y.2.10`。
啟用 Slang 的 `cmake --preset linux-development` 與
`cmake --build --preset linux-development` 通過。Xvfb／Native-required 的
`ctest --preset linux-development` **205/205 通過，零略過／失敗，137.44 秒**
（Slang 2026.18、Mesa lavapipe 24.0.5、Zig 0.14.0）。聚焦執行
`ctest --preset linux-development -R '^renderer\.(parallel_graph_recording|contracts|graphics_capabilities|vulkan_texture_recording|vulkan_descriptor_isolation)$' -V`：
**5/5 通過，零略過，0.28 秒**。Shipping Configure／Build 通過（Monolithic／Minimal、LTO）。
四項原生 Object／Synchronization Validation **4/4 通過，零略過，2.53 秒**，使用 Khronos
Layer 1.3.275，Loader 證明載入；移除繼承 Shader 路徑後沒有 Validation Error／VUID／
SYNC-HAZARD 訊息。Windows／Metal 原生執行須由 Hosted CI 證明。新增能力欄位後須重建
全部 C++ Module；Stable C ABI 與 SDK／Shader 基線不變。共用 Texture 讀取、非同步重用、
完整 Compiler／Artifact 就緒及同條件實體 GPU 效能仍待完成。詳見
[RHI Contract](../../Engine/RHI/README.md#immutable-shared-buffer-reads)。

### 原生圖形／取消整合 — 2026-10-17

✅ PR626 整合取消／同池 Coordinator、Vulkan Framebuffer／Texture 隔離，以及 Vulkan、
D3D12、Metal 的有界 Color-target 錄製。共用圖形 Oracle 現另取消僅清除畫面的平行 Cohort，
要求零提交及像素 Sentinel 不變，再以新的取消狀態重播。資源快照與全部取消檢查／Worker
身分 Guard 共存；預設仍為序列，完整平行 Feature 保持關閉。MSVC 匯出例外的 Virtual
診斷 Member，其他平台保留可見 Type RTTI，維持跨 Module 的型別化 Catch，未壓制 Compiler
警告。

整合後啟用 Slang 的 `cmake --preset linux-development` 與
`cmake --build --preset linux-development` 通過。Xvfb／Native-required 完整
`ctest --preset linux-development` **229/229 通過，零略過／失敗，259.33 秒**。
`ctest --preset linux-development -R '^(core\.runtime|renderer\.(parallel_graph_recording|vulkan_texture_recording|vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven))$' --output-on-failure`：
**7/7 通過，零略過，0.36 秒**。Shipping Configure／Build 通過（Monolithic／Minimal、LTO）。
下方四項 Vulkan Object／Synchronization Validation 指令 **4/4 通過，零略過，2.15 秒**；
移除繼承 Shader 路徑後，Loader 確認 12 次 Layer 載入，完整 Log 沒有 Validation Error／
VUID／SYNC-HAZARD 訊息。Windows／Metal 執行須通過新 Head 的 Hosted Gate。Beads
`nexora-45y.2.6`、`.2.7`、`.2.8` 共用 PR626；完整 V2-GFX 及實體效能 Gate 仍待完成。

### 有界原生 Color-target Graph 錄製 — 2026-10-17

✅ 有限的 `independent_graphics_recording` Contract 將獨立 Color-target Graphics Cohort
加入選用的 Graph 排程器，與獨立檢查的 Buffer-only Compute 共存。Graphics Pass 須完整
宣告資源，至少一項 RenderTarget 寫入，不得讀取 Texture 或寫入 Buffer。共用資源（含
Read/Read）、相依、未支援／未標記 Pass 與單一 Cohort 會序列 Fallback。預設 Execute
仍為序列。Worker 只錄製；Coordinator 等全部結束，再依 Graph 順序提交並等待完成才回收。

D3D12／Metal 現由各清單快照 Texture Generation／Revision／原生 Metadata 及邏輯狀態，
執行前驗證，Queue 接受後才提交。讀回使舊快照過期；失敗／放棄錄製保留已提交狀態。
Validation 模擬相同政策，包含上傳使快照失效。D3D12 RTV 分配會檢查 256 項 Heap 容量並
回收已退役 Slot。原生 Color 測試讀回會去除 Staging Row Padding；正式執行不讀回。
Metal Graphics 入口使用 Autorelease Pool，清單以 ARC 保留 Texture／Encoder。

Vulkan／D3D12／Metal 共用 Oracle 涵蓋 24 組混合 Format／Extent 的序列／平行逐像素比對、
每清單六個 Scope、四 Worker 重疊及反向提交；過期快照與放棄復原；512 次目標分配／回收；
以及八組 Graph 輸出比對、放棄批次 Sentinel 與重播。Portable 測試另涵蓋能力獨立檢查、
Compute／Graphics 混合 Cohort、共用讀取、Texture Hazard／相依、未選用、Sampling／Copy
Fallback、提交順序與完成後回收。

來源分支：`feat/v2-native-graphics-recording`；Beads `nexora-45y.2.8`。
啟用 Slang 的 `cmake --preset linux-development` 與
`cmake --build --preset linux-development` 通過。Xvfb／Native-required 的
`ctest --preset linux-development` **205/205 通過，零略過／失敗，134.48 秒**
（Slang 2026.18、Mesa lavapipe 24.0.5、Zig 0.14.0）。Shipping Configure／Build 通過
（Monolithic／Minimal、LTO）。原生 Vulkan Object／Synchronization Validation 使用
**1.3.275** Layer，**4/4 通過，零略過，8.24 秒**；Loader 證明已載入，沒有 Validation
Error、VUID 或 SYNC-HAZARD 訊息。移除繼承 Shader 路徑後由 CTest 提供原生 Artifact。
最終混合 Cohort Portable 測試亦通過。Windows／Metal 執行驗收須由 Hosted CI 證明，
Linux 不認證這兩個後端。新增能力欄位後須重建全部 C++ Module；Stable C ABI 與
Shader／SDK 基線不變。完整平行 Feature、共用讀取、非同步 Queue／重用、Compiler／
Artifact 就緒矩陣及同條件實體 GPU 效能仍待完成。詳見
[Graph Contract](../../Engine/Renderer/README.md#native-color-target-graph-acceptance) 與
[RHI Contract](../../Engine/RHI/README.md#independent-color-target-recording-and-transactional-textures)。

### Vulkan Texture 錄製與完整 Framebuffer 回收 — 2026-10-17

✅ Vulkan Texture Transition 現使用各清單擁有的 Generation／Revision／State 快照；Rendering
檢查私有狀態，提交在關閉／送入 Queue 前拒絕過期快照，只有已接受工作才提交裝置最終狀態。
讀回恢復 Layout，但會使舊錄製失效。每個 Render Scope 的 Framebuffer 都在完成或放棄清單
清理後回收，不再由單一欄位遺失前面 Scope。Undefined 目的狀態、非法 Queue、超出 Target
範圍與清單關閉後的錄製，都在原生命令前拒絕。

原生測試使用 24 個獨立、不同格式／大小的 Target，每份清單六個 Render Scope、四個
Worker，要求有時限的重疊錄製，之後逆序提交，對非空序列參考進行 24 次逐像素精確比對。
另驗證放棄狀態／輸出及重試、提交／讀回／失效 Handle 的過期拒絕、Callback 失敗復原與
序列 Graph 重播。既有 CI 同步／物件 Validation Step 新增 Graphics Texture 錄製及
Compute Descriptor 隔離，沒有新增 CI 相依或 Job。未新增 Texture Upload API；資源
變更／提交仍須由 Caller 相對錄製序列化。

來源分支：`feat/v2-vulkan-texture-recording`；Beads `nexora-45y.2.7`。
本機啟用 Slang 的 `cmake --preset linux-development`、
`cmake --build --preset linux-development` 與 Xvfb／Native-required 的
`ctest --preset linux-development` 通過：**205/205，零略過／失敗，118.15 秒**
（Slang 2026.18、Mesa lavapipe 24.0.5、Zig 0.14.0）。Shipping Configure／Build
通過（Monolithic／Minimal、LTO）。原生 Validation 指令
`ctest --preset linux-development -R '^renderer\.(vulkan_texture_recording|contracts|v2_gpu_driven|vulkan_descriptor_isolation)$' -V`
**4/4 通過，零略過，0.87 秒**；移除繼承 Shader 路徑，由 CTest 提供兩份 Artifact。
Ubuntu Validation Layer **1.3.275** 解壓於專案外；Loader 輸出證實載入，啟用同步
Validation 後完整 Log 的 **Validation Error／VUID／SYNC Marker 均為零**。
本次 C++ Format 與 `git diff --check` 通過。較早 Slang-off／手動 Artifact 的完整 Gate
也曾 195/195 通過；以上啟用 Slang 的結果為最終 Gate。

完整 Graph Graphics 檢查、DX12／Metal Texture 隔離、共用讀取、非同步 Queue／重用、
Compiler／Artifact 就緒與同條件實體 GPU 效能仍待完成。完整平行 Feature 維持未啟用／
未實作，Shader／SDK 基線與 Stable C ABI 不變。詳見
[RHI 所有權 Contract](../../Engine/RHI/README.md#vulkan-texture-snapshots-and-framebuffer-retirement-v2-gfx-1-slice)。

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

### 取消與同池 Coordinator — 2026-10-17

✅ Pool-local Worker 身分可讓位於自身錄製 Pool 的 Graph Coordinator 選擇序列 Fallback，
即使全部 Worker 同時協調 Graph，也不會因耗盡執行緒而互等。其他 Pool 的 Coordinator
仍可使用有界平行錄製。Start／Stop 與物件生命週期仍須外部序列化；一般 Wait 並未新增
Work Stealing 或循環等待復原。

選用的 Cancellation Token 可在預先取消時，於 RHI 分配／Callback 前停止，或在錄製後、
未提交 Cohort 送入 GPU 前停止。每個已啟動 Job 都須結束；Callback／排程錯誤優先於觀察到
的取消。Cohort 一旦開始提交便會完成並回收，下一批停止；最後一批接受後才取消仍回傳
正常完成。型別 `RenderGraphCancelled` 區分取消。既有預設與 Stable C ABI 不變；新增
Query／Exception 及 Token 欄位後須重建所有 C++ Module。

Portable 測試涵蓋耗盡 Worker 的巢狀 Coordinator、其他 Pool 重疊、停止／重啟身分、預先／
序列／平行取消、Transient 清理、錯誤優先序、已接受批次完成與最後取消。原生 Vulkan
另驗證取消後 Sentinel 保留、零提交／讀回與 Buffer 重用，保留既有 320 組 Dispatch 及
48 組 Graph CPU 比對。來源分支：`feat/v2-recording-cancellation-and-nesting`；
Beads `nexora-45y.2.6`。

本機 `cmake --preset linux-development` 與 `cmake --build --preset linux-development`
通過；Xvfb／Native-required 的 `ctest --preset linux-development` **194/194 通過，
零略過／失敗，107.80 秒**（Slang 2026.18 SPIR-V、Mesa lavapipe 24.0.5、Zig 0.14.0）。
聚焦執行
`ctest --preset linux-development -R '^(core\.runtime|renderer\.(parallel_graph_recording|vulkan_descriptor_isolation|contracts|graphics_capabilities|v2_gpu_driven))$' --output-on-failure`：
**6/6 通過，零略過，0.27 秒**。`cmake --preset linux-shipping` 與
`cmake --build --preset linux-shipping` 通過（Monolithic／Minimal、LTO）。本次 C++ 的
`clang-format --dry-run --Werror` 與 `git diff --check` 通過。

PR625 後整合：已 Rebase 至 main `c5c5b897`；啟用 Slang 的完整 Development
Configure／Build 與 Xvfb／Native-required CTest **228/228 通過，零略過／失敗，267.94 秒**。
最終上述聚焦指令 **6/6 通過，零略過，0.27 秒**。Shipping Monolithic／Minimal LTO
Configure／Build 通過。自 Dependency Head Rebase 後驗證過的 Source Tree 完全相同；
D3D12 與 Metal 的 Hosted 原生 Gate 現亦編譯共用的取消 Oracle。

完整 Gate、Graphics／Texture 並行、共用讀取、非同步 Queue／
重用、Compiler／Artifact 就緒及同條件效能驗收仍待完成。詳見
[取消 Contract](../../Engine/Renderer/README.md#recording-cancellation-and-nested-coordinators)。

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
均不變。詳見 [執行 Contract](../../Engine/Renderer/README.md#bounded-opt-in-parallel-compute-and-color-target-recording)。

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
初始基礎切片的保留輸出：Compute／Indirect 在 1.0 支援、已啟用且已實作；當時平行錄製與 Tessellation
回報支援 1.0，但未實作／啟用。後續有界 Tessellation 切片另於下節驗收；其他進階能力仍未完成。Pipeline 檢查會在配置 native PSO 前
拒絕所有尚未實作的進階路徑；既有完整 GPU-driven 參考比對仍通過。

Portable 測試另覆蓋獨立 tier 編碼（含 feedback 90／100 與 DXR 1.2）、未知／不支援／
停用／未實作的拒絕結果、明確 fallback 耗盡、SM 6.10 比較，以及已快取的基線 PSO
不能放行進階要求。本 Linux 環境未產生 Windows／Apple native 執行結果或實體 GPU 效能
證據，既有平台 CI／target-host gate 仍須完成。環境原有 SwiftShader 因缺少既有 Vulkan
基線能力而被拒絕，未為取得通過結果而放寬要求。

## V2-GFX-2 有界 Tessellation 施工狀態

✅ 獨立的 Portable／Linux 切片加入三控制點 PatchList、Hull／Domain 階段位元、隱含裝置准入、選用階段產物檢查
及幾何路徑獨立的 Pipeline／Cache 識別。Vulkan 啟用已查詢支援的 Tessellation 並建立真實四階段 PSO；
D3D12 建立 VS／HS／DS／PS 及 PATCH／三點 IA 拓撲。Metal 拒絕原生 Hull／Domain，使用明確的
預先細分16三角形／48頂點後備。預設普通三角形不變；無效組合及有界 Draw／Indirect 誤用在原生錄製前拒絕。
固定 Factor4 與曲面輸出屬 Bootstrap 範圍，不代表通用 Patch／Material 組裝已完成。

真實 Slang Reflection 驗證 Hull／Domain 名稱及64-byte 後備 Frame Layout；產生的來源／Include／
產物／Profile／Topology 識別區分不同程式。後備 SPIR-V 不含 Tessellation Capability。
控制／評估階段位元8/16 經 Binding 驗證、Layout Hash 與 Cooked Format1 往返保留；選用 Entry
識別不同，未知位元拒絕。有界唯讀 SPIR-V1.0–1.3 Entry Metadata 不宣稱語意／來源驗證。
既有圖形 SM6.6／Compute SM6.0 基線不變，新增 DXIL 階段使用 SM6.0。

來源 `feat/v2-bounded-native-tessellation`；Beads `nexora-45y.3.1`。原生 Vulkan 獨立探測在64x64
與後備完全相符（**466覆蓋像素、最大 Channel 差0**）。永久 Oracle 檢查 RGBA／BGRA 差異至多一階、
非空且不同於普通三角形的幾何、強制雙 Worker、失敗／取消哨兵及成功重試。
聚焦准入／DXIL／原生／Cooked 測試 **5/5 通過、0.14秒**；Khronos1.3.275 物件／同步驗證
**5/5 通過、3.19秒**，確認14次 Layer 插入，Validation Error／VUID／SYNC-HAZARD 為零。
完整 Slang／Zig／原生 Development configure/build 與 Xvfb 強制原生 CTest **208/208 通過、
零略過／失敗、129.49秒**（Showcase／Graphical Shell 關閉）；Shipping Monolithic／Minimal LTO
configure/build 通過，格式及 Diff 檢查通過。不關閉任何完整 Gate。

整合已合併 PR630／main `1d48e918` 後，完整 Development configure/build 與 Xvfb CTest
再次通過：**214/214、零略過／失敗、188.04秒**；聚焦原生／准入／Cooked／計時
**6/6、0.31秒**，Shipping 通過。合併原生 Renderer 物件／同步驗證 **11/11、13.94秒**，
確認27次 Layer 插入，Validation／VUID／Sync 錯誤為零。

另行暫存的官方 Linux DXC **v1.9.2609／1.9.2609.5** 可供 Slang2026.18 真實 DXIL 編譯／Reflection／
Reader 探測，不變更專案相依：Control4008 bytes、Evaluation3960 bytes、後備 Vertex5488 bytes，
解碼階段8/16/1，皆為 SM6.0；這不是 D3D12 執行。Windows HS／DS 與 Metal 後備 Hosted 驗收
另見下方整合章節；更廣 Patch Authoring、Runtime Program 組裝／Cook 執行、Enhanced Barriers、完整目標主機／
效能驗收仍待完成，五個完整 Gate 全部維持開放。

## V2-GFX-4 有界每次繪製 VRS 施工狀態

D3D12 Tier1 有界路徑施工中：Pipeline 明確選擇1x1／2x2著色率，自動加入能力准入與完整 Cache
識別，List 獨占 list5 錄製並恢復 Full Rate，僅計算已接受 List 的粗著色 Draw，另設後端實作 Tier
上限。硬體 Tier2 不會自動放行未實作的 Rate Image／Combiner；其他後端明確選擇 Full Rate 後備。
原生 RGBA／BGRA 漸層覆蓋／誤差、雙 Worker 取消／重試與狀態恢復 Oracle 驗證中，尚不宣稱原生
或效能驗收。來源 `feat/v2-d3d12-draw-shading-rate`，Beads `nexora-45y.5.1`。
完整 VRS／Ultimate、Meshlet、Feedback、實機預算及所有完整 Gate 維持開放。

✅ 本地 Portable 准入／Cache／實作上限及真實 Vulkan Full Rate 後備通過。
完整原生／Slang／Zig Development configure/build 與 Xvfb CTest **215/215、零失敗／略過、
177.24秒**（Showcase／Graphical Shell 關）；聚焦 **4/4、0.21秒**，Shipping Monolithic／Minimal
LTO 通過。合併原生物件／同步回歸 **12/12、18.32秒**，確認29次 Layer 插入，Validation／VUID／Sync
錯誤為零。Vulkan 後備覆蓋512像素、Channel 差為零；enabled=0／coarse_draws=0 明確不算原生 VRS。
Windows2x2 與 Hosted 平台 Gate 仍待驗證，不關閉任何完整 Gate。

## V2-GFX-4 有界 Mesh Shader 施工狀態

D3D12 有界原生 MS／PS 路徑施工中，包含明確階段／Profile／Variant 准入、List 獨占唯讀 GPU 統計
輸入與 DispatchMesh(1,1,1)。真實 Slang2026.18／DXC1.9.2609.5 探測產出4876-byte、SM6.5 的
Mesh DXIL，解碼階段32；實際 Frame／SRV／Entry Reflection 與原生 SDK Pipeline Stream 語法
探測通過。這些 Linux 探測不代表 D3D12 執行。Oracle 比對 GPU 視錐剔除0/1與 CPU、Mesh 與
GPU 產生 Indirect 的 RGBA／BGRA 像素、過期／放棄輸入、共享 Worker 及取消／重試。
來源 `feat/v2-bounded-d3d12-mesh-shader`，Beads `nexora-45y.5.2`；原生執行／Hosted Gate 待驗證。
通用 Meshlet／Amplification／Indirect Mesh Dispatch 與所有完整 Gate 維持開放。

✅ 本地 Mesh 准入／Cache／DXIL／Profile／Cooked 階段及真實 GPU Compute／Indirect 後備通過。
完整 Slang／原生／Zig Development configure/build 與 Xvfb CTest **216/216、零失敗／略過、
136.60秒**（Showcase／Graphical Shell 關）；聚焦 **7/7、0.46秒**，Shipping Monolithic／Minimal
LTO 通過。合併原生物件／同步回歸 **13/13、15.24秒**，確認31次 Layer 插入，Validation／VUID／Sync
錯誤為零。暫存的真實4876-byte Mesh DXIL 通過引擎 Reader（階段32／SM6.5）；Private 原生
Pipeline Adapter 通過官方 SDK 語法檢查。Linux 結果不代表 D3D12 Mesh Shader 執行，Hosted
原生 MS／PS 仍待驗證，不關閉任何完整 Gate。

## Enhanced Barriers、Mesh 與 VRS 整合本地驗收

自己的 PR632 已在合併 Tessellation／main `77090f7de8340c606c4e770218600c6d41e0fa58` 上整合三條
有界 D3D12 路徑。Windows Mesh／VRS Oracle 分別執行 Legacy 與要求的 Enhanced Barriers，回報實際
啟用及已接受 Texture／Buffer 計數。原生 Adapter 接受 Pixel、Non-Pixel 與合併 ShaderRead，仍拒絕
混合 Read／UAV。真實 SDK Enum 回歸涵蓋 Graph 的合併192遮罩；純映射檢查不代表 D3D12 執行。
Aliasing、多 Queue、Tier2 Rate Image、通用 Meshlet 與完整 Gate 維持開放，整合版本 Hosted 原生
驗收仍待完成。

✅ 整合原生／Slang／Zig Development configure/build 與 Xvfb CTest **241/241、零失敗／略過、
252.40秒**（Showcase 開、Graphical Editor 關）；聚焦 **8/8、0.31秒**；Shipping Monolithic／Minimal
LTO 通過。合併原生 Vulkan 物件／同步驗證 **14/14、17.20秒**，確認31次 Layer 插入，Validation／
VUID／Sync 錯誤為零。Root README 保持讀者導向；全部 C++ 模組重建，穩定 C ABI 與 Shader／SDK
基線不變。

✅ 自己的 Tessellation PR631 在6b05393b的36/36檢查及兩個首輪工作流程39019440906／39019475539
通過後，以77090f7d Squash Merge。兩輪 Linux360/360、Windows303/303、macOS299/299 全數通過。
Windows 真實 HS／DS、Vulkan Control／Evaluation 皆通過 RGBA／BGRA 曲面後備比對、產物負例與
重疊 Worker 取消／重試；Metal 真實執行後備並明確回報原生 Tessellation 不可用。
更廣 Authoring／Cook 與實機驗收仍待完成。

## V2-GFX-5 有界 Work Graph 施工狀態

預設關閉的 Windows 原生選項另尋 SDK DXC 編譯 SM6.8 Node Library，保留 Slang Compute／Indirect
後備與既有 Profile。真實 Linux DXC1.9.2609.5 可編譯雙節點 Library；引擎 Library Reader 解碼
SM6.8，而單一 Entry Inspector 仍拒絕 Library。Slang2026.18 尚未實作所需 Node 型別，因此不使用
一般 Slang 路徑。原生 SDK Helper 語法通過，尚不宣稱 D3D12 執行。

有界 Recipe 將八-byte CPU 輸入展開為至多八筆標準 Visible Record；原生建立驗證 Program／Node／
Entry ABI 與專用僅輸出 Root。每份 Live List 獨占初始化／Backing Context、不可變 Program Snapshot，
並使用既有輸出 Revision 檢查。Oracle 比對 CPU／真實 GPU Compute、實際編譯錯誤 ABI、過期／放棄
資源、輸入複製、Program／Root 切換及取消／重試。來源 `feat/v2-native-work-graph-fanout`，
Beads `nexora-45y.6.1`。

✅ 本地 Library／Admission 與真實 Compute 後備驗收：Linux Development Configure／Build 及完整
CTest 217/217 通過，無失敗或略過（133.53s）；Shipping Minimal／Monolithic LTO 建置通過。
最後輸入邊界修改後，Library／Mesh／VRS／Work Graph／Runtime 專項 7/7 通過（0.26s）。
Khronos Object／Synchronization Validation 14/14 通過（15.65s），確認 33 次 Layer 插入，無
Validation Error／VUID／同步危害。Windows 原生 Work Graph 執行仍待完成；Linux 後備回報
enabled=0、dispatches=0。通用 Graph／Graphics Node、新 Profile、實機預算及所有完整 Gate 維持開放。


Work Graph 候選版本已接上 PR632 最新 Barrier／Mesh／VRS 整合來源。重新執行 Development
Configure／Build 與完整 CTest **218/218** 通過，零失敗／略過（133.84s），Shipping
Minimal／Monolithic LTO 與專項契約／Profile **8/8** 通過。Khronos Object／Sync 原生選項
**13/13** 通過（4.06s），確認24次 Layer 插入，無 Validation／VUID／同步錯誤。Windows
Legacy 與 Enhanced Work Graph 變體仍待驗收；Linux Compute 後備不代表原生節點證據。

## V2-GFX-3 有界三角形 Ray 施工狀態

`feat/v2-bounded-native-rays`、Beads `nexora-45y.4.2` 實作不可變1–8三角形 BLAS／TLAS Recipe、
專用 Root／State／Shader Table、原生 `DispatchRays` 及 Tier1.1 Inline Query。保留呼叫端序列化
建立／提交、提交前 Handle／Revision 驗證、List 私有快照及 Compute 狀態還原。沿用 Slang Opt-in
另產生 Library6.3／Query6.5 與較低 SM 的 Compute Traversal，不提高既有 Profile。實際 Reflection
驗證 Root／32-byte 輸入／16-byte 命中 ABI，私有完整位元組比對在原生建立前拒絕相同 Profile 的
改寫 Recipe；完整 Gate 尚未關閉。

✅ 真實 Slang2026.18＋官方 DXC1.9.2609.5 編譯探測、引擎 Library／Entry Reader，以及 Linux 原生
Vulkan Compute 專項通過。精確浮點模式下，Slang Library6972bytes／SM6.3、Query5236bytes／SM6.5
皆為真實產物，尚非原生執行證據。雙精度 CPU Oracle 與真實 Compute 比對1／2／8三角形及
1／64 Ray 的 Hit／Miss／最近交點／重心座標、無效 Ray、Canary 與強制雙 Worker 失敗／取消／重試。
原生 SDK Helper 在警告視為錯誤下通過語法檢查。Windows AS Build、Ray／Query Dispatch、過期／
放棄生命週期及 Enhanced／Legacy 比對仍待完成；通用 AS Update／Compaction、Shadow／Reflection／
AO、DXR1.2、SER、實機預算及完整驗收維持開放。

下一個自己的 PR 整合 Work Graph 與 Ray Recipe，兩者共用 Library Admission 與原生 Program
Ownership。整合來源 Development Configure／Build 與完整 CTest **219/219** 通過，零失敗／略過
（134.55s），契約／Profile 專項 **9/9**（0.31s）與 Shipping Minimal／Monolithic LTO 通過。
Xvfb 下 Khronos Object／Synchronization Validation **17/17** 通過（9.41s），確認37次 Layer 插入，
無 Validation／VUID／同步錯誤。另有精確浮點 Ray 候選218/218（123.92s）；Vulkan 後備明確回報
Ray／Node enabled=0。實際 Production CMake 產物流程在暫存 Linux DXC 下執行成功，包括 Windows
SDK 預檢、正常／錯誤 Entry Node Library，以及 Reflection 驗證後的完整 Ray 位元組識別。改寫 Ray
Library 仍為合法 SM6.3，但與固定 Recipe 不同，驗證預期負例邊界。整合 SDK Helper 與 Windows
條件測試分支在警告視為錯誤下通過語法檢查。Windows 額外測試兩能力皆可用時的 Node／Ray／Node
Program／Root 切換；原生互通及兩種進階 Program 的執行仍待 Hosted 證據，完整 Gate 維持開放。

## Enhanced Barriers／Mesh／VRS Hosted 驗收（PR632）

✅ 自己的 PR632 在7f35467e的36項檢查及兩個首輪工作流程39026010174／39026013053 全數成功後，
以 `bb61b59d3d9d1591932a866dcf9bf17c49ab0a64` Squash Merge。Push／PR 分別為 Linux363/363
（1342.22／1349.13s）、Windows310/310（1278.89／1322.51s）、macOS302/302（640.91／563.69s）。
兩輪 Windows 皆在 Legacy／Enhanced 模式真實執行 Mesh enabled1／16次 Dispatch、VRS enabled1／
8次 Coarse 呼叫；Enhanced Barriers 回報 Graphics Texture182／Buffer2、Compute Buffer2629、
VRS Texture44、Mesh Texture64／Buffer40。原生 CPU／像素比對、產物負例、Full-rate 還原、共享
輸入 Worker、過期／放棄／取消後重試皆通過。Linux／Metal 真實執行 Compute／Indirect／Full-rate
後備，明確回報 Mesh／VRS enabled0。Package、Vulkan Validation、Editor Display、Build-contract
及 Sanitizer 皆通過。關閉子任務 nexora-45y.3.2／.5.1／.5.2；完整 V2-GFX-2／4 及 Ultimate 仍
待完成。更廣 Topology／Cook、Aliasing、多 Queue、通用 Meshlet／Amplification／Indirect Mesh、
VRS2 Image 與實機預算維持開放。

目前 Hosted D3D12 獨立查詢為 RT1.1、Mesh1.0、VRS1.0、Feedback1.0、Work Graph1.0、SM6.8，
可繼續實作相容路徑，尚不代表其執行驗收或 RT1.2／SM6.9／6.10／SER。Node／Ray 來源由632head
改接 Squash 後，受測樹完全相同（`99bf21beb28b15a99d9010a9e0d8c4f6f3c27577`）；下一個 PR 不重複
既有 Tessellation／Mesh／VRS／Barrier 提交。

## V2-GFX-4 有界 Min-mip Feedback 施工狀態

`feat/v2-native-minmip-feedback`、Beads `nexora-45y.5.4` 新增固定64x64／七層 Mip、16x16 Region
的僅 Metadata Footprint。原生 Device8 建立 Paired／Opaque View，透過獨立 SM6.5 Write、Clear／
UAV 相依、List1 Decode 成4x4 R8_UINT，再由 SM6.0 GPU Kernel 擴成16個 uint Min-mip 值。List
Context 保留不可變 Root／PSO／Paired Texture，並獨占 Opaque／Decoded Texture 與 Descriptor。
實作上限 Tier0.9，不改寫硬體1.0；實際 Reflection 與完整 Recipe 位元組識別在原生建立前拒絕
不相容產物。此路徑不取樣 Color、不連接 App Texture；通用 Streaming／Residency 與完整 Ultimate／
V2-GFX-4 仍待完成。

✅ 本地 Linux Development Configure／Build 與完整 CTest220/220 通過，零失敗／略過（282.94s）；
Shipping Minimal／Monolithic LTO 通過。真實 Vulkan 較低 Profile Compute 與 CPU 比對 Clear255、
0／1／16／64 Request、重複最小值、未寫入／Canary、全無效輸入，以及強制雙 Worker 取消／重試。
Xvfb 下 Khronos Object／Synchronization Validation18/18 通過（10.90s），確認39次 Layer 插入，無
Validation／VUID／同步錯誤。實際 Production CMake 編譯 Write4800B／SM6.5、Pack3468B／SM6.0，
驗證真實 Reflection 並產生完整 Recipe 識別。官方 SDK Helper 與 Windows 條件測試分支在警告視為
錯誤下通過語法檢查。原生過期／Context／Heap／Program 與 Node／Feedback／Node Oracle 已實作，
但 Windows 原生 Feedback 執行仍待完成；enabled0／dispatches0 僅代表 Compute 後備。

## 基線與範圍

`5201d9f6e02d2a24406004174a50f61ad9cebf9b` 的原始碼查核已確認原生 DX12 Queue、
Command List、Descriptor Heap、傳統 Resource Barrier、Graphics／Compute PSO、Compute
Dispatch 與 Indirect Draw。沿用現有實作，不將其重複計為新增工作。Windows DXIL 建置使用
SM 6.6 與 SM 6.0 profile；原生 Presentation 也使用 SM 5.0。這些事實不代表已通過
DX12 Ultimate 或更新 Shader 功能的驗收。

可程式化 Shader 已存在；統一 Shader 架構屬於硬體特性。本次不新增 DX9／DX10／DX11 API
後端，而是納入其中尚缺的渲染能力 Tessellation。降低 CPU 開銷必須與現有序列提交路徑
實測比較，不能只憑 API 版本宣稱達成。

## V2-GFX-2 有界 Enhanced Barriers 施工狀態

✅ Portable 同步／存取／Layout 獨立映射與 Null 明確不可用測試通過。
D3D12 新增預設關閉、受 SDK guard 保護的 list7 實作：將已准入的 Copy、Shader／UAV、Render Target
與 Indirect 用途分別映射同步、存取與 Layout，保留交易式 Snapshot，加入同用途 UAV 相依，以及
避免不安全 Legacy 交錯的 Enhanced Texture 讀回。SDK／硬體／介面可用性分開檢查；不支援時保留
Legacy 輸出。測試回報實際啟用狀態與已接受使用者 List 的 Buffer／Texture 計數。
Aliasing、Subresource 範圍、多原生 Queue 交接及完整 GPU Validation 驗收仍待完成。
Portable 測試與 Linux 編譯不代表 D3D12 執行。來源 `feat/v2-d3d12-enhanced-barriers`，
Beads `nexora-45y.3.2`；Hosted 啟用／後備證據待補。

main `1d48e918` 本地驗證：原生／Slang／Zig Development configure/build 與 Xvfb CTest
**238/238、零失敗／略過、293.89秒**（Showcase 開、Graphical Editor 關）。
聚焦映射／原生回歸 **6/6、0.71秒**；Shipping Monolithic／Minimal LTO 通過。
既有 Vulkan 原生物件／同步回歸 **10/10、4.00秒**，確認25次 Layer 插入，Validation／VUID／Sync
錯誤為零。Private 原生 Adapter 另以獨立暫存的 Microsoft DirectX-Headers
`461ef5138bd30922b44fbad77c4e47662812d2d5` 通過 Clang19 語法檢查；這僅驗證 SDK 識別字，
不是 D3D12 執行，不新增專案相依。

## 交付 Gate

所有完整 gate 仍為**待完成**，包含依能力啟用的路徑。Runtime fallback 能讓裝置繼續執行，
但不能取代加速路徑的實作或 target-host 驗收。

| Gate | V2 預定工作 | 相依 | 驗收與 Fallback |
| --- | --- | --- | --- |
| V2-GFX-1 | Capability／SDK／compiler matrix、多執行緒 Command Recording、CPU 開銷基準 | 既有 V2-M3 contract | 獨立查詢 feature tier 與 shader target；每個錄製 worker 獨占 allocator／list，依完成 fence 回收 allocator、descriptor 與資源。RenderGraph 管理相依順序與提交。比對序列／平行輸出，執行 validation、生命週期壓力及 CPU／GPU 計時；保留可選的序列錄製路徑。 |
| V2-GFX-2 | Tessellation、Enhanced Barriers | V2-GFX-1 能力與生命週期 contract | ✅ 有界拓撲／階段／Cache／Cooked Metadata 及 Linux 原生曲面／預細分網格比對（見上節）。更廣 Patch／Program 組裝與 Hosted 原生證據仍待完成，保留網格後備。將 graph access／sync／layout 對應 Enhanced Barriers，涵蓋 UAV、copy、aliasing 與 queue handoff；以 GPU validation 比較傳統 barrier 路徑，兩條路徑均保留。 |
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

## Hosted 原生 Work Graph 與 Ray 驗收（PR633）

✅ 自己的 PR633 在 ee4d8043 通過36檢查、兩輪 Workflow39029985978／39029989660 成功後，
以 `f347b90773470783a58a9eb28d3fc8c6633f27ba` Squash 合併。Push／PR 的 Windows314/314
（1317.40／808.31秒）、Linux365/365（1340.76／1337.94秒）、macOS304/304
（541.70／582.44秒）均通過。兩輪 Windows 實際執行 Work Graph enabled1／16 Dispatch、
Pipeline Ray enabled1／45 Call、Inline Query enabled1／45 Call；Legacy／Enhanced、
Node/Ray/Node Root、CPU／Compute 輸出比對、Generation／Revision 負例及取消／重播均通過。
Enhanced Node／Ray Buffer Barrier 分別68／354。Linux／Metal 明確回報 enabled0，執行
真實低 Profile Compute 後備。本節取代上述有界 Recipe 的 Windows 待驗收紀錄，僅關閉
子任務 nexora-45y.6.1／.4.2；一般 Graph Node、AS Update／Compaction、Lighting、
DXR1.2／SER、實體效能及完整 Gate 持續開放。

Windows64f83e47 完整 CTest 在兩種 Feedback Barrier 模式均發現真實 Decode Map 不符，尚未驗收。正式 MIN_MIP Transcode 規則要求來源 Subresource 為 UINT_MAX，表示攤平的整條 Opaque Mip Chain，已取代初版0；目的地仍為單一 R8_UINT Slice0。負例診斷新增 Native／Reference、Count 與每個 Actual／Expected Word。本機修正版 Development220/220 通過（125.60秒），真正 SDK Helper 語法、Session 清理25/25及文件／Workflow16/16 亦通過。仍須最終 Windows 原生證據，不由本機 Compute Oracle 推定 Feedback 完成。

### 隔離的新型 Compute 程式 — 實作進行中

預設關閉的 `NEXORA_ENABLE_D3D12_PREVIEW` 使用獨立 SHA-256 固定的 Agility
1.721.3-preview、WARP1.65535.20-preview 與 DXC1.10.2605.37。僅專用測試執行檔放置
app-local WARP／Agility，普通建置沿用原 SDK、編譯器與 Profile。明確的 D3D12 Device 選項
建立獨立 SDK721 Factory，禁止發布 Singleton，僅對該 Factory 啟用實驗 Shader Model；
不套用至 D3D12 全域狀態。Factory 的生命週期長於所有 Device 資源。一次性 CI 主機啟用
Developer Mode，後端本身不修改作業系統設定。

三種固定 Compute Variant 沿用 SRVt0／UAVu1 Root：SM6.9 八分量整數 Long Vector、
SM6.10 Thread Scope F32 4x4 矩陣／向量乘法，以及128執行緒、64 KiB Shared Storage 的
Reduction，帶有 `GroupSharedLimit(65536)`。後者宣告靜態配置上限，不是執行時動態配置
Shared Memory。原生准入分別檢查實際裝置 Profile、明確6.10許可／私有 Runtime、F32
Operation Query 或 Shared 容量、實際 DXIL Stage／Profile 與建置產生的完整 Recipe 位元組。
許可與 Variant 納入完整 Cache Identity；限制單一 Group、不同128-byte輸入／32-byte輸出，
沿用普通不可變 Recording／Revision 生命週期。Vulkan／Metal／Validation 拒絕這些 DXIL
Recipe，不會宣稱已執行。

公開 Preview Compiler 已通過本機正式編譯命令，包括真正65540-byte超額配置負例；
線性代數 DXIL 確實包含矩陣載入／乘法指令。Windows 測試要求三種原生 GPU 輸出與 Canary
符合 CPU Oracle，驗證過期／放棄 List 拒絕與已接受計數不變。Windows 原生執行及完整
整合 CI Gate 仍待驗證。通用新型 Shader 編寫／Cook、低 Profile Compute 後備整合、
DXR1.2／Opacity Micromap／SER 與實體 GPU 效能另列未完成工作；本切片不關閉 V2-GFX-5
或任何上層完整驗收 Gate。

Sampler Feedback 父版本上的本機整合驗證通過 Development220/220、零失敗／略過（132.25秒），Shipping Minimal／Monolithic／LTO 亦通過。三個公開 Windows 套件的固定 SHA-256 均驗證成功；正式編譯流程使用真正 Windows 套件 HLSL Header 亦通過。Windows Runtime 執行仍待驗證。

拆開 Sampler 父版本後，以 PR633 為基礎的獨立來源通過 Development219/219、零略過／失敗（154.39秒）、Shipping Minimal／Monolithic／LTO，以及 Xvfb Khronos 物件／同步17/17（18.24秒）、37次確認載入，零 Validation Error／VUID／SYNC-HAZARD。非 Windows Preview Configure 拒絕及雙語文件檢查亦通過；原生 Preview 執行仍待 Windows CI。

前一版本2f7d89c7 的兩輪 Windows Preview Job117400353506／117402090855 均通過1/1（2.09／0.29秒），實際載入 app-local WARP SM6.10、65536 Shared Bytes、F32 Matvec 支援，九組 Kernel 輸出／Canary／生命週期通過。後續補齊 Caller 未填最低版本時的一般產物准入：實際6.9以上 DXIL 仍須符合裝置上限，實際6.10仍要求明確許可／私有 Runtime；新增一般入口未啟用拒絕／啟用 PSO 建立且不 Submit 的原生測試。本機 Development 再通過219/219（122.37秒），真實 SDK 准入 Helper 及 Windows 條件語法亦通過。清理機制僅納入本次三個確切分支、Owner／同 Repository Actor 的過期進行中 SHA，保留最新 Head 的兩種 Event；取消安全測試25/25、文件／Workflow16/16 通過。合併前仍要求最終後續版本 Windows CI。

## 託管現代運算驗收（PR635）

✅ 有界 SM6.9 長向量、SM6.10 F32 矩陣／向量與靜態共用記憶體配方已通過原生測試並合併 PR635（b6fd5053）。4e35aef1 的 push／PR 兩套流程共38項檢查全數通過。Windows 專用工作117408135751／117408317595 載入固定版本 app-local WARP，通過九組輸出／canary／生命週期測試，以及未宣告最低6.10版本時的權限拒絕／明確允許 PSO 建立測試。兩套桌面矩陣均通過 Windows314/314、Linux365/365、macOS304/304。此處驗收固定配方的軟體原生執行；一般現代 Shader 編寫／cook、實體 GPU 效能及完整 V2-GFX-5 gate 仍未完成。官方 [Variable Group Shared Memory 提案](https://github.com/microsoft/hlsl-specs/blob/main/proposals/0049-variable-groupshared-memory.md) 保留編譯期陣列大小；此處已驗證裝置相關上限與 GroupSharedLimit，執行期動態配置並不屬於該規格。

Sampler Feedback 分支整合此已合併 runtime，保留三種配方的既有編號：feedback reference 新增於其後，並排除於現代 profile／dispatch 範圍之外。可攜准入與原生 fallback 工作負載驗證此區分。合併後原始碼完整迴歸及 feedback 原生驗收仍待通過。

合併後原始碼通過本機 Development220/220，零失敗／跳過（127.24s）、Shipping Minimal/Monolithic/LTO 及 Khronos 物件／同步18/18（10.75s），確認39次 layer 插入、零 Validation Error／VUID／SYNC-HAZARD 訊息。可攜准入證明 feedback reference 不會繼承 SM6.9／6.10 要求；原生 Vulkan 執行真正的較低 profile map oracle。Windows feedback 執行仍為必要 CI gate。

僅修正來源 subresource 尚不足以完成原生驗收：5218dee2 的 Windows 工作117411437732 通過314/316，但兩種 feedback barrier 模式均在 count1 失敗（觸及格要求 mip2、解碼為0；clear255 與全部 canary 保持正確）。仍要求精確比較。後續改以來源／目的 UINT_MAX 進行整張 map 解碼、僅在不相符時讀回 GPU request 診斷，並於隔離的固定版本 preview WARP 執行相同嚴格測試。Windows desktop 在完整建置前先跑原生 preflight；成功 head 仍須通過完整桌面矩陣。未完成比對前，不宣稱是 SDK／runtime 問題，也不宣稱 feedback 原生執行成功。

診斷／preflight 後續通過本機完整 Development220/220，零失敗／跳過（121.58s）、真正 SDK helper 與 Windows 條件 preview 測試語法（警告即錯誤）、雙語文件／workflow16/16 及 cleanup25/25。私有 sampler LOD 範圍明確限制於配對 texture 最後一層 mip6；允許的 request 仍為 mip0..2。整張 map 解碼與 preview 比對仍須通過原生 CI。

Preview feedback 測試直接進入獨立 factory，避免以系統 runtime 可用性探測 app-local preview WARP DLL。此測試強制要求 factory、SM6.10 與啟用的原生 feedback，不允許 fallback／跳過。完整本機迴歸再次通過220/220，零失敗／跳過（136.85s）；Windows 條件語法檢查以警告即錯誤通過。

成對比對已確認軟體 runtime 就緒狀態的邊界：ff684697 的 push preview 工作117424066271
通過3/3（0.63s），兩種 barrier 模式各13次原生 feedback dispatch，enhanced buffer barrier60次。
系統路徑117424066272 仍將要求的 mip2 解碼為0；GPU request 讀回確認實際上傳 mip2／enabled1。
原生程式／輸出精確比較不變。因此 backend 保留原始查詢 support／tier 及已實作程式碼 metadata，
但停用系統軟體路徑；軟體原生執行要求明確的獨立 preview runtime。預設軟體裝置選用真正的 compute
fallback。必要 preview 測試仍在原生 feedback 不可用時失敗，並驗證精確 mip／canary 輸出。
此為保守路徑 gate，不推論特定 DLL 有錯。實體 adapter 驗收及最終 head 完整 CI 仍未完成。

Runtime 就緒狀態後續通過本機完整 Development220/220，零失敗／跳過（125.85s）、雙語文件／workflow16/16、cleanup25/25 與 Windows 條件測試語法（警告即錯誤）。軟體路徑另要求已測試的 SM6.10 profile；原生 feedback 本身仍宣告真正的 SM6.5 需求。Preview 測試在 Work Graph 選項開啟時亦保留其 library 相依／環境設定，以於整合後延續 Node／Feedback／Node 覆蓋。
### 有界 HitObject／SER 語意路徑 — 實作進行中

獨立且預設關閉的 `NEXORA_ENABLE_D3D12_SER` 要求 Preview 建置與既有 Slang Ray／Compute
產物。`RayTracingMode::ReorderedPipeline` 使用固定 DXC lib_6_9 Recipe，實際包含
`HitObject::TraceRay`、`MaybeReorderThread`、`HitObject::Invoke`。真實 DXC Reflection
驗證 SRVt0／t1／UAVu1、32-byte Ray Input 與16-byte Hit／Payload ABI；Manifest 記錄
Compiler／Source／Binary Identity，不宣稱已執行。同 Profile 的真實變更 Recipe 負例、
原生 Export／Identifier 與完整位元組准入補強 Metadata 驗證。

正式 HLSL SER Device Support 要求 SM6.9 與 Raytracing 支援同時成立；Guard 保護的
OPTIONS22 Query 獨立回報是否真的重排執行緒。False 仍可具備合法 SER 功能，WARP 即是
此情況。未知 Query 保持拒絕准入；功能支援、已編譯 Recipe 與啟用狀態分開。OMM 路徑
實作前，既有有界 RT Implementation Ceiling 維持1.1。專用已接受 SER Call 計數只在
成功 Submit 後發布，並屬於普通 Ray Dispatch 的子集合；沿用專用 Root／State／Table、
不可變 Snapshot 與 Generation／Revision 退役生命週期。

原生 Preview Oracle 在普通 Ray／Query Fixture 加入 SER Mode，涵蓋1／2／8 Triangle、
1／64 Ray、CPU／原生 Compute 的 Hit／Miss／Nearest／Barycentric／Invalid 比對、Canary、
生命週期及 Worker 失敗／取消／重播、Legacy／Enhanced Barrier 與變更／遺失／錯誤產物。
要求真實原生 SER 准入／Call，以及獨立可用的 Actual Reordering Query。WARP 只驗證語意，
不證明實體重排、效能或 Coherence。Windows 原生證據待完成，完整 DXR1.2／SER／效能驗收
維持開放。

SER 候選版本本機 Native Slang／Zig Development219/219 通過、零失敗／略過（163.27秒），Shipping Minimal／Monolithic／LTO 及 Xvfb Khronos 物件／同步17/17（9.94秒）亦通過；37次確認載入，零 Validation Error／VUID／SYNC-HAZARD。正式 DXC Preview 編譯正常／負例 lib_6_9，驗證真實 Reflection／指令並產生完整 Pin。Windows 條件測試及真正 SDK OPTIONS22／Factory 語法檢查通過，仍不屬 Windows 原生執行證據。Workflow 路由16/16、雙語文件檢查通過。

### 有界 DXR1.2 Opacity Micromap — 實作進行中

獨立且預設關閉的 `NEXORA_ENABLE_D3D12_OPACITY_MICROMAPS` 要求固定 Preview／SER Recipe。
`CreateOpacityMicromapScene(triangles, masks)` 接受1～8個不可變有限 Triangle，各配一個
低四位元遮罩，依 A／中央／B／C 空間曲線順序編碼 Subdivision1 OC1 二狀態微三角形。
未知／Any-hit 狀態、更高細分與可變連結不屬本 Recipe。原生建置上傳128-byte對齊的遮罩、
八位元組 GPU Description 與 R32 Index，查詢真實有界 OMM Array Prebuild 需求，再錄製
真實 OMM Array Build／UAV Dependency／Linked BLAS Build／UAV Dependency／TLAS Build／
UAV Dependency。建置由 Caller 序列化，Fence 完成後才發布並退役 Scratch；Linked OMM
Storage 宣告於 BLAS／TLAS Owner 之前，每個 Command List Scene Snapshot 保留所有依賴。

`RayTracingMode::ReorderedOpacityPipeline` 獨立要求實際 RT1.2、SM6.9 與 SER 功能；原生
Config1 明確允許 OMM，沿用已 Reflection／完整 Pin 的 SER Library／Root／Payload／Table。
Scene／Program 的 Opacity 許可不符時，在原生 Traversal 前拒絕，避免普通 Pipeline 碰到
OMM 的未定義行為。僅此額外編譯選項將有界 RT Implementation Ceiling 由1.1提高到1.2，
SER 單獨不提高。原生 OMM Call 僅成功 Submit 後發布專用已接受計數，並納入 SER／普通 Ray。

Oracle 在1／2原始三角形、1／64條 Ray 下測試全部16種遮罩，涵蓋每個微三角形內點、透明
前層／不透明後層的最近交點、Miss、反向／非單位／Invalid Ray、原始 Primitive／Barycentric
與 Canary。獨立 Double CPU 分類器對照真正低 Profile GPU 的顯式不透明微幾何 Traversal，
包含空幾何／全透明；准入時，原生 OMM 結果必須符合兩者。八三角形全不透明原生案例沿用
雙 Worker 失敗／取消／重播、過期／銷毀／重建 Scene／Program／Input／Output、放棄 List、
八 Snapshot 上限及 Ray/Compute/Ray Root；普通／OMM Scene／Program 混用、遺失／高位元
遮罩均拒絕。Linux 後備及 SDK 語法證據不代表原生 OMM，Windows Preview 證據仍待完成。
四狀態／Any-hit、Update／Compaction、通用材質／Lighting／Cook 與實體完整 DXR／Ultimate
驗收維持開放。

整合 OMM 候選版本通過本機 Native Slang／Zig Development219/219、零略過／失敗（126.28秒）、Shipping Minimal／Monolithic／LTO，以及 Xvfb Khronos 物件／同步17/17（10.50秒）、37次確認載入，零 Validation Error／VUID／SYNC-HAZARD。真正 Agility721 OMM Build／Linkage／Config1 Helper 與 Windows 條件 SER／OMM 測試以警告即錯誤編譯通過；雙語文件／Workflow16/16 亦通過。Vulkan 明確回報 Opacity Native0，執行真正微幾何 GPU Oracle；Windows 原生 OMM 證據待完成。

### 原生 VRS Tier2 Rate Image — 實作進行中

有界 VRS 擴充加入不可變 `ShadingRateImageHandle` 與明確的 `ShadingRate::Image` Pipeline
模式。獨立查詢 Tier2／Tile Size 後才准入 Implementation Ceiling2.0；Tile Query 遺失／
無效時維持1.0 Ceiling 及 Per-draw／Full-rate 選項。僅接受1x1／1x2／2x1／2x2 編碼、
Raster Extent1～4096 與符合查詢 Tile Grid 的完整資料。使用私有 R8_UINT Storage，避免
新增公開 Texture／Cooked Format。建立時驗證有界 Copy Footprint，複製 Caller Bytes 至
私有 Upload，轉為 SHADING_RATE_SOURCE，等待 Caller 序列化的 Creation Fence 後才發布並
退役暫存 Upload。

每個 List 在所有原生 Command-list Interface 之前保留最多16個獨立不可變 Source Snapshot。
綁定須處於已選 Image Graphics Pipeline 且符合實際 Target Extent；Draw／Indirect／Mesh
要求明確 Image，重綁 Pipeline 時清除並還原原生 Base／Combiner／Image 狀態。Submit 在
Close／Execute 前驗證 Source Generation，放棄／過期／取消 List 不發布 Image Draw 計數。
不同 Target Worker 可共享不可變 Image。Image 已接受 Draw 與 Per-draw Coarse 計數分開，
不推定 Shading Invocation／效能下降。

Oracle 比對真正 Full-rate 與 Uniform／Mixed Image 的 RGBA／BGRA Coverage、32/255
Gradient Error 及 Full-rate Reset，涵蓋 Caller 記憶體修改、遺失／過期／重建／錯誤 Extent、
16 Snapshot 上限與雙 Worker 失敗／取消／重播。獨立 Preview WARP 測試要求 Legacy／
Enhanced Color Barrier 下實際 Tier2 執行，另以真實 DXC6.4 VS／PS 編碼硬體提供的
`SV_ShadingRate`，要求在非空 Coverage 觀察到1x1／1x2／2x1／2x2，允許 Primitive 邊緣
使用更細 Rate；Witness 亦在建立後修改 Caller Storage。Windows 原生證據仍待完成；
Vulkan／Metal 明確執行 Full-rate 後備，不代表原生 Rate Image。額外4x Rate、Primitive
Rate 編寫、GPU 產生可變 Image、實體預算及完整 Ultimate 驗收持續開放。預設 SDK／Slang
Profile、穩定 C ABI 與 Cooked Format 不變。公開 WARP1.0.13 已加入 VRS Tier2／Feature
Level12_2，固定 Preview 套件保留此文件化路徑，仍須真實輸出證據。

Rate-image 候選版本通過 Native Slang／Zig Development220/220、零略過／失敗（156.57秒）、Shipping Minimal／Monolithic／LTO，以及 Xvfb Khronos 物件／同步18/18（10.50秒）、39次確認載入，零 Validation Error／VUID／SYNC-HAZARD。真實 DXC Preview 編譯 Witness VS／PS6.4，真正 SDK Upload Helper 與 Windows 條件語法以警告即錯誤通過。Vulkan 回報 enabled0／tile0／draws0，執行真正 Full-rate Quality／Worker 後備。僅四個確切 Session 分支使用過期 Run 清理，安全測試27/27、文件／Workflow16/16 通過。Windows 原生 Image／Witness 執行仍待完成，不關閉完整 VRS／Ultimate Gate。

DXR1.2／SER 與 Tier2 Image 已整合為 Profile Runtime 父版本上的同一後續切片，共用可選原生驗收 Job。第一輪整合 Development220/220、零略過／失敗通過（122.61秒），Shipping Minimal／Monolithic／LTO 亦通過。真正 Agility721 宣告確認 Work Graph Tier1.1 的原生數值為11，Portable Decoder 保留硬體1.1，實作上限仍為1.0。可選原生驗收以固定 Preview DXC 啟用既有有界 Work Graph Recipe，於 Test Environment 設定後加入正確 Library 與依賴，延伸 Node／SER／Node 互通，不宣稱 Mesh Node。仍須最終整合回歸與 Windows 原生執行。

最終整合迴歸通過 Development220/220，零失敗／跳過（121.81s）、Shipping Minimal/Monolithic/LTO，以及 Khronos 物件／同步18/18（11.21s），確認39次 layer 插入、零 Validation Error／VUID／SYNC-HAZARD 訊息。Windows 原生 SER／OMM／VRS2 驗收仍待通過。

### SM6.10 功能名稱與託管 profile 驗收

✅ PR635 已在固定版本 app-local WARP 驗收三種現代運算配方：兩個 Windows 原生工作均通過九組輸出／canary／生命週期及一般6.10權限檢查；兩套矩陣均通過 Windows314/314、Linux365/365、macOS304/304，共38項檢查全數通過。官方 [Variable Group Shared Memory 提案](https://github.com/microsoft/hlsl-specs/blob/main/proposals/0049-variable-groupshared-memory.md) 明確保留編譯期陣列大小：功能是查詢裝置相關上限與 `[GroupSharedLimit]`，64 KiB 配方及超額配置負例已驗證兩者。執行期動態配置不屬於此規格。一般編寫／cook 及實體 GPU 驗收仍未完成。

MSVC 原生驗收發現 VRS witness 的有號／無號比較及 byte fill 窄化問題。明確使用無號比較運算元與 uint8_t fill 值，保留全部像素與呼叫端資料變更斷言；警告即錯誤仍維持啟用。Windows 原生 SER／OMM／VRS2 驗收仍須通過。

MSVC 型別修正後，本機完整 Development220/220 通過，零跳過／失敗（128.17s）；Windows 條件 witness 語法檢查（警告即錯誤）、雙語文件／workflow16/16 與 cleanup27/27 亦通過。Windows 原生執行仍為最終 head 必須通過的 CI gate。

15178ebd 的 Windows 工作117420601029／117420450398 以 MSVC 警告即錯誤編譯通過，並通過兩種原生 VRS2 模式：tile8、imageDraws14，五種 witness 圖樣均 covered512／exact512。現代運算亦通過。兩個 SER／OMM 測試均在輸出 ray mode 前觸發有界配置拒絕，故不宣稱原生 SER／OMM 成功。後續允許零 scratch 需求（傳入空 scratch 位址），結果配置仍必須大於零，全部配置仍限制64MiB；各操作 prebuild 結果／scratch 診斷於原生配置前拒絕無效尺寸。仍須由原生 CI 釐清原失敗是零 scratch 或無效 prebuild 輸出。

光追 prebuild／scratch 後續通過本機完整 Development220/220，零失敗／跳過（122.52s）、Shipping Minimal/Monolithic/LTO 與真正 Agility721 全部進階 helper 語法檢查（警告即錯誤）。雙語文件／workflow16/16 與 cleanup27/27 通過。仍須通過原生 SER／OMM 執行驗證。

### Runtime 就緒條件修正與已驗收進階路徑整合

最終8b1c8995 系統 Windows preflight 仍將明確 mip2 解成 mip0，且裝置回報
`software_device=false`。先前僅限制軟體裝置的條件不足；該旗標不能證明 feedback 正確。
所有裝置的原生 feedback 現在均要求明確的隔離 preview runtime 與已測試 SM6.10 profile。
保留原始 support／tier，一般裝置執行既有真正 compute reference。必要 preview 測試仍要求
原生執行及完全相等的輸出／canary，不得選擇 fallback。實體裝置與其他 runtime 路徑仍須
同等原生證據方可驗收。

✅ PR636 已於 push／PR 兩套流程全部38項最終版本檢查通過後合併：Windows315/315、
Linux366/366、macOS305/305；兩個 preview 工作均通過5/5，涵蓋有界 SER／DXR1.2 OMM、
VRS2 witness 與現代配方。此 feedback 分支整合已驗收實作，保留功能 enum 與有界現代
variant 判斷。Preview 工作改為七項測試，包含兩種 feedback barrier 模式及
Node／Feedback／Node。最終整合本機與託管驗證仍須通過；實體效能與完整 V2-GFX gate 維持開放。

整合後最終本機 gate 通過 Development223/223，零失敗／跳過（130.05s）、Shipping Minimal／Monolithic／LTO，以及 Khronos 物件／同步19/19（10.18s），確認41次 layer 插入，零 Validation Error／VUID／SYNC-HAZARD。真正 feedback SDK helper 與 Windows 條件 preview 測試語法以警告即錯誤通過；文件／workflow16/16、清理安全測試27/27 通過。合併前仍須通過託管 Windows runtime 就緒狀態及全部最終版本 CI。

現代運算的較低 profile 等效路徑施工中：明確的 LongVectorReference、LinearAlgebraReference、GroupSharedReference 以基準 Slang DXIL6.0／SPIR-V／Metal 保留已驗收固定配方的 buffer ABI。沿用明確 preferred／fallback 選擇；不宣稱這些變體執行 SM6.9／6.10。驗收前仍須原生結果對照、生命週期／同步與完整平台驗證。

### 較低 profile 現代運算等效路徑：本機驗收

✅ 原生 Vulkan 執行三種基準等效路徑，通過15組正負／零／溢位資料、獨立 CPU oracle 與精確輸出 canary。
另通過範圍、缺少／別名／過小 binding、CPU 輸入／輸出變更及 handle 重建、放棄錄製、三工作執行緒
失敗／取消／重試，以及同一列表三配方切換後最後 binding 無 dispatch 的測試。CPU reduction oracle
使用與 Shader 迴圈獨立的封閉公式。Vulkan／Metal 僅針對等效變體新增版本 snapshot，在原生 Close／
queue 接受前檢查，接受後才公布寫入版本。

Development configure／build／完整 Xvfb CTest 通過221/221，零失敗／跳過（168.88s）；Shipping
Minimal／Monolithic／LTO 通過108個 target。Khronos 物件／同步19/19 通過11.13s，確認41次 layer
插入、零 Validation Error／VUID／SYNC-HAZARD 訊息，包含原生 handle 重建。三種真正 DXIL6.0、
SPIR-V、Metal 編譯配方與可攜語法檢查通過。先前磁碟滿載的測試不列為驗收；清理過期任務建置暫存後，
以上完整重跑已通過。Windows／Metal 原生 runtime 與完整託管 CI 仍待通過；不宣稱執行現代指令或實體效能。

最終審查原始碼再次通過完整 Development221/221，零失敗／跳過（127.44s）；Shipping 重建通過。雙語文件／workflow16/16 與 guarded cleanup25/25 通過。以上本機 gate 不推論 Windows／Metal runtime 或前置 PR 已合併。

整合後較低 profile 等效候選版本通過 Native Slang／Zig Development224/224，零失敗／跳過（130.55s）、Shipping Minimal／Monolithic／LTO，以及 Khronos 物件／同步20/20（10.95s），確認43次 layer 插入，零 Validation Error／VUID／SYNC-HAZARD。編譯語法及文件／workflow16/16 通過。過期 CI 清理僅加入第五個確切自有圖形分支；29/29 安全測試保留現行 head 的兩種事件及其他 actor。父 PR634 最終 head 的兩組原生 preview 均通過7/7，包含每種 barrier 模式14次 feedback dispatch、64次 enhanced buffer barrier 與 Node／Feedback／Node；其完整 CI 及本候選版本的目標平台 CI 仍須通過。

最終版本 preview 工作117482265049 在建置後複製 Agility SDK 目錄時遭 Permission denied，尚未進入 CTest。Modern／SER／VRS 測試程式原先共用 preview-runtime 目的目錄，且各自執行複製。現在每個預覽程式使用獨立部署目錄（modern、SER、VRS image、feedback）；既有相對 target 的 DLL 複製與 CTest 工作目錄讓 ./Agility/ 查找保持私有。此修正消除共用目的目錄的並行寫入，不放寬原生測試。除已觀察到的複製失敗外，不推定 Windows 拒絕存取的確切原因。部署修正仍須通過本機與完整託管驗證。

部署隔離修正通過完整 Development224/224，零失敗／跳過（123.20s）、Shipping Minimal／Monolithic／LTO、雙語文件／workflow16/16 與清理安全測試29/29。合併前仍須通過原生 Windows 編譯／部署與七項 preview 測試，以及兩套完整最終版本 workflow。
