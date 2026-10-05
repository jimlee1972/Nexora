# Nexora 圖形化 Editor Roadmap

> 版本：v1.3｜狀態：AI 可執行交付計畫｜更新：2026-10-06

> **進度：0%**（ED-M0～ED-M7 尚無任一 milestone 通過圖形化 Editor 驗收；
> 已完成的 Runtime/Editor SDK 前置不向上取整為 Editor milestone。）

**已完成前置：** ✅ reflection metadata；✅ command/undo data model；✅ prefab override/rebase；
✅ isolated PIE session；✅ dynamic plugin ABI gate；✅ standalone process 與 portable
workspace/document core。**待辦：** 其餘 graphical view、authoring workflow、target-host 驗收
與 production hardening。

所需主機不可用時，先暫緩純平台驗收，繼續獨立實作與自動化驗證。未驗證的證據列保持待辦，
對應 milestone 仍須通過主機 gate 才能標記已驗收。

✅ Linux 自動化 Vulkan 驗證現會拒絕 exit-zero error，已涵蓋 attachment 同步與 native RHI
shader feature（[證據](../../Tools/Build/evidence/EditorEDM0-VulkanValidation-2026-10-06.md)）。ED-M0 仍待 target-host 與 workflow gate。

### Repository 完成度稽核（2026-10-02）

本稽核明確區分「已打勾的 implementation foundation」與「已驗收的 graphical milestone」。Source
與 contract test 能確認下列已存在的 foundation；目前沒有任何 ED milestone 同時通過完整 automated
與 target-host gate，因此整體圖形化驗收仍是 **0/8（0%）**。

| Scope | Repository 證據 | 已驗收 |
| --- | --- | :---: |
| ED-M0 shell foundation | Standalone process、optional ImGui host、stable panel、initial docking、input/DPI/IME forwarding、live Hierarchy、recovery modal、retained native GPU rendering、project layout persistence 與 recovery failure contract 已存在。Linux 虛擬顯示 recovery 現會以 durable seeded journal 驗證 SIGKILL、已提交 workspace 不變、重新取得 writer lease，以及 keyboard-only Recover／Discard；實體顯示器 Linux 與 Windows DPI／IME host evidence 仍待完成；已記錄 bounded Windows/DX12 開發機 shell smoke。 | [ ] |
| ED-M1 project/assets | Portable create/open、schema upgrade、single-writer／read-only access、recent-project state、deterministic indexing/search、persistent sidecar UUID、virtualized Content Browser state、breadcrumb／selection、transactional mutation、typed generation-safe drag payload、dependency／cycle inspection、transactional reimport、watcher debounce 與 dirty-conflict decision 已存在。Native shell 已顯示 project 狀態、提供圖形化 create/open/recent selector、將真實 index 綁到圖形化 Content panel 與可回復的 project-local mutation，執行具 bounded progress 與 structured diagnostic 的 cancellable background import/reimport、顯示 dependency cycle，並提供阻塞式 reload／keep／compare conflict UX；實體顯示／Windows workflow 驗收仍待完成。 | [ ] |
| ED-M2 scene authoring | Portable hierarchy/selection、reparent、兄弟重新排序（可復原的 Hierarchy 拖曳模型）、multi-selection、clipboard、transform transaction、undo、atomic save/reload 已存在，另有與 UI 無關的 pick ray、AABB picking、軸向拖曳、snapping 與 viewport resize hysteresis 數學，以及 Unity 式的移動／旋轉／縮放 gizmo 數學（含 Global／Local 軸、Pivot／Center、父物件、負縮放規則與多選最上層判定）。圖形化 Hierarchy 現已有 parent-aware expandable tree、filter、以 generation 為 key 的 expansion／selection、可見列裁切提交、可復原 rename、兄弟排序與 cycle-safe reparent，且會拒絕 stale entity／document generation。Docked Inspector 已提供 generation-safe 的 position、Euler 度數（quaternion storage）與 scale 單選／mixed-value 多選編輯，並具 atomic Runtime validation 與單步 undo。完整的 authored-mesh Scene View、reflected Inspector、material shader workflow、camera authoring 與 missing-plugin 還原仍待完成；有界唯讀 opaque component Inspector 與 persistence 已實作；原生代理預覽已提供 Move／Rotate／Scale 把手。 | [ ] |
| ED-M3 PIE/debugging | Portable `PlaySession`、structured bounded Console records、owning inspection snapshots、debugger adapter/pause reasons、failure recovery 與 deterministic transform conflict rejection 已存在。圖形化 Console 會顯示有界紀錄與 Editor 診斷；docked Game panel 可控制隔離 clone 並顯示複製的檢視資料。有界原生 camera／OBJ Game View 已實作；完整材質／多個 canvas、完整 gameplay 服務／擴充 input、完整 log 路由與 native debugger integration 仍待完成。 | [ ] |
| ED-M4 prefab/scenes | Portable override diff/revert/apply、variant 與 nested rebase 已存在。Graphical prefab/multi-scene、migration/recovery、conflict 與 source-control workflow 仍待完成。 | [ ] |
| ED-M5 specialized tools | Stable capability ID 與誠實的 implemented/read-only/unavailable state 已存在。尚無 production graphical reference tool 通過 edit-preview-save 驗收。 | [ ] |
| ED-M6 build/profile/extensions | Portable build manifest/checksum 與有界的 monotonic profile capture 已存在。Docked Profiler 可繪出即時 Editor frame processing 時間，具暫停／清除與丟棄數。Build/deploy/log、GPU／memory profiling、export 與 plugin manager workflow 仍待完成。 | [ ] |
| ED-M7 hardening | Portable virtual hierarchy、trust/signature policy 與 telemetry opt-in test 已存在。Graphical scale/soak、migration/corruption、keyboard 與 screen-reader audit 仍待完成。 | [ ] |

Focused [Dear ImGui 計畫](Editor_ImGui_Integration_Plan.md) 已列出細部打勾的 ED-M0 foundation。只有
具備 §14 milestone Definition of Done 的證據才能勾選上表；不得用已實作 prerequisite 數量推算
milestone 進度。

## 1. 產品願景

建立類似 Unity/Unreal 工作流的獨立 `NexoraEditor`，而不是宣稱複製其全部功能。第一條 production path 包含 Project Browser、Hierarchy、Scene View、Game View、Inspector、Content Browser、Console、Profiler、Gizmo、Undo/Redo、Play-in-Editor（PIE）、import/cook/build。Editor 是 Runtime 的 client，不得成為遊戲執行必要相依。

## 2. 架構邊界

```text
NexoraEditor (tool process)
  UI shell + docking + commands + workspace persistence
  Editor document/model + selection + transactions
  Scene/Game render surfaces + gizmos
  Asset tools + build/cook/package frontend
          |
          v public Runtime/Renderer/Asset/Editor SDK APIs
  Editor World -- snapshot/clone --> Play World
```

- Editor-only metadata 不進 Shipping runtime component。
- UI selection 使用 stable IDs/handles，不保存可 relocation pointer。
- 所有修改走 command/transaction；property edit、gizmo drag、reparent、multi-edit 都可 undo。
- PIE 複製/序列化隔離的 Play World；停止時預設丟棄變更，明確選擇才 apply back。
- Engine Core 不依賴 UI framework；UI backend 可替換，第一階段選型的 docking、IME、accessibility、多 viewport 與維護性評估記錄在 [ADR-0001](ADR-0001-Editor-UI-Framework.md)。
- 外掛只能經 versioned Editor SDK 註冊 panel、command、importer 與 inspector，不接觸私有 singleton。

## 3. Milestones

Milestone 必須嚴格依 **ED-M0 → ED-M1 → ED-M2** 交付。ED-M0 尚未確立產品與 UX contract
前，不開始 production widget 實作。尤其 Editor shell 依賴 public window/swapchain path，
不得只為顯示 UI 而另造暫時性的 private presentation path。

### ED-M0 — Product shell 與 UX contract

確立 supported OS、project/workspace format、stable panel IDs、command/shortcut routing、docking、
theme、DPI、IME、accessibility 與 crash recovery；透過 ADR 與聚焦 prototype 選擇 UI framework。
Wireframe 或孤立的 widget demo 不構成本 milestone 完成。

- ✅ 已實作 standalone `NexoraEditor` process、versioned project/workspace format、stable panel
  ID、command namespace、atomic workspace replacement 與 recovery journal。
- ✅ UI framework ADR：[ADR-0001](ADR-0001-Editor-UI-Framework.md) 選定 Dear ImGui
  （docking/multi-viewport），透過 `Nexora::RHI` 渲染而非另帶一套視窗系統，並點名 ED-M7 仍要展開
  的無障礙缺口。這份 ADR 只確定了 framework 選型，不等於圖形化 docking、theme、DPI、IME、
  accessibility 或 crash UX 已完成。
- ✅ Feature-gated Dear ImGui host 已實作
  [Editor_ImGui_Integration_Plan.md](Editor_ImGui_Integration_Plan.md) 所述的 portable docking、
  theme／DPI、input／IME、live panel、recovery UX 與 accessibility-direction contract。
- ✅ 在 Vulkan host 上，圖形化 process 會將 ImGui draw data composite 至 public
  `RenderSurface` 已 acquire 的 swapchain backbuffer；Linux 與 Windows window event 也會正規化
  完整的 Editor 按鍵／modifier 集合。
- ✅ WP0 可重現性盤點：graphical OFF 71/71、ON 加 Slang 115/115 通過且無 skipped，
  Shipping engine build 通過。[Linux 證據](../../Tools/Build/evidence/EditorEDM0-Linux-2026-10-05.md)
  與 focused plan 現已提供剩餘 target-host checklist；ED-M0 維持 open。
