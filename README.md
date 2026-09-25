# Nexora

> Open-source cross-platform 3D engine architecture and roadmap  
> 開源跨平台 3D 引擎架構與 Roadmap

## English

Nexora is an open-source cross-platform 3D engine initiative focused on a high-performance C++20 core, Zig gameplay, a language-neutral stable C ABI, modern rendering, scalable world systems, and AI-assisted engineering.

This repository contains an executable C++20 engine/runtime baseline in addition to its architecture and implementation plans. The roadmap documents are available in English under `Roadmap/en/`, with their original Traditional Chinese editions preserved under `Roadmap/zh-TW/`.

### Direction

- Windows, macOS, Android, and iOS support
- C++20 engine core with Zig as the primary gameplay language
- DX12, Vulkan, and Metal through a unified RHI direction
- Node + Component authoring with data-oriented runtime storage
- GPU-driven rendering, large-world streaming, networking, animation, physics, AI, and editor tooling as staged capabilities
- AI-assisted development with automated validation gates, reproducible builds, and human review

### Roadmap

See the bilingual document index in [`Roadmap/README.md`](Roadmap/README.md).

Progress is measured against each document's explicit milestone acceptance gates, not by counting
paragraphs or treating a planned scope matrix as implementation. Completed acceptance items contribute only when evidence exists; partial work is recorded inside
the roadmap and never rounded up to a completed milestone. The V1
percentage is specifically the portable contract-foundation scope described below, not production
completion on every target platform.

Completed roadmap items use the green `✅` marker. Every content change must re-evaluate affected
roadmap items and keep this GitHub README's progress and status text synchronized with the evidence;
an unchecked or unmarked item remains incomplete.

| Roadmap | Progress | Basis |
| --- | ---: | --- |
| ✅ [V1 Complete Plan](Roadmap/en/Cross-platform_3D_Engine_V1_Complete_Plan_v1_2.md) | **100%** | 13/13 portable M0–M12 contract foundations delivered; native/product adapters remain separate gates. |
| ✅ [V1 AI Implementation Plan](Roadmap/en/Cross-platform_3D_Engine_V1_AI_Implementation_Technology_and_System_Plan_v1_2.md) | **100%** | Tracks the same accepted portable V1 implementation baseline. |
| [V2 Complete Plan](Roadmap/en/Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md) | **31%** | ✅ V2-M0 through ✅ V2-M2 and ✅ V2-M4 are accepted; V2-M4 covers adaptive/3D partitioning, CellGroups, origin rebasing, HLOD/impostors, persistent deltas, and the headless incremental commandlet. V2-M3 native execution evidence remains open. |
| [V2 AI Implementation Plan](Roadmap/en/Cross-platform_3D_Engine_V2_AI_Implementation_Technology_and_System_Plan_v1_2.md) | **31%** | ✅ V2-M0 through ✅ V2-M2 and ✅ V2-M4 are accepted; V2-M3 contract checks exist, but native parity remains open. |
| [V3 Complete Plan](Roadmap/en/Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md) | **0%** | No V3 delivery milestone has an accepted repository gate. |
| [V3 AI Implementation Plan](Roadmap/en/Cross-platform_3D_Engine_V3_AI_Implementation_Technology_and_System_Plan_v1_3.md) | **0%** | Execution plan only; no V3 milestone accepted. |
| ✅ [Engine API Foundation](Roadmap/en/Engine_API_Foundation_Roadmap.md) | **100%** | ✅ API-M1 through ✅ API-M6 complete for portable scope. |
| ✅ [Window and Native Presentation](Roadmap/en/Window_Presentation_Roadmap.md) | **100%** | ✅ WP-M0 through ✅ WP-M4 are implemented; native backend execution remains a target-host acceptance gate. |
| [Zig Showcase](Roadmap/en/Zig_Showcase_Roadmap.md) | **90%** | ✅ ZS-M0 through ✅ ZS-M4 are complete; ZS-M5 has automated Linux isolated-package launch evidence, while other target-host distribution acceptance remains open. |
| [Graphical Editor](Roadmap/en/Editor_Roadmap.md) | **0% (0/8)** | Repository audit confirms portable foundations for every ED track and an in-progress Dear ImGui shell, but no graphical ED milestone has passed all automated and target-host gates. |
| [Focused Roadmaps AI Plan](Roadmap/en/Focused_Roadmaps_AI_Implementation_Plan.md) | **60%** | Mean of API 100%, Zig Showcase 90%, and Editor 0%, rounded down to 10%. |
| [V1 Visual Showcase](Roadmap/en/V1-Visual-Showcase-Long-Term-Plan.md) | **10%** | Headless entry/reporting prerequisites exist; no windowed phase is complete. |


