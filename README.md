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
| [V2 Complete Plan](Roadmap/en/Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md) | **46%** | ✅ V2-M0 through ✅ V2-M2 and ✅ V2-M4 through ✅ V2-M6 are accepted. V2-M7 now includes bounded multi-world self-play coordination but remains in progress; V2-M9 through V2-M12 also have unaccepted portable foundations, while V2-M3 and V2-M8 remain open. |
| [V2 AI Implementation Plan](Roadmap/en/Cross-platform_3D_Engine_V2_AI_Implementation_Technology_and_System_Plan_v1_2.md) | **46%** | Tracks the same accepted V2 baseline plus the in-progress V2-M7 and V2-M9–M12 portable foundations; production backends, distributed infrastructure, cross-device diagnostics, and hardening acceptance remain open. |
| [V3 Complete Plan](Roadmap/en/Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md) | **0%** | No V3 delivery milestone has an accepted repository gate. |
| [V3 AI Implementation Plan](Roadmap/en/Cross-platform_3D_Engine_V3_AI_Implementation_Technology_and_System_Plan_v1_3.md) | **0%** | Execution plan only; no V3 milestone accepted. |
| ✅ [Engine API Foundation](Roadmap/en/Engine_API_Foundation_Roadmap.md) | **100%** | ✅ API-M1 through ✅ API-M6 complete for portable scope. |
| ✅ [Window and Native Presentation](Roadmap/en/Window_Presentation_Roadmap.md) | **100%** | ✅ WP-M0 through ✅ WP-M4 are implemented; Windows/DX12 WP-M1/WP-M2 runtime acceptance is recorded, while Showcase, Vulkan/Metal, and other native-host acceptance remain separate gates. |
| ✅ [Zig Showcase](Roadmap/en/Zig_Showcase_Roadmap.md) | **100%** | ✅ ZS-M0 through ✅ ZS-M5 are complete; the independently provisioned Windows clean-machine Development package acceptance is recorded with 16/16 checksums and a PASS launch report. |
| [Graphical Editor](Roadmap/en/Editor_Roadmap.md) | **0% (0/8)** | Repository audit confirms portable foundations for every ED track and an in-progress Dear ImGui shell, but no graphical ED milestone has passed all automated and target-host gates. |
| [Focused Roadmaps AI Plan](Roadmap/en/Focused_Roadmaps_AI_Implementation_Plan.md) | **60%** | Mean of API 100%, Zig Showcase 100%, and Editor 0%, rounded down to 10%. |
| [V1 Visual Showcase](Roadmap/en/V1-Visual-Showcase-Long-Term-Plan.md) | **10%** | Phase A is implemented pending native acceptance; Phase B now has a ✅ portable scene-frame/resource foundation, while native binding and the interactive Rendering Room remain open. |


### Repository status

The repository now builds and tests Foundation, Core, RHI, Renderer, Runtime, API samples, a Zig gameplay consumer, and a headless `NexoraShowcase`. The milestone sections below describe the implemented portable contract foundations and explicitly call out platform or production backends that remain future work. Parsers for persisted or external data (for example scene snapshots) reject hostile size fields before allocating.

#### V2 networking status

