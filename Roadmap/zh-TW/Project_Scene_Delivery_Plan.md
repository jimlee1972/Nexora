# 專案啟動場景、多場景建置與執行時切換計畫

狀態：限定實作與交付已驗收；**✅ 5 個實作階段全部通過**。
更新：2026-10-26。相關里程碑：ED-M1、ED-M3、ED-M6；本計畫不代表任何完整里程碑通過。
持久任務與相依以 Beads 為準，下列階段表不是另一套任務系統。

✅ 階段 1 設定／core 與圖形 Build Settings 的限定自動化契約已驗收。PR #716 的
`6877b09d` 通過全部 38 項 exact-head CI 檢查，並 squash 合併為 `72c82cc7`。
Hosted Linux Development 完整 gate（登錄 454 項、零失敗、保留既有字型 skip）、Shipping、
sanitizer／TSan、native display 及各平台 gates 通過。Windows 本機 core／1x／2x UI 與啟動
smoke 通過；此開發機仍無法建立 Windows symlink fixture，完整 verbose Linux core gate
已走 alias 分支且沒有 unavailable 診斷。設定至原生遊戲的串接與最終交付仍屬階段 4／5。
[驗收與結案證據](../../Tools/Build/evidence/EditorProjectBuildSettings-2026-10-25.md)。

✅ 階段 2 cooker／package 的限定資料／producer 契約已驗收。PR #717 的 `c66cecd0`
通過全部 38 項 exact-head checks，並 squash 合併為 `7968bf62`。Hosted Linux Development
登錄 457 項、Windows 397 項、macOS 376 項，均零失敗並保留既有字型 skip。Linux Shipping、
sanitizer／TSan、feature-off 及 native／平台 gates 通過。保留初輪 macOS 暫存路徑 fixture
失敗；改用實體 fixture root 後通過，正式讀取器拒絕 alias 的規則不變。
詳見[套件證據](../../Tools/Build/evidence/EditorGameProjectPackage-2026-10-26.md)、
[套件契約](../../Engine/Runtime/GameProjectPackage.md)及 [producer 契約](../../Engine/Editor/GameProjectExport.md)。

✅ 階段 3 新增 owner-thread 排隊式 Runtime session 與可選 C／Zig Scene service，包含候選
World／presentation 準備、ID 重映射、persistent 保留與舊場景綁定清理。十二項 Windows
限定測試通過，包含實際 SDK 建置的 Zig fixture 要求 Level01 → Title → Level01。
PR #718 的 `8726bb7f` 已通過全部 38 項 exact-head checks，並 squash 合併為 `b45d880e`。
Linux Development 登錄 461 項、Windows 401 項、macOS 380 項，均零失敗並保留既有字型 skip。
Shipping、sanitizer／TSan、feature-off 及 native／平台 gates 均通過。詳見
[session 契約](../../Engine/Runtime/GameSceneSession.md)與
[限定證據](../../Tools/Build/evidence/GameSceneSession-Windows-2026-10-26.md)。

✅ 本次限定交付的階段 4／5 已驗收。PR #721 的 `f327bd86` 通過全部 41 項 exact-head
checks，並 squash 合併為 `e0036e73`。Hosted Linux Development 登錄 469 項、Windows 407 項、
macOS 386 項，均零失敗並保留既有字型 skip。Shipping、feature-off、sanitizer／TSan 及平台
gates 通過。Linux 實際 Editor 像素、圖形 script Build、獨立原生遊戲像素，以及 Windows
開發機執行、隱藏來源後無參數遊戲與真正 ZIP 解壓驗證均通過。

Editor Play 使用已儲存的啟用場景與共用 Scene V1 session，遊戲 World 與作者文件分離；
髒場景提供 Save／Use Saved／Cancel，重映射 ID 不回寫作者資料。圖形 Build 與 CLI 共用
受保護 producer 及隨附 runtime／工具。首次 configure 編譯器擷取與 POSIX 各 target profile
位置回歸已修正並覆蓋；原始失敗保留於歷史 host／build 證據。

最終 Windows Editor-only ZIP 為 `NexoraEditor-Windows-x64-f327bd86260a.zip`（102,785,740 bytes），
SHA-256 `a5e97b52f0fd80801ff0310dcd5ba6cca6aa3e499a07b1208902fcbee5bc3cc7`。
18,501 個 inventory hash 全部吻合；實際中文路徑解壓後，無參數啟動、正常有限幀關閉及
1,200 幀／三次啟用的 Play 均通過，已儲存來源不變。包含私有 Runtime／Tools 必要資源與
授權，不含 Nexora／遊戲原始碼 checkout、範例或額外 2D 應用程式。解壓後須保留整個目錄。

