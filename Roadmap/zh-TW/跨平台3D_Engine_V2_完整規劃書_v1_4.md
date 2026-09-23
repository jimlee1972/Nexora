# 跨平台 3D Engine — V2 完整規劃書

**文件版本：Master Draft v1.4**
**Engine 世代：V2.x — Scale-Up / Production**

> 本文件為 **V2 Master Plan**，所有 V1 Contract 預設繼承；只有本文件明確標示「V2 supersede」的項目可以改變 V1 行為。
>
> V2 的主題不是重寫引擎，而是把 V1 擴張到 GPU-Driven、Large World V2、Networking / Dedicated Server、進階 AI / Navigation / Animation、Distributed Build、LiveOps 與 Production Tooling。
>
> 本文件末端包含完整 V1 Master Baseline，使此檔可獨立閱讀。


**文件版本：Draft v1.0**  
**對應 Engine 世代：V2.x**  
**基線：V1 / Architecture Completion Baseline 已完成並通過 Gate 後進入**  
**定位：Scale-Up / GPU-Driven / Large World / Networking / Production Tooling**

---

# 一、V2 的定位

V1 的目標是：

```text
可以完整製作、Build、Profile、發布一款 3D 遊戲。
```

V2 不重新設計一套新引擎。

V2 的目標是：

```text
在不破壞 V1 核心架構 Contract 的前提下，
把引擎從「可完成一般 3D 遊戲」
提升到：

大型無接縫世界
+
高密度 GPU-driven Rendering
+
多人連線 / Dedicated Server
+
大型團隊內容製作
+
更成熟的 Editor / Build Farm / LiveOps
+
更進階的 AI / Animation / Navigation
```

V2 不是「把所有 Future 功能全部打開」。

V2 仍遵守：

```text
Profile-driven
Platform-aware
Budget-driven
Optional Framework
No hidden expensive path
```

---

# 二、V2 不改變的核心 Contract

下列 V1 Contract 在 V2 繼續成立。

```text
C++20 Engine Core
```

```text
Gameplay
→ Zig Primary
→ Stable C ABI
```

```text
SharedPtr
→ only shared-lifetime smart-pointer alias
```

不新增：

```text
UniquePtr Alias
WeakPtr Alias
```

Engine / GPU Resource 繼續優先使用：

```text
Handle
ResourceID
EntityID
UUID
```

而不是跨系統 shared ownership。

---

```text
SceneGraph
≠
SpatialWorld
≠
RenderWorld
≠
PhysicsWorld
≠
NavigationWorld
```

V2 不把這些重新合成一個 God Object。

---

```text
World
≠ Scene
≠ StreamingCell
```

V2 只增加更強的 partition / streaming 能力。

---

```text
Physics Gameplay Truth
→ CPU Authoritative / Jolt
```

V2 不改成 GPU-authoritative gameplay physics。

---

```text
Runtime UI
≠ Editor ImGui
```

Dear ImGui 仍只屬於 Editor / Debug Tool。

---

```text
Shader Source
→ Slang
```

不因 V2 Renderer 增加：

```text
Handwritten HLSL path
Handwritten MSL path
Independent GLSL source
```

---

```text
RenderGraph
→ Owns pass dependency / resource state / transient lifetime
```

GPU-driven / async compute 不允許繞過 RenderGraph 偷送 command。

---

```text
Bundle
→ Data / Asset only

Native Code
→ Never remote-downloaded as content bundle
```

V2 LiveOps 不改變這條安全邊界。

---

# 三、V2 主要目標

V2 分成八個主要方向：

```text
V2
├─ A. GPU-Driven Renderer
├─ B. Large World V2
├─ C. Networking / Dedicated Server
├─ D. Advanced Animation / AI / Navigation
├─ E. Production Editor / Collaboration Workflow
├─ F. Asset / DDC / Distributed Build
├─ G. Runtime Feature Expansion
└─ H. Production Hardening / LiveOps
```

---

# 四、V2 Scope Matrix

符號：

```text
✅ V2 committed
△ Conditional / profile-driven / optional plugin
❌ Explicitly not V2 core
```

## Rendering

```text
Compute Culling                         ✅
GPU Frustum Culling                     ✅
GPU Hi-Z Occlusion                      ✅
GPU LOD Selection                       ✅
GPU Material / Draw Classification      ✅
Indirect Draw                           ✅
GPU Driven Rendering                    ✅
GPU Instance Compaction                 ✅
Async Compute Scheduling                ✅
Compute Skinning                        ✅
GPU Pose Sampling for Crowd             ✅
Meshlet Runtime Path                    △ High-end profile
Mesh Shader Backend                     △ Capability-dependent
Virtual Texturing                       ✅ Terrain; △ General
Temporal Upscaler Interface             ✅
Hardware Ray Tracing                    △ Optional plugin/profile
Path Tracing                            ❌ V2 core
```

## World / Streaming

```text
Adaptive Quadtree Cell Generation       ✅
Hierarchical Streaming Cell Groups      ✅
3D Volume Partition                     ✅
Octree                                  △ Profile-driven
World Origin Rebasing                   ✅
Large World Coordinate Full Runtime     ✅
HLOD V2 / Multi-tier                    ✅
Impostor Builder                        ✅
Cell Persistent State Integration       ✅
World Partition Build Commandlets       ✅
Scene Runtime Structural Patch          △ Restricted
Distributed MMO World Partition         △ Future / server R&D
```

## Networking

```text
Transport Abstraction                   ✅
Datagram Realtime Transport             ✅
Reliable / Unreliable Channels          ✅
Connection / Handshake                  ✅
Snapshot Replication                    ✅
Delta Compression                       ✅
Interest / Relevancy                    ✅
Dormancy                                ✅
Client Prediction                       ✅
Server Reconciliation                   ✅
Character Prediction Integration        ✅
Dedicated Server Build                  ✅
Headless Linux Server                   ✅
Replay / Network Capture                ✅
Rollback Islands                        △ Game-defined
Lobby API                               ✅ Interface
Matchmaking                             △ Provider Plugin
Voice Chat                              △ Provider Plugin
P2P NAT Traversal                       △ Provider Plugin
Full MMO Distributed Server Mesh        ❌ V2 core
```

## AI / Navigation

```text
Hierarchical Pathfinding                ✅
NavMesh Streaming V2                    ✅
Crowd System                            ✅
Navigation Query Budget                 ✅
Utility AI                              ✅
Behavior Tree V2                        ✅
AI Simulation LOD                       ✅
Perception Budget / LOD                 ✅
Influence / Cost Field                  ✅
GOAP                                    △ Optional module
Learned Policy Runtime Interface        ✅
ONNX Policy Backend                     △ Optional plugin
Self-play Training Interface            ✅ Tooling boundary
Engine-native ML Trainer                ❌ V2 core
```

## Editor / Tooling

```text
Clang AST Reflection Generator          ✅
Scene Diff / Merge                      ✅
Prefab Conflict / Rebase UI             ✅
Externalized World Entity Files         ✅
Partition / HLOD Build Commandlet       ✅
Remote Device Inspector                 ✅
Live Device Profiling                   ✅
Editor Automation / Headless Command    ✅
Shared DDC Client                       ✅
Source Control Provider Interface       ✅
Multi-user Realtime Collaborative Edit  △ Future
```

## Runtime UI / Web

```text
Flex-like Layout                        ✅
Advanced Grid Layout                    ✅
RichText                                ✅
Inline Image / Icon                     ✅
UI StyleSheet / Theme                   ✅
RenderTexture Surface UI                ✅
Offscreen WebView                       △ Platform-dependent
Runtime Accessibility Foundation        ✅
UI Animation Timeline integration       ✅
```

## Animation

```text
Compute Skinning                        ✅
GPU Animation Sampling                  ✅
Compressed Pose Storage                 ✅
Runtime Retarget V2                     ✅
Motion Warping                          ✅
Inertialization                         ✅
Sync Group                              ✅
Pose Search                             ✅
Motion Matching                         △ Optional framework
GPU Full Animation Graph Evaluation     △ Later V2.x
```

## Physics

```text
GPU Cloth Production Path               ✅
GPU Visual Debris                       ✅
Physics LOD                             ✅
Deferred Query V2                       ✅
Destruction / Fracture                  △ Optional module
GPU Broadphase                          △ Profile-driven R&D
GPU Authoritative RigidBody World       ❌
```

## Build / Content

```text
Content-addressable DDC                 ✅
Shared / Remote DDC                     ✅
Import Worker Process                   ✅
Distributed Shader Compile              ✅
Distributed HLOD Build                  ✅
Distributed Asset Cook                  ✅
Incremental Patch Build                 ✅
Binary DataTable Runtime Format         ✅
Memory-mapped Runtime Table             ✅
Live Data Overlay                       ✅
Native Code Remote Update               ❌
```

---

# 五、V2 Renderer：GPU-Driven Rendering

V1 Renderer 已有：

```text
RenderGraph
Forward+
Material System
GPU Instancing
LOD
Culling Foundation
Resource Registry
Bindless-first
```

V2 正式把 rendering submission 改為 GPU-driven first。

## 5.1 Runtime Pipeline

```text
CPU World / Render Extraction
↓
GPU Scene Buffer
↓
GPU Frustum Culling
↓
GPU Hi-Z Occlusion
↓
GPU LOD Selection
↓
Material / PSO Classification
↓
Instance Compaction
↓
Indirect Command Generation
↓
Render Passes
```

CPU 不再每 frame 對大量 draw item：

```text
for every object
→ visibility check
→ select LOD
→ submit draw
```

CPU 主要工作：

```text
Update dirty GPU scene records
Build high-level view state
Dispatch culling
Submit indirect passes
```

---

# 六、GPU Scene

V2 建立正式：

```text
GPUScene
```

GPUScene 儲存 rendering-relevant compact state：

```text
GPUSceneObject
├─ World / Render Relative Transform
├─ Previous Transform
├─ Bounds
├─ Mesh ResourceIndex
├─ Material ResourceIndex
├─ Skinning Info
├─ LOD Info
├─ Visibility Flags
├─ Layer Mask
└─ Custom Render Data
```

正式規則：

```text
Scene Entity
≠ GPUSceneObject
```

一個 Entity 可產生：

```text
0
1
N
```

個 GPU scene records。

GPUScene 使用 stable runtime slot / generation handle。

Object Destroy：

```text
Mark Retired
↓
Fence-safe reclaim
```

不得立即 reuse GPU-visible slot。

---

# 七、GPU Culling

V2 至少：

```text
GPU Frustum Culling
GPU Distance Culling
GPU LOD Culling
Hi-Z Occlusion
```

Pipeline：

```text
GPUScene Bounds
+
Camera Frustum
+
Hi-Z
↓
Visible Instance List
↓
Compaction
```

Hi-Z：

```text
Previous / Current Depth
↓
Depth Pyramid
↓
Occlusion Test
```

必須處理：

```text
Camera teleport
Fast rotation
Occluder appearance
Large dynamic object
```

以 conservative policy 避免錯誤消失。

---

# 八、Indirect Draw

RHI 對外提供统一 abstraction：

```text
IndirectDrawBuffer
IndirectDrawCount
```

Backend：

```text
DX12
→ ExecuteIndirect

Vulkan
→ DrawIndirect / DrawIndirectCount

Metal
→ Indirect Command Buffer / equivalent capability path
```

不向上層暴露 backend native command signature。

---

# 九、GPU LOD Selection

LOD 不再要求 CPU 每個 instance 決定。

輸入：

```text
Bounds
Projected Screen Size
LOD Threshold
Quality Tier
Performance Bias
```

GPU：

```text
Select LOD
↓
Append to LOD-specific Draw Bin
```

支援：

```text
Object LOD
Vegetation LOD
Skinned Mesh LOD
HLOD transition candidate
```

HLOD 的 residency 決策仍由 Streaming / World Partition 管理。

GPU 只從已 resident representation 中選擇。

---

# 十、Meshlet Pipeline

V2 高階 profile 可加入：

```text
Mesh
↓
Cook
↓
Meshlet Builder
↓
Meshlet Bounds / Cone
↓
GPU Culling
↓
Mesh Shader or Indexed Fallback
```

重要：

```text
Meshlet Asset
≠ Requires Mesh Shader
```

即使平台沒有 Mesh Shader：

```text
Meshlet visibility result
→ indirect indexed draw
```

因此 Asset Pipeline 可共用。

Mesh Shader 僅：

```text
Capability-dependent optimization
```

不成為 V2 content compatibility 必要條件。

---

# 十一、Async Compute

V2 RenderGraph 增加 queue-aware scheduling：

```text
Graphics Queue
Compute Queue
Copy Queue
```

Candidate：

```text
VFX Simulation
Skinning
Hi-Z
GPU Culling
Some PostProcess
VT Feedback Resolve
```

RenderGraph Compiler 負責：

