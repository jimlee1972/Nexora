# Nexora Engine API 基礎 Roadmap

> 版本：v1.0｜狀態：規劃基線｜更新：2026-09-21

## 1. 目的與缺口

現有模組已證明 lifecycle、RHI、scene 與各 runtime contract，但「有測試的內部型別」不等於「遊戲作者可長期依賴的 Engine API」。本 Roadmap 補齊一般 3D 引擎應有的 Math、Transform、字串、檔案 I/O、時間、識別、容器與診斷表面，並同時定義 C++、穩定 C ABI、Zig 三層界線。

## 2. 設計原則

- `Foundation` 放無世界狀態的 value types；`Core` 放 process service；`Runtime` 放 world/entity API。
- 公開 C++ API 可使用型別安全 wrapper；跨 DLL/語言邊界只用固定寬度 scalar、POD、handle、pointer+length。
- 公開座標系固定為 right-handed、Y-up；角度 API 名稱必須帶 radians/degrees；矩陣 layout 與乘法方向寫入 contract。
- 不讓 `std::string`、STL container、exception、RTTI object 或 allocator ownership 穿越 C ABI。
- deterministic 與 fast math 分開；檔案存取一律經 VFS，gameplay 不直接依賴 OS path。
- API 加入後不得以「尚未被 Showcase 使用」視為完成；文件、單元測試、ABI test 與 sample 缺一不可。

## 3. 目標 API 分層

### 3.1 Math 與幾何（API-M1）

`Scalar`、`Vector2/3/4`、`Quaternion`、`Matrix3/4`、`Transform`、`Color`、`Rect`、`Ray`、`Plane`、`Aabb`、`Sphere`、`Frustum`。提供 dot/cross、normalize-safe、lerp/slerp、TRS compose/decompose、inverse、projection/look-at、intersection 與 epsilon policy。

驗收：scalar/SIMD 結果容差測試、NaN/zero-length policy、座標系 golden tests、序列化 round trip，並對 C ABI layout 做 `sizeof/alignof/offsetof` gate。

### 3.2 基礎資料型別（API-M2）

- UTF-8 `StringView` 與 engine-owned `String`；明確 byte/code-point 邊界、format、parse、case-folding 限制。
- `Uuid`、`Name/StringId`、generational handle、`Result/ErrorCode`、`Span`、`ByteBuffer`。
- engine allocator-aware array/map 只作 C++ convenience；ABI 使用 caller buffer 或 engine-owned opaque buffer + destroy function。

驗收：無效 UTF-8、embedded NUL、locale-independent number parsing、hash collision diagnostics、跨模組配置/釋放測試。

### 3.3 VFS 與檔案 I/O（API-M3）

公開 mount URI（如 `content://`、`user://`、`cache://`）、canonical path、同步小檔與 async request、stream、enumeration、metadata、atomic write、watch notification。定義 sandbox、symlink、case sensitivity、取消、partial read 與 completion thread；Shipping 禁止任意 host filesystem escape。

驗收：memory/temp/bundle backend contract suite、路徑 traversal、取消競態、atomic replace、超大檔 offset 與錯誤注入。

### 3.4 Engine services（API-M4）

公開 monotonic time、game time、fixed tick、random stream（seed/version）、structured logging、profiling marker、configuration、task dispatch、event subscription。禁止 gameplay 以 wall clock 驅動 simulation；callback 明訂 thread、lifetime、unsubscribe 與 reentrancy。

### 3.5 World/Game API（API-M5）

用 opaque `WorldHandle`、`EntityHandle` 與 component type ID 提供 spawn/destroy、get/set/batch query、deferred command、scene load、asset reference、input snapshot、camera/light/renderable/physics/audio facade。第一版不向 Zig 暴露 C++ ECS storage pointer，避免 archetype relocation 破壞 lifetime。

### 3.6 Language bindings 與版本化（API-M6）

建立 canonical C header 與 ABI manifest；C++ facade 和 Zig `@cImport`/薄 wrapper 由同一份宣告衍生。每項 export 有 `since`、ownership、nullability、threading、error、determinism 標記；新增欄位使用帶 `struct_size` 的 versioned descriptor，移除或改語意須走 major ABI。

## 4. 建議公開結構

```text
Engine/Foundation/include/Nexora/Math/       value math and geometry
Engine/Foundation/include/Nexora/Text/       UTF-8 and identifiers
Engine/Core/include/Nexora/IO/               VFS and streams
Engine/Core/include/Nexora/Services/         time, log, jobs, config
Engine/Runtime/include/Nexora/Game/           world-facing C++ facade
Engine/API/include/nexora/nexora.h            canonical stable C ABI
Bindings/Zig/nexora.zig                       safe Zig wrapper
Tests/API/                                    conformance and ABI tests
```

這是目標結構，不代表目前已存在或已完成。

## 5. 施工順序與 Definition of Done

| 階段 | 交付 | 進入下一階段的 Gate |
| --- | --- | --- |
| A | conventions、error/ownership table、ABI manifest schema | architecture review + ABI fixture |
| B | Math/geometry/Transform | unit、property、golden、layout tests |
| C | text/ID/buffer/result | fuzz corpus + cross-module allocation test |
| D | VFS/stream/async I/O | backend contract suite + sandbox tests |
| E | services/world facade | deterministic replay + lifecycle/thread tests |
| F | C/Zig bindings與 API reference | C/C++/Zig consumer builds + compatibility diff |

每個 API 必須有：public header、contract、成功與失敗範例、測試、版本資訊、對應 Showcase 使用點。效能目標以 benchmark 基線記錄，不先承諾未量測的數字。

## 6. 非目標與風險

本 Roadmap 不自製完整 STL、Unicode shaping、通用腳本 VM 或直接暴露所有 renderer internals。主要風險是 ABI layout 漂移、temporary view lifetime、跨 allocator free、Transform 非一致 scale、async callback 在 shutdown 後觸發；必須由 ABI snapshot、sanitizer、shutdown stress 與 owner-tag diagnostics 阻擋。

## 7. 與其他 Roadmap 的關係

API-M1～M4 是 Zig Showcase 與 Editor 共用前置；API-M5/M6 讓兩者只能經受支援的 public API 操作 Runtime。Zig Showcase 是第一個外部 consumer，Editor 則是高壓 consumer；兩者發現的缺口回補 API，而不是各自建立私有捷徑。
