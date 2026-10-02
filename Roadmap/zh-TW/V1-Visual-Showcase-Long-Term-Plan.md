# Nexora V1 可視化展示 Demo 長期規劃

> **進度：Linux、Windows CI 與本地開發機 GPU 可視化切片已驗證；完整 V1 驗收仍待完成。**
> 原百分比缺少可重現的加權清單，改以以下驗收證據追蹤。

## 0. 現況盤點

- ✅ Linux/X11/Vulkan 原生 indexed、lit、depth-tested 3D 與可讀 GPU UI 已在 Xvfb/lavapipe 執行；resize、有序關閉與實際鍵鼠互動測試通過且未 skip。
- ✅ 八個可切換房間、F1/F2/F3 overlay、F5 snapshot reload、滑鼠 orbit/zoom、輸入路由、locale 切換／fallback 與可暫停／重播的 210 秒導覽已實作。
- ✅ 公開 API 整合展示涵蓋 scene snapshot、Editor/Play 隔離、Modify/Undo、prefab override/rebase、procedural mesh import/cook/bundle/load 與 rollback、character/collision/navigation/AI、animation/skin 狀態、particle capacity、audio/video contract、cell/HLOD budget 與 lifecycle/shipping 模擬。
- ✅ 整合探針範圍明確：`runtime_rooms.integration_probes` 不代表 CTest 或 clean-host 視覺驗收通過；原 CTest mapping 保留獨立權威，未觀察的 synthetic error-injection metadata 維持 `NOT_RUN`。
- ✅ Development/Modular 封裝包含 Linux 所需的七個 engine library；isolated-copy launch evidence 會拒絕從封裝外解析 engine dependency，Full/Monolithic 與 Minimal build 分開驗證。
- ✅ Full profile package preset、原創 content catalog、互動啟動腳本、可重現 ZIP／SHA-256 與版本化 Linux 截圖已提供。
- 既有 Windows/DX12 開發機 GPU 證據仍只適用於其記錄版本；本次 Linux session 未在 Windows 執行新增房間、Full Windows package 或 workflow 變更。
- 待辦：Windows 乾淨機圖形啟動、physical-display 驗收及其他 native backend parity。Audio/video/WebView adapter 持續明確標示 contract-only／unavailable。

證據與精確驗證結果：[`Linux-Vulkan-Visual-Slice-2026-10-03`](../../Apps/Showcase/evidence/Linux-Vulkan-Visual-Slice-2026-10-03/acceptance.md)。

> 文件版本：v1.1
>
> 文件狀態：Linux 與 Windows 開發機 vertical slice 已實作；完整驗收待完成
>
> 更新日期：2026-10-03

## 1. 文件目的

本文件規畫一個可長期維護的 NexoraShowcase，讓 Nexora 同時擁有：

1. 可直接執行的 Windows NexoraShowcase.exe，展示 3D 畫面、場景、輸入、Gameplay、資產與 Runtime 能力。
2. 可重複執行的 Demo Probe 與 headless smoke，用來驗證 V1-M0～V1-M12 是否能在同一個 Runtime 組合起來運作。

Demo 不取代 CTest。CTest 負責 deterministic、headless、錯誤路徑與效能基線；NexoraShowcase 負責可觀察的跨模組整合、視覺結果與人工展示。

## 2. 現況與必要邊界

目前 repository 已有 V1 Runtime、RHI、Renderer 與多個 milestone contract，但還不是完整的視窗化 3D Engine：

