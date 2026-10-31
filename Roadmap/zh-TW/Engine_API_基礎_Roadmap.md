# Nexora Engine API 基礎 Roadmap

> 版本：v1.0｜狀態：規劃基線｜更新：2026-09-21

> **進度：100%**（portable roadmap scope；API-M1～API-M6 均通過本文定義的 gate。
> Target-platform runtime evidence 不得解讀為已在 Linux cloud 驗證。）

**已完成：** ✅ API-M1 Math/geometry；✅ API-M2 foundation types；✅ API-M3 VFS/I/O；
✅ API-M4 services；✅ API-M5 World/Game facade；✅ API-M6 C/Zig bindings 與 ABI versioning。

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

### ✅ 3.1 Math 與幾何（API-M1）

`Scalar`、`Vector2/3/4`、`Quaternion`、`Matrix3/4`、`Transform`、`Color`、`Rect`、`Ray`、`Plane`、`Aabb`、`Sphere`、`Frustum`。提供 dot/cross、normalize-safe、lerp/slerp、TRS compose/decompose、inverse、projection/look-at、intersection 與 epsilon policy。

驗收：scalar/SIMD 結果容差測試、NaN/zero-length policy、座標系 golden tests、序列化 round trip，並對 C ABI layout 做 `sizeof/alignof/offsetof` gate。

### ✅ 3.2 基礎資料型別（API-M2）

- UTF-8 `StringView` 與 engine-owned `String`；明確 byte/code-point 邊界、format、parse、case-folding 限制。
- `Uuid`、`Name/StringId`、generational handle、`Result/ErrorCode`、`Span`、`ByteBuffer`。
- engine allocator-aware array/map 只作 C++ convenience；ABI 使用 caller buffer 或 engine-owned opaque buffer + destroy function。

驗收：無效 UTF-8、embedded NUL、locale-independent number parsing、hash collision diagnostics、跨模組配置/釋放測試。

### ✅ 3.3 VFS 與檔案 I/O（API-M3）

公開 mount URI（如 `content://`、`user://`、`cache://`）、canonical path、同步小檔與 async request、stream、enumeration、metadata、atomic write、watch notification。定義 sandbox、symlink、case sensitivity、取消、partial read 與 completion thread；Shipping 禁止任意 host filesystem escape。

驗收：memory/temp/bundle backend contract suite、路徑 traversal、取消競態、atomic replace、超大檔 offset 與錯誤注入。

### ✅ 3.4 Engine services（API-M4）

公開 monotonic time、game time、fixed tick、random stream（seed/version）、structured logging、profiling marker、configuration、task dispatch、event subscription。禁止 gameplay 以 wall clock 驅動 simulation；callback 明訂 thread、lifetime、unsubscribe 與 reentrancy。

### ✅ 3.5 World/Game API（API-M5）

用 opaque `WorldHandle`、`EntityHandle` 與 component type ID 提供 spawn/destroy、get/set/batch query、deferred command、scene load、asset reference、input snapshot、camera/light/renderable/physics/audio facade。第一版不向 Zig 暴露 C++ ECS storage pointer，避免 archetype relocation 破壞 lifetime。

### ✅ 3.6 Language bindings 與版本化（API-M6）

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

依第 2 節「文件、單元測試、ABI test 與 sample 缺一不可」核對，API-M1 至 API-M4 已完成 portable roadmap scope：

