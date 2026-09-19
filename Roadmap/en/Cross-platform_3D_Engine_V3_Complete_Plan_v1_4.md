# Cross-Platform 3D Engine — V3 Complete Planning Document

**Document Version: Master Draft v1.4**
**Engine Generation: V3.x — Distributed / Simulation / Next-Gen**

> V3 is built on the complete V1 + V2. V3 does not turn all new capabilities into Mandatory features; instead, it increases the “optional capability ceiling.”
>
> V3 Core Directions:
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

# I. Core Philosophies Unchanged in V3

```text
Capability
≠ Mandatory Workflow
```

Therefore:

```text
V3
≠ Ray Tracing required on all platforms
≠ All Physics moved to GPU
≠ All Gameplay deterministic
≠ All Games including MMO runtime
≠ Shipping Client including ML Trainer
```

Still retained:

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

# II. V3 Scope Matrix

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
GPU Navigation / Flow Field             △
Distributed Build / DDC V3             ✅
```

---

# III. Distributed World Topology

V2 Dedicated Server typically:

```text
One Server Process
→ One or N Runtime Worlds
→ Session authority
```

V3 MMO:

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

Formal identifiers:

```text
GlobalWorldID
RegionID
ServerNodeID
NetworkEntityID
PersistentEntityID
```

All are separate.

---

# IV. Global Identity

V3 adds:

```text
GlobalEntityID
```

Uses:

```text
Persistent online identity
Cross-region handoff
Persistence
Audit / replay
```

Formally:

```text
GlobalEntityID
≠ Local EntityID
≠ Scene UUID
≠ NetworkEntityID
```

Local Runtime:

```text
GlobalEntityID
↓ mapping
EntityID
```

Local EntityID must never be transmitted across servers.

---

# V. Region Partition

A Server Region does not need to correspond 1:1 with a Client Streaming Cell.

```text
Client Cell
→ Residency / Presentation

Server Region
→ Simulation Authority
```

Dynamic allocation may be based on:

```text
Spatial Area
Population Density
Gameplay Zone
Load
Instance Boundary
```

The first V3 version adopts:

```text
Stable Region Identity
+
Dynamic Region → Server Assignment
```

Region identity is not changed arbitrarily at runtime.

---

# VI. World Directory

Establish:

```text
IWorldDirectory
```

Stores:

```text
RegionID → ServerNode
Server Health
Region Epoch
Authority Lease
Routing Metadata
```

The Directory does not store all Gameplay Entity state.

Implementations may include:

```text
Single-process dev backend
Replicated service provider
Cloud provider
Custom MMO backend
```

---

# VII. Authority Lease

Each Region has:

```text
AuthorityLease
├─ RegionID
├─ OwnerServer
├─ Epoch
├─ Expiry / Heartbeat
└─ TransitionState
```

The Epoch prevents:

```text
Old Server after partition
→ continues writing stale authority
```

All cross-region authority messages include:

```text
Region Epoch
```

---

# VIII. Entity Handoff

When a player / NPC crosses a Region:

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

Handoff state contains only an explicit schema:

```text
Transform
Velocity
Gameplay State
Inventory Ref
Quest Ref
Prediction State
Relevant transient state
```

Runtime Entity memory must not be copied using `memcpy`.

Terrain Streaming Boundary integration: before `Authority Epoch / Ownership Switch` is executed, the Target Region must confirm that the character’s Occupied Cell Set (following the Terrain Streaming Boundary Contract of the Character Framework) has, on the Target side, at least entered a loading state compatible with `StreamingPending`; if the Target is not yet Ready, the Handoff must delay the Ownership Switch. The character must retain its existing Authority and Collision on the Source Region side, and a period in which “neither side has Collision” must not occur due to the cross-Region handoff. Cross-border collision approximation by a Border Ghost may serve as temporary buffering before handoff, where explicitly permitted, but must not replace the formal Ready determination.

---

# IX. Handoff Failure

Target unavailable:

```text
Prepare
↓
Target Reject / Timeout
↓
Source Retains Authority
```

If authority has already switched but the target crashes:

```text
Directory Epoch
+
Persistence / Transfer Journal
↓
Recovery Policy
```

Split authority writes are prohibited.

---

# X. Cross-Region Interaction

Cross-Region Gameplay does not directly share pointers / EntityID.

Use:

```text
CrossRegionMessage
```

Types:

```text
Intent
Event
RPC-like Command
State Summary
Interest Proxy
```

Long-distance communication:

```text
eventual / asynchronous
```

When precise interaction is required near a boundary:

```text
Border Ghost / Proxy
```

---

# XI. Border Ghost

Adjacent Regions may maintain:

```text
Ghost Entity
```

Stores only:

```text
Position
Bounds
Velocity
Relevant Gameplay Summary
```

Ghost:

```text
not authoritative
```

Uses:

```text
Visibility
Interest
Pre-handoff
Cross-border collision approximation where explicitly allowed
```

Core gameplay authority still belongs to only one Region.

---

# XII. Global Interest Federation

Client Interest:

```text
Player / Camera / Party / Quest
↓
Local Region Interest
↓
Federated Interest Router
↓
Relevant Adjacent / Remote Region summaries
```

The remote world does not send a complete replication stream to every client.

Supported:

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

# XIII. Gateway / Session Routing

V3 Server Roles:

```text
Gateway
Session / Auth Adapter
World Directory
Region Server
Persistence Service Adapter
Telemetry / Audit
```

The Gateway only handles:

```text
Connection Termination
Routing
Rate / Abuse policy hooks
Session mapping
```

It does not assume complete world simulation.

---

# XIV. Persistence Model

V3 MMO Persistence does not save the entire server memory image.

Model:

```text
Authoritative Snapshot
+
Event / Delta Journal
+
Versioned Schema
```

Persistent Entity:

```text
GlobalEntityID
Persistent Component Set
Revision
```

Transient presentation data is not written to the DB.

---

# XV. Checkpoint / Journal

Region:

```text
Periodic Checkpoint
+
Incremental Journal
```

Recovery:

```text
Load checkpoint
↓
Replay committed journal
↓
Resume Region
```

Required:

```text
Monotonic Region Revision
Idempotent Operation ID
```

This prevents duplicate rewards / duplicate transactions during replay.

---

# XVI. Economy / Transaction Boundary

V3 Engine does not include MMORPG economy rules by default.

However, it provides:

```text
Transactional Persistence Hook
Idempotency Key
Authoritative Revision
Audit Event
```

Whether Inventory / currency requires strong transactions is determined by the Game Backend.

The Gameplay thread must not perform blocking SQL.

---

# XVII. Region Failover

Formal first-version capabilities:

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

V3 does not guarantee zero-second failover.

Goals:

```text
No split brain
Recoverable world state
Explicit reconnect / migration policy
```

---

# XVIII. Rolling Deployment

Server native code updates do not use arbitrary runtime DLL hot swapping.

Formal process:

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

When different builds temporarily coexist:

```text
Protocol Compatibility Window
```

This must be explicitly declared.

---

# XIX. Distributed Observability

All distributed events include:

```text
TraceID
RegionID
ServerNodeID
GlobalEntityID optional
ConnectionID
BuildID
Epoch
```

Traceable flow:

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

# XX. Deterministic Simulation Domain

V3 does not implement Full Engine Determinism.

Formal structure:

```text
World
├─ Deterministic Simulation Domain
└─ General Runtime / Presentation Domain
```

Only explicitly registered Systems / Components enter the deterministic domain.

---

# XXI. Deterministic State Schema

Each deterministic component must have:

```text
Stable TypeID
Stable PropertyID
Explicit Serialization
Canonical Byte Order
No Raw Pointer
No Unordered Iteration
```

State Hash:

```text
Canonical deterministic state
↓
Hash
```

Used for:

```text
Desync Detection
Replay Validation
Rollback Test
Self-play Reproduction
```

---

# XXII. Deterministic Tick

```text
Fixed Tick
Integer TickIndex
Deterministic Timer
Deterministic RNG
InputCommand per Tick
```

A deterministic System is prohibited from reading:

```text
Wall Clock
Render Delta
OS Random
Unordered thread timing
GPU simulation result
```

---

# XXIII. Deterministic Math

Provide:

```text
Deterministic.Math
```

May include:

```text
Fixed-point
Deterministic integer vector
Controlled float profile where verified
```

Formal policy:

```text
Cross-platform lockstep-critical state
→ fixed/integer preferred

