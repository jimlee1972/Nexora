# Nexora Zig Showcase 與 Engine-owned Entry Point Roadmap

> 版本：v1.0｜狀態：規劃基線｜更新：2026-09-21

> **進度：40%**（截至 2026-09-23；依第 4 節 6 個 milestone 的加權驗收清單計算，
> 已完成項目以 ✅ 標示，結果向下取整至 10%。）

**施工狀態（2026-09-23）：** ZS-M0 已開始。ABI V3 現在定義明確的 result 與 capability 值、
分離 create/start/stop/destroy 階段、可回報失敗的 variable/fixed update，以及 C++ Host 的
transactional state migration；repository 內的 C++ fake module 會驗證此 contract。Zig module
現在會透過成對的 V3 host allocator callback 取得與釋放獨立 state，reload candidate
也不再共用 global state。Fake 與 Zig module 現在共用一份 vector set；allocation tag、negative
descriptor/callback coverage 與 Linux sanitizer preset 已完成 ZS-M0。Runtime dynamic discovery
與 generation lifecycle 已完成；Showcase artifact wiring 與 ZS-M1 的 native presentation 仍待完成。

**本機施工狀態（2026-09-23）：** 已提供 deterministic headless/static 的 ZS-M1 驗證 slice，
輸出 `NexoraShowcase.exe`。C++ 擁有 `main`、Engine lifecycle、小型 `GameWorld`、
fixed/update scheduling、offscreen scene rendering、reload 與 shutdown；Zig consumer 透過
公開 ABI 修改 primary entity 的 Transform，並由 executable 輸出 JSON evidence report。Dynamic
module discovery 與 native window/swapchain 仍未完成；report 會將後者標示為 `CONTRACT ONLY`。

## 1. 核心決策

Showcase 的 `main`、平台視窗、Engine lifecycle、render loop 與 shutdown 必須由 C++ Host/Engine 啟動；Zig 是被載入的 gameplay module，不是 process owner。Zig 透過穩定 C ABI 呼叫 Engine API，建立內容、處理 tick/input、操作 entity/component 並更新展示狀態。這取代「Zig 只做 ABI smoke」作為對外示範，但保留 smoke test。

```text
OS -> C++ NexoraShowcase main
   -> Engine bootstrap/window/RHI/runtime
   -> load Zig gameplay module
   -> nx_game_query_api / nx_game_create
   -> on_start -> fixed_update/update -> on_stop
   -> Engine render/present and final shutdown
```

## 2. Module contract

Zig dynamic/static module只輸出固定的 C symbols；entry descriptor 帶 ABI version、struct size、capability bits 與 callbacks。Engine 提供 versioned `NxEngineApi` function table 和 opaque context。Engine 擁有 world、assets、window、GPU、thread pool 與 callback invocation；Zig 擁有自己的 gameplay state，並在 `destroy` 前釋放。

規則：

- callback 不拋 exception 穿越 ABI；以 `NxResult` 回報。
- Engine API pointer 僅在 module lifetime 有效，不得 cache borrowed frame data。
- hot reload 須先 quiesce jobs，Zig 用 versioned blob `serialize_state`/`restore_state`；失敗回滾舊 generation。
- render callback 只能提交高階 debug draw/render requests，不取得 native device pointer。
- Shipping 可 static link，但使用相同 ABI contract；Development 優先 dynamic reload。

## 3. Showcase 內容

第一個可玩 slice 是小型 3D gallery：Zig spawn camera、旋轉 cubes、燈光與材質；以 input snapshot 控制 camera；用 raycast 選取物件；顯示 Transform、entity、asset 與 frame diagnostics；切換一個物理/角色區與 large-world streaming 區。所有畫面由 Engine renderer 呈現，Zig 不建立 swapchain。

每個房間同時展示 API：

| 房間 | Zig 呼叫的公開能力 | 可觀察證據 |
| --- | --- | --- |
| Math Lab | Vector/Quaternion/Transform、ray/AABB | gizmo、數值與 property test 狀態 |
| Scene Lab | entity/component、scene、asset | spawn/despawn、reload、generation |
| Gameplay Lab | input、fixed tick、physics/nav | controllable actor、query trace |
| Presentation Lab | animation/audio/VFX facade | counters、fallback/backend badge |
| Streaming Lab | cell request與 residency query | cell/HLOD overlay |

UI 必須標示 `IMPLEMENTED`、`CONTRACT ONLY`、`UNAVAILABLE`，不得以 placeholder 冒充 backend。

## 4. 階段

