# 跨平台 3D Engine — V1 AI 施工技術與系統規劃

**文件版本：AI Technical Draft v1.1**  
**對應來源：跨平台3D_Engine_V1_完整規劃書_v1_1.md**  
**用途：AI 施工、Engine Programmer 實作、系統拆分、Code Review、CI Gate。**



---

# 開發環境與 IDE 基線（Normative）

本專案正式採用：

```text
Primary IDE
→ Visual Studio Code

Build System
→ CMake

Build Executor
→ Ninja preferred

Version Control
→ Git

AI Coding Workflow
→ VS Code workspace + repository-local technical documents + tests
```

Visual Studio IDE：

```text
Optional
```

不作為專案結構或 Build 設定的權威來源。

Apple 平台的 Xcode：

```text
Platform Toolchain / Signing / Deployment / Device Debug
```

可以使用，但日常 Engine / Gameplay 程式開發仍以 VS Code 為主要工作環境。

## IDE 與 Build System 分離

正式原則：

```text
VS Code
≠ Build System
```

VS Code 只負責：

```text
Editing
Navigation
Debug Launch
Tasks
Terminal
AI Agent Workflow
```

真正 Build Contract 由：

```text
CMake
+
CMake Presets
+
Toolchain Files
+
Ninja / Platform Generator
```

決定。

禁止把關鍵 Build 設定只放在：

```text
.vscode/settings.json
.vscode/c_cpp_properties.json
Visual Studio .sln/.vcxproj
Xcode project manual settings
Developer local environment
```

Repository clone 後，CI 與新開發機必須能不依賴個人 IDE 設定重建。

## Repository VS Code Workspace

Repository root 建議：

```text
Engine.code-workspace

.vscode/
├─ extensions.json
├─ settings.json
├─ tasks.json
└─ launch.json
```

`c_cpp_properties.json`：

```text
Avoid when clangd + compile_commands.json is sufficient
```

如特定工具需要才加入。

## 建議 VS Code Extensions

Repository 的：

```text
.vscode/extensions.json
```

建議列出：

```text
clangd
CMake Tools
Zig language support
C/C++ debugger support as needed
Shader / Slang syntax support if available
Git tooling optional
```

Extension 只是建議依賴，不應成為 Build correctness 的必要條件。

## C++ Code Intelligence

優先：

```text
clangd
```

資料來源：

```text
compile_commands.json
```

CMake 必須支援：

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

Windows 若使用 MSVC / clang-cl，仍應產生可供 clangd 使用的 compilation database。

禁止大量手動維護 include path。

## CMake Presets

Repository 必須提供：

```text
CMakePresets.json
```

建議：

```text
configurePresets
├─ win-debug
├─ win-dev
├─ win-shipping
├─ mac-debug
├─ mac-dev
├─ android-dev
├─ ios-dev
└─ tools-dev
```

再依 V2 / V3 增加：

```text
server
training-headless
build-worker
benchmark
```

使用：

```text
CMakeUserPresets.json
```

保存開發者本機私人路徑與 SDK override。

`CMakeUserPresets.json`：

```text
.gitignore
```

不可作為 CI 所需設定來源。

## Windows Toolchain

主要：

```text
VS Code
+
CMake
+
Ninja
+
MSVC or clang-cl
+
Windows SDK
```

可以只安裝：

```text
Visual Studio Build Tools
```

不要求使用 Visual Studio IDE。

DX12：

```text
Windows SDK
```

Vulkan 依 repository / CI 定義的 SDK 或 headers / loader package。

## macOS / iOS Toolchain

主要編輯：

```text
VS Code
```

Toolchain：

```text
Apple Clang
Xcode SDK
CMake
Ninja where applicable
```

iOS：

```text
Xcode
→ signing
→ provisioning
→ device deployment
→ platform debugging when needed
```

但 Engine Source / CMake / Zig / Slang 仍以 repository 為權威。

禁止手動在 Xcode project 中建立只有 Xcode 才知道的核心 build rule。

## Android Toolchain

```text
VS Code
+
Android SDK
+
Android NDK
+
CMake
+
Ninja
+
NDK Clang
```

Android build 的 ABI / API Level / STL / Vulkan capability 由 CMake Preset / Toolchain 設定。

禁止讓 Android Studio 成為唯一可建置路徑。

Android Studio 可作：

```text
Optional platform debugging / packaging inspection
```

## Zig Gameplay Toolchain

Gameplay：

```text
Zig
```

工作流程：