```text
Queue Assignment
Cross-Queue Dependency
Semaphore / Fence
Resource Ownership
Overlap Analysis
```

不得由 subsystem 自行：

```text
submit compute command
```

繞過 RenderGraph。

若某 GPU 上 async compute 反而變慢：

```text
Quality / Device Profile
→ collapse to graphics queue
```

---

# 十二、Temporal Upscaler Framework

V2 新增：

```text
ITemporalUpscaler
```

輸入：

```text
Color
Depth
Velocity
Exposure
Reactive Mask
Jitter
Render Resolution
Display Resolution
```

輸出：

```text
Display-resolution Color
```

Built-in 至少提供：

```text
TAAU-like Engine Backend
```

第三方：

```text
FSR / DLSS / XeSS style plugin
```

由 Plugin / License Policy 決定。

Gameplay 不得依賴某個特定 vendor upscaler。

---

# 十三、Virtual Texturing

V1 Terrain 已預留 VT。

V2 正式提供 Terrain Virtual Texturing：

```text
Terrain Shader
↓
VT Feedback
↓
Feedback Resolve
↓
Virtual Tile Requests
↓
Async IO / Decode
↓
Physical Tile Cache
↓
Page Table
```

V2 第一階段：

```text
Terrain VT
✅
```

General Material Virtual Texturing：

```text
△
```

需先 profile：

- Content size。
- IO pressure。
- Mobile memory。
- Tile border cost。
- Shader sampling overhead。

---

# 十四、HLOD V2

V1 已有 Offline HLOD。

V2 增加：

```text
Multi-tier HLOD
Impostor
Material Atlas
Streaming-aware HLOD
GPU-driven HLOD selection hints
Incremental Distributed Build
```

Hierarchy：

```text
World
↓
Region HLOD
↓
SubRegion HLOD
↓
Cell HLOD
↓
Full Content
```

V2 HLOD Builder 支援：

```text
Static Mesh Merge
Material Merge
Texture Bake
Impostor
Vegetation Cluster Proxy
Custom Proxy
```

---

# 十五、Large World V2

V2 把 V1 foundation 升級成完整大型世界 runtime。

```text
Logical Scene
↓
World Partition Builder
↓
Stable Cell Identity
+
Adaptive Spatial Hierarchy
+
HLOD Hierarchy
+
Streaming Metadata
```

V1 的 Character Terrain Streaming Boundary Contract（Occupied Cell Set → Physics Collision Pinned、`StreamingPending` Ground State）正式延伸適用於 V2 的 Adaptive Quadtree / CellGroup / 3D Volume Partition：不論底層 partition 形狀如何變化，凡是 Character 的 Physical Footprint 覆蓋到的最小 Cell 單位（Adaptive Cell、Octree Leaf 或 Explicit Volume Cell），其 Physics Collision Residency 一律視為 Pinned，判定邏輯不因 partition 從 V1 Stable Fixed Grid 換成 V2 Adaptive/3D 結構而改變。

---

# 十六、Adaptive Quadtree Cell Generation

V1：

```text
Stable Fixed Grid
+
Loose Quadtree Spatial Index
```

V2 可由 cooker 根據 density / content cost 建立 variable resolution partition。

例如：

```text
Dense City
→ small cells

Forest
→ medium cells

Empty Region
→ large cells
```

但仍維持：

```text
QuadtreeNode
≠ Persistent Cell Identity
```

使用：

```text
Stable CellKey
```

由：

```text
Scene UUID
Partition Domain
Logical Region
Stable Spatial Key
```

生成。

Partition rebuild 不應無必要讓所有 Cell identity 重新編號。

---

# 十七、Hierarchical Cell Group

V2 新增：

```text
CellGroup
```

用途：

```text
Streaming
HLOD
Budget
Prefetch
Build Distribution
```

例如：

```text
Region
├─ CellGroup A
│  ├─ Cell 1
│  ├─ Cell 2
│  └─ Cell 3
└─ CellGroup B
```

CellGroup 不是 Scene。

也不是 Bundle。

```text
CellGroup
→ Runtime / Build hierarchy node
```

---

# 十八、3D Volume Partition

V2 正式支援非地表型世界：

```text
High-rise
Space
Underground multi-level
Flying world
```

提供：

```text
VolumePartition
```

V1 fixed XZ grid 不再是唯一 outdoor spatial mode。

可使用：

```text
3D Grid
Loose Octree
Explicit Volume
```

Octree：

```text
△
```

由專案 profile 決定。

---

# 十九、World Origin Rebasing

V1 已使用高精度 World Position foundation。

V2 正式加入：

```text
World Origin Rebasing
```

但它是 runtime optimization，不是 gameplay-visible teleport。

概念：

```text
Global WorldPosition
→ remains stable

Local Simulation Origin
→ shifts

Render Origin
→ camera-relative
```

所有需要 origin shift awareness 的 subsystem：

```text
Physics
Navigation
Particles
Audio
Debug Draw
Editor Runtime Gizmo
```

都透過 World Coordinate Service。

禁止 subsystem 自行保存假設永遠不變的 float world origin。

---

# 二十、Cell Persistent State

V2 把 Save 與 World Partition 深度整合。

```text
Scene Asset
+
Persistent World State
+
Cell Runtime Delta
↓
Runtime Cell
```

例如：

```text
Chest opened
Boss dead
Door destroyed
Resource depleted
NPC moved
```

Persistent Delta 的擷取時機與 Cell 是否可 unload 是兩件事：即使 Cell 已符合 Persistent State 擷取條件，若該 Cell 仍在某 Character 的 Occupied Cell Set 內（見 Character Framework 的 Terrain Streaming Boundary Contract），其 Physics Collision 仍必須維持 Pinned、不得先行卸載；Persistent Delta 可正常擷取存檔，但 Runtime Collision Unload 需等待 Character 離開該 Cell。

Cell unload 前：

```text
Extract Persistent Delta
↓
PersistentWorldState
↓
Unload
```

Reload：

```text
Scene Defaults
+
Persistent Delta
↓
Runtime State
```

不是保存整份 Runtime Memory Snapshot。

---

# 二十一、World Partition Commandlets

大型世界不能要求整個 World 都載進 Editor 才 Build。

V2 提供 headless tools：

```text
WorldPartitionBuild
HLODBuild
NavBuild
ProbeBuild
Validation
CellCook
```

可：

```text
Region-only
Changed-only
Distributed
CI
```

---

# 二十二、Networking Framework

Networking 在 V1 刻意不做。

V2 正式建立，但保持：

```text
Networking
≠ Gameplay Model
```

Engine 提供：

```text
Transport
Connection
Replication
Prediction
Interest
Replay
Server Runtime
```

遊戲定義：

```text
Match Rules
Lobby Rules
Gameplay Commands
Authority Policy
Specific Replicated State
```

---

# 二十三、Transport Layer

正式介面：

```text
INetTransport
```

V2 Built-in realtime transport：

```text
Datagram / UDP-oriented
```

提供 channel：

```text
Unreliable
UnreliableSequenced
ReliableOrdered
ReliableUnordered
```

內部：

```text
Packet Sequence
Ack / AckBits
Fragmentation
Reassembly
Congestion / Rate Control
MTU policy
Timeout
Connection state
```

加密不得自行發明 cryptographic algorithm。

安全層使用成熟 library / platform backend。

---

# 二十四、Connection Handshake

```text
Client
↓
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
Authentication Hook
↓
Connection Established
```

Mismatch：

```text
Reject with explicit reason
```

不得讓不同 replication schema 靜默連線。

---

# 二十五、Network Entity Identity

正式：

```text
NetworkEntityID
≠ EntityID
≠ UUID
```

`EntityID`：

```text
Local Runtime Handle
```

`UUID`：

```text
Persistent Authoring Identity
```

`NetworkEntityID`：

```text
Connection / Session Replication Identity
```

Server spawn：

```text
Server Entity
↓
NetworkEntityID
↓
Client Entity Mapping
```

Client EntityID 可完全不同。

---

# 二十六、Replication Schema

利用 canonical reflection metadata codegen：

```text
Replicated Type
↓
Replication Schema
↓
Field IDs
↓
Serializer
↓
Delta Encoder
```

標記：

```text
Replicated
OwnerOnly
InitialOnly
ReliableEvent
Quantized
PredictionState
```

但 reflection metadata 只是 schema source。

Hot path 不使用 slow reflection traversal。

Cook / Codegen：

```text
Metadata
↓
Generated Replication Codec
```

---

# 二十七、Snapshot Replication

Server：

```text
Authoritative World
↓
Interest Set
↓
Snapshot
↓
Delta against acknowledged baseline
↓
Packet
```

Client：

```text
Receive Snapshot
↓
Decode
↓
Network State Buffer
↓
Interpolation / Reconciliation
↓
Presentation
```

Snapshot Tick 與 Render Frame 解耦。

---

# 二十八、Interest Management

直接整合：

```text
World Partition
Spatial Query
Gameplay Relevancy
Network Ownership
```

不是：

```text
replicate all entities to everyone
```

Interest Source：

```text
Player
Camera
Party
Quest
Spectator
Server gameplay rule
```

輸出：

```text
Relevant NetworkEntity Set
```

支援：

```text
Distance
Zone
Portal / Room
Team
Ownership
Custom Filter
```

---

# 二十九、Network Dormancy

遠端物件長時間不改變：

```text
Dormant
```

不持續發 snapshot delta。

Wake：

```text
State Change
Gameplay Event
Interest transition
Explicit Wake
```

Dormancy 與 Scene streaming 分離：

```text
Network Dormant
≠ Cell Unloaded
```

---

# 三十、Client Prediction

Character Framework 直接提供 prediction hook：

```text
InputCommand
↓
CharacterMotor
↓
Predicted State
```

Server：

```text
Same command
↓
Authoritative result
```

Client：

```text
Authoritative State
↓
Compare
↓
Reconcile
↓
Replay pending InputCommands
```

CharacterController collision 結果不要求 bitwise deterministic。

使用：

```text
State Correction
+
Re-simulation
```

而不是假設不同 CPU / platform 浮點完全一致。

---

# 三十一、Rollback Islands

V2 不承諾：

```text
Entire engine deterministic rollback
```

可支援：

```text
Rollback Simulation Island
```

例如：

```text
Fighting combat
Projectile logic
Small deterministic gameplay layer
```

這些 gameplay state 必須：

```text
Explicit Snapshot
Deterministic Tick
Deterministic RNG
No uncontrolled wall-clock input
```

PhysicsWorld 不自動成為 deterministic rollback state。

---

# 三十二、Dedicated Server

V2 正式建立：

```text
Headless Server Build Profile
```

預設目標：

```text
Linux x64
Windows x64 optional
```

Server stripping：

```text
No Renderer
No Runtime UI
No GPU assets
No Editor
Minimal Audio or none
```

保留：

```text
World
Scene
Physics
Navigation
Gameplay
Networking
DataTable
Save / persistence adapters
```

---

# 三十三、Session / Lobby / Matchmaking Boundary

Engine Core 提供：

```text
ISessionService
ILobbyService
IMatchmakingService
```

V2 Built-in：

```text
Local / LAN / Direct Connect reference backend
```

Cloud services：

```text
Steam
EOS
Console service
Custom backend
```

以 Plugin 實作。

不把單一 commercial service 寫死到 Engine Core。

---

# 三十四、Replay Framework V2

Input Replay foundation 升級為：

```text
Gameplay Replay
Network Replay
```

Replay chunk：

```text
Header
Build ID
Schema Version
Initial State
Input / Network Frames
Events
Checkpoints
```

用途：

```text
Bug Reproduction
Spectator
Network Debug
AI Training Data
Regression Test
```

---

# 三十五、Navigation V2

V1：

```text
Recast / Detour
Tile NavMesh
Async Query
Streaming
```

V2：

```text
NavigationWorld
├─ NavMesh Tiles
├─ Portal / Region Graph
├─ Cost Field
├─ Dynamic Obstacle Layer
├─ Crowd
└─ Query Scheduler
```

---

# 三十六、Hierarchical Pathfinding

長距離：

```text
Start
↓
High-level Region / Portal Graph
↓
Target Region
↓
Local NavMesh Path
```

避免：

```text
一次 A* 搜整個大陸 NavMesh
```

High-level node：

```text
NavRegion
Room
World Partition Region
Portal
OffMesh Connection
```

---

# 三十七、Navigation Query Scheduler

Nav query 不允許所有 NPC 任意同步 pathfind。

正式：

```text
NavigationQueryScheduler
```

Request：

```text
Priority
Start
Goal
AgentType
CostProfile
Deadline
MaxWork
```

Frame budget：

```text
Critical
High
Normal
Background
```

可跨 frame 完成。

---

# 三十八、Crowd System

V2 建立：

```text
CrowdAgentPool
```

不建立大量 per-agent heavy object。

