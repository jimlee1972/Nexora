# Nexora Zig Showcase 與 Engine-owned Entry Point Roadmap

> 版本：v1.0｜狀態：規劃基線｜更新：2026-09-21

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
- **ZS-M1 Bootstrap**：`NexoraShowcase` C++ target 負責 CLI、window/headless、module discovery；Zig `on_start/update/on_stop` 可執行。
- **ZS-M2 API-driven scene**：只用 API Roadmap 的 C/Zig bindings 建 scene、camera、mesh、input 與 diagnostics。
- **ZS-M3 Feature gallery**：加入 physics、animation/audio/VFX、streaming 的 bounded showcases 與 capability fallback。
- **ZS-M4 Reload and failure**：transactional hot reload、state migration、錯誤 module/ABI rejection、舊版本 rollback。
- **ZS-M5 Distribution**：Development dynamic 與 Shipping static/packaged profiles，產生 license/build/API manifest。

## 5. 測試與驗收

- Engine 先於 Zig module 建立，且 module 永遠先於 Engine service 銷毀。
- C++ fake consumer 和 Zig consumer 跑同一份 ABI conformance vectors。
- headless scripted tour 產生 deterministic JSON；window smoke 驗證真實 present，但 screenshot 只做視覺 regression，不代替 correctness assertion。
- 測試 module 缺 symbol、版本不符、callback failure、reload 中有 job、restore failure、device lost 與 shutdown。
- Address/Undefined/Thread sanitizer 覆蓋 host；Zig 使用安全檢查 build；跨邊界 allocation 有 owner tags。
- Windows/DX12 為第一個 graphical gate；Linux headless 保持 CI gate；Vulkan/Metal 在對應 host 實測後才升級狀態。

## 6. 完成定義與非目標

完成代表 clone/build 後由 `NexoraShowcase` 啟動，明確證明 C++ 擁有 `main`，Zig gameplay 經公開 API 驅動畫面，且 headless、錯誤、reload 與 Shipping profile 有證據。第一版不允許 Zig 直接呼叫 RHI/native window，不要求 Zig 編寫 editor UI，也不把缺少平台 SDK 的功能標成完成。

本文件與既有 [`V1-Visual-Showcase-Long-Term-Plan.md`](V1-Visual-Showcase-Long-Term-Plan.md) 互補：該文件描述整體展示產品，本文件專門約束 Zig consumer 與 Engine-owned entry point。
