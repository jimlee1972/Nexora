# HD-2D 光源、柔影與選擇性 Bloom 施工計畫

> 基準：Nexora `8c4aa989`；Beads 主工單 `nexora-m6m`。工程 **7/7（100%）**；實機 **NOT_RUN**。

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
| ✅ HD2D-06 | `nexora-m6m.6` | Zig 診斷與原生驗證 | 03, 04, 05 | 已驗收 PR #739／`67db45d2`；最終41 項 CI、實際四尺寸 Zig 與三後端55 畫面 CPU 像素通過 |
| ✅ HD2D-07 | `nexora-m6m.7` | SDK 0.2.0 與遊戲接入 | 06 | 已驗收 SDK0.2.0／93a8dd51、修正 PR753／0ebfe07e、遊戲 PR31／f404f59d；兩組最終 Windows CI 通過；[收據](../../Tools/SDK/evidence/HD2D-07-Linux-2026-10-31.md) |
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

## 歷史施工紀錄

以下為各次嘗試當時的狀態；現況以上表為準。失敗與重試紀錄均保留。

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

HD2D-07 候選清理沿用既有 owner-only 機制，只增加本 SDK 明確分支；取消過期同分支 Build／candidate，保留目前 push／PR 與 main。精確 JavaScript／47 防護測試及完整 Linux297／通過289／略過八項／零失敗109.52s 與 Shipping 通過。最終 CI／發布仍待完成。

HD2D-07 發布前 Windows 候選 run41158981348 通過獨立 producer／consumer；原生144幀／features7／timing mask15／兩次重載／active-staging歸零／配置平衡，另通過 headless 及離線重建。尚未發布0.2.0或驗收遊戲／PC；仍需最終 main-push候選／完整CI與相同位元組發布。

HD2D-07 最新59826131 獨立 Windows consumer124420670087 通過實際 DX12144 幀／全部階段／兩次重載／離線 gate，但相依 macOS job124420244953 有三個空直譯器 NOT_RUN 失敗。兩種結果均保留，接入 HD2D-06 明確 Python 探測；發布前仍須完整／原生重試通過。

HD2D-07 接入直譯器修正後通過完整 Linux297／289 通過／八項略過／零失敗108.57 秒、Shipping、SDK5 與 cleanup47。macOS 重試及不可覆寫發布／遊戲 lock 仍待完成，不增加驗收完成度。

HD2D-07 接入限定 Development 的 macOS 診斷匯出修正，保留 job124428232886 在實際 Zig 啟動前的失敗。Python／素材已通過；真正 dyld 搬移／簽署／原生執行仍須重試，Windows 候選收據仍保留。

HD2D-07 接入 Mac Development 匯出後通過完整 Linux298／290 通過／八項略過／零失敗110.97 秒、Shipping（環境重連後續建完成）、SDK5／cleanup47。真正 Mac 原生重試及不可覆寫發布／遊戲 lock 仍待完成。

HD2D-06 已驗收 PR #739／`67db45d2`，受測05c563b8 與合併 tree 相同。41 項檢查全部成功，push／PR Build41167913390／41167916242 均成功結束。DX12／Vulkan／Metal 實際四尺寸 Zig Player 與55 畫面 CPU 像素通過；完整 Windows430／Linux488／Metal405 項。DX12／Vulkan timing mask15；此 Metal CI 裝置明確回報計時不可用。保留全部 Mac 直譯器／匯出／fixture 失敗及不改斷言的重試。HD2D-07 接入已驗收相依，仍需不可覆寫0.2.0 發布與遊戲實際 lock／CI。工程6/7；PC NOT_RUN。

HD2D-07 最終已驗收相依施工通過完整 Linux298／290 通過／八項略過／零失敗106.94 秒、Shipping、SDK5／cleanup47。最終提交 Windows 候選／完整 CI、不可覆寫 main-push 發布與遊戲正式 lock 仍待完成；工程6/7。