功能：

```text
Local Avoidance
Desired Velocity
Neighbor Sampling
Path Corridor
Density / Flow
Separation
```

Crowd output：

```text
Desired Motion
↓
CharacterIntent
↓
CharacterMotor
```

Crowd 不直接改 Character transform。

---

# 三十九、AI Framework V2

V1：

```text
Perception
Blackboard
Behavior Tree
AI LOD
```

V2 增加：

```text
Utility AI
Influence Field
Hierarchical AI
Learned Policy Interface
Self-play Tooling Boundary
```

仍維持：

```text
Engine does not force one decision model
```

---

# 四十、Utility AI

提供 data-oriented scoring framework：

```text
Consideration
↓
Score
↓
Curve / Normalize
↓
Action Score
↓
Select
```

Hot path：

```text
Batch Evaluate
```

不強迫每個 Consideration 使用 virtual object。

Editor 可視覺化：

```text
Action
├─ Consideration
├─ Weight
├─ Curve
└─ Runtime Score
```

---

# 四十一、GOAP

GOAP：

```text
△ Optional Module
```

原因：

- 適合特定策略 / 模擬型遊戲。
- 規劃成本與 debug 較重。
- 不應讓所有 AI 專案負擔。

Interface：

```text
WorldState
Action Preconditions
Effects
Cost
Planner
```

可建立在同一 Blackboard / Action execution framework。

---

# 四十二、AI Perception Budget

Perception：

```text
Sight
Hearing
Damage
Area Trigger
Gameplay Signal
```

V2 正式加入：

```text
Perception LOD
```

例如：

```text
Near
→ full sight query

Mid
→ reduced frequency

Far
→ coarse spatial / event-only

Dormant
→ no active perception
```

Physics query 使用 batch API。

---

# 四十三、Influence / Cost Field

大型策略、RPG AI 可使用：

```text
InfluenceField
ThreatField
CoverCostField
HeatMap
```

底層 representation 可為：

```text
Grid
Tile
Sparse Region
```

不綁死 NavMesh polygon。

Navigation 可讀取：

```text
Dynamic Cost Provider
```

---

# 四十四、Learned Policy Interface

建立：

```text
IAIPolicy
```

輸入：

```text
ObservationBuffer
PolicyState
```

輸出：

```text
Action / Intent
```

Engine Core 不知道：

```text
Neural Network
Behavior Tree
Rule Table
```

Policy backend 可以：

```text
Native
ONNX plugin
External trainer bridge
```

---

# 四十五、Self-Play Training Boundary

V2 不在 Shipping Runtime 內放完整 trainer。

正式：

```text
Engine Simulation
↓
Training Bridge
↓
Observation
↓
External Trainer
↓
Action
↓
Engine Simulation
```

支援：

```text
Headless Simulation
Multiple Worlds per Process
Accelerated Simulation Time
Deterministic Seed
Episode Reset
Metrics
Replay Capture
```

Trainer 可使用：

```text
Python
JAX
PyTorch
Other
```

不寫死。

產出的 policy 可轉：

```text
Engine-native data
or
ONNX
```

---

# 四十六、Animation V2

V1 已有：

```text
Animation Graph
State Machine
CPU Pose Evaluation
GPU Vertex Skinning
BAT Crowd Animation
```

V2：

```text
Compute Skinning
GPU Pose Sampling
Compressed Pose
Retarget V2
Motion Warping
Inertialization
Sync Group
Pose Search
```

---

# 四十七、Compute Skinning

```text
Bone Matrix Buffer
+
Bind Pose Vertex Buffer
↓
Compute Skinning
↓
Skinned Vertex Buffer
```

適合需要重複使用 skinned vertex 的：

```text
Depth
Shadow
Main
Outline
Motion Vector
```

Runtime 根據：

```text
Pass Count
Vertex Count
GPU Capability
Memory Budget
```

選：

```text
Vertex Skinning
or
Compute Skinning
```

不強制所有角色都 compute。

---

# 四十八、GPU Pose Sampling

大量 NPC：

```text
Animation Clip Data
↓
GPU Sampling
↓
Bone Pose / Skin Matrix
↓
Skinning
```

Gameplay-authoritative：

```text
Root Motion
Animation Event
Hit Window
```

仍由 CPU / Gameplay timing 管理。

不得讓 GPU event readback 成為 gameplay authority。

---

# 四十九、Compressed Pose Storage

V2 可加入：

```text
CompressedTRS
QuantizedTRS
DualQuaternion
```

Canonical runtime interface：

```text
AnimationPoseStorage
```

Consumer 不依賴具體 compression。

平台 profile 可選不同格式。

---

# 五十、Motion Warping

Character animation 可：

```text
Animation Root Motion
+
Target Transform
↓
Motion Warp
↓
Adjusted Root Motion
↓
CharacterMotor
↓
CharacterController Resolve
```

使用案例：

```text
Vault
Mantle
Execution
Attack alignment
Door interaction
Climb
```

Physics resolve 仍是最終 authority。

---

# 五十一、Inertialization / Sync Group

Animation Graph 增加：

```text
SyncGroup
Marker Sync
Inertial Blend
```

降低：

```text
Long crossfade
Pose popping
Locomotion phase mismatch
```

---

# 五十二、Pose Search / Motion Matching

V2 建立通用：

```text
PoseDatabase
FeatureExtractor
PoseSearch
```

Motion Matching：

```text
△ Optional Framework
```

原因：

- Asset authoring 成本高。
- 不適合每種遊戲。
- Engine 應支援，但不是角色動畫唯一 workflow。

Editor 提供：

```text
Pose DB Builder
Feature Visualization
Query Debug
Trajectory Debug
```

---

# 五十三、Physics V2

CPU Gameplay Truth 保持 Jolt。

V2 強化：

```text
Physics LOD
GPU Cloth
GPU Debris
Deferred Query
Destruction optional
```

---

# 五十四、Physics LOD

距離 / importance：

```text
Near
→ Full Character / RigidBody

Mid
→ Reduced update / simplified collider

Far
→ Sleeping / proxy / no dynamic simulation
```

切換必須由 explicit policy 管理。

不允許隱式讓重要 gameplay body 進入 approximation。

---

# 五十五、GPU Cloth V2

V1 foundation 升級：

```text
Cloth Asset
↓
Cooked Constraint Data
↓
GPU Simulation
↓
Collision Proxy
↓
Skin / Render
```

Gameplay 不依賴 cloth particle result。

Collision proxy 可：

```text
Capsule
Sphere
SDF
Simplified Mesh
```

---

# 五十六、Destruction / Fracture

```text
△ Optional Module
```

Authoring：

```text
Mesh
↓
Fracture Tool
↓
Chunk Graph
↓
Cooked Destruction Asset
```

Runtime：

```text
Gameplay state
→ authoritative coarse destruction

Visual fragments
→ GPU / pooled simulation
```

避免每一塊碎片都成 Scene Entity。

---

# 五十七、Runtime UI V2

V1 UI 已有 Retained Mode、Layout、Input、WorldSpace。

V2 增加：

```text
Flex Layout
Advanced Grid
RichText
StyleSheet
Theme
Accessibility
RenderTexture Surface UI
Timeline Animation Integration
```

---

# 五十八、Flex-like Layout

支援：

```text
Row / Column
Grow
Shrink
Basis
Gap
Wrap
Alignment
Justify
```

但不照搬完整 CSS runtime。

Engine 定義：

```text
UIFlexStyle
```

Cook 成 compact computed style data。

---

# 五十九、Advanced Grid Layout

支援：

```text
Fixed Track
Auto Track
Fraction Track
Row / Column Span
Gap
```

用於：

```text
Inventory
Skill grid
Settings
Complex panel
```

---

# 六十、RichText

V2：

```text
RichTextElement
```

支援：

```text
Style span
Color
Font
Size
Inline Image
Icon
Link
Localization parameter
```

Parsing 在 content/build 或 cache layer，避免每 frame parse markup。

---

# 六十一、UI StyleSheet / Theme

```text
UIStyleSheet
↓
Selector / StyleClass
↓
Computed Style
```

不追求 full browser CSS。

支援：

```text
Type
Class
State
Theme Variable
```

例如：

```text
.primary-button
.danger
:hover
:disabled
```

Cook / runtime 做快速 style resolution。

---

# 六十二、RenderTexture Surface UI

V1：

```text
WorldSpace Direct Geometry
```

V2 增加：

```text
UIDocument
↓
Offscreen UI Render
↓
RenderTexture
↓
Mesh / Material
```

用途：

```text
Curved Screen
Monitor
Vehicle dashboard
3D terminal
```

Input：

```text
World Ray
↓
Mesh UV
↓
UIDocument coordinate
↓
Normal UI Hit Test
```

---

# 六十三、Offscreen WebView

```text
△ Platform-dependent
```

架構：

```text
WebView
↓
Offscreen Surface
↓
GPU Texture
↓
Runtime UI / World Mesh
```

若平台原生 WebView 不支援可靠 offscreen capture：

```text
Capability = Unsupported
```

不得用極高成本 CPU screenshot path 假裝正常支援。

---

# 六十四、Accessibility Foundation

V2 Runtime UI 增加：

```text
Semantic Role
Accessible Name
Accessible Value
Focus Order
Text Scale
High Contrast Hook
Reduced Motion Hook
```

平台 adapter 再對接：

```text
iOS Accessibility
Android Accessibility
Desktop accessibility API
```

不要求 V2 一次實作所有平台完整 feature，但 runtime semantic tree 必須存在。

---

# 六十五、Web / Native UI Input V2

持續使用：

```text
InputLayer → UILayer Matrix
```

增加：

```text
Accessibility Focus
Remote Control Focus
Multiple UI User
Surface UI Pointer
```

Native overlay 和 GPU UI 仍必須 single-owner routing。

---

# 六十六、Audio V2

Audio runtime 保留 Backend abstraction。

V2：

```text
Room / Portal Acoustics
HRTF / Spatial Profile
Convolution Reverb
Advanced Occlusion
Audio Authoring Graph V2
Middleware Backend optional
```

---

# 六十七、Room / Portal Acoustics

直接利用 World Room / Portal graph：

```text
Listener Room
↓
Portal Path
↓
Transmission
↓
Occlusion
↓
Reverb Send
```

避免所有聲源每 frame 多次 physics raycast。

Physics raycast 可作：

```text
Near-field refinement
```

---

# 六十八、Audio DSP Graph

提供限定、安全的 authoring graph：

```text
EQ
Filter
Compressor
Limiter
Delay
Reverb Send
Spatializer
```

Runtime audio thread：

```text
No allocation
No lock
No file IO
```

規則保持不變。

---

# 六十九、Cinematic / Timeline Framework

V2 正式新增 V1 尚未建立的完整系統：

```text
Timeline
```

這不是 Animation State Machine。

Timeline 是：

```text
Time-based Multi-System Sequencing
```

---

# 七十、Timeline Asset

```text
TimelineAsset
├─ Camera Track
├─ Transform Track
├─ Animation Track
├─ Audio Track
├─ Event Track
├─ Material Track
├─ UI Track
├─ Data / Parameter Track
└─ Custom Plugin Track
```

支援：

```text
Scrub
Play
Pause
Seek
Loop
Section
Blend
Nested Timeline
```

---

# 七十一、Timeline Runtime

```text
TimelinePlayer
↓
Evaluate at Time
↓
Track Outputs
↓
Presentation Commands
```

Track 不直接持有 raw engine pointer。

使用：

```text
ObjectBindingID
Entity UUID / Runtime Binding
PropertyID
Resource Handle
```

Editor scrub 不必啟動完整 Gameplay simulation。

---

# 七十二、Camera Rig V2

Camera Framework 增加：

```text
CameraRig
├─ Follow
├─ LookAt
├─ Orbit
├─ Rail
├─ Shake
├─ Noise
├─ Collision
└─ Blend
```

Timeline 可驅動 Rig。

Gameplay 也可驅動。

Camera Rig 不硬綁 Timeline。

---

# 七十三、Editor V2

V1 Editor 已可製作遊戲。

V2 目標：

```text
Large Team
Large World
Diffable Content
Headless Automation
Remote Device Workflow
```

---

# 七十四、Clang AST Reflection Generator

V1：

```text
Macro / constexpr producer
```

V2：

```text
Clang AST Header Tool
↓
Canonical Metadata Schema
```

Consumer 不變：

```text
Serialization
Inspector
Prefab
Networking
AI Tool
Property Binding
```

正式：

```text
Producer Changes
Consumer Contract Does Not
```

Tool output deterministic。

CI：

```text
Duplicate TypeID
Duplicate PropertyID
Schema Break
Nondeterministic Output
```

全部 hard fail。

---

# 七十五、Externalized World Entity Files

大型 Scene 不應只是一個巨大 monolithic authoring file。

V2 支援：

