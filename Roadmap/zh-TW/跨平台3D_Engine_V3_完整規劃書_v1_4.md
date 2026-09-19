# 跨平台 3D Engine — V3 完整規劃書

**文件版本：Master Draft v1.4**  
**Engine 世代：V3.x — Distributed / Simulation / Next-Gen**

> V3 建立在完整 V1 + V2 上。V3 不把所有新能力變成 Mandatory；它增加的是「可選擇的能力上限」。
>
> V3 主軸：
>
> ```text
> Distributed World / MMO
> Deterministic Simulation Domain
> GPU Simulation
> Ray Tracing / Path Tracing
> Mesh Shader / Work Graph / Cluster Geometry
> ML Training / Self-play
> Massive Multi-World Simulation
> Distributed Production Infrastructure
> ```

---

# 一、V3 不改變的核心哲學

```text
Capability
≠ Mandatory Workflow
```

因此：

```text
V3
≠ 所有平台必須 Ray Tracing
≠ 所有 Physics 都搬到 GPU
≠ 所有 Gameplay 都 deterministic
≠ 所有 Game 都包含 MMO runtime
≠ Shipping Client 包含 ML Trainer
```

仍保持：

```text
C++20 Core
Zig Primary Gameplay
Stable C ABI
Slang canonical shader source
RenderGraph owns GPU dependency
World ≠ Scene ≠ StreamingCell
SceneGraph ≠ PhysicsWorld ≠ NavigationWorld ≠ RenderWorld
Plugin disabled = no shipping cost
```

---

# 二、V3 Scope Matrix

```text
Distributed MMO World                 ✅
Region / Shard Authority              ✅
Cross-Server Entity Handoff           ✅
Global Interest Federation            ✅
Region Failover Foundation            ✅
Rolling Server Deployment             ✅

Deterministic Simulation Domain        ✅
Rollback Domain                        ✅
Lockstep Domain                        ✅
Deterministic Physics Plugin           ✅ optional
Entire Engine Bitwise Determinism      ❌

GPU Physics Domain                     ✅
GPU Broadphase / Constraint Solve      ✅
GPU Authoritative Simulation Island    ✅ explicit-domain only
GPU Fluid                              △
GPU SoftBody                           △
Replace CPU Gameplay Physics Entirely  ❌

Hardware Ray Tracing Framework         ✅
RT Shadow / Reflection / AO            ✅
RT GI                                  △ profile
Path Tracer                            ✅ optional
Ray Tracing Mandatory                  ❌

Mesh Shader First-class Path           ✅
Indexed Indirect Fallback              ✅
Work Graph                             △ capability / experimental
Cluster / Virtual Geometry             ✅ optional

Built-in ML Training Platform          ✅ toolchain
PPO                                    ✅
SAC                                    ✅
DQN                                    ✅
Self-play                              ✅
Curriculum                             ✅
Evaluation / Tournament                ✅
ONNX Runtime                           ✅ optional runtime
PyTorch / JAX Bridge                   ✅ training-only
Trainer in Shipping Client             ❌

Simulation Farm                        ✅
Multi-World Headless                   ✅
Accelerated Simulation Time            ✅

Massive Crowd / Aggregate Simulation   ✅ optional
GPU Navigation / Flow Field            △
Distributed Build / DDC V3             ✅
```

---

# 三、Distributed World Topology

V2 Dedicated Server 通常：

```text
One Server Process
→ One or N Runtime Worlds
→ Session authority
```

V3 MMO：

```text
Global World
        │
        ▼
World Directory
        │
 ┌──────┼─────────────┐
 ▼      ▼             ▼
Region A Region B    Region C
 │        │             │
Server   Server        Server
Node     Node          Node
```

正式：

```text
GlobalWorldID
RegionID
ServerNodeID
NetworkEntityID
PersistentEntityID
```

全部分離。

---

# 四、Global Identity

V3 增加：

```text
GlobalEntityID
```

用途：

```text
Persistent online identity
Cross-region handoff
Persistence
Audit / replay
```

正式：

```text
GlobalEntityID
≠ Local EntityID
≠ Scene UUID
≠ NetworkEntityID
```

Local Runtime：

```text
GlobalEntityID
↓ mapping
EntityID
```

跨 server 傳輸永不傳 local EntityID。

---

# 五、Region Partition

Server Region 不必與 Client Streaming Cell 1:1。

```text
Client Cell
→ Residency / Presentation

Server Region
→ Simulation Authority
```

可依：

```text
Spatial Area
Population Density
Gameplay Zone
Load
Instance Boundary
```

動態分配。

V3 第一版採：

```text
Stable Region Identity
+
Dynamic Region → Server Assignment
```

不在 runtime 任意改 Region identity。

---

# 六、World Directory

建立：

```text
IWorldDirectory
```

保存：

```text
RegionID → ServerNode
Server Health
Region Epoch
Authority Lease
Routing Metadata
```

Directory 不保存所有 Gameplay Entity state。

實作可以：

```text
Single-process dev backend
Replicated service provider
Cloud provider
Custom MMO backend
```

---

# 七、Authority Lease

每個 Region 有：

```text
AuthorityLease
├─ RegionID
├─ OwnerServer
├─ Epoch
├─ Expiry / Heartbeat
└─ TransitionState
```

Epoch 防止：

```text
Old Server after partition
→ continues writing stale authority
```

所有跨 region authority message 都帶：

```text
Region Epoch
```

---

# 八、Entity Handoff

玩家 / NPC 跨 Region：

```text
Source Region
↓
Prepare Handoff
↓
Serialize Transfer State
↓
Target Region Pre-create Ghost
↓
Target validates
↓
Authority Epoch / Ownership Switch
↓
Client Routing Switch
↓
Source retires old entity
```

Handoff state 只包含明確 schema：

```text
Transform
Velocity
Gameplay State
Inventory Ref
Quest Ref
Prediction State
Relevant transient state
```

不得 memcpy Runtime Entity memory。

Terrain Streaming Boundary 銜接：`Authority Epoch / Ownership Switch` 執行前，Target Region 必須確認角色的 Occupied Cell Set（沿用 Character Framework 的 Terrain Streaming Boundary Contract）在 Target 端至少已進入與 `StreamingPending` 相容的載入狀態；若 Target 尚未 Ready，Handoff 必須延後 Ownership Switch，角色在 Source Region 端維持既有 Authority 與 Collision，不得因跨 Region 交接而出現「雙方都沒有 Collision」的空窗。Border Ghost 的 Cross-border collision approximation 可作為交接前的暫時性緩衝，但不得取代正式 Ready 判定。

---

# 九、Handoff Failure

Target 不可用：

```text
Prepare
↓
Target Reject / Timeout
↓
Source Retains Authority
```

已切 authority 但 target crash：

```text
Directory Epoch
+
Persistence / Transfer Journal
↓
Recovery Policy
```

禁止雙寫 authority。

---

# 十、Cross-Region Interaction

跨 Region Gameplay 不直接共享 pointer / EntityID。

使用：

```text
CrossRegionMessage
```

類型：

```text
Intent
Event
RPC-like Command
State Summary
Interest Proxy
```

長距離：

```text
eventual / asynchronous
```

邊界附近需要精確互動時：

```text
Border Ghost / Proxy
```

---

# 十一、Border Ghost

相鄰 Region 可維護：

```text
Ghost Entity
```

只保存：

```text
Position
Bounds
Velocity
Relevant Gameplay Summary
```

Ghost：

```text
not authoritative
```

用途：

```text
Visibility
Interest
Pre-handoff
Cross-border collision approximation where explicitly allowed
```

核心 gameplay authority 仍只有一個 Region。

---

# 十二、Global Interest Federation

Client Interest：

```text
Player / Camera / Party / Quest
↓
Local Region Interest
↓
Federated Interest Router
↓
Relevant Adjacent / Remote Region summaries
```

遠端世界不把完整 replication stream送給每個 client。

支持：

```text
Spatial
Party
Guild / Social
Quest
Spectator
Global event
Custom
```

---

# 十三、Gateway / Session Routing

V3 Server Roles：

```text
Gateway
Session / Auth Adapter
World Directory
Region Server
Persistence Service Adapter
Telemetry / Audit
```

Gateway 只做：

```text
Connection Termination
Routing
Rate / Abuse policy hooks
Session mapping
```

不承擔完整 world simulation。

---

# 十四、Persistence Model

V3 MMO Persistence 不保存整個 server memory image。

模型：

