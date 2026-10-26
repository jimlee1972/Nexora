# ED-M0 至 ED-M7 無障礙交接

更新：2026-10-27。狀態：✅ 交接盤點已交付；semantic bridge 與 target-host 驗收仍待完成。
範圍為 [ImGui 整合計畫](Editor_ImGui_Integration_Plan.md) 的 shell、Hierarchy、Console 與 recovery
邊界。本盤點不實作 screen-reader、不認證鍵盤操作，也不授權新增 dependency。

## 既有 identity 與缺少的語意資料

權威 [ProductShell 清單](../../Engine/Editor/src/EditorWorkspace.cpp) 只提供
`PanelDescriptor{id,title}`。目前 panel ID 與英文標題如下：

| Stable panel ID | 目前標題 | 未來仍缺的語意 role／資料 |
| --- | --- | --- |
| `nexora.project` | Project | Region；project identity／access／state 與可用操作 |
| `nexora.hierarchy` | Hierarchy | Tree；父子關係、selected／expanded、階層、item position／count |
| `nexora.scene` | Scene | Region／canvas；selection、tool／mode、camera 與 gesture 的鍵盤替代路徑 |
| `nexora.game` | Game | Region／canvas；Play state 與明確的 input-capture owner |
| `nexora.inspector` | Inspector | Region／property group；名稱、role、typed value／unit／range、read-only／invalid |
| `nexora.content` | Content | Tree／list；asset identity、selection、loading／error 與可用操作 |
| `nexora.console` | Console | Log；severity、source、message／operation identity 與公告政策 |
| `nexora.profiler` | Profiler | Region／chart；measurement source、unit、availability 與文字替代 |

後續 milestone 的 panel 已列於目前清單；盤點不會把它們加入 ED-M0 scope，也不代表其 milestone
完成。Host window label 使用 `###stable.id`；visible label、dock-node number 或 ImGui hash 都不是
portable accessibility identity。

[Host](../../Engine/EditorImGui/src/EditorImGui.cpp) 目前路由這些 scene command string：
`editor.scene.save`、`undo`、`redo`、`create`、`rename`、`delete`、`copy`、`cut`、`paste`、`duplicate`
與 `select-all`（每個 suffix 都接於 `editor.scene.`）。`ProductShell::RouteCommand` 只檢查
`editor.` prefix 並保存 `LastCommand`；它不是 action registry、permission system、native accessibility
provider 或可執行的 command dispatcher。Bridge 仍須由各 owner 提供明確 label、role、shortcut、
enabled／busy state、current scope 與具 admission 的 handler，不得憑此 prefix check 執行任意字串。

Recovery 使用 host-local popup identity `editor.recovery`，標題／摘要保留文字
"Recover workspace"／"A recovery journal is available."。Recover 與 Discard recovery data
現為圖示按鈕，hover tooltip 提供原功能名稱，停用控制項也可查看；既有 ImGui identity 與 admission
保留。Tooltip 不是已發布的 semantic label／action API 或 native accessibility bridge。
[圖示呈現驗證紀錄](../../Tools/Build/evidence/EditorIconsTransform-Linux-2026-10-27.md) 記錄 Linux 驗證；screen-reader 與 target-host
驗收仍待完成。未來如 `editor.recovery.recover`／`editor.recovery.discard`
只是設計提案，不是既有 registered command。必要 role 為 modal dialog、summary、button 與 error／status
region；破壞性語意、modal ownership、busy／disabled state 都須明列。

## 待設計的 snapshot、ownership 與 action 邊界

Application／window owner thread 已序列化 ImGui 與 document 操作。未來 bridge 應消費由該 owner
發布、擁有 label／value／bounds／state 副本的 semantic snapshot。Platform callback 應送出有界、
擁有資料的 action request，回 owner 做 admission；不得在 OS callback thread 呼叫 ImGui 或修改
文件。本交接沒有新增 semantic snapshot API 或跨執行緒 queue。

Scope 應結合 stable panel／control identity、current project identity 與 document lifetime。
[SceneDocument::NodeKey](../../Engine/Editor/include/Nexora/Editor/EditorWorkspace.h) 已包含 entity ID、
entity generation、document generation；`NodeView::name` 是借用的 string view。不得發布或保留該 view、
widget pointer、row index、native GPU handle 或 ECS pointer。執行 action 時須重新檢查 node／project／
document generation 與當前權限，包含 read-only、recovery、focus、modal、Play gate。Scope 關閉或替換
必須移除舊 node 並撤銷排隊 request。虛擬化 row 應取 model identity，設計有界 reveal／query，不能只
列出當前 ImGui 已畫的 item。