Presentation / general gameplay
→ normal float
```

The entire engine is not forced to use fixed-point.

---

# XXIV. Deterministic Scheduler

The Deterministic Domain permits parallelism, but results must be independent of thread scheduling.

Strategies:

```text
Stable Work Partition
Stable Reduction Order
Command Buffer
Deterministic Merge
```

Prohibited:

```text
atomic race determines gameplay order
```

---

# XXV. Rollback

Rollback State:

```text
Tick Snapshot
Input History
Pending Deterministic Events
RNG State
```

Process:

```text
Receive correction
↓
Restore Tick T
↓
Replay Inputs T+1..Now
↓
Publish corrected state
```

Presentation:

```text
Visual smoothing
```

Separated from deterministic state.

The boundary between the Deterministic Domain and World Partition Streaming must be explicitly declared; otherwise, Rollback is easily broken:

```text
Deterministic Domain Streaming Policy
├─ Bounded / Pre-loaded（V3 V1 default）
│  → The spatial range covered by the Deterministic Domain is fully Resident before Tick 0
│  → Does not participate in subsequent Cell Unload/Evict
│  → Rollback requires no additional handling of Cell Residency history
│
└─ Streamed（Future / Opt-in, requires ADR）
   → Requires a Deterministic Residency Log: record Cell Residency snapshots per Tick
   → `Restore T` in Rollback must first restore the Residency state for the corresponding Tick before Replay Inputs
   → Otherwise, Cell Collision during Replay may differ from that at Tick T, breaking determinism