- 待驗收：具真實 display 的 Linux visual／input／recovery 證據，以及 Windows DPI／IME 證據；
  已記錄 bounded Windows/DX12 開發機 shell smoke，但完整 target-host gate 通過之前 ED-M0
  仍維持 open。

- ✅ 原生 client-pixel pointer event 現會先依當前 frame DPI 轉成 UI 邏輯座標，再做 hit test。
  scale 改變會重新投影快取位置，失焦會清除快取，DPI 改變會取消中斷的 Scene gesture。測試涵蓋
  100／125／150／175／200%、小數／負座標、事件排序、靜止 pointer、無效 scale fallback，
  以及 200% Apply dialog 點擊；render deferred／zero-extent frame 也會轉送 gameplay 按鍵釋放與失焦，
  不需 GUI frame 或 Play tick；target-host 實體顯示器 DPI 證據仍待完成。

### ED-M1 — Project 與 Asset workspace

建立、開啟與升級 project；Content Browser 支援 search/filter、folder/UUID、drag/drop、import
status、dependency 檢視與 reimport；background import 必須提供取消、進度與可採取行動的錯誤，
並產生 deterministic artifact。

- ✅ Typed OBJ reimport 現 staging 不可變 geometry 及 hash，通過 project／asset／source／settings／
  dependency 與 128 MiB mesh 預算後，透過 live content model 原子發布。測試驗證同步／背景更新、
  穩定 resource ID、舊 owning snapshot、失敗／取消／過期／超限保留、重新命名 Undo 與刪除／Undo
  保留最新幾何，以及預算回復。單調 content revision 會在原生繪製前更新 mesh catalog；rendering／
  picking 不讀取來源檔案。

- ✅ 背景 workspace 匯入現保留有界、不可變的已三角化 OBJ CPU 幾何，包含 UV、指定／產生的
  法線、index 及局部 bounds。Portable 測試涵蓋格式錯誤／溢位、取消、vertex 上限、移動／重開
  後 UUID／hash 穩定性、owning snapshot 及 worker 結構化診斷。持續的每資產 GPU cache 與完整 Scene View 驗收仍待完成。

- ✅ 已實作 project create/open、deterministic content-tree indexing、UUID/path search/filter、
  cancellation、progress、可檢查錯誤與 deterministic artifact hash。
- ✅ 已實作並測試 portable virtualized Content Browser／breadcrumb／selection model、
  transactional rename／move／delete、typed generation-safe drag validation、dependency／cycle
  inspection、transactional reimport、watcher debounce 與明確的 dirty-conflict decision。
- ✅ 圖形化 shell 現已將真實 deterministic index 綁到 docked Content Browser，包含
  breadcrumb／folder、search／type filter、virtualized UUID row、selection 與 thumbnail state。
  Generation-tagged drag/drop、dependency inspection、background reimport，以及 filesystem-backed、
  可回復的 rename／move／delete／undo 全部經 authoring-thread `ProjectContentSession`；UI 不直接寫檔。
- ✅ Versioned sibling `<asset>.meta` record 會持久保存 UUID 與 importer type。Read-only indexing
  會拒絕缺少或損壞的 identity state；writable indexing 以 atomic write 建立缺少的 record。
  Rename、move、delete 與 undo 將 source/sidecar 視為同一 transaction；derived artifact 以 UUID
  加 source bytes 定址，因此搬移與 process reopen 後 identity 仍保持不變。
- ✅ Project descriptor 現具 stable UUID，並會在 OS-held single-writer lease 下由 schema 1
  原子升級至 schema 2。明確的 read-only open 不得升級或修改 project-owned state；recent
  project 使用有上限且版本化的 user-level store；docked Project panel 會顯示 canonical root、
  schema／upgrade、access 與 recent-project 狀態。Core、graphical contract 與 Linux real-process
  test 涵蓋第二 writer 拒絕及 read-only 共存。
- ✅ 圖形化 Project Browser 只發出 one-shot create/open request，不擁有 project state；
  application 會交易式啟用候選 workspace／index／content session，失敗時錯誤留在 selector。
  Linux Xvfb 驗收會在沒有 `--project` 的情況下，以純鍵盤完成 create 與 read-only reopen，
  再驗證 descriptor 與實際 access mode。
- ✅ Editor-owned `AssetImportQueue` 現會以可取消的 Core job 執行 project indexing 與 reimport。
  Worker 只產生帶 generation 的 staging result、bounded progress 與 structured diagnostic；
  authoring thread 會重新驗證 revision／dependency 後，才啟用候選 index 或原子發布 reimport。
  Queued cancellation、stale completion 與 shutdown 都會保留舊 index／artifact，selector 與
  Content Browser 也會呈現 progress／cancel／failure state。
- ✅ 圖形化 Content panel 現會顯示 dependency cycle，並以一次一筆的阻塞式 dialog 序列化
  dirty external change。Compare 顯示保留的兩側 hash 且不解決 conflict；Reload 或 Keep 記錄
  authoring-thread 的終局決定，UI 不會直接覆寫檔案。
- ✅ Focused Content F2 與 context Rename 現共用 owning UUID／generation／root／path draft、
  focused／select-all UTF-8 檔名、Enter／Apply 與 Escape／Cancel。無效名稱可重試，同名
  清除錯誤並保留 Content Undo；失焦在延後繪製前立即取消。Modal 阻擋 authoring／File 指令，
  並在焦點／write loss、隱藏 Content、
  外部 modal 或 stale asset scope 時取消。真正 1×／2× Unicode／gate 測試及 Linux Xvfb
  F2／Enter 後的 Scene Save／Undo／重啟驗證流程；實體 IME 與完整圖形驗收仍待完成。
- 待辦：實體顯示／Windows 新 project 全流程驗收。

### ED-M2 — Scene authoring core

- ✅ Native Move 現可繪製／命中 Global／Local 與 Pivot／Center 的 XY／XZ／YZ 平面把手，保留
  numeric 平面基底，讓 preview／release 共用位移計算與各軸 world-unit snapping。Shift 保持
  明確選中的平面；Escape 可取消。測試涵蓋非法射線、共用 box、鏡像／非均勻父節點、選中的
  descendants、opaque bytes、preview／Redo 與一步 Undo。Linux Xvfb 在 Scale／Rotate 檢查後，
  經六個平面移動並儲存 proxy／OBJ roots。完整 gizmo 驗收仍未完成。

- ✅ Native Scene 的 X 現可切換 Global／Local axes，Scale 維持 Local；P 切換 Pivot／Center，
  兩者皆在同幀點擊／拖曳開始前處理。1x／2x 真實輸入涵蓋 Home navigation、實際 hover／focus、
  modifier／文字／modal、拖曳中與放開待提交、唯讀等限制，並保留 World／Redo。
  Linux Xvfb 驗證 proxy／OBJ roots 的 Home／P center scale／rotate／move 與一步 Undo。
  完整 gizmo 與目標主機驗收仍未完成。

- ✅ Scene Ctrl+A 與 Select all 現以 generation-keyed selection 選取，不改動 World、dirty、
  clipboard 或 history。Overview 選取場景節點；native 使用 owning 單一 frame token 與實際
  3,999-bounded draw／pick candidate，跳過 hidden／locked record 及未提交的 tail。
  真正 1×／2× 驗證唯讀、空場景、panel／text／modal／focus／backend 及 active／pending drag。
  測試保留 unknown bytes、clipboard 與 Redo，拒絕 stale／malformed／duplicate packet，並涵蓋
  4,001 個節點。Linux Xvfb 選取兩個 proxy／OBJ root；完整 Scene View 驗收仍待完成。

- ✅ Native Scene Select（Q）保留一般／Ctrl picking，隱藏變形把手並停止 drag preview／commit。
  W／E／R 回到 Move／Rotate／Scale。真正 1×／2× 輸入測試涵蓋同 frame Q／click、toolbar
  一致性、Home 導覽、active／pending drag、唯讀、panel／focus／modal／text gate 及保留 Redo。
  Linux Xvfb 驗證 proxy／OBJ picking、隱藏把手、Select 拖曳後 saved bytes 不變，再切回變形工具。
  完整 Scene View 與 target-hardware 驗收仍待完成。

- ✅ Scene Home 與 Frame all 導覽而不改動 selection、World、dirty
  state 或 history。Owning token 使用實際 3,999-bounded native submission candidate、
  upload／proxy fallback 與精確 affine bounds，僅於發出 request 的 frame 套用一次；沿用 FOV distance；
  overview 依 logical canvas 在 zoom 範圍內對準全部世界 origin。真正 1×／2× 測試驗證
  空 selection／scene、唯讀、button／keyboard 一致、modal／panel gate、拒絕 bounds 與保留
  Redo，包含 release／Home、4,001-node limit 與 stale／一次性 packet。按鈕 enable 不新增
  idle frame 的完整 node snapshot；Linux Xvfb 還原 proxy 及 authored OBJ 的 native 像素。
  完整 Scene View 與實體顯示驗收仍待完成。

- ✅ Native F 與 Frame selected 現依精確世界換算 CPU mesh 及旋轉 proxy bounds 對準選取
  forest，子節點不重複計入。使用目前裁切 framebuffer aspect 及較窄的 viewport FOV 設定
  有界距離；無效／超出範圍的 bounds 保留原相機。真正 1x／2x 鍵盤／按鈕測試涵蓋偏移與
  鏡像／剪切 geometry、portrait viewport、catalog 替換／stale fallback、唯讀導航、modal／
  panel gate，以及保留 World／Redo。Framing 不擁有 GPU data 且不做 source IO；完整 Scene View 驗收仍待完成。