| 現況 | 證據 | 對 Demo 的影響 |
| --- | --- | --- |
| NexoraHost 已能執行 Offscreen -> Main -> Present RenderGraph workload | Apps/Host/main.cpp、Engine/Renderer/src/FramePipeline.cpp | 可重用為 headless/render contract 基線，但不是可見視窗 |
| V1-M3 native backend 的 Present 驗證的是資源狀態 | Engine/RHI/README.md | 必須新增 window surface/swapchain contract，不能把 offscreen Present 宣稱成畫面輸出 |
| NEXORA_ENABLE_EDITOR_SDK 包含 reflection、plugin、scene editor、prefab 基礎 | Engine/Runtime/src/EditorSdk.cpp、Engine/Runtime/README.md | 可先做 Runtime 驅動的 Inspector/Undo/Prefab 展示；圖形化 Editor UI 是後續工作 |
| M4～M12 目前是 platform-neutral executable baseline | Engine/Runtime/README.md | Demo 要顯示狀態、計數器與可視化結果，不能把 portable contract 誤寫成第三方 SDK 已完成 |
| 現有測試 target 分散在 Foundation、Core、Renderer、Runtime | Tests/*/CMakeLists.txt | Demo 應共用 Runtime API，但不能把測試程式直接當成展示程式 |

因此第一個展示版本的目標是「可執行的 V1 Showcase Vertical Slice」，不是一次宣稱所有平台、SDK 與 production editor 都已完成。

## 3. 目標產品定義

### 3.1 產品與 executable

- CMake target：NexoraShowcase
- Windows executable：NexoraShowcase.exe
- Windows 第一階段 backend：Win32 window + DX12 surface/swapchain
- Runtime 模式：Development 與 Shipping / Full
- 預設啟動畫面：Showcase Hub
- 預設解析度：1280×720，可調整視窗大小
- 預設啟動參數：

~~~text
NexoraShowcase.exe --mode=interactive --scene=hub --backend=dx12
~~~

### 3.2 Demo 必須提供的能力

- 真正可見的 3D camera、mesh、材質、深度、光源與 RenderGraph 畫面。
- 鍵盤、滑鼠與可擴充的 gamepad input。
- Scene/Feature 選單，可進入各 V1 展示房間。
- 每個展示房間顯示功能名稱、目前狀態、probe 結果、錯誤訊息與關鍵 counters。
- F1 顯示總覽 UI；F2 顯示 profiler/diagnostics；F3 顯示 V1 matrix；F5 重新載入目前場景。
- headless validate-v1 模式可不開視窗執行可攜式 probe，供 CI 與本機 smoke 使用。
- Demo 啟動、場景切換、錯誤與效能資料寫入附帶 build ID 的 log。

### 3.3 不把 Demo 當成什麼

- 不是完整的遊戲產品，也不是 V1 的全部內容資產。
- 不是用靜態圖片模擬 3D rendering。
- 不是把所有 CTest case 改成需要人工操作的 UI 測試。
- 不是第一階段就完成跨平台 graphical editor、WebView SDK、硬體 video decoder 或 store installer。

## 4. 使用模式

### 4.1 Interactive Showcase

給開發者、貢獻者與外部展示使用。使用者可以自由移動 camera、切換房間、觸發功能按鈕、觀察 counters 與錯誤狀態。

### 4.2 Guided Tour

以 3～5 分鐘固定流程依序展示：

1. Engine 啟動與 BuildInfo
2. 3D scene 與 RenderGraph
3. Asset import/cook 與 scene reload
4. Character/physics/navigation/AI
5. Animation/audio/VFX/video
6. Large-world streaming/HLOD
7. Shipping profile、manifest 與診斷結果

Guided Tour 必須可暫停、重播，並在每一步留下可讀的 probe 結果。

### 4.3 V1 Validation Lab

讓工程師從 UI 選擇單一 milestone，重跑其 probe、查看輸入與輸出，並將結果輸出為 JSON/Markdown artifact。這是整合驗證，不取代該 milestone 的 CTest。

### 4.4 Headless CI Smoke

~~~text
NexoraShowcase.exe --headless --validate-v1 --report=artifacts/showcase-v1.json
~~~

Headless 模式不得依賴 GPU window、滑鼠或人工按鍵；需要畫面時由 Windows native smoke 或人工展示流程另外驗證。

## 5. 建議架構

### 5.1 目標依賴圖

~~~text
NexoraShowcase
    |
    +-- ShowcaseApp       window / input / UI / scene routing / CLI
    +-- ShowcaseProbes    V1 capability probes and report serialization
    +-- ShowcaseContent   procedural demo content and authored bundles
    |
    +-- NexoraRuntime
          +-- NexoraRenderer
                +-- NexoraRHI
                      +-- Win32 + DX12 WindowSurface (new)
~~~

現有 NexoraRHI::Device 的 offscreen contract 必須保留。視窗化能力應增加獨立的 WindowSurface/swapchain 邊界，而不是把 Win32 型別放進 public backend-neutral header。

### 5.2 建議目錄

以下是目標結構，標記為 proposed，不代表本次文件建立時已存在：

~~~text
Apps/Showcase/
  CMakeLists.txt
  main.cpp
  ShowcaseApp.cpp/.h
  ShowcaseCommandLine.cpp/.h
  ShowcaseSceneCatalog.cpp/.h
  ShowcaseUi.cpp/.h

Engine/Window/
  include/Nexora/Window/Surface.h
  src/Win32Surface.cpp
  src/Dx12Swapchain.cpp

Showcase/
  Probes/
  Scenes/
  Reporting/
  Content/

Tests/Showcase/
  ShowcaseStartupTests.cpp
  ShowcaseProbeTests.cpp

Content/Showcase/
  Scenes/
  Materials/
  Meshes/
  Textures/
  Audio/
  Video/
  Localization/
~~~

### 5.3 CMake 選項

建議新增：

~~~cmake
option(NEXORA_BUILD_SHOWCASE "Build the long-lived visual V1 showcase" ON)
set(NEXORA_SHOWCASE_BACKEND "Auto" CACHE STRING "Showcase window backend")
set_property(CACHE NEXORA_SHOWCASE_BACKEND PROPERTY STRINGS Auto Win32D3D12)
~~~

NexoraShowcase 只能透過 target_link_libraries 使用 Engine public API。展示用 UI、內容與 probe 不得反向依賴測試 executable，也不得繞過 Runtime 直接修改其內部狀態。

## 6. Showcase 場景設計

Demo 不應為每個 milestone 建立互相孤立的測試視窗，而應建立一個可漫遊的 Showcase Hub 與數個可重用的展示房間。

### 6.1 Showcase Hub

- 中央 3D 展示台、Nexora logo、目前 build ID、backend、resolution、frame time。
- 八個傳送門或 UI 卡片：Core、Rendering、World、Gameplay、Presentation、Large World、Platform、Shipping。
- 每張卡片顯示 PASS、PARTIAL、CONTRACT ONLY、UNAVAILABLE；禁止把未整合項目顯示成綠色完成。

### 6.2 Rendering Room

- Procedural triangle/quad/cube/instanced mesh。
- Camera orbit、depth、material、light、shadow placeholder 與 RenderGraph pass overlay。
- 顯示 Offscreen -> Main -> Present pass graph，以及 window surface 的 acquire/present counters。
- Native backend 選擇失敗時，清楚顯示 fallback 與原因，不靜默退回成假畫面。

### 6.3 Scene / Asset / Editor Room

- M4 的 editor world/play world 隔離、entity lifecycle、deferred command 與 scene snapshot。
- M5 的 asset source -> import -> cook -> bundle -> load 流程，以及 hash、generation、residency、rollback。
- M6 的 Create/Modify/Undo、reflection property panel、plugin ABI gate 與 prefab override/rebase。
- 這個房間是 Runtime 驅動的 editor foundation 展示，不宣稱已完成獨立 graphical editor。

### 6.4 Gameplay Room

- 可控制的 capsule/character、地面、斜坡、障礙物與可視化 collision query。
- Character motor、ground snap、step/crouch/teleport 結果。
- Navigation tile、desired velocity、AI blackboard、behavior trace 與 perception budget。
- 任何真正的第三方 physics/navigation adapter 都必須在卡片上標示 adapter 狀態。

### 6.5 Presentation Room

- Skeleton/clip blend、root motion、skin palette 的可視化。
- Particle emitter、particle count、bounded capacity 與 dropped spawn counter。
- Audio bus、voice limit、residency panel。
- Video queue、timestamp、seek invalidation 與 back-pressure panel；沒有 decoder SDK 時使用明確的 contract-only mode。

### 6.6 Large World Room

- 多個 world cell、portal prefetch、occupied-cell pin、HLOD proxy 與 streaming budget。
- 顯示 cell identity、bundle identity、RAM/VRAM estimate、load/unload reason。
- 使用程序化地形與植被先驗證 streaming orchestration，避免第一版被大型美術資產阻塞。

### 6.7 Platform / Shipping Room

- App lifecycle、safe-area、memory/thermal pressure 與 input focus 模擬器。
- Native WebView 若沒有平台 adapter，顯示 adapter unavailable 與 ownership contract，不放一個看似可用的假 WebView。
- M12 profile preview：Minimal、Full、Dedicated 的 plugin、shader、asset、presentation strip 結果。
- Package manifest、update staging、rollback、crash breadcrumbs 與 build/device evidence 摘要。

## 7. V1-M0～M12 驗證矩陣

每一列都要有兩個結果：Contract Gate 是自動化證據，Showcase View 是人可以看到的整合結果。兩者任一缺少，都不能把該項目標成完整展示。

| Milestone | 現有/預期 Contract Gate | Showcase View | 第一階段狀態判定 | Owner |
| --- | --- | --- | --- | --- |
| M0 | CMake preset、module graph、build/CTest、Host startup | Build ID、module list、startup diagnostics | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Build |
| M1 | core.runtime、Foundation/Gameplay ABI | frame time、job graph、allocator/log/VFS counters | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Core |
| M2 | shader reflection、validation device、renderer contracts | shader/pass/resource overlay | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Renderer |
| M3 | native DX12/Vulkan/Metal offscreen path | backend badge、native present counters、3D frame | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Presentation |
| M4 | runtime.v1_m4_vertical_slice、scene snapshot/lifecycle | 可操作 scene、entity、undo、play/editor world | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Scene |
| M5 | runtime.v1_m5_asset_pipeline | import/cook/bundle/progress/reload/rollback | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Assets |
| M6 | runtime.v1_m6_editor_sdk、plugin ABI/prefab | reflection inspector、Undo、prefab rebase、plugin status | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Editor SDK |
| M7 | runtime.v1_m7_input_ui_localization | key binding、UI widgets、locale switch、fallback | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Input/UI |
| M8 | runtime.v1_m8_gameplay_simulation | character、collision、nav、AI trace | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Gameplay |
| M9 | runtime.v1_m9_presentation | animation、particles、audio、video queue | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Presentation |
| M10 | runtime.v1_m10_large_world | streaming map、HLOD、RAM/VRAM budget | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Large World |
| M11 | runtime.v1_m11_platform | lifecycle/pressure/WebView ownership panel | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Platform |
| M12 | runtime.v1_m12_shipping | package/profile/rollback/crash/device evidence | PARTIAL — Linux／Windows 開發機 view 已驗證；乾淨主機／實體顯示待驗收 | Runtime Shipping |

### 7.1 Probe 統一介面

每個 probe 應回傳可序列化的結果，不直接依賴 UI：

~~~cpp
struct ShowcaseProbeResult {
  std::string id;
  std::string milestone;
  ProbeStatus status;
  std::string summary;
  std::vector<ProbeMetric> metrics;
  std::vector<ProbeIssue> issues;
};
~~~

UI、headless report、CTest adapter 與 Guided Tour 都消費同一份結果。Probe 只能使用公開 contract，不能讀取測試私有資料或以 screenshot 判定 correctness。

## 8. 分階段施工順序

### Phase A — ✅ Linux Windowed App Shell

- ✅ 重用 `NexoraShowcase`、Window 與 RenderSurface；新增 `NEXORA_BUILD_SHOWCASE` feature gate，要求 Zig gameplay 與 Window Presentation。
- ✅ Native startup、resize、shutdown、clear color、indexed triangle 與可讀 diagnostics 已在 Xvfb/lavapipe 執行。
- ✅ `showcase.linux_vulkan_virtual_display` 與 `showcase.linux_vulkan_interaction` 通過且未 skip；CI 配置 Xvfb/xdotool，缺少 native evidence 會 fail。
- ✅ Headless/windowed report object 維持分離；close request 會在下一次 acquire 前停止。

此處只驗收 Linux virtual-display 切片；physical display 與 Windows/macOS target-host 驗收仍獨立追蹤。

### Phase B — First 3D Vertical Slice

- ✅ Portable vertex/index/uniform/depth/sample-texture 與 frame-resource contract 持續由 Renderer 測試覆蓋。
- ✅ Linux Vulkan indexed geometry、directional light、depth 與 UI composition 是原生 GPU 工作，由每幀 fence 保護資源。
- ✅ Hub／Rendering 房間的 procedural geometry 與互動 camera 可見；diagnostics 顯示實際 native counter 與取樣時間。
- ✅ Linux 原生 triangle/quad/instanced-cube 與 native-owner RenderGraph scene binding 已驗證；最新 Windows/native 與本地目標主機 gate 仍須分開驗收。

### Phase C — Probe 與 V1 Validation Lab

- ✅ M0～M12 registry、版本化 JSON／Markdown export，並區分 CTest 與 runtime-integration 權威。
- ✅ F3 matrix、Tab 選取、R rerun、`--probe=v1.MN` 與 `--markdown=PATH` 提供 live integration result。
- ✅ Scene snapshot、Modify/Undo、隔離 Play World、prefab override/rebase 與真正 cooked procedural mesh 會影響場景畫面。
- ✅ Empty asset、dependency cycle、asset-generation rollback 與 shipping-update rollback 使用公開 Runtime API。
- ✅ Validation Lab 顯示取樣時的輸入／輸出 metrics 與 issues，可捲動細節、切換錯誤案例並匯出 JSON／Markdown。空資產、相依循環、資產／更新回滾與真實動態函式庫的 ABI 拒絕皆透過公開 Runtime API 執行。缺少插件檔案為 FAIL；不支援的案例／milestone 組合為 UNSUPPORTED。
- Schema fixture injection metadata 仍與這些實際 runtime 觀察結果分開。

### Phase D — Gameplay / Presentation / World Rooms

- ✅ Normalized held-key／pointer input、Runtime UI hit testing、locale/fallback、character motion/crouch/teleport、collision hit、navigation point 與 AI/perception counter。
- ✅ 同一 application loop 更新 animation translation、skin palette count、particle occupancy/drop count 與 audio/video queue/bus contract；明確標示 native audio/video playback unavailable。
- ✅ Streaming cell、occupied pin、portal prefetch、HLOD residency、procedural terrain/vegetation 與 RAM/VRAM budget 已有可見狀態。
- ✅ 七步導覽共 210 秒，可 pause／replay，並保留可讀結果。
- ✅ 原生 indexed capsule geometry、碰撞一致的 AABB 階梯坡道、實際 collision ray/path 管線網格、以公開 palette 做 CPU 權重蒙皮的 column、雙 clip blend 與 live particle position mesh 均在 application loop 執行。
- ✅ Streamed Full terrain 細分網格、原創粗略 HLOD proxy 與 vegetation geometry 跟隨公開 cell residency。Memory counter 是 authored streaming estimate；production GPU skinning、平滑坡道 physics adapter 與高解析度美術不屬於此 procedural 展示範圍。

### Phase E — Platform / Shipping / Distribution

- ✅ Lifecycle/pressure simulation 與 WebView ownership／unavailable status。
- ✅ Minimal/Full/Dedicated Packager preview、staged-update rollback 與 bounded crash breadcrumb。
- ✅ Full profile preset、packaged content manifest、interactive script、可重現 ZIP／SHA-256 與 isolated-copy headless package smoke。
- ✅ 版本化 Linux screenshot／native interaction artifact；CI 保存 screenshot、package 與 CTest log。
- 待辦：新版 Full Windows executable 在乾淨主機執行、physical-display／version-tag evidence。

## 9. 建置、執行與打包規格

### 9.1 Development build

Target 完成後，Windows 的標準流程應為：

~~~powershell
cmake --preset windows-development
cmake --build --preset windows-development --target NexoraShowcase
ctest --preset windows-development -R "showcase|runtime|renderer"
~~~

Visual Studio generator 的多組態 build 使用：

~~~powershell
cmake --build build\windows-development --config Development --target NexoraShowcase --parallel 4
~~~

### 9.2 執行參數

~~~text
NexoraShowcase.exe --mode=interactive --scene=hub --backend=auto
NexoraShowcase.exe --mode=tour --scene=hub --tour=v1
NexoraShowcase.exe --headless --validate-v1 --report=showcase-v1.json
NexoraShowcase.exe --scene=rendering --backend=dx12 --vsync=off
~~~

所有參數都要在啟動 log 與報告中回寫，讓 screenshot、bug report 與 CI artifact 能追溯到同一個 build。

### 9.3 發布 package

~~~text
dist/NexoraShowcase/<version>/
  NexoraShowcase.exe
  Nexora*.dll
  Plugins/
  Content/Showcase/
  manifest.json
  checksums.sha256
  README.txt
  run-showcase.ps1
~~~

package 必須由 M12 Packager/manifest contract 產出或驗證，不允許靠人工複製 DLL 當成發布流程。展示版建議使用 Shipping + NEXORA_SHIPPING_PROFILE=Full；Minimal 與 Dedicated 用於 strip/headless 驗證，不作為完整視覺展示包。

## 10. 自動化驗證與 CI

必要 gates：

- showcase.configure：CMake option、module graph、content manifest 可配置。
- showcase.build：Windows Development 可產出 NexoraShowcase.exe。
- showcase.headless_startup：可在無視窗模式初始化、載入 probe registry、正常退出。
- showcase.v1_probe：各 probe 的 status、schema、錯誤路徑穩定。
- showcase.package_launch：從乾淨 artifact 目錄啟動並產生 report。
- showcase.windowed_smoke：Windows runner 有 GPU 時執行；沒有 GPU 時不得阻塞一般 CI。
- 原有所有 Foundation/Core/Renderer/Runtime CTest 必須繼續通過。

### 10.1 結果分類

| 狀態 | 意義 |
| --- | --- |
| PASS | contract、整合 probe 與必要畫面都完成 |
| PARTIAL | contract 通過，但 platform/native adapter 或畫面仍缺少 |
| CONTRACT_ONLY | 只有 headless contract，有明確理由尚不能視覺化 |
| UNAVAILABLE | 本機缺少依賴或 backend，已留下可診斷錯誤 |
| FAIL | 已實作能力的 probe 失敗 |

## 11. 長期維護規則

1. 每新增一個 V1 capability，必須同時新增或更新 public contract、CTest、Demo Probe、展示卡片與文件矩陣。
2. 展示內容優先使用程序化或可再分發的資產；第三方資產需記錄 license、source、hash 與替換方案。
3. Demo 不得把 debug-only 內部 API 暴露成 Engine public API。
4. Scene 與 bundle 必須有版本、schema 與 migration policy；舊 Demo package 要能顯示不相容原因。
5. 所有畫面上的 counter 都要標明資料來源與取樣時間，避免展示 stale data。
6. 每次 release 都固定保存 Windows artifact、showcase-v1.json、CTest log、BuildInfo、Git commit 與 screenshot。
7. Performance budget 與 visual quality budget 分開管理；60 FPS 不是 correctness 證據，screenshot 也不是 contract 證據。
8. Demo 若依賴未安裝的 SDK，必須 graceful degrade 並顯示 UNAVAILABLE，不能在啟動時整個崩潰。

## 12. Definition of Done

### 第一個可展示版本

- NexoraShowcase.exe 可在乾淨 Windows 環境開啟視窗。
- Hub 有真正的 3D camera、mesh、材質與可見 render output。
- 至少完成 Rendering Room、Scene/Asset/Editor Room、Gameplay Room 三個房間。
- F1/F2/F3 overlay 可顯示 build、backend、frame time 與 probe status。
- headless validate-v1 能產生可解析的 JSON report。
- 舊有 CTest 不退化，並新增 showcase startup、probe、package gates。

### V1 Showcase 完成版本

- M0～M12 每一列都有明確的 Contract Gate、Showcase View、Status 與 owner。
- M2/M3 的 offscreen 與 windowed rendering 結果分開報告。
- M6 清楚區分 Editor SDK 與 graphical editor；若 graphical editor 未完成，UI 不得誤標為完成。
- M9/M11 的第三方 adapter 缺失時仍可執行 contract-only 展示。
- Shipping / Full package 可在乾淨 Windows 環境啟動，並由 manifest 驗證內容。
- 每次 tag 都能產出 executable、package manifest、probe report、CTest report 與版本化 screenshot。

## 13. 第一個施工 ticket 建議

建議將下一個實作 milestone 命名為 V1-Showcase-M0 Windowed Demo Shell，只做以下範圍：

1. 新增 NEXORA_BUILD_SHOWCASE 與 Apps/Showcase/CMakeLists.txt。
2. 新增 NexoraShowcase.exe，能解析 mode、scene、backend、headless。
3. 新增 Windows Win32 window lifecycle 與 DX12 swapchain/surface adapter。
4. 新增 clear color、三角形與 diagnostics overlay；先不引入大型內容資產。
5. 保留 NexoraHost、offscreen renderer 與既有 CTest 不變。
6. 新增 startup、resize、shutdown、headless smoke。
7. 更新 Windows VS Code/CMake 使用說明與本文件的實作狀態。

這個 ticket 完成後，才進入 3D scene、probe registry 與各 V1 展示房間施工。這樣可以先取得真正能執行的 exe 基線，再逐步把 V1 能力接上去，而不會用尚未存在的畫面功能掩蓋 RHI/window boundary 尚未完成的事實。

✅ Validation Lab 後續：`Tab` 選取 M0-M12；`I` 切換 None／空資產／循環相依／Plugin ABI／Rollback；`R` 執行；PageUp/PageDown 捲動輸入、輸出與錯誤；`X` 匯出 `showcase-lab.json` 與 `showcase-lab.md`。空資產、相依循環、資產／更新回滾與真正動態函式庫的 ABI 拒絕皆使用公開 Runtime API 執行，記錄 sample tick 與實際輸入。範例插件啟用時會放在可執行檔旁並納入套件；`--plugin-library=PATH` 指定 M6 的真實函式庫。缺少插件檔案顯示 FAIL；不支援的探針／錯誤組合顯示 UNSUPPORTED。原有 schema-fixture 注入 metadata 仍與實際執行結果分開。

✅ Validation Lab／geometry 後續與 feet-origin ground-contact 回歸驗收：[Linux-V1-Lab-Geometry-2026-10-03](../../Apps/Showcase/evidence/Linux-V1-Lab-Geometry-2026-10-03/acceptance.md)，完整 Linux Development gate 75/75，五個 native gate 全數於 Khronos／synchronization validation 下執行。

Windows CI run 37043085582 的 Linux/macOS 建置與測試通過，但 Windows 因 `ShowcaseRooms.cpp` 的 MSVC `/WX` 型別轉換及成員遮蔽警告失敗。已加入明確的索引／座標轉型並區分幾何半徑參數名稱，Linux Development 仍為 75/75；修正後的 Windows CI 與 clean-host 驗收仍待執行。

✅ Rendering 房的地板及三個不同位置／尺寸／顏色的 cube 現在共用一份 24 頂點 mesh，由原生 hardware instance draw 繪製。Vulkan 像素驗收確認一次 draw 的兩個獨立 instance；DX12 已實作相同輸入契約，目標主機執行仍待驗收。本地目標主機驗收仍待完成。

✅ [Linux native instancing acceptance](../../Apps/Showcase/evidence/Linux-V1-Instancing-2026-10-03/acceptance.md).

Windows CI run 37044292214 在幾何警告修正後通過 Full Shipping 封裝；Development 仍因上游 Editor 測試區域變數遮蔽而失敗，目前已改用不同名稱。Clean-host 圖形驗收仍待執行。

✅ Hub/Rendering 已使用原生 RGBA8 棋盤取樣材質，結合 UV、lighting/base color 與 instance tint。Linux 像素驗收確認 texture selection、不可變快取重用及 resize 後重傳；DX12 提供相同 source binding，仍待目標主機執行。新版 Windows／physical-display 驗收仍待完成。

✅ [Linux sampled-material acceptance](../../Apps/Showcase/evidence/Linux-V1-Textures-2026-10-03/acceptance.md).

✅ Linux native-owner RenderGraph 已對實際 fence-owned scene color、GPU copy 與 swapchain/UI 操作排程 Offscreen -> Main -> UI -> Present；requested logical transition 與實際完成 callback order 和 RHI offscreen contract 分開報告。Rendering 的 P 可切換 instanced cubes、quad、triangle。套件的 accept-v1.ps1 可產生 Windows isolated-copy 原生截圖／JSON；physical-display 與 clean-host 驗收由使用者本地執行，結果記錄前仍未通過。

✅ sampled-material 基底 commit `36a178b6ec3d8406d5099952bdfc44a39840d5ce` 的 CI [37047161660](https://github.com/jimlee1972/Nexora/actions/runs/37047161660) 全部 job 通過，包含 Windows Development／Full Shipping、macOS／Linux 與 TSan／ASan contract。此結果驗證前一版基底；原生 graph／本地驗收腳本版本仍以各自 CI 與目標主機證據為準。

✅ [Linux native-owner RenderGraph acceptance](../../Apps/Showcase/evidence/Linux-V1-Native-Graph-2026-10-03/acceptance.md).

Windows 原生後續：CI 37052188209 已執行 DX12 graph 與所有房間截圖，應用程式報告 PASS（357 次 acquired／offscreen／copy／present）。stock PowerShell wrapper 讀取 process 退出碼時失敗，現已在退出前持有 process handle、記錄退出碼，並在失敗時保留啟動報告。修正版 wrapper CI 與本地乾淨主機／實體顯示驗收仍是各自待驗收 gate。

Windows 驗收腳本在擷取前調整並置頂原生視窗，且避免開啟遮擋畫面的終端。目標主機驗收仍須檢視實際截圖。

✅ 新版 Windows Full Shipping／DX12 隔離副本圖形驗收已在 [CI 37053279518](https://github.com/jimlee1972/Nexora/actions/runs/37053279518) 通過：14 筆 checksum、12 張可見截圖、asset／plugin 拒絕案例、339 次原生 graph／copy／present 及退出碼 0。[版本化 Windows CI 證據](../../Apps/Showcase/evidence/Windows-V1-Native-Graph-CI-2026-10-03/acceptance.md)。此 hosted VM 不代表實體顯示或獨立乾淨主機驗收；兩項仍由使用者本地完成。

✅ source revision `ecf2277f07bc51ef09112c652a0ccbc0511d0a99` 的 Build 37053279518 所有 job 通過，包含 Windows／Linux／macOS Development、Windows 原生 Full 驗收、mimalloc 與 sanitizer contract。

✅ Windows DX12 本地 Development/Full 與 Shipping/Full 原生執行已記錄於
[Windows-V1-DX12-Local-2026-10-03](../../Apps/Showcase/evidence/Windows-V1-DX12-Local-2026-10-03/acceptance.md)。
GTX 960 開發主機驗收涵蓋八個房間、primitive/texture/instance rendering、
原生 offscreen graph/UI/present、輸入／編輯／reload／animation 截圖、Lab asset/plugin 拒絕
以及完整 210 秒 guided tour。明確的 Development preset 與 stock PowerShell 5 預設路徑修正
讓本地文件流程可重現。乾淨主機與實體顯示操作聲明仍待驗收；V1 最終驗收尚未完成。
✅ Windows 本地驗收腳本現要求當次匯出、已修改場景的成功 snapshot round trip、
導覽重播歸零與 M5／M6 指定拒絕案例的實際輸入／輸出。Source／package hash 採固定 LF 位元組。
基底 e4a140139189 通過 Linux Development 77/77 與 Shipping package CI；
實體顯示與乾淨主機最終 gate 仍待驗收。