```text
Scene Header
+
External Entity Records
```

例如：

```text
Town.scene
Town/Entities/uuid-A.entity
Town/Entities/uuid-B.entity
...
```

用途：

```text
Git merge conflict reduction
Large World partial load
Editor checkout
Incremental cook
```

Runtime cook 後仍轉 compact cell blob。

不讓 shipping runtime 去掃數十萬 authoring entity files。

---

# 七十六、Scene Diff / Merge

V2 Editor 提供 structural diff：

```text
Entity Added / Removed
Component Added / Removed
Property Changed
Hierarchy Change
Reference Change
Prefab Override Change
```

不能只做純文字 diff。

輸出：

```text
Ours
Theirs
Base
```

支援 interactive merge。

---

# 七十七、Prefab Conflict / Rebase UI

Prefab V2 工具：

```text
Prefab Asset Changed
↓
Instance Overrides
↓
Rebase
↓
Conflict Detection
```

UI 顯示：

```text
Base
New Base
Instance Override
Resolved Value
```

對：

```text
Added child
Removed child
Component replacement
Property rename / migration
```

都有明確處理。

---

# 七十八、Editor Commandlet / Headless Mode

Editor executable 或 tool executable 支援：

```text
--headless
--command=<...>
```

Command：

```text
Import
Cook
Validate
BuildHLOD
BuildNav
BuildProbe
RunTests
Package
GenerateReflection
```

讓 CI 不依 GUI automation。

---

# 七十九、Remote Device Inspector

Editor 可連：

```text
Android device
iOS device
Desktop build
Dedicated server
```

查看：

```text
Scene / Entity summary
Memory
Streaming
Input
Audio
Physics
Network
Profiler
Logs
CVars
```

Shipping build 預設關閉或需要 secure dev entitlement。

---

# 八十、Source Control Provider

Editor 不綁死 Git。

```text
ISourceControlProvider
```

可提供：

```text
Git
Perforce
Custom
None
```

Editor 使用：

```text
Status
Checkout if required
Add
Delete
Move
Diff
History link
```

Git workflow 不要求 checkout。

---

# 八十一、Asset Pipeline V2

V1：

```text
Importer
Cook
Asset DB
Bundle
Streaming
```

V2：

```text
Content-addressable DDC
Shared DDC
Import Worker
Distributed Cook
Incremental Patch
```

---

# 八十二、Content-Addressable DDC

Key：

```text
Source Content Hash
Importer ID
Importer Version
Settings Hash
Platform
Feature Profile
Engine Build / Relevant Cooker Version
```

Value：

```text
Derived Artifact
Metadata
Dependency Hashes
```

同樣 input：

```text
→ same output key
```

---

# 八十三、Shared / Remote DDC

Local：

```text
L1 Local Cache
```

Remote：

```text
L2 Shared DDC
```

流程：

```text
Lookup L1
↓ miss
Lookup L2
↓ miss
Build
↓
Store L1
↓
Upload L2
```

Remote DDC 失效：

```text
Build still works locally
```

不能讓網路服務成為開發完全不可用的單點失效。

---

# 八十四、Import Worker Process

高風險 importer：

```text
FBX
Image codec
Third-party converter
```

可在：

```text
ImportWorker
```

獨立 process 執行。

Importer crash：

```text
Editor survives
↓
Import marked failed
↓
Previous valid runtime artifact preserved
```

---

# 八十五、Distributed Build Worker

統一 worker protocol：

```text
Task
Input Hash
Tool Version
Platform Profile
Output Hash
```

Candidate：

```text
Shader Compile
Texture Cook
Mesh Cook
HLOD
Nav
Probe
DataTable
Package Chunk
```

Worker 不直接修改 Asset DB authoritative state。

Coordinator 驗證 output hash 後 commit。

---

# 八十六、Incremental Patch Build

V2 Content Build：

```text
Previous Manifest
vs
Current Manifest
↓
Changed Assets
Changed Bundles
Changed Cells
Changed Tables
↓
Patch Manifest
```

支援：

```text
Add
Replace
Retire
```

不做 native executable hot patch。

---

# 八十七、DataTable V2

V1 Runtime 可用 typed immutable tables。

V2：

```text
.tablebin
Packed Memory Block
Memory Mapping
Data Overlay
LiveOps Layer
```

---

# 八十八、Packed Runtime Table

```text
Header
Rows
Primary Index
Secondary Index
Sorted Views
Group Pool
Array Pool
String Pool
Derived Data
```

可：

```text
Single / Few allocations
or
Memory-mapped read-only block
```

C++ / Zig view 都不暴露 STL pointer。

---

# 八十九、Data Overlay

Live configuration 可：

```text
Base Table
+
Patch Overlay
↓
Resolved Table Generation
```

仍遵守：

```text
Immutable after finalize
```

更新：

```text
Build N+1
↓
Validate
↓
Atomic Registry Swap
↓
Old Generation remains pinned until refs drain
```

不允許 row 原地 mutate 造成 thread race。

---

# 九十、General Serialization V2

Cooked binary format 升級為：

```text
Relocatable
Versioned
Endian-defined
Pointer-free
```

目標：

```text
Fast load
Memory map where appropriate
Skip unknown optional section
Schema migration at cook/editor
```

Shipping runtime 不承擔任意歷史 authoring format migration。

---

# 九十一、File System / IO V2

增加：

```text
IO Batch
Read Coalescing
Priority Inheritance
Cancellation
Streaming Trace
Optional Direct IO backend
```

Asset Streaming 可提交：

```text
IORequestBatch
```

而非大量微小 read syscall。

---

# 九十二、Package / Plugin V2

Project package system：

```text
Package Manifest
Version
Dependency
Optional Feature
Platform Filter
Editor-only
Runtime
```

Lock：

```text
Project.lock
```

保證 CI 與其他開發機拿相同 plugin / package version。

---

# 九十三、Platform V2

新增正式：

```text
Linux Headless
Linux Desktop △
```

Headless Linux：

```text
✅ V2 Networking / Server required
```

Linux Desktop：

```text
△
```

若產品需求存在則啟用 Vulkan desktop backend。

WebGPU / WASM：

```text
△ R&D
```

不列 V2 release gate。

Console：

```text
△ SDK / business-dependent
```

Engine interface 保持可移植，但不在沒有 SDK 的情況假裝完成。

---

# 九十四、Save / Cloud V2

V1 Save：

```text
Local Slot
Profile
Migration
Atomic Write
Recovery
```

V2：

```text
Cloud Save Adapter
Conflict Metadata
Cross-device Revision
Server-authoritative Boundary
```

Cloud backend 以 plugin。

核心只定義：

```text
Revision
Timestamp
DeviceID
Content Hash
Conflict State
```

Engine 不自動選擇哪份存檔勝出。

Gameplay / Product policy 決定。

---

# 九十五、Localization V2

V1 已完成 locale / ICU foundation。

V2 加：

```text
Remote Localization Pack
DLC Locale Bundle
Localized Voice Pack
Pseudo Localization
Localization Coverage Report
```

CI：

```text
Missing Key
Unused Key
Missing Font Glyph
Placeholder mismatch
Plural form missing
```

---

# 九十六、Profiler / Diagnostics V2

V2 把 profiler 變成跨 process / remote 工具。

```text
Trace Producer
↓
Local Ring
↓
Stream / File
↓
Editor Profiler
```

事件：

```text
CPU Span
Job
GPU Pass
RenderGraph
IO
Asset
Streaming
Network
Physics
Animation
AI
Audio
Memory
```

---

# 九十七、Unified Trace ID

跨 subsystem 使用：

```text
FrameID
WorldID
EntityID when safe
JobID
AssetID
CellID
NetworkEntityID
```

例如一次卡頓可追：

```text
Cell Requested
↓
IO
↓
Decompress
↓
GPU Upload
↓
HLOD Switch
↓
Frame Spike
```

---

# 九十八、Runtime Developer Console V2

V1 developer console 升級：

```text
Command
CVar
Watch
Remote Command
Role / Permission
```

Remote server build：

```text
Readonly
Developer
Admin
```

權限必須可限制。

Shipping 預設：

```text
disabled
or
authenticated restricted mode
```

---

# 九十九、LiveOps / Remote Content

V2 正式支援：

```text
Remote Content Manifest
DataTable Overlay
Localization Pack
Asset Bundle Patch
Event Configuration
```

仍禁止：

```text
Native DLL / dylib / executable remote content update
```

Gameplay native module 更新仍需正常 App / executable update。

---

# 一百、Feature Flag System

V2 Project Settings 增加：

```text
FeatureFlag
```

類型：

```text
Build-time
Cook-time
Runtime Data-driven
Server-authoritative
```

Feature flag 不應使用任意字串散落 gameplay。

可由 schema / ID 管理。

---

# 一百零一、Security Boundary

V2 因 Network / LiveOps 增加，正式建立：

```text
Untrusted Network Data
Untrusted Remote Content Manifest
Untrusted Web Content
Untrusted Save / User Data
```

所有解析器：

```text
Bounds Checked
Size Limited
Version Checked
No raw pointer serialization
```

Remote manifest 必須：

```text
Signature / Integrity verification
```

具體 cryptographic implementation 採成熟 library，不自行實作 crypto primitive。

---

# 一百零二、Performance Budget V2

每個 Platform Profile 不只 Quality Tier。

新增：

```text
PerformanceBudgetProfile
├─ CPU Frame Budget
├─ GPU Frame Budget
├─ Draw Budget
├─ Visible Instance Budget
├─ Animation Budget
├─ Physics Budget
├─ AI Budget
├─ Navigation Budget
├─ RAM
├─ VRAM
├─ Streaming IO
└─ Network Bandwidth
```

Subsystem 可以：

```text
Request
Observe
Degrade
Recover
```

而不是各自硬編碼。

---

# 一百零三、Scalability Governor

V2 可加入：

```text
ScalabilityGovernor
```

輸入：

```text
GPU time
CPU time
Memory Pressure
Thermal
Battery
Frame target
```

輸出：

```text
Dynamic Resolution
Shadow Budget
Vegetation Distance
VFX Budget
Animation LOD
AI LOD
Streaming aggressiveness
```

但：

```text
Gameplay Authority
```

不得被 presentation scalability 任意改變。

---

# 一百零四、Thermal / Mobile V2

Mobile 可根據：

```text
Thermal State
Battery State
Sustained GPU Time
```

進入：

```text
Normal
Warm
Hot
Critical
```

由 Performance Profile 提供 downgrade policy。

避免只靠 FPS 掉了才降設定。

---

# 一百零五、Editor Quality / Device Preview

Editor 可以：

```text
Preview Device Profile
```

例如：

```text
Android Low
Android High
iPhone class
Desktop Mid
Desktop Ultra
```

Preview：

```text
Texture Residency
Shadow
LOD
UI Safe Area
Dynamic Resolution target
Feature Strip
```

不是只改一個 graphics quality dropdown。

---

# 一百零六、V2 Project Migration

V1 Project 升 V2：

```text
Project Copy / Branch
↓
Migration Scan
↓
Scene / Prefab / Metadata Migration
↓
Rebuild DDC
↓
Rebuild HLOD / Nav / Shader
↓
Validation
```

不直接覆蓋唯一 project copy。

Editor 提供：

```text
Migration Report
```

列出：

```text
Changed Schema
Deprecated Setting
Plugin ABI mismatch
Missing migration
Rebuild required
```

---

# 一百零七、ABI / Plugin Migration

V2 major version 可以更新 internal ABI。

但 Plugin：

```text
Plugin API Version
Engine ABI Hash
Build Configuration
Platform
Architecture
```

握手。

Mismatch：

```text
Reject Load
```

不得 crash 後才發現。

Stable C Gameplay ABI：

```text
保持 versioned compatibility strategy
```

但 major schema 不保證二進位完全不重編。

---


---

# V2 施工 Milestone（Implementation Milestones）

> V2 建立在 **V1 全部 Gate 已通過** 的前提下。  
> V2 不重寫核心；施工重點是先把「Production Metadata / Toolchain」穩定，再往 GPU-Driven、Large World、Networking 與高階 Gameplay Framework 擴張。

## ✅ V2-M0 — V1 → V2 Migration / Production Baseline

> **Repository 狀態：portable gate 已驗收。** `Tools/Migration/ScanV1Project.py` 會稽核 canonical module schema、gameplay ABI、plugin manifest，以及必要的 Development／Shipping profile。穩定的 content fingerprint 同時作為 reference-project snapshot 與 regression-baseline identity；`build.v2_migration_scanner` 證明 deterministic output 與可採取行動的 failure report。Native target performance 仍屬 target-host gate。


施工：

```text
V1 Project Migration Scanner
Plugin / Package ABI Audit
Schema Audit
Build Profile Audit
Performance Baseline
Reference Project Snapshot
```

**Gate：**

