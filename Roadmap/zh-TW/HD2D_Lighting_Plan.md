# HD-2D 光源、柔影與選擇性 Bloom 施工計畫

> 基準：Nexora `8c4aa989`；Beads 主工單 `nexora-m6m`。工程 **1/7（14%）**；實機 **NOT_RUN**。

## 目標與界線

通用 2D Player 提供動態局部光、動態遮擋柔影與選擇性 bloom，DX12／Vulkan／Metal 同一契約。遊戲與診斷操作全部 Zig；通用引擎可使用既有 C++。第一版平面受光，不含 normal map、3D 高度陰影、HDR 螢幕輸出或完整 Editor 編輯工具。

## 工單與進度

Beads 是任務狀態來源；本表為審閱用快照。工程以已驗收子工單／7 計算，不含實機；部分施工不算完成。只有附證據的完成項使用 ✅。主工單等 PC 驗收通過才關閉。

| ID | Beads | 交付 | 相依 | 狀態／證據 |
| --- | --- | --- | --- | --- |
| ✅ HD2D-01 | `nexora-m6m.1` | 公開契約與 Zig bindings | — | 契約／快照已交付；[Linux 證據](../../Engine/TwoD/evidence/HD2D-01-Linux-2026-10-31.md) |
| HD2D-02 | `nexora-m6m.2` | 三後端線性 RGBA16F 管線 | 01 | IN_PROGRESS — 三個原生實作；Linux 軟體 Vulkan 像素證據，Windows／Metal CI 待驗 |
| HD2D-03 | `nexora-m6m.3` | 動態局部點光源 | 02 | IN_PROGRESS — 原生點光源、Zig 識別及 Linux 參考值；相依 02 驗收 |
| HD2D-04 | `nexora-m6m.4` | 動態遮擋與柔影 | 03 | NOT_RUN |
| HD2D-05 | `nexora-m6m.5` | 選擇性發光與 bloom | 02 | NOT_RUN |
| HD2D-06 | `nexora-m6m.6` | Zig 診斷與原生驗證 | 03, 04, 05 | NOT_RUN |
| HD2D-07 | `nexora-m6m.7` | SDK 0.2.0 與遊戲接入 | 06 | NOT_RUN |
| HD2D-PC | `nexora-m6m.8` | Windows 實機驗收 | 07 | NOT_RUN |

## 實作契約

- **01 — API：**新增獨立協商 `NexoraGameplayTwoDEffectsApiV1`，services 尾端 getter；保留既有前綴與 Sprite ABI。提供能力／容量／狀態、環境光、整批光源／輪廓／Sprite 效果、bloom 與強度。僅 owner thread，驗證後複製、下一畫面邊界生效。entity／pool handle 關聯不依賴排序位置；無效／超量保留上一狀態，舊宿主明確 UNSUPPORTED。
- **02 — 管線：**預設關閉；獨立 2D RGBA16F 場景與 emission，依既有 Y 排序／straight alpha 合成，bloom 後固定亮度映射至 SDR，最後 UI、只編碼一次 sRGB。畫框／resize／零尺寸／GPU 完成後回收必須正確。不把 3D DrawScene 冒充 2D 路徑；能力不足明確拒絕，由 Zig 選既有 fallback。
- **03 — 光源：**圓形點光源位置／linear RGB／強度／半徑／平滑衰減；環境光、受光開關、分組遮罩與畫框／半徑剔除。32 光源、最多 8 投影光源，超量整批拒絕；容量不等於 60FPS 承諾。
- **04 — 陰影：**封閉簡單多邊形，最多 256 個、每個 16 頂點、合計 2048 邊；拒絕自交／退化／非有限資料。每投影光源 1024 角度最近距離樣本、有界柔化、投影／接影分組及偏移。三後端同契約與 CPU oracle；處理接縫、貼邊、自遮擋，不宣稱物理面光源半影。
- **05 — 發光：**可選 linear emission mask／發光色／強度，以獨立素材描述關聯既有 Atlas／region。同步裁切／padding／repack，不改舊 Atlas／Clip 格式。emission 保留透明遮擋順序；半解析度起始、最多五層 bloom。支援 export／inventory／延後載入／回收；HUD 不進效果。100／75／50% 降低裝飾光及 bloom，必要危險線獨立；off 回到舊路徑。
- **06 — 診斷：**通用 Player 載入真 Zig 模組，移動燈籠、重疊彩光、移動遮擋／柔影、局部發光、白 HUD；提供效果／強度／輪廓／陰影圖切換。素材明示 DEV ONLY。
- **07 — SDK：**發布不可覆寫 0.2.0、保留 0.1.0；獨立 consumer 驗過的候選 ZIP 原位元組發布。遊戲更新 lock／hash，保留專案 Zig、一鍵及離線建置，新增同產品診斷模式並保留 NX-00 預設。無引擎原始碼／MSVC 需求。

## 驗證與交付

每階段保留命令、版本、結果與失敗重試。完整 `cmake --preset linux-development`、`cmake --build --preset linux-development`、`ctest --preset linux-development`；公開／連結變更補 `linux-shipping`。shader、module graph、ABI 與新舊宿主回歸必須通過。

三後端原生像素讀回比對 CPU oracle，固定且記錄誤差範圍；驗證衰減／分組／遮擋／柔邊／接縫、非 emission 白物件與 HUD 排除、透明／翻轉／相機旋轉／畫框。尺寸 1080p、21:9、4:3、奇數；DPI／resize／最小化恢復。驗證替換／卸載／轉場／關閉資源回收。記錄真實 GPU pass 時間、draw calls 與資源峰值；無 query 時標不可用，不造零值。

Windows／macOS 原生結果由對應 CI 提供，未跑不填 PASS。工程全數通過仍不代表正式遊戲驗收。PC 工單另驗 i5-8400／GTX 1050 Ti／8GB、正式美術與操作、長時間幀時間；現為 NOT_RUN。既有 SP-01～07、M6／M7 不因此結案。

先合併本計畫，再依相依逐階段 PR。每階段通過適用測試／當前 head CI 後 squash merge，同步雙語 roadmap、Beads 與證據。來源、commit、PR、測試與待驗項目必須可追溯，不提交私人設備識別資訊。

參考：[2D roadmap](2D_Roadmap.md)、[色彩與既有 fallback](../../Engine/TwoD/ColorAndFallback.md)。

HD2D-01 僅交付可協商契約、Zig 封裝與 owning snapshots；預設原生能力為 0，Player 尚未註冊，不代表光影已可呈現。此 PR 以適用 CI 成功為合併條件。

HD2D-01：[PR #734](https://github.com/jimlee1972/Nexora/pull/734)，squash `3628a62e`，當前提交 41 項檢查全數通過。保留既有 Windows VM 逾時失敗，未修改斷言，重試通過。HD2D-02 在原生後端 gate 通過前不增加已驗收完成度。

HD2D-02: [Linux 證據](../../Engine/TwoD/evidence/HD2D-02-Linux-2026-10-31.md).

HD2D-03: [本地證據](../../Engine/TwoD/evidence/HD2D-03-Linux-2026-10-31.md); IN_PROGRESS.