- **ZS-M0 Contract**：確立 Host-owned lifecycle、function table、錯誤/記憶體/thread contract；C++ fake module 與 Zig smoke 共用 conformance suite。
  - ✅ ABI V3 lifecycle、result/capability、paired allocator 與 transactional state migration。
  - ✅ C++ fake module 的 lifecycle/reload contract 驗證。
  - ✅ C++ fake 與 Zig consumer 共用同一份 conformance vectors。
  - ✅ owner tags、missing symbol/version/struct-size/callback failure，以及 Linux sanitizer gates。
- **ZS-M1 Bootstrap**：`NexoraShowcase` C++ target 負責 CLI、window/headless、module discovery；Zig `on_start/update/on_stop` 可執行。
  - ✅ C++-owned `main`、Engine/World lifetime、fixed/update scheduling 與 ordered shutdown。
  - ✅ deterministic headless validation backend、JSON evidence 與 static Zig object consumer。
  - ✅ Runtime dynamic library discovery、generation ownership、job quiescence 與真正的 library unload/rollback。
  - 待辦：建置 Zig Development shared-library artifact，並由 `NexoraShowcase` 選取。
  - 待辦：native window/input/swapchain；該邊界由 Window & Presentation Roadmap 負責。
- **ZS-M2 API-driven scene**：只用 API Roadmap 的 C/Zig bindings 建 scene、camera、mesh、input 與 diagnostics。
  - ✅ Zig 經 public host table 建立 scene、camera、light 與 cubes；C++ 保留 engine/world/render ownership。
  - ✅ 以 append-only 方式補齊 spawn/despawn、scene、input snapshot、opaque asset handle、raycast、高階 debug draw 與 diagnostics callbacks。
  - ✅ C header、ABI manifest/baseline、Zig binding、ownership/thread/error contract、C++ ABI gates、Zig smoke 與 deterministic headless evidence 已同步。
- **ZS-M3 Feature gallery**：加入 physics、animation/audio/VFX、streaming 的 bounded showcases 與 capability fallback。
  - ✅ Math、Scene、Gameplay、Presentation 與 Streaming room 已有可 script 選擇及 deterministic
    capability／fallback 證據。
  - 待辦：Math、Scene、Gameplay、Presentation 與 Streaming 五個可操作房間。
  - 待辦：camera input、selection/raycast、physics/navigation、animation/audio/VFX 與 large-world overlays。
  - 待辦：逐項顯示 `IMPLEMENTED` / `CONTRACT ONLY` / `UNAVAILABLE`，並提供 capability fallback tests。
- **ZS-M4 Reload and failure**：transactional hot reload、state migration、錯誤 module/ABI rejection、舊版本 rollback。
  - ✅ 真正 dynamic generations 的 job drain、restore failure、old-generation rollback、shutdown-during-reload 與 repeated reload stress。
  - 待辦：file stabilization，以及 device lost 與 dynamic update/fixed-update failure 整合。
- **ZS-M5 Distribution**：Development dynamic 與 Shipping static/packaged profiles，產生 license/build/API manifest。
  - 待辦：Development dynamic package、Shipping static package、clean-machine launch smoke。
  - 待辦：license/build/API/content manifests、checksums 與可重現的 packaging command。

## 5. 測試與驗收

- Engine 先於 Zig module 建立，且 module 永遠先於 Engine service 銷毀。
- C++ fake consumer 和 Zig consumer 跑同一份 ABI conformance vectors。
- headless scripted tour 產生 deterministic JSON；window smoke 驗證真實 present，但 screenshot 只做視覺 regression，不代替 correctness assertion。
- 測試 module 缺 symbol、版本不符、callback failure、reload 中有 job、restore failure、device lost 與 shutdown。
- Address/Undefined/Thread sanitizer 覆蓋 host；Zig 使用安全檢查 build；跨邊界 allocation 有 owner tags。
- Windows/DX12 為第一個 graphical gate；Linux headless 保持 CI gate；Vulkan/Metal 在對應 host 實測後才升級狀態。

## 6. 完成定義與非目標

完成代表 clone/build 後由 `NexoraShowcase` 啟動，明確證明 C++ 擁有 `main`，Zig gameplay 經公開 API 驅動畫面，且 headless、錯誤、reload 與 Shipping profile 有證據。第一版不允許 Zig 直接呼叫 RHI/native window，不要求 Zig 編寫 editor UI，也不把缺少平台 SDK 的功能標成完成。

本文件與既有 `V1-Visual-Showcase-Long-Term-Plan.md` 互補：該文件描述整體展示產品，本文件專門約束 Zig consumer 與 Engine-owned entry point。