- ✅ Hierarchy 有焦點時，F2 現開啟 Rename 並聚焦／全選名稱；Enter 提交一次 metadata Undo，
  Escape 取消。Dialog 阻擋其他 authoring／clipboard／Undo／Save／Play 快捷鍵及 queued
  Hierarchy 寫入。真正 1x／2x input 驗證 UTF-8 CJK／supplementary 字元、空名稱重試、
  clipboard／history 保留、save／reload，以及 write loss、外部 modal、失焦或 stale document
  的取消。ImGui／library consumer 共用 32-bit Unicode text；字型涵蓋及實體 Windows IME
  驗收仍待完成。

- ✅ Hierarchy 有焦點時，Ctrl+A 現經 generation-keyed selection 選取全部 filter／expansion
  可見列，包含被裁切的列。空結果清除選取，唯讀 project 仍可使用。鍵盤測試驗證 collapsed
  descendant、filter 順序、其他 panel／text-input 焦點、recovery／close gate，以及 World／
  Redo 不變。大型 scene 的 scale／soak 驗收仍待完成。

- ✅ Typed Content mesh 拖曳現可指派 Inspector Mesh field 的顯示選取。Hover 只預覽，
  放開後以一次 generation-checked batch 保留既有材質並補上缺少的 MeshRenderer。真正
  1x／2x pointer 測試涵蓋初始化 Undo／Redo、save／reload、非 mesh／stale catalog／project
  拒絕、取消 input 與 workspace／modal gate；拒絕保留 Redo。完整材質及 reflected Inspector
  流程仍待完成。

- ✅ Hierarchy Cut 與 Ctrl+X 現先擷取完整選取 forest，再以一次 atomic transaction 刪除。
  Undo 還原原始 ID 與選取；首次成功 Paste 保留 root 名稱並建立新 ID，之後保留的 clipboard
  資料改用 Copy 命名。失敗 Cut／Paste 與 Duplicate 保留 pending clipboard state。真正
  1x／2x key／pointer、workspace／modal／text-input gate、replay 與持久化測試涵蓋生命週期。
  新 layout 將 Inspector dock 在右側，保持 Hierarchy 可操作。

- ✅ 圖形化 Copy／Paste／Duplicate 現以 owning snapshot 擷取完整選取 root forest，保留
  copy-time 世界 root pose、child local transform、Camera／Light／MeshRenderer payload、
  authored Euler hint 及 opaque bytes。Parent 對應新 stable ID，已選 descendant 僅複製一次。
  一次初始化建立 Undo 移除整個 forest 並還原 prior selection，Redo 保留初始化值。真正
  Ctrl+C／Ctrl+V／Ctrl+D／replay 及持久化測試涵蓋 copy-time 隔離、clipboard 保留、forward
  parent、validation／collision／lifecycle 拒絕與 1,000 次 cycle。

- ✅ 多選 Delete 現以一次 atomic Runtime／document transaction 刪除全部選取 subtree。
  一次 Undo 還原 stable ID、sibling order、元件、名稱、Euler hint、opaque payload 與完整
  選取。真正 Delete／Ctrl+Z／Ctrl+Y 測試驗證流程；ID collision／lifecycle 及外部擴展
  subtree 拒絕保留 history，無關 entity 保留；不同外部 parent 消失後，orphan root 保留
  已擷取世界 pose 與共同 merged sibling order，且 1,000 次 replay cycle 維持 snapshot。

- ✅ Typed Content asset 拖曳現能將已解析 mesh 放到 overview X/Z 游標位置或 native Scene
  的 Y=0 ground 交點；owning UUID／generation payload 在拖曳開始時固定。Tooltip 預覽落點
  不修改 World，放開後以一次初始化 Undo 建立／選取具名稱 root。真正 pointer 測試涵蓋
  1x／2x DPI、Undo／Redo、save／reload，以及權限、generation、modal、Escape／失焦與
  不支援 ground 的拒絕，保留 Redo。Native 放置共用 preview 的 clamped camera 與裁切限制。
  表面吸附、geometry ghost 預覽、完整材質及 target-host 驗收仍待完成。

- ✅ Content 現可透過 Add mesh to Scene 在 Scene center 建立／選取單一已解析 mesh root；
  初始化 entity 共用一次 Undo，Redo 保留 stable ID、名稱、pose 與 component。Project-generation
  catalog 檢查及 workspace／content 可寫、modal gate 拒絕 stale、缺失、非 mesh 或多選資產。
  Native 放置共用 preview 的 center 限制與 target 高度。真正 UI 點擊、Game geometry 準備與
  save／reload 驗證此路徑；action 不讀 source。完整材質仍待完成。

- ✅ 單一 Camera 的 Inspector 現可將世界位置／旋轉對齊已儲存的 Scene 3D 視角，並以一次
  Undo 還原，保留 lens、local scale 及 parent。逐一反轉 root 至 parent 的 local TRS，正確
  處理 shear／mirrored 父鏈；無效、stale 或非 Camera 目標會在修改前拒絕。等價 pose 保留
  Redo。真正 UI 點擊、Runtime camera matrix 及 save／reload 驗證 root／parented Camera、
  唯讀及不可用 view gate。極端有限 center 會在對齊前套用與 native preview 相同的
  +/-100,000 限制。完整 camera authoring 驗收仍待完成。

- ✅ Scene／Hierarchy 編輯現在要求所附 workspace 可寫，且沒有復原、Play review 或關閉
  對話框。控制項、快捷鍵與 queued create／rename／reparent／reorder 共用此 gate；唯讀
  選取、Copy 與鏡頭導航仍可用。真正鍵盤／pointer 測試驗證保留 Undo／Redo、恢復後的
  Paste／Duplicate、唯讀 picking，以及權限切換取消 overview／native 拖曳。
  完整圖形化 scene authoring 驗收仍待完成。
  Escape 現會明確取消關閉確認，之後才恢復 Save；Xvfb 驗證此路徑。

Hierarchy、Scene View、Inspector、camera controls、selection/picking、translate/rotate/scale gizmo、
parent/reorder、multi-selection、copy/paste、undo/redo 與 save/reload。Reflection 產生 property
widgets；未知 component 保留 raw data，不靜默遺失。

- ✅ Vulkan Scene／Game upload 現會重用 fence-protected frame slot 的有界容量。穩定、縮小
  或沒有 Scene 的 frame 保留配置；放大時先完成新配置才釋放舊配置，resize／teardown 會等待
  GPU 完成。Native call tracing 與像素測試驗證 100 個穩定 frame、最大 descriptor 預算、
  放大失敗及無洩漏生命週期。每次 draw 仍複製最新 geometry；persistent per-asset GPU cache
  及完整圖形化驗收仍待完成。

- ✅ Hierarchy 現可選擇 Empty、Camera 或 Light，套用於 Create root／Create child 及
  Ctrl+Shift+N。Camera／Light 以 identity local TRS 初始化，並具完整 stable-ID 的單步
  Undo／Redo。預設名稱隨型別選擇調整，自訂名稱保留。Owning queued request 重新核對
  存取權與 scene／parent generation。Portable 及 1x／2x 真正選單／pointer／key 測試涵蓋 root／child、
  過期 scene／parent、唯讀／modal gate、100-step replay 及 save／reload。完整 reflected component
  creation 與 target-host 驗收仍待完成。

- ✅ Inspector Copy values／Paste values 現可擷取單一 entity 已提交的 Transform／Euler、
  Camera 或 Light 數值，再以原子單步 Undo 套用到多選。Camera／Light 保留缺少元件的狀態；
  Transform 保留作者輸入的圈數。Typed owning clipboard 跨來源修改／刪除及 reload 保留，
  並獨立於 Hierarchy clipboard。Portable 及 1x／2x 真正 UI input 測試涵蓋唯讀 Copy、
  停用／不符型別的 Paste、草稿取消、no-op Redo 及持久化。完整 reflected Inspector 仍待完成。

- ✅ Inspector 現提供多選 Reset Transform、Reset Camera 及 Reset Light。Transform 重設會
  清除 local TRS 與可見／過期的 Euler 圈數；Camera／Light 重設保留缺少元件的狀態。變更 batch 以原子
  單步 Undo／Redo 提交，no-op 保留 Redo。重設取消未提交草稿及 Scene 手勢，並遵守 workspace／
  modal gate。Portable 與 1x／2x 真正 UI 輸入測試涵蓋 metadata-only Undo、mixed presence、
  無關 payload 保留及 save／reload。完整 reflected Inspector 與 target-host 驗收仍待完成。

- ✅ Camera／Light Inspector 現支援多選及 mixed presence／value。Mixed component 的 Enable
  會補到缺少的 entity 並保留現有值；Enter 只套用編輯欄位，以 generation-checked 原子交易
  完成單步 Undo／Redo。無效／過期／重複 batch 與唯讀／復原寫入會整批拒絕。真實鍵盤測試
  涵蓋 Camera FOV、Light intensity、未編輯欄位保留、重複 Undo／Redo 及保存／重開；完整
  reflected 編輯仍待完成。

- ✅ Camera／Light 控制項現在於唯讀、復原、Play review 與關閉確認時停用，並丟棄草稿及
  pending request。真正鍵盤測試涵蓋 Escape、失焦、取消選取／reload、Play Inspector 與
  Inspector 收合，防止恢復可用後復活舊輸入。Pending batch 重新核對當前選取並取消待提交
  Scene 手勢；locale-independent 數值格式保留完整精度。完整 reflected 編輯仍待完成。