```text
Authoritative Snapshot
+
Event / Delta Journal
+
Versioned Schema
```

Persistent Entity：

```text
GlobalEntityID
Persistent Component Set
Revision
```

Transient presentation data 不寫 DB。

---

# 十五、Checkpoint / Journal

Region：

```text
Periodic Checkpoint
+
Incremental Journal
```

Recovery：

```text
Load checkpoint
↓
Replay committed journal
↓
Resume Region
```

需要：

```text
Monotonic Region Revision
Idempotent Operation ID
```

避免重放造成重複獎勵 / 重複交易。

---

# 十六、Economy / Transaction Boundary

V3 Engine 不內建 MMORPG 經濟規則。

但提供：

```text
Transactional Persistence Hook
Idempotency Key
Authoritative Revision
Audit Event
```

Inventory / currency 是否需要強 transaction 由 Game Backend 決定。

Gameplay thread 不直接 blocking SQL。

---

# 十七、Region Failover

第一版正式能力：

```text
Health Monitor
↓
Lease Expired
↓
Select Recovery Node
↓
Load Checkpoint / Journal
↓
Acquire New Epoch
↓
Route Clients
```

V3 不保證零秒 failover。

目標：

```text
No split brain
Recoverable world state
Explicit reconnect / migration policy
```

---

# 十八、Rolling Deployment

Server native code 更新不使用任意 runtime DLL hot swap。

正式：

```text
Deploy Build N+1
↓
Mark N draining
↓
Move / finish sessions or regions
↓
Persist / Handoff
↓
Route to N+1
↓
Retire N
```

不同 build 可短暫共存時：

```text
Protocol Compatibility Window
```

必須顯式聲明。

---

# 十九、Distributed Observability

所有 distributed event 帶：

```text
TraceID
RegionID
ServerNodeID
GlobalEntityID optional
ConnectionID
BuildID
Epoch
```

可追：

```text
Client Input
↓
Gateway
↓
Region A
↓
Handoff
↓
Region B
↓
Persistence
```

---

# 二十、Deterministic Simulation Domain

V3 不做 Full Engine Determinism。

正式：

```text
World
├─ Deterministic Simulation Domain
└─ General Runtime / Presentation Domain
```

只有明確註冊的 System / Component 進 deterministic domain。

---

# 二十一、Deterministic State Schema

每個 deterministic component 必須：

```text
Stable TypeID
Stable PropertyID
Explicit Serialization
Canonical Byte Order
No Raw Pointer
No Unordered Iteration
```

State Hash：

```text
Canonical deterministic state
↓
Hash
```

用於：

```text
Desync Detection
Replay Validation
Rollback Test
Self-play Reproduction
```

---

# 二十二、Deterministic Tick

```text
Fixed Tick
Integer TickIndex
Deterministic Timer
Deterministic RNG
InputCommand per Tick
```

禁止 deterministic System 讀：

```text
Wall Clock
Render Delta
OS Random
Unordered thread timing
GPU simulation result
```

---

# 二十三、Deterministic Math

提供：

```text
Deterministic.Math
```

可有：

```text
Fixed-point
Deterministic integer vector
Controlled float profile where verified
```

正式政策：

```text
Cross-platform lockstep-critical state
→ fixed/integer preferred

Presentation / general gameplay
→ normal float
```

不強迫整個引擎使用 fixed-point。

---

# 二十四、Deterministic Scheduler

Deterministic Domain 允許平行，但結果必須與 thread scheduling 無關。

策略：

```text
Stable Work Partition
Stable Reduction Order
Command Buffer
Deterministic Merge
```

禁止：

```text
atomic race determines gameplay order
```

---

# 二十五、Rollback

Rollback State：

```text
Tick Snapshot
Input History
Pending Deterministic Events
RNG State
```

流程：

```text
Receive correction
↓
Restore Tick T
↓
Replay Inputs T+1..Now
↓
Publish corrected state
```

Presentation：

```text
Visual smoothing
```

與 deterministic state 分離。

Deterministic Domain 與 World Partition Streaming 的邊界必須明確宣告，否則 Rollback 容易被破壞：

```text
Deterministic Domain Streaming Policy
├─ Bounded / Pre-loaded（V3 V1 預設）
│  → Deterministic Domain 涵蓋的空間範圍於 Tick 0 前全部 Resident
│  → 不參與後續 Cell Unload/Evict
│  → Rollback 不需額外處理 Cell Residency 歷史
│
└─ Streamed（Future / Opt-in，需 ADR）
   → 需要 Deterministic Residency Log：逐 Tick 記錄 Cell Residency 快照
   → Rollback 的 `Restore T` 必須先還原對應 Tick 的 Residency 狀態，才能 Replay Inputs
   → 否則 Replay 時的 Cell Collision 可能與 Tick T 發生當下不一致，破壞 determinism
```

V3 V1 範圍限制：Deterministic Domain 預設採 Bounded / Pre-loaded 策略。若專案需要 Deterministic Domain 覆蓋 Streamed 大世界（V2 Large World / Adaptive Partition），必須走 ADR 流程並實作 Deterministic Residency Log；不得隱式假設 Rollback 天然相容 Streaming。

---

# 二十六、Lockstep

```text
Input for Tick N
↓
Agreement / Deadline
↓
Simulate Tick N
↓
State Hash
```

支援：

```text
Peer-hosted
Server-coordinated
```

但 V3 預設仍推薦 Server coordinated。

---

# 二十七、Deterministic Physics

獨立 Plugin：

```text
Deterministic.Physics
```

不替換 Jolt。

適合：

```text
RTS
Rollback combat
Deterministic projectile / hit simulation
```

不要求與 Jolt 完全 feature parity。

---

# 二十八、GPU Simulation Domains

V3 正式允許：

```text
GPU Authoritative Simulation Island
```

但 authority scope 必須顯式。

例如：

```text
Mass Debris
Fluid
SoftBody
Crowd Local Motion
Special RigidBody Island
```

核心角色 / server gameplay 仍可 CPU authoritative。

---

# 二十九、GPU Physics Architecture

```text
GPUPhysicsWorld
├─ Broadphase
├─ Bodies
├─ Constraints
├─ Solver
├─ Query Acceleration
└─ Readback Summary
```

提交：

```text
CPU Commands
↓
GPU Command Buffer
↓
RenderGraph Compute Passes
↓
GPU State
```

不得每個 body 跨 PCIe / unified memory boundary 即時 round-trip。

---

# 三十、GPU Authority Boundary

CPU Gameplay 若需要讀 GPU-authoritative domain：

```text
GPU Summary
↓
Deferred Readback
↓
Next / Later Tick Consumption
```

不能：

```text
Gameplay immediate query
↓
stall GPU
↓
same-frame result
```

Immediate gameplay query 仍使用 CPU authoritative representation。

---

# 三十一、GPU Physics State Transfer

GPU island 建立 / destroy / migration 使用：

```text
Spawn Batch
Destroy Batch
Parameter Update Batch
```

必要時可：

```text
GPU → CPU Snapshot
```

用於：

```text
Save
Debug
Migration
Failure Recovery
```

不是 per-frame path。

---

# 三十二、GPU Broadphase / Solver

V3 Built-in GPU physics 可先支援有限 shape set：

```text
Sphere
Box
Capsule
Convex
Static Mesh proxy
SDF optional
```

不要求 V1 Jolt 所有 feature 一次移植。

專注：

```text
massive count
regular memory layout
batch processing
```

---

# 三十三、GPU Fluid / SoftBody

作 Optional Plugin：

```text
GPUPhysics.Fluid
GPUPhysics.SoftBody
```

它們：

```text
do not become required Physics.Core dependency
```

---

# 三十四、Hardware Ray Tracing Framework

V3 正式建立：

```text
RTASManager
BLAS
TLAS
RayTracingPipeline
RayQuery
Denoiser Interface
RT Budget Manager
```

Renderer Core 仍可完全 Raster。

---

# 三十五、BLAS / TLAS

Static Mesh：

```text
Cooked RT Geometry Metadata
↓
BLAS Build / Cache
```

Dynamic：

```text
Refit
or
Rebuild
```

TLAS：

```text
Visible / Relevant RT Instances
↓
TLAS
```

RT residency 與 general mesh residency 整合。

---

# 三十六、RT Budget

每個 profile：

```text
RT Instance Budget
BLAS Memory
TLAS Build Time
Ray Count
Bounce Count
Denoiser Budget
```

低階：

```text
RT disabled
```

中階：

```text
RT Shadow / Reflection selective
```