這只驗收原生 mesh／scalar-PBR 場景交付契約，不涵蓋所有自訂 2D／physics／plugin 格式或
任何完整 Editor 里程碑；完整圖形里程碑仍為 0/8。不宣稱 Windows 像素擷取及獨立乾淨主機
驗收。[最終驗收紀錄](../../Tools/Build/evidence/ProjectSceneDelivery-Acceptance-2026-10-26.md)保存命令、數量、原始失敗、artifact／exit／hash 證據與界線。

## 1. 使用者操作

- 在專案 Build Settings 選擇遊戲啟動 Scene 與要打包的 Scene。
- 開啟專案還原 Editor 上次編輯的 Scene，不會因此改變遊戲啟動 Scene。
- 打包後的遊戲從指定 Scene 啟動，不要求場景名稱為 Main。
- Gameplay 可以替換目前 Scene、附加載入另一個 Scene，以及卸載附加 Scene。
- 主 Editor 維持一次編輯一個 Scene；替換前保留 Save／Discard／Cancel。
- 專案目錄就是 root，不新增 root 輸入、Scene 分頁或 Preview Project 步驟。
- Editor 與命令列共用引擎提供的設定、cooker 與建置入口。
- 最終 Editor ZIP 包含可執行檔、必要私有 Runtime／Tools 資源與授權，不含 Nexora／遊戲
  原始碼 checkout、TwoDPlayer、TwoDStudio 或 TwoDAssetCompiler；編譯器標準函式庫是必要
  工具資料。建置出的遊戲 runtime 是另一份產物。

## 2. 已查證基線

main `d1ec642bbf8330df21d9c3f3e6b87510dcbfd42d` 的 `ProjectDescriptor` 只有識別、名稱與 schema，
沒有遊戲啟動場景或打包清單。`SceneFileSession` 將上次 Editor 場景記在
`.nexora/scene-session.ini`；應用程式退回預設時使用 `.nexora/scenes/Main.scene`。
圖形匯出協調器只擷取當前文件。StaticView schema 1 僅接受一個 Scene 及其 mesh／scalar-PBR
相依閉包，額外 Scene 會被拒絕。`World` 已支援場景 snapshot、啟用與延後卸載。
Gameplay V3 的 `load_scene` 建立邏輯 Scene，不會解析遊戲套件中的製作場景；原生 StaticView
palette 是不可變資料。這些是前置能力，不是完整場景交付驗收。

依據：[workspace](../../Engine/Editor/include/Nexora/Editor/EditorWorkspace.h)、
[Editor host](../../Apps/Editor/main.cpp)、[場景檔](../../Engine/Editor/src/SceneFiles.cpp)、
[套件契約](../../Engine/Runtime/ProjectPackage.md)、
[原生 Player](../../Apps/ProjectPlayer/README.md)、
[SDK 建置介接](../../CMake/Modules/NexoraGameProject.cmake)。

## 3. 專案設定與識別

在 `project.nexora` 旁使用版本化、專案擁有的 UTF-8 `project.build.json`。
保留既有身分 descriptor 與 Zig script manifest 的可讀性，不把模組建置／ABI 接線複製到
作者專案。這份設定納入版本控制，Editor／CLI 共用；Editor session metadata 仍分開儲存。

預定 schema 1：

```json
{
  "schema": 1,
  "start_scene": "11111111-1111-4111-8111-111111111111",
  "scenes": [
    {"id": "11111111-1111-4111-8111-111111111111", "path": "Content/Scenes/Title.scene", "enabled": true},
    {"id": "22222222-2222-4222-8222-222222222222", "path": "Content/Scenes/Level01.scene", "enabled": true}
  ]
}
```

UUID 是執行／打包身分，path 是標準化的專案相對製作路徑。已有 catalog 的檔案沿用其身分；
專案根目錄及 `.nexora/scenes` 中的既有場景，需要明確且安全的身分登錄，不複製或偷偷移入
Content。透過 Editor 改名時更新定位路徑、保留 UUID；外部改名若無法恢復身分則要求明確修復，
不以同名檔案代替。重複 UUID／路徑、catalog 身分衝突、無效 UTF-8、alias、穿越或 root 外路徑、
超量資料及未知 schema 都要明確拒絕。設定上限為 256 KiB、256 筆 Scene 及每個相對路徑
1024 UTF-8 bytes；階段 1 parser／測試須在配置前檢查上限。