### Repository status

The repository now builds and tests Foundation, Core, RHI, Renderer, Runtime, API samples, a Zig gameplay consumer, and a headless `NexoraShowcase`. The milestone sections below describe the implemented portable contract foundations and explicitly call out platform or production backends that remain future work.

#### Engine API status

The Engine API is **complete for the portable roadmap scope** defined by the [Engine API Foundation Roadmap](Roadmap/en/Engine_API_Foundation_Roadmap.md). Platform-specific runtime evidence remains a target-platform validation responsibility and is not represented as missing API functionality.

| Track | Status | Available now / remaining gate |
| --- | --- | --- |
| API-M1 Math and geometry | Complete for the roadmap scope | Full math, geometry, transforms, ABI/layout tests, SSE2/NEON paths, independent DirectXMath coordinate goldens, and an executable sample are available; ARM runtime evidence remains target-host validation. |
| API-M2 Foundation data types | Complete for the roadmap scope | UTF-8 strings/views, buffers/spans, UUIDs, names, results, parsing, generational handles, and caller-owned or opaque engine-owned C ABI buffers are implemented and tested. |
| API-M3 VFS and file I/O | Complete for the roadmap scope | Directory, memory, read-only package, and bundle backends; streams, ranged/async reads, mapping, watches, atomic writes, Shipping host-mount restrictions, and >4 GiB sparse-offset gates are implemented. |
| API-M4 Engine services | Complete for the roadmap scope | Monotonic/game/fixed time, versioned deterministic random, configuration, logging, jobs, events, and profiling-marker emission are implemented and tested; JobSystem shutdown now has lifecycle-leak regression coverage for drain, join, capture release, and restart. |
| API-M5 World/game facade | Complete for the roadmap scope | Handle/value-based entity, scene, transform, camera, light, mesh-renderer, physics, character, audio, asset-reference, and input access are implemented and tested. |
| API-M6 Bindings and versioning | Complete for the roadmap scope | A canonical C11 header, machine-readable ABI manifest, C and Zig consumers, append-only compatibility gate, versioned descriptors, real `GameWorld` wire paths, and embedding-owned event/tick hooks are implemented and tested. |

#### Zig gameplay and Showcase status

Zig is no longer only a planned language direction. The repository builds a Zig 0.14.0 gameplay object and ABI smoke consumer. The current headless/static ZS-M1 verification slice has C++ own `main`, engine/world lifetime, fixed and variable updates, offscreen rendering, transactional reload, and shutdown, while Zig mutates a live entity Transform through the public V3 ABI. See the [Showcase README](Apps/Showcase/README.md) for the supported workflow.

ZS-M0 through ZS-M4 are complete: the capability-aware gallery supports camera input, selection raycasts, honest feature overlays and tested fallbacks; dynamic reload now stabilizes files and reports callback/device-loss recovery scopes. Clean-machine distribution acceptance (ZS-M5) remains open.

#### Window and native presentation status

The [Window and Native Presentation Roadmap](Roadmap/en/Window_Presentation_Roadmap.md) is **100% implementation complete**: WP-M0 through WP-M4 provide the module boundary, native window/input implementations, DX12/Vulkan/Metal presentation paths, reusable Showcase/Editor surfaces, and lifecycle/failure hardening. This percentage records implementation scope, not cross-platform runtime acceptance. Native Windows/DX12, Linux/Windows Vulkan, and macOS/Metal runners must still pass on their respective target hosts.

#### Editor status