```text
VS Code
↓
Task / CMake integration
↓
zig build / explicit Zig compiler invocation
↓
Stable C ABI artifact
↓
Engine Host load/link
```

Zig module 不依賴：

```text
Visual Studio project
Xcode manually configured target
Android Studio-only Gradle source layout
```

Mobile Shipping：

```text
Build-time native code only
```

不得提供 remote native code download / replace path。

## Slang Shader Toolchain

Shader authoring：

```text
VS Code
```

Canonical source：

```text
Slang
```

Compile：

```text
Command-line compiler / Engine shader tool
```

不得依賴 IDE-specific shader compiler。

Output：

```text
DXIL
SPIR-V
MSL
```

Shader build、reflection、variant cook 必須能由：

```text
CLI
CI
Headless Tool
```

執行。

## VS Code Tasks

`.vscode/tasks.json` 建議只作 command wrapper。

標準 task：

```text
Configure
Build Editor
Build Game
Build Tests
Run Unit Tests
Run Integration Tests
Run Editor
Run Game
Cook Assets
Validate Assets
Build Shaders
Package Development
Package Shipping
Clean
```

V2+ 可增加：

```text
Build Dedicated Server
Run Network Test
Build HLOD
Run Distributed Worker
```

V3 可增加：

```text
Run Determinism Test
Run Training Headless
Run Simulation Worker
Run Distributed Region
```

Task 不應重新定義 build logic，只呼叫：

```text
cmake
ctest
engine commandlet
zig
python tooling
```

等 repository-owned command。

## VS Code Launch Profiles

`.vscode/launch.json` 建議：

```text
Engine Editor
Game Client
Unit Test
Tool / Commandlet
```

V2：

```text
Dedicated Server
Client + Server compound
```

V3：

```text
Region Server
Gateway
Training Worker
Simulation Worker
```

Debug launch args 必須與正式 CLI 一致。

## Command-line First

任何功能如果：

```text
只能從 VS Code 按按鈕執行
```

就不算完成。

必須有 CLI：

```text
configure
build
test
cook
validate
package
```

VS Code 只是 UI shortcut。

這是 AI Agent、CI、Build Farm、Headless Tool 共用的必要條件。

## AI Agent 在 VS Code 專案中的修改規則

AI Agent 優先修改：

```text
Source
CMake
Tests
Schemas
Tool scripts
Repository-owned configuration
```

避免：

```text
Generated IDE project
Build output
Cache
Local user settings
CMakeUserPresets.json
.vscode local-only override
```

除非任務本身就是 VS Code workspace 設定。

AI 新增 Module 時必須同步檢查：

```text
CMake target
Public / Private dependency
Feature resolver
Plugin manifest if applicable
Tests
Compile commands
Shipping strip
```

AI 新增 executable 時必須補：

```text
CLI
CMake target
Preset / profile integration if needed
Launch task only as convenience
CI coverage
```

## Repository 不提交內容

`.gitignore` 至少涵蓋：

```text
/build
/out
/.cache
/.vs
.vscode local history
CMakeUserPresets.json
platform temporary package output
generated shader cache
DDC local cache
```

但以下要提交：

```text
.vscode/extensions.json
.vscode/settings.json
.vscode/tasks.json
.vscode/launch.json
CMakePresets.json
CMake toolchain files
Engine.code-workspace
```

前提是內容不得含個人絕對路徑、憑證或秘密。

## Tool Version Pinning

Repository 應能記錄或檢查：

```text
CMake version range
Ninja version range
Compiler family/version
Zig version
Slang version
Python/tooling version where required
Android NDK version
Xcode minimum version
```

CI 應報：

```text
Toolchain fingerprint
```

方便 AI / 開發者重現問題。

## Secret / Signing Separation

以下不得寫入：

```text
.vscode
CMakePresets.json
source tree
```

包含：

```text
Signing certificate private data
API secret
Store password
Cloud credential
DRM secret
```

透過：

```text
OS keychain
CI Secret
Environment injection
Provider-specific secure store
```

傳入。

## Development Environment Definition of Done

```text
✓ 新 clone repository 可由文件化 command 完成 configure/build
✓ VS Code 不是必要 Build dependency
✓ Windows 不要求 Visual Studio IDE
✓ macOS/iOS 不依賴手改 Xcode project
✓ Android 不依賴 Android Studio-only build
✓ compile_commands.json 可供 clangd
✓ CMakePresets.json 可描述正式 build profile
✓ VS Code task 只包裝正式 CLI
✓ CI 使用與本機相同的 CMake / toolchain contract
✓ AI Agent 不需操作 GUI 才能 build / test / cook
```



