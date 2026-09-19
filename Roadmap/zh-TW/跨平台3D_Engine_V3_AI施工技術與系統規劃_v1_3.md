# 跨平台 3D Engine — V3 AI 施工技術與系統規劃

**文件版本：AI Technical Draft v1.3**  
**對應來源：跨平台3D_Engine_V3_完整規劃書_v1_4.md**  
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



## V3 開發環境擴充

V3 增加：

```text
region-server-dev
gateway-dev
determinism-test
training-headless
simulation-worker
offline-renderer
```

VS Code compound launch 可在單機啟動：

```text
World Directory
Gateway
Region A
Region B
Test Client
```

用於 Handoff / Epoch / Failure 測試。

但是 production distributed topology 不依賴 IDE。

ML / Simulation Farm：

```text
VS Code
→ edit / debug / launch sample
```

真正 Training：

```text
CLI
Headless
Worker Process
Farm
```

Determinism Test 必須可：

```text
ctest
or
engine commandlet
```

無 GUI 執行。

V3 AI Agent 不得因為 VS Code 支援複合啟動，就把 server process orchestration 寫死在 `.vscode/launch.json`。


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


# V3 技術定位

V3 = **Distributed / Simulation / Next-Gen**。

V3 最大原則：

```text
更高能力上限
≠ 更大的預設 Build
```

V3 所有重量級能力都是 Optional Capability Domain。

---

# V3 Delta Module Graph

```text
Deterministic.Core
Deterministic.Rollback
Deterministic.Lockstep
Deterministic.Physics

DistributedWorld.Core
DistributedWorld.Directory
DistributedWorld.Region
DistributedWorld.Handoff
DistributedWorld.Recovery
MMO.Gateway

GPUPhysics.Core
GPUPhysics.Fluid
GPUPhysics.SoftBody

RT.Core
RT.Effects
Renderer.PathTracer
RenderFeature.MeshShader
RenderFeature.ClusterGeometry
RenderFeature.WorkGraph
RenderFeature.VirtualGeometry

ML.Training.Core
ML.Training.PPO
ML.Training.SAC
ML.Training.DQN
ML.Training.SelfPlay
ML.Runtime
SimulationFarm
Crowd.Massive
```

---

# V3 Deterministic Domain

## Boundary

```text
World
├─ Deterministic Domain
└─ General Runtime / Presentation
```

只有註冊進 deterministic domain 的 state 才受 deterministic contract。

## State Schema

```text
Stable TypeID
Stable PropertyID
Canonical Serialization
Canonical Endianness
Stable Iteration
Explicit RNG
```

禁止：

```text
raw pointer
unordered_map iteration as gameplay order
wall clock
OS random
GPU async result
```

---

# V3 Deterministic Scheduler

Parallel 可行，但結果不得受 thread timing 影響。

策略：

```text
Stable Chunk Partition
↓
Local Command Buffer
↓
Stable Merge Order
↓
Stable Reduction
```

禁止：

```text
race winner = gameplay outcome
```

---

# V3 State Hash

每 tick 可：

```text
Canonical State Bytes
↓
Hash
```

Debug 可細化：

```text
World Hash
System Hash
Component Type Hash
Entity Range Hash
```

以快速定位 desync。

---

# V3 Rollback

Snapshot 策略：

```text
Full Snapshot every K ticks
+
Delta / page copy between
```

具體依 state size profile。

Rollback：

```text
Restore T
↓
Restore RNG / Timer
↓
Replay Inputs
↓
Validate Hash
↓
Publish corrected state
```

Presentation smoothing 不進 deterministic state。

Deterministic Domain 預設 Bounded / Pre-loaded（Tick 0 前全部 Resident，不參與 Streaming Unload）。若需涵蓋 V2 Streamed 大世界，須走 ADR 並實作 Deterministic Residency Log（逐 Tick 記錄 Cell Residency），`Restore T` 需先還原對應 Tick 的 Residency 才能 Replay，否則 Replay 期間 Cell Collision 與原始 Tick 不一致會破壞 determinism。

---

# V3 Lockstep

Server-coordinated default：

```text
Collect Input N
↓
Deadline
↓
Broadcast / confirm
↓
Sim Tick N
↓
Hash
```

