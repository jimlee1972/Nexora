# Nexora 2D 引擎 Roadmap

> 版本：v1.7 | 狀態：雲端能力交付已驗收，PC 主機 gate 待完成 | 更新：2026-10-19

> **能力驗收進度：82%**（9/11，四捨五入；P0 6/8）。M0～M5 與 M8～M10 已有
> Portable／Linux 功能證據。M6 改以目前開發機 5,000 物件整合驗收為必要負載，狀態為 NOT RUN；
> 保留的 10,000 物件壓力篩檢 23/24、整合短測 3/4、30 分鐘長測 1/2，歷史判定不變。M6 仍待完成。
> M7 乾淨 Windows
> Runtime／Shipping 維持 **PARTIAL／NOT ACCEPTED**；重建的目前 main VM 封裝已通過 CPU、
> 原生軟體 DX12、Zig Instancing 及人工鍵盤移動，完整裝置／產品整合仍待完成。目前實體開發機新增
> 硬體 DX12／Vulkan UI 短測、人工鍵盤／中文 UI／音訊與 100／150／175% DPI 證據；乾淨主機／完整裝置 gate 仍開放。

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
| 2D-M6 | P0 | 效能分析／壓力驗收 | CPU／GPU 幀時間、記憶體、繪製呼叫、配置尖峰與透明重疊 | 可重現 5,000 物件整合負載在指定 PC 達到預定 30 FPS／幀時間及資源預算；保留 10,000 物件壓力結果與 trace／設定 | 5,000 物件整合 gate NOT RUN；保留歷史 10k 壓力紀錄；M6 NOT ACCEPTED |
| 2D-M7 | P0 | Windows Runtime／Shipping 驗收 | 輸入（含任一實體遊戲控制器）、UI／文字、音效、非同步載入、序列化／存檔、Cooked 資產與獨立打包 | 乾淨 Windows 電腦無 Editor／開發依賴即可執行 Cooked 2D 驗證程式；鍵鼠／實體遊戲控制器、UI／文字呈現、音訊播放、非同步載入完成／失敗、重載、存檔／重開及釋放通過 | VM CPU／軟體 DX12／Zig，加上實體開發機 DX12／Vulkan UI、人工鍵盤／音訊及 100／150／175% DPI 切片通過；乾淨主機／完整裝置／產品 gate 仍開放；M7 NOT ACCEPTED |
| ✅ 2D-M8 | P1 | 2D 場景製作工具 | Sprite 放置、Pivot／排序編輯、碰撞視覺化與 Editor 正交預覽 | 編輯／預覽／Undo／Redo／儲存／重開保留場景；隔離 Play 不改製作 World | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M9 | P1 | Atlas／動畫工具 | 切圖／Atlas 預覽、打包診斷、逐格編輯、播放／事件預覽 | Cooked 輸出與 Runtime 呈現一致；驗證 Undo／Redo、無效資料及重新匯入 | ✅ Portable／Linux 能力驗收；證據見下方 |
| ✅ 2D-M10 | P1，條件式 | Tilemap | 圖層、繪製、碰撞資料、區塊載入／剔除 | 實作繪製、圖層／碰撞資料及區塊載入／剔除，通過 Undo／儲存／重開及區塊邊界渲染／查詢測試；決定延後仍維持未驗收 | ✅ Portable／Linux 能力驗收；證據見下方 |

### M7 實體遊戲控制器驗收

可使用任一實體遊戲控制器完成 M7 輸入驗收，例如 DUALSHOCK、DualSense、Xbox 控制器或
其他搖桿；不限定品牌或 XInput。須由遊戲實際讀取並驗證產品所需的按鈕、搖桿軸／死區、
UI 導覽／確認／取消、插拔重連、鍵鼠切換及失焦重設。保留型號、連線方式、輸入後端與操作結果；
單一裝置通過只證明該型號及連線方式，不表示所有控制器均已相容。

