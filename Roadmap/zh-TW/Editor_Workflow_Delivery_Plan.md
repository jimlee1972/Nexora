# Editor 腳本工作流程與 Sprite 整合交付計畫

[English](../en/Editor_Workflow_Delivery_Plan.md) · [Epic #741](https://github.com/jimlee1972/Nexora/issues/741)

更新：2026-11-19。基線：`8c4aa989ee7443cc0b2129d54f0f0f77c73d2b49`。
最初文件發布基底：`67db45d23e631b0e05b42c23716245a34ae996d5`。
ED-WF-04 整合基底：main `a21c1ca2293e4200de8dcaa4b892ffef34687c9c`，包含外觀更新。
驗收快照：**6/9 工單（66.7%）**；ED-WF-01 已由 [PR #751](https://github.com/jimlee1972/Nexora/pull/751)、ED-WF-02 由 [PR #754](https://github.com/jimlee1972/Nexora/pull/754)、ED-WF-03 由 [PR #756](https://github.com/jimlee1972/Nexora/pull/756)、ED-WF-04 由 [PR #758](https://github.com/jimlee1972/Nexora/pull/758) 合併，ED-WF-06 由 [PR #772](https://github.com/jimlee1972/Nexora/pull/772) 及其前置切片完成驗收。
ED-WF-07 由[整合製作驗收](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07.md)完成驗收。
下方歷史支援紀錄保留交付當時的進度數字。
後續已驗收項目及 merged PR／證據記於 Epic。完整圖形化 Editor 驗收維持 **0/8**；
本計畫不自動驗收實體裝置或乾淨主機 gates。

## ✅ Editor 來源製作整合驗收（2026-11-19）

同一個真實原生專案串接 Build 登錄場景的開啟／Inspector 指派、精確 Undo／Redo／儲存重開、
Atlas／Clip 編輯、四次設定式 Play 與六次 Content Reimport，包含損壞／缺檔的上一代保留。
同一份 GUI 儲存場景與資產 Build 後，刪除來源、搬移到 Unicode 路徑的 Player 原生像素符合
獨立預期。最後凍結來源通過完整 Linux **560/560**，零失敗／略過／重跑，**5913.08 秒**；
整合案例 **191.49 秒**、實際 SDK 重載 **1684.38 秒**（33 次替換／7 次拒絕）、來源重匯入
**383.03 秒**（70 次發布）。詳見[驗收矩陣與收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07.md)。
審查交付後 workflow 為 **6/9（66.7%）**；圖形 Editor **0/8**、TwoD **9/11**、ED-WF-05／08／09
及實體／乾淨主機 gate 仍分開驗收。例行 CI 維持手動執行。

## ✅ 主 Game 來源與 SDK 交錯更新

獨立的 `nexora-whr.8.3.9` 案例驗證同一段 Play 中，於 SDK 替換尚未接納時執行真實 Content
發布。最後原生案例通過 **367.14 秒**：五次替換、13 次發布、上一代模組持續執行與嚴格計數器
保留、損壞／缺檔保留、隱藏 pool 非法修改拒絕、回復、六次 finalizer 與五階段場景／動畫。
完整 Linux **561/561** 通過，零失敗／略過／重跑，**6562.32 秒**；既有 33 次替換與 70 次發布
案例也維持原斷言通過。詳見[限定證據與收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-Interleaved-Linux.md)。
僅驗收這項交錯支援切片；ED-WF-08／parent8.3、workflow **6/9**、Editor **0/8**、TwoD **9/11**、
實體主機 gate 與獨立 8.2／ED-WF-05 認領仍分開處理。

## 現有基礎與缺口

Editor 已透過共用 producer 提供腳本勾選、manifest、Build 與 Cancel。Runtime 已擁有 V3
模組世代及可選 save/load callbacks；計畫基線的型別化 Zig game adapter 尚未發布遷移 callbacks。
ED-WF-04 ✅ 已由 PR #758 驗收，提供有界型別化狀態、實際註冊工作 barrier 及嚴格重載交易。
macOS panic 隱含 TLS 與已棄用 section API 的失敗均已修復並重新驗證，保留原卸載斷言。
Sprite source 已有不可變 metadata、C++ 來源感知擷取及 NX2S schema 3 場景持久化。
以上屬於基礎，不代表下列完整工作流程已驗收；現有 CLI 與 `.bat` 開發流程持續可用。

共用 CMake/Zig SDK 負責產生入口及 ABI 接線。Authored 專案只維護場景、資產、腳本、
manifest 與設定，不要求使用者維護第二份 SDK 或建置管線。
SortingGroup、九宮格／Tiled、遮罩及進階動畫狀態機不在本批範圍。

## 工單

| ID | GitHub | 相依 | 必須通過的驗收 | 狀態 |
| --- | --- | --- | --- | --- |
| ED-WF-01 | [#742](https://github.com/jimlee1972/Nexora/issues/742) | 無 | 發布雙語計畫、能力基線、工單圖及驗證／證據對應 | ✅ PR #751 |
| ED-WF-02 | [#743](https://github.com/jimlee1972/Nexora/issues/743) | 01 | 保存腳本 Build 設定，真實 GUI Build／Build & Run、取消、進度與診斷 | ✅ PR #754 |
| ED-WF-03 | [#744](https://github.com/jimlee1972/Nexora/issues/744) | 01 | 無原始碼的 Runtime 成品；開發紀錄與發布 inventory 留在成品外 | ✅ PR #756 |
| ED-WF-04 | [#745](https://github.com/jimlee1972/Nexora/issues/745) | 01 | Runtime／Zig 交易式遷移；替換失敗保留舊模組及 World | ✅ PR #758 |
| ED-WF-05 | [#746](https://github.com/jimlee1972/Nexora/issues/746) | 02、04 | 背景編譯來源並保留狀態熱重載，不需 Stop／Play | 施工中；整合未驗收 |
| ED-WF-06 | [#747](https://github.com/jimlee1972/Nexora/issues/747) | 01 | 相容的 C／Zig 來源 API 與 cooked Player 相依閉包 | ✅ PR #767、#769、#770、#772；[驗收](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06.md) |
| ED-WF-07 | [#748](https://github.com/jimlee1972/Nexora/issues/748) | 06 | 真實 Editor 來源索引／Inspector／Undo／儲存重開／Play／Build／重匯入 | ✅ [整合驗收](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07.md) |
| ED-WF-08 | [#749](https://github.com/jimlee1972/Nexora/issues/749) | 04、06、07 | 替換後保留來源動畫錨點、存活 pool 實例與有效 handle | 待辦 |
| ED-WF-09 | [#750](https://github.com/jimlee1972/Nexora/issues/750) | 02、03、05、07、08 | 整合原生驗收及實際解壓驗證的 Windows Editor ZIP | 待辦 |

✅ 獨立的 ED-WF-06 註冊表切片（`nexora-whr.6.1`）新增獨立版本的 C／Zig Sprite source
註冊表，以及具備 Atlas 相依驗證與有界退休機制的不可變 C++ resolver。
[Linux 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-SourceRegistry-Linux-2026-11-12.md)
記錄實際執行的註冊表 gate 與限制。

✅ 來源 API 切片（`nexora-whr.6.2`）新增檢查 size／version 的 C／Zig 來源 Sprite／pool
descriptor、相容的 optional 表協商，以及標準 Player 註冊與擷取。真正由 SDK 編譯的 Zig
模組在匯出、搬移並刪除來源後執行；原生像素驗證負縮放 pool 幾何及來源／Atlas 替換。
詳見 [API／Player 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-SourceAPI-Linux-2026-11-12.md)。
Editor 來源流程及來源動畫重載仍待完成。完整 [ED-WF-06 驗收](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06.md)
合併下方 catalog 及原生重排 gate；完整 Editor 里程碑仍為 **0/8**。

✅ 獨立 catalog 切片（`nexora-whr.6.3`）新增 cooked Sprite source 目錄種類 4、Player
預載／延後准入與標準來源匯出。既有 opaque 種類 3、C／Zig 表格版面、明確先載入 Atlas 的
順序及不可變 ownership 保持一致。[Linux 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-SpriteCatalog-Linux-2026-11-12.md)
記錄實際搬移成品的准入與損壞／相依閉包拒絕。真正由 SDK 建置的 Zig 檔案目錄遊戲接上
PR #769 已驗收的來源 API，驗證搬移、刪除作者端來源及替換後的場景／pool 原生像素。
Editor 製作仍屬 ED-WF-07；下方完整 ED-WF-06 驗收將流程進度提高為 **5/9**，
圖形化 Editor 仍為 **0/8**。

各工單於 `Tools/Build/evidence/EditorWorkflow/ED-WF-XX.md` 記錄精確指令、結果及限制。
現有 [ED-WF-01 基線](../../Tools/Build/evidence/EditorWorkflow/ED-WF-01.md) 區分規劃、
實作與已驗收交付；後續證據檔存在後才加入連結。
[ED-WF-05 技術基線](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05.md)記錄已核對的 SDK
生命週期、編譯器所有權及資源隔離缺口。首批 SDK 生命週期正在施工，已有 Windows 限縮證據；
編譯協調、Editor 資源隔離與完整驗收仍待完成，不改變已驗收數。
PR #762 首版 Linux CI 在 runner 改為 Ubuntu 26／GNU 15.2 後未通過：既有 Runtime、Prefab、
場景 session／儲存及反射／比較 GUI fixture 觸發懸空指標警告。修正改用持有資料的具名 fixture，
保留全部斷言、payload 及警告設定。僅於失敗後蒐集完整編譯診斷，保留原 gate 失敗狀態；
仍須重新通過修正版本的完整 Cloud／CI。Editor 資源隔離及整合驗收仍待完成。
後續原生建置已清除這些編譯失敗，但 `201d73e1` 的 hosted display gate 仍未驗收
（231/247 通過、16 失敗）：注入 F5 時同時帶入 Alt，Play 未啟動。
[原生輸入修正](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Input-CI-Failure.md)
改用已驗證的 X11 實體功能鍵代碼，保留全部 Runtime／畫面／期限斷言；仍須重跑修正版原生與
完整 gate，不增加已驗收數。
PR #762 已合併為 `f7537be874d8`，tree 與 `d093841b` 相同。
[Cloud 收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Cloud-d093841b.json)記錄496/496
及五個 gate 均 exit0；[CI 清單](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-CI-d093841b.json)
記錄41項成功檢查／三個 workflow。此基礎修正不代表 ED-WF-05 已驗收，完成度維持4/9。
[ED-WF-02 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-02.md)及[雲端收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-02-Cloud.json)記錄已驗收的 PR #754：42 項 CI 成功、雲端 490/490 與 Shipping 建置通過，保留初次環境／本機畫面擷取失敗。Beads 已安全採用既有歷史並恢復同步；Epic `nexora-whr` 與 `.1`～`.9` 對應連結的 GitHub 工單。
已驗收的 [ED-WF-03 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-03.md)、[雲端收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-03-Cloud.json)及[CI 清單](../../Tools/Build/evidence/EditorWorkflow/ED-WF-03-CI.json)記錄 PR #756：原生 GUI／核心通過、雲端 490/490 與 Shipping、42 項 CI 全數通過，且合併 tree 相同。ED-WF-04 已由 PR #758 驗收。
已驗收的 [ED-WF-04 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-04.md)、
[Cloud 收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-04-Cloud.json)與
[CI 清單](../../Tools/Build/evidence/EditorWorkflow/ED-WF-04-CI.json)記錄 Cloud 495/495、Shipping、
Windows／macOS 原生證據、42 項 CI 全數成功及合併 tree 相同。真實 Zig 保留 32 次成功切換及
224 次預期拒絕。Editor 協調、來源製作／動畫及最後交付仍屬 ED-WF-05、ED-WF-07、ED-WF-08 與 ED-WF-09。
完成度為具備驗收證據及 merged delivery PR 的子工單數除以九；已驗收列使用 `✅`，
待辦及 blocked 項目維持未標記。

## 實作契約

- Build 沿用共用 producer／publisher 與可信 SDK 工具。保存相容的 mode／manifest 設定，
  舊設定維持 SceneOnly。Save／Discard／Cancel 仍明確處理；取消、過期 capture 或工具失敗
  不得覆蓋前次成功成品。
- `Build.commands`、`Build.sources`、`Build.tools` 與工具用 inventory 移至引擎使用者資料區，
  不寫入唯讀 authored 專案或 Runtime 交付目錄；向呼叫端提供 owning build receipt。
  保留驗證後替換、異物／alias 防護及回滾；舊 inventory 成品須先驗證再遷移。
  真正執行需要的 metadata 與授權仍保留。
- 沿用 V3 `save_state`／`load_state`，SDK 加入成對型別化 `saveState`／`loadState`。
  狀態使用有界、版本化資料，不保存原生指標；缺少安全遷移能力時繼續舊世代並回報需重新啟動，
  不得偷偷重設遊戲。
- 保存狀態及切換前，停止新 dispatch 並等待已登記 job／callback 的實際完成。
  候選 host／World／資源副作用須隔離，拒絕候選時回滾；退休舊模組不得刪除已移交的
  Play-owned 狀態。最終 Stop 仍排空並銷毀整個 session；callback／函式指標不得超過模組世代壽命。
- 沿用 watcher 去抖，背景編譯時舊遊戲繼續執行。使用獨立 DLL 世代副本，只接受目前
  project／source／Play 世代；連續修改、Stop、關閉及切換專案均須取消／排空工作。
  Shipping 不帶編譯器或自動腳本重載路徑。
- 保留舊 C／Zig descriptor layout，以能力及 size 檢查擴充來源 API；保留無來源格式與
  NX2S schema 3。Editor／Player 使用相同不可變 Sprite／Atlas 閉包，不依賴 authored
  來源路徑或目前工作目錄；重匯入／重打包發布新世代，已保留畫面仍持有舊資源。
- 完成來源動畫與 pool 發布，不移動 Pivot／World 排序錨點，也不使同一 session 的存活 handle
  失效；跨 session 的過期 handle 仍須拒絕。

## 驗證與交付

真實 GUI Build 覆蓋中文／空白路徑、保存設定、未儲存場景選擇、取消、編譯失敗、過期來源及
重複發布。重開並搬移無來源成品，以一般程序旗標、無參數啟動，要求只有一個可回應遊戲視窗，
沒有 console 或子 launcher，並正常關閉。不能用隱藏 console 的測試代替 GUI 入口。

以真實 Zig library 連續重載至少 32 次，驗證 counter／entity／scene／animation／pool 狀態、
ABI／遷移／啟動失敗、進行中 jobs／callbacks、取消及舊程式碼退休。Sprite 驗證包含裁切、
Pivot、Flip、負縮放、重打包、缺少／損壞閉包、舊 C／Zig consumer 及 feature-OFF。
比較真正原生 Editor Play 與搬移後 Player 像素；helper／CPU 測試不能代替這些路徑。

每批實作執行完整 gate，保留真實退出碼及測試清單：

```sh
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development
```

ABI／linkage／lifetime 改動另跑 `cmake --preset linux-shipping` 及
`cmake --build --preset linux-shipping`。Windows／macOS 原生證據與 Linux 分開，
不可取得的實體乾淨主機／裝置驗收維持 open；失敗及條件 skip 不得被無條件綠燈摘要取代。

每批從最新 remote main 開始，保留同一工作分支及其他工作。檢查 diff／結果後 commit／push、
建立英文 conventional PR；適用驗證及全部 required CI 成功後才 squash merge，不繞過
checks／protection，不合併其他貢獻者的 PR。依證據更新 Epic、子工單及雙語 roadmap。
GitHub README 維持讀者導向，整體 milestone 未變時不加入逐工單歷史。

依 Beads skill 使用固定 CLI 與外部任務路徑。同步失敗則保留資料並記錄精確阻礙，不初始化、
替換或 force 共享歷史；GitHub 工單維持可見，不能將失敗同步回報為成功。

## ✅ Editor Sprite 來源索引與 Play 快照（2026-11-12）

獨立的 ED-WF-07 基礎（`nexora-whr.7.1`）在專案索引與同步／背景重匯入中接納不可變
`.nx2sprite` 資料，保留穩定 sidecar／importer 身分，並擷取精確來源／Atlas 的 cooked／專案
相依閉包。主 Editor Play 透過既有幾何實作解析場景與 pool 的來源參照。真實合成 Editor 像素
驗證 Pivot 替換、負縮放與來源擁有者釋放後的舊快照；詳見[雲端證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceSnapshots-Linux-2026-11-12.md)。
獨立的 catalog／封裝切片（`nexora-whr.6.3`）已於 PR #770 合併。Inspector 來源指定、製作場景
儲存重開、Build 與即時動畫／重載仍待完成。本 supporting 切片不驗收 ED-WF-07；
流程 **5/9**、Editor **0/8**、2D **9/11** 維持不變。

## ✅ 無遊戲模組的主場景 Sprite Play（2026-11-13）

獨立的 `nexora-whr.7.6` 消費端只凍結主場景實際參照的來源 UUID 與 Atlas 閉包，不載入
遊戲程式庫，即可在真正隔離的 Play clone 綁定自有預覽。既有 2D Game 選取、Pause／Step／
Resume 與 Stop 保留不可變來源幾何，並在 World 釋放前退休註冊；無來源的 3D Play 不受
無關且不可用的 2D 資產影響。真實 Editor 原生驗收重複 32 次場次，紅藍遮罩精確一致，
製作畫面／來源位元組未變；CPU 案例另驗證無 callback 擁有權、拒絕與來源／專案釋放後重播。
詳見[契約](../../Apps/Editor/README.md#module-free-main-scene-sprite-play)與
[證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-ModuleFreePlay-Linux-2026-11-13.md)。
已設定套件的 Play／Build、Scene View 與動畫／重載仍未完成，已合併的 Inspector 指定能力
持續保留；不代表 ED-WF-07 驗收。工作流 **5/9**、圖形 Editor **0/8**、2D **9/11** 與實機 gate 不變。

## ✅ 主 Editor module Play 的製作來源參照（2026-11-13）

獨立的 ED-WF-07 host 切片（`nexora-whr.7.4`）將主場景來源 UUID 參照與精確 Runtime／opaque
擷取、專案資產閉包一起凍結，在 module `Create` 前於真正隔離 Play 場景綁定預設來源 Sprite，
啟動前重查製作內容，並在 callback 前拒絕缺失／不支援／過期綁定。既有 gameplay 自建來源與
負縮放 pool 仍受支援；legacy／製作參照／legacy 的混合原生快照驗證 Pivot 重匯入與舊擁有者
保留。詳見[契約](../../Apps/Editor/README.md#authored-source-references-in-module-backed-play)
與[證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-PlayReferences-Linux-2026-11-13.md)。
Scene View、已設定套件的 Play／Build 及動畫／重載仍待完成；獨立合併的
ND2D／Studio 交付已保留。此基礎不驗收 ED-WF-07；流程 **5/9**、Editor **0/8**、2D **9/11** 不變。

## ✅ 主 Editor Sprite 來源參照模型（2026-11-13）

獨立的 ED-WF-07 模型切片（`nexora-whr.7.3`）新增擁有來源／Atlas 的目錄，以及主 Editor
場景包裝中的版本化 UUID 參照。完整選取的指定／清除共用一次 metadata Undo／Redo，保留
無關／不支援的元件，並可隨場景儲存／重開。精確比對專案世代與 Content 修訂，拒絕來源或
Atlas 重匯入前的過期操作；保留的快照持續擁有舊不可變資產。Region 索引避免為每個來源重複
完整驗證 Atlas。詳見[契約](../../Engine/Editor/README.md#main-editor-sprite-source-reference-transactions)
與 [Linux 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceReferences-Linux-2026-11-13.md)。
主 Scene View／已設定套件 Play 轉換與 Build 匯出仍未完成；另一個已認領的 authored
ND2D／Studio 切片不在本次範圍。此基礎不驗收 ED-WF-07；流程 **5/9**、Editor **0/8**、
2D **9/11** 維持不變。

## ✅ ED-WF-06 來源 API 與 Player 驗收

ED-WF-06 的原生驗證（`nexora-whr.6.4`）透過搬移後的 catalog 遊戲，以非對稱原生像素
實測 SDK／C ABI 的來源 Flip 與 Atlas 重排。只替換 Atlas 後，場景及負縮放 pool 的 handle、
作者端 transform 與來源身分維持不變。Development 及嚴格 Shipping 原生驗證已通過；
[證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06-NativeSource-Linux-2026-11-12.md)
保留初次案例／設定失敗。完整回歸 486/486 通過，沒有失敗或跳過；全部 41 項指定版本最新 hosted 檢查通過，PR #772 已合併。
完整[驗收](../../Tools/Build/evidence/EditorWorkflow/ED-WF-06.md)記錄相容消費端、不可變 cooked
閉包、feature-OFF 建置及原生重排。流程為 **5/9（55.6%）**；Editor **0/8**、2D **9/11**
及實機／乾淨主機 gate 維持不變。

## ✅ 主 Editor Sprite 來源 Inspector（2026-11-13）

配套子項 `nexora-whr.7.5` 在實際主 Inspector 提供已匯入來源的選擇與清除。
完整選取共用一次 metadata Undo／Redo，並隨場景包裝存檔／重開；混合、缺失來源／Atlas
與不支援的參照分別呈現，不支援的位元組持續保留。每次操作檢查專案寫入權、停止的 Play、
焦點／復原限制、存活選取與匯入版本。選單自有有界不可變目錄、路徑與節點鍵，過期匯入
或上下文變更會取消操作；控件不執行檔案 IO，也不修改 Runtime Sprite。
實際 1×／2× 控件與 Linux X11 操作驗證批次指派、清除、精確 Undo／Redo、存檔重開及
唯讀專案位元組不變。詳見[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceInspector-Linux-2026-11-13.md)。
模組主 Play 已由獨立的 `nexora-whr.7.4`／PR #776 交付；Scene View、已設定套件 Play、Build、來源動畫／熱重載及
完整 ED-WF-07 仍開放。工作流 **5/9**、圖形 Editor **0/8**、TwoD **9/11** 與實機驗收維持不變。

### ✅ 來源幾何資訊檢視（2026-11-14）

支援工作 `nexora-whr.7.10` 在主 Inspector 新增預設收合的唯讀來源詳細資訊，顯示來源／Atlas UUID、
原始尺寸、左上起算裁切／Atlas 區域、左下起算 Pivot、PPU、邊框及套用實體變換前的來源局部範圍。
共用幾何解析器讀取目前已匯入的不可變資料，不讀寫檔案、不修改編輯資料、不註冊 GPU 紋理。
實際 1×／2× 控制驗證指派與歷史、混合／缺失／不支援參考、重新匯入、唯讀／Play 檢視及內容移除。
Linux X11 驗證非對稱數值、實際呈現畫面、精確 Undo／Redo／儲存／重開及唯讀專案雜湊不變。
完整 Linux **511/511**、Shipping、TwoD-OFF／testing-OFF 與圖形 Monolithic gates 通過；
交付仍須確切提交的 hosted CI。見[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceDetails-Linux-2026-11-14.md)。
工作流程 **5/9**、Editor **0/8**、TwoD **9/11** 不變；完整 ED-WF-07／08、正式動畫／重載與實體／
乾淨主機驗收仍開放。獨立 Clip 目錄工作由 PR #786 負責。

### ✅ 主 Editor 既有 Sprite 來源編輯（2026-11-15）

支援工作 `nexora-whr.7.12` 新增持有草稿的 Content **Edit Sprite source** 對話框與序列化
同 UUID／路徑儲存服務。原始尺寸、左上起算 Trim、左下像素 Pivot、PPU、同 Atlas 穩定區域及
合法完整畫布邊框中繼資料沿用共用驗證器。匯入／磁碟／sidecar 基準、標準普通目錄、單一硬連結
檔案、有界大小寫別名、寫入／復原、焦點、停止 Play 與忙碌狀態限制寫入；先準備正常匯入、
Undo／相依擁有者，再於同父目錄原子替換。GUI 不做檔案 IO；已儲存／拒絕／不確定結果保持區別。
Scene／Runtime 位元組、C／Zig 表格與編碼格式不變。

實際 1×／2× 控制、瞬間失焦拒絕與原生 X11 建立／指派／編輯／儲存，驗證穩定 UUID／sidecar、
相機不變下獨立舊區域／共同區域 Scene 像素、即時 Inspector、檔案移動 Undo／舊不可變擁有者、
取消與逐位元相同的唯讀重開。完整 Linux **527/527**、零失敗／略過、**1895.68 秒**，
Shipping Full、實際 TwoD-OFF／testing-OFF Editor／原生 Player、圖形 Monolithic 控制／原生 gates
通過；交付仍須確切提交的 hosted CI。[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceEditing-Linux-2026-11-15.md)
保留原始失敗、未完成的檢查輪次及測試資料可攜性修正來源。工作流程 **5/9**、Editor **0/8**、
TwoD **9/11** 與完整 ED-WF-07／08 不變／仍開放；Atlas 切片、Clip 作者流程、切片繪製及
實體／乾淨主機 gates 仍獨立，保留其他擁有者的重載與即時來源／pool 發布工作。

交付前已獨立重驗與最新 main PR #793 的整合版本：完整 Linux **528/528**、零失敗／略過、
**2212.45 秒**，包含實際 configured 來源／Atlas 重匯入與來源編輯；Shipping／OFF／Monolithic
來源與 Play 模型／原生／文件 gates 均在凍結的整合來源通過。原基底 **41/41** hosted 檢查保留為
歷史；整合檔案樹仍須新的確切提交 hosted 檢查。保留 PR #793 發布能力；原生重載與完整父里程碑／
實體驗收仍開放。

### ✅ 主 Editor Sprite 來源建立（2026-11-14）

支援工作 `nexora-whr.7.11` 透過實際 Content 右鍵選單與擁有草稿的對話框，從已匯入 Atlas 區域
建立新的標準來源。使用者可設定原始尺寸、左上起算裁切、左下起算像素 Pivot 與 PPU；裁切尺寸
依穩定區域 ID 決定，邊框初始為零。編輯擁有者排他發布新的 UUID／檔案，只匯入該已儲存來源，
保留既有 Content 檢視與歷史。Undo 保留的名稱與大小寫別名在寫入前拒絕；重新命名／移動後已釋放
的路徑會依目前 Content 校正索引並立即匯入。範圍／版本、焦點、寫入權限／復原、停止 Play、忙碌狀態、名稱、
幾何、容量及目的地身分均限制寫入；GUI 繪製不新增檔案 IO。已建立但匯入失敗，以及發布不確定
的結果保持區別。實際 1×／2× 控制與原生 X11 建立／匯入／指派／檢視／儲存／唯讀重開通過，
產生的 UUID／metadata 正確，既有資產不變，唯讀專案所有檔案逐位元相同。
完整 Linux **516/516**、Shipping Full、實際 TwoD-OFF／testing-OFF Editor／原生 Player，
以及圖形 Monolithic 控制／原生 gates 通過；交付仍須確切提交的 hosted CI。
見[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceCreation-Linux-2026-11-14.md)。
工作流程 **5/9**、Editor **0/8**、TwoD **9/11** 不變，完整 ED-WF-07／08 仍開放。
既有來源編輯、Atlas 切片、正式動畫／重載與實體驗收仍為獨立工作；保留其他擁有者的 Clip／重載任務。

## ✅ Studio source 文件持久化（2026-11-13）

配套子項 `nexora-whr.7.2` 加入不可變 source 所有權、ND2D schema-2 持久化，以及來源感知的
Undo／重開／獨立 Play；無來源文件維持 schema 1。Studio 匯入／指派／預覽來源 UUID，Player
在模組 callback 前驗證內嵌 Atlas／source 閉包。實際搬移並刪除來源的套件及原生像素覆蓋
持久化 Pivot／trim 與負縮放。初始測試模組與建置排程失敗，和通過結果分別保留於
[雲端證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-AuthoredSources-Linux-2026-11-13.md)。
獨立 Flip／repack 子項 `nexora-whr.6.4` 已於上節驗收。主 Editor 製作畫面／已設定套件 Play／Build
轉換與來源動畫／熱重載仍未完成。ED-WF-07 尚未驗收；工作流 **5/9**、圖形 Editor
**0/8** 與 2D **9/11** 維持不變。

## ✅ 主場景 Sprite 來源 Build 支援（2026-11-13）

支援工作 `nexora-whr.7.7` 擷取已存檔的來源 UUID 參照，擁有去重後的來源／Atlas 相依資料與
精確發佈基準。含來源遊戲使用 capability 3，無來源 capability 2 位元組保持相容；Runtime 不依賴
選用 TwoD 的型別編碼器。標準 Player 驗證型別相依關係，將重映射後的實際 World 變換繪成 2D 覆蓋層。
CPU 案例涵蓋過期、損壞、缺失相依資料、擁有者生命週期與 additive ID 重映射；實際 Build／SDK Zig、
刪除專案、中文路徑搬移及無參數啟動 Player 驗證完整 Sprite 色彩遮罩，底下的原生 3D 像素保持完全相同。
詳見 [Build 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceBuild-Linux-2026-11-13.md)。
設定式套件 Play 預覽、Scene View、來源動畫／物件池與熱重載仍未完成；保留獨立的無模組 Play 工作。
ED-WF-07 保持開啟。工作流 **5/9**、圖形編輯器 **0/8**、TwoD **9/11** 與實體／乾淨主機驗收不變。

與已獨立交付的無模組 Play（PR #778、main `2e900aa2`）整合後，詳見
[主線整合補充證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceBuild-MainIntegration-Linux-2026-11-13.md)：
完整 **521/521**、零失敗／略過，實際 Development／Shipping 原生像素、Monolithic 與 2D-OFF 組建通過。
原生測試傳輸修正保留相同逾時與全部原有斷言；前輪失敗的完整測試分開保留，父項驗收與里程碑數字不變。

## ✅ 來源動畫 Clip 基礎（2026-11-13）

支援工作 `nexora-whr.8.1` 新增純來源 Clip schema 2，保留舊 Clip 的精確位元組與 C/Zig 表格。
型別匯入／cook／目錄及 Editor 快照驗證完整來源／Atlas 相依；明確 Play／Advance、保留的不可變
Clip 擁有者、ND2D 歷史與 Studio 預覽保留來源身分及樣式。實際 SDK／搬移 Player 的 Development
與 Shipping 像素涵蓋 trim／Pivot、翻轉、負縮放、第二組預載來源／Atlas，以及穩定的來源物件池 handle。
完整 Linux **523/523**、零失敗／略過，Shipping／原生、Monolithic 與 TwoD-OFF 驗證通過。
[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-SourceAnimation-Linux-2026-11-13.md) 分別保留前輪失敗及中斷紀錄。
來源重匯入、32 次實際 Zig 重載／失敗回復、主 Editor 設定式套件 Play 及完整 ED-WF-07／08 仍未完成；
原生 `repacked` 階段只是預載動畫影格的名稱，不代表即時重匯入。工作流 **5/9**、圖形 Editor **0/8**、
TwoD **9/11** 及實體／乾淨主機驗收維持不變。

[編譯器可攜性補充證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-SourceAnimation-Portability-Linux-2026-11-13.md) 記錄修正後完整
**523/523**、零失敗／略過，以及 Development／Shipping 原生、Monolithic 與 TwoD-OFF 驗證。
GNU 15.2 前後對照重現原始 benchmark 暫存值警告，並驗證全部 17 個受影響 C++ 翻譯單元；
測試採明確且有界的整數型別處理 MSVC 警告。警告規則與原生斷言維持不變，初始 `1ec968b6`
的本機成功及 CI 失敗分開保留；完整 ED-WF-07／08 與里程碑數字不變。

`nexora-whr.8.1` 額外原生測試可攜性修正通過523/523及全部12項本機驗證；GCC15.2編譯全部309個
實際TwoD消費端命令版本，包含既有CPU／原生PlayContext測試。[最終原生版本證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-SourceAnimation-NativeContext-Linux-2026-11-13.md)
保留兩份先前已提交紀錄及新增的雲端CI失敗。正式32次來源重載、設定式套件Play及完整ED-WF-07／08
仍未完成；**5/9**、**0/8**、**9/11**維持不變。

## ✅ 獨立來源重載世代基礎（2026-11-13）

支援工作 `nexora-whr.8.2.1` 暫存獨立TwoD World／中繼資料、相機／查詢、動畫時鐘／暫停／樣式、
場景請求／版本及精確物件池handle；配置、准入與重入失敗不改動正式值。實際標準SDK測試通過32次
獨立程式庫替換、128次候選拒絕、舊程式工作租約等待，以及保留錨點的來源UUID重匯入。
完整525/525、Shipping／原生回歸、Monolithic及TwoD-OFF通過；[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-GenerationStaging-Linux-2026-11-13.md)
保留初期失敗與中斷紀錄。這是可重用的獨立TwoD先決基礎；`nexora-whr.8.2` 的正式Player／Editor
服務與原生重載整合仍未完成，完整ED-WF-07／08、**5/9**、**0/8**、**9/11**及實體／乾淨主機驗收不變。

## ✅ 單次UI中的受柵欄保護Scene影像（2026-11-13）

支援工作 `nexora-whr.7.8.1` 提供明確的離屏LDR Scene參照，在單次UI提交內取樣；上傳前拒絕
過期／異表面參照，各視圖資源由完成柵欄保護。實際Linux Development／Shipping各通過12影格、
24次Scene繪製、48個UI命令、兩次縮放及非對稱像素／遮擋檢查；反轉UV的負向對照如預期失敗。
完整527/527、Monolithic與TwoD-OFF通過；[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SceneUiComposition-Linux-2026-11-13.md)
保留初期失敗並區分本機Linux與託管平台檢查。`nexora-whr.7.8` 的正式設定封裝Editor來源Play
仍有保護且未完成；正式重載、完整ED-WF-07／08、Workflow **5/9**、Editor **0/8**、TwoD **9/11**
及實體／乾淨主機驗收維持不變。

## ✅ Editor Sprite 拒絕 Scene 標記（2026-11-14）

支援工作 `nexora-whr.7.8.1` 在既有索引 Sprite 轉譯器附加 ImGui 幾何前，拒絕所有非零 Scene
參照。同一個實際外部測試對舊函式庫失敗、套用正式檢查後通過；完整／不完整標記均拒絕，普通
提交仍有效。新一輪完整 527/527、Shipping 原生、Monolithic 五項及 TwoD-OFF 通過；
[補充證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SceneUiComposition-EditorTag-Linux-2026-11-14.md) 保留原始兩份原生證據與實際重現失敗。
這尚未實作有擁有者的 ImGui Scene 綁定或受保護的封裝來源 Play；`nexora-whr.7.8`、正式重載、
完整 ED-WF-07／08、**5/9**、**0/8**、**9/11** 及實體／乾淨主機驗收仍未完成／維持不變。

## ✅ 設定式套件 Sprite Play（2026-11-14）

支援工作 `nexora-whr.7.8` 將已交付的 Build 目錄與受柵欄保護的 Scene 影像整合進實際主 Editor。
Play 前先准入全部套件 Atlas；候選場景的重映射來源與完整 Scene／Sprite／GUI 單次提交成功後，
才發布 World 並執行初始回呼。唯讀原生驗收重複32次操作，檢查完整裁切／Pivot／鏡像遮罩及未變的
底層3D像素，再以實際 SDK Zig 驗證 Additive／Replace／返回、浮動視窗遮擋、縮放與模組 Start 前
拒絕過期來源。保留無來源的直接3D路徑及 TwoD-OFF 明確拒絕。
[契約](../../Apps/Editor/README.md#configured-packaged-sprite-play) 與
[執行紀錄](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-ConfiguredSpritePlay-Linux-2026-11-14.md)。
前述底層基礎紀錄描述各自交付範圍；設定式 Play 現由本獨立消費端完成。Scene View 製作畫面、
正式來源發布／重載與完整 ED-WF-07／08 仍未完成。Workflow **5/9**、完整圖形 Editor **0/8**、
TwoD **9/11** 及實體／乾淨主機驗收維持不變。

## ✅ 獨立 Player 嚴格重載整合（2026-11-14）

支援子項 `nexora-whr.8.2.2` 已在正式 Player 主機採用隔離 TwoD 世代，包含穩定位址的快取服務
context、唯讀存狀態階段，以及候選／退役世代的外部服務准入。聚焦的正式主機 fixture 通過32次
canonical SDK 替換與128次拒絕；Linux 原生版本在替換、回復與來源重新匯入／重打包過程中，
驗證192次有呈現識別標記的來源／物件池畫面。另一個完整原生 Player 範例行程回歸亦通過。
[所有權契約](../../Apps/TwoDPlayer/README.md#strict-generation-ownership) 區分 fixture 與完整行程範圍。
完整531/531、14本機 gates 與十組實際 GCC15 編譯命令皆通過；交付仍須確切提交的雲端 CI。主程式封裝 Editor／Player
來源動畫／物件池服務、專案監看及完整 ED-WF-07／08 仍開放；Workflow **5/9**、Editor **0/8**、TwoD **9/11** 不變。

[完整本機執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-PlayerGeneration-Linux-2026-11-14.md) 保留初期失敗及完整範圍。

## ✅ 原生 Scene Sprite 來源預覽（2026-11-14）

支援項目 `nexora-whr.7.9` 在實際原生 3D Scene View，以持有已匯入來源／Atlas 的 cutout
平面顯示 Sprite。Trim／Pivot／PPU、鏡像與完整階層矩陣、來源／mesh 聯集精確邊界、透明度
選取，以及一般變形預覽／Undo／Redo 共用相同來源幾何。單一應用 Scene 紋理 owner 持有精確
不可變 Atlas，於幀之間退役被替換的原生紋理。兩個 fence 保護的 LDR 取樣視圖先於同一次
所屬 GUI 提交；保留無來源繪圖及設定式 Play 的發布保護。Scene／Game 畫布仍須不相交，浮動
輔助視窗依 GUI 順序遮擋。
[契約](../../Apps/Editor/README.md#native-scene-sprite-sources) 與
[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SceneSpritePreview-Linux-2026-11-14.md)。
此功能沿用線性 PBR 取樣及 alpha cutoff0.5。部分透明混色、像素精確 2D／XY 編輯、主程式封裝來源
動畫／物件池／發布／reload 與完整 ED-WF-07/08 仍未完成。完整 Linux 510/510、Shipping、實際 TwoD-OFF／noTests 與圖形 Monolithic gates 通過；
交付仍須確切提交的 hosted CI。工作單
**5/9**、圖形 Editor **0/8**、TwoD **9/11** 及實體／乾淨主機驗收維持不變。

## ✅ 主遊戲明確動畫根資產擷取（2026-11-14）

支援工作 `nexora-whr.8.3.1` 在 Build 設定加入明確且不可變的 Clip 根資產、完整擁有的
Clip／來源／Atlas 擷取，以及獨立的遊戲封裝能力版本 4。沒有根資產的設定與能力版本 2／3
封裝維持既有位元組。型別驗證檢查傳統影格區域與完整來源依賴；保留的 Clip 在產生者與
原始檔移除後仍擁有影格 UUID。完整 Linux **536/536**、零失敗／略過、**2184.80 秒**、13 項本機閘門、
Development／Shipping 原生像素、Monolithic 與 TwoD-OFF 明確拒絕均通過。
[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-GameClipRoots-Linux-2026-11-14.md) 記錄所有權與相容性。
交付前仍須精確提交的最新雲端 CI。設定 Editor 來源／動畫／
物件池接合、即時發布與完整 ED-WF-07/08 仍未完成。流程 **5/9**、
Editor **0/8**、TwoD **9/11** 與實體機／乾淨主機驗收維持不變。

## ✅ 圖形化動畫根資產選取（2026-11-14）

支援子任務 `nexora-whr.8.3.2` 透過主編輯器 Build Settings 的 Animation assets 視窗明確加入／移除不可變 Clip 根資產。有界擁有式中繼資料快照與獨立虛擬清單保留失效 UUID，拒絕過期／外來／忙碌／唯讀編輯，維持既有 Build／Run／Stop 位置。實際 1x／2x 控制及原生 X11 儲存／重載／唯讀重開，接上 SceneOnly 與標準 SDK ScriptProject Build；刪除來源並搬移至 Unicode 路徑後，兩種原生模式均驗證 8192 個 Sprite 像素、獨立中央 80x80 的 3D 區域與完整排空。完整 Linux **538/538**、零失敗／跳過、**2273.16 秒**、**13 組本地關卡**、四個實際 GCC15 編譯命令、Shipping 原生、Monolithic 五項 CPU 及實際 TwoD-OFF 拒絕均通過。

[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-BuildClipPicker-Linux-2026-11-14.md) 保留原始失敗與精確範圍；交付前仍須通過確切提交的雲端 CI。設定 Editor 動畫／物件池接合、發布／熱重載與完整 ED-WF-07/08 仍未完成；流程 **5/9**、Editor **0/8**、TwoD **9/11** 及實體／乾淨主機驗收維持不變。

## ✅ 主原生 Game Player 來源服務（2026-11-14）

支援子任務 `nexora-whr.8.3.4` 在同一 Runtime World 提供主原生 Player 的 Sprite／碰撞器狀態及既有可選 2D／來源／動畫／物件池 API。候選中繼資料及原生影格先準備，成功後才發布 World；保留的樣式、動畫時鐘與全域物件池控制代碼能跨 Replace／Additive／卸載存活。缺失來源、過期對映、未完成準備及外來執行緒均拒絕。來源空白／StaticView 表與 C／Zig／套件／素材格式保持相容；不新增自動播放／時鐘、Tilemap 或可變內容註冊。

完整 Linux **540/540**、零失敗／跳過、**2671.41 秒**、**14 組本地關卡**及 **13 個實際 GNU15 編譯命令**均通過。Development／Shipping 標準 SDK 主 Build、刪除來源後搬移至 Unicode 路徑及五階段原生輸入，驗證 **65,536** 個獨立 Sprite／物件池像素、第二次穩定擷取、完全相同的中央 80x80 3D 像素、保留的物件池身分及真正 SDK 關閉。Monolithic 圖形化 Editor／Player 與七項 CPU、實際 TwoD-OFF 拒絕也通過。[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-MainGameServices-Linux-2026-11-14.md) 保留範圍與歷史；交付前仍須確切提交 CI。設定 Editor 接合、來源監看／發布、完整嚴格熱重載／32 次替換及 ED-WF-07/08 仍未完成；流程 **5/9**、Editor **0/8**、TwoD **9/11** 與實體／乾淨主機驗收維持不變。

## ✅ 主 Game／重載 bridge 整合修復（2026-11-14）

支援子任務 `nexora-whr.8.3.5` 恢復合併後的 World 指標綁定、遺失 bridge 狀態複製／分支、共用私有物件池複製、封裝 Sprite 型別驗證、無模組製作預覽及 Scene／Sprite 合成所有權，同時保留新重載掛鉤與啟動觀察。主 Game／場景服務安全拒絕單場景重載操作，維持既有 World／來源／物件池狀態；既有 bridge／世代分支及主 Editor 重載路徑仍受測試涵蓋。私有衝突選項觀察排除隱藏 sizing frame；保留的原生紀錄證實暫時座標錯誤，輸入界限與結果檢查均不變。新的製作綁定先驗證擷取的 Runtime／來源參照，再於 Create 前填入私有 Sprite 資料；Source 回呼共用世代階段限制，父 API 協商保留呼叫端支援的前綴與尾端。Windows Showcase 匯出也恢復讀取既有快照包裝；實際 PowerShell 讀取器／呼叫端回歸保留新輸入、期限與精確場景證據。

整合後完整 Linux **549/549**、零失敗／跳過、**2280.01 秒**、**15 組本地關卡**、**16 個實際 GNU15 編譯命令**及 **10 項 Monolithic CPU** 均通過。Development／Shipping 標準 SDK、刪除來源並搬移至 Unicode 路徑，各驗證 **65,536** 個獨立 Sprite／物件池像素、不變的 3D、穩定控制代碼及真正關閉；既有靜態 Shipping 動畫與實際 TwoD-OFF 拒絕也通過。[執行與整合歷史](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-MainGameIntegration-Linux-2026-11-14.md) 區分原始41項檢查成功、外部合入不同來源後的失敗，以及這次整合修復。交付前仍須通過確切修復提交 CI。完整主 Game 來源監看／重載及 ED-WF-07/08 仍未完成；流程 **5/9**、Editor **0/8**、TwoD **9/11** 及實體／乾淨主機驗收保持不變。

## ✅ 設定式 Editor 主 Game 來源服務（2026-11-15）

支援子任務 `nexora-whr.8.3.6` 將同一 World 的 `GameSpriteSession` 接入設定式 Editor Play。遊戲修改的來源／動畫／物件池值交由既有 Scene／Sprite 合成器繪製；已存檔的 SDK ScriptProject 與手動函式庫路徑取得受世代限制的父／來源 API。明確的擁有者分支／重新綁定參與封裝 World／Runtime 場景重載；候選修改保持私有，退役回呼在清理完畢前持有隔離的 World／資料。C／Zig 配置、編碼及模組圖保持不變。

重點驗證通過32次成功 CPU 替換、160次注入失敗，以及實際唯讀 Editor 的 SDK 編譯／載入；五個完整來源／物件池遮罩共 **5,184 像素**、穩定物件池控制代碼、獨立比對不變的 3D 與製作檔案及原生關閉均通過。完整 Linux **550/550**、零失敗／跳過、**2289.26 秒**、全部 **14 組本地關卡**、**20 個實際 GNU15 命令**、完整 Shipping 及 TwoD-OFF 均通過。Monolithic 圖形 Editor／Player、**11 項 CPU** 及相同標準 SDK 原生 Editor 驗證也通過。交付前須通過中繼資料／文件關卡與不變的自身確切提交 CI；見[執行紀錄](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-EditorGameServices-Linux-2026-11-15.md)。這是原生初始世代驗收，並非32次原生 Zig 替換或素材監看／重新匯入／發布完成。ED-WF-07/08 仍未完成；流程 **5/9**、圖形 Editor **0/8**、TwoD **9/11** 及實體／乾淨主機驗收維持不變。

最後合併前審查發現 host 刪除只清除資料，遺留動畫／查詢狀態。新增失敗測試重現舊 overlap／sweep 快照；改經 bridge 清理後立即使快照失效、Lifecycle 保持輸出不變，重建後無命中，物件池控制代碼／值與合成仍有效。修正後重新執行完整550項／14關卡／GNU15驗證。原始16afa135雖通過41項檢查，仍先轉草稿保留；合併前須取得修正提交的新 CI 並核對已處理的審查討論。

## ✅ 配置式 Editor Sprite／Atlas 重新匯入（2026-11-15）

支援子項 `nexora-whr.8.3.7` 在完整 GUI 畫面成功後，一起發布 Content 來源／Atlas 與相符的原生
貼圖登記；保留同一個 Play World、場景／實體身分、動畫時鐘、樣式與物件池控制代碼／值。
未改變的 Atlas 共用登記；過期、缺失、損壞、容量不足或原生接納失敗均保留既有持有者。
Clip 位元組及資產身分／型別集合保持固定；變更閉包範圍或 Clip 時須重啟 Play。既有無來源／
自訂路徑及 C／Zig、格式、模組圖維持相容，C++ 消費端須重新建置。

修正測試後的完整 Linux **551/551**、零失敗／略過、**2717.30 秒**通過。未變更的實作
保留 `8448806d` 的 **14 項本機關卡**基準，包含完整 Shipping、TwoD-OFF、Monolithic
圖形 Editor／Player 及兩項原生 Editor 案例。**重新建置的 11 項 Monolithic CPU 測試**與
**21 組 GNU 15 編譯／19 個編譯單元**通過，來源範圍分別記錄於下方。真實 Reimport 驗證
**32 輪／70 次發布**、**31,104** 個獨立來源／物件池像素，加上五段場景／動畫流程
（**5,184** 像素）、未改變的3D／作者資料、穩定控制代碼／半幀時鐘、更新後的標準 SDK Build
與保存重開。Development／Shipping SDK Player 各驗證刪除來源後 Unicode 搬移的 **65,536** 像素。
[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-EditorSourceReimport-Linux-2026-11-15.md)
保留原始測試觀測失敗與最終來源的重跑關卡；交付前仍須通過同一提交的雲端 CI。主 Editor
實際32 次 SDK Zig 替換仍保留於8.3.8；聯合 ED-WF-07/08、Workflow **5/9**、圖形 Editor **0/8**、
TwoD **9/11** 及實體／乾淨主機關卡均不變。

Windows MSVC 揭露兩個測試檔中的四處隱式數值轉換。第一次修正的完整 Linux 551 項
於 2680.03 秒通過後，才發現 CLI 的同類警告。兩檔均保持原有判定條件；最終完整
551 項、實際 GNU 15 編譯及重新建置的 Monolithic 11 項通過。全部引擎／原生實作
與 17 個實作／建置／測試檔中的 15 個，維持 `8448806d` 的 14 項關卡基準。
機器紀錄分開標示來源範圍；合併前仍須取得最終提交的新 CI。


## ✅ 設定式 Editor 原生 SDK 原始碼重載（2026-11-15）

支援工作 `nexora-whr.8.3.8` 在舊程式碼退役前，加入完整候選綁定與畫面准入。
實體與隱藏 pool 參照都必須能解析；含來源的已儲存 SDK 替換，必須先成功提交所屬
Scene／Sprite／GUI frame，才能發布 World／模組。最後一刻的 scope／artifact 檢查、
原生準備失敗與例外都保留舊世代。冷啟動／無來源路徑維持原有 dispatch 邊界；
排隊場景操作與來源重新匯入維持既有 owner，沿用同一 Runtime World、場景／來源 API 與 codec。

新的真實 SDK fixture 明確編碼 80 bytes 純量狀態，重新取得暫態服務表，確認發布後第一個
Update，並透過正常 watcher／compiler 處理已儲存編輯。驗證獨立來源／pool 與 3D 像素、
場景／實體／pool 識別、半格動畫時鐘、編譯／準備／還原／reloaded／綁定／Capture 回滾、
真正退役 Linux image finalizer、過期結果與 Stop 取消。CPU regression 先重現無法解析的
實體／隱藏 pool 被准入並執行舊清理，再檢查拒絕與原生失敗時的狀態保留。
整合後完整 Linux **554/554**、零失敗／略過、**4678.26 秒**，全部 **14 項本機 gate**、
**21 組實際 GNU15 編譯／19 個 translation unit**、完整 Shipping SDK Player 及真正 TwoD-OFF
拒絕驗證通過。Monolithic graphical Editor／Player、**11 項 CPU** 及三項真正原生 Editor 案例
全部通過，包含 **33 次真正 SDK 替換**、七種失敗拒絕、五個場景／動畫階段、真正 image
finalizer、過期結果與 Stop 取消。來源重新匯入維持獨立 **32 輪／70 次發布** 證據。
Stop driver 順序失敗與早期基底結果分開保留；修正並整合 PR794 後已重跑完整 gate。
詳見 [執行紀錄](../../Tools/Build/evidence/EditorWorkflow/ED-WF-08-EditorNativeReload-Linux-2026-11-15.md)。
交付前仍須通過同一不可變 own-head CI 與來源／review guard。
聯合 ED-WF-07/08、Workflow **5/9**、graphical Editor **0/8**、TwoD **9/11** 及實機／
clean-host 驗收維持未完成／不變；獨立持有的 standalone reload 工作不受影響。

## ✅ 主編輯器 Atlas 切片與 Clip 編輯（2026-11-16）

主 Content 已支援 Atlas Grid／個別矩形與 Clip 幀、時間、事件、loop、來源、順序編輯，
使用 owning 草稿、獨立草稿 Undo／Redo 與 immutable 預覽。保留 identity 的 typed 發布檢查
目前基線、Ready 相依閉包及 writer／idle／stopped Play，維持 Scene／Runtime 不變。
實際1x／2x UI 與原生編輯／儲存／唯讀重開／Build／刪除來源後搬移 Player 流程均通過。
Modular 與 Monolithic 各完成33次 canonical SDK 真實替換、7次失敗拒絕，以及32回合／70次
reimport 發布；Shipping Full 編輯資產／SDK 流程與實際 TwoD-OFF Editor／Player 拒絕均通過。
完整 Development 執行與明確的原生重跑分別記錄，不宣稱單輪558項全綠。CI 保留原始原生證據，
交付前須通過最終不可變提交的全部適用檢查。
Workflow **5/9**、圖形化 Editor **0/8**、TwoD **9/11**、ED-WF-07／08 及實機／乾淨主機 gate
維持不變／未完成，保留獨立 standalone 重載任務的既有 claim。
詳見[執行證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-AtlasClip-Linux-2026-11-16.md)
與[來源／驗收 receipt](../../Tools/Build/evidence/EditorWorkflow/ED-WF-AtlasClip-Linux-2026-11-16.json)。

PR review followup 補強最大 Clip 閉包成本、無關損壞來源下的可用選項，以及 ID 排序／歷史後的
切片選取；詳見[review 紀錄](../../Tools/Build/evidence/EditorWorkflow/ED-WF-AtlasClip-Review-Linux-2026-11-16.md)。