Late input policy：

```text
Drop
Delay
Rollback
```

由 game profile 定義，不能 silent。

---

# V3 Deterministic Physics

獨立 Plugin。

目標：

```text
Small / medium deterministic simulation
```

不追求 Jolt 全 feature parity。

第一版 shape 建議：

```text
Circle / Sphere
AABB / Box
Capsule
Simple Convex
```

固定點或 verified deterministic math。

---

# V3 Distributed World Identity

```text
GlobalWorldID
RegionID
ServerNodeID
GlobalEntityID
NetworkEntityID
Local EntityID
```

全部不可混用。

Mapping 必須顯式。

---

# V3 World Directory

API：

```text
ResolveRegionOwner
AcquireLease
RenewLease
ReleaseLease
WatchRegion
ListNeighbors
```

資料：

```text
RegionID
OwnerNode
Epoch
LeaseExpiry
BuildCompatibility
Health
```

Directory 不保存 gameplay entity blob。

---

# V3 Authority Epoch

所有 authoritative message：

```text
RegionID
Epoch
```

接收端：

```text
if epoch != current
→ reject stale
```

防 split-brain。

---

# V3 Entity Handoff Protocol

Phase：

```text
Prepare
TransferState
PreCreateGhost
Validate
CommitAuthority
RouteClient
RetireSource
```

Message 必須 idempotent。

每次 Handoff：

```text
HandoffID
SourceRegion
TargetRegion
EntityGlobalID
SourceEpoch
TargetExpectedEpoch
StateRevision
```

CommitAuthority 前必須確認 Target Region 的角色 Occupied Cell Set（見 V1 Terrain Streaming Boundary Contract）已達 `StreamingPending` 相容狀態；未 Ready 則延後 Ownership Switch，角色在 Source 端維持 Authority/Collision，避免雙邊同時無 Collision。

---

# V3 Border Ghost

Ghost 不 authoritative。

資料只保留：

```text
GlobalEntityID
Transform summary
Velocity
Bounds
Relevance summary
```

不執行完整 gameplay system。

---

# V3 Persistence

Region：

```text
Checkpoint
+
Journal
```

每個 journal op：

```text
OperationID
Revision
SchemaVersion
Payload
Checksum
```

Recovery 必須 idempotent。

---

# V3 Rolling Deploy

不做 server DLL hot swap。

```text
Build N+1 deploy
↓
N drain
↓
Region checkpoint / handoff
↓
Directory route N+1
↓
Retire N
```

短暫跨版本：

```text
Compatibility Window
```

需 schema / protocol 明確支援。

---

# V3 GPU Physics Domain

```text
GPUPhysicsWorld
```

資料：

```text
Body SoA
Shape Data
Broadphase Data
Constraint Data
Solver State
Readback Summary
```

CPU interaction：

```text
Command Batch → GPU
Summary Batch ← GPU later
```

禁止 immediate per-body query stall。

---

# V3 GPU Authority

Project 必須標示：

```text
Visual
Approximate
AuthoritativeIsland
```

只有 AuthoritativeIsland 才影響對應 gameplay domain。

Dedicated Server 若沒有 GPU：

```text
不得依賴 client-only GPU authority
```

所以 online authoritative game 的關鍵 simulation 需 server-capable profile 或 CPU equivalent policy。

---

# V3 GPU Fluid

Optional：

```text
SPH
Grid
Hybrid
```

第一版 API 不暴露 solver private buffer。

Consumer：

```text
FluidHandle
Emitter Command
Collider Proxy
Surface / Particle Render Handle
Summary
```

---

# V3 RT Architecture

```text
RTASManager
├─ BLAS
├─ TLAS
├─ Update / Refit
└─ Residency
```

RenderGraph Pass：

```text
Build / Update AS
Ray Pass
Denoise
Composite
```

No RT：

```text
No AS allocation
No RT shader cook
```

---

# V3 RT Effects

Feature modules：

```text
RT.Shadow
RT.Reflection
RT.AO
RT.GI
```

每個有：

```text
Quality Profile
Ray Budget
Fallback
Denoiser
```

---

# V3 Path Tracer

用途：

