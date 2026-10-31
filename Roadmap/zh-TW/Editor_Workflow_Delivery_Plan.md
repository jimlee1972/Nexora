# Editor 腳本工作流程與 Sprite 整合交付計畫

[English](../en/Editor_Workflow_Delivery_Plan.md) · [Epic #741](https://github.com/jimlee1972/Nexora/issues/741)

更新：2026-10-31。基線：`8c4aa989ee7443cc0b2129d54f0f0f77c73d2b49`。
文件發布基底：目前 main `67db45d23e631b0e05b42c23716245a34ae996d5`。
驗收快照：**1/9 工單（11.1%）**；ED-WF-01 已由 [PR #751](https://github.com/jimlee1972/Nexora/pull/751) 合併。
後續已驗收項目及 merged PR／證據記於 Epic。完整圖形化 Editor 驗收維持 **0/8**；
本計畫不自動驗收實體裝置或乾淨主機 gates。

## 現有基礎與缺口

Editor 已透過共用 producer 提供腳本勾選、manifest、Build 與 Cancel。Runtime 已擁有 V3
模組世代及可選 save/load callbacks；型別化 Zig game adapter 尚未發布遷移 callbacks。
Sprite source 已有不可變 metadata、C++ 來源感知擷取及 NX2S schema 3 場景持久化。
以上屬於基礎，不代表下列完整工作流程已驗收；現有 CLI 與 `.bat` 開發流程持續可用。

共用 CMake/Zig SDK 負責產生入口及 ABI 接線。Authored 專案只維護場景、資產、腳本、
manifest 與設定，不要求使用者維護第二份 SDK 或建置管線。
SortingGroup、九宮格／Tiled、遮罩及進階動畫狀態機不在本批範圍。

## 工單

| ID | GitHub | 相依 | 必須通過的驗收 | 狀態 |
| --- | --- | --- | --- | --- |
| ED-WF-01 | [#742](https://github.com/jimlee1972/Nexora/issues/742) | 無 | 發布雙語計畫、能力基線、工單圖及驗證／證據對應 | ✅ PR #751 |
| ED-WF-02 | [#743](https://github.com/jimlee1972/Nexora/issues/743) | 01 | 保存腳本 Build 設定，真實 GUI Build／Build & Run、取消、進度與診斷 | 施工中，待驗收 |
| ED-WF-03 | [#744](https://github.com/jimlee1972/Nexora/issues/744) | 01 | 無原始碼的 Runtime 成品；開發紀錄與發布 inventory 留在成品外 | 待辦 |
| ED-WF-04 | [#745](https://github.com/jimlee1972/Nexora/issues/745) | 01 | Runtime／Zig 交易式遷移；替換失敗保留舊模組及 World | 待辦 |
| ED-WF-05 | [#746](https://github.com/jimlee1972/Nexora/issues/746) | 02、04 | 背景編譯來源並保留狀態熱重載，不需 Stop／Play | 待辦 |
| ED-WF-06 | [#747](https://github.com/jimlee1972/Nexora/issues/747) | 01 | 相容的 C／Zig 來源 API 與 cooked Player 相依閉包 | 待辦 |
| ED-WF-07 | [#748](https://github.com/jimlee1972/Nexora/issues/748) | 06 | 真實 Editor 來源索引／Inspector／Undo／儲存重開／Play／Build／重匯入 | 待辦 |
| ED-WF-08 | [#749](https://github.com/jimlee1972/Nexora/issues/749) | 04、06、07 | 替換後保留來源動畫錨點、存活 pool 實例與有效 handle | 待辦 |
| ED-WF-09 | [#750](https://github.com/jimlee1972/Nexora/issues/750) | 02、03、05、07、08 | 整合原生驗收及實際解壓驗證的 Windows Editor ZIP | 待辦 |

各工單於 `Tools/Build/evidence/EditorWorkflow/ED-WF-XX.md` 記錄精確指令、結果及限制。
現有 [ED-WF-01 基線](../../Tools/Build/evidence/EditorWorkflow/ED-WF-01.md) 區分規劃、
實作與已驗收交付；後續證據檔存在後才加入連結。
[ED-WF-02 Windows 證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-02.md) 記錄原生通過項目、保留失敗及待執行的雲端／CI gates，不代表工單已驗收。
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
