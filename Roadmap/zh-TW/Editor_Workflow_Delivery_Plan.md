# Editor 腳本工作流程與 Sprite 整合交付計畫

[English](../en/Editor_Workflow_Delivery_Plan.md) · [Epic #741](https://github.com/jimlee1972/Nexora/issues/741)

更新：2026-11-01。基線：`8c4aa989ee7443cc0b2129d54f0f0f77c73d2b49`。
最初文件發布基底：`67db45d23e631b0e05b42c23716245a34ae996d5`。
ED-WF-04 整合基底：main `a21c1ca2293e4200de8dcaa4b892ffef34687c9c`，包含外觀更新。
驗收快照：**4/9 工單（44.4%）**；ED-WF-01 已由 [PR #751](https://github.com/jimlee1972/Nexora/pull/751)、ED-WF-02 由 [PR #754](https://github.com/jimlee1972/Nexora/pull/754)、ED-WF-03 由 [PR #756](https://github.com/jimlee1972/Nexora/pull/756)、ED-WF-04 由 [PR #758](https://github.com/jimlee1972/Nexora/pull/758) 合併。
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
| ED-WF-06 | [#747](https://github.com/jimlee1972/Nexora/issues/747) | 01 | 相容的 C／Zig 來源 API 與 cooked Player 相依閉包 | 待辦 |
| ED-WF-07 | [#748](https://github.com/jimlee1972/Nexora/issues/748) | 06 | 真實 Editor 來源索引／Inspector／Undo／儲存重開／Play／Build／重匯入 | 待辦 |
| ED-WF-08 | [#749](https://github.com/jimlee1972/Nexora/issues/749) | 04、06、07 | 替換後保留來源動畫錨點、存活 pool 實例與有效 handle | 待辦 |
| ED-WF-09 | [#750](https://github.com/jimlee1972/Nexora/issues/750) | 02、03、05、07、08 | 整合原生驗收及實際解壓驗證的 Windows Editor ZIP | 待辦 |

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
224 次預期拒絕。Editor 協調、Sprite 流程及最後交付仍屬 ED-WF-05～ED-WF-09。
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

本機未提交的後續整合已串接實際 Play 2D 資源分支與場景服務配額；32 次交接保留動畫時間、
物件池 handle、輸入邊緣狀態與貼圖識別。此為 Windows 程序範圍證據，未包含於 PR762；
主程式交易與 watcher 結果准入、實際 Editor 腳本修改及完整精確版本驗證仍待完成，不調整完成度。

[Editor 主交易檢查點](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Transaction-Windows.md)
加入裸 World／設定專案／2D 的嚴格世代交易。14個原生 target 建置、23項 Windows 範圍測試
及兩次真實無參數 GUI 啟動通過；主程式 C++ fixture 記錄96次成功交接／576次預期拒絕。
實際腳本修改的 watcher／compiler 准入與完整精確版本 Cloud／Shipping／CI 仍待完成；
ED-WF-05 尚未驗收，完成度維持4/9。

[啟動診斷追蹤](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Startup-Diagnostic.md)
保留 PR763 Windows 打包啟動失敗，加入有界的首幀紀錄。本機兩次無參數啟動仍通過；
CI 根因與精確版本完整驗證待完成。此為診斷檢查點，尚未驗收修復，完成度維持4/9。

[即時腳本檢查點](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05.md#live-script-workflow-checkpoint-unaccepted)
已將實際設定式 Play 接到背景內容觀察、共用編譯入口與保留狀態的准入交易。
本機 SDK／建置／協調器回歸3/3通過，真實 Editor 一次來源修改與替換通過。
加強的32次原生驗收仍在執行，啟動 CI 仍失敗；精確版本完整驗證及其他情境待完成，
驗收維持4/9、Editor0/8。

後續證據：本機32次嘗試在11次替換後因 Editor 提前退出而失敗，不能驗收。
0f901cb3 的 Cloud 也為 FAIL（498/499，按鍵釋放／pose 觀察失敗）。
已將實際解壓 ZIP 的 Windows32次驗收接到目前施工分支的自動包裝及手動 workflow；原輸入／啟動期限保留。
見[目前證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05.md)，完成度不變。

操作人員已確認：本機11次替換後是使用者手動關閉 Editor，分類為使用者中斷，
不是產品崩潰；仍未滿足32次驗收。隔離 Windows CI 驗收待完成，獨立的 Cloud 按鍵失敗仍待處理。

[啟動工作區／可見性收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Startup-Fit-Windows.json)
記錄 Windows 原生啟動／品牌／入口5/5通過（26.58秒）。隱藏主視窗先適配螢幕工作區；
測試自己的啟動器建立前景父程序情境，不替代 Editor，不放寬像素／回應／NoActivate／期限。
新精確版本 CI／Cloud 與解壓 ZIP32次仍待驗收，工單與里程碑計數不變。


`51bd8a16` 的雲端 Windows 封裝已通過原啟動 gate 與實際解壓 ZIP 的32次重載：計數0→27512、保留 Scene1／entity4、實際 Ctrl+P 與等 mtime 修改、觀察到的編譯子程序已排空、正常 exit0 且所屬 Job 程序0。下載產物雜湊與 ZIP CRC 已核對，見[封裝收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Package-51bd8a16.json)。這是 Windows 範圍證據，非最終驗收。完整 Cloud 重驗仍為 FAIL495/499，四項 Prefab GUI 失敗；Development／Shipping 建置命令皆 exit0，原中鍵案例則通過。另有四個 macOS／iOS 工作遭 abandoned／cancelled，造成兩個 CI 總 gate 失敗。不合併、不增加已驗收數。
補強的原生 harness 要求編譯、狀態還原、候選初始化及候選啟動修改 World 後的失敗，都保留舊 dispatch／狀態／handle，再驗證正確腳本恢復。首次本機嘗試已觀察四次拒絕，但恢復完成前失去 runner，沒有終態 exit／收據，仍算未完成；新增 checkpoint 只保存部分觀察，不宣稱 PASS。更新後的 harness 與發布驗證仍待通過。

下一份準備狀態修正讓 Linux 主視窗在首幀完成前維持隱藏，X11 取得視窗須在原15秒預算內找到可見視窗。原生 source-apply 的像素等待共用外層10秒手勢期限，失敗 fixture 也會保留。尚未驗收，也不宣稱這已證明四項 Cloud 失敗的完整根因。Windows 原生測試共用既有輸入桌面的明確選取與每次實際啟動前的自有前景啟動器，不對其他視窗送鍵。首次解壓 ZIP 故障嘗試因前景不屬於測試而拒絕開始 Play，本機啟動重驗另有遮蔽失敗，均保留。未採用 LP worker 數候選；該歷史完整 CTest 仍為 FAIL493/499。

整合納入上游 main `1df6ab357da69ee3257097a9b5176695b7c96b9a`（PR #765 authored Scene V1）。GameplayBridge 同時保留 catalog 實體路由與既有 World 指標重新綁定契約。Catalog bridge 對單場景 fork／rebind 會無寫入拒絕，新增回歸檢查保留 World／primary／revision／API 狀態；不宣稱已完成 catalog 熱重載，該部分仍屬後續整合工作。合併後原生重建／範圍測試與精確版本 Cloud／Shipping／CI 仍須通過。

整合後 Windows checkpoint：9個目標重建全部 exit0，6項 bridge／catalog／compiler／proxy／reload／packaged Play 範圍 CTest 全數通過（exit0、5.64秒）。見[收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Main-Integration-Windows.json)。不代表 GUI／完整 gate 驗收或增加完成數；新的精確版本 Cloud／Shipping／CI 仍須通過。

8870雲端 Windows 建置與原五項啟動檢查通過，但封裝驗證在32次重載前的啟動器前景條件
被拒絕；完整 job log 與73,590-byte失敗封存保留。後續啟動器在工作區內以自有 Button 的
內部座標、有限訊息確認，以及拒絕直接啟用時的受控原生點擊／放開建立前景。本機三次
啟動器獨立原生試驗通過、沒有殘留視窗，不代表新的 Editor 驗收。Linux hosted display
另因 packaged Play 的兩處私有搜尋仍選到未顯示 Editor 而觸發 X_SetInputFocus BadMatch，
現於原預算內都要求 onlyvisible。見[啟動器／失敗收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Launcher-Windows.json)。
精確版本 Editor／Cloud／Shipping／完整CI仍待通過，不增加已驗收數。

進行中的8870 Cloud另有兩項Showcase失敗：關閉陰影的像素比較，以及原30秒虛擬顯示
子程序期限。範圍修正保留逾時部分輸出、拒絕驗收的啟動收據，以及已排空／回收的Xvfb
診斷；失敗的互動測試目錄、直接寫檔的子程序輸出與輸入命令紀錄亦保留。
13項可攜證據規則測試通過。此為證據收集修正，不宣稱原生失敗已解決，也未改動
期限或斷言。完整gate終態與精確版本重驗仍待完成。
見[診斷證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05.md#linux-showcase-failure-evidence-follow-up-unaccepted)。
同一Cloud後續另回報原game-build worker與script-build CTest期限失敗，保留測試目錄
供診斷，不豁免逾時。

精確885693db的hosted Linux Editor CI已通過250/250項（1406.98秒），包含先前
Prefab／滑鼠釋放／packaged Play失敗案例與兩項SDK建置；範圍原生Player4/4、Editor
validation3/3、renderer20/20亦通過。下載封存的CRC與SHA-256已核對。
見[範圍收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Linux-CI-885693db.json)。
此結果不替代完整Codex Cloud／Shipping或Windows封裝驗收；完成度維持4/9。

885693db解壓後的Windows ZIP已通過32次原生來源替換、四項真實失敗／回滾、正確
恢復與Stop／編譯程序排空（Editor exit0、所屬Job0）。Scene1／entity4維持，計數
0→32570→37300。搬移與無參數Editor／遊戲啟動、準備畫面順序／回應及無console檢查
通過。兩層ZIP的CRC、封存SHA-256與全部22697筆inventory檔案雜湊均已核對；公開根目錄
只有一個Editor執行檔，沒有來源checkout。
見[Windows收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Package-885693db.json)。
完整Cloud／Shipping／CI、Sprite整合及最終交付仍未完成；維持4/9。

精確885693db的42項CI與4個workflow現已全數成功，見
[完整CI收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-CI-885693db.json)。
獨立的原8870 Cloud已有至少10項失敗，仍須保存完整終態／Shipping並重驗精確版本。
尚不合併或增加已驗收數。

原8870 Cloud已封存為FAIL491/502、11項失敗、零跳過／重試；Development／Shipping
四個configure／build命令均exit0。見[完整收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Cloud-8870d63e.json)。
SDK修正透過共用CMake入口移除每代Zig建置程式的重編譯，保留既有入口／測試、各模組
最佳化／target、compiler-rt與私有快取。Windows範圍重建及原4項案例通過；拆分後的
原腳本案例與腳本測試拒絕案例2/2通過，未延長原期限。
見[SDK證據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-SDK-Direct-Windows.json)。
最新main整合及精確版本完整Cloud／Shipping／CI／原生封裝重驗仍待完成；維持4/9。

已整合mainb1671eaa當機日誌與SDK修正；兩次狀態保存呼叫同時保留capture保護與診斷
情境。Windows選定14個目標重建成功，15項範圍原生CTest全通過（658.98秒）。
見[整合收據](../../Tools/Build/evidence/EditorWorkflow/ED-WF-05-Crash-Integration-Windows.json)。
精確版本完整Cloud／Shipping／CI與更新後Windows解壓／原生封裝驗收仍須通過；維持4/9。