HD2D-07 發布重試：PR740 已合併93a8dd51，與測試樹一致；41項檢查與同來源 main Build41178167997／candidate41178168044 通過。0.2.0 Tag 固定在93a8dd51。發布41184604229 因 artifact 共同根目錄使唯一 acceptance.json 位於下層而在建立 Release 前停止。修正為遞迴定位唯一收據、拒絕多份，保留所有 ZIP／來源／原生／離線斷言。release/windows-2d-sdk-v<版本> 恢復分支須先取得修正發布器本身的完整 push Build 成功，才能發布既有 Tag；不移動 Tag、不重建候選包、不略過 CI。正式發布與遊戲接入通過前仍為6/7；PC NOT_RUN。

## 已發布 SDK 0.2.0

[不可覆寫 Release](https://github.com/jimlee1972/Nexora/releases/tag/sdk-2d-windows-x64-v0.2.0)，來源 `93a8dd51301c50d605a315a39279b0e5b325d91c`，ZIP SHA-256 `cddf164d55be02c0524485c17054426eab8ab096801e842aace1f3cc3cd4fb92`。PR740／93a8dd51 通過41項檢查及同來源 main candidate41178168044／完整 Build41178167997。獨立 consumer124470394777 在無原生開發工具鏈下完成144幀 DX12、全部4095階段／兩次重載／功能7／計時15／active-staging歸零／配置平衡，並離線重建。同來源 Windows430／Linux488／Metal405 項完整桌面 CTest 與55幀 CPU 像素比對通過。

首輪發布41184604229 在建立 Release 前因巢狀收據路徑停止。修正 PR753／0ebfe07e 的41項最終檢查通過；測試 afd543f6 與合併樹 fd0bb1cd 相同。成功重試41190794168／job124507009006 核對唯一收據、原 ZIP SHA／來源／Tag／內嵌 manifest／原生／離線證據，發布原候選位元組。實際選用的完整 Build41184604230 也在同一不可移動來源成功。Tag 未移動，0.1.0 保持可用。

[遊戲 PR31](https://github.com/jimlee1972/OnmyojiSurvivors/pull/31) 鎖定實際下載 ZIP，新增同一產品 Zig F9 DEV ONLY 模式。發布 SDK 的4/4測試、Windows DLL 跨編譯、11項 Python 與 CMake 鎖定／失敗檢查通過。本作最終 Windows 冷建置／無原生工具鏈／離線／inventory／雙程序／F9 runs41192048648／41192045465 在be8a5a5b 通過；遊戲 PR31 合併f404f59d。Beads07 CLOSED；工程7/7，實機 NOT_RUN。

遊戲 PR31 首輪68af9fa8 的 PR／push runs41191107578／41191084107 在冷建置／離線／inventory／NX00重播通過後，零資源檢查失敗；固定0.5秒等待未保證 GPU 完成槽位已回收。重試be8a5a5 由 Zig 查詢既有公開狀態／diagnostics：關閉請求已生效且 scene／shadow／bloom／upload 所有者歸零後才回報 RETIRED，原生 CI 每次返回均等待確認。最終零資源／功能／配置斷言與120秒期限不變。正式 SDK4/4測試／Windows DLL跨編譯及軟體 Vulkan2400幀／兩次回收確認通過；最終 Windows 重試待驗。

## 工程驗收 7/7；實機工單保持開啟

遊戲 PR31／f404f59d 在最終be8a5a5b 的 Windows PR41192048648／push41192045465 全數成功後合併：專案 Zig／拒絕原生工具鏈的冷建置、正式 SDK 來源與 SHA／完整 inventory、離線修改重建、兩個600 tick NX00程序與固定 hash、兩次原生 F9／RETIRED 確認、最終零資源／配置平衡／正常退出，以及故意失敗時保留原套件。直接樹差異確認 Game／Tools／CI／README／HD2D 證據與測試提交相同；僅額外保留最新 main 的敵人12／13美術及需求文件。首輪失敗與重試均保留。

Beads nexora-m6m.7 已 CLOSED；工程7/7。主工單 nexora-m6m 與 HD2D-PC 仍為 OPEN／NOT_RUN。遊戲預設 NX00；BuildWindows.bat 使用正式 SDK／專案 Zig、不需引擎 checkout，F9 開啟 DEV ONLY 診斷。正式美術／戰鬥、實體輸入、乾淨主機及 i5-8400／GTX1050Ti／8GB 長時間60FPS 尚未驗收。SP01～07、M6／M7 歷史判定不變。
