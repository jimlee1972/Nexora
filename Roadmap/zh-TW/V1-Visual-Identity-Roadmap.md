# Nexora V1 視覺特色 Showcase Roadmap

> 版本：v0.2
>
> 日期：2026-10-04
>
> 狀態：VIS-M0 工程灰盒施工中；VIS-M0～VIS-M6 尚未驗收（0/7）。
>
> 主題：風格化遺跡庭院。
>
> 對應英文版：[V1 Visual Identity Showcase Roadmap](../en/V1-Visual-Identity-Roadmap.md)。

## 1. 目標

完成一座可探索的風格化遺跡庭院，用材質、光影與環境動態建立 Nexora 的視覺特色。觀眾關掉所有文字與計數器後，仍能從一張截圖或十秒影片看見引擎的視覺表現。

本計畫接續 [V1 可視化展示 Demo 長期規劃](V1-Visual-Showcase-Long-Term-Plan.md)，沿用 NexoraShowcase、資產流程、導覽與封裝基礎。VIS 里程碑獨立追蹤，不改寫既有 V1 portable contract 完成率，也不代表 V1 最終平台驗收已完成。

### 三個核心賣點

| 賣點 | 畫面表現 | 觀眾如何觀察 |
| --- | --- | --- |
| 材質有質感 | 石材、金屬、陶瓷、發光符文 | 同一光照下比較材質；繞行鏡頭觀察反射、粗糙度與法線細節 |
| 光影有風格 | 暖色陽光、冷色陰影、可控制的明暗層次 | 全景觀看空間層次；切換基礎／風格化光照比較 |
| 環境有生命 | 植被隨風擺動、符文裝置啟動、粒子流動 | 播放、暫停與重播動態段落 |

## 2. 現有基礎與整合缺口

以下是本次規劃依據，不是新增 VIS 里程碑的完成證據。

- 原生展示已有 indexed geometry、方向光、深度測試、基本貼圖、hardware instancing 與 Offscreen → Main → UI → Present RenderGraph。
- 共享 shader library 已有 PBR／IBL、Stylized、Anime、植被風動／透光、陰影與後處理 helpers，以及代表性 shader 的編譯契約。
- Showcase 已有程序化地形、植被幾何、動畫混合、CPU deformation 與粒子位置展示。
- 現有原生展示路徑仍以基本材質為主；共享 helpers 尚需完成實際場景整合與畫面驗收。多材質、normal／ORM、IBL、HDR 與真實陰影是本計畫的主要工程缺口。
- 現有場景批次仍共用 lighting、texture 與 base material；多材質整合需要設計新的場景／材質綁定邊界。
- GTX 960 的既有幀時間是簡單場景觀察值，不能當作本計畫的效能承諾。

依據：[Shader library](../../Shaders/README.md)、[Renderer](../../Engine/Renderer/README.md)、[Presentation](../../Engine/Presentation/README.md)、[Showcase](../../Apps/Showcase/README.md)。

## 3. 場景與鏡頭

採用一座有中央雕像／符文裝置的庭院，周圍配置石牆、金屬構件、陶瓷飾物與植被。色盤以暖陽、冷色陰影與單一符文強調色為主。

| 鏡頭 | 內容 | 要展示的能力 |
| --- | --- | --- |
| 材質近景 | 石雕、金屬、陶瓷與符文表面 | 法線、粗糙度、反射、emission 與細節密度 |
| 庭院全景 | 斜射陽光、建築投影、植被與前中後景 | 光照風格、真實陰影、空間層次與構圖 |
| 動態收尾 | 鏡頭靠近裝置，植被擺動、符文啟動、粒子流動 | 環境動態、效果節奏與可重播性 |

資產在 VIS-M0 記錄作者、來源、授權、可再散布條件與製作成本；工程模型與最終美術資產分別追蹤。

### 已確認的美術方向

✅ 使用者於 2026-10-04 在本對話確認下列生成預覽圖。後續選材、構圖、材質與光照以此圖為美術方向參考。

![已確認的風格化遺跡庭院概念圖；AI 生成，非引擎截圖](../art/V1-Visual-Identity-Concept.png)

