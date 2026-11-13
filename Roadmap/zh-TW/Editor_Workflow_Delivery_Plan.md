# Editor 腳本工作流程與 Sprite 整合交付計畫

[English](../en/Editor_Workflow_Delivery_Plan.md) · [Epic #741](https://github.com/jimlee1972/Nexora/issues/741)

更新：2026-11-12。基線：`8c4aa989ee7443cc0b2129d54f0f0f77c73d2b49`。
最初文件發布基底：`67db45d23e631b0e05b42c23716245a34ae996d5`。
ED-WF-04 整合基底：main `a21c1ca2293e4200de8dcaa4b892ffef34687c9c`，包含外觀更新。
驗收快照：**5/9 工單（55.6%）**；ED-WF-01 已由 [PR #751](https://github.com/jimlee1972/Nexora/pull/751)、ED-WF-02 由 [PR #754](https://github.com/jimlee1972/Nexora/pull/754)、ED-WF-03 由 [PR #756](https://github.com/jimlee1972/Nexora/pull/756)、ED-WF-04 由 [PR #758](https://github.com/jimlee1972/Nexora/pull/758) 合併，ED-WF-06 由 [PR #772](https://github.com/jimlee1972/Nexora/pull/772) 及其前置切片完成驗收。
後續已驗收項目及 merged PR／證據記於 Epic。完整圖形化 Editor 驗收維持 **0/8**；
本計畫不自動驗收實體裝置或乾淨主機 gates。

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
| ED-WF-07 | [#748](https://github.com/jimlee1972/Nexora/issues/748) | 06 | 真實 Editor 來源索引／Inspector／Undo／儲存重開／Play／Build／重匯入 | 施工中；不可變來源索引／Play 快照切片 |
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

## ✅ 主 Editor Sprite 來源參照模型（2026-11-13）

獨立的 ED-WF-07 模型切片（`nexora-whr.7.3`）新增擁有來源／Atlas 的目錄，以及主 Editor
場景包裝中的版本化 UUID 參照。完整選取的指定／清除共用一次 metadata Undo／Redo，保留
無關／不支援的元件，並可隨場景儲存／重開。精確比對專案世代與 Content 修訂，拒絕來源或
Atlas 重匯入前的過期操作；保留的快照持續擁有舊不可變資產。Region 索引避免為每個來源重複
完整驗證 Atlas。詳見[契約](../../Engine/Editor/README.md#main-editor-sprite-source-reference-transactions)
與 [Linux 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-07-SourceReferences-Linux-2026-11-13.md)。
Inspector 控件、主 Scene View／Play 轉換與 Build 匯出仍未完成；另一個已認領的 authored
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
