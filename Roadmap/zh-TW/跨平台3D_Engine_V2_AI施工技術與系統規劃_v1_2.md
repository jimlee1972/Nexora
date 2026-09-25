# 跨平台 3D Engine — V2 AI 施工技術與系統規劃

**文件版本：AI Technical Draft v1.2**  
**對應來源：跨平台3D_Engine_V2_完整規劃書_v1_4.md**  
**用途：AI 施工、Engine Programmer 實作、系統拆分、Code Review、CI Gate。**

> **進度：38%**（✅ V2-M0 至 ✅ V2-M2、✅ V2-M4 與 ✅ V2-M5 已驗收；V2-M3、V2-M6 至 V2-M12 仍待完成。）



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



## V2 開發環境擴充

V2 增加 Preset：

```text
linux-server-dev
linux-server-shipping
network-test
world-build
distributed-worker
benchmark
```

VS Code compound launch 可：

```text
Dedicated Server
+
Client A
+
Client B
```

只作 Local Development Convenience。

Network CI 不依賴 VS Code compound launch。

V2 Distributed Build / Import Worker / Shader Worker 全部必須是：

```text
CLI executable
```

不能是 Editor-only in-process tool。

Remote Device Inspector 的 VS Code workflow 可有 helper task：

```text
Deploy
Attach
Start Remote Inspector
```

但底層仍使用正式 command-line / protocol。


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


# V2 技術定位

V2 = **Scale-Up / Production**。

V2 所有技術都建立在 V1 Contract 上。

不可為了做 V2：

```text
重寫 World/Scene identity
破壞 Stable C ABI
繞過 RenderGraph
把 Optional Feature 變 Mandatory
```

---

# V2 Delta Module Graph

新增 / 強化：

```text
Reflection.Clang
DDC.Shared
Build.Distributed
GPUScene
GPUCulling
VirtualTexture
WorldPartitionV2
Network
Online
DedicatedServer
Crowd
AI.Utility
AI.Policy
Animation.Compute
Animation.PoseSearch
Timeline
UI.Advanced
Media.Streaming
LiveOps
RemoteInspector
```

---

# V2 Clang Reflection Generator

輸入：

```text
C++ Public Reflection Headers
Attributes / Macros
```

輸出 canonical：

```text
TypeDescriptor
PropertyDescriptor
Migration Alias
Replication Metadata
Editor Metadata
```

Producer 可換，但 Consumer Contract 不變。

Generator 必須：

```text
Deterministic
Incremental
Versioned
CI-validatable
```

Output 不依 source file absolute path 產不穩定 ID。

---

# V2 Content-addressable DDC

Key：

```text
Hash(
  SourceContent
  ImporterID
  ImporterVersion
  Settings
  Dependencies
  PlatformProfile
  CookerRelevantVersion
)
```

L1：

```text
Local DDC
```

L2：

```text
Shared Remote DDC
```

Lookup：

```text
L1
↓ miss
L2
↓ miss
Build
↓
L1
↓ async L2 publish
```

Remote outage 不得阻止 Local Build。

---

# V2 Import Worker

Importer 可 out-of-process。

IPC：

```text
ImportRequest
Source descriptors
Settings
Dependency descriptors
↓
Worker
↓
Artifact temp
Diagnostics
Dependency list
```

Coordinator 驗證：

```text
Hash
Schema
Output type
```

才 publish。

---

# V2 GPUScene

> **V2-M2 實作狀態：** 已驗收。Renderer contract 現已提供穩定的 generational slots、分類且
> deterministic 的 dirty uploads、current/previous transform history、world bounds、mesh/material
> indices、visibility 與 LOD metadata、fence-safe retirement，以及 deterministic CPU reference
> snapshot。完整 GPU-driven culling 仍屬於 V2-M3 範圍。

> **V2-M3 實作狀態：** 進行中。Deterministic CPU reference 現已涵蓋
> frustum／distance／LOD culling、conservative Hi-Z 與 invalidation、compaction、classification
> 及 indirect-command generation。Portable compute／indirect recording、RenderGraph queue
> ownership、CPU／GPU comparison 與 no-readback diagnostics 現已有 contract test 覆蓋；native
> Vulkan 已有完整 compute 實作，以及在 indirect draw 前強制通過 `CompareGPUDrivenResults()` 的 native gate。
> Slang-enabled Linux Vulkan 證據、DX12／Metal execution 與完整 target-tier parity 仍須完成後才能驗收。

Repository 證據清單：