```text
Golden Reference
Editor Lookdev
Photo Mode
Offline Capture
```

不與 realtime renderer 共用所有 heuristic，允許不同 integrator。

但 Material semantics 要共用 canonical material data。

---

# V3 Mesh Shader / Cluster

Cook：

```text
Mesh
↓
Cluster / Meshlet
↓
Hierarchy
↓
Stream Pages optional
```

Runtime：

```text
Mesh Shader
or
Indexed Indirect Fallback
```

Content 不可只有單一 vendor path。

---

# V3 Work Graph

Optional experimental。

只允許：

```text
Capability Check
Feature Plugin
Fallback Path
```

不得讓 gameplay / asset schema依賴它才能存在。

---

# V3 ML Environment

Interface：

```cpp
struct EnvironmentAPI {
    ResetResult reset(uint64_t seed);
    StepResult step(ActionBatchView actions);
    ObservationBatchView observations() const;
};
```

實際 Stable ABI 用 C POD / function table。

Environment：

```text
No JSON per tick
No per-agent cross-language callback
```

使用 batch buffer。

---

# V3 Observation Schema

Field：

```text
Scalar
Vector
Discrete
Mask
EntitySet
TensorHandle
```

每個 schema：

```text
SchemaID
Version
Shape
Type
Normalization
Semantic Name
```

Policy artifact 綁 schema hash。

---

# V3 Action Schema

```text
Discrete
MultiDiscrete
Continuous
Hybrid
ActionMask
```

Action 轉：

```text
AI Action / CharacterIntent / Gameplay Command
```

不能直接 set arbitrary engine memory。

---

# V3 Trainer Core

共通 service：

```text
Rollout Buffer
Replay Buffer
Optimizer Adapter
Checkpoint
Metric Sink
Evaluator
Policy Store
```

Algorithm Plugin：

```text
PPO
SAC
DQN
```

Trainer metadata：

```text
Engine BuildID
Environment Version
Observation Schema Hash
Action Schema Hash
Algorithm Version
Hyperparameters
Seed
```

---

# V3 PPO

最小技術需求：

```text
Vectorized rollout
GAE
Clipped objective
Value loss
Entropy
Mini-batch epochs
Checkpoint
```

不要把 PPO 寫進 EngineCore；它是 `ML.Training.PPO`。

---

# V3 SAC

需要：

```text
Replay Buffer
Actor
Twin Critic
Target network
Entropy temperature
```

主要適合 continuous action。

---

# V3 DQN

需要：

```text
Replay Buffer
Target Network
Epsilon / exploration policy
Discrete action
```

可以後續加 prioritized replay，不阻塞初版。

---

# V3 Self-play

Policy Pool：

```text
Main
Historical
Exploit
Evaluation
```

Matchmaker：

```text
Rating
Sampling Policy
Scenario
Seed
```

結果不可只存 win/loss，還要保存 evaluation metrics。

---

# V3 Curriculum

Stage data：

```text
Conditions
Environment Overrides
Opponent Distribution
Reward Config
Observation Noise
```

Stage transition：

```text
Metric-based
Manual
Schedule
```

---

# V3 Simulation Farm

Coordinator data：

```text
JobID
BuildID
EnvironmentID
PolicyVersion
RequiredCapability
LeaseTTL
Priority
```

Worker：

```text
Register
Lease Job
Run Episodes
Upload Rollout / Metrics
Heartbeat
Complete
```

Worker crash：

```text
Lease expiry
↓
reassign
```

---

# V3 Dataset

Chunk format：

```text
Header
Schema Hash
Build ID
Episode
Observation
Action
Reward
Done
Metrics
```

Compression / sharding 可 profile-driven。

---

# V3 Massive Crowd

Simulation tiers：

```text
Full Individual
Reduced Individual
Aggregate
Dormant
```

Transition：

```text
Individual → Aggregate
```

必須先收斂：

```text
Count
State Distribution
Resource / Faction summary
```

Materialize：

```text
Aggregate
↓ deterministic/seeded spawn policy
Individuals
```

避免 duplication。

---

# V3 Security Boundary

Network / distributed：

```text
Authenticate
Authorize
Validate Size
Validate Version
Replay Protection where needed
Rate Limit Hook
Audit
```