高階：

```text
RT GI / richer reflection
```

---

# 三十七、RT Effects

First-class optional passes：

```text
RT Shadow
RT Reflection
RT AO
RT GI
```

每個都可：

```text
Off
Hybrid
Full
```

Raster fallback 必須存在。

---

# 三十八、Path Tracer

Optional：

```text
Renderer.PathTracer
```

用途：

```text
Editor reference
Look-dev
Photo Mode
Offline / high-end capture
Golden Image reference
```

不取代 realtime renderer。

---

# 三十九、Denoiser Interface

```text
IDenoiser
```

可有：

```text
Engine temporal denoiser
Vendor plugin
Offline high-quality denoiser
```

Renderer 不寫死 vendor。

---

# 四十、Mesh Shader First-class Path

V2 是 capability path，V3 升級為 first-class。

Asset：

```text
Mesh
↓
Cluster / Meshlet Cook
```

Runtime：

```text
Meshlet
├─ Mesh Shader Path
└─ Indexed Indirect Fallback
```

同一 content 不因沒有 Mesh Shader 完全無法執行。

---

# 四十一、Cluster Geometry

Optional：

```text
RenderFeature.ClusterGeometry
```

Hierarchy：

```text
Cluster Tree
↓
Projected Error
↓
GPU Select
↓
Visible Cluster
```

與：

```text
Object LOD
HLOD
```

共存。

不要求所有 mesh 改成 virtualized cluster asset。

---

# 四十二、Work Graph

```text
RenderFeature.WorkGraph
△
```

只有 backend capability 支援時使用。

用途：

```text
GPU work generation
complex culling / material work
simulation scheduling
```

不可成為跨平台 Renderer 唯一路徑。

---

# 四十三、Virtual Geometry

V3 可提供：

```text
RenderFeature.VirtualGeometry
```

但採 Optional。

Cook：

```text
Source Mesh
↓
Cluster Hierarchy
↓
Streamable Geometry Pages
```

Runtime：

```text
Visibility
↓
Page Demand
↓
Geometry Streaming
```

低階 profile 仍走傳統 Mesh LOD / HLOD。

---

# 四十四、ML Training Platform

V2 只有 Training Bridge。

V3 正式提供 Built-in Training Toolchain。

架構：

```text
Engine Simulation
↓
Vectorized Environment API
↓
Trainer
├─ PPO
├─ SAC
├─ DQN
├─ Self-play
└─ Custom Algorithm
↓
Checkpoint
↓
Evaluation
↓
Runtime Policy Export
```

Trainer 不進 Shipping Client。

---

# 四十五、Environment API

每個 training environment：

```text
Reset(seed)
Step(action)
GetObservation()
GetReward()
GetDone()
GetMetrics()
```

Batch：

```text
N Worlds
↓
ObservationBatch
↓
Policy
↓
ActionBatch
↓
N Worlds
```

減少語言 / process 邊界 call 次數。

---

# 四十六、Multi-World Headless

同一 process：

```text
World 0
World 1
...
World N
```

共用：

```text
Immutable Assets
Code
Read-only DataTable Generation
```

各自：

```text
Entity Registry
PhysicsWorld
NavigationWorld
AI State
RNG
Time
```

可：

```text
Faster-than-realtime
```

---

# 四十七、Training Clock

Training Profile：

```text
No wall-clock pacing
```

模擬：

```text
Step Fixed Tick as fast as budget allows
```

只有需要 rendering observation 時才建立 rendering world / offscreen view。

---

# 四十八、Observation System

Observation 不以 ad-hoc JSON 每 tick 傳輸。

正式：

```text
ObservationSchema
↓
Packed Observation Buffer
```

支持：

```text
Scalar
Vector
Discrete
Mask
Image / Tensor Handle optional
Entity Set / Attention-style packed set
```

---

# 四十九、Action System

```text
ActionSchema
```

支持：

```text
Discrete
MultiDiscrete
Continuous
Hybrid
Action Mask
```

Policy output 轉：

```text
Gameplay Intent / AI Action
```

而不是直接改 Transform。

---

# 五十、Reward / Metrics

Engine 提供 framework：

```text
Reward Channel
Episode Metric
Evaluation Metric
```

具體 reward 由 Game / Training Plugin 定義。

禁止 Engine Core 寫死「Kill = +1」。

---

# 五十一、PPO / SAC / DQN

V3 官方 Trainer Plugin：

```text
ML.Training.PPO
ML.Training.SAC
ML.Training.DQN
```

它們共用：

```text
Replay / Rollout Storage
Checkpoint
Metrics
Evaluation
```

演算法可版本化，訓練 artifact 記錄：

```text
Trainer Version
Hyperparameters
Environment Build ID
Observation Schema
Action Schema
Seed
```

---

# 五十二、Self-play

```text
Policy Pool
↓
Matchmaker
↓
Environment
↓
Result
↓
Rating / Evaluation
↓
Policy Update
```

避免只永遠對「最新自己」訓練造成崩壞。

支援：

```text
Historical Opponent
League
Exploit / Main Policy role
Custom
```

---

# 五十三、Curriculum

```text
Curriculum Stage
```

可依：

```text
Success Rate
Episode Count
Metric Threshold
Manual Schedule
```

調整：

```text
Map
Opponent
Difficulty
Observation noise
Rule parameters
```

---

# 五十四、Evaluation

Training 與 Evaluation 分離。

Evaluation：

```text
Frozen Policy
Fixed Scenario Set
Known Seeds
No Exploration Noise
```

CI 可比較：

```text
Win Rate
Score
Safety Metrics
Regression
```

---

# 五十五、Policy Export

Trainer 輸出：

```text
Policy Artifact
```

可：

```text
Engine-native optimized format
ONNX
Vendor backend format via plugin
```

Runtime：

```text
AI.Policy
↓
Policy Runtime Backend
↓
Action
```

---

# 五十六、Simulation Farm

大型訓練：

```text
Coordinator
↓
Workers
↓
N Headless Worlds
↓
Rollout / Metrics
↓
Trainer
```

Worker capability：

```text
CPU
GPU
Memory
Build ID
Environment Set
```

---

# 五十七、Simulation Farm Fault Tolerance

Worker crash：

```text
Lease expires
↓
Episode discarded / reassigned
```

Checkpoint：

```text
Trainer Checkpoint
Policy Checkpoint
Curriculum State
Rating State
```

定期 durable save。

---

# 五十八、Dataset / Replay

可把：

```text
Observation
Action
Reward
State Summary
```

寫成 Dataset。

用途：

```text
Offline Evaluation
Behavior Cloning
Debug
Regression
```

Data 格式 versioned。

---

# 五十九、Massive Crowd V3

Optional：

```text
Crowd.Massive
Crowd.FlowField
Crowd.GPUAvoidance
Crowd.AggregateSimulation
```

Near：

```text
Individual Character
```

Mid：

```text
Reduced individual
```

Far：

```text
Aggregate population state
```

不是遠方幾萬 NPC 全跑完整 Behavior Tree。

---

# 六十、Aggregate Simulation

Region-level：

```text
Population Count
Faction State
Resource Flow
Travel Flow
Combat Aggregate
```

需要靠近 / 被觀察時：

```text
Materialize Individuals
```

離開：

```text
Aggregate Back
```

轉換必須有 deterministic / auditable policy，避免 dup entity。

---

# 六十一、GPU Navigation

Optional：

```text
GPU Flow Field
GPU Cost Field
GPU Crowd Neighbor Query
```

不取代 Recast/NavMesh。

適合：

```text
RTS
Massive Crowd
Large formation
```

---

# 六十二、V3 Media

V3 繼承 V2 Streaming / DRM / Capture。

可新增 Optional：

```text
Media.LowLatency
Media.WebRTC
Media.VideoCompositorGPU
Media.CloudStreamClient
```

但不列 V3 核心主軸。

Media 仍是一個 Optional Feature family。

---

# 六十三、V3 Security

V3 新增 distributed / plugin / remote tool 風險。

正式：

```text
Authentication boundary
Authorization
Region Epoch
Replay Protection
Rate Limit hook
Packet size limits
Remote command role
Plugin privilege metadata
Artifact signature
Audit log
```

Engine 不自行設計 crypto primitive。

---

# 六十四、Plugin Privilege

Plugin 可宣告：

```text
Filesystem.ReadProject
Filesystem.WriteCache
Network.Client
Network.Server
GPU.Compute
Editor.ModifyDocument
Build.RunExternalTool
```

