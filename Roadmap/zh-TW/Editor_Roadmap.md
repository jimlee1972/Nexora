# Nexora 圖形化 Editor Roadmap

> 版本：v1.2｜狀態：AI 可執行交付計畫｜更新：2026-09-24

> **進度：0%**（ED-M0～ED-M7 尚無任一 milestone 通過圖形化 Editor 驗收；
> 已完成的 Runtime/Editor SDK 前置不向上取整為 Editor milestone。）

**已完成前置：** ✅ reflection metadata；✅ command/undo data model；✅ prefab override/rebase；
✅ isolated PIE session；✅ dynamic plugin ABI gate；✅ standalone process 與 portable
workspace/document core。**待辦：** window/docking/UI shell、graphical views、authoring workflows
與 production hardening。

### Repository 完成度稽核（2026-09-24）

本稽核明確區分「已打勾的 implementation foundation」與「已驗收的 graphical milestone」。Source
與 contract test 能確認下列已存在的 foundation；目前沒有任何 ED milestone 同時通過完整 automated
與 target-host gate，因此整體圖形化驗收仍是 **0/8（0%）**。

| Scope | Repository 證據 | 已驗收 |
| --- | --- | :---: |
| ED-M0 shell foundation | Standalone process、optional ImGui host、stable panel、initial docking、input/DPI/IME forwarding、live Hierarchy、recovery modal、retained native GPU rendering、project layout persistence 與 recovery failure contract 已存在。Real-process recovery 與 Linux/Windows host evidence 仍待完成。 | [ ] |
| ED-M1 project/assets | Portable create/open、deterministic indexing/search、virtualized Content Browser state、breadcrumb／selection、transactional mutation、typed generation-safe drag payload、dependency／cycle inspection、transactional reimport、watcher debounce 與 dirty-conflict decision 已存在。Graphical workflow 驗收仍待完成。 | [ ] |
| ED-M2 scene authoring | Portable hierarchy/selection、reparent、multi-selection、clipboard、transform transaction、undo、atomic save/reload 已存在。Scene View、Inspector、picking、camera、gizmo 與 reflected graphical widget 仍待完成。 | [ ] |
| ED-M3 PIE/debugging | Isolated `PlaySession`、fixed tick、pause/step、focus policy、discard 與 explicit transform apply-back 已存在。Graphical Game View、Console/runtime inspection 與 debugger integration 仍待完成。 | [ ] |
| ED-M4 prefab/scenes | Portable override diff/revert/apply、variant 與 nested rebase 已存在。Graphical prefab/multi-scene、migration/recovery、conflict 與 source-control workflow 仍待完成。 | [ ] |
| ED-M5 specialized tools | Stable capability ID 與誠實的 implemented/read-only/unavailable state 已存在。尚無 production graphical reference tool 通過 edit-preview-save 驗收。 | [ ] |
| ED-M6 build/profile/extensions | Portable build manifest/checksum 與 monotonic profile capture 已存在。Graphical build/deploy/log/profile/plugin-manager workflow 仍待完成。 | [ ] |
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
- 待驗收：具真實 display 的 Linux visual／input／recovery 證據，以及 Windows DPI／IME 證據；
  target-host gate 通過之前 ED-M0 仍維持 open。

### ED-M1 — Project 與 Asset workspace

建立、開啟與升級 project；Content Browser 支援 search/filter、folder/UUID、drag/drop、import
status、dependency 檢視與 reimport；background import 必須提供取消、進度與可採取行動的錯誤，
並產生 deterministic artifact。

- ✅ 已實作 project create/open、deterministic content-tree indexing、UUID/path search/filter、
  cancellation、progress、可檢查錯誤與 deterministic artifact hash。
- ✅ 已實作並測試 portable virtualized Content Browser／breadcrumb／selection model、
  transactional rename／move／delete、typed generation-safe drag validation、dependency／cycle
  inspection、transactional reimport、watcher debounce 與明確的 dirty-conflict decision。
- 待辦：圖形化 Content Browser、drag/drop、dependency inspection 與 reimport UX 驗收。

### ED-M2 — Scene authoring core

Hierarchy、Scene View、Inspector、camera controls、selection/picking、translate/rotate/scale gizmo、
parent/reorder、multi-selection、copy/paste、undo/redo 與 save/reload。Reflection 產生 property
widgets；未知 component 保留 raw data，不靜默遺失。

