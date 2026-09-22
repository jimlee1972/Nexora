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

## 8. API-M1～M4 現況（2026-09-22）

依第 2 節「文件、單元測試、ABI test 與 sample 缺一不可」核對，逐項如實記錄，不視為整體完成：

- **API-M1 Math**：`Engine/Foundation/include/Nexora/Math/Math.h` 已有 dot/cross/normalize-safe/lerp/slerp、TRS compose/decompose、`Matrix3`/`Matrix4` inverse（epsilon-fallback policy）、`LookAt`、`Orthographic`/`PerspectiveRadians`、`ExtractFrustum` 與 `Intersects(Frustum, Aabb/Sphere)`。`Tests/API/ApiFoundationTests.cpp` 提供 `sizeof/alignof/offsetof` ABI layout gate、byte-level round trip、Compose/Decompose 反函數驗證。`Vector4` 的 `Dot`/`Length`/`NormalizeSafe`/`Lerp` 跟 `Vector3` 同名操作共用名稱，但宣告成限定為 `Vector4` 的函式模板（`template <typename T> requires std::is_same_v<T, Vector4>`），不是一般的 `Vector4` overload：一般 overload 對裸 brace-init list 呼叫（如 `Dot({1, 2, 3}, {4, 5, 6})`）會是 ambiguous，因為兩種型別都是 aggregate，3-of-4 成員的 initializer list 對兩者都是同樣有效的轉換——這正是先前同名 plain overload 版本短暫合併後被 review 抓到的真實 regression。因為 template argument deduction 永遠不會對裸 braced-init-list 針對一般 type-template parameter 進行推導，這些 `Vector4` 模板對這種呼叫根本不會是候選者，ambiguity 就不會發生；用已經有明確型別的 `Vector4` 呼叫時則正常綁定。`Dot` 現在有 SSE2 SIMD 路徑（`NEXORA_MATH_HAS_SSE2` 偵測到 x86/x64 SSE2 時啟用，否則退回 scalar），並有 10,000 次迭代的 SIMD/scalar 容差比對 fuzz test（容差以各分量乘積絕對值總和為基準，避免 catastrophic cancellation 讓最終結果趨近零時容差反而失真）。**尚未做**：SIMD 只涵蓋 `Dot(Vector4, Vector4)` 這一條路徑 -- `Vector3` 的 dot/cross、`Matrix3`/`Matrix4` 乘法等其餘運算仍是純 scalar；SSE2 路徑本身也只在 x86/x64 驗證過，ARM/NEON 只是透過 `NEXORA_MATH_HAS_SSE2` guard 正確地退回 scalar，並未實際跑過驗證（此 sandbox 沒有 ARM 目標）。座標系 golden tests（只有內部一致性驗證，沒有跟外部參考引擎比對）仍未做。`Quaternion::NormalizeSafe` 與 `Vector3` 只帶一個引數（用預設 fallback）的 `NormalizeSafe(Vector3)` 現在也改成限定型別的函式模板，原因與修法都跟 `Vector4` 一樣：裸的 `NormalizeSafe({1, 2, 3})` 先前確實在兩者之間 ambiguous（`Quaternion::w` 預設值是 `1.0F`，3 個元素的 list 對兩種型別都能 aggregate-init），這是跟這次 SIMD 改動無關的既有問題；因為所有實際呼叫端都是傳已經有明確型別的引數，修掉這個 ambiguity 沒有帶來任何行為改變。`Vector3` 帶**兩個**引數的 `NormalizeSafe(Vector3, Vector3)` 則刻意維持一般 overload：這個 arity 跟 `Quaternion` 那個只吃一個引數的版本本來就不會 ambiguous（光是引數個數就排除了），第一版把整個雙引數 overload 都改成模板反而讓 `NormalizeSafe({3, 4, 0}, {0, 1, 0})` 這種裸 brace 雙引數呼叫壞掉、卻沒帶來任何好處——這是 review 抓到的 regression，這次修正把兩種 arity 分開處理來解決。另外還保留了 `Dot4`/`Length4`/`NormalizeSafe4`/`Lerp4`，作為對上面模板版本的一般（非模板）轉發包裝，給在這個名稱曾經是唯一寫法的短暫期間就採用它的呼叫端使用。
- **API-M2 基礎型別**：`Engine/Foundation/include/Nexora/Foundation/Types.h` 有 UTF-8 驗證、`StringView`/`String`/`ByteBuffer`/`Span`、`Uuid`（含 `Parse`/`ToString`）、`Name`、`Result<T>`、locale-independent `ParseNumber`。Generational handle 由既有的 `Nexora::Core::Handle<Tag>`/`HandlePool<Tag>`（`Engine/Core/include/Nexora/Core/Handle.h`）滿足，故意不在 Foundation 重複一份。**尚未做**：本節提到的「ABI 使用 caller buffer 或 engine-owned opaque buffer + destroy function」——目前沒有任何型別需要把 `String`/`ByteBuffer`/`Span` 本身（而不是 handle 或 POD）帶過 C ABI，所以還沒有為此建立對應機制。
- **API-M3 VFS**：`Engine/Core/include/Nexora/Core/Vfs.h` 的 `VirtualFileSystem` 現在有兩種 backend：`Mount`（目錄）與 `MountMemory`（記憶體，同一套 Read/WriteAtomic/Metadata/Enumerate 介面），兩者現在都在 `WriteAtomic` 時自動建立缺少的父目錄，不會失敗。Mount 自己的 root（`"mount://"`，scheme 後面什麼都沒有）現在解析成空的相對路徑，不會被當成 `InvalidPath`，所以 `Enumerate` 可以列出一個 mount 的完整頂層內容，不用呼叫端先猜一個子路徑。`Tests/API/ApiCoreContractTests.cpp` 對兩種 backend 跑同一套 contract（讀寫/metadata/含-root-的-enumerate/traversal 拒絕/錯誤注入/數 MiB 級大檔往返）。`Nexora::Runtime::MountBundle`（`Engine/Runtime/include/Nexora/Runtime/BundleMount.h`）是 bundle backend 這項交付：驗證一個 V1-M5 `Bundle`（`BundleBuilder::Verify`），然後把每個 asset 投影到一個 `MountMemory` mount 上，路徑是 `<name>://<uuid>.blob`——讀一個封裝在 bundle 裡的 cooked asset，就是 `vfs.Read(...)` 再 `AssetCooker::Deserialize`，不需要另外一套 bundle 專用 read API；這個東西沒辦法放進 `VirtualFileSystem` 本身，因為 `Bundle` 是 Runtime 的型別，Core 不能依賴 Runtime。Mount 名稱本身是呼叫端自訂；`Engine::Initialize` 現在掛了 `content` 跟 `temp`（後者用 `std::filesystem::temp_directory_path()`，跨平台、不用寫新的平台特定程式碼）；`engine`、`project`、`user`、`cache` 還沒接，因為這裡還沒有真正的 per-OS user-data/cache/install 目錄查詢（Windows `%APPDATA%`、macOS Application Support、Linux XDG dirs、Android/iOS sandbox path），沒有就不硬湊一個假的；`bundle` 根本不是單一靜態 mount——它是 `MountBundle` 在 runtime 針對每個 bundle 建立出來的 mount *類型*。**尚未做**：`PlatformPackageMount`、完整 async IO scheduler（priority preemption、request merge、aligned read、streaming deadline hint）、memory mapping、Shipping 下的呼叫端權限管控（`Mount` 目前對任何呼叫端一視同仁，只擋 mount 內部的路徑逃逸）、真正的多 GB/offset 邊界測試（現有測試是數 MiB smoke test，不是完整 huge-file 測試）。
- **API-M4 Engine services**：`Engine/Core/include/Nexora/Core/Services.h` 的 `MonotonicNanoseconds`/`RandomStream`（PCG32，含 version）/`Configuration` 搭配既有 `FixedTickClock`（game time/fixed tick）、`AsyncLogService`（structured logging）、`JobSystem`（task dispatch）、`EventBus`（event subscription）已覆蓋清單全部九項。`ProfilingMarker` 新增了 `SetSink` 這個 plain-function-pointer emission hook（此前只是計時器，沒有任何輸出機制）。`Tests/API/ApiCoreContractTests.cpp` 涵蓋 `MonotonicNanoseconds` 單調性與 `ProfilingMarker` sink 呼叫驗證。

