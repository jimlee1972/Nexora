# Nexora 2D 引擎 Roadmap

> 版本：v1.6 | 狀態：雲端能力交付已驗收，PC 主機 gate 待完成 | 更新：2026-10-16

> **能力驗收進度：82%**（9/11，四捨五入；P0 6/8）。M0～M5 與 M8～M10 已有
> Portable／Linux 功能證據。M6 本機 DX12 Screening 超出預算，最低規格實體 PC 驗收仍待完成。M7 乾淨 Windows
> Runtime／Shipping 維持 **PARTIAL／NOT ACCEPTED**；重建的目前 main VM 封裝已通過 CPU、
> 原生軟體 DX12、Zig Instancing 及人工鍵盤移動，完整裝置／產品整合與 Vulkan 仍待完成。
> 這不是完整 Windows PC 產品驗收。

## 1. 範圍與基線

在 V1 基礎上補齊大量 Sprite、俯視視角、單機 Windows PC 應用需要的引擎能力。
正交相機、Unlit 材質、實例化、VFX Sprite、UI／文字、輸入、音效、非同步資產、序列化與
打包已有規劃或切片。可選 TwoD 模組已提供場景 Sprite 擷取與輕量 2D 查詢，沿用 World、
資產、Core VFS、原生 Presentation 與 EditorImGuiHost。實體 PC 效能及乾淨 Windows
整合維持獨立；不替換 Jolt；Gameplay 規則維持範圍外。

範圍包含可重用的渲染、資產、查詢、Runtime 生命週期、效能分析、打包與製作工具。
Gameplay 規則、戰鬥、刷怪邏輯、成長與遊戲內容不列入本 Roadmap。

## 2. 能力里程碑

P0 建立可用 Runtime 與證據；P1 改善製作效率，Tilemap 為條件式。
每項完整驗收通過前均維持未驗收，通過後才使用 `✅`。

| ID | 優先順序 | 能力 | 必要內容 | 驗收條件 | 目前判斷 |
| --- | --- | --- | --- | --- | --- |
| ✅ 2D-M0 | P0 | 場景 Sprite Renderer | 圖集區域、Pivot、翻轉、顏色／透明度、排序層與穩定 Y 軸排序；沿用 Transform／正交相機 | 原生畫面驗證 UV、Pivot、翻轉、透明度及重疊排序；儲存／重開保留欄位 | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M1 | P0 | Sprite 逐格動畫 | 逐格時間／播放速度、循環、切換、動畫事件，與骨架動畫分開 | 幀／事件紀錄驗證循環邊界、切換、暫停／恢復與大時間步進，事件不重複 | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M2 | P0 | 2D 資產管線 | 切圖、可重現 Atlas 打包、匯入／取樣設定、透明邊緣處理與穩定引用 | 相同輸入／設定／工具版本重複產生一致 Atlas 版面、區域及輸出雜湊；匯入／Cook／重新匯入保留引用；原生取樣無邊緣滲色；無效輸入保留已發布資產 | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M3 | P0 | 輕量 2D 碰撞／查詢 | 圓形／AABB 重疊、分組、範圍查詢、空間索引與高速移動掃掠檢查 | 以獨立暴力比對驗證索引結果，涵蓋邊界、分組、高速移動及插入／移除 | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M4 | P0 | 大量物件生命週期／更新 | 批量生成／回收、世代識別、配置控制、資料導向更新與固定時間步進 | 反覆操作拒絕過期 handle、安全釋放並記錄配置／幀時間尖峰；不同 frame delta 驗證固定步數、順序、累積餘額及相同模擬時間下的等價狀態 | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M5 | P0 | 大量 Sprite 渲染 | 經由既有 RHI 整合圖集批次／實例化、可見性剔除、透明排序與 overdraw 控制 | 原生畫面保留跨批次排序；稀疏／重疊負載記錄繪製呼叫數與 CPU／GPU 時間 | ✅ Portable／Linux 能力驗收；證據見下方 |
| 2D-M6 | P0 | 效能分析／壓力驗收 | CPU／GPU 幀時間、記憶體、繪製呼叫、配置尖峰與透明重疊 | 可重現混合引擎負載在指定最低 PC 配備達到預定 60 FPS／幀時間預算；保留 trace／設定 | 本機 DX12 Screening 已記錄；最低 PC gate 為 NOT ACCEPTED |
| 2D-M7 | P0 | Windows Runtime／Shipping 驗收 | 輸入、UI／文字、音效、非同步載入、序列化／存檔、Cooked 資產與獨立打包 | 乾淨 Windows 電腦無 Editor／開發依賴即可執行 Cooked 2D 驗證程式；輸入、UI／文字呈現、音訊播放、非同步載入完成／失敗、重載、存檔／重開及釋放通過 | 既有 Runtime 下，重建 VM CPU／軟體 DX12／Zig 與人工鍵盤切片通過；完整產品／裝置 gate 與 Vulkan 待完成；M7 NOT ACCEPTED |
| ✅ 2D-M8 | P1 | 2D 場景製作工具 | Sprite 放置、Pivot／排序編輯、碰撞視覺化與 Editor 正交預覽 | 編輯／預覽／Undo／Redo／儲存／重開保留場景；隔離 Play 不改製作 World | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M9 | P1 | Atlas／動畫工具 | 切圖／Atlas 預覽、打包診斷、逐格編輯、播放／事件預覽 | Cooked 輸出與 Runtime 呈現一致；驗證 Undo／Redo、無效資料及重新匯入 | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M10 | P1，條件式 | Tilemap | 圖層、繪製、碰撞資料、區塊載入／剔除 | 實作繪製、圖層／碰撞資料及區塊載入／剔除，通過 Undo／儲存／重開及區塊邊界渲染／查詢測試；決定延後仍維持未驗收 | ✅ Portable／Linux 能力驗收；證據見下方 |