```text
✓ V1 Project 不開任何 V2 feature 仍可正常 build
✓ Migration Report 可列出 schema / plugin / rebuild requirements
✓ V1 benchmark 成為 V2 regression baseline
```

---

## V2-M1 — Clang Reflection / DDC / Headless Toolchain

先做：

```text
Clang AST Reflection Generator
Canonical Metadata Output
Content-addressable DDC
Import Worker Process
Headless Commandlet
Externalized World Entity Files
Structural Scene Diff foundation
```

原因：後面的 Networking Schema、Distributed Cook、Large World Build 都依賴穩定 metadata 與 headless tool。

**Gate：**

```text
✓ Reflection output deterministic
✓ Import worker crash 不拖垮 Editor
✓ Headless Cook / Validate 可在 CI 跑
✓ DDC same input → same artifact hash
```

---

## V2-M2 — GPUScene / Render Extraction V2

施工：

```text
GPUScene
Stable GPU Object Slot
Dirty Update
Previous Transform
Bounds
Mesh / Material ResourceIndex
Visibility Flags
LOD Metadata
Fence-safe retirement
```

先不做完整 GPU culling。

**Gate：**

```text
✓ CPU extraction 可穩定更新 GPUScene
✓ Destroy / reuse 不發生 GPU stale slot
✓ CPU reference path 與 GPUScene rendering 可比對
```

---

## V2-M3 — GPU-Driven Rendering

順序：

```text
Compute Frustum Culling
↓
GPU Distance / LOD
↓
Hi-Z
↓
Instance Compaction
↓
Material / PSO Classification
↓
Indirect Command Generation
↓
Async Compute Integration
```

再加入：

```text
Temporal Upscaler Interface
Compute Skinning
Meshlet metadata
```

**Gate：**

```text
✓ 大量 instance 不再需要 CPU one-draw-per-object
✓ DX12 / Vulkan / Metal target tier parity
✓ No normal-path GPU readback
✓ RenderGraph owns queue / barrier / lifetime
✓ CPU fallback 可做 correctness comparison
```

---

## V2-M4 — Large World V2

施工：

```text
Adaptive Quadtree Cell Generation
Hierarchical Cell Group
3D Volume Partition
World Origin Rebasing
HLOD V2
Impostor
Persistent Cell Delta
World Partition Commandlet
```

**Gate：**

```text
✓ Origin rebase 對 gameplay identity 不可見
✓ Partition build deterministic
✓ HLOD switch 不出現 hole
✓ Persistent cell unload/reload state 正確
✓ Changed region 可 incremental rebuild
✓ Character 跨 Adaptive/3D Partition Cell 邊界不失去 Collision（Occupied Cell Pinned 延伸至 V2 Partition）
```

---

## V2-M5 — Dedicated Server / Transport Foundation

先做：

```text
Headless Linux Profile
INetTransport
UDP-oriented transport
Connection
Handshake
Protocol Version
Build ID
Channel Semantics
Packet Simulation
```

不要一開始就做 Prediction。

**Gate：**

```text
✓ Linux headless server 無 Renderer dependency
✓ Client / Server connect / disconnect 穩定
✓ Loss / latency / jitter simulator 可用
✓ Protocol mismatch clean reject
```

---

## V2-M6 — Replication / Interest / Prediction / Replay

順序：

```text
NetworkEntityID
↓
Replication Schema Codegen
↓
Snapshot
↓
Delta Compression
↓
Interest
↓
Dormancy
↓
Prediction
↓
Reconciliation
↓
Replay
```

**Gate：**

```text
✓ Client / Server local EntityID 可完全不同
✓ Interest 不會 global replicate
✓ Character prediction 在測試 latency 下可玩
✓ Reconciliation 可 replay pending input
✓ Replay 足以重現 network bug
```

---

## V2-M7 — Navigation / Crowd / AI V2

施工：

```text
Hierarchical Navigation
Navigation Query Scheduler
Crowd
Utility AI
Perception LOD
Influence / Cost Field
Learned Policy Runtime Interface
Self-play Bridge
```

**Gate：**

```text
✓ Thousands AI 不會同 frame synchronous pathfind
✓ Far AI 可降頻 / dormant
✓ Crowd 輸出 CharacterIntent，不直接改 Transform
✓ Learned Policy 可被同一 Action Interface 消費
```

---

## V2-M8 — Animation V2

施工：

```text
Compute Skinning
GPU Pose Sampling
Compressed Pose
Runtime Retarget V2
Motion Warping
Inertialization
Sync Group
Pose Search
Motion Matching optional
```

**Gate：**

```text
✓ Compute / Vertex Skinning 可 profile-driven 選擇
✓ GPU crowd animation 不成為 gameplay event authority
✓ Motion Warping 最終仍經 CharacterMotor / Controller resolve
✓ Pose Search database 可重建且 deterministic
```

---

## V2-M9 — Timeline / UI V2 / Audio / Media V2

施工：

```text
Timeline
Camera Rig
Flex Layout
Advanced Grid
RichText
StyleSheet / Theme
Surface UI
Accessibility foundation
Room / Portal Audio
Media Streaming HLS / DASH
DRM Provider Boundary
Capture / Encoder optional
```

**Gate：**

```text
✓ Timeline 可 scrub / seek
✓ Surface UI world ray → UV → UI hit 正確
✓ RichText 走既有 shaping / localization
✓ Media Streaming 不改壞 V1 local VideoPlayer contract
✓ DRM / Capture / Encoder 可完全 strip
```

---

## V2-M10 — Shared DDC / Distributed Build / LiveOps

施工：

```text
Shared DDC
Distributed Shader Compile
Distributed HLOD
Distributed Cook
Patch Manifest
Data Overlay
Localization Pack
Remote Content Verification
```

**Gate：**

```text
✓ Remote DDC 掛掉仍能本機開發
✓ Two workers same input → same artifact hash
✓ Patch 驗證完成前不能 activate
✓ Old generation pins until refs drain
✓ Remote content 不可帶 native executable code
```

---

## V2-M11 — Remote Tools / Device Profiling / Production Diagnostics

施工：

```text
Remote Device Inspector
Remote Profiler
Unified Trace ID
Streaming Trace
Network Trace
Memory / GPU / IO correlation
Build Size / Feature report foundation
```

**Gate：**

```text
✓ Android / iOS / Desktop / Server 都可遠端觀察
✓ 一次 streaming hitch 可跨 IO → cook artifact → GPU upload 追蹤
✓ PluginID 可做 cost attribution
```

---

## V2-M12 — V2 Hardening / Reference Projects / Shipping

執行：

```text
Massive Outdoor
Indoor Portal Dungeon
Network Arena
Crowd City
Mobile Stress
```

長時間測：

```text
Streaming Soak
Network Soak
Memory Pressure
Thermal
Patch Rollback
Save Corruption
Server Reconnect
```

**Gate：**

```text
✓ V2 reference projects 全過
✓ 24h+ streaming soak 無 unbounded growth
✓ Network mapping disconnect 後無 leak
✓ Patch rollback 可回 known-good
✓ V2 feature 關閉後 V1-like project footprint 不異常膨脹
```

---

## V2 施工依賴圖

```text
M0 Migration / Baseline
 │
 ▼
M1 Reflection / DDC / Headless Tools
 │
 ├───────────────┐
 ▼               ▼
M2 GPUScene    M4 Large World foundation
 │
 ▼
M3 GPU Driven
 │
 ├───────────────┐
 ▼               ▼
M5 Transport   M7 AI/Nav foundation
 │               │
 ▼               ▼
M6 Replication M8 Animation V2
 │               │
 └───────┬───────┘
         ▼
M9 Timeline / UI / Audio / Media
         │
         ▼
M10 Distributed Build / LiveOps
         │
         ▼
M11 Remote Diagnostics
         │
         ▼
M12 Hardening / Shipping
```


# 一百零八、V2 Development Phases

V2 不一次平行全部做。

建議順序：

```text
V2-A Production Foundation
↓
V2-B GPU Driven
↓
V2-C Large World V2
↓
V2-D Networking
↓
V2-E Advanced Character / AI / Navigation
↓
V2-F Runtime / Cinematic / UI
↓
V2-G Distributed Build / LiveOps
↓
V2-H Hardening
```

---

# 一百零九、Phase V2-A — Production Foundation

內容：

```text
Clang AST Reflection
Content-addressable DDC
Import Worker
Headless Commandlet
Externalized World Entity Files
Scene Structural Diff
Prefab Rebase / Conflict UI
Remote Device Inspector foundation
Unified Trace IDs
```

Gate：

```text
Reflection generated deterministically
Large Scene can use external entity storage
Editor survives importer worker crash
Headless cook works in CI
Scene diff understands entity/component/property changes
Shared metadata consumer remains compatible
```

---

# 一百一十、Phase V2-B — GPU Driven Renderer

內容：

```text
GPUScene
Compute Frustum Culling
Hi-Z
GPU LOD
GPU Draw Classification
Indirect Draw
Async Compute
Compute Skinning
Temporal Upscaler API
```

Gate：

```text
100k+ static instances do not require one CPU draw submission each
GPU-driven path has backend parity on DX12 / Vulkan / Metal target tier
No hidden sync GPU readback in normal culling path
RenderGraph owns all transitions / queue sync
GPU culling can be disabled for debug and compared against CPU reference
```

---

# 一百一十一、Phase V2-C — Large World V2

內容：

```text
Adaptive Partition
Hierarchical Cell Group
3D Volume Partition
World Origin Rebasing
HLOD V2
Impostor Builder
Cell Persistent State
World Partition Commandlets
Distributed HLOD Build
```

Gate：

```text
Large test world streams for hours without cell leak
Origin rebasing does not visibly move gameplay world
Cell unload/reload preserves persistent gameplay delta
HLOD never leaves visible empty hole during switch
Partition build is deterministic
Changed region can rebuild without full world rebuild
```

---

# 一百一十二、Phase V2-D — Networking

內容：

```text
Transport
Handshake
NetworkEntityID
Replication Schema
Snapshots
Interest
Dormancy
Prediction
Reconciliation
Dedicated Server
Replay
```

Gate：

```text
Client / Server can run different local EntityID mappings
Protocol mismatch rejects cleanly
Packet loss / jitter simulation supported
Character prediction remains playable under test latency
Server headless build contains no renderer dependency
Interest management prevents global replication
Replay can reproduce network session state sufficiently for debugging
```

---

# 一百一十三、Phase V2-E — AI / Navigation / Animation

內容：

```text
Hierarchical Nav
Query Scheduler
Crowd
Utility AI
Perception LOD
Influence Field
Learned Policy API
Training Bridge
Motion Warping
Inertialization
Pose Search
GPU Pose Sampling
```

Gate：

```text
Thousands of AI do not synchronously pathfind in one frame
Far AI automatically reduces simulation cost
Crowd drives CharacterIntent, not transforms directly
Training simulation can run headless multiple worlds
GPU crowd animation does not become gameplay event authority
```

---

# 一百一十四、Phase V2-F — Runtime Feature Expansion

內容：

```text
Timeline / Cinematic
Camera Rig
Flex / Grid UI
RichText
StyleSheet
Surface UI
Accessibility foundation
Room / Portal Audio
GPU Cloth production
```

Gate：

```text
Timeline can scrub in Editor
Camera / Animation / Audio tracks remain synchronized
Surface UI receives correct UV-mapped input
RichText uses existing localization / shaping pipeline
Audio portal routing works without per-source expensive raycast requirement
```

---

# 一百一十五、Phase V2-G — Distributed Build / LiveOps

內容：

```text
Shared DDC
Distributed Cooker
Distributed Shader Build
Distributed HLOD
Patch Manifest
Data Overlay
Localization Pack
Remote Content Validation
```

Gate：

```text
Remote DDC outage does not stop local development
Two build machines produce matching artifact hash for same input
Patch cannot activate before full validation
Old generation remains usable until refs drain
Remote content cannot introduce executable native code
```

---

# 一百一十六、Phase V2-H — Hardening

內容：

```text
Long-run streaming soak
Network soak
Memory pressure
Device loss
Thermal tests
Save corruption tests
Patch rollback
Server scale
Editor large-project scale
```

Gate：

```text
24h+ streaming soak without unbounded residency growth
Network disconnect/reconnect leaves no leaked entity mapping
Crash report includes build / module / trace metadata
Patch rollback returns to known valid content
Mobile thermal policy behaves deterministically by profile
```

---

# 一百一十七、V2 Reference Test Projects

V2 不只靠 unit test。

至少建立以下 internal reference projects。

## Project A — Massive Outdoor

```text
Large Terrain
Vegetation
Town
HLOD
Adaptive Streaming
Vehicle-speed traversal
```

驗證：

```text
GPU Driven
VT
World Partition
HLOD
Origin
Streaming
```

## Project B — Indoor Dungeon

