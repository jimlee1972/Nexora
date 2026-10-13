# Nexora 2D 引擎 Roadmap

> 版本：v1.0｜狀態：規劃中，僅涵蓋引擎能力｜更新：2026-10-13

> **進度：0% 驗收**（11 個能力里程碑已驗收 0 個，依里程碑數等權計算）。
> 尚未記錄專屬 2D 驗收證據。既有基礎交付不計為 2D 里程碑完成；這不代表引擎沒有可重用支援。

## 1. 範圍與基線

在 V1 基礎上補齊大量 Sprite、俯視視角、單機 Windows PC 應用需要的引擎能力。
正交相機、Unlit 材質、實例化、VFX Sprite、UI／文字、輸入、音效、非同步資產、序列化與
打包已有規劃或切片，完整 2D 整合仍待驗證。VFX／UI Sprite 不等於場景 Sprite Renderer；
Jolt 3D 不等於原生 2D 碰撞支援。尚未確認的能力先盤點再補做；不必等待完整 V2／V3。

範圍包含可重用的渲染、資產、查詢、Runtime 生命週期、效能分析、打包與製作工具。
Gameplay 規則、戰鬥、刷怪邏輯、成長與遊戲內容不列入本 Roadmap。

## 2. 能力里程碑

P0 建立可用 Runtime 與證據；P1 改善製作效率，Tilemap 為條件式。
每項完整驗收通過前均維持未驗收，通過後才使用 `✅`。

| ID | 優先順序 | 能力 | 必要內容 | 驗收條件 | 目前判斷 |
| --- | --- | --- | --- | --- | --- |
| 2D-M0 | P0 | 場景 Sprite Renderer | 圖集區域、Pivot、翻轉、顏色／透明度、排序層與穩定 Y 軸排序；沿用 Transform／正交相機 | 原生畫面驗證 UV、Pivot、翻轉、透明度及重疊排序；儲存／重開保留欄位 | 尚未確認完整專用規格 |
| 2D-M1 | P0 | Sprite 逐格動畫 | 逐格時間／播放速度、循環、切換、動畫事件，與骨架動畫分開 | 幀／事件紀錄驗證循環邊界、切換、暫停／恢復與大時間步進，事件不重複 | 尚未確認專用 Sprite 動畫路徑 |
| 2D-M2 | P0 | 2D 資產管線 | 切圖、可重現 Atlas 打包、匯入／取樣設定、透明邊緣處理與穩定引用 | 匯入／Cook／重新匯入保留引用；原生取樣無邊緣滲色；無效輸入保留已發布資產 | 已有通用基礎；2D 流程待盤點 |
| 2D-M3 | P0 | 輕量 2D 碰撞／查詢 | 圓形／AABB 重疊、分組、範圍查詢、空間索引與高速移動掃掠檢查 | 以獨立暴力比對驗證索引結果，涵蓋邊界、分組、高速移動及插入／移除 | Jolt 3D 不能作為此能力已有的證明 |
| 2D-M4 | P0 | 大量物件生命週期／更新 | 批量生成／回收、世代識別、配置控制、資料導向更新與固定時間步進 | 反覆操作拒絕過期 handle、安全釋放，記錄負載下配置與幀時間尖峰 | 架構已有方向；容量待量測 |
| 2D-M5 | P0 | 大量 Sprite 渲染 | 經由既有 RHI 整合圖集批次／實例化、可見性剔除、透明排序與 overdraw 控制 | 原生畫面保留跨批次排序；稀疏／重疊負載記錄繪製呼叫數與 CPU／GPU 時間 | 已有通用渲染；2D 路徑待驗證 |
| 2D-M6 | P0 | 效能分析／壓力驗收 | CPU／GPU 幀時間、記憶體、繪製呼叫、配置尖峰與透明重疊 | 可重現混合引擎負載在指定最低 PC 配備達到預定 60 FPS／幀時間預算；保留 trace／設定 | 實際容量必須量測 |
| 2D-M7 | P0 | Windows Runtime／Shipping 驗收 | 輸入、UI／文字、音效、非同步載入、序列化／存檔、Cooked 資產與獨立打包 | 乾淨 Windows 電腦無 Editor／開發依賴即可執行 Cooked 2D 驗證程式；輸入、重載、存檔／重開及釋放通過 | 既有子系統切片仍需完整 2D 驗收 |
| 2D-M8 | P1 | 2D 場景製作工具 | Sprite 放置、Pivot／排序編輯、碰撞視覺化與 Editor 正交預覽 | 編輯／預覽／Undo／Redo／儲存／重開保留場景；隔離 Play 不改製作 World | 尚未確認完整圖形化 2D 流程 |
| 2D-M9 | P1 | Atlas／動畫工具 | 切圖／Atlas 預覽、打包診斷、逐格編輯、播放／事件預覽 | Cooked 輸出與 Runtime 呈現一致；驗證 Undo／Redo、無效資料及重新匯入 | 尚未確認專用工具 |
| 2D-M10 | P1，條件式 | Tilemap | 圖層、繪製、碰撞資料、區塊載入／剔除 | 採用時通過繪製／Undo／儲存／重開與區塊邊界渲染／查詢測試；未採用則記錄流程決策並延後 | 地圖製作採 Tile 時才需要 |

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
  `Config/Modules/modules.json` 並提供 Feature Option。本次文件變更不新增模組或改既有 Runtime Contract。

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

11 個里程碑只在本 Roadmap 等權計算，P0 可另報 8/8；延後 Tilemap 保留在分母直到經審查修訂範圍。
完成規劃不改 V1 基礎、Editor 或聚焦 AI 規劃百分比。細部驗收保留在模組／工具文件或證據目錄；
Beads 仍是持久任務追蹤來源。

延後：完整 2D 剛體、多人網路、額外平台，以及超出所列 P1 流程的進階製作功能。

- [Roadmap 索引](../ReadMe.md)
- [V1 完整規劃書](跨平台3D_Engine_V1_完整規劃書_v1_2.md)
- [V1 AI 施工規劃](跨平台3D_Engine_V1_AI施工技術與系統規劃_v1_2.md)
- [圖形化 Editor Roadmap](Editor_Roadmap.md)
- [聚焦 Roadmap AI 施工規劃](聚焦_Roadmap_AI施工技術與系統規劃.md)
- [Renderer](../../Engine/Renderer/README.md)、[Runtime](../../Engine/Runtime/README.md)
  與 [Editor Contract](../../Engine/Editor/README.md)
- [英文版](../en/2D_Roadmap.md)