## 3. 依賴與施工順序

1. 盤點 Runtime、Renderer、資產／Cook、查詢、Editor 與公共 C ABI；新增模組／API 前
   記錄可重用 Contract、缺口及平台證據。
2. 定義 M0／M2 Sprite／Atlas 資料，再完成渲染、匯入／Cook 與 M1 動畫。
   完整製作工具就緒前可用合成 fixture 驗證 Runtime。
3. 完成 M3 查詢與 M4 生命週期／更新整合，不另建 World、Scheduler、Asset Manager 或複製 3D Physics 服務。
4. 將 M5 批次／剔除接入 M0／M2／M4，及早量測；渲染／查詢／更新整合後才驗收 M6。
5. 依其他已驗收 P0 能力完成 M7 Cooked Windows 打包。
6. 沿用 Editor shell、資產及場景 Transaction 完成 M8／M9；地圖流程確定後評估 M10。
   Editor 里程碑驗收維持獨立。

## 4. 整合邊界

- Renderer 擷取 Sprite 資料並管理批次／排序；RHI 保留後端中立資源／命令，不另建私有 UI／VFX 場景渲染分支。
- Runtime 管理 Component 狀態、固定步進及識別；Atlas／Clip 沿用既有匯入／Cook、穩定引用、世代與退役釋放規則。
- 查詢回傳通用重疊／命中資料，由應用解釋；完整 2D 剛體延後，本計畫不選定新物理相依。
- Editor 管理製作 Metadata／Transaction；Shipping 不得依賴其 UI 或工具。
- 後續公共 C++／C ABI、序列化與 Optional Module 變更須審查 Contract；模組相依登錄
  `Config/Modules/modules.json` 並提供 Feature Option。TwoD 以 Feature 控制，依賴 Runtime／Presentation；
  Editor 條件式依賴 TwoD。公共 C++ 新增需重建 Consumer；原 V3 C／Zig 前綴保留，新增可選 2D 擴充。

## 5. 效能與平台證據

宣稱 60 FPS 前，先固定最低 CPU／GPU／RAM、Windows 版本、後端／Driver、解析度、建置與
Frame Pacing；固定可見／更新／碰撞物件數、Atlas／材質數、Sprite 像素覆蓋／重疊、動畫頻率、
VFX／UI 負載與載入變動量。硬體／負載預算目前為 **TBD**；物件數本身不是效能保證。