- ✅ Deterministic CPU culling、compaction、classification 與 indirect-command reference。
- ✅ Portable compute dispatch 與 indirect draw recording。
- ✅ RenderGraph compute／graphics ownership transition。
- ✅ CPU／backend comparison 與 normal-path no-readback diagnostics。
- ✅ Linux offscreen gate 的 native Vulkan indirect execution。
- ✅ Native Vulkan compute-pipeline dispatch 與 storage-buffer binding/readback 驗收。
- 完整 native Vulkan compute 實作與強制 CPU／GPU comparison（驗收證據待補）。
- [ ] Slang-enabled Linux Vulkan 證據、DX12／Metal execution 與完整 target-tier parity。

核心資料：

```cpp
struct GPUSceneObject {
    Float3x4 current_transform;
    Float3x4 previous_transform;
    Sphere bounds;
    uint32_t mesh_index;
    uint32_t material_index;
    uint32_t lod_index;
    uint32_t flags;
};
```

實際 layout 依 GPU packing 調整，但 semantic 固定。

Slot：

```text
index + generation
```

destroy：

```text
Retire
↓
Fence-safe reclaim
```

---

# V2 GPU Culling Pipeline

```text
GPUScene
↓
Frustum
↓
Distance
↓
LOD
↓
Hi-Z Occlusion
↓
Visible Compaction
↓
Material / PSO Bin
↓
Indirect Args
↓
Render
```

禁止 CPU 每 object submit。

Debug 必須保留：

```text
CPU Reference Culling
```

可做 correctness diff。

---

# V2 Hi-Z

來源：

```text
Depth
↓
Mip Pyramid
```

Conservative：

```text
Camera teleport
Fast camera rotation
New occluder
Large moving object
```

要有 invalidation / relaxed policy。

不要因 aggressive occlusion 造成物件 flicker 消失。

---

# V2 Async Compute

RenderGraph queue：

```text
Graphics
Compute
Copy
```

Compiler 負責：

```text
Queue Assignment
Cross Queue Fence
Ownership
Overlap
```

Device profile 可：

```text
Collapse Compute → Graphics
```

---

# V2 Temporal Upscaler

Interface：

```text
Color
Depth
Velocity
Exposure
ReactiveMask
Jitter
InputResolution
OutputResolution
```

Backend：

```text
Engine TAAU
FSR Plugin
DLSS Plugin
XeSS Plugin
```

Gameplay 不知道 vendor。

---

# V2 Virtual Texturing

Terrain first：

```text
Feedback
↓
Resolve
↓
Tile Request
↓
IO
↓
Decode
↓
Physical Cache
↓
Page Table
```

Manager：

```text
Tile Budget
Priority
Residency
Eviction
Upload
```

General material VT optional。

---

# V2 Adaptive World Partition

**已交付（V2-M4）：✅** Adaptive quadtree cell、hierarchical CellGroup、3D volume policy、origin-rebase identity、ready-gated HLOD／impostor、persistent delta reload、occupied-footprint pin 與 changed-region commandlet 均有 Linux test 覆蓋。

V1 stable grid → V2 adaptive authoring/cook partition。

仍維持：

```text
Cell Identity
≠ Quadtree Node identity
```

CellKey 來源：

```text
Scene UUID
Partition Domain
Stable Spatial Key
```

重建不應無必要全部換 ID。

Character Occupied Cell（見 V1 Character streaming boundary）延伸適用：Physics Collision Pinned 判定以「Character Footprint 覆蓋的最小 Adaptive Cell」為準，partition 形狀改變不影響判定邏輯。

---

# V2 Origin Rebasing

```text
Global World Position
→ stable

Simulation Origin
→ shiftable

Render Origin
→ camera-relative
```

所有 subsystem 經：

```text
WorldCoordinateService
```

禁止自行 cache 假設永久不變的 float world origin。

---

# V2 Persistent Cell State

```text
Scene Defaults
+
Persistent Delta
↓
Materialized Runtime Cell
```

Delta 只保存 persistent gameplay state。

禁止：

```text
dump entire runtime memory
```

Delta 擷取與 Collision Unload 為兩件事：Cell 仍在任一 Character 的 Occupied Cell Set 內時，Persistent Delta 可正常擷取，但 Physics Collision 需保持 Pinned，Runtime Unload 需等待 Character 離開。

---

# V2 Networking Layer

```text
Transport
↓
Connection
↓
Channel
↓
Replication
↓
Prediction / Replay
↓
Game Model
```

Game Rule 不寫死到 Net Core。

---

# V2 Transport

Built-in：

```text
UDP-oriented
```

Channel：

```text
Unreliable
UnreliableSequenced
ReliableOrdered
ReliableUnordered
```

Packet：