- ✅ 缺少外掛的元件現有有界唯讀 Inspector，顯示 owning 名稱、完整 entity／type ID、bytes
  與最多 64-byte 預覽。Scene format 3 會在保存／重載、clipboard 複製、刪除及獨立 metadata
  Undo／Redo 中保留 opaque data；無 opaque 資料仍寫 format 2，reader 相容 format 1／2。
  Generation 與每元件 1 MiB、總 payload 16 MiB、每 entity 64、總 4096 records 限制會拒絕過期／
  超限匯入。損壞、重複、orphan 與缺失 entity 紀錄在修改 live state 前拒絕。外掛還原／執行
  及完整 reflected 編輯仍待完成。

- ✅ 原生 picking／放開拖曳的編輯命令現先於 Save、Save-and-exit 及 GPU 提交完成。
  真正的 Xvfb XYZ 拖曳放開後立即 Save，並驗證已完成的姿態；仍按住手勢時的 Save
  會等待提交或取消後才儲存。

- ✅ 原生 Scene 預覽現以有界 Presentation batch 繪製解析後的 OBJ vertices／indices，shared
  resource 只打包一次，使用絕對 16-bit index 與精確 affine world instance。解析 mesh 以 transformed
  bounds 與雙面 triangle picking 取代代理選取。Portable 測試涵蓋範圍回復、幾何／座標預算、
  負縮放／旋轉 picking 及輪廓 miss；Xvfb 不同 triangle／quad 資產驗證選取、Center 縮放／旋轉、
  預覽／放開像素及單步 Undo。缺失／刪除／超限資產保留參照、顯示代理並警告。持續的每資產
  GPU cache、material shader 與完整 Scene View 驗收仍待完成。

- ✅ Inspector 現能為單選及 mixed 多選指派匯入 OBJ mesh 資產及移除 MeshRenderer，以一個
  atomic、generation-safe Undo／Redo 操作完成。各 entity 保留自己的 material 參照；混合有無
  元件時僅對缺少者新增預設值。Runtime／document batch 在修改前拒絕重複、缺失及過期 target；
  無變更 batch 保留 Redo。Contract 測試以真正 combo／remove 點擊驗證整批操作、重複 replay、
  唯讀拒絕、selection／project／document 過期及 save／reload。缺失參照仍保留；完整 Scene View
  驗收仍待完成。

- ✅ MeshAssetCatalog 現以 UUID 穩定衍生 64-bit resource ID，並依專案 generation 發布 owning
  匯入 geometry。測試固定保存 ID、驗證重新命名／重新開啟後的參照、原子拒絕碰撞，並在卸載後
  保留 snapshot。持續的每資產 GPU cache 與完整 Scene View 驗收仍待完成。

- ✅ SceneDocument 現透過 Runtime Undo／Redo 提供 generation-safe MeshRenderer 新增、mesh／
  material 資源參照替換、移除及 owning 查詢。測試驗證完整 64-bit 與尚未解析的 ID 可場景儲存／
  重載、無變更編輯保留 Redo，並拒絕過期 key。持續的每資產 GPU cache 與完整 Scene View 驗收仍待完成。

- ✅ 原生與概覽拖曳在應用程式失焦時先於合成放開事件取消，並在 Undo／Redo、建立／貼上／複製物件快捷鍵、文件世代
  改變、畫布隱藏、復原提示及預覽模式切換時取消。Contract 測試涵蓋概覽／原生失焦、Undo 及
  文件替換；Xvfb 在移動預覽途中觸發真正的 FocusOut，驗證已儲存場景位元組不變。

- ✅ 公共 Presentation SceneDrawData 邊界支援在同一個原生 depth pass 繪製有界的 geometry／
  instance 批次，保留完整 mesh 預設行為，具 portable 範圍拒絕及不同 geometry 的 Vulkan 像素證據。
  [ADR-0002](ADR-0002-Editor-Scene-Mesh-Batches.md) 記錄契約；Editor 持續的每資產 GPU cache
  及完整 Scene View 驗收仍待完成。

- ✅ 原生 3D gizmo 提供 Pivot／Center（P）切換。Center 把手、旋轉／縮放預覽與提交共用選取根節點
  的平均原點及首個根節點的 local 軸；選取子節點不重複加權，拖曳期間鎖定工具、pivot 及相機。
  Linux Xvfb 驗證雙根節點選取、Center 縮放吸附、Center 旋轉及單次 Undo。

- ✅ Authored Scene geometry、保守 bounds 與 triangle picking 現跨鏡像／剪切 ancestry 使用精確
  WorldMatrix；preview 使用與 commit 一致的 owning 預期 matrix。Native Game mesh 採用 fixed tick
  後的 live Play matrix，即使 inspection metadata 較舊亦然。測試以 closed-form affine hit 區別
  lossy TRS miss，保留 Stop 後的 frame matrix、逐物件拒絕無法表示的換算，並維持 frozen asset／
  clone isolation。Proxy／gizmo 維持 TRS policy；material workflow、GPU cache 與完整驗收仍待完成。

- ✅ Public Presentation instance 現可接收精確 affine model matrix，Vulkan／DX12／Metal 共用
  private inverse-transpose normal packing。Portable 與 native Vulkan pixel test 涵蓋鏡像／剪切
  matrix 及 invalid-then-valid draw；Scene／Game authored mesh 現使用 owning 精確 matrix，graphical 驗收仍待完成
  （[ADR-0003](ADR-0003-Presentation-Affine-Instances.md)）。

- ✅ 深層鏡像／剪切階層的原點與 gizmo 位置換算現為 affine-exact。SceneDocument 擁有精確
  world 及預期 matrix snapshot；位移與 Center 旋轉／縮放的位置與 commit 一致，
  `editor.affine_gizmo_contract` 驗證單次 Undo／Redo 及 save／reload。原生 authored mesh 現使用這些精確 matrix。

- ✅ 原生 Move／Rotate／Scale 預覽與提交共用 SceneDocument 根節點編輯及 Runtime 階層組合。
  預期世界姿態快照不改動 dirty 狀態、選取或 Undo／Redo；測試涵蓋旋轉、鏡像、非均勻縮放祖先、
  選取子節點、過期 key、無效倍率、單次 Undo，以及等比例縮放放開後畫面不跳動。

- ✅ Editor Core 已實作 stable-ID hierarchy/selection、cycle-safe reparenting、multi-selection、
  clipboard duplication、transform transaction、undo 與 atomic scene save/reload。
- ✅ 已實作並測試 portable Inspector property adapter 與 mixed-value multi-selection、opaque
  unknown-component round trip、可安全取消的 gizmo transaction state machine、generation-safe
  asynchronous picking、atomic camera persistence、1,000-step undo/redo replay，以及 corrupt scene
  的既有狀態保留。
- ✅ 圖形化 Hierarchy 現會繪製 parent-aware expandable tree、依 entity name 過濾、以保留且
  generation-keyed 的 anchor 處理 plain／Ctrl／Shift selection、裁切可見列提交，並把 rename、
  兄弟排序與 drag/drop reparent 送進 generation-safe、可復原的 `SceneDocument` contract；stale
  entity／document generation 會被拒絕。
- ✅ Docked 圖形化 Inspector 會呈現單選或 mixed 多選的 local position、Euler 度數
  （quaternion storage）與 scale。Position／Scale 現保留有界數值草稿，僅在 Enter 時提交為一個
  atomic Undo step，保留最新的其他欄位與各 entity 的旋轉。完整精度科學記號及負縮放可用；
  無效／零縮放輸入拒絕整批操作，相同值 Enter 保留 Redo。真正鍵盤測試涵蓋輸入、Escape、
  失焦、selection／reload、Play Inspector、唯讀／復原／關閉確認與 save／reopen。
  草稿不會儲存；完整 reflected Inspector 及 target-host 驗收仍待完成。
- ✅ Inspector 的單選 Camera 區可新增／移除元件，並以 generation-keyed Undo 編輯經驗證的
  垂直視角與遠近裁切面。場景儲存／重新載入會保留數值；完整 reflected Inspector 仍待完成。
- ✅ 單選 Light 區也可新增／移除元件、編輯經驗證的非負亮度，並具同樣的 Undo 與場景持久化。
- ✅ 圖形化旋轉欄位使用度數、明確的 Z-X-Y composition 與有限值驗證。
  Enter 將多選變更提交為一個 atomic transaction，保留各 target 的其他軸、position 與 scale。
  Contract test 以公開 key/text event 驅動真正的文字欄位。
- ✅ SceneDocument 擁有 Euler 提示，跨 selection／save／reload 保留輸入圈數，且 undo 會還原
  提示，即使 quaternion 未改變。Editor scene format 2 驗證有限值、唯一性與旋轉一致性，並讀取
  舊版 format 1。Atomic 同 World reload 保留 scene ID／state；損壞資料與跨 scene ID 衝突
  會被拒絕且不改變 live state。Target-host 驗收與完整圖形化 save／restart workflow 仍待完成。
- ✅ File New／Open／Save／Save As 現管理單一活動場景，支援 project-relative UTF-8 路徑、
  document／project token、dirty Save／Discard／Cancel、Untitled 的巢狀 Save As 與明確 Replace。
  Open 失敗保留文件／history／path；覆寫失敗保留目的路徑及既有暫存檔。唯讀允許 Open 並拒絕寫入；Play／recovery／close gate
  拒絕替換場景。New 清除 history／clipboard 並保持 dirty 直到儲存。Content 儲存發布 persistent
  identity，僅串流自己的有限來源檔並保留其他 geometry 及先前 Content Undo。Canonical alias
  維持 metadata namespace／type，關閉儲存失敗則重開原路徑與錯誤以供重試。各檔案 CPU camera state 在切換及可寫 shutdown 時保留。
  真正 1×／2× menu／key／modal 測試與 Linux Xvfb 驗證 New、輸入 Save As、Open／重開、
  原檔案保留及唯讀 bytes。Content 中文資料夾／檔名顯示、搜尋、改名／移動與 Undo 使用 UTF-8
  與原生路徑，portable 及 1×／2× panel 測試驗證不經 Windows 系統字碼頁。
  ✅ 啟動現會恢復上次成功 Open／Save 的場景及各檔案 view state，包含唯讀重開。有界
  project／UTF-8 metadata 重新驗證 managed scope；無效／aliased、其他專案或無法載入的資料
  回到 Main 並在本次 session 保留原檔案。New／失敗操作保留先前選擇；獨立 metadata
  儲存失敗保留成功的場景儲存。Portable 及 Linux Xvfb 的重啟／編輯／儲存／fallback 驗證
  流程；additive tab 與完整 ED-M4 圖形驗收仍待完成。