Native Plugin 仍屬高權限 code；Privilege 是管理 / audit boundary，不宣稱可安全 sandbox 惡意 native binary。

---

# 六十五、Distributed Build V3

V2 worker 擴展：

```text
Build Coordinator
↓
Content-addressable Task
↓
Worker Pool
↓
Verified Artifact
```

加入：

```text
Geometry Cluster Build
RT Data Build
ML Dataset Prep
Distributed World Validation
Server Package
```

---

# 六十六、Artifact Provenance

每個重要 build artifact 記錄：

```text
Source Hash
Tool Version
Plugin Version
Platform Profile
Dependency Hash
Worker Build ID
Output Hash
```

確保：

```text
Reproducible
Auditable
Cacheable
```

---

# 六十七、Build Size Analyzer

V3 正式工具：

```text
Executable
Plugin
ThirdParty
Shader
Asset
Localization
Symbols
```

可顯示：

```text
Transitive Dependency Tree
```

回答：

```text
Why did enabling this plugin add 350MB?
```

---

# 六十八、Runtime Plugin Trust

分類：

```text
BuiltIn
Verified
ProjectLocal
ThirdParty
ToolOnly
```

影響：

```text
Editor Warning
Privilege
Package validation
Distribution policy
```

---

# 六十九、Telemetry / Privacy Boundary

Telemetry Provider 是 Optional。

Engine Core 只定義：

```text
Metric
Trace
Crash
Audit
```

實際上傳目的地 / consent / retention 由 Product / Provider 決定。

---

# 七十、V3 Build Profiles

```text
Editor
DesktopClient
MobileClient
DedicatedServer
MMORegionServer
MMOGateway
TrainingHeadless
SimulationWorker
BuildWorker
Benchmark
OfflineRenderer
```

不同 profile 有完全不同 module set。

---

# 七十一、MMO Region Server Profile

Include：

```text
World
Scene
Physics
Navigation
AI
Networking
DistributedWorld.Region
Persistence
Replay / Audit
```

Strip：

```text
Renderer
Runtime UI
Audio Output
VFX presentation
WebView
Media
```

---

# 七十二、Gateway Profile

Include：

```text
Network Transport
Session
Routing
Auth Provider
Rate Limit
Directory Client
Telemetry
```

不需要：

```text
Physics
Navigation
Renderer
Gameplay World
```

---

# 七十三、Training Headless Profile

Include：

```text
World
Gameplay
AI
ML Training Bridge
Physics / Navigation as project needs
Replay
Metrics
```

Strip presentation。

---


---

# V3 施工 Milestone（Implementation Milestones）

> V3 建立在 **V2 Production Gate 已通過** 的前提下。  
> V3 最大風險來自「Distributed Authority、Determinism、GPU Authority、ML Training」；因此施工順序必須先把邊界與可驗證性建立，再擴規模。

## V3-M0 — Compatibility / Capability Baseline

施工：

```text
V2 → V3 Migration Scanner
Plugin ABI / Privilege Audit
Deterministic-capable System Audit
Server Protocol Compatibility Audit
GPU Capability Matrix
TrainingHeadless Baseline
```

**Gate：**

```text
✓ V2 Project 不啟用 V3 feature 仍可 build
✓ V3 optional module 不造成 V2 baseline footprint 明顯增加
✓ Capability matrix 能決定 RT / Mesh Shader / GPU Physics path
```

---

## V3-M1 — Deterministic Core

先做：

```text
Deterministic State Schema
Stable State Serialization
Fixed Tick
Deterministic Timer
Deterministic RNG
Canonical Hash
Stable Scheduler Partition
Stable Reduction / Merge
```

**Gate：**

```text
✓ Same Build + Input + Seed → same State Hash
✓ 不依 wall clock / global random / unordered iteration
✓ CI 可重跑 deterministic scenario
```

---

## V3-M2 — Rollback / Lockstep / Deterministic Physics

施工：

```text
Snapshot
Input History
Restore / Replay
Rollback Window
Lockstep Tick Agreement
Desync Detection
Deterministic Physics Plugin foundation
```

**Gate：**

```text
✓ Rollback 可復原指定 tick
✓ Lockstep desync 可定位
✓ Deterministic Physics 可獨立開關
✓ General Jolt runtime 不被迫 deterministic
```

---

## V3-M3 — Distributed World Foundation

施工：

```text
GlobalWorldID
GlobalEntityID
RegionID
World Directory
ServerNode
Authority Lease
Epoch
Region Routing
```

先不做完整 Handoff。

**Gate：**

```text
✓ Region authority 唯一
✓ Stale epoch message 被拒
✓ Directory backend 可替換
✓ Local EntityID 永不跨 server 持久化
```

---

## V3-M4 — Handoff / Gateway / Persistence / Recovery

順序：

```text
Gateway
↓
Session Routing
↓
Entity Handoff
↓
Border Ghost
↓
Federated Interest
↓
Checkpoint + Journal
↓
Region Failover
↓
Rolling Deployment
```

**Gate：**

```text
✓ Handoff fail 可回 source authority
✓ Node kill 不產生雙 authority
✓ Checkpoint + Journal 可恢復 Region
✓ N / N+1 server build 可依 compatibility window rolling deploy
```

---

## V3-M5 — GPU Simulation Foundation

施工：

```text
GPUPhysicsWorld
GPU Body Storage
Broadphase
Constraint Graph
Solver
Batch Spawn / Destroy / Update
Deferred Summary Readback
GPU Resource Lifetime
```

正式限定：

```text
GPU Authoritative Simulation Island
≠ Entire Gameplay Physics
```

**Gate：**

```text
✓ 無 per-body same-frame CPU roundtrip
✓ Gameplay immediate query 不 stall GPU
✓ GPU state 可按需 snapshot / migrate
✓ Jolt CPU-authoritative domain 繼續正常工作
```

---

## V3-M6 — GPU Fluid / SoftBody / Destruction Extensions

Optional 施工：

```text
GPU Fluid
GPU SoftBody
GPU Destruction
GPU Debris
```

**Gate：**

```text
✓ 各 Extension 可單獨 strip
✓ 不變成 Physics.Core dependency
✓ Gameplay coarse state 與 visual detail 可分離
```

---

## V3-M7 — Ray Tracing / Path Tracing

施工：

```text
RTAS Manager
BLAS
TLAS
Ray Query
RT Shadow
RT Reflection
Denoiser Interface
RT AO
RT GI optional
Path Tracer
```

**Gate：**

```text
✓ Raster fallback 完整
✓ RT disabled 不建立 BLAS / TLAS
✓ RT memory / build budget 可 profile
✓ Path Tracer 可作 reference / offline / photo mode
```

---

## V3-M8 — Mesh Shader / Cluster Geometry / Virtual Geometry

施工：

```text
Meshlet / Cluster Cook
Mesh Shader Path
Indexed Indirect Fallback
Cluster Hierarchy
Geometry Page Streaming optional
Work Graph experimental
```

**Gate：**

```text
✓ 同一 asset 可 Mesh Shader / fallback
✓ 不支援 Work Graph 平台可完全不編
✓ Cluster streaming 不破壞 HLOD / traditional LOD
```

---

## V3-M9 — ML Training Runtime Foundation

先做：

```text
TrainingHeadless Profile
Vectorized Environment API
Observation Schema
Action Schema
Reward Channel
Metrics
Multi-World per Process
Faster-than-realtime Tick
Checkpoint Format
```

先不要一開始就做 PPO。

**Gate：**

```text
✓ N Worlds 可同 process 跑
✓ Reset / Step / Observation / Action batch 可用
✓ Seed 可重現支援 deterministic 的 environment
✓ Training Profile 無 Renderer / UI / Audio baggage
```

---

## V3-M10 — PPO / SAC / DQN / Self-play

施工：

```text
PPO
SAC
DQN
Rollout / Replay Storage
Policy Checkpoint
Self-play League
Curriculum
Evaluation
Policy Export
```

**Gate：**

```text
✓ Checkpoint resume
✓ Frozen Evaluation 與 Training 分離
✓ Policy artifact 記錄 schema / build / trainer version
✓ Runtime Policy 可由 AI.Policy backend 載入
```

---

## V3-M11 — Simulation Farm / Massive Crowd / GPU Navigation

施工：

```text
Simulation Farm Coordinator
Worker Lease
Fault Recovery
Dataset
Distributed Metrics
Massive Crowd
Aggregate Simulation
GPU Flow Field / Cost Field optional
```