目前 Windows 原生 Adapter 使用 XInput；其他裝置的輸入路徑仍須實作或整合並實測。
作業系統辨識到 DUALSHOCK／其他 HID 裝置本身，不等於遊戲已收到輸入。
實作範圍見[輸入契約](../../Engine/Runtime/GameplayInput.md#native-gamepad-adapter-and-evidence)。

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

M6 最低規格驗收目標為 30 FPS。驗收前，先固定最低 CPU／GPU／RAM、Windows 版本、後端／Driver、解析度、建置與
Frame Pacing；固定可見／更新／碰撞物件數、Atlas／材質數、Sprite 像素覆蓋／重疊、動畫頻率、
VFX／UI 負載與載入變動量。產品最低硬體／負載預算仍為 **TBD**；下方各次同機 fixture 計畫
另行固定其負載與資源門檻。物件數本身不是效能保證。

使用可重現合成物件、動畫、移動查詢形狀、VFX 與 UI，不需遊戲機制。
涵蓋基線、大量物件、透明重疊及配置／載入尖峰；保留暖機／測試時間、平均／p95／p99／
最大幀時間、可取得時的獨立 CPU／GPU 區間、記憶體峰值與遺失樣本。
30 FPS 對應約 33.33 ms 幀預算，先定義允許尖峰再驗收；軟體光柵器只作正確性證據。

2026-10-18 依使用者要求將 M6 必要目標由 60 FPS 調整為 30 FPS。既有 60 FPS／16.67 ms
Screening 紀錄保留其原先預算與判定；此次目標調整本身不代表最低規格實體 PC 驗收通過。

### M6 目前開發機負載修訂（2026-10-19）

依使用者要求，目前開發機必要整合驗收負載改為 **5,000 個一般物件**。
10,000 個物件保留為額外壓力測試；僅在該負載發生的幀時間超標，不單獨阻擋 5,000 物件 gate。
原始結果與尚未解決的正確性、資源或提交／呈現問題均保留；降低目標不表示這些問題已修復。

| 一般物件數 | 用途 | 驗收角色 |
| --- | --- | --- |
| 1,000 | 功能短測 | 快速正確性檢查，不代表 M6 通過 |
| 5,000 | 日常整合效能與持續運行驗證 | 目前開發機 M6 必要負載 |
| 10,000 | 額外壓力與退步比較 | 保留每次結果，不作為必要效能負載 |

目前測試機為 AMD FX-6100、16 GiB RAM、GTX 960 2 GiB、Windows 10 x64 build 19045。
原生 DX12／Vulkan 分開驗證，使用 Monolithic Shipping、640x480 與 VSync；量測前固定精確
Driver、建置與執行檔身分。此為本機 fixture 目標，不宣稱所有正式遊戲的通用最低規格。

每幀仍須完整更新全部 5,000 個物件、計算 Transform 並加入查詢索引，保留移動查詢、動畫、
擷取／剔除／排序、物件池／Atlas／Codec 重載及載入配置工作。額外 256 個透明 VFX、兩個
Sprite Atlas 批次與 120 個實際 GameplayUi Quad／Command 不變；混合及稀疏階段均保留，
不得跳過更新或替換成純渲染測試。

完整／cycle 與各階段 p95／p99 仍須 ≤ 1000/30 ms，最大值 ≤ 2000/30 ms。計畫維持每後端
兩輪各四個暖機幀加 600 個量測幀、每 300 幀重載；再於 1,200 個暖機幀後測量至少 1,800 秒，
每 600 幀重載。沿用[固定計畫](../../Engine/TwoD/evidence/Windows-M6-IntegratedSoak-2026-10-18/plan.json)
的資源取樣峰值／成長門檻、正確性 oracle、完整逐幀／已完成 GPU 樣本核對及釋放檢查。

**5,000 物件整合驗收：NOT RUN。** 此次只修改規劃；目前 `--integrated` benchmark／runner
仍固定 10,000 個物件，須先提供明確的 5,000 物件支援才能執行。既有 5,000 物件單一案例
篩檢或 10,000 物件整合／長測，不能替代新的整合證據。所有歷史判定保留其原始負載與預算。
只有修訂後全部必要檢查取得通過證據，才將 M6 標記 `✅` 並關閉驗收任務；在此之前
M6 維持 NOT ACCEPTED，9/11（82%）、P0 6/8 不變。

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
實體 PC 30 FPS gate 通過。
M7 雲端切片：✅ Linux Monolithic Shipping 已用 Cooked Hash、Core 非同步載入、損毀拒絕
及不依賴 Editor／Development Engine Library 的獨立封裝驗證。第 10 節記錄 Windows VM
相依／CPU 部分驗證；原生 DX12 失敗、Vulkan 不可用。鍵鼠／手把、UI／文字、實際音效、
存檔／重開、顯示／DPI 及完整產品關閉仍未驗證。**M7 尚未通過（NOT ACCEPTED）**；
CI 的 Windows 編譯不等於這項驗收。

保留的 [Linux 驗收紀錄](../../Engine/TwoD/evidence/Linux-2026-10-14/acceptance.md) 包含
指令、平台範圍、擷取圖與負載 CSV。參閱 [模組 Contract](../../Engine/TwoD/README.md) 與
[工具](../../Tools/TwoD/README.md)。產生的報告位於 `build/<preset>/artifacts/`。
上述已驗收雲端基線已完成；其餘產品／裝置整合切片與獨立實體目標主機 Gate 仍由 Beads 追蹤。

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
主專案／Cooked 資產准入與主 host API 掛接見下段；整合 Game 預覽／Reload 仍待完成，里程碑數不變。
[證據](../../Tools/Build/evidence/EditorEDM3-TwoDPlayContext-Linux-2026-10-15.md)。


✅ `TwoDPlayAssets` 現可從明確 Runtime 資產世代凍結有限的 Cooked Atlas／Clip 相依閉包，
驗證相依與 Frame Region，並於替換、世代回收及 Store 銷毀後保留擁有式資料。
成功／失敗皆平衡暫時 Pin 與 Resident 引用。這是整合前置項，不代表專案／來源准入，
也未接上主 Editor Play／Game 預覽；相關整合仍待完成，里程碑計數不變。
[證據](../../Tools/Build/evidence/EditorEDM3-TwoDPlayAssets-Linux-2026-10-15.md)。

✅ 可選專案索引與同步／背景重新匯入現可保留不可變 Atlas／Clip 型別資料；初始內嵌
UUID、既有 Sidecar、容量上限與原子 Clip 相依更新，於無效、過期或取消的發布中保留
原模型及歷史。這是專案資產前置項；主 host API 掛接見下段，有界 Game 預覽見下段，無縫 Reload 仍待完成。
限定專案的 Cook／凍結准入由下述擷取擁有者提供，里程碑計數不變。[證據](../../Tools/Build/evidence/EditorEDM1-TypedTwoDContent-Linux-2026-10-16.md)。

✅ `ProjectTwoDPlayAssets` 可對目前專案選定的 Atlas／Clip 依賴閉包進行 cook 並凍結，
檢查專案 UUID／根目錄、Content 世代／修訂、寫入權限／復原狀態，以及精確來源／sidecar
與依賴的一致性。擁有式觀察值在啟動前重新驗證；專案銷毀後仍可保留凍結資料。
主 Editor host API 掛接見下段；有界 Game 預覽見下段；無縫 reload 仍未完成，ED／2D 里程碑計數不變。
參見[擷取契約](../../Engine/Editor/README.md#project-scoped-cooked-2d-play-capture)與
[驗證紀錄](../../Tools/Build/evidence/EditorEDM3-ProjectTwoDPlayAssets-Linux-2026-10-16.md)。

✅ 主 Editor 的 Play 啟動流程已擷取並重新驗證專案 Atlas／Clip Content，將選用的 2D gameplay
服務掛到實際複製的 World 與所選作用中場景。全新的 Sprite／碰撞中繼資料及有界 instance pool
保留到模組 Stop／Destroy 結束；一般子樹 despawn 會清理每個被刪除的 ID。32 輪 CPU clone
生命週期涵蓋完整模組替換、失敗載入清理及既有 3D host。有界 Game View 2D 繪製見下段；作者端 Sprite
持久化、Tilemap 掛接、無縫 hot reload 與實體驗收仍未完成，ED／2D 里程碑計數不變。
[契約](../../Apps/Editor/README.md#optional-2d-services-in-project-play)。
[證據](../../Tools/Build/evidence/EditorEDM3-MainPlayTwoDServices-Linux-2026-10-16.md)。

✅ 主 Game 面板現可明確選擇 3D／2D，從實際 Play clone、凍結的多 Atlas 與 pool 繪製有界
原生 2D 預覽。標準排序、4096 筆輸入准入、GUI 貼圖世代／預算、DPI／裁切、Scene 並存／
重疊處理與 Stop 保全已有驗證。新 session／Play 擁有者會重設為 3D；原生圖像滿載重送可
包含字型。實際 Linux Editor 通過 32 輪原生循環，涵蓋混色像素、
Pause／Step／輸入、作者端檔案及並存 3D 來源像素不變，Vulkan 驗證錯誤為零。作者端 Sprite
持久化、Tilemap 掛接、無縫 DLL reload 與實體驗收仍未完成；ED／2D 里程碑計數不變。
[契約](../../Apps/Editor/README.md#bounded-2d-game-view)與
[證據](../../Tools/Build/evidence/EditorEDM3-GameTwoDPreview-Linux-2026-10-16.md)。


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

## Windows Gameplay 音訊前置項（2026-10-16）

`GameplayAudio` 除 ALSA／可攜 Host Sink，現另提供同步 Windows 預設裝置 waveOut Adapter。
僅連結 Runtime 的 `NexoraTwoDAudioProbe` 提交低音量測試音、關閉／重新開啟裝置，並將輸出可用性
與人工可聽驗收分開回報；可用於不含 Editor 的 Monolithic Shipping。既有 Zig Host 明確指定的
`--audio` 路徑沿用此 Adapter；模組仍需自行註冊／播放 WAV，不新增遊戲規則。

✅ Linux 完整 Gate（167 通過、三項無關 Vulkan 略過）、實際 ALSA Null／缺少裝置生命週期、
Windows 交叉編譯／連結及獨立 Monolithic Shipping 檢查已通過。
[保留證據](../../Engine/TwoD/evidence/Device-Audio-Linux-2026-10-16/acceptance.md)。Windows 原生播放／可聽音訊、手把、完整產品與顯示／DPI Gate 仍待完成；
M6／M7 與 **9/11（82%）** 不變。參閱[音訊契約](../../Engine/Runtime/GameplayAudio.md)。

## Windows 手把路由前置項（2026-10-16）

原生 Gameplay Adapter 除 Linux Raw Joystick，現另動態輪詢 Windows XInput。每個 Slot 的完整
快照沿用既有動作服務，支援重綁、失焦／斷線釋放與未改變 Packet 的恢復。互動 Zig 2D 移動／暫停
也改用同一動作服務（WASD／Space 或預設左搖桿／A），保留非互動 Fixture 的確定性。
Windows 實體手把／裝置及完整 M7 驗收仍待完成；9/11（82%）不變。

✅ Linux Development 完整 Gate 為 311/311、無略過／失敗；Shipping 與 Windows 交叉編譯通過。
[保留證據](../../Engine/TwoD/evidence/Cooked-Services-Linux-2026-10-16/acceptance.md)。
參閱[輸入契約](../../Engine/Runtime/GameplayInput.md#native-gamepad-adapter-and-evidence)。

## Cooked 服務整合驗證前置項（2026-10-16）

既有真實 Zig 2D Host 新增明確的 `--validation-package` 模式：非同步雜湊／型別／身分／相依
檢查的 Cooked Atlas／Clip、WAV 解碼／輸出、自備可讀 ASCII Fixture 字形、UI SAVE／OPEN、
動作式原生鍵盤／手把路由、跨程序位置存檔／續開、不可變 Cooked／模組 Reload，以及依序停止
Callback／音訊／Storage／Surface。可重現封裝拒絕 Editor 相依；一般使用要求獨立 Monolithic
Shipping 執行檔，Development 測試副本另行標示。

Fixture 驗證引擎服務整合，不新增遊戲規則、正式美術、通用專案匯出或主 Editor 專案 Play。
Linux 軟體 Vulkan 像素／點擊／縮放與 Null 音訊，不代表 Windows 裝置、實體可聽音訊、DPI／
字型外觀或乾淨主機通過。M6／M7 維持未驗收，**9/11（82%）** 不變。

✅ 保留證據包含 311/311 Development、獨立 Shipping、900 幀原生 UI／Sprite、
滑鼠 Save／Open、實際文字像素、縮放及損毀／缺少資料拒絕。
[驗收](../../Engine/TwoD/evidence/Cooked-Services-Linux-2026-10-16/acceptance.md)；參閱[工具契約](../../Tools/TwoD/README.md#integrated-cooked-service-validation)。

✅ Windows 音訊後續改用四個擁有式 PCM Slot，只在重用待完成 Slot 時等待，不再等待剛提交的
區塊。實際可攜佇列驗證複製生命週期、端點、背壓／逾時、不覆寫待處理資料與重設，移除 Host
迴圈明確的逐塊等待回授；實體延遲／FPS／可聽驗收仍未通過。詳見上述證據。

✅ 最終佇列整合 Gate：Linux Development 313/313、無失敗／略過；Shipping／獨立原生封裝
及嚴格 Windows 交叉編譯通過。實體 Gate 與 82% 不變。

✅ Cooked 服務原生輸入確認修正：驗收 driver 現在會先確認 CRC 正確的 SAVE 發布內容已包含
鍵盤造成的正位移，再執行 OPEN 並比對還原位置。連續五次原生驗收通過；刻意讓第一次
D press 未送達時可恢復，所有按鍵皆未送達時仍會失敗。本機啟用 Zig 的完整 gate 為
190/190、無略過。此為驗收 harness 修正，M6／M7 與 9/11（82%）不變。
CI 失敗、驗證範圍與受控實驗見上述 Cooked 服務驗收證據連結。

## 通用 Zig 專案 Player 與 Windows 交付工具（2026-10-16）

✅ `NexoraTwoDPlayer` 與原生 ProjectPlayer 的明確 2D 模式，可載入另外編譯的可信 V3 模組及
專案自己的 Atlas／Clip、WAV、字型圖集，無須固定測試場景或精靈數量。全新目錄匯出、MSVC CRT
複製、啟動器與 hash 清單支援 Shipping 交付。Windows 字型烘焙工具檢查指定真實字型與有界
UTF-8 BMP 語料；缺字會失敗。使用者目錄中的永久存檔仍由模組定義 schema，與熱更新遷移分開。

✅ 主 Editor 使用凍結專案素材，在所選 2D Game View 合成場景／物件池精靈與 gameplay UI，HUD
提交不重複繪製精靈。通過焦點／
擷取規則的動作輸入、原生音訊控制／重試與專案限定存檔保留到 Stop／Destroy；原生紋理由 host
釋放。CPU 與 Linux Vulkan 像素驗證 32 輪 Play；真正 Zig Shipping 匯出及獨立模組的重啟／損壞
測試驗證通用 Player 路徑。參見[驗收紀錄](../../Tools/Build/evidence/Zig-Project-Player-Linux-2026-10-16.md)與
[Windows 實機清單](../../Tools/TwoD/WindowsAcceptance.md)。

正式美術／字型／玩法，以及 Windows 實際聽音、實體遊戲控制器、焦點／DPI、乾淨電腦啟動與最低
硬體驗收，仍需真正專案與目標機。作者端 Sprite 持久化、Editor Tilemap 掛接、無縫 reload
仍未完成。M6／M7 與 **9/11（82%）** 不變；hosted MSVC CI 與軟體 Vulkan 不算實體產品驗收。

## 本機 MSVC Player 驗收切片（2026-10-17）

✅ PR #620 Player 與獨立 Zig 模組已用原生 x64 MSVC Monolithic Shipping 建置。
獨立匯出／Unicode／存檔重啟／損壞拒絕、Windows GDI 字型、實體主機三幀 DX12 面板提交與
Run.cmd 均通過。waveOut 提交 48,001 frames 並重開成功；人工聽音仍為 NOT VERIFIED。
文件已補正 Visual Studio multi-config 輸出路徑。
[驗收與原始證據](../../Engine/TwoD/evidence/Windows-MSVC-PR620-2026-10-17/acceptance.md)
保留範圍、inventory 與指令。VM 個別原生案例與最終雜湊已有觀測，但後續 Guest Control／RPC
失敗令 VM 中止，完整 VM 生命週期未驗收。實體輸入／手把、音訊裝置恢復、中文外觀／DPI、
乾淨實體 PC 與最低硬體效能仍待完成；M6／M7 維持 NOT ACCEPTED，**9/11（82%）、P0 6/8** 不變。

## Windows VM 傳輸恢復切片（2026-10-17）

✅ 驗收工具已對每個 VM 序列化操作，保留 VBoxManage 原始位元組串流，拒絕 NUL／無效 UTF-8
inventory，對全新 staging 做 flush 與雜湊檢查，並記錄真正程序退出碼。原生案例等待 desktop
level 3，再透過 InteractiveToken 工作執行；關閉前確認 fixture 像素已呈現。RAM 回收等待不降低
預留額度，清理失敗回報 unknown，不沿用前輪 poweroff。本機 12 項 Python 回歸與 2/2 CTest 通過。

✅ 完整目前狀態的恢復複本已通過**兩次冷開機、14/14 案例、存檔 visits 0–4、每輪 120 個
Zig 原生幀、零殘留 Player 與兩次正常 poweroff**。
[原始報告與範圍](../../Engine/TwoD/evidence/Windows-VM-Recovery-2026-10-17/acceptance.md)
保留舊 806-NUL inventory 與失敗嘗試。該輪結束時，原 VM 第二次開機的 Windows 登入停滯仍未解決，RAM／
磁碟維持 saved；只移除新複本重複的 RAM 狀態，沒有還原舊 snapshot 或刪除原存檔。此為凍結
`63bb5f7a` 的恢復 VM／軟體正確性切片，不是乾淨／實體 PC 驗收。實體手把／音訊／DPI、
整合產品與最低硬體門檻仍開放；**M6／M7 NOT ACCEPTED、9/11（82%）、P0 6/8** 不變。

✅ 原 VM 恢復後續（`nexora-j4q.7.7`）：使用者重開機恢復桌面後，原 UUID 已通過
**兩次冷開機、14/14 案例、visits 0–4、每輪 120 個 Zig 原生幀、零殘留 Player 與兩次正常關機**。
此為有界的恢復驗證，尚未證明先前「歡迎」停滯的根因。保留檢查、歷史 RAM 狀態限制與凍結
二進位身分見[原 VM 紀錄](../../Engine/TwoD/evidence/Windows-Original-VM-2026-10-17/acceptance.md)。
第三次唯讀冷開機確認 16 個舊存檔雜湊、兩個 snapshot UUID／目前選擇與舊 snapshot RAM 不變，並正常關機。
完整 M6／M7 與 **9/11（82%）、P0 6/8** 維持不變。

## Windows 實體開發機 M6／M7 切片（2026-10-17）

✅ 目前 Shipping Player 透過獨立公開 C ABI 中文主機 fixture，在 GTX 960 以明確指定的
DX12／Vulkan 呈現。人工確認實際聽到 probe、Music／SFX 循環、三個 bus 音量、暫停／繼續／
停止、外接喇叭接回、鍵盤／重綁、SAVE／OPEN、失焦／最小化／縮放，以及 100／150／175%
中文顯示。三輪 DX12 互動生命週期皆正常退出；重測記錄 87 次滾輪事件與四次 UI 取消。
位置與重綁狀態跨獨立 DX12／Vulkan 程序保留。原始計數與人工觀察分開保存在
[驗收紀錄](../../Engine/TwoD/evidence/Windows-Host-M6M7-2026-10-17/acceptance.md)。

本機有 PS4 USB HID，但該輪使用的 XInput 後端未讀到其輸入，實體控制器 gate 仍未驗收。
後續可使用 DUALSHOCK 或其他搖桿依上方條件實測，不要求另備 XInput 裝置。標準縮放選單最高 175%，
200% 與跨螢幕為 NOT RUN。喇叭連線恢復未造成 waveOut 失效／重開，不能證明 USB 端點失效／
預設裝置切換恢復。UI-only fixture 與 Sprite 壓力負載、正式遊戲分開；乾淨主機、最低規格／
整合負載／長時間運行、Editor 生命週期與完整裝置 gate 仍開放。
**M6／M7 NOT ACCEPTED、9/11（82%）、P0 6/8** 維持不變。

✅ 真實 Zig Cooked 服務 fixture 另在每個後端通過兩個 120 幀原生程序：各有 34 個可見 Sprite、一次 Cooked 重載、96,000 音訊幀、存檔／續存與平衡釋放。自動 fixture 移動與音訊提交不額外代表人工手把／聽音驗收。

本機兩個硬體後端皆完成六種各 600 幀篩檢，但 12 列皆超出預先宣告的 p95／p99／最大值預算。10k 混合負載 frame p95：DX12 31.97 ms、Vulkan 30.99 ms；10k 稀疏：40.33／49.06 ms。外部程序峰值 working set 為 90.9／148.0 MB，不代表 allocator／VRAM 或最低規格驗收。功能 gate 通過與 M6 效能未驗收分開記錄。

上述負載已使用 GPU Instancing。另一組 60 幀 DX12 診斷指出 CPU 資料提取（10k 混合 p95 16.48 ms）與空間索引重建（10k 稀疏 p95 27.16 ms）的成本；純渲染基準與最佳化仍待完成。主機 fixture 另揭露 headless Linux 執行收到系統手把輸入，現將原生輪詢限於 native 模式，並加入存檔／續存零輸入斷言。Linux headless fixture 音訊於 sanitizer 下使用明確的 ALSA null 設定，與實體輸出分開。失敗證據與診斷限制見同一份驗收紀錄。

## Windows M6 30 FPS 本機重驗（2026-10-18）

✅ 依更新後的 30 FPS 目標，從 `9e3adfdd` 重建 x64 MSVC Monolithic Shipping；
FX-6100／GTX 960 實體主機以 DX12／Vulkan 各完成兩輪、每輪六種各 600 幀混合負載。
四個程序正常退出，硬體識別與原生生命週期檢查通過。量測前固定 p95／p99 ≤ 33.3333 ms、
最大值 ≤ 66.6667 ms；10k 混合與 10k 稀疏在兩個後端的兩輪皆超標。
完整 24 列、原始 CPU／GPU／幀時間、程序記憶體採樣、預先計畫與二進位雜湊見
[本次驗收證據](../../Engine/TwoD/evidence/Windows-M6-30FPS-2026-10-18/acceptance.md)。

此為目前開發機的重驗，未修改引擎或降低原混合負載。舊 60 FPS Screening 保留原判定。
最低產品規格、整合 UI／VFX、配置／VRAM、持續運行與逐幀 trace 仍待完成；
**M6 NOT ACCEPTED、9/11（82%）、P0 6/8** 不變，M7 gate 未於本輪執行。

## 共用 2D 路徑最佳化與同機重驗（2026-10-18）

✅ 共用空間索引改用連續格子參照與可重用暫存快照；Sprite 擷取使用連續 ID 驗證及輕量排序參照，
已排序輸入省下重新排序。逐幀完整重建、查詢／掃掠、透明排序、矩陣快照、擁有式幾何與無效輸入
拒絕保留；公開函式及 C／Zig ABI 不變，私有 C++ 佈局變更需重建消費端。

MSVC Development 全建置、Shipping 基準／資產工具及 20/20 本機相關回歸通過。
FX-6100／GTX 960 保持原六種混合負載與 10k 物件案例，DX12 兩輪各 6/6、6/6，
Vulkan 兩輪各 4/6、5/6 符合原 30 FPS／尖峰預算；四個程序退出碼皆為 0。
24 列的可見／剔除數、批次、覆蓋、查詢命中與採樣數和最佳化前一致，沒有降低負載或放寬門檻。
[原始證據與前後比較](../../Engine/TwoD/evidence/Windows-M6-SharedPath-2026-10-18/acceptance.md)
保留來源 patch／雜湊、CPU／GPU／幀時間與記憶體。完整 Linux／原生像素 gate 另由最終 PR CI 驗證。

這是共用引擎改善與目前開發機篩檢；未完成最低產品規格、整合 UI／VFX、配置／VRAM、
逐幀 trace 與持續運行驗收。**M6 NOT ACCEPTED、9/11（82%）、P0 6/8** 不變。

## Vulkan FIFO 節奏與同機重驗（2026-10-18）

✅ 共用 Vulkan surface 現於 FIFO 影像真正可用後才開始 CPU frame work，使用一個可重用 fence
及每張影像各自持有的呈現 semaphore。Immediate 仍採非同步取得；兩／三個 upload slot 依
descriptor 配置，與影像同步資源分開。Native regression 涵蓋首次 SUBOPTIMAL 恢復、部分配置
失敗清理、零尺寸、資源 drain 及放棄中的 frame。預設影像數、VSync、解析度、benchmark 原始碼
及全部 10,000 物件的更新／查詢／擷取／重載工作均未減少。

FX-6100／GTX 960 實機篩檢通過 **23/24**：Vulkan 兩輪皆為 6/6；每組四個暖身幀
加 600 個量測幀，640x480，沿用 p95／p99 <= 1000/30 ms、最大值 <= 2000/30 ms 門檻。
全部資料列的可見／剔除數、batch、覆蓋率、查詢命中及 GPU sample 與前輪相同。
本機 MSVC Development 完整建置與 Shipping 建置通過；相關測試 31/31，無略過。
[證據](../../Engine/TwoD/evidence/Windows-M6-VulkanPacing-2026-10-18/acceptance.md) 保留完整結果、執行檔／來源雜湊、process memory，以及獨立逐幀／render-only 診斷。
完整 Linux／native pixel 與其他 hosted gate 由最終 PR CI 記錄。

DX12 為 6/6、5/6。第二輪 10k sparse 的 p99 為 21.55 ms，但最大值 72.36 ms，超過原訂
66.67 ms 尖峰上限。此失敗完整保留並持續追蹤；Vulkan 原三組 p99 超標已由本輪篩檢消除。
不宣稱全部 backend 已通過驗收。

本次驗收範圍為目前開發機篩檢；產品最低規格、整合 UI／VFX、allocator／VRAM 及長時間驗收
仍待完成。**M6 NOT ACCEPTED；9/11（82%）、P0 6/8 不變。**

## DX12 VSync admission 與同幀篩檢（2026-10-18）

✅ 共用 DX12 VSync 現以一個 DXGI frame-latency handle 控制 CPU frame work 的開始，
佇列上限與既有二／三個 frame slot 一致。獨立 GPU fence 繼續保護 upload、allocator 與
retirement。Immediate、影像數、縮放／恢復、零尺寸與釋放均有涵蓋。單一排隊幀試驗使
10k p99 退步，因此不採用；失敗的兩輪仍保留於證據。最終策略保留 CPU／GPU 平行處理。

✅ Benchmark 可選 `--frame-trace` 緩衝每幀 CPU／完整區間與八個連續分段，明確標記
暖機幀，幀內不寫檔。額外時鐘成本納入量測；原動畫、移動、完整 query rebuild／sweep、
extraction／排序、instanced draw 與中途 pool／Atlas reload 工作全部保留。聚合 CSV schema
及區間邊界不變。Portable regression 由同一批幀重算 p95／p99／max，並檢查輸出失敗。

實體 FX-6100／GTX 960 篩檢通過 **24/24**：DX12 與 Vulkan 兩輪均各 6/6，維持原
640x480 VSync、600 個量測幀＋四個暖機幀、p95／p99 <= 1000/30 ms、最大值 <= 2000/30 ms。
物件計數、覆蓋率、query、GPU 樣本與原生裝置身分全部符合前次篩檢。本機 MSVC Development
完整建置與正式 Shipping 建置通過；相關 CTest 33/33、無 skip。
[證據](../../Engine/TwoD/evidence/Windows-M6-DX12Pacing-2026-10-18/acceptance.md) 保留所有正式幀、未採用的試驗、精確來源／執行檔身分與原失敗篩檢連結。
最終 head 的完整託管 Linux／Shipping 及其他平台 gate 記錄於
[PR #644](https://github.com/jimlee1972/Nexora/pull/644)。

前次 72.3556 ms DX12 事件未在固定診斷輪次重現，該次事件的唯一同幀原因仍未追出；
目前 Windows 原則阻擋 kernel CPU trace。本次通過不證明該歷史原因已排除。
產品最低規格、整合 UI／VFX、allocator／VRAM 與長時間驗收仍開放。
**M6 NOT ACCEPTED；9/11（82%）、P0 6/8 不變。**

## 整合 10k UI／VFX 與實體 30 分鐘長測（2026-10-18）

✅ 新增可選 benchmark 模式與固定計畫 runner，保留原六組 `Measure` 本體、全部 10,000 個 CPU
更新／查詢形狀與原 30 FPS 門檻。同一原生 pass 新增 256 個動畫透明 VFX、兩組 Sprite Atlas／
batch，以及實際 GameplayUi 面板／按鈕／文字（120 個 UI 四邊形／command）。一般／稀疏布局
交替時，反覆執行完整物件池／不可變 Atlas 回收、codec 重載及觸頁後釋放的 8 MiB 範圍配置。
此可重現 fixture 不推論即時模擬、正式美術、非同步磁碟 I/O 或實體輸入驗收。

✅ 兩個硬體後端皆在 1,200 幀暖機後完成 1,800 秒測量，沒有遺失幀／已完成 GPU 樣本，正常釋放。
正式流程保留 210,785 個原始幀與精確來源／執行檔身分。原負載的非計時輸出與前次篩檢一致；
本機 Development／Shipping 建置及相關 34/34 測試通過，無略過。

嚴格效能仍為 **NOT ACCEPTED**：原篩檢 23/24（DX12 11/12、Vulkan 12/12）、整合短測 3/4、
長測 1/2。DX12 長測共 95,701 個測量幀，cycle p99／最大值 28.4662／62.8658 ms。Vulkan 共
95,772 幀，p99 27.7955 ms，但最大值 **134.9232 ms** 超出原 66.6667 ms 門檻。DX12 原混合
10k 第一輪 p99 為 39.3942 ms；Vulkan 整合第二輪 p95／p99 為 33.5469／49.2560 ms。
保留全部失敗列、測量區間冷幀與尖峰。

Vulkan 第 24,415 幀 CPU 14.9771 ms、Draw 5.6583 ms、EndFrame **113.6250 ms**，當幀沒有
重載或資源取樣。對應提交 24,416 的 GPU 區間為 **0.448736 ms**，於兩幀後觀測到。此資料縮小
到提交／呈現邊界，尚不能識別個別 API 或 OS／Driver 原因。`nexora-j4q.2.6` 追蹤原生 API／
執行緒週期／排程關聯；歷史 DX12 72.3556 ms 原因仍由 `nexora-j4q.2.4` 獨立追查。

✅ 六組整合資源篩檢皆符合固定取樣峰值／成長門檻。兩次長測皆維持兩次原生 buffer 配置、
161 次平衡的範圍配置／釋放，且沒有拒絕貼圖。DX12 private memory 前後五分鐘中位數成長
0.3984 MiB，Vulkan 為 -0.0078 MiB；兩後端 local GPU usage 中位數成長皆為零。幀後 RSS／
commit／WDDM 預算用量及明確載入配置，不代表全引擎 allocator 統計或實體 VRAM residency。

[完整計畫、結果、失敗幀與壓縮原始證據](../../Engine/TwoD/evidence/Windows-M6-IntegratedSoak-2026-10-18/acceptance.md)。
[工具契約](../../Tools/TwoD/README.md#integrated-10k-load-and-sustained-native-validation)。
本次補充前一輪成功的 24/24 紀錄，保留該次歷史判定。**M6 NOT ACCEPTED；9/11（82%）、
P0 6/8 不變。** 產品最低規格與完整 allocator／正式負載／裝置／乾淨主機 gate 仍開放。
依使用者明確要求，驗收工具與保留證據已於 [PR #646](https://github.com/jimlee1972/Nexora/pull/646) 合併；
此舉不代表 M6 通過，也不改寫任何失敗結果。上方修訂後的 5,000 物件 gate 仍為 NOT RUN。