**Sample**：`Samples/Api/ApiFoundationSample.cpp`（`NEXORA_FEATURE_API_SAMPLES`，預設 ON）是一個會被實際編譯、執行的最小範例，涵蓋 Math/Types/VFS/Services，並以 `samples.api_foundation` 掛進 CTest；不是 `Roadmap/V1-Visual-Showcase-Long-Term-Plan.md` 定義的視窗化 `NexoraShowcase`（那是獨立的 V1-M0～M12 專案，需要 Windows/DX12）。

以上驗證僅在 Linux `linux-development` preset 跑過（含一次手動 ASan/UBSan 編譯，對新增測試與 sample 無記憶體錯誤/UB/洩漏）；Windows/macOS/Android/iOS 未在此驗證。

## 9. API-M5～M6 現況（2026-09-22）

- **API-M5 World/Game facade**：新增 `Engine/Runtime/include/Nexora/Game/GameWorld.h`。`World::FindEntity`/`FindScene` 回傳的是 `World` 自己 `std::vector<Entity>`/`std::vector<Scene>` 內部儲存的指標，`CreateEntity`/`LoadScene` 隨時可能讓它失效；`GameWorld` 把 spawn/destroy、get/set、batch query（OR-mask）、scene load 都包成只回傳 `nexora::runtime::Id`（純量）與 `EntitySnapshot`（by-value）的介面，滿足 roadmap「不得讓 Zig 拿到可能被搬移的 C++ ECS storage pointer」的明文要求。`Id` 本身**沒有**另外包一層 generational handle——它已經是單調遞增、永不重複使用的識別碼，屬於 V1 完整規劃書 ABI 規則自己列的「EntityID」這個獨立可跨 ABI 類別，跟 index+generation 的 Opaque Handle 是分開的兩種，疊加沒有意義。Camera/light/mesh-renderer 透過 `EntitySpawnDescriptor`/`EntitySnapshot` 做到 entity-integrated；asset reference 直接重新匯出既有的 `AssetUuid`（API-M2）；input snapshot 包了一層 `InputSystem::Consume`。**尚未做**：`PhysicsWorld`/`CharacterController`/`AudioMixer` 目前仍是獨立系統，用自己的 `SimulationId`/資源 id 空間，沒有跟 entity 綁定——這是更大的設計，這次沒有處理。
- **API-M6 bindings**：新增 `Engine/Runtime/include/Nexora/Game/GameplayHostBridge.h`，把既有的 `NexoraGameplayHostV2` C ABI（`Nexora/Foundation/GameplayABI.h`，「Zig gameplay bridge」那個 contract）接到真正的 `GameWorld`——在這之前，`read_component`/`write_component` 這些 function pointer 槽位只有 `Gameplay/Zig/ZigGameplayTests.cpp` 裡一個測試用的假 host（`HostState`，自己存一個私有值，跟任何真實 `World` 無關）填過。`MakeHost()` 產出的 host 讓 `read_component`/`write_component` 真的讀寫一個 entity 的 `Transform`，component_type 用 `TransformComponentType()`（`nexora::foundation::Name("Nexora.Transform")` 的穩定 FNV-1a hash，不是雙方各自硬編一個魔術數字）。目前已接上 Transform、camera、light、mesh-renderer 四種 component type，並各自使用明確 wire layout，不直接跨 ABI 暴露 Runtime C++ component layout。`log` 現在會在 `GameplayHostContext::log` 有設定時轉發到真正的 `core::AsyncLogService`（先前不管有沒有設定都是純 no-op）——會先驗證 level 落在 `LogLevel` 自己的範圍內才轉型，訊息用 `(指標, 長度)` 這一對建構，不假設有 NUL 結尾；沒設定 log 的話仍然維持 no-op。`subscribe_event`/`set_tick_enabled` 老實回報「沒做」（回傳失敗/什麼都不做），不假裝成功——這不只是還沒接線的問題：ABI 本身根本沒有讓 host 在事件觸發時回呼 module 的 function pointer 槽位，而讓 `GameplayModuleHost::Update` 受控於 tick 開關則需要一個真正跨 namespace 的設計決定，不是這次順手能做的。**沒有驗證的部分**：`Gameplay/Zig/src/game_module.zig` 跟它的測試完全沒有改動——這個環境裡沒有 zig 工具鏈（`which zig` 失敗），改了 `.zig` 檔案也沒辦法在本地重新編譯驗證，只有 `gameplay.zig_abi_smoke` 的 CI runner 上有 zig。`GameplayHostBridge` 改成純 C++ 測試驗證（`Tests/Runtime/GameplayHostBridgeTests.cpp`，直接呼叫建置出來的 `NexoraGameplayHostV2` function pointer）。讓 Zig sample 真的透過這個 bridge 呼叫（取代它自己那個私有假 host）還是待辦。

驗證：`linux-development` 24/24、`linux-shipping`（Monolithic，因為這次新增了 Runtime 的 link 面）、外加一次 ASan/UBSan 手動編譯，均乾淨。Windows/macOS/Android/iOS、Zig 側整合未在此驗證。