## V1 開發環境落地範圍

V1-M0 必須直接建立：

```text
Engine.code-workspace
.vscode/extensions.json
.vscode/settings.json
.vscode/tasks.json
.vscode/launch.json

CMakeLists.txt
CMakePresets.json
/CMake/Toolchains/
```

V1-M0 不應只建立 Windows 專案後再「日後跨平台」。

從 M0 就必須有：

```text
Windows configure smoke
macOS configure smoke
Android toolchain configure smoke
iOS toolchain configure smoke
```

M2 前再補齊真正平台 RHI / Zig executable smoke。

V1 建議開發預設：

```text
Windows Host
→ VS Code + Ninja + MSVC/clang-cl

Editor
→ EngineEditor target

Gameplay
→ Zig module target
```

Editor 執行：

```text
VS Code F5
→ launch EngineEditor
```

但 CI 使用：

```text
cmake --preset ...
cmake --build --preset ...
ctest --preset ...
```


---

# 文件用途

本文件不是產品介紹，也不是高階概念摘要。

它是給：

```text
Codex / AI Coding Agent
Engine Programmer
Tool Programmer
Technical Lead
Reviewer
CI / Build Engineer
```

使用的「施工級技術規格 + 系統規劃文件」。

## 權威順序

當資訊衝突時：

```text
1. 本版本 Master Plan 的 Architecture Contract
2. 本 AI 技術施工文件
3. ADR / Module Manifest / Schema
4. 已通過測試與 CI 的程式碼
5. 註解 / TODO
```

任何 AI Agent 不得自行推翻上層 Contract。

## AI Agent 基本施工規則

每個任務開始前：

```text
Read Scope
↓
Identify Owning Module
↓
Identify Public / Private Boundary
↓
Identify ABI / Thread / Lifetime Contract
↓
Identify Tests / CI Gate
↓
Implement Smallest Complete Slice
↓
Run Tests
↓
Report Files Changed + Risks
```

禁止：

```text
✕ 為了快速完成而跨越 Module Private Boundary
✕ 把 backend native type 暴露到 public API
✕ 用 raw pointer 取代既有 Handle / ID Contract
✕ 在 hot path 新增無界 heap allocation
✕ 加入未經規劃的全域 singleton
✕ 偷用 wall-clock 當 deterministic / fixed tick state
✕ 繞過 RenderGraph 直接提交正式 GPU workload
✕ 讓 Plugin include Engine private header
✕ 為方便而把 Optional Feature 改成 Core Dependency
✕ 修改與任務無關的大量檔案
```

每個 AI 任務輸出至少包含：

```text
Implementation Summary
Files Changed
Public API Changes
Serialization / ABI Changes
Threading Impact
Memory Impact
Tests Added / Run
Known Limitations
Follow-up
```

---

# 共通命名與目錄原則

建議 repository：

```text
/Engine
  /Core
  /Platform
  /RHI
  /RenderGraph
  /Renderer
  /World
  /Scene
  /Asset
  /Serialization
  /Reflection
  /Input
  /UI
  /Physics
  /Character
  /Navigation
  /AI
  /Animation
  /Audio
  /Media
  /Terrain
  /Vegetation
  /VFX
  /Plugin
  /Editor
  /Tools
  /ThirdParty

/Game
  /Zig
  /Plugins
  /Assets
  /Config

/Tests
  /Unit
  /Integration
  /Golden
  /Performance
  /Soak

/Build
  /CMake
  /Profiles
  /CI
```

每個 Runtime Module：

```text
<Module>/
├─ Public/
├─ Private/
├─ Tests/
└─ CMakeLists.txt
```

規則：

```text
Public/
→ 只能放穩定 public contract

Private/
→ backend / implementation / third-party wrapper

Tests/
→ module contract tests
```

---

# 基本 C++ / ABI Contract

C++：

```text
C++20
```

Smart pointer 規則：

```cpp
template<typename T>
using SharedPtr = std::shared_ptr<T>;
```

只允許這個 ownership alias。

禁止：

```text
UniquePtr alias
WeakPtr alias
```

Non-owning：

```text
T*
T&
Span/View
Handle
ID
```

Engine / GPU / Entity：

```text
Handle / ResourceID / EntityID / UUID
```

優先於 shared ownership。

跨 Stable ABI：

```text
Opaque Handle
POD Struct
Span = pointer + count
Versioned Function Table
Explicit Allocator Ownership
Explicit String Encoding = UTF-8
```