```

V3 V1 scope limitation: the Deterministic Domain uses the Bounded / Pre-loaded strategy by default. If a project requires the Deterministic Domain to cover a Streamed large world (V2 Large World / Adaptive Partition), it must go through the ADR process and implement a Deterministic Residency Log; it must not implicitly assume that Rollback is naturally compatible with Streaming.

---

# XXVI. Lockstep

```text
Input for Tick N
↓
Agreement / Deadline
↓
Simulate Tick N
↓
State Hash
```

Supported:

```text
Peer-hosted
Server-coordinated
```

However, V3 still recommends Server coordinated by default.

---

# XXVII. Deterministic Physics

Independent Plugin:

```text
Deterministic.Physics
```

Does not replace Jolt.

Suitable for:

```text
RTS
Rollback combat
Deterministic projectile / hit simulation
```

Complete feature parity with Jolt is not required.

---

# XXVIII. GPU Simulation Domains

V3 formally permits:

```text
GPU Authoritative Simulation Island
```

However, the authority scope must be explicit.

For example:

```text
Mass Debris
Fluid
SoftBody
Crowd Local Motion
Special RigidBody Island
```

Core characters / server gameplay may still be CPU authoritative.

---

# XXIX. GPU Physics Architecture

```text
GPUPhysicsWorld
├─ Broadphase
├─ Bodies
├─ Constraints
├─ Solver
├─ Query Acceleration
└─ Readback Summary
```

Submission:

```text
CPU Commands
↓
GPU Command Buffer
↓
RenderGraph Compute Passes
↓
GPU State
```

Each body must not perform an immediate round trip across the PCIe / unified memory boundary.

---

# XXX. GPU Authority Boundary

When CPU Gameplay needs to read a GPU-authoritative domain:

```text
GPU Summary
↓
Deferred Readback
↓
Next / Later Tick Consumption
```

The following is not allowed:

```text
Gameplay immediate query
↓
stall GPU
↓
same-frame result
```

Immediate gameplay queries continue to use the CPU-authoritative representation.

---

# XXXI. GPU Physics State Transfer

GPU island creation / destruction / migration uses:

```text
Spawn Batch
Destroy Batch
Parameter Update Batch
```

When necessary:

```text
GPU → CPU Snapshot
```

Used for:

```text
Save
Debug
Migration
Failure Recovery
```

This is not a per-frame path.

---

# XXXII. GPU Broadphase / Solver

V3 Built-in GPU physics may initially support a limited shape set:

```text
Sphere
Box
Capsule
Convex
Static Mesh proxy
SDF optional
```

All V1 Jolt features do not need to be ported at once.

Focus on:

```text
massive count
regular memory layout
batch processing
```

---

# XXXIII. GPU Fluid / SoftBody

As Optional Plugins:

```text
GPUPhysics.Fluid
GPUPhysics.SoftBody
```

They:

```text
do not become required Physics.Core dependency
```

---

# XXXIV. Hardware Ray Tracing Framework

V3 formally establishes:

```text
RTASManager
BLAS
TLAS
RayTracingPipeline
RayQuery
Denoiser Interface
RT Budget Manager
```

Renderer Core can still be fully Raster.

---

# XXXV. BLAS / TLAS

Static Mesh:

```text
Cooked RT Geometry Metadata
↓
BLAS Build / Cache
```

Dynamic:

```text
Refit
or
Rebuild
```

TLAS:

```text
Visible / Relevant RT Instances
↓
TLAS
```

RT residency is integrated with general mesh residency.

---

# XXXVI. RT Budget

Each profile:

```text
RT Instance Budget
BLAS Memory
TLAS Build Time
Ray Count
Bounce Count
Denoiser Budget
```

Low-end:

```text
RT disabled
```

Mid-range:

```text
RT Shadow / Reflection selective
```

High-end:

```text
RT GI / richer reflection
```

---

# XXXVII. RT Effects

First-class optional passes:

```text
RT Shadow
RT Reflection
RT AO
RT GI
```

Each can be:

```text
Off
Hybrid
Full
```

A Raster fallback must exist.

---

# XXXVIII. Path Tracer

Optional:

```text
Renderer.PathTracer
```

Uses:

```text
Editor reference
Look-dev
Photo Mode
Offline / high-end capture
Golden Image reference
```

Does not replace the realtime renderer.

---

# XXXIX. Denoiser Interface

```text
IDenoiser
```

May include:

```text
Engine temporal denoiser
Vendor plugin
Offline high-quality denoiser
```

The Renderer does not hard-code a vendor.

---

# XL. Mesh Shader First-class Path

V2 was a capability path; V3 upgrades it to first-class.

Asset:

```text
Mesh
↓
Cluster / Meshlet Cook
```

Runtime:

```text
Meshlet
├─ Mesh Shader Path
└─ Indexed Indirect Fallback
```

The same content must not become completely unexecutable without Mesh Shader support.

---

# XLI. Cluster Geometry

Optional:

```text
RenderFeature.ClusterGeometry
```

Hierarchy:

```text
Cluster Tree
↓
Projected Error
↓
GPU Select
↓
Visible Cluster
```

Coexists with:

```text
Object LOD
HLOD
```

Not all meshes are required to become virtualized cluster assets.

---

# XLII. Work Graph

```text
RenderFeature.WorkGraph
△
```

Used only when supported by backend capabilities.

Uses:

```text
GPU work generation
complex culling / material work
simulation scheduling
```

It must not become the sole cross-platform Renderer path.

---

# XLIII. Virtual Geometry

V3 may provide:

```text
RenderFeature.VirtualGeometry
```

But it remains Optional.

Cook:

```text
Source Mesh
↓
Cluster Hierarchy
↓
Streamable Geometry Pages
```

Runtime:

```text
Visibility
↓
Page Demand
↓
Geometry Streaming
```

Low-end profiles continue to use traditional Mesh LOD / HLOD.

---

# XLIV. ML Training Platform

V2 had only a Training Bridge.

V3 formally provides a Built-in Training Toolchain.

Architecture:

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

The Trainer is not included in the Shipping Client.

---

# XLV. Environment API

Each training environment:

```text
Reset(seed)
Step(action)
GetObservation()
GetReward()
GetDone()
GetMetrics()
```

Batch:

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

This reduces the number of calls across language / process boundaries.

---

# XLVI. Multi-World Headless

Within the same process:

```text
World 0
World 1
...
World N
```

Shared:

```text
Immutable Assets
Code
Read-only DataTable Generation
```

Separate:

```text
Entity Registry
PhysicsWorld
NavigationWorld
AI State
RNG
Time
```

Can run:

```text
Faster-than-realtime
```

---

# XLVII. Training Clock

Training Profile:

```text
No wall-clock pacing
```Simulation:

```text
Step Fixed Tick as fast as budget allows
```

Only create the rendering world / offscreen view when rendering observation is required.

---

# Forty-Eight, Observation System

Observation is not transmitted as ad-hoc JSON every tick.

Formal:

```text
ObservationSchema
↓
Packed Observation Buffer
```

Supports:

```text
Scalar
Vector
Discrete
Mask
Image / Tensor Handle optional
Entity Set / Attention-style packed set
```

---

# Forty-Nine, Action System

```text
ActionSchema
```

Supports:

```text
Discrete
MultiDiscrete
Continuous
Hybrid
Action Mask
```

Policy output is converted into:

```text
Gameplay Intent / AI Action
```

rather than directly modifying the Transform.

---

# Fifty, Reward / Metrics

The Engine provides the framework:

```text
Reward Channel
Episode Metric
Evaluation Metric
```

Specific rewards are defined by the Game / Training Plugin.

The Engine Core must not hard-code “Kill = +1”.

---

# Fifty-One, PPO / SAC / DQN

V3 official Trainer Plugins:

```text
ML.Training.PPO
ML.Training.SAC
ML.Training.DQN
```

They share:

```text
Replay / Rollout Storage
Checkpoint
Metrics
Evaluation
```

Algorithms can be versioned; training artifacts record:

```text
Trainer Version
Hyperparameters
Environment Build ID
Observation Schema
Action Schema
Seed
```

---

# Fifty-Two, Self-play

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

Avoid training only against the “latest self” forever, which can cause collapse.

Supports:

```text
Historical Opponent
League
Exploit / Main Policy role
Custom
```

---

# Fifty-Three, Curriculum

```text
Curriculum Stage
```

Can be adjusted according to:

```text
Success Rate
Episode Count
Metric Threshold
Manual Schedule
```

Adjusts:

```text
Map
Opponent
Difficulty
Observation noise
Rule parameters
```

---

# Fifty-Four, Evaluation

Training and Evaluation are separated.

Evaluation:

```text
Frozen Policy
Fixed Scenario Set
Known Seeds
No Exploration Noise
```

CI can compare:

```text
Win Rate
Score
Safety Metrics
Regression
```

---

# Fifty-Five, Policy Export

The Trainer outputs:

```text
Policy Artifact
```

Available formats:

```text
Engine-native optimized format
ONNX
Vendor backend format via plugin
```

Runtime:

```text
AI.Policy
↓
Policy Runtime Backend
↓
Action
```

---

# Fifty-Six, Simulation Farm

Large-scale training:

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

Worker capabilities:

```text
CPU
GPU
Memory
Build ID
Environment Set
```

---

# Fifty-Seven, Simulation Farm Fault Tolerance

Worker crash:

```text
Lease expires
↓
Episode discarded / reassigned
```

Checkpoint:

```text
Trainer Checkpoint
Policy Checkpoint
Curriculum State
Rating State
```

Perform periodic durable saves.

---

# Fifty-Eight, Dataset / Replay

The following can be written as a Dataset:

```text
Observation
Action
Reward
State Summary
```

Uses:

```text
Offline Evaluation
Behavior Cloning
Debug
Regression
```

Data formats are versioned.

---

# Fifty-Nine, Massive Crowd V3

Optional:

```text
Crowd.Massive
Crowd.FlowField
Crowd.GPUAvoidance
Crowd.AggregateSimulation
```

Near:

```text
Individual Character
```

Mid:

```text
Reduced individual
```

Far:

```text
Aggregate population state
```

Do not run complete Behavior Trees for tens of thousands of distant NPCs.

---

# Sixty, Aggregate Simulation

Region-level:

```text
Population Count
Faction State
Resource Flow
Travel Flow
Combat Aggregate
```

When an entity needs to approach or be observed:

```text
Materialize Individuals
```

When it leaves:

```text
Aggregate Back
```

The transition must use a deterministic / auditable policy to avoid duplicate entities.

---

# Sixty-One, GPU Navigation

Optional:

```text
GPU Flow Field
GPU Cost Field
GPU Crowd Neighbor Query
```

Does not replace Recast/NavMesh.

Suitable for:

```text
RTS
Massive Crowd
Large formation
```

---

# Sixty-Two, V3 Media

V3 inherits V2 Streaming / DRM / Capture.

Optional additions:

```text
Media.LowLatency
Media.WebRTC
Media.VideoCompositorGPU
Media.CloudStreamClient
```

However, these are not part of the V3 core focus.

Media remains an Optional Feature family.

---

# Sixty-Three, V3 Security

V3 adds distributed / plugin / remote tool risks.

Formal:

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

The Engine does not design cryptographic primitives itself.

---

# Sixty-Four, Plugin Privilege

Plugins may declare:

```text
Filesystem.ReadProject
Filesystem.WriteCache
Network.Client
Network.Server
GPU.Compute
Editor.ModifyDocument
Build.RunExternalTool
```

Native Plugins are still high-privilege code; Privilege is a management / audit boundary and does not claim to securely sandbox malicious native binaries.

---

# Sixty-Five, Distributed Build V3

V2 worker expansion:

```text
Build Coordinator
↓
Content-addressable Task
↓
Worker Pool
↓
Verified Artifact
```

Adds:

```text
Geometry Cluster Build
RT Data Build
ML Dataset Prep
Distributed World Validation
Server Package
```

---

# Sixty-Six, Artifact Provenance

Each important build artifact records:

```text
Source Hash
Tool Version
Plugin Version
Platform Profile
Dependency Hash
Worker Build ID
Output Hash
```

Ensures:

```text
Reproducible
Auditable
Cacheable
```

---

# Sixty-Seven, Build Size Analyzer

V3 official tool:

```text
Executable
Plugin
ThirdParty
Shader
Asset
Localization
Symbols
```

Can display:

```text
Transitive Dependency Tree
```

Answers:

```text
Why did enabling this plugin add 350MB?
```

---

# Sixty-Eight, Runtime Plugin Trust

Categories:

```text
BuiltIn
Verified
ProjectLocal
ThirdParty
ToolOnly
```

Affects:

```text
Editor Warning
Privilege
Package validation
Distribution policy
```

---

# Sixty-Nine, Telemetry / Privacy Boundary

Telemetry Provider is Optional.

The Engine Core only defines:

```text
Metric
Trace
Crash
Audit
```

Actual upload destination / consent / retention are determined by the Product / Provider.

---

# Seventy, V3 Build Profiles

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

Different profiles have completely different module sets.

---

# Seventy-One, MMO Region Server Profile

Include:

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

Strip:

```text
Renderer
Runtime UI
Audio Output
VFX presentation
WebView
Media
```

---

# Seventy-Two, Gateway Profile

Include:

```text
Network Transport
Session
Routing
Auth Provider
Rate Limit
Directory Client
Telemetry
```

Not required:

```text
Physics
Navigation
Renderer
Gameplay World
```

---

# Seventy-Three, Training Headless Profile

Include:

```text
World
Gameplay
AI
ML Training Bridge
Physics / Navigation as project needs
Replay
Metrics
```

Strip presentation.

---


---

# V3 Implementation Milestones

> V3 is established on the premise that the **V2 Production Gate has passed**.
> The greatest risks in V3 come from “Distributed Authority, Determinism, GPU Authority, and ML Training”; therefore, the implementation order must first establish boundaries and verifiability before expanding scale.

## V3-M0 — Compatibility / Capability Baseline

Implementation:

```text
V2 → V3 Migration Scanner
Plugin ABI / Privilege Audit
Deterministic-capable System Audit
Server Protocol Compatibility Audit
GPU Capability Matrix
TrainingHeadless Baseline
```

**Gate:**

```text
✓ V2 Projects can still build without enabling V3 features
✓ V3 optional modules do not noticeably increase the V2 baseline footprint
✓ The capability matrix can determine the RT / Mesh Shader / GPU Physics path
```

---

## V3-M1 — Deterministic Core

Implement first:

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

**Gate:**

```text
✓ Same Build + Input + Seed → same State Hash
✓ Does not depend on wall clock / global random / unordered iteration
✓ CI can rerun deterministic scenarios
```

---

## V3-M2 — Rollback / Lockstep / Deterministic Physics

Implementation:

```text
Snapshot
Input History
Restore / Replay
Rollback Window
Lockstep Tick Agreement
Desync Detection
Deterministic Physics Plugin foundation
```

**Gate:**

```text
✓ Rollback can restore a specified tick
✓ Lockstep desync can be localized
✓ Deterministic Physics can be independently enabled or disabled
✓ General Jolt runtime is not forced to be deterministic
```

---

## V3-M3 — Distributed World Foundation

Implementation:

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

Do not implement complete Handoff yet.

**Gate:**

```text
✓ Region authority is unique
✓ Stale epoch messages are rejected
✓ The Directory backend is replaceable
✓ Local EntityID is never persisted across servers
```

---

## V3-M4 — Handoff / Gateway / Persistence / Recovery

Order:

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

**Gate:**

```text
✓ Handoff failure can return to the source authority
✓ Node kill does not produce dual authority
✓ Checkpoint + Journal can recover a Region
✓ N / N+1 server builds can be rolling-deployed according to the compatibility window
```

---

## V3-M5 — GPU Simulation Foundation

Implementation:

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

Formally restricted to:

```text
GPU Authoritative Simulation Island
≠ Entire Gameplay Physics
```

**Gate:**

```text
✓ No per-body same-frame CPU roundtrip
✓ Gameplay immediate queries do not stall the GPU
✓ GPU state can be snapshotted / migrated on demand
✓ Jolt CPU-authoritative domains continue to work normally
```

---

## V3-M6 — GPU Fluid / SoftBody / Destruction Extensions

Optional implementation:

```text
GPU Fluid
GPU SoftBody
GPU Destruction
GPU Debris
```

**Gate:**

```text
✓ Each Extension can be stripped independently
✓ Does not become a Physics.Core dependency
✓ Gameplay coarse state can be separated from visual detail
```

---

## V3-M7 — Ray Tracing / Path Tracing

Implementation:

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

**Gate:**

```text
✓ Raster fallback is complete
✓ BLAS / TLAS are not created when RT is disabled
✓ RT memory / build budgets can be profiled
✓ Path Tracer can serve as a reference / offline / photo mode
```

---

## V3-M8 — Mesh Shader / Cluster Geometry / Virtual Geometry

Implementation:

```text
Meshlet / Cluster Cook
Mesh Shader Path
Indexed Indirect Fallback
Cluster Hierarchy
Geometry Page Streaming optional
Work Graph experimental
```

**Gate:**

```text
✓ The same asset supports Mesh Shader / fallback
✓ Platforms that do not support Work Graph can omit compilation completely
✓ Cluster streaming does not damage HLOD / traditional LOD
```

---

## V3-M9 — ML Training Runtime Foundation

Implement first:

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

Do not implement PPO first.

**Gate:**

```text
✓ N Worlds can run in the same process
✓ Reset / Step / Observation / Action batching is available
✓ Seed reproducibility is supported for environments supporting determinism
✓ The Training Profile has no Renderer / UI / Audio baggage
```

---

## V3-M10 — PPO / SAC / DQN / Self-play

Implementation:

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

**Gate:**

```text
✓ Checkpoint resume
✓ Frozen Evaluation is separated from Training
✓ Policy artifacts record schema / build / trainer version
✓ Runtime Policy can be loaded by the AI.Policy backend
```

---

## V3-M11 — Simulation Farm / Massive Crowd / GPU Navigation

Implementation:

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

**Gate:**

```text
✓ Worker crashes do not corrupt trainer state
✓ Episodes can be discarded / reassigned
✓ Far populations do not require complete per-agent AI
✓ Aggregate ↔ individual does not duplicate entities
```

---

## V3-M12 — Security / Observability / Plugin Trust / Build Provenance

Implementation:

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

**Gate:**

```text
✓ Region / Gateway / Plugin operations can be audited
✓ Native Plugin privilege metadata can be validated
✓ Build artifacts can be traced to source / tool / plugin / worker hashes
✓ It is possible to explain why a feature/plugin increased package size
```

---

## V3-M13 — Distributed / GPU / ML Production Hardening

Reference Projects:

```text
Distributed Continent
Deterministic RTS
GPU Physics Sandbox
RT Showcase
Self-play Arena
```

Fault Injection:

```text
Node Kill
Network Partition
Stale Epoch
Handoff Timeout
GPU Device Stress
Worker Loss
Checkpoint Recovery
```

**Gate:**

```text
✓ 24h+ distributed soak
✓ No split-brain authority
✓ Determinism repeat hash passes
✓ GPU simulation has no hidden synchronous readback
✓ ML checkpoint / farm recovery passes
✓ Raster / non-ML / non-MMO projects are not polluted by V3 heavyweight features
```

---

## V3 Implementation Dependency Graph

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


# Seventy-Four, V3 Development Phases

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

# Seventy-Five, Phase V3-A — Deterministic Foundation

Contents:

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

Gate:

```text
Same input + seed + build
→ same deterministic state hash
```

Repeat testing across supported platforms.

---

# Seventy-Six, Phase V3-B — Distributed World

Contents:

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

Gate:

```text
No split brain
Handoff rollback safe
Server restart recoverable
```

---

# Seventy-Seven, Phase V3-C — GPU Simulation

Contents:

```text
GPUPhysicsWorld
Broadphase
Solver
Batch Spawn / Update
Deferred Readback
GPU Fluid / SoftBody hooks
```

Gate:

```text
No same-frame gameplay readback dependency
GPU state lifetime fence-safe
CPU authoritative world remains functional
```

---

# Seventy-Eight, Phase V3-D — Next-gen Renderer

Contents:

```text
RTAS
RT Shadow / Reflection
Denoiser
Path Tracer
Mesh Shader
Cluster Geometry
Work Graph experiment
```

Gate:

```text
Raster fallback remains complete
No platform becomes unsupported solely due RT/MS absence
```

---

# Seventy-Nine, Phase V3-E — ML Training

Contents:

```text
Vectorized Environment
Observation / Action Schema
PPO / SAC / DQN
Self-play
Curriculum
Evaluation
Policy Export
```

Gate:

```text
Headless N-world run
Checkpoint resume
Deterministic seed reproduction where environment supports
Runtime export loads through AI.Policy
```

---

# Eighty, Phase V3-F — Simulation Farm

Contents:

```text
Coordinator
Worker
Lease
Dataset
Metrics
Fault tolerance
```

Gate:

```text
Worker loss does not corrupt trainer state
Artifacts tied to Build ID / schema
```

---

# Eighty-One, Phase V3-G — MMO Hardening

Contents:

```text
Region Failover
Rolling Deploy
Gateway Scale
Persistence Load
Cross-region soak
Security / abuse hooks
```

Gate:

```text
24h+ distributed soak
Repeated handoff
Node kill test
No duplicate authority
```

---

# Eighty-Two, Phase V3-H — Production Hardening

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

# Eighty-Three, V3 Reference Projects

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

# Eighty-Four, V3 CI

Adds:

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

# Eighty-Five, V3 Non-Goals

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

# Eighty-Six, V3 Definition of Done

```text
1. V2 Projects can be upgraded and retain the existing architecture when V3 plugins are not enabled.
2. Distributed World supports Region authority / handoff / recovery.
3. Region epoch prevents split-brain stale authority.
4. Deterministic Domain passes repeat-run hash validation.
5. Rollback / Lockstep are Optional Domains and do not contaminate the general runtime.
6. A GPU authoritative simulation island can exist without forcing gameplay physics onto the GPU.
7. The raster renderer remains fully usable on hardware without RT / Mesh Shader support.
8. RT shadow / reflection have a production path.
9. Path tracer can serve as an optional editor/reference path.
10. Mesh Shader has an indexed-indirect fallback.
11. The built-in Trainer can execute PPO / SAC / DQN.
12. Self-play / Curriculum / Evaluation are reproducible and support checkpointing.
13. TrainingHeadless can run multiple Worlds in the same process.
14. Simulation Farm worker failures can be recovered.
15. MMO Region / Gateway / Client use different build profiles.
16. ML Trainer / distributed server tooling is not included in ordinary clients.
17. Plugin / Build Size Analyzer can trace transitive costs.
18. All V3 reference projects pass soak / CI.
```---

# Eighty-Seven, V3 Final Architecture

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

The most important sentence for V3:

```text
A higher capability ceiling
does not mean a higher default cost.
```


---

# V3 Plugin / Third-party SDK Detailed Contract（Normative）

# Cross-platform 3D Engine — V3 Plugin / Feature Module Modularization Plan

**Document Version: Draft v1.1**
**Corresponding Engine Generation: V3.x**
**Positioning: Distributed / Simulation / Next-Gen。**

---

# I. V3 Modularization Goals

V3 will add the most demanding capabilities:

```text
Distributed MMO
Deterministic Simulation
GPU Physics
Ray Tracing
Mesh Shader / Work Graph
ML Training
Massive Headless Simulation
```

Therefore, the principle of V3 is not:

```text
V3 Build = All Next-gen features are present
```

Instead:

```text
V3 Repository
→ Provides capabilities