啟動 UUID 必須指向啟用、存在且可讀的 Scene。停用場景不打包；啟用場景仍引用的共用資源保留。
刪除或停用啟動場景前，必須選擇另一個啟動場景才能儲存設定。新專案可以在第一個 Scene 成功
儲存時，明確且僅初始化一次啟動場景；只開啟另一個 Scene 不得改變設定。沒有設定的舊專案仍可
開啟；Build Settings 提供「以當前已儲存 Scene 初始化」操作。遊戲建置要求有效設定，不猜 Main。

Build Settings 提供 Add Current／Add Scene、Remove、Enable、Set Startup。顯示相對路徑及
啟動選擇，解釋無效／遺失項目。原子儲存遵守 read-only、recovery 與 writer lease 保護。
匯出遇到相關 dirty 編輯狀態時，先要求儲存；取消或失敗不得改變設定、文件或上一份套件。
建置使用已儲存 snapshot。

## 4. 多場景套件與建置歸屬

新增獨立版本的 gameplay scene-package capability。維持 StaticView schema 1 的 bytes、
驗證、CLI 行為與不可變 consumer，不讓新套件冒充 StaticView。實作 codec 前，固定 magic／
version、project UUID、startup UUID、scene catalog、排序資產記錄、hash、大小上限與支援元件能力。
初始總量准入上限為 256 個 Scene、4096 個資產、256 MiB 序列化套件、64 MiB World snapshot
文字、100000 個 entity、128 MiB 解碼 geometry。既有較嚴格的單資產 codec 上限仍適用；
這些是准入上限，不是整個 process 記憶體或效能保證。

Producer 擷取所有啟用場景，計算相依閉包聯集，以完整 UUID 將共用資源只 cook 一次。同 UUID
卻有不同 bytes／type／相依時拒絕。場景保存作者識別，Runtime 另做 instance 內 entity reference
重映射。缺少相依、無效啟動／catalog reference、不支援內容及超量資料必須在發佈前拒絕。
StaticView 保留為 inactive 的 opaque bytes 不代表 Gameplay 行為可執行；遊戲建置必須明確報告
不支援的必要行為。首版不支援序列化跨 Scene entity reference，遇到時拒絕。

完整驗證後才原子發佈；過期 project／document／settings generation 及取消要求不得發佈。
Build manifest 記錄來源／設定身分、套件 hash、啟動與納入場景、相依清單、執行檔／模組／DLL
清單及命令。Editor／CLI 呼叫同一個引擎建置服務，沿用既有 CMake／Zig 介接。
編譯／cache／ABI 接線留在 SDK 或 build 目錄。只有明確宣告 scene-only 的遊戲可不含 script；
已設定的 Gameplay script 必須成功編譯與載入。

獨立遊戲不需使用者提供參數或作者原始碼，依執行檔所在位置找到 runtime、Gameplay module 與
套件，從其他 CWD 啟動也相同。執行時不依賴 Editor 或編譯器。開發機搬移驗收與獨立乾淨主機／
平台驗收分開記錄。

## 5. Runtime 切換契約

建立 Runtime 擁有的 scene-package session，不依賴 Editor。套件生命週期內保留已驗證、不可變的
CPU catalog 與共用 cooked assets；每次載入配置新的 runtime entity／scene ID，重映射階層與
元件 reference，不改作者 bytes。Gameplay 對外 reference 使用有 instance／generation 驗證的
handle，已卸載 entity 的 reference 必須拒絕。不得覆寫另一個 Scene 來處理重複 ID，也不得先把
候選 snapshot 載入 live World 再嘗試回復。

Scene request 在 Gameplay callback 結束後，於安全的 owner-thread frame boundary 序列執行。
同時只允許一個 pending transition，另一個 request 回傳 Busy。保留最後完成的 request status
直到被取代或 session 銷毀，最多載入 256 個 Scene instance。提供固定 not-found／unsupported／
error 結果。Commit 前準備
World 資料、所需原生資源與 camera／render binding。任何準備失敗都保留舊 active World、camera
與 render state。初始啟動失敗須帶有可理解的 log 結束，不偷偷替換為 Main。

- **Replace（預設）**：完整準備的新 Scene 啟用後，才退役原本非 persistent Scene。
- **Additive**：保留既有 Scene；首版拒絕重複載入已 active 的相同 Scene asset。
- **Unload**：退役指定的非 persistent instance；persistent Scene 遵守明確 host policy。
- 契約固定 active-camera 選擇及無 camera 場景的 fallback；Additive 不得意外搶走 camera。
  卸載 camera 所在場景後，選擇有效保留 camera 或已記錄的 fallback。