- ✅ Content Browser 場景開啟現支援雙擊、context Open scene、Open scene 按鈕及 focused Enter。
  Owning 路徑沿用 deferred scene-file request 與 dirty Save／Discard／Cancel；UI widget 不執行
  檔案 IO 或 World 替換。唯讀允許 Open；Play／modal／token gate 拒絕替換。真正 1×／2×
  pointer／key 測試涵蓋 Unicode 路徑、單／雙擊、非場景／多選拒絕、dirty 決策及過期
  request；additive scene tab 與完整 ED-M4 圖形驗收仍待完成。
- ✅ 目前 Content 場景現透過 stable UUID 跟隨重新命名／移動及 Content Undo，保留文件
  generation、dirty 內容、selection、history 與 live view state。已提交的移動也會更新啟動
  檔名而不儲存 dirty World，Discard and Exit／重啟可載入改名後已提交的來源。
  刪除／失去、unsafe 或過期
  追蹤資產時禁止一般 Save；還原後恢復儲存，同一舊檔名的不同 UUID 不可取代關聯。
  已儲存場景 index 只在舊 source／sidecar 都不存在時更新 stale 移動項目。Portable
  Unicode／唯讀／失敗契約及 Linux Xvfb context rename／delete／Save／Undo 驗證流程；
  完整 ED-M4 仍待完成。
- ✅ Scene panel 現可透過按鈕及文字輸入欄位以外的 Ctrl+Z／Ctrl+Y／Ctrl+Shift+Z 執行
  Undo／Redo。Runtime 重播會還原穩定 ID、階層、transform 與 Camera／Light 元件；文件重播會
  還原名稱、選取與輸入的 Euler 圈數。新編輯會清除 Redo 分支；測試涵蓋重播及撤銷建立後的
  儲存。完整視覺工作流程仍待完成。
- ✅ 圖形化 Hierarchy 可透過檢查 generation 的請求建立具名稱的根節點與子節點，選取新實體
  並在樹狀清單中顯示。Contract test 涵蓋過期 parent、Undo 與 save／reload；Scene View 和
  完整 ED-M2 驗收仍待完成。
- ✅ Ctrl+Shift+N 可在文字輸入欄位以外建立 Hierarchy 根節點。Linux Xvfb 現會透過圖形化
  流程建立、儲存、重新啟動並載入場景，確認新增根節點仍存在。
- ✅ 圖形化 Hierarchy 的 Copy／Paste 按鈕與 Ctrl+C／Ctrl+V 現使用世界姿態快照剪貼簿，
  並選取及顯示貼上的根節點；文字輸入欄位保留自身的剪貼簿快捷鍵。Contract test 涵蓋
  快捷鍵、複製後移動來源，以及單一貼上物件的 Undo。
- ✅ 圖形化 Hierarchy 現可透過按鈕或視窗取得焦點時的 Delete 鍵刪除選取的 subtree；游標停在原生 3D 畫布時按 Delete 也會使用同一個可復原的場景動作。
  Undo 會還原節點資料與選取；同時選取的子孫節點不會重複刪除。
- ✅ Duplicate 按鈕與 Ctrl+D 現可複製圖形化選取，同時保留使用者原本的剪貼簿。
  Hierarchy 會在 Paste 或 Duplicate 修改文件後才建立當幀的節點檢視。
- ✅ 中央 Scene panel 現有可互動的 X/Z 俯視概覽：含父節點合成後的世界位置、網格、滾輪縮放、
  中鍵平移，以及與 Hierarchy 同步的點選。原生 OBJ 輸出現已提供，完整 Scene View 驗收仍待完成。
- ✅ 原生 Vulkan／DX12 場景繪製 contract 現可指定有界的實體像素 viewport。
  Portable 邊界檢查與 Vulkan Xvfb 像素讀回涵蓋裁切。
- ✅ Docked Scene canvas 現會在 layout 與 DPI 縮放後提供可見的 framebuffer 像素邊界，
  並於每幀重設。
- ✅ Scene panel 現提供 Vulkan／DX12 原生 3D 代理預覽，在 UI 提交後於該 canvas 繪製有深度測試的
  地面與 live scene 節點位置代理，保留 canvas 外的控制項。X/Z 編輯概覽仍可切回。解析後的 OBJ 現以原生 batch 繪製及 triangle picking；
  authored mesh 現使用精確 shear 矩陣；material shader 仍待完成，完整 renderer-backed
  Scene View 驗收仍未通過。
  代理 instance 現反映合成後的世界旋轉與縮放；保守包圍範圍先篩選候選物件，再精確點選
  旋轉盒體及位移把手，避免點到包圍範圍的空角落。
  右鍵拖曳可旋轉預覽鏡頭，中鍵拖曳可平移 X/Z 目標，Shift 加中鍵拖曳可平移目標高度，
  滾輪可縮放；F 或 Frame selected 依精確世界換算 CPU mesh 與旋轉 proxy bounds 將 X/Y/Z
  目標對準選取 forest，子節點不重複計入，並依較窄的 viewport FOV 調整距離（2–100 世界單位）。旋轉角度、距離與目標高度現會逐場景保存。
  點選可見的位置代理可同步選取 Scene、Hierarchy 與 Inspector 中的節點；Ctrl 點選可切換選取。
  拖曳代理會即時預覽選取根節點及其後代的世界 X/Z 位移；按住 Shift 起始拖曳則沿世界 Y 軸
  移動。選取代理會顯示彩色 X/Y/Z 位移把手；Local axes 可讓把手依第一個選取節點的世界旋轉，
  點擊把手後拖曳會固定於該軸。
  放開左鍵時以單次可復原 transaction 提交。Escape 可取消拖曳；可選 0.25–4 世界單位吸附同時作用於預覽與提交。
  Rotate 工具（游標位於畫布時按 E；W 回到 Move）顯示世界或 Local X/Y/Z 旋轉環；放開滑鼠
  時以單次 Undo 提交選取根節點的原地旋轉；拖曳時即時預覽選取根節點及其後代。
  Scale 工具（游標位於畫布時按 R）顯示 Local X/Y/Z 立方把手及白色等比例把手；拖曳期間即時顯示選取根節點與子節點的縮放，Escape 可取消預覽；放開滑鼠時以單次 Undo 提交單軸或三軸 local scale。起始拖曳時按住 Shift 可將旋轉吸附至每 15 度、local scale 增量吸附至每 0.25 倍；預覽與提交使用相同結果。
- ✅ Scene 概覽現共用 Hierarchy 的 Ctrl／Shift 多選錨點，並可透過 F 或 Frame selected
  將檢視中心移至選取物件的世界位置。輸入事件測試涵蓋這兩項操作。
- ✅ Scene 概覽標記拖曳會預覽世界 X/Z 位移；可見的 X 與 Z 把手可將位移限制於單一世界軸，
  放開後以單一可復原的 atomic transform transaction 移動選取的根節點。可選 0.25、0.5、
  1、2 或 4 世界單位吸附，預覽與提交使用相同位移。Escape 可取消；測試涵蓋單軸約束、
  父節點縮放、subtree 根節點及吸附後的子節點位置。
  完整 3D gizmo 操作把手與 renderer-backed Scene View 仍待完成。
- ✅ Scene 概覽中心與縮放現透過經驗證的逐場景 `CameraPersistence`，在開啟 project 與正常的可寫
  關閉流程中載入／儲存。損壞的 camera 檔案會保留原狀；Linux Xvfb 驗收檢查滾輪縮放在重新
  開啟 project 後維持。代理預覽現已有基本 3D camera 操作；旋轉角度與距離另存於逐場景的
  camera 檔案。
- ✅ 圖形化 Scene panel 現顯示依實際內容計算的未儲存標記。僅成功 save／reload 會更新基準，
  Undo 回到該內容時會清除標記；contract test 涵蓋 subtree 刪除、儲存失敗及外部 Runtime
  修改。
- ✅ 原生關閉要求遇到未儲存場景時，會顯示儲存後離開、捨棄後離開與取消選項；儲存失敗或
  唯讀時保留對話框。外部視窗銷毀仍會停止繪製；圖形化關閉／重開驗收仍待完成。
- 待辦：圖形化 Scene View、完整 reflected Inspector、renderer-backed picking、camera control、
  gizmo、reflected widget
  與 unknown-component visual workflow。ED-M2 exit 仍需 UI 中完成 select／edit／undo／save／restart
  驗收與視覺證據。

### ED-M3 — PIE 與 debugging

Game View、play/pause/step、fixed tick、input focus、Editor/Play World 隔離、apply changes policy、Console、entity/component inspection、breakpoint adapter boundary。Zig gameplay 由 Engine Host 載入，Editor 不成為 Zig `main`。

- ✅ Portable `PlaySession` prerequisite 已涵蓋隔離 Play World ownership、fixed tick、
  play/pause/step、input-focus policy、預設丟棄及明確 transform apply-back。
- ✅ Portable debugging prerequisite 新增 structured bounded Console records、owning runtime
  inspection snapshots、debugger boundary/pause reasons、contained update recovery，以及 deterministic
  all-or-nothing transform conflict detection。