- **氣氛與色盤：** 平靜、神秘的遺跡聖所；傍晚金色陽光、暖赭色砂岩、冷藍陰影、低飽和綠色植被，以青綠符文作為單一強調色。
- **視覺主體：** 帶有舊青銅構件的風化石環，環繞懸浮的多面水晶，立於雕刻基座上。中央符文裝置建立場景辨識度。
- **建築與構圖：** 模組化破損拱門、厚實石柱、磨損地磚與陶器；前景清楚，中景集中於裝置，背景以遺跡建立深度。
- **材質表現：** 風格化 PBR、明確輪廓、石材表面細節、粗糙度差異、適量金屬高光與霧面陶瓷。免費素材須調整成此風格，不以素材包改變既定方向。
- **動態與效果：** 輕柔植被風動、背光透光、符文啟動、少量飄浮粒子與克制的發光 bloom。
- **V1 範圍：** 先做一座小型庭院，降低建築與植被密度。圖中的反光水窪、遠景瀑布、大量背景與鏡頭模糊不列為初版驗收要求；水面與反射保留為後續擴充。

預覽使用圖片生成工具製作，是視覺目標，並非 Nexora 渲染畫面、正式模型、貼圖或效能結果。原始 PNG 完整保存，SHA-256：`ad4cd12a0331e9c30b4a2e54d2773ee0718a63bdf2791a65743f38e1bd7a9b3b`。

使用者另要求先從網路尋找免費模型與貼圖。優先使用可免費下載且允許再散布的素材，以 CC0 為首選，不採購付費素材包。具體候選與來源查核見雙語 [免費素材清單](../art/Free-Asset-Sourcing.md)。確認美術方向不代表 VIS-M0 已完成：資產盤點、灰盒、流程載入、固定鏡頭與基線截圖仍待交付。

## 4. 里程碑

VIS-M0 施工中，其餘里程碑為「規劃中」。通過各自驗收且保留證據後，才標記完成。

| ID | 工作內容 | 可見成果 | 驗收條件 |
| --- | --- | --- | --- |
| VIS-M0 | 視覺定稿、資產盤點、灰盒、固定鏡頭與初始效能取樣 | 完整構圖與展示路線 | 三個鏡頭成立；代表性資產能經 Import → Cook → Bundle → Runtime 載入；保存基線截圖 |
| VIS-M1 | 共享 PBR shader 整合、多材質、normal／ORM／emission、切線資料、IBL、線性色彩與 HDR 輸出 | 材質近景 | 相同光照下材質差異清楚；鏡頭繞行時反射與法線正確；缺圖 fallback 可用；無重複 gamma 轉換 |
| VIS-M2 | 方向光 shadow map、PCF、偏移控制、風格化明暗與陰影色調 | 光影全景 | 移動物件投影更新；固定路線無明顯閃爍、陰影痤瘡或懸浮；保存光照比較畫面 |
| VIS-M3 | 中央遺跡、地面與周邊內容；曝光、tone mapping、色彩與適量 bloom | 第一個完整主視覺 | 隱藏 UI 後構圖完整；近看有細節、遠看有主體；目標硬體截圖人工檢視通過 |
| VIS-M4 | 植被風動、alpha cutout、背光透光、符文粒子與裝置啟動 | 有生命的庭院 | 風動連續；邊緣與遮擋正確；效果可暫停／重播；粒子與透明渲染成本可觀察 |
| VIS-M5 | 90–120 秒視覺導覽、自由鏡頭、功能比較與截圖模式 | 完整觀看與操作體驗 | 導覽可重播；功能差異明確；預設畫面只保留必要操作提示；截圖不含診斷 overlay |
| VIS-M6 | 最佳化、畫質分級、DX12／Vulkan 驗證、封裝、影片與效能報告 | 可分享的 V1 Visual Showcase | 固定路線效能符合經確認的預算；兩後端畫面驗收；隔離套件啟動；影片、截圖、報告對應同一版本 |

### VIS-M1 的先行技術決策

- 多材質綁定、mesh／material identity 與 GPU 資源生命週期。
- 頂點 tangent 與 normal-map 慣例、ORM 通道配置、貼圖色彩空間。
- IBL 的環境貼圖來源、irradiance／prefilter／BRDF LUT 產製與封裝。
- HDR render target、曝光、tone mapping，以及 UI 的輸出合成順序。
- Resize、場景切換、資產重載與關閉時的資源回收。

決策先寫入相關 contract 或 ADR，再擴展場景內容。Helpers 編譯通過不能取代原生像素與目標硬體畫面驗收。

## 5. 施工順序與責任

主路線：VIS-M0 → VIS-M1 → VIS-M2 → VIS-M3 → VIS-M4 → VIS-M5 → VIS-M6。

效能取樣自 VIS-M0 開始，隨每項效果持續進行；VIS-M6 負責最終收斂。VIS-M0 後可準備美術資產，但材質外觀須等 VIS-M1 的色彩與 shading 路徑穩定後確認。