- 共用 CPU asset 不隨場景退役消失；GPU 資源須等已提交工作排空才能釋放。失敗 request 釋放
  暫存 ownership。重複切換後資源數應穩定。
- App 級 Gameplay module 在切換間保持 attached；卸載 Scene 所有的狀態前完成 callback。
  複製的 table／handle 不得超過 owner 壽命，需記錄事件順序與 stale-handle 行為。
  Stop／teardown 在銷毀 package／service 前取消 pending work。

沿用 C Gameplay service 機制，新增可選、size／version 協商的 Scene service，並提供 Zig
request／status helper。不更改 V3 `load_scene` 語意或既有 ABI offset。舊模組沿用協商 prefix；
不支援的 host 回報 Unsupported。Editor Play 與獨立 Player 登錄相同 service、使用同一 session，
避免行為分歧。目前自訂 2D 內容／匯出仍是另一個 capability，須有明確且通過測試的 adapter 才能
納入；不得宣稱本次接受所有 2D／physics／plugin 格式或完整 Editor 里程碑。

## 6. 依序交付階段

| 階段 | 範圍 | 必須交付的結果 | 驗收 |
| --- | --- | --- | --- |
| 1 | Settings/core 與圖形 Build Settings | 獨立啟動／打包清單、migration 與受保護持久化 | ✅ 限定設定／UI 契約已驗收；PR #716 |
| 2 | 多場景 cooker／package | 可重現 catalog 與相依聯集，既有 StaticView 不變 | ✅ 限定 cooker／package 契約已驗收；PR #717 |
| 3 | Runtime session 與 C／Zig Scene service | 原子 Replace／Additive／unload、識別與生命週期驗證 | ✅ 限定 session／API 契約通過；PR #718 |
| 4 | 共用 build、Editor Play 與原生 Player | 各入口使用同樣的作者場景、啟動設定及切換 | ✅ 已驗收；PR #721 |
| 5 | 整合驗證與 Editor 交付 | 證據、CI、已合併原始碼及可執行 Windows Editor-only ZIP | ✅ 已驗收；最終驗收紀錄 |

在同一個功能分支依相依順序實作，將階段證據記在相關契約／roadmap 與 Beads。
規劃 PR 可以先建立契約，不代表任何實作階段通過。保留其他人的 claim，以及歷史 additive-editor／
StaticView 驗收記錄。

## 7. 驗收專案與 gates

建立隔離專案，包含 Title、Level01 與不打包的第三個 Scene。Title／Level01 有可明確區分的
geometry／camera 及共用資源，刻意使用重疊的作者 local entity ID。將 Level01 設為 startup，
但 Editor 最後編輯 Title。小型 Zig Gameplay script 呼叫 Scene service，記錄可重現 frame／status
與原生畫面觀察。

| Gate | 必須觀察的結果 |
| --- | --- |
| Settings／UI | 儲存／重開啟動選擇、納入／排除、改名、read-only／recovery、dirty Save／Discard／Cancel |
| 可重現／完整性 | 同輸入得到同套件；共用資源一份、排除場景不存在；損壞／缺少／衝突項目拒絕 |
| 啟動 | 無參數遊戲從 Level01 啟動，不受 Main、Editor 最後場景或 CWD 影響 |
| 切換 | Level01 → Title → Level01 改變實際原生 pixels／camera，來源檔案不變 |
| Additive／卸載 | 重疊作者 ID 正確重映射；persistent 保留、重複載入及 stale handle 遵守契約 |
| 失敗保全 | 遺失場景、損壞資源及原生準備失敗後，舊場景仍可玩且可見 |
| 生命週期 | 至少 100 次切換及 module Stop／teardown；無殘留 process；資源／handle 計數穩定並通過 sanitizer |
| 相容性 | 舊專案、StaticView 套件、V3 module prefix 及關閉 optional feature 的建置仍有效 |
| 搬移 | 移除來源後從其他 CWD 執行遊戲副本；保存 hash、相依、exit／stdout／stderr 及觀察 |
| Editor ZIP | 搬移後無參數啟動；使用隨附相依即可操作 Build Settings 與作者驗收專案 |

實作驗收必須在 Linux 完整執行以下命令，保存 log／測試數／skip：

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

另執行相關 C／Zig ABI、optional-feature stripping、native display 與 Windows 測試。
保存精確來源／工具身分、套件 hash、命令、log 與原始失敗嘗試。Hosted Linux／Windows、此開發機、
VM 與獨立乾淨主機的證據分開。建立／更新本 agent 的 PR，僅在適用的 exact-head CI 成功後合併。
只有具證據的完成範圍能標示綠色勾；這份需求規劃本身不代表任何實作完成。