Project
→ Selects only the capabilities it needs
```

---

# II. V3 Core Principles

Continuing from V1 / V2:

```text
Feature exists in Repository
≠
Feature exists in Build
```

Formally:

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

# III. V3 Does Not Make Next-gen Features Mandatory

Even in V3:

```text
Ray Tracing
Mesh Shader
GPU Physics
Deterministic Simulation
ML Runtime
Distributed World
```

Still:

```text
Capability
≠ Mandatory Workflow
```

---

# IV. Distributed World Plugins

Split into:

```text
DistributedWorld.Core
DistributedWorld.Directory
DistributedWorld.Region
DistributedWorld.Handoff
DistributedWorld.Federation
DistributedWorld.Recovery
```

Provided only for:

```text
MMO
Large persistent online world
Distributed simulation
```

usage.

For general single-player / session multiplayer:

```text
Strip all
```

---

# V. MMO Gateway Plugins

```text
MMO.Gateway
MMO.RegionRouter
MMO.AuthorityTransfer
MMO.SessionTransfer
```

Server-only.

The client only needs:

```text
Network routing protocol support
```

It does not include the server implementation.

---

# VI. Distributed Authority

```text
DistributedWorld.Authority
```

Functions:

```text
Region Ownership
Entity Authority
Authority Lease
Transfer
Recovery
Conflict Detection
```

When MMO is not used:

```text
strip
```

---

# VII. Region Failover

```text
DistributedWorld.Failover
```

Production MMO server-only.

It may be omitted from local development and general dedicated servers.

---

# VIII. Persistence Providers

```text
Persistence.Core
Persistence.SQLProvider
Persistence.CloudProvider
Persistence.Custom
```

The Engine does not bind to:

```text
MySQL
PostgreSQL
MongoDB
Cloud vendor
```

Specific backends are all Provider Plugins.

---

# IX. Fleet / Orchestration Plugins

```text
ServerFleet.Telemetry
ServerFleet.Health
ServerFleet.OrchestrationAdapter
```

Providers:

```text
Kubernetes
Cloud Provider
Custom Infrastructure
```

The Engine Core does not depend on them.

---

# X. Deterministic Simulation Plugins

```text
Deterministic.Core
Deterministic.Math
Deterministic.Random
Deterministic.Timer
Deterministic.Rollback
Deterministic.Lockstep
Deterministic.Physics
```

They can be combined as required.

For example:

```text
Fighting Game
→ Rollback

