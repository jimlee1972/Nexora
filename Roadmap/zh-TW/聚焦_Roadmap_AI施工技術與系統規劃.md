# Engine API、Zig Showcase、Editor：AI 施工技術與系統規劃

> 版本：v1.0｜狀態：施工中｜更新：2026-09-24


> **進度：60%**（API 100%、Zig Showcase 90%、Editor 0%；算術平均 63.3%，向下取整至 10%。）

**已驗收 delivery：** ✅ Engine API Foundation portable scope，以及 ✅ Zig Showcase ZS-M0 至 ZS-M4。**待辦：** Zig Showcase ZS-M5 乾淨 target distribution 驗收，以及全部圖形化 Editor delivery gates。

## 1. 分析結論

三份 Roadmap 不適合讓 AI 平行「把檔案生出來」：API contract 是上游，Showcase 是第一個外部 consumer，Editor 是高複雜度 consumer。建議關鍵路徑為 **API conventions → Math/Text/VFS → C ABI/Zig binding → Engine-owned Showcase → Editor shell/authoring → PIE/tools**。AI 擅長 bounded implementation、adapter、測試矩陣與文件同步；人類必須決定 ABI、UX、第三方相依、資安與 release status。

## 2. AI 施工系統

### 2.1 Work packet

每個 AI 任務必須包含：roadmap ID、問題與非目標、可修改路徑、前置 contract、public API diff、ownership/lifetime/thread/error/determinism、驗收 command、平台矩陣、rollback。缺少 contract 時 AI 只能提出 ADR/prototype，不得猜測後直接固化 public API。

### 2.2 四角色流水線

1. **Planner**：把 milestone 分成小於一個 review unit 的 dependency DAG。
2. **Implementer**：只修改 packet 範圍，提交 production code、tests、docs。
3. **Verifier**：從 contract 產生 boundary/failure/concurrency/ABI tests，不以 implementer 自述為證據。
4. **Reviewer**：檢查 public surface、平台假設、資安、效能與 status wording；高風險 gate 由 human owner 核准。

角色可由同一模型分時執行，但 context、輸出與證據要分離；禁止 verifier 只重述 patch。

### 2.3 Source of truth 與產物

Roadmap/ADR 定義意圖；header/schema 定義 contract；tests 定義可執行行為；generated binding 必須可重現。CI 保存 ABI manifest/diff、test report、benchmark delta、fuzz corpus、headless Showcase report、Editor golden/migration result。AI 不可用 README 宣稱覆蓋尚未在 target host 執行的平台。

## 3. 各 Roadmap 所需 AI 技術

### 3.1 Engine API

- AST/schema inventory：盤點已有型別，避免 duplicate Vector/Transform；產生 public/internal dependency map。
- Contract-first generation：由 canonical schema 驗證 C header、Zig extern、ABI manifest與 reference docs；generated file 禁止手改。
- Property/metamorphic testing：向量恆等式、quaternion round trip、TRS tolerance；不可只 snapshot 單一數值。
- Fuzz與negative generation：UTF-8、path、parser、descriptor sizes、handle generations、async cancel/shutdown。
- ABI guardian：比較 symbol、calling convention、layout、enum value、ownership annotation；breaking change 阻擋 merge。
- Performance agent 只能報測量與 confidence，不得自動降低 tolerance 來讓 benchmark 過關。

### 3.2 Zig Showcase

- 產生/檢查 Zig wrapper 時，建立 C++ fake host 與 Zig consumer 的雙向 conformance vectors。
- Scenario DSL 描述 scripted input、expected events/counters與 capability requirements；headless 先判 correctness，image diff 僅補充。
- Reload chaos：隨機 callback failure、outstanding job、state schema N/N-1、device loss、shutdown ordering。
- Visual agent 可分類明顯破圖，但不得單憑 screenshot 宣告 render 正確；GPU capture與counter是必要證據。
- 靜態檢查禁止 Zig import platform/RHI private headers，並驗證 binary entry point 來自 C++ target。

### 3.3 Editor

- AI 先由 user story 產生 state machine、command/undo model與wireframe，再實作 widget；避免 UI code 先決定資料 contract。
- Model-based test 產生長序列 edit/undo/redo/save/reload/PIE；比較 canonical scene state。
- Synthetic projects（小、中、100k entities、corrupt/old schema）作為可版本化 fixture。
- 視覺 regression 覆蓋 DPI/theme/locale，但 focus、keyboard、accessibility tree 需結構化 assertion 和人工 audit。
- Asset importer 與 plugin 視為不可信輸入：隔離、resource limit、fuzz、簽章/權限政策由 security reviewer 決定。

## 4. 分波次施工與並行限制

| Wave | 可並行工作 | 禁止並行/完成條件 |
| --- | --- | --- |
| 0 | inventory、ADR、contract tables、fixtures | ABI conventions 未核准前不產生 bindings |
| 1 | Math、Text prototype、VFS test harness | 共用 error/allocator contract 先合併 |
| 2 | C ABI、Zig wrapper、fake host | ABI snapshot與跨 allocator gate 通過 |
| 3 | Showcase host、headless scenario、第一個 scene | Engine-owned lifecycle gate 通過 |
| 4 | Editor shell、document model、asset browser | 不直接 fork 私有 Runtime API |
| 5 | PIE、reload、prefab、specialized tool pilots | world isolation/transaction recovery 通過 |
| 6 | platform backends、performance、distribution | 只能在實測 host 更新 support matrix |

同一 public header 同時只能有一個 owner；agents 可平行新增不同 tests/adapter，但不可各自設計相互衝突的 ABI。跨 Roadmap integration 固定在 wave exit 進行，不讓長分支累積。

## 5. Gate、風險分級與 Human-in-the-loop

- **低風險**：private refactor、額外 tests/docs；完整 CI 後一般 review。
- **中風險**：新 non-breaking API、serialization minor migration、UI workflow；owner review + compatibility evidence。
- **高風險**：ABI break、allocator/ownership、threading、file sandbox、plugin/import execution、data migration、release signing；ADR、threat model、兩位 human approvers、target-host evidence。

必要 gate：format/static analysis、unit/property/fuzz、sanitizers、ABI diff、deterministic serialization、module graph、headless integration。GUI/GPU/platform gate 分開記錄，缺環境顯示 `NOT RUN`，不是 `PASS`。

## 6. Prompt 與 review template

AI 任務 prompt 固定要求：引用 contract 段落；列 assumptions；先寫 failure tests；禁止新增未核准 dependency；不跨 ABI 傳 STL/Zig slice；更新中英文文件；回報 commands、target、結果與未驗範圍。Review 固定問：誰配置/釋放？borrow 多久？哪個 thread？shutdown/reload 怎樣？版本不符怎樣？資料能否 deterministic round trip？能力缺少時是否明示？

## 7. 衡量與停止條件

衡量 lead time、首次 review 通過率、escaped defect、flaky rate、ABI break、fuzz discoveries、benchmark regression、AI patch revert rate；不以生成行數衡量。若同類 failure 重複、contract 持續變動、測試 oracle 不可靠或 target platform 無法驗證，停止自動施工，回到 ADR/fixture/human prototype。AI 不可自行修改 gate 來使自己的 patch 通過。

## 8. 完成定義

AI 系統完成不是「能大量產 code」，而是每個 milestone 都可追溯到 contract、具有獨立 verifier、能復現產物、清楚標記平台證據，且 human owner 能拒絕/回滾。三份 Roadmap 的共同 release gate 是：公開 API 穩定、Zig Showcase 只走公開面、Editor 也沒有私有捷徑。