**Gate：**

```text
✓ Worker crash 不破壞 trainer state
✓ Episode 可被丟棄 / reassigned
✓ Far population 不需完整 per-agent AI
✓ Aggregate ↔ individual 不複製 entity
```

---

## V3-M12 — Security / Observability / Plugin Trust / Build Provenance

施工：

```text
Authentication Boundary
Authorization Hook
Rate Limit
Audit Log
Plugin Privilege
Plugin Trust Level
Artifact Provenance
Distributed Trace
Build Size Analyzer
Transitive Dependency Report
```

**Gate：**

```text
✓ Region / Gateway / Plugin operation 可 audit
✓ Native Plugin privilege metadata 可驗證
✓ Build artifact 可追 source / tool / plugin / worker hash
✓ 能解釋 feature/plugin 為何增加 package size
```

---

## V3-M13 — Distributed / GPU / ML Production Hardening

Reference Projects：

```text
Distributed Continent
Deterministic RTS
GPU Physics Sandbox
RT Showcase
Self-play Arena
```

Fault Injection：

```text
Node Kill
Network Partition
Stale Epoch
Handoff Timeout
GPU Device Stress
Worker Loss
Checkpoint Recovery
```

**Gate：**

```text
✓ 24h+ distributed soak
✓ 無 split-brain authority
✓ Determinism repeat hash 通過
✓ GPU simulation 無 hidden synchronous readback
✓ ML checkpoint / farm recovery 通過
✓ Raster / non-ML / non-MMO projects 不被 V3 重量級功能污染
```

---

## V3 施工依賴圖

```text
M0 Compatibility Baseline
 │
 ├───────────────────┐
 ▼                   ▼
M1 Deterministic   M3 Distributed Foundation
 │                   │
 ▼                   ▼
M2 Rollback        M4 Handoff / Recovery
 │                   │
 ├───────────┐       │
 ▼           ▼       │
M5 GPU Sim  M9 ML Runtime Foundation
 │           │
 ▼           ▼
M6 GPU Ext  M10 Trainer / Self-play
 │           │
 ├──────┐    ▼
 ▼      ▼  M11 Sim Farm / Massive Crowd
M7 RT  M8 Mesh/Cluster
 │      │
 └──┬───┘
    ▼
M12 Security / Observability / Provenance
    │
    ▼
M13 Production Hardening
```


# 七十四、V3 Development Phases

```text
V3-A Deterministic Foundation
↓
V3-B Distributed World Foundation
↓
V3-C GPU Simulation
↓
V3-D RT / Mesh Shader / Cluster Geometry
↓
V3-E ML Training Platform
↓
V3-F Simulation Farm / Massive Crowd
↓
V3-G MMO Hardening
↓
V3-H Production Hardening
```

---

# 七十五、Phase V3-A — Deterministic Foundation

內容：

```text
Deterministic State Schema
Tick / Timer
RNG
Hash
Rollback
Lockstep
Deterministic Scheduler
Deterministic Physics Plugin foundation
```

Gate：

```text
Same input + seed + build
→ same deterministic state hash
```

跨支援平台重複測試。

---

# 七十六、Phase V3-B — Distributed World

內容：

```text
World Directory
Region
Authority Lease
Epoch
Entity Handoff
Gateway
Federated Interest
Persistence Checkpoint / Journal
```

Gate：

```text
No split brain
Handoff rollback safe
Server restart recoverable
```

---

# 七十七、Phase V3-C — GPU Simulation

內容：

```text
GPUPhysicsWorld
Broadphase
Solver
Batch Spawn / Update
Deferred Readback
GPU Fluid / SoftBody hooks
```

Gate：

```text
No same-frame gameplay readback dependency
GPU state lifetime fence-safe
CPU authoritative world remains functional
```

---

# 七十八、Phase V3-D — Next-gen Renderer

內容：

```text
RTAS
RT Shadow / Reflection
Denoiser
Path Tracer
Mesh Shader
Cluster Geometry
Work Graph experiment
```

Gate：

```text
Raster fallback remains complete
No platform becomes unsupported solely due RT/MS absence
```

---

# 七十九、Phase V3-E — ML Training

內容：

```text
Vectorized Environment
Observation / Action Schema
PPO / SAC / DQN
Self-play
Curriculum
Evaluation
Policy Export
```

Gate：

```text
Headless N-world run
Checkpoint resume
Deterministic seed reproduction where environment supports
Runtime export loads through AI.Policy
```

---

# 八十、Phase V3-F — Simulation Farm

內容：

```text
Coordinator
Worker
Lease
Dataset
Metrics
Fault tolerance
```

Gate：

```text
Worker loss does not corrupt trainer state
Artifacts tied to Build ID / schema
```

---

# 八十一、Phase V3-G — MMO Hardening

內容：

```text
Region Failover
Rolling Deploy
Gateway Scale
Persistence Load
Cross-region soak
Security / abuse hooks
```

Gate：

```text
24h+ distributed soak
Repeated handoff
Node kill test
No duplicate authority
```

---

# 八十二、Phase V3-H — Production Hardening

```text
GPU Device Stress
RT Memory Pressure
Simulation Farm Scale
ML Long Run
Distributed Build Reproducibility
Build Size Audit
Migration from V2
```

---

# 八十三、V3 Reference Projects

### A. Distributed Continent

```text
4+ Region Servers
Cross-region player movement
Persistence
Failover
```

### B. Deterministic RTS

```text
Lockstep
Thousands units
State hash
Replay
```

### C. GPU Physics Sandbox

```text
100k+ simulation elements where hardware permits
No CPU per-body roundtrip
```

### D. RT Showcase

```text
Raster
Hybrid RT
Path tracer comparison
```

### E. Self-play Arena

```text
N headless worlds
PPO / Self-play
Policy export
```

---

# 八十四、V3 CI

新增：

```text
Determinism Repeat Test
Cross-platform State Hash Test
Region Handoff Fault Injection
Region Handoff Target Collision Readiness
Authority Epoch Test
Node Kill / Recovery
GPU Physics CPU Stall Detector
RT / Raster Visual Regression
Mesh Shader / Fallback Golden Comparison
ML Checkpoint Resume
Simulation Worker Fault Test
Plugin Privilege Validation
Build Size Regression
```

---

# 八十五、V3 Non-Goals

```text
❌ Force every project into MMO architecture
❌ Remove CPU Jolt
❌ Require RT hardware
❌ Require Mesh Shader
❌ Make renderer/audio/UI deterministic
❌ Put Python/PyTorch in shipping client
❌ Allow remote native plugin injection into shipping runtime
❌ Promise malicious native plugin sandbox
❌ Make cloud provider mandatory
```

---

# 八十六、V3 Definition of Done

```text
1. V2 Project 可升級且不啟用 V3 plugin 時維持原有架構。
2. Distributed World 可支援 Region authority / handoff / recovery。
3. Region epoch 能防 split-brain stale authority。
4. Deterministic Domain 可通過 repeat-run hash。
5. Rollback / Lockstep 是 Optional Domain，不污染一般 runtime。
6. GPU authoritative simulation island 可存在但不強迫 gameplay physics 搬 GPU。
7. Raster renderer 在無 RT / Mesh Shader 硬體仍完整可用。
8. RT shadow / reflection 有 production path。
9. Path tracer 可作 editor/reference optional path。
10. Mesh Shader 有 indexed-indirect fallback。
11. Built-in Trainer 可執行 PPO / SAC / DQN。
12. Self-play / Curriculum / Evaluation 可重現與 checkpoint。
13. TrainingHeadless 可同 process 跑多 World。
14. Simulation Farm worker failure 可恢復。
15. MMO Region / Gateway / Client 使用不同 build profile。
16. ML Trainer / distributed server tooling 不進普通 client。
17. Plugin / Build Size Analyzer 可追 transitive cost。
18. V3 reference projects 全部通過 soak / CI。
```

---

# 八十七、V3 最終架構

```text
                               Engine V3
                                  │
        ┌─────────────────────────┼─────────────────────────┐
        │                         │                         │
   Local Runtime            Distributed Runtime       Training / Tools
        │                         │                         │
        ├─ V1/V2 Systems          ├─ Directory              ├─ Trainer
        ├─ GPU Driven             ├─ Region                 ├─ Sim Farm
        ├─ Large World            ├─ Handoff                ├─ Dataset
        ├─ RT / Mesh Shader       ├─ Persistence            ├─ Evaluation
        ├─ GPU Simulation         └─ Gateway                └─ Policy Export
        │
        ├─ Deterministic Domain
        └─ General Presentation Domain
```