- **API-M1 Math（portable implementation 已完成）**：`Engine/Foundation/include/Nexora/Math/Math.h` 已交付完整 value surface、safe normalization、interpolation、TRS compose/decompose（含 reflected scale）、inverse/projection/look-at、geometry intersection 與文件化 epsilon policy。`Tests/API/ApiFoundationTests.cpp` 鎖定所有 POD 的 ABI layout、byte round trip、zero/non-finite 行為、geometry boundary 與 transform round trip。`Vector4::Dot` 和 `Matrix4` 乘法均有 SSE2 與 ARM NEON 實作，並保留永遠可用的 scalar reference；deterministic fuzz test 以考慮 reduction order 的容差，比對目前選用路徑與 scalar 結果。座標 golden 常數由 DirectXMath 的 right-handed look-at/perspective 函式獨立產生，再從其 row-vector convention 轉成 Nexora convention，因此 handedness、layout 或 depth 錯誤不能靠 Nexora 自己推導的期望值自我通過。Public sample 會實際使用 transform、camera、projection 與 frustum。Cloud Linux x86-64 gate 驗證 scalar 與 SSE2；NEON 已實作且會在 ARM 編譯目標自動選用，但 ARM runtime evidence 必須在 ARM target 補跑，本次 Linux 驗證不會宣稱已執行 ARM。
- **API-M2 基礎型別（Roadmap scope 已完成）**：`Engine/Foundation/include/Nexora/Foundation/Types.h` 有 UTF-8 驗證、`StringView`/`String`/`ByteBuffer`/`Span`、`Uuid`（含 `Parse`/`ToString`）、`Name`、`Result<T>`、locale-independent `ParseNumber`。Generational handle 由既有的 `Nexora::Core::Handle<Tag>`/`HandlePool<Tag>`（`Engine/Core/include/Nexora/Core/Handle.h`）滿足，故意不在 Foundation 重複一份。`Engine/Foundation/include/Nexora/Foundation/DataAbi.h` 以 pointer-length view、會回報必要大小的 caller-owned copy，以及附 matching destroy function 的 opaque Foundation-owned buffer 完成跨 ABI contract。UTF-8 constructor 會保留 embedded NUL 並拒絕 malformed encoding。編譯後的 API test 涵蓋 invalid argument、無 partial write 的 sizing、borrowed-view lifetime 語意，以及跨 Modular Foundation 邊界的 allocation/destruction；API sample 也會使用同一套公開 C 介面。
- **API-M3 VFS（roadmap scope 已完成）**：`VirtualFileSystem` 以同一套 URI surface 提供 directory、memory、唯讀 platform-package adapter，以及 Runtime bundle mount。功能包含完整與 64-bit range read、positional read stream、具 priority/deadline/alignment 選項且在 I/O 前後檢查取消的 async request、metadata、排序 enumeration、atomic replacement、明確 polling 的 watch，以及唯讀 mapping（host file 使用原生 mapping；memory/package 使用 owned snapshot）。Canonicalization 會拒絕 absolute path、反斜線、traversal 與 symlink escape。Shipping 中公開的任意 host-directory mount 會被停用，`Engine` 則保留 private trusted bootstrap path。Contract gate 覆蓋 backend parity 與失敗注入、取消/alignment、mapping、watch lifecycle、唯讀 package，以及超過 4 GiB 的 sparse-file read/map。相同的 in-flight request 會 coalesce 到同一次底層讀取，而每個回傳 handle 仍保有獨立的取消結果。
- **API-M4 Engine services（Roadmap scope 已完成）**：`Engine/Core/include/Nexora/Core/Services.h` 的 `MonotonicNanoseconds`/`RandomStream`（PCG32，含版本的 checkpoint）/`Configuration` 搭配 `FixedTickClock`（game time/fixed tick）、`AsyncLogService`（structured logging）、`JobSystem`（task dispatch）、`EventBus`（event subscription），已覆蓋清單全部九項。`ProfilingMarker::SetSink` 是 plain-function-pointer emission hook；`kEngineServicesApiVersion` 與 random checkpoint version 明確定義相容性。Core contract 已說明 callback thread、ownership、lifetime、unsubscribe、reentrancy、failure 與 deferred-dispatch 行為。`Tests/API/ApiCoreContractTests.cpp` 驗證 monotonic time、profiling emission/停用、random checkpoint 精確 replay 與不相容版本拒絕、configuration 成功/失敗、event self-unsubscribe/reentrant publish/deferred dispatch thread、worker-thread dispatch，以及 task failure containment。`Tests/Core/CoreTests.cpp` 另會驗證 JobSystem drain、join 時釋放 callback capture，以及 shutdown 後 restart。會編譯的 API sample 也會使用 random、configuration 與 profiling service。