RTS
→ Lockstep

Self-play
→ Core + Random + Timer
```

---

# XI. Deterministic Physics

```text
Deterministic.Physics
```

Independent of:

```text
Physics.Jolt
```

Formal principle:

```text
General Physics
→ Jolt

Deterministic Domain
→ Deterministic Physics Plugin
```

The entire Engine is not forcibly converted to fixed-point arithmetic.

---

# XII. Rollback Plugin

```text
Deterministic.Rollback
```

Provides:

```text
State Snapshot
Input History
Re-simulation
Rollback Window
State Hash
```

Manages only the deterministic domain.

Does not roll back:

```text
Renderer
Audio
GPU VFX
WebView
```

---

# XIII. Lockstep Plugin

```text
Deterministic.Lockstep
```

Provides:

```text
Tick Agreement
Input Command
Hash Check
Desync Detection
Recovery Hook
```

Can be used by strategy games / RTS.

---

# XIV. GPU Physics Plugins

```text
GPUPhysics.Core
GPUPhysics.Broadphase
GPUPhysics.RigidBody
GPUPhysics.Constraint
GPUPhysics.Query
```

Provided only for massive GPU simulation projects.

Formal principle:

```text
GPUPhysics Domain
≠ automatically Gameplay Truth
```

The project can explicitly determine whether a given simulation island is GPU-authoritative.

---

# XV. GPU Fluid Plugin

```text
GPUPhysics.Fluid
```

Completely Optional.

May use:

```text
SPH
Grid Fluid
Hybrid
```

But it is not placed in Core.

---

# XVI. GPU SoftBody Plugin

```text
GPUPhysics.SoftBody
```

Optional.

---

# XVII. GPU Destruction V3

```text
GPUPhysics.Destruction
```

For large-scale fragmentation / fracture simulation.

Coarse Gameplay destruction state is separated from visual GPU fragments.

---

# XVIII. Ray Tracing Plugins

```text
RenderFeature.RayTracing
```

Further split into:

```text
RT.Shadow
RT.Reflection
RT.AO
RT.GI
RT.RayQuery
```

Without RT:

```text
BLAS / TLAS Manager
RT shader
RT pipeline
```

none of these enter the build.

---

# XIX. Path Tracing Plugin

```text
Renderer.PathTracer
```

Typical uses:

```text
Editor
Offline Preview
Photo Mode
High-end PC
```

It is not required to be carried by general Shipping games.

---

# XX. Mesh Shader Plugin

```text
RenderFeature.MeshShader
```

In V3, it can be a first-class renderer path, but remains Optional.

Fallback:

```text
Meshlet
↓
Indexed Indirect
```

---

# XXI. Work Graph Plugin

```text
RenderFeature.WorkGraph
```

Experimental / Capability-dependent.

For unsupported backends:

```text
Do not compile
```

It does not change core asset compatibility.

---

# XXII. Cluster Geometry Plugin

```text
RenderFeature.ClusterGeometry
```

Uses:

```text
Hierarchical Geometry
GPU Culling
Fine-grain LOD
Large static world
```

Can coexist with traditional:

```text
Mesh LOD
HLOD
```

---

# XXIII. Advanced Virtual Geometry

If needed later in V3, it can be:

```text
RenderFeature.VirtualGeometry
```

But still:

```text
Optional
```

The entire Mesh Pipeline is not forcibly converted to a single virtualized format.

---

# XXIV. ML Training Platform Plugins

Split into:

```text
ML.Training.Core
ML.Training.PPO
ML.Training.SAC
ML.Training.DQN
ML.Training.SelfPlay
ML.Training.Curriculum
ML.Training.Evaluation
```

All belong to:

```text
Tool / Training Build
```

Shipping Client:

```text
Trainer = 0 byte
```

---

# XXV. ML Runtime Plugins

```text
ML.Runtime.Core
ML.Runtime.ONNX
ML.Runtime.Custom
```

If Shipping only requires inference:

```text
ML.Runtime.*
```

The Trainer is not included.

---

# XXVI. PyTorch / JAX Bridge

```text
ML.Bridge.PyTorch
ML.Bridge.JAX
```

Only for:

```text
Development
Training
```

They do not enter the Shipping Runtime.

---

# XXVII. Simulation Farm Plugins

```text
SimulationFarm.Coordinator
SimulationFarm.Worker
SimulationFarm.Telemetry
SimulationFarm.Dataset
```

Uses:

```text
Self-play
RL Training
Massive simulation
Automated balance
Regression
```

General game builds do not need them at all.

---

# XXVIII. Training Headless Meta-Plugin

```text
BuildProfile.TrainingHeadless
```

Typical:

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

# XXIX. Massive Crowd Plugins

```text
Crowd.Massive
Crowd.FlowField
Crowd.GPUAvoidance
Crowd.AggregateSimulation
```

Required only by:

```text
MMO
RTS
City Simulation
Large Crowd
```

---

# XXX. GPU Navigation Plugins

```text
Navigation.GPUFlowField
Navigation.GPUCostField
Navigation.GPUCrowdQuery
```

Optional.

General NavMesh projects do not need them.

---

# XXXI. Aggregate Simulation Plugin

```text
Simulation.Aggregate
```

Provides:

```text
Far Population Simulation
Region-level Economy State
Abstract Battle Resolution
Resource Flow
```

However, the specifics of:

```text
Economy
Faction
Trade
Population Rule
```

are implemented by Game Plugins and are not hard-coded into the Engine.

---

# XXXII. Ray-traced Audio Plugin

```text
Audio.RayTracing
```

High-end Optional.

General:

```text
Audio.RoomPortal
```

can still exist independently.

---

# XXXIII. Distributed Replay / Observability

```text
Distributed.Trace
Distributed.Replay
Distributed.Audit
```

Provided only for server infrastructure.

---

# XXXIV. Server Security Plugins

```text
Server.Security.Auth
Server.Security.Attestation
Server.Security.RateLimit
Server.Security.AbuseDetectionHook
```

Provider / server-only.

Third-party security SDKs are not written into Core.

---

# XXXV. V3 Renderer Feature Registry

The V3 renderer module still follows:

```text
Build / Init
↓
Register Render Feature
↓
RenderGraph Pass Factory
↓
Compiled Runtime Pipeline
```

Prohibited:

```text
per-draw plugin virtual dispatch
```

---

# XXXVI. GPU Physics Module Boundary

Public:

```text
Handle
POD
Batch Command
Batch Query
```

Internal:

```text
GPU buffers
Solver
Broadphase
Constraint graph
```

Backend native resources must not be exposed to the Gameplay ABI.

---

# XXXVII. Deterministic Domain Boundary

```text
World
├─ Deterministic Domain
└─ Presentation / General Runtime Domain
```

The Deterministic Plugin controls only explicitly registered:

```text
Components
Systems
RNG
Timers
InputCommands
Snapshot State
```

Other systems are not dragged into bitwise determinism.

---

# XXXVIII. Distributed World Boundary

```text
Global World Identity
↓
Region
↓
Authority Node
↓
Local Runtime World / Region Runtime
```

Across Regions:

```text
Entity Handoff
```

rather than sharing raw EntityID.

---

# XXXIX. Network / MMO Separation

V3 still separates:

```text
Network.Core
```

from:

```text
DistributedWorld.*
```

Therefore:

```text
MOBA
Co-op
Arena
```

need only Network and do not need the MMO mesh.

---

# XL. ML / AI Separation

AI can still use:

```text
Behavior Tree
Utility
GOAP
Learned Policy
```

The ML Trainer does not cause AI.Core to depend on PyTorch.

---

# XLI. Tool-only / Runtime-only Separation

For example:

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

Different Profiles are not mixed together.

---

# XLII. V3 Build Profiles

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

Each profile has a completely different Plugin Set.

---

# XLIII. V3 Example: General Single-player RPG

Even when the Engine has reached V3:

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

are sufficient.

No need at all for:

```text
DistributedWorld
GPUPhysics
RayTracing
ML
Deterministic
WorkGraph
```

This is the most important value of V3 Plugin modularization.

---

# XLIV. V3 Example: High-end PC RPG

The following can be added:

```text
RT.Reflection
RT.Shadow
MeshShader
ClusterGeometry
GPUCloth
```

But the following are not needed:

```text
MMO
ML Trainer
Deterministic Physics
```

---

# XLV. V3 Example: MMO

Client:

```text
Network.Core
Network.Replication
WorldPartition
HLOD
UI
Audio
Renderer
```

Region Server:

```text
Network.Core
DistributedWorld.Region
DistributedWorld.Handoff
Persistence
Physics
Navigation
AI
```

Gateway:

```text
MMO.Gateway
MMO.RegionRouter
Auth Provider
```

Different executables should not share a complete plugin set.

---

# XLVI. V3 Example: RTS Lockstep

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

No need for:

```text
Distributed MMO
GPUPhysics
ML Trainer
RT
```

---

# XLVII. V3 Example: AI Self-play

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

Strip:

```text
Renderer
UI
Audio
WebView
PostProcess
```

---

# XLVIII. V3 Capacity Control

The most demanding third-party dependencies must all be optional:

```text
ONNX Runtime
PyTorch bridge
Vendor RT library
Cloud SDK
Database client
Voice SDK
MMO orchestration SDK
```

If the project does not use them, they are completely absent.

---

# XLIX. V3 Shader Control

The Render Plugin declares:

```text
ShaderFamilies
PermutationDomain
Capabilities
```

Cook:

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

When not enabled:

```text
RT
MeshShader
WorkGraph
VirtualGeometry
```

all related Shaders are stripped.

---

# L. V3 Asset Control

Asset types can belong to a Plugin:

```text
MLPolicyAsset
RTMaterialExtension
GPUPhysicsAsset
DistributedWorldRegionAsset
```

When the Plugin is disabled:

```text
Editor
→ preserve metadata / show missing plugin

