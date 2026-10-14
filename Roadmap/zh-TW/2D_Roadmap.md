# Nexora 2D 引擎 Roadmap

> 版本：v1.1 | 狀態：雲端能力交付已驗收，PC 主機 gate 待完成 | 更新：2026-10-14

> **能力驗收進度：82%**（9/11，四捨五入；P0 6/8）。M0～M5 與 M8～M10 已有
> Portable／Linux 功能證據。M6 最低規格實體 PC 60 FPS 與 M7 乾淨 Windows
> Runtime／Shipping 仍為 **NOT RUN**；這不是完整 Windows PC 產品驗收。

## 1. 範圍與基線

在 V1 基礎上補齊大量 Sprite、俯視視角、單機 Windows PC 應用需要的引擎能力。
正交相機、Unlit 材質、實例化、VFX Sprite、UI／文字、輸入、音效、非同步資產、序列化與
打包已有規劃或切片。可選 TwoD 模組已提供場景 Sprite 擷取與輕量 2D 查詢，沿用 World、
資產、Core VFS、原生 Presentation 與 EditorImGuiHost。實體 PC 效能及乾淨 Windows
整合維持獨立；不替換 Jolt，也不加入 Gameplay。

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
| 2D-M6 | P0 | 效能分析／壓力驗收 | CPU／GPU 幀時間、記憶體、繪製呼叫、配置尖峰與透明重疊 | 可重現混合引擎負載在指定最低 PC 配備達到預定 60 FPS／幀時間預算；保留 trace／設定 | 雲端實作已驗證；實體 Windows gate 為 NOT RUN |
| 2D-M7 | P0 | Windows Runtime／Shipping 驗收 | 輸入、UI／文字、音效、非同步載入、序列化／存檔、Cooked 資產與獨立打包 | 乾淨 Windows 電腦無 Editor／開發依賴即可執行 Cooked 2D 驗證程式；輸入、UI／文字呈現、音訊播放、非同步載入完成／失敗、重載、存檔／重開及釋放通過 | 雲端實作已驗證；實體 Windows gate 為 NOT RUN |
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
  Editor 條件式依賴 TwoD。公共 C++ 新增需重建 Consumer；C／Zig ABI 不變。

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
及不依賴 Editor／Development Engine Library 的獨立封裝驗證。乾淨 Windows 的鍵鼠／手把、
UI／文字、實際音效、載入失敗／重載／存檔、原生 DX12／Vulkan、顯示／DPI 及關閉仍為
**NOT RUN**；CI 的 Windows 編譯不等於這項驗收。

保留的 [Linux 驗收紀錄](../../Engine/TwoD/evidence/Linux-2026-10-14/acceptance.md) 包含
指令、平台範圍、擷取圖與負載 CSV。參閱 [模組 Contract](../../Engine/TwoD/README.md) 與
[工具](../../Tools/TwoD/README.md)。產生的報告位於 `build/<preset>/artifacts/`。
雲端可執行工作已完成；實體目標主機 follow-up 保留於 Beads。

- [Roadmap 索引](../ReadMe.md)
- [V1 完整規劃書](跨平台3D_Engine_V1_完整規劃書_v1_2.md)
- [V1 AI 施工規劃](跨平台3D_Engine_V1_AI施工技術與系統規劃_v1_2.md)
- [圖形化 Editor Roadmap](Editor_Roadmap.md)
- [聚焦 Roadmap AI 施工規劃](聚焦_Roadmap_AI施工技術與系統規劃.md)
- [Renderer](../../Engine/Renderer/README.md)、[Runtime](../../Engine/Runtime/README.md)
  與 [Editor Contract](../../Engine/Editor/README.md)
- [英文版](../en/2D_Roadmap.md)