```text
Room / Portal
Multiple floors
Dynamic doors
Nav
Audio portal
AI
```

驗證：

```text
Portal streaming
Portal visibility
Audio
Hierarchical path
```

## Project C — Network Arena

```text
Dedicated Server
16~64 clients test bots
Prediction
Snapshot
Replay
```

驗證 Networking。

## Project D — Crowd City

```text
Thousands of NPC
Crowd
AI LOD
GPU Animation
Utility AI
```

## Project E — Mobile Stress

```text
Android / iOS
Thermal
Memory Pressure
Touch UI
Streaming
GPU budget
```

---

# 一百一十八、V2 Performance Targets

實際數值由硬體 profile 決定，不將單一數字寫死成全平台 Contract。

但 V2 必須建立 benchmark class。

例如 Desktop Reference：

```text
Large visible instance count
High draw-source count
Thousands of animated agents
Large streaming world
```

Mobile Reference：

```text
Thermal-stable sustained session
Memory budget respected
No emergency allocation storm
```

每個 benchmark 必須紀錄：

```text
Median
P95
P99
Peak RAM
Peak VRAM
IO Peak
Job Queue Stall
GPU Bubble
```

---

# 一百一十九、V2 CI Gates

新增：

```text
GPU Driven CPU fallback comparison
Partition deterministic build hash
HLOD deterministic hash
Network schema compatibility
Packet fuzz / malformed packet test
Save / patch corruption recovery
DDC deterministic artifact
Reflection generator deterministic output
Timeline serialization
Surface UI hit-test regression
Character Streaming Boundary under Adaptive/3D Partition + Origin Rebasing
```

---

# 一百二十、V2 Fuzz / Robustness

對不可信輸入：

```text
Network Packet
Save File
Remote Manifest
Data Overlay
WebView Message
```

建立：

```text
Fuzz Test
Size Limit
Malformed Data Test
Timeout
```

避免 parser / decoder 成為 crash surface。

---

# 一百二十一、V2 Memory Model

V2 增加大量 GPU / Network / Streaming 系統，但不因此回到 uncontrolled allocation。

Memory Tags：

```text
World
Scene
Streaming
GPUScene
Animation
Network
AI
Navigation
UI
Audio
DDC
Editor
```

Profiler 可看：

```text
Current
Peak
Lifetime
Allocation Count
Fragmentation Estimate
```

GPU-driven temporary buffers：

```text
Frame / Ring / Pool
```

不用 per-frame heap new/delete。

---

# 一百二十二、V2 Network Memory

Packet / Snapshot 使用：

```text
Packet Pool
Snapshot Arena
Bitstream Buffer Pool
```

Network thread 不把 arbitrary STL object graph 跨 thread 傳給 gameplay。

使用：

```text
POD Message
Handle
Batch
```

---

# 一百二十三、V2 AI Memory

Blackboard / BT / Utility Runtime：

```text
Data-oriented Instance State
```

Static tree / utility definition：

```text
Shared immutable asset
```

每個 NPC 只保存：

```text
Runtime state
Blackboard values
Active node / action
```

不 clone 整棵 graph object。

---

# 一百二十四、V2 Timeline Memory

Timeline Asset immutable。

TimelinePlayer 保存：

```text
Current Time
Binding Table
Track Runtime State
Active Section Cache
```

不複製所有 keyframe data。

---

# 一百二十五、V2 Threading

新增 thread / task 類型不等於新增固定 OS Thread。

仍使用：

```text
Job System
IO Service
Render Thread / Backend needs
Audio Thread
Network IO thread/service
```

AI / Nav / HLOD runtime work：

```text
Jobs
```

而不是：

```text
AI Thread
Nav Thread
Streaming Thread
```

各一條固定 thread。

---

# 一百二十六、V2 Server Tick

Dedicated Server：

```text
Fixed Simulation Tick
```

可與 client render 完全分離。

Server 無：

```text
Render interpolation
GPU presentation
```

可配置：

```text
Tick Rate
Max Catch-up
Network Snapshot Rate
AI Budget
Physics Budget
```

---

# 一百二十七、V2 Multi-World Server

為 AI training / server instance 預留：

```text
Process
├─ World A
├─ World B
├─ World C
└─ ...
```

每個 World：

```text
Own Entity Registry
PhysicsWorld
NavigationWorld
Gameplay state
Time
```

共用：

```text
Immutable Assets
Data Tables
Global code
Optional resource cache
```

這同時支援：

```text
Dungeon instances
Self-play simulation
Server instance hosting
```

---

# 一百二十八、V2 MMO Boundary

V2 Networking 可以做：

```text
Online RPG
Co-op
MOBA
Arena
Session-based game
Moderate persistent world server
```

但：

```text
Global seamless MMO shard
Cross-server handoff
Distributed authoritative simulation
Massive persistence cluster
```

不列 V2 Core Definition of Done。

只預留：

```text
World Partition server hooks
NetworkEntity identity
Session transfer hooks
Persistent backend adapter
```

避免過度設計。

---

# 一百二十九、V2 Database / Backend Boundary

Engine 不內建遊戲後端 database ORM。

提供：

```text
IGamePersistenceBackend
```

Server gameplay 可接：

```text
Custom Service
SQL service layer
Cloud backend
```

但 Engine Core 不直接讓 gameplay thread 執行 blocking SQL query。

---

# 一百三十、V2 Asset Security

Remote Bundle：

```text
Manifest
Hash
Signature
Size
Dependency
Version
```

驗證完成才進：

```text
Verified Cache
```

再：

```text
Atomic Activate
```

壞 Patch：

```text
Rollback
```

與現有 generation pinning 完整整合。

---

# 一百三十一、V2 Build Profiles

Build Profile 增加：

```text
Client
DedicatedServer
Editor
Tool
Benchmark
TrainingHeadless
```

每個 Profile 能做：

```text
Module Strip
Asset Strip
Shader Strip
Platform Feature
Logging Policy
Telemetry Policy
```

---

# 一百三十二、Training Headless Profile

```text
TrainingHeadless
```

移除：

```text
Renderer
Runtime UI
Audio Output
Expensive presentation
```

保留：

```text
Gameplay
Physics if needed
Navigation
AI
Data
Replay / Metrics
```

可：

```text
Run faster than realtime
```

前提 gameplay logic 不依賴 wall clock。

---

# 一百三十三、V2 Determinism Policy

Engine 不宣稱：

```text
All systems deterministic
```

而定義：

```text
Deterministic-capable subsystem
```

例如：

```text
DataTable
Gameplay RNG Service
Timer Tick
Selected AI logic
Rollback island
```

不可控：

```text
GPU Simulation
Rendering
Audio
General floating physics cross-platform
```

明確區分。

---

# 一百三十四、V2 Random Service

建立：

```text
RandomStream
```

Seed：

```text
World Seed
System Seed
Entity / Gameplay Seed
```

支援：

```text
Replay
AI Training
Test Reproduction
```

Gameplay 不應大量使用 process-global `rand()`。

---

# 一百三十五、V2 Testing API

Headless Test World：

```text
CreateWorld()
LoadScene()
StepFixedTicks(N)
InjectInput()
QueryState()
CaptureSnapshot()
DestroyWorld()
```

用於：

```text
Gameplay regression
Network prediction
AI
Save
Scene lifecycle
```

---

# 一百三十六、V2 Editor Automation

提供：

```text
EditorAutomation API
```

能力：

```text
Open Project
Open Scene
Create Entity
Set Property
Run Import
Build
Capture Screenshot
Validate
Run Play Test
```

可供：

```text
CI
Internal Tool
AI coding agent
Test harness
```

但權限明確，不暴露 unrestricted OS operations 給 runtime content。

---

# 一百三十七、V2 AI-Assisted Tooling Boundary

引擎可以允許 AI 工具：

```text
Read Scene Metadata
Read Asset Metadata
Create Editor Command
Generate Config
Run Validation
```

但 AI 不直接修改 private engine memory。

所有 mutation 走：

```text
Editor Command / Transaction
```

因此：

```text
Undoable
Auditable
Validatable
```

---

# 一百三十八、V2 Documentation Generation

Reflection / Settings / Console / Plugin metadata 可生成：

```text
API Reference
Property Reference
CVar Reference
Project Settings Reference
```

避免文件與 runtime schema 完全手動同步。

---

# 一百三十九、V2 Deprecation Policy

API：

```text
Deprecated
↓
Warning
↓
Migration Guide
↓
Removal no earlier than planned major boundary
```

Asset schema：

```text
Migration function
```

C ABI：

```text
Versioned function table
```

不做 silent behavior change。

---

# 一百四十、V2 Non-Goals

V2 明確不做為核心要求：

```text
❌ Full Unreal-style UObject ecosystem
❌ Full Unity-compatible API
❌ GPU-authoritative gameplay physics
❌ Mandatory ray tracing
❌ Mandatory Mesh Shader
❌ Entire-engine deterministic rollback
❌ Built-in commercial cloud platform lock-in
❌ Native executable remote hot update
❌ Full browser engine / Chromium replacement
❌ Built-in ML training framework
❌ Global distributed MMO server mesh
❌ Mandatory visual scripting language
```

Visual scripting 若未來需要：

```text
Plugin / Future
```

---

# 一百四十一、V2 Definition of Done

V2.0 可以正式稱為完成，至少必須：

```text
1. V1 Project 可經 migration 升級並重新 cook。
2. GPU-driven renderer 在目標 desktop backend production usable。
3. Mobile 可使用 compatible reduced path，不要求所有高階 GPU feature。
4. Large World V2 可長時間無接縫 streaming。
5. Adaptive partition / HLOD build deterministic。
6. World origin shift 對 gameplay identity 不可見。
7. Dedicated server 可 headless build。
8. Snapshot replication / interest / prediction production usable。
9. Character prediction 與 reconciliation 有 reference implementation。
10. Navigation hierarchical query / crowd production usable。
11. AI LOD / Utility AI production usable。
12. Compute skinning / GPU crowd path production usable。
13. Timeline / cinematic 可完整 author / preview / runtime。
14. Shared DDC 與 headless cook 可用於 CI。
15. Scene / Prefab structural diff / merge 可使用。
16. Remote device profiling 可追 CPU/GPU/IO/Streaming/Network。
17. Asset patch / Data overlay 有 rollback。
18. No subsystem bypasses established lifetime / RenderGraph / ABI contracts。
19. V2 reference projects 全部通過 CI / soak。
20. Documentation / migration / profiler / crash diagnostics 可支援實際 production。
```

---

# 一百四十二、最終 V2 架構摘要

```text
                              Engine V2
                                  │
 ┌────────────────────────────────┼─────────────────────────────────┐
 │                                │                                 │
 ▼                                ▼                                 ▼
World Runtime                 GPU Runtime                       Toolchain
 │                                │                                 │
 ├─ Multi Scene                   ├─ GPUScene                       ├─ Clang Reflection
 ├─ Adaptive Partition            ├─ Compute Culling                ├─ Shared DDC
 ├─ HLOD V2                       ├─ Hi-Z                           ├─ Distributed Cook
 ├─ Origin Rebasing               ├─ GPU LOD                        ├─ HLOD Builder
 ├─ Persistent Cell State         ├─ Indirect Draw                  ├─ Scene Diff
 │                                ├─ Compute Skinning               └─ Commandlets
 │                                └─ Async Compute
 │
 ├─ PhysicsWorld
 ├─ NavigationWorld
 │   ├─ Hierarchical Nav
 │   └─ Crowd
 │
 ├─ AI
 │   ├─ BT
 │   ├─ Utility
 │   ├─ Policy Interface
 │   └─ Self-play Bridge
 │
 ├─ Networking
 │   ├─ Transport
 │   ├─ Snapshot
 │   ├─ Interest
 │   ├─ Prediction
 │   └─ Dedicated Server
 │
 ├─ Animation
 │   ├─ Pose Search
 │   ├─ Motion Warp
 │   └─ GPU Crowd
 │
 ├─ Runtime UI V2
 │   ├─ Flex / Grid
 │   ├─ RichText
 │   └─ Surface UI
 │
 └─ Timeline / Cinematic
```

---

# 一百四十三、V1 → V2 思想上的差別

V1：

```text
「引擎能不能完整做出遊戲？」
```

V2：

```text
「同一套架構能不能在更大的世界、更大的內容量、
更多角色、更大的團隊、多人伺服器與長期營運下仍然成立？」
```

所以 V2 的核心不是單純多加效果。

真正的 V2 是：

```text
Scale
+
Automation
+
Parallelism
+
Network
+
Large World
+
Production Reliability
```

---

# 一百四十四、V2 最優先開發順序

若 V1.0 已穩定，我建議實作優先序：