V3 最重要的一句：

```text
更高的能力上限，
不等於更大的預設成本。
```


---

# V3 Plugin / Third-party SDK 詳細 Contract（Normative）

# 跨平台 3D Engine — V3 Plugin / Feature Module 模組化規劃

**文件版本：Draft v1.1**  
**對應 Engine 世代：V3.x**  
**定位：Distributed / Simulation / Next-Gen。**

---

# 一、V3 模組化目標

V3 會加入最重的能力：

```text
Distributed MMO
Deterministic Simulation
GPU Physics
Ray Tracing
Mesh Shader / Work Graph
ML Training
Massive Headless Simulation
```

因此 V3 的原則不是：

```text
V3 Build = 所有 Next-gen 功能全部存在
```

而是：

```text
V3 Repository
→ 提供能力

Project
→ 只選自己需要的能力
```

---

# 二、V3 核心原則

延續 V1 / V2：

```text
Feature exists in Repository
≠
Feature exists in Build
```

正式：

```text
Disabled Plugin
=
No Code
+
No Runtime Registration
+
No Shader
+
No Asset Type
+
No Third-party SDK
+
No Runtime Dependency
```

---

# 三、V3 不把 Next-gen Feature 升成 Mandatory

即使到了 V3：

```text
Ray Tracing
Mesh Shader
GPU Physics
Deterministic Simulation
ML Runtime
Distributed World
```

仍然：

```text
Capability
≠ Mandatory Workflow
```

---

# 四、Distributed World Plugins

拆：

```text
DistributedWorld.Core
DistributedWorld.Directory
DistributedWorld.Region
DistributedWorld.Handoff
DistributedWorld.Federation
DistributedWorld.Recovery
```

只給：

```text
MMO
Large persistent online world
Distributed simulation
```

使用。

一般單機 / session multiplayer：

```text
全部 strip
```

---

# 五、MMO Gateway Plugins

```text
MMO.Gateway
MMO.RegionRouter
MMO.AuthorityTransfer
MMO.SessionTransfer
```

Server-only。

Client 只需要：

```text
Network routing protocol support
```

不包含 server implementation。

---

# 六、Distributed Authority

```text
DistributedWorld.Authority
```

功能：

```text
Region Ownership
Entity Authority
Authority Lease
Transfer
Recovery
Conflict Detection
```

不使用 MMO：

```text
strip
```

---

# 七、Region Failover

```text
DistributedWorld.Failover
```

Production MMO server-only。

本機開發、一般 dedicated server 可不加入。

---

# 八、Persistence Providers

```text
Persistence.Core
Persistence.SQLProvider
Persistence.CloudProvider
Persistence.Custom
```

Engine 不綁：

```text
MySQL
PostgreSQL
MongoDB
Cloud vendor
```

具體 backend 都是 Provider Plugin。

---

# 九、Fleet / Orchestration Plugins

```text
ServerFleet.Telemetry
ServerFleet.Health
ServerFleet.OrchestrationAdapter
```

Provider：

```text
Kubernetes
Cloud Provider
Custom Infrastructure
```

Engine Core 不依賴。

---

# 十、Deterministic Simulation Plugins

```text
Deterministic.Core
Deterministic.Math
Deterministic.Random
Deterministic.Timer
Deterministic.Rollback
Deterministic.Lockstep
Deterministic.Physics
```

可依需求組合。

例如：

```text
Fighting Game
→ Rollback

RTS
→ Lockstep

Self-play
→ Core + Random + Timer
```

---

# 十一、Deterministic Physics

```text
Deterministic.Physics
```

獨立於：

```text
Physics.Jolt
```

正式原則：

```text
General Physics
→ Jolt

Deterministic Domain
→ Deterministic Physics Plugin
```

不把整顆 Engine 強迫改成固定點數。

---

# 十二、Rollback Plugin

```text
Deterministic.Rollback
```

提供：

```text
State Snapshot
Input History
Re-simulation
Rollback Window
State Hash
```

只管理 deterministic domain。

不 rollback：

```text
Renderer
Audio
GPU VFX
WebView
```

---

# 十三、Lockstep Plugin

```text
Deterministic.Lockstep
```

提供：

```text
Tick Agreement
Input Command
Hash Check
Desync Detection
Recovery Hook
```

策略 / RTS 可用。

---

# 十四、GPU Physics Plugins

```text
GPUPhysics.Core
GPUPhysics.Broadphase
GPUPhysics.RigidBody
GPUPhysics.Constraint
GPUPhysics.Query
```

只給 massive GPU simulation 專案。

正式：

```text
GPUPhysics Domain
≠ automatically Gameplay Truth
```

可由 project 明確決定某個 simulation island 是否 GPU-authoritative。

---

# 十五、GPU Fluid Plugin

```text
GPUPhysics.Fluid
```

完全 Optional。

可能用：

```text
SPH
Grid Fluid
Hybrid
```

但不放 Core。

---

# 十六、GPU SoftBody Plugin

```text
GPUPhysics.SoftBody
```

Optional。

---

# 十七、GPU Destruction V3

```text
GPUPhysics.Destruction
```

大量碎片 / fracture simulation。

Coarse Gameplay destruction state 與 visual GPU fragment 分離。

---

# 十八、Ray Tracing Plugins

```text
RenderFeature.RayTracing
```

再細拆：

```text
RT.Shadow
RT.Reflection
RT.AO
RT.GI
RT.RayQuery
```

沒有 RT：

```text
BLAS / TLAS Manager
RT shader
RT pipeline
```

都不進 build。

---

# 十九、Path Tracing Plugin

```text
Renderer.PathTracer
```

典型：

```text
Editor
Offline Preview
Photo Mode
High-end PC
```

不要求一般 Shipping 遊戲攜帶。

---

# 二十、Mesh Shader Plugin

```text
RenderFeature.MeshShader
```

V3 可以是 first-class renderer path，但仍 Optional。

Fallback：

```text
Meshlet
↓
Indexed Indirect
```

---

# 二十一、Work Graph Plugin

```text
RenderFeature.WorkGraph
```

Experimental / Capability-dependent。

不支援 backend：

```text
不編譯
```

不改變 core asset compatibility。

---

# 二十二、Cluster Geometry Plugin

```text
RenderFeature.ClusterGeometry
```

用途：

```text
Hierarchical Geometry
GPU Culling
Fine-grain LOD
Large static world
```

可與傳統：

```text
Mesh LOD
HLOD
```

並存。

---

# 二十三、Advanced Virtual Geometry

若 V3 後續需要，可：

```text
RenderFeature.VirtualGeometry
```

但仍：

```text
Optional
```

不把整個 Mesh Pipeline 強制轉成單一 virtualized format。

---

# 二十四、ML Training Platform Plugins

拆：

```text
ML.Training.Core
ML.Training.PPO
ML.Training.SAC
ML.Training.DQN
ML.Training.SelfPlay
ML.Training.Curriculum
ML.Training.Evaluation
```

全部屬：

```text
Tool / Training Build
```

Shipping Client：

```text
Trainer = 0 byte
```

---

# 二十五、ML Runtime Plugins

```text
ML.Runtime.Core
ML.Runtime.ONNX
ML.Runtime.Custom
```

Shipping 若只需要 inference：

```text
ML.Runtime.*
```

不帶 Trainer。

---

# 二十六、PyTorch / JAX Bridge

```text
ML.Bridge.PyTorch
ML.Bridge.JAX
```

僅：

```text
Development
Training
```

不進 Shipping Runtime。

---

# 二十七、Simulation Farm Plugins

```text
SimulationFarm.Coordinator
SimulationFarm.Worker
SimulationFarm.Telemetry
SimulationFarm.Dataset
```

用途：

```text
Self-play
RL Training
Massive simulation
Automated balance
Regression
```

一般遊戲 build 完全不需要。

---

# 二十八、Training Headless Meta-Plugin

```text
BuildProfile.TrainingHeadless
```

典型：

```text
Include:
World
Gameplay
Physics optional
Navigation optional
AI
Deterministic domain optional
ML Bridge
Replay
Metrics

Strip:
Renderer
UI
Audio output
PostProcess
WebView
```

---

# 二十九、Massive Crowd Plugins

```text
Crowd.Massive
Crowd.FlowField
Crowd.GPUAvoidance
Crowd.AggregateSimulation
```

只有：

```text
MMO
RTS
City Simulation
Large Crowd
```