禁止跨 ABI：

```text
STL owning container
std::string
std::function
C++ exception
RTTI object
private class pointer
backend native handle
```

---

# 錯誤處理

Runtime API 原則：

```text
Expected / Result
ErrorCode
Diagnostic Context
```

不可把：

```text
assert
```

當成可恢復輸入錯誤的唯一處理。

Assert 用於：

```text
Programmer Error
Invariant Violation
Impossible Internal State
```

使用者 / Asset / Network / Plugin 輸入錯誤：

```text
Return Error
Log Structured Diagnostic
Keep Previous Known-good State when applicable
```

---

# Threading 基本規則

正式 thread/service：

```text
Main / Gameplay
Render
Worker Pool
IO Service
Audio Thread
Platform-specific Media / Network services
```

避免：

```text
One permanent OS thread per subsystem
```

AI / Nav / Streaming / Asset work 優先：

```text
TaskGraph / JobSystem
```

Cross-thread payload 優先：

```text
POD
Handle
Batch
Command Buffer
```

不要跨 thread 傳 mutable object graph。

---

# 記憶體基本規則

分類：

```text
Persistent Heap
Pool / Slab
Frame Arena
Job Scratch
Streaming
GPU Upload / Readback
Module Boundary
```

每個大型 subsystem 必須有：

```text
Memory Tag
Current
Peak
Allocation Count
Budget
```

Frame memory：

```text
不得跨 Frame
不得被 async job 長期 retention
不得被 Plugin 保存
```

---

# 測試階層

每個 System 至少考慮：

```text
Unit
Contract
Integration
Cross-platform
Serialization Compatibility
Performance
Failure Injection
Soak
```

CI Gate 是 Architecture Contract，不是最後才補的驗收。


---


# V1 技術定位

V1 = **Production Foundation**。

目標：

```text
同一套 Engine Architecture
→ 可以實際製作、Build、Profile、Package、發布一般 3D 遊戲
```

V1 不追求：

```text
Full MMO
Production GPU-driven Renderer
Full Deterministic Engine
Built-in ML Trainer
Mandatory RT / Mesh Shader
```

---

# V1 Module Graph

```text
EngineCore
├─ Memory
├─ Job / TaskGraph
├─ Time / Timer
├─ Event
├─ Logging
├─ VFS / IO
├─ Reflection / Serialization
├─ PluginHost
│
├─ Platform
│
├─ RHI
├─ RenderGraph
├─ Renderer
│  ├─ Camera
│  ├─ Lighting
│  ├─ Shadow
│  ├─ Material
│  └─ PostProcess
│
├─ World
├─ Scene
├─ Transform
├─ Entity / Component Storage
│
├─ Asset
├─ Bundle
├─ DataTable
│
├─ Input
├─ UI
├─ Localization
├─ Text / IME
│
├─ Physics
├─ Character
├─ Navigation
├─ AI
├─ Animation
├─ Audio
├─ Media
├─ VFX
├─ Terrain
└─ Vegetation
```

Optional Module 必須可被 Build Profile 移除。

---

# V1 Build System 技術規格

## CMake Target 分層

建議：

```text
EngineCore
EnginePlatform
EngineRHI
EngineRenderGraph
EngineRenderer
EngineWorld
EngineAsset
EngineEditor

Plugin_<Name>
Backend_<Name>
Tool_<Name>
```

Public dependency 只透過：

```cmake
target_link_libraries(Target
  PUBLIC  ...
  PRIVATE ...
)
```

禁止：

```text
Public target 因 private implementation include 路徑而洩漏 dependency
```

## Build Profile

至少：

```text
Editor
DesktopClient
MobileClient
DedicatedServerFoundation
Tool
Benchmark
```

每個 profile 生成：

```text
ResolvedFeatureSet
ResolvedPluginSet
ShaderFeatureSet
AssetCookProfile
PlatformCapabilityProfile
```

---

# V1 Core Runtime

## Engine Context

禁止建立無界 God Singleton。

建議：

```cpp
struct EngineServices {
    IMemoryService* memory;
    IJobSystem* jobs;
    IVirtualFileSystem* vfs;
    ILogService* log;
    IPluginHost* plugins;
};
```

Global process service 與 World-local service 分離。

```text
Process
├─ EngineServices
└─ Worlds[]
```

---

# V1 ID / Handle System

## EntityID

建議 64-bit：

```text
index
generation
```

規則：

```text
generation == 0
→ invalid
```

destroy：