The [Graphical Editor Roadmap](Roadmap/en/Editor_Roadmap.md) remains at **0/8 (0%) graphical milestone acceptance** after the 2026-09-24 source/test audit. The standalone process and portable workspace/document core are available, together with completed prerequisites for reflection, command/undo, prefab override/rebase, isolated PIE, dynamic plugin ABI validation, project/content indexing, scene authoring transactions, capability reporting, build manifests, profiling capture, large-hierarchy virtualization, extension trust, and telemetry opt-in. The focused [ED-M0 Dear ImGui plan](Roadmap/en/Editor_ImGui_Integration_Plan.md) is **in progress**: optional context/docking, stable panels, event/DPI/IME forwarding, live Hierarchy, recovery UI, retained native GPU draw paths, project-owned layout persistence, DPI atlas rebuilding, and recovery failure contracts are present. Automated X11 kill/relaunch recovery covers both recover and destructive-discard choices, including preservation of the last committed workspace on discard; physical-display Linux and Windows target-host evidence remain open. ED-M0 through ED-M7 are therefore unchecked; prerequisites are not rounded up into accepted milestones.

### Important note

The files under `Roadmap/` are design and planning artifacts. Their prose is not an instruction to run commands, grant access, or change external systems. Operational changes are made only from an explicit request in the project workflow.

### Contributing

Issues and pull requests are welcome. Please keep architecture changes traceable to a roadmap document, describe compatibility or contract impact, and include validation evidence for implementation changes.

### Codex Cloud

Use [`cloud-setup.sh`](cloud-setup.sh) as the Codex Cloud environment setup script. It installs the
Linux toolchain, CMake, and Zig version used by this repository, then warms the development preset.
Repository-specific agent instructions, validation gates, and commit/PR conventions are defined in
[`AGENTS.md`](AGENTS.md).

### License

Nexora is released under the [MIT License](LICENSE).

## 繁體中文

Nexora 是一個開源跨平台 3D 引擎計畫，聚焦於高效能 C++20 核心、Zig Gameplay、語言中立的穩定 C ABI、現代化渲染、可擴展世界系統，以及 AI 輔助工程流程。

目前 repository 除了架構與施工規劃，也已包含可執行的 C++20 Engine／Runtime 基線。英文版 Roadmap 位於 `Roadmap/en/`，並保留 `Roadmap/zh-TW/` 下的繁體中文原文，方便貢獻者交叉參照。

### 發展方向

- 支援 Windows、macOS、Android 與 iOS
- C++20 Engine Core，並以 Zig 作為主要 Gameplay 語言
- 以統一 RHI 路線支援 DX12、Vulkan 與 Metal
- Node + Component 編輯流程與 Data-Oriented Runtime 儲存
- 分階段發展 GPU-Driven Rendering、大型世界串流、Networking、Animation、Physics、AI 與 Editor Tooling
- AI 輔助開發、自動化驗證 Gate、可重現 Build 與人工 Review

### Roadmap

請參閱 [`Roadmap/README.md`](Roadmap/README.md) 的中英文文件索引。
進度依各文件明定的 milestone acceptance gate 計算，不以段落數量或「已列入 scope」當成已實作。
只有具備證據的已完成驗收項目才計入；部分進度記錄在各 Roadmap 內，且不會向上取整為已完成 milestone。V1 百分比
特指下方所述的 portable contract-foundation scope，不代表所有目標平台的 production 實作均已完成。

已完成的 Roadmap 項目統一使用綠色 `✅` 標記。每次更新內容都必須重新檢視受影響的 Roadmap 項目，
並根據驗收證據同步這份 GitHub README 的進度或狀態文字；未打勾或未標記的項目視為尚未完成。

