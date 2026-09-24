# Nexora Roadmap

## English

This directory contains the version roadmaps and focused capability roadmaps for the Nexora open-source baseline. English editions are under [`en/`](en/), and matching Traditional Chinese editions are under [`zh-TW/`](zh-TW/).

Completed items use the green `✅` marker. After every repository content change, affected roadmap
status and the progress/status summary in the repository-root [`README.md`](../README.md) must be
updated together and remain evidence-based.

### Document index

| Document | Progress | English edition |
| --- | ---: | --- |
| ✅ V1 Complete Plan | **100%** | [Cross-platform 3D Engine — V1 Complete Plan](en/Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md) |
| ✅ V1 AI Implementation Technology and System Plan | **100%** | [Cross-platform 3D Engine — V1 AI Implementation Technology and System Plan](en/Cross-platform_3D_Engine_V1_AI_Implementation_Technology_and_System_Plan_v1_2.md) |
| V2 Complete Plan | **23%** | [Cross-platform 3D Engine — V2 Complete Plan](en/Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md) |
| V2 AI Implementation Technology and System Plan | **23%** | [Cross-platform 3D Engine — V2 AI Implementation Technology and System Plan](en/Cross-platform_3D_Engine_V2_AI_Implementation_Technology_and_System_Plan_v1_2.md) |
| V3 Complete Plan | **0%** | [Cross-platform 3D Engine — V3 Complete Plan](en/Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md) |
| V3 AI Implementation Technology and System Plan | **0%** | [Cross-platform 3D Engine — V3 AI Implementation Technology and System Plan](en/Cross-platform_3D_Engine_V3_AI_Implementation_Technology_and_System_Plan_v1_3.md) |
| ✅ Engine API Foundation Roadmap | **100%** | [Engine API Foundation Roadmap](en/Engine_API_Foundation_Roadmap.md) |
| Zig Showcase Roadmap | **90%** | [Zig Showcase and Engine-owned Entry Point Roadmap](en/Zig_Showcase_Roadmap.md) |
| ✅ Window and Native Presentation Roadmap | **100% implementation** | [Window and Native Presentation Roadmap](en/Window_Presentation_Roadmap.md) |
| V1 Visual Showcase Long-Term Plan | **10%** | [V1 Visual Showcase Demo Long-Term Plan](en/V1-Visual-Showcase-Long-Term-Plan.md) |
| Editor Roadmap | **0% graphical acceptance** | [Graphical Editor Roadmap](en/Editor_Roadmap.md) |
| ✅ ADR-0001: Editor UI Framework | **Accepted** | [ADR-0001: Editor UI Framework](en/ADR-0001-Editor-UI-Framework.md) |
| Editor ED-M0 Dear ImGui Integration Plan | **In progress; no WP exit gate accepted** | [Editor ED-M0 Dear ImGui Integration Plan](en/Editor_ImGui_Integration_Plan.md) |
| V2-M3 GPU-Driven Native Execution Plan | **In progress; ✅ Phase 1a complete** | [V2-M3 GPU-Driven Native Execution Plan](en/V2-M3_GPU_Driven_Native_Execution_Plan.md) |
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

已完成項目統一使用綠色 `✅` 標記。每次 repository 內容更新後，必須一併更新受影響的
Roadmap 狀態與 repository root [`README.md`](../README.md) 的進度／狀態摘要，且所有完成標記都必須有驗收證據。

### 文件索引

| 文件 | 進度 | 繁體中文版 |
| --- | ---: | --- |
| ✅ V1 完整規劃書 | **100%** | [跨平台 3D Engine — V1 完整規劃書](zh-TW/跨平台3D_Engine_V1_完整規劃書_v1_2.md) |
| ✅ V1 AI 施工技術與系統規劃 | **100%** | [跨平台 3D Engine — V1 AI 施工技術與系統規劃](zh-TW/跨平台3D_Engine_V1_AI施工技術與系統規劃_v1_2.md) |
| V2 完整規劃書 | **23%** | [跨平台 3D Engine — V2 完整規劃書](zh-TW/跨平台3D_Engine_V2_完整規劃書_v1_4.md) |
| V2 AI 施工技術與系統規劃 | **23%** | [跨平台 3D Engine — V2 AI 施工技術與系統規劃](zh-TW/跨平台3D_Engine_V2_AI施工技術與系統規劃_v1_2.md) |
| V3 完整規劃書 | **0%** | [跨平台 3D Engine — V3 完整規劃書](zh-TW/跨平台3D_Engine_V3_完整規劃書_v1_4.md) |
| V3 AI 施工技術與系統規劃 | **0%** | [跨平台 3D Engine — V3 AI 施工技術與系統規劃](zh-TW/跨平台3D_Engine_V3_AI施工技術與系統規劃_v1_3.md) |
| ✅ Engine API 基礎 Roadmap | **100%** | [Engine API 基礎 Roadmap](zh-TW/Engine_API_基礎_Roadmap.md) |
| Zig Showcase Roadmap | **90%** | [Zig Showcase 與 Engine-owned Entry Point Roadmap](zh-TW/Zig_Showcase_Roadmap.md) |
| ✅ Window 與 Native Presentation Roadmap | **100% 實作** | [Window 與 Native Presentation Roadmap](zh-TW/Window_Presentation_Roadmap.md) |
| V1 可視化展示 Demo 長期規劃 | **10%** | [Nexora V1 可視化展示 Demo 長期規劃](zh-TW/V1-Visual-Showcase-Long-Term-Plan.md) |
| Editor Roadmap | **0% 圖形化驗收** | [圖形化 Editor Roadmap](zh-TW/Editor_Roadmap.md) |
| ✅ ADR-0001：Editor UI Framework | **Accepted** | [ADR-0001：Editor UI Framework](zh-TW/ADR-0001-Editor-UI-Framework.md) |
| Editor ED-M0 Dear ImGui 整合計畫 | **施工中；尚無 WP 通過 exit gate** | [Editor ED-M0 Dear ImGui 整合計畫](zh-TW/Editor_ImGui_Integration_Plan.md) |
| V2-M3 GPU-Driven Native Execution 計畫 | **施工中；✅ Phase 1a 已完成** | [V2-M3 GPU-Driven Native Execution 計畫](zh-TW/V2-M3_GPU_Driven_Native_Execution_Plan.md) |
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