Plugin：

```text
Privilege Metadata
Trust Level
```

但 native plugin 不是安全 sandbox。

---

# V3 Artifact Provenance

Artifact metadata：

```text
Source Hash
Dependency Hash
Tool Version
Plugin Version
Worker BuildID
Profile
Output Hash
Timestamp optional non-hash metadata
```

Deterministic artifact hash 不可混入 unstable timestamp。

---

# V3 Build Size Analyzer

輸出：

```text
Executable
Static Library
Plugin
ThirdParty
Shader
Asset
Localization
Symbol
```

Dependency chain：

```text
Feature A
→ Plugin B
→ SDK C
→ Shader Family D
```

---

# V3 AI 施工規則

任何 AI 任務先分類：

```text
Deterministic
Distributed
GPU
ML
Tooling
```

如果跨兩個以上高風險 Domain：

```text
拆 Task
```

例如：

```text
不要一個 task 同時做
Region Handoff + Persistence + Client Prediction + UI
```

Distributed task 必須寫：

```text
Authority Owner
Epoch Behavior
Retry / Idempotency
Failure State
Recovery
```

Deterministic task 必須寫：

```text
Input Order
Iteration Order
RNG Source
Snapshot State
Hash State
```

GPU task 必須寫：

```text
Queue
Resource Lifetime
Readback Latency
Fallback
Capability
```

ML task 必須寫：

```text
Schema
Batch Shape
Checkpoint
Reproducibility
Runtime Export
```

---

# V3 Milestone AI 工作包

## M1-M2 Determinism

可拆：

```text
State Schema
RNG
Timer
Hash
Scheduler
Snapshot
Rollback
Lockstep
Deterministic Physics
```

Hash / Serialization schema 單一 owner。

## M3-M4 Distributed

可拆：

```text
Directory
Lease
Epoch
Gateway
Handoff
Interest Federation
Checkpoint
Journal
Failover
```

Handoff protocol schema 單一 owner。

## M5-M8 GPU / Renderer

分：

```text
GPUPhysics Storage
Broadphase
Solver
RTAS
RT Effects
Meshlet Cook
Mesh Shader
Cluster Streaming
```

每個 feature 都必須有 capability/fallback test。

## M9-M11 ML

分：

```text
Environment API
Schema
Vectorized World Runner
Trainer Core
PPO
SAC
DQN
Self-play
Curriculum
Evaluator
Farm Coordinator
Worker
Dataset
```

不要 Trainer Core 與 PPO 耦合。

---

# V3 CI Gate

```text
Determinism repeat hash
Cross-platform deterministic hash
Rollback replay
Lockstep desync injection
Stale epoch reject
Split-brain fault injection
Handoff timeout / retry
Handoff target collision readiness
Checkpoint journal recovery
Node kill
GPU readback stall detector
GPU resource lifetime
RT off = zero RT allocation
Mesh shader fallback parity
ML checkpoint resume
Self-play evaluation reproducibility
Worker lease recovery
Plugin privilege validation
Artifact provenance
Build size regression
```

---

# V3 Technical Complete

完成代表：

```text
V2 Project can stay V2-like
Deterministic Domain is isolated
Distributed authority is recoverable
GPU authority is explicit
Raster fallback remains
ML training is tool/headless
Simulation farm tolerates worker loss
All V3 heavy features can be stripped
```


---

# Milestone Index


```text
V3-M0  Compatibility / Capability Baseline
V3-M1  Deterministic Core
V3-M2  Rollback / Lockstep / Deterministic Physics
V3-M3  Distributed World Foundation
V3-M4  Handoff / Gateway / Persistence / Recovery
V3-M5  GPU Simulation Foundation
V3-M6  GPU Fluid / SoftBody / Destruction
V3-M7  Ray Tracing / Path Tracing
V3-M8  Mesh Shader / Cluster / Virtual Geometry
V3-M9  ML Training Runtime Foundation
V3-M10 PPO / SAC / DQN / Self-play
V3-M11 Simulation Farm / Massive Crowd / GPU Navigation
V3-M12 Security / Observability / Provenance
V3-M13 Production Hardening
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