- ✅ Docked Console 現顯示有界 Runtime 紀錄，提供文字／嚴重度篩選、來源、時間戳與丟棄數；
  Editor 會記錄啟動及場景開啟／儲存診斷。
- ✅ Docked Game panel 現可操作隔離的 PlaySession：Play／Stop、Pause／Resume 與單一步進；
  顯示複製的 entity 檢視資料、有界的 X/Z 世界座標俯視預覽與 fixed tick 計數，F5／F6／F10 提供鍵盤操作，Linux Xvfb
  會執行整段流程。可選 gameplay library 現提供回呼，Stop 會捨棄 clone。
- ✅ 有界原生 Game View 透過隔離 Play World 的第一個有效 active camera 繪製匯入 OBJ。
  Start 會凍結資產版本，fixed tick 後的 frame 擁有 upload 資料。測試涵蓋共用 geometry、
  inactive scene、reimport 隔離、Pause／Step／Stop，以及 Xvfb/lavapipe 攝影機像素與未變動
  的 Editor 場景。同視窗每幀共用一次原生 3D submission；兩個 canvas 同時顯示時 Game 保留檢視圖。
- ✅ Game View 現提供 clipped Preview camera 選單，預設 Automatic，可選有效 active camera。
  選擇只作用於目前 Play session 的預覽；唯讀專案仍可操作，modal 提示會停用選單，Stop／
  新 Start 會清除選擇。真正 UI 點擊與 Runtime 測試涵蓋元件移除、無效 native-float 投影、
  scene 卸載、確定性回退，以及 Editor／Play 資料和 Undo 未變動；frame 在 tick 後重新驗證。
- ✅ Runtime component wire 讀寫現可直接接受隔離的 PlaySession World，與 GameWorld 共用
  解碼及原子 command 驗證。Fixed／Step 回呼測試涵蓋 Editor 隔離、父階層世界姿態、component
  payload 與失敗寫入 rollback；初版圖形化 input adapter 現已實作。
- ✅ Game panel 現可選擇 project-relative V3 gameplay library。Start 會綁定隔離 clone；
  optional FixedUpdate 在 tick／Step 執行，Update 每個 playing frame 執行一次。有界訊息進入
  Console，失敗會拒絕 Start／暫停 Play，Stop／視窗關閉會先卸載再銷毀 clone。Static lifecycle
  測試與 Xvfb 真實動態 library 已驗證 mesh 移動、Pause／Step／Stop、Editor 場景未變動。
  初版 host 支援 component，未宣告 scene／physics 服務；擴充 input 裝置與 hot reload 仍待完成。
- ✅ 點擊 playing Game canvas 現可透過複製的 gameplay input snapshot 路由 user-zero
  WASD／方向鍵位移與 Space／滑鼠／Shift／Ctrl 按鈕。Escape、pointer 離開、隱藏 Game、
  Pause／Stop、提示視窗與 native 失焦會清除擷取及 held state。擷取中的按鍵不會觸發 authoring
  快捷鍵；F5／F6／F10 保留控制。測試涵蓋狀態轉移、owning snapshot、callback，以及 Xvfb
  input-only mesh 移動／釋放。Gamepad、pointer look、rebinding 與多使用者仍待完成。
- ✅ 選取 Game entity 現會開啟唯讀 Play Inspector，顯示複製的 local／world pose、parent、
  scene state、Camera／Light 與完整寬度的 mesh／shader ID。Game panel 顯示暫停原因及 callback
  失敗次數；每 frame 與 fixed callback 失敗皆會釋放 input。快照在元件移除與 Stop 後仍有效，
  Editor selection 與編輯狀態保持獨立。
- ✅ Gameplay library 選擇現會存入有界 schema-1 專案設定，與場景及 recovery journal 分開。
  唯讀存取不會寫入；無效設定會保留至明確替換，未處理的 recovery journal 會阻擋關閉時儲存。
  明確 CLI 路徑（含空字串）優先於已存設定。Xvfb 已驗證不帶 CLI 路徑重新開啟後，已存 library
  仍可驅動 Play mesh；單純重新開啟不會載入模組。
- ✅ Game Apply Changes 現會先暫停並顯示 owning transform diff。確認時重新檢查 Play、document
  與 entity generation，以及 original／Editor／Play 值，再用單次 atomic SceneDocument transaction
  套用並停止、捨棄 clone。衝突／重新掛接／其他場景會拒絕整批；Undo 會還原所有套用值。
  元件／建立／刪除不會複製；modal 會阻擋 authoring／Play 快捷鍵，預設 Stop 仍捨棄變更。
- 待辦：完整 Game View 材質／多個原生 canvas、完整 gameplay 服務與擴充輸入路由、完整 Runtime／build log
  路由，以及 native debugger/IDE 整合。

### ED-M4 — Prefab、場景與 collaboration safety

Prefab create/open/variant、override diff/revert/apply、nested rebase；additive scenes；stable serialization、schema migration、autosave/recovery、external-change detection、human-readable diff/merge。先支援安全的 source-control workflow，不先承諾即時多人協作。

- ✅ Portable prefab prerequisite 已涵蓋可檢視 override diff、單筆／全部 revert、immutable
  apply、variant 與 nested-path rebase。
- ✅ 已實作並測試 portable additive-scene ownership／dependency ordering、migration dry-run、
  atomic bounded autosave／corrupt recovery，以及 stable-path three-way conflict records。
- 待辦：圖形化 workflow 與 source-control provider UI 整合。

### ED-M5 — Specialized tools

Material/shader graph、animation state/curve、particle/VFX、audio mixer、navigation/physics debug、terrain/vegetation、localization。每個工具以 capability plugin 交付，缺 backend 時 read-only 或清楚 unavailable。

- ✅ Portable capability registry 強制 stable tool ID 與誠實的 implemented/read-only/unavailable
  狀態，fallback 必須附原因。
- 待辦：由各 production subsystem 支援的圖形化 specialized tool 與 capability plugin。

### ED-M6 — Build、profile 與 extensibility

Build profiles、cook/package frontend、target/device matrix、remote deploy/log、CPU/GPU/memory/frame profiler、plugin manager、script/API docs。任何「Build Success」必須附 target manifest 與可重現 command。

- ✅ Portable build frontend 會驗證並 atomic 寫入 target/configuration/command 與帶 checksum
  的 artifact manifest；有界的 monotonic CPU/GPU/memory frame capture 已實作。
- ✅ 圖形化 Profiler 現顯示即時且有界的 Editor frame processing wall-time 曲線，支援暫停／
  清除、最新／平均／峰值與丟棄數。GPU 時間與記憶體在量測接線完成前會明示為不可用。
- ✅ Profiler 現可把保留的 Editor frame-processing wall time 匯出為專案 CSV，保留 double
  精度與丟棄 frame 數，GPU／memory 欄保持空白。同步 writer 驗證 1-600 筆有序且有限的 sample，
  拒絕唯讀／recovery 寫入，驗證失敗會保留舊檔；實際 UI 點擊會送出一次性 request。
- 待辦：圖形化 build frontend、remote deployment/log、GPU／memory profiling、版本化 export
  與 plugin manager。

### ED-M7 — Production hardening

大型 project incremental index、virtualized UI、100k entity hierarchy、長時 soak、workspace migration、corrupt document recovery、signed extension policy、telemetry opt-in/privacy、keyboard-only與螢幕閱讀器 audit。

- ✅ Portable tests 已涵蓋 100k-item virtual hierarchy range、trusted-publisher/signature policy，
  以及明確 opt-in 前會丟棄 event 的 telemetry。
- 待辦：圖形化 performance/soak 驗收、workspace migration、corrupt-document recovery，以及
  keyboard/screen-reader audit。

## 4. 儲存與 transaction contract

Scene/Prefab/Project 格式要 versioned、deterministic、atomic write；每筆 command 有 target stable ID、before/after 或 reversible operation、merge key、authoring timestamp（不影響 runtime determinism）。長操作先寫 staging 再 commit；crash recovery journal 不覆蓋最後 good file。Schema migration 必須可 dry-run、備份、report，重大版本不得 silent downgrade。

## 5. 驗收矩陣

| 領域 | 自動 Gate | 人工/視覺 Gate |
| --- | --- | --- |
| Documents | golden round trip、migration、corruption/fuzz | recovery workflow |
| Transactions | undo/redo property tests、1000-step replay | gizmo/multi-edit behavior |
| Scene View | picking/gizmo math、render contract | DPI、多 viewport、resize |
| PIE | world isolation、start/stop stress、state diff | focus、pause/step workflow |
| Assets | import determinism、cancel/rollback、dependency cycle | browser/status usability |
| Extensions | ABI/version/capability rejection | install/disable/recovery |
| Performance | startup/index/frame/interaction baselines | representative large project |

## 6. 發布切片

**Editor Preview** 必須完整通過 ED-M0、ED-M1 與 ED-M2，任一單獨 milestone 都不足以構成
Preview；**Creator Alpha** 加入 M3/M4；**Production Beta** 加入至少一組 specialized tools、
build/profile 與 hardening。版本標章依實際驗收證據，不因 panel 存在或前置 groundwork
已完成就標為完成。

## 7. 依賴與非目標

依賴 Engine API M1～M5、window/swapchain、reflection、asset pipeline、scene snapshot 與 Editor
SDK。因此已有 reflection、undo model 或 prefab groundwork，仍不代表圖形化 Editor 已完成。
第一版不做完整 visual scripting、Marketplace、雲端協作、電影級工具或所有平台 remote
deployment；這些在核心 authoring loop 穩定後另立 Roadmap。