需要。

---

# 三十、GPU Navigation Plugins

```text
Navigation.GPUFlowField
Navigation.GPUCostField
Navigation.GPUCrowdQuery
```

Optional。

一般 NavMesh project 不需要。

---

# 三十一、Aggregate Simulation Plugin

```text
Simulation.Aggregate
```

提供：

```text
Far Population Simulation
Region-level Economy State
Abstract Battle Resolution
Resource Flow
```

但具體：

```text
Economy
Faction
Trade
Population Rule
```

由 Game Plugin 實作，不寫死到 Engine。

---

# 三十二、Ray-traced Audio Plugin

```text
Audio.RayTracing
```

High-end Optional。

一般：

```text
Audio.RoomPortal
```

仍然可獨立存在。

---

# 三十三、Distributed Replay / Observability

```text
Distributed.Trace
Distributed.Replay
Distributed.Audit
```

只給 server infrastructure。

---

# 三十四、Server Security Plugins

```text
Server.Security.Auth
Server.Security.Attestation
Server.Security.RateLimit
Server.Security.AbuseDetectionHook
```

Provider / server-only。

不把第三方安全 SDK 寫進 Core。

---

# 三十五、V3 Renderer Feature Registry

V3 renderer module 仍遵守：

```text
Build / Init
↓
Register Render Feature
↓
RenderGraph Pass Factory
↓
Compiled Runtime Pipeline
```

禁止：

```text
per-draw plugin virtual dispatch
```

---

# 三十六、GPU Physics Module Boundary

Public：

```text
Handle
POD
Batch Command
Batch Query
```

Internal：

```text
GPU buffers
Solver
Broadphase
Constraint graph
```

不得把 backend native resource 暴露到 Gameplay ABI。

---

# 三十七、Deterministic Domain Boundary

```text
World
├─ Deterministic Domain
└─ Presentation / General Runtime Domain
```

Deterministic Plugin 只控制明確註冊的：

```text
Components
Systems
RNG
Timers
InputCommands
Snapshot State
```

其他系統不被拖進 bitwise determinism。

---

# 三十八、Distributed World Boundary

```text
Global World Identity
↓
Region
↓
Authority Node
↓
Local Runtime World / Region Runtime
```

跨 Region：

```text
Entity Handoff
```

而不是共享 raw EntityID。

---

# 三十九、Network / MMO 分離

V3 仍分：

```text
Network.Core
```

和：

```text
DistributedWorld.*
```

所以：

```text
MOBA
Co-op
Arena
```

只需要 Network，不需要 MMO mesh。

---

# 四十、ML / AI 分離

AI 仍可：

```text
Behavior Tree
Utility
GOAP
Learned Policy
```

ML Trainer 不會讓 AI.Core 依賴 PyTorch。

---

# 四十一、Tool-only / Runtime-only 分離

例如：

```text
ML.Training.*
→ Tool-only

ML.Runtime.ONNX
→ Runtime optional

Distributed Build
→ Tool-only

DistributedWorld.Region
→ Server runtime

RayTracing
→ Client runtime
```

不同 Profile 不混裝。

---

# 四十二、V3 Build Profiles

```text
Editor
DesktopClient
MobileClient
DedicatedServer
MMORegionServer
MMOGateway
TrainingHeadless
SimulationWorker
BuildWorker
Benchmark
```

每個 profile 有完全不同 Plugin Set。

---

# 四十三、V3 範例：一般單機 RPG

即使 Engine 已到 V3：

```text
Physics.Jolt
Character
Navigation
AI.BehaviorTree
Audio
UI
Save
Terrain
VFX
```

就可以。

完全不需要：

```text
DistributedWorld
GPUPhysics
RayTracing
ML
Deterministic
WorkGraph
```

這就是 V3 Plugin 化最重要的價值。

---

# 四十四、V3 範例：高階 PC RPG

可加入：

```text
RT.Reflection
RT.Shadow
MeshShader
ClusterGeometry
GPUCloth
```

但不需要：

```text
MMO
ML Trainer
Deterministic Physics
```

---

# 四十五、V3 範例：MMO

Client：

```text
Network.Core
Network.Replication
WorldPartition
HLOD
UI
Audio
Renderer
```

Region Server：

```text
Network.Core
DistributedWorld.Region
DistributedWorld.Handoff
Persistence
Physics
Navigation
AI
```

Gateway：

```text
MMO.Gateway
MMO.RegionRouter
Auth Provider
```

不同 executable 不應共享完整 plugin set。

---

# 四十六、V3 範例：RTS Lockstep

```text
Deterministic.Core
Deterministic.Math
Deterministic.Random
Deterministic.Lockstep
Deterministic.Physics optional
Navigation
AI.Utility
Crowd.FlowField
```

不需要：

```text
Distributed MMO
GPUPhysics
ML Trainer
RT
```

---

# 四十七、V3 範例：AI Self-play

```text
World
Gameplay
Physics as needed
Navigation as needed
AI.Policy
Deterministic Core optional
ML.Training.Core
ML.Training.PPO / SAC / DQN
ML.Training.SelfPlay
SimulationFarm.Worker
TrainingHeadless
Replay
Metrics
```

Strip：

```text
Renderer
UI
Audio
WebView
PostProcess
```

---

# 四十八、V3 容量控制

最重的第三方 dependency 都必須 optional：

```text
ONNX Runtime
PyTorch bridge
Vendor RT library
Cloud SDK
Database client
Voice SDK
MMO orchestration SDK
```

Project 沒用就完全沒有。

---

# 四十九、V3 Shader 控制

Render plugin 宣告：

```text
ShaderFamilies
PermutationDomain
Capabilities
```

Cook：

```text
Enabled Plugin
+
Used Material
+
Target Platform
+
Device Profile
↓
Final Shader Set
```

未啟用：

```text
RT
MeshShader
WorkGraph
VirtualGeometry
```

相關 Shader 全 strip。

---

# 五十、V3 Asset 控制

Asset 類型可屬於 Plugin：

```text
MLPolicyAsset
RTMaterialExtension
GPUPhysicsAsset
DistributedWorldRegionAsset
```

Plugin 關閉：

```text
Editor
→ preserve metadata / show missing plugin

Cook
→ referenced disabled runtime asset hard fail
```

---

# 五十一、V3 Plugin ABI

大型 Server Plugin：

```text
Plugin API Version
Engine ABI Hash
Build Profile
Platform
Architecture
Capabilities
```

握手 mismatch：

```text
Reject Load
```

不允許 undefined ABI crash。

---

# 五十二、V3 Hot Reload

只有適合的模組才 hot reload。

```text
ML Trainer Algorithm
→ Tool hot reload possible

Gameplay Zig
→ Runtime hot reload

Distributed Authority Runtime
→ restart / rolling deploy

RHI / MeshShader Backend
→ restart preferred
```

---

# 五十三、Rolling Server Upgrade

MMO module 不靠 process 內 hot reload。

正式：

```text
Deploy New Server Build
↓
Drain Region
↓
Transfer / Persist State
↓
Route New Sessions
↓
Retire Old Process
```

比 unload native server module 安全。

---

# 五十四、Plugin Observability

Profiler 顯示：

```text
Loaded Plugins
Static Features
Memory per Plugin
Jobs per Plugin
Shader Families per Plugin
Asset Residency by Plugin
Server Module Cost
```

可回答：

```text
為什麼這個 Build 這麼大？
```

以及：

```text
是哪個 Plugin 拉進這個 dependency？
```

---

# 五十五、Build Size Analyzer

V3 Tooling 建議正式加入：

```text
BuildSizeAnalyzer
```

輸出：

```text
Executable Size
Plugin Contribution
Third-party Library Size
Shader Size
Asset Size
Localization Size
Debug Symbol Size
```

並支援 dependency tree。

---

# 五十六、Plugin Cost Metadata

Plugin Manifest 可附：

```text
Estimated Runtime Footprint
ThirdParty SDK
Shader Family Count
Editor-only Size
Server-only
Client-only
```

Editor 啟用功能時能提示成本。

---

# 五十七、V3 Definition of Done

```text
1. Distributed MMO 可完全從非 MMO 專案移除。
2. Deterministic domain 可獨立開關。
3. GPU Physics 可獨立於 Jolt 世界存在。
4. Ray Tracing / Mesh Shader / Work Graph 全部 capability-driven。
5. ML Trainer 不進 Shipping Client。
6. ONNX runtime 只在 inference project 存在。
7. MMO Region / Gateway / Client 可使用不同 plugin set。
8. Server fleet provider 不污染 Engine Core。
9. Disabled next-gen render feature 不產 shader / PSO。
10. Build Size Analyzer 能追出 plugin dependency 與容量來源。
11. V1 / V2 Project 升 V3 後，不啟用新 plugin 也能維持接近原本 build footprint。
```

