# 跨平台 3D Engine — V1 完整規劃書
**文件版本：Master Draft v1.2**
**Engine 世代：V1.x — Production Foundation**
**目標平台：Windows / macOS / Android / iOS**

> 本文件為 **V1 Master Plan**。它整合先前完整引擎架構分析、Architecture Completion Baseline、Scene/World/Streaming/HLOD、Input、UI、Physics、Animation、Audio、DataTable、Editor、Asset/Bundle、Build/CI，以及最新的 **Plugin SDK / Bridge Layer / 第三方擴充 / Video & Media Framework**。
>
> 若前段較早的 Plugin 摘要與文件末端「V1 Plugin / Media 詳細 Contract」有差異，**以末端詳細 Contract 為準**。
>
> V1 的定義不是「Prototype」，而是：可用同一套架構完成並發布一般 3D 遊戲，且不阻斷 V2/V3 的 Large World、Networking、Distributed Simulation、GPU Simulation 與 ML 擴展。


> v4.0 目的：在 v3.33 已完成的 Scene / World、Input、Runtime UI、Physics、Animation、DataTable、WebView 等設計之上，將尚未細談的 Transform、System Scheduler、Event、Camera、Lighting、PostProcess、Navigation、AI、Editor、Prefab、Undo、Reflection、Serialization、Asset Import、VFS、Save、Localization、Platform、Logging、Memory、TaskGraph、Timer 與 Authoring Tools 補成可實作的正式 Framework。


## 一、引擎定位

目標：
- 本引擎採 AI-Assisted Development，由 AI 大量協助架構設計、程式碼生成、測試生成、文件維護與重構
- 所有 AI 產出必須經過自動化驗證 Gate，不以「生成完成」視為「功能完成」
- 原生 C++20 高效能 3D Engine
- Engine Core 維持 C++20；Zig 作為 Primary Gameplay Language
- Gameplay 語言透過 Language-Neutral Stable C ABI 接入，避免綁死單一語言
- 支援 Windows / macOS / Android / iOS
- V1 Renderer 正式實作 DX12 + Vulkan + Metal
- RHI 從第一天同時支援 DX12 / Vulkan / Metal
- 不以 OpenGL 作為核心渲染路線
- Windows 預設優先使用 DX12，並允許切換 Vulkan
- Editor 採 Node + Component 工作流
- Runtime 底層採 Data-Oriented / SoA Component Pool
- Renderer 與 Scene Graph 分離
- World 與 Scene 分離；同一 World 支援 Multiple / Additive Scene 與 Persistent Scene
- World Partition 支援 Stable Grid Cell + Loose Quadtree + Room / Portal Hybrid Streaming
- HLOD 由 Editor / Cooker 離線生成，Runtime 僅進行選擇與 Streaming
- 自有 Editor
- 自有 Asset Pipeline
- 自有 Runtime UI
- 內建 Terrain / Vegetation / 空間裁剪 / Mesh LOD / Texture LOD & Streaming
- 畫質目標至少對標 Unity URP
- 正式支援 PBR / StylizedPBR / Anime / Vegetation / Water / Unlit Shading Model
- Data-Oriented VFX / Particle Framework：1 VFX Instance + N Logical Emitters + Runtime Batch Fusion
- Animation Framework：Animation Graph + GPU Skinning + Skinned Instancing + Automatic Bone Animation Texture / GPU Crowd Animation
- 動畫風場景採 StylizedPBR，角色採 Anime NPR，可在同一 Forward+ / RenderGraph 混合
- 手機優先，同時兼顧桌面高畫質
- 明確控制 CPU / GPU / RAM 預算
- 架構上保留未來擴充 Networking、DX12、GPU Driven Rendering 等能力
- Bundle Hot Update / Remote Manifest / Cache / Rollback

平台與 API：

```text
Windows  -> DX12（Default / Primary）
Windows  -> Vulkan（Selectable Secondary Backend）
Android  -> Vulkan（Primary）
macOS    -> Metal（Primary）
iOS      -> Metal（Primary）
```

原則：
- Gameplay / Scene / Material 不得直接依賴 Vulkan / Metal 型別
- 所有 GPU API 都經過 RHI
- 原始資產與 Runtime 資產分離
- Runtime 不依賴 Editor
- Source of Truth 必須明確
- Cache / Derived Data 不得與主資料混淆


## 二、主要技術選型

語言：
- Engine Core / Renderer / RHI / Editor：C++20
- Primary Gameplay Language：Zig
- Engine ↔ Gameplay 邊界：Language-Neutral Stable C ABI
- 架構保留未來加入 Rust / C# 等 Binding 的能力，但 V1 不依賴它們

Build：
- CMake
- Precompiled Headers (PCH)
- Runtime / Renderer / Editor 分離 PCH

Renderer：
- Direct3D 12（Windows Default）
- Vulkan（Windows 可切換 / Android Primary）
- Metal（macOS / iOS Primary）
- Unified RHI

Shader：
- Slang
- DX12   -> DXIL
- Vulkan -> SPIR-V
- Metal  -> MSL / Metal Shader

注意：
- Slang Metal backend 在正式採用前必須經過 PoC 驗證
- 不假設 Slang Metal 路線與 SPIR-V 路線同樣成熟

Editor UI：
- Dear ImGui
- Docking
- Multi-Viewport

Physics：
- Jolt Physics

Font：
- FreeType
- HarfBuzz

Audio：
- miniaudio

Navigation：
- Recast / Detour（V1 Default Backend，僅存在於 private backend）

Localization / Unicode：
- ICU4C（V1 Locale / Plural / Number / Date / Bidi / Break Iterator backend；依 Project Locale 裁切資料）

Persistent Allocator Backend：
- mimalloc（V1 Default Internal Backend，可替換；不進 Public ABI）

模型：
- glTF / GLB
- FBX Importer（Editor Only）
- OBJ（基礎支援）

Texture：
- PNG
- JPG
- TGA
- HDR
- EXR
- KTX2 / BasisU（後續可加入）

Scene：
- Node + Component API

Runtime Data：
- Component Pools
- SoA
- Job System
- Dirty Flag
- GPU Instancing
- 後續 GPU Driven Rendering


## Zig Gameplay / Native Scripting Contract

### 定位

Zig 為本引擎 V1 的 **Primary Gameplay Language**。

引擎核心仍維持：

```text
C++20 Engine Core
↓
Language-Neutral Stable C ABI
↓
Zig Gameplay Binding
↓
GameModule
```

Zig 不取代：

```text
Renderer / RHI
Asset Pipeline
Editor Core
Job System Core
Physics Backend
Audio Backend
Platform Backend
```

上述核心仍以 C++20 為主。

Zig 主要負責：

```text
Gameplay Logic
Game-specific Systems
Behavior
State Machine
AI Logic
Quest / Skill / Rule Logic
Game-side UI Logic
Project-specific Native Module
```

### 選用理由

Zig 作為 Gameplay Language 的主要目標：

- Native compilation。
- 無 tracing GC。
- Runtime / language overhead 小。
- Explicit allocator model 與本引擎 Arena / Pool / Lifetime Contract 相容。
- C ABI / C header interop 直接。
- 適合 Data-Oriented / Handle-based API。
- 適合開發期快速重編譯與 GameModule reload。
- 不要求 Engine Core 配合 borrow-checker / trait-object ABI 等語言專屬模型。
- Windows / macOS / Android / iOS 使用同一 Gameplay API Contract。

### Language-Neutral Gameplay ABI

Zig 不得直接依賴 Engine C++ private class layout。

正式邊界：

```text
Zig Gameplay
↓
Generated / Handwritten Safe Zig Wrapper
↓
Engine Gameplay C ABI
↓
C++ Public Gameplay API
↓
Engine Internal
```

跨 ABI 優先使用：

```text
Opaque Handle
EntityID
AssetID / UUID
POD / C-compatible Struct
Explicit Span / View
Versioned Function Table
Error Code / Result Struct
```

禁止直接跨 ABI 暴露：

```text
std::vector
std::string
C++ template type
SharedPtr<T>
Engine Internal class
C++ RTTI object
Exception
Backend native graphics object
```

### GameModule ABI

Gameplay module 使用版本化 C ABI entry point。

概念：

```c
GameModule_Init(...)
GameModule_Shutdown(...)
GameModule_Update(...)
GameModule_OnEvent(...)
GameModule_SerializeState(...)
GameModule_DeserializeState(...)
```

Module handshake 至少驗證：

```text
Engine API Version
Gameplay ABI Version
Build Configuration
Platform / Architecture
Module Build ID
Capability Flags
```

ABI mismatch：

```text
Reject Module Load
```

不得 partial initialize。

### Hot Reload Contract

Hot Reload 只允許經過明確 Module Boundary。

標準流程：

```text
Edit Zig Gameplay Code
↓
Compile New GameModule
↓
Stop New Gameplay Dispatch
↓
Reach Module-safe Barrier
↓
Serialize / Extract Module-owned Persistent State
↓
Shutdown Old Module
↓
Ensure No In-flight Call / Job / Callback References Old Module
↓
Unload Old Module
↓
Load New Module
↓
ABI Handshake
↓
Deserialize / Migrate State
↓
Resume Gameplay
```

不得假設「native DLL reload」可以自動保留所有程式狀態。

### Module-safe Barrier Definition

「Reach Module-safe Barrier」與「Ensure No In-flight Call / Job / Callback References Old Module」不是時間點假設，必須綁定既有 Job System 的 Completion Fence 機制（見 Thread-Safe Job System / Frame Synchronization Lifecycle），不得只靠文字約定。

```text
Identify all Job Class that may call into Zig GameModule
↓
Stop scheduling new instances of those Job Class
↓
Wait for Completion Fence of every in-flight instance
↓
Confirm no pending Callback / Event Dispatch still references old Module
↓
Module-safe Barrier reached
```

規則：

- 任何可能呼叫進 Zig GameModule 的 Job Class，必須在其 Task Handle 中登記，Hot Reload 才能正確列舉待等待對象。
- Barrier 判定必須查詢 Completion Fence 實際狀態，不得以「假設某個 Frame 已結束」代替。
- 若存在無法明確歸類的 Callback / Closure Context，Hot Reload 必須視為不安全並中止，而不是靜默略過。

### Hot Reload Lifetime Rule

Engine 不得在 module unload 後保留：

```text
Old Zig function pointer
Old module-owned object pointer
Old allocator-owned allocation
Old callback closure / context pointer
Old module code address
Old vtable-equivalent dispatch table
```

所有 callback / function table 在 reload 後必須重新註冊。

跨 Reload 持久資料優先：

```text
Engine-owned Handle / Data
或
Versioned Serialized State
```

而不是 module-native object graph。

### Hot Reload Platform Policy

Desktop Editor：

```text
Windows
→ GameModule DLL reload

macOS
→ Development dylib / equivalent local development module reload
```

Mobile：

```text
Android / iOS
→ Gameplay code 在 Build-time 編譯進 App / native package
→ 不透過 Remote Bundle 下載 native gameplay code
```

iOS Shipping 不以 runtime 動態下載 / 執行新 native code 作為 Gameplay Hot Update 機制。

Remote Update 仍只允許：

```text
Asset
Data
Scene
Localization
Bundle Content
Gameplay Data / Config
```

不得將 Zig native binary 放入 Remote Bundle 後下載執行。

### Memory / Allocator Contract

Zig Gameplay 可使用：

```text
Zig-local allocator
Engine-provided persistent allocator API
Engine temporary/frame API（僅在明確 lifetime contract 下）
```

但必須符合既有 Dynamic Module Allocator Contract：

```text
Module allocates
→ Module destroys

或

Engine allocator API allocates
→ Engine allocator API frees
```

禁止：

```text
Zig Module allocate
→ C++ Engine 使用不相容 allocator/free 直接釋放
```

Frame-lifetime memory 跨 Module 時仍只能使用既有：

```text
FrameSpan<T>
FrameDataHandle
```

不得因 Zig binding 另開例外。

### Gameplay API Data Model

Zig Gameplay 應優先操作：

```text
EntityID
Component Handle
Data Component View
Event
Command
System Batch
```

而不是持有 C++ Node / Component raw pointer。

Gameplay Component / Behavior 建議分層：

```text
Native Core Component
→ Transform / Render / Physics / Animation

Gameplay Data Component
→ Health / Faction / Inventory / Interactable / project-specific data

Zig Behavior / System
→ Event / Tick Registration / State Machine / Batch Update
```

不要求所有 Gameplay 功能建立固定 C++ Component class。

### Tick / Event Policy

Zig Gameplay 不預設採用：

```text
Every Component
→ virtual Update()
```

而是：

```text
Batch System
+
Event-driven
+
Explicit Tick Registration
+
Scheduler
```

Tick 可依需求分級：

```text
Every Frame
Fixed Tick
10 Hz
1 Hz
Event-only
Sleeping
```

避免大量細粒度 Engine ↔ Zig ABI call。

### Batch-first ABI

禁止：

```text
每個 Entity
每個 Component
每 Frame
C++ ↔ Zig 跨 ABI 多次細粒度呼叫
```

優先：

```text
Engine provides contiguous views / batches
↓
Zig processes batch
↓
Command / result batch returned
```

例如：

```text
UpdateAIBatch(...)
ProcessGameplayEvents(...)
UpdateSkillBatch(...)
```

### Reflection / Inspector Bridge

Canonical Engine Reflection Metadata 為 Source of Truth。

Zig gameplay type 若需要：

```text
Inspector
Serialization
Prefab Override
Save
Property Animation
```

必須透過：

```text
Zig Compile-time Metadata / Codegen
↓
Canonical Engine Reflection Schema
```

不得建立一套與 C++ Reflection 不相容的獨立 metadata 世界。

### Debug / Tooling

Editor 開發工作流目標：

```text
Double-click Zig gameplay source
↓
Open configured external IDE / editor
↓
Build GameModule
↓
Load / Reload
↓
Native Debugger Attach / Launch
```

至少提供：

```text
Build Error → Editor Console
Source File / Line Mapping
GameModule Build Timing
Reload Timing
ABI Mismatch Diagnostic
State Migration Diagnostic
Gameplay Allocation Metrics
```

### Cross-platform Build Contract

CI 必須驗證 Zig Gameplay Module：

```text
Windows x64
macOS arm64
Android arm64
iOS arm64
```

並依正式支援的 platform/toolchain profile 擴充。

Gameplay Public API 必須保持平台無關。

禁止 Gameplay Zig code 直接依賴：

```text
DX12
Vulkan
Metal
Android JNI internal backend
Objective-C++ Engine Internal
```

Platform-specific 功能必須經 Engine Public Platform API。

### AI / Safety Gate

由於 Zig 不提供 Rust 等級的 compile-time ownership / borrow safety，
AI 產生 Zig gameplay code 必須額外依賴：

```text
Handle Generation Validation
Debug Allocator
Bounds / Safety Checks（Development）
ASan / UBSan（可用平台）
Module Lifetime Guard
Frame Lifetime Guard
Static Analysis
Unit / Integration Test
Long-run Reload Test
```

AI 不得將「成功編譯 Zig」視為 lifetime 正確。

### V1 Definition of Done

- C++ Engine Core 與 Zig Gameplay 透過 Stable C ABI 完成基本整合。
- Zig 可建立、讀取與修改公開 Gameplay Component Data。
- Zig 可接收 Event 並註冊 Explicit Tick。
- Desktop Editor 可重新編譯並 Reload GameModule。
- Reload 前後 Engine-owned Entity / Asset / Component Handle 維持有效。
- Module-owned persistent state 可經 versioned state migration 恢復。
- Reload 後不存在舊 function pointer / callback / module allocation escape。
- Windows / macOS / Android / iOS CI 均能編譯最小 Zig Gameplay sample。
- Shipping Mobile 不依賴 runtime native-code download。
- Profiler 可觀察 Gameplay Update、ABI Call Count、Allocation 與 Reload Cost。

## 三、AI-Assisted Development 治理原則

本引擎明確採 AI-Assisted Development。

AI 的角色：
- 架構草案
- 模組實作
- 重構
- 測試生成
- Shader / RHI Backend 實作
- Editor Tool
- Serialization
- 文件維護
- Bug 分析
- CI Script
- 平台適配

原則：
AI 產出程式碼 ≠ 已完成程式碼。
任何 AI Session / Agent 的產出都必須符合：
1. 既定架構
2. Coding Convention
3. Static Analysis
4. Unit / Integration Test
5. Platform Validation
6. Module-specific Gate
7. Definition of Done


### AI 自主程度分類

A. AI 可高度自主生成：
- Reflection 樣板
- Serialization
- Asset Metadata
- Inspector UI
- Runtime UI Component
- Editor Utility
- RHI Backend 在既定 Interface 下的機械性實作
- Platform Wrapper
- Test Case
- Documentation
- Build Script
- Code Migration / Rename / Mechanical Refactor

B. AI 可生成，但必須通過工具驗證才能合併：
- Vulkan / Metal Barrier
- Resource State Transition
- Queue Synchronization
- Descriptor / Argument Buffer
- Slang Shader
- Render Graph
- Job System
- Lock-free / Concurrent Structure
- Memory Allocator
- GPU Driven Rendering
- Asset Streaming
- Save Migration

C. AI 不得只靠靜態推論視為完成：
- Android GPU Driver 相容性
- iOS / macOS Metal 真機行為
- GPU Crash
- Frame Pacing
- Thermal / Battery
- Mobile Memory Pressure
- Shader Compiler / Driver 特定問題

上述項目必須透過實體設備或對應平台工具驗證。


### AI 合併前強制 Gate

所有 AI 產出 PR 必須依影響範圍通過：

基礎 Gate：
- clang-format
- clang-tidy
- Build
- Unit Test
- No New Warning
- API Convention Check

Renderer Gate：
- Shader Compile Test（依 Dependency Graph 只編譯受影響 Variant）
- Render Graph Validation
- Resource Lifetime Validation
- Golden Image Test
- GPU Validation Layer
- GPU Capture Smoke Test

Asset Gate：
- Import / Reimport Test
- Dependency Test
- Deterministic Build Test
- Runtime Asset Load Test
- Bundle Dependency DAG Check

UI Gate：
- Layout Test
- Multi-resolution Test
- Draw Call / Batch Regression Check
- Text / Font Fallback Test

Platform Gate：
- Native Build
- App Launch
- Suspend / Resume
- Save Path
- Crash Handler
- Memory Budget Smoke Test

VFX Gate：
- VFX Graph Compile Determinism
- Dependency DAG Cycle Fail
- GPU Buffer Fence-safe Reuse
- Soft Particle / Billboard / Mesh / Ribbon Golden Image
- VFX Feature Stripping Validation

Animation Gate：
- Animation Graph Compile Determinism
- Bind Pose Hash Mismatch Hard Fail
- Vertex / Compute Skinning Golden Image
- GPU Animation Bake Determinism
- GPUAnimationClip Dependency Invalidation Test

只有 Gate 全部通過，才能 Merge。

### 多 AI Session / Agent 一致性

所有 Session 必須共享並遵守：
- ARCHITECTURE.md
- CODING_CONVENTION.md
- RHI_CONTRACT.md
- MEMORY_MODEL.md
- THREADING_MODEL.md
- ERROR_HANDLING.md
- ASSET_FORMAT.md
- TESTING_POLICY.md
- DEFINITION_OF_DONE.md
- GAMEPLAY_ABI.md
- ZIG_GAMEPLAY_GUIDE.md

禁止：
- 個別 Session 自行改變核心架構
- 同一問題出現兩套不同 Ownership 模型
- 部分模組使用 Exception、部分模組隨意使用 Error Code
- 部分模組使用 Raw Pointer Ownership、部分模組使用 Handle
- 未更新架構文件就改核心 Contract

重大架構變更必須：
1. 先更新 Architecture Decision Record（ADR）
2. 再修改 Interface
3. 再由 AI 實作 Backend
4. 最後通過 Regression Gate


## 四、Coding Convention / Static Analysis

本章為所有人工與 AI 產出程式碼的強制規範。
Coding Style 不只處理排版，也包含 Ownership、Error Handling、Threading、Resource Lifetime 與 Hot Path 設計原則。

### 4.1 Naming Convention

```text
Class / Struct / Enum      PascalCase
Function / Method          PascalCase
Local Variable             camelCase
Member Variable            mCamelCase
Static Variable            sCamelCase
Global Constant            kPascalCase
Enum Value                 PascalCase
Template Parameter         PascalCase
File Name                  PascalCase.h / PascalCase.cpp
Namespace                  lowercase
```

範例：

```cpp
class TextureManager
{
public:
    TextureHandle CreateTexture(const TextureDesc& desc);

private:
    GraphicsDevice* mDevice = nullptr;
    uint32_t mTextureCount = 0;
};
```

### 4.2 Namespace

使用巢狀 namespace 簡寫：

```cpp
namespace engine::render
{

class RenderGraph
{
};

}
```

禁止將 Engine Type 大量放在 global namespace。

### 4.3 Header / Source 分離

```text
TextureManager.h
TextureManager.cpp
```

Header 原則：
- 儘量使用 forward declaration
- 減少不必要 include
- 不在 header 放大型實作
- 公開 API 優先保持穩定

### 4.4 Smart Pointer 命名

引擎提供標準智能指標 alias：

```cpp
template<typename T>
using SharedPtr = std::shared_ptr<T>;
```

建立 helper 可使用：

```cpp
template<typename T, typename... Args>
SharedPtr<T> MakeSharedPtr(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}
```


規範：

- Engine 智能指標別名只使用 `SharedPtr<T>`
- `SharedPtr<T>` 對應 `std::shared_ptr<T>`
- `MakeSharedPtr<T>()` 只允許存在單一定義
- Non-owning reference 使用 `T*` 或 `T&`
- Engine Resource / GPU Resource 仍優先使用 Handle，不因 SharedPtr 而改變資源管理模型

### 4.5 Ownership 規則

```text
SharedPtr<T>
→ Shared Ownership
→ 只有真正需要 Shared Lifetime 的 C++ 物件才使用

T*
→ Nullable Non-owning Reference
→ 不負責釋放

T&
→ Non-null Non-owning Reference
→ 不負責釋放

Handle<T>
→ Engine-managed Resource Reference
→ 真正生命週期由 ResourceManager / 對應 Subsystem 管理

EntityID
→ Runtime Entity Identity
→ 不代表 Memory Ownership
```

核心原則：

- 不把 `SharedPtr<T>` 當成「單一所有權」工具
- 不把 Smart Pointer 類型本身視為架構 Ownership 的唯一表達方式
- Ownership 由 `Subsystem / Manager / Pool / Resource Registry` 的責任邊界決定
- `SharedPtr<T>` 僅用於確實需要跨物件 / 跨系統共享生命週期的 C++ 物件
- `T*` / `T&` 一律視為 non-owning reference
- GPU Resource / Mesh / Texture / Material 優先使用 Handle，不使用 SharedPtr 管理 GPU 資源生命週期
- Entity / Node Runtime Reference 優先使用 EntityID / NodeHandle，不使用 SharedPtr
- 一般 Runtime Hot Path 避免頻繁 copy SharedPtr，降低 reference-count 操作

### 4.6 Handle 優先

GPU / Asset / Runtime Resource 使用 generation-based Handle：

```cpp
template<typename Tag>
struct Handle
{
    uint32_t index = kInvalidIndex;
    uint32_t generation = 0;
};
```

例如：

```cpp
TextureHandle texture;
MeshHandle mesh;
MaterialHandle material;
```

禁止直接讓 Gameplay / Material 長期持有 native resource pointer。

### 4.7 Error Handling

Engine Core 不使用 Exception 作為一般控制流程。

預期可恢復錯誤：

```cpp
Result<TextureHandle> LoadTexture(const AssetID& asset);
```

使用：

```cpp
auto result = LoadTexture(asset);

if (!result)
{
    LOG_ERROR("Failed to load texture: {}", result.Error());
    return result.Error();
}
```

不可恢復錯誤：

```cpp
ENGINE_ASSERT(device != nullptr);
ENGINE_FATAL("Failed to create graphics device");
```

規則：
- Expected Failure → Result / ErrorCode
- Programming Error → Assert
- Fatal Initialization Failure → Fatal Path
- Platform Error → 轉換成 Engine Error
- 禁止不同模組任意混用 Exception / bool / magic integer error code

### 4.8 const / API 語意

不修改輸入時：

```cpp
void CreateTexture(const TextureDesc& desc);
```

只有真的修改時才收 non-const reference。

Getter 可依需求提供：

```cpp
const TransformData& GetTransform() const;
TransformData& GetTransform();
```

### 4.9 Data-Oriented Component Style

避免肥大的 inheritance hierarchy：

```text
Component
└─ RenderComponent
   └─ MeshComponent
      └─ AnimatedMeshComponent
```

優先：

```text
Data
+
System
```

例如：

```cpp
struct MeshRendererData
{
    MeshHandle mesh;
    MaterialHandle material;
    uint32_t flags = 0;
};

class MeshRendererSystem
{
public:
    void Update(const FrameContext& frame);
};
```

### 4.10 Hot Loop 規則

禁止在大量 Component 更新中使用 virtual dispatch + pointer chasing：

```cpp
for (Component* component : components)
{
    component->Update(dt);
}
```

優先直接跑 SoA / Chunk：

```cpp
auto positions = transformPool.Positions();
auto velocities = velocityPool.Values();

for (size_t i = 0; i < positions.size(); ++i)
{
    positions[i] += velocities[i] * dt;
}
```

Hot Loop 原則：
- 避免 SharedPtr copy
- 避免 Heap Allocation
- 避免 RTTI lookup
- 避免 Virtual Dispatch
- 避免 Hash Lookup（可預先 resolve 時）
- 優先 contiguous memory
- 優先 batch processing

### 4.11 Virtual Function 使用範圍

允許：
- RHI Backend Interface
- Editor Plugin Interface
- 少量高階 subsystem abstraction

避免：
- Transform hot loop
- Animation hot loop
- Visibility / Culling hot loop
- Render submission hot loop

### 4.12 Enum / Strong Type

Enum 一律優先 `enum class`：

```cpp
enum class TextureFormat : uint8_t
{
    RGBA8,
    BC7,
    ASTC
};
```

不同資源 ID 不使用同一個裸 `uint32_t`：

```cpp
TextureHandle texture;
MeshHandle mesh;
MaterialHandle material;
```

避免誤把 Mesh Index 傳入 Texture API。

### 4.13 Magic Number

禁止：

```cpp
if (distance > 500.0f)
```

優先：

```cpp
constexpr float kDefaultShadowDistance = 500.0f;
```

或使用 Settings / Config。

### 4.14 RAII

OS / File / Lock / Temporary Native Resource 優先 RAII。

```cpp
class File
{
public:
    explicit File(const char* path);
    ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;
};
```

禁止依賴呼叫端記得手動 Close / Release。

### 4.15 Move-only Resource

具有唯一 ownership 或昂貴複製成本的物件設為 non-copyable：

```cpp
class CommandList
{
public:
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;

    CommandList(CommandList&&) noexcept = default;
    CommandList& operator=(CommandList&&) noexcept = default;
};
```

### 4.16 Allocation 規則

Hot Path 禁止無限制 per-frame heap allocation。

優先：
- reserve
- Frame Allocator
- Linear / Arena Allocator
- Object Pool
- Chunk Storage
- Fixed-capacity container（適合時）

Render Submission 可使用 Frame Arena：

```cpp
FrameVector<RenderItem> renderItems(frameAllocator);
```

### Frame Allocator Threading

禁止多個 Worker Thread 無同步地共用單一 global Frame Arena。

預設策略：

```text
FrameAllocatorSet
├─ MainThread Arena
├─ RenderThread Arena
└─ Worker Arena[N]
```

每個 Worker 使用自己的 Thread-local / Worker-local Arena：

```cpp
FrameAllocator& allocator = FrameAllocators::CurrentWorker();
FrameVector<RenderItem> renderItems(allocator);
```

Job 若會產生大量暫存資料，也可配置 Job-local Arena / Scratch Allocator。

原則：

- Allocation Hot Path 不使用 global allocator lock
- Worker Arena 不跨 Worker 同時寫入
- Job 結束後可丟棄 Scratch 範圍
- Frame End 在所有使用該 Frame memory 的 Job / Render work 完成後統一 reset
- Reset 前必須有明確 frame fence / job barrier
- 不允許 Frame Arena 物件逃逸到下一 Frame
- 不允許把 Frame Arena pointer 存入 Persistent Resource / Scene Object

Frame lifecycle：

```text
Begin Frame
↓
Acquire Per-thread Arenas
↓
Parallel Jobs Allocate Locally
↓
Join / Barrier
↓
Render Submission Complete
↓
Frame Fence / Lifetime Complete
↓
Reset Arenas
```

如果需要跨 Worker 合併結果：

```text
Per-worker Output
↓
Prefix / Merge / Compaction
↓
Final Contiguous Buffer
```

避免把所有 Worker allocation 集中到單一鎖定 allocator。

### 4.17 Logging

統一：

```cpp
LOG_TRACE(...)
LOG_INFO(...)
LOG_WARNING(...)
LOG_ERROR(...)
LOG_FATAL(...)
```

禁止散落：

```cpp
printf("error");
```

重要 GPU / Asset error 必須包含 context：

```cpp
LOG_ERROR(
    "Failed to create pipeline. Shader={}, Variant={}",
    shaderName,
    variantKey);
```

### 4.18 Assert

Debug Assertion：

```cpp
ENGINE_ASSERT(index < mEntities.size());
```

帶訊息：

```cpp
ENGINE_ASSERT_MSG(
    state == ResourceState::ShaderResource,
    "Texture must be ShaderResource before sampling");
```

### 4.19 RHI 邊界規則

Renderer 高層禁止出現：

```text
VkImage
VkBuffer
VkDescriptorSet
ID3D12Resource
D3D12_GPU_DESCRIPTOR_HANDLE
MTLTexture
MTLBuffer
```

高層只能使用：

```text
TextureHandle
BufferHandle
GraphicsPipelineHandle
CommandBuffer
ResourceState
ResourceIndex
```

Backend-specific type 只允許出現在：

```text
RHI/D3D12/
RHI/Vulkan/
RHI/Metal/
```

### 4.20 Resource State

禁止高層直接寫：

```cpp
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
```

必須使用：

```cpp
ResourceState::ShaderResource
```

由各 Backend 映射。

### 4.21 Threading Convention

每個核心 API 必須明確標註：
- Thread-safe
- Main-thread-only
- Render-thread-only
- Worker-thread-safe

例如：

```cpp
// Main thread only.
void Scene::DestroyNode(NodeHandle node);

// Thread-safe.
AssetRequest AssetManager::LoadAsync(const AssetID& asset);

// Render thread only.
void Renderer::Submit(const RenderWorld& world);
```

後續可導入 annotation：

```cpp
ENGINE_MAIN_THREAD
void DestroyNode(NodeHandle node);
```

### 4.22 Comment Style

Comment 解釋「為什麼」，不是重述程式碼。

不建議：

```cpp
// Increase i.
++i;
```

建議：

```cpp
// Increment the generation so stale handles fail validation
// when this slot is reused.
++mGenerations[index];
```

### 4.23 TODO 規則

禁止：

```cpp
// TODO fix later
```

統一：

```cpp
// TODO(Renderer): Replace CPU culling with GPU Hi-Z path.
// Tracking: ENG-142
```

方便 AI Agent、CI 與 Issue Tracker 搜尋。

### 4.24 Formatting

由 `.clang-format` 統一控制：
- Indentation
- Braces
- Line wrapping
- Include ordering
- Pointer / Reference spacing

人工與 AI 不得自行使用另一套格式。

### 4.25 Static Analysis / CI Gate

每個 PR 至少執行：

```text
clang-format --check
↓
clang-tidy
↓
Compiler Warning Check
↓
Architecture / Include Rule Check
↓
Unit Test
↓
Sanitizer（適用平台 / 模組）
```

建議工具：
- clang-format
- clang-tidy
- AddressSanitizer
- UndefinedBehaviorSanitizer
- ThreadSanitizer（適合的 subsystem）
- Vulkan Validation Layer
- DX12 Debug Layer / DRED

CI 也應檢查禁止模式，例如：
- Renderer 高層 include Vulkan / DX12 / Metal native header
- Gameplay 使用 native GPU resource
- Hot Loop 新增 SharedPtr
- Core Runtime 出現不允許的 exception flow
- Resource Owner 不明確

### 4.26 AI Coding Rule

AI Agent 在生成或修改程式碼前必須讀取：

```text
ARCHITECTURE.md
CODING_CONVENTION.md
RHI_CONTRACT.md
MEMORY_MODEL.md
THREADING_MODEL.md
ERROR_HANDLING.md
```

AI 不得自行：
- 發明新的 Smart Pointer alias
- 改變 SharedPtr 定義
- 改變 Error Handling Model
- 將 Handle 改成 SharedPtr
- 在 RHI 高層洩漏 native graphics type
- 為單一模組建立不同的命名規則

若確實需要改 Convention：
1. 建立 ADR
2. 更新 Coding Convention
3. 更新 clang-tidy / CI Rule（若適用）
4. 再修改 Production Code

### 4.27 Coding Style 核心摘要

```text
Ownership
→ SharedPtr 統一使用
→ SharedPtr 嚴格限制
→ Handle 管理 Engine Resource

Runtime
→ Node API + EntityID
→ Component Pool / SoA
→ Hot Loop 不反覆 lookup

Renderer
→ ResourceHandle / ResourceIndex
→ Backend Native Type 不外漏

Error
→ Result / ErrorCode
→ Assert / Fatal 處理程式錯誤

Memory
→ RAII
→ Frame / Arena / Pool
→ 避免 per-frame heap allocation

AI Development
→ clang-format
→ clang-tidy
→ Architecture Gate
→ Unit / Integration / Platform Test
```



### Smart Pointer Policy

智能指標統一沿用 4.4 定義的 `SharedPtr<T>`；此處不重複宣告 alias。

使用：

```cpp
SharedPtr<GraphicsDevice> graphicsDevice;
SharedPtr<EditorDocument> document;
SharedPtr<AsyncRequest> request;
```

建立：

```cpp
auto device = std::make_shared<GraphicsDevice>();
```

規則：
- 智能指標只使用 `SharedPtr<T>`
- 不另外定義 `SharedPtr<T>`
- non-owning reference 使用 `T*` 或 `T&`
- Engine Resource 仍優先使用 `Handle<T>`
- Entity 使用 `EntityID`
- RHI / GPU Resource 不因 SharedPtr 而繞過 Resource Manager / Handle System
- Hot Loop 不反覆複製 SharedPtr，避免不必要的 reference count 操作



### Precompiled Header (PCH) Policy

PCH 為 V0.1 Build System 的正式標準功能，用於降低大型 C++ 工程的重複 Header Parse 成本。

建議結構：

```text
Engine/
├─ EnginePCH.h
├─ Core/
├─ Scene/
├─ Asset/
└─ ...

Renderer/
├─ RendererPCH.h
├─ RHI/
└─ ...

Editor/
├─ EditorPCH.h
└─ ...
```

基本規則：

```text
EnginePCH.h
→ STL
→ 穩定、低變動的 Engine 基礎型別
→ 常用平台無關 Header

RendererPCH.h
→ Renderer 常用穩定 Header
→ Math / Handle / Resource Description
→ 不直接混入所有 Backend Native Header

EditorPCH.h
→ Dear ImGui
→ Editor 常用 STL
→ Editor Framework Common Header
```

可放入 PCH：
- `<cstdint>`
- `<cstdlib>`
- `<memory>`
- `<string>`
- `<string_view>`
- `<vector>`
- `<array>`
- `<span>`
- `<unordered_map>`
- `<functional>`
- `<algorithm>`
- 其他高頻、低變動標準 Header
- 穩定且廣泛使用的共用 Engine Header

不建議放入共用 PCH：
- 經常修改的 Gameplay Header
- 經常修改的 Component Header
- 大型且只有少數模組使用的 Third-party Header
- Vulkan Native Header
- Direct3D 12 Native Header
- Metal Backend-specific Header
- 單一平台專用 Header

Backend Native Header 應限制在對應 Backend：

```text
RHI/D3D12/
→ d3d12.h
→ dxgi*.h

RHI/Vulkan/
→ vulkan.h

RHI/Metal/
→ Metal / Objective-C++ Headers
```

目的：
- 避免修改 Backend Header 時造成整個 Engine PCH 失效
- 避免 Platform Native Type 滲透到共用模組
- 控制 rebuild 範圍

CMake：

```cmake
target_precompile_headers(EngineRuntime PRIVATE
    EnginePCH.h
)

target_precompile_headers(Renderer PRIVATE
    RendererPCH.h
)

target_precompile_headers(Editor PRIVATE
    EditorPCH.h
)
```

PCH 原則：
- PCH 是編譯效能工具，不是解決依賴設計問題的手段
- 不因為有 PCH 就允許 Header 任意互相 include
- 仍優先使用 Forward Declaration
- 保持 Module Boundary
- 減少 Public Header Dependency
- PCH 內容變更頻率必須低

CI：
- Debug / Release 都驗證 PCH Build
- 提供可選 `ENGINE_USE_PCH=OFF` Build
- 定期執行 Non-PCH Build，避免原始碼暗中依賴 PCH 提供的 Header
- Non-PCH Build 必須能成功，以確認每個 Translation Unit 的 include 完整性

後續可評估：
- Unity / Jumbo Build
- C++20 Modules

但不在 V0.1 強制導入，避免與 PCH / Incremental Build 同時增加 Build System 複雜度。

## 五、Engine Core

目錄概念：

```text
Engine/
├─ Core/
│  ├─ Application
│  ├─ Memory
│  ├─ Threading
│  ├─ JobSystem
│  ├─ FileSystem
│  ├─ Logging
│  ├─ UUID
│  ├─ Reflection
│  ├─ Serialization
│  ├─ Time
│  ├─ Testing
│  └─ Crash
├─ Scene/
├─ Renderer/
├─ RHI/
├─ Asset/
├─ Animation/
├─ Physics/
├─ Audio/
├─ UI/
├─ Localization/
├─ Save/
├─ Platform/
└─ Editor/
```

核心要求：
- Runtime 不依賴 Editor
- Debug / Release / Shipping
- Handle-based resource management
- 非同步資產載入
- Thread-safe Job System
- UUID Asset Reference
- Reflection 支援 Inspector 與 Serialization
- Crash Log / Dump 基礎設施
- Memory Budget 追蹤


## 六、Scene / World Lifecycle / Node 架構

本引擎正式區分：

```text
Engine
↓
World
↓
Scene
↓
Entity / Node
↓
Component
```

核心 Contract：

```text
World
≠ Scene
```

```text
World
→ Runtime Simulation Context

Scene
→ Loadable / Serializable Content Ownership Unit
```

Scene 不直接等同 Physics World、Navigation World、Render World 或 Streaming Cell。

### World

`World` 是可獨立模擬的一個 Runtime Universe。

```text
World
├─ Scene Registry
├─ Entity Registry
├─ System Scheduler
├─ PhysicsWorld
├─ NavigationWorld
├─ Render Extraction Context
├─ Audio Context
├─ Event Queue
└─ Time / Simulation State
```

World Type：

```text
WorldType
├─ Editor
├─ Play
├─ Preview
├─ Bake
└─ Server      ← Future
```

正式規則：

```text
EditorWorld
≠
PlayWorld
```

Play Mode：

```text
EditorWorld
↓
Clone / Build Runtime World
↓
PlayWorld
↓
Simulation
```

Stop：

```text
PlayWorld Destroy
↓
EditorWorld 保留原始編輯狀態
```

避免 Runtime 產生的 Entity、Physics State、Animation State、Gameplay State 污染 Editor Authoring World。

### World Lifecycle

```text
WorldState

Creating
↓
Ready
↓
Running
↕
Paused
↓
Stopping
↓
Destroying
↓
Destroyed
```

World Destroy 必須有 deterministic shutdown：

```text
Stop accepting new gameplay jobs
↓
Drain / Cancel outstanding work
↓
Deactivate scenes
↓
Destroy scenes
↓
Destroy Physics / Navigation / Runtime subsystem state
↓
Destroy World
```

`Pause` 與 `Scene Deactivate` 不同：

```text
World Pause
→ Simulation Time stops / changes policy

Scene Deactivate
→ Scene content leaves active simulation participation
```

### Multiple Scene / Additive Scene

同一個 World 可同時載入與啟用多個 Scene。

```text
World
├─ Persistent.scene
├─ Town.scene
├─ Forest.scene
├─ Mountain.scene
└─ Dungeon.scene
```

狀態可同時為：

```text
Persistent.scene  → Active
Town.scene        → Active
Forest.scene      → Active
Mountain.scene    → LoadedInactive / Prefetched
Dungeon.scene     → Unloaded
```

正式 Contract：

```text
World
→ May contain N loaded Scenes

N Scenes
→ May be Active simultaneously
```

支援：

```text
SceneLoadMode
├─ Single
└─ Additive
```

`Single` 不作為獨立 Runtime 架構；可視為：

```text
Load New Scene
↓
Activate
↓
Unload previous non-persistent scenes
```

底層建立在 Additive Scene 能力上。

### Persistent Scene

不採用把任意 Object 偷偷搬移的 `DontDestroyOnLoad` 類型隱式規則。

正式：

```text
World
├─ PersistentScene
└─ Gameplay Scenes
```

例如：

```text
PersistentScene
├─ Player
├─ Camera
├─ GlobalAudio
├─ GlobalUI
└─ Game-level persistent entities
```

跨區域時：

```text
Gameplay Scene A
→ unload

PersistentScene
→ remains

Gameplay Scene B
→ load
```

若 Entity 需要改變 ownership，使用明確：

```text
MoveEntityToScene()
```

而不是特殊 lifetime flag。

### Scene State Machine

```text
SceneState

Unloaded
↓
Loading
↓
LoadedInactive
↓
Activating
↓
Active
↓
Deactivating
↓
LoadedInactive
↓
Unloading
↓
Unloaded
```

錯誤：

```text
Loading / Activating
↓
Failed
```

`LoadedInactive` 表示：

- Scene Asset 已讀取。
- Entity / Component Runtime Data 已建立。
- Reference 已 resolve 到目前可 resolve 的範圍。
- 必要 Physics / Navigation / Render registration 準備完成。
- Gameplay 尚未開始更新。

### Async Scene Load

Scene Load 正式走非同步 pipeline：

```text
Scene Asset
↓
Async IO
↓
Deserialize / Runtime Blob Read
↓
Resolve Asset Dependencies
↓
Create Entity Storage
↓
Create Components
↓
Resolve References
↓
Prepare Physics / Navigation / Render / Audio
↓
LoadedInactive
↓
Activation Barrier
↓
Active
```

不得：

```text
LoadScene()
↓
Main Thread 同步阻塞數秒
↓
半完成 Scene 直接暴露給 Gameplay
```

### Atomic Scene Activation

Scene 對 Simulation 的可見性必須原子化。

禁止：

```text
Camera 已出現
Collider 尚未建立
AI 已更新
Navigation 尚未 Ready
```

正式：

```text
Build
↓
Validate
↓
Ready
↓
Safe Frame / Simulation Barrier
↓
Atomic Activate
```

Activation 後 Gameplay / Physics / Animation / AI / Audio / Render Extraction 才能正式看到該 Scene。

### Safe Scene Unload

Scene Unload：

```text
Active
↓
Deactivating
↓
Stop Gameplay Participation
↓
Unregister Physics / Navigation / Render / Audio
↓
Cancel / Drain Scene-owned Jobs
↓
Release Asset References
↓
Destroy Entity Runtime Data
↓
Unloaded
```

Scene 不可在仍有 Job / Render Frame / Physics Step / Async callback 持有其 transient data 時直接 free。

### Scene Transition Transaction

Scene Transition 不硬編碼 Fade 或 Loading UI。

Scene Core 提供：

```text
Prepare
Activate
Deactivate
Commit
Rollback
```

例如 A → B：

```text
Scene A Active
↓
Begin Transition
↓
Async Load B
↓
Build B Inactive
↓
Validate B
↓
Presentation Fade / Transition
↓
Activation Barrier
↓
Activate B
↓
Switch Camera / Input policy
↓
Deactivate A
↓
Presentation Fade In
↓
Unload A when safe
```

若 B 失敗：

```text
Load / Validation Failed
↓
Rollback
↓
Keep Scene A
```

Presentation transition 由 Gameplay / Runtime UI 控制，可使用 Fade、Loading Screen、Cloud Transition 等。

### Scene Load Progress

提供真實 stage progress，而非單一假百分比：

```text
SceneLoadProgress
├─ IO
├─ Dependency
├─ Deserialize
├─ Instantiate
├─ Prepare
└─ Finalize
```

Loading UI 可聚合顯示。

### Scene / Entity Active State

正式區分：

```text
SceneActive
EntityActiveSelf
EntityActiveInHierarchy
ComponentEnabled
```

Effective active：

```text
EffectiveActive
=
SceneActive
&& EntityActiveInHierarchy
&& ComponentEnabled
```

Parent Disable 不應以深度 recursive callback 直接同步處理大量 Entity。

建議：

```text
Hierarchy Dirty
↓
Activation Propagation Job
↓
Effective State Change List
↓
System Lifecycle Dispatch
```

### Component Lifecycle

可提供：

```text
OnCreate
OnEnable
OnDisable
OnDestroy
```

但不因此引入：

```text
virtual Update() per component per frame
```

正式：

```text
Lifecycle Hook
✓

Per-component virtual Update
✕
```

System / Component Registry 負責批次 lifecycle dispatch。

### Structural Change / WorldCommandBuffer

Simulation Job 正在讀寫 SoA 時，不允許任意直接改變 storage 結構。

正式：

```text
WorldCommandBuffer
├─ CreateEntity
├─ DestroyEntity
├─ AddComponent
├─ RemoveComponent
├─ Reparent
├─ SetActive
└─ MoveEntityToScene
```

流程：

```text
Gameplay / Jobs
↓
WorldCommandBuffer
↓
Structural Barrier
↓
Apply
```

### Scene Root

Scene 擁有 root entity list，不建立假的 SceneRootEntity。

```text
Scene
├─ Root Entity A
├─ Root Entity B
└─ Root Entity C
```

```text
Parent = InvalidEntityID
→ Root Entity
```

### Hierarchy Ownership Rule

Entity hierarchy 不允許跨 Scene ownership：

```text
Scene A Entity
↓ parent
Scene B Entity
```

正式禁止。

```text
Hierarchy Parent
→ Must belong to same Scene
```

跨 Scene 邏輯關係使用 Reference / Attachment System，不用 hierarchy ownership。

### Move Entity Between Scenes

提供：

```text
MoveEntityToScene()
```

這是 ownership transfer，不是普通 Reparent。

必須處理：

- Children policy。
- Scene-local serialization ownership。
- Runtime registration。
- Persistent identity。
- Cross-scene references。
- Streaming cell reassignment。
- Editor Undo / Redo。

V1 可限制 Runtime 使用情境；Editor 必須可用。

### Persistent Identity / Runtime Identity

持久化：

```text
UUID128
```

Runtime：

```text
EntityID
= Index + Generation
```

Scene unload 後，舊 EntityID 失效。

同一個持久化 Object 重新載入：

```text
Same UUID
→ may resolve to a different EntityID
```

正式：

```text
Persistent UUID
≠ Runtime EntityID
```

### Cross-Scene Reference

Serialized cross-scene reference 不保存 raw EntityID。

正式：

```text
SceneObjectRef
├─ SceneAssetUUID / SceneInstance identity
└─ ObjectUUID
```

Runtime：

```text
SceneObjectRef
↓
SceneReferenceResolver
↓
EntityID / SceneObjectHandle
```

Target Scene 未載入時，reference 可保持 unresolved。

Scene unload：

```text
Resolved EntityID
→ invalidated
```

Logical reference 仍存在。

重新 load：

```text
UUID
↓
Resolve new EntityID
```

Cross-scene logical reference 不自動等於硬 load dependency。

```text
Build / Load Dependency
≠
Logical Object Reference
```

避免 reference graph 意外把整個世界全部拉入 residency。

### Scene Reference Policy

可支援：

```text
SceneReferencePolicy
├─ Required
├─ Optional
├─ Weak
└─ LoadOnDemand      ← Restricted / Future-sensitive
```

`Required` 可阻止 Scene activation。

`Optional / Weak` 在 target 未載入時允許 unresolved。

`LoadOnDemand` 不作為任意 cross-reference 的預設行為，避免隱性 streaming dependency。

### Scene Serialization

Authoring Scene Asset：

```text
Scene Asset
├─ Scene Metadata
├─ Entity UUID
├─ Hierarchy
├─ Component Types
├─ Component Serialized Data
└─ Asset / Object References
```

Runtime：

```text
Scene Authoring Data
↓
Validate
↓
Cook
↓
Runtime Scene Blob
↓
Fast Instantiate
```

Shipping Runtime 優先讀 Cook 後格式，不依賴重型 Editor reflection JSON path。

### Scene Schema Version / Migration

正式：

```text
SceneSchemaVersion
```

流程：

```text
Old Scene
↓
Deserializer
↓
Migration Chain
↓
Current Schema
```

Editor Save 後更新至目前 schema。

Shipping Runtime 優先只接受 Cook 完成的 current runtime format。

Missing plugin / missing component 在 Editor 不直接丟失資料，可保留：

```text
MissingComponentProxy
├─ Original TypeID
└─ Serialized Payload
```

Plugin 恢復後可重新 resolve。

---

### Seamless World / Multi-Scene Streaming

大型無接縫世界在同一個 `World` 內以多 Scene + Streaming Cell 運作。

```text
World
├─ Persistent Scene
├─ Town Scene
├─ Forest Scene
└─ Mountain Scene
```

玩家移動：

```text
Town Active
+
Forest Prefetch
↓
Forest LoadedInactive
↓
Forest Activate
↓
Town + Forest overlap Active
↓
Player continues
↓
Town retires / unloads when safe
```

正式：

```text
Scene Boundary
≠
Rendering Boundary
≠
Physics Boundary
≠
Navigation Boundary
≠
Gameplay Zone Boundary
```

World-level subsystem：

```text
World
├─ PhysicsWorld
├─ NavigationWorld
├─ RenderWorld
├─ Audio Context
├─ Entity Registry
└─ System Scheduler
```

Scene load / unload 只向這些 World subsystem Register / Unregister content。

不要為每個 Additive Scene 建立獨立 PhysicsWorld / NavigationWorld。

### Logical Scene != Streaming Cell

這是 Large World 的核心 Contract：

```text
Scene
→ Authoring / Ownership / Serialization Unit

StreamingCell
→ Runtime Residency Unit
```

同一個 Scene 可 Cook 成大量 Streaming Cells。

```text
KyotoTown.scene
├─ Cell 0,0
├─ Cell 0,1
├─ Cell 0,2
└─ ...
```

Editor 可維持單一 logical Scene；Runtime physical partition 不應強迫 Editor hierarchy 被拆成大量 Scene Asset。

### Streaming Cell

正式：

```text
StreamingCell
= Runtime Residency Unit
```

Cell 不一定是固定方格。

可來自：

```text
StreamingCellType
├─ SpatialGrid
├─ Volume
├─ Room
└─ Explicit
```

Runtime common metadata：

```text
CellID
Bounds
Dependencies
Priority
ResidencyState
ContentReferences
Neighbors
```

### Streaming Cell State

Scene State 與 Cell Residency State 分離。

```text
StreamingCellState

Unloaded
↓
Requested
↓
IOResident
↓
BuiltInactive
↓
Active
↓
Retiring
↓
Unloaded
```

可再區分 compound residency：

```text
CellResidency
├─ Metadata
├─ CPU Content
├─ Physics
├─ Navigation
├─ GPU Resources
└─ Simulation
```

正式：

```text
Simulation Residency
≠
Visual Residency
```

### World Partition Policy

World Partition 不綁死單一 Grid。

```text
WorldPartitionPolicy
├─ Grid
├─ Loose Quadtree
├─ Volume
├─ Room / Portal
├─ Explicit Region
└─ Custom
```

推薦：

```text
Outdoor
→ Fixed Grid Cells + Loose Quadtree Spatial Index

Indoor
→ Room / Portal Graph

Special Area
→ Explicit / Volume Cell

Outdoor ↔ Indoor
→ Streaming Gateway
```

同一個 World 可 Hybrid 使用。

### Fixed Grid Cell

Outdoor V1 使用 stable fixed grid 作為主要 residency/cook unit：

```text
CellX = floor(WorldX / CellSize)
CellZ = floor(WorldZ / CellSize)
```

優點：

- Deterministic。
- Direct coordinate lookup。
- Cache-friendly。
- 容易做 Bundle patch / streaming debug。
- Runtime 不需要遍歷全世界 Cell。

### Loose Quadtree

V1 正式支援 Loose Quadtree 作為 hierarchical spatial acceleration。

定位：

```text
Loose Quadtree
= Spatial Acceleration
+ Streaming Candidate Index
+ HLOD Hierarchy Foundation
```

不是：

```text
Quadtree Node
= Streaming Cell
```

正式：

```text
StreamingCell
→ Stable Residency Unit

QuadtreeNode
→ Runtime / Cook Spatial Index
```

Quadtree 用途：

- Radius / Box / Frustum cell query。
- Streaming Source candidate query。
- Camera visibility candidate query。
- Large object spatial registration。
- HLOD hierarchy。
- Editor region query。

大型 Object 跨格時使用 Loose Bounds 降低重複註冊。

### Adaptive Quadtree

V1 不要求 adaptive quadtree 直接決定可變大小 Cell identity。

```text
Quadtree Spatial Index
→ V1

Adaptive Quadtree Cell Generation
→ Future / V2
```

避免 V1 同時承擔：

- Variable cell patch identity。
- HLOD stitching。
- Terrain / Nav / Physics ownership complexity。
- Neighbor resolution。
- Deterministic cell ID churn。

### Room / Portal Graph

Indoor / Dungeon 正式支援 Room / Portal。

```text
Room A
   │
 Portal
   │
Room B
   │
 Portal
   │
Room C
```

Room：

```text
Room
├─ Bounds / Volume
├─ RoomID
├─ Portals[]
└─ Environment / Metadata
```

Portal：

```text
Portal
├─ RoomA
├─ RoomB
├─ Plane
├─ Convex Polygon
├─ State
└─ Traversal Flags
```

正式：

```text
Room
= Spatial / Connectivity Concept

StreamingCell
= Residency Concept
```

可能：

```text
1 Room → 1 Cell
N Small Rooms → 1 Cell
1 Huge Room → N Cells
```

### Portal Role

Portal 是 connectivity edge，不是 Gameplay Door。

```text
Portal
≠ Gameplay Door
```

Gameplay Door 可控制 Portal state：

```text
Door Open
↓
Portal SetOpen(true)
```

但 Portal 也可代表：

- Door opening。
- Corridor opening。
- Cave opening。
- Stair opening。
- Elevator exit。
- Window。
- Area transition opening。

Portal 可提供 subsystem-specific traversal information：

```text
Portal Capability
├─ VisibleThrough
├─ StreamThrough
├─ AudioThrough
├─ NavThrough
└─ AIThrough
```

不同 subsystem 不必使用完全相同規則。

### Portal Visibility

可建立 Portal Culling foundation：

```text
Camera
↓
Find Current Room
↓
Traverse Visible Portals
↓
Clip Frustum against Portal Polygon
↓
Visible Room Set
↓
Render Extraction
```

Full aggressive Portal Frustum optimization 可後續 profile-driven 強化，但 Room/Portal connectivity foundation 列入 V1。

### Portal Streaming

Indoor Streaming 不只使用直線距離。

```text
Current Room
↓
Portal Graph Distance
↓
Streaming Demand
```

例如：

```text
Graph Distance 0
→ Active

Distance 1
→ Active / High Priority

Distance 2
→ Prefetch

Farther
→ Unloaded / Low
```

避免牆後幾公尺但實際路徑很遠的房間因純 radius query 全部載入。

Portal state 可影響 streaming：

```text
Closed / Locked
→ lower / block traversal demand according to policy

Opening
→ prefetch room behind portal
```

### Outdoor ↔ Indoor Streaming Gateway

支援：

```text
Outdoor Grid / Quadtree
↓
Streaming Gateway
↓
Indoor Room / Portal Graph
```

靠近入口：

```text
Gateway Proximity
↓
Prefetch Interior Entry Cell
```

進入室內後：

```text
Portal Graph
→ controls deeper interior residency
```

Outdoor preload 可維持 hysteresis，離開時無需 loading screen。

### Streaming Source

正式：

```text
StreamingSource
```

來源可包含：

```text
Player
Camera
Party Member
Vehicle
Teleport Destination
Cutscene Camera
Gameplay Request
Editor Pin
```

每個 source 可有：

```text
Position
Radius / Shape
Priority
Velocity
LookDirection
PrefetchDistance
DesiredResidency
```

高速移動可使用 forward-biased / swept volume prefetch。

### Streaming Demand

Partition / Portal 不直接呼叫 Bundle load。

統一產生：

```text
StreamingDemand
├─ Source
├─ CellID
├─ Priority
├─ Reason
└─ DesiredState
```

Streaming Manager：

```text
Multiple Sources
↓
Merge Demands
↓
Priority / Budget
↓
Actual Residency
```

Reason 可用於 profiler：

```text
Why is this cell resident?
```

### Streaming Priority / Hysteresis

至少支援：

```text
StreamingPriority
├─ Critical
├─ High
├─ Normal
├─ Low
└─ Speculative
```

避免邊界 thrashing：

```text
Load / Activate Distance
<
Unload Distance
```

以及：

```text
MinimumResidentTime
```

尚未完成的 speculative request 應可 cancel。

### Streaming Cell != Bundle

正式：

```text
StreamingCell
→ Runtime Residency Unit

Bundle
→ Physical Packaging / Download / Versioning Unit
```

不強制 1:1。

一個 Cell 可引用多個 Bundle；一個 Bundle 也可包含多個相關 Cell content。

Cooker 可基於 locality 優化，但語意不綁死。

### Streaming Group / KeepTogether

Auto partition 必須允許 author override。

例如：

```text
HLODGroup / StreamingGroup
BossArena
Castle
Large Landmark
```

可標記：

```text
KeepTogether = true
```

避免大型 object 或 gameplay-critical cluster 被 auto partition 切成不完整 residency。

---

### HLOD

HLOD 正式列入 Large World / World Partition Framework。

```text
LOD
→ Single Object

HLOD
→ Multiple Objects / Spatial Region
```

HLOD 不等於 Mesh LOD。

例如：

```text
House A
House B
Trees
Fence
Props
↓
HLOD Builder
↓
Village_Block_01_Proxy
```

### HLOD Generation

HLOD 由 Editor / Cooker 離線產生。

```text
Authoring Scene
↓
World Partition / Spatial Hierarchy
↓
HLOD Builder
├─ Spatial Clustering
├─ Mesh Merge
├─ Geometry Simplification
├─ Material Merge
├─ Texture Atlas / Bake
├─ Impostor Build
└─ Custom Proxy
↓
HLOD Assets
↓
Asset / Bundle System
```

Runtime 不在一般遊戲流程做重型 mesh simplify / texture bake。

正式：

```text
HLOD Generation
→ Editor / Cooker Offline

HLOD Runtime
→ Selection + Streaming only
```

### HLOD Hierarchy

Loose Quadtree 可作 HLOD hierarchy foundation。

```text
Root
└─ Far HLOD
   ├─ Region HLOD
   │  ├─ Leaf HLOD
   │  └─ Leaf HLOD
   └─ Region HLOD
      └─ ...
```

Runtime 依：

- Projected screen size。
- Distance。
- Quality profile。
- Performance policy。
- Memory residency。

選擇合適層級。

### HLOD / Cell Residency

HLOD Node 與 Streaming Cell 分離：

```text
StreamingCell
→ Full-detail runtime content

HLODNode
→ Far visual representation
```

例如：

```text
Cell 1 ┐
Cell 2 ├─ HLOD Node A
Cell 3 ┤
Cell 4 ┘
```

遠距：

```text
Simulation      ✕
Physics         ✕
Navigation      ✕
Full Geometry   ✕
HLOD Proxy      ✓
```

靠近：

```text
Ensure Full Cells Ready
↓
Crossfade / Switch
↓
Hide HLOD Proxy
```

離開：

```text
Ensure HLOD Ready
↓
Switch
↓
Retire Full Cells
```

正式：

```text
Full Simulation Residency
≠
Far Visual Residency
```

### HLOD Participation

物件可設定：

```text
HLODParticipation
├─ Auto
├─ Include
├─ Exclude
└─ CustomProxy
```

通常：

```text
Static Building
→ Auto

Large Landmark
→ CustomProxy / Group Override

Player / NPC / Dynamic Vehicle / Animated Boss
→ Exclude
```

Vegetation 可使用專用：

```text
Near
→ Full Mesh / GPU Instancing

Mid
→ Lower LOD Instancing

Far
→ Cluster Impostor / HLOD
```

共享 hierarchy，但 proxy generation strategy 可不同。

### HLOD Determinism / Incremental Build

同樣輸入：

```text
Source Assets
+
Partition Settings
+
HLOD Settings
+
HLODBuilderVersion
```

必須產生 deterministic output / build hash。

Incremental rebuild：

```text
Changed Object
↓
Affected Cell / HLOD Leaf
↓
Affected Parent HLOD Chain
↓
Rebuild only impacted nodes
```

避免局部物件修改就重建整個 World HLOD。

### HLOD Editor

Editor 至少提供：

```text
HLOD Preview
HLOD Level Visualization
HLOD Group Override
Custom Proxy Assignment
Rebuild Selected Region
Show Source ↔ Proxy Relation
Memory / Triangle Estimate
```

Partition View 應可顯示：

```text
Loaded
Prefetched
Active
Pinned
HLOD-only
Memory
```

### World Partition / Portal Editor Tools

至少：

```text
Partition View
Portal Graph View
Streaming Source Preview
Cell Residency Debug
Load / Unload Region
Pin Region
Rebuild Cell
Validate Seam
Preview Streaming
```

Portal Graph View 顯示：

```text
Room A
  │
Portal
  │
Room B
```

並可觀察：

```text
Open / Closed
Visibility
Streaming
Audio
Navigation / AI flags
```

### Partition / Seam Validation

Editor / CI 應檢查：

Portal：

```text
Valid convex portal polygon
Exactly / correctly connected rooms
Portal plane / room boundary relation
Disconnected graph
Invalid overlap
Unreachable region
```

Cell seam：

```text
Terrain edge mismatch
Nav seam missing
Physics gap
HLOD seam
Probe / lighting discontinuity
Vegetation / biome gap
```

統一：

```text
WorldPartitionValidator
```

### Large World Coordinate Foundation

大型無接縫世界需要避免單精度長距離誤差。

正式要求：

```text
Simulation / World Position
→ High Precision Representation

Renderer
→ Camera-relative float
```

可採：

```text
WorldPosition
= High-precision region/cell coordinate
+ Local float position
```

或 CPU simulation double + renderer camera-relative float。

最終具體 representation 在 Transform Framework 細談時定案。

但 API 必須預留：

```text
WorldPosition
↔ LocalWorldPosition
↔ RenderRelativePosition
```

Gameplay 不應自行 scattered cast double → float。

### Scene Streaming / Runtime State Boundary

Scene Asset 是 content template，不是 Save State。

正式：

```text
Scene Asset
+
Persistent World State
↓
Runtime Scene
```

例如寶箱已開、Boss 已死亡、橋已毀壞等狀態不能因 Cell unload / reload 自動恢復成 Scene Asset 預設值。

詳細 Persistent World State / Save ownership 在 Save Framework 章節細談。

### Scene / World Profiler

至少顯示：

```text
Loaded Worlds
Loaded Scenes
Active Scenes
Entities / Scene
Components / Scene
Scene Memory
Cell Residency
Streaming Sources
Streaming Demand
Cell Churn
Prefetch Hit / Miss
Emergency Stall
Asset Residency
Load / Activation / Unload Time
Cross-scene unresolved refs
HLOD Residency / Switch
Retired Scene Generations
```

必須可回答：

```text
Why is this Scene / Cell / HLOD resident?
```

### Scene / World V1 Scope

V1：

```text
✓ World / WorldType
✓ EditorWorld / PlayWorld separation
✓ Multiple loaded / active Scenes
✓ Additive Scene
✓ Persistent Scene
✓ Async Scene Load
✓ LoadedInactive
✓ Atomic Activation
✓ Safe Unload
✓ Scene Transition Transaction / Rollback
✓ Scene Load Progress
✓ Scene / Entity / Component Active State
✓ Component Lifecycle
✓ WorldCommandBuffer / Structural Barrier
✓ Scene UUID / Entity UUID / Runtime EntityID separation
✓ Cross-scene logical reference
✓ Same-scene hierarchy rule
✓ MoveEntityToScene
✓ Scene Serialization / Schema Migration

✓ StreamingCell
✓ Fixed Grid Cells
✓ Loose Quadtree Spatial Index
✓ Explicit / Volume Cells
✓ Room / Portal Graph
✓ Outdoor ↔ Indoor Gateway
✓ Multiple Streaming Sources
✓ Streaming Demand / Priority / Hysteresis
✓ Cell / Bundle separation
✓ Streaming Group / KeepTogether

✓ Offline HLOD Builder
✓ HLOD Hierarchy Foundation
✓ HLOD Streaming / Selection
✓ HLOD Participation / Custom Proxy
✓ Deterministic / Incremental HLOD Build
✓ Editor Partition / Portal / HLOD Debug
✓ Partition / Seam Validation

✓ Large-world coordinate foundation
```

Future / profile-driven：

```text
△ Adaptive Quadtree Cell Generation
△ Octree / 3D adaptive partition
△ Automatic Room Detection
△ Full aggressive Portal Frustum Culling
△ Runtime Scene Structural Diff / Patch
△ World Origin Rebasing
△ Distributed MMO World Partition
△ Seamless Network World Travel
```

核心總結：

```text
World
→ Simulation Universe

Scene
→ Content Ownership / Serialization

StreamingCell
→ Runtime Residency

Loose Quadtree
→ Spatial Acceleration / HLOD Hierarchy

Room / Portal
→ Indoor Connectivity

HLOD
→ Far Visual Representation

Bundle
→ Physical Packaging
```

以及：

```text
Scene
≠ StreamingCell
≠ GameplayZone
≠ PhysicsWorld
≠ NavigationWorld
≠ RenderWorld
```

---

對外採 Node + Component：

```text
Scene
└─ Node
   ├─ EntityID
   ├─ Name
   ├─ Parent
   └─ Children
```

Component 不直接以完整 OOP Object 儲存在 Node 內。

例如：

EntityID = 125

TransformPool
[125] -> Transform Data

MeshRendererPool
[125] -> MeshRenderer Data

RigidBodyPool
[125] -> RigidBody Data

Gameplay API：

```cpp
Node* player = scene.CreateNode("Player");
```

```cpp
player->AddComponent<MeshRenderer>();
player->AddComponent<Animator>();
player->AddComponent<CharacterController>();
```

Node API 只是方便使用的外層介面。

底層實際透過：

```text
Node
↓
EntityID
↓
Component Pool
↓
SoA Data
```

設計目標：
- Editor / Gameplay 保有熟悉的 Node / Component 工作流
- Runtime 資料不需要 GameObject <-> ECS 雙份同步
- 避免兩份 Transform / Renderer / Physics 資料
- Component Pool 直接是 Source of Truth


## 七、Node / SoA 資料流

核心原則：
- Node 本身負責 Identity / Hierarchy
- Component Pool 負責真正 Runtime Data
- Render World 是 Derived Data，不是 Source of Truth

架構：

```text
Node API
   ↓
EntityID
   ↓
Component Registry
   ↓
┌────────────────┬────────────────┬────────────────┐
│ TransformPool  │ RenderPool     │ PhysicsPool    │
│ SoA            │ SoA            │ SoA            │
└────────────────┴────────────────┴────────────────┘
```

Transform SoA 例：

```text
TransformPool
├─ EntityIDs[]
├─ Positions[]
├─ Rotations[]
├─ Scales[]
├─ WorldMatrices[]
└─ DirtyFlags[]
```

外部可寫：

```cpp
player->transform().SetPosition(...);
```

實際上回傳的是 TransformHandle，
由 Handle 修改對應 Pool Index。

Render Extraction：

```text
Scene Runtime Data
   ↓
Visibility / Extraction
   ↓
Render World / Frame Data
   ↓
Renderer
```

Render World 只是每 Frame 的快取 / 衍生資料。


## 八、Entity ID / UUID 身分策略

引擎將「持久化身分」與「Runtime 身分」完全分離。

### Persistent Identity

Scene / Prefab / Asset 等需要跨執行階段保存的識別，使用：

```text
128-bit UUID
```

用途：
- Scene Serialization
- Prefab Reference
- Asset Reference
- Editor Reference
- Save / Persistent Reference
- 跨 Session 穩定識別

UUID 不作為 Hot Loop 的主要索引。

### Runtime EntityID

Runtime 使用 64-bit packed EntityID：

```text
EntityID (uint64_t)

[ Generation : 32-bit ][ Index : 32-bit ]
```

```cpp
using EntityID = uint64_t;

inline uint32_t GetEntityIndex(EntityID id)
{
    return static_cast<uint32_t>(id);
}

inline uint32_t GetEntityGeneration(EntityID id)
{
    return static_cast<uint32_t>(id >> 32);
}

inline EntityID MakeEntityID(uint32_t index, uint32_t generation)
{
    return (static_cast<uint64_t>(generation) << 32) | index;
}
```

### Entity 建立與回收

採用：
- Generation Array
- Free List
- O(1) Create
- O(1) Destroy
- O(1) Validation

建立流程：

```text
Create Entity
↓
Free List 有可用 Index？
├─ Yes -> 重用 Index
└─ No  -> 擴增新的 Index
↓
讀取 generations[index]
↓
組成 EntityID
```

回收流程：

```text
Destroy Entity
↓
generation[index]++
↓
index 放回 Free List
```

Index 會重複利用，不會因為長時間執行而單純一路遞增到耗盡。

### Stale Handle 防護

例如舊 Entity：

```text
Index      = 42
Generation = 3
```

刪除後 Index 42 被重新使用：

```text
Index      = 42
Generation = 4
```

舊的 `{42, 3}` 因 generation 不一致而判定失效，避免 stale reference / use-after-destroy。

```cpp
bool IsAlive(EntityID id)
{
    const uint32_t index = GetEntityIndex(id);
    const uint32_t generation = GetEntityGeneration(id);

    return index < generations.size()
        && generations[index] == generation;
}
```

### EntityID 與 Node / Component Pool

Node 只負責：
- Identity
- Name
- Parent / Child
- Editor Hierarchy

Runtime Component 資料由 EntityID 對應 Component Pool：

```text
Node
↓
EntityID
↓
Component Registry
↓
Component Pool / SoA
```

EntityID 不代表資料本身，只是 Runtime Handle。

### Hot Loop 不反覆查 EntityID

EntityID 主要用於：
- 建立 / 刪除
- 外部引用
- Component Lookup
- Editor / Gameplay API

高頻 System 更新不應每筆反覆執行：

```text
EntityID
↓
Validation
↓
Component Lookup
↓
Data
```

而是直接迭代 SoA / Chunk：

```cpp
auto positions = transformPool.Positions();
auto velocities = velocityPool.Values();

for (size_t i = 0; i < positions.size(); ++i)
{
    positions[i] += velocities[i] * dt;
}
```

最終策略：

```text
Persistent Identity
→ 128-bit UUID

Runtime Identity
→ packed uint64 EntityID
   ├─ 32-bit Generation
   └─ 32-bit Index

Hot Loop
→ Direct SoA / Chunk Iteration
```

目標：
- O(1) 建立 / 回收
- O(1) 存取與驗證
- Stale Handle 安全性
- 高 Cache Locality
- 不使用 UUID 作 Runtime Hot Path Lookup
- 不讓 EntityID Lookup 成為 System 內迴圈成本


### Generation Overflow Policy

32-bit Generation 對一般 Scene Entity 已極度充裕，但仍必須定義 overflow 行為。

Slot reuse：

```cpp
uint32_t nextGeneration = generation + 1u;

ENGINE_ASSERT_MSG(
    nextGeneration != 0u,
    "Entity generation overflow detected");

generation = nextGeneration;
```

正式規則：

- Generation `0` 保留為 Invalid / Never-issued state
- Debug / Development Build 發生 wrap-around 時直接 Assert / Fatal
- Shipping Build 不得靜默把 wrapped generation 當成正常可重用 ID
- 若真的碰到 generation exhaustion，該 slot 應 retire，不再放回 free-list

更重要的是：

```text
High-frequency transient object
≠
Scene Entity
```

以下類型預設不使用 Scene `EntityID`：

- GPU Particle
- CPU Particle
- Bullet / Projectile swarm（大量短生命週期時）
- Decal particle
- Temporary VFX element
- Per-frame render item
- Short-lived simulation micro-object

這些應使用：

```text
Dedicated Pool
Batch System
SoA Storage
Generation Handle（若需要）
```

避免高頻 create / destroy 污染 Scene Entity allocator，也避免把 Entity system 當成所有暫存物件的通用容器。

## 九、Source of Truth 規則

資料                         Source of Truth
Node Hierarchy               Scene World
Transform                    Transform Pool
Mesh Renderer                Render Component Pool
Physics                      Physics Component Pool
Gameplay Component           Component Pool
Render Submission            Derived / Cached
GPU Buffer                   Derived / Cached
Inspector                    Reflection View
Serialized Scene             Persistent Representation
Prefab                       Persistent Representation
Asset Import Cache           Derived / Cached

任何模組都必須明確知道：
- 哪份資料是主資料
- 哪份資料只是 cache
- 哪個方向允許更新
- 哪些資料不可雙向同步


## 十、Reflection 系統

V1 不先做大型 Clang-based code generator。

V1 採：
- Runtime Metadata
- Template
- Macro Registration

使用方式：

```cpp
class Light : public Component
{
```
public:
    float intensity = 1.0f;
    bool castShadow = true;

    REFLECT_BEGIN(Light)
        REFLECT_PROPERTY(intensity)
        REFLECT_PROPERTY(castShadow)
    REFLECT_END()
};

Metadata：

```text
TypeInfo: Light
├─ intensity
│  ├─ type = Float
│  ├─ editable = true
│  └─ attributes...
└─ castShadow
   ├─ type = Bool
   └─ attributes...
```

Reflection Database 同時服務：
- Inspector
- Serialization
- Undo / Redo
- Prefab Override
- Copy / Paste
- Property Animation
- Runtime Debugging

後續版本可加入：
- Clang-based Header Tool
- *.generated.cpp
- Annotation Scan
- 自動 Metadata 生成

原則：
- V1 先確保架構穩定
- 避免還沒完成 Engine 就先投入大型 Header Tool


## 十一、Renderer 總體架構

流程：

```text
Scene / Node
    ↓
```
```text
Render World
    ↓
```
```text
Visibility / Culling
    ↓
```
```text
Sorting / Batching
    ↓
```
```text
Render Graph
    ↓
Renderer
    ↓
RHI
   ├─ D3D12
   ├─ Vulkan
   └─ Metal
```

嚴禁：
Scene -> Vulkan API
Scene -> Metal API

Renderer 上層不得知道：
- VkImage
- VkBuffer
- VkDescriptorSet
- MTLTexture
- D3D12 Descriptor Handle


## 十二、RHI 設計

核心抽象：

- GraphicsDevice
- CommandBuffer
- Buffer
- Texture
- Sampler
- Pipeline
- Shader
- Fence
- SwapChain
- Queue
- ResourceState
- ResourceBinding
- RenderPass / RenderingInfo

API 概念：

```cpp
cmd->BeginRendering(...);
cmd->SetPipeline(...);
cmd->BindResources(...);
cmd->DrawIndexed(...);
cmd->EndRendering();
```

Backend：

```text
RHI/
├─ D3D12/
├─ Vulkan/
└─ Metal/
```

目標：
任一 Backend 擴充或修正時，不重寫：
- Scene
- Material
- Render Graph
- Renderer
- Editor
- Asset


## 十三、Resource State / Barrier

自有 ResourceState：

Undefined
CopySource
CopyDestination
ShaderResource
RenderTarget
DepthWrite
DepthRead
Present
Storage
IndirectArgument

禁止讓 Vulkan Image Layout 滲透到 Renderer 上層。

Render Graph 負責推導：

Undefined
 -> RenderTarget
 -> ShaderResource
 -> Present

DX12 / Vulkan / Metal 各自映射。


## 十四、Binding / Descriptor 架構

這是 RHI 最核心的模組之一，不視為單純 API 包裝。

策略：
- Hybrid Bindless-first
- 同時保留較舊裝置 fallback path

高階模型：

Global Resource Table
        +
Per-Frame Constants
        +
Per-Pass Constants
        +
Per-Material Data
        +
Per-Draw Data

Resource Registry：
- Texture
- Buffer
- Sampler

每個 GPU Resource 取得：
- Stable Resource Handle
- GPU Resource Index

Material 不保存原生 Descriptor。

例如：

```cpp
struct MaterialGPU
{
    uint32_t baseColorTexture;
    uint32_t normalTexture;
    uint32_t ormTexture;
```

    float metallic;
    float roughness;
};

Shader 透過 ResourceIndex 取得 Texture。

Vulkan：
- Descriptor Set
- Descriptor Indexing
- Large Resource Arrays

Metal：
- Argument Buffer
- Resource Table

DX12：
- Shader-visible Descriptor Heap
- Descriptor Table
- Root Signature Mapping

禁止：
- 上層直接看到 VkDescriptorSet
- Material 每次 Draw 建一套 Descriptor
- API 綁死傳統 slot-based model


## 十五、Binding Tier

不能假設所有手機都支援完整 Bindless。

定義 Binding Capability Tier：

Tier 1：
- Traditional / Limited Resource Binding
- 適合較舊或限制較多的裝置

Tier 2：
- Large Indexed Resource Tables

Tier 3：
- Full Bindless / GPU-driven Friendly

啟動時做 Capability Query：

```cpp
if (device.bindingTier >= BindingTier::Bindless)
{
    UseBindlessRenderer();
}
else
{
    UseFallbackBinding();
}
```

Renderer 必須能依 Tier 選擇 Binding Path。


## 十六、Draw Data / GPU Driven 預留

DrawData：

```cpp
struct DrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
};
```

GPU：

```text
DrawData
 ↓
MaterialIndex
 ↓
```
```text
Material Buffer
 ↓
```
```text
Texture ResourceIndex
 ↓
```
Global Resource Table

這樣可直接支援未來：
- GPU Culling
- Indirect Draw
- GPU Driven Renderer
- GPU LOD Selection


## 十七、Render Graph

V1 即導入。

功能：
- Pass Dependency
- Resource Lifetime
- Temporary Render Target
- Barrier
- Synchronization
- Render Target Reuse
- Debug Visualization

典型流程：

```text
Shadow Pass
↓
```
```text
Depth Prepass（可選）
↓
```
```text
Light Culling
↓
```
```text
Forward+ Opaque
↓
Sky
↓
Transparent
↓
SSAO
↓
Bloom
↓
```
```text
Tone Mapping
↓
```
Runtime UI


## 十八、渲染路線

主 Renderer：
- Forward+

原因：
- 適合 Android / iOS
- 較低 bandwidth
- MSAA 友善
- Transparent 整合自然
- 桌面也可維持高畫質

V1：
- CPU Frustum Culling
- Material Sorting
- GPU Instancing
- Forward+
- Shadow

V2：
- Compute Culling
- Indirect Draw
- GPU Occlusion Culling
- GPU LOD Selection
- GPU Driven Rendering



## 十八-A、VFX / Particle Framework

本引擎的粒子 / 特效系統不採用「每個特效由大量獨立 Scene Node + ParticleSystem Component 組成」作為 Runtime 核心模型。

Editor 可保留「一個完整特效由 N 個 Emitter 組成」的直覺工作流，但 Runtime 必須使用 Data-Oriented VFX Program。

正式模型：

```text
Scene
└─ VFXComponent
   └─ VFXInstance

VFX Asset
├─ System Parameters
├─ Logical Emitters[]
│  ├─ Spawn Modules
│  ├─ Update Modules
│  ├─ Render Modules
│  ├─ Events
│  └─ Local Parameters
└─ Timeline / Sequencing

        ↓ Compile

VFX Program
├─ Dependency DAG
├─ Stateless Emitters
├─ Stateful CPU Emitters
├─ Stateful GPU Emitters
├─ Fused Simulation Batches
├─ Renderer Batches
└─ Resource Layout
```

### 一個特效可包含多個 Logical Emitter

例如：

```text
Explosion
├─ Flash
├─ Fire
├─ Smoke
├─ Sparks
├─ Debris
├─ Shockwave
└─ Decal
```

Editor 中上述項目可以像 Unity / Niagara 一樣清楚分層，但：

- Logical Emitter 不是 Scene Node。
- Logical Emitter 不是 EntityID。
- Logical Emitter 不是獨立 Component lifecycle object。
- 一個 `VFXComponent` 對應一個 `VFXInstance`。
- `VFXInstance` 內可包含 N 個 Logical Emitter。
- N 個 Logical Emitter 不代表 N 個 Compute Dispatch 或 N 個 Draw Call。

### Runtime Batch Fusion

符合相容條件的 Logical Emitter 可在 Cook / Compile 階段融合。

例如：

```text
Fire Emitter
Spark Emitter
Ember Emitter
↓
Same Simulation Domain
Same Renderer Domain
Compatible Material / Blend
↓
GPU Simulation Batch A
```

正式關係：

```text
N Logical Emitters
→ M Simulation Batches
→ K Render Batches

通常：
M <= N
K <= N
```

Fusion 條件至少包含：

```text
Simulation Domain
Renderer Domain
Material / Shader Compatibility
Blend Mode
Collision Mode
Sort Requirement
Event Dependency
Resource Layout
```

不允許為了追求 Batch 數量而破壞正確性。

### Simulation Domain

正式定義：

```text
SimulationDomain
├─ Stateless
├─ CPU
└─ GPU
```

#### Stateless Emitter

適用：

```text
Torch Flame
Ambient Dust
Rain
Snow
Simple Spark
Simple Looping Aura
```

若粒子狀態可由：

```text
ParticleState = F(
    InstanceSeed,
    ParticleID,
    Time,
    Parameters
)
```

推導，則不要求 persistent per-particle state。

目標：

- 降低 CPU Tick。
- 降低 persistent memory。
- 適合大量背景 / 環境特效。
- 適合 mobile profile。

#### CPU Particle

適用：

```text
Low particle count
Gameplay-relevant interaction
Precise CPU event
Physics query
Readback-required state
```

資料結構使用 Dedicated Particle SoA：

```text
ParticleSoA
├─ Position[]
├─ Velocity[]
├─ Age[]
├─ Lifetime[]
├─ Size[]
├─ Rotation[]
├─ Color[]
├─ CustomData[]
└─ AliveIndices[]
```

CPU 粒子更新使用 Job System，不允許每粒子 virtual Update。

#### GPU Particle

適用：

```text
Smoke
Fire
Snow
Rain
Leaves
Magic particles
Sparks
Environment FX
```

典型流程：

```text
Spawn Command Buffer
↓
GPU Particle Pool
↓
Compute Simulation
↓
Alive / Dead Compaction
↓
Culling
↓
Optional Sort
↓
Indirect Draw
```

### VFX Instance Shared State

同一完整特效內的 Logical Emitters 共用：

```text
VFXInstance
├─ Transform
├─ Time
├─ Seed
├─ Visibility
├─ LOD
├─ Quality
├─ ParameterBlock
└─ Runtime Handles
```

避免每個 Emitter 各自複製：

```text
Transform
Time
Visibility State
Quality State
Common Parameters
```

### VFX Module Graph

Editor 工作流：

```text
Spawn
↓
Initialize
↓
Update
↓
Output
```

內建 Module Library 至少包含：

```text
Spawn
├─ Rate
├─ Burst
├─ Shape
└─ Initial Velocity

Update
├─ Gravity
├─ Drag
├─ Noise
├─ Attraction
├─ Color Over Life
├─ Size Over Life
├─ Rotation Over Life
└─ Collision

Render
├─ Sprite
├─ Stretched Sprite
├─ Mesh
├─ Ribbon
├─ Trail
└─ Decal / Projected FX
```

Runtime 不以高成本字串式 interpreter 執行 Module Stack。

正式流程：

```text
Editor VFX Graph
↓
VFX Compiler
↓
Constant Folding
↓
Dead Module Elimination
↓
Module Fusion
↓
Dependency Analysis
↓
CPU Program / GPU Compute Program
↓
Cooked VFXProgram
```

### Dead Module Elimination

若功能在 Asset / Quality Profile 中明確關閉：

```text
Collision Disabled
Distortion Disabled
Ribbon Disabled
```

則對應 Module 不得進入 Runtime Program。

VFX Compiler 必須支援：

```text
Constant Folding
Unused Parameter Removal
Dead Module Elimination
Compatible Module Fusion
Feature Stripping
```

### Renderer Domain

正式 Renderer 類型：

```text
RendererDomain
├─ Billboard
├─ StretchedBillboard
├─ Mesh
├─ Ribbon
├─ Trail
├─ Decal
└─ FutureVolume
```

同一 Simulation Data 可掛多個 Renderer Output，但必須由 Compiler 分析是否共享成本合理。

### VFX Material / Shader

VFX 使用既有 Material / Shader Framework，不建立獨立 Renderer。

支援：

```text
Unlit
Additive
Alpha Blend
Alpha Cutout
Distortion
Soft Particle
Lit Particle
Stylized / Anime VFX
```

動畫風 VFX 可支援：

```text
Hard Ramp
Dissolve
Fresnel
Emission
Distortion
Stylized Trail
Stylized Rim
```

### Soft Particle

V1 建議正式支援：

```text
Particle Depth
-
Scene Depth
↓
Depth Fade
↓
Particle Alpha
```

避免 Billboard 與場景幾何交界出現硬切線。

### Collision Tier

不得讓所有 GPU 粒子都使用完整 Physics。

正式分級：

```text
CollisionTier

Tier 0
→ None

Tier 1
→ Depth Buffer Collision

Tier 2
→ Heightfield / SDF / Simplified Scene Collision

Tier 3
→ CPU Physics Query
```

例如：

```text
Rain
→ Depth Collision

Spark
→ Depth / Heightfield

Gameplay Projectile
→ Gameplay / Physics System
```

Gameplay projectile 不視為普通 VFX particle。

### Gameplay 與 Visual Separation

正式原則：

```text
Gameplay Projectile / Damage Logic
→ Gameplay System / Physics

Visual Trail / Spark / Impact
→ VFX System
```

VFX 不得成為 gameplay authoritative hit detection 的預設來源。

### Event / SubEmitter

支援：

```text
Particle Spawn Event
Particle Death Event
Collision Event
Custom VFX Event
Gameplay-triggered Event
```

SubEmitter 不建立新的 Scene Node / Particle Component。

流程：

```text
Emitter Event
↓
Event Buffer
↓
Spawn Commands
↓
Target Logical Emitter
```

Emitter dependency 必須在 Compile 階段建立 DAG。

```text
Rocket
└─ Death Event
   ↓
Explosion Emitter
```

禁止 Runtime 以字串名稱動態搜尋 Emitter。

GPU → CPU event readback 必須受到限制。

優先：

```text
GPU Visual Event
→ GPU-side consume

Gameplay Authoritative Event
→ CPU-side explicit path
```

### Bounds / Visibility Policy

每個 VFX Asset 必須指定 Visibility Simulation Policy：

```text
Always Simulate
Reduce Frequency
Pause Simulation
Catch-up On Visible
Kill When Invisible
```

不得假設所有 Off-screen VFX 都可安全停止。

### Simulation Rate Decoupling

粒子 Simulation Rate 不必等於 Render FPS。

例如：

```text
Render
120 Hz

VFX Simulation
60 / 30 / 15 Hz
```

允許：

```text
Low-frequency simulation
+
Render interpolation
```

特別適合：

```text
Smoke
Fog particles
Ambient dust
Distant environment FX
```

### VFX Budget Manager

不只限制 Max Particle Count。

正式 Budget：

```text
VFXBudgetManager
├─ CPU Time Budget
├─ GPU Simulation Budget
├─ GPU Render Budget
├─ Active VFX Instance Count
├─ Active Logical Emitter Count
├─ Particle Count
├─ Draw Count
├─ Overdraw Estimate
└─ Memory Budget
```

每個 VFX Asset 可設定 Priority：

```text
Critical
Gameplay
Character
Environment
Cosmetic
Background
```

超過 Budget 時降級順序可由 Policy 控制：

```text
Background
→ Lower Spawn Rate

Environment
→ Lower Simulation Rate

Cosmetic
→ Disable Collision / Distortion

Gameplay / Critical
→ Preserve
```

與既有：

```text
PerformancePolicyManager
```

整合，可依：

```text
Thermal
Battery
Frame Time
Memory Pressure
Quality Profile
Platform Profile
```

調整：

```text
Spawn Rate
Max Particle Count
Simulation Frequency
Collision Tier
Distortion
Shadow
Ribbon Quality
```

### Dedicated Particle Handles

粒子與短生命 VFX 不使用 Scene EntityID。

使用專用：

```text
ParticleHandle
VFXInstanceHandle
EmitterRuntimeHandle
```

並保留 generation validation。

### VFX Memory

CPU 粒子：

```text
Dedicated SoA Pool
```

GPU 粒子：

```text
GPU Particle Pool
Alive List
Dead List
Spawn Buffer
Event Buffer
Indirect Args Buffer
```

Frame 暫存可使用既有 FrameAllocator，但：

- 不得跨 Frame 保留 FramePtr / FrameSpan。
- GPU resource reuse 必須等待對應 GPU Fence。
- Persistent particle state 不放在 Frame Arena。

### RenderGraph Integration

GPU VFX 必須透過 RenderGraph 表達：

```text
Spawn Pass
Simulation Pass
Compaction Pass
Optional Sort Pass
Render Pass
```

讓 RenderGraph 可追蹤：

```text
Buffer State
Resource Lifetime
Barrier
Queue Ownership
Async Compute Opportunity
```

VFX 不得私自繞過 RenderGraph 直接注入不可追蹤的 GPU work。

### Async Compute

若 Backend / Platform Profile 允許，可將：

```text
Particle Simulation
Compaction
Culling
```

放入 Async Compute。

必須由 Scheduler / RenderGraph 決定，不允許 VFX Asset 自己綁定 backend queue。

### VFX Profiler

至少顯示：

```text
Active VFX Instances
Logical Emitters
Runtime Simulation Batches
Render Batches
CPU Particle Count
GPU Particle Count
Spawn Count / Frame
CPU Simulation Time
GPU Simulation Time
GPU Render Time
Overdraw Estimate
Sort Cost
Collision Cost
Event Count
Memory
```

Profiler 必須可顯示：

```text
Logical Emitters
→ Actual Runtime Batches
```

用來確認 Fusion 是否生效。

### VFX CI / Validation

CI 至少包含：

- VFX Graph compile determinism。
- Dependency DAG cycle fail。
- Dead Module Elimination test。
- Stateless / CPU / GPU representative asset。
- CPU particle lifetime / handle generation validation。
- GPU buffer lifetime / fence-safe reuse。
- VFX feature stripping。
- Soft Particle Golden Image。
- Billboard / Mesh / Ribbon / Trail representative Golden Image。
- Runtime Batch Fusion correctness。
- Off-screen simulation policy regression。
- VFX Budget degradation policy test。
- Android / iOS mobile particle budget test。
- DX12 / Vulkan / Metal representative GPU simulation test。

### VFX DoD

V1 VFX Framework 至少完成：

```text
1 VFXComponent
→ 1 VFXInstance

1 VFXInstance
→ N Logical Emitters

N Logical Emitters
→ M Runtime Simulation Batches
→ K Render Batches
```

並能完成代表性效果：

```text
Explosion
├─ Flash
├─ Fire
├─ Smoke
├─ Sparks
└─ Shockwave
```

要求：

- Editor 中仍能獨立調整每個 Logical Emitter。
- Runtime 不建立 5 個 Scene Particle Components。
- GPU-compatible Emitters 可融合。
- 可顯示 Logical Emitter 與 Runtime Batch 對照。
- 可在 mobile profile 動態降級。
- Gameplay logic 與 visual particles 分離。



## 十八-B、Animation Framework

本引擎的 Animation System 採 Data-Oriented Runtime，並將動畫邏輯、Pose Evaluation、Skinning、GPU Crowd Animation 分層。

正式流程：

```text
Gameplay / AI
↓
Animation Parameters
↓
Animation Graph / State Machine
↓
Pose Evaluation
↓
Local Pose
↓
Global Pose
↓
Skinning Data
↓
GPU Skinning / GPU Animation Path
```

### 核心資產

```text
Animation Asset
├─ Skeleton
├─ Skinned Mesh
├─ Animation Clip
├─ Animation Graph
├─ Bone Mask
├─ Retarget Profile
├─ Animation Profile
└─ GPU Animation Cooked Data
```

Source Asset 不作為 Shipping Runtime 直接格式。

```text
FBX / glTF / Source Animation
↓
Importer
↓
Canonical Skeleton / Clip
↓
Animation Cooker
↓
Runtime Animation Assets
```

### Skeleton / Rig

Skeleton Asset 至少包含：

```text
Skeleton
├─ Bone Hierarchy
├─ Parent Index
├─ Bind Pose
├─ Inverse Bind Pose
├─ Bone Name / Stable Bone ID
├─ Skeleton UUID
└─ Optional Skeleton LOD Mapping
```

Runtime 不以每骨骼 Scene Node / Component 表示 Skeleton。

Skeleton 為共享 Asset：

```text
1 Skeleton Asset
→ N Character Instances
```

### Animation Clip

Runtime Clip 至少支援：

```text
Translation Track
Rotation Track
Scale Track
Root Motion Track
Animation Events
Compression Metadata
```

Cook 階段支援：

```text
Track Optimization
Key Reduction
Compression
Constant Track Folding
Unused Bone Track Removal
Platform-specific Cook
```

### Animation Graph

Editor 中可使用：

```text
State Machine
Blend Tree
Layer
Bone Mask
Additive
Pose Cache
IK
Root Motion Node
Custom Animation Node
```

但 Runtime 不直接執行 Editor Graph Object。

正式流程：

```text
Animation Graph
↓
Animation Compiler
↓
AnimationProgram
```

AnimationProgram 至少包含：

```text
State Table
Transition Table
Parameter Table
Pose Ops
Blend Ops
Layer Ops
Event Ops
Runtime Metadata
```

Parameter 不允許以高頻字串 lookup 驅動。

使用：

```text
AnimationParameterID
StateID
TransitionID
ClipID
BoneMaskID
```

### State Machine / Blend Tree

V1 至少支援：

```text
State Machine
1D Blend
2D Blend
Direct Blend
Cross Fade
Transition Conditions
Trigger / Bool / Float / Int Parameters
```

典型：

```text
Idle
↓ Speed
Walk
↓ Speed
Run
```

以及：

```text
Grounded == false
→ Jump

AttackTrigger
→ Attack
```

### Layer / Bone Mask / Additive

支援：

```text
Base Layer
Upper Body Layer
Face Layer
Additive Layer
```

合成：

```text
Final Pose
=
Base
+ Masked Override
+ Additive
```

Bone Mask 必須 Cook 成 compact bone ranges / bitset / index table，不使用 runtime 字串名稱搜尋。

### Pose Cache

同一 Frame 若多個 Animation Node 需要同一中間 Pose：

```text
Evaluate Once
↓
Pose Cache
↓
Reuse
```

Profiler 必須提供：

```text
Pose Cache Hit
Pose Cache Miss
Pose Memory
Duplicate Evaluation Avoided
```

### Root Motion

Animation System 只輸出：

```text
RootMotionDelta
```

不直接偷偷修改 Scene Node。

流程：

```text
Animation
↓
Extract RootMotionDelta
↓
Gameplay / Character Movement
↓
Apply / Reject / Warp
```

方便：

```text
Prediction
Networking
Motion Warping
Gameplay Authority
```

### IK / Procedural Pose

V1 建議：

```text
Two Bone IK
Look At
Aim IK
Foot IK
```

後續：

```text
FABRIK
CCD
Pose Warping
Motion Warping
```

IK 必須受 Animation LOD 控制，遠距角色可跳過或降頻。

### Retargeting

正式支援：

```text
Source Skeleton
↓
Retarget Profile
↓
Target Skeleton
```

Retarget Profile 至少包含：

```text
Bone Mapping
Retarget Pose
Root Mapping
Scale Rules
Twist Rules
IK Bone Mapping
```

Runtime 高成本 Retarget 不作為大量 Crowd 預設路徑。

優先在：

```text
Import / Cook
```

完成可提前處理的資料。

### Secondary Motion

動畫 Pose 後可加入：

```text
Spring Bone
Simple Chain Dynamics
Physics-driven Bone
Cloth Interface
Accessory / Hair / Tail Dynamics
```

流程：

```text
Base Animation Pose
↓
IK / Procedural
↓
Secondary Motion
↓
Final Pose
```

### GPU Skinning

V1 正式支援 GPU Skeletal Mesh Skinning。

基本流程：

```text
CPU Animation Graph
↓
CPU Pose Evaluation
↓
Final Bone Matrices
↓
Global Skinning Buffer
↓
GPU Vertex Skinning
```

沿用正式 Draw Data：

```cpp
struct alignas(16) SkinnedDrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t skinningMatrixOffset;
};
```

規則：

- 使用 `skinningMatrixOffset`。
- 不使用 per-draw `skinningBufferIndex`。
- 不為每個角色建立獨立 Constant Buffer。
- 不做 blanket 256-byte per-character padding。
- GPU storage alignment 依實際 Backend / Buffer Contract 處理。

Global Skinning Buffer：

```text
Character A
→ offset 0

Character B
→ offset N

Character C
→ offset M
```

### Compute Skinning

V2 支援：

```text
Bone Matrix Buffer
+
Bind Pose Vertex Buffer
↓
Compute Skinning
↓
Skinned Vertex Buffer
```

適合：

```text
Shadow Pass
Depth Pass
Main Pass
Outline Pass
Motion Vector Pass
```

共用同一次 skinning result。

正式 Skinning Mode：

```text
SkinningMode
├─ Auto
├─ VertexShader
└─ Compute
```

`Auto` 可依：

```text
Vertex Count
Render Pass Count
Skeleton Size
Distance
Platform
GPU Budget
```

選擇。

### Skinned Mesh Instancing

正式支援：

```text
1 Skeleton Asset
1 Skinned Mesh
1 Material
→ N Character Instances
```

每個 Instance 可有：

```text
World Transform
Animation State
Animation Time
Pose / Pose Handle
Skinning Matrix Offset
Material Params
LOD
```

正式 Instance Data：

```cpp
struct SkinnedInstanceData
{
    uint32_t transformIndex;
    uint32_t skinningMatrixOffset;
    uint32_t materialIndex;
    uint32_t flags;
};
```

GPU 使用：

```text
InstanceID
↓
SkinnedInstanceData
↓
skinningMatrixOffset
↓
Bone Transform
```

支援：

```text
GPU Instancing
Indirect Draw
GPU Culling
LOD Selection
```

N 個 Instance 不代表 N 個 Draw Call。

### Animation Sharing

大量相同 Skeleton / Clip / 相近 Time 的角色可共享 Pose。

```text
1000 NPC
↓
Pose Bucketing
↓
50 Unique Poses
```

Instance：

```text
Instance
→ poseIndex
```

而不是：

```text
Instance
→ dedicated full bone matrices
```

Animation Sharing 可與：

```text
Skeleton LOD
GPU Instancing
Indirect Draw
GPU Culling
```

整合。

### Animation Update Rate LOD

動畫邏輯 / Pose Update 不必等於 Render FPS。

例如：

```text
Near
→ 60 Hz

Mid
→ 30 Hz

Far
→ 15 Hz

Very Far
→ Pose Hold / Shared Pose / Impostor
```

Render 可做 interpolation。

### Skeleton LOD

支援：

```text
LOD0
→ Full Skeleton

LOD1
→ Reduced Skeleton

LOD2
→ Crowd Skeleton
```

可移除：

```text
Finger
Facial
Accessory
Secondary Bones
```

但 Mesh Skin Weight / Bone Remap 必須由 Cook Pipeline 正確生成。

### GPU Animation / Bone Animation Texture

正式支援 Bone Animation Texture 類 GPU Animation 路徑。

高階 API 不將 storage 寫死為 Texture2D。

使用抽象：

```text
GPUAnimationPoseStorage
```

Backend 可使用：

```text
Texture2D
Texture2DArray
StructuredBuffer
StorageBuffer
```

正式流程：

```text
Source Animation Clip
↓
Animation Cooker
↓
Sample Clip
↓
Evaluate Skeleton Hierarchy
↓
Global Bone Transform
↓
Inverse Bind Pose
↓
Final Skin Matrix
↓
Pack
↓
GPUAnimationClip
+
GPUAnimationPoseStorage
```

### Bone Animation Texture 自動 Bake

Bone Animation Texture 必須由 Asset / Cook Pipeline 自動產生。

使用者不需要：

```text
Manual Export Texture
Manual Frame Layout
Manual Bone Packing
Manual Atlas Maintenance
```

正式工作流：

```text
FBX / glTF / Animation Clip
↓
Import
↓
Animation Profile
↓
Animation Cooker
↓
CPU Runtime Clip
+
GPU Animation Clip
```

Source Clip 不因 Bake GPU Data 而被刪除。

### Animation Runtime Mode

Asset / Profile：

```text
AnimationRuntimeMode
├─ Auto
├─ Skeletal
├─ BoneTexture
└─ VAT
```

`Auto` 可依：

```text
Character Importance
Distance
Instance Count
Skeleton Complexity
Animation Feature Requirement
Platform Profile
Performance Budget
```

選擇 Runtime Path。

### Animation Profile

Bake 設定集中於 Animation Profile，不要求每個 Clip 重複設定。

範例：

```text
CrowdAnimationProfile

Runtime:
Skeletal + BoneTexture

GPU Animation:
Encoding = Matrix3x4
SampleRate = 30 Hz
Interpolation = On
SimpleCrossFade = On
SkeletonLOD = On
Compression = Auto
```

Profile 可套用整個 Asset Folder / Character Class / Cook Rule。

### Platform-specific GPU Animation Cook

不同 Platform Profile 可 Cook：

```text
Windows High
→ 30 / 60 Hz

macOS High
→ 30 / 60 Hz

Android High
→ 30 Hz

Android Low
→ 15 Hz + Compression

iOS
→ 30 Hz + Compression
```

Runtime 不需要攜帶所有 Platform 的資料。

### GPU Animation Encoding

V1：

```text
Matrix3x4
```

理由：

```text
Simple
Stable
Low shader complexity
Direct final skinning matrix
```

V2 可加入：

```text
CompressedTRS
DualQuaternion
QuantizedTRS
```

### Final Skin Matrix Bake

Crowd Bone Animation Texture V1 預設直接 Bake：

```text
Final Skin Matrix
```

而不是 Local Bone Transform。

流程：

```text
Local Pose
↓
Hierarchy Solve
↓
Global Bone Matrix
↓
Inverse Bind Pose
↓
Final Skin Matrix
↓
Bake
```

Runtime：

```text
Sample Final Skin Matrix
↓
Skin Vertex
```

優點：

```text
No runtime hierarchy solve
Low GPU control-flow complexity
Good for massive crowd
```

限制：

```text
No full runtime IK
No arbitrary procedural bone edit
No full runtime retarget
```

若未來需要更彈性，可追加：

```text
LocalTRS GPU Animation Mode
```

但不是 V1 Crowd 預設。

### GPU Animation Clip Metadata

至少：

```cpp
struct GPUAnimationClipMeta
{
    uint32_t poseOffset;
    uint32_t frameCount;
    float sampleRate;
    float duration;
};
```

實際 Cook Metadata 還應包含：

```text
Skeleton ID
Bind Pose Hash
Encoding
Bone Count
Skeleton LOD
Compression Version
Cook Version
```

### GPU Animation Instance Data

大量 Crowd Runtime Instance 只需少量資料。

例如：

```cpp
struct GPUAnimationInstanceData
{
    uint32_t clipIndex;
    uint32_t frame0;
    uint32_t frame1;
    float frameBlend;

    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t lod;
    uint32_t flags;
};
```

或 Runtime 只輸入：

```text
clipIndex
normalizedTime
```

再由 GPU 推導：

```text
frameFloat
frame0
frame1
blend
```

### Frame Interpolation

GPU Animation 支援：

```text
Frame 0
Frame 1
↓
Interpolation
↓
Final Pose
```

降低 Bake Sample Rate 時的視覺跳動。

### Simple Cross Fade

Crowd GPU Animation 支援有限的簡單 transition：

```text
Clip A + Time A
Clip B + Time B
Transition Weight
```

GPU：

```text
Sample Pose A
Sample Pose B
↓
Blend
↓
Skin
```

不要求 GPU Crowd Path 執行完整任意 Animation Graph。

正式區分：

```text
Full Animation Path
→ CPU Graph
→ CPU Pose
→ GPU Skinning

GPU Crowd Path
→ Simple State
→ GPUAnimationClip
→ GPU Pose Sampling
→ GPU Skinning
```

### Clip Atlas / Pose Storage Packing

Animation Cooker 自動 pack：

```text
Idle
Walk
Run
Attack
Hit
Death
...
```

進：

```text
GPUAnimationPoseStorage
```

不要求使用者手動維護 Texture Atlas。

Cooker 必須維護：

```text
Clip Offset
Frame Count
Bone Count
Encoding
Alignment
Version
```

### Bake Dependency / Invalidation

GPU Animation Cooked Data 必須依賴：

```text
Source Clip Hash
Skeleton UUID
Skeleton Layout
Bind Pose Hash
Bone Count
Sample Rate
Encoding
Compression Settings
Skeleton LOD Mapping
Cook Version
```

任一變更：

```text
Invalidate GPU Animation Asset
↓
Automatic Rebuild
```

避免 Skeleton / BAT 資料錯配。

### Bake Validation

Cook 必須驗證：

```text
Skeleton ID Match
Bone Count Match
Bind Pose Hash Match
Clip Duration
Frame Count
No NaN / Inf
Matrix Validity
Encoding Validity
Storage Bounds
```

失敗：

```text
Cook Hard Fail
```

不允許等到 Runtime 才發現。

### Character Animation LOD

推薦：

```text
LOD0
→ CPU Animation Graph
→ Unique Pose
→ Vertex / Compute Skinning

LOD1
→ Bone Animation Texture
→ 30 Hz
→ Per-instance Time

LOD2
→ Bone Animation Texture
→ 15 Hz
→ Pose Bucket / Animation Sharing

LOD3
→ VAT / Impostor
```

此為建議 Profile，不強制所有遊戲固定使用此數值。

### VAT

VAT 作為更激進的大量 / 遠距 / VFX Animation Path。

```text
Vertex Position
Normal
Optional Tangent
↓
Bake per frame
↓
Vertex Animation Storage
```

適用：

```text
Background Crowd
Very Far Character
Fixed Animation Creature
Cloth / Destruction / VFX Mesh
```

VAT 不取代一般 Skeletal Animation。

### Animation Events

使用：

```text
AnimationEventID
Payload
Time
```

避免高頻字串 Callback。

例如：

```text
FootstepLeft
FootstepRight
WeaponTrailOn
WeaponTrailOff
AttackWindowStart
AttackWindowEnd
```

Gameplay-critical authority 不應完全依賴 Visual Animation Event。

### Gameplay / Animation Separation

原則：

```text
Gameplay
→ 決定 Attack / Movement / Ability

Animation
→ 表現 / 同步角色動作
```

Animation State 不作為所有 Gameplay Truth 的唯一來源。

### Animation Memory

Persistent：

```text
Skeleton Asset
Animation Clip
Animation Program
Pose Pool
GPU Animation Pose Storage
Skinning Buffer
```

Transient：

```text
Frame Pose Scratch
Blend Scratch
IK Scratch
Upload Staging
```

Transient 資料必須遵守既有 FramePtr / FrameSpan lifetime contract。

### Job System Integration

Animation Evaluation 使用：

```text
Animation Graph Jobs
Clip Sampling Jobs
Pose Blend Jobs
IK Jobs
Global Pose Jobs
Skinning Upload Jobs
```

必須在 Render Extraction 前完成必要 Pose。

與既有 Frame Barrier / Module-safe Barrier 一致。

### RenderGraph / GPU Integration

Compute Skinning / GPU Animation 可透過 RenderGraph：

```text
GPU Animation Sample
↓
Optional Compute Skinning
↓
Depth / Shadow / Main / Outline
```

GPU resource lifetime / barrier / queue ownership 交由 RenderGraph 管理。

### Animation Profiler

至少：

```text
Active Animators
Active Skeletons
Unique Poses
Shared Poses
Animation Sharing Hit Rate
Graph Evaluation Time
Clip Sampling Time
Pose Blend Time
IK Time
Retarget Time
Pose Cache Hit Rate
Bone Count
Skeleton LOD Distribution
Skinning Matrix Upload Bytes
GPU Animation Instances
GPU Animation Pose Storage Size
Vertex Skinning GPU Time
Compute Skinning GPU Time
GPU Animation Sampling Time
```

### Animation CI / Validation

CI 至少包含：

- Skeleton import determinism。
- Clip compression regression。
- Animation Graph compile determinism。
- State Machine transition test。
- Blend Tree representative test。
- Pose Cache correctness。
- Root Motion extraction regression。
- Retarget representative skeleton test。
- Skeleton LOD bone remap validation。
- CPU Pose vs reference Golden Pose。
- Vertex Skinning representative Golden Image。
- Compute Skinning representative Golden Image。
- Skinned Instancing N-instance test。
- Animation Sharing correctness。
- GPU Animation Bake determinism。
- GPUAnimationClip dependency invalidation。
- Bind Pose Hash mismatch hard fail。
- Bone Animation Texture / Pose Storage frame interpolation test。
- GPU Crowd simple crossfade test。
- DX12 / Vulkan / Metal representative GPU animation test。
- Android / iOS crowd animation performance gate。

### Animation DoD

V1 Animation Framework 至少完成：

```text
Skeleton
+
Animation Clip
+
Animation Graph
+
State Machine / Blend Tree
+
Layer / Bone Mask
+
Pose Cache
+
Root Motion
+
Basic IK
+
GPU Vertex Skinning
+
Skinned Mesh Instancing
+
Animation LOD
```

GPU Crowd / Bone Animation Texture 路徑至少完成：

```text
Automatic Bake
GPUAnimationClip
GPUAnimationPoseStorage
Final Skin Matrix Encoding
Frame Interpolation
Per-instance Animation Time
GPU Instancing
Indirect Draw Integration
Animation Sharing / Pose Bucket foundation
```

使用者正常匯入：

```text
Skeleton + Animation Clips
```

即可由 Cook Pipeline 自動產生 GPU Crowd Animation Data，不要求人工製作 Animation Texture。


## 十九、渲染品質目標

V1 目標：
至少 Unity URP 級別。

PBR：
- Cook-Torrance
- GGX
- Smith Geometry
- Schlick Fresnel
- Metallic / Roughness Workflow

Material Inputs：
- Base Color
- Metallic
- Roughness
- Normal
- AO
- Emission
- Alpha

Lighting：
- Directional Light
- Point Light
- Spot Light
- Forward+ Light Culling

Shadow：
- Cascaded Shadow Map
- PCF
- Shadow Bias
- Normal Bias

Environment：
- HDR
- IBL
- Reflection Probe
- Prefiltered Environment
- BRDF LUT

Post：
- SSAO
- Bloom
- FXAA
- Tone Mapping
- Color Grading
- Fog

後續進階效果：
- TAA
- Contact Shadow
- PCSS
- SSR
- Volumetric Fog
- Water
- Terrain Decal
- Terrain Virtual Texturing
- Advanced Vegetation Shading / Impostor Refinement

### Shading Model Framework

本引擎不將所有材質強制塞進單一 PBR Shader，也不將整個世界套用單一 Toon Shader。

正式 Shading Model：

```text
ShadingModel
├─ PBR
├─ StylizedPBR
├─ Anime
├─ Vegetation
├─ Water
└─ Unlit
```

共同原則：

- 所有 Shading Model 共用同一套 Renderer / Forward+ / RenderGraph。
- 共用 Shadow、Light、Fog、Reflection、Lightmap / Probe、Resource Binding 與 Shader Variant 系統。
- 不為 Anime / StylizedPBR / Vegetation / Water 建立獨立平行 Renderer。
- Shading Model 是 Material / Shader Feature 層級差異，不改變 RHI 架構。
- 每個 Shading Model 必須可依 Build Profile / Asset Usage 做 Shader Variant Stripping。

### PBR

用途：

```text
寫實或接近寫實的材質
→ 金屬
→ 塑膠
→ 石材
→ 工業物件
→ 寫實場景
```

V1 Contract：

```text
Cook-Torrance
+ GGX
+ Smith Geometry
+ Schlick Fresnel
+ Metallic / Roughness Workflow
```

並明確規範：

```text
Linear Lighting
sRGB Texture Sampling
Energy Conservation
HDR Lighting
IBL
Diffuse Irradiance
Prefiltered Specular Environment
BRDF LUT
```

### StylizedPBR

`StylizedPBR` 為「動畫風 / Painterly 場景」的主要 Scene Shading Model。

用途：

```text
Architecture
Rock
Props
General Environment Mesh
Stylized Open-world Scene
```

核心不是完全捨棄 PBR，而是：

```text
PBR Foundation
↓
Art-directed Material
↓
Stylized Light / Shadow Response
↓
Baked Lighting / Probe
↓
Fog / Atmosphere / Color Grading
↓
Stylized Scene Final Look
```

支援項目：

```text
Base Color
Normal
AO
Roughness
Metallic
Emission
Lightmap
Reflection Probe
Shadow Tint
Light Tint
Contrast Curve
Saturation Control
Stylized Roughness
Optional Lighting Ramp
```

原則：

- 保留 GGX / Roughness / Reflection 等物理材質基礎。
- 可對 Direct Lighting 套用可配置的 Stylization Curve。
- Shadow 不只做線性變暗，可使用 Shadow Tint / Material-specific dark color。
- 不使用角色式 Face SDF / Hair Highlight 邏輯處理一般場景。
- Static Scene 優先支援 Lightmap / Probe / Baked GI 資料整合。
- Dynamic Main Light 仍可提供 Directional Light / Shadow Map。
- 近、中、遠距可使用不同 Lighting LOD，但資料來源與切換規則必須明確。

### Anime Character Shading

`Anime` 為角色專用 NPR / Anime Shading Framework，不等同於一般二段式 Toon。

正式架構：

```text
Anime
├─ Body
│  ├─ Multi-Ramp Diffuse
│  ├─ AO
│  ├─ Material Region
│  └─ Stylized Specular
│
├─ Face
│  ├─ Face SDF Shadow
│  ├─ Face Orientation
│  └─ Face-specific Color
│
├─ Hair
│  ├─ Toon Diffuse
│  ├─ Hair Highlight
│  ├─ Optional Flow / Anisotropic Direction
│  └─ Bangs Shadow
│
├─ Eye
│  ├─ Iris
│  ├─ Highlight
│  └─ Optional Parallax
│
├─ Rim
│
└─ Outline
   ├─ Smoothed Normal
   ├─ Width Mask
   └─ Material-dependent Color
```

V1 最低要求：

```text
Multi-Ramp Material Lighting
Face SDF
Hair-specific Shading
Per-material Outline
```

#### Multi-Ramp Lighting

```text
NdotL
↓
Lighting Remap / Half-Lambert when needed
↓
Material Region / Ramp Index
↓
Ramp Texture / Curve
↓
Light / Mid / Shadow Color
```

材質區域至少可區分：

```text
Skin
Hair
Cloth
Metal
Leather / Hard Surface
```

不同區域可有不同：

```text
Ramp
Shadow Color
Specular Response
Rim Response
Outline Color
```

#### Anime Control Map

不直接綁定任何特定遊戲資產的通道定義；引擎使用自己的 Canonical Packing。

建議 V1：

```text
AnimeControlMap

R = AO
G = Specular Mask
B = Material Region / Ramp Index
A = Outline Width
```

如資訊不足，可使用第二張：

```text
AnimeDetailMap

R = Hair Highlight Mask
G = Face / Special Mask
B = Rim Mask
A = Emission / Custom
```

Asset Importer / Material Inspector 必須顯示每個 channel 語意，避免 magic channel。

#### Face SDF

一般角色 Face 不直接完全依賴 Mesh Normal 的 NdotL。

```text
Face Forward / Right
+
Main Light Direction
↓
Relative Horizontal Light Angle
↓
Face SDF Sample
↓
Threshold / Smooth Transition
↓
Artist-authored Face Shadow
```

目標：

- 避免鼻樑 / 眼窩形成寫實但不適合動畫角色的碎陰影。
- 正面 / 左側 / 右側 / 背光都維持穩定動畫臉部陰影。
- Face SDF 僅作為 Anime Face feature，不污染通用 PBR pipeline。

#### Hair Shading

Hair 不與一般 Cloth / Skin 共用完全相同的 Specular。

可支援：

```text
Hair Diffuse
Hair Shadow
Hair Highlight Mask
Directional / Flow-based Highlight
Optional Stylized Anisotropic Specular
```

V1 可以先採：

```text
Artist-authored Highlight Mask
+
Directional Highlight Offset
+
Threshold / Ramp
```

後續再加入 Flow Map / anisotropic model。

#### Rim Light

Rim 不應只是整個角色固定一圈白光。

```text
1 - NdotV
↓
Threshold / Power
↓
Light Direction
↓
Material / Rim Mask
↓
Rim Color
```

支援：

```text
Backlight stronger
Front light weaker
Per-material intensity
Per-region mask
```

#### Outline

角色輪廓優先支援 Geometry Outline / Inverted Hull。

```text
Original Mesh
↓
Smoothed Normal Extrusion
↓
Cull Front
↓
Render Back Faces
↓
Outline
```

Outline 可由：

```text
Material Region
Vertex Color / Mask
Distance Compensation
Outline Width
Outline Color
```

共同控制。

Screen-space Outline 可作為場景或 Debug / Style extension，但不取代 Character Geometry Outline。

### Vegetation Shading

Vegetation 為專用 Shading Model，而不是一般 PBR Mesh 加 Alpha。

```text
Vegetation
├─ Alpha Cutout
├─ GPU Instancing
├─ Wind Vertex Animation
├─ Vertex Weight / Variation
├─ AO
├─ Optional Normal
├─ Transmission / Back Lighting
├─ Light Probe / SH
├─ Shadow
└─ LOD / Billboard
```

Wind：

```text
World Position
+
Time
+
Wind Direction
+
Noise
+
Vertex Weight
↓
Vertex Offset
```

可使用 Vertex Color / Asset Data 定義：

```text
Root Weight
Tip Weight
Bend Weight
Variation
```

葉片透光：

```text
Back Lighting
+
Thickness / Material Parameter
+
Transmission Color
```

避免背光植被直接變成黑塊。

### Water Shading

Water 為獨立 Shading Model / Render Feature。

V1/進階路線：

```text
Water
├─ Animated Normal
├─ Fresnel
├─ Shallow / Deep Color
├─ Scene Depth Fade
├─ Reflection Probe
├─ Optional SSR
├─ Refraction
├─ Foam
└─ Optional Caustics
```

核心：

```text
Scene Depth
↓
Shallow / Deep Blend

Fresnel
↓
Reflection Weight

Normal
↓
Reflection / Refraction Distortion
```

Reflection fallback：

```text
SSR Hit
→ SSR

SSR Miss / SSR Disabled
→ Reflection Probe / Environment
```

### Scene Lighting / Baked Lighting

動畫風大型場景不能只依賴大量 realtime light。

Static Environment 必須支援：

```text
Directional Sun
+
Shadow Map
+
Lightmap / Baked Lighting
+
Light Probe / SH
+
Reflection Probe
+
AO
```

Lighting LOD 可依 profile 使用：

```text
Near
→ Full / High-quality Baked Lighting

Mid
→ Lower-cost Baked / Probe representation

Far
→ Probe / SH / Vertex-baked fallback when appropriate
```

此處是引擎能力設計，不強制所有專案使用固定三級策略。

### Environment / Atmosphere

場景風格的一部分由 Environment Shader / Render Feature 提供：

```text
Sky
Cloud
Distance Fog
Height Fog
Atmospheric Color
Cloud Shadow
Color Grading
Tone Mapping
```

Cloud Shadow 可使用低成本 World-space projected mask：

```text
World Position XZ
↓
Scrolling Cloud Shadow Texture
↓
Directional Light Modulation
```

不要求天空雲幾何與地面雲影逐像素完全一致。

### Scene / Character Combination

推薦的動畫風世界：

```text
World

Terrain
→ Terrain / StylizedPBR

Buildings
→ StylizedPBR

Rock / Props
→ StylizedPBR

Vegetation
→ Vegetation

Water
→ Water

Character
→ Anime

Sky / Fog / Cloud
→ Environment Render Features
```

目標是：

```text
角色有清楚的 Anime NPR 識別
+
場景保留 Stylized PBR 的材質與空間感
+
共用統一光源、陰影、霧、反射與 RenderGraph
```

### Shading Model CI / Golden Image Gate

至少建立以下 Golden Scene：

```text
PBR Material Sphere
StylizedPBR Architecture / Rock
Anime Character Body
Anime Face SDF
Anime Hair Highlight
Anime Outline
Vegetation Back Lighting
Water Reflection / Depth Fade
Mixed Scene：StylizedPBR Environment + Anime Character
```

CI 驗證：

- DX12 / Vulkan / Metal 代表性 Golden Image。
- Direct / Shadow / Fog / Probe integration。
- Shading Model Variant compile / strip。
- Anime Face SDF 左右光向測試。
- Outline width 在距離變化下不出現明顯失控。
- Vegetation Wind / Transmission 不破壞 Shadow / Instancing。
- Water SSR unavailable 時 fallback 正確。
- Mixed Scene 不需要第二套 Renderer。

## 二十、Shader 系統

主語言：
- Slang

目標：
- HLSL-like syntax
- Module
- Generic
- Interface
- Reflection
- Specialization
- 跨平台輸出

Pipeline：

```text
Slang
├─ DXIL   -> DX12
├─ SPIR-V -> Vulkan
└─ MSL    -> Metal
```

重要風險：
Slang -> Metal 路線必須先完成 PoC。

PoC 驗證項目：
- Vertex Shader
- Fragment Shader
- Compute Shader
- PBR
- Skinning
- Instancing
- Forward+ Compute
- Shadow
- Texture Array
- Argument Buffer
- Specialization Constant
- Reflection
- Windows DX12
- Windows Vulkan
- macOS Apple Silicon
- iPhone 真機
- Xcode GPU Capture / Frame Debugger

只有全部通過後，才將 Slang 定為唯一 Shader Frontend。

### Shader Source of Truth / Metal Fallback

正式規範：

```text
Slang
→ 唯一 Shader Source of Truth
```

禁止另外維護一套獨立手寫 MSL Shader 來作為正常 Fallback，避免：

- Resource Binding Layout 分歧
- Reflection Table 分歧
- Material Parameter Layout 分歧
- Shader Feature / Variant Key 分歧
- DX12 / Vulkan / Metal 行為不一致

Metal 首選路徑：

```text
Slang
↓
MSL
↓
Apple Metal Compiler
```

若 Slang 直出 MSL 在實際版本或特定 Feature 上有阻礙，Fallback 統一為：

```text
Slang
↓
SPIR-V
↓
SPIRV-Cross
↓
MSL
↓
Apple Metal Compiler
```

不允許：

```text
Independent Hand-written MSL
→ 繞過 Slang Reflection / Binding Contract
```

### Unified Reflection / Binding Contract

不論 Metal 使用 Direct MSL 或 SPIR-V → SPIRV-Cross → MSL，Engine 上層必須使用同一份 canonical shader metadata。

Canonical metadata 至少包含：

```text
Resource ID
Resource Type
Set / Space
Binding
Array Count
Access
Stage Visibility
Argument Buffer Group
Material Parameter Layout
Push / Root Constant-equivalent metadata
Specialization Constant
Vertex Input / Fragment Output
```

Pipeline：

```text
Slang Source
↓
Canonical Reflection / Binding Metadata
├─ DX12 Mapping
├─ Vulkan Mapping
└─ Metal Mapping
     ├─ Direct Slang → MSL
     └─ SPIR-V → SPIRV-Cross → MSL
```

Metal Backend 必須建立 explicit remap table，將 canonical Resource Binding 映射到：

```text
[[buffer(N)]]
[[texture(N)]]
[[sampler(N)]]
Argument Buffer [[id(N)]]
```

因此 `MaterialGPU` / Resource Registry / ResourceIndex 不感知 Metal fallback 實際走哪條 compiler path。

### Metal Fallback Gate

CI / Shader Gate 必須同時驗證：

- Direct Slang → MSL Reflection Mapping
- Fallback SPIR-V → SPIRV-Cross → MSL Mapping
- Resource Count
- Resource Type
- Logical Binding ID
- Argument Buffer Layout
- Material Parameter Offset / Size
- Pipeline Creation
- Golden Image（代表性 Shader）

若兩條 Metal path 對 canonical reflection contract 產生不同結果：

```text
Build / CI Fail
```

而不是讓 Runtime 或 Material System 特判。



## Shader Variant 管理

Shader Variant 必須避免對所有組合做笛卡兒積全量預編譯。

可能的 Variant 維度：

```text
Graphics Backend
× Graphics Quality Tier
× Binding Tier
× Material Feature
× Geometry Feature
× Lighting Feature
× Platform Capability
```

Feature 使用 Bitmask 分類：

```text
MaterialFeature
├─ NormalMap
├─ Emission
├─ AlphaTest
├─ TerrainLayerBlend
├─ ShadingModelPBR
├─ ShadingModelStylizedPBR
├─ ShadingModelAnime
├─ ShadingModelVegetation
├─ ShadingModelWater
├─ ShadingModelUnlit
├─ AnimeFaceSDF
├─ AnimeHairHighlight
├─ AnimeOutline
├─ VegetationTransmission
├─ WaterSSR
└─ FutureMaterialFeatures

GeometryFeature
├─ Skinning
├─ Instancing
├─ Morph
└─ VegetationWind

LightingFeature
├─ Shadow
├─ IBL
├─ Fog
└─ AdditionalLights
```

Variant Key：

```text
ShaderVariantKey
=
Hash(
    ShaderID,
    Backend,
    QualityTier,
    BindingTier,
    FeatureMask
)
```

核心策略：

1. 只編譯實際被使用的 Variant：掃描 Scene、Prefab、Material、Terrain Material、Vegetation Asset、Quality Profile、Platform Capability。
2. 不對所有可能 Feature 組合預編譯；未使用 Normal Map、Skinning、Terrain Layer Blend 等功能時不產生對應 Variant。
3. Static / Dynamic Feature 分流：會改變 Resource Layout、Pipeline State 或有顯著效能差異的功能才做 Static Variant；小型功能優先使用 Dynamic Branch / Data-driven 控制。
4. Variant Cache：Editor On-demand Compile、Disk Cache、Pipeline Cache、Build Precompile、Stable Cache Key，並依 Shader Source Hash 失效。
5. Incremental Shader Compile：建立 Shader dependency graph，記錄 Shader Source、Included Module、Feature Definition、Material Usage、Target Backend。Shader 改動後只重新編譯受影響 Variant。
6. Variant Budget：每個 Shader 設定 Variant Budget；CI 監控 Total / Newly Added / Compiled / Stripped Variants、Compile Time 與 Pipeline Count，異常增加可 Warning 或 Fail。

CI Shader Gate：

```text
Changed Shader / Module
↓
Dependency Resolve
↓
Affected Variant Set
↓
Compile DX12 / Vulkan / Metal Targets
↓
Reflection Validation
↓
Pipeline Creation Smoke Test
↓
Golden Image / Feature Test（需要時）
```

### Metal Shader Fallback Risk Policy

Metal Direct MSL 與 SPIRV-Cross Fallback 都必須在 CI / device lab 預先驗證。

Golden Image / Reflection mismatch：

```text
CI Detect
↓
Mark Direct Path Unsupported for that Engine/Shader Feature Profile
↓
Build Pipeline selects validated fallback
```

禁止在 Shipping Runtime 因單次畫面差異「動態自行切 compiler pipeline」。

Fallback 選擇應是：

```text
Build-time / Cook-time validated policy
```

並記錄：

```text
Engine Version
Slang Version
SPIRV-Cross Version
Device / OS Profile
Shader Feature Profile
```


### Variant Budget Accounting

單一 Shader 的 Variant Budget 不使用固定全引擎常數，
而由：

```text
Shader Profile
+
Platform Profile
+
Quality Tier
```

共同決定。

CI / Build Report 至少追蹤：

```text
Theoretical Variant Count
After-Pruning Count
Project Used Count
Cooked Count
Budget
```

例如：

```text
TerrainPBR

Theoretical : 8192
After Pruning: 1240
Project Used : 386
Cooked       : 386
Budget       : 512
```

這些數值分別用來定位不同類型的問題：

```text
Theoretical 過高
→ Feature 定義組合爆炸

After-Pruning 仍過高
→ Mutually Exclusive / Constraint Rule 不足

Project Used 過高
→ 專案實際 Feature 使用過度分散

Cooked > Project Used
→ Stripping / Cook Pipeline 異常
```

### CI Gate

每個 Shader / Platform Profile 都有自己的：

```text
Variant Budget
```

超過：

```text
Cooked Count > Budget
→ Hard Fail
```

也可對：

```text
Theoretical
After-Pruning
Project Used
```

設定 Trend / Warning 閾值，提前發現成長趨勢。

禁止在架構層硬編：

```text
All Shaders <= 256
```

這類全域固定數字。

實際 Budget 應由：

```text
Mobile Low
Mobile High
Desktop
Platform / Quality Profile
```

各自配置。

## 二十一、Material 系統

### Material / Shading Model 分層

Material 不以「每種視覺效果都建立完全獨立 Renderer」的方式擴充。

正式分層：

```text
Material
├─ Shading Model
├─ Surface / Blend Mode
├─ Feature Mask
├─ Texture Handles
├─ Parameters
└─ Render State
```

初始 Shading Model：

- PBR
- StylizedPBR
- Anime
- Vegetation
- Water
- Unlit

其他專用 Material / Render Feature：

- Transparent
- Sky / Atmosphere
- Terrain
- UI

Surface / Blend Mode 與 Shading Model 分離，例如：

```text
Opaque
Masked
Transparent
Additive / Special when explicitly supported
```

避免把：

```text
Transparent
```

誤當成與：

```text
PBR / Anime / StylizedPBR
```

同層級的 Lighting Model。

Material 儲存：
- Shader ID
- Texture Resource Handles
- Parameters
- Render State

不保存：
- VkDescriptorSet
- Metal Argument Encoder State
- DX12 Descriptor Heap Pointer

Material Setter 可以是：

material.SetTexture("BaseColor", texture);

但底層實際上是：
Texture
-> Resource Registry
-> ResourceIndex
-> MaterialGPU


### Material Shading Model Contract

Material Asset 必須明確記錄：

```text
ShadingModel
SurfaceMode
FeatureMask
ShaderProfile
Quality Overrides（若有）
```

`ShadingModel` 不使用 arbitrary string runtime lookup；Cook 後轉成穩定 enum / ID。

建議：

```cpp
enum class ShadingModel : uint8_t
{
    PBR,
    StylizedPBR,
    Anime,
    Vegetation,
    Water,
    Unlit
};
```

高階 Gameplay / Asset API 不直接知道 Backend Pipeline State。

### Common Lighting Inputs

Shading Model 可共享：

```text
Main Directional Light
Forward+ Additional Light List
Shadow Data
IBL / Reflection Probe
Lightmap / Probe
Fog
Camera / View
Environment Parameters
```

但各 Model 可選擇實際使用 subset。

例如：

```text
Anime
→ Main Light + Additional Light policy + Shadow + Fog

StylizedPBR
→ Main Light + Forward+ + IBL + Lightmap + Probe + Fog

Vegetation
→ Main Light + Probe + Shadow + Transmission + Fog

Water
→ Main Light + Environment + Reflection + Fog
```

不得因共用資料而強迫每個 Shading Model 執行全部計算。

### Material Region / Control Map Metadata

Packed Control Texture 必須有 Asset Metadata 描述 Channel Semantics。

例如：

```text
Texture Semantic = AnimeControlMap
R = AO
G = SpecularMask
B = MaterialRegion
A = OutlineWidth
```

Importer / Inspector / Cook Pipeline 共用同一 Semantic Schema。

禁止只靠文件外的「約定俗成 RGBA」而沒有 metadata。

### Shading Model Stripping

Build Scanner 必須分析：

```text
Scene
Prefab
Material
Terrain
Vegetation
Water
Quality Profile
Platform Profile
```

若專案未使用：

```text
Anime
Water
Vegetation
StylizedPBR
```

對應 Shader / Variant / Asset Cook path 可被移除。




## JSON / Serialization Framework

Engine 內建通用 JSON Parse / Generate Framework。

底層採用：

```text
yyjson
```

正式邊界：

```text
Engine Systems / Gameplay / Tools
↓
Engine JSON API
↓
yyjson
```

禁止 Engine Runtime / Editor / Gameplay 散佈：

```text
yyjson_doc*
yyjson_val*
yyjson_mut_doc*
```

`yyjson_*` type 只允許存在於：

```text
Serialization/Json/Private/
```

### JSON Core API

V1 至少提供：

```text
JsonDocument
MutableJsonDocument
JsonValue
JsonObject
JsonArray
JsonReader
JsonWriter
JsonError
JsonAllocator
```

Read 概念：

```cpp
Result<JsonDocument> ParseJson(StringView text);
```

```cpp
JsonDocument document = Json::Parse(buffer);
JsonObject root = document.Root().AsObject();

uint32_t version = root.GetUInt("version");
StringView name = root.GetString("name");
bool enabled = root.GetBool("enabled");
```

Write 概念：

```cpp
JsonWriter writer;
writer.BeginObject();
writer.Key("id");
writer.UInt(10001);
writer.Key("attack");
writer.Float(120.0f);
writer.EndObject();
```

Mutable DOM 可供 Editor / Tool 建立與修改 JSON tree：

```text
MutableJsonDocument
↓
Mutable JsonObject / JsonArray
↓
JsonWriter
```

### JSON Write Mode

正式支援：

```text
JsonWriteMode
├─ Compact
├─ Pretty
└─ Deterministic
```

用途：

```text
Editor Save
→ Pretty

Runtime / Internal
→ Compact

CI / Generated Asset
→ Deterministic
```

Deterministic Writer 必須保證：

```text
Stable Property Order
Stable Float Formatting
UTF-8
LF Newline
Locale Independent
```

相同 logical data 在 Windows / macOS / CI 應產生相同 deterministic JSON content / hash。

### JSON Encoding / Strictness

Engine JSON 正式規定：

```text
UTF-8 Only
Strict JSON
```

Runtime JSON 不預設接受：

```text
// comment
/* comment */
NaN
Infinity
-Infinity
undefined
```

需要註解時，使用 Schema / Editor metadata，或明確允許的 `_comment` field；不建立非標準 JSON dialect。

若未來需要 JSON5，應作為獨立 Authoring Import Format，不改變 Runtime JSON Contract。

### JSON Number Validation

JSON 原生只有 `number`，Engine / DataTable Schema 必須負責驗證目標型別：

```text
int32
uint32
int64
uint64
float
double
```

禁止 silent cast：

```text
uint32 field = -1
→ Error

integer ID = 1.5
→ Error

overflow / underflow
→ Error
```

Writer 不允許輸出 NaN / Infinity。

### JSON Error Reporting

JSON Parse / Deserialize Error 至少包含：

```text
Error Code
Source File
Line
Column / Offset
JSON Path（可取得時）
Readable Message
Expected Type（可取得時）
Actual Type（可取得時）
```

Data Table error 可再附加：

```text
Table
Row Key
Field
```

例如：

```text
Character.json:128:17
Table: Character
Row: 10001
Field: skill
JSON Path: $.rows[10].skill
Expected: SkillID / uint32
Found: string "Fireball"
```

### JSON Memory

- yyjson allocation 可接 Engine Allocator。
- 大型 Parse 可使用 dedicated JSON Arena。
- JSON DOM 不要求與 Runtime Data 同生命週期。
- Parse → typed/compact representation 後可整體釋放 JSON Arena。
- Hot Loop 不直接查 `JsonValue`。
- 避免每 Frame Parse JSON。

典型：

```text
Asset Buffer
↓
JSON Arena
↓
JsonDocument
↓
Typed / Compact Runtime Data
↓
Destroy JsonDocument
↓
Release Entire JSON Arena
```

### JSON Threading

- `JsonDocument` ownership 必須明確。
- 不假設同一 Mutable Document 可被多執行緒任意修改。
- Background Parse 可在 Job System 執行。
- Parse 完成後發布 immutable / typed data。
- Audio / Render / Physics 等 real-time / hot-path thread 不執行 JSON parse。

### JSON 使用範圍

主要：

```text
Engine Config
Project Settings
Editor Settings
Build Settings
Bundle Manifest
Remote Content Manifest
Asset Metadata
Localization Metadata
Data Table Runtime Asset
Save / Debug Data（依系統需求）
```

JSON 不作為大型 GPU-ready / streaming payload 的主要格式。

以下仍優先 Binary Runtime Format：

```text
Mesh
Animation
Skeleton
Texture Runtime Payload
Terrain Chunk
Vegetation Chunk
Navigation Data
Large Scene Runtime Data
GPU-ready Resource Data
```

格式分工：

```text
Human-readable Config / Data Table
→ JSON

Runtime-heavy / Streaming / GPU-ready Asset
→ Binary
```

Data Table JSON 是 V1 明確允許的 Runtime JSON Asset；載入後會轉成 typed Runtime DataTable，不在 Gameplay hot loop 直接操作 JSON DOM。

Third-party：

```text
yyjson
→ ThirdParty/LICENSES
→ Version Locked
→ Source / License / Redistribution Notice
```


## Data Table Framework

### 定位

Data Table 定義為：

```text
Game Design / Configuration Data
```

例如：

```text
Character
Skill
Item
Monster
Stage
Quest
Drop Table
Shop
Level Curve
Difficulty
Game Config
Platform Profile
Gameplay Balance
```

Data Table 不代表：

```text
Runtime Mutable State
Save State
Entity / Component Runtime State
```

例如：

```text
Current HP
Current Inventory
Quest Progress
Current Buff
Transform
Runtime AI State
```

應存在 Runtime Component / Gameplay State / Save System。

正式原則：

```text
DataTable
→ Configuration Source

Runtime Component / Gameplay State
→ Simulation Source
```

### Authoring / Runtime Pipeline

V1 正式 Runtime DataTable Asset 使用 JSON。

```text
                    Authoring
         ┌─────────────┼─────────────┐
         │             │             │
        JSON          CSV           XLSX
         │             │             │
         └─────────────┼─────────────┘
                       ↓
              Canonical Table Data
                       ↓
                 JSON Generator
                       ↓
                  *.json Asset
                       ↓
              Asset / Bundle System
                       ↓
               Engine JSON Parser
                       ↓
               Schema Validation
                       ↓
             Reference Validation
                       ↓
                Typed Table Build
                       ↓
          DataTable Preprocess Pipeline
                       ↓
                    Finalize
                       ↓
             Immutable Runtime Table
                       ↓
               DataTableRegistry
                       ↓
          ┌────────────┴────────────┐
          ↓                         ↓
        C++                       Zig
     Typed API                Stable C ABI
```

CSV / XLSX 為 Authoring / Import Format，不要求 Shipping Runtime 帶 CSV / Excel parser。

Excel 建議：

```text
1 Sheet
→ 1 Logical DataTable
```

例如：

```text
GameData.xlsx
├─ Character
├─ Skill
├─ Item
└─ Stage
```

Import：

```text
XLSX / CSV
↓
Canonical Table Data
↓
Engine JsonWriter
↓
Character.json / Skill.json / Item.json / Stage.json
```

Excel Formula 只輸出 evaluated result；Runtime 不包含 Excel Formula Engine。

Importer 必須檢查：

```text
#REF!
#VALUE!
#DIV/0!
```

### Data Table JSON Format

範例：

```json
{
  "table": "Character",
  "schemaVersion": 1,
  "contentVersion": 17,
  "rows": [
    {
      "id": 10001,
      "key": "character.warrior",
      "name": "LOC_CHARACTER_WARRIOR",
      "maxHP": 1000,
      "attack": 120,
      "skill": 20001
    }
  ]
}
```

Runtime 原則：

```text
JSON
→ Parse Once
→ Typed Table Build
→ Preprocess
→ Immutable Runtime Containers
```

Gameplay 禁止以：

```text
json["rows"][...]["attack"]
```

作正常 Runtime access path。

### Schema = Source of Truth

JSON 只描述 primitive JSON type；DataTable Schema 才定義真正的 Engine Type / Constraint。

```text
DataTableSchema
├─ TableID
├─ SchemaVersion
├─ PrimaryKey
├─ Columns[]
├─ SecondaryIndices[]
├─ Preprocess[]
├─ Validators[]
└─ StrictFields
```

Column 可描述：

```text
Name
Type
Required
Default
Min / Max
Enum
TableRowRef
AssetRef
LocalizationKey
Attributes
```

V1 基本型別：

```text
bool
int32
uint32
int64
uint64
float
double
String
StringID
Enum
Vec2
Vec3
Vec4
AssetRef
TableRowRef
LocalizationKey
Array<T>
```

`Variant / Any / arbitrary Map<string, Any>` 不作 V1 的預設資料建模方式，避免 Data Table 演變成第二套 scripting language。

### Primary Key System

不全域強迫 Int Key。

```text
DataTablePrimaryKeyType
├─ UInt32
├─ UInt64
└─ String
```

每張 Table 的 Primary Key Type 由 Schema 決定。

典型：

```text
Persistent Gameplay Content Identity
→ 通常 UInt32 Strong ID

Named Configuration / Profile / Rule
→ 通常 String Primary Key

兩者都需要
→ UInt32 Primary ID + String Unique Secondary Key
```

例如：

```text
CharacterTable
→ UInt32

SkillTable
→ UInt32

ItemTable
→ UInt32

GameConfigTable
→ String

DifficultyTable
→ String

GraphicsProfileTable
→ String
```

### Numeric Strong ID

Numeric Content ID 不直接在 C++ Gameplay 以裸 `uint32_t` 表達所有 domain。

```cpp
struct CharacterID
{
    uint32_t value;
};

struct SkillID
{
    uint32_t value;
};
```

因此：

```text
CharacterID
≠
SkillID
```

即使底層 representation 相同。

預設可採：

```text
0 = Invalid
1..N = Valid
```

`uint64` 僅在確實需要更大 identity space / distributed identity contract 時使用。

### String Primary Key

String Key 適合：

```text
PlayerMoveSpeed
Normal / Hard / Nightmare
Graphics.High
Windows / Android / iOS
Gameplay / MainMenu
Forest / Snow
```

預設 String Key Contract：

```text
UTF-8
Exact Match
Case Sensitive
```

禁止未宣告就自動：

```text
tolower
trim
locale-dependent normalization
```

如某 Table 需要 normalization，必須由 Schema 明確宣告。

技術 Key 建議使用：

```text
character.warrior
skill.fireball
achievement.first_boss
graphics.high
```

玩家顯示文字仍走 Localization，不使用本地化字串作 content identity。

### Persistent String Identity / Rename

若 String Primary Key 本身已被 Save / Server / Content reference 使用：

```text
Rename Key
= Breaking Data Change
```

必要時支援：

```text
Alias / Migration
```

例如：

```json
{
  "key": "achievement.kill_first_boss",
  "aliases": [
    "achievement.first_boss"
  ]
}
```

Alias migration 必須是 explicit policy，不得自動猜測 rename。

### Runtime String Optimization

Logical String Key 可透過：

```text
String Pool
+
Hash Index
+
Collision-safe String Verification
```

或 runtime intern：

```text
String
↓
Runtime StringID
```

但：

```text
Runtime StringID
≠
Persistent Identity
```

除非另有 stable serialization contract，Runtime StringID 不直接寫入 Save / Network / Persistent Storage。

### Primary ID + String Secondary Key

大型 Content Table 推薦：

```json
{
  "id": 10001,
  "key": "character.warrior"
}
```

責任：

```text
10001
→ Persistent Runtime Identity

character.warrior
→ Editor / Debug / Search / Script-friendly Name
```

可同時提供：

```text
Find(CharacterID{10001})
FindByKey("character.warrior")
```

### Row Identity

正式 identity 永遠是 Primary Key，不是：

```text
JSON Array Index
Excel Row Number
Runtime RowIndex
Current Sort Order
```

Row reorder 不改變 content identity。

Duplicate Primary Key 必須：

```text
Hard Fail
```

禁止 First Wins / Last Wins。

### Secondary Index

Schema 可宣告：

```text
SecondaryIndex
├─ Numeric
├─ String
├─ Enum
└─ Composite
```

例如 ItemTable：

```text
Primary
→ ItemID

Secondary
├─ key unique
├─ category
├─ rarity
└─ price sorted
```

Secondary Unique Key duplicate 亦為 Validation Error。

### Cross Table Reference

Schema 支援：

```text
TableRowRef<TargetTable>
```

例如：

```text
Character.skill
→ SkillID
→ SkillTable
```

Validation：

```text
Referenced Row exists?
├─ Yes
└─ No → Error
```

String Key Table 亦可：

```text
difficulty
→ TableRowRef<DifficultyTable, String>
```

Cross-table data error 不等到 Gameplay runtime 才發現。

### Asset Reference

DataTable 支援：

```text
AssetRef<Prefab>
AssetRef<Texture>
AssetRef<AudioEvent>
...
```

Runtime identity 使用 Asset UUID / Asset Handle contract，不使用 source path 作 persistent identity。

```text
DataTable
↓
Asset UUID
↓
Asset Dependency Graph
```

Referenced Asset 不存在 → Validation / Build Fail。

### Localization Reference

玩家顯示文字使用：

```text
LocalizationKey / LocalizationStringID
```

例如：

```json
{
  "name": "LOC_CHARACTER_WARRIOR"
}
```

流程：

```text
DataTable
↓
LocalizationKey
↓
Localization System
↓
ZH-TW / ZH-CN / JA / EN
```

### Strict Field Validation

Data Table 預設：

```text
StrictFields = true
```

Unknown Field：

```text
moveSpeeed
→ Error
```

Missing Field：

```text
Required = true
→ Error

Required = false + Default
→ Use Schema Default
```

禁止各 Loader 任意自行提供隱藏 default。

Duplicate JSON Object Key 在 DataTable JSON 應視為 Error，不依賴 First / Last Wins。

### Typed Runtime Row

JSON Parse / Validate 後轉成 typed row。

例如：

```cpp
struct CharacterRow
{
    CharacterID id;
    StringID key;
    LocalizationStringID name;
    AssetID prefab;
    float maxHP;
    float attack;
    SkillID skill;
};
```

Generic API：

```text
Editor
Inspector
Debug
Tooling
```

Typed Generated API：

```text
C++ Gameplay
Zig Gameplay
Runtime Systems
```

Gameplay 不以 string column name hot lookup：

```text
row.GetFloat("Attack")
```

作主要 access path。

### DataTable-specific Schema Codegen

Canonical DataTable Schema 可產生：

```text
C++ Row Struct
C ABI Struct / View
Zig Type / Wrapper
JSON Reader
JSON Writer
Editor Metadata
Validation Metadata
```

這是 DataTable 專用的窄範圍 Schema Codegen，不代表 V1 已完成全 Engine general-purpose Reflection Codegen。

禁止 C++ / Zig / Editor 各自維護互相獨立的 Row Layout definition。

### DataTable Preprocess Pipeline

Data Table 讀取後，正式支援 user-defined / schema-defined preprocessing。

執行時機：

```text
JSON Parse
↓
Schema Validation
↓
Typed Table Build
↓
DataTable Preprocess Pipeline
↓
Finalize
↓
Immutable Runtime DataTable
↓
Publish
```

Preprocess 發生於：

```text
Typed Data 已建立
但尚未 Publish 給 Gameplay
```

### Process Stage

V1 定義：

```text
DataTableProcessStage
├─ Normalize
├─ Resolve
├─ Derive
├─ Index
├─ Optimize
└─ Finalize
```

典型順序：

```text
Parse
↓
Validate
↓
Normalize
↓
Resolve
↓
Derive
↓
Index
↓
Optimize
↓
Finalize
↓
Publish
```

### Built-in Preprocess Capability

至少支援：

```text
Physical Sort
Sorted View
Primary Index
Secondary Index
Group Index
Filter / Partition
Reference Resolve
Derived Column / Derived Data
Weighted Table
Range Table
Tag Index
Search Index
Custom Processor
```

### Physical Sort vs Sorted View

Sort 分為：

```text
PhysicalSort
SortedView
```

`PhysicalSort`：

```text
直接重排 canonical Rows
```

適合 sequential iteration / range query / cache locality 確實需要的 Table。

`SortedView`：

```text
Canonical Rows 不動
↓
建立 RowIndex[] View
```

例如：

```text
Rows
Row0: Level 30
Row1: Level 10
Row2: Level 20

ByLevel
[1, 2, 0]
```

V1 預設優先 `SortedView`，因為同一 Table 可同時存在：

```text
ByLevel
ByAttack
ByPrice
ByName
```

而不複製 Row data。

### Group Index

例如：

```text
MonsterTable
↓
ByType
├─ Normal[]
├─ Elite[]
└─ Boss[]
```

或：

```text
ByBiome
├─ Forest[]
├─ Snow[]
└─ Cave[]
```

Group 儲存 RowIndex / ArrayRef，不複製完整 Row。

### Derived Data

Preprocess 可建立純由 configuration 推導的資料：

```text
BaseDamage + AttackSpeed
→ DPS
```

但禁止把：

```text
Current Buff
Current Player Level
Current Equipment
Runtime State
```

混入 DataTable Preprocess。

正式：

```text
Preprocess
= Derived Configuration Data

not
= Runtime Gameplay State
```

### Weighted Table

Drop / Spawn / Encounter 等可在 Load 時建立：

```text
Weight
↓
Cumulative Weight / Alias Table
```

避免 Runtime 每次重新建立 sampling structure。

### Range Table

例如：

```text
EXP → Level
Score → Reward Tier
Distance → Config Tier
```

可 Preprocess 成：

```text
Sorted Range
+
Binary Search Index
```

### Reference Resolve

Preprocess 可將 Stable ID 輔助 resolve 成：

```text
Generation-aware RowIndex / Handle
```

不建議把跨 Table 永久 raw C++ pointer 寫入 Runtime Row，避免破壞 Hot Reload / Generation Safety。

### Declarative Preprocess

常見 Preprocess 可直接由 Schema 宣告：

```json
{
  "preprocess": [
    {
      "type": "index",
      "field": "id",
      "unique": true
    },
    {
      "type": "sortView",
      "name": "ByLevel",
      "field": "level",
      "order": "ascending"
    },
    {
      "type": "group",
      "name": "ByType",
      "field": "type"
    }
  ]
}
```

正式原則：

```text
Common Processing
→ Schema Declarative

Game-specific Processing
→ Custom Processor
```

### Custom Processor

提供概念介面：

```text
IDataTableProcessor
DataTableBuildContext
DataTableProcessResult
```

Custom Processor 可用於：

```text
MonsterTableProcessor
DropTableProcessor
StageTableProcessor
EconomyTableProcessor
```

Processor 只操作 build-time / loading-time builder data，不直接 mutate 已 Publish 的 Runtime Table。

### Processor DAG

Processor 順序不只依註冊順序。

```text
Build Skill Index
↓
Resolve Character Skill Reference
↓
Build Character Secondary Index
```

使用：

```text
DataTable Processor DAG
```

Cycle：

```text
A → B → C → A
→ Hard Fail
```

### Processor Determinism / Version

硬性要求：

```text
Same JSON
+
Same Schema
+
Same Processor Version
↓
Same Runtime Data
```

Processor 不得依賴：

```text
Current Time
Random Device
Thread Scheduling
Machine Locale
Unspecified Hash Iteration Order
```

Sort 若 key 相同，需有 stable tie-breaker，例如：

```text
Level ASC
then PrimaryKey ASC
```

Build key 至少包含：

```text
Source Hash
Schema Version
Processor ID
Processor Version
```

Processor 演算法改變時，相關 DataTable cache / derived data 必須失效。

### Preprocess Memory / Job System

Preprocess 可使用：

```text
Build Arena
Scratch Allocator
```

Finalize：

```text
Compact Persistent Runtime Data
↓
Release Build Arena
```

沒有 dependency 的不同 Table 可平行 preprocess；有 dependency 時依 Processor / Table DAG 排程。

### Runtime Container Model

Data Table 載入完成後存放於 Runtime Container，而不是保留 JSON DOM 作主要資料來源。

正式模型：

```text
DataTableRuntime
=
Contiguous Rows
+
Indices
+
Views
+
Pools
+
Derived Data
```

一張 Table：

```text
CharacterTableRuntime
│
├─ Rows[]
│
├─ PrimaryIndex
│  └─ PrimaryKey → RowIndex
│
├─ SecondaryIndices
│  ├─ StringKey → RowIndex
│  ├─ Type → RowIndex[]
│  └─ Rarity → RowIndex[]
│
├─ SortedViews
│  ├─ ByLevel[]
│  └─ ByAttack[]
│
├─ GroupIndices
│
├─ StringPool[]
├─ ArrayPool[]
└─ DerivedData[]
```

### Canonical Rows

V1 Row storage 預設：

```text
Contiguous RowMajor / AoS
```

概念內部可由：

```text
EngineArray<Row>
```

或 V1 private implementation 使用 `std::vector<Row>`。

Public API / DLL ABI / Zig ABI 不暴露 STL container type。

### Primary Index

Rows 本身不需要複製進 Hash Map。

```text
PrimaryKey
↓
PrimaryIndex
↓
RowIndex
↓
Rows[RowIndex]
```

例如：

```text
10001 → 0
10002 → 1
10003 → 2
```

Hash / Dense Index strategy 可依 Key distribution / Table profile 選擇，但 public contract 不綁定特定 STL container。

### String Index

String Primary / Secondary Key：

```text
Hash Table
+
StringPool
+
Collision-safe compare
```

不要求每 Row 各自持有獨立 `std::string` allocation。

### Sorted View Storage

Sorted View 只存：

```text
RowIndex[]
```

例如：

```text
ByLevel = [1, 2, 0]
```

不複製完整 Row。

### Group Storage

Group 建議：

```text
GroupKey
↓
ArrayRef<RowIndex>
↓
Shared RowIndex Pool
```

避免每個 group 各自大量小 `vector` allocation。

### Variable-length Field Storage

Array field 不建議每 Row 使用 `std::vector<T>`。

使用：

```cpp
struct ArrayRef
{
    uint32_t offset;
    uint32_t count;
};
```

例如：

```cpp
struct CharacterRow
{
    CharacterID id;
    ArrayRef skills;
};
```

資料存在：

```text
Shared ArrayPool
```

### String Pool

Variable String 可使用：

```text
StringPool
+
StringRef / StringID
```

避免每 Row 一個 heap `std::string`。

### Future Packed Memory Block

V1 不要求一開始完全單一 allocation，但 Runtime Layout 應允許後續收斂成：

```text
RuntimeDataTable Memory Block

Header
Rows
Primary Index
Secondary Indices
Sorted Views
Group / RowIndex Pool
Array Pool
String Pool
Derived Data
```

有利：

```text
Load
Unload
Generation Swap
Memory Tracking
Cache Locality
```

### DataTableRegistry

Runtime Registry：

```text
DataTableRegistry
├─ CharacterTable
├─ SkillTable
├─ ItemTable
├─ StageTable
├─ GameConfigTable
└─ ...
```

Gameplay 不直接操作 JSON Asset；透過 typed Table API / Registry 查詢。

### Runtime Immutable

Finalize / Publish 後：

```text
DataTable
→ Immutable
```

禁止：

```text
characterTable[10001].attack += 50
```

Runtime modifier 應存在 Component / Gameplay State。

### Hot-loop Policy

DataTable 不作高頻 simulation storage。

不推薦：

```text
10000 Characters
×
Every Frame DataTable Lookup
```

推薦：

```text
Spawn / Initialize
↓
Resolve DataTable Row
↓
Copy Required Runtime Config
↓
Component SoA
↓
Simulation Hot Loop
```

DataTable 主要用於：

```text
Initialization
Configuration
Rules
Occasional Lookup
```

### Zig Stable C ABI

Zig 不直接看到：

```text
JsonDocument
JsonValue
C++ Template
std::vector
std::unordered_map
```

正式：

```text
DataTableRegistry
↓
Stable C ABI
↓
Generated Zig Typed Wrapper
```

大量 lookup 使用 batch API：

```text
ID[]
↓
ResolveRowsBatch
↓
Typed Row View[]
```

String Key 頻繁 lookup 可：

```text
Resolve String Key Once
↓
DataTableKeyHandle / Runtime StringID
↓
Repeated Lookup
```

避免每次跨 ABI 傳 `const char*` 再重新 hash / compare。

### Reference Graph Semantics

正式區分：

```text
Build Dependency
≠
Logical Row Reference
```

例如：

```text
Character → Skill
Skill → Character
```

Logical Row reference cycle 不必然非法。

只有真正造成 Asset Build / Load Dependency cycle 時才依 Asset / Bundle DAG 規則 Hard Fail。

### Hot Reload / Generation

Data Table JSON 是標準 Asset / Bundle content，沿用既有：

```text
UUID
Version
Hash
Bundle
Remote Update
Generation Pinning
Atomic Activation
Rollback
```

Hot Reload：

```text
Character.json Changed
↓
Parse
↓
Schema Validation
↓
Reference Validation
↓
Typed Table Build
↓
Preprocess Pipeline
↓
Finalize
↓
Success?
├─ No
│  ↓
│ Keep Generation N
│
└─ Yes
   ↓
   Publish Generation N+1
```

禁止直接 mutate 正在被 Runtime 讀取的 Generation N containers。

```text
Build completely new DataTable Generation
↓
Validate
↓
Safe / Atomic Publish
```

新 lookup：

```text
→ N+1
```

舊 pinned view：

```text
→ N
```

等相關 pin / read context 歸零後才回收 N。

仍不鼓勵 Gameplay 長期持有 raw row pointer；能在 initialization copy 必要 config 時優先 copy。

### SchemaVersion / ContentVersion

分離：

```text
SchemaVersion
ContentVersion
```

結構 / field contract 改變：

```text
SchemaVersion++
```

純數值 / content 調整：

```text
ContentVersion++
```

### DataTable Residency

V1 不建立獨立 `DataTableResidencyScope`。

DataTable JSON / Runtime Table 直接使用既有：

```text
Asset Residency
Bundle Residency
Generation Pinning
```

Audio 有 `AudioResidencyScope` 是因為 Voice / Streaming / Fade 有特殊 lifetime；DataTable 暫無需複製同一模型。

### Custom Validation

支援：

```text
IDataTableValidator
```

例如：

```text
SkillValidator
StageValidator
DropTableValidator
EconomyValidator
```

可驗證：

```text
Cooldown > 0
Drop Probability valid
Boss ID exists
Shop Item exists
Required Asset exists
Localization Key exists
```

直接進 CI / Asset Gate。

### DataTable Profiler / Debug

至少追蹤：

```text
Loaded Tables
Loaded Generations
Rows / Table
Table Memory
JSON Parse Time
Validation Time
Typed Build Time
Preprocess Time
Lookup Count
Lookup Miss
Batch Lookup Count
String Index Lookup
Reload Count
Old Generation Pin Count
Primary / Secondary Index Memory
Sorted View Memory
Derived Data Memory
String Pool Memory
Array Pool Memory
```

Development Mode 可偵測高頻 repeated Table lookup，提示將必要設定 copy 至 Runtime Component。

### DataTable CI / Gate

至少：

```text
JSON Parse / Generate
UTF-8 Validation
Deterministic JSON Write
Schema Validation
UInt32 Primary Key
UInt64 Primary Key
String Primary Key
Duplicate Primary Key Fail
Unique Secondary Key Fail
Unknown Field Fail
Missing Required Field Fail
Number Overflow / Type Fail
Cross Table Reference Validation
Asset UUID Reference Validation
Localization Reference Validation
Sorted View
Group Index
Weighted Table
Range Table
Custom Processor
Processor DAG Cycle Fail
Processor Determinism
Processor Version Cache Invalidation
Hot Reload Failure Keeps Old Generation
Generation Pinning
Runtime Container Index Correctness
C++ / C ABI / Zig Generated Layout Validation
No yyjson Backend Type Leak
No STL Container Across Stable ABI
```

### DataTable V1 Definition of Done

V1 至少完成：

```text
Engine JSON Parse / Generate Framework
+
JSON Runtime DataTable Asset
+
Schema Validation
+
UInt32 / UInt64 / String Primary Key
+
Secondary Index
+
Typed Runtime Row
+
Contiguous Rows
+
Primary / Secondary Runtime Indices
+
String Pool / Array Pool
+
Sorted View / Group Index
+
Preprocess Pipeline
+
Declarative Preprocess
+
Custom Processor
+
Immutable Runtime Table
+
DataTableRegistry
+
Hot Reload / Generation Swap
+
Asset / Bundle Integration
+
C++ / Zig Typed API
+
Profiler / CI
```

Future optional optimization：

```text
JSON
↓
Binary Table Cook
```

只有 profiling 證明大型 Table 的 JSON load / parse / memory 成為實際瓶頸時才導入，不作 V1 前置條件。

## 二十二、Asset Pipeline

三層：

Assets/
- 原始美術資產

Library/
- Import Cache
- Engine Runtime Format

GameData/
- Build 後 Bundle

流程：

```text
Source Asset
↓
Importer
↓
UUID
↓
```
```text
Engine Runtime Asset
↓
```
```text
Platform Compression
↓
```
Asset Bundle


## 二十三、Asset UUID

每個 Asset 都有 UUID。

例如：

Character.fbx
Character.fbx.meta

meta：

uuid: ...
type: Model

引用使用 UUID，不使用路徑。

優點：
- 搬移資產不會破壞 Scene / Prefab / Material
- Dependency 容易追蹤
- Hot Update 容易管理


## 二十四、Asset Database

儲存：
- UUID
- Path
- Type
- Dependencies
- Import Settings
- Hash
- Generated Artifacts

支援：
- Incremental Import
- Dependency Tracking
- Reimport
- Cache
- Thumbnail
- Search
- Asset Reference


## 二十五、FBX / 模型資產

Source：
- FBX
- glTF
- GLB
- OBJ

FBX 規則：
- FBX 僅屬於 Editor Import Format
- Runtime 不包含 FBX SDK
- Android / iOS Shipping Runtime 不解析 FBX
- Build 時全部轉為 Engine Runtime Format

流程：

Windows/macOS Editor

```text
Hero.fbx
↓
```
```text
FBX Importer
↓
```
```text
Intermediate Scene
↓
```
```text
Engine Asset Compiler
↓
Hero.mesh
Hero.skeleton
Idle.anim
Run.anim
Attack.anim
```

Runtime 僅讀：
- .mesh
- .skeleton
- .anim

好處：
- 減少 Binary Size
- 降低 Runtime Dependency
- 降低授權與部署複雜度
- 提升載入速度


## 二十六、Texture Pipeline

Source：
- PNG
- JPG
- TGA
- HDR
- EXR

Runtime Compression：

Windows：
- BC7
- BC5
- BC1 / BC3 視需要

Android：
- ASTC

macOS：
- ASTC / BC，依硬體策略

iOS：
- ASTC

Import Settings：
- Texture Type
- sRGB
- Mipmap
- Normal Map
- Max Size
- Compression
- Platform Override


## 二十七、Asset Bundle

Build 後：

base.bundle
ui.bundle
character.bundle
map01.bundle
audio.bundle

功能：
- Compression
- Async Loading
- Streaming
- Dependency
- Patch
- Hot Update
- DLC / Remote Asset（後續）



### Streaming Bundle 類型

- Terrain Chunk Bundle
- Vegetation Cluster Bundle
- Large Map Cell Bundle

這些 Bundle 與一般 Bundle 共用 UUID、Dependency Graph、Compression、Async IO、Asset Registry；差別只在 Load Policy：一般 Bundle 偏功能/關卡級，Streaming Bundle 偏空間位置/Residency 級。


### Bundle Dependency DAG Gate

Bundle dependency graph 必須是 DAG（Directed Acyclic Graph）。

Build / Asset Packaging 階段：

```text
Collect Bundle Dependencies
↓
Build Directed Graph
↓
Topological Sort
↓
Cycle Detection
```

若存在：

```text
A.bundle → B.bundle
B.bundle → A.bundle
```

或更長循環：

```text
A → B → C → A
```

必須：

```text
Build Fail
```

並輸出完整 cycle path，例如：

```text
Bundle dependency cycle:
characters.bundle
→ shared_fx.bundle
→ common_materials.bundle
→ characters.bundle
```

禁止 Runtime 嘗試以 lazy load / reference count 來「容忍」循環依賴，避免：

- Streaming unload 無法收斂
- Bundle lifetime reference cycle
- Hot Update activation deadlock
- Memory 無法釋放
- Load order 不確定

Asset Gate / CI 必須包含：

```text
Bundle Dependency DAG Check
```


## Bundle 熱更新 / Remote Content

Bundle System 必須原生支援 Asset / Data 熱更新。

核心模型：

```text
Built-in Bundle
+
Downloaded Cache Bundle
+
Remote Manifest
```

Runtime 資產解析優先順序：

```text
Downloaded Cache
↓
Built-in Bundle
```

因此 App 內建舊版 Bundle 時，只要 Cache 中存在已驗證的新版本，Runtime 即使用 Cache 版本。

範例：

```text
Built-in:
ui.bundle v1

Cache:
ui.bundle v3

Runtime:
→ 使用 ui.bundle v3
```

### Remote Manifest

每次 Content Build 產生版本 Manifest：

```json
{
  "contentVersion": "1.2.5",
  "bundles": {
    "map01": {
      "version": 7,
      "hash": "...",
      "size": 125829120,
      "dependencies": [
        "shared_environment"
      ]
    }
  }
}
```

Manifest 至少包含：

- Bundle ID
- Bundle Version
- Content Hash
- File Size
- Dependencies
- Optional / Required
- Minimum App Version（需要時）
- Platform / Quality / Compression Variant（需要時）

更新流程：

```text
Download Remote Manifest
↓
Compare Local Manifest
↓
Check Version / Hash
↓
Determine Required Bundles
↓
Download Temporary File
↓
Verify Hash
↓
Verify Bundle Header / Version
↓
Atomic Commit
↓
Activate New Bundle
```

### Download 與 Atomic Replace

禁止下載時直接覆蓋正在使用的正式 Bundle。

流程：

```text
map01.bundle.tmp
↓
Download Complete
↓
Hash Verification
↓
Bundle Validation
↓
Check Target Bundle State
↓
Atomic Activate / Commit
```

目標 Bundle 若仍處於：

```text
Loaded
Open File Descriptor
Memory-mapped
Streaming Read In-flight
Asset Decode In-flight
```

不得直接原地替換。

BundleManager 必須先：

```text
Stop New Loads
↓
Wait / Cancel In-flight IO
↓
Unload Bundle Index
↓
Release File Handle
↓
Unmap Memory
↓
Verify No Active Mapping / Descriptor
```

之後才允許 Activate 新版本。

跨平台採保守一致策略：

```text
Can Safely Activate Now
→ Atomic Rename / Version Switch

Cannot Safely Activate Now
→ Mark Pending Update
→ Keep Current Bundle Active
→ Activate During Next App Pre-init
```

正式策略優先使用 versioned cache，避免對 active bundle 做同名 in-place replacement：

```text
Cache/map01/v6/map01.bundle
Cache/map01/v7/map01.bundle
```

舊版本只有在：

```text
No Open Handle
No mmap
No In-flight IO
No Active Asset Dependency
```

後才標記 `SafeToDelete`；實際刪除可延後到背景清理或下次 App Pre-init。

Active Manifest 只在新版本驗證完成且可安全啟用後切換：

```text
ActiveVersion: v6
↓
Validate v7
↓
Switch Manifest Pointer
↓
ActiveVersion: v7
```

這可避免不同 OS 對 open / mapped file rename/delete 語意差異影響 Runtime 正確性。

若下載或驗證失敗：

```text
.tmp / candidate version
→ Delete / Resume Later

Existing Valid Bundle
→ 保持不變
```

避免網路中斷、磁碟不足、open handle 或 mmap 尚未釋放造成更新失敗或資產損壞。

### Rollback

Bundle Cache 必須支援至少一個可用舊版本的回復策略。

```text
Current Bundle
↓
Update Candidate
↓
Validation Failed
↓
Rollback
↓
Previous Valid Bundle
```

可採：

```text
map01.bundle
map01.bundle.prev
```

或版本目錄：

```text
Cache/
└─ map01/
   ├─ v6/
   └─ v7/
```

Bundle 啟用完成後才更新 Active Manifest。

### 熱更新範圍

Bundle 可更新：

- Texture
- Mesh
- Material
- Shader Data
- Animation
- UI Prefab
- Scene Data
- Localization
- Audio
- Terrain Chunk
- Vegetation Chunk
- JSON Config
- Binary Runtime Data

Bundle 不直接作為 Native C++ Executable / Library 的跨平台熱更新機制。

```text
Native Engine / Native Gameplay Code
→ App Update

Asset / Data
→ Bundle Hot Update
```

若未來需要 Gameplay Logic 熱更新，應另外評估 Script / VM 層，而不是依賴下載 Native C++ Code。

### Full Bundle Update

V1：

```text
Bundle Hash Changed
↓
Download Full New Bundle
```

優點：

- 架構單純
- 驗證容易
- Rollback 清楚
- Build Pipeline 容易穩定
- 跨平台一致

V1 不強制實作 Binary Delta Patch。

### Delta Patch

後續版本可加入：

```text
Old Bundle
+
Patch
↓
New Bundle
```

適合：

- 大型地圖
- 大量 Audio
- Terrain / Vegetation Data
- 長期營運遊戲

但 Patch 必須驗證：

- Source Bundle Hash
- Patch Hash
- Result Bundle Hash

若任何一項不符：

```text
Fallback
→ Full Bundle Download
```

### Bundle Download Cache

Downloaded Bundle 儲存於平台可寫目錄。

Cache Manager 管理：

- Current Cache Size
- Cache Budget
- LRU
- Last Used
- Bundle Version
- Bundle Hash
- Pin / Required Bundle
- Download-in-progress
- Previous Valid Version

已下載 Bundle：

```text
Download
→ Disk Cache
```

不代表：

```text
→ RAM
→ GPU Memory
```

三層仍保持分離：

```text
DownloadBundle
→ Network → Disk

LoadBundle
→ Bundle Metadata / Index

LoadAsset
→ Disk → RAM / GPU
```

### Streaming 與 Hot Update

Streaming Bundle 可以與熱更新共用同一套 Manifest / Cache。

```text
Player Position
↓
Streaming Cell Required
↓
Check Cache
├─ Exists + Valid
│  → Load
└─ Missing / Old
   → Download
   → Verify
   → Activate
   → Load
```

離開 Cell：

```text
Unload Asset
↓
RAM / GPU Release
↓
Bundle remains in Disk Cache
```

再次進入時若 Version / Hash 沒變，不需要重新下載。

### Content Compatibility

Remote Manifest 可以設定：

```text
MinimumAppVersion
MaximumAppVersion（需要時）
ContentSchemaVersion
```

避免新版 Bundle 使用舊 App 不認識的：

- Asset Format
- Shader Feature
- Serialization Schema
- Component Type
- Runtime Opcode / Script Data

若不相容：

```text
Do Not Activate Content
→ Request App Update
```

### 安全與完整性

V1 至少：

- Cryptographic Hash Verification
- HTTPS Download
- Atomic File Commit
- Manifest Validation
- Bundle Header Validation
- Rollback

後續商業營運版本可評估：

- Manifest Signature
- Bundle Signature
- Encryption（只作內容保護，不視為完整防破解方案）

### V1 Definition of Done

Bundle Hot Update V1 完成條件：

- Build 能產生 Remote Manifest
- Runtime 能取得 Remote Manifest
- 能比較 Local / Remote Bundle
- 能下載 Full Bundle
- 支援 Resume 或安全重新下載
- Hash 驗證
- Atomic Commit
- Cache Bundle 優先於 Built-in
- Bundle Dependency 正確解析
- 更新失敗不破壞現有版本
- Rollback 可運作
- 重啟後仍能使用已下載 Bundle
- 未變更 Bundle 不重複下載
- Download / LoadBundle / LoadAsset 行為完全分離
- Bundle Dependency Graph 必須通過 DAG Check
- Active / mapped Bundle 不直接原地替換
- 無法安全啟用的新 Bundle 可延後到下次 App Pre-init
- Active Manifest 只指向已完成驗證且可安全載入的版本
## Plugin System

Engine 預留正式 Plugin System，但 Plugin 不屬於 Remote Content。

核心規則：

```text
Plugin
→ 隨 Editor / App 安裝
或
→ Build 時一起編譯 / 打包

不經 Remote Bundle
不從 CDN 下載
不作為 Native Code Hot Update
```

因此：

```text
Bundle Hot Update
→ Asset / Data

Plugin System
→ Local Installed / Build-time Module
```

兩者完全分離。

### Plugin 類型

主要分為：

```text
Editor Plugin
Runtime Plugin
```

Editor Plugin 優先支援：

- Custom Inspector
- Asset Importer
- Asset Processor
- Build Step
- Editor Window
- Menu / Toolbar
- Scene Gizmo
- Profiler Panel
- Terrain Tool
- Animation Tool
- Third-party Content Tool
- Platform SDK Integration Tool

Runtime Plugin 可擴充：

- Physics Backend
- Audio Backend
- Rendering Feature
- File System
- Network Transport
- Gameplay Module
- Platform Integration
- Third-party Runtime SDK

### Editor Plugin

Editor Plugin 可採 Dynamic Module。

範例：

```text
Editor/
└─ Plugins/
   ├─ TerrainTools
   ├─ SpineImporter
   └─ CustomProfiler
```

Windows：

```text
.dll
```

macOS：

```text
.dylib / framework
```

Editor 啟動流程：

```text
Editor Startup
↓
PluginManager
↓
Discover Local Plugins
↓
Validate Manifest
↓
Resolve Dependencies
↓
Load Module
↓
Initialize
```

### Runtime Plugin

Runtime Plugin 預設採：

```text
Build-time selectable module
```

而不是：

```text
Runtime remote downloaded module
```

不同平台可依需求使用：

```text
Windows
→ DLL / Static Library

Android
→ .so / Static Library

macOS
→ dylib / Framework / Static Library

iOS
→ Static Library / Framework
→ 隨 App 一起簽署與提交
```

是否採 Dynamic Module，由平台能力與部署策略決定。

### Plugin API Boundary

Plugin 不允許任意依賴 Engine Private Header。

正確：

```text
Plugin
↓
Public Engine API
↓
Engine Core
```

避免：

```text
Plugin
→ Engine Private Internal Header
→ Backend-specific Internal State
```

目的：

- 降低 Plugin 與 Engine Core 耦合
- 提升 Engine 版本升級穩定性
- 保持 ABI / API 邊界清楚
- 避免第三方 Plugin 破壞 Internal Invariant
- 讓 AI Agent 產生 Plugin 時有固定 Contract

### Plugin Interface

Editor Plugin：

```cpp
class IEditorPlugin
{
public:
    virtual ~IEditorPlugin() = default;

    virtual void OnLoad(EditorContext& context) = 0;
    virtual void OnUnload() = 0;
};
```

Runtime Plugin 可使用：

```cpp
class IRuntimePlugin
{
public:
    virtual ~IRuntimePlugin() = default;

    virtual void OnLoad(RuntimeContext& context) = 0;
    virtual void OnUnload() = 0;
};
```

實際 ABI 邊界需避免直接暴露不穩定 STL Container Layout；跨 DLL / dylib Boundary 時應採穩定 C ABI 或明確 Engine ABI Contract。

### Plugin Manifest

每個 Plugin 應包含描述檔。

範例：

```json
{
  "name": "TerrainTools",
  "version": "1.0.0",
  "type": "Editor",
  "engineApi": 1,
  "entry": "TerrainTools",
  "dependencies": [],
  "platforms": [
    "Windows",
    "macOS"
  ]
}
```

至少記錄：

- Plugin Name
- Plugin Version
- Plugin Type
- Engine API Version
- Entry Module
- Dependencies
- Supported Platforms
- Enabled / Disabled
- Optional Load Order（需要時）

### PluginManager

核心職責：

```text
PluginManager
├─ Discover
├─ Read Manifest
├─ Validate Engine API Version
├─ Validate Platform
├─ Resolve Dependencies
├─ Detect Dependency Cycle
├─ Load
├─ Initialize
├─ Shutdown
└─ Unload
```

Editor 還應支援：

```text
Enable Plugin
Disable Plugin
Reload Plugin（安全時）
Show Dependency Error
Show API Version Mismatch
```

### Version Compatibility

Plugin 必須有明確：

```text
Engine API Version
Plugin Version
Dependency Version
Platform Support
```

例如：

```json
{
  "engineApi": 2,
  "dependencies": [
    "RendererCore >= 1.3"
  ]
}
```

若不相容：

```text
Do Not Load
↓
Editor / Runtime 顯示明確錯誤
```

禁止在 API 不相容時強制載入。

### Plugin 與 Bundle

必須維持以下邊界：

```text
Plugin
→ Code / Extension Module

Bundle
→ Asset / Data
```

Plugin 可以：

```text
引用 Bundle
註冊 Asset Importer
新增 Asset Type
新增 Build Processor
```

但 Plugin 本身不可包在 Remote Bundle 內下載執行。

Remote Manifest 不負責管理 Native Plugin。

### Security / Deployment

Plugin 來源：

```text
Engine Built-in
Project Local
Installed Third-party
```

不支援：

```text
Remote CDN Native Plugin
Downloaded DLL
Downloaded dylib
Downloaded .so
```

正式 Build 只打包：

- Enabled Plugins
- Target Platform Supported Plugins
- Dependency Complete Plugins

未使用 Plugin 不進最終 Runtime Package。

### V1 Scope

V1：

- Plugin Manifest
- PluginManager
- Editor Plugin
- Enable / Disable
- Dependency Resolution
- Engine API Version Validation
- Windows / macOS Editor Dynamic Module
- Runtime Build-time Plugin Module
- Plugin Build Integration

後續：

- Plugin Hot Reload（Editor only，若安全）
- Plugin SDK
- Plugin Template Generator
- Plugin Package Manager（Local / Installed only）
- Plugin Marketplace Integration（僅安裝流程，不代表 Remote Runtime Execution）

### Definition of Done

Plugin System V1 完成條件：

- Editor 能掃描本機 Plugin
- Manifest 可解析
- 能啟用 / 停用 Plugin
- Dependency Resolution 正確
- Dependency Cycle 能偵測
- Engine API Version 不符時拒絕載入
- Editor Plugin 可註冊至少一種 Editor Extension
- Runtime Plugin 可在 Build 時被選入或排除
- Plugin 不依賴 Remote Bundle 才能啟動
- Plugin 不從 CDN 動態下載
- Plugin 與 Bundle Hot Update 路徑完全分離




## Public API / Internal API / Backend Boundary

Engine 對使用者提供穩定的 Public API。

核心原則：

```text
Game / Plugin / Editor Extension
↓
Public Engine API
↓
Subsystem API
↓
Internal Implementation
↓
Platform / Backend
```

一般 Gameplay 與一般 Plugin 不需要接觸底層實作。

### Public Engine API

Public API 至少包含：

```text
Core
Scene
Asset
Rendering
Audio
Input
UI
WebView
Physics
Animation
Localization
Save
Plugin
Platform Services
```

例如：

```cpp
auto texture = Assets::Load<Texture>(assetId);
Audio::Play(eventId);
Jobs::Dispatch(...);
FileSystem::ReadAsync(...);
Time::DeltaTime();
Platform::OpenURL(url);
```

使用者不需要知道：

```text
Bundle Offset
File IO Backend
Decompression
GPU Upload
Descriptor Allocation
Resource State Transition
Native Audio Object
Platform WebView Object
Thread Scheduler Implementation
```

### Internal API

Internal API 僅供 Engine Module 使用：

```text
Entity Pool
Resource Registry
Render Graph Internals
Job Scheduler Internals
Allocator Internals
Bundle Reader
Streaming Scheduler
Asset Cooker
Shader Compiler Pipeline
```

### Backend API

Backend 只存在於平台 / RHI implementation：

```text
DX12
Vulkan
Metal
Android JNI
Objective-C++
OS Native API
```

以下 Native Type 不允許暴露到 Gameplay：

```text
ID3D12*
Vk*
MTL*
JNIEnv*
WKWebView*
FMOD Native Types
```

### Header / Folder Boundary

建議：

```text
Engine/
├─ Public/
│  ├─ Core/
│  ├─ Scene/
│  ├─ Asset/
│  ├─ Render/
│  ├─ Audio/
│  ├─ UI/
│  └─ ...
│
├─ Internal/
│  ├─ Resource/
│  ├─ RenderGraph/
│  ├─ Streaming/
│  └─ ...
│
└─ Backends/
   ├─ DX12/
   ├─ Vulkan/
   ├─ Metal/
   └─ Platform/
```

規則：

- Game Code 只能 include `Engine/Public`
- 一般 Plugin 只能依賴 Public Plugin/API Contract
- Internal Header 不安裝到 Game SDK include path
- Backend Header 不可被 Public Header include
- Public API 不應依賴 Backend Native Type
- Public API 應盡量維持穩定，Internal 可自由重構

## Feature / Module-based Build 與 Runtime Stripping

Engine 採模組化 Build 架構，並把 Optional Subsystem Stripping 視為正式 Architecture Contract：

```text
Engine Supports Feature
≠
Every Game Must Package Feature
```

核心原則：

```text
Engine Core
→ 固定存在

Subsystem / Plugin / Rendering Feature
→ 可依專案與 Build Profile 選擇是否編譯、連結與打包
```

目的：

- 降低 Runtime Package Size
- 降低 Native Library Size
- 降低 Shader Variant 數量
- 降低不必要的 Runtime Dependency
- 降低啟動與初始化成本
- 讓 2D / 輕量專案不必攜帶完整 3D Feature
- 讓大型 3D 專案仍可啟用完整功能

### Feature State

每個 Optional Feature 使用三態：

```text
Auto
Enabled
Disabled
```

語意：

```text
Auto
→ Build Pipeline 依 Project / Scene / Prefab / Asset Dependency 自動判斷

Enabled
→ 強制編譯 / 連結 / 打包

Disabled
→ 強制排除
→ 若 Build Scanner 偵測到實際依賴，Build Fail
```

此模式比單純 Checkbox 更安全，避免動態使用或隱性依賴被錯誤裁切。

### Compile-time Module Stripping

可完全不編譯進 Runtime 的 Feature：

```text
Spine Plugin
FMOD Plugin
WebView
Terrain
Vegetation
Physics / Jolt
Audio
Localization
Save / Persistent
Networking Transport
Remote Bundle / Hot Update
Streaming
Navigation / Pathfinding
Particle / VFX
Skeletal Animation
GPU Skinning
Post Processing
Shadow System
Hi-Z / Occlusion
GPU Driven Rendering
Runtime Profiler
Debug Renderer
Third-party SDK Plugins
```

CMake 範例：

```cmake
if(ENGINE_ENABLE_SPINE)
    add_subdirectory(Plugins/Spine)
endif()

if(ENGINE_ENABLE_FMOD)
    add_subdirectory(Plugins/FMOD)
endif()

if(ENGINE_ENABLE_WEBVIEW)
    add_subdirectory(Runtime/WebView)
endif()

if(ENGINE_ENABLE_TERRAIN)
    add_subdirectory(Runtime/Terrain)
endif()
```

完全 Disabled 時：

```text
Do Not Compile
↓
Do Not Link
↓
Do Not Package
↓
No Runtime Initialization
```

### Build-time Dependency Stripping

即使 Engine Source 支援某 Feature，只要專案沒有實際依賴，也可以裁切 Runtime Data。

例如：

```text
Terrain System Available
+
Project Has No Terrain
↓
No Terrain Runtime Asset
No Terrain Shader Variant
No Terrain Material Data
```

Spine：

```text
Spine Plugin Installed
+
No Spine Asset
↓
Strip Spine Runtime
```

FMOD：

```text
FMOD Plugin Installed
+
Project Uses MiniAudio
↓
Do Not Package FMOD Runtime / Library / Bank
```

### Feature Usage Scanner

Build Pipeline 必須提供：

```text
FeatureUsageScanner
```

掃描來源：

```text
Scene
Prefab
Asset Dependency
Component Type
Material
Shader Feature
Build Settings
Plugin Dependency
Serialized Runtime Reference
```

輸出範例：

```text
Detected Feature Usage

Spine             Not Used
FMOD              Not Used
Terrain           Used
Vegetation        Used
WebView           Not Used
SkeletalAnimation Used
Physics           Used
```

Build Report 必須記錄每個 Feature：

```text
State
Detected Usage
Included / Stripped
Reason
Package Size Contribution（可取得時）
```

### Dynamic Code Dependency

Auto Detect 不能假設 Static Asset Scan 能涵蓋所有情況。

例如：

```cpp
WebView::Create(...);
```

或：

```cpp
PluginManager::Load(...);
```

可能由 Gameplay Code 動態觸發。

因此：

- Dynamic-only Feature 建議明確設為 `Enabled`
- Code Generation / Reflection 可提供 Feature Dependency Metadata
- Feature 被設為 `Disabled` 但偵測到依賴時必須 Build Fail
- 不允許靜默裁切後在 Runtime 才崩潰

### Build Settings

建議：

```text
Features
────────────────────────────

Rendering
Forward+             Enabled
Shadows              Auto
Post Processing      Auto
GPU Driven           Auto
Terrain              Auto
Vegetation           Auto
Hi-Z Occlusion       Auto

Animation
Skeletal Animation   Auto
Spine                Auto

Physics
Jolt Physics         Auto

Audio
Audio                Enabled
Backend              MiniAudio
FMOD                 Disabled

Platform Services
WebView              Auto
Networking           Auto
Localization         Auto

Content
Bundle System        Enabled
Remote Content       Auto
Asset Streaming      Auto

Diagnostics
Runtime Profiler     Disabled
Debug Renderer       Disabled

Optimization
Strip Unused Features        Enabled
Strip Unused Plugins         Enabled
Strip Unused Shader Variants Enabled
```

### Plugin Stripping

Optional Plugin 預設不進 Runtime Package。

只有：

```text
Enabled
+
Target Platform Compatible
+
Required
```

才加入最終 Build。

例如：

```text
Spine Plugin Disabled
↓
No Spine Runtime
No Spine Native / Third-party Dependency
No Spine Asset Import Runtime
No Spine Shader Variant
```

Editor-only Plugin 永遠不進 Game Runtime Package，除非它同時具有獨立 Runtime Module 且該 Runtime Module 被啟用。

### Rendering Feature Stripping

Build Pipeline 可依 Feature Usage 移除：

```text
Shadow Pass
Post-process Pass
Skinning Pipeline
Terrain Pipeline
Vegetation Pipeline
GPU Driven Pipeline
Occlusion Pipeline
Unused Material Feature
Unused Shader Variant
```

Shader Variant Key 與 Feature Stripping 必須整合。

例如：

```text
SkeletalAnimation = Disabled
↓
No Skinning Feature Bit
↓
No Skinning Shader Variants
```

```text
Shadows = Disabled
↓
No Shadow Pass
No Shadow Receiver Variant
No Shadow Caster Variant
```

### Build Profile

不同 Build Profile 可以使用不同 Feature Set。

例如：

```text
Windows Full
→ Terrain
→ Vegetation
→ WebView
→ High Quality Post
→ GPU Driven

Android Lite
→ Reduced Post
→ Optional Vegetation
→ No Runtime Profiler

Internal QA
→ Runtime Profiler
→ Debug Renderer
→ Validation Features

Release
→ Strip Debug / Development Features
```

Feature State 屬於：

```text
Project Default
+
Build Profile Override
```

### Core Modules

以下屬於 Engine Foundation，不應由一般專案任意裁掉：

```text
Core Types
Memory / Allocator Foundation
Job System Foundation
File System
Logging / Error Foundation
Platform Foundation
Asset Runtime Foundation
Resource Manager
RHI Core
Render Graph Core
Basic Renderer Path
Scene / Entity Identity Foundation
Serialization Foundation
```

可以針對平台最佳化實作，但不能讓一般 Feature Strip 破壞核心 Contract。

### Diagnostics Stripping

Development / QA：

```text
Profiler
Validation Layer
Debug Renderer
Render Debug Views
Verbose Logging
Capture Metadata
```

Release 可以裁切大部分 Development-only 功能。

但：

```text
Crash Handling
Fatal Error Reporting
Essential Logging
```

仍應保留最低限度 Runtime 支援。

### Package Report

每次 Build 建議輸出：

```text
Build Feature Report

Included:
- Terrain
- Vegetation
- MiniAudio
- Localization

Stripped:
- Spine
- FMOD
- WebView
- Runtime Profiler

Shader Variants:
Before Strip: 18,420
After Strip:   4,315

Optional Module Size:
...
```

讓開發者能知道「為什麼某功能進了包」以及「裁切實際省了多少」。

### CI Validation

CI 至少驗證：

```text
Full Feature Build
Minimal Feature Build
Representative Mobile Build
Representative Desktop Build
```

Minimal Build 用於確認 Optional Module 沒有偷偷形成硬依賴。

此矩陣必須同步納入「四十五、自動化測試 / CI」的實際 Pipeline；不能只存在於 Build System 規格中。

執行層級：

```text
PR
→ Representative Profile + dependency failure checks

Nightly / Scheduled
→ Minimal Profile + Max / Full Feature Profile

Release
→ Shipping / Platform Feature Matrix
```

另需測試：

```text
Feature = Disabled + Asset Dependency Exists
→ Build Must Fail

Plugin = Disabled + Runtime Reference Exists
→ Build Must Fail
```

### V1 Scope

V1 必須完成：

- Feature State：Auto / Enabled / Disabled
- CMake Optional Modules
- Plugin Stripping
- Feature Usage Scanner
- Build Dependency Validation
- Shader Variant Stripping Integration
- Editor Build Settings UI
- Build Feature Report
- Minimal / Full Build CI

V1 不需要做到所有 Subsystem 都可裁切，但架構上所有非 Core 系統都必須避免形成不必要硬依賴。

### Definition of Done

完成條件：

- 未使用的 Optional Plugin 不進 Runtime Package
- 未使用的 Optional Native Library 不被 Link
- 未使用 Feature 不產生相關 Runtime Asset
- 未使用 Rendering Feature 不產生相關 Shader Variant
- `Disabled` Feature 被引用時 Build Fail
- `Auto` 能掃描主要 Scene / Prefab / Asset Dependency
- Build Profile 可 Override Feature State
- Build Report 能列出 Included / Stripped / Reason
- Minimal Build 可成功編譯與啟動
- Full Feature Build 可成功編譯與啟動


## Optional Runtime Subsystem Architecture

除 Foundation Core 外，所有 High-level Subsystem 原則上必須設計成 Optional Runtime Module。

核心規則：

```text
Subsystem Supported By Engine
≠
Subsystem Must Exist In Final Game Package
```

未使用的 Subsystem：

```text
Do Not Compile
↓
Do Not Link
↓
Do Not Initialize
↓
Do Not Package Native Library
↓
Do Not Cook Related Assets
↓
Do Not Generate Related Shader Variants
```

### Foundation Core

Foundation Core 為 Runtime 骨架，不由一般專案任意裁切：

```text
Core Types
Memory / Allocator Foundation
File System
Logging / Error Foundation
Job System Foundation
Platform Foundation
Asset Runtime Foundation
Resource Manager
Entity Identity Foundation
Serialization Foundation
RHI Core
Render Graph Core
Basic Renderer Path
```

### Optional High-level Subsystem

以下原則上皆為 Optional Runtime Module：

```text
Physics / Jolt
Audio
FMOD
Animation
Spine
Terrain
Vegetation
WebView
Networking
Localization
Save / Persistent
Remote Content
Streaming
Navigation
Particle / VFX
Skeletal Animation
GPU Skinning
Post Processing
Shadow System
Hi-Z / Occlusion
GPU Driven Rendering
Runtime Profiler
Debug Renderer
Third-party SDK Integrations
```

### API Exists, Implementation Optional

Public API 可以存在於 Engine SDK，但 Runtime Implementation 只有需要時才加入。

例如：

```text
Engine SDK
├─ Physics API
├─ Audio API
├─ WebView API
├─ Terrain API
└─ Networking API
```

某個 Game Build：

```text
Included:
Core
Renderer
UI
MiniAudio
Localization

Stripped:
Terrain
Vegetation
Jolt
Spine
FMOD
WebView
Networking
Runtime Profiler
```

### Dependency Rule

Optional Subsystem 不得形成不必要的硬依賴。

避免：

```text
Scene
→ Jolt Physics
```

應為：

```text
Scene
→ Physics Public Contract

PhysicsModule
→ Optional Implementation
```

同樣：

```text
AudioManager
→ IAudioBackend

MiniAudioBackend
FMODBackend
→ Optional Implementations
```

### Third-party Dependency Stripping

第三方 Library 只有 Subsystem 啟用時才進 Build。

例如：

```text
Spine Disabled
→ No Spine Runtime

FMOD Disabled
→ No FMOD DLL / SO / Framework

Physics Disabled
→ No Jolt

WebView Disabled
→ No WebView Platform Integration Module

Networking Disabled
→ No Network Transport Runtime
```

不得因為 Engine Source Tree 支援第三方 SDK，就自動進所有遊戲包。

### CMake Module Graph

Build System 應以 Target / Module 為單位。

例如：

```text
EngineCore
RendererCore
RuntimeUI
PhysicsJolt
AudioMini
AudioFMOD
SpineRuntime
TerrainRuntime
VegetationRuntime
WebViewRuntime
NetworkingRuntime
```

CMake 依 Feature Resolution 選擇：

```cmake
if(ENGINE_ENABLE_PHYSICS)
    target_link_libraries(GameRuntime PRIVATE PhysicsJolt)
endif()

if(ENGINE_ENABLE_WEBVIEW)
    target_link_libraries(GameRuntime PRIVATE WebViewRuntime)
endif()
```

Disabled Module 不應只靠 runtime `if` 關閉，而是應盡可能從 Build Graph 移除。

### Final Build Pipeline

```text
Project
↓
Feature Usage Scan
↓
Build Profile
↓
Resolve Required Subsystems
↓
Resolve Plugin Dependencies
↓
Generate CMake Module Graph
↓
Compile Required Modules Only
↓
Link Required Libraries Only
↓
Cook Required Assets Only
↓
Generate Required Shader Variants Only
↓
Copy Required Runtime Libraries Only
↓
Final Package
```

### Build Size Optimization

Build Report 應能列出：

```text
Module
Included / Stripped
Reason
Native Binary Size
Third-party Library Size
Asset Size
Shader Variant Count
```

目標是能回答：

```text
Why is this module in the package?
How much size does it contribute?
Can it be stripped?
```

### Hard Architecture Rule

正式規範：

> 除 Foundation Core 外，所有 High-level Subsystem 必須避免形成不可拆除的隱性 Runtime Dependency；若專案未使用，應能從 compile / link / package / asset / shader 五個層面裁切。

此規則屬於 Architecture Contract，不只是 Build Optimization。


## Modular Development Build / Monolithic Shipping Build

Engine Build System 採雙模式：

```text
Development / Editor
→ Modular Build

Shipping / Release
→ Modular 或 Monolithic
```

正式原則：

> 開發期優先縮短 iteration time；發行期保留最佳化與部署彈性。

### Core Library

Engine Core 應以預編譯 Library 形式存在。

建議：

```text
EngineCore
RendererCore
PlatformCore
```

Windows 可為：

```text
EngineCore.lib
RendererCore.lib
```

macOS / iOS / Android 依平台使用：

```text
Static Library
Framework
Archive
```

Core 特性：

- 高頻使用
- 架構穩定
- 低階 Foundation
- 不適合頻繁跨 Dynamic Library Boundary
- 可被多個 Subsystem 共用

### Dynamic Subsystem

大型、獨立、低耦合 Subsystem 可在 Development Build 以 Dynamic Module 存在。

Windows：

```text
Physics.dll
Audio.dll
WebView.dll
Spine.dll
FMOD.dll
Terrain.dll
Vegetation.dll
Networking.dll
```

macOS：

```text
.dylib / Framework
```

Android：

```text
.so
```

iOS：

```text
以平台允許的 Static Library / Framework 為主
```

### Development Build

Development / Editor 預設：

```text
ENGINE_MONOLITHIC_BUILD = OFF
```

行為：

```text
Game Code Changed
↓
Recompile Game Module
↓
Relink Game
↓
Reuse Prebuilt Engine Core / Subsystem Modules
```

若 Subsystem Public ABI 未變：

```text
Physics
Audio
WebView
Terrain
...
```

不需要因 Gameplay 修改而重新編譯。

目的：

- 降低全量 Build 次數
- 加快 Play / Iteration
- 模組可獨立編譯
- Module Cache 可重用
- Editor Plugin / Runtime Plugin 可各自更新

### Shipping Build

Release / Shipping 可選：

```text
ENGINE_MONOLITHIC_BUILD = ON
```

Monolithic：

```text
Game Runtime
+
Required Subsystems
↓
Single / Reduced Binary Set
```

優點：

- Linker 可做更完整最佳化
- 減少 Dynamic Library 數量
- 簡化部署
- 降低 Loader / Symbol 管理複雜度
- 更容易做 Whole Program Optimization / LTO

但 Shipping 不強制一定 Monolithic。

Build Profile 可選：

```text
Runtime Link Mode
[ Modular ▼ ]

Options:
- Modular
- Monolithic
```

### Suggested Module Classification

適合 Dynamic Module：

```text
Physics
Audio
FMOD
Spine
WebView
Networking
Editor Plugin
Third-party SDK
Large Optional Tooling Runtime
```

通常不建議切成大量 DLL：

```text
Entity Foundation
Resource Manager
Render Graph Core
Renderer Hot Path
Job System Core
Memory Allocator
RHI Core
```

原因：

- 呼叫頻率高
- ABI boundary 成本
- 共享狀態多
- Memory Ownership 複雜
- 過度切分會增加維護與部署成本

### ABI Boundary

Dynamic Subsystem 必須有明確 ABI Contract。

禁止任意跨 DLL / dylib / so 傳遞：

```text
不穩定 STL Container Layout
Backend Native Object
Allocator-private Object
Internal Concrete Class
```

優先：

```text
C ABI
Opaque Handle
Plain Struct
Versioned Interface
Engine-owned Buffer/View
```

例如：

```cpp
struct PhysicsApiV1
{
    PhysicsWorldHandle (*CreateWorld)(const PhysicsWorldDesc*);
    void (*DestroyWorld)(PhysicsWorldHandle);
};
```

或使用穩定的 Engine Interface Contract。

### Memory Ownership Across Module Boundary

跨 Dynamic Module：

```text
Allocate In Module A
→ Free In Module A
```

避免：

```text
Allocate In DLL
→ Free In EXE
```

除非明確使用同一 Engine Allocator Contract。

所有跨 Module 資源優先使用：

```text
Handle
Span / View
Engine Allocator API
```

### Module Versioning

每個 Dynamic Subsystem 應有：

```text
Module Name
Module Version
Engine API Version
ABI Version
Build Configuration
Target Platform
```

Load 時驗證：

```text
Engine ABI
vs
Subsystem ABI
```

不相容：

```text
Do Not Load
→ Report Clear Error
```

### Feature Stripping Integration

Modular Build 必須與 Feature / Subsystem Stripping 整合。

例如：

```text
WebView = Disabled
↓
Do Not Build WebView Module
Do Not Link WebView
Do Not Copy WebView DLL
```

```text
FMOD = Disabled
↓
Do Not Build FMOD Module
Do Not Deploy FMOD Runtime
```

因此：

```text
Optional Subsystem
+
Modular Build
+
Feature Stripping
```

共用同一套 dependency graph。

### CMake

建議：

```cmake
option(ENGINE_MONOLITHIC_BUILD
       "Link optional runtime modules into main runtime"
       OFF)
```

概念：

```cmake
if(ENGINE_MONOLITHIC_BUILD)
    add_library(PhysicsModule STATIC ...)
else()
    add_library(PhysicsModule SHARED ...)
endif()
```

實際 target type 仍依平台與 subsystem 特性決定。

### Build Cache

穩定 Subsystem 應允許：

```text
Prebuilt Binary Cache
Compiler Cache
CI Artifact Cache
```

Cache Key 至少包含：

```text
Source Revision
Compiler Version
Compiler Flags
Target Platform
Architecture
Build Configuration
Public ABI Version
Feature Defines
```

避免使用錯誤 Binary Cache。

### Editor

Editor 本身優先採 Modular：

```text
Editor.exe
├─ EngineCore
├─ Renderer
├─ Asset Tools
├─ UI Editor
└─ Optional Plugins / Subsystems
```

這有利於：

- 快速開發 Editor Tool
- Plugin Reload
- 降低單次 Link 時間
- 各工具模組獨立測試

### CI

CI 至少驗證：

```text
Modular Development Build
Monolithic Shipping Build
Minimal Modular Build
Full-feature Modular Build
Representative Shipping Build
```

並檢查：

- Module dependency graph
- ABI version
- Missing DLL / dylib / so
- Incorrect runtime library deployment
- Disabled Subsystem 不應出現在 package
- Monolithic Build 不應殘留不必要 Dynamic Runtime Module

### V1 Scope

V1：

- Prebuilt Core Libraries
- CMake Module Graph
- Dynamic Subsystem on supported desktop platforms
- Module ABI Version
- Feature Stripping integration
- `ENGINE_MONOLITHIC_BUILD`
- Modular Development Build
- Monolithic Shipping Build
- Build Cache foundation

平台策略：

```text
Windows
→ 優先完整驗證 DLL Modular Build

macOS
→ dylib / Framework 視部署需求

Android
→ .so Module where appropriate

iOS
→ 以 Static / Framework deployment 為主
```

### Definition of Done

完成條件：

- 修改 Gameplay 不會無條件重編所有 Subsystem
- 未修改且 ABI 相容的 Subsystem Binary 可直接重用
- Optional Subsystem Disabled 時不產生其 Runtime Module
- Modular Development Build 可成功執行
- Monolithic Shipping Build 可成功執行
- ABI 不相容時拒絕載入
- 跨 Module Memory Ownership 規則明確
- Build Report 能列出每個 Runtime Module 的來源與大小

## Cross-platform In-Game WebView

Engine 內建跨平台 Runtime WebView System。

WebView 是通用 In-Game Platform Service，不綁定 Login / OAuth。

主要用途：

- 遊戲公告
- 活動頁
- 客服中心
- FAQ
- 隱私權政策
- 使用條款
- 社群 / 活動頁
- HTML-based Tool / Content
- 帳號相關頁面
- 其他 In-Game Web Content

Auth Plugin 若需要 Browser / Web-based Flow，可使用對應平台允許的 Authentication Flow，但一般 WebView 與 Authentication System 必須保持解耦。

### Architecture

```text
Runtime UI / UIDocument
    │
    ├─ Image
    ├─ Label
    ├─ Button
    └─ WebViewElement
           │
           ▼
      WebViewManager / WebView Service
           │
           ▼
      Platform Backend
      ├─ Windows → Microsoft WebView2
      ├─ Android → android.webkit.WebView
      ├─ iOS     → WKWebView
      └─ macOS   → WKWebView
```

### Runtime API

建議：

```cpp
struct WebViewDesc
{
    Rect rect;
    bool visible = true;
    bool enableJavaScript = true;
};

WebViewHandle webView = WebView::Create(desc);

webView->LoadUrl("https://example.com/event");
webView->Show();
webView->Hide();
webView->Reload();
webView->GoBack();
webView->GoForward();
webView->SetRect(rect);
webView->Close();
```

### JavaScript Bridge

WebView 必須支援 JS ↔ Engine 雙向溝通。

Web → Engine：

```javascript
Engine.postMessage("claim_reward");
```

Engine：

```cpp
webView->OnMessage([](StringView message)
{
    if (message == "claim_reward")
    {
        // Game logic
    }
});
```

Engine → Web：

```cpp
webView->EvaluateJavaScript(
    "window.updatePlayerLevel(25);"
);
```

資料流：

```text
C++ Game
↕
WebView Bridge
↕
JavaScript / HTML
```

Message Bridge 應支援：

- String Message
- JSON Message
- Request / Response ID（後續）
- Error Callback
- Domain / Origin Validation

### Coordinate System

WebView API 不直接綁死某平台 Physical Pixel。

至少支援：

```text
UI Logical Coordinates
Normalized Coordinates
```

可選支援：

```text
Physical Pixel Coordinates
```

Platform Backend 必須處理：

- Windows DPI Scaling
- macOS Retina Scale
- Android Density
- iOS Display Scale
- Safe Area
- Orientation
- Window Resize
- Fullscreen
- Multi-window（後續）

WebView Rect 必須可與 Runtime UI RectTransform / Anchor System 對齊。

重要：V1 的 `WebViewElement` 是 **UIElement Tree 中的 Native Overlay Proxy**，不是一般 `UIRenderItem` / Canvas Renderable。

```text
RectTransform / Anchor
→ 計算 WebView Screen-space Rect
→ Platform Backend
→ Native View Bounds
```

它不會產生一般 Runtime UI Mesh，也不會進入 UI Batch / Draw Call / Render Graph Pass。

### Native Overlay Strategy

V1 採：

```text
Native Overlay WebView
```

概念：

```text
Game Rendering Surface
+
Native WebView Layer
```

各平台：

```text
Windows
→ Game Window + WebView2

Android
→ Game Surface + Android WebView

iOS
→ Metal View + WKWebView

macOS
→ Metal View + WKWebView
```

### Native Overlay 與 Canvas 行為邊界

V1 Native Overlay 由 OS / Platform UI compositor 合成，而不是由 Engine Renderer 繪製。

因此 WebView 雖可掛在 Canvas / UI Hierarchy 中方便 Layout 與生命週期管理，但 **不可假設它與 Image / Text / Button 有完全相同的 Render 行為**。

V1 明確限制：

```text
Supported / Integrated
✓ RectTransform Position / Size
✓ Anchor
✓ Safe Area
✓ Show / Hide
✓ Screen-space Layout
✓ Window Resize / Orientation
✓ Native Overlay 間的有限 z-order 管理

Not Canvas-rendered
✗ UI Batch
✗ Canvas Draw Call
✗ Render Graph UI Pass
✗ Stencil Mask / Canvas Mask
✗ 3D Depth Test
✗ 與 3D Object 交錯深度排序
✗ 與一般 Canvas Element 任意交錯排序
✗ RectTransform Rotation
✗ Skew / Perspective Transform
✗ 非矩形 Clip
✗ Canvas Shader / Material Effect
```

z-order 原則：

```text
Game / Canvas Render Surface
↓
Native Overlay Layer
↓
WebView
```

V1 不保證：

```text
Canvas Image
↓
WebView
↓
Canvas Text
```

這類「WebView 夾在兩個 Canvas draw element 中間」的任意交錯排序。

如需完整 Canvas Mask、Transform、Material、Depth 與排序行為，必須使用未來的：

```text
Offscreen WebView
→ Render Texture
→ Canvas / 3D Renderer
```

Runtime UI Profiler 的 Batch / Draw Call / Overdraw 統計 **不把 Native WebView 當成 Canvas Draw Call**；WebView 應另列 Native Overlay count / visible area / platform cost 等診斷資訊。

V1 不強制實作：

```text
WebView
↓
Render To Texture
↓
GPU Texture
↓
3D / UI Quad
```

原因：

- Offscreen Rendering 跨平台差異大
- Keyboard / IME 複雜
- Video Playback 複雜
- Touch / Mouse Input Routing 複雜
- Hardware Acceleration 行為不同
- WebView → Texture 同步成本高

### Future: Offscreen WebView

後續可評估：

```text
Offscreen WebView
→ Render Texture
→ Runtime UI Texture
→ 3D World Screen
```

使用案例：

- 3D 世界中的電腦螢幕
- 遊戲內瀏覽器材質
- 特殊 HUD
- VR / AR Surface

此功能不列為 V1 必要條件。

### Runtime UI WebViewElement

WebView 正式以 `WebViewElement` 身分存在於 UIDocument / UIElement Tree 中，用於共用 Layout、Anchor、Safe Area、Visibility 與生命週期管理：

```text
UIDocument / UIElement Tree
├─ Image              → UIRenderItem
├─ Label              → UIRenderItem
├─ Button             → UIRenderItem
└─ WebViewElement     → Native Overlay Proxy
```

正式 Contract：

```text
WebViewElement
= UIElement

Native WebView
≠ UIRenderItem
≠ UI Batch Item
≠ RenderGraph UI Pass
```

`WebViewElement` 在 V1 應明確標示：

```text
Rendering Mode: Native Overlay
```

Editor Inspector 應提示 Native Overlay 限制，避免設計者誤認為它支援一般 Canvas Mask / Sort / Transform。

Inspector 建議提供：

```text
URL
Visible
Enable JavaScript
Allow Back Navigation
User Agent
Allowed Domains
RectTransform
Anchor
Safe Area Mode
Rendering Mode: Native Overlay (V1)
Native Overlay Z Order
Input Hit-Test Mode
Viewport Sync Mode
```

以下一般 Canvas Inspector 能力對 V1 WebView 應 Disabled / Hidden 或顯示 Unsupported：

```text
Stencil / Mask
Canvas Material
Rotation / Skew / Perspective
3D Depth
Canvas Batch / Sorting Interleave
```

### Native Overlay Input / Viewport Sync

V1 `WebViewElement` 必須提供：

```text
Input Hit-Test Mode
├─ Block
└─ Pass-Through
```

語意：

```text
Block
→ WebView Native View 接收其 bounds 內的 pointer / touch input
→ Engine UI / Game 不接收同一次命中事件

Pass-Through
→ WebView 不攔截一般 pointer / touch input
→ 事件交由 Engine UI / Game
```

平台實作必須明確處理：

- Touch
- Mouse
- Pointer Capture
- Focus
- Keyboard / IME Focus
- Visibility Change
- View Destroy / Recreate

`Pass-Through` 的精確能力依平台 Native View API 實作，但 Public API 行為必須一致；若某平台無法完整支援特定交互模式，必須在 Capability Query / Editor Validation 中明確回報，不可靜默表現不同。

### Viewport Sync Logic

Native Overlay 僅跟隨 RectTransform 計算後的 **Screen-space Absolute Bounding Box**：

```text
RectTransform / Anchor / Safe Area
↓
Canvas Layout Resolve
↓
Screen-space Absolute Bounding Box
↓
DPI / Retina / Density Conversion
↓
Native View Bounds
```

同步事件至少包含：

- Window Resize
- Orientation Change
- Safe Area Change
- DPI / Display Scale Change
- Canvas Scale Change
- Fullscreen / Windowed Change
- UI Layout Dirty
- WebView Show / Hide

V1 WebView 不接受會要求非軸對齊矩形的 Canvas Transform。

Editor 對 V1 WebView 必須禁止或 Disabled：

```text
Mask / Stencil
Alpha Gradient / Canvas Material Fade
Rotation
Skew
Perspective
3D Depth Sorting
Arbitrary Canvas Interleave
Non-rectangular Clip
```

若需要淡入淡出，V1 只能使用平台 Native View 明確支援且跨平台一致的整體 visibility / opacity 能力；不得把一般 Canvas Vertex Alpha 語意套用到 Native Overlay。

### Modal / Fullscreen Engine UI Interaction

Native Overlay WebView 通常位於 Engine render surface 之上，因此全螢幕 Engine Modal / Pause UI 顯示時必須有明確 policy。

V1 保底策略：

```text
Show Fullscreen Engine Modal
↓
WebViewService SuspendOverlay()
↓
Native WebView Hide()
↓
Release / Transfer Input Focus
↓
Engine Modal Receives Input
```

Modal 關閉後：

```text
Restore WebView visibility if previously visible
↓
Re-sync viewport
↓
Restore focus only when explicitly requested
```

不把 `SetOpacity(0)` 視為所有平台都可靠的 V1 行為；若 backend 提供一致且驗證過的 native opacity，才可作為額外能力。

Editor 必須對「Canvas element 疊在 Native WebView 上方」顯示警告，避免設計者誤認為一般 Canvas z-order 可以覆蓋 Native Overlay。

### WebView Render Mode

Public API 預留：

```cpp
enum class WebViewRenderMode
{
    NativeOverlay,
    OffscreenTexture
};
```

V1：

```text
NativeOverlay
→ Supported
```

Future：

```text
OffscreenTexture
→ Optional Backend Capability
```

V1 Native Overlay：

```text
ScreenSpace
→ Supported

WorldAnchored
→ 可投影成 Screen-space Rect

True WorldSpace
→ 不直接支援 Native Overlay
```

真正 3D Web Content 需未來：

```text
Offscreen WebView
↓
GPU Texture
↓
WorldSpace UI / Mesh
```

### WebView Local / Remote Content

支援：

```text
LoadURL
LoadLocalAsset
Reload
Back
Forward
Stop
```

Local HTML / CSS / JS / Image 可作為一般 Asset / Bundle Content。

例如：

```text
ui/web/help/index.html
```

可用於：

- Help
- News
- Terms
- Patch Notes
- Event Page
- Complex formatted content

核心 Gameplay UI 不依賴 Remote Web Content 才能工作。

### WebView Storage / Cache Policy

WebView Browser Storage 與 Engine Asset Cache / AudioResidencyScope 分離。

正式：

```text
WebViewCachePolicy
├─ Default
├─ NoCache
├─ Session
└─ Persistent
```

以及：

```text
WebViewStorageProfile
├─ Shared
├─ Isolated
└─ Ephemeral
```

管理範圍包括：

- HTTP Cache
- Cookie
- Local Storage
- Web Storage

Logout / Account switching 必須能清除或切換對應 Storage Profile。

### WebView Trust / Bridge Policy

正式：

```text
WebViewTrustLevel
├─ LocalTrusted
├─ TrustedOrigin
└─ UntrustedRemote
```

Bridge 權限由：

```text
Trust Level
+
Allowed Origins
+
Allowed Message Types
```

共同決定。

`UntrustedRemote` 預設：

```text
Engine Bridge Disabled
```

### JSON Message Bridge

JS ↔ Engine Bridge 使用 Message-based contract，資料格式優先使用 Engine JSON Framework。

例如 Web → Engine：

```json
{
  "type": "shop.buy",
  "requestId": 123,
  "payload": {
    "itemId": 10001
  }
}
```

Engine → Web：

```json
{
  "requestId": 123,
  "ok": true
}
```

正式原則：

```text
Web Content
→ Untrusted Input

Bridge
→ Explicit Message Interface

Sensitive Action
→ Engine-side Revalidation
```

禁止 Web Content 取得：

- Native Pointer
- C++ Function Address
- Arbitrary Engine API
- Private Backend Object

### WebView Gesture Ownership

WebView 與 Runtime UI / Game Input 必須使用 Single-owner policy。

```text
Pointer inside active WebView bounds
→ WebView owns gesture

Pointer outside
→ Engine UI / Game owns gesture
```

不得讓同一個：

- Drag
- Scroll
- Pinch
- Long Press

同時被 WebView 與 Engine ScrollView 消費。

不建議把可自行 Scroll 的 WebView 放入另一個 Engine ScrollView 中並期待雙方同時處理相同 gesture。

### WebView Editor Integration

Editor Hierarchy 中 `WebViewElement` 與其他 UIElement 一起顯示。

Scene View 預設可使用：

```text
WebView Placeholder
```

例如顯示：

```text
WebView
URL: https://example.com
Rendering: Native Overlay
```

需要時才啟動：

```text
Preview WebView
```

避免 Editor Scene View 無條件建立大量 Platform Native WebView。


### Navigation / Security

WebView 必須提供：

- Navigation Started
- Navigation Completed
- Navigation Failed
- URL Changed
- Close Requested
- External Link Request

可設定：

```text
Allowed Domains
Blocked Domains
Open External Browser Rules
JavaScript Enabled / Disabled
Local File Access
Mixed Content Policy
```

重要：

- 不允許任意 Web Content 直接取得 Engine Private API
- JS Bridge 必須是明確註冊的 Message Interface
- Sensitive Action 必須由 Engine 端再次驗證
- WebView Content 不視為可信輸入

### WebView 與 Authentication

架構保持分離：

```text
WebView System
→ 通用 In-Game Web Content

Authentication Provider System
→ Google / Apple / Future Providers
```

OAuth / Sign-In 流程由 Auth Provider 使用平台允許的登入方式。

不可因為 Engine 有 WebView，就強制所有 Authentication 都使用一般 Embedded WebView。

### Plugin Integration

Plugin 可以使用 WebView Service：

```text
Editor / Runtime Plugin
↓
Public WebView API
↓
Platform WebView Backend
```

例如：

- Customer Support Plugin
- News / Event Plugin
- Account Plugin
- Third-party SDK UI

Plugin 不直接操作：

```text
WKWebView
Android WebView
WebView2 Native Object
```

Platform Native WebView 仍由 Engine Backend 管理。

### Lifetime

WebView 使用：

```text
WebViewHandle
```

由 WebViewManager 管理實際 Native Object Lifetime。

```text
Create
↓
WebViewHandle
↓
WebViewManager
↓
Platform Native WebView
```

避免 Gameplay / Plugin 直接保存 Backend Native Pointer。

### Threading

- WebView UI 操作依平台規則切回 Platform UI Thread
- Game Thread 呼叫 WebView API 時由 Backend Dispatch
- JS Callback 不直接在任意 Platform Thread 修改 Scene
- Callback 應轉交 Event Queue / Main Thread
- Close / Destroy 必須處理正在進行的 callback

### V1 Scope

V1 必須支援：

- Windows WebView2
- Android WebView
- iOS WKWebView
- macOS WKWebView
- Load URL
- Show / Hide
- Close
- Rect / Resize
- Safe Area
- Orientation
- Back / Forward
- Reload
- JavaScript Execution
- JS → C++ Message Bridge
- Navigation Callback
- Error Callback
- External Browser
- Runtime UI `WebViewElement`

V1：

```text
Native Overlay
```

Future：

```text
Offscreen / Render-to-Texture WebView
```

### Definition of Done

WebView V1 完成條件：

- 四個目標平台使用相同 Public API
- In-Game 可建立與關閉 WebView
- Runtime UI 可控制 WebView Rect
- DPI / Density / Retina 座標正確
- Safe Area 正確
- Orientation / Window Resize 正確
- URL Load 正常
- Back / Forward / Reload 正常
- JS Execution 正常
- JS ↔ Engine Message Bridge 正常
- Native Callback 正確回到 Engine Event Queue
- External Link 可切換到 System Browser
- Platform Native Type 不暴露到 Gameplay / Plugin
- WebView System 不與 Authentication System 綁死
- Native Overlay 不參與 Canvas Batch / Render Graph UI Pass
- Editor 能明確顯示 Mask / Depth / Rotation / Arbitrary Canvas Sort 等限制
- WebViewElement 的 RectTransform 只作 Screen-space Native View Layout，不暗示完整 UIRenderItem / Canvas Render Semantics

## 二十八、Editor

技術：
- C++
- Dear ImGui
- Docking
- Multi-Viewport

主要視窗：
- Hierarchy
- Scene View
- Game View
- Inspector
- Asset Browser
- Console
- Profiler
- Build Settings

需求：
- Scene View 可拖出主視窗
- Game View 可拖出主視窗
- 多螢幕工作
- Gizmo
- Grid Snap
- Rotation Snap
- Local / World
- Perspective / Orthographic


## 二十九、Editor Play Mode

架構：

```text
Edit World
↓ Play
```
```text
Runtime World Clone
↓
Play
```

Stop：
Runtime World Dispose

避免 Play Mode 修改污染原始 Scene。

後續可加入：
- Apply Runtime Changes


## 三十、Prefab

Prefab 功能：
- Prefab Asset
- Prefab Instance
- Override Tracking
- Nested Prefab（後續）
- Apply
- Revert

Inspector：

Overrides
- Transform.Position
- Material
- Script Property

功能：
- Apply Selected
- Revert Selected
- Apply All
- Revert All


## 三十一、Undo / Redo

V1 必做。

Command Pattern：

ICommand
- Execute
- Undo

涵蓋：
- Move
- Rotate
- Scale
- Rename
- Add / Remove Component
- Delete Node
- Property Change
- Material Change
- Prefab Change


## 三十二、Runtime UI Framework

Dear ImGui 只用於 Editor / Debug / Development Tool。

正式分層：

```text
Engine UI
├─ Runtime UI
│  └─ engine::ui
│
└─ Editor UI
   └─ engine::editor
      └─ Dear ImGui
```

核心原則：

```text
Dear ImGui
→ Editor / Debug / Development Tools

Custom Retained Mode UI
→ Player-facing Runtime UI
```

兩者可共用：

- Renderer / RHI
- Font
- Texture
- Input
- Asset System
- Localization

但不共用高階 Widget Framework。

### Runtime UI Architecture

```text
UI JSON / UIDocument Asset
↓
UIDocument
↓
UIElement Tree
├─ Widget
├─ Layout
├─ Style
├─ Binding
├─ Animation
└─ Event
↓
Layout / Text Shaping
↓
Input / Hit Test / Focus
↓
UI Render Extraction
↓
Clip / Sort / Batch
↓
UI Renderer
↓
RenderGraph
```

Runtime UI 採 Retained Mode。

### UIElement / UI Tree

`UIElement` 是 Runtime UI 自己的 Lightweight Hierarchy Node。

```text
UIDocument
└─ UIElement
   ├─ Panel
   │  ├─ Image
   │  └─ Label
   └─ Button
      └─ Label
```

正式 Contract：

```text
UIElement
≠ SceneNode
≠ EntityID
```

禁止：

```cpp
class UIElement : public SceneNode
```

UI Tree 使用獨立：

```text
UIElementID
```

概念 API：

```cpp
namespace engine::ui
{
    class UIElement
    {
    public:
        UIElementID GetID() const;
        UIElementID GetParent() const;
        UIElementID GetFirstChild() const;
        UIElementID GetNextSibling() const;
    };
}
```

Runtime 可使用 lightweight hierarchy data：

```text
UIElementPool
├─ UIElementID[]
├─ Parent[]
├─ FirstChild[]
├─ NextSibling[]
├─ Type[]
├─ Flags[]
├─ Layout[]
├─ ComputedRect[]
├─ Style[]
└─ RenderState[]
```

Widget-specific data 可拆為：

```text
UIImagePool
UILabelPool
UIButtonPool
UIScrollViewPool
...
```

也就是：

```text
Public API
→ Node-like UIElement

Runtime Storage
→ Handle + Pool / Data-Oriented
```

### Scene ↔ UI Boundary

Scene 只管理 UIDocument 的邊界，不把每個 Widget 都變成 Scene Entity。

```text
Scene Entity
└─ UIComponent
   └─ UIDocument
      └─ N UIElements
```

正式：

```text
1 Scene Entity
→ 1 UIDocument
→ N UIElements
```

禁止為 500 個 Widget 建立 500 個 Scene Entity。

`UIDocument` 是 Scene ↔ Runtime UI Tree 的正式 boundary。

### Editor Hierarchy Integration

Runtime UI 不繼承 SceneNode，但 Editor 必須能與 Scene 在同一 Hierarchy 中編輯。

Editor 使用統一：

```text
EditorObjectHandle
├─ SceneEntity
├─ UIElement
├─ Asset
└─ ...
```

以及：

```text
EditorObjectAdapter
├─ SceneEntityAdapter
├─ UIElementAdapter
└─ AssetAdapter
```

統一能力至少：

```text
GetName
GetParent
GetChildren
Rename
Delete
Duplicate
CanReparent
GetInspector
GetIcon
GetSelectionBounds
```

Editor Hierarchy 可呈現：

```text
Scene
├─ Camera
├─ Player
└─ UIRoot
   └─ UIComponent
      └─ MainHUD
         ├─ PlayerStatus
         │  ├─ HPBackground
         │  ├─ HPFill
         │  └─ PlayerName
         └─ SkillBar
            ├─ Skill01
            └─ Skill02
```

底層實際仍為：

```text
Scene Entity
      │
      └─ UIComponent
              │
              ▼
         UIDocument
              │
              ▼
         UIElement Tree
```

### Editor Scene View / Gizmo

Scene View 同時顯示：

```text
3D Scene
+
Runtime UI Preview
+
Scene Gizmo
+
UI Rect Gizmo
```

Selection policy：

```text
Scene Entity
→ Move / Rotate / Scale Gizmo

UIElement
→ Rect / Anchor / Pivot / Resize / Margin / Padding Gizmo
```

Editor 可依 Selection 自動切換 UI editing tool。

### UI Render Space

正式：

```cpp
enum class UIRenderSpace
{
    ScreenSpace,
    WorldAnchored,
    WorldSpace
};
```

三種 Render Space 共用：

```text
Widget
Layout
Style
Event
Binding
Animation
Text
```

差異只在：

```text
Coordinate Mapping
Render Extraction
Input Hit Test
Depth / Occlusion
```

### ScreenSpace UI

一般：

- HUD
- Menu
- Inventory
- Shop
- Settings
- Pause
- Chat

流程：

```text
UIDocument
↓
Layout
↓
Screen-space Geometry
↓
UI Render Pass
```

### WorldAnchored UI

適用：

- NPC Name
- Enemy HP Bar
- Quest Marker
- Damage Number
- Interaction Prompt

流程：

```text
World Position
↓
Camera Projection
↓
Screen Position
↓
Screen-space UI
```

WorldAnchored UI 優先於真正 WorldSpace geometry，除非需求確實需要 3D surface。

### WorldSpace UI

真正存在 3D 世界中的 UI：

- Cockpit Display
- World Terminal
- Elevator Panel
- VR / AR Panel
- In-world Computer
- 3D Shop Display

流程：

```text
UIDocument
↓
UIElement Tree
↓
UILayout
↓
UI Geometry
↓
UIDocument World Transform
↓
View / Projection
↓
3D Scene
```

Scene：

```text
Computer Entity
└─ UIComponent
   └─ UIDocument
      ├─ Label
      ├─ Button
      └─ Slider
```

只有 UIDocument 邊界持有 Scene World Transform；內部 Widget 仍為 UIElement。

### WorldSpace Coordinate Mapping

```text
Logical UI Space
↓
UILayout
↓
ComputedRect
↓
UI Local Transform
↓
UIDocument World Scale
↓
World Transform
```

例如：

```text
Design Size
1920 × 1080

World Size
2.0m × 1.125m
```

Layout 仍使用 Logical UI Unit，不將每個 Widget 改造成一般 3D Transform。

### WorldSpace Depth / Occlusion

正式：

```text
WorldUIOcclusionMode
├─ DepthTest
├─ AlwaysVisible
└─ Custom
```

例如：

```text
World Terminal
→ DepthTest

Quest Marker
→ WorldAnchored / AlwaysVisible
```

### WorldSpace UI Input

ScreenSpace：

```text
Pointer Position
↓
2D UI Hit Test
```

WorldSpace：

```text
Mouse / Touch / Controller Ray
↓
Camera Ray
↓
UIDocument Plane / Surface
↓
Ray Intersection
↓
Convert to UI Local Coordinate
↓
Normal UI Hit Test
↓
UI Event
```

Input Adapter：

```text
UI Input
├─ ScreenPointerAdapter
└─ WorldPointerAdapter
```

一般 WorldSpace UI interaction 不為每個 Widget 建立 Jolt Collider。

```text
WorldSpace UI Hit Test
≠
Physics RigidBody / Collider
```

僅在 Gameplay 確實需要物理互動時額外建立 Collider。

### Surface UI / RenderTexture

後續可支援：

```text
UIDocument
↓
RenderTexture
↓
3D Mesh Material
```

適用：

- Curved Monitor
- Cylinder Display
- VR Curved Panel
- Irregular Mesh Surface

定位：

```text
WorldSpace Direct Geometry
→ V1

RenderTexture Surface UI
→ Optional / V2
```

### Widget V1

V1 至少支援：

```text
UIElement
├─ Panel
├─ Image
├─ Label
├─ Button
├─ Toggle
├─ Slider
├─ ProgressBar
├─ InputField
├─ ScrollView
├─ ScrollBar
├─ Mask
└─ Layout
```

後續：

```text
Dropdown
TabView
TreeView
RichText
```

### RectTransform / Layout

Runtime UI 使用自己的：

```text
UIRectTransform
├─ AnchorMin
├─ AnchorMax
├─ Pivot
├─ AnchoredPosition
├─ Size
├─ MinSize
├─ MaxSize
├─ Margin
└─ Padding
```

Layout：

```text
HorizontalLayout
VerticalLayout
GridLayout
OverlayLayout
```

後續可加入：

```text
Flex-like Layout
```

正式區分：

```text
Declared Layout
≠
Computed Layout
```

流程：

```text
Declared Layout
↓
Measure
↓
Layout Solver
↓
ComputedRect
```

### Nine-Slice

正式核心能力：

```text
UIImageMode
├─ Simple
├─ Sliced
├─ Tiled
└─ Filled
```

Nine-Slice 用於：

- Dialog Bubble
- Panel
- Button
- Window Background

### Text / Font Integration

使用：

```text
FreeType
+
HarfBuzz
```

流程：

```text
UTF-8
↓
Localization
↓
HarfBuzz Shaping
↓
Line Breaking
↓
FreeType Glyph
↓
Dynamic Glyph Atlas
↓
UI Renderer
```

支援：

- Traditional Chinese
- Simplified Chinese
- Japanese
- English
- Kerning
- Ligature
- Wrap
- Line Break

Text Measure 必須與 Layout Solver 整合：

```text
Available Width
↓
Text Measure
↓
Line Breaking
↓
Preferred Height
↓
Layout Solver
↓
Final Rect
```

### UI Input / Event

流程：

```text
Platform Input
↓
Engine Input System
↓
UI Hit Test
↓
UI Event System
```

事件至少：

```text
PointerDown
PointerUp
PointerMove
Click
DoubleClick
Scroll
Drag
Drop
Focus
Blur
Submit
Cancel
```

Mouse / Touch / Pen 統一為：

```text
PointerEvent
```

### Event Routing

支援：

```text
Capture
↓
Target
↓
Bubble
```

例如：

```text
ScrollView
└─ Button
```

ScrollView 可處理 Drag，Button 可處理 Click。

### Focus / Navigation

正式：

```text
UIFocusSystem
```

支援：

```text
Keyboard
Gamepad
Remote Controller
```

Actions：

```text
NavigateUp
NavigateDown
NavigateLeft
NavigateRight
Submit
Cancel
```

### ScrollView Virtualization

正式支援：

```text
VirtualizedListView
```

例如：

```text
10,000 Data Rows
↓
Visible 15 Rows
↓
約 20 個 UI Item Instances 重複使用
```

核心：

```text
ItemProvider
ItemRecycler
VisibleRange
ScrollOffset
```

不得為 10,000 筆資料建立 10,000 UIElement。

### Data Binding

Runtime UI 不直接綁 Gameplay Object implementation。

正式：

```text
Gameplay
↓
ViewModel
↓
UIBindingContext
↓
UIElement
```

例如：

```text
PlayerStatusViewModel
├─ HP
├─ MaxHP
├─ Level
└─ PlayerName
```

UI：

```text
HPBar
← HP / MaxHP

NameLabel
← PlayerName
```

DataTable 與 Runtime UI 的責任：

```text
DataTable
→ Configuration

Runtime Component
→ Current Runtime State

ViewModel
→ UI Presentation Data
```

UI 不直接修改 DataTable。

### JSON UI / UIDocument Asset

正式支援：

```text
*.ui.json
```

流程：

```text
UI JSON
↓
Engine JSON Framework
↓
UIDocument Loader
↓
Typed UI Tree
↓
Runtime
```

與 DataTable 一樣：

```text
JSON
→ Load / Parse Once

Runtime
→ Typed Runtime Data
```

不得每 Frame 操作 JSON DOM。

大型 UI：

- MainHUD
- Inventory
- Shop
- MainMenu

建議使用 External `*.ui.json` Asset。

Scene `UIComponent` 保存：

```text
UIDocument Asset UUID
```

優點：

- Independent Editing
- Reuse
- Bundle
- Hot Reload
- Version Control

### UI Style

正式：

```text
UIStyle
```

至少：

```text
Font
FontSize
TextColor
Background
Sprite
NineSlice
Padding
Margin
Opacity
```

State：

```text
Normal
Hover
Pressed
Disabled
Focused
```

後續可擴充：

```text
UIStyleSheet
```

V1 不要求完整 CSS。

### UI Animation / Tween

Runtime UI 自有：

```text
UIAnimation
/
UITween
```

至少支援：

```text
Position
Scale
Rotation
Opacity
Color
Size
Layout Parameter
```

一般 UI Tween 不依賴 Skeleton Animation。

### UI Render Extraction

禁止一個 Widget 一個 Draw Call。

流程：

```text
UITree
↓
UIRenderItem[]
↓
Sort
↓
Clip
↓
Batch
↓
UIRenderer
↓
RenderGraph
```

Batch 目標：

```text
Image
Nine-Slice
Glyph
Simple Shape
```

依下列狀態分組：

```text
Texture
Material
Blend
Clip
RenderSpace
DepthState
```

### Clip / Mask

V1：

```text
Rect Clip
```

後續：

```text
RoundedRect
Stencil
Texture Mask
```

### Logical UI Resolution

正式：

```text
Logical UI Resolution
≠
Physical Pixel Resolution
```

例如：

```text
Design Resolution
1920 × 1080
```

由：

```text
UIScalePolicy
```

轉換到實際裝置。

Safe Area 必須支援：

- iPhone Notch
- Dynamic Island
- Android Cutout
- Tablet Safe Area

UI Layout 透過：

```text
SafeAreaRect
```

取得，不讓 Gameplay UI 直接詢問 OS。

### UI 與 Dynamic Resolution

正式：

```text
3D Scene
→ Dynamic Resolution

Screen UI
→ Native / Logical UI Resolution

↓
Composite
```

UI 不跟 3D Dynamic Resolution 一起降解析度。

### Multiple UIDocument

Scene 可存在多個：

```text
Scene
├─ MainHUD
│  └─ UIComponent
├─ PauseMenu
│  └─ UIComponent
└─ InteractionUI
   └─ UIComponent
```

每個 UIDocument 可有：

```text
RenderSpace
RenderLayer
InputPriority
Visibility
```

### UI Hot Reload

```text
MainHUD.ui.json Changed
↓
Parse
↓
Validate
↓
Build New UIDocument
↓
Success?
├─ No → Keep Old
└─ Yes
   ↓
   Replace / Rebind
```

錯誤 UI Asset 不得清除目前正常 UIDocument。

### Runtime UI Performance

設計目標：

- UI Batch
- Glyph Atlas
- Dirty Update
- Layout Dirty Propagation
- Recycled List / Virtualized List
- Texture Atlas
- Material Batch
- Clip / Mask Optimization
- Static UI Cache
- Dynamic Vertex Buffer

流程：

```text
UI Tree
↓
Dirty Check
↓
Layout / Transform
↓
Geometry Build
↓
Sort / Batch
↓
GPU
```

大型 List：

```text
10,000 筆資料
↓
僅保留約 10~30 個可見 Item Instance
```

### Runtime UI Namespace

Runtime：

```cpp
engine::ui
```

必要時：

```cpp
engine::ui::layout
engine::ui::text
engine::ui::render
engine::ui::input
engine::ui::binding
```

不建立獨立：

```text
engine::ui3d
```

因為 ScreenSpace / WorldAnchored / WorldSpace 共用同一套 Runtime UI Framework。

Editor：

```cpp
engine::editor
```

Dear ImGui backend：

```cpp
engine::editor::imgui
```

禁止：

```cpp
engine::ui::imgui
```

### Runtime UI V1 Scope

V1：

```text
✓ UIDocument
✓ UIElement / UIElementID
✓ UI Tree
✓ UIComponent

✓ Panel
✓ Image
✓ Nine-Slice
✓ Label
✓ Button
✓ Toggle
✓ Slider
✓ ProgressBar
✓ InputField
✓ ScrollView
✓ VirtualizedListView

✓ RectTransform
✓ Anchor
✓ Pivot
✓ Layout
✓ Safe Area

✓ FreeType + HarfBuzz Text
✓ Localization

✓ Pointer Event
✓ Capture / Target / Bubble
✓ Focus
✓ Keyboard / Gamepad Navigation

✓ ViewModel / Data Binding

✓ JSON UI
✓ UI Style
✓ UI Animation / Tween

✓ Rect Clip
✓ UI Render Extraction
✓ UI Batch Renderer
✓ RenderGraph Integration

✓ ScreenSpace
✓ WorldAnchored
✓ WorldSpace
✓ WorldSpace Input Ray Mapping
✓ Depth / Occlusion
```

Future：

```text
△ RichText
△ Advanced Mask
△ CSS-like Style
△ Flex Layout
△ RenderTexture Surface UI
△ Curved UI
△ Advanced World-space UI
△ UI Particle / Special Effects
```

### Runtime UI Definition of Done

V1 至少驗證：

- UIElement 不依賴 SceneNode / EntityID。
- UIDocument 可由 Scene `UIComponent` 掛載。
- Editor Hierarchy 可同時呈現 Scene Entity 與 UIElement。
- UIElement Reparent / Delete / Duplicate 可 Undo / Redo。
- ScreenSpace / WorldAnchored / WorldSpace 共用 Widget / Layout / Event。
- WorldSpace UI ray mapping 正確。
- WorldSpace UI 一般 hit-test 不需要 Jolt Collider。
- Layout / Text Measure / Localization 可穩定重排。
- VirtualizedListView 不依資料數量建立等量 Widget。
- JSON UI parse failure 保留舊 UIDocument。
- UI Batch / Clip / Glyph Atlas profiler 可觀測。
- UI Logical Resolution 與 3D Dynamic Resolution 完全解耦。

核心 Contract：

```text
Runtime UI
→ Custom Retained Mode Framework

Editor UI
→ Dear ImGui

UIElement
→ Lightweight UI Hierarchy Node

UIElement
≠ SceneNode
≠ EntityID

UIDocument
→ Scene ↔ UI Boundary

Editor Hierarchy
→ Scene Hierarchy + Mounted UI Hierarchies

ScreenSpace / WorldAnchored / WorldSpace
→ Same Widget / Layout / Style / Event / Binding

UI JSON
→ Parse Once → Typed UI Tree

UI Resolution
→ Independent from 3D Dynamic Resolution
```


## 三十三、Runtime UI 效能

本節能力已整合至「Runtime UI Framework」的 Runtime UI Performance、VirtualizedListView、Render Extraction 與 V1 DoD。

## 三十四、文字系統

使用：
- FreeType
- HarfBuzz

支援：
- TTF / OTF
- Unicode
- CJK
- Ligature
- Complex Script
- Font Fallback
- Glyph Atlas

文字以 Glyph Batch 渲染，避免每字 Draw Call。


## 三十五、Localization

V1 納入。

最低功能：
- Key-based String Table
- Locale 切換
- Font Fallback
- Language-specific Font
- Basic Formatting
- Plural Rule 基礎支援
- Editor Preview

例：

ui.menu.start
ui.menu.settings
item.weapon.sword

不把顯示文字硬編碼在 Gameplay。


## 三十六、Save / Persistent Data

V1 納入。

功能：
- Platform-specific Save Path
- JSON / Binary Save
- Version Number
- Save Migration
- Atomic Write
- Temp File + Replace
- Corruption Detection
- User Settings
- Game Progress

原則：
- Save Data 不直接使用 Scene Serialize Format
- 必須有版本升級機制


## 三十七、Networking

V1 不內建高階 Networking Framework。

原因：
- 不同遊戲需求差異很大
- 可能使用 Photon / 自建 UDP / TCP / WebSocket / 第三方 SDK
- 過早決定會綁死架構

V1：
- 保留 Platform Socket / Transport Interface
- Gameplay Network Layer 不實作

Future：
- Reliable UDP
- Replication
- Snapshot
- Prediction
- Reconciliation
- Lobby / Matchmaking Adapter

明確標註：
Networking 不是遺漏，而是 V1 刻意排除。


## 三十八、Animation

本節為摘要；完整 Runtime Contract 以「Animation Framework」章節為準。

V1：
- Skeleton
- Animation Clip
- Animator / Animation Graph
- State Machine
- Cross Fade / Blend Tree 基礎
- Layers / Avatar Mask 基礎
- Root Motion
- Animation Event
- GPU Vertex Skinning
- BAT Crowd Animation

Future / Advanced：
- Advanced IK Graph
- Control-Rig-like Authoring
- Compute Skinning



### GPU Skinning Resource Binding

GPU Skinning 不建立獨立的特殊 Binding 路徑。Skinning Matrix / Bone Palette 走統一 Resource Registry。

```text
Animator
↓
Pose Evaluation
↓
Packed Bone Matrices
↓
Global Skinning Matrix Buffer
├─ Resource Identity → Resource Registry → GPU Buffer ResourceIndex
└─ Per-character Range → skinningMatrixOffset
                         ↓
                  SkinnedDrawData
                         ↓
                      Shader
```

```cpp
struct SkinnedDrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t skinningMatrixOffset;
};
```

`skinningMatrixOffset` 是 Global Skinning Matrix Buffer 內的 matrix element offset，不是 GPU `ResourceIndex`、descriptor index，也不是 byte offset。

Global Skinning Matrix Buffer 本身由 Resource Registry 管理，並透過 Engine Resource Binding abstraction 映射至 DX12 SRV / Descriptor Heap、Vulkan Buffer Descriptor / Descriptor Indexing、Metal Argument Buffer / Resource Table。Animation System 只負責 Pose / Skinning Data；GPU Buffer ownership 與 lifetime 由 Renderer / Resource Manager 管理。


## Dynamic Skinning Data / Transient GPU Buffer Architecture

Animation / Skinning Runtime 採集中式、線性配置的 Dynamic GPU Data Strategy。

核心目標：

```text
Per-character Pose
↓
Packed Bone Matrices
↓
Shared Dynamic GPU Buffer
↓
Per-draw Offset
```

避免：

- 每角色建立獨立 GPU Buffer
- 每幀大量小型 GPU allocation
- Descriptor fragmentation
- 高頻 create / destroy GPU resource
- 不必要的 CPU pointer chasing

### SkinnedDrawData

邏輯 Draw Payload：

```cpp
struct alignas(16) SkinnedDrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t skinningMatrixOffset;
};

static_assert(sizeof(SkinnedDrawData) == 16);
```

語意：

```text
transformIndex
→ Transform Buffer 內的 element index

materialIndex
→ MaterialGPU Buffer 內的 element index

meshIndex
→ Mesh / Geometry metadata index

skinningMatrixOffset
→ Global Skinning Matrix Buffer 內的 matrix offset
```

重要：

`transformIndex` / `materialIndex` / `meshIndex` 是資料 Buffer 的 element index，不等於 Bindless Descriptor Index。

### Frame Resource Indices

資源本身的 Bindless / Resource Registry index 應由 Frame Context 分離管理。

例如：

```cpp
struct FrameResourceIndices
{
    uint32_t transformBuffer;
    uint32_t skinningBuffer;
    uint32_t materialBuffer;
};
```

概念：

```text
ResourceIndex
→ 找到 GPU Resource

ElementIndex
→ 找到該 Resource 內的資料
```

兩者不可混用。

### Frames In Flight

Dynamic Skinning Buffer 至少支援 2~3 Frames In Flight。

例如：

```text
Frame 0 Buffer
Frame 1 Buffer
Frame 2 Buffer
```

但：

```text
Triple Buffering
≠
Zero Synchronization
```

在 CPU 重用某 Frame Slot 前，必須確認 GPU 已完成使用。

Frame 流程：

```text
Acquire Frame Slot
↓
Check / Wait Frame Fence
↓
Reset Linear Cursor
↓
CPU Writes
↓
Submit GPU Work
↓
Signal Fence
```

不得只使用：

```text
frameIndex % 3
→ reset
```

而不確認 GPU lifetime。

### Dynamic GPU Buffer Strategy

高階架構不寫死所有平台都使用 CPU-visible GPU memory。

統一抽象：

```text
DynamicGpuBufferPool
↓
Backend Chooses Memory Strategy
```

UMA / Unified Memory 裝置：

```text
CPU
↓
Persistent Mapped Shared Buffer
↓
GPU
```

可能適用：

- Apple Silicon
- 多數 Mobile UMA 架構
- 其他 coherent / shared memory platform

Discrete GPU：

```text
CPU
↓
Upload / Staging Ring Buffer
↓
GPU Copy
↓
Device-local Buffer
↓
Shader Read
```

可能適用：

- Windows discrete GPU
- 部分 Vulkan desktop device

RHI 必須自行決定：

- Host Visible
- Host Coherent
- Device Local
- Upload / Staging
- Flush / Invalidate Requirement
- Copy Scheduling

Gameplay / Animation System 不感知底層 memory type。

### Vulkan / Metal Memory Visibility

CPU `memcpy()` 不代表所有 backend 上 GPU 一定立即可見。

Vulkan：

```text
Host-visible + non-coherent
→ 需要 backend 執行適當 Flush
```

Metal：

```text
Storage Mode / Platform
→ 由 Metal backend 處理 shared / managed synchronization
```

上述行為必須封裝在 RHI / Dynamic GPU Buffer Backend。

### Resource Registry / Bindless Mapping

Shader 不直接依賴 DX12-specific：

```text
ResourceDescriptorHeap[index]
```

作為跨平台 contract。

正式架構：

```text
Canonical ResourceIndex
↓
RHI Binding Mapping
├─ DX12
│  └─ Descriptor Heap
├─ Vulkan
│  └─ Descriptor Indexing / Large Descriptor Set
└─ Metal
   └─ Argument Buffer / Resource Table
```

Slang Shader 使用 Engine 提供的 bindless abstraction / generated helper。

概念：

```slang
StructuredBuffer<float4x4>
GetSkinningBuffer(uint resourceIndex);
```

而不是把 DX12 SM 6.6 intrinsic 當成通用 API。

### Shader Access Model

概念 shader：

```text
FrameResourceIndices.skinningBuffer
↓
Get Skinning Buffer Resource
↓
SkinnedDrawData.skinningMatrixOffset
↓
+ Vertex Bone Index
↓
Bone Matrix
```

Transform：

```text
FrameResourceIndices.transformBuffer
↓
Transform Buffer Resource
↓
SkinnedDrawData.transformIndex
↓
World Matrix
```

因此：

```text
Resource Binding
與
Resource Internal Indexing
```

保持分離。

### Draw Data Transport

`SkinnedDrawData` 保持 16-byte logical payload。

Backend 可以自行選擇傳輸方式：

```text
DX12
→ Root Constants
→ 或 Draw Data Buffer

Vulkan
→ Push Constants
→ 或 Draw Data Buffer

Metal
→ Constant Buffer
→ 或 Argument-buffer referenced Draw Data
```

重要：

```text
Logical DrawData Layout
→ Unified

Physical Transport
→ Backend-specific
```

不把 Push Constant / Root Constant / Argument Buffer 視為完全等價 API。

### Dynamic Skinning Pool

概念：

```cpp
class DynamicSkinningBufferPool
{
public:
    void BeginFrame(FrameContext& frame);
    SkinningRange AllocateMatrices(
        Span<const Matrix4x4> matrices);

    ResourceIndex GetSkinningBufferResourceIndex() const;
};
```

`SkinningRange`：

```cpp
struct SkinningRange
{
    uint32_t matrixOffset;
    uint32_t matrixCount;
};
```

### V1 Allocation Strategy

V1 允許使用 Atomic Linear Cursor：

```text
Atomic Fetch-Add
↓
Reserve Matrix Range
↓
Parallel Copy
```

目的：

- 實作簡單
- 易驗證
- 不建立 per-character GPU buffer

但 Atomic Cursor 只是 V1 baseline。

### Advanced Allocation Strategy

Worker 數量增加後，可改為：

```text
Animation Jobs
↓
Collect Required Matrix Counts
↓
Prefix Sum / Batched Range Reservation
↓
Assign Non-overlapping Range Per Worker
↓
Parallel Copy
```

例如：

```text
Worker 0 → [0, 1200)
Worker 1 → [1200, 2700)
Worker 2 → [2700, 3900)
```

優點：

- Hot Path 無 global atomic contention
- 每個 Worker 寫入獨立 range
- 與 Thread-local Frame Allocator 策略一致

### Capacity / Budget

不可把：

```text
100000 matrices
```

定義成永久硬上限。

應使用：

```text
Initial Capacity
Soft Budget
Hard Budget
Peak Usage
Overflow Policy
```

例如：

```cpp
struct SkinningBufferBudget
{
    uint32_t initialMatrixCapacity;
    uint32_t maxMatrixCapacity;
};
```

Profiler 必須追蹤：

```text
Current Matrix Count
Peak Matrix Count
Capacity
Soft Budget
Hard Budget
Overflow Count
Upload Bytes
Copy Bytes
```

### Overflow Policy

Development：

```text
Assert
+
Profiler Warning
+
Capture offending workload
```

Shipping 不應只依賴 Assert。

可依專案策略：

```text
Grow Next Frame
Temporary Fallback Allocation
Reduce Animation Update Rate
Apply Animation LOD
Drop Lower-priority Skinning Work
```

若真的無法恢復才 Fatal。

### Animation LOD Integration

Skinning Buffer Budget 與 Animation LOD 可整合。

例如：

```text
Skinning Budget Pressure
↓
Far Character
↓
Lower Animation Update Rate
↓
Reduce Bone Evaluation
↓
Lower Skinning Matrix Usage
```

### Threading

Pose Evaluation：

```text
Worker-local
```

GPU Data Write：

```text
Per-worker reserved range
```

規則：

- Worker 不寫入彼此 range
- 不共享未同步 linear cursor，除非使用 atomic reservation
- Frame Slot reset 前必須等待所有 CPU Jobs 完成
- GPU reuse 前必須等待對應 Frame Fence
- Frame-local mapped pointer 不得逃逸至下一 frame

### GPU Resource Lifetime

Dynamic Skinning Buffer：

```text
ResourceManager / RHI
→ Owns GPU Resource

Animation / Renderer
→ Holds ResourceIndex / Range
```

不使用：

```text
SharedPtr<GPUBuffer>
```

作為 Gameplay / Draw Item ownership。

### Profiler

Profiler 新增：

```text
Animation / Skinning
├─ Evaluated Characters
├─ Evaluated Bones
├─ Matrix Count
├─ Buffer Capacity
├─ Upload Bytes
├─ Copy Bytes
├─ Atomic Reservation Cost
├─ Worker Range Usage
├─ Overflow Count
└─ GPU Skinning Time
```

### V1 Definition of Done

- 所有角色 bone matrix 可 packed 到共用 Dynamic Skinning Buffer。
- 每個 draw 只保存 `skinningMatrixOffset`。
- Transform / Material / Mesh element index 與 ResourceIndex 明確分離。
- Frames-in-flight slot 在 reuse 前經過 GPU fence 驗證。
- UMA / Discrete GPU 可使用不同 backend upload strategy。
- Shader 不直接依賴 DX12-only `ResourceDescriptorHeap` contract。
- Slang 經 Engine Resource Binding abstraction 存取 skinning buffer。
- V1 Atomic Linear Allocation 可運作。
- Profiler 能顯示 matrix capacity / peak / overflow。
- Overflow 在 Shipping Build 有非 Assert-only 策略。

### Future

- Prefix-sum / Batched Range Reservation
- GPU Pose Evaluation
- Compute Skinning
- GPU Animation Sampling
- Mesh Shader / GPU-driven skinning integration

## 三十九、Physics

採用：
- Jolt Physics

功能：
- RigidBody
- Static Body
- Collider
- Trigger
- Raycast
- Character Controller
- Physics Scene

### Physics Execution Domain

Physics 採 Hybrid CPU / GPU 架構；Gameplay-authoritative Physics 以 CPU / Jolt 為主，GPU 只處理適合大量平行運算、可容忍延遲或純視覺的 workload。

```text
PhysicsExecutionDomain
├─ CPUAuthoritative
├─ CPUBatched
├─ GPUVisual
└─ GPUDeferred
```

定義：

```text
CPUAuthoritative
→ Gameplay RigidBody
→ Character Controller
→ Contact / Trigger
→ Immediate Hit / Query
→ Gameplay-critical Constraint

CPUBatched
→ Large CPU Query Batch
→ Job System
→ AI / Character / World Query

GPUVisual
→ VFX Particle Physics
→ Cloth
→ Rope / Chain
→ Debris
→ Visual Secondary Motion

GPUDeferred
→ Crowd / Background Query
→ Heightfield / SDF Query
→ One-or-more-frame latency allowed
```

正式原則：

```text
Gameplay Physics
→ CPU / Jolt authoritative

Visual / Massive / Deferred Physics
→ GPU acceleration optional
```

禁止將「GPU 更快」視為所有 Physics workload 的預設答案；若結果需要 CPU 當 frame 立即消費，不得強制搬到 GPU 造成同步 / readback stall。

### GPU Physics Framework

GPU Physics 不是另一套完整 Gameplay Physics World，而是既有 Physics Framework 的 optional acceleration domain。

```text
Physics Framework
├─ CPU Physics Runtime
│  └─ Jolt Backend
└─ GPU Physics Runtime
   ├─ Particle
   ├─ Cloth
   ├─ Rope
   ├─ Debris
   ├─ SoftBody（後續）
   └─ Deferred Query
```

GPU Physics 資料使用 dedicated pool / buffer，不建立大量 Scene Entity / Component。

```text
GPUPhysicsPool
├─ Position[]
├─ PreviousPosition[]
├─ Velocity[]
├─ Constraint[]
├─ CollisionData[]
├─ ActiveIndices[]
└─ Optional CustomData[]
```

### GPU Physics Simulation Flow

典型 Compute 流程：

```text
Spawn / Initialize
↓
Integrate
↓
Collision
↓
Constraint Solve
↓
Compaction
↓
Output Buffer
↓
Render / VFX / Secondary Motion
```

GPU Physics Pass 必須納入 RenderGraph / GPU scheduler 管理 resource lifetime、barrier、queue ownership 與 async compute opportunity。

不得在 Physics subsystem 私自提交未被 RenderGraph / scheduler 追蹤的 GPU work。

### GPU Collision Sources

GPU Visual Physics 可依功能與平台使用：

```text
Depth Buffer Collision
Heightfield Collision
SDF Collision
Simplified Scene Collision
```

Collision source 必須明確標記其精度與 gameplay authority。

```text
GPU Visual Collision
≠
Gameplay Authoritative Collision
```

### GPU Cloth

正式支援 GPU Cloth foundation。

流程：

```text
Character Skeleton / Attachment
↓
Cloth Attachment Points
↓
GPU Cloth Simulation
↓
Constraint Solve
↓
Collision
↓
Final Cloth Vertices
↓
Render
```

適用：

```text
Cape
Skirt
Sleeve
Flag
Long Cloth
Hair / Accessory mesh where appropriate
```

GPU Cloth 預設屬於 visual secondary simulation；若布料狀態會直接決定 gameplay collision / rule，必須有 CPU-authoritative representation 或專門 gameplay proxy。

### GPU Rope / Chain

純視覺 Rope / Chain 可使用 GPU particle + distance constraint solver。

```text
Rope Particles
+
Distance / Bend Constraints
↓
GPU Solver
```

若 Rope 參與：

```text
Gameplay Pulling
Character Suspension
Mechanism Activation
Force Feedback
```

則不得只依賴 GPU visual result 作 authority。

### GPU Debris

大量視覺碎片不建立大量 Jolt RigidBody。

```text
Explosion
↓
GPU Debris Pool
↓
Position / Velocity
↓
Depth / Heightfield / SDF Collision
↓
Indirect Render
```

Gameplay-relevant debris 才建立 CPU / Jolt body。

後續可評估：

```text
GPU Debris
↓ Player / Gameplay relevance
Promote to CPU Jolt Body
↓
CPU Authoritative

Far / irrelevant again
↓
Demote / Destroy
```

Promotion / Demotion 不列為 V1 必要能力。

### Immediate Query vs Deferred GPU Query

Physics Query 分為：

```text
ImmediateQuery
→ CPU / Jolt
→ Same-frame result

DeferredBatchQuery
→ CPU Job or GPU
→ Result may arrive later
```

ImmediateQuery 用於：

```text
Player Movement
Jump / Grounding
Weapon Hit
Gameplay Collision
Character Controller
```

Deferred GPU Query 可用於：

```text
Background Crowd
Vegetation Interaction
Ambient Agents
Non-critical Ground Probe
Large Heightfield / SDF Query
```

禁止把需要 same-frame Gameplay decision 的 query 放到 GPU readback 路徑。

### Batch Query

與既有 Zig Stable C ABI / Batch-first 原則一致，Physics 支援：

```text
RaycastBatch
SphereCastBatch
CapsuleCastBatch
BoxCastBatch
OverlapBatch
DeferredQueryBatch
```

避免：

```text
for each NPC
    Zig → Engine ABI → Raycast()
```

優先：

```text
Zig / Gameplay
↓
QueryBatch
↓
Physics Job / GPU Deferred Path
↓
ResultBatch
```

### GPU Broadphase Policy

GPU Broadphase / GPU RigidBody World 不列為 V1 核心能力。

原因：

```text
CPU ↔ GPU synchronization
Readback latency
Debug complexity
Platform divergence
Mobile GPU / thermal pressure
```

對數百至數千個 gameplay rigid bodies，優先使用 Jolt multi-threaded CPU path。

後續 R&D：

```text
AABB[]
↓
GPU Broadphase
↓
Candidate Pairs
↓
CPU or GPU Narrowphase
```

僅在實際 profile 證明 CPU broadphase 成為瓶頸後導入。

### Mobile / Thermal Policy

GPU Physics 必須接入既有 `PerformancePolicyManager`。

可調：

```text
Simulation Rate
Solver Iterations
Particle / Cloth Count
Collision Tier
GPU Debris Count
Deferred Query Budget
Async Compute Usage
```

範例：

```text
High
→ Full GPU Cloth
→ Higher solver iterations

Medium
→ Lower simulation rate
→ Fewer iterations

Low
→ Simplified spring / bone motion
→ Disable expensive GPU Cloth / Debris
```

手機不得因 GPU Physics 導致 Rendering / VFX / Animation / Skinning 全部競爭同一 GPU budget 而失控。

### CPU / GPU Authority Boundary

任何 Physics data 必須標記 authority。

```text
CPU Authoritative
→ Gameplay Truth

GPU Visual
→ Presentation / Secondary Motion

GPU Deferred
→ Advisory / Delayed Query Result
```

禁止：

```text
GPU visual result
→ silently becomes gameplay truth
```

若需要把 GPU 結果回傳 Gameplay，必須透過顯式 readback queue / result generation / frame latency contract。

### GPU Readback Contract

Profiler / Runtime 必須追蹤：

```text
Readback Bytes
Readback Count
Readback Latency
GPU→CPU Stall Count
```

Physics API 不允許隱藏式同步 readback。

```text
Request GPU result
↓
Async Result Handle / Deferred Result Buffer
↓
Consume only when ready
```

若呼叫端要求 same-frame immediate result，必須使用 CPU Query path。

### GPU Physics Memory

GPU Physics Memory 納入全域 Memory Budget：

```text
GPU Physics Memory
├─ Particle State
├─ Cloth State
├─ Constraint Buffer
├─ Collision Data
├─ Debris State
├─ Deferred Query Buffer
├─ Readback Buffer
└─ Indirect Args
```

Memory Pressure 時可：

```text
Reduce non-critical cloth particles
Reduce solver iterations
Reduce debris count
Disable distant GPU visual simulation
Reduce deferred-query budget
```

不得因 Memory Pressure 破壞 CPU-authoritative Gameplay Physics correctness。

### GPU Physics Profiler

Physics Profiler 分 CPU / GPU 類別。

```text
Physics CPU
├─ Jolt Step
├─ Broadphase
├─ Narrowphase
├─ Solver
├─ Active / Sleeping Bodies
├─ Contacts
├─ Constraints
└─ Queries

Physics GPU
├─ Particle Simulation
├─ Cloth
├─ Rope
├─ Debris
├─ GPU Collision
├─ Deferred Queries
├─ Solver Iterations
├─ GPU Time
├─ Readback Bytes
└─ Readback Latency
```

### GPU Physics V1 Scope

V1：

```text
✓ CPU Jolt Physics
✓ CPU Batch Query
✓ GPU VFX Collision
✓ GPU Cloth foundation
✓ GPU Debris optional
✓ GPU Deferred Query interface
```

後續：

```text
△ GPU Broadphase
△ GPU Soft Body
△ GPU Crowd Physics
△ GPU RigidBody Simulation
△ Physics Promotion / Demotion
```

GPU RigidBody World 不得阻塞 V1。


### Character Framework

角色控制不視為單一 Physics `CharacterController` Component，而正式拆分為：

```text
Input / AI / Gameplay
↓
CharacterIntent
↓
CharacterMotor
↓
CharacterController
↓
Physics Query / Collision Resolve
↓
CharacterMotionResult
↓
Scene Transform
+
Animation Parameters
+
Gameplay Events
```

核心原則：

```text
CharacterController
≠
Movement Gameplay Logic
```

`CharacterController` 負責碰撞、地面、斜坡、階梯、穿透恢復與平台交互；`CharacterMotor` 負責移動手感、速度、跳躍、重力、外力、Root Motion 與 Facing Policy。

#### CharacterController Responsibilities

正式責任：

```text
CharacterController
├─ Shape
├─ Collision Layer / Mask
├─ Ground Detection
├─ Slope Handling
├─ Step Handling
├─ Ground Snap
├─ Penetration Recovery
├─ Moving Platform Support
├─ Dynamic Body Interaction
└─ Motion Resolve
```

主要輸入：

```text
Current Transform
Desired Displacement
Desired Rotation
Delta Time
```

主要輸出：

```text
Resolved Position
Resolved Rotation
Actual Displacement
Resolved Velocity
Ground State
Ground Normal
Ground Body
Ground Point Velocity
Hit Result[]
```

Controller 不負責：

```text
Walk Speed
Run Speed
Sprint Rules
Stamina
Attack State
Skill State
```

上述屬於 Gameplay / CharacterMotor。

#### CharacterMotor

`CharacterMotor` 負責角色移動 policy。

```text
CharacterMotor
├─ Ground Acceleration
├─ Ground Deceleration
├─ Air Acceleration
├─ Gravity
├─ Jump
├─ Max Speed
├─ Crouch
├─ Sprint
├─ External Velocity
├─ Root Motion Integration
└─ Facing Policy
```

可存在不同 Motor implementation：

```text
ActionCharacterMotor
FPSCharacterMotor
PlatformerMotor
SwimmingMotor
FlyingMotor
AICharacterMotor
```

不同遊戲可共用同一 CharacterController，而替換 CharacterMotor。

#### CharacterIntent

Zig Gameplay / AI 優先提交 POD `CharacterIntent`，不直接執行細粒度 Physics Query。

概念：

```cpp
struct CharacterIntent
{
    float moveX;
    float moveY;
    float facingX;
    float facingY;
    uint32_t flags;
};
```

Flags 可表示：

```text
Jump
Crouch
Sprint
Dash
```

流程：

```text
Zig / AI
↓
CharacterIntent
↓
Native Character System
↓
Batch Update
```

禁止讓 Zig 每角色每 Frame 反覆跨 ABI 執行：

```text
Raycast
GroundCheck
MoveCapsule
ResolveWall
StepUp
```

應維持既有 Batch-first Stable C ABI 原則。

#### Runtime Data-Oriented Layout

Editor 可使用：

```text
Node
├─ Transform
├─ CharacterController
├─ Animator
└─ Gameplay
```

Runtime 底層採 Data-Oriented Pool：

```text
CharacterControllerPool
├─ EntityID[]
├─ CharacterHandle[]
├─ ShapeHandle[]
├─ Position[]
├─ Velocity[]
├─ DesiredVelocity[]
├─ GroundState[]
├─ GroundNormal[]
├─ GroundBody[]
├─ GroundVelocity[]
├─ ConfigIndex[]
├─ Flags[]
└─ ...
```

禁止：

```text
N CharacterController Objects
→ N virtual Update()
```

優先：

```text
CharacterSystem
→ Process Character Batch
```

#### Jolt Backend Mapping

Gameplay / Public API 不暴露 Jolt-specific type。

```text
Engine CharacterController
↓
Character Backend Adapter
↓
Jolt Character / CharacterVirtual-style Path
```

Player / NPC Character 預設採 Character Controller 路徑；一般物理物件才使用 Dynamic RigidBody。

```text
Character
≠
Dynamic RigidBody
```

Gameplay 僅看到：

```text
CharacterControllerHandle
CharacterMoveRequest
CharacterMotionResult
```

#### Ground State

禁止只有單一 `bool grounded` 作為完整地面狀態。

正式：

```text
CharacterGroundState
├─ InAir
├─ OnGround
├─ OnSteepGround
├─ Sliding
└─ Unsupported
```

並提供：

```text
GroundNormal
GroundDistance
GroundBody
GroundPointVelocity
```

角色碰到斜面不代表一定能站立。

#### Slope Handling

Character Config 至少包含：

```text
MaxWalkableSlope
```

流程：

```text
Ground Contact
↓
Slope Test
├─ Walkable
└─ Too Steep
   ↓
   Sliding / Unsupported
```

禁止只靠「碰撞點在腳下」判定 Grounded。

#### Step Up / Step Down

正式支援：

```text
StepHeight
StepForwardDistance
GroundSnapDistance
```

流程：

```text
Forward Motion
↓
Hit Low Obstacle
↓
Can Step?
├─ No → Block
└─ Yes
   ↓
   Move Up
   ↓
   Move Forward
   ↓
   Move Down
```

用於：

```text
Stair
Curb
Small Rock
Small Height Difference
```

#### Ground Snap

角色走下小斜坡 / 階梯時，避免反覆：

```text
Airborne
Grounded
Airborne
Grounded
```

當：

```text
Character moving downward
+
Ground within snap distance
```

可執行 Ground Snap。

若角色正在：

```text
Jumping upward
```

則暫停 Ground Snap。

#### Moving Platform

Moving Platform 列為 V1 角色控制必要能力。

支援：

```text
Elevator
Moving Platform
Rotating Platform
Ship
Vehicle Surface
```

CharacterController 需取得：

```text
GroundBody
GroundLinearVelocity
GroundAngularVelocity
GroundPointVelocity
```

角色實際運動：

```text
Own Motion
+
Platform Point Motion
↓
Resolved Motion
```

旋轉平台需使用 contact-point velocity，不得只加 world translation。

#### Terrain Streaming Boundary Contract

Character Controller 是 persistent kinematic 物件，不同於 Particle / Projectile 可接受短暫消失或延遲生成；跨越 Terrain / World Partition Streaming Cell 邊界時，不能只沿用既有「static body 隨 Chunk load/unload」規則，必須有獨立契約。

正式原則：

```text
Character Physical Footprint
= Capsule Bounds + Safety Margin
  (Ground Probe Distance, Max Step, Max Fall Distance)
↓
Occupied Cell Set
```

Occupied Cell Set 內的 Physics Collision Representation（HeightFieldShape / Static Body）視為 Pinned，獨立於該 Cell 的 Render / Vegetation Residency 之外——即使畫面 LOD 已因距離判定降級或卸載，角色腳下的 Collision 仍不得被回收。Physics Collision Residency 與 Render Residency 分開追蹤，沿用既有 Streaming Residency 的 Double Budget 精神，兩者不得連動。

Cell Unload 規則正式修正為：

```text
Static Physics Body Unload 條件
= Cell 不在任何 Character 的 Occupied Cell Set
AND
  Cell 不在任何 Character 的 Adjacent Prefetch Set
```

角色仍在 Occupied Cell Set 內時，該 Cell 的 HeightFieldShape 禁止進入 `PendingUnload` / `Evicting`。

Cell Load 規則（角色移動快於 Streaming 完成，例如 Teleport 或高速位移進入未載入區域）：

```text
Character 進入尚未 Ready 的 Cell
↓
CharacterGroundState = StreamingPending
↓
暫停一般 Ground Snap / Step / Slide 判定
↓
使用 Last-Known Ground（若有）或 Hold Position（若無）
↓
禁止因「當下無 Collision」直接 Free Fall
```

`CharacterGroundState` 正式新增第六種狀態：

```text
CharacterGroundState
├─ InAir
├─ OnGround
├─ OnSteepGround
├─ Sliding
├─ Unsupported
└─ StreamingPending      ← 新增：Ground Cell 尚未 Ready
```

Character Movement 正式列為 High-priority Streaming Source，銜接既有 Streaming Priority Preemption / IO Concurrency Contract：Occupied Cell 與 Adjacent Prefetch Cell 的請求優先權高於一般 Camera Frustum Streaming Demand。

Teleport Contract 同步規定：Teleport 目的地 Cell 若未 Ready，必須先觸發同步 / 高優先 Load，並在 Load 完成前保持 `StreamingPending`，不得直接把角色放到一個沒有 Collision 的世界座標。

V1 範圍限制：本契約僅涵蓋 Persistent Kinematic Character（Player / NPC，走 CharacterController 路徑）；GPU Physics / Ragdoll / Debris 等非 gameplay-authoritative 物件不適用。

#### Jump Policy

Jump 的 gameplay policy 屬於 CharacterMotor。

```text
Grounded
+
Jump Request
↓
Detach Ground
↓
Vertical Velocity
↓
CharacterMotor
↓
CharacterController Resolve
```

常見：

```text
Jump Buffer
Coyote Time
```

可由 Gameplay Motor 實作，不寫死在 Physics backend。

#### Gravity Ownership

Gravity velocity 由 CharacterMotor / CharacterState 管理：

```text
VerticalVelocity += Gravity * dt
```

最終 displacement 仍交給 CharacterController collision resolve。

原則：

```text
Motor
→ owns desired motion

Controller
→ owns collision resolution
```

#### Desired vs Resolved Velocity

Runtime 必須區分：

```text
DesiredVelocity
ResolvedVelocity
```

例如撞牆：

```text
DesiredVelocity = 5 m/s
ResolvedVelocity = 0 or wall-slide velocity
```

Animation 通常應使用 `ResolvedVelocity`，避免角色撞牆仍播放完整跑速動畫。

#### Root Motion Integration

與 Animation Framework 的 `RootMotionDelta` 正式接合：

```text
Animation RootMotionDelta
+
Gameplay Motor Motion
+
External Velocity
+
Gravity
↓
CharacterMoveRequest
↓
CharacterController
↓
Physics Resolve
↓
CharacterMotionResult
```

Controller 必須輸出：

```text
RequestedMotion
ActualMotion
MotionError
```

例如：

```text
Requested Root Motion = 2.0 m
Actual Motion = 0.6 m
```

因前方碰牆時，不允許 Animation Root Motion 穿過 collision。

`MotionError` 可提供給：

```text
Animation
Motion Warping
Gameplay
```

#### External Velocity / Knockback

角色不是 Dynamic RigidBody 仍可被：

```text
Explosion
Knockback
Launch
Moving Platform
Gameplay Force
```

影響。

CharacterMotor 維護：

```text
ExternalVelocity
```

合成：

```text
Locomotion Velocity
+
External Velocity
+
Root Motion
+
Gravity
↓
CharacterController
```

不要求為 Knockback 臨時切換成 Dynamic RigidBody。

#### Character ↔ Dynamic Body Interaction

支援：

```text
Character pushes Dynamic Body
Dynamic Body pushes Character
```

Character Config 可包含：

```text
CanPushBodies
CanBePushed
MaxPushForce
MassInteractionPolicy
```

角色推物理物件時需限制 impulse / force，避免角色無條件推動超大質量物件。

Dynamic Body 撞擊 Character 時，Controller 必須處理：

```text
External Body Velocity
Penetration Recovery
Character Displacement
```

不得假設 Character 永遠是不可推動障礙。

#### Crouch / Capsule Resize

Crouch 不只是 Animation。

```text
Standing Capsule
↓
Crouch
↓
Short Capsule
```

起身：

```text
Check Overhead Clearance
↓
Enough Room?
├─ Yes → Standing Capsule
└─ No  → Keep Crouching
```

Shape Resize 預設保持 feet position，不以 capsule center 原地縮放造成角色腳底漂移。

#### Teleport Contract

正式區分：

```text
Move()
Teleport()
```

Teleport：

```text
Set Position
↓
Reset or Preserve Velocity by Policy
↓
Invalidate Ground Support
↓
Re-evaluate Collision
```

大位移不得自動當成普通 Character Move。

#### Facing Policy

角色朝向與移動方向分離。

```text
FacingPolicy
├─ MovementDirection
├─ CameraDirection
├─ TargetDirection
├─ RootMotion
└─ ExplicitGameplay
```

CharacterController 只負責 collision shape orientation；Gameplay / CharacterMotor 決定角色視覺 / gameplay Facing。

不得硬綁：

```text
Facing == Velocity Direction
```

#### Fixed Tick / Render Interpolation

CharacterController 與 Physics Fixed Tick 整合：

```text
Input / AI
↓
CharacterIntent Buffer
↓
Fixed Physics Tick
↓
CharacterMotor
↓
CharacterController
↓
Resolved Character State
↓
Render Interpolation
```

Character Transform 保存：

```text
PreviousPhysicsTransform
CurrentPhysicsTransform
```

Render 使用 interpolation，避免 Physics 60 Hz、Render 90/120 Hz 時角色視覺抖動。

#### Networking-ready State

V1 不實作完整 Networking，但 Character Framework 不得阻塞未來 prediction / re-simulation。

狀態需能形成明確 snapshot：

```text
CharacterMotorState
├─ Position
├─ Rotation
├─ Velocity
├─ DesiredVelocity
├─ ExternalVelocity
├─ GroundState
├─ GroundBody
└─ Motor-specific State
```

CharacterMotor 不依賴大量隱藏 global state，方便未來：

```text
Input
↓
Prediction
↓
Character Motor
↓
Re-simulation
```

#### Character Query Budget / LOD

大量 NPC 不得每角色無限制執行多次 Query。

支援 Character LOD / Query Budget：

```text
Near
→ Full Ground / Step / Interaction

Mid
→ Standard Controller

Far
→ Reduced Query / Lower Tick

Very Far
→ Navigation Proxy / No Detailed Controller
```

但：

```text
Player
Boss
Gameplay-critical NPC
```

不得因距離自動降級成不正確的 Gameplay Simulation。

Character Priority / LOD 必須接入既有 PerformancePolicyManager。

#### Navigation Integration

Navigation / NavMesh 不直接修改 Transform。

正式：

```text
Navigation
↓
Desired Velocity
↓
CharacterIntent / CharacterMotor
↓
CharacterController
↓
Actual Movement
```

避免：

```text
Nav Agent
→ Direct Set Transform
```

Player / AI 可共用同一 Character collision / movement pipeline。

#### Animation Integration

Character System 輸出：

```text
Speed
PlanarSpeed
VerticalVelocity
Grounded
GroundState
Slope
Acceleration
TurnRate
DesiredDirection
ActualDirection
```

Animator 消費：

```text
CharacterMotionState
↓
Animation Parameters
```

Animation 不直接成為 Gameplay movement authority。

Root Motion 為明確例外，但仍必須經 CharacterController resolve。

#### Character Controller Component / Config

Editor-facing CharacterControllerComponent 至少包含：

```text
Shape
├─ Capsule Radius
├─ Standing Height
└─ Crouching Height

Collision
├─ Layer
├─ Mask
└─ Contact Offset

Ground
├─ Max Slope
├─ Step Height
├─ Ground Snap
└─ Ground Probe

Interaction
├─ Push Bodies
├─ Can Be Pushed
└─ Push Force Policy
```

Gameplay-side 可使用：

```text
CharacterMotorComponent
CharacterMotorState
```

Engine 提供標準 Motor implementation；專案可透過 Zig Gameplay 建立自訂 Motor policy。

#### Character Framework Profiler

Profiler 至少追蹤：

```text
Active Characters
Character Fixed Tick Time
Ground Query Count
Step Query Count
Character Batch Count
Moving Platform Count
Push Interaction Count
Penetration Recovery Count
Character LOD Distribution
Reduced Query Count
Root Motion Requested / Actual Distance
Motion Error
Character ABI Batch Count
```

#### Character Framework CI / Validation

CI 至少包含：

- Ground / InAir / Steep Ground 狀態切換。
- Slope Limit。
- Step Up / Step Down。
- Ground Snap。
- Jump detach / landing。
- Moving translation platform。
- Rotating platform contact-point velocity。
- Dynamic Body pushes Character。
- Character pushes Dynamic Body。
- Crouch / stand clearance。
- Capsule resize feet preservation。
- Teleport vs Move semantics。
- Root Motion collision clamp。
- Desired / Resolved velocity correctness。
- Fixed Tick + Render interpolation regression。
- Character Batch ABI。
- Character LOD 不得降級 gameplay-critical character。
- Jolt backend-specific type 不得洩漏至 Gameplay / Stable C ABI。
- Character 站在 Streaming Cell 邊界時觸發 Chunk unload，Collision 不得被移除（Occupied Cell Pinned 驗證）。
- Teleport / 高速位移進入尚未 Ready 的 Cell，必須進入 `StreamingPending` 而非直接 Free Fall 或穿模。

#### Character Framework V1 Definition of Done

V1 至少完成：

```text
CharacterIntent
+
CharacterMotor
+
CharacterController
+
Jolt Backend Adapter
+
Ground / Slope
+
Step Up / Down
+
Ground Snap
+
Jump
+
Moving Platform
+
Root Motion Resolve
+
External Velocity / Knockback
+
Dynamic Body Interaction
+
Crouch / Resize
+
Teleport
+
Fixed Tick
+
Render Interpolation
+
Terrain Streaming Boundary Contract
+
Batch-first Zig ABI
```

架構最終關係：

```text
Input / AI
↓
CharacterIntent
↓
CharacterMotor
├─ Locomotion
├─ Jump
├─ Gravity
├─ External Velocity
├─ Root Motion
└─ Facing
↓
CharacterController
├─ Capsule Collision
├─ Ground
├─ Slope
├─ Step
├─ Snap
├─ Moving Platform
├─ Dynamic Body Interaction
└─ Penetration Recovery
↓
Jolt Physics
↓
CharacterMotionResult
├─ Actual Position
├─ Actual Velocity
├─ Ground State
├─ Ground Normal
├─ Ground Body
└─ Hit Events
↓
Scene Transform
+
Animation
+
Gameplay
```

正式架構決策：

```text
CharacterController
→ Native Core System

CharacterMotor
→ Replaceable Gameplay Policy
```


### Terrain Collision

Terrain Collision 使用 Jolt `HeightFieldShape`。

Source of Truth：

```text
Terrain Heightmap
= Persistent / Authoritative Height Source

        ↓ Build / Runtime Chunk Resolve

Jolt HeightFieldShape
= Physics Derived Representation
```

禁止維護另一份獨立可編輯的 Physics Heightmap。Terrain Heightmap 修改後，只重建或更新受影響 Chunk 的 Physics HeightField representation。Physics collision chunk 與 Terrain streaming chunk 對齊；Chunk unload 時移除對應 static physics body，load 時建立對應 HeightFieldShape。V1 預設 Terrain Collision 為 static。Kinematic Character 佔用中的 Chunk 為例外，其 unload/load 時序另受 Character Framework 的 Terrain Streaming Boundary Contract 規範（見上），不套用一般 static body 規則。

## 四十、Audio

採用：
- miniaudio

功能：
- 2D Audio
- 3D Positional Audio
- Listener
- Streaming Music
- Audio Groups
- Volume
- Loop
- Audio Residency Scope




### Audio Residency Scope

Audio Resource Runtime 正式支援 `AudioResidencyScope`，用來管理一組 Audio Resource 的 Runtime Residency / Lifetime，而不是建立多份彼此獨立的實體 Cache。

核心用途：

```text
Enter Scene / Zone / Encounter
↓
Create / Activate AudioResidencyScope
↓
Preload / Resolve Required Audio
↓
Runtime Play
↓
Leave Scene / Zone / Encounter
↓
Release AudioResidencyScope
↓
Only resources no longer referenced or pinned become evictable
```

典型 Scope：

```text
AudioResidencyScope
├─ Global
├─ UI
├─ Music
├─ Scene
├─ Zone
├─ Character
├─ Encounter
├─ Cutscene
└─ Temporary
```

例如：

```text
Global
→ Common UI
→ Common Notification
→ Shared System SFX

Scene:Map01
→ Ambient
→ Environment SFX
→ Local Creature SFX
→ Scene-specific Voice

Encounter:Boss01
→ Boss Attack SFX
→ Boss Voice
→ Boss Music / Stingers

UI:Shop
→ Shop UI SFX
```

離開 Map01：

```text
ReleaseResidencyScope(Scene:Map01)
```

不得影響仍由：

```text
Global
UI
Music
Player
Other Active Scope
```

引用的 Audio Resource。

#### One Registry, Multiple Residency Owners

禁止：

```text
Global Cache
Scene Cache
UI Cache
→ same AudioClip decoded / loaded independently multiple times
```

正式架構：

```text
                 AudioResourceRegistry
                         │
          ┌──────────────┼──────────────┐
          │              │              │
       Audio A        Audio B        Audio C
          ▲              ▲
          │              │
   ┌──────┴──────┐       │
   │             │       │
Global Scope   Scene Scope
```

真正 Audio Resource 只有一份 canonical runtime entry；`AudioResidencyScope` 只增加 / 移除 Residency Reference。

同一 AudioClip / AudioEvent 可以同時被多個 Scope 引用。

```text
Scene Scope Release
↓
Scene Residency Ref--
↓
Other Scope Ref still exists?
├─ Yes → Keep Resident
└─ No  → Continue Pin / Eviction Check
```

#### Audio Residency Scope Handle

Runtime 使用 generation-based handle：

```text
AudioResidencyScopeHandle
```

概念流程：

```text
Create Scope
↓
Associate Audio Assets / Events
↓
Preload / Load
↓
Use
↓
Release Scope
```

Scope Release 不等於立即 free：

```text
Release Scope
→ remove residency ownership
→ resource becomes evictable only when safe
```

#### Resource Pinning

Active Voice、Streaming、Preload transaction 等必須能 pin Audio Resource。

至少追蹤：

```text
ScopeRefCount
VoicePinCount
StreamingPinCount
PreloadPinCount
```

基本 Eviction 條件：

```text
CanEvict =
ScopeRefCount == 0
&& VoicePinCount == 0
&& StreamingPinCount == 0
&& PreloadPinCount == 0
```

因此：

```text
Scene Scope Released
+
Voice still playing
→ Audio Resource remains valid
```

不得因場景切換直接釋放仍被 Audio Thread / Active Voice 使用的 memory。

#### Scope Release Policy

Scope Release 支援明確 policy：

```text
AudioScopeReleasePolicy
├─ StopImmediately
├─ FadeOutAndRelease
├─ LetActiveVoicesFinish
└─ DetachActiveVoices
```

建議預設：

```text
Scene / Zone
→ FadeOutAndRelease

Temporary OneShot
→ LetActiveVoicesFinish

UI / Global Shared
→ LetActiveVoicesFinish or explicit policy
```

`FadeOutAndRelease`：

```text
Release Scope
↓
Reject new play from released scope
↓
Active Voices
→ Fade Out
↓
Stop
↓
Voice Pin released
↓
Unused Resources become evictable
```

Fade 必須由 Audio Runtime / Audio Thread 執行，不由 Gameplay 每 Frame 手動修改 volume。

#### Residency Data Granularity

Audio Residency 不只追蹤「檔案是否載入」，而應區分：

```text
Audio Runtime Residency
├─ Event Metadata
├─ Compressed Resident Data
├─ Decoded PCM
├─ Streaming Read Buffer
├─ Decode Buffer
└─ Preload State
```

正式支援：

```text
AudioResidencyMode
├─ MetadataOnly
├─ Compressed
├─ DecodedPCM
└─ Streaming
```

典型：

```text
UI Click
→ DecodedPCM

Common Short SFX
→ DecodedPCM / Compressed by profile

Dialogue
→ Compressed / Streaming

BGM
→ Streaming
```

#### Scene Transition / Preload

Audio Residency Scope 與 Scene transition 正式整合。

```text
Current:
Scene:Map01

Preload:
Scene:Map02
```

切換：

```text
Create Scope(Map02)
↓
Preload Required Audio
↓
Required Audio Ready
↓
Activate Map02
↓
Release Scope(Map01, FadeOutAndRelease)
```

避免新場景第一次播放重要 Audio 時才同步 decode / load。

#### Nested / Sub-scopes

支援邏輯上的父子 Scope：

```text
Scene:Map01
├─ Ambient
├─ NPC
├─ BossArea
└─ Cutscene01
```

例如：

```text
Enter Boss Area
↓
Activate BossArea Scope
↓
Preload Boss Audio

Boss Encounter End
↓
Release BossArea Scope
```

不需要等待整張 Scene unload。

父子關係主要服務工具、批量 preload / release 與 profiler；實際 Resource Registry 仍以 canonical resource + ref/pin tracking 為準。

#### Bundle Boundary

`AudioResidencyScope` 不等於 Bundle。

```text
Bundle
→ Physical Packaging / Download / Versioning

AudioResidencyScope
→ Runtime Residency / Lifetime Policy
```

Audio Scope 可以由 Bundle dependency 建立 preload set，但同一 Audio Asset 仍可被其他 Bundle / Scope 共用。

不得因 Scope 設計建立第二套 Asset Identity。

#### Hot Update / Generation Pinning

Audio Residency Scope 必須遵守既有 Bundle Generation Pinning。

例如：

```text
AudioClip Generation N
↓
Active Voice
```

Bundle 更新：

```text
Generation N+1 activated
```

則：

```text
New Play
→ N+1

Existing Voice
→ continues N until finished / released
```

只有舊 generation 的：

```text
Scope Ref
Voice Pin
Streaming Pin
Load Context
```

全部歸零後才允許回收。

#### Residency Priority

Scope 可帶 residency priority：

```text
AudioResidencyPriority
├─ Pinned
├─ Critical
├─ High
├─ Normal
├─ Low
└─ Speculative
```

範例：

```text
Global UI
→ Pinned

Player
→ Critical

Current Scene
→ High

Next Scene Preload
→ Normal

Far Zone
→ Low

Speculative Prefetch
→ Speculative
```

Memory Pressure eviction 可優先：

```text
Speculative
↓
Unused Far Zone
↓
Released Scene
↓
Low-priority Decoded PCM
```

但不得 evict Active Voice / Critical pinned data。

#### Memory Budget Integration

Audio Residency Scope 屬於既有全域 Audio Memory Budget 的 ownership / residency layer。

Profiler / Budget 至少分類：

```text
Audio Memory
├─ Event Metadata
├─ Resident PCM
├─ Compressed Audio
├─ Streaming Buffers
├─ Decode Buffers
├─ Voice State
└─ Backend Memory
```

Memory Pressure 可以：

```text
Release low-priority preload refs
↓
Evict unused decoded PCM
↓
Evict unused compressed data
↓
Keep metadata where useful
```

不得用「清空全部 Audio Cache」作為一般 Memory Pressure 策略。

#### Audio Residency Profiler

Profiler 新增：

```text
Audio Residency
├─ Scope Count
├─ Scope Name / Type
├─ Scope Priority
├─ Resource Count
├─ PCM Memory
├─ Compressed Memory
├─ Streaming Memory
├─ Active Voice Pins
├─ Streaming Pins
├─ Preload Count
├─ Evictable Memory
└─ Pending Release
```

Editor 可提供 Scope View，例如：

```text
Scope              PCM       Compressed    Voices
--------------------------------------------------
Global             12 MB       3 MB           5
Player              8 MB       4 MB           7
Scene:Map01        34 MB      18 MB          21
Boss01             15 MB       8 MB           0
Map02 Preload       6 MB      11 MB           0
```

#### Audio Residency CI / Validation

至少驗證：

- 同一 Audio Asset 被多個 Scope 引用時只存在 canonical runtime resource。
- Release 一個 Scope 不會卸載仍被其他 Scope 使用的 resource。
- Active Voice pin 能阻止 premature unload。
- Streaming pin 能阻止 streaming data 被提前釋放。
- `FadeOutAndRelease` 完成前 resource 不被回收。
- Released Scope 不接受新的 play request。
- Scene A → Scene B preload / release 不產生同步 audio load spike。
- Bundle Generation N / N+1 active voice pinning 正確。
- Memory Pressure 只 eviction 可安全回收的 Audio Resource。
- Audio Thread 不因 Scope Release 執行 blocking free / file IO / asset lookup。

#### Audio Residency Scope V1 Definition of Done

V1 至少完成：

```text
AudioResourceRegistry
+
AudioResidencyScopeHandle
+
Global / Scene / Character / UI Scope
+
Multiple-Scope Shared Resource Ref Tracking
+
Voice / Streaming Pin
+
Release Policy
+
FadeOutAndRelease
+
Residency Priority
+
Memory Budget Integration
+
Profiler
+
Bundle Generation Pinning
```

正式生命週期：

```text
Create Scope
↓
Preload / Load Resources
↓
Play
↓
Voice / Streaming Pins Resources
↓
Release Scope
↓
Optional Fade Out
↓
Remove Residency References
↓
Wait Active Pins
↓
Mark Resource Evictable
↓
Memory Budget / Cache Manager Evicts
```


## Audio Backend / FMOD Plugin

Audio System 採可替換 Backend 架構。

預設：

```text
miniaudio
→ Built-in
→ Default
```

可選：

```text
FMOD Plugin
→ Optional Professional Audio Backend
```

核心架構：

```text
Gameplay
↓
AudioManager
↓
IAudioBackend
├─ MiniAudioBackend
└─ FMODBackend
```

Gameplay / Runtime UI / Scene System 不直接依賴 FMOD API。

建議共用 API：

```cpp
AudioHandle Audio::Play(AudioEventID eventId);
void Audio::Stop(AudioHandle handle);
void Audio::SetVolume(AudioHandle handle, float volume);
void Audio::SetParameter(AudioHandle handle, StringView name, float value);
```

### FMOD Plugin

FMOD 以 Local Installed / Build-time Plugin 方式整合。

```text
Plugins/
└─ FMOD/
   ├─ Runtime/
   ├─ Editor/
   ├─ ThirdParty/
   └─ FMOD.plugin.json
```

Plugin 本身：

```text
不經 Remote Bundle
不從 CDN 下載
不作為 Native Code Hot Update
```

FMOD native library 由 App Build / Packaging 隨平台一起部署。

### FMOD Core / Studio

FMOD Backend 可分成：

```text
FMOD Core API
FMOD Studio API
```

若專案使用 FMOD Studio，可支援：

- Event
- Mixer
- Snapshot
- Parameter
- Adaptive Music
- Bus / VCA
- Bank
- Live Update
- Profiler
- Event Browser
- Event Preview

### Editor Integration

FMOD Editor Plugin 可提供：

```text
FMOD Studio Project
Bank Builder
Event Browser
Event Preview
Bank Dependency View
Live Update
Profiler Integration
```

Inspector 範例：

```text
Audio Event

Event:
[event:/Character/Attack ▼]

Auto Play     [ ]
Loop          [ ]
3D Spatial    [✓]
```

避免 Gameplay / Designer 手動輸入任意 Event 字串。

### FMOD Asset Type

FMOD Studio 產生的 Bank 應作為獨立 Asset Type 管理。

```text
FMOD Studio Project
↓
FMOD Build
↓
.bank
↓
Asset Database
↓
Bundle
```

Asset Database 應追蹤：

- Bank UUID
- Bank Version
- Event Dependency
- Platform Variant
- Bundle Assignment

### FMOD Bank 與 Bundle

FMOD Bank 屬於 Asset / Data，因此可以由 Bundle System 管理。

例如：

```text
audio_common.bundle
└─ Master.bank

map01_audio.bundle
└─ Map01.bank
```

因此：

```text
FMOD Native Library
→ Plugin / App Build

FMOD Bank
→ Bundle / Asset
→ 可使用 Remote Content Hot Update
```

Bundle 熱更新 Bank 時仍必須經：

```text
Remote Manifest
↓
Download
↓
Hash Verification
↓
Atomic Commit
↓
Activate
```

Runtime 必須處理 Bank Reload / Unload 的安全時機，避免仍有 Voice / Event Instance 使用舊 Bank。

### Build Settings

Project Build Settings 可提供：

```text
Audio Backend
[ MiniAudio ▼ ]

Options:
- MiniAudio
- FMOD
```

若選擇 FMOD：

- 驗證 FMOD Plugin 已安裝
- 驗證 SDK / Library 可用
- 驗證目標平台 Library
- 驗證 Bank Build Output
- 驗證 License / Third-party Notice 設定

若 FMOD 不可用：

```text
Build Fail
→ 明確錯誤
```

不可靜默 fallback 而造成音效內容不一致。

### Backend Boundary

FMOD Native Type 不應暴露到 Gameplay：

```text
FMOD::Studio::EventInstance
FMOD::Sound
FMOD::Channel
```

只能存在於：

```text
FMODBackend
FMOD Plugin Internal
```

上層統一使用：

```text
AudioHandle
AudioEventID
AudioBusID
AudioParameter
```

### Platform Support

FMOD Plugin 只在實際 SDK 支援且專案啟用的平台編譯。

目標：

```text
Windows
macOS
Android
iOS
```

平台 Backend / Library 路徑由 Plugin Build Script 管理。

### Memory / Streaming

FMOD Backend 必須接入 Engine Memory Budget / Streaming 診斷。

Profiler 至少顯示：

- Loaded Banks
- Bank Memory
- Active Voices
- Virtual Voices
- Streaming Audio
- Audio CPU Time
- Decode Cost
- Audio Heap / Buffer Usage

大型 BGM / Voice 應優先支援 Streaming，而不是全部一次載入 RAM。

### Audio Profiler Integration

Engine Profiler：

```text
Audio
├─ Backend
├─ Active Voices
├─ Virtual Voices
├─ Playing Events
├─ Loaded Banks
├─ Streaming
├─ Decode CPU
└─ Memory
```

若使用 FMOD，可額外提供跳轉 / 對應 FMOD Studio Profiler 的開發工具能力。

### Licensing

miniaudio：

```text
Built-in Default
→ 按實際鎖定版本維護 License / Notice
```

FMOD：

```text
Optional Third-party Plugin
→ 需依實際專案 / 商業用途確認 FMOD License
```

Third-party manifest 必須記錄：

- FMOD Version
- SDK Source
- License Type
- Runtime / Editor Components
- Redistribution Requirements
- Notice Requirements

Engine 不假設所有使用 FMOD 的專案都適用相同授權條件。

### Future Audio Backend

`IAudioBackend` 應允許未來新增：

```text
WwiseBackend
CustomAudioBackend
PlatformSpecificBackend
```

但 V1 不需要同時支援多個專業 Middleware。

### V1 Scope

V1 Audio：

```text
MiniAudioBackend
→ Built-in / Default
```

FMOD：

```text
Optional Plugin
→ Architecture Ready
→ Integration when selected
```

FMOD Plugin 完成條件：

- FMOD Backend 可註冊到 AudioManager
- Build Settings 可選 FMOD
- FMOD Native Type 不洩漏到 Public API
- FMOD Studio Bank 可進 Asset Database
- Bank 可被 Bundle 打包
- Remote Bundle 可更新 Bank
- Plugin Native Library 不走 Remote Update
- Editor 能瀏覽 / 選擇 Event
- Runtime 能播放 / 停止 Event
- Parameter 可設定
- Bank Load / Unload 正確
- Profiler 可看到基本 FMOD 狀態

## 四十一、Input Framework

Input 正式為獨立 Subsystem：

```text
engine::input
```

核心定位：

```text
Engine Input
≠ 預先定義好的 Move / Jump / Attack 系統
```

`Move / Look / Jump / Attack` 只可出現在 Sample / Documentation，不得成為 Engine Reserved Action。

Engine 定義的是：

```text
Device
Control
Raw Event
Device State
Binding Infrastructure
Action Value Type
Context
Routing Layer
Processor Infrastructure
```

Game / Developer 定義的是：

```text
Action Names
Action IDs
Action Maps
Bindings
Gameplay Commands
Gameplay Meaning
UI Input Routing Policy
```

### Input Architecture

```text
Platform Input Backend
↓
Timestamped RawInputEvent Queue
↓
Input Device Manager
↓
Device / Control State
↓
┌──────────────────────────────────────────────┐
│ Developer may consume Raw / Device API here │
└──────────────────────────────────────────────┘
↓
Optional Binding / Action System
↓
Input Context / Input Routing Layer
↓
Immutable InputSnapshot / TickInput
↓
┌───────────────┬────────────────┬───────────────┐
│               │                │               │
Gameplay      Runtime UI       Editor         Tools
│               │                │
Game-defined   UI Event       Dear ImGui
Command
```

Input 必須允許開發者停在任意層使用：

```text
Level 0 → Raw Input Event
Level 1 → Device / Control State
Level 2 → Optional Action Mapping
Level 3 → Game-defined Input Command
```

`InputAction System` 是高階便利層，不是強制 Gameplay Interface。

### Platform Backend

上層不得依賴：

```text
WM_KEYDOWN
WM_MOUSEMOVE
Android MotionEvent
UIKit Touch
NSEvent
GCController native object
```

平台 Backend：

```text
IInputBackend
├─ WindowsInputBackend
├─ MacOSInputBackend
├─ AndroidInputBackend
└─ IOSInputBackend
```

Platform backend 只負責將 OS event 正規化為 Engine Input Data。

### Input Device Model

V1 Device：

```text
InputDevice
├─ Keyboard
├─ Mouse
├─ Touchscreen
├─ Gamepad
└─ Pen / Pointer（平台支援時）
```

Future / Custom：

```text
Steering Wheel
HOTAS
Dance Pad
MIDI Controller
VR Controller
Arcade Controller
Custom Hardware
```

每個實體裝置都有：

```text
InputDeviceID
```

不得假設只有 Keyboard 0 / Gamepad 0。

### Input Control

底層統一為：

```text
InputDevice
└─ InputControl
```

概念 Control Path：

```text
<Keyboard>/w
<Keyboard>/space
<Mouse>/delta
<Mouse>/buttonLeft
<Gamepad>/leftStick
<Gamepad>/buttonSouth
<Touchscreen>/primaryTouch
```

Editor Binding UI 可使用 `InputControlPath`，Runtime Cook 後使用穩定 ID / compact binding data。

### Raw Input Event

概念資料：

```cpp
struct RawInputEvent
{
    InputDeviceID device;
    InputControlID control;
    InputEventType type;
    uint64_t timestamp;
    InputValue value;
};
```

Raw event 必須保留 timestamp，不允許 Platform callback 直接呼叫 Gameplay / Zig。

流程：

```text
OS Callback
↓
Bounded RawInputEvent Queue
↓
Input::BeginFrame
↓
Device State Update
↓
Action Resolve
↓
Immutable Snapshot
```

### Low-level Device API

開發者可完全繞過 Action System，直接取得：

```text
KeyboardState
MouseState
GamepadState
TouchSnapshot
DeviceSnapshot
RawInputEvent Stream
```

例如自訂：

- 格鬥遊戲 Command Buffer。
- 節奏遊戲 timestamp judgement。
- 特殊輸入硬體。
- 自訂 RTS / Simulation input model。

低階自由度不代表大量跨 ABI 小呼叫；Zig / other gameplay language 仍優先使用 batch snapshot / data view。

### Keyboard Physical Input ≠ Text Input

正式分離：

```text
PhysicalKey / LogicalKey
→ Gameplay / Shortcut

Text Input / IME
→ Text Editing
```

`InputField` 不可透過 KeyDown 自己拼文字。

IME 至少支援：

```text
CompositionStart
CompositionUpdate
CompositionEnd
TextCommit
```

需支援繁中 / 簡中 / 日文 / 英文輸入法 composition。

### Input Action System（Optional）

Engine 提供：

```text
InputAction
InputActionID
InputActionMap
InputBinding
InputProcessor
InputInteraction
InputContext
```

但不提供固定 Gameplay Action enum。

禁止：

```cpp
enum class EngineInputAction
{
    Move,
    Jump,
    Attack
};
```

Authoring 名稱可以是：

```text
"character.walk"
"camera.orbit"
"vehicle.throttle"
"spell.cast"
"editor.frame_selection"
"custom.foo"
```

Cook / Runtime 使用：

```cpp
struct InputActionID
{
    uint32_t value;
};
```

### Input Action Value Type

Engine 只定義資料型態：

```text
InputActionValueType
├─ Button
├─ Axis1D
├─ Axis2D
└─ Axis3D
```

不定義 Gameplay 語意。

例如：

```text
ship.throttle    → Axis1D
camera.orbit     → Axis2D
editor.move      → Axis3D
spell.cast       → Button
```

### Binding / Processor / Interaction

Binding：

```text
Simple Binding
Composite Binding
Modifier / Chord Binding
```

Engine 內建 Processor：

```text
DeadZone
Normalize
Scale
Invert
Clamp
ResponseCurve
Sensitivity
```

Analog Dead Zone 至少支援：

```text
Axial
Radial
```

開發者可註冊自訂 `IInputProcessor`。

Interaction 可提供常用：

```text
Press
Release
Hold
Tap
MultiTap
Repeat
```

但遊戲可自行實作 Charge / ComboWindow / RhythmTiming 等，不強迫走 Engine Interaction。

### Action Source Resolve

同一 Action 可同時綁多種裝置：

```text
Keyboard
Mouse
Gamepad
Touch
VirtualControl
CustomDevice
```

多來源 Resolve Policy：

```text
InputSourceResolvePolicy
├─ LatestActive
├─ MaxMagnitude
├─ Priority
├─ AdditiveClamp
└─ Custom
```

不同 physical source 可先套自己的 processor，再進 Action Resolver。

例如：

```text
Mouse Delta
↓ MouseSensitivity

Gamepad RightStick
↓ DeadZone / Curve / Sensitivity

Touch Look Region
↓ TouchSensitivity

→ camera.look
```

不得直接把 raw Mouse Delta 與 raw Stick 值生硬相加。

### Multiple Devices Simultaneously

正式 Contract：

```text
Multiple Input Devices
→ May be active simultaneously
```

不是：

```text
One Active Device at a time
```

同一個 `InputUser` 可同時擁有：

```text
Keyboard
+
Mouse
+
Gamepad
+
Touchscreen
+
Virtual Controls
```

例如同一 frame：

```text
Keyboard WASD
→ movement action

Mouse Delta
→ camera action

Gamepad Button
→ skill action
```

全部可同時有效。

```text
Device Activity
≠ Device Exclusivity
```

`LastActiveDevice` 僅作 UI Prompt / Glyph / Presentation Hint，不得自動停用其他裝置。

### InputUser / Local Multiplayer

正式：

```text
Physical Device(s)
↓
InputUser
↓
Action Map / Routing
↓
Player
```

例如：

```text
InputUser 0
├─ Keyboard
├─ Mouse
└─ Gamepad #1

InputUser 1
└─ Gamepad #2
```

一個 User 可以擁有多個 Device；Device disconnect 只清除該 Device state，不應重置其他 Device。

### Gamepad

Gamepad 上層使用 logical control：

```text
South
East
West
North
LeftStick
RightStick
LeftTrigger
RightTrigger
```

UI Glyph 可依實際 Device Family 顯示 Xbox / PlayStation / Nintendo 對應圖示。

### Mouse / Cursor

正式支援：

```text
CursorMode
├─ Normal
├─ Hidden
├─ Confined
└─ Locked
```

Relative Mouse Motion 與 Absolute Cursor Position 必須分離。

FPS / Camera 使用 platform-supported relative motion，不以 `currentPosition - previousPosition` 假造。

### Multi-touch

Multi-touch 為 V1 核心能力。

```text
Touchscreen
↓
Multiple Independent Pointer Streams
↓
PointerID 0..N
```

每一個 Touch 至少保存：

```text
PointerID
Position
Delta
StartPosition
TouchPhase
Pressure（若支援）
Timestamp
CaptureOwner
```

正式：

```text
TouchPhase
├─ Began
├─ Moved
├─ Stationary
├─ Ended
└─ Cancelled
```

`PointerID` 在 Began → Ended / Cancelled lifetime 內必須穩定；不得用 Touch array index 當永久 identity。

Touch device capability：

```text
HasTouch
MaxTouchPoints
HasPressure
HasStylus
```

不寫死固定最大手指數，由平台 capability 回報。

### Per-pointer Capture / Ownership

Pointer Capture 必須是 per `PointerID`：

```text
Pointer #0 → VirtualJoystick
Pointer #1 → CameraLookRegion
Pointer #2 → SkillButton
```

正式：

```text
Each Pointer
→ One Owner at a time
```

不是：

```text
All Touch
→ One Global Owner
```

Widget 可宣告：

```text
PointerAcceptance
├─ PrimaryOnly
├─ Single
└─ Multiple
```

UI / Gameplay / WebView / Virtual Control 必須遵循 single-owner / no-duplicate-delivery contract。

### Gesture

Gesture 建立在 Pointer Stream 之上：

```text
Raw Touch
↓
Pointer Events
↓
Gesture Recognizer
↓
Gesture Events
```

可支援：

```text
Tap
DoubleTap
LongPress
Pan
Swipe
Pinch
Rotate
TwoFingerPan
```

Advanced Gesture 可視 V1 時程條件式納入，但底層 Multi-touch / PointerID / Capture 必須先完成。

### Virtual Control Framework

Virtual Control 是 Runtime UI 與 Input System 之間的正式輸入來源：

```text
Touch / Pointer
↓
Runtime UI
↓
VirtualControl
↓
Raw Virtual Value or User-defined InputAction
↓
Game-defined Input Logic
```

正式類型：

```text
VirtualControl
├─ VirtualJoystick
├─ VirtualButton
├─ VirtualDPad
├─ VirtualTrigger
└─ VirtualTouchRegion
```

Virtual Control 不可綁死任何 Gameplay 語意。

禁止：

```text
VirtualJoystick = Move
VirtualButton = Attack
```

正式：

```text
VirtualJoystick
→ Raw Axis2D output
或
→ User-selected Axis2D Action

VirtualButton
→ Raw Button output
或
→ User-selected Button Action
```

可用於：

```text
character.move
camera.orbit
vehicle.steering
spell.direction
menu.radial_select
custom.foo
```

### VirtualJoystick

支援：

```text
VirtualJoystickMode
├─ Fixed
├─ Floating
└─ Dynamic
```

輸出：

```text
Vec2 [-1, +1]
```

處理流程：

```text
Touch Position
↓
Relative to Joystick Center
↓
Inner Dead Zone
↓
Clamp Radius
↓
Normalize / Remap
↓
Response Curve
↓
Axis2D Output
```

Constraint：

```text
Circular
Square
Horizontal
Vertical
```

一般角色搖桿預設 Circular，避免 diagonal magnitude > 1。

支援：

```text
ActivationRegion
RecenterOnRelease
Visual Return Tween
```

放手時 Gameplay Output 必須立即回零；Thumb 視覺回中心可另外 Tween。

### VirtualButton / VirtualDPad / VirtualTouchRegion

`VirtualButton` 可輸出：

```text
Pressed
Held
Released
```

並可支援 Hold / Repeat / Charge source semantics，但最終 Gameplay 意義由使用者定義。

`VirtualDPad` 輸出 Axis2D。

`VirtualTouchRegion` 可為透明 UIElement，用於：

```text
Camera Look
Drag Direction
Touch Delta
Custom Gesture Region
```

### Virtual Controls + Multi-touch

Virtual Control 必須完整支援 Multi-touch / Pointer Capture。

例如：

```text
Finger #0 → Left VirtualJoystick
Finger #1 → Right Camera Look Region
Finger #2 → Skill Button
Finger #3 → Another Skill Button
```

可同時工作。

Virtual Controls 本身是 Runtime UI Element，因此自然支援：

```text
Anchor
Safe Area
UIScalePolicy
Orientation Layout
Visibility Policy
```

可選：

```text
VirtualControlVisibilityPolicy
├─ Always
├─ TouchOnly
├─ Auto
└─ Manual
```

`Auto` 只改 Presentation / Visibility，不改其他 Device 是否可用。

### Virtual Control User Override

開發者可允許玩家自訂：

```text
Position
Scale
Opacity
Joystick Radius
Sensitivity
DeadZone
```

保存至 User Input Profile，而非修改原始 UI / Input Asset。

### Input Context

`InputContext` 控制目前哪些 action map / routing layer / input route 活躍。

例如：

```text
Gameplay
↓
Inventory
↓
ModalDialog
```

Context Stack 可提供：

```text
InputConsumePolicy
├─ PassThrough
├─ ConsumeMatched
└─ ConsumeAll
```

Input Context 解決「現在誰有資格接收輸入」，不等同 UI Input Matrix。

### Input Layer / Routing Layer

正式加入粗粒度：

```text
InputLayerID
```

可視為：

```text
Input Routing Layer
```

用途是決定 Input 可以 route 到哪些 UI Layer / interaction domain。

它不是 InputAction。

```text
InputAction
→ 細粒度「要做什麼」

InputLayer
→ 粗粒度「可以送到哪裡」
```

不要建立：

```text
MoveInput
JumpInput
AttackInput
Skill1Input
...
```

這種 action-per-layer 模式。

Input Layer 名稱完全由專案定義，例如：

```text
PlayerGameplay
PlayerUI
TouchGameplay
WorldInteraction
Debug
Player1
Player2
CustomFoo
```

### UI Layer

UI Layer 也由專案定義，負責 Input Routing Target，不等同 Render Layer。

```text
UI Render Layer
≠
UI Input Layer
```

例如：

```text
HUD
Menu
Modal
WorldUI
Overlay
Debug
VirtualControls
P1_UI
P2_UI
```

不要為每一個畫面建立一個 UI Layer；Inventory / Shop / Settings 可共用 `Menu`。

### UI Input Interaction Matrix

正式加入類似 Unity Physics Layer Collision Matrix 的 Project-level Editor 設定，但 Input → UI 是有方向性的，所以資料不是對稱矩陣。

Editor 顯示完整：

```text
UI Input Interaction Matrix

                         UI Layer
                 HUD   Menu   Modal   WorldUI   Debug   WebView
Input Layer
────────────────────────────────────────────────────────────────
PlayerInput        ✓      ✕       ✕        ✓        ✕       ✕
MenuInput          ✕      ✓       ✓        ✕        ✕       ✓
TouchGameplay      ✓      ✕       ✕        ✕        ✕       ✕
DebugInput         ✓      ✓       ✓        ✓        ✓       ✕
WorldInteract      ✕      ✕       ✕        ✓        ✕       ✕
```

語意：

```text
Input Layer → UI Layer
```

不是：

```text
Layer A ↔ Layer B
```

因此 Editor 外觀可類似 Unity Physics Matrix，但不可只儲存對稱半矩陣。

### UI Input Matrix Runtime Representation

Authoring：

```text
InputLayer × UILayer Matrix
```

Cook 後：

```text
InputLayerID
→ Allowed UILayerMask
```

概念：

```cpp
struct UIInputLayerRule
{
    UILayerMask allowedLayers;
};
```

若 UI Layer 數量在 64 以內，可直接：

```cpp
using UILayerMask = uint64_t;
```

Runtime 判斷為 bit test，不跑字串 / List search。

### UI Input Routing Pipeline

正式流程：

```text
Platform Pointer / Navigation
↓
engine::input
↓
Input Context Stack
↓
Active InputLayer
↓
UI Input Interaction Matrix
↓
Allowed UILayerMask
↓
UIDocument Input Priority
↓
PassThrough / ConsumeOnHit / BlockBelow
↓
Hit Test
↓
Pointer Capture
↓
Capture / Target / Bubble
↓
UIElement
```

Matrix 解決：

```text
Can this Input Layer affect this UI Layer?
```

Priority / Policy 解決：

```text
If multiple UI documents are eligible, who receives first?
```

### UIDocument Input Policy

每個 UIDocument 至少可設定：

```text
UILayer
InputPriority
InputPolicy
ReceivePointer
ReceiveNavigation
```

Policy：

```text
UIInputLayerPolicy
├─ PassThrough
├─ ConsumeOnHit
└─ BlockBelow
```

例如 Modal：

```text
UILayer = Modal
InputPriority = 1000
InputPolicy = BlockBelow
```

可讓下層 HUD / Menu 繼續 Render，但不接收 Input。

```text
Visible
≠
Interactive
```

### Project Matrix + Local Override

UI Input Matrix 為 Project Default。

特殊 UIDocument 可選：

```text
Input Routing Override
├─ UseProjectMatrix
├─ OverrideAllow
└─ OverrideDeny
```

Override 應視為例外工具，不鼓勵大量使用，避免 routing rule 再次散落到各 UI Asset。

Editor Inspector 必須顯示 effective rule 來源。

### Local Multiplayer + UI Matrix

Matrix 可處理多人 UI 隔離：

```text
                  P1_UI   P2_UI   SharedUI
P1Input              ✓       ✕        ✓
P2Input              ✕       ✓        ✓
KeyboardDebug        ✓       ✓        ✓
```

因此不同玩家的 Gamepad / Keyboard route 可直接被限制在指定 UI domain。

### WorldSpace UI Routing

WorldSpace UI 同樣走 Matrix：

```text
Camera Ray
↓
InputLayer
↓
UI Input Matrix
↓
Allowed WorldUI Documents
↓
Nearest Valid Surface
↓
Local UI Hit Test
```

可建立例如：

```text
WorldInteract → WorldUI
MenuInput     → Menu
```

### WebView Routing

`WebViewElement` 也必須映射到 UI Input Layer / routing policy。

若 Matrix / Context 使 WebView 不可互動，Native backend 需同步：

```text
Disable Hit Test
或
Hide / Suspend Interaction
```

不能只讓 Engine Hit Test 忽略，卻讓 OS Native View 繼續收到 Pointer。

Native WebView 的平台限制仍遵循其 backend capability。

### UI / Gameplay Focus

正式區分：

```text
Window Focus
Keyboard Focus
UI Focus
Pointer Capture
Gameplay Input Focus
```

例如 Text InputField 取得 Keyboard Focus 時，`W` 應進文字輸入，不應同時驅動 Gameplay Action，除非 Context / Routing Policy 明確允許。

### Frame Snapshot / Fixed Tick

Render Frame 與 Simulation Tick 分離：

```text
Timestamped Raw Events
↓
Frame Input Snapshot
↓
Simulation Tick Sampling
↓
TickInput / Game-defined InputCommand
```

短暫 Press / Release 不得因落在兩個 Fixed Tick 之間而遺失。

`InputSnapshot` 在 current frame / tick 內為 Immutable。

### Game-defined Input Command

Engine 不固定 `PlayerInputCommand` schema。

例如 RPG：

```text
RPGPlayerInput
├─ movement
├─ aim
├─ dodge
├─ normalAttack
└─ requestedSkill
```

Vehicle：

```text
VehicleInputCommand
├─ steering
├─ throttle
├─ brake
└─ handBrake
```

RTS 可完全不經 Character Framework。

正式：

```text
Input Framework
≠ Character Framework
```

Character 遊戲可自行接：

```text
Game-defined Input
↓
CharacterIntent
↓
CharacterMotor
↓
CharacterController
```

AI / Replay / Network 也可以產生同樣的 game-defined command / CharacterIntent。

### Zig / Stable C ABI

Zig 可使用兩種資料層：

```text
Resolved Action Snapshot
或
Device Snapshot / Raw Event Batch
```

但必須維持 Batch-first：

```text
Native Input
↓
POD Snapshot / Span
↓
Stable C ABI
↓
Zig
```

避免每 frame 大量：

```text
Input_GetAction("...")
Input_IsKeyDown(...)
```

跨 ABI 小呼叫。

### Rebinding / Input Asset

Action Mapping 可走 Asset System，例如：

```text
Default.input.json
UI.input.json
Vehicle.input.json
```

流程：

```text
JSON
↓
Engine JSON Framework
↓
Schema Validation
↓
Typed Input Mapping
↓
Runtime
```

User Rebind 採：

```text
Default Binding
+
User Override
```

不直接修改原始 Asset。

### Device Hot Plug / Suspend / Resume

正式事件：

```text
DeviceConnected
DeviceDisconnected
```

Disconnect 必須清除該 device Held / Pointer / Capture state，不造成 stuck input。

Mobile background：

```text
Background
↓
Cancel Active Pointer / Gesture
↓
Clear unsafe Held State
↓
Suspend
```

Resume：

```text
Re-enumerate Devices
↓
Rebuild Device State
```

### Haptics

Device subsystem 提供：

```text
Gamepad Rumble
Mobile Vibration
```

API 可抽象：

```text
PlayHaptic(device, pattern)
StopHaptic(device)
```

Virtual Control 可 request haptic，但實際輸出仍走 Device / Haptics backend。

### Input Recording / Replay Foundation

建議 V1 建立基礎能力：

```text
Resolved Game-defined InputCommand / TickInput
↓
Recorder
↓
Replay
```

優先記錄 resolved input command，而非平台-specific raw keyboard message。

用途：

- Bug reproduction。
- Character Controller test。
- Automated gameplay test。
- Network prediction / rollback foundation。
- AI / human control comparison。

### Input Debugger / Profiler

Editor 至少可檢視：

```text
Connected Devices
Device Controls
Raw Events
Action Values
Active Contexts
Active Input Layers
UI Input Matrix Result
Consumed / Blocked Route
Pointer Captures
Focus Owner
InputUser Assignments
Gamepad Dead Zones
Virtual Control Values
LastActiveDevice
```

Routing Debug 需能回答：

```text
Why did this UI receive the input?
Why was this UI blocked?
Which Context / InputLayer / Matrix rule / Priority decided it?
```

### Input Namespace

正式：

```cpp
engine::input
```

必要時：

```cpp
engine::input::device
engine::input::action
engine::input::gesture
engine::input::haptics
```

避免過深 namespace。

Virtual UI Widget 位於：

```cpp
engine::ui::VirtualJoystick
engine::ui::VirtualButton
engine::ui::VirtualDPad
engine::ui::VirtualTouchRegion
```

其輸出進 `engine::input`，不直接呼叫 Gameplay。

### Input V1 Scope

```text
✓ Platform Input Backend abstraction
✓ RawInputEvent + Timestamp
✓ InputDevice / InputControl / DeviceSnapshot
✓ Keyboard / Mouse / Touchscreen / Gamepad
✓ Simultaneous Keyboard + Mouse + Gamepad + Touch
✓ Custom Device registration foundation

✓ Optional InputAction System
✓ User-defined InputAction / InputActionMap
✓ Button / Axis1D / Axis2D / Axis3D
✓ Binding / Composite / Modifier
✓ Processor / Custom Processor
✓ Context Stack
✓ Rebinding / User Override

✓ Multi-touch
✓ Stable PointerID lifetime
✓ Per-pointer Capture / Ownership
✓ PointerAcceptance policy
✓ Text Input / IME
✓ Cursor Mode / Relative Mouse

✓ VirtualJoystick
✓ Fixed / Floating / Dynamic
✓ VirtualButton
✓ VirtualDPad
✓ VirtualTouchRegion
✓ Virtual Controls + Multi-touch
✓ Safe Area / UIScale integration
✓ User Virtual Control layout override

✓ InputLayer / Routing Layer
✓ Project UI Input Interaction Matrix
✓ Matrix Cook → UILayerMask bitset
✓ UIDocument InputPriority / Policy
✓ Project Matrix + exceptional local override
✓ Local Multiplayer UI routing
✓ WorldSpace UI routing
✓ WebView interaction routing

✓ InputUser / Multi-device assignment
✓ Device Hot Plug
✓ Haptics foundation
✓ Frame / Fixed Tick Snapshot
✓ Zig Batch Snapshot / Stable C ABI
✓ Input Debugger / Routing Debugger
✓ InputCommand recording / replay foundation
```

Advanced Gesture Recognizer：

```text
△ Full advanced gesture library depending on V1 schedule
```

但 Multi-touch / PointerID / Capture foundation 為 V1 必做。

### Input Definition of Done

V1 至少驗證：

- Engine 沒有 Reserved `Move / Jump / Attack` Action。
- Developer 可完全不用 Action System，直接讀 Raw / Device Snapshot。
- Keyboard + Mouse + Gamepad 可在同一 InputUser、同一 frame 同時有效。
- Touch 支援多根 Pointer 同時獨立操作。
- VirtualJoystick + CameraLookRegion + multiple VirtualButtons 可同時工作。
- Pointer Capture 為 per PointerID，無 duplicate delivery。
- UI / Gameplay / WebView ownership rule 可觀測且無雙重消費。
- Input Context 與 InputLayer / UILayer routing 職責分離。
- UI Input Interaction Matrix 可在 Editor 中直接設定。
- Matrix Runtime Cook 為 compact mask，不在 hot path 做字串查詢。
- Modal UI 可 BlockBelow 而不影響下層 Render。
- Local Multiplayer 可限制 P1/P2 Input 只操作指定 UI Layer。
- WorldSpace UI 可依 Input Layer / UI Matrix 過濾。
- WebView 被 route deny 時 Native hit-test 同步被停用。
- Text Input / IME 不會誤觸 Gameplay physical key action。
- Device disconnect / app suspend 不留下 stuck Held state。
- Fixed Tick 不遺失短暫 Press / Release。
- Zig 使用 batch snapshot，不依賴大量 per-action ABI call。
- Input Debugger 能顯示 Context → InputLayer → Matrix → UIDocument → Element 的完整 routing 原因。

核心 Contract：

```text
Physical Input
≠ Gameplay Action

InputAction System
→ Optional Convenience Layer

Developer
→ May consume Raw / Device State directly

Multiple Devices
→ Simultaneously Active

Touchscreen
→ Multi-touch by Default

Each Pointer
→ Independent Owner / Capture

Virtual Controls
→ Generic Input Sources, not Gameplay-specific controls

InputAction
→ Fine-grained meaning

InputLayer
→ Coarse-grained routing category

UI Render Layer
≠ UI Input Layer

Input Layer → UI Layer
→ Project UI Input Interaction Matrix

Editor Matrix
→ Authoring friendly

Runtime Matrix
→ Precompiled UILayerMask / Bit Test

Game-defined InputCommand
→ Gameplay semantic boundary
```

## 四十二、Platform Layer

```text
Platform/
├─ Windows
├─ macOS
├─ Android
└─ iOS
```

負責：
- Window
- App Lifecycle
- Native Handle
- File System
- Clipboard
- Dialog
- Thread
- Input
- Touch
- Device Info
- Save Path
- Crash Path


## 四十三、Memory Budget

V0.x 即納入，不延後到上市前。

分類追蹤：
- Texture Memory
- Texture Streaming Resident Mips
- Terrain Memory
- Vegetation Instance / Cluster Memory
- Mesh Memory
- Animation Memory
- GPU Animation Pose Storage（Bone Animation Texture / Structured Buffer）
- VFX / Particle Memory（CPU Particle SoA、GPU Particle Pool、Spawn / Event / Indirect Args Buffer）
- Audio Memory
- UI Atlas
- GPU Buffer
- Render Target
- Transient Render Graph Resources
- Physics
- Script / Gameplay
- Asset Cache

VFXBudgetManager 的 Memory Budget 為此分類下的 Subsystem-level 細分追蹤，仍受本節全域 High / Emergency Watermark 策略約束，觸發時機與強度必須與其他分類一致，不得自成一套獨立於全域 Watermark 之外的判斷邏輯。

但兩者的「降級手段」不共用同一套語意，需分開執行：

```text
Discrete Resident Asset
（Texture / Terrain Chunk / Vegetation Cluster / GPU Animation Pose Storage）
→ 依 Evictable / Pinned / Priority / Last Used
→ 個別資源 Eviction（LRU-style unload）

VFX Particle Pool / Live Simulation
（CPU Particle SoA、GPU Particle Pool）
→ 無個別 Evictable / Pinned 資源可淘汰
→ 透過 VFXBudgetManager 既有 Priority Tier
   （Critical / Gameplay / Character / Environment / Cosmetic / Background）
→ 執行 VFX 自身降級動作表
   （Lower Spawn Rate / Lower Simulation Rate / Disable Collision-Distortion）
```

Global Watermark 只負責決定「何時、多嚴重」，實際執行降級的手段由各分類自己的既有機制負責；不得把 Evictable / Pinned 這種個別資源淘汰語意，套用在沒有離散可淘汰項目的 VFX Particle Pool 上。

Profiler 必須可顯示：
- Current
- Peak
- Budget
- Over Budget Warning

Mobile 必須有 Quality Tier / Memory Tier。

例如：
Low Memory Device
Medium Memory Device
High Memory Device

Asset Streaming 與 Texture Max Size 必須能依 Tier 調整。


## 四十四、Crash Reporting

V1 先做 Crash Infrastructure。

必備：
- Crash Log
- Stack Trace
- Build Version
- Platform
- GPU
- Renderer Backend
- Windows Graphics API（DX12 / Vulkan）
- Last Loaded Scene
- Last Render Pass
- Memory Snapshot 摘要

Windows：
- MiniDump

Apple：
- Crash Log / Symbolication 支援

Android：
- Native Crash Log

第三方 Crash Backend：
- V1 可不綁定
- 預留 Sentry / Backtrace / 自建服務 Adapter


## 四十五、自動化測試 / CI

V0.x 開始。

Unit Test：
- Math
- UUID
- Serialization
- Reflection
- Asset Import
- Resource Handle
- Container / Utility

Engine Test：
- Scene Load
- Prefab
- Animation
- Physics
- Save Migration

Renderer Test：
- Smoke Test
- Shader Compile Test（依 Dependency Graph 只編譯受影響 Variant）
- Golden Image / Image Regression（DX12 / Vulkan / Metal）
- Cross-Backend Golden Image Comparison
- Render Graph Validation
- GPU Resource Lifetime Test

CI：
- Windows DX12 Build
- Windows Vulkan Build
- Windows DX12 Smoke Test
- Windows Vulkan Smoke Test
- macOS Build
- Android Build
- Shader Compile
- Unit Test
- Asset Import Test

Feature / Module Stripping Build Matrix：

```text
PR CI
→ Representative Default Profile
→ Stripping dependency validation
→ Disabled + detected dependency must fail

Nightly / Scheduled CI
→ Minimal Profile Build
→ Max / Full Feature Profile Build
→ Representative Mobile Minimal Profile
→ Representative Desktop Full Profile

Release Branch
→ Shipping Profile
→ Required platform-specific Feature Matrix
→ Modular / Monolithic packaging validation
```

Minimal Profile 用於驗證：

- Optional Subsystem 沒有隱性硬依賴
- Disabled Plugin / Subsystem 不被 Compile / Link / Package
- 不產生被裁切 Feature 的 Asset / Shader Variant
- 最小 Runtime 可以成功啟動

Max / Full Feature Profile 用於驗證：

- Optional Subsystem 同時啟用時 Dependency Graph 正確
- Module Registration / ABI / Build Order 正確
- Feature 組合沒有因 Stripping Define 而產生編譯缺口

不要求每個 PR 跑完整 Feature 組合矩陣；PR 使用代表性 Profile，Minimal / Max Profile 至少定期在 Nightly / Scheduled CI 執行。實際頻率依 CI 成本與歷史失敗率調整。

iOS：
- 至少做編譯驗證
- 真機測試可獨立進行


## 四十六、裝置相容性測試矩陣

Android Vulkan 的正確性不能只依賴模擬器或 AI 推論。
V0.6 Mobile Milestone 必須建立實體設備矩陣。

最低 GPU Vendor 覆蓋：
- Qualcomm Adreno
- ARM Mali
- Imagination PowerVR（若最低支援市場仍有代表性設備）

每一類至少一台代表實機。
若專案進入商業發行階段，擴充為：
- Low Tier
- Mid Tier
- High Tier
- 近年 Flagship
- 最低支援 OS / Driver 邊界機

必測項目：
- Vulkan Instance / Device 建立
- Swapchain
- Descriptor Indexing
- Binding Tier Fallback
- Compute Shader
- Forward+ Light Culling
- ASTC
- MSAA
- Depth / Stencil
- Render Pass / Dynamic Rendering 路線
- Synchronization
- Suspend / Resume
- Background / Foreground
- Orientation Change
- Thermal Throttling
- Frame Pacing
- Memory Pressure
- GPU Crash / Device Lost
- 長時間 Stability

特別關注：
- Adreno Driver workaround
- Mali Tile-Based Rendering 效能
- PowerVR Tile-Based Rendering 行為
- Descriptor / Bindless 邊界
- Compute Workgroup 限制
- FP16 行為
- Texture Format 支援

所有廠商特定 workaround：
- 必須放入 Capability / Backend 層
- 必須附上 Device / Driver 條件
- 不得散落在 Gameplay / Material / Scene



## Terrain System

Terrain 為引擎正式內建模組，不以單一大型 Mesh 實作。

核心架構：

```text
Terrain
├─ Heightmap
├─ Chunk / Tile
├─ Quadtree
├─ Terrain LOD
├─ Layer / Splat Map
├─ Normal / Macro Texture
├─ Collision
├─ Streaming
└─ Vegetation Placement / Mask
```

功能：
- Heightmap Terrain
- Chunk-based Streaming
- Quadtree Spatial Partition
- Screen-space Error LOD
- Terrain Layer Blend
- Splat / Weight Map
- Height Blend
- Detail Normal
- Macro Variation
- Triplanar（Optional）
- Terrain Collision
- Terrain Hole（Future）
- Terrain Decal（Future）
- Virtual Texturing（Future）

LOD 原則：
- 不只用 Distance
- 以 Screen-space Error 為主要依據
- 支援 Geomorphing / Stitching，避免 Chunk LOD 裂縫與跳動

Terrain Render Path：
```text
Terrain Chunks
↓
Quadtree Query
↓
Frustum / Distance Culling
↓
LOD Selection
↓
Material / Layer Resolve
↓
GPU Draw
```

後續 GPU-driven：
```text
Terrain Candidate Chunks
↓
GPU Culling
↓
GPU LOD Selection
↓
Indirect Draw
```



### Terrain Streaming Asset Layout

Terrain Chunk 仍走統一 UUID + Asset Database，不建立第二套 Asset Identity；但 Build Package 採 Streaming-friendly Bundle Layout。

```text
map01/
├─ map01_core.bundle
├─ terrain/
│  ├─ terrain_00_00.bundle
│  ├─ terrain_00_01.bundle
│  └─ ...
└─ vegetation/
   ├─ vegetation_00_00.bundle
   ├─ vegetation_00_01.bundle
   └─ ...
```

每個 Terrain Chunk / Vegetation Cluster：
- 有自己的 UUID
- Asset Database 記錄 spatial coordinate / dependency
- Bundle 只是實體打包與 streaming granularity，不改變 Asset reference 模型

Runtime：

```text
Player / Camera Position
↓
Streaming Grid / Radius
↓
Required Chunk UUID Set
↓
Bundle Resolver
↓
Async Load / Unload
↓
Terrain Chunk + Collision + Vegetation Residency
```

Terrain Chunk 可拆為 Height Data、Render Patch、Material/Layer Data、Collision Heightfield Derived Data、Vegetation Placement/Cluster Data。Core Map Data 常駐；Terrain Chunk 依位置 residency；Vegetation 可更積極卸載；Texture Mip residency 由 Texture Streaming System 獨立管理。

## Vegetation / SpeedTree-like System

引擎提供 SpeedTree-like Runtime Vegetation System，
但 V1 不自製完整 SpeedTree 建模工具。

Tree Asset：

```text
Tree Asset
├─ Trunk Mesh
├─ Branch Mesh
├─ Leaf Mesh / Cards
├─ Materials
├─ Wind Parameters
├─ LOD0
├─ LOD1
├─ LOD2
└─ Billboard / Impostor
```

支援：
- Tree LOD
- Billboard / Impostor
- GPU Instancing
- Vegetation Cluster
- GPU Culling
- Indirect Draw（後續）
- Global Wind
- Branch Bend
- Leaf Flutter
- Instance Wind Phase
- Terrain Vegetation Mask
- Density Map
- Random Scale / Rotation
- Species Variation

植被分類：

```text
Large Tree
→ Mesh + LOD + Billboard

Bush
→ Mesh / Card + Instancing

Grass
→ Cluster / GPU-generated Instances
```

大量草木不得在 Scene 中建立數十萬個獨立 Node。

Forest Culling：

```text
Forest
├─ Cluster A
├─ Cluster B
└─ Cluster C

Cluster Frustum Culling
↓
Hi-Z Occlusion
↓
Instance Culling
↓
LOD Selection
↓
Indirect Draw
```

若未來支援第三方 SpeedTree Asset / Runtime：
- 必須單獨做格式 / SDK / License Review
- 不讓 Engine Runtime 核心依賴 proprietary format


## Spatial Culling / Spatial World

Scene Graph 不等於 Spatial Structure。

正式分成：

```text
Scene Graph
= 邏輯 / Transform / Prefab

Spatial World
= 空間查詢 / Culling

Render World
= 當 Frame 的可渲染衍生資料
```

推薦結構：

```text
Static Environment
→ BVH

Dynamic Entities
→ Spatial Hash / Uniform Grid

Terrain
→ Quadtree

Vegetation
→ Cluster
```

裁剪 Pipeline：

```text
All Renderables
↓
Layer / Visibility Mask
↓
Distance Culling
↓
Spatial Query
↓
Frustum Culling
↓
Occlusion Culling
↓
LOD Selection
↓
Visible Render List
```

CPU Stage：

```text
Visibility Mask
↓
Distance Culling
↓
Spatial Query
   ├─ Static BVH
   ├─ Dynamic Grid
   ├─ Terrain Quadtree
   └─ Vegetation Cluster
↓
Frustum Culling
↓
Coarse LOD
↓
Upload Candidate List
```

GPU Stage（後續）：

```text
Candidate Objects
↓
Hi-Z Occlusion
↓
Fine LOD Selection
↓
Instance Compaction
↓
Indirect Command Generation
↓
DrawIndirect
```

Occlusion：
- Hi-Z / Hierarchical Z
- Conservative Test
- Bounding Expansion
- Temporal Hysteresis
- Newly-visible object safeguard

室內場景可選：
- Room / Portal Culling

Bounding Volume：
- Bounding Sphere：快速 Reject
- AABB：主要精確測試
- OBB：只有必要物件使用


## Mesh LOD System

Mesh LOD 為引擎內建正式能力。

支援：

```text
LOD0 → Highest Detail
LOD1 → Medium
LOD2 → Low
LOD3 → Very Low / Billboard
```

LOD 選擇：
- 主要採 Projected Screen Size / Screen Percentage
- 不只依賴 Camera Distance
- 考慮物件尺寸、FOV、解析度

Asset Import：
- Manual LOD
- Auto LOD Generation
- LOD Validation

Importer：

```text
Mesh LOD

Generate LOD      [✓]

LOD0  100%
LOD1   50%
LOD2   20%
LOD3    5%

Screen Threshold
LOD0  0.20
LOD1  0.08
LOD2  0.02
```

Runtime：

```text
RenderObject
↓
LOD Selection
↓
MeshHandle
```

LOD 穩定機制：
- Hysteresis
- Dither Crossfade
- Avoid LOD oscillation

進階 LOD：

```text
Geometry LOD
+
Material LOD
+
Shadow LOD
+
Animation LOD
```

Skinned Mesh：
- Mesh Triangle LOD
- Skeleton / Bone LOD
- Animation Update Rate LOD
- 遠距離可降至 30Hz / 15Hz
- Very Far 可使用 Impostor

Shadow：
- Shadow Pass 可使用比 Main Camera 更低的 Mesh LOD
- 遠距離物件可停止 Cast Shadow

V1：
- CPU Screen-space LOD Selection

後續：
- GPU LOD Selection
- GPU Culling + LOD + Indirect Draw


## Texture LOD / Mipmap / Streaming

Texture LOD 為引擎內建核心能力。

V1 必做：
- Mipmap Generation
- GPU Automatic Mip Selection
- Mip Bias
- Platform Max Texture Size
- Quality Tier
- sRGB / Linear
- Normal Map Mip Handling

Importer：

```text
Texture Settings

Generate MipMaps   [✓]
Streaming          [✓]

Max Size           4096
Min Resident Mip   256
Mip Bias           0
Priority           Normal
```

Mipmap：

```text
4096
↓
2048
↓
1024
↓
512
↓
256
↓
...
```

Texture Streaming：

```text
Far
→ 256 / 512 mip resident

Nearer
→ load 1024

Near
→ load 2048 / 4096
```

Streaming 決策依據：
- Camera Distance
- Projected Screen Size
- Mesh LOD
- Texture Priority
- Current Quality Tier
- Memory Tier
- Current GPU Memory Budget

Runtime：

```text
Asset Bundle
↓
Async IO
↓
Decompress
↓
Staging / Upload Buffer
↓
GPU Texture
↓
Resource Registry
↓
TextureHandle
```

Streaming System：
- Async
- Budget-aware
- Eviction
- Residency Tracking
- Priority
- Preload
- Placeholder Texture
- Graceful Degradation

Texture LOD 與 Mesh LOD 可協作：

```text
Mesh LOD0 → 2K / 4K mip
Mesh LOD1 → 1K mip
Mesh LOD2 → 512 mip
Mesh LOD3 → 256 / Billboard
```

但不強制一對一綁定，以 Runtime projected size 與 memory budget 為最終依據。

Mobile：
- ASTC
- 嚴格 Memory Budget
- 避免一次載入不需要的最高 mip
- Background / Foreground 後重新評估 residency



## 四十七、Profiler

V1：
- FPS
- CPU Frame
- GPU Frame
- Draw Calls
- Triangle Count
- Texture Memory
- Texture Streaming Resident / Requested Mip
- Terrain Visible Chunks
- Terrain LOD Distribution
- Vegetation Visible Clusters / Instances
- Mesh LOD Distribution
- Texture Streaming Resident Mips
- Terrain Memory
- Vegetation Instance / Cluster Memory
- Buffer Memory
- Asset Memory
- Node Count
- UI Draw Calls
- Binding Tier
- Active Graphics API
- Render Pass Cost

後續：
- CPU Timeline
- GPU Pass Timeline
- Job System View
- Render Graph Inspector
- Asset Streaming Inspector
- Memory Budget Graph


## 四十八、Build Pipeline

Windows：
- x64
- DX12（Default）
- Vulkan（Selectable）
- BC Texture
- EXE
- Editor / Build Settings 可切換 Backend
- 命令列可覆寫 Backend

Android：
- ARM64
- Vulkan
- ASTC
- APK / AAB

macOS：
- Apple Silicon
- Universal（視需求）
- Metal
- .app

iOS：
- ARM64
- Metal
- ASTC
- Xcode Project



### Windows Graphics API 選擇策略

Editor：
```text
Project Settings
└─ Graphics
   ├─ Default API: Direct3D 12
   └─ Available APIs:
      ├─ Direct3D 12
      └─ Vulkan
```

Build Profile 可覆寫：
```text
Windows-DX12
Windows-Vulkan
```

Runtime Debug 可覆寫：
```text
-gameapi=dx12
-gameapi=vulkan
```

預設原則：
- Windows Release / Shipping：DX12 優先
- Android：Vulkan
- macOS / iOS：Metal
- 不使用 OpenGL fallback


## DX12 / Vulkan Windows 雙後端策略

Windows 正式支援：
- Direct3D 12
- Vulkan

預設：
```text
Windows Default Graphics API
→ DX12
```

切換方式：
```text
Editor Build Settings
[Graphics API]
- Direct3D 12   ← Default
- Vulkan
```

Runtime / Debug 命令列：
```text
-gameapi=dx12
-gameapi=vulkan
```

啟動策略：
1. 未指定 API 時，優先初始化 DX12
2. 若使用者 / Build 設定指定 Vulkan，直接初始化 Vulkan
3. Debug Build 可允許 DX12 初始化失敗後自動嘗試 Vulkan
4. Shipping Build 是否允許自動 fallback 由專案設定決定
5. Backend 選擇結果必須寫入 Log / Crash Report

RHI 正式 Backend：
```text
RHI/
├─ D3D12/
│  ├─ D3D12Device
│  ├─ D3D12Buffer
│  ├─ D3D12Texture
│  ├─ D3D12Pipeline
│  ├─ D3D12CommandList
│  ├─ D3D12DescriptorManager
│  ├─ D3D12SwapChain
│  └─ D3D12Fence
├─ Vulkan/
└─ Metal/
```

DX12 必須完整支援：
- Device / Adapter Selection
- Swap Chain
- Graphics / Compute / Copy Queue
- Command Allocator / Command List
- Fence / Synchronization
- Resource Barrier
- Descriptor Heap
- Root Signature
- Pipeline State Object
- Upload / Readback Heap
- Texture / Buffer
- Render Target / Depth Stencil
- Indirect Draw
- Timestamp Query
- Debug Layer
- DRED / Device Removed Diagnostics
- Pipeline Cache / PSO Cache
- Slang -> DXIL
- Golden Image / GPU Capture Test

Windows DX12 與 Vulkan 必須共用：
- Scene
- Render World
- Render Graph
- Material
- Shader Feature System
- Asset
- UI
- Culling
- Lighting
- Post Processing

不得出現：
- DX12 專用邏輯滲透到 Scene / Material
- Vulkan 專用 layout 滲透到 Renderer 高層
- 為兩個 Backend 維護兩套 Gameplay / Material 資料


## 五十、效能策略

CPU：
- Job System
- Data-Oriented
- SoA
- Dirty Update
- Batch
- Async Loading
- Minimize Allocations

Spatial / Visibility：
- Static BVH
- Dynamic Grid / Spatial Hash
- Terrain Quadtree
- Vegetation Cluster
- Frustum Culling
- Hi-Z Occlusion（後續）
- Screen-space LOD

GPU：
- Forward+
- Instancing
- Material Sorting
- Texture Compression
- Hybrid Bindless-first
- GPU Culling（後續）
- Indirect Draw（後續）
- Occlusion Culling（後續）
- Mesh LOD
- Terrain LOD
- Vegetation LOD
- Texture Mip / Streaming

Mobile：
- ASTC
- FP16 優先
- Bandwidth Optimization
- Tile-Based GPU Friendly
- 控制 Overdraw
- 減少 Render Target
- 可調畫質 Tier
- 可調 Memory Tier


## 五十一、Graphics Quality Tier

Low：
- Reduced Shadow
- No SSAO
- Lower Texture
- Limited Lights
- FXAA

Medium：
- Cascaded Shadow
- SSAO
- Bloom
- Standard PBR

High：
- Better Shadow
- Higher Texture
- TAA
- Better Reflection
- More Lights
- Advanced Post


## 五十二、V1 Scope Matrix

本表為 Engine Planning Document 的範圍規劃表，不代表實作進度。

符號定義：

```text
✅
→ 已列入 V1 規劃範圍（Committed Scope）
→ 不代表已完成實作、已通過測試，或已有可執行程式碼
→ 實際完成狀態以對應 Roadmap Phase 的 Gate / DoD 是否通過為準

❌
→ 明確排除於 V1 範圍
→ 非遺漏，而是刻意決策（理由應在對應章節說明，如 Networking）

△
→ 部分納入 / 條件式納入 / 延後至後續版本
→ 說明文字標示具體條件（例如「後續 GPU-driven 階段」「V2」）
```

任何項目的實際實作進度，應以 Roadmap（引擎階段性開發路線圖）對應 Phase 的 Gate 通過與否為準，本表僅表達規劃範圍，不作為進度追蹤依據。

Renderer DX12                    ✅ Windows Default
Renderer Vulkan                  ✅ Windows / Android
Renderer Metal                   ✅ macOS / iOS
Windows DX12/Vulkan Switch       ✅
OpenGL Backend                   ❌

Node / Component                 ✅
World / Scene Lifecycle            ✅
EditorWorld / PlayWorld Separation ✅
Multiple / Additive Scene          ✅
Persistent Scene                   ✅
Async Scene Load / Atomic Activate ✅
WorldCommandBuffer                 ✅
Cross-Scene Logical Reference      ✅
Scene Serialization / Migration    ✅
World Partition Foundation         ✅
Fixed Grid Streaming Cells         ✅
Loose Quadtree Spatial Index       ✅
Room / Portal Graph                ✅
Outdoor / Indoor Gateway           ✅
Streaming Source / Demand          ✅
Streaming Priority / Hysteresis    ✅
Offline HLOD Builder               ✅
HLOD Runtime Selection / Streaming ✅
Adaptive Quadtree Cell Generation  △ Future / V2
Octree / 3D Adaptive Partition     △ Future
SoA Component Pool               ✅
Render Graph                     ✅
Forward+                         ✅
Hybrid Bindless-first            ✅
Binding Fallback                 ✅

Editor                           ✅
Prefab                           ✅
Undo / Redo                      ✅
Runtime UI                       ✅
UIDocument / UIElement Tree       ✅
UIElement ≠ SceneNode             ✅
UI VirtualizedListView            ✅
UI JSON / Hot Reload              ✅
UI Data Binding / ViewModel       ✅
UI ScreenSpace                    ✅
UI WorldAnchored                  ✅
UI WorldSpace                     ✅
WebViewElement Native Overlay     ✅
Offscreen Texture WebView         △ V2 / Optional
Input Framework                   ✅
Raw / Device-level Input          ✅
Optional Action Mapping           ✅
Simultaneous Multi-device Input   ✅
Multi-touch / Per-pointer Capture ✅
Virtual Controls                  ✅
UI Input Interaction Matrix       ✅
InputLayer → UILayer Routing      ✅
Input Rebinding / User Override   ✅
Input Fixed-Tick Snapshot         ✅
Input Replay Foundation           ✅

Transform System                   ✅
Large-world Position Foundation    ✅
System Scheduler / Access DAG      ✅
Typed Engine Event Framework       ✅
Camera Framework                   ✅
Lighting / Shadow Framework        ✅
PostProcess Volume Framework       ✅
Navigation / Recast-Detour         ✅
Streaming NavMesh Tiles            ✅
AI Blackboard / Behavior Tree      ✅
AI Perception / Update LOD         ✅
GOAP                               △ Future / Plugin
Editor Document / Adapter Model    ✅
Nested Prefab / Variant            ✅
Prefab Structural Override         ✅
Transaction Undo / Redo            ✅
Stable Reflection Type/Property ID ✅
Generic Serialization Framework    ✅
Cooked Binary Runtime Blob         ✅
Asset Importer Registry / Local DDC ✅
Mesh Optimization / LOD Cook       ✅
VFS / Async IO                     ✅
Persistent World State / Save      ✅
ICU-backed Localization            ✅
Text Editing / IME Model           ✅
Platform Service Framework         ✅
Structured Async Logging           ✅
Persistent Allocator Framework     ✅
High-level TaskGraph                ✅
Timer Scheduler                     ✅
Audio Event Authoring               ✅
Animation Authoring / Graph Editor  ✅
Physics Authoring / Collision Matrix ✅
Project Manifest / Settings Layers  ✅
Runtime Developer Console           ✅

Terrain System                    ✅
Vegetation / SpeedTree-like       ✅ Runtime System
Spatial World / Culling           ✅
Mesh LOD                          ✅
Texture Mipmap LOD                ✅
Texture Streaming                 ✅
Hi-Z GPU Occlusion                △ 後續 GPU-driven 階段
Localization                     ✅
Save System                      ✅
Engine JSON Parse / Generate      ✅ yyjson backend + Engine abstraction
Data Table JSON Runtime Asset     ✅
Data Table Schema / Validation    ✅
Data Table Key System             ✅ UInt32 / UInt64 / String
Data Table Preprocess Pipeline    ✅
Data Table Runtime Containers     ✅ Rows + Index / View / Pool
DataTable Schema Codegen          ✅ Narrow DataTable-only codegen
Audio Residency Scope            ✅
Audio Scope Fade/Release Policy  ✅

Networking Framework             ❌
Low-level Network Interface      △ Future boundary / transport plugin

Crash Infrastructure             ✅
Cloud Crash Backend              △ Optional
Memory Budget System             ✅
Automated Tests                  ✅
CI                               ✅

Shader Slang/Vulkan              ✅
Shader Slang/Metal               PoC 後確認
Reflection Macro Metadata        ✅
Reflection Codegen               ❌ Future

FBX Runtime                      ❌
FBX Editor Import                ✅

VFX / Particle Framework          ✅
GPU Particle Simulation           ✅
Runtime Batch Fusion              ✅
VFX Async Compute                 △ 後續 GPU-driven 階段
Animation Graph / State Machine   ✅
GPU Vertex Skinning               ✅
Skinned Mesh Instancing           ✅
GPU Crowd Animation (BAT)         ✅
Compute Skinning                  △ V2
Physics / Jolt CPU Core           ✅
Character Controller Framework    ✅
Character Motor / Intent           ✅
Moving Platform Support            ✅
Root Motion Character Resolve      ✅
CPU Physics Batch Query           ✅
GPU VFX Collision                 ✅
GPU Cloth Foundation              ✅
GPU Debris                        △ Optional
GPU Deferred Physics Query        ✅ Interface / Conditional Runtime
GPU Broadphase                    △ Future / Profile-driven
GPU RigidBody World               △ Future R&D
Shading Model Framework           ✅ PBR / StylizedPBR / Anime / Vegetation / Water / Unlit


## 五十三、版本規劃

V0.1 - Engine 基礎
- CMake Build System
- EnginePCH / RendererPCH / EditorPCH
- PCH On / Off Build Validation
- Windows
- DX12 Backend Skeleton
- Vulkan Backend Skeleton
- DX12 預設 Backend Selector
- Vulkan 可切換
- Window
- RHI
- Triangle
- Texture
- Mesh
- Camera
- Node
- EntityID
- Component Pool
- Transform
- Scene
- Unit Test 基礎
- Memory Tracking 基礎

V0.2 - RHI / Shader PoC
- AI Code Gate 第一版正式啟用
- clang-format / clang-tidy CI
- DX12 Descriptor Heap / Root Signature
- Vulkan Resource Binding
- Windows DX12 / Vulkan Backend Parity Test
- Binding Tier
- Render Graph Skeleton
- Slang -> SPIR-V
- Slang -> Metal PoC
- Reflection Metadata
- Resource Registry
- GPU Resource Index

V0.3 - Editor
- Dear ImGui
- Hierarchy
- Inspector
- Scene View
- Asset Browser
- Transform Gizmo
- Scene Save / Load
- Undo / Redo

V0.4 - 真正 3D Renderer
- PBR
- Spatial World 基礎
- Static BVH / Dynamic Grid
- Frustum Culling
- Mesh LOD V1
- Texture Mipmap LOD
- Material
- Directional Light
- Shadow
- glTF / FBX Import
- Texture Import
- Render Graph
- Forward+

V0.5 - 可做遊戲
- Prefab
- Animation
- Physics
- Audio
- Input
- Runtime UI
- Play Mode
- Localization
- Save System

V0.6 - Mobile / Apple
- Android Vulkan
- Terrain Chunk / Quadtree 基礎
- Texture Streaming Mobile Validation
- Android 實體 GPU Vendor Compatibility Matrix
- Adreno / Mali / PowerVR（依市場需求）真機驗證
- macOS Metal
- iOS Metal
- Touch
- Lifecycle
- ASTC
- Mobile Memory Tier

V0.7 - 效能 / 工具
- GPU Instancing
- Terrain System
- Vegetation / SpeedTree-like Runtime System
- Vegetation Cluster
- Texture Streaming
- Mesh / Material / Shadow / Animation LOD 整合
- LOD
- Async Asset Loading
- Asset Bundle
- Job System
- Profiler
- Crash Infrastructure
- CI 強化

V0.8+
- Compute Culling
- Hi-Z Occlusion
- GPU LOD Selection
- Vegetation GPU Culling
- Terrain GPU-driven Rendering
- Indirect Rendering
- GPU Driven
- TAA
- Terrain
- Vegetation
- Particle
- NavMesh
- Advanced Shadow

V1.0
- Windows / macOS / Android / iOS
- 可完整 Build / 發布一款 3D 遊戲
- Renderer / Editor / Asset / UI / Physics / Audio / Animation / Localization / Save 基本完整
- Networking Framework 明確不包含於 V1



## Thread-Safe Job System / Frame Synchronization Lifecycle

Job System 採 Work-Stealing Scheduler。

Priority Class：

```text
High
→ Render Extraction / Frame-critical work

Normal
→ Animation / Physics / Gameplay jobs

Low
→ Background Asset IO / Import / Non-critical work
```

Priority 不代表允許飢餓；Scheduler 必須有 starvation prevention / aging policy。

### Task Dependency Model

Job 必須能描述：

```text
Task Handle
Dependency List
Priority
Frame Lifetime
Scratch Arena
Completion Fence
```

Frame Execution Graph：

```text
Frame Begin
↓
Input / Gameplay
↓
Physics / Animation Parallel Jobs
↓
Frame Barrier
↓
Render Extraction Parallel Jobs
↓
Render Graph Compile
↓
Command Recording / Submit
↓
Frame End Barrier
↓
Arena Reset
```

規則：

- Job 不得存取已超過 lifetime barrier 的 scratch memory
- Frame Arena reset 前，所有使用該 arena 的 jobs 必須 completed
- Render thread reuse frame slot 前必須確認 GPU fence
- Background IO Job 不得持有 frame-local pointer
- Worker-local arena 僅能由對應 Worker / Job ownership 範圍使用

CI / Development Assertion：

```text
Frame-memory Escape
Thread Ownership Violation
Use-after-reset
Dependency Cycle
Job Lifetime Violation
```

任一成立均視為 Architecture Contract failure。

## Streaming Asset Residency State Machine / Memory Pressure

Streaming Asset 必須具有明確 Residency State。

```text
Unloaded
↓
PendingDiskIO
↓
RAMResident
↓
PendingUpload
↓
VRAMResident
↓
Ready
```

可包含補充狀態：

```text
PendingUnload
Evicting
Failed
Cancelled
```

### Double Budget Control

獨立追蹤：

```text
RAM Budget
VRAM Budget
```

每類資產至少追蹤：

```text
Current
Peak
Budget
High Watermark
Emergency Watermark
Evictable
Pinned
Last Used
Priority
```

Watermark 不寫死成固定全平台數值；例如 85% / 95% 僅作初始建議，實際由 Platform / Device Tier Profile 決定。

High Watermark：

```text
Prefer:
LOD downgrade
Mip reduction
Streaming priority reduction
Evict cold optional resources
```

Emergency Watermark：

```text
Aggressive LRU eviction
Unload non-pinned resource
Reduce texture residency
Reduce animation / vegetation budget
```

目標是避免：

- Mobile Low Memory Kill
- OS Memory Pressure termination
- Desktop swap / paging storm
- VRAM overcommit stutter

Residency State Transition 必須是非同步且可取消，禁止 Gameplay 直接假設 `Load()` 呼叫完成後資產立刻 Ready。

## Unified Input System / Event Routing

完整 Input 架構以「四十一、Input Framework」為唯一權威定義。

本節只保留跨系統 routing 摘要：

```text
Platform Raw Event
↓
engine::input
↓
Input Context Stack
↓
InputLayer / Routing Layer
↓
┌─────────────────────────────┬─────────────────────────────┐
│                             │                             │
UI Input Interaction Matrix   Gameplay / Tool Routing
│                             │
Allowed UILayerMask           Game-defined Input Logic
│
UIDocument Priority / Policy
↓
UI / WebView / WorldSpace UI Hit Test
↓
Per-Pointer Ownership / Capture
↓
Capture / Target / Bubble
```

正式支援來源：

```text
Keyboard
Mouse
Touch / Multi-touch
Gamepad
Pen / Pointer
Virtual Control
Custom Device
```

重要 Contract：

```text
Action Mapping is Optional
Multiple Devices may be active simultaneously
One Pointer / Gesture Sequence → One Owner
UI Input Matrix is directional: InputLayer → UILayer
Native Overlay / WebView must not receive duplicate delivery
```

## Time / Tick Model

採：

```text
Fixed Simulation Tick
+
Variable Render Frame
```

Physics / deterministic-like simulation 使用 Fixed Delta。

概念：

```cpp
while (accumulator >= kFixedDeltaTime)
{
    PhysicsWorld::Step(kFixedDeltaTime);
    accumulator -= kFixedDeltaTime;
}

float alpha = accumulator / kFixedDeltaTime;
RenderWorld::ExtractTransforms(alpha);
```

RenderWorld 以：

```text
PreviousTransform
CurrentTransform
Interpolation Alpha
```

取得平滑 render transform。

規則：

- Physics Tick 與 Display Refresh Rate 解耦
- 120 / 144 Hz 顯示器不改變 physics step
- 防止 high-refresh physics jitter
- 必須限制單幀最大 catch-up steps，避免 spiral of death
- Background / pause / suspend 必須定義 accumulator reset / clamp policy
- TimeScale 與 FixedDeltaTime 分離管理

## Font Rendering Pipeline / Dynamic Glyph Atlas / MSDF

FreeType + HarfBuzz 負責：

```text
Font Rasterization
+
Text Shaping
```

### CJK

CJK 預設採：

```text
Dynamic Glyph Rasterization
↓
Dynamic Texture Atlas
↓
LRU Eviction
```

原因：

- CJK glyph 數量龐大
- 全量預計算 MSDF 會造成 Bundle 過大
- 多語系 fallback 字體需要動態 glyph residency

Atlas Size 例如 2048x2048 僅為 profile 預設，不寫死為架構限制。

應支援：

```text
Multiple Atlas Pages
LRU
Glyph Pinning
Prewarm
Fallback Font
Atlas Rebuild / Eviction
```

### Latin / Icon Font

可選：

```text
Precomputed MSDF
```

適用：

- Latin UI
- Icon Font
- 需要大範圍縮放的 glyph
- 高 DPI UI

Text System 可依 Font Asset / Locale 選擇：

```text
Dynamic Raster Atlas
MSDF
```

不強制所有語言使用同一 rendering mode。

## Crash Reporting / MiniDump / Symbolication

Crash Infrastructure 採 Backend abstraction，不把 Engine Core 綁死單一第三方 SDK。

```text
CrashService
↓
ICrashBackend
├─ Platform Native
├─ Crashpad-compatible Backend
└─ Future Telemetry Provider
```

平台目標：

```text
Windows
→ Minidump / PDB Symbol

macOS / iOS
→ Crash Report / dSYM Symbolication

Android
→ Native Tombstone / Native Crash Capture / symbol files
```

需處理：

```text
Fatal Signal
Unhandled C++ Exception
Engine Fatal
GPU Device Lost metadata
Last Log Ring Buffer
Build ID
App Version
Module List
```

Crash handler 內禁止執行非 async-signal-safe 的複雜工作。

Crash upload 採：

```text
Crash Capture
↓
Local Persist
↓
Next Launch
↓
User / Product Policy允許時
↓
Telemetry Upload
```

### Symbolication Pipeline

Shipping Build 必須保存：

```text
PDB
dSYM
Unstripped ELF / symbol mapping
Build ID
Module UUID
```

CI / Release Artifact Server 以 Build ID 對應 symbols。

Shipping package 可 strip symbols，但 symbol archive 不得遺失。

Crashpad / Breakpad 可作為候選實作，實際採用版本與授權需在第三方整合時確認。

## Terrain Virtual Texturing / Vegetation Wind Extension

### Terrain Virtual Texturing

Virtual Texturing 為大型 Terrain 的進階可選功能，不阻塞 V1 Terrain。

目標架構：

```text
Visible Terrain
↓
VT Feedback
↓
Feedback Resolve / Compute
↓
Requested Virtual Tiles
↓
Async Streaming
↓
Physical Texture Pool
↓
Page Table Update
```

RHI / Render Graph 負責：

- Feedback Buffer lifetime
- Compute Resolve Pass
- Physical Texture Pool resource state
- Async upload synchronization
- Page Table update scheduling

若平台支援 sparse resource，可利用平台能力；若不適合則允許 software-managed physical atlas fallback。

### Vegetation Wind

Vegetation wind 優先在 GPU vertex / compute path 完成：

```text
Global Wind Field
+
Procedural Noise
+
Pivot / Hierarchical Branch Data
↓
Vertex Displacement
```

避免 CPU 每幀更新大量 vegetation transform。

Wind Feature 必須參與 Shader Variant / Feature Stripping，但應控制 variant growth，優先使用 runtime parameter 取代不必要 static permutation。

## Systemic Risk Register

### Risk 1: Slang / Metal Compiler & Driver Divergence

Mitigation：

- Direct MSL 與 SPIRV-Cross MSL 雙路徑 CI 驗證
- Canonical Reflection consistency
- macOS / iOS 真機 Golden Image
- Build-time validated fallback policy
- 不允許獨立手寫 MSL fallback

### Risk 2: AI-generated Lifetime / Memory Bugs

Mitigation：

- clang-tidy memory / lifetime rules
- ASan / UBSan regression build
- Handle generation validation
- Frame / Pool / Arena boundary guard
- Allocator leak reporting
- Ownership contract review

### Risk 3: Mobile GPU Synchronization / UMA Coherency

Mitigation：

- Render Graph 自動推導 Resource State / Barrier
- RHI backend 控制 flush / invalidate
- Vulkan Validation Layers
- Metal API validation / GPU capture
- Android vendor device matrix
- RenderDoc / vendor profiler 作為開發診斷工具

自動化程度依工具與平台能力決定，不假設所有 profiler capture 都能完整 unattended CI。

### Risk 4: Remote Bundle File Lifetime / State Desync

Mitigation：

- Versioned cache directory
- No active bundle in-place replacement
- Active Manifest pointer switch
- SafeToDelete state
- mmap / FD / in-flight IO validation
- Pre-init deferred activation / cleanup

### Risk 5: Native WebView Z-order / Input Conflict

Mitigation：

- Native Overlay 明確標示
- Input Hit-Test Mode
- Viewport Sync
- Canvas z-order limitation warning
- Fullscreen Modal 自動 Suspend / Hide WebView
- Focus handoff

## PSO Cache / Pipeline Warmup Strategy

現代低階 API 下，Runtime PSO 建立可能造成明顯 frame jank，因此 PSO 建立與 cache 必須視為 Renderer 核心能力。

### Core Rule

```text
Hot Render Loop
→ 不允許隱式同步建立昂貴 PSO
```

建議流程：

```text
Scene / Prefab / Material Feature Scan
↓
Required Shader Variant Set
↓
Required PSO Key Set
↓
Load Compatible Pipeline Cache
↓
Async PSO Pre-create / Warmup
↓
Enter Gameplay
```

### PSO Key

PSO Key 至少包含：

```text
Shader Variant Key
Render Pass / Attachment Format
Depth / Stencil State
Raster State
Blend State
Vertex Layout
Topology
Sample Count
Binding Tier
Backend
Platform
Quality Tier
```

### Platform Cache Strategy

Pipeline cache 不視為跨裝置通用 binary。

DX12：

```text
Cached PSO / Pipeline Library
→ 依 driver / adapter / build compatibility 管理
```

Vulkan：

```text
VkPipelineCache
→ 必須驗證 vendorID
→ deviceID
→ pipelineCacheUUID
→ driver-compatible identity
```

Metal：

```text
MTLBinaryArchive
→ 以實際 MTLDevice / OS / build profile 相容性管理
```

因此：

```text
GameData/
└─ PipelineCache/
   ├─ DX12/
   ├─ Vulkan/
   └─ Metal/
```

實際 cache key 建議包含：

```text
Engine Build ID
Shader Compiler Version
Backend
GPU Vendor
GPU Device / Family
Driver / OS Version
Pipeline Cache UUID / equivalent compatibility token
Quality Tier
Binding Tier
```

若 cache 不相容：

```text
Ignore Cache
↓
Rebuild / Warmup
↓
Write New Compatible Cache
```

### Warmup

Loading Screen / Scene Transition：

```text
Gather PSO Requests
↓
Async Create
↓
Track Progress
↓
Gameplay Start
```

允許：

```text
Background Lazy Warmup
```

但不允許在高頻 draw path 中無限制 synchronous compile。

### Runtime Fallback

若 gameplay 中仍遇到未預熱 PSO：

```text
Development
→ Log + Hitch Marker + Capture Key

Shipping
→ Async create when possible
→ Temporary fallback material / skip draw / controlled stall policy
```

策略依內容類型決定，但必須被 Profiler / Telemetry 記錄。

### CI / Renderer Gate

追蹤：

```text
Total PSO Count
Warmup PSO Count
Runtime-created PSO Count
PSO Creation Time
Cache Hit Rate
Cache Miss Rate
Worst PSO Creation Time
```

Gate 可設定：

```text
Runtime synchronous PSO creation count
→ 必須低於 profile threshold
```

閾值以實測 baseline 校準，不先硬編固定數字。

## Android GPU Workaround Database

Vulkan Backend 內建：

```text
GPUWorkaroundDatabase
```

啟動時讀取：

```text
GPU Vendor ID
Device ID
Driver Version
API Version
Feature / Extension Set
```

再產生：

```text
Capability Profile
+
Workaround Flags
```

例如：

```text
Disable Dynamic Rendering
Force Conservative Barrier
Disable Specific Extension
Prefer Legacy Render Pass
Disable Descriptor Feature
Limit Async Compute
Adjust Present Mode
```

Workaround 必須隔離在：

```text
RHI/Vulkan/Capability
```

禁止 Gameplay / Renderer High-level 直接判斷 GPU 型號。

### Fallback Tier

```text
Tier A
→ Full Feature

Tier B
→ Reduced Feature / Workaround

Tier C
→ Restricted Compatibility Mode

Blacklisted
→ Refuse Launch / Show Unsupported Device
```

Database 必須：

- 有版本
- 可被 build-time 更新
- 由實機驗證結果產生
- 每條 workaround 有 issue / reason / affected range
- 避免以單一 vendor 名稱粗暴套用所有裝置

## OS Memory Pressure / Device Loss Recovery

Core Platform / Memory 建立：

```cpp
enum class MemoryPressureLevel
{
    Normal,
    Warning,
    Critical
};
```

事件：

```text
OnMemoryPressure(Level)
```

### Warning

```text
Evict cold texture mips
Evict unused mesh residency
Trim streaming cache
Release optional decoded audio buffers
Compact subsystem caches
```

### Critical

```text
Aggressive LRU eviction
Release unused bundles
Release transient caches
Reduce texture / animation / vegetation budgets
Release optional render targets
Force lower quality residency profile
```

注意：

Frame Arena 的 reset 仍必須遵守 frame/job lifetime，不能因 memory pressure callback 任意重置仍在使用的 arena。

### Device Lost / GPU Recovery

需要統一事件：

```text
OnDeviceLost
OnDeviceRestored
```

DX12：

```text
Device Removed / Reset
→ Capture diagnostics
→ Tear down GPU resources
→ Recreate device where supported
→ Rebuild Resource Registry backing objects
```

Metal / Mobile：

```text
Background / interruption / command failure
→ Preserve CPU-side asset identity
→ Recreate transient GPU state as needed
```

不是所有 GPU failure 都可恢復；不可恢復時走 crash / graceful restart policy。

所有 GPU Resource 必須能區分：

```text
Persistent Asset Identity
vs
Backend Native Object
```

以利 restore。

## Cache Line / False Sharing Protection

多執行緒控制結構必須避免 false sharing。

適用：

```text
Worker Queue Head / Tail
Worker Allocator Cursor
Hot Atomic Counter
Per-thread Statistics
Job Completion Counter
```

可使用：

```cpp
alignas(std::hardware_destructive_interference_size)
```

但不得把該值寫死為跨平台 ABI 常數。

原因：

```text
std::hardware_destructive_interference_size
→ implementation-defined
```

若編譯器 / 標準庫不提供，Engine 可使用 platform profile fallback，例如 64 bytes，但該 fallback 只作性能排列，不作 serialization / network / file ABI。

### Job Slice

SoA / Chunk partition 應避免多 Worker 寫入同一 cache line。

原則：

```text
Partition Boundary
→ align to element/cache-friendly granularity
```

但 Chunk Capacity 不強制單純等於 cache-line bytes 的倍數；應根據：

```text
Element Size
Write Pattern
SIMD Width
Cache Line
Batch Size
```

共同決定。

CI / Benchmark：

```text
False Sharing Microbenchmark
Worker contention counters
Cache miss / coherence profiling
```

僅對真正跨 thread 熱寫入資料做 padding，避免全面 padding 導致 working set 膨脹。

## Reflection Metadata Schema / AI Generation Boundary

Reflection 系統必須把：

```text
C++ Declaration Syntax
```

與：

```text
Canonical Reflection Metadata
```

分離。

### V1

V1 可保留輕量 Macro / constexpr registration，但：

- 巨集只做最薄的 annotation / registration
- 不把大量 serialization code 展開在巨集內
- 優先 C++20 constexpr / type traits
- Error message 必須保持可讀

### Canonical Metadata Schema

從第一版就定義統一 schema。

例如：

```json
{
  "type": "TransformComponent",
  "version": 1,
  "fields": [
    {
      "name": "position",
      "type": "Vector3",
      "flags": ["Serialize", "EditorVisible"]
    }
  ]
}
```

正式 schema 至少定義：

```text
Type ID
Type Name
Version
Base Type
Field ID
Field Name
Field Type
Array / Container
Serialization Flags
Editor Flags
Default Value
Range / Attribute
Migration Metadata
```

### Generation Pipeline

```text
V1 Macro / constexpr
        │
        ▼
Canonical Metadata
        │
        ├─ Serialization
        ├─ Inspector
        ├─ Asset Dependency
        └─ AI Tooling

V2 Clang Header Tool
        │
        ▼
Same Canonical Metadata Schema
```

因此未來換 AST generator：

```text
Producer Changes
Consumer Contract Does Not
```

CI 必須：

- Validate metadata schema
- Detect duplicate type/field IDs
- Detect incompatible schema migration
- Compare generated metadata deterministically

## Architecture Risk / Gate Matrix

| 模組系統 | 核心風險點 | 防護 / 驗證 Gate | 補充機制 |
| --- | --- | --- | --- |
| Renderer / RHI | Runtime PSO 建立造成 frame jank | Renderer Gate + runtime PSO creation metrics | Compatible disk PSO cache + loading-screen warmup |
| Vulkan Backend | Android GPU / Driver 差異 | Physical Device Validation Gate | `GPUWorkaroundDatabase` + capability override |
| Memory / Core | OS memory pressure / GPU loss | Memory Pressure + Device Recovery smoke test | `OnMemoryPressure` + `OnDeviceLost/Restored` |
| Threading / SoA | False sharing / cache contention | Threading benchmark / profiler checklist | cache-aware padding + partition strategy |
| Reflection / AI | Macro / generated metadata 漂移 | Metadata schema validation + clang-tidy | canonical JSON metadata contract |

## Shader CI / Fallback Final Policy

Shader compiler path comparison 不做 binary-equality 判定。

正式 Gate 比對：

```text
Canonical Reflection
Resource Mapping
Constant / Parameter Layout
Stage Visibility
Specialization Metadata
Argument Buffer Mapping
Representative Golden Image
```

Metal：

```text
Primary
Slang → Direct MSL → Apple Metal Compiler

Fallback
Slang → SPIR-V → SPIRV-Cross → MSL → Apple Metal Compiler
```

Fallback 選擇：

```text
CI / Build / Cook
→ validate both paths
→ choose validated path
```

禁止：

```text
Shipping Runtime
→ dynamic compiler-path switching
```

若 Direct MSL：

- Compile Fail
- Canonical Reflection mismatch
- Binding Contract mismatch
- Golden Image regression 超出 profile tolerance

則 Build/Cook 選用已驗證 SPIRV-Cross fallback。

不比較 DXIL / SPIR-V / MSL binary 是否相同。

## RHI Modern Dynamic Rendering Contract

Public RHI 僅曝露 modern dynamic rendering semantics：

```cpp
BeginRendering(const RenderingInfo& info);
EndRendering();
```

High-level Renderer / Render Graph 不感知：

```text
VkRenderPass
VkFramebuffer
ID3D12GraphicsCommandList*
MTLRenderPassDescriptor native lifetime
```

Backend mapping：

```text
Vulkan
→ Primary: vkCmdBeginRendering / Dynamic Rendering
→ Fallback: legacy render pass path only inside Vulkan backend capability/workaround layer

DX12
→ BeginRenderPass where supported / appropriate
→ backend may fall back to explicit RTV/DSV command sequence

Metal
→ transient MTLRenderPassDescriptor builder
→ MTLRenderCommandEncoder
```

正式規則：

- Public RHI contract 不暴露 legacy Vulkan RenderPass model
- Backend capability layer 可保留 compatibility fallback
- Android `GPUWorkaroundDatabase` 可以選擇 fallback path
- Render Graph 永遠只依賴 unified `RenderingInfo`
- Dynamic resolution / transient attachment reuse 不應要求 High-level 建立永久 framebuffer objects

## Bundle Versioned Storage / Generation Pinning

Remote Bundle 採：

```text
Versioned Storage
+
Bundle Generation
+
Load Context Pinning
```

Directory：

```text
Cache/
├─ characters/
│  ├─ v1.0.1/
│  │  └─ characters.bundle
│  └─ v1.0.2/
│     └─ characters.bundle
```

更新流程：

```text
Download v1.0.2
↓
Verify
↓
Create New Bundle Generation
↓
Validate dependency set
↓
Atomic Active Generation Switch
```

重要：

```text
Existing Load Context
→ pin old generation

New Load Context
→ use new generation
```

禁止在同一 Load Context 內任意混讀：

```text
Material → old bundle generation
Texture  → new bundle generation
```

以避免 Mixed-Version Dependency Graph。

Load Context 至少包含：

```text
Scene Load
Prefab Load Group
Streaming Cell Transaction
Bundle Dependency Resolution Transaction
```

舊 generation：

```text
RefCount > 0
→ remain valid

RefCount = 0
+ no mmap
+ no open FD
+ no in-flight IO
→ SafeToDelete
```

刪除可延後：

```text
Background Cleanup
或
Next App Pre-init
```

Active Manifest 僅保存「新 Load Context 預設使用哪個 generation」，不強迫既有 Context 即時切換。

## Frame Arena Reset Ownership

Frame Arena Reset 的核心條件是 lifetime safety，不綁死 Main Thread。

允許：

```text
Main Thread
或
FrameAllocatorManager
```

在 Life Barrier 後統一 Reset。

必要條件：

```text
All jobs using arena completed
No escaped pointers
Render extraction finished
Relevant CPU frame lifetime ended
```

GPU-backed frame slot 另需：

```text
GPU fence completed before reuse
```

禁止：

- Job 尚未完成就 reset
- Background job 保存 frame-local pointer
- Persistent component / asset 保存 arena memory
- 以 Thread Affinity 取代真正 lifetime validation

## Terrain / Vegetation Physics Integration

Terrain Heightmap：

```text
Source of Truth
```

Jolt：

```text
HeightFieldShape
→ Derived Physics Representation
```

Physics chunk size 不硬編為固定 64x64。

改為：

```text
TerrainChunkProfile
├─ Render Chunk Size
├─ Physics Chunk Size
└─ Streaming Cell Size
```

三者可不同，依：

- CPU cost
- collision query locality
- streaming granularity
- memory
- platform tier

調整。

### Vegetation Physics Proxy

High-level contract：

```text
Vegetation Physics Proxy System
↓
Near-field collider activation
↓
Jolt backend representation
```

Jolt backend 可依 profile 選：

```text
Static Body
Compound Shape
Batched / grouped collider representation
```

High-level Terrain / Vegetation 不直接依賴 Jolt 特定 Shape organization。

遠距 vegetation：

```text
Rendering only
```

玩家 / gameplay interaction radius 內才建立必要 Physics Proxy。

## Refined AI CI Gates

### Architecture Gate — Hard Fail

失敗條件：

```text
Gameplay / Public layer includes:
- d3d12.h
- vulkan.h
- Metal native headers

Hot Path:
- heap allocation
- malloc / new
- SharedPtr copy where forbidden by hot-path policy
```

Custom AST Checker / clang-tidy 必須能檢查 namespace / module boundary。

### Shader Gate — Hard Fail

```text
Canonical Reflection mismatch
Resource Mapping mismatch
Constant Layout mismatch
Argument Buffer mapping mismatch
```

Golden Image 依 profile tolerance 驗證。

### Asset Gate — Hard Fail

```text
Deterministic Cook Hash mismatch
Bundle Dependency DAG cycle
Invalid Bundle Generation dependency set
```

### Memory Gate — Hard Fail

```text
ASan failure
UBSan failure
Frame Arena escape
Use-after-reset
Allocator boundary violation
Handle generation validation failure
```

### Performance Gate

Dedicated Performance Runner：

```text
Hot-path allocation > 0
→ Hard Fail

Regression > calibrated threshold
→ Hard Fail
```

Shared CI Runner：

```text
Frame-time / timing fluctuation
→ Trend / Warning only
```

不因虛擬化 host jitter 對一般 shared runner 做不可靠 hard fail。

### Platform Gate — Hard Fail

```text
Blacklisted GPU Feature enabled
Capability Profile says unsupported but feature forced on
Known critical workaround omitted
```

### Gate Principle

```text
Architecture Contract
→ Automated Check where feasible
```

無法完全自動化的 hardware behavior 必須進 Device Lab / Manual Validation Checklist，而不是假裝 CI 已覆蓋。


Additional Hard Gates:

- FrameLifetime AST Storage Check
- Metal Binding Snapshot Semantic Diff
- Render Graph Transient Aliasing Validation
- Dynamic Module Allocator / ABI Boundary Check
- Bundle Pending-Delete Reference Drain Validation


v3.8 Additional Gates / Metrics:

- PipelineLayoutMetadata Determinism Check
- Component Sparse/Dense Integrity Check
- Render Graph Redundant Barrier Metric
- AI Change Scope / Risk Class Policy Check
- Texture Cook Determinism / Platform Format Validation


v3.9 Additional Hard Gate:

- Frame Memory × Module Boundary Lifetime Check


v3.10 Additional Gates:

- Unified Architecture Dependency Graph Check
- Shader Generated C++ Layout Determinism / Offset Check
- Third-party Worker Oversubscription Policy Check
- Runtime Plugin ABI Handshake Smoke Test
- Streaming Priority / IO Budget Scheduler Test


v3.11 Additional Gates / Metrics:

- Terrain LOD Seam Regression Scene
- Input Gesture Single-Owner / Duplicate Delivery Test
- Simultaneous Keyboard / Mouse / Gamepad Input Test
- Multi-touch Stable PointerID / Per-pointer Capture Test
- VirtualJoystick + LookRegion + VirtualButton Concurrent Touch Test
- UI Input Interaction Matrix Routing Test
- InputLayer → UILayer Runtime Mask Cook Test
- Local Multiplayer P1/P2 UI Isolation Test
- Fixed Tick Short Press / Release Preservation Test
- IME / Physical Key Separation Test
- Profiler Trace Schema / Correlation Validation
- CI Cache Key Isolation / Wrong-config Reuse Test


v3.13 PSO Residency Gates / Metrics:

- Runtime PSO Residency Long-run Stability Test
- PSO Fence-safe Eviction Validation
- PSO Eviction Miss Must Not Block Render Thread


v3.14 Additional Gates:

- Variant Budget Stage Accounting Check (Theoretical / Pruned / Used / Cooked)
- Multi-Queue PSO Fence-safe Destruction Validation

## v3.6 Architecture Decision Matrix

| 模組 / 領域 | v3.6 最終決策 | 核心價值 |
| --- | --- | --- |
| Shader CI / Fallback | Canonical Reflection 比對；Build/Cook 選 fallback；Shipping 不動態切 compiler path | 穩定 binding contract |
| RHI Rendering | Public 僅 modern `BeginRendering` semantics；Backend 保留 compatibility fallback | High-level API 乾淨且保留 driver 生存空間 |
| Dynamic Skinning | 維持 Global Skinning Buffer + `skinningMatrixOffset` | 減少 descriptor 壓力 |
| Frame Arena | Life Barrier 後由 Main Thread 或 AllocatorManager reset | 以 lifetime 正確性為核心 |
| Bundle Update | Versioned Storage + Generation Pinning | 避免 mixed-version dependency |
| Terrain / Vegetation | Configurable chunk profile + abstract physics proxy | 解耦 render/physics/streaming |
| Performance CI | Dedicated Runner hard gate；Shared Runner trend only | 降低 false positive |

## Frame Memory Lifetime Safety Contract

Frame-lifetime memory 使用不具 Ownership 語意的型別：

```cpp
template<typename T>
class FramePtr;

template<typename T>
class FrameSpan;
```

禁止使用：

```text
FrameUniquePtr
```

避免讓開發者或 AI Agent 誤以為 Frame Memory 具有一般 owning pointer / destructor lifetime。

### Runtime Guard

Debug / Development Build 需維護：

```text
Allocator Region Metadata
Frame Generation Counter
Poison-on-reset
Use-after-reset Guard
```

每個 Frame Region 至少記錄：

```text
Region Begin
Region End
Frame Generation
Owning Arena
Owning Thread / Worker
Reset State
```

`FramePtr<T>` / `FrameSpan<T>` 在 Debug 下解引用時可驗證：

```text
Pointer belongs to active frame region
Generation matches active frame
Arena not reset
Thread access policy valid
```

Reset 後可使用 poison pattern / guard page（平台允許時）提高錯誤可觀察性。

### Static / AST Rule

Custom Clang-Tidy / AST Checker 必須禁止：

```text
Persistent Scene Component
Resource Manager
Asset Object
Long-lived Subsystem State
Global / Static Storage
```

保存：

```text
FramePtr<T>
FrameSpan<T>
FrameAllocator-backed container
Any type explicitly tagged FrameLifetime
```

Architecture Gate：

```text
Frame-lifetime type stored in persistent field
→ Hard Fail
```

---

### Module Boundary Extension

Frame lifetime safety 同樣適用於：

```text
Runtime Plugin
Dynamic Module
Third-party SDK Bridge
Deferred Callback
Async Worker Queue
```

不得因資料跨 DLL / so / dylib 邊界而失去 frame lifetime annotation。

## Native Overlay WebView Logical Screen Contract

WebView V1 定位基準升格為正式 API Contract：

```text
OS Window / Logical Screen Space
```

而不是：

```text
Render Graph Internal Resolution
Dynamic Resolution Render Size
3D Viewport Scale
```

例如：

```text
Logical Screen: 1920 x 1080
3D Dynamic Resolution: 70%
Internal Render Size: 1344 x 756

WebView Layout:
→ 仍以 1920 x 1080 logical coordinate 計算
```

Viewport Sync：

```text
RectTransform / Anchor / Safe Area
↓
Logical Screen Rect
↓
DPI / Retina / Density Conversion
↓
Native View Bounds
```

Dynamic Resolution / Render Scale 變化：

```text
→ 不直接改變 WebView logical rect
```

Editor：

- 若 Canvas / View 使用 Dynamic Resolution，WebView Inspector 顯示 Native Overlay warning
- Runtime 自動做 logical-to-native bounds remap
- 禁止將 Render Graph viewport 當作 WebView layout reference

---

## Bundle Pending-Delete / Mapping Lifetime Contract

Bundle Hot Update 使用：

```text
Versioned Storage
+
Generation Pinning
+
Pending Delete Queue
```

舊 Generation 不在固定 frame 邊界強制刪除。

狀態：

```text
Active
↓
Retired
↓
PendingDelete
↓
SafeToDelete
↓
Deleted
```

進入 `SafeToDelete` 的必要條件：

```text
LoadContext RefCount == 0
AsyncIO RefCount == 0
MappedView RefCount == 0
OpenFileHandle Count == 0
Decode / Decompress Job Count == 0
Streaming Transaction Count == 0
```

只有全部歸零才允許：

```text
munmap / UnmapViewOfFile
close / CloseHandle
delete directory
```

新版 Bundle：

```text
Downloaded
↓
Verified
↓
New Generation Created
↓
Active Generation Pointer Switch
```

舊版：

```text
Read-only until all references drained
↓
PendingDelete Queue
↓
Background cleanup or next Pre-init
```

不依賴「下一 Frame 一定安全」的假設。

---

## GPU Skinning Alignment Contract

Global Skinning Buffer 使用：

```text
Structured / Storage Buffer
```

`SkinnedDrawData`：

```cpp
struct alignas(16) SkinnedDrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t skinningMatrixOffset;
};
```

`skinningMatrixOffset`：

```text
Matrix Element Index
```

不是：

```text
Byte Offset
Descriptor Index
Per-character Constant Buffer Offset
```

因此不強制每角色 256-byte padding。

概念：

```cpp
byteOffset =
    skinningMatrixOffset * sizeof(Matrix4x4);
```

Alignment 權責：

```text
DynamicGpuBufferPool
+
RHI Backend
```

由 Resource Type 與 Device Limits 決定：

```text
Storage Buffer alignment
Uniform / Constant Buffer alignment
Copy alignment
Backend-specific resource alignment
```

若某 backend 使用 dynamic storage/uniform descriptor，才依裝置實際回報限制處理。

高階 Animation / Renderer 不硬編固定 alignment 常數。

---

## Shader Binding Snapshot / Render Graph Aliasing Gate

### Slang Metal Binding Snapshot

Metal Shader Gate 除 Canonical Reflection 外，新增 normalized binding snapshot。

兩條路徑：

```text
Direct:
Slang → MSL

Fallback:
Slang → SPIR-V → SPIRV-Cross → MSL
```

各自產生：

```text
AST-normalized / Reflection-normalized Binding Snapshot
```

比較：

```text
Logical Resource ID
Resource Type
Binding Group
Array Count
Access
Stage
Argument Buffer Group
Argument Buffer Member ID
Constant Layout
Specialization Metadata
```

任一不一致：

```text
Shader Gate → Hard Fail
```

注意：

```text
不比較文字 MSL 是否相同
不比較 binary 是否相同
```

只比較 canonical / normalized semantic contract。

### Transient Aliasing Gate

Render Graph 對 Transient Resource Reuse 必須執行：

```text
Lifetime Interval Validation
Read / Write Overlap Validation
Alias Compatibility Validation
Barrier / Transition Validation
```

禁止：

```text
Resource A still live
+
same memory reused by Resource B
```

或：

```text
Aliased resource overlapping read/write lifetime
```

Development / CI：

```text
Transient Aliasing Hazard
→ Hard Fail
```

Render Graph compiler 應輸出可讀診斷：

```text
Pass A
Resource X
Lifetime [3, 7]

Pass B
Resource Y
Lifetime [6, 9]

Aliased Heap Range overlap detected
```

---

## Dynamic Module Engine Allocator Contract

Modular Development Build 的跨 DLL / dylib / so 邊界採嚴格 allocator contract。

跨 Module Public API 優先使用：

```text
Handle
POD
Span / View
StringView
Opaque ID
Versioned C ABI Struct
```

避免跨 Module 傳遞：

```text
std::vector
std::string ownership
SharedPtr-managed internal object
Allocator-private container
Concrete internal class
```

### Heap Ownership Rule

預設：

```text
Module A allocates
→ Module A destroys
```

或：

```text
All participating modules
→ Explicit Engine Allocator API
```

禁止：

```text
DLL A CRT new
↓
EXE / DLL B CRT delete
```

若需跨 Module 配置：

```cpp
IMemoryAllocator* allocator = MemoryManager::GetAllocator(...);
```

但 Public API 不應要求 Gameplay 直接操作低階 allocator。

推薦 pattern：

```text
CreateObject()
→ returns Handle

DestroyObject(handle)
→ routed back to owning module
```

或：

```text
Module-provided Destroy function
```

### CI / ABI Gate

檢查：

```text
Exported API contains forbidden STL owning type
Cross-module allocator mismatch
Missing destroy function
ABI version mismatch
```

任一成立：

```text
Architecture / ABI Gate → Hard Fail
```

### Frame / Transient Memory Boundary

跨 Module Public API 雖可使用 `Span / View`，但僅限：

```text
Stable / persistent backing storage
+
明確文件化 lifetime
```

若 backing storage 來自 Frame Arena / Transient Allocator：

```text
普通 Span / View
→ Forbidden

FrameSpan / FrameDataHandle
→ Required
```

Plugin 若要保存資料，必須自行複製到 Plugin-owned persistent memory。

## Component Storage Contract / Sparse Set + Dense SoA

一般 Scene Component Pool 預設採：

```text
Sparse Set / Sparse Array
+
Dense Entity List
+
Dense SoA Component Data
```

核心資料流：

```text
EntityID
↓
Sparse Index
↓
Dense Slot
↓
Dense SoA Component Data
```

### Lookup

```text
EntityID.index
↓
sparse[index]
↓
dense slot
```

驗證：

```text
denseEntities[slot] == EntityID
```

可提供：

```text
HasComponent → O(1)
GetComponent → O(1)
Iteration → Dense Sequential Memory
Remove → Swap-and-Pop
```

### Default, Not Universal

此結構是：

```text
General Scene Component Pool
→ Default Contract
```

不是所有 Subsystem 的唯一 Storage。

下列資料可採專用結構：

- Singleton data
- RenderWorld temporary arrays
- Particle / VFX batches
- GPU-driven instance data
- highly dense specialized arrays
- Terrain / Vegetation cluster data

核心原則：

```text
Hot Iteration
→ Dense

Random Entity Lookup
→ Sparse
```

避免以單一 heavyweight OOP object model 作為 runtime hot-path storage。

---

## Offline Pipeline Layout Metadata

Shader Binding Pipeline 正式定義：

```text
Slang Source
↓
Shader Compiler
↓
Canonical Reflection
↓
Pipeline Layout Builder
↓
PipelineLayoutMetadata
↓
Cooked Runtime Asset
↓
Runtime Direct Indexed Access
```

`PipelineLayoutMetadata` 至少包含：

```text
Shader ID
Variant Key
Resource ID
Resource Type
Set / Space
Binding
Array Count
Access
Stage Visibility
Argument Buffer Group
Argument Buffer Member ID
Constant / Parameter Layout
Push / Root / Constant Transport Metadata
Specialization Metadata
Vertex Input
Fragment Output
Binding Tier
```

### Runtime Rule

禁止：

```text
Runtime
→ string lookup
→ dynamic hash binding assembly
→ build descriptor layout on demand
```

Runtime 應：

```text
Load PipelineLayoutMetadata
↓
Direct index / precomputed mapping
↓
Create / lookup compatible PSO
```

目標：

- 降低 runtime CPU overhead
- 避免 binding contract 漂移
- 讓 DX12 / Vulkan / Metal 共用相同 canonical logical layout
- 讓 AI / CI 可直接 diff metadata

### Determinism

同一：

```text
Shader Source
Compiler Version
Variant Key
Backend Profile
```

應產生 deterministic metadata。

不一致：

```text
Shader / Asset Gate
→ Hard Fail
```

---

## Worker Frame Arena Page / Chunk Growth Policy

`FrameAllocatorSet`：

```text
MainThread Arena
RenderThread Arena
Worker Arena[N]
```

保持不變。

Worker Arena 的容量策略補充為：

```text
Primary Chunk
↓ overflow
Additional Chunk
↓ overflow
Additional Chunk
```

Allocation hot path：

```text
Thread-local
+
O(1) pointer bump
```

不引入 global allocator lock。

### Alignment

每次 allocation 依實際資料需求提供：

```text
Requested Alignment
```

例如：

```text
16-byte
32-byte
SIMD / Backend-required alignment
```

不把單一 alignment 值硬編為所有物件的永久 ABI。

### Budget

每個 Arena / Worker Profile 追蹤：

```text
Initial Capacity
Current Reserved
Current Used
Peak Used
Soft Limit
Hard Limit
Overflow Count
Chunk Count
```

### Reuse

Frame 完成後：

```text
Reset cursors
↓
Reuse existing chunks
```

不必每幀 free 已配置 page。

Spike：

```text
Normal usage: 4 MB
Temporary spike: 12 MB
Next frames: reuse pages
Long-term low usage: trim excessive pages
```

### Trim Policy

避免：

```text
每次 overflow → system heap allocation
每幀 free / realloc
```

可在：

```text
Memory pressure
Scene transition
Long idle window
Periodic maintenance
```

執行 trim。

Hard Limit 超過：

```text
Development
→ Assert + Capture

Shipping
→ controlled fallback / fail-safe policy
```

依 subsystem 定義。

---

## Performance Policy Manager

Mobile / Laptop runtime 建立統一：

```text
PerformancePolicyManager
```

輸入：

```text
Thermal State
Battery / Power State
CPU Frame Time
GPU Frame Time
Frame Pacing
Memory Pressure
Display Refresh Rate
Device Tier
User Quality Setting
```

輸出：

```text
Target FPS
Dynamic Resolution Scale
Shadow Quality
LOD Bias
Post Processing Quality
Vegetation Density
Animation Update Rate
Streaming Budget Bias
```

### Platform Timing

Android：

```text
Frame Pacing abstraction
→ 可整合 Android Frame Pacing / Swappy 類型 backend
```

iOS：

```text
Display timing abstraction
→ CADisplayLink / platform display timing backend
```

Public Engine 不直接依賴特定第三方名稱。

### Thermal Policy Example

```text
Normal
→ target 60 FPS
→ render scale 1.0

Warning
→ keep target if possible
→ lower internal resolution

Serious
→ reduce GPU features
→ lower resolution
→ optional lower target FPS

Critical
→ aggressive quality reduction
→ lower target FPS
```

具體 FPS / Scale 不寫死於架構層，交由：

```text
Quality Profile
+
Device Tier
+
Game Policy
```

決定。

Gameplay 不直接讀取 OS thermal API 後自行調畫質。

---

## AI Change Scope / Risk Policy

AI Agent 的變更限制以：

```text
Risk
+
Architecture Boundary
+
Primary Goal
```

為核心。

不使用單純固定 Diff Line Count 作為主要 Hard Limit。

### Session Contract

一個 AI Session / PR 原則：

```text
Primary Goal
→ 1

Primary Subsystem
→ Prefer 1
```

跨 Subsystem 修改允許於：

```text
明確 Vertical Slice
或
必要 Architecture Change
```

### Risk Class

```text
Risk A
→ Docs / Tests / Generated Metadata / Mechanical Changes

Risk B
→ Normal Subsystem Implementation

Risk C
→ RHI
→ Allocator
→ Job System
→ Render Graph
→ Asset Format
→ ABI / Plugin Boundary
→ Serialization Compatibility
```

Risk C：

- 必須 narrow scope
- 必須 dedicated CI Gate
- 必須 Architecture Contract review
- 架構變更需 ADR

### Diff Size

Diff Size：

```text
Soft Signal
```

不是唯一 Hard Gate。

例如：

```text
2000 lines generated metadata
→ may be low risk

20 lines allocator lifetime change
→ may be high risk
```

### Cross-module Change

避免單一 AI PR 無明確理由同時大幅修改：

```text
RHI
+
Editor UI
+
Asset Cooker
+
Physics
+
Runtime UI
```

若是 Vertical Slice：

```text
可以跨 module
但每個 boundary 必須有對應 test / gate
```

---

## Render Graph Barrier Optimization Policy

現有 Render Graph Resource Lifetime / Barrier / Aliasing Contract 繼續保留。

新增 Transition Planner：

```text
Logical Resource State
↓
Render Graph Compiler
↓
Transition Planner
↓
Barrier Merge / Optimization
↓
Backend Translation
```

Validation：

```text
Missing Barrier
→ Hard Fail

Invalid Transition
→ Hard Fail

Aliasing Hazard
→ Hard Fail

Illegal Read / Write Overlap
→ Hard Fail

Redundant Barrier
→ Metric / Warning
```

Redundant Barrier 預設不 Hard Fail。

原因：

```text
Driver Workaround
Conservative Backend Policy
Debug Validation Mode
```

可能刻意產生額外 synchronization。

Profiler 應追蹤：

```text
Barrier Count
Merged Barrier Count
Redundant Barrier Estimate
Transition Count
Alias Barrier Count
```

---

## Texture Cook Profile / Platform Transcoding

Runtime Texture Format 不採：

```text
Desktop = 全部 BC7
Mobile = 全部 ASTC 同一 Block Size
```

而是依：

```text
Texture Semantic
Quality Tier
Platform
GPU Capability
Memory Budget
```

決定。

Desktop 範例：

```text
Color / Albedo
→ BC7

Normal
→ BC5 or profile-selected BC format

Single-channel Mask
→ BC4

Two-channel Data
→ BC5

HDR
→ BC6H
```

Mobile：

```text
ASTC
→ block size by texture class / quality profile
```

例如：

```text
UI / Hero Texture
→ higher quality ASTC profile

Terrain / Background
→ more aggressive ASTC block size
```

Asset Pipeline：

```text
Source Texture
↓
Import
↓
Canonical Texture Intermediate
↓
Platform Cook
├─ Windows BC*
├─ Android ASTC
├─ iOS ASTC
└─ macOS capability-selected
↓
Bundle
```

Runtime 不直接依賴 PNG / TGA / source format。

---

## Golden Image Contract Retained

既有 Golden Image 架構維持，不建立重複規則。

正式保留：

```text
SSIM
+
Perceptual Diff
+
Pixel Error Guardrail
```

並依 profile：

```text
Same Backend
Cross Backend
PBR
Shadow
Post Processing
Compression
Temporal
Platform / Driver Family
```

設定 tolerance。

Threshold：

```text
以 Device Lab / Baseline 實測校準
```

而非要求跨 GPU pixel-perfect。

## Frame Memory × Module Boundary Contract

Frame-lifetime memory 的生命週期規則必須跨越 Engine / Runtime Plugin / Dynamic Module 邊界。

### Core Rule

禁止將來自：

```text
Frame Arena
Transient Allocator
Per-frame Scratch Buffer
Worker Frame Arena
Render Extraction Scratch
```

的記憶體，偽裝成一般：

```text
Span<T>
View<T>
Raw Pointer
```

跨 Module 傳遞。

跨 Module 的 transient frame data 僅允許使用：

```text
FrameSpan<T>
FrameDataHandle
```

### FrameSpan Contract

```text
Ownership
→ None

Lifetime
→ Current callback / declared frame scope only

Retention
→ Forbidden

Async Capture
→ Forbidden

Persistent Storage
→ Forbidden

Implicit Conversion to Span/View
→ Forbidden
```

例如：

```cpp
void MyPlugin::OnRenderExtract(
    FrameSpan<const RenderItem> items)
{
    Process(items);       // OK

    // m_items = items;   // Forbidden
}
```

若 Plugin 需要跨 callback 或跨 frame 保存資料：

```text
FrameSpan<T>
↓
Explicit Copy
↓
Plugin-owned Persistent Storage
```

例如：

```cpp
m_items.assign(items.begin(), items.end());
```

### FrameDataHandle

對高風險第三方 Runtime Plugin，可優先使用：

```text
FrameDataHandle
```

概念：

```text
FrameDataHandle
├─ Index
└─ FrameGeneration
```

Resolve：

```text
FrameDataHandle
↓
Engine API
↓
Validate FrameGeneration
↓
FrameSpan<const T>
```

若 Handle 過期：

```text
Resolve
→ Invalid / Error
```

禁止回傳 stale pointer。

### Synchronous Consumption Rule

V1 規範：

```text
FrameSpan callback
→ synchronous consumption only
```

Plugin 不得：

- 將 `FrameSpan` 保存到 member
- 捕捉到 deferred lambda
- 傳給 background thread
- 丟入 async queue
- 轉型成普通 `Span/View`
- 將其 backing pointer 存入 persistent object

若需要 async / deferred：

```text
Plugin must copy
```

V1 不提供 `FrameAsyncLease` 類型，以降低 lifetime 複雜度。

### Dynamic Module Public API Rule

跨 Module Public API 分為：

```text
Persistent / Stable Data
→ Handle
→ POD
→ Stable Span / View with documented lifetime

Transient Frame Data
→ FrameSpan<T>
→ FrameDataHandle
```

普通 `Span/View` 不得承載 Frame Arena / Transient Allocator backing storage。

### Static Analysis / CI

Custom AST / Clang-Tidy 必須檢查：

```text
FramePtr / FrameSpan
→ Persistent member
→ Hard Fail

FramePtr / FrameSpan
→ Async/deferred lambda capture
→ Hard Fail

FramePtr / FrameSpan
→ converted to ordinary Span/View
→ Hard Fail

FramePtr / FrameSpan
→ exported without lifetime annotation
→ Hard Fail

Frame-lifetime backing storage
→ exposed through generic Public API
→ Hard Fail
```

Runtime Plugin 若為第三方、無法執行完整 AST Checker：

```text
Debug Runtime Guard
+
Frame Generation Validation
+
Region Metadata Validation
```

仍必須捕捉 lifetime violation。

### Relationship to Systemic Risk

此 Contract 直接對應：

```text
AI-generated Lifetime / Memory Bugs
```

並把以下三個既有架構接起來：

```text
Frame Memory Lifetime Safety
+
Dynamic Module Allocator / ABI Contract
+
AI Architecture / Memory Gate
```

## Unified Architecture Dependency Graph Gate

既有 RHI / Plugin / Public API / ABI 等邊界規則，統一收斂到共用的：

```text
Architecture Dependency Graph
```

目的不是建立另一套重複規範，而是讓所有既有 Dependency Direction Contract 使用同一套靜態分析基礎。

### Layer Direction

範例：

```text
Game / Gameplay
↓
Engine Public API
↓
Subsystem API
↓
Engine Internal
↓
Platform / Backend
```

禁止反向依賴，例如：

```text
Core
→ Gameplay

Renderer Internal
→ Editor Feature

Engine Public
→ Vulkan / DX12 / Metal native header
```

### Tooling

Architecture Gate 可由：

```text
Include Graph Scanner
+
Clang AST / Compile Database
+
Module Rule Manifest
```

共同產生 Dependency Graph。

每個 module 定義：

```text
Module Name
Layer
Allowed Dependencies
Forbidden Dependencies
Public Include Roots
Private Include Roots
```

CI：

```text
Forbidden Edge
→ Hard Fail
```

並輸出完整 dependency path，避免只報單一 include。

---

## Third-party Job System Adapter Contract

第三方 Runtime Library 若具有自己的 Task Scheduler / Thread Pool，不得默認自行建立與 Engine 無關的完整 Worker Pool。

新增：

```text
JobSystemTaskAdapter
```

### Goal

避免：

```text
Engine Job Workers
+
Jolt Workers
+
Middleware Workers
+
Plugin Workers
```

同時建立大量 thread，造成：

```text
CPU Oversubscription
Context Switch
Cache Thrash
Frame Pacing Regression
```

### Preferred Integration

若第三方 API 支援 custom job/task interface：

```text
Third-party Task
↓
JobSystemTaskAdapter
↓
Engine Job System
```

Adapter 必須映射：

```text
Task Priority
Dependency / Completion
Worker-safe Scratch
Cancellation where supported
Thread Affinity requirement
```

### Exception

若第三方 library 強制使用自己的 threads：

```text
Dedicated Thread Budget
+
Affinity / Priority Policy
+
Profiler Visibility
```

必須由 Engine 統一記錄，不得無限制建立背景 thread。

### Middleware Examples

適用於：

```text
Jolt
Audio / FMOD related worker integration where supported
Asset Decoder
Networking Library
Runtime Plugin
Third-party SDK
```

是否能真正共用 Engine Job System 依第三方 API 能力決定，不強迫不支援的 library 做不安全 wrapper。

Profiler 至少追蹤：

```text
Engine Worker Count
External Worker Count
Runnable Task Count
Context Switch Trend
Third-party Job Time
Oversubscription Warning
```

---

## Shader Reflection → C++ Layout Generation

Canonical Reflection 除了驗證，也作為 C++ Shader Data Layout 的單一生成來源。

Pipeline：

```text
Slang Source
↓
Slang Reflection
↓
Canonical Reflection Metadata
↓
C++ Layout Generator
↓
Generated Shader Parameter Header
```

例如產生：

```cpp
struct alignas(16) GeneratedMaterialParams
{
    // generated fields...
};
```

以及：

```cpp
static_assert(sizeof(GeneratedMaterialParams) == kExpectedSize);
```

### Source of Truth

禁止：

```text
Shader CBuffer layout
+
Manually maintained independent C++ struct
```

長期雙重維護。

C++ generated header 必須由 canonical reflection 產生。

### CI

Shader Gate 驗證：

```text
Generated Header Hash
Canonical Reflection Layout
Field Offset
Field Size
Alignment
Array Stride
Matrix Layout
```

任何不一致：

```text
Hard Fail
```

Generated file 應 deterministic，並可選：

```text
Build-time generation
或
Cook-time generation + checked-in snapshot
```

但不得由 Runtime 動態產生。

---

## Streaming Priority Preemption / IO Concurrency Contract

既有 Asset UUID / Virtual Indirection / Residency State Machine 保持不變。

新增 Streaming Scheduler 行為：

```text
Priority-aware Queue
+
Preemption Policy
+
Per-platform IO Concurrency Budget
```

### Priority

例如：

```text
Critical
→ Player teleport destination
→ Required collision / terrain

High
→ Near-camera texture / mesh

Normal
→ Normal scene streaming

Low
→ Prefetch / speculative content
```

### Preemption

高優先級 request 到達時：

```text
Queued Low Priority Work
→ may be reordered / deferred
```

若底層 IO 已不可取消：

```text
Do not force unsafe cancellation
→ limit new low-priority dispatch
→ prioritize next dispatch slot
```

### IO Concurrency

不得無限制同時發出 disk/decompression/upload 工作。

Profile 定義：

```text
Max Disk IO In-flight
Max Decode Jobs
Max Upload In-flight
Max Remote Download In-flight
```

實際數值依：

```text
Platform
Storage Type
Device Tier
Thermal / Power State
```

調整。

Scheduler Profiler：

```text
Queue Depth by Priority
Average Wait Time
Preemption Count
In-flight IO
Decode Saturation
Upload Saturation
Starvation Count
```

需要 starvation prevention，避免 Low Priority 永遠無法完成。

---

## Runtime Plugin ABI Handshake

Build / CI 時 ABI 檢查保留，Runtime 載入 Plugin DLL / so / dylib 時再執行一次握手。

建議 export 穩定 C ABI entry point：

```cpp
extern "C" PluginInitResult Plugin_Init(
    const EnginePluginHost* host,
    const PluginInitInfo* initInfo);
```

### Runtime Validation

`Plugin_Init()` 前後至少驗證：

```text
Engine API Version
Plugin API Version
ABI Version
Build Configuration Compatibility
Platform / Architecture
Plugin Manifest ID
Binary / Manifest Hash or Build ID
Required Capability Set
```

若 Plugin Binary 被手動替換、版本錯誤或 manifest 不匹配：

```text
Reject Load
→ Clear Error
```

禁止進入部分初始化狀態。

### Stable Boundary

握手 struct：

```text
POD
Versioned
Size-tagged where appropriate
C ABI compatible
```

不得在 entry point 直接跨 module 傳遞：

```text
std::string
std::vector
SharedPtr
Internal concrete class
FrameSpan without explicit callback lifetime
```

### Failure Safety

Plugin load 流程：

```text
Discover
↓
Manifest Validate
↓
Binary Identity Validate
↓
Plugin_Init Handshake
↓
Capability Validate
↓
Initialize
```

任一步失敗：

```text
No partial activation
```

## Terrain Height Precision / LOD Seam Contract

Terrain Heightmap 為 Source of Truth，儲存精度必須由 Terrain Asset Profile 明確定義。

### Height Format

V1 建議支援：

```text
R16_UNORM
FP16 / R16F
```

選擇依：

```text
World Height Range
Required Vertical Precision
Compression / Storage Budget
Platform Capability
```

決定。

不得把「FP16 永遠優於 R16_UNORM」寫成固定規則。

例如大範圍但高度範圍可預先正規化的 Terrain：

```text
R16_UNORM
→ 可提供穩定且可預測的量化精度
```

需要特殊高度表示或運算流程時：

```text
FP16
→ profile-selectable
```

Importer / Cooker 應記錄：

```text
Min Height
Max Height
Scale
Bias
Height Encoding
```

避免 Runtime 自行猜測。

### LOD Seam Prevention

Terrain Quadtree / Chunk 跨 LOD 邊界必須具備 seam prevention。

允許策略：

```text
Edge Stitching
Skirt
LOD Morphing
```

或其組合。

High-level Terrain Contract 不把實作綁死單一方法。

必要條件：

```text
Adjacent LOD Levels
→ no visible gap / crack
```

LOD Morphing 若使用：

```text
Current LOD Height
↔ Parent / Coarser LOD Height
→ smooth transition
```

Streaming 導致鄰近 Chunk 暫時不同 LOD / residency 狀態時，仍需維持 seam-safe fallback。

Profiler / Debug View 可顯示：

```text
Terrain LOD
Chunk Boundary
Stitch / Skirt State
Morph Factor
Missing Neighbor
```

---

## Vegetation Interaction Field

既有 Vegetation Wind：

```text
Global Wind
+
Procedural Noise
+
Pivot / Hierarchical Bend
```

繼續保留。

新增簡化 gameplay interaction：

```text
Vegetation Interaction Field
```

來源可包含：

```text
Player
Character
Vehicle
Explosion / Force Event
```

Runtime 將少量 interaction primitives 寫入：

```text
Interaction Buffer
```

Shader / Compute 根據：

```text
Position
Radius
Strength
Direction
Falloff
Lifetime
```

產生局部 Bend。

### Core Rule

大量植被不得因互動而建立逐 Instance CPU Transform Update。

優先：

```text
Small interaction primitive set
↓
GPU evaluation
↓
Vertex / Compute deformation
```

Physics Proxy 與 visual bend 分離：

```text
Visual Bend
≠
Full rigid-body simulation
```

只有真正 gameplay-relevant 的近距離植被才建立 Physics Proxy。

---

## Native Overlay Gesture Ownership / Forwarding Contract

Input Pipeline：

```text
Platform Raw Event
↓
engine::input
↓
Input Context / InputLayer
↓
UI Input Interaction Matrix / Route Policy
↓
Native Overlay / Runtime UI / Gameplay
```

Block / Pass-Through / ConsumeOnHit / BlockBelow 與 per-pointer ownership 規則繼續保留。

若未來支援特定 gesture forwarding，不允許同一事件被 Native 與 Engine 兩邊同時消費。

### Ownership Rule

每個 pointer / gesture sequence 在任一時刻必須有唯一 Owner：

```text
Native Overlay
或
Engine UI / Gameplay
```

允許：

```text
Explicit Ownership Transfer
```

禁止：

```text
Duplicate Delivery
```

### Gesture Forwarding

可選擇建立：

```text
Gesture Forward Policy
```

例如：

```text
Single Tap
Pinch
Two-finger Pan
Wheel / Trackpad Gesture
```

但 forwarding 必須是：

```text
Native Recognize
↓
Normalized Engine Gesture Event
↓
Ownership Transfer / Synthetic Event
```

而不是把同一 raw touch stream 同時交給兩邊。

若平台無法可靠支援：

```text
Capability Query
→ Feature Disabled / Editor Warning
```

V1 預設仍以清楚的 Rect + Block / Pass-Through 為主，複雜 partial-forwarding 屬 optional capability。

---

## Profiler Trace Export Contract

Engine Profiler 除 Editor 即時顯示外，新增離線 Trace Export。

最低支援：

```text
Engine JSON Trace
+
Chrome Trace Event compatible JSON
```

並保留未來：

```text
Perfetto-compatible trace
```

整合能力。

Trace 至少可包含：

```text
CPU Thread Timeline
Job Events
Render Pass Events
GPU Timing
Frame Markers
Asset Streaming Events
Memory Budget Events
Bundle IO
PSO Creation
Shader Compile
WebView / Platform Events
```

每個事件建議包含：

```text
Timestamp
Duration
Thread / Queue
Category
Name
Frame ID
Optional Correlation ID
```

### Correlation

跨 CPU / GPU / IO 工作建議使用：

```text
Correlation ID
```

例如：

```text
Asset Request
→ Disk IO
→ Decode
→ GPU Upload
→ Ready
```

能在離線 trace 中串成完整 lifecycle。

### Shipping

Shipping Profiler 預設可裁切。

Development / Profile Build：

```text
Trace Capture Enabled
```

Release：

```text
Feature Stripping
→ optional / disabled by default
```

---

## CI Build Cache / Matrix Parallelization Policy

大型 Feature Matrix 不應每個 Job 從零重建所有內容。

CI Build Infrastructure 建立：

```text
Compiler Cache
+
Artifact Cache
+
Parallel Matrix Jobs
```

可採：

```text
sccache
ccache
compiler-native cache where appropriate
```

具體工具依平台與 Compiler Profile 決定。

### Cache Key

Build Cache Key 至少包含：

```text
Source Revision
Compiler Version
Compiler Flags
Platform
Architecture
Build Configuration
Public ABI Version
Feature Set
PCH / Module Inputs
```

Shader / Asset Cooker Cache 需使用各自獨立 deterministic key。

禁止：

```text
incorrect cross-config cache reuse
```

### CI Matrix

PR Gate：

```text
Representative Matrix
+
Fast Architecture / Shader / Asset Gates
```

Nightly：

```text
Wider Backend / Feature Matrix
```

Release：

```text
Full Shipping Matrix
```

平行化維度可包含：

```text
Platform
Backend
Feature Profile
Minimal / Full Build
Sanitizer Profile
```

### Time Budget

PR feedback latency 應持續監控，但：

```text
15–20 minutes
```

只作 operational target / KPI，不作架構硬性保證。

CI Profiler 追蹤：

```text
Total Pipeline Time
Queue Time
Compile Time
Cache Hit Rate
Cache Miss Rate
Slowest Matrix Job
Artifact Upload / Download Time
```

## Runtime PSO Residency Budget / Fence-safe Eviction

PSO Warmup 解決「建立時機」，但不解決「長時間執行後的常駐量」。

Runtime 必須對已建立的 PSO / Pipeline Object 有明確的 Budget 與回收機制，
避免跨 Scene / Terrain Chunk / Vegetation Material 長時間遊玩後無上限累積。

此機制與 CI 階段的 `Variant Compile Budget`（監控編譯出的 Variant 總數）
是不同層級的 Gate，不得混用：

```text
CI Variant Compile Budget
→ Build / Compile 時期
→ 監控「編譯出多少 Variant」

Runtime PSO Residency Budget
→ Runtime 執行期
→ 監控「目前常駐 GPU / Driver 的 PSO 數量與估計成本」
```

### Residency Tracking

沿用既有 Streaming Asset Residency State Machine 的模型，
將 PSO / Pipeline Object 視為同一套 Residency / Budget Framework 下的資源類別，
不另建平行系統。

至少追蹤：

```text
Current Resident Count
Peak
Budget
High Watermark
Emergency Watermark
Evictable
Pinned
Last Used Frame
Last-Used Fence Value
Estimated Cost
```

`Pinned` 適用於例如：

```text
Loading / Error Fallback Pipeline
Core UI
Always-required Debug / Bootstrap Pipeline
```

實際是否常駐由 Shipping Profile 決定。

### Fence-safe Eviction

禁止僅依 CPU 端 Last-Used Time 判斷可回收：

```text
Mark Candidate (LRU)
↓
Check Last-Used Fence Value
↓
GPU Fence Completed?
├─ No  → 保留，延後至下次檢查
└─ Yes → 允許 Destroy
```

任一 In-flight Command Buffer 仍可能引用該 PSO 時，不得 Destroy。

Fence 語意與：

```text
Frame Arena GPU Frame Reuse
Dynamic GPU Buffer Reuse
Deferred GPU Resource Destruction
```

使用同一套 GPU completion contract。

### Eviction Miss Handling

PSO 被回收後若又被需要，不得違反既有規則：

```text
Hot Render Loop
→ 不允許隱式同步建立昂貴 PSO
```

因此：

```text
PSO Miss
↓
Async Recreate
↓
Existing Warmup / Pipeline Creation Queue
↓
Temporary Fallback Policy
```

允許的過渡策略必須由 Render Feature / Material / Pass 明確宣告：

```text
Compatible Fallback Pipeline
或
Defer / Skip Draw for bounded frames
```

禁止任意使用「相近 Variant」替代，
除非該 fallback 已被明確驗證具有相容的：

```text
Vertex Layout
Render Target Format
Depth / Stencil Contract
Resource Layout
Shader Semantic
```

不得在 Render Thread 同步阻塞等待 PSO 重建完成。

### Watermark Behavior

```text
High Watermark
→ 優先回收 Evictable 且非近期使用的 PSO
→ 減少非必要 Warmup 預先建立量

Emergency Watermark
→ 更積極回收 cold PSO
→ 允許短暫觸發 Eviction Miss Fallback
```

不得回收：

```text
Pinned
In-flight
Currently Building
Required by active critical render path
```

### Platform Note

部分 Mobile GPU Driver 對同時存在的大量 Pipeline Object
可能具有額外的 driver-side memory / internal object pressure。

因此 Device Compatibility Matrix 應記錄代表機種的：

```text
Recommended Resident PSO Budget
Observed Stable Peak
PSO Creation Cost
Driver / OS Version
Warmup Behavior
Eviction / Recreate Behavior
```

這不是只看 CPU RAM / GPU VRAM 的問題，
也需要觀察 driver-side stability 與 frame-time behavior。

### GPU Queue / Fence Timeline Contract

`LastUsedFenceValue` 的判斷方式取決於 RHI 的 submission timeline 模型。

若引擎將所有會引用 PSO 的提交統一映射到單一 Global GPU Completion Timeline：

```text
Single Unified Timeline
→ one LastUsedFenceValue
→ completedFence >= lastUsedFenceValue
→ Safe to Destroy
```

此時不需要另外逐一檢查 Frames-in-Flight slot。

若 Graphics / Compute 等 Queue 使用彼此獨立的 Fence Timeline：

```text
PSO Last Use
├─ Graphics Queue → LastUsedGraphicsFence
└─ Compute Queue  → LastUsedComputeFence
```

則 Destroy 條件必須是所有 relevant queue 都完成：

```text
Graphics Completed >= LastUsedGraphicsFence
AND
Compute Completed >= LastUsedComputeFence
→ Safe to Destroy
```

Copy Queue 通常不直接引用 Graphics/Compute PSO，
但若未來 Backend / Pipeline 類型存在例外，仍依實際 resource usage contract 納入。

核心原則：

```text
CPU Last-Used Frame
→ only LRU ordering signal

GPU Completion Fence
→ actual destruction safety condition
```

不得用 `FrameID` 或 CPU 時間取代 GPU completion 驗證。

### Profiler

Profiler 至少顯示：

```text
Resident PSO Count
Peak Resident Count
Pinned Count
Evictable Count
PSO Budget
High / Emergency Watermark
Eviction Count
Eviction Deferred by Fence
Eviction Miss Count
Async Recreate Count
Runtime-created PSO Count
Worst Recreate Time
```

### V1 Definition of Done

- Runtime PSO Residency 有 Current / Peak / Budget / Watermark 追蹤
- Eviction 僅在 GPU Fence 確認完成後執行
- In-flight / Pinned PSO 不得被 Destroy
- Eviction Miss 走 Async 路徑，Render Thread 不同步阻塞
- Fallback 必須是明確宣告且 layout / semantic compatible
- Watermark 觸發行為可被 Profiler 觀察
- 至少一個代表性 Mobile 機種驗證長時間、多 Scene 遊玩下 Resident PSO 數量穩定收斂


## v4.0、尚未細談系統正式決策（Architecture Completion Baseline）

本節依據先前「尚未細談系統清單」補齊尚未形成完整 Runtime Contract 的區塊。

若本節與前面較早期的簡短描述在細節上有差異：

```text
v4.0 Architecture Completion Baseline
→ 優先於早期摘要文字
```

但不推翻已經確立的核心 Contract，例如：

```text
C++20 Native Core
Zig Primary Gameplay + Stable C ABI
Node + Component Public Workflow
EntityID + Sparse Set + Dense SoA Runtime
SceneGraph ≠ SpatialWorld ≠ RenderWorld
DX12 / Vulkan / Metal
Slang-only Shader Source
RenderGraph-owned GPU Hazards
SharedPtr only for true shared lifetime
UIElement ≠ SceneNode
World ≠ Scene ≠ StreamingCell
CPU Physics = Gameplay Truth
```

---

### A. Transform System

正式採用：

```text
Hierarchy Identity
+
Local TRS
+
High-precision World Position Foundation
+
Data-Oriented Transform Storage
```

Runtime storage：

```text
TransformStorage
├─ EntityID[]
├─ Parent[]
├─ FirstChild[]
├─ NextSibling[]
├─ LocalPosition[]
├─ LocalRotation[]
├─ LocalScale[]
├─ WorldTransform[]
├─ WorldVersion[]
├─ LocalVersion[]
└─ Flags[]
```

正式區分：

```text
Local Transform
≠
World Position Representation
≠
Render-relative Transform
```

大型世界 V1 foundation 採：

```text
WorldPosition
= High-precision Region / Cell Coordinate
+ Local float position
```

Renderer 使用：

```text
WorldPosition
↓
Camera-relative conversion
↓
float GPU Transform
```

避免所有 GPU / SIMD 路徑全面改用 double。

Hierarchy propagation：

```text
Local Changed
↓
LocalVersion++
↓
Hierarchy Dirty Queue
↓
Level / Depth ordered batch
↓
Parallel World Transform Propagation
↓
WorldVersion++
```

Reparent 必須透過 structural command：

```text
Reparent(entity, newParent, KeepLocal | KeepWorld)
```

規則：

- Parent 必須與 Entity 同 Scene ownership。
- `KeepWorld` 需重新計算 Local TRS。
- 禁止 hierarchy cycle。
- Negative Scale 可存在於 render transform，但 Physics / certain skinning / tangent path 必須有明確限制與 validation。
- `StaticTransform` 只是 optimization hint；若被修改，自動 invalidate static cache，不產生 undefined behavior。
- Transform traversal 不採每 Node recursive virtual callback。

V1 DoD：

- 100k+ transform hierarchy 可 batch 更新。
- Parent change / KeepWorld / KeepLocal 有 CI case。
- hierarchy cycle hard fail。
- Large World → Render-relative conversion 有 precision test。
- Scene unload 不留下 dangling parent handle。

---

### B. System Scheduler / Component Update Model

不採：

```text
Component::Update()
Component::FixedUpdate()
Component::LateUpdate()
```

大量 per-object virtual dispatch 模型。

正式採：

```text
World
↓
SystemRegistry
↓
SystemGraph
↓
JobSystem
```

System 宣告：

```text
Reads<ComponentType>
Writes<ComponentType>
Reads<ResourceType>
Writes<ResourceType>
Phase
Priority
DeterminismPolicy
```

Scheduler 根據 read/write conflict 建 DAG：

```text
TransformRead + AnimationWrite
PhysicsWrite
GameplayRead/Write
...
↓
Dependency Graph
↓
Parallel Jobs
```

正式 Phase：

```text
FrameBegin
Input
VariableGameplay
FixedPrePhysics
FixedPhysics
FixedPostPhysics
Animation
LateGameplay
RenderExtraction
FrameEnd
```

不是所有遊戲都必須使用全部 phase。

Structural Change：

```text
Create / Destroy / AddComponent / RemoveComponent / Reparent
→ WorldCommandBuffer
→ Structural Barrier
```

Zig Gameplay System 使用 batch query / view：

```text
Native Component View
↓ Stable C ABI
Zig System Batch
↓ Command Buffer
```

避免每 Entity 跨 ABI call。

Deterministic mode：

- 固定 chunk partition。
- stable iteration order（只有宣告需要 deterministic 的 system）。
- deterministic reduction API。
- 禁止 deterministic system 隱式讀 wall-clock / random device / unordered iteration。

V1 DoD：

- 系統 read/write conflict 可自動產生依賴。
- 無 conflict systems 可 parallel。
- Structural mutation 只能在 safe barrier apply。
- Profiler 能顯示 System → Job dependency / wait。

---

### C. Engine Event / Message Framework

Input Event、UI Event 不取代 Engine-wide typed event framework。

正式：

```text
EventBus
├─ Immediate Local Dispatch
├─ Deferred Queue
├─ Cross-thread Queue
└─ Cross-ABI Event Stream
```

Event identity：

```text
EventTypeID
→ Stable numeric ID
```

Hot path 不使用 string event name。

Immediate event 僅限：

- 同 thread。
- 明確 bounded call stack。
- listener 不可造成 unsafe structural mutation。

Deferred event：

```text
Producer
↓
Typed Event Queue
↓
Named Delivery Phase
↓
Consumer Systems
```

Cross-thread：

```text
Worker / Platform Thread
↓
MPSC / Thread-local staged queue
↓
World Event Merge
↓
Delivery Phase
```

Zig：

```text
Native Event Batch
↓
POD Event Records
↓ Stable C ABI
Zig
```

禁止保存 Gameplay DLL raw callback/function pointer 跨 Hot Reload。

Listener lifetime：

```text
SubscriptionHandle = Index + Generation
```

Module reload barrier 必須先撤銷 module subscription。

V1 DoD：

- Typed events。
- Immediate + Deferred。
- Cross-thread enqueue。
- World-local event bus 為預設；global bus 僅 platform/engine events。
- Zig event batch。
- stale listener generation 不可被呼叫。

---

### D. Camera Framework

Camera 是 Scene Component，但 Runtime Renderer 使用 extracted `CameraView`。

```text
CameraComponent
↓
CameraSystem
↓
CameraView[]
↓
RenderWorld
↓
Renderer
```

支援：

```text
Projection
├─ Perspective
└─ Orthographic
```

Camera data：

```text
Transform / WorldPosition
Projection Settings
Viewport
RenderTarget
CullingMask
ClearPolicy
Priority
Stack / Composition Policy
DynamicResolutionPolicy
PostProcessContext
```

Multiple Camera：V1 正式支援。

Camera Stack 定義：

```text
Base Camera
↓
Overlay Cameras
↓
Composite Target
```

Overlay 不等於 Runtime UI；可用於 weapon camera、mini-map、special pass。

TAA：

```text
Logical Projection      ← non-jittered
Render Projection       ← jittered
```

`WorldToScreen()` / `ScreenToRay()` 預設使用 non-jittered projection，避免 input picking 抖動。

Culling Mask 使用 project-defined render/culling layer bitmask，不與 Physics Layer / UI Input Layer 混用。

Editor Camera 是 Editor-owned view，不必成為 PlayWorld Camera Entity。

V1 DoD：

- Perspective / Orthographic。
- Multiple camera / viewport / render target。
- camera stack basic composition。
- ScreenToRay / WorldToScreen precision test。
- TAA jitter 不污染 gameplay picking。
- Dynamic Resolution per camera policy。

---

### E. Lighting / Shadow Framework

Light 是 Scene Component；Renderer 使用 data-oriented LightRegistry。

```text
LightComponent
↓
Light Extraction
↓
LightRegistry / GPU Light Buffer
↓
Forward+ / Clustered Lighting
```

V1 Light Types：

```text
Directional
Point
Spot
```

後續：

```text
Rect / Area Light
→ Bake / Advanced Runtime
```

Light mobility：

```text
Static
Mixed
Dynamic
```

Shadow：

```text
ShadowManager
├─ Directional Cascades
├─ Spot Shadow Atlas
├─ Point Shadow Atlas / Cubemap Strategy
├─ Cached Static Shadow
└─ Dynamic Shadow Budget Scheduler
```

不是每 Light 永久持有固定大 shadow map。

Shadow allocation 根據：

```text
Importance
Screen Coverage
Distance
Mobility
Quality Tier
Recent Visibility
```

Forward+ light list 由 GPU/CPU clustered builder 產生，Gameplay 不接觸 renderer-native data。

Probe：

```text
LightProbe / SH Volume
ReflectionProbe
```

Bake output 走 Asset Pipeline / Streaming Cell。

Large World：

```text
Probe Residency
→ follows World Partition / Streaming demand
```

V1 DoD：

- Directional/Point/Spot。
- CSM。
- spot/point shadow atlas。
- shadow cache foundation。
- light culling / Forward+。
- baked probe / reflection probe runtime sampling。
- profiler 顯示 shadow atlas occupancy / update cost。

---

### F. Post Processing Framework

正式採 Volume + Profile：

```text
PostProcessVolume
↓
PostProcessProfile
↓
Per-Camera Volume Resolve
↓
Blended Settings
↓
RenderGraph Passes
```

Volume：

```text
Global
Local Box / Sphere / Custom Volume
Priority
BlendDistance
Weight
```

Profile 為 Asset，可 reuse。

V1 effects：

```text
Exposure
Tone Mapping
Bloom
TAA
FXAA fallback
SSAO
Color Grading
Vignette
```

條件式 / 後續：

```text
Depth of Field
Motion Blur
SSR / advanced screen-space effects
```

RenderGraph 僅建立實際需要的 pass；disabled effect 不保留完整 runtime cost。

Camera stack 規則：Base Camera 決定主要 PostProcess；Overlay 可選擇 inherit / own limited profile。

Environment volume 可以提供 Fog / Exposure / Color / Wind / Audio ambience 等跨區域 blend source，但各 subsystem 自己 resolve 對應資料，不做一個巨型 God Volume。

V1 DoD：

- Volume priority / blend deterministic。
- per-camera profile resolve。
- quality tier 可 strip effect。
- profiler 顯示 effect timing / temporary RT memory。

---

### G. Navigation / Pathfinding Framework

V1 Default Backend：

```text
Recast
+
Detour
```

但只存在：

```text
Navigation/Private/RecastDetour
```

Public API 不暴露 `dtNavMesh*` 等 backend type。

World-level：

```text
World
└─ NavigationWorld
   ├─ NavMeshSet
   ├─ NavTile Registry
   ├─ Query Pool
   ├─ OffMesh Links
   └─ Dynamic Obstacle State
```

核心流程：

```text
Scene / Cell Geometry
↓
Editor Nav Bake
↓
Tiled NavMesh Asset
↓
Bundle / Streaming Cell
↓
NavigationWorld Register Tile
```

V1：

```text
NavMesh Agent Type
Query Filter
Path Query
Async Path Query
OffMesh Link
Runtime Obstacle
Streaming Nav Tile
Debug Draw
```

Runtime obstacle 預設不做頻繁 full rebake：

- dynamic obstacle / tile-cache style local update。
- 大規模 geometry 變化可標記 affected tile async rebuild（有 budget）。

Navigation Agent 不直接驅動 Transform：

```text
Navigation Path
↓
Desired Velocity / Steering
↓
Game-defined AI / CharacterIntent
↓
CharacterMotor
↓
CharacterController
```

Room / Portal Graph 可作 high-level path hint：

```text
Room Graph
→ coarse route
NavMesh
→ local precise route
```

V1 Crowd：

```text
Basic local avoidance / crowd service
→ Optional per agent
```

不要求所有 NPC 使用同一 Crowd solver。

V1 DoD：

- Tiled bake / load / unload。
- cross-tile path。
- Cell unload 不留下 stale nav poly ref。
- Async query cancellation。
- OffMesh Link。
- Character Controller integration sample。
- Nav profiler / debug view。

---

### H. Game AI Framework

Engine 提供 generic AI capability，不預設「敵人」「玩家」「MOBA」等 gameplay semantic。

```text
AI Agent
├─ Perception
├─ Blackboard
├─ Decision Runtime
├─ Navigation Adapter
└─ Action Adapter
```

V1 Decision framework：

```text
Behavior Tree      ✅
Blackboard         ✅
FSM Utility        ✅ lightweight
Utility Scoring    ✅ reusable scorer layer
GOAP               △ Future / Plugin
ML Policy          ✅ Interface/Foundation
```

Behavior Tree 不採一個 C++ heap object per node per agent。

```text
BT Asset
↓
Editor Compiler
↓
Compact Node Program / Tables
↓
Per-Agent Execution State
```

Blackboard：

```text
Typed Slots
Bool / Int / Float / Vec / EntityRef / AssetRef / User-defined POD
```

slot layout 在 cook 時固定；hot loop 不以 string dictionary 查 key。

Perception：

```text
Stimulus Registry
↓
Spatial Query / Physics Batch Query
↓
Budgeted Perception Update
```

V1 perception：Sight / Hearing event abstraction；不要求每 AI 每 frame raycast。

AI LOD：

```text
Critical
Full
ReducedRate
Lightweight
Dormant
```

與 Render LOD 分離。

ML / Self-play interface：

```text
Observation Buffer
↓
Policy Interface
↓
Action Buffer
```

Runtime Engine 不內建訓練框架；Training bridge / self-play runner 作為 tools / external process。

這樣 RPG / Strategy / MOBA / Simulation 可共用底層。

V1 DoD：

- BT compiler/runtime。
- typed blackboard。
- perception budget。
- nav/action adapters。
- AI update LOD。
- deterministic debug trace。
- ML policy interface 不依賴特定 ML runtime。

---

### I. Editor Framework

Editor UI：Dear ImGui Docking / Multi-Viewport。

但 Editor Architecture 不綁死 ImGui widget tree。

```text
EditorApplication
├─ EditorDocumentManager
├─ EditorSelectionService
├─ EditorObjectAdapter Registry
├─ InspectorRegistry
├─ PropertyDrawerRegistry
├─ EditorCommand / Undo Service
├─ DragDrop Service
├─ Clipboard Service
├─ Gizmo Service
├─ Asset Browser
├─ Search Service
├─ Project Settings
└─ Editor Plugin Host
```

Document Model：

```text
EditorDocument
├─ SceneDocument
├─ PrefabDocument
├─ MaterialDocument
├─ DataTableDocument
├─ AnimationDocument
└─ AudioEventDocument
```

Document 具有：

```text
Dirty State
Save / SaveAs
Undo Domain
Selection Context
External-change Detection
Reload / Merge Policy
```

Selection 不是 `Node*`：

```text
EditorObjectHandle
→ Adapter
```

Inspector：

```text
Reflection Metadata
+
Custom PropertyDrawer
+
Custom Component Inspector
```

Scene View：

- Perspective / Orthographic。
- move/rotate/scale gizmo。
- grid / angle / surface snap。
- local/world/pivot modes。
- Scene + Runtime UI mixed hierarchy editing。
- World Partition / Portal / HLOD overlays。

Editor Plugin：

```text
RegisterWindow
RegisterInspector
RegisterPropertyDrawer
RegisterImporter
RegisterMenuCommand
RegisterToolbar
RegisterAssetEditor
```

Plugin unload 必須先撤銷 Editor registry entries。

V1 DoD：

- 多 document。
- dock layout persistence。
- selection/inspector adapter。
- scene gizmo。
- asset browser/search。
- settings pages。
- DnD/clipboard。
- plugin extension points。

---

### J. Prefab Framework

Prefab 是 persistent composition asset，不是 runtime special entity type。

```text
PrefabAsset
├─ LocalObjectID
├─ Hierarchy
├─ Components
├─ NestedPrefabInstance
└─ Default Properties
```

Scene 中：

```text
PrefabInstance
├─ PrefabAssetUUID
├─ InstanceRootUUID
├─ LocalID → Instance UUID Map
└─ OverrideSet
```

正式支援：

```text
Nested Prefab      ✅ V1
Prefab Variant     ✅ V1
Property Override  ✅
Add Component      ✅
Remove Component   ✅
Add Child          ✅
Remove Child       ✅
```

Override identity：

```text
PrefabLocalObjectID
+
Stable PropertyID / PropertyPath
```

不要用 hierarchy index 作 override identity。

Variant：

```text
Base Prefab
↓
Variant Override Set
↓
Instance Override Set
```

Rebase：

```text
Old Base
+
Instance Override
+
New Base
↓
Deterministic Rebase
```

若 base property 改變且 instance 沒 override → 接受新值。

若 instance 已 override → 保留 override。

若 target component/property 被 source 刪除或型別不相容 → `PrefabConflict`，不得 silent discard。

Runtime Cook：

```text
Authoring Prefab Instance
↓
Cook
↓
Optimized Scene / Spawn Data
```

Shipping runtime 不需要為了 prefab workflow 保留完整 Editor override machinery。

Hot Reload：Editor / Development 可 rebuild prefab instance；Gameplay runtime state 不自動無條件覆蓋，需 explicit policy。

---

### K. Undo / Redo Framework

正式採 Transaction Journal，不只是單一 ICommand stack。

```text
EditorTransaction
├─ Property Deltas
├─ Structural Commands
├─ Asset Changes
└─ Selection Restore Metadata
```

連續拖曳：

```text
MouseDown
↓ Begin Transaction
N × transient changes
↓ coalesce
MouseUp
↓ Commit single undo step
```

支援：

- Property Change。
- Multi-object Edit。
- Create/Delete Entity。
- Add/Remove Component。
- Reparent。
- Prefab Apply/Revert。
- UI Edit。
- Material / DataTable / Asset editor transaction。

Snapshot 僅用於難以 delta 表達的 complex operation；一般 property 不複製整個 Scene。

Memory：

```text
UndoMemoryBudget
→ oldest transaction eviction
```

跨 Document mutation 必須放同一 transaction group 或明確拆分，不允許「一半 undo」。

Undo/Redo 為 Editor-only；Shipping 不攜帶 history。

---

### L. Reflection Framework

維持既有 V1 決策：

```text
V1
→ Runtime Metadata + Template / Macro Registration

Future
→ Optional Clang-based Metadata Generator
```

但 v4.0 補齊 stable identity。

```text
TypeDescriptor
├─ TypeGUID / Stable TypeID
├─ Name
├─ Base Type
├─ Size / Alignment
├─ Properties[]
├─ Attributes[]
└─ Lifecycle / Factory Hooks where allowed
```

Property：

```text
PropertyDescriptor
├─ Stable PropertyID
├─ Name
├─ Type
├─ Offset / Accessor
├─ Container Metadata
└─ Attributes
```

Attributes：

```text
Range
Step
Tooltip
Header
ReadOnly
Hidden
AssetType
Enum
Multiline
Color
Units
EditorOnly
```

Reflection 不依賴 C++ RTTI 作 stable serialized identity。

PropertyID / TypeID rename 必須有 alias/migration mechanism；禁止單純以當前 string hash 作唯一長期 persistent contract。

Plugin registration：

```text
Module Load
→ Register TypeDescriptor

Module Unload Barrier
→ No live reflected instance / callback
→ Unregister Descriptor
```

Zig 只取得 flat metadata view / generated C descriptor，不暴露 C++ templates / STL。

Reflection 用途：

- Inspector。
- Serialization。
- Prefab Override。
- Undo/Redo。
- Property Animation。
- Runtime Debug。

Hot loop simulation 不依賴 reflection lookup。

---

### M. General Serialization Framework

Engine JSON Framework 是 Serialization backend 之一，不代表所有 runtime format 都是 JSON。

正式：

```text
Serialization
├─ JSON Authoring / Human-readable
├─ Binary Runtime Blob
└─ Specialized Asset Formats
```

通用 API：

```text
ArchiveReader / ArchiveWriter
SchemaVersion
ObjectReferenceResolver
MigrationRegistry
```

Authoring：

```text
Scene / Prefab / Settings
→ Deterministic JSON where practical
```

Shipping Runtime：

```text
Authoring Data
↓ Cook
Relocatable Binary Blob
↓
Fast Runtime Load
```

Runtime binary 原則：

- Canonical little-endian format。
- 不直接 dump C++ struct memory。
- offset / index-based references。
- explicit alignment。
- version / magic / checksum。
- platform-specific GPU cooked payload 可另外存。

Object graph load：

```text
Phase 1: Allocate / Create Identity
Phase 2: Deserialize Data
Phase 3: Resolve References
Phase 4: Validate / Finalize
```

因此 cyclic logical references 可支援，不依賴 deserialize call order。

Editor 對 unknown field / missing plugin：保留可恢復 payload；Shipping 對 unknown required schema hard fail。

Migration：

```text
Version N
→ N+1
→ ...
→ Current
```

Migration 必須 deterministic 且可 CI fixture 測試。

---

### N. Asset Import Framework / Derived Data Cache

正式 Pipeline：

```text
Source Asset
↓
Importer Registry
↓
Import Settings
↓
Intermediate Canonical Data
↓
Asset Compiler / Cooker
↓
Runtime Artifact
↓
DDC / Library
```

Importer interface：

```text
IAssetImporter
├─ Supported Source Types
├─ ImporterVersion
├─ Read Import Settings
├─ Discover Dependencies
├─ Import
└─ Diagnostics
```

DDC key：

```text
SourceContentHash
+
ImporterVersion
+
ImportSettingsHash
+
DependencyHashes
+
TargetProfile
=
DerivedDataKey
```

File watcher：Editor only，debounce/coalesce changes。

Reimport：

```text
New Import
↓
Validate
↓ success
Atomic Publish New Artifact

Failure
→ Keep previous known-good artifact
```

大型 / 不可信第三方 importer 可使用 out-of-process Import Worker；worker crash 不拖垮 Editor。

Parallel import 使用 Job / worker pool，但同 UUID publish 必須 serialized transaction。

DDC：

```text
Local DDC       ✅ V1
Shared Remote DDC △ Future / Studio Feature
```

---

### O. Mesh / Model Pipeline

Authoring source：

```text
glTF / GLB   → Primary open interchange
FBX          → Editor import only
OBJ          → Basic static mesh support
```

建議 private importer backend：

```text
glTF → fastgltf / equivalent wrapped backend
FBX  → ufbx / equivalent wrapped backend
```

Public Asset Pipeline 不暴露 backend type。

Canonical intermediate mesh：

```text
MeshSourceData
├─ Positions
├─ Normals
├─ Tangents
├─ UV Sets
├─ Colors
├─ Indices
├─ Skin Weights
├─ Morph Targets
└─ Submeshes / Material Slots
```

Tangent：MikkTSpace compatible generation。

Optimization：

```text
Index Reorder
Vertex Fetch Optimization
Vertex Dedup
LOD Simplification
Bounds / Cone data
```

可使用 meshoptimizer-like backend，仍包在 cooker private layer。

V1 Runtime Mesh：

```text
StaticMeshRuntime
SkinnedMeshRuntime
```

Skin influence：

```text
V1 default max 4 influences / vertex
Optional import preserve 8 → cook profile decides
```

Compression：

- quantized UV / weights where quality allows。
- packed normal/tangent。
- position quantization only when bounds/error budget permits。

LOD：

```text
Manual LOD
Auto-generated LOD
Hybrid
```

Collision cook：

```text
Primitive
Convex Hull
Compound Convex
Triangle Mesh (static)
VHACD-like decomposition △ optional offline
```

Meshlet：Future / GPU-driven profile-driven，不作 V1 runtime requirement。

---

### P. Virtual File System / Async IO

正式建立：

```text
engine::io
```

VFS logical roots：

```text
engine://
project://
bundle://
cache://
user://
temp://
```

Shipping 可 strip `project://` source mount。

Path contract：

- UTF-8 logical path。
- `/` canonical separator。
- normalize `.` / `..`。
- logical path comparison 規則由 VFS 定義，不直接依賴 host filesystem case behavior。

Mount：

```text
DirectoryMount
BundleMount
MemoryMount
PlatformPackageMount
```

Async IO：

```text
IORequest
├─ Priority
├─ Offset / Size
├─ Destination / Buffer Policy
├─ CancellationToken
└─ Completion Handle
```

IO Scheduler 支援：

- request merge / coalescing。
- priority preemption。
- cancellation。
- aligned read。
- bounded concurrent requests。
- streaming deadline hint。

Memory Mapping 只用於 immutable / stable lifetime file，必須受 Bundle generation / pending-delete contract 管理。

Atomic Write：

```text
Write temp
↓
Flush / Validate
↓
Atomic Replace
```

用於 Save、Settings、Cache metadata。

---

### Q. Save / Persistent Data Framework

Save 不使用 Scene Serialization 當作 runtime save dump。

正式：

```text
SaveManager
├─ UserProfile
├─ SaveSlot
├─ WorldState
├─ UserSettings
└─ CloudAdapter
```

Save schema：

```text
SaveFileHeader
├─ FormatVersion
├─ GameBuildVersion
├─ SchemaVersion
├─ SlotID
├─ Timestamp Metadata
├─ Payload Hash
└─ Chunk Directory
```

Payload 使用 typed chunks / stable IDs，而不是直接序列化任意 live pointer graph。

Async save：

```text
Gameplay State
↓
Safe Snapshot / Copy
↓
Background Serialize + Compress
↓
Atomic Write
```

IO thread 不直接讀取正在變動的 live component storage。

可靠性：

```text
slot.tmp
↓
checksum / parse verify
↓
slot.save
+
slot.backup
```

Corruption：主檔壞 → backup fallback → 明確 error；不 silent reset。

Compression：zstd 預設可選。

Encryption：可選 project/platform adapter；只視為資料保護，不宣稱能阻止有能力的本機攻擊者。Integrity 可使用 authenticated hash / platform key service。

Persistent World State：

```text
Scene Asset Defaults
+
Persistent Object State by Stable Identity
↓
Runtime Scene
```

寶箱、Boss、橋、NPC progression 不因 Streaming Cell unload/reload 回復預設。

Cloud Save：`ICloudSaveBackend` plugin；V1 local first，cloud backend optional。

Server-authoritative game：角色資產/經濟等由 Server contract 決定，Local Save 不被當作 authority。

---

### R. Localization Framework

正式資料：

```text
LocalizationCatalog
├─ Stable LocalizationKeyID
├─ Source Key
└─ Locale Entries
```

Display text 永不作 persistent identity。

Locale：

```text
zh-TW
zh-CN
ja-JP
en-US
...
```

Fallback chain 例：

```text
zh-HK
→ zh-TW
→ zh
→ Project Default
```

V1 Locale backend：ICU4C behind Engine abstraction，Project Cook 只攜帶宣告 locale 所需資料以控制 binary/data size。

使用：

- plural categories。
- number formatting。
- date/time formatting。
- bidi。
- grapheme / line break iterator。

Message formatting：

```text
Key
+
Named Parameters
+
Plural / Select Rules
→ Localized String
```

不鼓勵 `%s %d` position-only format 作跨語言長期 contract。

Runtime language switch：

```text
LocalizationGeneration++
```

UI 元件即使當下 disabled，重新 enable / layout 時也必須檢查 generation，不依賴「當下有收到 event」才能正確更新。

Font fallback：

```text
Locale Font Profile
→ Ordered Font Families
→ Dynamic Glyph Atlas
```

Localization bundle 可依 locale 分離下載 / DLC。

Missing translation：Development 顯示明確 marker + log once；Shipping fallback chain，不回傳 silent empty string。

---

### S. Text Editing / IME Framework

`InputField` 使用獨立 `TextEditModel`，不把 editing state 散在 Widget。

```text
TextEditModel
├─ UTF-8 Text Buffer
├─ Selection
├─ Caret
├─ Composition Range
├─ Undo Stack
├─ Validation
└─ Input Constraints
```

正式區分：

```text
Physical Key Event
≠ Text Input Event
≠ IME Composition
```

IME platform adapter：

```text
CompositionStart
CompositionUpdate
CompositionEnd
TextCommit
```

Caret / selection 移動以 Unicode grapheme cluster 為使用者可見單位，不以 UTF-8 byte 或 UTF-16 code unit 當 UX cursor step。

Clipboard：Platform service。

支援：

- single/multiline。
- password display masking。
- max grapheme / byte policy。
- validation callback / regex-like project validator。
- mobile keyboard type / return key type。
- candidate window rect / IME caret position。
- copy/cut/paste/select-all。
- local text undo/redo。

Password model 不把 masked string 當真實資料 source。

---

### T. Platform Layer

Platform Layer 保持 thin abstraction + native service adapter，不建立巨大 God interface。

```text
engine::platform
├─ Application
├─ Window
├─ Display
├─ Clipboard
├─ Dialog
├─ URL / DeepLink
├─ Permission
├─ DeviceInfo
├─ Power / Thermal
├─ SafeArea / Orientation
├─ Haptics
├─ Notification
└─ NativeHandle Bridge
```

V1 以原生 backend 實作：

```text
Windows → Win32 / platform native APIs
macOS   → Cocoa / Objective-C++ bridge
iOS     → UIKit / Objective-C++ bridge
Android → Android native / Java-JNI bridge where required
```

不把 JNI / Objective-C object / HWND 等 native type 暴露到 Gameplay ABI。

Application lifecycle：

```text
Foreground
Background
Suspend
Resume
LowMemory
ThermalChanged
OrientationChanged
```

LowMemory / Thermal 進入 `PerformancePolicyManager` / `MemoryBudgetManager`，而不是各 subsystem 自己詢問 OS。

Editor-only：Native File Picker / Message Dialog 可以直接使用 platform service；Shipping 依 project permission/feature stripping。

---

### U. Logging / Console / Diagnostics

正式：

```text
engine::log
```

Log Record：

```text
Timestamp
Severity
CategoryID
ThreadID
JobID optional
WorldID optional
Message
Structured Fields optional
```

Severity：

```text
Trace
Debug
Info
Warning
Error
Fatal
```

Sinks：

```text
Debugger / Stdout
Rotating File
Editor Console
Crash Ring Buffer
Remote Dev Sink △
```

多 thread producer 不直接對檔案 mutex-heavy write；使用 thread-local / MPSC queue → logging worker。

Rate limit：

```text
LogOnce
LogEveryN
RateLimit(key, interval)
```

避免錯誤 loop 每 frame 產生數十萬 log。

Shipping：Compile-time + runtime category filtering；保留 Warning/Error/Fatal 與明確允許 category。

Fatal log 必須同步 flush crash ring / critical sink，再交 Crash Reporting。

Zig 使用 Stable C ABI logging batch / function，category ID 由 engine registry 管理。

---

### V. General Memory Allocator Framework

既有 Frame Arena / Module Allocator contract 保留，補齊 persistent memory。

V1 Default Internal Persistent Allocator：

```text
mimalloc
```

但 public engine contract 是：

```text
EngineAllocator / MemoryResource
```

backend 可替換。

Memory classes：

```text
Persistent Heap
Pool / Slab
Frame Arena
Job Scratch
Streaming Buffer
GPU Upload Ring
Readback Buffer
Module Boundary Allocator
```

C++ internal containers 可透過 `std::pmr` / engine memory resource 使用指定 allocator；Public ABI 不暴露 PMR/STL type。

Allocation Tag：

```text
Subsystem
Asset Type
World
Lifetime Class
Callsite Hash (Debug)
```

Debug build：

- leak tracking。
- double free detect where practical。
- guard / poison modes。
- arena generation validation。
- high watermark。

不建議全域強制 override 所有 third-party `new/delete`；第三方可由 adapter 或 allocator hook 納入統計。

---

### W. Job / Task Graph 高階模型

底層 Work-Stealing Job System 保留。

上層正式：

```text
TaskGraph
├─ TaskID
├─ Dependencies
├─ Priority
├─ Affinity
├─ CancellationToken
├─ CompletionFence
└─ DebugName / Category
```

Affinity：

```text
AnyWorker
MainThread
RenderThread / RenderSubmission domain
IO domain handled by IO Scheduler
PlatformThread explicit only where required
```

TaskGraph 不取代 RenderGraph；GPU hazard/order 仍由 RenderGraph 管理。

Recurring SystemGraph 可 compile/cache dependency topology，每 frame 只換 data / chunk jobs，避免持續重建大量 metadata。

Cancellation cooperative；取消不代表可任意殺 thread。

Frame task metadata 使用 Frame Arena，barrier 後不得逃逸。

Profiler：task dependency、queue latency、execution、steal、wait、critical path。

---

### X. Time / Timer / Scheduler

正式區分：

```text
RealClock        → OS monotonic time
GameClock        → World time scale / pause
UnscaledClock    → UI / transition convenience
FixedTickClock   → integer simulation tick
```

每 World 擁有自己的 `WorldTimeState`。

```text
TimeScale
PausePolicy
FixedDelta
Accumulator
TickIndex
MaxCatchupSteps
```

Timer framework：

```text
TimerScheduler
├─ OneShot
├─ Repeating
├─ GameTime
├─ UnscaledTime
└─ FixedTickTimer
```

大量 timer V1 採 bucket/timing-wheel-like scheduler，而不是每 frame 全表 scan。

Deterministic timer：

```text
ExpiryTick = currentTick + N
```

不使用 floating wall-clock comparison。

Timer callback 不允許保存 Zig module raw function pointer 跨 reload；Gameplay timer 可投遞 stable EventID / GameplayCommandID。

Background / resume 對各 clock 有明確 policy；Fixed accumulator clamp 防 spiral of death。

---

### Y. Audio Authoring / Event Editor

Audio Runtime 已有 Event / Voice / Bus / Residency；Editor 補齊 Authoring。

```text
AudioEvent Asset
↓
Audio Event Graph Editor
↓
Cooked Event Program
↓
Audio Runtime
```

V1 native event nodes：

```text
PlayClip
Random
Sequence
Switch
BlendByParameter
Loop
Delay
SetParameter / BusSend basic
```

不在 V1 追求完整 FMOD Studio 等級 graph。

Editor：

- Waveform。
- loop / trim marker。
- audition / preview。
- random/sequence preview seed。
- Bus graph。
- Snapshot editor。
- loudness analysis / normalization metadata。
- streaming/decompress residency preview。

FMOD plugin：若使用 FMOD Event，Engine AudioEvent API 映射到 FMOD backend，不要求將 FMOD authoring graph 轉成 native graph。

---

### Z. Animation Editor / Authoring Tools

Runtime Animation Framework 已定；Editor 正式補：

```text
Skeleton Viewer
Animation Clip Preview
Event Track Editor
Root Motion Visualization
Retarget Profile Editor
Blend Preview
Animation Graph / State Machine Editor
BAT Bake Preview / Debug
```

Animation Graph authoring：

```text
Graph Asset
↓
Validate
↓
Compile
↓
Compact Runtime Program
```

Runtime 不遍歷 Editor node objects。

Retarget profile：Skeleton bone semantic mapping + scale/translation rules；profile asset 可 reuse。

BAT tool 顯示：sample rate、matrix format、atlas/chunk usage、estimated memory、root/event CPU authority。

V1 將 Animation Graph / State Machine 視為 committed（與 Scope Matrix 一致）；Advanced IK graph / control-rig-like authoring 可 Future。

---

### AA. Physics Editor / Debug Tools

Editor 正式提供：

```text
Collider Authoring
Compound Collider
Physics Material Editor
Collision Layer Matrix
Character Controller Debug
Contact / Manifold Debug
Query Debug
Constraint Debug
GPU Physics Debug
Physics Profiler
```

Physics Layer Matrix 與 UI Input Matrix 類似，但 Physics collision pair 是對稱 rule，可用 triangular editor representation。

Project 設定：

```text
ObjectLayer
BroadPhaseLayer Mapping
Collision Pair Matrix
Query Mask Defaults
```

Cook 後變 bitmask/filter table；Jolt native filter object 不暴露給 Gameplay。

Character debug 顯示 capsule、ground probe、slope、step test、ground normal、moving platform velocity、requested/actual motion。

GPU Physics debug 必須標示：

```text
CPU Authoritative
GPU Visual
GPU Deferred
```

避免 developer 誤把 visual result 當 gameplay truth。

---

### AB. Project / Package / Settings System

正式 Project Manifest：

```text
<ProjectName>.engineproj
```

使用 deterministic JSON，內容：

```text
ProjectID
EngineVersionRange
DefaultScene
EnabledPlugins
FeatureFlags
TargetPlatforms
BuildProfiles
DefaultQualityProfile
DeclaredLocales
ProjectSettings Assets
```

另有 lock file：

```text
Project.lock
```

保存 plugin/package exact resolved version / source hash，確保 CI 可重現。

Settings layering：

```text
Engine Defaults
↓
Project Settings
↓
Platform Override
↓
Quality Profile
↓
Build Profile / Command Line
```

Editor User Preference 不進 Project Runtime Settings：

```text
User Preference
→ local per-user storage
```

Project Settings pages：

- Input Settings / Input Action Assets。
- UI Input Matrix。
- Physics Layer Matrix。
- Graphics / Quality。
- Audio。
- Localization。
- World Partition defaults。
- Memory budgets。
- Feature / Plugin list。

Settings 使用 schema + reflection metadata，自動產生基礎 Inspector；複雜頁面可 custom editor。

Feature flag 必須接 Feature Stripping / CI matrix，而不是單純 runtime bool。

---

### AC. Networking Future Contract

V1 仍維持：

```text
High-level Networking Framework
→ Explicitly out of V1
```

但 v4.0 定義不可破壞的 future boundary。

```text
engine::net          ← optional future module
```

層級：

```text
Transport
↓
Connection / Channel
↓
Replication / Snapshot
↓
Prediction / Reconciliation
↓
Game-defined Network Model
```

核心 future identity：

```text
NetworkEntityID
≠ EntityID
≠ SceneObjectRef
```

Input 已使用 tick-based `InputCommand`，可直接作 prediction/replay foundation。

World / Scene Streaming 不等於 Network Authority；Region server / instance world 可後續映射，不在 Scene Core 寫死。

Dedicated Server：Future headless build，不建立 RenderWorld / Audio device / Runtime UI。

Transport 不先綁定特定 provider；UDP/QUIC/WebSocket/third-party 可 plugin。

Lobby / Matchmaking 屬 service adapter，不塞進 replication core。

---

### AD. Runtime Debug / Developer Console

Development build 正式提供：

```text
DeveloperConsole
CVar Registry
Command Registry
Runtime Inspector
DebugDraw
Performance Overlay
Remote Dev Console
RenderGraph Inspector
```

CVar：

```text
Bool / Int / Float / String / Enum
Flags: ReadOnly / Cheat / Archive / RestartRequired / ShippingAllowed
```

CVar ID 為 stable registry ID；hot path 可 cache handle。

Command：typed arguments + help metadata；不以任意 eval script 作核心 console。

DebugDraw：

```text
Line / Ray / Box / Sphere / Capsule / Frustum / Text
```

thread-safe enqueue → frame debug buffer → renderer。

Runtime Inspector 使用 Reflection，但預設 Development only；Shipping remote console 必須完全 strip 或 explicit authenticated project feature。

Remote console 不預設對 public network listen。

RenderGraph Inspector 可查看 pass、resource lifetime、barrier、alias、timing，不允許修改 backend native object。

---

### AE. 跨系統正式關係

上述 Framework 最終關係：

```text
Platform
├─ App / Window / Device Services
├─ Raw Input
└─ File / OS Services
        │
        ▼
Core
├─ Memory
├─ Job / Task Graph
├─ Time
├─ Logging
├─ Event
├─ Reflection
├─ Serialization
└─ VFS / Async IO
        │
        ▼
World
├─ Scene / Entity / Transform
├─ System Scheduler
├─ PhysicsWorld
├─ NavigationWorld
├─ AI Runtime
├─ Audio Context
└─ Render Extraction
        │
        ├───────────────┐
        ▼               ▼
Asset / Data         Runtime UI
Pipeline              / Input
        │               │
        ▼               ▼
Renderer / RHI      Gameplay / Zig
```

核心原則仍然是：

```text
High-level Framework
→ Optional Convenience

Low-level Capability
→ Still Available
```

例如：

```text
InputAction optional
AI BehaviorTree optional
Prefab Editor feature not runtime requirement
PostProcess effect quality-strip capable
Networking future module not core dependency
```

---

### AF. v4.0 V1 / Future 決策摘要

新增正式列入 V1：

```text
Transform System / Large-world position foundation
System Scheduler / access-declared DAG
Typed Event Framework
Camera Framework
Lighting / Shadow Framework
PostProcess Volume Framework
Recast/Detour Navigation backend
Behavior Tree + Blackboard + Perception AI foundation
Full Editor service architecture
Nested Prefab + Variant + structural overrides
Transaction-based Undo / Redo
Stable Reflection Type/Property identity
Generic Serialization + cooked binary blob
Importer Registry + Local DDC
Mesh cook / optimization / collision cook
VFS + Async IO
Save / Persistent World State
ICU-backed Localization
TextEdit / IME framework
Expanded Platform Services
Structured Async Logging
Persistent Allocator framework
TaskGraph high-level model
Timer Scheduler
Audio Authoring V1
Animation Authoring / Graph Editor V1
Physics Authoring / Collision Matrix
Project Manifest / Settings Layering
Runtime Developer Console
```

Future / Optional：

```text
Clang Reflection Generator
Shared Remote DDC
GOAP
Advanced Crowd Simulation
Area Light advanced runtime
Advanced DOF / Motion Blur profile
Adaptive Quadtree Cell Generation
Octree World Partition
World Origin Rebasing if needed
High-level Networking / Replication
Offscreen WebView
Meshlet pipeline
Cloud Save backend
Authenticated Remote Dev Console
```

---


---

# V1 施工 Milestone（Implementation Milestones）

> 本節是「實際施工順序」，與後面的 Phase Roadmap 並存。  
> Phase 描述功能群與版本 Gate；Milestone 描述工程團隊應該先把哪條依賴鏈打通。  
> 原則：**地基先行、Vertical Slice、每一步都能執行與驗證、避免十個系統同時半成品。**

## ✅ V1-M0 — Repository / Build / CI 骨架

**目的：** 先把所有後續工程都必須依賴的 Build Contract 定死。

實作：

```text
CMake
Build Configuration
Platform Toolchain
Module Graph
Feature Flag / Feature Stripping
Plugin Manifest skeleton
Debug / Development / Shipping
Modular Dev / Monolithic Shipping
clang-format / clang-tidy
Warnings-as-Errors
CI
```

平台：

```text
Windows
macOS
Android
iOS
```

此階段只要求「最小可編譯、可啟動、可執行測試」，不做遊戲功能。

**Gate：**

```text
✓ 四平台最小 Host 可建置
✓ Modular / Monolithic 都可 Link
✓ Module dependency cycle 能被 CI 阻止
✓ Disabled Feature 可在 build graph 被排除
✓ Build ID / Engine Version / ABI Version 可查
```

---

## ✅ V1-M1 — Core Runtime Foundation

**目的：** 讓 Engine 本身先「活起來」。

實作：

```text
Engine Init / Shutdown
Memory / EngineAllocator
mimalloc backend
Memory Tag / Statistics
Job System
TaskGraph foundation
Logging
Crash Ring
Time / Fixed Tick foundation
Timer
Typed Event
VFS
Async IO foundation
Platform Core
Handle / Generation ID
```

第一個執行結果：

```text
Engine.exe
↓
Engine Init
↓
Workers Start
↓
VFS Mount
↓
Main Loop
↓
Clean Shutdown
```

**Gate：**

```text
✓ 反覆 Init / Shutdown 無 leak
✓ Job dependency / fence / cancel smoke test
✓ Frame Arena 不跨 frame 逃逸
✓ VFS 基本 mount/read/cancel 可用
✓ Structured log 可跨 thread 正常收集
```

---

## ✅ V1-M2 — Slang / RHI Cross-Platform PoC

**目的：** 最早驗證整個 Renderer 最危險的跨平台假設。

施工：

```text
One Slang Shader
├─ DXIL → DX12
├─ SPIR-V → Vulkan
└─ MSL → Metal
```

同時做：

```text
Canonical Shader Reflection
Pipeline Layout Metadata
Zig Stable C ABI PoC
Zig Android / iOS cross-compile smoke test
```

**Gate：**

```text
✓ DX12 / Vulkan / Metal 同一 Slang Triangle
✓ Canonical Reflection layout 一致
✓ 不暴露 backend native binding 到上層
✓ Zig 最小 GameModule 可跨四平台 build
```

如果這個 Gate 失敗，必須在這裡調整 RHI / Shader / Zig 邊界，不能把風險拖到後面。

---

## ✅ V1-M3 — Renderer Mainline

**目的：** 完成真正可擴充的 Renderer 主幹。

順序：

```text
DX12 Reference RHI
↓
Vulkan parity
↓
Metal parity
↓
Resource / Descriptor
↓
PSO
↓
Command / Fence
↓
RenderGraph
↓
Transient Resource
↓
Barrier / Dependency
↓
Frame Pipeline
```

**Gate：**

```text
✓ Offscreen Pass → Main Pass → Present
✓ RenderGraph 自動 Barrier
✓ DX12 / Vulkan / Metal 執行同一基本 workload
✓ PSO cache / async creation foundation 可用
✓ GPU resource lifetime 可被 validator 檢查
```

---

## ✅ V1-M4 — First Engine Vertical Slice

**目的：** 第一次打通「Data → Scene → Render」。

最小鏈：

```text
World
↓
Scene
↓
Entity / Node / Component
↓
Transform
↓
Camera
↓
Light
↓
MeshRenderer
↓
Material
↓
Slang
↓
RenderGraph
↓
Screen
```

同時建立：

```text
EditorWorld / PlayWorld
Scene Lifecycle
Additive Scene
Persistent Scene
WorldCommandBuffer
System Scheduler
Forward+ foundation
Shadow foundation
PostProcess foundation
Large-world coordinate foundation
```

**Gate：**

```text
✓ 能載入一個模型並顯示
✓ Camera / Light / MeshRenderer 可工作
✓ Scene Save / Load 後結果一致
✓ 兩個 Additive Scene 同時 Active
✓ LoadedInactive → Active → Safe Unload 正確
```

---

## ✅ V1-M5 — Asset / Serialization / Cooker / Bundle

**目的：** 把「可顯示一個模型」升級成正式內容管線。

施工：

```text
Source
↓
Importer
↓
Canonical Intermediate
↓
Cook
↓
Runtime Binary
↓
Asset Registry
↓
Bundle
↓
Runtime Load
```

優先順序：

```text
glTF
↓
Texture
↓
Material
↓
Scene
↓
Prefab
↓
FBX
↓
Audio / Font / Spine importer as needed
```

同時完成：

```text
UUID
Dependency DAG
Generic Serialization
Runtime Blob
DDC
Bundle Manifest
Generation Pinning
Hash
Rollback
Streaming Residency
DataTable foundation
```

**Gate：**

```text
✓ Runtime 不依賴 Source Asset
✓ Asset DAG cycle → build fail
✓ Import failure 保留上一版 good artifact
✓ Bundle update / verify / rollback 可運作
✓ Active generation 不會被新 generation 強制破壞
```

---

## ✅ V1-M6 — Reflection / Editor / Prefab / Plugin SDK

**目的：** 讓引擎開始能被「真正製作內容」。

施工：

```text
Reflection Metadata
Hierarchy
Scene View
Game View
Inspector
Property Drawer
Asset Browser
Gizmo
Undo Transaction
Prefab
Nested Prefab
Variant
Project Settings
Profiler foundation
```

同時完成第三方擴充地基：

```text
PluginHost
Service Registry
Extension Registry
Stable C Plugin ABI
C++ Plugin SDK
Bridge Plugin
Custom Asset Type
Editor Extension
```

**Gate：**

```text
✓ 不改 Engine source 即可寫第三方 Plugin
✓ Plugin 不需 include private Engine header
✓ Editor 可建立 / 修改 / Undo Scene
✓ Prefab override / rebase 基礎可用
✓ ABI mismatch 在 Load 前拒絕
```

---

## ✅ V1-M7 — Input / Runtime UI / Text / Localization

**目的：** 讓專案第一次具備完整玩家互動介面。

施工：

```text
Raw Input
InputUser
Optional Action Map
Keyboard / Mouse / Gamepad / Touch
Stable PointerID
Virtual Control
UI Routing Matrix
```

再：

```text
UIDocument
UIElement
RectTransform
Layout
Image / Label / Button
ScrollView
VirtualizedList
Nine-Slice
FreeType
HarfBuzz
IME
Localization
ViewModel / Binding
```

**Gate：**

```text
✓ 多輸入裝置可同時工作
✓ Multi-touch 無 duplicate delivery
✓ VirtualizedList 不依資料量建立等量 Element
✓ Disabled UI 恢復時可依 LocalizationGeneration 更新
✓ UI Logical Resolution 與 3D Dynamic Resolution 分離
```

---

## ✅ V1-M8 — Physics → Character → Navigation → AI

**目的：** 按真正依賴關係建立 Gameplay Simulation。

正式順序：

```text
Jolt Physics
↓
Query
↓
CharacterController
↓
CharacterMotor
↓
CharacterIntent
↓
Recast / Detour
↓
Navigation DesiredVelocity
↓
Blackboard / BT / Perception
```

不要反過來先做 AI 再補角色移動。

**Gate：**

```text
✓ Character Requested Motion / Actual Motion 分離
✓ Ground / Step / Slope / Snap 正常
✓ Root / external velocity 可經 Motor resolve
✓ Nav 不直接寫 Transform
✓ AI 不直接控制 Physics Body
```

---

## ✅ V1-M9 — Animation / Audio / VFX / Video

**目的：** 補完整 Presentation / Character Runtime。

Animation：

```text
Skeleton
Clip
Graph
StateMachine
Blend
Root Motion
GPU Vertex Skinning
BAT foundation
```

Audio：

```text
Audio.Core
MiniAudio
AudioEvent
Bus
Streaming
Residency
```

VFX：

```text
CPU Particle SoA
GPU Particle foundation
Sprite / Mesh / Trail
```

Video：

```text
Media.Core
VideoPlayer
Hardware Decode Backend
VideoTexture
UI.VideoElement
Subtitle
A/V Sync
Video Audio → Audio.Core
```

**Gate：**

```text
✓ Animation Graph 可驅動角色
✓ Active Audio Voice 不會被錯誤 unload
✓ Video 不阻塞 Gameplay main thread
✓ VideoTexture 可進 UI / Material
✓ Dedicated / Headless 可完全 strip Media / Audio / VFX
```

---

## ✅ V1-M10 — Large Scene / Streaming / Terrain / Vegetation / HLOD

**目的：** 在所有基礎系統都成熟後才正式擴到大世界。

施工：

```text
StreamingCell
Stable Fixed Grid
Loose Quadtree
Room / Portal
Streaming Source
Demand
Priority
Hysteresis
Prefetch
Offline HLOD
Terrain
Vegetation
LOD
Culling
```

**Gate：**

```text
✓ Cell / Bundle identity 分離
✓ Far HLOD 可在 Full Cell unload 時存在
✓ Terrain / Vegetation 不建立大量 Scene Entity
✓ Room / Portal 可驅動 prefetch
✓ Streaming RAM / VRAM budget 可觀察
✓ Character 跨 Streaming Cell 邊界不失去 Collision（Occupied Cell Pinned）
```

---

## ✅ V1-M11 — Mobile / Platform / WebView

**目的：** 完成產品層平台整合，而不是到最後才第一次 Port。

施工：

```text
App Lifecycle
Permissions
Safe Area
Orientation
Thermal
Haptics
Clipboard
Deep Link
IME
Native Share
Platform Login extension
Android GPU Workaround
WebView2 / Android WebView / WKWebView
```

**Gate：**

```text
✓ iOS / Android 實機完整 run
✓ Background / Resume 正常
✓ WebView ownership / touch routing 正常
✓ WebView 不進 RenderGraph UI pass
✓ Thermal / memory pressure 可觸發 policy
```

---

## ✅ V1-M12 — Shipping / Packaging / Hardening

**目的：** 把「能做遊戲」變成「能交付」。

施工：

```text
Minimal Profile
Full Profile
Monolithic Shipping
LTO / WPO
Shader Strip
Asset Strip
Plugin Strip
SDK Strip
Bundle Update
Rollback
Crash Report
Soak Test
Device Matrix
```

**Gate：**

```text
✓ Disabled Plugin 不進 package
✓ Disabled Shader Family 不進 cooked shader
✓ Dedicated / Headless 無 presentation baggage
✓ 四平台 Shipping Profile 可啟動
✓ Update / rollback / restart 可持續使用
✓ Long-run Scene / Streaming / Audio / Asset generation 無 leak
```

---

## V1 施工依賴圖

```text
M0 Build / CI
 │
 ▼
M1 Core Runtime
 │
 ├──────────────┐
 ▼              ▼
M2 RHI PoC   ABI / Plugin Skeleton
 │
 ▼
M3 Renderer
 │
 ▼
M4 Scene Vertical Slice
 │
 ▼
M5 Asset / Cook / Bundle
 │
 ▼
M6 Editor / Reflection / Plugin SDK
 │
 ├─────────────┬─────────────┐
 ▼             ▼             ▼
M7 UI/Input   M8 Gameplay   M9 Media/Animation
               │             │
               └──────┬──────┘
                      ▼
               M10 Large World
                      │
                      ▼
                M11 Platform
                      │
                      ▼
                M12 Shipping
```

## V1 Renderer Scope 修正

施工時正式採：

```text
V1 必做：
CPU Frustum Culling
Static BVH / Spatial Grid
Instancing
LOD
Occlusion foundation
RenderGraph
Streaming / HLOD
```

```text
V1.x Optional / PoC：
Indirect Draw
Hi-Z prototype
GPU Culling prototype
```

```text
V2 Production：
GPUScene
Compute Culling
GPU LOD
Instance Compaction
Indirect Generation
Async Compute
```

這避免 V1 被 V2 GPU-Driven 目標拖慢。


## 引擎階段性開發路線圖 (Engine Milestone Roadmap)

本 Roadmap 採「基石先行、縱向切片（Vertical Slice）、漸進擴充、持續驗證」策略。

編譯期裁切（Feature Stripping）等基礎架構能力於 Phase 0 即完成設計，並於各階段隨功能開發陸續補齊對應的 CI 驗證閘門（CI Gates）。

### Phase 0: Foundation / Build / Memory / Shader & RHI PoC

**核心目標：**

建立基礎設施、記憶體隔離機制，並驗證跨平台 Shader Canonical Reflection 一致性。

**重點任務：**

- **Build System Setup**
  - 設定 CMake 支援 Modular（開發期）與 Monolithic（發行期）雙編譯模式。
  - 建立 Compile-time Feature Stripping 標籤架構。
  - 建立基礎 Module Graph / Dependency Rule。
  - PCH、clang-format、clang-tidy、Warnings-as-Errors 與基礎 CI 從此階段啟用。

- **Job System Foundation**
  - Work-Stealing Scheduler foundation。
  - Task dependency / priority / frame barrier contract。
  - Frame End Barrier 與 scratch lifetime assertion。
  - High-level TaskGraph metadata / CancellationToken / CompletionFence foundation。
  - System Scheduler access declaration prototype。

- **Persistent Memory / Diagnostics Foundation**
  - EngineAllocator / MemoryResource abstraction；mimalloc 作 V1 internal default backend。
  - Allocation Tag / subsystem statistics。
  - Structured async Logging / rotating file / crash ring buffer。
  - VFS mount table / Async IO Scheduler foundation。
  - Real / Game / Fixed clock + TimerScheduler foundation。

- **Memory Isolation**
  - 實作 `FrameAllocatorSet`。
  - Main / Render / Worker 各自使用專屬 Arena。
  - 加入跨 Frame 物件逃逸檢查。
  - 加入 Thread Ownership / Reset Timing Assertion。

- **Cross-Backend Shader Contract (PoC Gate)**
  - 使用同一份 Slang Source 建立以下編譯鏈：

```text
Slang Source
├─ DX12
│  └─ DXIL
├─ Vulkan
│  └─ SPIR-V
└─ Metal
   ├─ Direct
   │  └─ MSL
   └─ Fallback
      └─ SPIR-V → SPIRV-Cross → MSL
```

- **Canonical Reflection Gate**
  四條路徑輸出的 Canonical Reflection Contract 必須一致，至少驗證：

```text
Resource ID
Binding
Type
Stage
Constant Layout
Texture / Sampler
Argument Buffer Mapping
Material Parameter Layout
```

- Frame Memory type-safe wrapper（`FramePtr / FrameSpan`）與 AST persistent-storage prohibition。

- Worker Arena Page / Chunk Growth Policy 與 budget/trim telemetry。
- AI Change Scope / Risk Class baseline 與 ADR policy。

- Unified Architecture Dependency Graph Tool / module rule manifest baseline。

- Shader Reflection → generated C++ layout prototype。

- Shader Variant stage accounting（Theoretical / Pruned / Used / Cooked / Budget）。

- **Gameplay Language Boundary Foundation**
  - 定義 Language-Neutral Stable C ABI。
  - Opaque Handle / POD / versioned function table / allocator ownership contract。
  - Zig binding header / codegen PoC。
  - 禁止 C++ private class / STL owning type 穿越 Gameplay ABI。
  - 建立最小 Zig `GameModule_Init / Update / Shutdown` smoke test。
  - **Zig Mobile Toolchain PoC**：驗證 Zig cross-compile 至 `aarch64-android` / `aarch64-ios` 能成功產出可連結的最小 static/dynamic library，並能被對應平台最小 Host App 載入呼叫。此為 Zig 工具鏈本身的可行性驗證，範圍不含完整 Engine Mobile 整合（見 Phase 6）。

- **Shading Model Framework PoC**
  - PBR / StylizedPBR / Anime / Unlit enum + metadata。
  - Shared lighting input contract。
  - Minimal Anime Ramp / Face SDF prototype。
  - Minimal StylizedPBR scene material prototype。
  - Variant / stripping key includes ShadingModel。

**Gate：**

- Modular / Monolithic 最小 Build 可通過。
- Frame Allocator Thread Ownership 測試通過。
- 不允許 Frame-memory escape。
- DX12 / Vulkan / Metal PoC 能使用同一份 Slang Shader 成功繪製 Triangle。
- Metal Direct 與 Fallback path 的 Canonical Reflection 必須一致。

- Windows / macOS / Android / iOS 均能成功編譯並執行最小 Zig GameModule / Toolchain smoke test，且 ABI mismatch 能被拒絕。Android / iOS 若在此階段未能通過，Zig 作為 Primary Gameplay Language 的選型必須重新評估，不得延後至後續 Phase 才發現。

---

### Phase 1: DX12 Reference RHI + Render Graph + Frame Pipeline

**核心目標：**

完成首個完整參考 RHI（DX12），打通 Render Graph 與雙執行緒繪製主幹。

**重點任務：**

- **Reference RHI (DX12)**
  - 優先完成 Direct3D12。
  - 作為 RHI Reference Implementation。

- **Secondary RHI (Vulkan)**
  - 用於反向驗證 RHI abstraction 未發生 DX12 偏向化。
  - 驗證 Resource State / Barrier / Descriptor abstraction 是否足夠通用。

- **Metal RHI (Catch-up)**
  - 基於 Phase 0 Shader / RHI PoC 補齊基礎接口。
  - 驗證 Argument Buffer 與 Resource Table mapping。

- **PSO Cache Foundation**
  - PSO Key / async create / compatible cache identity。
  - Runtime synchronous PSO creation telemetry。

- **Render Graph & Pipeline**
  - Transient Resource 自動生命週期管理。
  - Barrier 動態插入。
  - Pass Dependency Topological Sort。
  - Main / Render 雙執行緒同步。
  - Render Command 傳遞與 Frame Fence。

- Public RHI 採 `BeginRendering / EndRendering` modern semantics；Backend capability fallback 僅存在於內部。

- Render Graph Transient Aliasing Hazard Validator。

- Offline `PipelineLayoutMetadata` generation / direct runtime binding path。
- Transition Planner / Barrier Merge / redundant-barrier metric。

- Runtime PSO Residency Budget / LRU / Fence-safe Eviction foundation。

- PSO Fence-safe Eviction 支援 unified timeline 與 independent multi-queue timeline。

**Gate：**

- Offscreen Pass + Main Pass 可正常運作。
- Render Graph 自動處理 Barrier / Resource Transition。
- DX12 為完整 Reference Backend。
- Vulkan 可跑相同基本 Render Graph workload。
- Metal 可跑基礎相同 workload。

---

### Phase 2: First Vertical Slice

**範圍：**

```text
Scene
+
Component
+
Mesh
+
Material
+
Camera
+
Light
```

**核心目標：**

打通引擎第一個真正可用的：

```text
Data
↓
Scene
↓
Render
```

完整鏈條。

**重點任務：**

- **Core Scene / World Pipeline**
  - World / Scene ownership model。
  - EditorWorld / PlayWorld separation。
  - Scene Graph。
  - Node / Component。
  - Transform hierarchy。
  - Multiple / Additive Scene foundation。
  - Persistent Scene。
  - Scene state machine：Unloaded → Loading → LoadedInactive → Active → Unloading。
  - Async load + atomic activation foundation。
  - WorldCommandBuffer / structural barrier foundation。
  - Camera。
  - Light。
  - Transform batch propagation / Large-world coordinate foundation。
  - Access-declared System Scheduler / WorldCommandBuffer integration。
  - Typed Engine Event Framework。
  - CameraView extraction / multi-camera / viewport / screen-ray。
  - Lighting registry / Forward+ / Shadow Manager foundation。
  - PostProcess Volume / Profile foundation。

- **Mesh & Material Pipeline**
  - MeshRenderer
  - Material
  - Slang Shader
  - RHI Resource Binding
  - Basic PBR / Lit

- **Asset Import Slice**

```text
FBX / glTF Source
↓
Importer
↓
Runtime Mesh
↓
Asset Database
↓
Scene
↓
Renderer
```

- General Scene Component Pool 採 Sparse Set + Dense SoA default storage。

**Gate：**

- Editor / Runtime 能載入模型。
- Scene 中可建立 Camera / Light / MeshRenderer。
- Material 能綁定 Slang Shader。
- Build 後 Runtime 能顯示相同模型。
- Asset UUID / dependency 能完整解析。
- Scene loading 能收集必要 PSO 並執行 warmup。
- EditorWorld 與 PlayWorld 狀態隔離。
- 至少兩個 Additive Scene 可同時 Active，並共享同一 Physics / Render World foundation。
- Scene 可完成 LoadedInactive → atomic Active → safe Unload 的完整 lifecycle。

---

### Phase 3: Asset Pipeline + Bundle + Streaming Foundation + Hot Update

**核心目標：**

建置資產數據庫、Bundle 打包、遠端 Manifest 與熱更新機制。

#### Phase 3A: Asset Core & DAG

- Asset Database
- UUID
- Runtime Binary Format
- Streaming Residency State Machine
- RAM / VRAM Budget Foundation
- World Partition foundation。
- Stable Fixed Grid Streaming Cells。
- Loose Quadtree spatial index。
- Room / Portal graph foundation。
- Outdoor ↔ Indoor Streaming Gateway。
- Multiple Streaming Sources / StreamingDemand。
- Streaming Priority / Hysteresis / Prefetch。
- Offline HLOD Builder foundation。
- HLOD assets enter Asset / Bundle pipeline。
- VFS / Async IO production integration。
- Importer Registry / deterministic Local DDC。
- Generic Serialization cook → relocatable runtime blob。
- Mesh optimization / collision / LOD cooker。
- Project Manifest / Settings / Build Profile integration。
- Bundle Builder
- Bundle Dependency Graph
- Bundle Dependency DAG Check
- Cycle Detection → Build Fail

#### Phase 3B: Remote & Hot Update

- Remote Manifest
- Versioned Cache
- Bundle Download
- Hash Verification
- Atomic Activation
- Active Manifest
- Pending Update
- Rollback
- `.prev` / versioned directory strategy
- mmap / open handle safe-unload rule
- Pre-init deferred activation

#### Phase 3C: Hot Update Verification

使用通用 Binary Asset 驗證完整流程：

```text
Build
↓
Publish
↓
Manifest Compare
↓
Download
↓
Verify
↓
Activate
↓
Reload
↓
Rollback
```

此階段不得依賴 FMOD / Spine 等第三方 Middleware 才能完成 Bundle 驗證。

- Bundle Versioned Storage + Generation Pinning / Load Context isolation。
- World Partition Build 在同樣 Scene / Partition / Builder Version 下產生 deterministic Cell / HLOD build output。
- Streaming Cell 與 Bundle identity 分離。
- Streaming Source 可觸發 Prefetch → BuiltInactive → Active，並具 hysteresis。
- Room / Portal Graph 可驅動 Indoor prefetch。
- HLOD 可在 Full Cell 非 resident 時維持 far visual representation。

- Bundle Pending Delete Queue / async mapping reference drain。

- Multi-platform Texture Cook Profile（BC*/ASTC by semantic/profile）。

- Streaming Scheduler priority preemption / per-platform IO concurrency budget。

#### Phase 3D: JSON / DataTable Foundation

- Engine JSON Framework：yyjson private backend + JsonDocument / JsonWriter abstraction。
- JSON Parse / Generate / Deterministic Write。
- DataTable JSON Runtime Asset。
- DataTable Schema / Strict Validation。
- Primary Key：UInt32 / UInt64 / String。
- Secondary Index / Strong ID / String Key Runtime Index。
- Typed Table Build。
- DataTable Preprocess Pipeline：Normalize / Resolve / Derive / Index / Optimize / Finalize。
- Sorted View / Group Index / Weighted Table / Range Table foundation。
- Custom Processor + Processor DAG / version / deterministic contract。
- Runtime Container：Contiguous Rows + Indices + Views + StringPool / ArrayPool + DerivedData。
- Immutable Table / DataTableRegistry。
- DataTable Generation N → N+1 safe publish / rollback。
- DataTable-specific C++ / C ABI / Zig typed schema codegen foundation。

**Gate：**

- Bundle DAG 無循環。
- Download / LoadBundle / LoadAsset 完全分離。
- Runtime 能安全更新 Binary Asset。
- DataTable JSON 可 Parse → Validate → Preprocess → Finalize → Publish typed runtime table。
- UInt32 / UInt64 / String Primary Key lookup 正確，Duplicate Key 必須 Fail。
- Sorted View / Secondary Index 不修改 Row Identity。
- DataTable Preprocess determinism / Processor DAG cycle validation 通過。
- DataTable Hot Reload 失敗時保留舊 Generation，不破壞 Runtime。
- DataTable Public / Stable ABI 不暴露 yyjson / STL container。
- Active mmap / file handle 不會造成錯誤替換。
- 更新失敗能 Rollback。
- 重啟後能沿用已下載內容。

---

### Phase 4: Editor + Runtime UI + Plugin System

**核心目標：**

建立工具鏈編輯器、Runtime UI Framework、C++ / Native Plugin 擴充機制，以及 Zig Gameplay Module 開發工作流。

**重點任務：**

- **Editor**
  - EditorDocument / Selection / EditorObjectAdapter architecture。
  - Inspector / PropertyDrawer registry。
  - Transaction Undo / Redo。
  - Nested Prefab / Variant / Override conflict workflow。
  - Project Settings / Plugin extension points。
  - Runtime Debug / CVar / DebugDraw tooling。
  - Hierarchy
  - Inspector
  - Asset Browser
  - Scene View
  - Game View
  - Console
  - Build Settings
  - Profiler Foundation

- **Runtime UI Framework**
  - Custom Retained Mode Runtime UI；Dear ImGui 僅 Editor / Debug。
  - `UIDocument` / `UIElementID` / UI Tree / `UIComponent`。
  - `UIElement ≠ SceneNode ≠ EntityID`。
  - Editor Hierarchy Adapter：Scene Entity + Mounted UI Hierarchy。
  - RectTransform / Anchor / Pivot / Layout / Safe Area。
  - Panel / Image / Label / Button / Toggle / Slider / ProgressBar / InputField。
  - ScrollView / VirtualizedListView。
  - Nine-Slice。
  - FreeType + HarfBuzz Text / Localization。
  - Pointer Event / Capture-Target-Bubble / Focus / Gamepad Navigation。
  - ViewModel / Data Binding。
  - JSON UIDocument Asset / Hot Reload。
  - UI Style / UI Animation / Tween。
  - UI Render Extraction / Batch / Clip / RenderGraph。
  - ScreenSpace / WorldAnchored / WorldSpace。
  - WorldSpace UI Ray Mapping / Depth / Occlusion。
  - Multi-resolution / Safe Area Preview。

- **Plugin Foundation**
  - Editor Plugin
  - Runtime Build-time Plugin
  - Static / Dynamic Module
  - Plugin Manifest
  - Dependency Resolution
  - Engine API / ABI Validation

- **Zig Gameplay Tooling**
  - Primary Gameplay Language：Zig。
  - Stable C ABI Binding。
  - GameModule build / load / reload。
  - Build error → Editor Console source mapping。
  - Explicit Tick / Event / Batch API。
  - Reflection metadata bridge。
  - Versioned state serialize / migrate / restore。
  - Reload safe barrier / old callback cleanup。
  - External IDE / native debugger launch or attach workflow。

**Architecture Boundary：**

```text
Plugin
→ Local Installed / Build-time Native Code

Bundle
→ Asset / Data / Remote Content
```

Native Plugin 永不透過 Remote Bundle 下載。

- **Reflection Tooling**
  - Canonical Reflection Metadata Schema。
  - V1 Macro/constexpr 與 V2 Clang Tool 共用同一 consumer contract。

- Dynamic Module Engine Allocator Contract / ABI-safe Public API。

- Frame Memory × Module Boundary Contract：FrameSpan / FrameDataHandle / no async retention。

- Runtime Plugin `Plugin_Init()` ABI/version/hash handshake。

**Gate：**

- Editor 可載入 Plugin。
- Runtime Optional Module 可被 Build Profile 啟用 / 移除。
- Plugin API 不暴露 Engine Private Header。
- Runtime UI 可建立基本遊戲介面。
- UIElement 不依賴 SceneNode / EntityID，UIDocument 可掛載於 Scene UIComponent。
- Editor Hierarchy 可同時編輯 Scene Entity 與 UIElement。
- ScreenSpace / WorldAnchored / WorldSpace 共用 Widget / Layout / Event Framework。
- VirtualizedListView 不依資料總量建立等量 UIElement。
- Runtime UI Logical Resolution 與 3D Dynamic Resolution 完全解耦。

- Zig GameModule 可從 Editor build 並載入。
- 修改 Zig gameplay code 後可安全 reload，不保留舊 module code pointer / callback。
- State migration 失敗時可拒絕 reload 並保留舊 module 或安全停止 Play Session。
- Gameplay ABI 不暴露 C++ STL / private type。

---

### Phase 5: Animation + Physics + Audio + FMOD / Spine Optional Plugins

**核心目標：**

整合動態系統與 Middleware，並用第三方資產驗證 Asset / Bundle Extensibility。

**重點任務：**

- **Animation**
  - Skeleton
  - Animation Clip
  - Crossfade
  - Root Motion
  - GPU Skinning foundation
  - Dynamic Skinning Buffer / packed bone matrices / per-draw offset

- **Physics**
  - Collider / Physics Material / Collision Matrix editor tools。
  - Character / Contact / Query debug visualization。
  - Jolt CPU-authoritative Physics。
  - Collision / Rigidbody / Trigger。
  - Character / Query foundation。
  - Immediate Query / CPU Batch Query。
  - Physics Execution Domain：CPUAuthoritative / CPUBatched / GPUVisual / GPUDeferred。
  - GPU VFX Collision integration。
  - GPU Cloth foundation。
  - GPU Deferred Query interface。
  - Physics CPU/GPU profiler foundation。
  - Character Framework：CharacterIntent → CharacterMotor → CharacterController。
  - Ground / Slope / Step / Snap。
  - Moving Platform / Dynamic Body Interaction。
  - Root Motion Resolve / External Velocity / Knockback。
  - Crouch / Capsule Resize / Teleport semantics。
  - Fixed Tick / Render Interpolation。
  - Batch-first Zig Character ABI。

- **Time / Tick Integration**
  - Fixed Simulation Tick
  - Variable Render Frame
  - Transform interpolation
  - Catch-up clamp / spiral-of-death protection
  - WorldTimeState / GameTime / UnscaledTime / FixedTickTimer。

- **Navigation / AI**
  - Recast / Detour tiled NavMesh backend。
  - NavTile streaming / async path query / OffMesh Link / runtime obstacle。
  - Navigation → DesiredVelocity → CharacterIntent integration。
  - Blackboard / Behavior Tree compiled runtime。
  - Perception budget / AI update LOD。
  - ML Policy interface foundation。

- **Audio**
  - Audio Event Authoring：waveform / event graph / bus graph / snapshot / preview。
  - MiniAudioBackend
  - AudioManager
  - Streaming Audio
  - Audio Handle
  - AudioResourceRegistry
  - AudioResidencyScope：Global / Scene / Character / UI。
  - Shared Resource Residency Ref / Voice Pin / Streaming Pin。
  - Scene transition preload / release。
  - Scope Release Policy / FadeOutAndRelease。
  - Audio Residency Priority / Memory Budget integration。
  - Audio Residency Profiler。

- **FMOD Optional Plugin**
  - FMOD Studio
  - Event
  - Bank
  - Parameter
  - Bank safe unload / reload
  - FMOD Bank 作為特殊 Bundle Asset

- **Spine Optional Plugin**
  - Runtime
  - Editor importer
  - Animation preview
  - Skin / Event
  - Bundle Asset integration

- `JobSystemTaskAdapter` for Jolt / middleware / plugin task integration。

- **VFX / Particle Runtime Foundation**
  - VFX Asset / Logical Emitter / Module Graph。
  - CPU Particle SoA。
  - Stateless Emitter。
  - Basic GPU Particle Pool。
  - Sprite / Mesh / Trail renderer。
  - Soft Particle。
  - VFX Instance Shared ParameterBlock。
  - Gameplay / Visual separation。

- **Animation Framework Foundation**
  - Skeleton Viewer / Clip Preview / Event Track / Retarget / Graph Editor authoring。
  - Skeleton / Animation Clip runtime format。
  - Animation Graph Compiler。
  - State Machine / Blend Tree / Layer / Bone Mask。
  - Pose Cache。
  - Root Motion。
  - Basic IK。
  - GPU Vertex Skinning。
  - Global Skinning Buffer / skinningMatrixOffset。
  - Skinned Mesh Instancing。
  - Animation Update LOD / Skeleton LOD foundation。

**Gate：**

- MiniAudio path 可獨立工作。
- Release Scene AudioResidencyScope 不會影響 Global / UI / Music / other active Scope。
- Active Voice / Streaming Pin 可阻止 Audio Resource premature unload。
- Scene A → Scene B Audio preload / release 不產生同步 load spike。
- FadeOutAndRelease 完成前 Audio Resource 不得被回收。
- FMOD Disabled 時不 Link / Package FMOD。
- Spine Disabled 時不 Link / Package Spine Runtime。
- FMOD Bank 可走 Bundle Update。
- Plugin Native Library 不走 Remote Update。

---

### Phase 6: WebView + Platform Services + Mobile Integration

**核心目標：**

完成移動端整合、Native Platform Service 與 Overlay Layout。

**重點任務：**

- **Native Overlay WebView**
  - Runtime UI `WebViewElement` / `UIElement` integration。
  - Windows WebView2。
  - Android WebView。
  - iOS / macOS WKWebView。
  - Native Overlay V1；OffscreenTexture future capability。
  - Local HTML Asset / Remote HTTPS。
  - JSON Message Bridge / Origin Whitelist / Trust Policy。
  - Cache / Cookie / Storage Profile。
  - Gesture single-owner routing。
  - Screen-space absolute bounding box
  - Viewport Sync
  - Safe Area
  - DPI / Retina / Density
  - Orientation

- **Input Hit-Test**

```text
Block
Pass-Through
```

- **Unified Input Framework**
  - Raw / Device-level API；Action Mapping 為 optional convenience layer。
  - User-defined InputAction / ActionMap；Engine 不保留 Move / Jump / Attack 語意。
  - Keyboard + Mouse + Gamepad + Touch 可同時作用於同一 InputUser。
  - Multi-touch + stable PointerID + per-pointer Capture / Ownership。
  - VirtualJoystick / VirtualButton / VirtualDPad / VirtualTouchRegion。
  - InputContext + coarse-grained InputLayer / Routing Layer。
  - Project-level UI Input Interaction Matrix：`InputLayer → UILayer`。
  - Matrix Cook → compact `UILayerMask` bitset。
  - UIDocument Priority / PassThrough / ConsumeOnHit / BlockBelow。
  - Local Multiplayer UI routing。
  - UI / WorldSpace UI / WebView / Gameplay single-owner routing。
  - Fixed Tick Input Snapshot / Zig batch C ABI / Replay foundation。

- **Android GPU Compatibility**
  - GPUWorkaroundDatabase
  - Capability Override
  - Restricted / Blacklist Tier

- **Platform Services**
  - App Lifecycle / Permissions / Deep Link / Clipboard / Dialog。
  - Battery / Thermal / Safe Area / Orientation / Haptics。
  - Localization ICU backend + locale data cook。
  - TextEdit / IME / grapheme / mobile keyboard integration。
  - External Browser
  - Clipboard
  - Native Share
  - Authentication extension points
  - Google / Apple Login integration path
  - Platform SDK bridge
  - Touch / Input adaptation

- WebView 僅以 Logical Screen Space 定位，與 Dynamic Resolution 完全解耦。

- `PerformancePolicyManager`：Thermal / Frame Pacing / Dynamic Resolution / Target FPS。

- Native Overlay Gesture Ownership / optional Forwarding capability validation。

**Gate：**

- WebView 不參與 Canvas Batch / Render Graph UI Pass。
- Mask / Depth / Rotation 等 unsupported behavior 在 Editor 中被明確限制。
- Hit-Test 行為跨平台一致。
- UI Input Interaction Matrix 可在 Editor 設定並正確 Cook 為 Runtime Mask。
- Keyboard / Mouse / Gamepad 可同時作用，不因 LastActiveDevice 互斥停用。
- Multi-touch / Virtual Control 的 per-pointer ownership 無 duplicate delivery。
- iOS / Android 實機驗證通過。

---

### Phase 7: LOD + Culling + Terrain + Vegetation + Streaming

**核心目標：**

實作大場景最佳化與資產動態載入。

**重點任務：**

- **LOD**
  - Mesh LOD
  - Screen-space LOD
  - Hysteresis
  - Dither Crossfade
  - Material / Shadow / Animation LOD

- **Culling**
  - Frustum Culling
  - Static BVH
  - Dynamic Spatial Hash / Grid
  - Terrain Quadtree
  - Vegetation Cluster
  - Occlusion foundation

- **Terrain**
  - Heightmap
  - Chunk
  - Quadtree
  - Screen-space Error
  - Terrain Collision
  - Streaming

- **Vegetation**
  - Tree Asset
  - GPU Instancing
  - Cluster Culling
  - Wind
  - Billboard
  - Streaming Cell

- Terrain Height Encoding Profile（R16_UNORM / FP16）與 LOD Seam Prevention。
- Vegetation Interaction Field（GPU-driven local bend）。

- **Stylized Scene Shading**
  - StylizedPBR Architecture / Rock / Prop material。
  - Static Lightmap / Probe / Reflection Probe integration。
  - Shadow Tint / Light Tint / Stylization Curve。
  - Lighting LOD foundation。
  - Environment Fog / Height Fog / Cloud Shadow integration。

- **Vegetation Shading**
  - Alpha Cutout。
  - Wind Vertex Animation。
  - Transmission / Back Lighting。
  - Probe / SH lighting。
  - Instancing-compatible material path。

- **Water Foundation**
  - Shallow / Deep Color。
  - Depth Fade。
  - Fresnel。
  - Reflection Probe fallback。
  - Refraction / Foam extension points。

**Gate：**

- Large Scene 可依 Streaming Radius 動態載入 / 卸載。
- Terrain / Vegetation 不建立大量 Scene Node。
- Memory Budget 能追蹤 Streaming Residency。
- StylizedPBR / Vegetation / Water 不建立獨立 Renderer。
- Vegetation Back Lighting / Wind 在 representative mobile profile 通過畫面與效能測試。
- Static Environment 可同時使用 Main Directional Light + Lightmap/Probe + Fog。

---

### Phase 8: GPU Driven + Advanced Rendering

**核心目標：**

推進高階繪製管線與 GPU-driven architecture。

**重點任務：**

- **GPU Driven Pipeline**
  - GPU Culling
  - Instance Compaction
  - Draw Indexed Indirect
  - Hi-Z Pass
  - GPU LOD Selection

- **Advanced Rendering**
  - Advanced Post-processing
  - Specialized Render Feature API
  - Custom Render Pass
  - Custom Compute Pass

- **Anime Shading Framework**
  - Multi-Ramp Material Lighting
  - Material Region / AnimeControlMap
  - Face SDF Shadow
  - Hair-specific Highlight
  - Stylized Specular
  - Material-aware Rim Light
  - Geometry Outline / Smoothed Normal
  - Eye feature extension
  - Bangs Shadow extension

- **Advanced Environment**
  - SSR + Reflection Probe fallback
  - Advanced Water Reflection / Refraction / Foam
  - Atmosphere / Fog refinement
  - Anime-friendly Tone Mapping / Color Grading profile

可後續評估：

```text
Ray Tracing
Dynamic Global Illumination
Advanced Temporal Techniques
```

上述項目不應阻塞基礎商業發行版本。

- **Advanced VFX**
  - GPU Simulation / Compaction / Indirect Draw。
  - Runtime Batch Fusion。
  - Optional Sort。
  - Async Compute integration。
  - Ribbon / Decal / Distortion。
  - Event Buffer / SubEmitter DAG。
  - Collision Tier。
  - VFX Budget Manager。
  - Visibility / Simulation Rate LOD。
  - Profiling / overdraw / batch-fusion diagnostics。

- **GPU Crowd Animation**
  - Automatic Bone Animation Texture / GPU Pose Storage Cook。
  - GPUAnimationClip metadata / dependency invalidation。
  - Final Skin Matrix bake path。
  - Frame interpolation。
  - Per-instance animation time。
  - Simple GPU crossfade。
  - Animation Sharing / Pose Bucketing。
  - GPU Culling + Indirect Draw integration。
  - Compute Skinning / multi-pass reuse。
  - VAT / Impostor extension path。

- **Advanced GPU Physics**
  - GPU Cloth refinement / async compute scheduling。
  - GPU Debris pool / indirect rendering。
  - GPU Deferred Query implementation。
  - Heightfield / SDF / simplified-scene GPU collision。
  - PerformancePolicyManager / thermal / memory budget integration。
  - Optional GPU Rope / Chain。
  - Optional GPU SoftBody R&D。
  - GPU Broadphase 僅在 profile 證明必要時導入。
  - GPU RigidBody World 保留 Future R&D，不作 V1 前置條件。

**Gate：**

- GPU-driven path 與 CPU fallback path 可比較。
- Profiler 能顯示 GPU Culling / Indirect Draw / Hi-Z 成本。
- Advanced Render Feature 不破壞 Renderer Core Contract。
- Anime / StylizedPBR / Vegetation / Water 共用同一 Forward+ / RenderGraph 基礎。
- Anime Face SDF、Hair Highlight、Outline 代表性 Golden Image 通過。
- Mixed Scene（StylizedPBR Environment + Anime Character）跨 DX12 / Vulkan / Metal 通過。
- SSR unavailable / disabled 時 Water / Scene Reflection fallback 正確。

---

### Phase 9: Feature Stripping + Packaging + Full Shipping Matrix

**核心目標：**

完成商業發行全管道驗證、最終模組裁切與發行最佳化。

Feature Stripping 的 Architecture 在 Phase 0 即建立；Phase 9 的工作是完整驗證與 Shipping Harden。

**重點任務：**

- **Feature Stripping Validation**
  - Auto / Enabled / Disabled
  - Optional Subsystem
  - Plugin
  - Native Library
  - Runtime Asset
  - Shader Variant
  - Build Dependency

若：

```text
Feature = Disabled
+
Code / Asset Dependency Exists
```

則：

```text
CI / Build Fail
```

- **Shipping Optimization**
  - Monolithic Build
  - LTO
  - Strip Debug Symbol from shipping package
  - Disable unnecessary dynamic exports
  - Shipping asset cook
  - Shader Variant final strip

- **Shipping Matrix**
  - Windows DX12
  - Windows Vulkan
  - macOS Metal
  - Android Vulkan
  - iOS Metal
  - Representative device / quality / binding tier

- CI Build Cache / Matrix Parallelization 與 pipeline latency telemetry。
- Profiler Chrome Trace / JSON export shipping-strip validation。

- 長時間多 Scene / Mobile PSO Resident 收斂與 eviction/recreate stability 驗證。

- **Zig Gameplay Shipping Validation**
  - Windows / macOS / Android / iOS Gameplay Module build。
  - Mobile Shipping 不包含 runtime native-code download path。
  - Zig runtime / support code 只保留實際需要部分。
  - Shipping symbol / debug metadata 依平台分離封存。
  - Gameplay ABI version / Build ID 與 App package 一致性驗證。

**Gate：**

- Minimal Profile Build 成功。
- Full Feature Profile Build 成功。
- Shipping Monolithic Build 成功。
- Disabled Subsystem 不存在於 package。
- 不需要的 DLL / dylib / so / Framework 不進包。
- Shader Variant / Asset / Module stripping Report 正確。

- 四平台最小 Zig Gameplay sample 在 Shipping Profile 可成功建置與啟動。

---

## 核心執行原則 (Architecture & CI Enforcement Guidelines)

### 1. CI Gate 即架構契約

CI 不是開發完成後的最後驗證工具，而是架構防護牆。

正式規範：

> 每當新增一項 Architecture Contract，必須同步建立能自動驗證它的 Automated CI Gate。

包含：

- Bundle DAG 循環依賴掃描
- Shader Canonical Reflection 一致性比對
- Subsystem Feature Stripping 阻斷驗證
- Frame Allocator Thread Ownership
- Frame-memory escape assertion
- Dynamic Skinning Frame-slot fence validation
- Skinning ResourceIndex / element-index contract validation
- RHI Resource Lifetime Validation
- Render Graph Barrier / Dependency Validation
- Plugin API / ABI Compatibility
- Minimal / Full Feature Build Matrix
- PSO Runtime Creation / Warmup metrics
- Reflection Metadata Schema Validation
- Memory Pressure / Device Recovery Smoke Test
- Android Workaround Profile Validation

- Zig Gameplay ABI Compatibility / Layout Check
- Zig GameModule Reload Lifetime / stale callback validation
- Zig Gameplay Cross-platform Build Matrix
- Gameplay ABI fine-grained call-count / allocation regression metric

- Shading Model Variant / Stripping Validation
- Anime Face SDF / Hair / Outline Golden Image Regression
- StylizedPBR Lightmap / Probe / Fog Integration Test
- Vegetation Transmission / Wind / Instancing Regression
- Water Reflection Fallback Validation
- Mixed Scene Single-Renderer Contract Validation

CI 執行層級：

```text
PR
→ Representative Profile
→ Fast Architecture Gates

Nightly / Scheduled
→ Minimal Profile
→ Full Feature Profile
→ Wider Backend Matrix

Release
→ Full Shipping Matrix
→ Representative Physical Devices
```

### 2. 嚴格維護 Plugin 與 Bundle 邊界

Native Code：

```text
Zig GameModule Binary
.dll
.so
.dylib
Framework
Static Library
```

永遠屬於：

```text
Plugin / App Build / Installation
```

Remote Bundle 僅限：

```text
Asset
Data
Runtime Binary Asset
FMOD Bank
Spine Asset
Localization
Scene
Terrain / Vegetation Chunk
```

禁止任何將 Native Code 塞入 Remote Bundle 並於 Runtime 下載執行的方案。

### 3. 優先進行真實硬體驗證

模擬器 / Desktop Simulation 不可視為最終 Hardware Validation。

應優先實機驗證：

- Metal Argument Buffer
- Android Descriptor Indexing
- Android GPU Vendor Compatibility
- WebView Hit-Test
- Safe Area / Orientation
- Touch / IME
- Suspend / Resume
- Thermal
- Frame Pacing
- Memory Pressure
- Bundle mmap / file lifetime
- Mobile GPU synchronization

### 4. Vertical Slice 優先於功能堆疊

每個大型 Subsystem 應盡早建立可執行的 End-to-End Slice。

避免：

```text
Renderer 做了一半
Asset 做了一半
Editor 做了一半
但沒有任何完整工作流
```

優先維持：

```text
Import
↓
Asset
↓
Scene
↓
Render
↓
Editor
↓
Build
↓
Runtime
```

持續可執行。

### 5. Feature Stripping 從 Phase 0 設計，Phase 9 Harden

Feature Stripping 不是 Phase 9 才開始實作。

```text
Phase 0
→ Module / Feature Architecture

Phase 1~8
→ 每新增 Subsystem 就同步加入 Stripping Rule

Phase 9
→ Full Matrix / Shipping Validation / Size Optimization
```

避免引擎完成後才嘗試拆除已形成的硬依賴。

## v4.0 Architecture Completion 狀態

本版已將先前「尚未細談系統清單」中的核心項目轉為正式設計決策；仍標示 Future / Optional 的項目不視為遺漏。

```text
Core Runtime foundation
→ Transform / System Scheduler / Event / Time / Memory / IO / Logging

World foundation
→ Scene / Streaming / Navigation / AI / Save State

Presentation foundation
→ Camera / Lighting / Post / Runtime UI / Audio / Animation

Tooling foundation
→ Editor / Prefab / Undo / Reflection / Import / Authoring Tools / Project Settings
```

## 五十四、最終架構摘要

Development Model:
AI-Assisted Development
AI 生成 + 自動化 Gate + 實機驗證 + Definition of Done

Language:
Engine Core / Renderer / Editor = C++20
Primary Gameplay Language = Zig
Engine ↔ Gameplay = Language-Neutral Stable C ABI

Gameplay Scripting:
Zig native GameModule
No tracing GC
Batch-first API + Explicit Tick + Event-driven
Desktop Editor supports build/reload
Mobile code remains build-time native code

Gameplay Hot Reload:
Safe Barrier
→ State Serialize / Migration
→ Unload Old Module
→ ABI Handshake
→ Load New Module
→ State Restore
No stale function pointer / callback / module allocation escape

Future Language Binding:
Stable ABI keeps Rust / C# / other bindings possible without redesigning Engine Core


Build:
CMake

PCH:
EnginePCH + RendererPCH + EditorPCH

Graphics:
DX12 + Vulkan + Metal

Windows:
DX12 Default / Vulkan Selectable

Shader:
Slang
Metal 需 PoC 驗證

Renderer:
Forward+ + Render Graph

Shading Models:
PBR
StylizedPBR
Anime
Vegetation
Water
Unlit

Stylized Scene:
StylizedPBR + Lightmap/Probe + Reflection Probe + Fog/Atmosphere + Color Grading

Anime Character:
Multi-Ramp + Face SDF + Hair-specific Shading + Stylized Specular + Rim + Per-material Geometry Outline

Vegetation Shader:
Alpha Cutout + Wind + Transmission/Back Lighting + Probe/SH + GPU Instancing

Water Shader:
Depth Fade + Shallow/Deep Color + Fresnel + Reflection Probe + optional SSR/Refraction/Foam

Scene / Character Mix:
Environment → StylizedPBR
Character → Anime
Shared Forward+ / Shadow / Fog / Reflection / RenderGraph

VFX / Particle:
One VFXComponent → One VFXInstance → N Logical Emitters → M Simulation Batches → K Render Batches
Stateless + CPU SoA + GPU Simulation
VFX Compiler performs DAG validation / dead-module elimination / compatible emitter fusion
Gameplay projectile and visual VFX remain separated
VFX Budget Manager integrates with PerformancePolicyManager


Animation:
Skeleton / Clip / Animation Graph / State Machine / Blend Tree / Layer / Pose Cache
CPU Animation Logic + GPU Skinning
Global Skinning Buffer with skinningMatrixOffset
Skinned Mesh Instancing + Animation Sharing + Skeleton LOD
Automatic GPUAnimationClip / Bone Animation Texture Cook
GPUAnimationPoseStorage abstraction instead of hardcoded Texture2D
Crowd path supports per-instance time / frame interpolation / simple crossfade / indirect draw


Spatial:
BVH + Dynamic Grid + Terrain Quadtree + Vegetation Cluster
World Partition uses Stable Grid Cells + Loose Quadtree spatial index
Indoor uses Room / Portal Graph; Outdoor ↔ Indoor via Streaming Gateway

Terrain:
Chunk / Quadtree / LOD / Streaming

Vegetation:
SpeedTree-like Runtime / Wind / LOD / Billboard / GPU Instancing

Mesh LOD:
Screen-space LOD + Hysteresis + Dither Crossfade

Texture:
Mipmap LOD + Budget-aware Texture Streaming

Binding:
Hybrid Bindless-first
Binding Tier + Fallback

Scene / World:
World = Runtime Simulation Universe
Scene = Content Ownership / Serialization Unit
EditorWorld ≠ PlayWorld
Multiple / Additive Scene + Persistent Scene
Async Load → LoadedInactive → Atomic Activation → Safe Unload
WorldCommandBuffer + Structural Barrier
Cross-scene reference uses persistent logical identity, not raw EntityID
Scene ≠ StreamingCell ≠ GameplayZone ≠ PhysicsWorld / NavigationWorld / RenderWorld

World Streaming:
StreamingCell = Runtime Residency Unit
Stable Fixed Grid Cells + Loose Quadtree Spatial Index
Room / Portal Graph for Indoor
Multiple Streaming Sources → StreamingDemand → Priority / Budget → Residency
Cell / Bundle identity remains separate
Large-world coordinate foundation + camera-relative rendering

HLOD:
Editor / Cooker offline generation
Loose Quadtree hierarchy foundation
HLOD Node ≠ Streaming Cell
Runtime only selects / streams HLOD representation
Full Simulation Residency ≠ Far Visual Residency
Deterministic + Incremental HLOD build

Runtime Components:
Component Pool / SoA

Render World:
Derived Frame Data

Reflection:
V1 Runtime Metadata + Macro
Future Codegen

Editor:
Dear ImGui

Runtime UI:
Custom Retained Mode Framework
UIDocument + UIElementID Tree; UIElement ≠ SceneNode / EntityID
Scene UIComponent mounts UIDocument; Editor Hierarchy mounts UI tree through adapter
ScreenSpace + WorldAnchored + WorldSpace share Widget / Layout / Style / Event / Binding
JSON UI + ViewModel/Data Binding + VirtualizedListView + UI Batch / RenderGraph
Logical UI Resolution independent from 3D Dynamic Resolution

Asset:
UUID + Asset Database + Importer + Bundle

FBX:
Editor Only

Physics:
Jolt CPU-authoritative core
Character Framework: CharacterIntent → CharacterMotor → CharacterController → CharacterMotionResult
CharacterController = Native Core System; CharacterMotor = Replaceable Gameplay Policy
Ground / Slope / Step / Snap / Moving Platform / Root Motion / Crouch / Teleport / Dynamic Body Interaction
Hybrid Physics Execution Domain: CPUAuthoritative / CPUBatched / GPUVisual / GPUDeferred
GPU acceleration targets Cloth / VFX Collision / Debris / Deferred Query
Gameplay-critical Immediate Query remains CPU / Jolt
GPU Broadphase / GPU RigidBody World remain profile-driven Future R&D

Audio:
miniaudio built-in default backend
AudioResourceRegistry + AudioResidencyScope
Global / Scene / Zone / Character / Encounter / UI residency ownership
Scope Release + Voice/Streaming Pin + FadeOutAndRelease
Bundle Generation Pinning + Audio Memory Budget integration

Fonts:
FreeType + HarfBuzz

Localization:
V1

Save:
V1

Networking:
V1 不內建高階 Framework

Testing:
V0.x 起導入

CI:
V0.x 起導入

Crash:
V1 Crash Infrastructure

Memory:
V0.x Memory Budget

Target:
Windows / macOS / Android / iOS

畫質目標：
至少 Unity URP 級別
並正式支援 StylizedPBR 場景 + Anime Character NPR 的混合動畫風渲染路線

效能目標：
針對固定遊戲類型，在相同畫質下盡量做到比通用引擎更低 CPU / RAM / Render Overhead。

END

Shader Variant:
Feature Bitmask + Used-Variant Compile + Incremental Cache + CI Variant Budget

Golden Image:
Perceptual Diff / SSIM + Pixel Error Guardrail

Terrain Streaming:
UUID / Asset Database + Spatial Streaming Bundle

Terrain Physics:
Terrain Heightmap Source of Truth -> Jolt HeightFieldShape Derived Data

GPU Skinning:
Resource Registry -> GPU Buffer ResourceIndex


Coding Style:
Modern C++ / RAII / Handle / Data-Oriented Hot Path

Smart Pointer:
SharedPtr<T>
SharedPtr 僅限真正 Shared Lifetime

Ownership:
Engine Resource 優先 Handle；Entity 使用 EntityID；Raw Pointer / Reference 僅代表 Non-owning Borrow


JSON:
yyjson private backend + Engine JsonDocument / JsonValue / JsonWriter abstraction
Parse + Generate + Pretty / Compact / Deterministic Write
UTF-8 / Strict JSON

Data Table:
JSON Runtime Asset + Schema Validation + Typed Runtime Table
Primary Key: UInt32 / UInt64 / String
Preprocess: Sort View / Index / Group / Resolve / Derived / Weighted / Range / Custom Processor
Runtime Container: Contiguous Rows + Indices + Views + StringPool / ArrayPool + DerivedData
Immutable + DataTableRegistry + Generation Hot Reload

Data Format:
Human-readable metadata/config/DataTable → JSON
Runtime-heavy / streaming / GPU-ready assets → Binary
Optional Binary DataTable Cook only if profiling later proves necessary


Bundle Hot Update:
Remote Manifest + Download Cache + Hash Verification + Atomic Commit + Rollback

Update Strategy:
V1 Full Bundle Update
Future Delta Patch


Plugin System:
Editor Plugin + Runtime Build-time Module

Plugin Distribution:
Local Installed / Build-time only
No Remote Native Plugin Download

Plugin / Bundle Boundary:
Plugin → Code Extension
Bundle → Asset / Data Hot Update


WebView:
Cross-platform In-Game Runtime WebView
WebViewElement = UIElement Native Overlay Proxy; Native WebView ≠ UIRenderItem
Windows WebView2 + Android WebView + iOS/macOS WKWebView
Logical UI Rect + Safe Area + single-owner gesture routing
Local HTML Asset / Remote HTTPS + JSON Message Bridge
Origin / Trust Policy + Cache / Cookie / Storage Profile

V1 Rendering:
Native Overlay

Future:
Offscreen / Render-to-Texture WebView for true WorldSpace / Surface Web UI

WebView / Auth Boundary:
WebView → General In-Game Web Content
Auth Provider → Platform-compliant Login Flow


Audio Backend:
miniaudio → Built-in Default
FMOD → Optional Plugin / Professional Audio Middleware

Audio Residency:
AudioResourceRegistry + AudioResidencyScope
Scope controls runtime residency/lifetime; Bundle controls physical packaging/versioning
Active Voice / Streaming / Generation Pin prevents premature eviction

FMOD Distribution:
Native Plugin → Local Installed / Build-time
FMOD Bank → Asset Bundle / Remote Content Hot Update


Feature Stripping:
Auto / Enabled / Disabled
Compile-time Module + Build-time Dependency + Shader Variant Stripping

Optional Runtime:
Only Enabled / Required / Platform-compatible modules are packaged

Core:
Foundation modules remain mandatory


API Boundary:
Game / General Plugin → Public API only
Internal / Backend implementation hidden

Subsystem Packaging:
Foundation Core mandatory
High-level Subsystem optional and strippable

Unused Subsystem:
No Compile + No Link + No Package + No Asset Cook + No Shader Variants


Build Linking Mode:
Development / Editor → Modular
Shipping / Release → Modular or Monolithic

Core:
Prebuilt Libraries

Optional Subsystem:
Dynamic Module where appropriate during development
Static / Monolithic integration available for shipping


WebView V1 Semantics:
WebViewElement participates in UIDocument/UIElement hierarchy and RectTransform layout
Native WebView is not UIRenderItem / UI Batch / RenderGraph UI Pass
No Stencil Mask / 3D Depth / arbitrary Canvas interleaving for Native Overlay

Stripping CI Matrix:
PR representative profile
Nightly Minimal + Max/Full Feature profiles
Release platform/shipping matrix


Shader Source of Truth:
Slang only
Metal fallback → Slang → SPIR-V → SPIRV-Cross → MSL
No independent hand-written MSL fallback

Bundle Dependency:
DAG required; cycle = Build Fail
Mapped/active bundle update may defer activation to next Pre-init

Frame Allocator:
Per-thread / per-worker arena
Frame-end reset after job/render lifetime fence

Entity Generation:
32-bit generation with overflow assert/retire policy
High-frequency transient objects use dedicated pools, not Scene EntityID


Roadmap Strategy:
Foundation → Reference RHI → First Vertical Slice → Asset/Bundle → Editor/UI/Plugin → Subsystems → Platform/Mobile → World Streaming → GPU Driven → Shipping Harden

CI Philosophy:
Architecture Contract = Automated Gate

Feature Stripping:
Designed from Phase 0, continuously enforced, fully hardened in Phase 9


Dynamic Skinning:
Packed bone matrices + per-draw matrix offset + frames-in-flight

GPU Upload:
UMA → persistent/shared buffer
Discrete GPU → upload ring + device-local buffer

Binding:
Canonical ResourceIndex separated from buffer element index


Job System:
Work-stealing + priority + dependency graph + frame barriers

Asset Residency:
Explicit state machine + separate RAM / VRAM budgets + pressure policy

Input:
Raw / Device-level API + optional user-defined Action Mapping
Keyboard / Mouse / Gamepad / Touch may be active simultaneously
Multi-touch + per-pointer capture + Virtual Controls
InputContext + InputLayer → UILayer Interaction Matrix
UI / WorldSpace UI / WebView / Gameplay single-owner routing
Fixed Tick Snapshot + Zig batch C ABI + replay foundation

Time Model:
Fixed simulation tick + variable render interpolation

Font Rendering:
CJK dynamic glyph atlas + optional MSDF for Latin/Icon

Crash Infrastructure:
Backend abstraction + dump capture + symbolication pipeline


PSO:
Compatible per-platform/device cache + async warmup + runtime creation telemetry

Android GPU Compatibility:
GPUWorkaroundDatabase + capability override + fallback tier

Memory Pressure:
OnMemoryPressure + resource eviction tiers + device loss recovery

Reflection Metadata:
Canonical schema shared by Macro/constexpr V1 and Clang AST V2


Modern Rendering Contract:
Public RHI = BeginRendering/EndRendering; backend-only compatibility fallback

Bundle Generation:
Versioned storage + load-context generation pinning

CI Performance:
Dedicated runner hard gate; shared runner trend/warning


Frame Lifetime Safety:
FramePtr / FrameSpan + generation/region guard + AST persistent-storage prohibition

WebView Coordinate:
OS logical screen space only; independent from render scale

Bundle Deletion:
Versioned generations + pending-delete queue + reference drain

Module Allocator:
Handle/POD/Span boundary + allocate/destroy in owning module or explicit Engine allocator contract


Component Storage:
Sparse Set / Sparse Array + Dense SoA as default Scene Component Pool

Pipeline Metadata:
Offline PipelineLayoutMetadata; runtime direct indexed binding

Worker Arena Growth:
Thread-local page/chunk growth + reuse + budget + trim

Performance Policy:
Thermal + frame pacing + dynamic resolution + target FPS

AI Change Scope:
Goal/risk/subsystem based governance; diff size is a soft signal


Frame Module Boundary:
FrameSpan / FrameDataHandle only for transient cross-module data; no retention / async capture / lifetime erasure


Dependency Graph Gate:
Unified module-layer dependency graph; forbidden dependency edge = hard fail

Third-party Jobs:
JobSystemTaskAdapter + external worker budget / oversubscription profiling

Shader C++ Layout:
Canonical reflection generates deterministic C++ parameter headers

Streaming Scheduler:
Priority preemption + platform IO/decode/upload concurrency budgets

Plugin Handshake:
Runtime C-ABI Plugin_Init version/build/hash/capability validation


Terrain Precision:
R16_UNORM / FP16 profile + stitching/skirt/LOD morph seam prevention

Vegetation Interaction:
GPU interaction field for local bend without per-instance CPU transforms

Gesture Ownership:
Native/Engine input has single owner; optional explicit forwarding only

Profiler Export:
JSON / Chrome Trace compatible offline capture with correlation IDs

CI Build Infrastructure:
compiler/artifact caches + parallel matrix + latency/cache-hit telemetry


PSO Residency:
Shared Residency framework + runtime budget/watermarks + LRU candidate selection + fence-safe destroy + async recreation; CI Variant Budget remains a separate build-time gate


Variant Accounting:
Per-shader/platform profile tracks Theoretical / After-Pruning / Project Used / Cooked / Budget

PSO Fence Timeline:
single global timeline uses one LastUsedFenceValue; independent queues require all relevant queue fences complete


---

# 附錄 A — V1 Plugin / Third-party SDK / Video & Media 詳細 Contract（Normative）

# 跨平台 3D Engine — V1 Plugin / Feature Module 模組化規劃

**文件版本：Draft v1.2**  
**對應 Engine 世代：V1.x**  
**目標：V1 即建立可長期延伸到 V2 / V3 的 Plugin / Feature Module 架構。**

---

# 一、V1 模組化目標

V1 必須從一開始就遵守：

```text
Feature exists in Engine Repository
≠
Feature exists in every Game Build
```

正式 Shipping 目標：

```text
Disabled Plugin
=
No Code
+
No Runtime Registration
+
No Asset Cooker
+
No Shader Variant
+
No Plugin Asset
+
No Runtime Dependency
```

不是：

```text
bool enabled = false;
```

---

# 二、Plugin 類型

```text
Plugin / Feature Module
├─ Core Module
├─ Feature Module
├─ Backend Plugin
├─ Editor / Cooker Plugin
└─ Provider Plugin
```

### Core Module
永遠存在：

```text
EngineCore
Math
Memory
JobSystem
TaskGraph Core
Reflection Core
Serialization Core
VFS Core
Asset Registry
Resource Registry
World Core
Scene Core
Entity / Component Core
Transform Core
Event Core
Time Core
Logging Core
Platform Core
Plugin Manager
RHI Core
RenderGraph Core
Shader Metadata Core
```

### Feature Module
遊戲可完全不使用：

```text
Physics
Character
Navigation
AI
Audio
Video / Media
Runtime UI
WebView
Terrain
Vegetation
VFX
Localization
Save
DataTable
Timeline foundation if introduced later
```

### Backend Plugin
Framework 存在，但實作可替換：

```text
Physics.Jolt
Audio.Miniaudio
Audio.FMOD
RHI.DX12
RHI.Vulkan
RHI.Metal
```

### Editor / Cooker Plugin
Shipping 不包含：

```text
Importer.FBX
Importer.glTF
Importer.Texture
Importer.Audio
Importer.Font
Importer.Spine
Editor.Scene
Editor.Prefab
Editor.Animation
Editor.Physics
Editor.Audio
Editor.UI
Editor.DataTable
Editor.WorldPartition
Editor.Profiler
```

### Provider Plugin
第三方服務：

```text
Crash Reporting Provider
Telemetry Provider
Cloud Save Provider
Platform SDK Provider
```

---

# 三、Plugin Manifest

每個 Plugin 都有：

```text
PluginManifest
├─ PluginID
├─ Version
├─ EngineVersionRange
├─ Type
├─ Runtime / Editor / Server / Client
├─ Dependencies
├─ OptionalDependencies
├─ Platforms
├─ FeatureFlags
├─ ABIVersion
└─ HotReloadPolicy
```

禁止 dependency cycle：

```text
Plugin A → B → C → A
```

CI hard fail。

---

# 四、Shipping Build 原則

```text
Project Feature Selection
↓
Dependency Resolve
↓
Compile Selected Modules
↓
Link Selected Modules
↓
Cook Selected Assets
↓
Cook Selected Shaders
↓
Package Selected Runtime Data
```

Development / Editor：

```text
Dynamic Module
→ Hot Reload / Faster iteration
```

Shipping：

```text
Selected Plugins only
→ Static / Monolithic Link allowed
→ LTO / WPO
→ Strip unused
```

Plugin 化不代表最終一定有很多 DLL。

---

# 五、Physics Plugin

```text
Physics.Core
Physics.Jolt
```

`Physics.Core` 定義：

```text
PhysicsWorld
RigidBody
Collider
PhysicsQuery
PhysicsMaterial
CharacterController Interface
```

V1 預設：

```text
Physics.Jolt
```

若遊戲完全不需要 Physics：

```text
Physics.Core
Physics.Jolt
```

都可 strip。

---

# 六、Character Framework Plugin

```text
Character.Core
Character.DefaultMotor
```

適合：

```text
Action RPG
MOBA
FPS
TPS
Platformer
```

不需要：

```text
Card Game
Pure Strategy
Server Simulation without characters
```

即可不加入。

---

# 七、Navigation Plugin

```text
Navigation.Core
Navigation.RecastDetour
```

V1 default：

```text
Recast / Detour
```

如果遊戲沒有 AI pathfinding：

```text
Navigation.*
→ strip
```

---

# 八、AI Plugin

拆成：

```text
AI.Core
AI.Blackboard
AI.BehaviorTree
AI.Perception
```

V1 不強迫整包加入。

例如：

```text
AI.Core + Blackboard
```

可以存在而不使用 Behavior Tree。

---

# 九、Audio Plugin

```text
Audio.Core
Audio.Miniaudio
Audio.FMOD
```

V1 Default：

```text
Audio.Miniaudio
```

FMOD：

```text
Optional Backend Plugin
```

Dedicated / Headless：

```text
Audio.*
→ strip
```

---

# 十、Runtime UI Plugin

```text
UI.Runtime
```

完全獨立於：

```text
Editor UI
→ Dear ImGui
```

因此：

```text
Dedicated Server
Headless Tool
Training Build
```

都可以完全移除 Runtime UI。

---

# 十一、WebView Plugin

```text
UI.WebView
UI.WebView.Windows
UI.WebView.Android
UI.WebView.iOS
UI.WebView.macOS
```

強烈 Optional。

沒有 WebView 的遊戲：

```text
Native WebView bridge
JS bridge
WebView asset support
Platform web code
```

全部不進 Build。

---

# 十二、Spine Plugin

```text
Animation.Spine
```

完全 Optional。

只做 3D Skeleton Animation 的專案：

```text
Spine Runtime
→ strip
```

---

# 十三、Terrain Plugin

```text
Terrain.Core
Terrain.Renderer
Terrain.Editor
```

可選。

不要求：

```text
所有 3D 遊戲都帶 Terrain
```

---

# 十四、Vegetation Plugin

```text
Vegetation.Core
Vegetation.Render
Vegetation.Editor
```

不綁死 Terrain。

可放在：

```text
Mesh World
Procedural World
Terrain World
```

---

# 十五、VFX Plugin

```text
VFX.Core
VFX.GPU
VFX.Editor
```

極簡專案可完全不加入。

VFX transient particles 仍不使用 Scene EntityID。

---

# 十六、Localization Plugin

```text
Localization.Core
Localization.ICU
Localization.Editor
```

單語系遊戲可不加入 ICU。

但：

```text
UTF-8 Text Core
```

不是 Localization Plugin，仍屬基礎文字能力。

---

# 十七、Save Plugin

```text
Save.Core
Save.Local
Save.Cloud.Provider.*
```

V1 預設：

```text
Save.Local
```

若專案不需要 local save：

```text
Save.*
→ strip
```

---

# 十八、DataTable Plugin

```text
DataTable.Core
DataTable.Editor
DataTable.CSVImporter
DataTable.XLSXImporter
```

Runtime 只包含：

```text
DataTable.Core
```

CSV / XLSX importer：

```text
Editor-only
```

---

# 十九、Asset Importer Plugins

全部 Editor-only：

```text
Importer.FBX
Importer.glTF
Importer.Texture
Importer.Audio
Importer.Font
Importer.Spine
```

Shipping Runtime：

```text
0 importer code
```

只讀 Cooked Asset。

---

# 二十、Material / Render Feature Plugins

Render Feature 可模組化：

```text
RenderFeature.PBR
RenderFeature.Stylized
RenderFeature.Anime
RenderFeature.Water
RenderFeature.Vegetation
RenderFeature.PostProcess
```

Cooker 只根據：

```text
Enabled Features
+
Actually Used Materials
+
Platform Capability
```

產 Shader / PSO。

---

# 二十一、Post Processing Plugins

```text
PostProcess.Core
PostProcess.Bloom
PostProcess.SSAO
PostProcess.DOF
PostProcess.MotionBlur
PostProcess.ColorGrading
```

不需要的效果：

```text
不編譯
不 Cook Shader
不建立 PSO
```

---

# 二十二、Editor Plugin

V1 Editor 最少拆：

```text
Editor.Scene
Editor.Prefab
Editor.UI
Editor.Animation
Editor.Physics
Editor.Audio
Editor.DataTable
Editor.WorldPartition
Editor.Profiler
```

Editor extension 只能透過正式 Extension API：

```text
Window
Inspector
Property Drawer
Asset Editor
Importer
Menu
Toolbar
Gizmo
Validator
Build Step
Profiler Panel
```

---

# 二十三、Plugin 與 Scene / Prefab

Plugin Component Serialize：

```text
TypeID
PluginID
Version
Payload
```

Plugin 缺失：

```text
Editor
→ MissingComponentProxy
→ Preserve payload
```

Shipping Cook：

```text
Referenced runtime plugin missing
→ hard fail
```

---

# 二十四、Plugin 與 Zig

Zig Gameplay 不直接拿 C++ Plugin pointer。

```text
Plugin Feature
↓
Versioned Stable C Function Table
↓
Zig Binding
```

Plugin reload：

```text
Stop new calls
↓
Drain Jobs
↓
Drain Callbacks
↓
Invalidate Handles
↓
Unload
```

---

# 二十五、Hot Reload Policy

```text
HotReloadPolicy
├─ Never
├─ EditorOnly
├─ DevelopmentOnly
└─ SafeRuntime
```

例如：

```text
RHI Backend
→ Never / restart preferred

FBX Importer
→ EditorOnly

Gameplay Zig
→ SafeRuntime
```

---

# 二十六、V1 Build Profile

```text
Editor
DesktopClient
MobileClient
DedicatedServerFoundation
Tool
```

不同 Profile 有不同 Plugin Set。

---

# 二十七、V1 常見組合

### 一般 RPG

```text
Physics.Jolt
Character
Navigation.RecastDetour
AI.BehaviorTree
AI.Perception
Audio.Miniaudio
Media.Video
UI.Runtime
Localization
Save.Local
DataTable
Terrain
Vegetation
VFX
```

### 跑酷

```text
Physics.Jolt
Character
Audio.Miniaudio
Media.Video
UI.Runtime
Localization
DataTable
VFX
```

可 strip：

```text
Navigation
AI
Terrain
Vegetation
WebView
Spine
```

### Headless Tool

```text
World
Scene
Asset
Data
```

Presentation 模組全部移除。

---

# 二十八、V1 Definition of Done

V1 Plugin Framework 完成標準：

```text
1. Project 可顯式 Enable / Disable Plugin。
2. Dependency graph 可驗證且無循環。
3. Disabled Runtime Plugin 不進 Shipping link。
4. Disabled Render Feature 不 Cook 對應 shader variants。
5. Editor-only Plugin 不進 Shipping。
6. Importer code 不進 Runtime。
7. Scene / Prefab 對 Missing Plugin 可保留資料。
8. Zig Plugin API 維持 Stable C ABI。
9. Dedicated / Headless Profile 可 strip UI / Audio / Renderer-dependent modules。
10. Development dynamic / Shipping monolithic 兩種模式都可 build。
11. Media / Video 可作 Optional Feature，Headless / Server 完全 strip。
```

---


# 三十、研發者自訂 Plugin / Bridge SDK

V1 就必須提供正式的第三方 Plugin 開發能力。

核心原則：

```text
Third-party Plugin
→ Bridge Layer / Public SDK
→ Engine Capability

Third-party Plugin
✕ Direct Engine Private Access
```

整體架構：

```text
Game / Third-party Plugin
        │
        ▼
┌─────────────────────────────┐
│      Engine Plugin SDK      │
│      / Bridge Layer         │
├─────────────────────────────┤
│ Stable C ABI                │
│ C++ Convenience SDK         │
│ PluginHost                  │
│ Service Registry            │
│ Extension Registry          │
│ Handle / POD API            │
│ Event / Command API         │
│ Asset / Type Registry       │
└──────────────┬──────────────┘
               │
               ▼
         Engine Public API
               │
               ▼
         Engine Internals
```

V1 支援兩種主要 Native Plugin 入口：

```text
1. Stable C ABI Plugin
2. C++ Convenience Plugin SDK
```

C++ SDK 建立在 Stable C ABI / Public Service Contract 之上，不讓第三方依賴 Engine private ABI。

---

# 三十一、Stable C Plugin ABI

Plugin export：

```c
typedef struct EnginePluginHostAPI EnginePluginHostAPI;
typedef struct EnginePluginExports EnginePluginExports;

ENGINE_PLUGIN_EXPORT
bool EnginePlugin_Load(
    const EnginePluginHostAPI* host,
    EnginePluginExports* out_plugin);

ENGINE_PLUGIN_EXPORT
void EnginePlugin_Unload(void);
```

`EnginePluginHostAPI` 只包含：

```text
Versioned Function Tables
Opaque Handles
POD Structs
Stable IDs
Allocator API
Logging API
Service Query API
Extension Registration API
```

禁止跨 ABI 傳：

```text
std::string
std::vector
std::function
RTTI object
C++ exception
Engine private class
Backend native pointer
```

---

# 三十二、C++ Plugin SDK

為提高易用性，可提供：

```cpp
class IEnginePlugin
{
public:
    virtual bool OnLoad(const PluginHost& host) = 0;
    virtual void OnUnload() = 0;
};
```

但這層只屬 Public Plugin SDK。

正式：

```text
C++ Convenience SDK
→ May change only within declared compatibility policy

Stable C ABI
→ Canonical low-level compatibility boundary
```

第三方若追求最穩定跨編譯器 / 跨語言相容性，應優先使用 Stable C ABI。

---

# 三十三、PluginHost

Plugin 不直接找 Engine singleton。

正式使用：

```text
PluginHost
```

能力：

```text
GetService()
RegisterService()
RegisterExtension()
RegisterAssetType()
RegisterImporter()
RegisterCooker()
RegisterEditorExtension()
RegisterEventSink()
RegisterCommand()
GetAllocator()
GetLogger()
```

禁止：

```text
Engine::GetSingleton()->PrivateSubsystem->...
```

---

# 三十四、Service Registry

Engine 核心與 Plugin 透過：

```text
ServiceID
+
Versioned Service API
```

互動。

例如：

```text
SERVICE_RENDER
SERVICE_PHYSICS
SERVICE_NAVIGATION
SERVICE_AUDIO
SERVICE_INPUT
SERVICE_UI
SERVICE_ASSET
```

Plugin：

```text
GetService(ServiceID)
```

若 capability 不存在：

```text
Unsupported / nullptr
```

不強迫整個引擎把該功能拉進 build。

---

# 三十五、Plugin 可提供 Service

第三方不只消費 Engine Service，也可以提供新的 backend / provider。

例如：

```text
Physics.Core
↓
Physics.CustomVendor Plugin
↓
Vendor Physics SDK
```

或：

```text
Navigation.Core
↓
MyNavigationBackend
```

或：

```text
Audio.Core
↓
CustomAudioBackend
```

因此：

```text
Framework
→ Stable Service Contract

Backend
→ Plugin
```

---

# 三十六、Bridge Plugin

V1 正式定義：

```text
Bridge Plugin
```

用途：

```text
Engine Framework
↓
Bridge Plugin
↓
Third-party SDK
```

Bridge 負責：

```text
Type Conversion
Handle Translation
Lifetime Translation
Thread Boundary
Callback Translation
Error Translation
Version Adaptation
Memory Ownership
```

例如：

```text
Audio.Core
↓
Audio.FMOD
↓
FMOD SDK
```

Gameplay 永遠只看到：

```text
AudioEventID
AudioHandle
```

而不是 `FMOD::Studio::*`。

---

# 三十七、Custom Asset Plugin

第三方可以註冊自己的 Asset Type：

```text
.quest
.dialogue
.voxel
.worldgen
.mydata
```

Plugin 可提供：

```text
Asset Type
Importer
Cooker
Runtime Loader
Inspector
Asset Editor
Validator
```

流程：

```text
*.quest
↓
Quest Importer
↓
Quest Authoring Asset
↓
Cook
↓
Quest Runtime Asset
```

Engine Core 不需要預先知道所有遊戲資料型態。

---

# 三十八、Editor Extension SDK

第三方可加入：

```text
Window
Inspector
Property Drawer
Asset Editor
Menu
Toolbar
Gizmo
Validator
Build Processor
Profiler Panel
Scene Overlay
```

所有 Editor mutation 必須透過：

```text
Editor Command
Transaction
Document API
```

確保：

```text
Undoable
Auditable
Validatable
```

---

# 三十九、Plugin Manifest 擴充

每個研發者 Plugin：

```text
plugin.json
```

至少：

```json
{
  "id": "com.company.quest",
  "version": "1.2.0",
  "engine": ">=1.0 <2.0",
  "type": "feature",
  "runtime": true,
  "editor": true,
  "dependencies": [
    "engine.asset",
    "engine.ui"
  ]
}
```

再可宣告：

```text
Capabilities
Platforms
Client / Server
EditorOnly
Required Services
Optional Services
HotReloadPolicy
ABIVersion
ThirdPartyLibraries
```

---

# 四十、Capability Negotiation

Plugin 不假設所有平台都有相同能力。

可要求：

```text
ComputeShader
RayTracing
MeshShader
WebView
Network
Touch
GPUStorageBuffer
```

Load / Build 時：

```text
Plugin Requirements
↓
Target Capabilities
↓
Compatible?
```

不相容時：

```text
Disable
or
Build Fail
```

由 Project Policy 決定。

---

# 四十一、ABI / Version Handshake

Plugin Load 前：

```text
Engine
↓
Plugin Handshake
├─ ABI Version
├─ Engine Version
├─ Build Configuration
├─ Platform
├─ Architecture
└─ Capabilities
```

Mismatch：

```text
Reject Load
```

不得等到 undefined behavior / crash 才發現。

---

# 四十二、Memory Ownership Bridge

跨 module 不允許：

```text
Plugin malloc
↓
Engine free
```

也不允許：

```text
Engine new
↓
Plugin delete
```

正式提供：

```text
EngineAllocatorAPI
```

或：

```text
Caller Owns Memory
Callee Copies
```

每個 API 明確標示 ownership。

---

# 四十三、Callback / Lifetime Safety

Plugin callback 不保存裸 C++ object pointer。

使用：

```text
CallbackID
UserToken
Generation
```

Unload：

```text
Stop New Calls
↓
Cancel / Drain Callbacks
↓
Wait Plugin Jobs
↓
Unregister Services
↓
Invalidate Handles
↓
Unload Binary
```

與 Zig Gameplay Hot Reload 使用同樣的 lifetime discipline。

---

# 四十四、Plugin API 等級

建議分：

```text
Stable Plugin SDK
Internal Plugin API
```

Stable Plugin SDK 給：

```text
Game Team
Third-party Developer
Middleware Vendor
```

Internal API 給：

```text
Engine Team
RHI Backend
Low-level Renderer
Memory / Job internals
```

第三方不得依賴 Internal API。

---

# 四十五、Game Plugin

遊戲自己的大型子系統也可以 Plugin 化：

```text
Game.Combat
Game.Quest
Game.Dialogue
Game.Crafting
Game.WorldEvent
```

一個 Game Plugin 可以同時提供：

```text
Runtime
Editor
Asset Type
Zig API
Validation
Cook Step
```

避免所有遊戲邏輯塞進單一巨大模組。

---

# 四十六、V1 Third-party Plugin Gate

V1 Plugin SDK 完成標準額外加入：

```text
11. 第三方可不修改 Engine Source 寫 Native Plugin。
12. Stable C ABI 可被 C / C++ / Zig / Rust 類 native language 使用。
13. Plugin 可查詢 Engine Service。
14. Plugin 可註冊自己的 Service。
15. Plugin 可註冊 Asset Type / Importer / Cooker / Editor Tool。
16. Plugin 缺失時 Scene / Prefab 能保留 serialized payload。
17. ABI mismatch 在 Load 前被拒絕。
18. Plugin unload 後無 dangling callback / job / handle。
19. Plugin 可 Development Dynamic、Shipping Static/Monolithic。
20. 第三方 Plugin 不需要 include Engine private headers。
```



# 四十七、V1 Video / Media Framework

V1 正式加入 Video Playback Framework。

定位：

```text
Video
→ Optional Runtime Feature Module

Headless / Dedicated Server
→ Can be fully stripped
```

Video 不直接綁死 Runtime UI，也不直接綁死 Scene。

核心架構：

```text
VideoSource
↓
Media Demuxer
↓
Video Decoder ──────────────┐
↓                           │
Decoded Video Frames        │
↓                           │
Video Frame Queue           │
↓                           │
GPU Import / Upload         │
↓                           │
VideoTexture                │
↓                           │
Renderer / UI / Material    │
                            │
Audio Decoder ──────────────┘
↓
PCM Queue
↓
Audio.Core
↓
Audio Bus / Mixer / Device
```

正式：

```text
Video Decode
≠ Renderer

Video Audio
≠ Separate Audio Device

Video UI
≠ Special Native Overlay by default
```

Video audio 必須進入既有 `Audio.Core`，才能使用：

```text
Bus
Volume
Fade
Mute
Snapshot
Duck
Output Device
```

---

# 四十八、Video Plugin 模組

建議拆成：

```text
Media.Core
Media.Video

Media.Backend.WindowsMF
Media.Backend.AndroidMediaCodec
Media.Backend.AppleAVFoundation

Media.Backend.FFmpeg        ← Optional fallback / tooling-dependent

UI.VideoElement             ← Optional
Editor.VideoImporter
Editor.VideoTranscoder      ← Optional
Editor.VideoPreview
```

正式：

```text
Media.Video
→ Framework / Player / State / Clock / Queue

Backend Plugin
→ Demux / Decode / Native Surface Integration
```

專案沒有影片：

```text
Media.*
UI.VideoElement
Editor.Video*
```

皆可不加入 Shipping。

---

# 四十九、V1 Backend Policy

V1 優先使用平台原生硬體解碼 backend。

```text
Windows
→ Media Foundation backend

Android
→ MediaCodec backend

iOS / macOS
→ AVFoundation / VideoToolbox based backend
```

另可提供：

```text
Media.Backend.FFmpeg
```

作為：

```text
Desktop fallback
Unsupported codec fallback
Editor probing / transcoding support
```

但 FFmpeg 不成為 Engine Core dependency。

Project 可依授權、平台與實際 codec 需求決定是否包含。

---

# 五十、V1 Portable Video Profile

V1 跨平台「最低可攜 profile」定義為：

```text
Container
→ MP4

Video
→ H.264 / AVC

Audio
→ AAC
```

其他 codec：

```text
H.265 / HEVC
VP9
AV1
Opus
```

採：

```text
Capability-driven / Backend-specific
```

Engine 不承諾所有平台都一定支援相同 codec。

Runtime 可查：

```text
VideoCodecCapabilities
```

包含：

```text
DecodeSupported
HardwareDecode
MaxResolution
MaxFPS
HDRSupport
AlphaSupport
SeekSupport
```

---

# 五十一、Video Source

```text
VideoSource
├─ VideoAsset
├─ VFS File
├─ Local File
└─ URL
```

V1 正式支援：

```text
Cooked Local Video Asset
VFS / Bundle-resolved file
Local file where platform policy permits
HTTP / HTTPS progressive source if backend supports
```

V1 不把以下列為跨平台必須能力：

```text
HLS
MPEG-DASH
DRM
Live Streaming
WebRTC Video
```

這些保留 V2+ / Provider Plugin。

---

# 五十二、Video Asset

Authoring：

```text
Source Video
↓
VideoImporter
↓
VideoAsset Metadata
```

Metadata 至少：

```text
Duration
Width
Height
FrameRate
Container
VideoCodec
AudioCodec
AudioTrackCount
SubtitleTrackCount
ColorSpace
HDR Metadata if present
Seekability
```

Runtime：

```text
VideoAssetID
→ resolve source / cooked representation
```

Gameplay 不使用原始 path 當 persistent identity。

---

# 五十三、Video Import / Cook

Video Importer 屬 Editor / Cooker。

流程：

```text
Source Video
↓
Probe
↓
Validate
↓
Optional Transcode
↓
Platform Video Profile
↓
Cooked Video Asset
↓
Bundle / Package
```

可設定：

```text
Keep Source Encoding
Force Portable Profile
Platform Override
Resolution Limit
Bitrate Target
Audio Track Policy
Subtitle Policy
```

Runtime 不包含 transcoder。

---

# 五十四、Video Transcoding

V1 可提供：

```text
Editor.VideoTranscoder
```

但它是 Optional Tool Plugin。

用途：

```text
Convert unsupported source
Generate H.264/AAC MP4 baseline
Generate platform-specific derivative
Generate preview proxy
```

Transcoder implementation 可使用：

```text
External Tool
FFmpeg Tool Plugin
Platform Encoder
```

Engine Runtime 不依賴。

---

# 五十五、VideoPlayer

Runtime public object：

```text
VideoPlayer
```

主要 API：

```text
Open(VideoSource)
Prepare()
Play()
Pause()
Stop()
Seek(Time)
SetLoop(bool)
SetPlaybackRate(float)
SetVolume(float)
SetMuted(bool)
Close()
```

Query：

```text
State
Duration
CurrentTime
BufferedRange
VideoSize
FrameRate
HasAudio
HasSubtitles
```

---

# 五十六、Video State Machine

```text
Closed
↓
Opening
↓
Preparing
↓
Ready
↓
Playing
↔
Paused
↓
Seeking / Buffering
↓
Playing
↓
Ended
```

任何主要狀態都可能進：

```text
Error
```

`Stop()`：

```text
Playback position → start / policy-defined
Decoder may remain prepared
```

`Close()`：

```text
Release decoder / source / frame queue
```

---

# 五十七、Video Events

提供 typed event：

```text
VideoReady
VideoStarted
VideoPaused
VideoBufferingStarted
VideoBufferingEnded
VideoSeekCompleted
VideoFirstFrameReady
VideoEnded
VideoError
```

不提供預設：

```text
OnEveryDecodedFrame Gameplay Callback
```

避免每 frame callback 跨 ABI / 跨 thread。

需要 frame processing 時走正式 VideoFrame consumer / render extension API。

---

# 五十八、A/V Sync

若影片有 audio track：

```text
Audio Clock
→ Master Clock
```

Video：

```text
Decoded Frame PTS
↓
Compare Master Clock
↓
Present
Drop
Hold
```

若沒有 audio：

```text
Monotonic Media Clock
→ Master
```

正式：

```text
Playback Time
≠ Render Frame Count
```

Render FPS 波動不應讓影片播放速度改變。

---

# 五十九、Seek

Seek 流程：

```text
Request Time
↓
Find Keyframe / Backend Seek Point
↓
Flush Decode Queue
↓
Decoder Seek
↓
Decode Forward
↓
First Valid Target Frame
↓
Seek Complete
```

V1 public contract 是：

```text
Time-based seek
```

不宣稱所有 codec / backend 都能做到完全 frame-exact random seek。

Editor Preview 可在 backend 支援時提供更精確 stepping。

---

# 六十、Playback Rate

V1 支援：

```text
0.5x ~ 2.0x
```

作為建議 portable range。

Backend capability 可限制。

Audio：

```text
Rate change
→ Time Stretch if backend / audio pipeline supports

otherwise
→ configurable mute or pitch-shift policy
```

不要讓平台差異靜默造成不可預測結果。

---

# 六十一、VideoFrame Runtime Representation

解碼後不統一強迫轉成 CPU RGBA。

正式：

```text
VideoFrame
├─ Timestamp
├─ Duration
├─ Width / Height
├─ PixelFormat
├─ ColorSpace
├─ NativeSurfaceHandle optional
└─ Plane Views / GPU Import Metadata
```

常見 pixel format：

```text
NV12
P010
YUV420
RGBA fallback
```

---

# 六十二、GPU Decode Surface / VideoTexture

優先：

```text
Native Decoder Surface
↓
GPU Import / External Texture
↓
YUV sampling / conversion shader
↓
VideoTexture
```

避免：

```text
Decode
↓
CPU YUV → RGBA conversion
↓
CPU copy
↓
GPU upload every frame
```

若 backend 不支援 zero-copy：

```text
Staging Upload Fallback
```

但 profiler 必須標示。

---

# 六十三、YUV → RGB / Color Management

Video Render 支援：

```text
BT.601
BT.709
BT.2020 where backend/profile supports
Limited / Full Range
SDR
HDR metadata path where supported
```

流程：

```text
Decoded YUV
↓
Color Metadata
↓
Video Conversion Shader
↓
Linear / Renderer Expected Space
↓
UI / Material / Composition
```

不得把所有 video frame 都假設成 sRGB RGBA。

---

# 六十四、VideoTexture

```text
VideoTexture
```

是 Runtime Media Resource。

它可被：

```text
UI.VideoElement
Material
World-space Screen
Fullscreen Presenter
Custom Render Feature
```

消費。

正式：

```text
VideoTexture
≠ Persistent Asset Texture

VideoAsset
→ persistent source asset

VideoTexture
→ runtime decoded presentation resource
```

---

# 六十五、Render Modes

V1 至少支援：

```text
1. Runtime UI VideoElement
2. Fullscreen Video Presenter
3. Material / Mesh VideoTexture
4. World-space UI Video
```

同一 decoder output 可經 VideoTexture 被不同 presentation layer 使用。

是否允許多 consumer：

```text
Explicit ref / presentation policy
```

不默認複製 decode。

---

# 六十六、Runtime UI Integration

新增：

```text
UI.VideoElement
```

繼承：

```text
UIElement
```

但只負責 presentation / controls binding。

不自行 decode。

```text
UI.VideoElement
↓
VideoPlayerHandle
↓
VideoTexture
```

支援：

```text
Fit
Fill
Stretch
NativeAspect
Letterbox
Crop
Opacity
Clip
UI Transform
```

---

# 六十七、World-space / Scene Integration

Scene 可有：

```text
VideoPlayerComponent
```

但它只是 optional scene wrapper：

```text
Scene Entity
└─ VideoPlayerComponent
   └─ VideoPlayerHandle
```

它不代表 Video 必須依賴 Scene。

World-space monitor 可：

```text
VideoTexture
↓
Material Parameter
↓
Mesh
```

---

# 六十八、Audio Integration

Video audio track：

```text
Decoder
↓
PCM Queue
↓
Audio External Stream Source
↓
Audio Bus
↓
Mixer
```

可指定：

```text
Output Bus
Volume
Mute
Fade
Spatialization = usually off
```

若 world monitor 需要空間音效：

```text
Video Audio
→ optional AudioEmitter routing
```

而不是建立第二套音訊系統。

---

# 六十九、Subtitle Framework

V1 Video 支援 timed subtitle。

Authoring importer 可讀：

```text
WebVTT
SRT
```

Cook 後統一成：

```text
SubtitleTrackAsset
```

Runtime：

```text
Media Clock
↓
Subtitle Cue
↓
Localization / Text Formatting
↓
Runtime UI
```

Subtitle Text 不直接 baked 成 video image。

---

# 七十、Localized Subtitle / Audio Track

Video Asset 可關聯：

```text
Subtitle Tracks by Locale
Optional Audio Tracks by Locale
```

例如：

```text
VideoAsset
├─ zh-TW subtitle
├─ ja-JP subtitle
├─ en-US subtitle
└─ optional localized audio
```

Localization system 決定 preferred track。

若 track 不存在：

```text
Fallback Chain
```

---

# 七十一、Video Controls

`UI.VideoElement` 不強制自帶 controls。

Engine 可提供 optional：

```text
VideoControlsWidget
```

包含：

```text
Play / Pause
Seek Bar
Current Time
Duration
Mute
Volume
Subtitle Toggle
Fullscreen
```

遊戲也可完全自己做 UI。

---

# 七十二、Thread Model

```text
Main / Gameplay Thread
→ Commands only

Media IO Worker
→ Read / Demux

Decode Worker / Native Decoder
→ Decode

Audio Thread
→ Consume PCM only

Render Thread / RenderGraph
→ Present VideoFrame / VideoTexture
```

禁止：

```text
Main Thread synchronous decode
Audio Thread file IO
Render Thread blocking network read
```

---

# 七十三、Frame Queue

V1 使用 bounded frame queue。

例如：

```text
2 ~ 6 decoded frames
```

依：

```text
Resolution
Codec
Latency Mode
Memory Budget
```

調整。

正式：

```text
Bounded Queue
```

避免影片 preload 無限制吃 RAM / VRAM。

---

# 七十四、Video Decode Budget

新增：

```text
VideoManager
```

負責：

```text
Active Decoder Count
Priority
Frame Queue Budget
CPU Decode Budget
GPU Decode Capability
Surface Memory
Background Suspension
```

每個 player 有：

```text
Priority
```

例如：

```text
Critical Cutscene
UI Video
World Monitor
Background Decorative Video
```

資源壓力時低 priority player 可：

```text
Reduce Queue
Pause Decode
Suspend
```

但不得影響 Critical Cutscene。

---

# 七十五、Multiple Video Playback

V1 Framework 支援多個 `VideoPlayer`。

但實際同時硬體 decoder 數量：

```text
Platform Capability-dependent
```

所以：

```text
VideoManager
↓
Capability / Budget
↓
Hardware Decode
Software Fallback
Suspend
Reject according to policy
```

不假設手機可以同時硬解大量影片。

---

# 七十六、Preload / Prepare

Cutscene 可：

```text
Open
↓
Prepare
↓
Demux ready
↓
Decoder ready
↓
First frame ready
↓
Ready
```

Gameplay 在真正需要時：

```text
Play()
```

避免第一幀卡頓。

---

# 七十七、Streaming / Buffering

對 URL Source：

```text
Network / Platform Media Source
↓
Buffer
↓
Demux
↓
Decode
```

Runtime 提供：

```text
BufferedRange
IsBuffering
```

V1 不自己實作完整 adaptive bitrate streaming stack。

HLS / DASH：

```text
V2+ / Provider Plugin
```

---

# 七十八、App Lifecycle

Mobile：

```text
App Background
↓
Video policy
├─ Pause
├─ Suspend Decoder
└─ Release Native Surface
```

Resume：

```text
Restore Decoder
↓
Seek to Saved Media Time if required
↓
Resume according to policy
```

Audio focus change 也走 Audio / Platform lifecycle。

---

# 七十九、Device Loss / Surface Loss

Video backend 必須能處理：

```text
GPU Device Loss
Swapchain recreation
Native Surface Loss
App suspend
```

VideoPlayer logical playback state 與 native decoder surface lifetime 分離。

---

# 八十、Video Error Model

統一：

```text
VideoError
├─ SourceNotFound
├─ UnsupportedContainer
├─ UnsupportedCodec
├─ DecoderUnavailable
├─ NetworkError
├─ DecodeError
├─ GPUImportError
├─ SeekFailed
└─ UnknownBackendError
```

保留 backend diagnostic code，但不直接讓 Gameplay 依賴 OS-specific error enum。

---

# 八十一、Profiler / Debug

Profiler 顯示：

```text
Active Video Players
Decoder Backend
Hardware / Software Decode
Resolution
FPS
Decode Time
Frame Queue Depth
Dropped Frames
Repeated Frames
A/V Drift
Buffered Time
CPU Copy Count
GPU Upload Bytes
Native Surface Import
Audio Queue
Memory per Player
```

Debug overlay 可顯示：

```text
PTS
Master Clock
Drift
Frame Drop
Backend
Codec
```

---

# 八十二、Video Logging

Log Category：

```text
Media
Video
VideoDecode
VideoIO
VideoAudio
VideoSubtitle
```

禁止每 frame spam。

同一錯誤需：

```text
Rate Limit
```

---

# 八十三、Video Hot Reload

Video asset metadata / subtitle / presentation settings：

```text
Editor Hot Reload
✓
```

Native decoder backend binary：

```text
Development restart preferred
```

正在播放的 video source content 替換：

```text
Build New Generation
↓
Current Player pins old generation
↓
New Open uses new generation
```

保持 Asset Generation contract。

---

# 八十四、Video 與 Bundle

```text
VideoAsset
→ normal Asset identity

Cooked video payload
→ Bundle / package content
```

但大型 video 應允許：

```text
Streamable payload
```

而不是強制一次整個載入 RAM。

Bundle residency：

```text
Video metadata
≠ Entire compressed media bytes resident in RAM
```

---

# 八十五、Video Compression / Package Policy

Video 已是壓縮 media。

Bundle 不應再次強制使用高成本整體壓縮，導致：

```text
要播放前先解壓完整大檔
```

Cooker 可標：

```text
MediaPayload
→ Stored / Streamable / Seekable
```

VFS 必須允許 range read / seek。

---

# 八十六、Video 與 VFS / Async IO

Video backend 需要：

```text
Seekable Stream Interface
Async Read
Range Read
Priority
Cancellation
```

大型本地影片不要求一次 materialize 到 RAM。

---

# 八十七、DRM Policy

V1 Core：

```text
DRM
✕
```

如果產品未來需要：

```text
Media.DRM.Provider.*
```

作 Provider Plugin。

避免把：

```text
FairPlay
Widevine
PlayReady
```

寫死進 `Media.Core`。

---

# 八十八、Video Capture / Recording

V1：

```text
Gameplay Video Recording
✕
Camera Capture
✕
Webcam
✕
Screen Capture Encoding
✕
Live Broadcast
✕
```

這些不是 Playback Framework 的必要部分。

後續可獨立：

```text
Media.Capture
Media.Encoder
Media.Streaming
```

避免 V1 scope 無限制膨脹。

---

# 八十九、Alpha Video

V1 不把 alpha-video 當 portable baseline。

可支援：

```text
Backend / Codec Capability
```

或採：

```text
Separate Alpha Track / Texture
```

特殊 Render Feature。

不得宣稱 MP4/H.264 baseline 自帶跨平台 alpha。

---

# 九十、HDR Video

V1 架構預留：

```text
ColorSpace Metadata
P010 / 10-bit Surface
HDR Metadata
```

但 V1 release gate 不要求所有平台完整 HDR video output。

可依：

```text
Platform Capability
Display HDR
Renderer HDR Path
```

啟用。

---

# 九十一、Video Security

Remote URL：

```text
HTTPS preferred
```

Backend / Platform policy 可限制 protocol。

WebView 與 Video URL trust policy 分離。

Video decoder input 視為：

```text
Untrusted Media Data
```

需：

```text
Size Limit
Duration sanity
Metadata validation
Backend error isolation
```

第三方 software decoder 建議可放獨立 worker process 的平台，可後續強化。

---

# 九十二、Video Plugin ABI

Backend Plugin public contract 使用：

```text
MediaBackendAPI
```

暴露：

```text
Open
Close
Prepare
Decode / AcquireFrame
Seek
Capability Query
Native Surface Import Descriptor
Audio Packet / PCM Interface
```

不暴露：

```text
IMFMediaSession*
AVPlayer*
MediaCodec*
AVSampleBuffer*
```

到 Engine Gameplay / UI API。

---

# 九十三、Video Platform Capability

```text
VideoCapabilities
├─ Containers
├─ Codecs
├─ HardwareDecode
├─ MaxResolution
├─ MaxConcurrentDecoders
├─ HDR
├─ ExternalTextureImport
├─ URLPlayback
└─ PlaybackRate
```

Project Settings / Device Profile 可基於能力選：

```text
Preferred Codec
Fallback Asset
Resolution Variant
```

---

# 九十四、Video V1 Build Profiles

### Desktop Client

```text
Media.Core
Media.Video
Media.Backend.WindowsMF or Apple backend
UI.VideoElement optional
```

### Android

```text
Media.Core
Media.Video
Media.Backend.AndroidMediaCodec
UI.VideoElement optional
```

### iOS

```text
Media.Core
Media.Video
Media.Backend.AppleAVFoundation
UI.VideoElement optional
```

### Dedicated Server

```text
Media.*
→ strip
```

### Training Headless

```text
Media.*
→ strip
```

---

# 九十五、Video V1 Definition of Done

V1 Video Framework 完成至少：

```text
1. 可播放 Cooked local MP4/H.264/AAC baseline。
2. Windows / Android / iOS / macOS 有正式 backend。
3. Video playback 不阻塞 Gameplay Main Thread。
4. Audio track 經 Audio.Core mixer，而不是第二套 audio device。
5. VideoTexture 可用於 Runtime UI 與 Material / World Screen。
6. UI.VideoElement 可保持 aspect、Fit / Fill / Crop。
7. 支援 Play / Pause / Stop / Seek / Loop。
8. 有 bounded frame queue 與 memory budget。
9. Hardware decode / software fallback 狀態可在 profiler 查看。
10. 支援 subtitles canonical runtime track。
11. App background / resume 不造成 dangling native surface。
12. Large video 可經 VFS seek / range read，不要求整檔進 RAM。
13. Disabled Media Plugin 在 Shipping 為 zero / near-zero cost。
14. DedicatedServer / TrainingHeadless 不包含 Video backend。
15. Video backend 不暴露 OS native decoder object 到 Gameplay ABI。
16. Asset generation 更新不破壞正在播放的舊 generation。
17. Unsupported codec / decoder failure 有統一 error model。
18. A/V sync 以 media clock 管理，而非 render frame count。
19. Shader / GPU conversion 支援 YUV metadata，不假設 CPU RGBA。
20. Video Import / Transcode tooling 不進 Runtime build。
```

---

# 九十六、Video V2+ 預留

V1 架構需預留，但不在 V1 必做：

```text
HLS
MPEG-DASH
Adaptive Bitrate
DRM Provider
Live Streaming
WebRTC
Video Capture
Video Encoder
Replay Video Export
Advanced HDR
360 / VR Video
Frame Processing Graph
Video Compositing
```

這些後續以：

```text
Media.Streaming
Media.DRM.*
Media.Capture
Media.Encoder
Media.Compositor
```

擴充，不修改 V1 `VideoPlayer` 基本 contract。



# 二十九、V1 最終原則

```text
Feature exists
≠
Feature shipped
```

```text
Plugin disabled
→ Zero / near-zero shipping cost
```

```text
Build-time selection first
Runtime dynamic loading only where useful
```

這套 Plugin Contract 必須在 V1 就定穩，V2 / V3 只增加更多可選模組，不重新推翻。


---

# 附錄 B — V1 Master Definition of Done

V1 完成必須同時滿足：

```text
Runtime
✓ World / Scene / Entity / Transform lifecycle 正式化
✓ Multiple / Additive Scene
✓ Persistent Scene
✓ Async Load / Atomic Activation / Safe Unload
✓ Data-oriented Component Storage
✓ System Scheduler / Job / Task Graph
✓ Time / Timer / Event Framework
```

```text
Rendering
✓ DX12 / Vulkan / Metal RHI
✓ RenderGraph
✓ Slang-only canonical shader source
✓ Forward+
✓ PBR / Stylized / Anime / Vegetation / Water / Unlit
✓ Shadow / Probe / PostProcess
✓ LOD / Texture Streaming
✓ HLOD V1
✓ Terrain / Vegetation
✓ VFX
```

```text
Gameplay Framework
✓ Zig Stable C ABI
✓ Hot Reload barrier
✓ Input Raw + Optional Actions
✓ Character Framework
✓ Physics / Jolt
✓ Navigation / Recast-Detour
✓ AI Blackboard / BT / Perception foundation
✓ Animation Graph / GPU Skinning / BAT
✓ Audio
✓ Runtime UI
✓ WebView optional
✓ Video / Media optional
✓ DataTable
✓ Save / Localization
```

```text
World
✓ Grid Streaming Cell
✓ Loose Quadtree spatial index
✓ Room / Portal
✓ Streaming Source / Demand / Budget
✓ Offline HLOD
✓ Large-world coordinate foundation
```

```text
Toolchain
✓ Editor
✓ Prefab / Undo
✓ Reflection / Serialization
✓ Asset Import / Cook
✓ Bundle / Patch / Generation Pinning
✓ Profiler
✓ Crash / Logging
✓ CI / Device Matrix
✓ Project / Package / Settings
```

```text
Modularity
✓ Feature Plugin
✓ Backend Plugin
✓ Editor / Cooker Plugin
✓ Provider Plugin
✓ Third-party Plugin SDK
✓ Stable C Plugin ABI
✓ PluginHost / Service Registry
✓ Bridge Layer
✓ Disabled Plugin = no shipping code / shader / asset / SDK
```

只有當上述 Gate 都通過，才進入 V2。