```text
Protocol
ConnectionID
Sequence
Ack
AckBits
Channel
Payload
```

需要：

```text
MTU
Fragmentation
Reassembly
Rate Control
Timeout
```

Crypto 用成熟 library/provider，不自創 primitive。

---

# V2 Handshake

```text
Transport Connect
↓
Protocol Version
↓
Build ID
↓
Gameplay Protocol ID
↓
Content Manifest Compatibility
↓
Auth Hook
↓
Established
```

Mismatch：

```text
Explicit Reject Reason
```

---

# V2 Network Identity

```text
NetworkEntityID
≠ EntityID
≠ UUID
```

Mapping：

```text
NetworkEntityID
→ Local EntityID
```

Client / Server local ID 可完全不同。

---

# V2 Replication

Schema：

```text
Reflected Metadata
↓
Generated Codec
```

Field attributes：

```text
Replicated
OwnerOnly
InitialOnly
Quantized
PredictionState
```

Hot path 禁止慢 reflection traversal。

---

# V2 Snapshot

Server：

```text
World
↓
Interest Set
↓
Snapshot
↓
Delta vs Acked Baseline
↓
Packet
```

Client：

```text
Decode
↓
State Buffer
↓
Interpolation / Reconciliation
```

Network tick ≠ render frame。

---

# V2 Interest

Sources：

```text
Player
Camera
Party
Quest
Spectator
Gameplay Rule
```

Filter：

```text
Distance
Zone
Portal
Team
Ownership
Custom
```

不允許 global replicate all。

---

# V2 Prediction

Input：

```text
InputCommand(Tick)
```

Client：

```text
Predict
Store command
```

Server：

```text
Authoritative Sim
```

Client correction：

```text
Restore authoritative
↓
Replay pending commands
↓
Presentation smoothing
```

不假設 Jolt cross-platform bitwise deterministic。

---

# V2 Dedicated Server

**已交付（V2-M5）：✅** Renderer-free `NexoraDedicatedServer` target、含版本與 build identity 的 handshake、明確 channel semantics、deterministic packet simulation，以及 connect／disconnect rejection gate 均由 Linux 測試覆蓋。


Profile：

```text
Linux x64
Windows x64 optional
```

Strip：

```text
Renderer
UI
GPU Asset
Audio output
Editor
```

保留：

```text
World
Physics
Navigation
Gameplay
Network
Data
Persistence Adapter
```

---

# V2 Replay

Chunk：

```text
Header
Build ID
Schema
Initial State
Input / Snapshot / Event
Checkpoint
```

用途：

```text
Bug repro
Spectator
Network debug
AI data
Regression
```

---

# V2 Navigation V2

```text
NavMesh Tiles
Region Graph
Portal Graph
Cost Field
Dynamic Obstacle
Crowd
Query Scheduler
```

長距離：

```text
High-level Route
↓
Local NavMesh Path
```

---

# V2 Query Scheduler

Request：

```text
Priority
Start
Goal
AgentType
CostProfile
Deadline
WorkBudget
```

可跨 frame。

不要所有 NPC 同 frame synchronous A*。

---

# V2 Crowd

Data-oriented：

```text
CrowdAgentPool
```

輸出：

```text
Desired Motion
```

再進：

```text
CharacterIntent
```

不直接 set transform。

---

# V2 Utility AI

```text
Action
├─ Consideration[]
├─ Weight
├─ Curve
└─ Score
```

batch evaluate。

不要每 consideration heap virtual object。

---

# V2 Perception LOD

```text
Near → full
Mid → reduced frequency
Far → coarse/event
Dormant → none
```

Physics query 用 batch。

---

# V2 AI Policy

```text
ObservationBuffer
↓
IAIPolicy
↓
ActionBuffer
```

Runtime backend：

```text
Native
ONNX optional
```

Trainer 仍在 tool/headless。

---

# V2 Animation

Compute Skinning：

```text
Bone Matrix
+
Source Vertex
↓
Compute
↓
Skinned Vertex Buffer
```

適合多 pass reuse。

GPU Pose：

```text
Clip Data
↓
GPU sample
↓
Pose / Matrix
```

RootMotion / Event 仍 CPU authority。

Motion Warping：

```text
Animation Root Motion
+
Target
↓
Warp
↓
CharacterMotor
```

---

# V2 Pose Search

Database：

```text
Pose Sample
Feature Vector
Clip / Time Ref
```

Query：

```text
Trajectory Feature
Pose Feature
Tags / Constraints
↓
Search
```

Builder deterministic。

Motion Matching 是 Optional framework。

---

# V2 Timeline