**Sample**：`Samples/Api/ApiFoundationSample.cpp`（`NEXORA_FEATURE_API_SAMPLES`，預設 ON）是一個會被實際編譯、執行的最小範例，涵蓋 Math/Types/VFS/Services，並以 `samples.api_foundation` 掛進 CTest；不是 [`V1-Visual-Showcase-Long-Term-Plan.md`](V1-Visual-Showcase-Long-Term-Plan.md) 定義的視窗化 `NexoraShowcase`（那是獨立的 V1-M0～M12 專案，需要 Windows/DX12）。

以上驗證僅在 Linux `linux-development` preset 跑過（含一次手動 ASan/UBSan 編譯，對新增測試與 sample 無記憶體錯誤/UB/洩漏）；Windows/macOS/Android/iOS 未在此驗證。

## 9. API-M5～M6 現況（2026-09-22）

- **API-M5 World/Game facade**：新增 `Engine/Runtime/include/Nexora/Game/GameWorld.h`。`World::FindEntity`/`FindScene` 回傳的是 `World` 自己 `std::vector<Entity>`/`std::vector<Scene>` 內部儲存的指標，`CreateEntity`/`LoadScene` 隨時可能讓它失效；`GameWorld` 把 spawn/destroy、get/set、batch query（OR-mask）、scene load 都包成只回傳 `nexora::runtime::Id`（純量）與 `EntitySnapshot`（by-value）的介面，滿足 roadmap「不得讓 Zig 拿到可能被搬移的 C++ ECS storage pointer」的明文要求。`Id` 本身**沒有**另外包一層 generational handle——它已經是單調遞增、永不重複使用的識別碼，屬於 V1 完整規劃書 ABI 規則自己列的「EntityID」這個獨立可跨 ABI 類別，跟 index+generation 的 Opaque Handle 是分開的兩種，疊加沒有意義。Camera/light/mesh-renderer、physics、character 與 audio 均透過 `EntitySpawnDescriptor`/`EntitySnapshot` 做到 entity-integrated；physics hit 會解析回 entity ID，character tick 會同步 entity Transform，entity 銷毀時也會移除 simulation/audio binding。Audio resource 在每個 world 內保持唯一，讓以 resource stop 的行為具決定性。Asset reference 直接重新匯出既有的 `AssetUuid`（API-M2），input snapshot 則包裝 `InputSystem::Consume`；可選的 physics/character API 會跟隨 gameplay-simulation feature switch。至此 API-M5 已完成 roadmap scope。
- **API-M6 bindings**：新增 `Engine/Runtime/include/Nexora/Game/GameplayHostBridge.h`，把既有的 `NexoraGameplayHostV2` C ABI（`Nexora/Foundation/GameplayABI.h`，「Zig gameplay bridge」那個 contract）接到真正的 `GameWorld`——在這之前，`read_component`/`write_component` 這些 function pointer 槽位只有 `Gameplay/Zig/ZigGameplayTests.cpp` 裡一個測試用的假 host（`HostState`，自己存一個私有值，跟任何真實 `World` 無關）填過。`MakeHost()` 產出的 host 讓 `read_component`/`write_component` 真的讀寫一個 entity 的 `Transform`，component_type 用 `TransformComponentType()`（`nexora::foundation::Name("Nexora.Transform")` 的穩定 FNV-1a hash，不是雙方各自硬編一個魔術數字）。目前已接上 Transform、camera、light、mesh-renderer 四種 component type，並各自使用明確 wire layout，不直接跨 ABI 暴露 Runtime C++ component layout。`log` 現在會在 `GameplayHostContext::log` 有設定時轉發到真正的 `core::AsyncLogService`（先前不管有沒有設定都是純 no-op）——會先驗證 level 落在 `LogLevel` 自己的範圍內才轉型，訊息用 `(指標, 長度)` 這一對建構，不假設有 NUL 結尾；沒設定 log 的話仍然維持 no-op。`subscribe_event`/`set_tick_enabled` 會同步委派給 `GameplayHostContext` 中由 embedding host 擁有的可選 hook；未提供 subscription hook 時明確回傳 unsupported，未提供 tick hook 時則安全 no-op，而 event delivery 與 update scheduling 的 ownership 仍留在 embedding host。`GameplayHostBridge` 仍由純 C++ 測試驗證（`Tests/Runtime/GameplayHostBridgeTests.cpp`，直接呼叫建置出的 `NexoraGameplayHostV2` function pointer）。V3 Zig Showcase 使用獨立的 V3 host table，不會把 V2 的 `MakeHost()` facade 偷換成 V3 contract。在本機 Windows `windows-zig-showcase` preset 下，`Gameplay/Zig/src/game_module.zig` 已用 repository-local Zig 0.14.0 編譯；`NexoraShowcase` 透過 V3 ABI 驅動真正的 `GameWorld` Transform，並產生通過的 headless render/reload report。本機結果不宣稱 Linux gate 或視窗化 native backend 已完成。新增的 append-only component wire `NexoraTransformV2` 與 `NexoraParent` 提供 `"Nexora.TransformV2"`、`"Nexora.WorldTransform"`（唯讀）與 `"Nexora.Parent"`；layout 由 C11 `_Static_assert`、baseline 與 Zig `comptime` 檢查固定，Zig 可交叉編譯到 Windows、macOS、Android 與 Linux；所有 host table 都透過同一條共用的 `GameWorld` 讀寫路徑。 唯讀 world-transform 的位置現與深層鏡像／剪切 ancestry 的精確 affine matrix 原點一致；C／Zig wire layout 不變（`runtime.entity_parenting`）。

