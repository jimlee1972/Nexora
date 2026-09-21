# Nexora 圖形化 Editor Roadmap

> 版本：v1.0｜狀態：規劃基線｜更新：2026-09-21

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
- Engine Core 不依賴 UI framework；UI backend 可替換，第一階段選型須以 docking、IME、accessibility、多 viewport 與維護性 ADR 決定。
- 外掛只能經 versioned Editor SDK 註冊 panel、command、importer 與 inspector，不接觸私有 singleton。

## 3. Milestones

### ED-M0 — Product shell 與 UX contract

確立 supported OS、project/workspace layout、panel IDs、command routing、shortcut conflict、theme/DPI/IME/accessibility、crash recovery；用 UX wireframe 與 ADR 選 UI framework，不先把 mockup 當完成品。

### ED-M1 — Project 與 Asset workspace

建立/開啟/升級 project；Content Browser 支援 search/filter、folder/UUID、drag/drop、import status、dependency與 reimport；background import 有取消、進度、錯誤與 deterministic artifact。

### ED-M2 — Scene authoring core

Hierarchy、Scene View、Inspector、camera controls、select/pick、translate/rotate/scale gizmo、parent/reorder、multi-selection、copy/paste、save/reload。Reflection 產生 property widgets；未知 component 保留 raw data，不靜默遺失。

### ED-M3 — PIE 與 debugging

Game View、play/pause/step、fixed tick、input focus、Editor/Play World 隔離、apply changes policy、Console、entity/component inspection、breakpoint adapter boundary。Zig gameplay 由 Engine Host 載入，Editor 不成為 Zig `main`。

### ED-M4 — Prefab、場景與 collaboration safety

Prefab create/open/variant、override diff/revert/apply、nested rebase；additive scenes；stable serialization、schema migration、autosave/recovery、external-change detection、human-readable diff/merge。先支援安全的 source-control workflow，不先承諾即時多人協作。

### ED-M5 — Specialized tools

Material/shader graph、animation state/curve、particle/VFX、audio mixer、navigation/physics debug、terrain/vegetation、localization。每個工具以 capability plugin 交付，缺 backend 時 read-only 或清楚 unavailable。

### ED-M6 — Build、profile 與 extensibility

Build profiles、cook/package frontend、target/device matrix、remote deploy/log、CPU/GPU/memory/frame profiler、plugin manager、script/API docs。任何「Build Success」必須附 target manifest 與可重現 command。

### ED-M7 — Production hardening

大型 project incremental index、virtualized UI、100k entity hierarchy、長時 soak、workspace migration、corrupt document recovery、signed extension policy、telemetry opt-in/privacy、keyboard-only與螢幕閱讀器 audit。

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

**Editor Preview** 完成 M0～M2；**Creator Alpha** 加入 M3/M4；**Production Beta** 加入至少一組 specialized tools、build/profile 與 hardening。版本標章依實際 gate，不因 panel 存在就標為完成。

## 7. 依賴與非目標

依賴 Engine API M1～M5、window/swapchain、reflection、asset pipeline、scene snapshot 與 Editor SDK。第一版不做完整 visual scripting、Marketplace、雲端協作、電影級工具或所有平台 remote deployment；這些在核心 authoring loop 穩定後另立 Roadmap。