Asset：

```text
Timeline
├─ Camera Track
├─ Transform
├─ Animation
├─ Audio
├─ Event
├─ Material
├─ UI
└─ Custom
```

Binding：

```text
ObjectBindingID
```

不用 raw pointer。

Runtime：

```text
Evaluate(Time)
↓
Track Commands
```

Editor Scrub 不必完整 gameplay sim。

---

# V2 UI Advanced

Optional：

```text
Flex
Grid
RichText
StyleSheet
Theme
Accessibility
Surface UI
```

Surface UI：

```text
World Ray
↓
Mesh UV
↓
UIDocument local coordinate
↓
Normal UI hit test
```

---

# V2 Media Streaming

```text
Media.Streaming.HLS
Media.Streaming.DASH
```

ABR：

```text
Throughput EWMA
Buffer Level
Decode Capability
Display Need
Thermal
```

要有：

```text
Hysteresis
Min Hold Time
Safety Margin
```

DRM：

```text
Provider Plugin
```

Capture / Encoder：

```text
Optional
```

---

# V2 LiveOps

Remote：

```text
Bundle Patch
Data Overlay
Localization Pack
Event Config
```

禁止：

```text
Remote Native DLL / dylib / executable update
```

Activation：

```text
Download
Verify Signature / Hash
Validate dependency
Build generation
Atomic activate
Rollback possible
```

---

# V2 Distributed Build

Task：

```text
TaskID
Input Hash
Tool Version
Profile
Expected Output Type
```

Worker：

```text
Fetch inputs
Execute hermetic-ish task
Produce artifact
Hash
```

Coordinator：

```text
Verify
Commit
```

Worker 不直接 mutate Asset DB。

---

# V2 Remote Inspector

Transport：

```text
Development-only secure channel
```

可觀察：

```text
World
Entity summary
Memory
Streaming
Physics
Audio
Network
GPU
IO
CVars
```

Shipping 預設關閉。

---

# V2 AI 施工規則

任何 V2 AI 任務先回答：

```text
Does this change V1 Contract?
```

若是：

```text
必須 ADR + explicit migration
```

若不是：

```text
must implement as extension / optional module
```

GPU 工作：

```text
No direct backend command outside RHI/RenderGraph owner
```

Network：

```text
No EntityID serialization
```

Large World：

```text
No Cell == Bundle assumption
```

---

# V2 Milestone Work Package

## M1 Toolchain

AI 可平行：

```text
Clang Generator
DDC
Import Worker
Commandlet
Scene Externalization
```

但 Canonical Metadata schema 必須單一 owner。

## M2-M3 Renderer

拆：

```text
GPUScene Storage
GPUScene Upload
Culling
Hi-Z
Compaction
Indirect
Async Queue
Debug Reference
```

每一層先有 CPU reference / test vector。

## M5-M6 Network

拆：

```text
Transport
Handshake
Codec
Snapshot
Interest
Prediction
Replay
```

不要同一 Agent 一次改全部。

## M10 Distributed Build

每個 worker task 必須：

```text
Pure-ish input
Explicit version
Hashable output
No hidden project mutation
```

---

# V2 CI Gate

```text
Reflection deterministic
DDC hash stable
Import worker crash isolation
GPUScene stale-handle
GPU culling vs CPU reference
RenderGraph async queue validator
Partition deterministic
Origin rebase
Character streaming boundary under adaptive/3D partition
Network fuzz
Protocol mismatch
Replication schema
Prediction replay
Headless server strip
Crowd budget
Pose database deterministic
Timeline serialization
ABR / media error
Patch rollback
Distributed artifact hash
```

---

# V2 Technical Complete

必須證明：

```text
V1 不被破壞
V2 Feature 可單獨關閉
GPU-driven 有 fallback
Large World build deterministic
Networking 有 failure path
Server 無 presentation dependency
Toolchain 可 headless
Distributed Build 可重現
LiveOps 可 rollback
```


---

# Milestone Index


```text
✅ V2-M0  Migration / Baseline
✅ V2-M1  Clang Reflection / DDC / Headless Toolchain
✅ V2-M2  GPUScene
V2-M3  GPU-driven Renderer
V2-M4  Large World V2
✅ V2-M5  Dedicated Server / Transport
V2-M6  Replication / Interest / Prediction / Replay
V2-M7  Navigation / Crowd / AI V2
V2-M8  Animation V2
V2-M9  Timeline / UI V2 / Audio / Media V2
V2-M10 Shared DDC / Distributed Build / LiveOps
V2-M11 Remote Tools / Diagnostics
V2-M12 Hardening / Shipping
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