使用可重現合成物件、動畫、移動查詢形狀、VFX 與 UI，不需遊戲機制。
涵蓋基線、大量物件、透明重疊及配置／載入尖峰；保留暖機／測試時間、平均／p95／p99／
最大幀時間、可取得時的獨立 CPU／GPU 區間、記憶體峰值與遺失樣本。
60 FPS 對應 16.67 ms 幀預算，先定義允許尖峰再驗收；軟體光柵器只作正確性證據。

Windows DX12 為主；提供 Windows Vulkan 支援時另記錄，證據不跨後端轉用。
Linux 可驗證 Portable Contract 與 Vulkan 正確性；Windows 實體 PC／乾淨機器證據須由
對應主機提供，缺少 Gate 標為 `NOT RUN`，不可標為 `PASS`。

## 6. 狀態、延後範圍與相關文件

11 個里程碑只在本 Roadmap 等權計算；P0 依實際已驗收數另報 x/8，目前為 6/8，全部取得證據才是 8/8。
延後 Tilemap 保留在分母直到經審查修訂範圍。
完成規劃不改 V1 基礎、Editor 或聚焦 AI 規劃百分比。細部驗收保留在模組／工具文件或證據目錄；
Beads 仍是持久任務追蹤來源。

延後：完整 2D 剛體、多人網路、額外平台，以及超出所列 P1 流程的進階製作功能。

## 7. 雲端交付與保留證據

✅ M0／M5：Affine 正交擷取、Pivot／UV／翻轉、Tint／Alpha、穩定 Layer／Order／Y 排序、
相鄰 Atlas 批次、剔除、共用原生 DrawSprites／HUD、最近點／線性取樣、Immutable 常駐與
GPU 安全退役。`twod.native_pixels` 驗證 Linux Vulkan 實際像素，包括跨 Atlas 透明重疊、
Gutter 與 Chunk 邊界。
✅ M1／M2：可重現 PNG 矩形／Grid 切片、Canonical Pack／Import／Cook／Reimport、UUID
參照與 Codec；Loop／Switch／Pause／大步長／事件紀錄及無效發布拒絕。`twod.asset_tool`、
`twod.runtime_contracts` 與 Cooked 原生雲端驗收保留證據。
✅ M3／M4：獨立暴力比對的 Circle／AABB／Mask 查詢與精確 Point／Disc Sweep；原子重建、
世代批次建立／回收、過期／外部 Handle／重入拒絕及 Fixed-step 分割等價。混合負載實際更新、
查詢、動畫、回收與重載。
✅ M8／M9／M10：沿用 Editor World／UndoStack／ImGui Host，提供獨立 2D Document 與
Studio 頁籤、Sprite／Collider／正交預覽、Atlas／Clip 逐格事件控制及分層 Tilemap 繪製。
存檔／重開、Undo／Redo、唯讀／無效編輯拒絕、隔離 Play 及負座標 Chunk 畫面／查詢邊界
通過。完整主 Editor 里程碑驗收與 Project Lease 整合仍獨立追蹤。

M6 雲端切片：✅ CPU／GPU 時間、Load／Reload 尖峰、Process RSS、批次、AABB 覆蓋、
Sparse／Overlap 情境皆可重現。硬體／最低規格／尖峰預算仍為 TBD；軟體 Vulkan 數據不代表
實體 PC 60 FPS gate 通過。
M7 雲端切片：✅ Linux Monolithic Shipping 已用 Cooked Hash、Core 非同步載入、損毀拒絕
及不依賴 Editor／Development Engine Library 的獨立封裝驗證。第 10 節記錄 Windows VM
相依／CPU 部分驗證；原生 DX12 失敗、Vulkan 不可用。鍵鼠／手把、UI／文字、實際音效、
存檔／重開、顯示／DPI 及完整產品關閉仍未驗證。**M7 尚未通過（NOT ACCEPTED）**；
CI 的 Windows 編譯不等於這項驗收。

保留的 [Linux 驗收紀錄](../../Engine/TwoD/evidence/Linux-2026-10-14/acceptance.md) 包含
指令、平台範圍、擷取圖與負載 CSV。參閱 [模組 Contract](../../Engine/TwoD/README.md) 與
[工具](../../Tools/TwoD/README.md)。產生的報告位於 `build/<preset>/artifacts/`。
雲端可執行工作已完成；實體目標主機 follow-up 保留於 Beads。

