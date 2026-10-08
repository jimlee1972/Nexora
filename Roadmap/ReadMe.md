# Nexora Roadmap

## Progress snapshot / 進度紀錄

Updated / 更新日期：2026-10-08（Asia/Taipei）

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

百分比以既有文件的驗收狀態為準，不代表投入工時或剩餘工作量。下方中英文來源連結保留詳細
驗收證據。ADR 決策及素材來源筆記不計實作百分比；版本規劃與其 AI 配套規劃追蹤同一份交付，
不可重複加總為整體進度。來源驗收改變時，同步更新本紀錄及雙語索引；部分實作不代表整個
里程碑完成。待量化項目先保留驗收狀態，待定義明確分母與權重後再記錄百分比。

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