Cook
→ referenced disabled runtime asset hard fail
```

---

# LI. V3 Plugin ABI

Large Server Plugins:

```text
Plugin API Version
Engine ABI Hash
Build Profile
Platform
Architecture
Capabilities
```

Handshake mismatch:

```text
Reject Load
```

Undefined ABI crashes are not allowed.

---

# LII. V3 Hot Reload

Only suitable modules may hot reload.

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

# LIII. Rolling Server Upgrade

The MMO module does not rely on in-process hot reload.

Formal process:

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

This is safer than unloading a native server module.

---

# LIV. Plugin Observability

The Profiler displays:

```text
Loaded Plugins
Static Features
Memory per Plugin
Jobs per Plugin
Shader Families per Plugin
Asset Residency by Plugin
Server Module Cost
```

It can answer:

```text
Why is this Build so large?
```

and:

```text
Which Plugin pulled in this dependency?
```

---

# LV. Build Size Analyzer

V3 Tooling is recommended to formally include:

```text
BuildSizeAnalyzer
```

Output:

```text
Executable Size
Plugin Contribution
Third-party Library Size
Shader Size
Asset Size
Localization Size
Debug Symbol Size
```

and support a dependency tree.

---

# LVI. Plugin Cost Metadata

The Plugin Manifest may include:

```text
Estimated Runtime Footprint
ThirdParty SDK
Shader Family Count
Editor-only Size
Server-only
Client-only
```

When enabling a feature, the Editor can display its cost.

---

# LVII. V3 Definition of Done

```text
1. Distributed MMO can be completely removed from non-MMO projects.
2. The Deterministic domain can be independently enabled or disabled.
3. GPU Physics can exist independently of the Jolt world.
4. Ray Tracing / Mesh Shader / Work Graph are all capability-driven.
5. The ML Trainer does not enter the Shipping Client.
6. ONNX runtime exists only in inference projects.
7. MMO Region / Gateway / Client can use different plugin sets.
8. Server fleet providers do not pollute the Engine Core.
9. Disabled next-gen render features do not produce shaders / PSOs.
10. Build Size Analyzer can trace plugin dependencies and capacity sources.
11. After a V1 / V2 Project is upgraded to V3, its build footprint can remain close to the original without enabling new plugins.
```

---


# LIX. V3 Third-party Plugin SDK Expansion

V3’s requirements for the Plugin ecosystem are elevated from “Engine extension” to:

```text
Distributed Runtime
Deterministic Domain
GPU Simulation
ML Training
Infrastructure Provider
```

while still following:

```text
Public Capability Boundary
≠ Engine Private Access
```

---

# LX. Distributed World Provider Plugin

The following can be extended:

```text
Region Directory
Authority Store
Handoff Transport
Global Interest Router
Persistence Adapter
Failover Provider
```

For example, an enterprise can connect its own:

```text
Shard Manager
Cloud Region Service
Persistent World Backend
```

The Engine Core does not know the actual cloud implementation.

---

# LXI. Deterministic Simulation Extension

Third parties can register a deterministic subsystem:

```text
DeterministicSystemDescriptor
├─ State Schema
├─ Tick Function
├─ Snapshot Codec
├─ Hash Function
└─ Input Command Schema
```

Only explicitly registered state enters the deterministic domain.

Prohibited:

```text
The Plugin claims to be deterministic
but secretly reads the wall clock / global RNG / presentation state
```

CI can provide a determinism verification harness.

---

# LXII. GPU Simulation Plugin SDK

Third parties can provide:

```text
GPU Fluid Solver
GPU SoftBody Solver
GPU Crowd Solver
Custom GPU Physics Island
```

But they must go through:

```text
RenderGraph / ComputeGraph Integration
Resource Registry
Fence-safe Lifetime
Capability Declaration
```

They are prohibited from directly obtaining and retaining the renderer’s private command buffer long-term.

---

# LXIII. ML Algorithm Plugin

The Trainer can register:

```text
PPO
SAC
DQN
SelfPlay Algorithm
Curriculum Strategy
Evaluation Strategy
Custom Optimizer
```

Interface:

```text
Observation Batch
Action Batch
Reward Batch
Episode State
Metrics
Checkpoint
```

Trainer Plugin:

```text
Tool / Training only
```

It is not included in Shipping.

---

# LXIV. ML Runtime Backend Plugin

Inference backends:

```text
ML.Runtime.ONNX
ML.Runtime.Custom
ML.Runtime.AcceleratorVendor
```

Through:

```text
IPolicyRuntimeBackend
```

The AI Framework sees only:

```text
PolicyHandle
ObservationView
ActionView
```

It does not see vendor tensor objects.

---

# LXV. Simulation Farm Provider

The following can be replaced:

```text
SimulationFarm.Local
SimulationFarm.Kubernetes
SimulationFarm.CloudVendor
SimulationFarm.Custom
```

The Coordinator uses only:

```text
Worker Capability
Job Descriptor
Artifact URI
Metrics Stream
```

It is not bound to a single orchestration platform.

---

# LXVI. Ray Tracing / Mesh Shader Feature Plugin

Third parties can provide:

```text
RT GI
RT Reflection
Custom Denoiser
Mesh Shader Geometry Path
Work Graph Pipeline
```

They still go through:

```text
Render Feature Registry
RenderGraph
Shader Metadata
Resource Registry
```

rather than directly using vendor-specific private paths that pollute the Renderer Core.

---
# Sixty-Seven, Server Plugin Rolling Deployment

Large Plugins on the Server side do not require hot reload within the process.

Formally supported:

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

This is the primary upgrade method for V3 MMO / distributed servers.

---

# Sixty-Eight, Plugin Privilege / Capability Model

V3 may add the following to the Plugin Manifest:

```text
Privileges
```

For example:

```text
Filesystem.ReadProject
Filesystem.WriteCache
Network.Client
Network.Server
GPU.Compute
Editor.ModifyDocument
Build.RunExternalTool
```

Editor / Tool Plugins may use only declared and authorized capabilities.

The goal is not a complete OS sandbox, but:

```text
Explicit capabilities
Auditable
Restrictable in enterprise environments
```

---

# Sixty-Nine, Plugin Trust Level

The following levels may be distinguished:

```text
BuiltIn
Verified
ProjectLocal
ThirdParty
UntrustedTool
```

Impact:

```text
Allowed Privileges
Native Binary Loading
External Process
Network Access
Editor Automation
```

Native Runtime Plugins are still considered high-privilege code; this does not claim that arbitrary malicious native binaries can be safely sandboxed.

---

# Seventy, Plugin Store / Registry Boundary

If a Plugin Registry is provided in the future:

```text
Registry Metadata
Package Signature
Version
Engine Compatibility
Platform
License
Dependency
```

Package installation and Runtime Loading are separate.

The Registry must not have the authority to:

```text
Directly inject a Native Plugin into a running Shipping Game after remote download
```

Shipping native code updates must still follow the formal application update process.

---

# Seventy-One, Plugin Cost Analyzer V3

Build Size Analyzer additionally outputs:

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

It can trace:

```text
Why did enabling Plugin A add 350 MB?
```

As well as:

```text
A
→ B
→ Vendor SDK C
→ Shader Family D
```

---

# Seventy-Two, V3 Third-party Plugin Gate

Additional Gates:

```text
12. Distributed World Provider can replace the cloud / region backend.
13. Deterministic Plugin can pass repeat-run state hash verification.
14. GPU Simulation Plugin must follow the RenderGraph / resource lifetime contract.
15. ML Training Algorithm can be implemented as a Tool Plugin and must not enter Shipping.
16. ML Runtime backend must not expose vendor tensor types to AI Core.
17. Simulation Farm can replace the orchestration provider.
18. Server Plugin supports the rolling deployment workflow.
19. Plugin privilege / trust metadata can be verified by Editor / Enterprise policy.
20. Build Size Analyzer can display Plugin transitive cost.
```


# Fifty-Eight, V3 Final Principles

```text
V3
≠ Bigger Default Build
```

Instead:

```text
V3
= Larger Capability Library
```

Projects still include only what they need.

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

This is the core objective of the V3 modular architecture.


---

# Appendix — V2 / V1 Contract Reference

The V2 (and its included V1) Master Baseline is no longer reproduced in full in this document, to avoid divergence between the content and the source documents and the need for repeated manual synchronization (especially since the V2 document itself embeds a complete copy of V1; reproducing it here again would amount to layering two sets of content requiring synchronization). V3 fully inherits all Architecture Contracts from V2 and V1 (see “1. Core Philosophies Unchanged by V3”); for the complete content, please refer to the source documents themselves:

```text
《Cross-Platform 3D_Engine_V2_Complete Planning Document》
→ Currently corresponds to Master Draft v1.3
→ Includes the Rollback + Streaming boundary declaration and Appendix synchronization statement

《Cross-Platform 3D_Engine_V2_AI Implementation Technology and System Planning》
→ Currently corresponds to AI Technical Draft v1.2

《Cross-Platform 3D_Engine_V1_Complete Planning Document》
→ Currently corresponds to Master Draft v1.2
→ Includes the Terrain Streaming Boundary Contract for the Character Framework

《Cross-Platform 3D_Engine_V1_AI Implementation Technology and System Planning》
→ Currently corresponds to AI Technical Draft v1.2
```

The V2 Plugin / Feature Module modular planning, V2 Media/Video Expansion, V1 Plugin/Third-party SDK/Video & Media detailed Contracts, and V1 Master Definition of Done should likewise be consulted in their corresponding source documents and are not reproduced here.

Recommended reading order: before implementing or reviewing this document, first confirm in sequence whether the V1 → V2 complete planning documents are the latest versions; this document (V3) describes only newly added and changed portions, and all existing systems not mentioned in this document shall be governed by the V1 / V2 documents.