```text
generation++
```

generation wrap：

```text
retire slot
```

不要讓 stale handle 重新有效。

## UUID128

Persistent identity：

```text
Scene Object
Asset
Prefab Local Object where required
```

Runtime hot path 不用 UUID 當 dense index。

---

# V1 Job System

## Job Descriptor

```cpp
struct JobDesc {
    JobFn fn;
    void* user;
    JobPriority priority;
    JobAffinity affinity;
    CancellationToken cancel;
    DebugCategory category;
};
```

Job System 必須提供：

```text
Work Stealing
Priority
Completion Fence
Cancellation
Wait / Help Execute
Debug Metadata
```

不得：

```text
Gameplay thread busy-spin wait
```

---

# V1 TaskGraph / SystemGraph

Recurring World System：

```text
System Descriptor
├─ Phase
├─ Reads[]
├─ Writes[]
├─ Resource Reads[]
├─ Resource Writes[]
├─ Priority
└─ Determinism Policy
```

Compiler：

```text
System Descriptors
↓
Conflict Analysis
↓
DAG
↓
Cached Schedule
```

Structural change：

```text
WorldCommandBuffer
```

只在 barrier commit。

---

# V1 Time

Clock：

```text
RealClock
GameClock
UnscaledClock
FixedTickClock
```

World：

```cpp
struct WorldTimeState {
    double game_time;
    double unscaled_time;
    uint64_t fixed_tick;
    float time_scale;
    bool paused;
};
```

Fixed accumulator 必須有：

```text
MaxCatchUpTicks
AccumulatorClamp
```

防 spiral-of-death。

---

# V1 Event Framework

Typed：

```text
EventTypeID
```

模式：

```text
Immediate
Deferred
CrossThread
CrossABI Batch
```

Subscription：

```text
SubscriptionHandle(index,generation)
```

Plugin unload：

```text
Remove subscriptions
↓
Drain queued callback ownership
↓
Unload
```

---

# V1 World / Scene

## Ownership

```text
Engine
→ World
→ Scene
→ Entity / Node
→ Component
```

規則：

```text
World ≠ Scene
```

World owns simulation services：

```text
PhysicsWorld
NavigationWorld
RenderWorld
Audio Context
```

Scene owns loadable / serializable content。

## World Type

```text
Editor
Play
Preview
Bake
Server future
```

## Scene State

```text
Unloaded
→ Loading
→ LoadedInactive
→ Activating
→ Active
→ Deactivating
→ LoadedInactive
→ Unloading
→ Unloaded
```

Async work 完成不等於直接 Active。

Activation：

```text
Safe Barrier
↓
Atomic register to world services
```

---

# V1 Transform System

Data-oriented storage：

```text
EntityID[]
Parent[]
FirstChild[]
NextSibling[]
LocalPosition[]
LocalRotation[]
LocalScale[]
WorldTransform[]
LocalVersion[]
WorldVersion[]
Flags[]
```

更新：

```text
Local mutation
↓
LocalVersion++
↓
Dirty Queue
↓
Depth ordered batch
↓
Parallel propagation
↓
WorldVersion++
```

Reparent：

```text
KeepLocal
KeepWorld
```

限制：

```text
No hierarchy cycle
Parent same Scene
```

Large World：

```text
High Precision World Position
+
Local Float
+
Camera-relative GPU transform
```

---

# V1 Component Storage

Default：

```text
Sparse Set
+
Dense Entity List
+
SoA Fields
```

禁止：

```text
Component::Update virtual call per entity
```

System batch iterate component view。

High frequency particles / debris：

```text
Dedicated Pool
```

不使用 Scene EntityID。

---

# V1 Reflection

Canonical metadata：

```text
TypeDescriptor
PropertyDescriptor
Attribute
```

Type：

```text
Stable TypeGUID / TypeID
```

Property：

```text
Stable PropertyID
```

rename：

```text
Alias / Migration
```

不允許只靠 current property string hash 破壞舊資料。

V1 metadata producer：

```text
Macro / constexpr / runtime registration
```

Consumer contract 必須可被 V2 Clang generator 沿用。

---

# V1 Serialization

Authoring：

```text
JSON
```

Runtime：

```text
Cooked Relocatable Binary
```

禁止：

```text
raw struct dump
raw pointer
native STL memory image
```

Binary：

```text
Magic
Version
Schema
Section Table
Offset / Index Ref
Alignment
Checksum
```

Object graph：

```text
Allocate IDs
↓
Deserialize
↓
Resolve references
↓
Validate
```