## 8. 固定交付決策

以下是施工輸入，不得由各 work package 重新決定。若要改動，必須新增 ADR（或修訂
ADR-0001）、相容性說明，並同步更新兩種語言版本。

| 主題 | 決策與必要後果 |
| --- | --- |
| 產品邊界 | `NexoraEditor` 是 public engine API 的獨立 client；Shipping 不連結 `Editor`、`EditorImGui` 或 editor metadata。 |
| UI 與模組 | 第一套 shell 是由 `NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL` 控制的 Dear ImGui docking；產品中立 model 留在 `Editor`，widget 留在 `EditorImGui`，相依維持宣告於 `Config/Modules/modules.json`。 |
| Presentation | 所有 Editor window 走 public `Window`／`Presentation`／`RHI`；禁止 private swapchain、backend downcast 與第二套 event pump。 |
| Identity | Project、asset、entity、component、document、panel、command、tool 使用 typed stable serialized ID；pointer、index、path 與 label 都不是 identity。 |
| 修改 | Authoring write 是 authoring thread 上的 transaction；worker 只回傳 immutable、帶 revision 的結果供驗證與 commit。 |
| 文件 | 格式須 versioned、deterministic、atomic replace 並保留未知 field/component；migration 支援 inspect、dry-run、backup、report。 |
| 非同步 | Import、index、thumbnail、build、source-control job 可取消；不得保留 document pointer 或直接改 UI state。 |
| PIE | Play 擁有 cloned world 與獨立 input domain；stop 預設 discard，apply-back 是明確的 diff transaction。 |
| Extension | Extension 只走 versioned Editor SDK capability registry，註冊前驗證 ABI、permission、dependency 與 trust policy。 |
| Accessibility | Keyboard、semantic mirror、contrast、screen-reader bridge 是 ED-M7 release gate；畫面上有 widget 不構成證據。 |
| 證據 | Automated test 證明 contract，target-host capture 證明圖形行為；mock、screenshot 或 compile-only 不可代替不同的指定 gate。 |

## 9. Ownership、threading、error 與 shutdown contract

```text
EditorApplication
  +-- WindowSystem / PresentationDevice / RenderSurface(s)
  +-- EditorUiHost（僅 graphical feature）
  +-- ProjectSession（零或一個）
      +-- AssetIndex + JobCoordinator
      +-- DocumentManager
      |   +-- SceneDocument(s) / PrefabDocument(s)
      |   +-- 每份文件的 SelectionModel + TransactionHistory
      +-- PlaySession（零或一個 cloned Play World）
      +-- ExtensionManager / CapabilityRegistry
```

Owner 使用 RAII 並反向銷毀 child。關閉 project 時，先停止 intake 與 job，再關 document；
document 先於 Runtime service 關閉；GPU resource 只能在 completion value 通過後 retire。UI 只保存
stable ID 或 scoped weak handle，不擁有 ECS、asset、plugin 或 GPU storage pointer。每個 async
result 都攜帶 project generation、target ID、input revision/hash 與 operation ID，因此 close、reload、
undo 或 reimport 之後才完成的結果會 fail closed。

Authoring thread 擁有 event、ImGui、document、selection、transaction、extension callback 與結果
commit。Render submission 只消費 immutable frame packet。Worker 讀 snapshot、寫 staging output，
並以 bounded queue 與 cancellation 回報。File watcher 只送 normalized event；conflict 決策回到
authoring thread。Shutdown 依序停止 intake、取消工作、drain callback 但不 commit、等待必要 job／
GPU value、寫 recovery state，再銷毀 service。

預期失敗使用帶 stable code、可處理 context、operation ID 的 typed result；assertion 只處理內部
invariant。Capability 失敗時停用該能力但保留文件。Write 失敗保留 last-good file，import 失敗保留
舊 artifact；device loss 保留 CPU authoring state，並重建 device-owned resource。

## 10. AI 施工順序

只有 dependency 與 exit gate 通過才能開始下一包。每包依序做 contract/model、automated test、
graphical binding、failure state、target evidence。Portable prerequisite 不會自動關閉 graphical milestone。

### WP0 — Baseline audit 與 evidence ledger

**相依：** 無。**影響：** 全部 milestone。

1. 將每項需求映射到 source、test、owner module，以及 `absent`、`contract-only`、`portable`、
   `graphical`、`target-accepted` 其中一個真實狀態。
2. 記錄 support tier、compiler/backend、feature flag、缺少的 SDK、baseline result 與精確 command；
   區分 pre-existing failure，且不提交 generated evidence。
3. 把每個 open bullet 轉成含 owner、dependency、automated check、manual evidence、rollback 的 gate，
   再選最小的 end-to-end slice。

**Exit gate：** inventory 與 source/test 一致，沒有把 open work 說成完成，下一 slice 有明確驗收。

### WP1 — ED-M0 graphical shell 驗收

**相依：** WP0 與 public Window/Presentation contract。

依 [Dear ImGui integration plan](Editor_ImGui_Integration_Plan.md) 執行其中 WP0～WP8。證明 public
`RenderSurface`、完整 event normalization、stable docking、DPI font rebuild、Unicode／IME candidate
placement、recovery UX、device/surface recovery。直到每個 platform window 都遵守同一 ownership 與
presentation path 才能啟用 multi-viewport。收集真實 display 的 Linux visual/input/recovery 與
Windows DPI/IME 證據。

**Exit gate：** focused plan 所有 gate 在 feature on/off 都通過；關閉 viewport/project/application
後沒有 callback 或 GPU resource 指向已銷毀 owner。

### WP2 — ED-M1 project/content vertical slice

**相依：** WP1。

1. ✅ 完成 create/open/upgrade：canonical root、schema compatibility、lock/read-only、recent project、
   actionable error，且不改 process working directory。
2. 將 deterministic index 綁到以 asset UUID 為 key 的 virtualized Content Browser；加入 breadcrumb、
   search/filter、selection、transactional rename/move/delete 與 loading/error thumbnail state。
3. Typed drag payload 攜帶 project generation 與 asset UUID；修改前驗證 type、target、permission、staleness。
4. ✅ Import/reimport 使用 cancellable job，包含 source/settings hash、dependency、staging、atomic publish、
   bounded progress、structured diagnostic；取消／失敗須保留舊 artifact。
5. ✅ 顯示 forward/reverse dependency 與 cycle；debounce file event，dirty conflict 必須提供 reload/keep/
   compare，禁止覆蓋。

**測試／Gate：** deterministic index/artifact golden、upgrade/corruption、每階段 cancel、stale completion、
watcher burst、drag validation。新 project 必須能全程由 UI import、搜尋、檢查、移動、reimport、recover，
且無 destructive failure path。

### WP3 — ED-M2 scene-authoring vertical slice

**相依：** WP2 與 production serialization/reflection API。

1. ✅ Hierarchy row、expansion、selection anchor、filter、rename、reorder、cycle-safe reparent 全部以 entity／
   document generation 為 key 並 virtualize。
2. Scene View 渲染至 Editor-owned、RHI-neutral texture token；resize 有 hysteresis，舊 GPU resource 依
   completion value retire，每 document 保存 camera。
3. Async picking 攜帶 frame/document/viewport/entity generation；丟棄 stale result，定義 empty、hidden、
   locked、overlap 行為。
4. Reflection adapter 支援 scalar、enum/flags、vector、color、asset/entity reference、array、nested struct；
   顯示 mixed value，保留 unknown component 與 opaque bytes。
5. 每個 gizmo gesture 是單一 `begin/update/commit|cancel` transaction；先定義 world/local、pivot/center、
   snapping、parent、negative scale、multi-selection，再做 polish。
6. 完成 clipboard、duplicate/delete、dirty prompt、save/reload 與 1,000-step undo/redo replay。

**Exit gate：** 使用者可 select、inspect、edit、undo、save、restart 並 visually verify scene；輸出 stable、
deterministic 且不遺失 unknown data。

### WP4 — ED-M3 PIE/debugging vertical slice

**相依：** WP3 與 Runtime snapshot/clone。

1. Freeze source revision 並 clone isolated Play World；Game View、camera、audio、input focus 分離；start
   失敗不能改 Editor World。
2. 實作 `Stopped -> Starting -> Playing <-> Paused -> Stopping -> Stopped`；拒絕非法 transition，
   stop idempotent，step 恰好一個 fixed tick。
3. 按明確 focus/capture 分流 device input 與 shortcut；emergency stop 與 focus recovery 永遠可用。
4. Console 經 bounded thread-safe buffer，含 severity/category/time/source 與 visible drop count；runtime
   inspect snapshot，不保存 relocatable pointer。
5. Stop 預設 discard；apply-back 顯示 supported-field diff，僅 source revision 相符時 commit 一筆
   transaction，否則進 conflict resolution。
6. Debugger attach/detach/pause/location/diagnostic 留在 adapter 後方；Editor 不成為 gameplay `main`。

**Exit gate：** 重複 start/pause/step/stop 不洩漏 world/focus，隔離成立，discard/apply conflict 已明確測試。

### WP5 — ED-M4 prefab、multi-scene 與 collaboration safety

**相依：** WP4 與 stable scene/prefab schema。

實作 prefab isolation、variant、nested stable-property override tree、diff、targeted/full revert、apply、
rebase；加入 additive-scene ownership、load order、cross-scene reference policy、save-all、dirty indicator；
再加入具 backup/report 的 dry-run migration、不取代 good file 的 recovery snapshot、external-change
conflict UX 與 canonical semantic source-control diff。Multi-document operation 先 stage 全部檔案，只能
整組 commit 或完整保留 originals。

**Exit gate：** golden project 通過 nested edit/rebase、additive save、migrate、crash、reopen；external
change 不會靜默抹除 local edit，diff 能指出 stable object/field。