- **API-M6 bindings 與版本化（Roadmap scope 已完成）**：`Engine/API/include/nexora/nexora.h` 是 canonical C11/C++ 宣告，舊 Foundation header 僅保留為 compatibility include。`Engine/API/abi_manifest.json` 提供每一項 export 所需的 metadata；`abi_baseline_v3.json` 與 `api.m6_manifest_compatibility` 強制 V3 descriptor 只能 append-only 演進，另以實際編譯的 C11 consumer 驗證 layout 與 include。`Bindings/Zig/nexora.zig` 是 Zig gameplay sample 使用的 thin wrapper。真正的 V2 `GameplayHostBridge` 會把 event subscription 與 tick control 委派給可選、由 embedding host 擁有的同步 hook，不再把已接線的整合做成 silent no-op。Linux gate 會共同檢查 C++、C11、manifest、baseline 與 Zig view；不宣稱其他作業系統已驗證。

上方的歷史 Linux 驗證仍代表 API-M5/M6 變更已通過；2026-09-23 的 Windows run 另外通過
`windows-zig-showcase` 完整 CTest suite（26/26），包含 Zig ABI smoke test 與 headless Showcase
驗證。Linux/macOS/Android/iOS 與視窗化 native backend 不在這次本機 Windows 結果之內。


## 可選 Zig 2D 引擎介面（2026-10-14）

✅ 原 V3 前綴新增可選、大小／能力／版本檢查的 2D 擴充。複製的 Sprite／Collider／
相機、動畫事件緩衝、查詢快照、Tilemap 與世代池化批次沿用既有 TwoD 服務。實際 Zig
靜態／動態／Reload 及布局契約通過；Linux 原生輸入驗收呈現 600 幀並確認 Zig 移動。
見 [Zig 使用範例](../../Gameplay/Zig/README.md)與[2D Roadmap](2D_Roadmap.md)。此引擎
控制擴充不改變本 Roadmap 原進度，也不代表實體 Windows gate 已完成。