---

# V1 Asset Pipeline

```text
Source
↓
Importer
↓
Canonical Intermediate
↓
Cooker
↓
Runtime Artifact
↓
DDC
↓
Bundle
↓
Asset Registry
```

DDC key：

```text
Source Hash
Importer Version
Import Settings
Dependency Hashes
Target Profile
```

failed import：

```text
Keep Previous Known-good Artifact
```

---

# V1 Mesh / Model

Primary open interchange：

```text
glTF / GLB
```

FBX：

```text
Editor-only importer
```

Runtime Mesh：

```text
Position
Normal
Tangent
UV
Color
Index
Skin Weight
Morph
Submesh
Bounds
LOD
```

Tangents：

```text
MikkTSpace compatible
```

Skin influence：

```text
Runtime default max 4
```

Cook policy 可從 source 8 influences 壓縮到 4。

---

# V1 Bundle / Generation

Bundle：

```text
Asset / Data only
```

Native code：

```text
Never remote bundle
```

Generation：

```text
Active Generation
Pending Generation
Pinned Old Generation
Pending Delete
```

更新：

```text
Download
↓
Hash Verify
↓
Build New Generation
↓
Atomic Activate
↓
Old refs drain
↓
Delete old
```

---

# V1 DataTable

只存：

```text
Configuration / Design Data
```

不存：

```text
Mutable Runtime State
Save State
Entity State
```

Authoring：

```text
JSON / CSV / XLSX
```

Runtime V1：

```text
Canonical JSON / typed immutable runtime structure
```

Schema：

```text
Source of Truth
```

Hot reload：

```text
Generation N
↓
Build N+1
↓
Validate
↓
Atomic Publish
```

---

# V1 Renderer

## RHI

Backend：

```text
DX12
Vulkan
Metal
```

No OpenGL。

RHI public：

```text
Buffer
Texture
Sampler
Pipeline
Command
Fence
ResourceState
RenderingInfo
```

禁止 Public RHI 暴露：

```text
ID3D12Resource*
VkImage
MTLTexture*
```

## RenderGraph

Pass 宣告：

```text
Reads
Writes
CreateTransient
Queue
```

Compiler：

```text
Dependency
Topological Sort
Lifetime
Aliasing
Barrier
Queue Sync
```

所有正式 Renderer Feature：

```text
must register through RenderGraph
```

---

# V1 Shader

Source：

```text
Slang only
```

Outputs：

```text
DXIL
SPIR-V
MSL
```

Canonical Reflection：

```text
Resource ID
Binding
Type
Stage
Constant Layout
Material Parameter Layout
```

Variant：

```text
Theoretical
↓ prune
Used
↓ cook
Budget
```

不得固定全域 256 variant hard limit。

---

# V1 Material / Shading

Shading Model：

```text
PBR
StylizedPBR
Anime
Vegetation
Water
Unlit
```

Renderer 共用：

```text
Forward+
RenderGraph
Shadow
Probe
Fog
Post
```

不要為 Anime / Vegetation 建另一套 renderer。

---

# V1 Camera

```text
CameraComponent
↓
CameraSystem
↓
CameraView[]
↓
RenderWorld
```

支援：

```text
Perspective
Orthographic
Viewport
RenderTarget
Culling Mask
Priority
Camera Stack
Dynamic Resolution metadata
PostProcess context
```

Gameplay projection 預設使用：

```text
Non-jittered matrix
```

---

# V1 Lighting / Shadow

Light：

```text
Directional
Point
Spot
```

Forward+ registry。

Shadow Manager：

```text
CSM
Spot Atlas
Point strategy
Static cache
Dynamic budget
```

Shadow importance：

```text
Screen Coverage
Distance
Mobility
Quality
Visibility
```

---

# V1 PostProcess

```text
Volume
Profile
Priority
BlendDistance
Weight
```

V1：

```text
Exposure
Tonemap
Bloom
TAA
FXAA fallback
SSAO
Color Grading
Vignette
```

---

# V1 Input

Raw API 永遠存在。

Action Mapping：

```text
Optional Convenience
```

不要有：

```text
Engine semantic Move / Jump / Attack
```

支援：

```text
Keyboard
Mouse
Gamepad
Touch
simultaneously
```

Pointer：

```text
Stable PointerID
Per-pointer Capture
Single Owner
```

UI Routing：

```text
InputLayer → UILayer
```

directional matrix，不是 symmetric。

---

# V1 Runtime UI

```text
UIElement ≠ SceneNode ≠ EntityID
```