```text
1. Clang Reflection + DDC + Headless Tooling
↓
2. GPUScene + Compute Culling + Indirect
↓
3. HLOD V2 + Adaptive World Partition
↓
4. World Origin + Persistent Cell State
↓
5. Dedicated Server + Transport
↓
6. Snapshot / Interest / Prediction
↓
7. Hierarchical Navigation + Crowd
↓
8. AI LOD + Utility AI
↓
9. Compute Skinning + GPU Pose
↓
10. Timeline / Camera Rig
↓
11. UI V2 / Surface UI
↓
12. Distributed Build + LiveOps
↓
13. Production Hardening
```

原因：

```text
Toolchain / Metadata
```

要先穩，

後面的：

```text
Networking
Large World
Distributed Build
```

才不會建立在不穩定 schema 上。

---

# 一百四十五、V2 最終設計原則

V2 仍遵循整個引擎最重要的方向：

```text
Engine 提供 Capability
而不是強迫 Gameplay Workflow。
```

```text
High-level Framework
→ Optional

Low-level Control
→ Always available
```

```text
Editor 可很方便
Runtime 必須保持 Data-Oriented / Budgeted / Explicit
```

```text
Small Game
→ 不需要支付 Large World / Network / GPU Crowd 的成本

Large Game
→ 不需要換另一套 Engine Architecture
```

因此同一套 Engine V2 可以：

```text
Small RPG
→ 1 World / Few Scenes / CPU submission path

Action RPG
→ Streaming / HLOD / advanced animation

Open World
→ Adaptive Partition / GPU Driven / HLOD V2

MOBA
→ Dedicated Server / Prediction / AI

Strategy / Simulation
→ Crowd / Utility AI / Influence Field

Online RPG
→ Snapshot / Interest / Persistent World State

Self-play Project
→ Headless Multi-World / Policy Bridge / Replay
```

不需要為每個遊戲類型建立另一套核心。


---


# V2 Media / Video Expansion

V1 已完成：

```text
Local Video Playback
MP4 / H.264 / AAC portable profile
Platform Hardware Decode
VideoTexture
UI.VideoElement
World-space Video
Subtitle
A/V Sync
VFS streaming
```

V2 將 Media Framework 擴展為可選的 Streaming / Capture / Encode / DRM 能力。

## Media Plugin 結構

```text
Media.Core
Media.Video

Media.Streaming.Core
Media.Streaming.HLS
Media.Streaming.DASH

Media.DRM.Core
Media.DRM.Widevine          ← provider / platform dependent
Media.DRM.FairPlay          ← provider / platform dependent
Media.DRM.PlayReady         ← provider / platform dependent

Media.Capture
Media.Encoder
Media.Compositor

Media.WebRTC                ← Optional
Media.Editor
```

全部為 Optional；一般遊戲只需要 V1 `Media.Video`。

## Adaptive Streaming

正式模型：

```text
Manifest
↓
Variant / Representation Set
↓
ABR Controller
↓
Segment Scheduler
↓
HTTP Cache
↓
Demux
↓
Decoder
↓
VideoTexture / Audio.Core
```

支援：

```text
HLS
MPEG-DASH
```

ABR 輸入：

```text
Measured Throughput
Buffer Duration
Decode Capability
Display Size
Thermal / Performance Profile
User Quality Policy
```

輸出：

```text
Selected Representation
```

禁止單純用最近一次下載速度立即上下跳畫質。

至少使用：

```text
Throughput Smoothing
Buffer Safety Margin
Minimum Hold Time
Hysteresis
```

## Media Network Cache

```text
Segment Cache
├─ Memory Tier
└─ Disk Tier optional
```

Cache Key：

```text
URL / Content ID
Range
ETag / Version
DRM state where applicable
```

不與 Asset Bundle Cache 混為同一 semantic ownership，但可共用 VFS / disk quota infrastructure。

## DRM Boundary

`Media.DRM.Core` 只定義：

```text
License Request
Session
Key Status
Secure Decode Requirement
Error Translation
```

實際 Widevine / FairPlay / PlayReady 全部 Provider Plugin。

Engine 不自行實作 DRM crypto protocol。

## Capture

`Media.Capture` 可提供：

```text
Camera Capture
Microphone Capture integration
Screen / RenderTarget Capture
```

Capture frame：

```text
CaptureSource
↓
Frame Queue
↓
Optional Processing
↓
Encoder / Gameplay Consumer
```

不允許 Capture callback 每 frame跨 Stable ABI 送大型 heap object。

使用：

```text
FrameHandle
PlaneView
Timestamp
```

## Encoder

`Media.Encoder`：

```text
VideoFrame
+
Audio PCM
↓
Encoder
↓
Container Writer
```

優先使用平台硬體 encoder。

用途：

```text
Replay Export
User Recording
UGC Capture
Tooling
```

V2 不要求所有平台都具有相同 codec encoder。

## Media Compositor

Optional：

```text
Media.Compositor
```

用途：

```text
Video + UI overlay
Video + Camera
Multi-video
Subtitle burn-in for export
Transition
```

Realtime game presentation 仍優先由 Renderer / UI composition，不要求所有畫面先經 Media Compositor。

## WebRTC

```text
Media.WebRTC
△ Optional Provider
```

用於：

```text
Low-latency remote video
Remote tool
Cloud rendering experiment
Voice/video communication
```

不列 V2 Core DoD。

## V2 Media DoD

```text
✓ HLS / DASH framework 可透過 Plugin 啟用
✓ ABR 有 buffer-aware policy
✓ DRM 為 Provider Plugin
✓ Video Capture / Encoder 可完全從一般 client strip
✓ Streaming / DRM 不改 V1 VideoPlayer 基本 local-playback contract
✓ Media cache 可被 profiler / quota manager 觀察
✓ Headless / Dedicated Server 預設不帶 Media
```


---

# V2 Plugin / Third-party SDK 詳細 Contract（Normative）

# 跨平台 3D Engine — V2 Plugin / Feature Module 模組化規劃

**文件版本：Draft v1.1**  
**對應 Engine 世代：V2.x**  
**前提：沿用 V1 Plugin Contract，不改核心規則。**

---

# 一、V2 模組化目標

V2 增加大量 Production / Large World / Networking / GPU 高階功能。

因此核心原則更重要：

```text
大型 Production 能力
≠
所有遊戲都必須攜帶
```

V2 要做到：

```text
Small Game
→ 不支付 Networking / HLOD V2 / Vendor SDK / ML / Distributed Build 成本

Large Game
→ Enable 所需 Feature Plugin
```

---

# 二、V2 仍沿用的 V1 Contract

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

Development：

```text
Dynamic / Modular
```

Shipping：

```text
Selected Plugins only
→ Static / Monolithic allowed
→ LTO / WPO
```

---

# 三、V2 Core 不新增重量級 Optional 功能

V2 Core 仍只保持：

```text
EngineCore
World / Scene
Entity / Transform
Memory
Job / Task Graph
Reflection Core
Serialization Core
Asset / VFS Core
Platform Core
Plugin Manager
RHI Core
RenderGraph Core
```

Networking、ML、RT、Timeline 等不升成永遠存在的 Core。

---

# 四、Networking Plugins

```text
Network.Core
Network.Transport.UDP
Network.Replication
Network.Interest
Network.Prediction
Network.Replay
```

單機遊戲：

```text
Network.*
→ 全部 strip
```

多人遊戲可依需求只選：

```text
Network.Core
Network.Transport.UDP
Network.Replication
```

不一定要 Prediction / Replay。

---

# 五、Dedicated Server

不是普通 runtime feature，而是：

```text
BuildProfile.DedicatedServer
```

典型：

```text
Include:
World
Scene
Physics
Navigation
Gameplay
Networking
DataTable
Persistence

Strip:
Renderer
Runtime UI
Audio
VFX
Texture-heavy content
PostProcess
WebView
```

---

# 六、Online Provider Plugins

```text
Online.Core
Online.Steam
Online.EOS
Online.PlayFab
Online.Custom
```

只編使用的 provider。

Engine Core 不綁任何 commercial service。

---

# 七、Voice Chat Plugin

```text
VoiceChat.Core
VoiceChat.Provider.*
```

完全 Optional。

不使用：

```text
codec
capture
network voice
platform voice SDK
```

全部不進包。

---

# 八、Cloud Save Plugins

```text
CloudSave.Core
CloudSave.Provider.*
```

可獨立於 Local Save。

---

# 九、Temporal Upscaler Plugins

```text
Upscaler.Core
Upscaler.EngineTAAU
Upscaler.FSR
Upscaler.DLSS
Upscaler.XeSS
```

專案可：

```text
EngineTAAU only
```

不需要 vendor SDK。

---

# 十、Virtual Texturing Plugins

```text
RenderFeature.VirtualTexture
Terrain.VirtualTexture
Material.VirtualTexture
```

可分層。

例如：

```text
Terrain VT
✓

General Material VT
✕
```

---

# 十一、Mesh Shader Plugin

```text
RenderFeature.MeshShader
```

Capability-dependent。

不支援平台：

```text
完全不編譯
```

Meshlet Asset 仍可走：

```text
Indexed Indirect fallback
```

---

# 十二、GPU-Driven Renderer Features

可拆：

```text
RenderFeature.GPUScene
RenderFeature.GPUCulling
RenderFeature.HiZ
RenderFeature.Indirect
RenderFeature.AsyncCompute
```

但若專案選：

```text
GPUDrivenRenderer
```

建議由 meta-plugin 拉進必需 dependency。

例如：

```text
RenderFeature.GPUDriven
depends:
→ GPUScene
→ GPUCulling
→ Indirect
```

---

# 十三、Compute Skinning Plugin

```text
Animation.ComputeSkinning
```

角色少的遊戲：

```text
Vertex Skinning only
```

可以不加入。

---

# 十四、GPU Pose Sampling Plugin

```text
Animation.GPUPose
```

大量 crowd 才需要。

一般 RPG：

```text
Optional
```

---

# 十五、Pose Search / Motion Matching Plugins

```text
Animation.PoseSearch
Animation.MotionMatching
```

Motion Matching 依賴：

```text
Animation.PoseSearch
```

不使用 Motion Matching 的專案完全不必攜帶 Pose DB runtime / editor tooling。

---

# 十六、GPU Cloth Plugin

```text
Physics.GPUCloth
```

依賴：

```text
Physics.Core
RenderGraph
```

不用 cloth：

```text
strip
```

---

# 十七、Destruction Plugin

```text
Physics.Destruction
Physics.FractureEditor
```

Runtime / Editor 分離。

Fracture editor：

```text
Editor-only
```

---

# 十八、Timeline / Cinematic Plugins

```text
Timeline.Core
Timeline.Camera
Timeline.Animation
Timeline.Audio
Timeline.UI
Timeline.Editor
```

純 gameplay project 可完全不加入。

若只需要簡單 camera cinematic：

```text
Timeline.Core
Timeline.Camera
```

即可。

---

# 十九、Camera Rig Plugin

```text
Camera.Rig
Camera.Cinematic
```

不讓 Camera Core 綁死複雜 cinematic。

---

# 二十、Surface UI Plugin

```text
UI.Surface
```

用途：

```text
3D Monitor
Curved Screen
Vehicle Dashboard
World Terminal
```

一般 Screen UI 不需要。

---

# 二十一、Advanced UI Plugins

```text
UI.Flex
UI.AdvancedGrid
UI.RichText
UI.StyleSheet
UI.Accessibility
```

`UI.Runtime` 可存在但不必攜帶全部高階功能。

---

# 二十二、Offscreen WebView Plugin

```text
UI.WebView.Offscreen
```

獨立於：

```text
UI.WebView.NativeOverlay
```

平台不支援：

```text
Capability = Unsupported
```

不做高成本 fake fallback。

---

# 二十三、AI Utility Plugin

```text
AI.Utility
```

Optional。

與：

```text
AI.BehaviorTree
```

可並存。

---

# 二十四、GOAP Plugin

```text
AI.GOAP
```

Optional。

不讓所有 AI 專案都承擔 planner cost。

---

# 二十五、ML Policy Runtime Plugins

```text
AI.Policy
AI.Policy.ONNX
```

只有使用 learned policy 才加入。

---

# 二十六、Self-play Training Bridge

```text
AI.TrainingBridge
```

僅：

```text
Editor
TrainingHeadless
Tool
```

Shipping Client：

```text
strip
```

---

# 二十七、Shared DDC Plugin

```text
Tool.DDC.Remote
```

僅：

```text
Editor
CI
Cooker
Build Farm
```

Shipping 0 byte。

---

# 二十八、Distributed Build Plugins

```text
Tool.DistributedBuild
Tool.ShaderWorker
Tool.HLODWorker
Tool.CookWorker
```

全部 Tool-only。

---

# 二十九、Source Control Plugins

```text
Editor.SourceControl.Git
Editor.SourceControl.Perforce
```

只載需要的 provider。

---

# 三十、Scene Externalization Plugin

```text
Editor.SceneExternalization
```