| 範圍 | 主要責任 |
| --- | --- |
| Renderer／共享 shader library | 材質、光照、陰影、後處理、RenderGraph 與後端中立描述 |
| RHI／原生 adapter | GPU 資源、pipeline、binding、barrier 與後端操作 |
| Presentation | Window、swapchain、輸出合成、resize 與 surface lifecycle |
| Runtime／資產流程 | 場景、材質資源、匯入、cook、封裝與版本生命週期 |
| Showcase／內容 | 庭院、鏡頭、導覽、互動、效果比較與展示 UI |

新視覺功能應沿用共享渲染能力，避免在 Showcase 或各 Presentation backend 重複實作 shading。實際模組／公共介面調整仍須另外完成架構與相容性 review。

## 6. 三次交付

| 交付 | 範圍 | 產物 |
| --- | --- | --- |
| Look Preview | VIS-M0～VIS-M3 | 完整固定主鏡頭、材質近景、自由旋轉鏡頭與基線截圖 |
| Living Scene | 加入 VIS-M4 | 20–30 秒動態展示與可重播的環境效果 |
| V1 Visual Showcase | 完成 VIS-M5～VIS-M6 | 完整導覽、互動程式、精華影片、截圖與效能報告 |

時程在 VIS-M0 完成資產盤點，並確認 VIS-M1 技術缺口後估算；本草案不承諾日曆日期。

## 7. 效能與平台驗收

### 候選效能基準

- GTX 960、1280×720、60 FPS（16.7 ms 每幀）作為候選基準，在 VIS-M1／VIS-M2 實測後確認或修訂。
- 記錄 CPU、GPU、driver、backend、build ID、解析度、畫質、VSync 與螢幕更新率。
- Benchmark 使用固定鏡頭、固定亂數種子與效果時間軸，暖機後重複執行，效能量測關閉 VSync／FPS cap；展示模式可使用 VSync。
- 報告包含平均 FPS、P95／P99 frame time、有效的 CPU／GPU 分項時間與記憶體用量。尚未提供 GPU timestamp 時明確標記不可取得，不能用整體幀時間替代。
- 基本／標準／高畫質分別控制陰影、IBL 資源、植被密度、粒子與後處理；每個比較保持其餘條件一致。

### 驗收範圍

- Windows DX12 為第一個完整視覺交付目標。
- Windows Vulkan 分別驗證相同場景；以色彩、材質、陰影與效果一致性檢視後端差異。
- Linux Vulkan 自動化可驗證契約、原生像素、互動與生命週期；software rasterizer 結果不代表 GTX 960 效能。
- Metal 的完整實體畫面／互動驗收延續既有待辦狀態，依現有指示暫緩；VIS-M6 完成範圍明確限於 DX12／Vulkan 展示，不等同全平台 V1 最終驗收。

每階段保存固定鏡頭畫面、效果開關比較、build provenance 與測試結果。實作變更按專案規範執行 Linux Development 完整 gate，連結邊界變更另驗證 Shipping；目標平台原生畫面另行驗收。

## 8. 後續擴充

水面、SSR、體積霧、完整角色、GPU skinning、大世界與大量物件壓力展示列為後續候選。先完成材質、光影與環境動態，再以新增效果的視覺收益與實際成本決定是否擴充。

## 9. 最終完成條件

- 關閉所有診斷文字後，場景仍有完整構圖與一致美術風格。
- 三個鏡頭分別證明材質、光影與環境動態。
- 導覽、自由探索、比較與截圖模式可正常操作。
- 畫面與效能都來自交付的原生執行版本。
- 套件可隔離啟動；資產來源與授權隨包提供。
- 每個完成里程碑都有可追溯的驗收證據；未驗收平台與效果保留明確狀態。

## 10. VIS-M0 首個實作切片（2026-10-05）

原生 Showcase 新增 `--scene=courtyard`／`9`，提供原創地磚、破損建築、中央石環装置、
陶器與植被占位的工程灰盒。`B` 循環三個可重現固定鏡頭；`F4` 隱藏全部診斷 UI，仍保留
Offscreen → Main → UI → Present 排程。水晶占位沿用目前 active cooked／bundled mesh；
編譯停用資產流程時改用程序化 fallback。報告記錄鏡頭／UI 狀態與代表性資產來源 hash。

盤點：[工程來源與替換項目](../../Apps/Showcase/content/Courtyard-Greybox.md)。
本切片開始 VIS-M0；正式免費素材採用、美術檢視與目標硬體驗收仍待完成。
材質／動態鏡頭名稱代表後續展示用途，不代表 PBR 或風動已交付。

✅ 首切片 Linux 驗證：Development 84/84 無 skip；原生固定鏡頭重播、UI 隱藏／还原及
代表性資產載入已驗證。[基線證據](../../Apps/Showcase/evidence/VIS-M0-Linux-Greybox-2026-10-05/acceptance.md)。