Boundary：

```text
UIDocument
```

UI storage：

```text
Handle / Pool / data-oriented
```

支援：

```text
ScreenSpace
WorldAnchored
WorldSpace
```

Text pipeline：

```text
UTF-8
↓
Localization
↓
HarfBuzz
↓
Line Break
↓
FreeType
↓
Glyph Atlas
```

Pointer：

```text
Capture
Target
Bubble
```

---

# V1 Localization / Text Edit

Localization backend：

```text
ICU4C private backend
```

stable key：

```text
LocalizationKeyID
```

language switch：

```text
LocalizationGeneration++
```

Disabled UI re-enable 時若 generation 改變，必須 refresh。

TextEdit：

```text
UTF-8 Buffer
Selection
Caret
Composition
Local Undo
Validation
```

Caret 以 grapheme cluster，不以 byte index。

---

# V1 Physics

Gameplay truth：

```text
CPU Authoritative / Jolt
```

Domain：

```text
CPUAuthoritative
CPUBatched
GPUVisual
GPUDeferred
```

GPU Visual 不回寫 gameplay truth。

Query：

```text
Immediate CPU
Batch CPU
Deferred GPU optional foundation
```

---

# V1 Character

```text
Input / AI
↓
CharacterIntent
↓
CharacterMotor
↓
CharacterController
↓
Physics
↓
MotionResult
↓
Animation / Events
```

正式欄位至少：

```text
RequestedVelocity
ActualVelocity
GroundState
GroundNormal
Slope
Platform
ExternalVelocity
RootMotionDelta
```

Character 不預設 Dynamic RigidBody。

---

# V1 Navigation

Backend：

```text
Recast / Detour private
```

Public：

```text
NavigationWorld
AgentType
Filter
AsyncPathRequest
PathResult
OffMeshLink
Obstacle
```

Nav 不直接改 Transform。

```text
Path
↓
DesiredVelocity
↓
CharacterIntent
```

---

# V1 AI

```text
AI Agent
├─ Perception
├─ Blackboard
├─ Decision Runtime
├─ Navigation Adapter
└─ Action Adapter
```

V1：

```text
Behavior Tree
Blackboard
FSM Utility foundation
Perception
AI LOD
ML Policy Interface foundation
```

BT：

```text
Asset
↓ compile
Compact Program / Tables
```

不要 per agent clone node object tree。

---

# V1 Animation

```text
Skeleton Evaluation
≠ Skinning
```

CPU：

```text
Graph
Blend
IK
Root Motion
Event
```

GPU：

```text
Vertex Skinning
Compute foundation
BAT
```

Global Skinning Buffer：

```text
ResourceIndex
```

Instance：

```text
skinningMatrixOffset
```

重要：

```text
skinningMatrixOffset = matrix element offset
```

不是 ResourceIndex / descriptor / bytes。

---

# V1 Audio

```text
Gameplay
↓
AudioEventID
↓
Audio.Core
↓
Backend
```

Default backend：

```text
miniaudio
```

FMOD：

```text
Optional Plugin
```

Audio thread 禁止：

```text
Heap Allocation
File IO
Mutex-heavy lock
Scene lookup
Zig callback
```

Residency：

```text
AudioResourceRegistry
AudioResidencyScope
```

---

# V1 Media / Video

Optional：

```text
Media.Core
Media.Video
```

Backends：

```text
Windows Media Foundation
Android MediaCodec
Apple AVFoundation / VideoToolbox
FFmpeg optional fallback/tooling
```

Portable baseline：

```text
MP4
H.264
AAC
```

Runtime：

```text
VideoPlayer
VideoFrame Queue
VideoTexture
UI.VideoElement
SubtitleTrack
```

A/V：

```text
Audio Clock = master when audio exists
```

Video audio：

```text
PCM
↓
Audio.Core
```

不要另開第二套 audio device。

---

# V1 Plugin SDK

第三方：

```text
Stable C ABI
C++ Convenience SDK
PluginHost
Service Registry
Extension Registry
Bridge Plugin
```

Plugin 可以：

```text
Register Service
Register Asset Type
Register Importer
Register Cooker
Register Editor Extension
Register Command
Register Render Feature foundation
```

Plugin 不可：

```text
include Engine Private/
```

Memory ownership 跨 module 必須顯式。

---

# V1 Large World Foundation

```text
Logical Scene
≠ Streaming Cell
```

V1 Partition：

```text
Stable Fixed Grid Cell
+
Loose Quadtree acceleration
```