Authoring-only。

Cook：

```text
External Entity Files
↓
Runtime Cell Blob
```

Shipping 不需要外部 authoring parser。

---

# 三十一、World Partition V2 Modules

建議拆：

```text
WorldPartition.Core
WorldPartition.AdaptiveQuadtree
WorldPartition.Volume3D
WorldPartition.HLOD
WorldPartition.Impostor
WorldPartition.Editor
WorldPartition.Commandlet
```

小型遊戲：

```text
WorldPartition.*
→ 可以全部不加入
```

一般大型地圖：

```text
Core
HLOD
```

真正需要 adaptive partition 才加入：

```text
AdaptiveQuadtree
```

---

# 三十二、Origin Rebasing Plugin

```text
World.LargeCoordinate
World.OriginRebase
```

LargeCoordinate foundation 可是 Core-compatible capability。

真正 Rebasing：

```text
Optional Feature
```

小地圖不需要。

---

# 三十三、HLOD V2 Plugins

```text
HLOD.Core
HLOD.MeshMerge
HLOD.TextureBake
HLOD.Impostor
HLOD.Editor
HLOD.Worker
```

Runtime 只需：

```text
HLOD.Core
```

Builder：

```text
Editor / Cooker / Worker only
```

---

# 三十四、Advanced Navigation Plugins

```text
Navigation.Hierarchical
Navigation.Crowd
Navigation.InfluenceField
```

可獨立。

例如 RPG：

```text
Hierarchical + Crowd
```

策略遊戲可能：

```text
InfluenceField
```

---

# 三十五、Profiler / Remote Tools

```text
Tool.RemoteInspector
Tool.RemoteProfiler
Tool.NetworkProfiler
Tool.StreamingProfiler
```

Development-only。

Shipping：

```text
strip
or
secure restricted diagnostics mode
```

---

# 三十六、DataTable V2 Modules

```text
DataTable.BinaryRuntime
DataTable.MemoryMapped
DataTable.LiveOverlay
```

不需要 LiveOps：

```text
LiveOverlay
→ strip
```

---

# 三十七、LiveOps Modules

```text
LiveOps.Manifest
LiveOps.BundlePatch
LiveOps.DataOverlay
LiveOps.LocalizationPack
```

單機 boxed product 可完全不加入。

---

# 三十八、Plugin 與 Shader Cook

V2 尤其重要：

```text
Enabled Render Plugin
+
Used Material Feature
+
Target GPU Capability
↓
Shader Variant Set
```

例如未啟用：

```text
MeshShader
RayTracing
VirtualTexture
DLSS
```

相關 shader entry / PSO 不應出現在 build。

---

# 三十九、Plugin 與 Backend SDK

第三方 SDK 只跟對應 Plugin 走。

例如：

```text
DLSS SDK
→ Upscaler.DLSS

FMOD SDK
→ Audio.FMOD

EOS SDK
→ Online.EOS
```

Project 沒選就完全不下載 / 不編譯也應可行。

---

# 四十、V2 Build Profiles

```text
Editor
DesktopClient
MobileClient
DedicatedServer
Benchmark
TrainingHeadless
Cooker
BuildWorker
```

每個 profile 可有不同 Plugin set。

---

# 四十一、V2 常見組合

### Open World RPG

```text
Physics.Jolt
Character
Navigation.RecastDetour
Navigation.Hierarchical
AI.BehaviorTree
AI.Utility optional
Audio.Miniaudio
UI.Runtime
Terrain
Vegetation
VFX
WorldPartition.Core
WorldPartition.HLOD
World.OriginRebase
RenderFeature.GPUDriven
Animation.ComputeSkinning
```

### MOBA Client

```text
Physics.Jolt
Character
Navigation
Audio
UI.Runtime
VFX
Network.Core
Network.Transport.UDP
Network.Replication
Network.Prediction
Network.Replay
```

### Dedicated Server

```text
World
Scene
Physics
Navigation
AI
Gameplay
DataTable
Network.*
Persistence
```

Strip：

```text
Renderer
UI
Audio
VFX
PostProcess
WebView
```

---

# 四十二、V2 Definition of Done

```text
1. Networking 可完全從單機 build 移除。
2. Vendor upscaler SDK 僅在對應 plugin 啟用時存在。
3. HLOD builder / DDC / distributed worker 不進 shipping client。
4. DedicatedServer profile 不依賴 Renderer / UI / Audio。
5. GPU Driven feature 可以以 meta-plugin 開啟。
6. Motion Matching / GOAP / ML Policy 可以獨立開關。
7. World Partition V2 不強迫小地圖 project 攜帶 adaptive partition。
8. Disabled plugin shader family 不進 cook。
9. Provider SDK 完全由 provider plugin ownership。
10. Project.lock 可固定 plugin / provider version。
```

---


# 四十四、V2 Third-party Plugin SDK 擴充

V2 沿用 V1：

```text
PluginHost
Stable C ABI
Service Registry
Extension Registry
Bridge Plugin
```

並增加大型 Production Extension Point。

---

# 四十五、Render Feature Plugin SDK

第三方可註冊：

```text
Render Feature
RenderGraph Pass Factory
GPU Resource Declaration
Shader Family
Material Extension
Debug Visualization
```

流程：

```text
Third-party Render Plugin
↓
Register Pass Factory
↓
RenderGraph
↓
RHI
```

禁止：

```text
Plugin
→ arbitrary vkCmd*
→ arbitrary ID3D12GraphicsCommandList*
```

繞過 RenderGraph。

RenderGraph 仍掌握：

```text
Barrier
Lifetime
Aliasing
Queue
Dependency
Synchronization
```

---

# 四十六、Custom Navigation Backend

V2 可提供：

```text
INavigationBackend
```

第三方可實作：

```text
Grid Navigation
Voxel Navigation
Flying Navigation
Custom Crowd Navigation
```

Public boundary：

```text
NavHandle
QueryBatch
POD Result
```

不暴露 Recast native pointer。

---

# 四十七、Custom AI Decision Plugin

Decision Model 可獨立擴充：

```text
AI.BehaviorTree
AI.Utility
AI.GOAP
AI.Policy
AI.CustomDecision
```

只需接：

```text
Blackboard
Perception Snapshot
Action Interface
CharacterIntent / Gameplay Command
```

Engine 不要求所有 AI 都走同一 graph。

---

# 四十八、Timeline Track Plugin

第三方可註冊：

```text
Custom Timeline Track
Custom Clip
Custom Binding Resolver
Custom Editor Drawer
```

例如：

```text
Weather Track
Quest Track
Dialogue Track
Camera Lens Track
Custom Gameplay Parameter Track
```

Track evaluation 不持有 raw Engine pointer。

---

# 四十九、Network / Online Provider Bridge

V2 正式允許：

```text
Custom Transport
Custom Online Provider
Custom Matchmaking
Custom Lobby
Custom Voice
Custom Cloud Save
```

透過：

```text
Provider Service API
```

而不是修改 Network Core。

---

# 五十、Distributed Build Worker Plugin

Build Farm 可註冊：

```text
Custom Build Task
Custom Worker Capability
Custom Artifact Validator
```

例如：

```text
World Generator
Custom Nav Bake
Proprietary Texture Cooker
ML Data Preprocessor
```

Worker 只輸出 artifact，不直接修改 authoritative Asset DB。

---

# 五十一、Remote Tool Plugin

Editor 可以讓第三方新增：

```text
Remote Device Panel
Network Debug Panel
Custom Profiler Track
Server Console Panel
Streaming Visualization
```

使用 Unified Trace / Remote Inspector Public API。

---

# 五十二、Plugin Package / Dependency Lock

V2 Project Package System 可管理第三方 Plugin：

```text
Plugin Package
├─ Manifest
├─ Binary / Source
├─ Editor Part
├─ Runtime Part
├─ ThirdParty
└─ License Metadata
```

`Project.lock` 固定：

```text
Plugin Version
Provider Version
Dependency Version
```

CI 與開發機必須解析成相同 dependency graph。

---

# 五十三、Plugin Build Variants

同一 Plugin 可提供：

```text
Editor
DesktopClient
MobileClient
DedicatedServer
TrainingHeadless
```

不同 implementation / stripping。

例如：

```text
MyOnlinePlugin
├─ Client
└─ Server
```

不要求所有 profile 使用同一 binary。

---

# 五十四、Plugin Profiling / Cost Attribution

Profiler 新增：

```text
PluginID
```

作為 trace dimension。

可統計：

```text
CPU Time
Jobs
Memory
GPU Pass
IO
Network
Shader Families
Asset Residency
```

因此可以回答：

```text
這個第三方 Plugin 實際成本多少？
```

---

# 五十五、V2 Third-party Plugin Gate

額外 Gate：

```text
11. Third-party Render Feature 可透過 RenderGraph extension 接入。
12. Custom Navigation Backend 可替換 Recast 而不改 Gameplay API。
13. Custom AI Decision Model 可接同一 Perception / Blackboard / Action framework。
14. Timeline 可載入第三方 Track。
15. Online / Matchmaking / Voice / Cloud provider 可用 Bridge Plugin。
16. Build Worker 可註冊 custom cooker task。
17. Project.lock 可固定第三方 Plugin dependency。
18. Profiler 可依 PluginID 追 CPU / memory / GPU / IO 成本。
19. DedicatedServer / Client 可對同一 Plugin 使用不同 build variant。
20. Third-party Plugin 不能繞過 RenderGraph / ABI / lifetime contract。
```


# 四十三、V2 最終原則

```text
Production Scale
→ Optional
```

```text
Multiplayer
→ Optional
```

```text
Large World
→ Optional
```

```text
Vendor SDK
→ Optional
```

```text
Toolchain Feature
→ Editor / Worker only
```

V2 越強，越不能讓所有遊戲一起背成本。


---

# V2 Complete Scope / Gate

V2 在 V1 已完成的基礎上，正式增加：

```text
Renderer
✓ GPUScene
✓ Compute Culling
✓ Hi-Z
✓ GPU LOD
✓ Indirect Draw
✓ Async Compute
✓ Compute Skinning
✓ Temporal Upscaler Interface
△ Mesh Shader capability path
✓ Terrain Virtual Texturing
```

```text
Large World
✓ Adaptive Quadtree Cell Generation
✓ Hierarchical Cell Group
✓ 3D Volume Partition
✓ World Origin Rebasing
✓ HLOD V2 / Impostor
✓ Persistent Cell State
✓ Headless World Partition Commandlets
```

```text
Networking
✓ Transport
✓ Connection / Protocol Handshake
✓ Replication Schema
✓ Snapshot / Delta
✓ Interest / Dormancy
✓ Client Prediction / Reconciliation
✓ Dedicated Server
✓ Replay
✓ Lobby / Matchmaking Provider boundary
```

```text
AI / Navigation / Animation
✓ Hierarchical Navigation
✓ Crowd
✓ Query Budget
✓ Utility AI
✓ AI LOD
✓ Influence Field
✓ Learned Policy Interface
✓ Self-play Training Bridge
✓ Motion Warping
✓ Inertialization
✓ Pose Search
△ Motion Matching
```

```text
Production
✓ Clang AST Reflection Generator
✓ Externalized World Entity Files
✓ Scene Diff / Merge
✓ Prefab Rebase / Conflict UI
✓ Content-addressable DDC
✓ Shared DDC
✓ Import Worker
✓ Distributed Cook / Shader / HLOD
✓ Live Data Overlay
✓ Remote Device Inspector
✓ Headless Editor Commandlets
```

```text
Runtime Expansion
✓ Timeline / Cinematic
✓ Camera Rig
✓ UI Flex / Advanced Grid / RichText / StyleSheet
✓ Surface UI
✓ Accessibility foundation
✓ Room / Portal audio
✓ Media Streaming foundation
△ Capture / Encoder by project profile
```

V2 完成後才進 V3。


---

# Appendix — V1 Contract Reference

V1 Master Baseline 不再於本文件內複製全文，避免內容與來源文件分岔、需要人工重複同步。V2 完整繼承 V1 的所有 Architecture Contract（見「二、V2 不改變的核心 Contract」），完整內容請參閱來源文件本身：

```text
《跨平台3D_Engine_V1_完整規劃書》
→ 目前對應 Master Draft v1.2
→ 含 Character Framework 的 Terrain Streaming Boundary Contract

《跨平台3D_Engine_V1_AI施工技術與系統規劃》
→ 目前對應 AI Technical Draft v1.2
```

V1 Plugin / Third-party SDK / Video & Media 詳細 Contract、V1 Master Definition of Done 兩份附錄，同樣請見 V1 文件本身，不在此複製。

閱讀順序建議：施工或審閱本文件前，先確認上述 V1 文件版本是否為最新；本文件（V2）只描述新增與變更的部分，未在本文件提及的既有系統一律以 V1 文件為準。