| Roadmap | 進度 | 計算依據 |
| --- | ---: | --- |
| ✅ [V1 完整規劃書](Roadmap/zh-TW/跨平台3D_Engine_V1_完整規劃書_v1_2.md) | **100%** | 13/13 個 portable M0–M12 contract foundation 已交付；native/product adapter 仍為獨立 gate。 |
| ✅ [V1 AI 施工規劃](Roadmap/zh-TW/跨平台3D_Engine_V1_AI施工技術與系統規劃_v1_2.md) | **100%** | 對應同一個已驗收的 portable V1 施工基線。 |
| [V2 完整規劃書](Roadmap/zh-TW/跨平台3D_Engine_V2_完整規劃書_v1_4.md) | **31%** | ✅ V2-M0 至 ✅ V2-M2 與 ✅ V2-M4 已驗收；V2-M4 涵蓋 adaptive／3D partition、CellGroup、origin rebase、HLOD／impostor、persistent delta 與 headless incremental commandlet。V2-M3 native execution 證據仍待完成。 |
| [V2 AI 施工規劃](Roadmap/zh-TW/跨平台3D_Engine_V2_AI施工技術與系統規劃_v1_2.md) | **31%** | ✅ V2-M0 至 ✅ V2-M2 與 ✅ V2-M4 已驗收；V2-M3 contract check 已存在，但 native parity 仍待完成。 |
| [V3 完整規劃書](Roadmap/zh-TW/跨平台3D_Engine_V3_完整規劃書_v1_4.md) | **0%** | 尚無 V3 delivery milestone 通過 repository gate。 |
| [V3 AI 施工規劃](Roadmap/zh-TW/跨平台3D_Engine_V3_AI施工技術與系統規劃_v1_3.md) | **0%** | 僅為施工規劃；尚無 V3 milestone 驗收。 |
| ✅ [Engine API 基礎](Roadmap/zh-TW/Engine_API_基礎_Roadmap.md) | **100%** | ✅ API-M1 至 ✅ API-M6 完成 portable scope。 |
| ✅ [Window 與 Native Presentation](Roadmap/zh-TW/Window_Presentation_Roadmap.md) | **100%** | ✅ WP-M0 至 ✅ WP-M4 已實作；native backend 執行仍為 target-host 驗收 gate。 |
| [Zig Showcase](Roadmap/zh-TW/Zig_Showcase_Roadmap.md) | **90%** | ✅ ZS-M0 至 ✅ ZS-M4 已完成；ZS-M5 現有自動化 Linux isolated-package launch evidence，其他 target-host distribution 驗收仍待完成。 |
| [圖形化 Editor](Roadmap/zh-TW/Editor_Roadmap.md) | **0%（0/8）** | Repository 稽核確認各 ED track 已有 portable foundation，Dear ImGui shell 亦在施工中，但尚無 graphical ED milestone 通過全部 automated 與 target-host gate。 |
| [聚焦 Roadmap AI 施工規劃](Roadmap/zh-TW/聚焦_Roadmap_AI施工技術與系統規劃.md) | **60%** | API 100%、Zig Showcase 90% 與 Editor 0% 的平均，向下取整至 10%。 |
| [V1 可視化 Showcase](Roadmap/zh-TW/V1-Visual-Showcase-Long-Term-Plan.md) | **10%** | 已有 headless entry/reporting 前置；尚無 windowed phase 完成。 |

### Repository 狀態

目前已可建置及測試 Foundation、Core、RHI、Renderer、Runtime、API sample、Zig gameplay consumer 與 headless `NexoraShowcase`。下方里程碑章節會列出已實作的 portable contract foundation，並明確標示仍待完成的平台或 production backend。

#### Engine API 狀態

Engine API 已完成 [Engine API 基礎 Roadmap](Roadmap/zh-TW/Engine_API_基礎_Roadmap.md) 定義的 **portable roadmap scope**。平台特定的 runtime evidence 仍須由目標平台驗證，不視為 API 功能缺漏。

| Track | 狀態 | 現有能力／剩餘 Gate |
| --- | --- | --- |
| API-M1 Math 與幾何 | Roadmap scope 已完成 | 已有完整 math、geometry、transform、ABI/layout tests、SSE2/NEON 路徑、獨立 DirectXMath 座標 golden 與可執行 sample；ARM runtime evidence 仍須在目標主機驗證。 |
| API-M2 基礎資料型別 | Roadmap scope 已完成 | UTF-8 string/view、buffer/span、UUID、name、result、parsing、generational handle，以及 caller-owned 或 opaque engine-owned C ABI buffer 均已實作及測試。 |
| API-M3 VFS 與檔案 I/O | Roadmap scope 已完成 | 已實作 directory、memory、唯讀 package 與 bundle backend、stream、range/async read、mapping、watch、atomic write、Shipping host-mount 限制及 >4 GiB sparse-offset gate。 |
| API-M4 Engine services | Roadmap scope 已完成 | Monotonic/game/fixed time、含版本的 deterministic random、configuration、logging、jobs、events 與 profiling-marker emission 均已有實作及測試；JobSystem shutdown 現有 drain、join、capture 釋放及 restart 的 lifecycle-leak regression coverage。 |
| API-M5 World/game facade | Roadmap scope 已完成 | Handle/value-based entity、scene、transform、camera、light、mesh-renderer、physics、character、audio、asset reference 與 input access 均已實作及測試。 |
| API-M6 Bindings 與版本化 | Roadmap scope 已完成 | 已實作 canonical C11 header、machine-readable ABI manifest、C 與 Zig consumer、append-only compatibility gate、versioned descriptor、連到真實 `GameWorld` 的 wire path，以及由 embedding host 擁有的 event／tick hook，並有測試覆蓋。 |