2026-10-15 的 GPU Instancing 交付沿用上述 V1 池化批次，不改 C／Zig Wire Layout。
原生 2D 繪製／上傳證據及後續 GPU API 範圍見 [2D Roadmap](2D_Roadmap.md)。

## 可選 Zig 遊戲服務（2026-10-16）

✅ V3 尾端新增服務協商，分別提供 C11／Zig 動作與指標輸入、HUD／選單／字型圖集、音訊、
永久槽位存檔，以及 cooked Atlas／Clip 註冊介面。Zig 可自行設定操作、提交 UTF-8 UI、複製
內嵌字型／WAV／2D cooked 素材、控制聲音，以及保存／還原自訂 schema 的資料，無須遊戲專用 C++。
主機 ownership、序列化呼叫、錯誤輸出保留與舊前綴協商約定見
[Zig 使用說明](../../Gameplay/Zig/README.md#optional-zig-gameplay-services)。

驗收使用真實 Zig／C++ 每個欄位的布局與呼叫（`gameplay.zig_services_consumer`）、各服務的失敗與
生命週期測試、ProjectPlayer 動態 V3 模組啟動，以及 Linux 原生呈現。
[驗收紀錄](../../Tools/Build/evidence/Zig-Gameplay-Services-Linux-2026-10-16.md) 保留完整 preset 結果與限制。
原有 API-M1–M6 完成比例維持不變；不代表 Windows 裝置輸出、實體手把、遊戲美術或 Editor Play 整合已驗收。


✅ 後續專案 host 整合已在主 Editor 2D Play 與通用、不依賴 Editor 的專案 Player 提供既有
服務，沒有改動 C／Zig wire。凍結素材 ownership、通過擷取規則的輸入、Game View UI／精靈
合成、原生音訊重試、使用者永久槽位與明確可信模組匯出，仍由 embedding 負責。真正 Zig
Shipping 啟動、獨立模組重啟／損壞檢查及 Linux Editor 原生像素已有驗證；Windows 實體裝置／
DPI／乾淨電腦驗收另列。[證據](../../Tools/Build/evidence/Zig-Project-Player-Linux-2026-10-16.md)。

## SP-01 獨立 Player 顯示整合（2026-10-23）

✅ 可攜實作與回歸檢查：252 項通過、16 項原生前提不足而跳過、0 失敗；Monolithic Shipping consumer 通過。

獨立 2D Player 已加入可選專案視窗設定、尾端擴充且獨立協商的 C／Zig 顯示 table、
每幀一致的像素／logical／DPI 快照、原生視窗模式請求、遊戲指定畫框比例、安全畫框
投影／裁切，以及正確 DPI 的 UI／輸入。舊 services consumer 與專案描述檔保留原有
前綴／預設值。世界鏡頭跨度、HUD 排版與 UI 縮放仍由遊戲決定；本次未接入 Editor Play。

參見[顯示契約](../../Engine/TwoD/GameplayDisplay.md)、
[Linux 證據](../../Tools/Build/evidence/SP01-GameplayDisplay-Linux-2026-10-23.md)與
[Windows 操作程序](../../Tools/TwoD/WindowsDisplayAcceptance.md)。
Windows 實機 DPI／點擊／模式驗收維持 NOT RUN／NOT ACCEPTED；2D M6／M7 與既有里程碑
完成數不變，可攜實作檢查不等同於通過上述驗收。

✅ X11 原生關閉回歸：Xvfb 驗證事件處理前已銷毀視窗的查詢與清理；修正前 DSO 在同一探針失敗。

## SP-02 圖像 UI 與 Zig 組合（2026-10-23）

✅ 已實作圖像／9-slice UI、獨立互動判定、字形 metrics、可回收字型 handle 及快照生命週期
貼圖回收，透過獨立協商的 C／Zig table 提供。原版與 SP-01 services 前綴維持相容。Zig
診斷畫面涵蓋 HUD 槽位、選卡、整備、換行字幕與設定，宿主不承載遊戲規則。

可攜式 NXFONT1 工具嚴格盤點輸入全文字元、來源／字型 hash 與圖集預算。企劃文件覆蓋
不等於最終台詞驗收，診斷圖示不算正式美術。見 [UI 契約](../../Engine/TwoD/GameplayUiDraw.md)
與[雲端證據](../../Tools/Build/evidence/SP02-ImageUi-Cloud-2026-10-23.md)。正式美術、Windows
字型／輸入、實體手把及既有里程碑驗收仍保持未完成。

## SP-03 延後素材載入與常駐生命週期（2026-10-23）

✅ 已實作有界目錄 IO、C／Zig 複製請求狀態、明確資產移除、依相依順序卸載及快照感知的原生貼圖回收。
原版／SP-01／SP-02 services 前綴與舊專案預載預設維持相容。容量可明確設定，不是無條件提高 64 MiB；
轉場、遊戲規則與事件政策仍由 Zig 控制。

可攜驗證涵蓋五張同時常駐的 2048² 圖集（超過 80 MiB）、八輪卸載／重載、舊版本預算背壓及實際程序
RSS／working-set 觀測。獨立 Zig 模組從超過 80 MiB 的封裝啟動，驗證 15 次轉場、1,800 個 pooled instances、
30 個 Animator 事件及毀損檔案拒絕。原生像素測試檢查 2048² GPU 新版本與保留畫面的回收；平台實際執行
證據須與僅編譯區分。見[契約](../../Engine/TwoD/GameplayAssets.md)及
[雲端證據](../../Tools/Build/evidence/SP03-AssetResidency-Cloud-2026-10-23.md)。

正式遊戲素材／全內容、完整拾取／戰鬥轉場及目標實機 RAM／VRAM／幀時間預算尚未驗收。
音訊串流／壓縮、色彩及既有 M6／M7 門檻保持原判定。

## SP-04 明確美術色彩與 2D fallback（2026-10-23）

✅ PNG 可明確選擇 sRGB 匯入，RGB 只解碼一次為既有 linear RGBA8 圖集；保留 straight alpha、
歷史預設、Atlas 格式及 services ABI。衝突 profile／ICC 在發布前拒絕，Zig 原始圖像／tint 工具
共用同一色彩契約。
✅ 原生 2D Player 明確要求 Vulkan／DX12／Metal 硬體 sRGB UI／Sprite 輸出；保留既有混合
3D 表面預設，原生像素測試只接受正確編碼灰階，不將 UNORM 當成通過。
✅ 獨立匯出的 Zig 診斷包含正俯視分層、Y 穿越、前景淡出、烘焙 glow／shadow 關閉及始終保留
的危險外框。這些是工程素材與一般 alpha 圖層，不是動態局部光、遮擋柔影或選擇性 bloom。

見[色彩／fallback 契約](../../Engine/TwoD/ColorAndFallback.md)及
[雲端證據](../../Tools/Build/evidence/SP04-ColorFallback-Cloud-2026-10-23.md)。
正式第一關美術、暗部漸層量化、透明過濾／白邊、可讀性、Windows 目標實機輸出與效能仍待驗收
（`nexora-ofe.1`）。原生測試須實際執行後才能宣稱像素通過，本機編譯不算。既有 M6／M7 gate
與其他 SP 缺口維持原判定。

## SP-05 有界本機語音播放（2026-10-23）

✅ 解碼 PCM 已有 active／live／數量預算，移除或替換後仍被聲音持有的樣本也正確計量。
可選 AudioState 服務提供用量快照與獨立 Voice bus；保留 Audio V1 及原版／SP-01／02／03
services 前綴。
✅ caller-owned Zig 對話控制器沿用 deferred 本機 blob，註冊 WAV 後播放／暫停／跳過／重播，
提供獨立語音音量／靜音與字幕 fallback。Player 的 audio capacity 與素材預算分開，完整語音包
不必啟動預載。
✅ 標準函式庫盤點工具可計算已宣告 WAV 的檔案量／時長、字幕 ID 缺漏與併發 PCM 集合；
metadata／工程語音報告不算正式配音全集完成。

✅ API-M6 已納入 AudioState 的 C／Zig 欄位順序及四個 callback 契約；變異測試拒絕移除／重排。
Zig 會記住載入中提出的暫停，在混音前套用，且重播沿用暫停直到明確恢復。

可攜驗證涵蓋真實 PCM 樣本、故障／重新配置、取消與舊前綴 canary；獨立 Zig 模組／Player
測試超過 96 MiB 離線安裝包、十二句各 87 秒工程語音、16 MiB peak PCM 及最終常駐歸零。
捕捉／null sink 與 RSS 採樣是診斷證據，不等於喇叭可聽或正式 RAM／幀時間驗收。見
[語音契約](../../Engine/Runtime/GameplayVoice.md)及
[雲端證據](../../Tools/Build/evidence/SP05-OfflineVoice-Cloud-2026-10-23.md)。

正式配音／字幕全集、完整離線安裝、最壞併發及 Windows 實機播放／拔除／恢復／長測仍由
`nexora-6d6.1` 追蹤。按句 PCM 路徑沒有實作壓縮解碼、音訊磁碟串流或 realtime mixing。
其他 SP 評估與 M6／M7 驗收維持原狀。

## SP-06 遊戲生命週期、關閉確認與震動（2026-10-23）

✅ 可選 Lifecycle V1 保留所有既有 services 前綴（含 SP-05 的 88 bytes），並檢查 C／Zig
欄位順序與每個 callback 的 API metadata。焦點／最小化／電源／斷線序號保留同批中斷，
固定更新前即可讀取；明確啟用 guard 後，Zig 可取消關閉 token，或實際保存成功後接受。
未啟用的舊模組維持原關閉行為。
✅ Windows 轉譯休眠／恢復及不被 resize 合併掉的最小化／恢復事件。XInput 有時限震動
回報能力與原生錯誤，在 owner tick 到期及失焦／最小化／休眠／關閉／Stop 時要求歸零。
DirectInput／DS4／Linux 震動仍明確不支援。
✅ 實際 Zig V3 診斷保留待選標記，在固定模擬前暫停，處理原子保存失敗重試及重新開啟暫停，
明確確認後三秒準備。可攜測試涵蓋原生輸出失敗重試、token／前綴／執行緒與實際 Storage
舊檔保留；獨立匯出 Player 驗證協商、UI 與資源所有權。

見[生命週期契約](../../Engine/Runtime/GameplayLifecycle.md)及
[雲端證據](../../Tools/Build/evidence/SP06-Lifecycle-Cloud-2026-10-23.md)。
正式遊戲卡片／交易接入及 Windows 實機焦點、最小化、拔插、休眠、震動仍待驗收
（`nexora-4j7.1`）；訊息注入不等於真實休眠或馬達感受。
本輪不代表 SP-06 正式產品驗收、M6／M7 或其餘 SP 缺口已完成。

前景恢復後的新移動輸入會保留；UI 確認仍等待中立狀態重新啟用。

## HD2D-01 選用契約擴充

✅ 新增獨立協商 effects table、services 尾端 getter、Zig wrapper 與原子快照，保留原有前綴及 V3。ABI baseline／metadata 與防重排測試已補齊；不宣稱原生效果完成。 [HD2D plan](HD2D_Lighting_Plan.md).

✅ HD2D-03 在 effects table 尾端加入獨立協商的 pool 識別 callback，保留原有前綴及 Sprite wire。完整 owner／index／generation 與 C／Zig／原生 gate 已由 PR #736／`40c96072` 驗收；證據依 HD2D 工單追蹤。

HD2D-06 施工加入獨立協商的診斷／陰影圖檢視尾端（40／48／56／64-byte 完整前綴）、C／Zig 佈局與相容性防護，以及已完成原生 GPU 觀測；metadata 共 137 exports。原生驗收依 HD2D 工單追蹤。