## 鍵盤、焦點與 failure 交接

已啟用 keyboard navigation，OS-level ImGui multi-viewport 保持關閉。既有 Hierarchy
Up／Down／Home／End、range selection 與 Left／Right expansion 詳見
[host contract](../../Engine/EditorImGui/README.md#hierarchy-keyboard-navigation)。文字編輯、失焦、
held pointer／gesture、Game capture 與 modal gate 可刻意抑制命令；bridge 應回報 availability／focus，
不得繞過 guard。

目前 recovery 自動演練用 Tab／Enter 執行 Recover、兩次 Tab／Enter 執行 Discard。這只證明有界的
鍵盤路徑，不證明 native focus tree 或通用 tab order。Target-host 驗收仍須觀察 visible focus、
Shift+Tab、modal focus containment、docking 後一般 shell／panel traversal，以及 close／recovery／error
後的 focus restoration。建議語意順序為先公告 dialog title／summary，再暴露 error，然後列出非破壞性
Recover 與破壞性 Discard。Default focus 本身不得執行 Discard。Docked shell 空間／順序變化應由未來
snapshot 反映；本文不固定全域順序。

`ApplyRecoveryChoice` 只委派 `ProjectWorkspace::RecoverWorkspace`／`DiscardRecovery`。
Failure 保留 modal／error 與 journal；success 由 `TakeRecoveryChoice` 消費一次。未來 action 必須
沿用該邊界與 duplicate-submit guard。Error 應以可處理的 context 公告，允許 retry 與安全的 native
close policy，不得捏造 success、刪除 journal 或默選 Discard。該政策仍需實機驗證；目前也缺少
已發布的 dedicated semantic safe-exit action。

Native input／control 穩定性仍待完成：[目前 Linux 驗證紀錄](../../Tools/Build/evidence/EditorEDM0-EvidenceAdmission-Linux-2026-10-20.md)
保留四項未改動 workflow 的失敗與其後成功的 serial 重跑。Beads `nexora-nqn.15` 追蹤診斷；
重跑不代表已修正時序問題，也不認證實體鍵盤操作。

Live-region 需求：新的 recovery error 依 operation／變化公告一次，接著公告 recovery success、modal
結束與 focus restoration。Console entry 需要 stable message identity、severity、有界 backlog、可選擇且
合併重複的公告政策，不得每個 rendered frame 都朗讀。重複且未改變的 error 不應淹沒語音。
Progress、unavailable value、read-only 與 focus change 需要明確 semantic state，不能只靠顏色。
目前 host 沒有 platform announcement／event channel。

## Platform 候選方案與待驗收項目

| Host | 候選 bridge | 必要 target-host 證據 |
| --- | --- | --- |
| Windows | UI Automation provider／pattern | Inspect／Narrator 或其他已安裝 reader；invoke／selection／value／focus／live event、DPI／IME、teardown |
| Linux | AT-SPI bridge | 有已安裝 reader（如 Orca）的真實 desktop；focus、tree／reveal、action 與 error 公告 |
| macOS | NSAccessibility／AX bridge | 宣告支援的 Editor host 上執行 VoiceOver 與 AX inspection；role、action、focus、lifecycle |

這些是候選 platform interface，不是已選定 implementation。Secondary semantic library 可另案評估，
新增 dependency 必須經專案自己的 ADR／review。Plugin 應透過未來明確 contract 提供核准的 semantic
資料／action，不得 inspect ImGui widget tree 或取得 native handle 來自行合成。

ED-M7 仍缺 snapshot／action contract、role／value coverage、有界虛擬化 query、platform adapter／event、
stale-scope／failure 自動測試與真實 assistive-technology session。證據須記錄確切 source、host／reader
version、keyboard path 與 artifact。既有 Xvfb、synthetic IME 與 keyboard test 不認證 speech output、
reader focus、真實 IME 或 physical display。目前仍只能宣稱基本可用鍵盤操作的 ED-M0 shell；
screen-reader support 尚未實作，target-host 驗收保持 open。