---


# 五十九、V3 Third-party Plugin SDK 擴充

V3 對 Plugin 生態的要求從「引擎擴充」提升到：

```text
Distributed Runtime
Deterministic Domain
GPU Simulation
ML Training
Infrastructure Provider
```

但仍遵守：

```text
Public Capability Boundary
≠ Engine Private Access
```

---

# 六十、Distributed World Provider Plugin

可擴充：

```text
Region Directory
Authority Store
Handoff Transport
Global Interest Router
Persistence Adapter
Failover Provider
```

例如企業可接自己的：

```text
Shard Manager
Cloud Region Service
Persistent World Backend
```

Engine Core 不知道實際 cloud implementation。

---

# 六十一、Deterministic Simulation Extension

第三方可註冊 deterministic subsystem：

```text
DeterministicSystemDescriptor
├─ State Schema
├─ Tick Function
├─ Snapshot Codec
├─ Hash Function
└─ Input Command Schema
```

只有顯式註冊的 state 進 deterministic domain。

禁止：

```text
Plugin 宣稱 deterministic
但偷偷讀 wall clock / global RNG / presentation state
```

CI 可提供 determinism verification harness。

---

# 六十二、GPU Simulation Plugin SDK

第三方可提供：

```text
GPU Fluid Solver
GPU SoftBody Solver
GPU Crowd Solver
Custom GPU Physics Island
```

但必須透過：

```text
RenderGraph / ComputeGraph Integration
Resource Registry
Fence-safe Lifetime
Capability Declaration
```

禁止直接取得 renderer private command buffer 長期持有。

---

# 六十三、ML Algorithm Plugin

Trainer 可註冊：

```text
PPO
SAC
DQN
SelfPlay Algorithm
Curriculum Strategy
Evaluation Strategy
Custom Optimizer
```

介面：

```text
Observation Batch
Action Batch
Reward Batch
Episode State
Metrics
Checkpoint
```

Trainer Plugin：

```text
Tool / Training only
```

Shipping 不包含。

---

# 六十四、ML Runtime Backend Plugin

推論 backend：

```text
ML.Runtime.ONNX
ML.Runtime.Custom
ML.Runtime.AcceleratorVendor
```

透過：

```text
IPolicyRuntimeBackend
```

AI Framework 只看到：

```text
PolicyHandle
ObservationView
ActionView
```

不看到 vendor tensor object。

---

# 六十五、Simulation Farm Provider

可替換：

```text
SimulationFarm.Local
SimulationFarm.Kubernetes
SimulationFarm.CloudVendor
SimulationFarm.Custom
```

Coordinator 只使用：

```text
Worker Capability
Job Descriptor
Artifact URI
Metrics Stream
```

不綁單一 orchestration 平台。

---

# 六十六、Ray Tracing / Mesh Shader Feature Plugin

第三方可提供：

```text
RT GI
RT Reflection
Custom Denoiser
Mesh Shader Geometry Path
Work Graph Pipeline
```

仍透過：

```text
Render Feature Registry
RenderGraph
Shader Metadata
Resource Registry
```

而不是 vendor-specific private path 直接污染 Renderer Core。

---

# 六十七、Server Plugin Rolling Deployment

Server 端大型 Plugin 不強求 process 內 hot reload。

正式支援：

```text
Plugin / Server Build N
↓
Deploy N+1
↓
Drain Region / Session
↓
Persist or Handoff State
↓
Route New Traffic
↓
Retire N
```

這是 V3 MMO / distributed server 的主要升級方式。

---

# 六十八、Plugin Privilege / Capability Model

V3 可為 Plugin Manifest 加：

```text
Privileges
```

例如：

```text
Filesystem.ReadProject
Filesystem.WriteCache
Network.Client
Network.Server
GPU.Compute
Editor.ModifyDocument
Build.RunExternalTool
```

Editor / Tool Plugin 只能使用宣告並被允許的 capability。

目標不是完整 OS sandbox，而是：

```text
明確能力
可稽核
可在企業環境限制
```

---

# 六十九、Plugin Trust Level

可區分：

```text
BuiltIn
Verified
ProjectLocal
ThirdParty
UntrustedTool
```

影響：

```text
Allowed Privileges
Native Binary Loading
External Process
Network Access
Editor Automation
```

Native Runtime Plugin 仍視為高權限 code，不宣稱可安全 sandbox 任意惡意 native binary。

---

# 七十、Plugin Store / Registry Boundary

若未來提供 Plugin Registry：

```text
Registry Metadata
Package Signature
Version
Engine Compatibility
Platform
License
Dependency
```

Package 安裝與 Runtime Loading 分離。

Registry 不應有權：

```text
遠端下載後直接把 Native Plugin 注入正在運行的 Shipping Game
```

Shipping native code 更新仍走正式應用程式更新流程。

---

# 七十一、Plugin Cost Analyzer V3

Build Size Analyzer 額外輸出：

```text
Plugin
├─ Executable Size
├─ ThirdParty Size
├─ Shader Size
├─ Asset Size
├─ Runtime Memory
├─ Startup Cost
├─ GPU Pass Cost
└─ Transitive Dependencies
```

能追：

```text
為何啟用 A Plugin 後多了 350 MB？
```

以及：

```text
A
→ B
→ Vendor SDK C
→ Shader Family D
```

---

# 七十二、V3 Third-party Plugin Gate

額外 Gate：

```text
12. Distributed World Provider 可替換 cloud / region backend。
13. Deterministic Plugin 可通過 repeat-run state hash 驗證。
14. GPU Simulation Plugin 必須走 RenderGraph / resource lifetime contract。
15. ML Training Algorithm 可作 Tool Plugin 且不進 Shipping。
16. ML Runtime backend 不暴露 vendor tensor type 到 AI Core。
17. Simulation Farm 可替換 orchestration provider。
18. Server Plugin 支援 rolling deployment workflow。
19. Plugin privilege / trust metadata 可被 Editor / Enterprise policy 驗證。
20. Build Size Analyzer 能顯示 Plugin transitive cost。
```


# 五十八、V3 最終原則

```text
V3
≠ Bigger Default Build
```

而是：

```text
V3
= Larger Capability Library
```

專案仍然只帶自己需要的東西。

```text
Small Game
→ Small Build

Open World
→ Enable World / HLOD / GPU-driven

Multiplayer
→ Add Network

MMO
→ Add Distributed World

Deterministic Game
→ Add Deterministic Domain

Next-gen PC
→ Add RT / Mesh Shader / GPU Simulation

AI Training
→ Add ML / Simulation Farm
```

這是 V3 模組化架構的核心目標。


---

# Appendix — V2 / V1 Contract Reference

V2（與其內含的 V1）Master Baseline 不再於本文件內複製全文，避免內容與來源文件分岔、需要人工重複同步（尤其 V2 文件本身也內嵌了 V1 的完整複製，若在此再複製一次等於疊了兩層需要同步的內容）。V3 完整繼承 V2 與 V1 的所有 Architecture Contract（見「一、V3 不改變的核心哲學」），完整內容請參閱來源文件本身：

```text
《跨平台3D_Engine_V2_完整規劃書》
→ 目前對應 Master Draft v1.3
→ 含 Rollback + Streaming 邊界宣告、Appendix 同步聲明

《跨平台3D_Engine_V2_AI施工技術與系統規劃》
→ 目前對應 AI Technical Draft v1.2

《跨平台3D_Engine_V1_完整規劃書》
→ 目前對應 Master Draft v1.2
→ 含 Character Framework 的 Terrain Streaming Boundary Contract

《跨平台3D_Engine_V1_AI施工技術與系統規劃》
→ 目前對應 AI Technical Draft v1.2
```

V2 Plugin / Feature Module 模組化規劃、V2 Media/Video Expansion、V1 Plugin/Third-party SDK/Video & Media 詳細 Contract、V1 Master Definition of Done，同樣請見對應來源文件本身，不在此複製。

閱讀順序建議：施工或審閱本文件前，先依序確認 V1 → V2 完整規劃書是否為最新版本；本文件（V3）只描述新增與變更的部分，未在本文件提及的既有系統一律以 V1 / V2 文件為準。