Indoor：

```text
Room / Portal
```

Streaming：

```text
Source
↓
Demand
↓
Merge
↓
Priority
↓
Residency
```

Cell：

```text
Unloaded
Requested
IOResident
BuiltInactive
Active
Retiring
```

HLOD：

```text
Offline Build
Runtime selection / streaming only
```

---

# V1 Editor

Service：

```text
DocumentManager
SelectionService
ObjectAdapter Registry
Inspector
PropertyDrawer
Undo
DnD
Clipboard
Gizmo
Asset Browser
Search
Settings
Plugin Host
```

Selection：

```text
EditorObjectHandle
```

不是 raw Node pointer。

Undo：

```text
Transaction Journal
```

---

# V1 AI 施工任務模板

每個 Issue / Agent Task 建議：

```md
## Goal
一句話說清楚完成後可觀察行為。

## Owning Module
Engine/...

## Allowed Files
列可修改目錄。

## Forbidden Boundaries
不可碰的 module / ABI。

## Inputs
既有 interface / schema / asset。

## Outputs
新增 interface / runtime behavior。

## Invariants
不得破壞的 contract。

## Threading
在哪個 thread / job 執行。

## Memory
allocator / lifetime / ownership。

## Serialization / ABI
是否改 schema / ABI version。

## Tests
unit / integration / golden / perf。

## Done
可自動驗證的條件。
```

---

# V1 Milestone AI 工作包

## M0

AI 可平行：

```text
Build Profiles
Module Graph Validator
Feature Resolver
CI bootstrap
Formatting / lint
```

不可平行修改同一 root CMake target definition，除非先切 ownership。

## M1

工作包：

```text
Memory
Logging
Time
Event
Job
VFS
```

先各自有 unit tests，再做 Engine Init integration。

## M2

工作包：

```text
Slang Compiler Wrapper
Canonical Reflection
DX12 PoC
Vulkan PoC
Metal PoC
Zig ABI PoC
```

Canonical Reflection owner 只能一個 task，避免三 backend 各自定 schema。

## M3

Renderer 分：

```text
RHI Resource
Descriptor
PSO
Command
Fence
RenderGraph
Frame Pipeline
```

RenderGraph barrier ownership 必須單一負責。

## M4-M12

每個 milestone 至少要有：

```text
One Vertical Slice Test
One Failure Test
One Performance Baseline
One Feature-strip Test
```

---

# V1 CI 必備 Gate

```text
Module DAG
Feature Strip
Frame Memory Escape
RHI Lifetime
RenderGraph Barrier
Shader Reflection Parity
Scene Lifecycle
Asset DAG
Bundle Generation Pin
Plugin ABI
Zig ABI
UI Routing
Physics Character
Audio Residency
Media Decode
Streaming Cell
Shipping Matrix
```

---

# V1 Definition of Technical Complete

AI 不得用「功能看起來有」判定完成。

必須：

```text
Public Contract exists
Private Implementation isolated
Error path exists
Threading documented
Memory lifetime documented
Serialization/versioning handled
Profiler visibility exists where relevant
Unit / integration tests pass
Feature strip works
Cross-platform gate passes
```


---

# Milestone Index


```text
V1-M0  Build / CI / Module Graph
V1-M1  Core Runtime
V1-M2  Slang / RHI Cross-platform PoC
V1-M3  Renderer Mainline
V1-M4  First Scene Vertical Slice
V1-M5  Asset / Serialization / Cooker / Bundle
V1-M6  Reflection / Editor / Prefab / Plugin SDK
V1-M7  Input / Runtime UI / Text / Localization
V1-M8  Physics / Character / Navigation / AI
V1-M9  Animation / Audio / VFX / Video
V1-M10 Large World / Terrain / Vegetation / HLOD
V1-M11 Mobile / Platform / WebView
V1-M12 Shipping / Packaging / Hardening
```


---

# AI 最終提交格式

每個施工任務完成時，回報：

```md
# Result

## Summary
完成什麼。

## Files Changed
- path
- path

## Architecture Contract
遵守哪些既有 contract。

## API / ABI
是否有 public API / ABI / schema 變更。

## Threading
新增 job / queue / thread interaction。

## Memory
allocation / ownership / lifetime。

## Serialization
version / migration / compatibility。

## Tests
執行哪些測試、結果。

## Performance
有無新增 hot path / allocation / sync point。

## Risks
仍存在的風險。

## Next
下一個最小工作包。
```

不要只回：

```text
Done
```

必須提供可驗證證據。