#### Zig Gameplay 與 Showcase 狀態

Zig 已不只是規劃中的語言方向。Repository 會建置 Zig 0.14.0 gameplay object 與 ABI smoke consumer；目前 headless/static ZS-M1 verification slice 由 C++ 擁有 `main`、Engine／World lifetime、fixed 與 variable update、offscreen rendering、transactional reload 及 shutdown，Zig 則透過公開 V3 ABI 修改真實 entity 的 Transform。支援的操作流程請參閱 [Showcase README](Apps/Showcase/README.md)。

ZS-M0 至 ZS-M4 已完成：capability-aware gallery 支援 camera input、selection raycast、誠實的 feature overlay 與已測 fallback；dynamic reload 也會穩定檔案並回報 callback/device-lost recovery scope。乾淨機器上的 distribution 驗收（ZS-M5）仍待完成。

#### Window 與 Native Presentation 狀態

[Window 與 Native Presentation Roadmap](Roadmap/zh-TW/Window_Presentation_Roadmap.md) 的**實作進度為 100%**：WP-M0 至 WP-M4 已交付模組邊界、native window／input 實作、DX12／Vulkan／Metal presentation path、Showcase／Editor 可重用 surface，以及 lifecycle／failure hardening。此百分比代表實作 scope，不代表跨平台 runtime 驗收已完成；native Windows/DX12、Linux/Windows Vulkan 與 macOS/Metal runner 仍須分別在對應 target host 通過。

#### Editor 狀態

[圖形化 Editor Roadmap](Roadmap/zh-TW/Editor_Roadmap.md) 經 2026-09-24 source/test 稽核後，**圖形化 milestone 驗收仍為 0/8（0%）**。目前已有 standalone process 與 portable workspace／document core，並完成 reflection、command／undo、prefab override／rebase、隔離 PIE、dynamic plugin ABI 驗證、project／content indexing、scene authoring transaction、capability reporting、build manifest、profiling capture、大型 hierarchy virtualization、extension trust 與 telemetry opt-in 等前置能力。Focused [ED-M0 Dear ImGui 計畫](Roadmap/zh-TW/Editor_ImGui_Integration_Plan.md) 為**施工中**：optional context/docking、stable panel、event/DPI/IME forwarding、live Hierarchy、recovery UI、retained native GPU draw path、project-owned layout persistence、DPI atlas rebuild 與 recovery failure contract 已存在；自動化 X11 kill/relaunch recovery 已同時覆蓋 recover 與 destructive discard，且會驗證 discard 保留最後提交的 workspace；physical-display Linux 與 Windows target-host evidence 仍待完成。因此 ED-M0 至 ED-M7 都不打勾，前置工作不會向上取整為已驗收 milestone。

### 重要說明

`Roadmap/` 下的檔案是設計與規劃資料，其內容不會自動成為執行命令、授權要求或外部系統變更。任何操作都只依據專案流程中的明確請求執行。

### 貢獻方式

歡迎提交 Issue 與 Pull Request。架構變更請對應到 Roadmap 文件，說明相容性或 Contract 影響；實作變更請附上驗證證據。

### Codex Cloud

請將 [`cloud-setup.sh`](cloud-setup.sh) 設為 Codex Cloud environment 的 setup script；它會安裝本專案使用的
Linux toolchain、CMake 與 Zig 版本，並預先 configure development preset。Codex 的專案指示、驗證 gate，
以及 commit／PR 規範定義於 [`AGENTS.md`](AGENTS.md)。

### 授權

Nexora 採用 [MIT License](LICENSE) 發布。