### WP6 — ED-M5 specialized-tool capability platform

**相依：** WP5 與至少一個有 Editor-safe public API 的 production subsystem。

Capability 加入 version、implemented/read-only/unavailable、reason、permission、document type、
contribution。先交付一個薄 reference plugin，證明 edit-preview-save-reopen、undo、unload、missing-
backend，且不存取 private engine。再獨立加入 material/shader、animation、VFX、audio、navigation/
physics、terrain/vegetation、localization；各自定義 schema、preview lifetime、diagnostic、undo boundary、
budget。Plugin 缺少時保留 payload 並提供 read-only fallback，不能丟資料或假裝 compile 成功。

**Exit gate：** reference tool 可安全 load/unload，且能處理 backend absent/failure。

### WP7 — ED-M6 build、profile 與 extension operation

**相依：** WP5 與 WP6 reference extension contract。

序列化 target、configuration、feature、cook root、output policy、toolchain ID。顯示 escaped reproducible
command，離開 UI thread 執行、可 cancel、bounded log，且成功除 zero exit 外還必須有 checksummed
artifact manifest。Remote adapter 使用 authenticated deploy/run/stop/log state，secret 不進 project。
Profiler 使用 monotonic time、bounded memory、drop count、stable ID、versioned export。Plugin install
在載入 code 前驗證 SDK/ABI、manifest、permission、trust/signature、dependency、restart requirement。

**Exit gate：** 相同 manifest input 可重現 output；cancelled/failed/incomplete build 不會報 success；
profiler/plugin failure 不影響 authoring。

### WP8 — ED-M7 hardening 與 release 驗收

**相依：** Release 所選的 WP1～WP7。

1. 以 dataset、machine、build、sample window、percentile 定義 startup、index、memory、frame、interaction
   budget；測 100k entity 與 production asset scale，優先清除 unbounded per-frame work/queue。
2. Soak 重複 project/document/PIE/plugin/device-loss，檢查 leak、deadlock、queue growth、recovery。
3. Migration 涵蓋所有 supported version，並 fuzz corrupt/truncated input；保留 last-good file 與 report。
4. Audit keyboard traversal、visible focus、shortcut conflict、contrast/scaling、semantic mirror、screen reader；
   platform gap 必須是 blocker，或有 owner/date 的 approved exception。
5. 驗證 signature/privacy：不載入 policy 外 plugin、opt-in 前無 telemetry、persist/transmit 前 redaction、
   inspectable queue、opt-out deletion、offline behavior。

**Exit gate：** 宣稱 release 的所有 gate 都在 supported target host 通過，證據已 review，exception 明確，
roadmap 狀態不把 prerequisite 向上取整。

## 11. 跨領域施工 contract

- **Identity：** typed ID 不可混用，透過 owner resolve；ImGui ID 由 stable object/property identity 加
  document generation 產生，label 可在地化；reuse 必須增加 generation。
- **Transaction：** `begin -> preview/update -> validate -> commit|cancel`；記錄 target ID、source revision、
  reversible state、merge key、description；commit 只增加一次 revision，cancel 精確還原，undo 使 stale job 失效。
- **Persistence：** 寫 sibling stage、flush、必要時 read-back validate、atomic replace、最後更新 journal；
  不修改唯一 good copy。Migration chain 必須有 version、preflight、backup、deterministic transform、validate、
  report，且不得 silent downgrade。
- **Job：** 攜帶 operation/project/target/revision、cancellation、progress、bounded diagnostic、staging、status；
  revalidate 後才能 commit。File event 要 normalize/debounce 並辨認 self-write。
- **Rendering：** Preview token 是 RHI-neutral 且 generation-safe；先 allocate replacement，再依 GPU completion
  retire。Empty/loading/error/suspended/minimized/occluded/out-of-date/device-lost 都是明確 state；禁止 cast
  backend handle 或假設 frames-in-flight。
- **Extension：** SDK struct 驗證 size/version 並定義 ownership。Unload 須 revoke registration、cancel job、
  close/fallback document、drain call，再 release library；safe mode 記錄 crash，本計畫不宣稱 native plugin sandbox。

## 12. 技術問題與指定解法

| 問題 | 指定解法 | 禁止捷徑／驗證 |
| --- | --- | --- |
| Portable 工作被當成 graphical 完成 | 分開追蹤 WP0 五種狀態。 | Panel stub/test 數量不能關 graphical gate。 |
| Late job 修改 reopened document | Revalidate project generation、target ID、revision/hash。 | Worker 不 capture model pointer。 |
| Drag 產生大量 undo | Preview 後只 commit 一筆 mergeable transaction；cancel 精確還原。 | 不得每 mouse move 一筆 command。 |
| Watcher 與 Editor write race | 辨識 self-write、debounce/hash、dirty conflict 提示。 | 不得 auto-reload 覆蓋 unsaved edit。 |
| Cancel import 損壞 output | Content-addressed staging，驗證後 atomic publish。 | 不直接覆寫 active artifact。 |
| ECS relocation 破壞 selection | 使用時以 generation resolve typed stable ID。 | 不跨 frame 保存 component address。 |
| Pick 是舊 frame | 驗證 frame/document/viewport/entity generation。 | 不盲收最新完成結果。 |
| Mixed Inspector 遺失值 | 顯示 mixed state，只以 multi-target transaction 改指定 field。 | 不把第一個 selection 複製給全部。 |
| Unknown component/plugin | 保留 opaque payload，顯示 read-only diagnostic。 | Save 不丟未知資料。 |
| PIE 汙染 authoring | Clone world，revision-checked explicit diff/apply。 | 不在 Editor World 跑 simulation。 |
| Multi-file save 部分成功 | 全部 stage/validate，加 recovery manifest，再 coordinated commit。 | 不回報 subset success。 |
| Resize/device loss 提早 free GPU | Generation registry、completion retirement/recovery。 | CPU frame age 不足以判定。 |
| Console/profiler 無限成長 | Bounded batching/backpressure 與 visible drop count。 | 禁止 unlimited history。 |
| Plugin unload 留 callback | Revoke、cancel、fallback/close、drain、unload。 | Unload 時不可仍 reach function pointer。 |
| Build 假成功 | Exit success 加 validated checksummed artifact。 | Exit code 單獨不足。 |
| 從 pixels 推論 accessibility | Semantic bridge 加 keyboard/screen-reader host audit。 | Screenshot/widget test 不足。 |
| Benchmark 無法重現 | 記錄 dataset、machine、build、window、percentile。 | 不用單筆無標示數據宣稱達標。 |

## 13. 驗證與證據

每個 implementation PR 都跑 Linux development gate。若可能影響 module boundary、export、optional
feature、plugin loading 或 Shipping exclusion，另跑 Shipping configure/build。Target-only gate 必須在
該 host 跑；Linux cloud 結果不能宣稱是 Windows/macOS/mobile 證據。

```bash
cmake --preset linux-development
cmake --build --preset linux-development
ctest --preset linux-development

# 連結或 Shipping boundary 變更另跑：
cmake --preset linux-shipping
cmake --build --preset linux-shipping
```

| Gate | Automated evidence | Manual／target-host evidence |
| --- | --- | --- |
| ED-M0 | feature on/off、input、persistence、RHI lifetime/recovery | Linux display、Windows DPI/IME、docking/recovery |
| ED-M1 | deterministic import/index、cancel、corruption、stale completion | import/reimport/dependency/conflict workflow |
| ED-M2 | hierarchy/property/gizmo/pick math、serialization、replay | 多 DPI Scene/Inspector edit-save-reopen |
| ED-M3 | isolation、state machine、fixed-step、apply conflict、stress | focus、pause/step、Console、Game View |
| ED-M4 | rebase、multi-scene atomicity、migration/recovery golden | external-edit conflict、crash drill |
| ED-M5 | SDK/capability/unload/unknown-data test | reference tool、missing-backend mode |
| ED-M6 | reproducibility、cancel/fail、bounded profiler、policy | build/deploy/log/profile/plugin recovery |
| ED-M7 | scale、soak、fuzz、migration、privacy | keyboard、screen reader、contrast、large project |

Manual record 包含 commit/configuration、OS、GPU/driver、display scale、locale/IME、flag、精確步驟、
expected/actual、log/capture、reviewer；須 redact secret、private/telemetry data。Commit 或 configuration
有實質差異的證據無效。

## 14. AI 變更流程與 Definition of Done

每個 slice 依序：讀本 roadmap、focused plan/ADR、相關 README、module graph、tests；核對 source truth；
列出 contract、compatibility、failure mode、rollback、evidence；先補 contract test；施工不得加入 downcast、
global、以 sleep 同步、persisted raw pointer、silent fallback；只 format touched C++；跑精確 gate 並檢查
diff；狀態或 ownership/lifetime/thread/error/deferred-work 改變時同步兩種 roadmap 與 contract README。
未跑的 check 不得寫成 passed。每個 PR 優先是一個可 review vertical slice；schema/SDK 變更附相容與
migration，架構決策先有 ADR 再寫 widget code。

Milestone 完成必須同時滿足：model、graphical workflow、degraded/failure state、persistence、undo boundary
可用；automated 與指定 host gate 通過；compatibility/migration 有文件；shutdown/cancellation/stale
completion/recovery 有 stress coverage；error 可採取行動；文件與 evidence 符合現況；未提交 build
output、local preset、credential、private project 或 sensitive capture。`Editor Preview` 要 ED-M0～M2；
`Creator Alpha` 再要 M3～M4；`Production Beta` 要所選 M5 tool 與 M6～M7。Preview/read-only capability
不提高 enclosing milestone 百分比，也不能免除 target-host gate。
