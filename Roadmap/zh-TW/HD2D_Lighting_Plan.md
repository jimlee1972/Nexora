# HD-2D 光源、柔影與選擇性 Bloom 施工計畫

> 基準：Nexora `8c4aa989`；Beads 主工單 `nexora-m6m`。工程 **5/7（71%）**；實機 **NOT_RUN**。

## 目標與界線

通用 2D Player 提供動態局部光、動態遮擋柔影與選擇性 bloom，DX12／Vulkan／Metal 同一契約。遊戲與診斷操作全部 Zig；通用引擎可使用既有 C++。第一版平面受光，不含 normal map、3D 高度陰影、HDR 螢幕輸出或完整 Editor 編輯工具。

## 工單與進度

Beads 是任務狀態來源；本表為審閱用快照。工程以已驗收子工單／7 計算，不含實機；部分施工不算完成。只有附證據的完成項使用 ✅。主工單等 PC 驗收通過才關閉。

| ID | Beads | 交付 | 相依 | 狀態／證據 |
| --- | --- | --- | --- | --- |
| ✅ HD2D-01 | `nexora-m6m.1` | 公開契約與 Zig bindings | — | 契約／快照已交付；[Linux 證據](../../Engine/TwoD/evidence/HD2D-01-Linux-2026-10-31.md) |
| ✅ HD2D-02 | `nexora-m6m.2` | 三後端線性 RGBA16F 管線 | 01 | 已驗收 [PR #735](https://github.com/jimlee1972/Nexora/pull/735)，squash `ed0e51e4`；41 項 CI 與三後端原生證據 |
| ✅ HD2D-03 | `nexora-m6m.3` | 動態局部點光源 | 02 | 已驗收 PR #736，squash `40c96072`；目前提交的原生 DX12／Metal 與 Linux gate 通過 |
| ✅ HD2D-04 | `nexora-m6m.4` | 動態遮擋與柔影 | 03 | 已驗收 PR #737／`0845c078`；最終提交的原生 DX12／Metal 與完整 Linux CI 通過 |
| ✅ HD2D-05 | `nexora-m6m.5` | 選擇性發光與 bloom | 02 | 已驗收 PR #738／`a28f1001`；最終34 項檢查及 DX12／Vulkan／Metal53 畫面 CPU 讀回通過 |
| HD2D-06 | `nexora-m6m.6` | Zig 診斷與原生驗證 | 03, 04, 05 | IN_PROGRESS — 實際 Zig 四尺寸原生診斷、GPU 時間與陰影圖檢視；完整／原生 CI 待驗 |
| HD2D-07 | `nexora-m6m.7` | SDK 0.2.0 與遊戲接入 | 06 | IN_PROGRESS — 獨立0.2.0 候選／consumer 與同遊戲 Zig 施工；不可覆寫發布／遊戲 lock 待完成 |
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

HD2D-04 本地施工：GPU 角度陰影圖與獨立 CPU 參考值；[證據](../../Engine/TwoD/evidence/HD2D-04-Linux-2026-10-31.md)。IN_PROGRESS；相依 PR／原生 CI 通過前不計驗收完成。

HD2D-05：[本地證據](../../Engine/TwoD/evidence/HD2D-05-Linux-2026-10-31.md)；IN_PROGRESS。

HD2D-03 已驗收：PR #736／`40c96072`；目前提交的 Windows job `124365092041` 與 macOS job `124365092312` 通過實際原生像素及完整桌面 CTest。HD2D-04 PR #737 與本相依階段仍需各自原生 CI。工程 3/7；實機 PC NOT_RUN。
HD2D-05 補上整條管線關閉後的回收：原繪製路徑逐槽釋放已完成的離屏資源，保護後加 HUD，重新啟用可重建。依連結保留本地／原生 gate 證據；CI 前不計驗收完成。

HD2D-04 已驗收：PR #737／`0845c078`，受測 `dd256588`。Windows52 原生畫面／425 CTests、Metal52／400、Linux483；最終34 項檢查完成（10 項為文件 push 正常略過）。HD2D-05 初步提交9e2323ce 的41 項檢查及 DX12／Metal53 畫面讀回通過；僅相依 rebase 保留執行程式內容，仍須最終提交 CI。工程4/7；實機 PC NOT_RUN。

HD2D-06：可重用 Zig 場景、原生 GPU 陰影圖檢視、已完成 slot 的逐 pass 時間、四尺寸匯出 Player 與明確不可用狀態。[本地證據](../../Engine/TwoD/evidence/HD2D-06-Linux-2026-10-31.md)；IN_PROGRESS，不增加已驗收完成度。

HD2D-03 驗收收據：PR #736／`40c96072`；目前提交的 Windows job `124365092041`（425 項 CTest）與 macOS job `124365092312`（400 項 CTest）通過實際原生像素讀回。實機 PC 仍 NOT_RUN。

HD2D-06 輸入證據補強：合成點擊改等 Zig 實際取樣的中立／按下確認，不再猜固定時間。保留觸發此修正的相依完整 gate 失敗；連續三輪四尺寸原生測試通過，仍要求恰好一次啟動及相機不變。驗收仍需最終提交 CI。

HD2D-06 讓移動 Sprite 使用上次成功提交的光源／遮擋姿態，包含零尺寸恢復；UI 重載延到下一次更新開頭。完整 Linux297／通過289／略過八項／零失敗111.82s 與 Shipping 通過。未改 ABI／宿主語意，CI 前不計驗收完成。

HD2D-06 四尺寸總 watchdog 改為涵蓋四個不變的90s case 與匯出／結束（420s），依據為實際 Windows144幀 consumer 約53s。像素／資源／輸入斷言及產品效能門檻不變。完整 Linux297／通過289／略過八項／零失敗105.54s 與 Shipping 通過；仍需最終提交 CI。

HD2D-06 保留 macOS job124420121268 的設定失敗：既有 Python 探測區塊排除 Apple，三個診斷測試取得空直譯器。改為在診斷註冊邊界明確要求直譯器。Metal 原生55 畫面 CPU 像素讀回本身通過；三個診斷測試須重跑後才驗收，不放寬斷言或略過。

HD2D-06 直譯器範圍修正通過完整 Linux Development297／289 通過／八項略過／零失敗112.54 秒，包含實際四尺寸 Zig Vulkan 與同步驗證；Shipping 通過。macOS 實際診斷重試仍須通過。

HD2D-06 保留相依 job124428232886 的第二次 macOS 診斷失敗：Python／素材生成已可執行，但 ExportGame 僅支援 Windows／Linux，拒絕 dylib。新增明確限定 Development 的 macOS dyld 圖封裝、繼承 RPATH 解析、複本 import 搬移／ad-hoc 簽署及完整 inventory。六個可攜圖與失敗 fixture 通過；仍須實際 Metal Zig 啟動重試，不宣稱 macOS Shipping 發布或放寬斷言。

HD2D-06 限定 Development 的 macOS 封裝通過完整 Linux298／290 通過／八項略過／零失敗109.56 秒及 Shipping，包含六個可攜 dyld fixture。實際 Mac 二進位搬移／簽署／原生 Zig 重試仍待驗。

HD2D-06 保留相依 macOS mimalloc job124435568972 的 fixture 失敗：暫存 /var 解析為 /private/var，mock import 字典卻用未解析路徑。以 Linux 真實符號連結 TMPDIR 重現六個 KeyError；僅將 fixture 根路徑正規化後，六項原測試通過。匯出器／執行程式及斷言不變，仍須實際 Mac 重試。

HD2D-06 fixture 根路徑正規化通過完整 Linux298／290 通過／八項略過／零失敗106.22 秒及 Shipping；仍須實際 Mac 重試。

HD2D-05 已驗收 PR #738／`a28f1001`，受測0cd1f27a 與 squash tree 相同：Windows 原生53／完整426 項、Metal53／401、Linux53／484；最終34 項檢查成功完成（10 項為文件 push 正常略過）。HD2D-06 相依 rebase 保留執行程式內容，仍需最終提交原生／完整 CI 與 Mac fixture 重試。工程5/7；實機 PC NOT_RUN。

HD2D-07 施工將明確 mask 工具與具授權 Zig 診斷納入 0.2.0 候選。無來源碼獨立 consumer 建置／執行實際 headless／原生 DX12 診斷並離線重建。遊戲預設 NX00 hash 不變；本地同一產品 Vulkan 兩輪 F9 證據通過。仍需候選原生 CI、相依驗收、實際發布摘要及遊戲 lock；不增加已驗收完成度或宣稱發布。

HD2D-07 發布來源補強：獨立 consumer 收據綁定實際候選 ZIP SHA-256、來源提交、原生 DX12 診斷與離線重建。CI 清理只限本交付四個明確自有分支的過期 Build；保留目前提交的 push／PR、main、其他操作者與相近名稱分支。47 項清理防護測試通過。

HD2D-07 [本地準備證據](../../Tools/SDK/evidence/HD2D-07-Linux-2026-10-31.md)：最終完整 Linux gate 登錄 297／通過 289／略過八項／零失敗，Shipping 通過。Windows 候選 consumer／發布／遊戲 lock 尚待完成；工程仍 3/7。

HD2D-07 首輪 Windows consumer 在 producer 成功後於 headless 資源 gate 失敗；保留 job `124399279404`／artifact `13024158144`。將常駐 Atlas／Clip 範例與空資源起始的 HD2D 診斷分開，保留資源歸零斷言。重跑／原生／發布仍待完成。

HD2D-07 本地配對重現確認：常駐基準素材320 active bytes，獨立 HD2D 為0，兩者皆144次更新／全部階段／正常清理。最新姿態對齊 SDK 的完整 Linux297／通過289／略過八項／零失敗106.49s 與 Shipping 通過。原生 consumer／發布仍待完成。