## 8. Zig 引擎控制擴充（2026-10-14）

✅ C ABI 以附加欄位、能力／大小／版本檢查，讓 Zig 控制 Sprite／Collider／正交相機、
Clip 播放／暫停／推進與有序事件緩衝、快照式重疊／掃掠查詢、Tilemap 繪製／區塊狀態，
以及具世代識別的原子池化批次。Runtime 不依賴 TwoD；未啟用的 Host 不宣告此能力，
既有 V3 Consumer 保留原前綴。
✅ 實際 Zig 0.14 Consumer 透過靜態／動態模組控制既有 World 與 2D 擷取，Reload 遷移
ID／Handle 而不重複建立內容；C／C++／Zig 布局及橋接負面契約通過。Linux／Xvfb／Vulkan
輸入驗收呈現 600 幀，確認標準化 D 鍵經 Zig 移動實體。見
[使用範例](../../Gameplay/Zig/README.md)與[契約](../../Engine/TwoD/README.md)。

本項補上應用 Gameplay 使用的引擎介面，遊戲規則仍不列入；不提高既有 9/11 里程碑
計數，也不代表 M6／M7 實體 Windows 驗收。主 Editor 專案／Play 掛接與正式遊戲內容
屬於另外的整合工作。

✅ 可選 Editor 現提供擁有式 `TwoDPlayContext` 前置項：隔離文件 World／Sprite／碰撞資料、
凍結 Atlas／Clip、複製 Tilemap／區塊，並以新的有限物件池包住標準 2D Bridge。
實際模組 Callback、32 次 Session 生命週期、擁有式繪圖資料與來源／History 保全已有驗證；
Linux 原生像素區分相同 Atlas UUID 下的凍結紅、動畫及保留綠、新 Session 藍。
主專案／Cooked 資產准入與整合 Game 預覽／Reload 仍待完成；里程碑數不變。
[證據](../../Tools/Build/evidence/EditorEDM3-TwoDPlayContext-Linux-2026-10-15.md)。


✅ `TwoDPlayAssets` 現可從明確 Runtime 資產世代凍結有限的 Cooked Atlas／Clip 相依閉包，
驗證相依與 Frame Region，並於替換、世代回收及 Store 銷毀後保留擁有式資料。
成功／失敗皆平衡暫時 Pin 與 Resident 引用。這是整合前置項，不代表專案／來源准入，
也未接上主 Editor Play／Game 預覽；相關整合仍待完成，里程碑計數不變。
[證據](../../Tools/Build/evidence/EditorEDM3-TwoDPlayAssets-Linux-2026-10-15.md)。

[保留的 Zig／Linux 驗收](../../Engine/TwoD/evidence/Zig-Linux-2026-10-14/acceptance.md)
記錄完整測試、首次 GUI 失敗／重跑與 Shipping 範圍。

## 9. Zig 控制 GPU Instancing（2026-10-15）

✅ `SpriteFrame::Draw` 現在以 44 bytes 的 Affine／UV／Tint 資料提交至原生
Vulkan／DX12／Metal 的 Instance-rate Vertex Pipeline，由 Vertex ID 產生六個 Quad 頂點。
沿用 Zig V1 的批次建立／讀取／更新／刪除，不改 C ABI。投影、剔除、排序及更新仍在 CPU；
CPU 三角形快照仍供 Editor 預覽及檢查。只有透明排序後相鄰且 Atlas 相容的 Sprite 才共用
Draw；背景／HUD、不可變 Atlas 世代、最近點／線性取樣與 In-flight Buffer 所有權皆保留。
無效／非 Affine Quad 或溢位 Instance 範圍在原生提交前拒絕。