- ✅ Editor Core 已實作 stable-ID hierarchy/selection、cycle-safe reparenting、multi-selection、
  clipboard duplication、transform transaction、undo 與 atomic scene save/reload。
- ✅ 已實作並測試 portable Inspector property adapter 與 mixed-value multi-selection、opaque
  unknown-component round trip、可安全取消的 gizmo transaction state machine、generation-safe
  asynchronous picking、atomic camera persistence、1,000-step undo/redo replay，以及 corrupt scene
  的既有狀態保留。
- 待辦：圖形化 Hierarchy／Scene／Inspector、renderer-backed picking、camera controls、gizmo、
  reflected widget 與 unknown-component visual workflow。ED-M2 exit 仍需 UI 中完成
  select／edit／undo／save／restart 驗收與視覺證據。

### ED-M3 — PIE 與 debugging

Game View、play/pause/step、fixed tick、input focus、Editor/Play World 隔離、apply changes policy、Console、entity/component inspection、breakpoint adapter boundary。Zig gameplay 由 Engine Host 載入，Editor 不成為 Zig `main`。

- ✅ Portable `PlaySession` prerequisite 已涵蓋隔離 Play World ownership、fixed tick、
  play/pause/step、input-focus policy、預設丟棄及明確 transform apply-back。
- 待辦：圖形化 Game View、Console/runtime inspection 與 debugger 整合。

### ED-M4 — Prefab、場景與 collaboration safety

Prefab create/open/variant、override diff/revert/apply、nested rebase；additive scenes；stable serialization、schema migration、autosave/recovery、external-change detection、human-readable diff/merge。先支援安全的 source-control workflow，不先承諾即時多人協作。

- ✅ Portable prefab prerequisite 已涵蓋可檢視 override diff、單筆／全部 revert、immutable
  apply、variant 與 nested-path rebase。
- 待辦：圖形化 workflow、additive scene tooling、migration、recovery 與 source-control diff/merge。

### ED-M5 — Specialized tools

Material/shader graph、animation state/curve、particle/VFX、audio mixer、navigation/physics debug、terrain/vegetation、localization。每個工具以 capability plugin 交付，缺 backend 時 read-only 或清楚 unavailable。

- ✅ Portable capability registry 強制 stable tool ID 與誠實的 implemented/read-only/unavailable
  狀態，fallback 必須附原因。
- 待辦：由各 production subsystem 支援的圖形化 specialized tool 與 capability plugin。

### ED-M6 — Build、profile 與 extensibility

Build profiles、cook/package frontend、target/device matrix、remote deploy/log、CPU/GPU/memory/frame profiler、plugin manager、script/API docs。任何「Build Success」必須附 target manifest 與可重現 command。

- ✅ Portable build frontend 會驗證並 atomic 寫入 target/configuration/command 與帶 checksum
  的 artifact manifest；monotonic CPU/GPU/memory frame capture 已實作。
- 待辦：圖形化 frontend、remote deployment/log、live profiler 整合與 plugin manager。

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

1. 完成 create/open/upgrade：canonical root、schema compatibility、lock/read-only、recent project、
   actionable error，且不改 process working directory。
2. 將 deterministic index 綁到以 asset UUID 為 key 的 virtualized Content Browser；加入 breadcrumb、
   search/filter、selection、transactional rename/move/delete 與 loading/error thumbnail state。
3. Typed drag payload 攜帶 project generation 與 asset UUID；修改前驗證 type、target、permission、staleness。
4. Import/reimport 使用 cancellable job，包含 source/settings hash、dependency、staging、atomic publish、
   bounded progress、structured diagnostic；取消／失敗須保留舊 artifact。
5. 顯示 forward/reverse dependency 與 cycle；debounce file event，dirty conflict 必須提供 reload/keep/
   compare，禁止覆蓋。

**測試／Gate：** deterministic index/artifact golden、upgrade/corruption、每階段 cancel、stale completion、
watcher burst、drag validation。新 project 必須能全程由 UI import、搜尋、檢查、移動、reimport、recover，
且無 destructive failure path。

### WP3 — ED-M2 scene-authoring vertical slice

**相依：** WP2 與 production serialization/reflection API。

1. Hierarchy row、expansion、selection anchor、filter、rename、reorder、cycle-safe reparent 全部以 entity／
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
