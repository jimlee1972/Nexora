# Nexora 圖形化 Editor Roadmap

> 版本：v1.1｜狀態：規劃基線｜更新：2026-09-23

> **進度：0%**（ED-M0～ED-M7 尚無任一 milestone 通過圖形化 Editor 驗收；
> 已完成的 Runtime/Editor SDK 前置不向上取整為 Editor milestone。）

**已完成前置：** ✅ reflection metadata；✅ command/undo data model；✅ prefab override/rebase；
✅ isolated PIE session；✅ dynamic plugin ABI gate；✅ standalone process 與 portable
workspace/document core。**待辦：** window/docking/UI shell、graphical views、authoring workflows
與 production hardening。

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
- 待辦：圖形化 Content Browser、drag/drop、dependency inspection 與 reimport UX。

### ED-M2 — Scene authoring core

Hierarchy、Scene View、Inspector、camera controls、selection/picking、translate/rotate/scale gizmo、
parent/reorder、multi-selection、copy/paste、undo/redo 與 save/reload。Reflection 產生 property
widgets；未知 component 保留 raw data，不靜默遺失。

- ✅ Editor Core 已實作 stable-ID hierarchy/selection、cycle-safe reparenting、multi-selection、
  clipboard duplication、transform transaction、undo 與 atomic scene save/reload。
- 待辦：圖形化 Hierarchy/Scene/Inspector、picking、camera controls、gizmo、reflection widget
  與 unknown-component visual workflow。

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