✅ Linux Vulkan 像素驗證涵蓋單次 256 Instances、跨 Atlas 透明排序、HUD、UV 翻轉、
線性取樣與 Tilemap 區塊變更。實際 Zig 輸入／Reload 驗證呈現 600 幀，並檢查原生
Instanced Draw／Instance 數量／上傳 Bytes。使用方法見
[Zig 文件](../../Gameplay/Zig/README.md#gpu-instanced-2d-rendering-from-zig)、
[Presentation Contract](../../Engine/Presentation/README.md) 及
[驗收紀錄](../../Engine/TwoD/evidence/GPU-Instancing-Linux-2026-10-15/acceptance.md)。

這補齊既有 M5 的原生實例化能力，不增加 9/11、P0 6/8。Windows 實體 M6／M7 仍未驗收；
下方較舊 VM 封裝切片不驗證本次 Instancing。Compute 模擬／剔除、Indirect Draw、Script 自有 GPU Buffer 與通用
Mesh／Material Instancing 留待此 2D 切片之後實作。

## 10. Windows VirtualBox 部分驗收（2026-10-15）

✅ 凍結的 Monolithic Shipping 封裝在 Windows 10 VirtualBox guest 核對 EXE／Atlas／Clip
三個雜湊一致。明確安裝 Microsoft 簽章的 VC++ Runtime 14.51.36247.0 後，兩種配置各六組
CPU 負載、Cooked 非同步載入／Reload 與損毀 Atlas 拒絕通過。原始基線因缺少 Runtime DLL，
以 `0xC0000135` 啟動失敗。
[驗收與原始證據](../../Engine/TwoD/evidence/Windows-VirtualBox-2026-10-15/acceptance.md)
保留執行檔來源、精確指令、雜湊、退出碼與限制。

**M7 維持 PARTIAL／NOT ACCEPTED，尚未通過。** 3D 關閉時軟體 DX12 路徑在
`stress draw` 失敗；3D 開啟時 VirtualBox adapter 拒絕建立 DX12 裝置（`0x887A0004`），
程式在 `stress acquire` 失敗。Vulkan loader／ICD 未找到，受測封裝也未啟用 Vulkan。
實際輸入／手把、音效、UI／文字畫面、VM Studio 存檔／重開、顯示／DPI 與成功原生呈現
仍未驗證。凍結執行檔早於最新 Instancing 程式碼，本切片不驗證目前 main。
M6 仍待完成，進度保持 **9/11、P0 6/8（82%）**。

VM 已正常關機並恢復 3D 關閉／64 MiB VRAM，保留已安裝的 Runtime。
Beads 父任務 `nexora-j4q` 保持開啟，執行紀錄為 `nexora-j4q.4`；依使用者要求在此告一段落。

## 11. 本機 Windows 開發 PC 切片（2026-10-14–15）

✅ 實體 AMD FX-6100／GTX 960／Windows 10 主機執行完整 MSVC Development 測試、
原生 DX12 Studio 文件生命週期、600 幀 Monolithic Shipping Zig Consumer，以及隔離的
Cooked Shipping 封裝、非同步載入／雜湊／損毀拒絕。六種混合原生 Shipping 負載保留
CPU／GPU／幀時間分布與 Process 記憶體採樣。
[Windows 證據](../../Engine/TwoD/evidence/Windows-2026-10-14/acceptance.md) 記錄精確指令、
硬體／Driver、量測前固定的 Screening 預算、原始結果及 MSVC Fixture 修正。

M6 維持 **NOT ACCEPTED**：各案例皆超出量測前固定的 Screening 預算；10,000 一般／
稀疏物件的整幀 p95 為 53.68／61.00 ms，GPU p95 小於 0.8 ms。這次 Screening
沒有宣告產品最低硬體，也沒有驗證持續的完整 UI／VFX／配置負載預算。本開發 PC 執行未跑 M7 乾淨主機 gate（上方另一次 VM 切片為部分執行／尚未通過）；實際鍵鼠／手把、
可聽音效、乾淨主機 Redistributable、顯示／DPI 切換及獨立 Windows Vulkan 執行仍待驗證。
Computer Use 初始化失敗，不推定實際輸入或擷取圖驗收通過；Zig 移動使用合成方向輸入。
使用者明確限定本次完成目前主機可驗證部分。進度保持 **9/11、P0 6/8（82%）**；
Beads 父任務 `nexora-j4q` 保持開啟，本切片由 `nexora-j4q.1` 追蹤；
CPU／幀節奏 follow-up 為 `nexora-j4q.2`。

本次實體量測的凍結執行檔早於上方 GPU Instancing 更新，這些結果不驗證目前 main 的渲染效能。


## 12. 重建版本的 Windows VirtualBox M7 切片（2026-10-15–16）

✅ 從 `350c40ee` 重建 Monolithic Shipping，客體核對四個 SHA-256 一致。六組 CPU 負載
（各 48 個量測幀）、六組原生 DX12 負載（各八幀）、Cooked 非同步載入／Reload 與損毀 Atlas
拒絕皆通過。Microsoft Basic Render Driver 軟體路徑完成 120／1,800 幀 Zig GPU Instancing，
另一次客體桌面可見視窗也完成 1,800 幀。舊執行檔的 `stress draw` 失敗在這個封裝未重現，
但不推定已定位或修復其根因。使用者確認 D／W 移動及 Esc 結束正常；獨立報告記錄 353 幀、
48 個移動幀、正值 Entity X、退出碼 0 與配置平衡。本次提早結束而未 Reload，另由合成輸入測試驗證。

✅ 獨立 Monolithic Development Studio 通過合成存檔／重開與另一程序唯讀重開。使用者確認
四分頁／文字／預覽及 Add sprite → Save → Open；新雜湊由獨立重開保持一致。原版按關閉鍵退出碼
為 1，已以兩行工具修正，在原生事件泵送後重查 CloseRequested。重建後由使用者關閉，退出碼 0、
無殘留 Studio 程序且文件不變。`twod.cloud_native` 補上 Linux 原生關閉回歸，交由完整 PR CI 驗證。
這是獨立製作工具證據，不代表完整 M7 通過。

[目前封裝 VM 證據](../../Engine/TwoD/evidence/Windows-VirtualBox-Current-2026-10-16/acceptance.md)
保留精確建置、雜湊、原始結果、畫面及範圍。Guest 保留上次安裝的 VC++ Runtime，PATH 無開發工具。
M7 維持 **PARTIAL／NOT ACCEPTED**：手把、可聽平台音訊、完整產品／UI／文字／存檔／釋放與
顯示／DPI gate 仍待完成。現有 Benchmark／Zig 程式不輸出裝置音訊，Runtime AudioEngine
亦不做裝置 I/O；缺少的整合驗收流程由 `nexora-j4q.7` 追蹤。
M6 實體效能與 9/11、P0 6/8（82%）不變。

- [Roadmap 索引](../ReadMe.md)
- [V1 完整規劃書](跨平台3D_Engine_V1_完整規劃書_v1_2.md)
- [V1 AI 施工規劃](跨平台3D_Engine_V1_AI施工技術與系統規劃_v1_2.md)
- [圖形化 Editor Roadmap](Editor_Roadmap.md)
- [聚焦 Roadmap AI 施工規劃](聚焦_Roadmap_AI施工技術與系統規劃.md)
- [Renderer](../../Engine/Renderer/README.md)、[Runtime](../../Engine/Runtime/README.md)
  與 [Editor Contract](../../Engine/Editor/README.md)
- [英文版](../en/2D_Roadmap.md)

## Zig 執行期服務整合（2026-10-16）

✅ 可選 Zig 服務已串接可重綁的鍵盤／滑鼠／手把動作快照、失焦／斷線重設、指標位置／位移／滾輪、
使用自備 UTF-8 字形圖集的原生 UI packet、PCM／WAV 音樂音效控制、自訂 schema 永久存檔，以及既有
cooked Atlas／Clip 註冊。2D 模組主機接收原生事件、保留可重綁 UI 動作、呈現 UI overlay，並解析
已註冊素材；啟動測試素材不再是唯一載入途徑。ProjectPlayer 可明確載入 V3 遊戲模組以使用既有
StaticView adapter，不會宣告尚未支援的 UI／2D／content 服務。詳見
[Zig 契約](../../Gameplay/Zig/README.md#optional-zig-gameplay-services) 與
[Linux 驗收](../../Tools/Build/evidence/Zig-Gameplay-Services-Linux-2026-10-16.md)。

此功能支援遊戲作者使用 Zig，未新增戰鬥規則或正式美術。M6／M7 與 82% 能力摘要維持不變：
實體 Windows 效能、裝置音訊／手把、字型外觀、完整 2D 專案匯出與乾淨主機產品驗收仍是獨立關卡。