V2-M5 is complete for the portable foundation: the `linux-headless` preset builds only the renderer-free server closure, and `NexoraNetwork` exposes synchronous loopback/simulated transports, a portable socket-provider boundary, and a connection contract with protocol/build identity checks, explicit channel semantics, and deterministic loss/latency/jitter simulation. Its server runtime owns fixed-step scheduling, bounded graceful drain, admission and per-client packet/byte budgets (both defer excess work to a later tick rather than dropping it, guaranteeing at least one packet's progress per tick even when a single packet exceeds the full per-tick byte budget), ordered replay capture, and canonical state hashing. V2-M6 is complete for its portable reference scope, adding dirty-generation dormancy with connection-local acknowledgement and re-entry baseline invalidation, sequenced client prediction, authoritative correction with pending-input replay, deterministic fixed-point latency tests, and versioned replay logs to the existing entity, schema, snapshot, delta, and interest foundations. Headless tests cover a fuzz-style malformed corpus, 10,000 reconnect cycles, scheduling/budget/drain behavior, malformed/truncated data, baseline expiry, connection isolation, dormant wake-up, deterministic correction, and network-bug reproduction. `NexoraDedicatedServer` links only through Network → Core → Foundation, keeping Renderer and presentation modules outside its dependency closure. Native UDP/DTLS adapters, encryption, and hosted production deployment remain backend gates; loopback/simulated transport does not establish production-networking completion.

#### V2 GPU-driven status

V2-M3 now has one fixed 36-byte indirect-command ABI shared by C++ and Slang: the Vulkan/D3D12/Metal-compatible non-indexed draw prefix is followed by backend-neutral classification metadata. Vulkan consumes this canonical stride directly. D3D12 command recording now covers `Dispatch` and canonical-stride `ExecuteIndirect`, but Windows execution and comparison evidence remain pending. The Slang-enabled Linux Vulkan RenderGraph test passes on Mesa lavapipe, including `CompareGPUDrivenResults()`, and the full Linux development, shipping package/evidence, sanitizer, and TSan jobs passed in GitHub Actions run 36609837931. This Work Mode container lacks `clang++`, so its local full CTest rerun is 50/51; physical-GPU performance and Metal execution remain open, while Windows DX12 requires target-host evidence. V2-M3 remains open independently of the 46% roadmap total.
The Showcase Validation Lab now provides a portable M0-M12 probe registry, honest five-state status model, versioned JSON/Markdown reports, CTest card mapping, four contained error injections, and M7-M10 capability-aware headless room evidence. The 3D Hub now renders all thirteen cards from that shared model and reports stable room/world-object associations without executing CTest logic; authored room visuals and target-host evidence remain open.

#### Shader system status

The shader production pipeline now has a ✅ portable acceptance gate: the Editor invokes the
configured `slangc` process directly without a shell, parses native Slang 2026 and single-line
file/line/column/severity/backend/variant diagnostics, tracks source/include invalidation per compile
request, and caches successful Development variants against an explicit budget; results are stamped
with a pre-compile input snapshot, so a save landing mid-compile is discarded as stale.
Runtime serializes and loads checksummed cooked artifacts, enforces Shipping cooked-only admission,
creates backend modules through an injected native adapter, publishes generations transactionally,
and retires replaced modules only after their GPU fence. Renderer exposes generation-bearing
pipeline-state keys and a named golden-image harness; Slang-enabled Linux Vulkan builds also run
`renderer.vulkan_golden_triangle`, an offscreen Mesa lavapipe render compared against a committed
baseline. That is a Linux software-rasterizer reference only; DX12, Metal, and physical-GPU baselines
remain target-host gates. `Shaders/Nexora/Common.slang` now contains the
portable shared surface for PBR/IBL, StylizedPBR, Anime, Vegetation, Water, Unlit, shadow/post-process,
skinning/instancing/Forward+, variant keys, and retained-mode UI helpers. `PbrSmoke.slang` and
`UiSmoke.slang` are real vertex/fragment smoke entries (including IBL resources, Texture2DArray,
atlas sampling, clip, and nine-slice); Linux Slang 2026.18 SPIR-V/MSL compilation and the shader
contract/cross-compile tests pass. Actual DXIL/native backend execution and captured target-host
golden baselines remain open platform gates; the portable harness does not claim those results.
Renderer now also exposes a ✅ backend-neutral material schema and integration contract covering all
six shading models, resource binding with semantic missing-texture fallbacks, used-variant stripping,
generation-based hot reload, and stable Material Inspector reflection/layout hashes. The existing
shared Slang library supplies PBR/IBL and specialized shading helpers; native DXIL/Metal execution
and physical-GPU visual acceptance remain target-host gates.


#### V2 late-milestone portable status

V2-M9 now has backend-neutral Timeline/Camera Rig, Flex/Grid, localized RichText, Theme/StyleSheet,
accessibility, Surface UI projection, room/portal audio, HLS/DASH segment contracts, and optional
DRM/capture boundaries while preserving the V1 `VideoPlayer` contract. V2-M10 extends the existing
production commandlet with deterministic content-addressed work identities, local-first Shared DDC,
transactional patch verification, generation pin/drain, and native-code rejection. V2-M11 adds a
versioned Core diagnostics wire schema with headless Trace ID correlation and PluginID attribution.
V2-M12 defines the five reference-project capability profiles and fast hardening probes for bounded
growth, reconnect drain, rollback, save corruption, thermal-policy evidence, and V1-like footprint
limits. These are portable foundations only: M9-M12 remain unaccepted until their production,
target-host, full-reference-project, and long-soak gates are satisfied.

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

ZS-M0 through ZS-M5 are complete: the capability-aware gallery supports camera input, selection raycasts, honest feature overlays and tested fallbacks; dynamic reload now stabilizes files, restores candidate state before `on_start` to prevent duplicate scenes, and reports callback/device-loss recovery scopes. Development-dynamic and Shipping-monolithic/static packages launch from checksum-verified isolated copies, and the independently provisioned Windows clean-machine Development package passed 16/16 checksums and emitted a PASS launch report. See the [ZS-M5 acceptance record](Apps/Showcase/evidence/ZS-M5-Windows-CleanMachine-2026-10-01/acceptance.md).

#### Window and native presentation status

The [Window and Native Presentation Roadmap](Roadmap/en/Window_Presentation_Roadmap.md) is **100% implementation complete**: WP-M0 through WP-M4 provide the module boundary, native window/input implementations, DX12/Vulkan/Metal presentation paths, reusable Showcase/Editor surfaces, and lifecycle/failure hardening. This percentage records implementation scope, not cross-platform runtime acceptance. Windows/DX12 WP-M1/WP-M2 runner evidence is now recorded; Showcase interactive, Linux/Windows Vulkan, and macOS/Metal runtime acceptance remain target-host gates.

#### Editor status

The [Graphical Editor Roadmap](Roadmap/en/Editor_Roadmap.md) remains at **0/8 (0%) graphical milestone acceptance**. Portable foundations now include ED-M4 additive-scene ownership and dependency ordering, migration dry-runs, bounded autosave recovery, and source-control-neutral three-way conflicts, and UI-neutral viewport pick-ray, AABB picking, axis-drag, snapping, and resize-hysteresis math, in addition to the existing ED-M1 through ED-M3 contracts. The [rotation and scale plan](Roadmap/en/Transform_Rotation_Scale_Plan.md) has ✅ all phases complete: `runtime::Transform` carries a quaternion rotation and per-axis scale following Unity/Unreal conventions, position-only writers preserve them, and `ViewportMath.h` provides Unity-style translate/rotate/scale gizmo math (Global/Local axes, Pivot/Center, parents, negative-scale rules, and multi-selection roots, covered by `editor.viewport_math`), and gameplay modules read and write `"Nexora.TransformV2"`, `"Nexora.WorldTransform"`, and `"Nexora.Parent"` through append-only C/Zig wires whose layouts are gated in C11, the ABI baseline, and Zig; the Euler Inspector hint waits for the graphical Inspector. Phases 1 and 2 of the [entity parenting plan](Roadmap/en/Entity_Parenting_Plan.md) are ✅ complete: entities form a Unity-style hierarchy with local transforms, world transform and exact world matrix, keep-world reparenting, and cascading destroy; scene snapshots are version 3 (versions 1 and 2 still load), the Editor scene document uses the runtime hierarchy, and character controllers work under a parent the way Unity's do (a moving parent carries them). Hierarchy batches are all-or-nothing, snapshot validation and cascading destroy are linear in the scene size, and PIE apply-back rejects entities reparented during play (`runtime.entity_parenting`, `editor.preview_contract`; Linux development, full-feature, Shipping, ASan/UBSan, and Editor-SDK-off builds, plus the full CI matrix after merge). Unity-style sibling order (`SetSiblingIndex`, reparent-to-last) and an undoable Hierarchy drag model (`SceneDocument::Move`) are in place; `RenderSceneSync` mirrors mesh renderers into the `GPUScene` with each entity's exact world matrix and conservative bounds, so a moved parent re-renders its subtree (`runtime.render_sync`); no application draw loop uses it yet. These are portable math and data contracts; the graphical gizmo handles remain open. The Linux Editor display acceptance now passes locally on a virtual display (Xvfb with Mesa lavapipe) after fixing three real defects it exposed; a dedicated CI job (`editor-linux-display`) runs it, and there is no physical-display or Windows evidence. The focused [ED-M0 Dear ImGui plan](Roadmap/en/Editor_ImGui_Integration_Plan.md) remains **in progress**; graphical workflows, native debugger integration, physical-display evidence, and UI acceptance remain open. ED-M0 through ED-M7 are therefore unchecked; portable prerequisites are not rounded up into accepted graphical milestones.

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
| [V2 完整規劃書](Roadmap/zh-TW/跨平台3D_Engine_V2_完整規劃書_v1_4.md) | **46%** | ✅ V2-M0 至 ✅ V2-M2 與 ✅ V2-M4 至 ✅ V2-M6 已驗收。V2-M7 現含有界 multi-world self-play 協調，但仍進行中；V2-M9 至 V2-M12 也仍是未驗收的 portable foundation，V2-M3 與 V2-M8 尚未完成。 |
| [V2 AI 施工規劃](Roadmap/zh-TW/跨平台3D_Engine_V2_AI施工技術與系統規劃_v1_2.md) | **46%** | 追蹤同一套已驗收的 V2 基線，以及進行中的 V2-M7 與 V2-M9～M12 portable foundation；production backend、distributed infrastructure、跨裝置 diagnostics 與 hardening 驗收仍待完成。 |
| [V3 完整規劃書](Roadmap/zh-TW/跨平台3D_Engine_V3_完整規劃書_v1_4.md) | **0%** | 尚無 V3 delivery milestone 通過 repository gate。 |
| [V3 AI 施工規劃](Roadmap/zh-TW/跨平台3D_Engine_V3_AI施工技術與系統規劃_v1_3.md) | **0%** | 僅為施工規劃；尚無 V3 milestone 驗收。 |
| ✅ [Engine API 基礎](Roadmap/zh-TW/Engine_API_基礎_Roadmap.md) | **100%** | ✅ API-M1 至 ✅ API-M6 完成 portable scope。 |
| ✅ [Window 與 Native Presentation](Roadmap/zh-TW/Window_Presentation_Roadmap.md) | **100%** | ✅ WP-M0 至 ✅ WP-M4 已實作；Windows/DX12 WP-M1/WP-M2 runtime 驗收已記錄，Showcase、Vulkan/Metal 與其他 native-host 驗收仍為獨立 gate。 |
| ✅ [Zig Showcase](Roadmap/zh-TW/Zig_Showcase_Roadmap.md) | **100%** | ✅ ZS-M0 至 ✅ ZS-M5 已完成；獨立配置 Windows 乾淨機器的 Development package 已通過 16/16 checksum 並產出 PASS launch report。 |
| [圖形化 Editor](Roadmap/zh-TW/Editor_Roadmap.md) | **0%（0/8）** | Repository 稽核確認各 ED track 已有 portable foundation，Dear ImGui shell 亦在施工中，但尚無 graphical ED milestone 通過全部 automated 與 target-host gate。 |
| [聚焦 Roadmap AI 施工規劃](Roadmap/zh-TW/聚焦_Roadmap_AI施工技術與系統規劃.md) | **60%** | API 100%、Zig Showcase 100% 與 Editor 0% 的平均，向下取整至 10%。 |
| [V1 可視化 Showcase](Roadmap/zh-TW/V1-Visual-Showcase-Long-Term-Plan.md) | **10%** | Phase A 已實作但仍待 native 驗收；Phase B 現有 ✅ portable scene-frame/resource foundation，native binding 與互動式 Rendering Room 仍待完成。 |

### Repository 狀態

目前已可建置及測試 Foundation、Core、RHI、Renderer、Runtime、API sample、Zig gameplay consumer 與 headless `NexoraShowcase`。下方里程碑章節會列出已實作的 portable contract foundation，並明確標示仍待完成的平台或 production backend。讀取持久化或外部資料的 parser（例如場景快照）會在配置記憶體前先拒絕惡意的大小欄位。

#### V2 Networking 狀態

V2-M5 portable foundation 已完成：`linux-headless` preset 僅建置 renderer-free server closure，而 `NexoraNetwork` 提供 synchronous loopback／simulated transport、portable socket-provider boundary 與 connection contract，包含 protocol／build identity 檢查、明確 channel semantics，以及 deterministic loss／latency／jitter simulation。Server runtime 擁有 fixed-step scheduling、bounded graceful drain、admission、per-client packet／byte budget（兩者都會把超出的工作延到下一個 tick，不會直接丟棄；即使單一封包超出整個 tick 的 byte budget，也保證每個 tick 至少能處理一個封包）、ordered replay capture 與 canonical state hash。V2-M6 已完成 portable reference scope：在既有 entity、schema、snapshot、delta 與 interest foundation 上，加入具 connection-local acknowledgement 與 re-entry baseline invalidation 的 dirty-generation dormancy、具序號的 client prediction、會 replay pending input 的 authoritative correction、deterministic fixed-point latency tests，以及 versioned replay log。Headless tests 涵蓋 fuzz-style malformed corpus、10,000 次 reconnect cycle、scheduling／budget／drain 行為、malformed／truncated data、baseline expiry、connection isolation、dormant wake-up、deterministic correction 與 network bug reproduction。`NexoraDedicatedServer` 僅透過 Network → Core → Foundation 連結，Renderer 與 presentation module 不會進入其 dependency closure。Native UDP／DTLS adapter、encryption 與 hosted production deployment 仍為 backend gate；loopback／simulated transport 不代表 production networking 已完成。

#### V2 GPU-driven 狀態

V2-M3 現已有一套由 C++ 與 Slang 共用的固定 36-byte indirect-command ABI：Vulkan／D3D12／Metal 相容的 non-indexed draw prefix 後接 backend-neutral classification metadata，且 Vulkan 直接消費此 canonical stride。D3D12 command recording 現已涵蓋 `Dispatch` 與採 canonical stride 的 `ExecuteIndirect`，但 Windows execution 與 comparison 證據仍待完成。啟用 Slang 的 Linux Vulkan RenderGraph 專項測試已於 Mesa lavapipe 通過，包含 `CompareGPUDrivenResults()`；GitHub Actions run 36609837931 的完整 Linux development、shipping package/evidence、sanitizer 與 TSan jobs 亦全數通過。本 Work Mode 雲端容器缺少 `clang++`，因此本地完整 CTest 為 50/51；實體 GPU 效能、Metal execution 與 Windows DX12 目標主機證據仍待完成；因此 V2-M3 仍獨立於 46% Roadmap 總進度維持未完成。
Showcase Validation Lab 現提供 portable M0～M12 probe registry、誠實的五態 status model、版本化 JSON／Markdown report、CTest card mapping、四種受控 error injection，以及 M7～M10 capability-aware headless room evidence。3D Hub 現由同一 model 繪製全部十三張卡片，並回報穩定的 room/world-object 關聯，且不執行 CTest 邏輯；authored room visuals 與 target-host evidence 仍待完成。

#### Shader 系統狀態

Shader production pipeline 現有 ✅ portable 驗收 gate：Editor 會不經 shell 直接呼叫設定的 `slangc` process、解析 Slang 2026 原生與單行 file／line／column／severity／backend／variant diagnostics、依每個 compile request 追蹤 source／include invalidation，並依明確 budget 快取成功的 Development variant；結果以編譯前的輸入快照標記，因此編譯期間存檔會被視為過期而丟棄。Runtime 會序列化及載入含 checksum 的 cooked artifact、強制 Shipping cooked-only admission、透過注入的 native adapter 建立 backend module、transactionally 發布 generation，且僅在 GPU fence 完成後回收被替換的 module。Renderer 提供包含 generation 的 pipeline-state key 與具命名 case 的 golden-image harness；啟用 Slang 與 Vulkan 的 Linux build 另有 `renderer.vulkan_golden_triangle`，在 Mesa lavapipe 上離屏渲染並與提交的基準圖比對。這只是 Linux 軟體光柵化的參考基準，DX12、Metal 與實體 GPU 基準仍是 target-host gate。`Shaders/Nexora/Common.slang` 現已提供 PBR／IBL、StylizedPBR、Anime、Vegetation、Water、Unlit、shadow／post-process、skinning／instancing／Forward+、variant key 與 retained-mode UI helper；`PbrSmoke.slang`／`UiSmoke.slang` 是含 IBL resource、Texture2DArray、atlas、clip、nine-slice 的實際 vertex／fragment smoke entry。Linux Slang 2026.18 的 SPIR-V／MSL 編譯與 shader contract／cross-compile tests 已通過；實際 DXIL／native backend execution 與 target-host capture golden baseline 仍是未完成的平台 gate，portable harness 不宣稱這些結果。
Renderer 現在也提供 ✅ backend-neutral material schema 與整合 contract，涵蓋六種 shading model、具 semantic 缺失貼圖 fallback 的 resource binding、used-variant stripping、generation-based hot reload，以及穩定的 Material Inspector reflection/layout hash。既有共用 Slang library 提供 PBR／IBL 與專用 shading helper；native DXIL／Metal execution 與實體 GPU 視覺驗收仍屬 target-host gate。


#### V2 後段 milestone portable 狀態

V2-M9 現已有 backend-neutral Timeline／Camera Rig、Flex／Grid、沿用 localization 的
RichText、Theme／StyleSheet、accessibility、Surface UI 投影、room／portal audio、
HLS／DASH segment contract 與 optional DRM／capture boundary，同時維持 V1
`VideoPlayer` contract。V2-M10 在既有 production commandlet 上加入 deterministic
content-addressed work identity、local-first Shared DDC、transactional patch verification、
generation pin／drain 與 native-code rejection。V2-M11 在 Core 加入版本化 diagnostics
wire schema、headless Trace ID correlation 與 PluginID attribution。V2-M12 定義五個
reference project 的 capability profile，並加入 bounded growth、reconnect drain、rollback、
save corruption、thermal-policy evidence 與 V1-like footprint limit 的快速 hardening probe。
這些仍只是 portable foundation；M9～M12 必須等 production、target-host、完整 reference
project 與長時間 soak gate 通過後才會驗收。

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

ZS-M0 至 ZS-M5 已完成：capability-aware gallery 支援 camera input、selection raycast、誠實的 feature overlay 與已測 fallback；dynamic reload 也會穩定檔案、在 `on_start` 前恢復候選 state 以避免重複場景，並回報 callback/device-lost recovery scope。Windows 開發工作站上的 Development-dynamic 與 Shipping-monolithic/static 套件已從 checksum 驗證過的隔離副本啟動；獨立配置 Windows 乾淨機器的 Development package 已通過 16/16 checksum 並產出 PASS launch report。

#### Window 與 Native Presentation 狀態

[Window 與 Native Presentation Roadmap](Roadmap/zh-TW/Window_Presentation_Roadmap.md) 的**實作進度為 100%**：WP-M0 至 WP-M4 已交付模組邊界、native window／input 實作、DX12／Vulkan／Metal presentation path、Showcase／Editor 可重用 surface，以及 lifecycle／failure hardening。此百分比代表實作 scope，不代表跨平台 runtime 驗收已完成；Windows/DX12 WP-M1/WP-M2 runner evidence 已記錄，Showcase interactive、Linux/Windows Vulkan 與 macOS/Metal runtime 驗收仍須在對應 target host 通過。

#### Editor 狀態

[圖形化 Editor Roadmap](Roadmap/zh-TW/Editor_Roadmap.md) 的**圖形化 milestone 驗收仍為 0/8（0%）**。Portable foundation 除既有 ED-M1 至 ED-M3 contract 外，現已加入 ED-M4 additive-scene ownership 與 dependency ordering、migration dry-run、bounded autosave recovery，以及 source-control-neutral three-way conflict，另有與 UI 無關的 viewport pick ray、AABB picking、軸向拖曳、snapping 與 resize hysteresis 數學。[旋轉與縮放計畫](Roadmap/zh-TW/Transform_Rotation_Scale_Plan.md)的 ✅ 所有階段皆已完成：`runtime::Transform` 含 quaternion 旋轉與逐軸縮放（沿用 Unity／Unreal 慣例），只寫位置的寫入者會保留它們，`ViewportMath.h` 提供 Unity 式的移動／旋轉／縮放 gizmo 數學（Global／Local 軸、Pivot／Center、父物件、負縮放規則與多選最上層判定，由 `editor.viewport_math` 涵蓋），gameplay module 可透過 append-only 的 C／Zig wire 讀寫 `"Nexora.TransformV2"`、`"Nexora.WorldTransform"` 與 `"Nexora.Parent"`，其 layout 由 C11、ABI baseline 與 Zig 檢查固定；Euler Inspector 提示留待圖形化 Inspector。[Entity parenting 計畫](Roadmap/zh-TW/Entity_Parenting_Plan.md)第 1、2 階段 ✅ 已完成：entity 形成 Unity 式階層，具 local transform、world transform 與精確的 world matrix、保留世界姿態的重新掛接，以及連帶刪除；場景快照為 v3（v1、v2 仍可載入），Editor 的場景文件以 runtime 階層為準，角色控制器也能像 Unity 一樣掛在父物件底下（移動中的父物件會帶著它走）。階層批次全有或全無、快照驗證與連帶刪除與場景大小成線性，PIE apply-back 會拒絕遊玩期間被重新掛接的 entity（`runtime.entity_parenting`、`editor.preview_contract`；Linux development、全功能、Shipping、ASan/UBSan 與關閉 Editor SDK 的組態，合併後也通過完整 CI）。Unity 式的兄弟順序（`SetSiblingIndex`、重新掛接後成為最後一個子物件）與可復原的 Hierarchy 拖曳模型（`SceneDocument::Move`）已就緒；`RenderSceneSync` 會以每個 entity 精確的 world matrix 與保守的包圍球，把 mesh renderer 同步到 `GPUScene`，移動父物件時整棵子樹的渲染資料都會更新（`runtime.render_sync`）；目前還沒有應用程式的繪製迴圈使用它。以上是 portable 的數學與資料 contract；圖形化 gizmo 操作把手仍待完成。Linux Editor 顯示驗收在修正它暴露的三個真實缺陷後，已能在虛擬顯示器（Xvfb 搭配 Mesa lavapipe）上於本機通過；已由專用 CI job（`editor-linux-display`）執行，仍沒有實體顯示器或 Windows 證據。Focused [ED-M0 Dear ImGui 計畫](Roadmap/zh-TW/Editor_ImGui_Integration_Plan.md) 仍為**施工中**；圖形化 workflow、native debugger integration、physical-display evidence 與 UI 驗收仍待完成。因此 ED-M0 至 ED-M7 都不打勾；portable prerequisite 不會向上取整為已驗收的 graphical milestone。

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
