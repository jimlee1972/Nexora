# Nexora Roadmap

Editor script workflow / 腳本工作流程：
[English delivery plan](en/Editor_Workflow_Delivery_Plan.md) ·
[繁體中文交付計畫](zh-TW/Editor_Workflow_Delivery_Plan.md) ·
[GitHub Epic #741](https://github.com/jimlee1972/Nexora/issues/741).
Acceptance / 驗收：2/9 work orders / 工單（22.2%）；ED-WF-01 ✅ PR #751；ED-WF-02 ✅ PR #754；full graphical milestone acceptance remains
separate / 完整圖形化里程碑另行驗收。

## Progress snapshot / 進度紀錄

Updated / 更新日期：2026-10-15（UTC）

| Roadmap / 規劃 | Progress / 進度 | Basis / 計算與驗收範圍 |
| --- | ---: | --- |
| ✅ V1 Complete + AI plans / V1 完整與 AI 規劃 | **100%** | Existing portable foundation acceptance / 既有 portable foundation 驗收 |
| V2 Complete + AI plans / V2 完整與 AI 規劃 | **46%** | 6/13 milestones accepted; rounded to whole percent / 13 個里程碑已驗收 6 個，四捨五入至整數 |
| V3 Complete + AI plans / V3 完整與 AI 規劃 | **0%** | Planned; no accepted delivery recorded / 規劃中，未記錄已驗收交付 |
| ✅ Engine API Foundation / Engine API 基礎 | **100%** | API-M1–M6 accepted in portable scope / portable 範圍 API-M1～M6 已驗收 |
| ✅ Zig Showcase | **100%** | Source roadmap's weighted acceptance calculation / 沿用原 Roadmap 加權驗收計算 |
| ✅ Window / Native Presentation | **100% implementation / 實作** | WP-M0–M4 implemented; remaining target-host gates tracked separately / WP-M0～M4 已實作，剩餘 target-host gate 獨立追蹤 |
| V1 Visual Identity Showcase / V1 視覺特色 Showcase | **71.4%** | 5/7 milestones accepted, equal milestone count; VIS-M3 and VIS-M6 pending / 依里程碑數等權計算，VIS-M3 與 VIS-M6 待驗收 |
| Graphical Editor / 圖形化 Editor | **0% acceptance / 驗收** | 0/8 graphical milestones accepted; implementation slices already exist / 圖形化里程碑已驗收 0/8，已有實作切片 |
| 2D Engine / 2D 引擎 | **82% capability acceptance / 能力驗收** | 9/11 portable/Linux; P0 6/8; rebuilt VM CPU/software DX12/Zig/keyboard slice passed; full M7 and physical M6 open / Portable／Linux 9/11、P0 6/8；重建 VM CPU／軟體 DX12／Zig／鍵盤切片通過，完整 M7 與實體 M6 待完成 |
| Focused Roadmaps AI Plan / 聚焦 Roadmap AI 施工規劃 | **60%** | Mean of API 100%, Zig 100%, Editor 0%, rounded down to 10% / 三者平均後向下取整至 10% |
| ✅ Transform Rotation and Scale / Transform 旋轉與縮放 | **100% planned phases / 計畫階段** | All five phases complete; graphical Editor acceptance remains separate / 五個階段完成，圖形 Editor 驗收另計 |
| ✅ Entity Parenting / Entity 父子階層 | **100% planned phases / 計畫階段** | 4/4 runtime, wire, Editor data-model and render-sync phases complete / runtime、wire、Editor 資料模型及渲染同步四階段完成 |
| V1 Visual Showcase Long-Term / V1 可視化展示長期規劃 | **Not quantified / 待量化** | Source removed its percentage because no reproducible weighted checklist exists / 原文件因缺少可重現加權清單而移除百分比 |
| Editor ED-M0 ImGui Integration / Editor ED-M0 ImGui 整合 | **Not quantified / 待量化** | Implementation slices exist; target-host gates remain open / 已有實作切片，target-host gate 尚待完成 |
| V2-M3 GPU-Driven Native Execution / V2-M3 GPU-Driven 原生執行 | **Not quantified / 待量化** | Linux Vulkan and Windows DX12 phases accepted; Metal and full parity pending / Linux Vulkan 與 Windows DX12 階段已驗收，Metal 與完整 parity 待完成 |

Percentages summarize documented acceptance, not time spent or remaining effort. The linked
English and Traditional Chinese source documents below retain detailed acceptance evidence.
ADR decisions and asset-sourcing notes do not have implementation percentages. Version plans
and their AI companion plans track the same delivery and must not be counted twice in a total.
Update this snapshot and both language indexes when the underlying acceptance changes; partial
implementation does not complete an enclosing milestone.

The 2D roadmap is an independent capability extension. Its Zig batches now drive native GPU
instancing, with Linux pixel/input evidence; physical Windows gates remain open. Its delivery does not change V1 foundation
acceptance or the existing API/Zig/Editor average in the focused AI plan.

百分比以既有文件的驗收狀態為準，不代表投入工時或剩餘工作量。下方中英文來源連結保留詳細
驗收證據。ADR 決策及素材來源筆記不計實作百分比；版本規劃與其 AI 配套規劃追蹤同一份交付，
不可重複加總為整體進度。來源驗收改變時，同步更新本紀錄及雙語索引；部分實作不代表整個
里程碑完成。待量化項目先保留驗收狀態，待定義明確分母與權重後再記錄百分比。

2D Roadmap 為獨立能力擴充；Zig 批次已接入原生 GPU Instancing，具有 Linux 像素／輸入證據，Windows 實體 Gate 仍待驗收；交付不改變 V1 基礎驗收，亦不納入聚焦 AI 規劃既有的 API／Zig／Editor 平均。

V2 graphics extension / V2 圖形擴充（2026-10-16）：
[V2 Advanced Graphics Capability Plan](en/V2_Advanced_Graphics_Capability_Plan.md) /
[V2 進階圖形能力計畫](zh-TW/V2_Advanced_Graphics_Capability_Plan.md) adds five open V2-GFX gates
for parallel recording, tessellation, DX12 Ultimate, Enhanced Barriers, Work Graphs and newer
shader profiles. The existing 46% is the original 6/13 milestone ratio and excludes this extension;
V2 final acceptance additionally requires all five gates.
新增五個待完成 V2-GFX gate，涵蓋平行錄製、Tessellation、DX12 Ultimate、Enhanced Barriers、
Work Graphs 與新 Shader profile。既有 46% 為原始 6/13 里程碑比例，未計入本次擴充；
V2 最終驗收另須通過全部五個 gate。

✅ V2-GFX-1 capability admission foundation / 能力檢查基礎（2026-10-16）：portable tests and
Linux native Vulkan admission passed; parallel recording and the five full graphics gates remain
open. See the paired graphics plans above for retained evidence. Portable 測試與 Linux native
Vulkan 能力檢查已通過；平行錄製與五個完整圖形 gate 仍待完成，驗收證據見上述雙語計畫。

✅ V2-GFX-1 Vulkan descriptor isolation / Vulkan Descriptor 隔離（2026-10-16）：immutable
per-dispatch snapshots passed 320 exact CPU/native-software comparisons in serial and four-worker
recording; full graph parallelism and performance acceptance remain open. 每次 Dispatch 的不可變
快照已於序列／四個 worker 錄製通過 320 組 CPU／原生軟體裝置精確比對；完整圖形排程平行化
與效能驗收仍待完成。詳細證據見上述雙語圖形計畫。


✅ V2-GFX-1 bounded graph compute recording / 有界 Graph Compute 錄製（2026-10-17）：explicit
opt-in, disjoint buffer-only Vulkan/Validation batches preserve deterministic submission and serial
fallback; Linux native output and failure-containment checks pass. General backend workers,
asynchronous retirement and performance acceptance remain open. 明確 opt-in 的獨立 Buffer Compute
批次已通過 Linux 原生輸出與失敗隔離驗證，保留固定提交及序列 Fallback；完整後端 Worker、
非同步回收與效能驗收仍待完成。詳細證據見上述雙語圖形計畫。

✅ V2-GFX-1 D3D12 buffer recording isolation / D3D12 Buffer 錄製隔離（2026-10-17）：list-owned
upload/state snapshots and stale-list rejection extend bounded disjoint Compute recording to D3D12;
Linux regression/Shipping gates pass and Windows native coverage is required by CI. Private 上傳／
狀態快照及過期清單檢查擴充至 D3D12 有界獨立 Compute 錄製；Linux 回歸／Shipping 通過，
Windows 原生覆蓋由 CI 強制執行。完整圖形 Gate、Metal Worker 與效能驗收仍待完成。

✅ V2-GFX-1 bounded Vulkan allocator reuse / 有界 Vulkan Allocator 重用（2026-10-17）：
default-off completed/unsubmitted command and descriptor pool caches pass Linux native output/lifetime and sync checks.
預設關閉的已完成／未提交 Command／Descriptor Pool 快取通過 Linux 原生輸出、生命週期及同步檢查；完整非同步與效能 Gate 仍開放。

✅ V2-GFX-1 matched recording measurement / 配對錄製量測（2026-10-17）：
real serial/four-worker workloads, separate CPU/GPU intervals, JSON and CPU output oracles pass
Linux regression, Shipping and synchronization validation. 真實串行／四執行緒工作、獨立 CPU／GPU
區間、JSON 與輸出比對通過 Linux 回歸、Shipping 及同步驗證；未宣稱加速、預算或實體驗收。

✅ V2-GFX-1 optional completed GPU submission timing / 選用 GPU 提交完成計時（2026-10-17）：
default-off RHI timestamps, owning completed observations and query retirement pass Linux regression,
native Vulkan timing/output and synchronization validation. 預設關閉的 RHI Timestamp、完成觀察值
與 Query 回收通過 Linux 回歸、原生 Vulkan 計時／輸出及同步驗證；Windows／Metal 原生、
同條件效能預算與實體 GPU 校正仍須驗收，完整平行 Gate 未關閉。

✅ V2-GFX-2 bounded native tessellation / 有界原生曲面細分（2026-10-17）：
portable stage/topology/cache/cooked metadata and Linux Vulkan curved-patch/baked-mesh parity pass
regression, Shipping and native sync validation. Windows HS/DS and Metal baked execution require
hosted evidence; Enhanced Barriers/full gate remain open. Portable 階段／拓撲／快取／Cooked Metadata
與 Linux 原生／網格比對通過回歸及同步驗證；目標平台及完整 Gate 仍待完成。

✅ V2-GFX-1 DXIL artifact stage/profile admission / DXIL Artifact 階段／Profile 檢查
（2026-10-17）：bounded program metadata and pre-PSO profile checks pass Linux regression and
Shipping; real Slang profile/PSO checks are required on Windows CI. 有界 Program Metadata 與
PSO 前 Profile 檢查已通過 Linux 回歸及 Shipping；Windows CI 須驗證真正 Slang Profile／
PSO。完整 Toolchain 矩陣、SM6.9／6.10 Preview 執行及進階 Gate 仍待完成。

✅ V2-GFX-1 Metal compute/worker implementation / Metal Compute／Worker 實作（2026-10-17）：
the real Slang kernel, private bindings/encoder replay, shared-buffer test readback and bounded
workers are wired into required macOS native gates; Linux Slang regression and Shipping pass.
真正 Slang Kernel、私有 Binding／Encoder 重播、Shared-buffer 測試讀回及有界 Worker 已接入
必要 macOS 原生 Gate；Linux Slang 回歸與 Shipping 通過。完整 V2-M3／圖形 Gate 及效能驗收
仍待完成，原生執行結果須由 macOS CI 證明。

✅ V2-GFX-1 immutable shared buffer inputs / 不變的共用 Buffer 輸入（2026-10-17）：
compatible read/read cohorts, D3D12 reader revisions and shared compute/indirect pixel oracles
pass Linux regression, synchronization validation and Shipping. 相容 Read/Read Cohort、D3D12
讀者 Revision、共用 Compute／Indirect 像素 Oracle 已通過 Linux 回歸、同步驗證及
Shipping；Windows／Metal 原生執行須由 CI 證明，完整平行／非同步／效能 Gate 仍待完成。

✅ V2-GFX-1 recording cancellation and nested coordinators / 錄製取消與巢狀 Coordinator
（2026-10-17）：same-pool serial fallback and cooperative batch cancellation pass Linux contract,
native Vulkan output-preservation and Shipping checks. 同池序列 Fallback 與合作式批次取消已通過
Linux Contract、原生 Vulkan 輸出保留及 Shipping 驗證；完整圖形、非同步回收與效能 Gate
仍待完成。詳見雙語 V2 圖形計畫與 Renderer Contract。

✅ V2-GFX-1 bounded color-target graph recording / 有界 Color-target Graph 錄製（2026-10-17）：
transactional DX12/Metal/Validation textures, limited graphics admission and a shared native pixel/
failure/recycle oracle pass Linux regression, Vulkan synchronization validation and Shipping gates.
DX12／Metal／Validation Texture 提交語意、有限 Graphics 檢查及共用原生像素／失敗／回收
Oracle 已通過 Linux 回歸、Vulkan 同步驗證及 Shipping；Windows／Metal 執行須由 Hosted
CI 證明，完整平行、共用讀取、非同步重用及效能 Gate 仍待完成。

✅ V2-GFX-1 Vulkan texture/graphics ownership / Vulkan Texture／Graphics 所有權（2026-10-17）：
private state/revision snapshots and complete framebuffer retirement pass exact serial/four-worker
pixels and synchronization/object validation. 私有狀態／Revision 快照與完整 Framebuffer 回收已
通過序列／四 Worker 像素精確比對及同步／物件 Validation；一般 Graph Graphics、其他後端
Texture 隔離、非同步回收與效能 Gate 仍待完成。詳細證據見雙語 V2 圖形計畫。


✅ V2-GFX-2 portable Enhanced Barrier projection / 可攜映射（2026-10-17）：
Independent sync/access/layout policy and Null unavailability are verified. A default-off D3D12
list7 path and legacy fallback are under native validation; this does not close the full gate.
已驗證同步／存取／Layout 映射與 Null 不可用；預設關閉的 D3D12 list7／Legacy 後備待原生驗證。
See the paired [graphics plan](en/V2_Advanced_Graphics_Capability_Plan.md).

✅ V2-GFX-4 VRS admission/full-rate fallback / 准入與 Full Rate 後備（2026-10-17）：
Portable rate/cache/tier ceilings and native Vulkan full-rate output/lifetime tests pass.
D3D12 2x2 execution remains under validation; full VRS/Ultimate acceptance stays open.
可攜著色率／Cache／Tier 上限與 Vulkan Full Rate 輸出／生命週期通過；D3D12 2x2及完整驗收仍待完成。


✅ V2-GFX-4 Mesh admission/GPU indirect fallback / Mesh 准入與 GPU 後備（2026-10-17）：
Real DXIL metadata/reflection, cooked-stage identity and GPU frustum/indirect output tests pass.
Native D3D12 MS/PS execution remains under validation; full meshlet/Ultimate acceptance stays open.
真實 DXIL／Reflection、Cooked 階段識別與 GPU 剔除／Indirect 輸出通過；原生及完整驗收仍待完成。

## English

This directory contains the version roadmaps and focused capability roadmaps for the Nexora open-source baseline. English editions are under [`en/`](en/), and matching Traditional Chinese editions are under [`zh-TW/`](zh-TW/).

Completed items use the green `✅` marker. After every repository content change, review and update
affected roadmap status using acceptance evidence. Keep detailed task completion and validation
records in the relevant roadmap, module documentation, or evidence files. Update the brief summary
in the repository-root [`README.md`](../README.md) only when overall progress or a major milestone
changes; supporting task completion does not require a root README update. See [AGENTS.md](../AGENTS.md).

Markdown-only edits use the [documentation CI route](../Tools/Build/README.md), including changed
local-link checks and paired updates for bilingual roadmaps, including mapped legacy filenames. CI/configuration,
code and tag changes retain full validation; roadmap implementation acceptance remains separate.

### Document index

| Document | Progress | English edition |
| --- | ---: | --- |
| ✅ V1 Complete Plan | **100%** | [Cross-platform 3D Engine — V1 Complete Plan](en/Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md) |
| ✅ V1 AI Implementation Technology and System Plan | **100%** | [Cross-platform 3D Engine — V1 AI Implementation Technology and System Plan](en/Cross-platform_3D_Engine_V1_AI_Implementation_Technology_and_System_Plan_v1_2.md) |
| V2 Complete Plan | **46%** | [Cross-platform 3D Engine — V2 Complete Plan](en/Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md) |
| V2 AI Implementation Technology and System Plan | **46%** | [Cross-platform 3D Engine — V2 AI Implementation Technology and System Plan](en/Cross-platform_3D_Engine_V2_AI_Implementation_Technology_and_System_Plan_v1_2.md) |
| V3 Complete Plan | **0%** | [Cross-platform 3D Engine — V3 Complete Plan](en/Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md) |
| V3 AI Implementation Technology and System Plan | **0%** | [Cross-platform 3D Engine — V3 AI Implementation Technology and System Plan](en/Cross-platform_3D_Engine_V3_AI_Implementation_Technology_and_System_Plan_v1_3.md) |
| ✅ Engine API Foundation Roadmap | **100%** | [Engine API Foundation Roadmap](en/Engine_API_Foundation_Roadmap.md) |
| ✅ Zig Showcase Roadmap | **100%** | [Zig Showcase and Engine-owned Entry Point Roadmap](en/Zig_Showcase_Roadmap.md) |
| ✅ Window and Native Presentation Roadmap | **100% implementation** | [Window and Native Presentation Roadmap](en/Window_Presentation_Roadmap.md) |
| V1 Visual Showcase Long-Term Plan | **Linux/Windows developer, clean-VM and physical-display slices verified; final acceptance pending** | [V1 Visual Showcase Demo Long-Term Plan](en/V1-Visual-Showcase-Long-Term-Plan.md) |
| V1 Visual Identity Showcase Roadmap | **71.4% milestone acceptance (5/7); ✅ Art direction confirmed; free-asset shortlist recorded; ✅ VIS-M0–M2 / VIS-M4–M5 accepted (5/7); quality/release evidence retained; golden-hour environment/HDR Linux 97/97 + 100s native evidence retained; reference art and physical performance pending** | [V1 Visual Identity Showcase Roadmap](en/V1-Visual-Identity-Roadmap.md) |
| Editor Roadmap | **0% graphical acceptance** | [Graphical Editor Roadmap](en/Editor_Roadmap.md) |
| Project Scene Delivery Plan | **✅ 5/5 scoped implementation phases accepted** | [Startup scene, multi-scene build and runtime transitions](en/Project_Scene_Delivery_Plan.md) |
| 2D Engine Roadmap | **82% portable/Linux capability acceptance (9/11); PC host gates open** | [2D Engine Roadmap](en/2D_Roadmap.md) |
| ✅ ADR-0001: Editor UI Framework | **Accepted** | [ADR-0001: Editor UI Framework](en/ADR-0001-Editor-UI-Framework.md) |
| Editor ED-M0 Dear ImGui Integration Plan | **In progress; ✅ WP0 baseline, Linux Vulkan validation and native atlas lifetime slices; target-host gates open** | [Editor ED-M0 Dear ImGui Integration Plan](en/Editor_ImGui_Integration_Plan.md) |
| V2-M3 GPU-Driven Native Execution Plan | **In progress; Linux Vulkan Phase 2 and Windows/DX12 Phase 3 accepted; Metal/full parity pending** | [V2-M3 GPU-Driven Native Execution Plan](en/V2-M3_GPU_Driven_Native_Execution_Plan.md) |
| Transform Rotation and Scale Plan | **Approved; ✅ all phases complete (data model, persistence, Zig/C wire, Editor gizmo math, docs); Euler Inspector hints are serialized; graphical Editor acceptance remains separate** | [Transform Rotation and Scale Plan](en/Transform_Rotation_Scale_Plan.md) |
| ✅ ADR-0002: Native Scene mesh batches | **Accepted; bounded public batch contract** | [ADR-0002: Native Scene mesh batches](en/ADR-0002-Editor-Scene-Mesh-Batches.md) |
| Entity Parenting Plan | **Approved; ✅ phases 1 (runtime core and Editor unification) and 2 (Zig/C wires and characters under a parent) complete; ✅ phase 3 (gizmo math, sibling order, Hierarchy drag model) complete; ✅ phase 4 (GPU scene sync and camera views through `WorldMatrix`) complete** | [Entity Parenting Plan](en/Entity_Parenting_Plan.md) |
| Focused Roadmaps AI Plan | **60%** | [AI Implementation Technology and System Plan](en/Focused_Roadmaps_AI_Implementation_Plan.md) |

> `Tools/Migration/ScanV1Project.py` (V1-to-V2 migration audit) and `Tools/Production/NexoraTool.py`
> (Clang reflection / DDC / headless toolchain) do not have dedicated roadmap documents. They are
> tracked inside the V2 Complete Plan as milestones V2-M0 and V2-M1 respectively — see that
> document rather than expecting a standalone entry here.

### Recommended reading order

Current execution focus: **V2-M3 GPU-Driven Rendering** remains the next open delivery milestone.
Its portable reference and command-contract checks are present; native compute/indirect execution
and DX12/Vulkan/Metal target-tier parity are the remaining acceptance gates.

1. V1 Complete Plan — the production-foundation baseline.
2. V1 AI Implementation Technology and System Plan — implementation, tooling, and validation contracts.
3. V2 Complete Plan — GPU-driven, large-world, networking, and production-scale extensions.
4. V3 Complete Plan — distributed simulation, GPU simulation, ray tracing, and ML expansion.
5. The V2/V3 AI implementation plans — execution rules and engineering gates for each generation.
6. The focused capability roadmaps — API foundation, window/presentation, Zig showcase, and editor delivery.
7. The V1 Visual Showcase Long-Term Plan — the windowed `NexoraShowcase` demo product this complements the Zig Showcase Roadmap with.
8. The focused-roadmaps AI plan — dependency-aware AI execution, evidence, and review policy.
9. The 2D Engine Roadmap — P0/P1 sprite, asset, query, performance, Windows packaging and authoring capabilities and C/Zig engine control; game rules excluded.

The documents are planning artifacts. They do not themselves authorize commands, credentials, publishing, or changes to external systems.

---

## 繁體中文

本目錄收錄 Nexora 公開基線使用的版本規劃與聚焦能力規劃。英文版位於 [`en/`](en/)，對應的繁體中文版位於 [`zh-TW/`](zh-TW/)。

已完成項目統一使用綠色 `✅` 標記。每次 repository 內容更新後，依驗收證據檢視並更新受影響的
Roadmap 狀態。細部任務完成紀錄與驗證資料保留在相關 Roadmap、模組文件或證據檔案；
只有整體進度或主要里程碑改變時，才更新 repository root [`README.md`](../README.md) 的簡短摘要。
完成 supporting task 不需要更新首頁。規則見 [AGENTS.md](../AGENTS.md)。

純 Markdown 變更使用[文件 CI 分流](../Tools/Build/README.md)，檢查變動文件的本地連結，並要求
中英文 roadmap（含映射配對的異名舊文件）同批更新。CI／設定、程式與 tag 仍執行完整驗證；引擎里程碑驗收獨立追蹤。

### 文件索引

| 文件 | 進度 | 繁體中文版 |
| --- | ---: | --- |
| ✅ V1 完整規劃書 | **100%** | [跨平台 3D Engine — V1 完整規劃書](zh-TW/跨平台3D_Engine_V1_完整規劃書_v1_2.md) |
| ✅ V1 AI 施工技術與系統規劃 | **100%** | [跨平台 3D Engine — V1 AI 施工技術與系統規劃](zh-TW/跨平台3D_Engine_V1_AI施工技術與系統規劃_v1_2.md) |
| V2 完整規劃書 | **46%** | [跨平台 3D Engine — V2 完整規劃書](zh-TW/跨平台3D_Engine_V2_完整規劃書_v1_4.md) |
| V2 AI 施工技術與系統規劃 | **46%** | [跨平台 3D Engine — V2 AI 施工技術與系統規劃](zh-TW/跨平台3D_Engine_V2_AI施工技術與系統規劃_v1_2.md) |
| V3 完整規劃書 | **0%** | [跨平台 3D Engine — V3 完整規劃書](zh-TW/跨平台3D_Engine_V3_完整規劃書_v1_4.md) |
| V3 AI 施工技術與系統規劃 | **0%** | [跨平台 3D Engine — V3 AI 施工技術與系統規劃](zh-TW/跨平台3D_Engine_V3_AI施工技術與系統規劃_v1_3.md) |
| ✅ Engine API 基礎 Roadmap | **100%** | [Engine API 基礎 Roadmap](zh-TW/Engine_API_基礎_Roadmap.md) |
| ✅ Zig Showcase Roadmap | **100%** | [Zig Showcase 與 Engine-owned Entry Point Roadmap](zh-TW/Zig_Showcase_Roadmap.md) |
| ✅ Window 與 Native Presentation Roadmap | **100% 實作** | [Window 與 Native Presentation Roadmap](zh-TW/Window_Presentation_Roadmap.md) |
| V1 可視化展示 Demo 長期規劃 | **Linux／Windows 開發機、乾淨 VM 與實體顯示切片已驗證；最終驗收待完成** | [Nexora V1 可視化展示 Demo 長期規劃](zh-TW/V1-Visual-Showcase-Long-Term-Plan.md) |
| V1 視覺特色 Showcase Roadmap | **71.4% 里程碑驗收（5/7）；✅ 美術方向已確認；已記錄免費素材候選；✅ VIS-M0–M2／VIS-M4–M5 已驗收（5/7）；品質／交付證據已齊備；實機美術／效能待驗收** | [Nexora V1 視覺特色 Showcase Roadmap](zh-TW/V1-Visual-Identity-Roadmap.md) |
| Editor Roadmap | **0% 圖形化驗收** | [圖形化 Editor Roadmap](zh-TW/Editor_Roadmap.md) |
| 專案場景交付計畫 | **✅ 限定實作階段通過 5/5** | [啟動場景、多場景建置與執行時切換](zh-TW/Project_Scene_Delivery_Plan.md) |
| 2D 引擎 Roadmap | **82% Portable／Linux 能力驗收（9/11）；PC 主機 gate 待完成** | [2D 引擎 Roadmap](zh-TW/2D_Roadmap.md) |
| ✅ ADR-0001：Editor UI Framework | **Accepted** | [ADR-0001：Editor UI Framework](zh-TW/ADR-0001-Editor-UI-Framework.md) |
| Editor ED-M0 Dear ImGui 整合計畫 | **施工中；✅ WP0 baseline、Linux Vulkan validation 與 native atlas lifetime slice，target-host gate 待完成** | [Editor ED-M0 Dear ImGui 整合計畫](zh-TW/Editor_ImGui_Integration_Plan.md) |
| V2-M3 GPU-Driven Native Execution 計畫 | **施工中；Linux Vulkan Phase 2 與 Windows/DX12 Phase 3 已驗收；Metal／完整 parity 待完成** | [V2-M3 GPU-Driven Native Execution 計畫](zh-TW/V2-M3_GPU_Driven_Native_Execution_Plan.md) |
| Transform 旋轉與縮放擴充計畫 | **已核准；✅ 所有階段皆已完成（資料模型、持久化、Zig／C wire、Editor gizmo 數學、文件）；Euler Inspector 提示已序列化；圖形 Editor 驗收另計** | [Transform 旋轉與縮放擴充計畫](zh-TW/Transform_Rotation_Scale_Plan.md) |
| ✅ ADR-0002：原生 Scene mesh 批次 | **Accepted；有界公共批次契約** | [ADR-0002：原生 Scene mesh 批次](zh-TW/ADR-0002-Editor-Scene-Mesh-Batches.md) |
| Entity Parenting 計畫 | **已核准；✅ 階段 1（runtime 核心與 Editor 統一）與 2（Zig／C wire 與父物件底下的角色）已完成；✅ 階段 3（gizmo 數學、兄弟順序、Hierarchy 拖曳模型）已完成；✅ 階段 4（透過 `WorldMatrix` 的 GPU scene 同步與攝影機視角）已完成** | [Entity Parenting 計畫](zh-TW/Entity_Parenting_Plan.md) |
| 聚焦 Roadmap AI 施工規劃 | **60%** | [AI 施工技術與系統規劃](zh-TW/聚焦_Roadmap_AI施工技術與系統規劃.md) |

> `Tools/Migration/ScanV1Project.py`（V1 到 V2 的 migration 稽核）與 `Tools/Production/NexoraTool.py`
> （Clang reflection／DDC／headless toolchain）沒有獨立的 roadmap 文件，而是收錄在 V2 完整規劃書的
> V2-M0 與 V2-M1 milestone 內 —— 請直接參閱該文件，不要預期這裡會有獨立條目。

### 建議閱讀順序

目前施工焦點：**V2-M3 GPU-Driven Rendering** 仍是下一個待交付 milestone。Portable reference 與
command-contract check 已就緒；剩餘驗收 gate 是 native compute／indirect execution 與
DX12／Vulkan／Metal target-tier parity。

1. V1 完整規劃書：Production Foundation 基線。
2. V1 AI 施工技術與系統規劃：施工、工具與驗證 Contract。
3. V2 完整規劃書：GPU-Driven、大型世界、Networking 與 Production Scale 擴展。
4. V3 完整規劃書：Distributed Simulation、GPU Simulation、Ray Tracing 與 ML 擴展。
5. V2/V3 AI 施工規劃：各世代的落地規則與工程 Gate。
6. 聚焦能力 Roadmap：API 基礎、Window/Presentation、Zig Showcase 與 Editor 交付。
7. V1 可視化展示 Demo 長期規劃：與 Zig Showcase Roadmap 互補的視窗化 `NexoraShowcase` 展示產品規劃。
8. 聚焦 Roadmap AI 施工規劃：具依賴順序的 AI 執行、證據與審查制度。
9. 2D 引擎 Roadmap：P0／P1 Sprite、資產、查詢、效能、Windows 打包與製作工具與 C／Zig 引擎控制能力，不含遊戲規則。

這些文件是規劃資料，不會自行授權命令、憑證、發布或外部系統變更。

The next VIS-M3 art iteration has retained sandstone/ivy source authoring, expanded paving,
chamfered ring geometry and wind-bent pennants; release captures are retained. Reference
parity and physical target validation remain pending (5/7 VIS milestones accepted).


The planar mirror iteration reuses source meshes through affine instances and shares HDR,
wind and shadow shading. Native reflection tests retain movement/restoration checks. Final
reference parity, crystal optics and physical target performance remain pending (VIS 5/7).
水面倒影迭代以 affine instance 重用網格，與 HDR、風動及陰影共用著色；原生測試保留移動／還原檢查。
最終預覽圖一致性、水晶光學與目標實機效能仍待完成（VIS 5/7）。


Tinted HDR crystal blending and nearest-layer focus now share the native adapters. This remains art refinement; refraction, final reference parity and physical performance acceptance remain open (VIS 5/7).
水晶帶色 HDR 合成與最近可見層景深共用原生後端；折射、最終預覽圖一致性與目標實機效能仍待驗收（VIS 5/7）。

✅ Tessellation hosted acceptance / 曲面細分 Hosted 驗收（2026-10-17）：
Own PR631 merged after all36 checks and both Linux/Windows/macOS workflows passed. Native Vulkan
and D3D12 curved patches and Metal baked fallback are verified; the full V2-GFX-2 gate stays open.
PR631 已通過36檢查及兩輪三平台 CI 後合併；原生曲面與 Metal 後備驗證完成，完整 Gate 維持開放。
✅ PR632 merged after36/36 checks and both native Windows310/310 suites; bounded Enhanced Barriers,
mesh and per-draw VRS are verified. Full graphics/Ultimate gates stay open.
PR632 已通過36檢查及兩輪 Windows310/310 後合併；有界 Enhanced Barriers／Mesh／Per-draw VRS
原生驗收完成，完整圖形／Ultimate Gate 維持開放。

✅ V2-GFX-5 Work Graph library/admission and compute fallback / Work Graph Library、能力檢查與 Compute 後備（2026-10-17）：
real SM6.8 DXC libraries and bounded library parsing pass Linux Development/Shipping and native Vulkan fallback validation.
Windows native node execution is accepted by PR633 below; no full graphics gate closes. 真實 SM6.8 DXC Library、有界解析及 Vulkan Compute 後備
通過 Linux Development／Shipping 與同步驗證；Windows 原生節點已由下列 PR633 驗收，完整圖形 Gate 維持開放。

✅ V2-GFX-3 ray compilation and real compute reference / Ray 編譯及真實 Compute 參考（2026-10-17）：
real Slang/DXC library and inline-query artifacts plus Linux native compute/CPU traversal oracles pass.
Windows bounded AS/native dispatch is accepted by PR633 below; complete DXR/SER acceptance remains pending. 真實 Library／Inline Query 編譯
與 Linux 原生 Compute／CPU 交點比對通過；Windows 有界 AS／原生 Dispatch 已由下列 PR633 驗收，完整 DXR／SER 驗收仍待完成。

✅ V2-GFX-4 min-mip compilation and real compute reference / Min-mip 編譯與真實 Compute 參考（2026-10-17）：
actual feedback/reflection recipes and Linux Development220/220, Shipping and native Vulkan synchronization pass.
Native Windows clear/write/decode remains pending; full streaming/residency/Ultimate gates stay open.
真實 Feedback／Reflection 產物及 Linux Development220/220、Shipping、Vulkan 同步驗證通過；
Windows 原生 Clear／Write／Decode 待驗收，完整 Streaming／Residency／Ultimate Gate 維持開放。

✅ PR633 merged after36 checks / 通過36檢查後合併：both Windows314/314 runs execute native Work Graphs and pipeline/inline rays; Linux365/365 and Metal304/304 compute fallbacks pass / 兩輪 Windows314/314 實際執行原生 Work Graph 與 Pipeline／Inline Ray，Linux365/365、Metal304/304 後備通過。Full gates and percentages stay open/unchanged / 完整 Gate 與百分比維持原狀。

Windows AS/native dispatch and complete DXR/SER acceptance remain pending. 真實 Library／Inline Query 編譯
與 Linux 原生 Compute／CPU 交點比對通過；Windows AS／原生 Dispatch 及完整 DXR／SER 驗收仍待完成。

SM6.9/6.10 isolated compute / 隔離 Compute：optional private SDK721/WARP/DXC preview path and three bounded recipes are in progress; native Windows evidence pending / 私有 Preview 路徑與三種有界程式實作中，Windows 原生證據待驗證。See / 詳見 [English](en/V2_Advanced_Graphics_Capability_Plan.md) / [繁體中文](zh-TW/V2_Advanced_Graphics_Capability_Plan.md). Full gates and milestone percentages remain unchanged / 完整 Gate 與里程碑百分比不變。

✅ Bounded SER / 有界 SER：PR636 validates pinned SM6.9 HitObject/Reorder/Invoke and independent actual-reordering queries on native preview WARP / 固定配方與獨立實際重排查詢已通過原生 preview WARP 驗證。WARP does not reorder threads; physical performance and full gates remain open / WARP 不重排執行緒，實體效能與完整 gate 仍開放。

✅ Bounded DXR1.2 OMM / 有界不透明微貼圖：PR636 validates native OMM-array/BLAS/TLAS and all16 masks against CPU and real microgeometry-GPU references / 原生建置及16種遮罩已通過 CPU／真正微幾何 GPU 比對。Physical/full gates remain open / 實體與完整 gate 仍開放。

✅ Bounded VRS Tier2 images / 有界 Rate Image：PR636 validates immutable native sources and all five SV_ShadingRate witness patterns in both barrier modes / 不可變原生來源與五種硬體 rate witness 已於兩種 barrier 模式驗證。Physical/full gates remain open / 實體與完整 gate 仍開放。

Modern compute lower-profile equivalents are under construction: explicit LongVectorReference, LinearAlgebraReference and GroupSharedReference variants preserve the accepted fixed-recipe buffer ABI on baseline Slang DXIL6.0/SPIR-V/Metal. The existing preferred/fallback policy remains explicit; these variants do not claim SM6.9/6.10 execution. Linux native parity and lifetime/synchronization validation are recorded in the advanced-graphics plan; hosted platform validation remains required before acceptance.

現代運算的較低 profile 等效路徑施工中：明確的 LongVectorReference、LinearAlgebraReference、GroupSharedReference 以基準 Slang DXIL6.0／SPIR-V／Metal 保留已驗收固定配方的 buffer ABI。沿用明確 preferred／fallback 選擇；不宣稱這些變體執行 SM6.9／6.10。Linux 原生對照及生命週期／同步驗證記錄於進階圖形計畫；驗收前仍須完整託管平台驗證。

## HD-2D extension / HD-2D 擴充

[English plan](en/HD2D_Lighting_Plan.md) / [繁體中文工單](zh-TW/HD2D_Lighting_Plan.md): Beads `nexora-m6m`; engineering / 工程 **7/7 (100%)**, physical / 實機 **NOT_RUN**. Separate extension; existing 2D 9/11 and M6/M7 are unchanged / 獨立擴充，不變更既有 2D 9/11 及 M6/M7。
