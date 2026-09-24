# Nexora V1 可視化展示 Demo 長期規劃

> **進度：10%**（截至 2026-09-23；依 Phase A～E 驗收項目加權計算，
> 已完成項目以 ✅ 標示；headless contract 不等於已完成視窗化 phase。）

## 0. 現況盤點

- ✅ 已有 C++-owned `NexoraShowcase` entry point、CLI 與 ordered Engine/module shutdown。
- ✅ 已有 deterministic headless scene、validation RHI、scene extraction 與 JSON evidence report。
- ✅ 已有 Zig static consumer 的 fixed/update、Transform read/write 與 transactional state migration。
- 待辦：native window/input/swapchain 由 Window 與 Native Presentation Roadmap 管理。
- 待辦：真正的 3D Hub、Rendering/Scene/Gameplay/Presentation/Large World/Platform/Shipping 房間。
- 待辦：M0～M12 probe registry、interactive/guided tour、error injection 與視覺 status UI。
- 待辦：clean package launch、manifest/checksum、windowed smoke 與版本化 screenshot evidence。

> 文件版本：v1.0
>
> 文件狀態：規畫基線（Draft）
>
> 更新日期：2026-09-22

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

| Milestone | 現有/預期 Contract Gate | Showcase View | 第一階段狀態判定 |
| --- | --- | --- | --- |
| M0 | CMake preset、module graph、build/CTest、Host startup | Build ID、module list、startup diagnostics | Contract 已有；視覺入口待新增 |
| M1 | core.runtime、Foundation/Gameplay ABI | frame time、job graph、allocator/log/VFS counters | Contract 已有；以 diagnostics 展示 |
| M2 | shader reflection、validation device、renderer contracts | shader/pass/resource overlay | Offscreen 可驗證；window path 待施工 |
| M3 | native DX12/Vulkan/Metal offscreen path | backend badge、native present counters、3D frame | Native offscreen 已有；swapchain 待施工 |
| M4 | runtime.v1_m4_vertical_slice、scene snapshot/lifecycle | 可操作 scene、entity、undo、play/editor world | Runtime foundation 已有；內容與 UI 待施工 |
| M5 | runtime.v1_m5_asset_pipeline | import/cook/bundle/progress/reload/rollback | Contract 已有；展示資產待施工 |
| M6 | runtime.v1_m6_editor_sdk、plugin ABI/prefab | reflection inspector、Undo、prefab rebase、plugin status | Editor SDK 已有；graphical editor 不在現況 |
| M7 | runtime.v1_m7_input_ui_localization | key binding、UI widgets、locale switch、fallback | Contract 已有；window input/UI 待施工 |
| M8 | runtime.v1_m8_gameplay_simulation | character、collision、nav、AI trace | Contract 已有；3D gameplay scene 待施工 |
| M9 | runtime.v1_m9_presentation | animation、particles、audio、video queue | Portable foundation；native media adapters 待施工 |
| M10 | runtime.v1_m10_large_world | streaming map、HLOD、RAM/VRAM budget | Contract 已有；visual world 待施工 |
| M11 | runtime.v1_m11_platform | lifecycle/pressure/WebView ownership panel | Portable contract；platform adapter 待施工 |
| M12 | runtime.v1_m12_shipping | package/profile/rollback/crash/device evidence | Contract 已有；可分發 exe 待施工 |

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

### Phase A — Windowed App Shell

目標：產出第一個能開窗、關窗、resize、顯示 clear color 與 diagnostics overlay 的 NexoraShowcase.exe。

- ✅ 建立 Apps/Showcase target 與 headless 命令列解析。
- 建立 backend-neutral WindowSurface contract。
- Windows 實作 Win32 window 與 DX12 swapchain。
- ✅ 保留現有 offscreen device/test path。
- 加入 showcase.startup、showcase.resize、showcase.shutdown smoke。

### Phase B — First 3D Vertical Slice

目標：Hub 場景出現真正可互動的 3D 畫面。

- 增加 vertex/index/uniform/depth/texture 所需的最小 RHI contract。
- 建立 camera、mesh、material、light 與 frame resource ownership。
- 將 FramePipeline 從固定 triangle 擴成可提交 scene frame 的最小路徑。
- 建立程序化 mesh/material，第一版不依賴大型外部資產。
- 實作 Rendering Room 與 frame diagnostics。

### Phase C — Probe 與 V1 Validation Lab

目標：Demo 可以逐一觸發 M0～M12 probe，並輸出 JSON/Markdown 報告。

- 建立 ShowcaseProbe registry、status model 與 report schema。
- 將既有 CTest 契約映射到展示卡片，但不複製其 correctness implementation。
- 實作 M4/M5/M6 的 scene/asset/editor foundation 展示。
- 加入 error injection：無效 asset、dependency cycle、plugin ABI mismatch、rollback。

### Phase D — Gameplay / Presentation / World Rooms

目標：把 M7～M10 的 Runtime contract 接到同一個 3D scene loop。

- Input/UI/localization overlay。
- Character/physics/navigation/AI 可視化。
- Animation/particle/audio/video 的 capability-aware demo。
- Streaming cell/HLOD/procedural terrain/vegetation demo。

### Phase E — Platform / Shipping / Distribution

目標：雙擊一個乾淨 package 的 exe 即可展示，並能證明 package 的內容與 profile 正確。

- M11 lifecycle/pressure/WebView adapter status。
- M12 Full showcase package、manifest、update/rollback、crash breadcrumbs。
- NexoraShowcase.exe --headless --validate-v1 package smoke。
- Windows artifact ZIP 與 SHA-256 manifest。

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
