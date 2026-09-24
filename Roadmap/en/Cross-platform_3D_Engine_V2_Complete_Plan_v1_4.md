# Cross-Platform 3D Engine — V2 Complete Planning Document

**Document Version: Master Draft v1.4**
**Engine Generation: V2.x — Scale-Up / Production**

> **Progress: 23%** (✅ V2-M0 through ✅ V2-M2 have passed their portable repository gates; V2-M3 through V2-M12 remain open. Native target evidence remains a separate gate.)

> This document is the **V2 Master Plan**. All V1 Contracts are inherited by default; only items explicitly marked “V2 supersede” in this document may change V1 behavior.
>
> The theme of V2 is not rewriting the engine, but expanding V1 to GPU-Driven, Large World V2, Networking / Dedicated Server, advanced AI / Navigation / Animation, Distributed Build, LiveOps, and Production Tooling.
>
> The complete V1 Master Baseline is included at the end of this document, allowing this file to be read independently.


**Document Version: Draft v1.0**
**Corresponding Engine Generation: V2.x**
**Baseline: Entered after V1 / Architecture Completion Baseline was completed and passed the Gate**
**Positioning: Scale-Up / GPU-Driven / Large World / Networking / Production Tooling**

---

# I. V2 Positioning

The goal of V1 is:

```text
Be able to fully create, Build, Profile, and release a 3D game.
```

V2 does not redesign a new engine.

The goal of V2 is:

```text
Without breaking the core V1 architecture Contract,
elevate the engine from “capable of completing a general 3D game”
to:

Large seamless worlds
+
High-density GPU-driven Rendering
+
Multiplayer / Dedicated Server
+
Large-team content production
+
More mature Editor / Build Farm / LiveOps
+
More advanced AI / Animation / Navigation
```

V2 is not “turning on all Future features.”

V2 still follows:

```text
Profile-driven
Platform-aware
Budget-driven
Optional Framework
No hidden expensive path
```

---

# II. Core Contracts Unchanged by V2

The following V1 Contracts continue to apply in V2.

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

Do not add:

```text
UniquePtr Alias
WeakPtr Alias
```

Engine / GPU Resources continue to prioritize:

```text
Handle
ResourceID
EntityID
UUID
```

rather than cross-system shared ownership.

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

V2 does not recombine these into a single God Object.

---

```text
World
≠ Scene
≠ StreamingCell
```

V2 only adds stronger partition / streaming capabilities.

---

```text
Physics Gameplay Truth
→ CPU Authoritative / Jolt
```

V2 does not change to GPU-authoritative gameplay physics.

---

```text
Runtime UI
≠ Editor ImGui
```

Dear ImGui remains exclusive to the Editor / Debug Tool.

---

```text
Shader Source
→ Slang
```

V2 Renderer does not add:

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

GPU-driven / async compute must not bypass RenderGraph to secretly submit commands.

---

```text
Bundle
→ Data / Asset only

Native Code
→ Never remote-downloaded as content bundle
```

V2 LiveOps does not change this security boundary.

---

# III. V2 Primary Goals

V2 is divided into eight primary directions:

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

# IV. V2 Scope Matrix

Symbols:

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

# V. V2 Renderer: GPU-Driven Rendering

The V1 Renderer already has:

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

V2 formally changes rendering submission to GPU-driven first.

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

The CPU no longer processes large numbers of draw items per frame:

```text
for every object
→ visibility check
→ select LOD
→ submit draw
```

The CPU’s primary tasks are:

```text
Update dirty GPU scene records
Build high-level view state
Dispatch culling
Submit indirect passes
```

---

# VI. GPU Scene

V2 establishes the formal:

```text
GPUScene
```

GPUScene stores compact rendering-relevant state:

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

Formal rules:

```text
Scene Entity
≠ GPUSceneObject
```

One Entity may produce:

```text
0
1
N
```

GPU scene records.

GPUScene uses stable runtime slots / generation handles.

Object Destroy:

```text
Mark Retired
↓
Fence-safe reclaim
```

GPU-visible slots must not be reused immediately.

---

# VII. GPU Culling

V2 provides at least:

```text
GPU Frustum Culling
GPU Distance Culling
GPU LOD Culling
Hi-Z Occlusion
```

Pipeline:

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

Hi-Z:

```text
Previous / Current Depth
↓
Depth Pyramid
↓
Occlusion Test
```

The following must be handled:

```text
Camera teleport
Fast rotation
Occluder appearance
Large dynamic object
```

A conservative policy is used to avoid incorrect disappearance.

---

# VIII. Indirect Draw

The RHI exposes a unified abstraction:

```text
IndirectDrawBuffer
IndirectDrawCount
```

Backend:

```text
DX12
→ ExecuteIndirect

Vulkan
→ DrawIndirect / DrawIndirectCount

Metal
→ Indirect Command Buffer / equivalent capability path
```

Backend-native command signatures are not exposed to upper layers.

---

# IX. GPU LOD Selection

LOD no longer requires the CPU to decide for every instance.

Inputs:

```text
Bounds
Projected Screen Size
LOD Threshold
Quality Tier
Performance Bias
```

GPU:

```text
Select LOD
↓
Append to LOD-specific Draw Bin
```

Supports:

```text
Object LOD
Vegetation LOD
Skinned Mesh LOD
HLOD transition candidate
```

HLOD residency decisions are still managed by Streaming / World Partition.

The GPU only selects from already resident representations.

---

# X. Meshlet Pipeline

The V2 high-end profile may add:

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

Important:

```text
Meshlet Asset
≠ Requires Mesh Shader
```

Even when the platform has no Mesh Shader:

```text
Meshlet visibility result
→ indirect indexed draw
```

Therefore, the Asset Pipeline can be shared.

Mesh Shader is only:

```text
Capability-dependent optimization
```

It is not a required condition for V2 content compatibility.

---

# XI. Async Compute

The V2 RenderGraph adds queue-aware scheduling:

```text
Graphics Queue
Compute Queue
Copy Queue
```

Candidates:

```text
VFX Simulation
Skinning
Hi-Z
GPU Culling
Some PostProcess
VT Feedback Resolve
```

The RenderGraph Compiler is responsible for:

```text
Queue Assignment
Cross-Queue Dependency
Semaphore / Fence
Resource Ownership
Overlap Analysis
```

Subsystems must not independently:

```text
submit compute command
```

bypassing RenderGraph.

If async compute is slower on a given GPU:

```text
Quality / Device Profile
→ collapse to graphics queue
```

---

# XII. Temporal Upscaler Framework

V2 adds:

```text
ITemporalUpscaler
```

Inputs:

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

Output:

```text
Display-resolution Color
```

At minimum, the built-in implementation provides:

```text
TAAU-like Engine Backend
```

Third parties:

```text
FSR / DLSS / XeSS style plugin
```

are determined by Plugin / License Policy.

Gameplay must not depend on a specific vendor upscaler.

---

# XIII. Virtual Texturing

V1 Terrain has already reserved VT.

V2 formally provides Terrain Virtual Texturing:

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

V2 Phase One:

```text
Terrain VT
✅
```

General Material Virtual Texturing:

```text
△
```

must first be profiled for:

- Content size.
- IO pressure.
- Mobile memory.
- Tile border cost.
- Shader sampling overhead.

---

# XIV. HLOD V2

V1 already has Offline HLOD.

V2 adds:

```text
Multi-tier HLOD
Impostor
Material Atlas
Streaming-aware HLOD
GPU-driven HLOD selection hints
Incremental Distributed Build
```

Hierarchy:

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

The V2 HLOD Builder supports:

```text
Static Mesh Merge
Material Merge
Texture Bake
Impostor
Vegetation Cluster Proxy
Custom Proxy
```

---

# XV. Large World V2

V2 upgrades the V1 foundation into a complete large-world runtime.

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

The V1 Character Terrain Streaming Boundary Contract (Occupied Cell Set → Physics Collision Pinned, `StreamingPending` Ground State) is formally extended to V2’s Adaptive Quadtree / CellGroup / 3D Volume Partition: regardless of how the underlying partition shape changes, for every minimum Cell unit covered by a Character’s Physical Footprint (Adaptive Cell, Octree Leaf, or Explicit Volume Cell), its Physics Collision Residency is always considered Pinned. The determination logic does not change because the partition changes from the V1 Stable Fixed Grid to the V2 Adaptive/3D structure.

---

# XVI. Adaptive Quadtree Cell Generation

V1:

```text
Stable Fixed Grid
+
Loose Quadtree Spatial Index
```

V2 may allow the cooker to create variable-resolution partitions according to density / content cost.

For example:

```text
Dense City
→ small cells

Forest
→ medium cells

Empty Region
→ large cells
```

However, it still maintains:

```text
QuadtreeNode
≠ Persistent Cell Identity
```

Using:

```text
Stable CellKey
```

Generated from:

```text
Scene UUID
Partition Domain
Logical Region
Stable Spatial Key
```

Partition rebuilds should not unnecessarily cause all Cell identities to be renumbered.

---

# XVII. Hierarchical Cell Group

V2 adds:

```text
CellGroup
```

Uses:

```text
Streaming
HLOD
Budget
Prefetch
Build Distribution
```

For example:

```text
Region
├─ CellGroup A
│  ├─ Cell 1
│  ├─ Cell 2
│  └─ Cell 3
└─ CellGroup B
```

CellGroup is not a Scene.

Nor is it a Bundle.

```text
CellGroup
→ Runtime / Build hierarchy node
```

---

# XVIII. 3D Volume Partition

V2 formally supports non-surface-based worlds:

```text
High-rise
Space
Underground multi-level
Flying world
```

Provides:

```text
VolumePartition
```

The V1 fixed XZ grid is no longer the only outdoor spatial mode.

The following may be used:

```text
3D Grid
Loose Octree
Explicit Volume
```

Octree:

```text
△
```

is determined by the project profile.

---

# XIX. World Origin Rebasing

V1 already uses a high-precision World Position foundation.

V2 formally adds:

```text
World Origin Rebasing
```

However, this is a runtime optimization, not a gameplay-visible teleport.

Concept:

```text
Global WorldPosition
→ remains stable

Local Simulation Origin
→ shifts

Render Origin
→ camera-relative
```

All subsystems requiring origin-shift awareness:

```text
Physics
Navigation
Particles
Audio
Debug Draw
Editor Runtime Gizmo
```

use the World Coordinate Service.

Subsystems are prohibited from independently storing a float world origin under the assumption that it will never change.

---

# XX. Cell Persistent State

V2 deeply integrates Save with World Partition.

```text
Scene Asset
+
Persistent World State
+
Cell Runtime Delta
↓
Runtime Cell
```

For example:

```text
Chest opened
Boss dead
Door destroyed
Resource depleted
NPC moved
```

The timing of Persistent Delta capture and whether a Cell can be unloaded are two separate matters: even if a Cell meets the conditions for Persistent State capture, if that Cell remains within a Character’s Occupied Cell Set (see the Character Framework’s Terrain Streaming Boundary Contract), its Physics Collision must remain Pinned and must not be unloaded first. Persistent Delta can be captured and saved normally, but Runtime Collision Unload must wait until the Character leaves that Cell.

Before Cell unload:

```text
Extract Persistent Delta
↓
PersistentWorldState
↓
Unload
```

Reload:

```text
Scene Defaults
+
Persistent Delta
↓
Runtime State
```

The entire Runtime Memory Snapshot is not saved.

---

# XXI. World Partition Commandlets

Large worlds cannot require the entire World to be loaded into the Editor before Build.

V2 provides headless tools:

```text
WorldPartitionBuild
HLODBuild
NavBuild
ProbeBuild
Validation
CellCook
```

Supports:

```text
Region-only
Changed-only
Distributed
CI
```

---

# XXII. Networking Framework

Networking was deliberately not implemented in V1.

V2 formally establishes it while maintaining:

```text
Networking
≠ Gameplay Model
```

The Engine provides:

```text
Transport
Connection
Replication
Prediction
Interest
Replay
Server Runtime
```

The game defines:

```text
Match Rules
Lobby Rules
Gameplay Commands
Authority Policy
Specific Replicated State
```---

# XXIII. Transport Layer

Formal interface:

```text
INetTransport
```

V2 Built-in realtime transport:

```text
Datagram / UDP-oriented
```

Provided channels:

```text
Unreliable
UnreliableSequenced
ReliableOrdered
ReliableUnordered
```

Internals:

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

Encryption must not involve inventing cryptographic algorithms.

The security layer uses mature libraries / platform backends.

---

# XXIV. Connection Handshake

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

Mismatch:

```text
Reject with explicit reason
```

Different replication schemas must not be allowed to connect silently.

---

# XXV. Network Entity Identity

Formal:

```text
NetworkEntityID
≠ EntityID
≠ UUID
```

`EntityID`:

```text
Local Runtime Handle
```

`UUID`:

```text
Persistent Authoring Identity
```

`NetworkEntityID`:

```text
Connection / Session Replication Identity
```

Server spawn:

```text
Server Entity
↓
NetworkEntityID
↓
Client Entity Mapping
```

The client EntityID may be completely different.

---

# XXVI. Replication Schema

Use canonical reflection metadata codegen:

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

Attributes:

```text
Replicated
OwnerOnly
InitialOnly
ReliableEvent
Quantized
PredictionState
```

However, reflection metadata is only the schema source.

The hot path does not use slow reflection traversal.

Cook / Codegen:

```text
Metadata
↓
Generated Replication Codec
```

---

# XXVII. Snapshot Replication

Server:

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

Client:

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

Snapshot Tick and Render Frame are decoupled.

---

# XXVIII. Interest Management

Directly integrate:

```text
World Partition
Spatial Query
Gameplay Relevancy
Network Ownership
```

Not:

```text
replicate all entities to everyone
```

Interest Sources:

```text
Player
Camera
Party
Quest
Spectator
Server gameplay rule
```

Output:

```text
Relevant NetworkEntity Set
```

Support:

```text
Distance
Zone
Portal / Room
Team
Ownership
Custom Filter
```

---

# XXIX. Network Dormancy

When a remote object remains unchanged for an extended period:

```text
Dormant
```

Do not continuously send snapshot deltas.

Wake:

```text
State Change
Gameplay Event
Interest transition
Explicit Wake
```

Dormancy is separate from Scene streaming:

```text
Network Dormant
≠ Cell Unloaded
```

---

# XXX. Client Prediction

The Character Framework directly provides prediction hooks:

```text
InputCommand
↓
CharacterMotor
↓
Predicted State
```

Server:

```text
Same command
↓
Authoritative result
```

Client:

```text
Authoritative State
↓
Compare
↓
Reconcile
↓
Replay pending InputCommands
```

CharacterController collision results are not required to be bitwise deterministic.

Use:

```text
State Correction
+
Re-simulation
```

rather than assuming floating-point results are completely identical across different CPUs / platforms.

---

# XXXI. Rollback Islands

V2 does not promise:

```text
Entire engine deterministic rollback
```

It may support:

```text
Rollback Simulation Island
```

For example:

```text
Fighting combat
Projectile logic
Small deterministic gameplay layer
```

These gameplay states must have:

```text
Explicit Snapshot
Deterministic Tick
Deterministic RNG
No uncontrolled wall-clock input
```

PhysicsWorld does not automatically become deterministic rollback state.

---

# XXXII. Dedicated Server

V2 formally establishes:

```text
Headless Server Build Profile
```

Default targets:

```text
Linux x64
Windows x64 optional
```

Server stripping:

```text
No Renderer
No Runtime UI
No GPU assets
No Editor
Minimal Audio or none
```

Retain:

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

# XXXIII. Session / Lobby / Matchmaking Boundary

Engine Core provides:

```text
ISessionService
ILobbyService
IMatchmakingService
```

V2 Built-in:

```text
Local / LAN / Direct Connect reference backend
```

Cloud services:

```text
Steam
EOS
Console service
Custom backend
```

Implemented as Plugins.

Do not hard-code a single commercial service into Engine Core.

---

# XXXIV. Replay Framework V2

Upgrade the Input Replay foundation to:

```text
Gameplay Replay
Network Replay
```

Replay chunk:

```text
Header
Build ID
Schema Version
Initial State
Input / Network Frames
Events
Checkpoints
```

Uses:

```text
Bug Reproduction
Spectator
Network Debug
AI Training Data
Regression Test
```

---

# XXXV. Navigation V2

V1:

```text
Recast / Detour
Tile NavMesh
Async Query
Streaming
```

V2:

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

# XXXVI. Hierarchical Pathfinding

Long-distance:

```text
Start
↓
High-level Region / Portal Graph
↓
Target Region
↓
Local NavMesh Path
```

Avoid:

```text
Running A* over the entire continental NavMesh at once
```

High-level node:

```text
NavRegion
Room
World Partition Region
Portal
OffMesh Connection
```

---

# XXXVII. Navigation Query Scheduler

Nav queries do not allow all NPCs to perform arbitrary synchronous pathfinding.

Formal:

```text
NavigationQueryScheduler
```

Request:

```text
Priority
Start
Goal
AgentType
CostProfile
Deadline
MaxWork
```

Frame budget:

```text
Critical
High
Normal
Background
```

Completion may span multiple frames.

---

# XXXVIII. Crowd System

V2 establishes:

```text
CrowdAgentPool
```

Do not create large numbers of heavy per-agent objects.

Functions:

```text
Local Avoidance
Desired Velocity
Neighbor Sampling
Path Corridor
Density / Flow
Separation
```

Crowd output:

```text
Desired Motion
↓
CharacterIntent
↓
CharacterMotor
```

Crowd does not directly modify the Character transform.

---

# XXXIX. AI Framework V2

V1:

```text
Perception
Blackboard
Behavior Tree
AI LOD
```

V2 adds:

```text
Utility AI
Influence Field
Hierarchical AI
Learned Policy Interface
Self-play Tooling Boundary
```

Still maintain:

```text
Engine does not force one decision model
```

---

# XL. Utility AI

Provide a data-oriented scoring framework:

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

Hot path:

```text
Batch Evaluate
```

Do not force every Consideration to use a virtual object.

The Editor can visualize:

```text
Action
├─ Consideration
├─ Weight
├─ Curve
└─ Runtime Score
```

---

# XLI. GOAP

GOAP:

```text
△ Optional Module
```

Reasons:

- Suitable for specific strategy / simulation-oriented games.
- Planning cost and debugging overhead are higher.
- It should not impose a burden on every AI project.

Interface:

```text
WorldState
Action Preconditions
Effects
Cost
Planner
```

It can be built on the same Blackboard / Action execution framework.

---

# XLII. AI Perception Budget

Perception:

```text
Sight
Hearing
Damage
Area Trigger
Gameplay Signal
```

V2 formally adds:

```text
Perception LOD
```

For example:

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

Physics queries use batch APIs.

---

# XLIII. Influence / Cost Field

Large-scale strategy and RPG AI may use:

```text
InfluenceField
ThreatField
CoverCostField
HeatMap
```

The underlying representation may be:

```text
Grid
Tile
Sparse Region
```

Do not hard-bind it to NavMesh polygons.

Navigation can read:

```text
Dynamic Cost Provider
```

---

# XLIV. Learned Policy Interface

Establish:

```text
IAIPolicy
```

Input:

```text
ObservationBuffer
PolicyState
```

Output:

```text
Action / Intent
```

Engine Core does not know whether it is:

```text
Neural Network
Behavior Tree
Rule Table
```

Policy backends may be:

```text
Native
ONNX plugin
External trainer bridge
```

---

# XLV. Self-Play Training Boundary

V2 does not place a complete trainer inside the Shipping Runtime.

Formal:

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

Support:

```text
Headless Simulation
Multiple Worlds per Process
Accelerated Simulation Time
Deterministic Seed
Episode Reset
Metrics
Replay Capture
```

The trainer may use:

```text
Python
JAX
PyTorch
Other
```

Do not hard-code it.

The resulting policy can be converted to:

```text
Engine-native data
or
ONNX
```

---

# XLVI. Animation V2

V1 already has:

```text
Animation Graph
State Machine
CPU Pose Evaluation
GPU Vertex Skinning
BAT Crowd Animation
```

V2:

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

# XLVII. Compute Skinning

```text
Bone Matrix Buffer
+
Bind Pose Vertex Buffer
↓
Compute Skinning
↓
Skinned Vertex Buffer
```

Suitable for cases requiring repeated reuse of skinned vertices:

```text
Depth
Shadow
Main
Outline
Motion Vector
```

The runtime selects based on:

```text
Pass Count
Vertex Count
GPU Capability
Memory Budget
```

Select:

```text
Vertex Skinning
or
Compute Skinning
```

Do not force all characters to use compute.

---

# XLVIII. GPU Pose Sampling

Large numbers of NPCs:

```text
Animation Clip Data
↓
GPU Sampling
↓
Bone Pose / Skin Matrix
↓
Skinning
```

Gameplay-authoritative elements:

```text
Root Motion
Animation Event
Hit Window
```

are still managed by CPU / Gameplay timing.

GPU event readback must not become gameplay authority.

---

# XLIX. Compressed Pose Storage

V2 may add:

```text
CompressedTRS
QuantizedTRS
DualQuaternion
```

Canonical runtime interface:

```text
AnimationPoseStorage
```

Consumers do not depend on the specific compression method.

Platform profiles may select different formats.

---

# L. Motion Warping

Character animation may use:

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

Use cases:

```text
Vault
Mantle
Execution
Attack alignment
Door interaction
Climb
```

Physics resolution remains the final authority.

---

# LI. Inertialization / Sync Group

The Animation Graph adds:

```text
SyncGroup
Marker Sync
Inertial Blend
```

Reduce:

```text
Long crossfade
Pose popping
Locomotion phase mismatch
```

---

# LII. Pose Search / Motion Matching

V2 establishes a general-purpose:

```text
PoseDatabase
FeatureExtractor
PoseSearch
```

Motion Matching:

```text
△ Optional Framework
```

Reasons:

- High asset authoring cost.
- Not suitable for every type of game.
- The Engine should support it, but it is not the only character animation workflow.

The Editor provides:

```text
Pose DB Builder
Feature Visualization
Query Debug
Trajectory Debug
```

---

# LIII. Physics V2

CPU Gameplay Truth remains Jolt.

V2 strengthens:

```text
Physics LOD
GPU Cloth
GPU Debris
Deferred Query
Destruction optional
```

---

# LIV. Physics LOD

Distance / importance:

```text
Near
→ Full Character / RigidBody

Mid
→ Reduced update / simplified collider

Far
→ Sleeping / proxy / no dynamic simulation
```

Switching must be managed by an explicit policy.

Important gameplay bodies must not be implicitly placed into approximation.

---

# LV. GPU Cloth V2

Upgrade the V1 foundation:

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

Gameplay does not depend on cloth particle results.

Collision proxies may be:

```text
Capsule
Sphere
SDF
Simplified Mesh
```

---

# LVI. Destruction / Fracture

```text
△ Optional Module
```

Authoring:

```text
Mesh
↓
Fracture Tool
↓
Chunk Graph
↓
Cooked Destruction Asset
```

Runtime:

```text
Gameplay state
→ authoritative coarse destruction

Visual fragments
→ GPU / pooled simulation
```

Avoid making every fragment a Scene Entity.

---

# LVII. Runtime UI V2

V1 UI already has Retained Mode, Layout, Input, and WorldSpace.

V2 adds:

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

# LVIII. Flex-like Layout

Support:

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

However, do not directly reproduce the complete CSS runtime.

The Engine defines:

```text
UIFlexStyle
```

Cook into compact computed style data.

---

# LIX. Advanced Grid Layout

Support:

```text
Fixed Track
Auto Track
Fraction Track
Row / Column Span
Gap
```

Used for:

```text
Inventory
Skill grid
Settings
Complex panel
```

---

# LX. RichText

V2:

```text
RichTextElement
```

Support:

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

Parsing occurs in the content/build or cache layer to avoid parsing markup every frame.

---

# LXI. UI StyleSheet / Theme

```text
UIStyleSheet
↓
Selector / StyleClass
↓
Computed Style
```

Do not pursue full browser CSS.

Support:

```text
Type
Class
State
Theme Variable
```

For example:

```text
.primary-button
.danger
:hover
:disabled
```

Cook / runtime perform fast style resolution.

---

# LXII. RenderTexture Surface UI

V1:

```text
WorldSpace Direct Geometry
```

V2 adds:

```text
UIDocument
↓
Offscreen UI Render
↓
RenderTexture
↓
Mesh / Material
```

Uses:

```text
Curved Screen
Monitor
Vehicle dashboard
3D terminal
```

Input:

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

# LXIII. Offscreen WebView

```text
△ Platform-dependent
```

Architecture:

```text
WebView
↓
Offscreen Surface
↓
GPU Texture
↓
Runtime UI / World Mesh
```

If the platform's native WebView does not support reliable offscreen capture:

```text
Capability = Unsupported
```

Do not pretend to provide normal support using an extremely expensive CPU screenshot path.

---

# LXIV. Accessibility Foundation

V2 Runtime UI adds:

```text
Semantic Role
Accessible Name
Accessible Value
Focus Order
Text Scale
High Contrast Hook
Reduced Motion Hook
```

Platform adapters then connect to:

```text
iOS Accessibility
Android Accessibility
Desktop accessibility API
```

V2 is not required to implement every platform's complete feature set at once, but the runtime semantic tree must exist.

---

# LXV. Web / Native UI Input V2

Continue using:

```text
InputLayer → UILayer Matrix
```

Add:

```text
Accessibility Focus
Remote Control Focus
Multiple UI User
Surface UI Pointer
```

Native overlays and GPU UI must still use single-owner routing.

---

# LXVI. Audio V2

The audio runtime retains Backend abstraction.

V2:

```text
Room / Portal Acoustics
HRTF / Spatial Profile
Convolution Reverb
Advanced Occlusion
Audio Authoring Graph V2
Middleware Backend optional
```

---

# LXVII. Room / Portal Acoustics

Directly utilize the World Room / Portal graph:

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

Avoid performing multiple physics raycasts per frame for every sound source.

Physics raycasts may be used for:

```text
Near-field refinement
```

---

# LXVIII. Audio DSP Graph

Provide a constrained, safe authoring graph:

```text
EQ
Filter
Compressor
Limiter
Delay
Reverb Send
Spatializer
```

Runtime audio thread:

```text
No allocation
No lock
No file IO
```

The rules remain unchanged.

---

# LXIX. Cinematic / Timeline Framework

V2 formally adds the complete system not yet established in V1:

```text
Timeline
```

This is not an Animation State Machine.

Timeline is:

```text
Time-based Multi-System Sequencing
```

---

# LXX. Timeline Asset

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

Support:

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

# LXXI. Timeline Runtime

```text
TimelinePlayer
↓
Evaluate at Time
↓
Track Outputs
↓
Presentation Commands
```

Tracks do not directly hold raw engine pointers.

Use:

```text
ObjectBindingID
Entity UUID / Runtime Binding
PropertyID
Resource Handle
```

Editor scrubbing does not need to start the complete Gameplay simulation.

---

# LXXII. Camera Rig V2

The Camera Framework adds:

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

Timeline can drive the Rig.

Gameplay can also drive it.

The Camera Rig is not hard-bound to Timeline.

---

# LXXIII. Editor V2

The V1 Editor can already create games.

V2 goals:

```text
Large Team
Large World
Diffable Content
Headless Automation
Remote Device Workflow
```

---

# LXXIV. Clang AST Reflection Generator

V1:

```text
Macro / constexpr producer
```

V2:

```text
Clang AST Header Tool
↓
Canonical Metadata Schema
```

Consumers remain unchanged:

```text
Serialization
Inspector
Prefab
Networking
AI Tool
Property Binding
```

Formal:

```text
Producer Changes
Consumer Contract Does Not
```

Tool output is deterministic.

CI:

```text
Duplicate TypeID
Duplicate PropertyID
Schema Break
Nondeterministic Output
```

All are hard failures.

---

# LXXV. Externalized World Entity Files

Large Scenes should not consist solely of one enormous monolithic authoring file.

V2 supports:

```text
Scene Header
+
External Entity Records
```

For example:

```text
Town.scene
Town/Entities/uuid-A.entity
Town/Entities/uuid-B.entity
...
```

Uses:

```text
Git merge conflict reduction
Large World partial load
Editor checkout
Incremental cook
```

After runtime cooking, they are still converted into compact cell blobs.

Do not let the shipping runtime scan hundreds of thousands of authoring entity files.

---

# LXXVI. Scene Diff / Merge

The V2 Editor provides structural diff:

```text
Entity Added / Removed
Component Added / Removed
Property Changed
Hierarchy Change
Reference Change
Prefab Override Change
```

It must not perform only plain-text diff.

Output:

```text
Ours
Theirs
Base
```

Support interactive merge.

---

# LXXVII. Prefab Conflict / Rebase UI

Prefab V2 tools:

```text
Prefab Asset Changed
↓
Instance Overrides
↓
Rebase
↓
Conflict Detection
```

The UI displays:

```text
Base
New Base
Instance Override
Resolved Value
```

Provide explicit handling for:

```text
Added child
Removed child
Component replacement
Property rename / migration
```

---

# LXXVIII. Editor Commandlet / Headless Mode

The Editor executable or tool executable supports:

```text
--headless
--command=<...>
```

Commands:

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
```Let CI not depend on GUI automation.

---

# Seventy-Nine, Remote Device Inspector

Editor can connect to:

```text
Android device
iOS device
Desktop build
Dedicated server
```

View:

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

Shipping builds are disabled by default or require a secure dev entitlement.

---

# Eighty, Source Control Provider

Editor is not hard-bound to Git.

```text
ISourceControlProvider
```

Can provide:

```text
Git
Perforce
Custom
None
```

Editor uses:

```text
Status
Checkout if required
Add
Delete
Move
Diff
History link
```

The Git workflow does not require checkout.

---

# Eighty-One, Asset Pipeline V2

V1:

```text
Importer
Cook
Asset DB
Bundle
Streaming
```

V2:

```text
Content-addressable DDC
Shared DDC
Import Worker
Distributed Cook
Incremental Patch
```

---

# Eighty-Two, Content-Addressable DDC

Key:

```text
Source Content Hash
Importer ID
Importer Version
Settings Hash
Platform
Feature Profile
Engine Build / Relevant Cooker Version
```

Value:

```text
Derived Artifact
Metadata
Dependency Hashes
```

Same input:

```text
→ same output key
```

---

# Eighty-Three, Shared / Remote DDC

Local:

```text
L1 Local Cache
```

Remote:

```text
L2 Shared DDC
```

Process:

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

Remote DDC failure:

```text
Build still works locally
```

Network services must not become a single point of failure that makes development completely unavailable.

---

# Eighty-Four, Import Worker Process

High-risk importers:

```text
FBX
Image codec
Third-party converter
```

Can execute in an independent process:

```text
ImportWorker
```

Importer crash:

```text
Editor survives
↓
Import marked failed
↓
Previous valid runtime artifact preserved
```

---

# Eighty-Five, Distributed Build Worker

Unified worker protocol:

```text
Task
Input Hash
Tool Version
Platform Profile
Output Hash
```

Candidates:

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

Workers do not directly modify the authoritative state of the Asset DB.

The Coordinator verifies the output hash before committing.

---

# Eighty-Six, Incremental Patch Build

V2 Content Build:

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

Supports:

```text
Add
Replace
Retire
```

Does not perform native executable hot patching.

---

# Eighty-Seven, DataTable V2

V1 Runtime can use typed immutable tables.

V2:

```text
.tablebin
Packed Memory Block
Memory Mapping
Data Overlay
LiveOps Layer
```

---

# Eighty-Eight, Packed Runtime Table

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

Can use:

```text
Single / Few allocations
or
Memory-mapped read-only block
```

C++ / Zig views do not expose STL pointers.

---

# Eighty-Nine, Data Overlay

Live configuration can use:

```text
Base Table
+
Patch Overlay
↓
Resolved Table Generation
```

Still follows:

```text
Immutable after finalize
```

Update:

```text
Build N+1
↓
Validate
↓
Atomic Registry Swap
↓
Old Generation remains pinned until refs drain
```

In-place row mutation that causes a thread race is not allowed.

---

# Ninety, General Serialization V2

The cooked binary format is upgraded to:

```text
Relocatable
Versioned
Endian-defined
Pointer-free
```

Goals:

```text
Fast load
Memory map where appropriate
Skip unknown optional section
Schema migration at cook/editor
```

Shipping runtime does not bear the responsibility of migrating arbitrary historical authoring formats.

---

# Ninety-One, File System / IO V2

Add:

```text
IO Batch
Read Coalescing
Priority Inheritance
Cancellation
Streaming Trace
Optional Direct IO backend
```

Asset Streaming can submit:

```text
IORequestBatch
```

rather than issuing a large number of tiny read syscalls.

---

# Ninety-Two, Package / Plugin V2

Project package system:

```text
Package Manifest
Version
Dependency
Optional Feature
Platform Filter
Editor-only
Runtime
```

Lock:

```text
Project.lock
```

Guarantees that CI and other development machines use the same plugin / package versions.

---

# Ninety-Three, Platform V2

Add officially:

```text
Linux Headless
Linux Desktop △
```

Headless Linux:

```text
✅ V2 Networking / Server required
```

Linux Desktop:

```text
△
```

Enable the Vulkan desktop backend if required by the product.

WebGPU / WASM:

```text
△ R&D
```

Not included in the V2 release gate.

Console:

```text
△ SDK / business-dependent
```

The Engine interface remains portable, but completion is not pretended without an SDK.

---

# Ninety-Four, Save / Cloud V2

V1 Save:

```text
Local Slot
Profile
Migration
Atomic Write
Recovery
```

V2:

```text
Cloud Save Adapter
Conflict Metadata
Cross-device Revision
Server-authoritative Boundary
```

The cloud backend is implemented as a plugin.

The core only defines:

```text
Revision
Timestamp
DeviceID
Content Hash
Conflict State
```

Engine does not automatically choose which save wins.

Gameplay / Product policy determines this.

---

# Ninety-Five, Localization V2

V1 has completed the locale / ICU foundation.

V2 adds:

```text
Remote Localization Pack
DLC Locale Bundle
Localized Voice Pack
Pseudo Localization
Localization Coverage Report
```

CI:

```text
Missing Key
Unused Key
Missing Font Glyph
Placeholder mismatch
Plural form missing
```

---

# Ninety-Six, Profiler / Diagnostics V2

V2 turns the profiler into a cross-process / remote tool.

```text
Trace Producer
↓
Local Ring
↓
Stream / File
↓
Editor Profiler
```

Events:

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

# Ninety-Seven, Unified Trace ID

Used across subsystems:

```text
FrameID
WorldID
EntityID when safe
JobID
AssetID
CellID
NetworkEntityID
```

For example, a single hitch can be traced through:

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

# Ninety-Eight, Runtime Developer Console V2

V1 developer console upgraded with:

```text
Command
CVar
Watch
Remote Command
Role / Permission
```

Remote server build:

```text
Readonly
Developer
Admin
```

Permissions must be restrictable.

Shipping default:

```text
disabled
or
authenticated restricted mode
```

---

# Ninety-Nine, LiveOps / Remote Content

V2 officially supports:

```text
Remote Content Manifest
DataTable Overlay
Localization Pack
Asset Bundle Patch
Event Configuration
```

Still prohibited:

```text
Native DLL / dylib / executable remote content update
```

Gameplay native module updates still require a normal App / executable update.

---

# One Hundred, Feature Flag System

V2 Project Settings adds:

```text
FeatureFlag
```

Types:

```text
Build-time
Cook-time
Runtime Data-driven
Server-authoritative
```

Feature flags should not be scattered throughout gameplay as arbitrary strings.

They can be managed through schemas / IDs.

---

# One Hundred One, Security Boundary

Because Network / LiveOps are added in V2, formally establish:

```text
Untrusted Network Data
Untrusted Remote Content Manifest
Untrusted Web Content
Untrusted Save / User Data
```

All parsers:

```text
Bounds Checked
Size Limited
Version Checked
No raw pointer serialization
```

Remote manifests must have:

```text
Signature / Integrity verification
```

Specific cryptographic implementations use mature libraries; cryptographic primitives must not be implemented from scratch.

---

# One Hundred Two, Performance Budget V2

Each Platform Profile includes more than a Quality Tier.

Add:

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

Subsystems can:

```text
Request
Observe
Degrade
Recover
```

rather than hardcoding independently.

---

# One Hundred Three, Scalability Governor

V2 can add:

```text
ScalabilityGovernor
```

Inputs:

```text
GPU time
CPU time
Memory Pressure
Thermal
Battery
Frame target
```

Outputs:

```text
Dynamic Resolution
Shadow Budget
Vegetation Distance
VFX Budget
Animation LOD
AI LOD
Streaming aggressiveness
```

However:

```text
Gameplay Authority
```

must not be arbitrarily changed by presentation scalability.

---

# One Hundred Four, Thermal / Mobile V2

Mobile can use:

```text
Thermal State
Battery State
Sustained GPU Time
```

to enter:

```text
Normal
Warm
Hot
Critical
```

The Performance Profile provides the downgrade policy.

Avoid lowering settings only after FPS has dropped.

---

# One Hundred Five, Editor Quality / Device Preview

Editor can:

```text
Preview Device Profile
```

For example:

```text
Android Low
Android High
iPhone class
Desktop Mid
Desktop Ultra
```

Preview:

```text
Texture Residency
Shadow
LOD
UI Safe Area
Dynamic Resolution target
Feature Strip
```

This is not limited to changing a single graphics quality dropdown.

---

# One Hundred Six, V2 Project Migration

V1 Project upgrade to V2:

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

Do not directly overwrite the only project copy.

Editor provides:

```text
Migration Report
```

Listing:

```text
Changed Schema
Deprecated Setting
Plugin ABI mismatch
Missing migration
Rebuild required
```

---

# One Hundred Seven, ABI / Plugin Migration

The V2 major version may update the internal ABI.

However, Plugins handshake using:

```text
Plugin API Version
Engine ABI Hash
Build Configuration
Platform
Architecture
```

Mismatch:

```text
Reject Load
```

It must not be discovered only after a crash.

Stable C Gameplay ABI:

```text
Maintain versioned compatibility strategy
```

However, a major schema does not guarantee that binary recompilation is completely unnecessary.

---


---

# V2 Construction Milestones（Implementation Milestones）

> V2 is established on the premise that **all V1 Gates have passed**.
> V2 does not rewrite the core; the construction focus is first to stabilize “Production Metadata / Toolchain,” then expand toward GPU-Driven, Large World, Networking, and advanced Gameplay Frameworks.

## ✅ V2-M0 — V1 → V2 Migration / Production Baseline

> **Repository status: accepted (portable gate).** `Tools/Migration/ScanV1Project.py` audits the canonical module schema, gameplay ABI, plugin manifests, and required Development/Shipping profiles. Its stable content fingerprint is the reference-project snapshot and regression-baseline identity; `build.v2_migration_scanner` proves deterministic output and actionable failure reporting. Native target performance remains a target-host gate.


Construction:

```text
V1 Project Migration Scanner
Plugin / Package ABI Audit
Schema Audit
Build Profile Audit
Performance Baseline
Reference Project Snapshot
```

**Gate:**

```text
✓ V1 Project can still build normally without enabling any V2 feature
✓ Migration Report can list schema / plugin / rebuild requirements
✓ V1 benchmark becomes the V2 regression baseline
```

---

## ✅ V2-M1 — Clang Reflection / DDC / Headless Toolchain

> **Repository status: accepted (portable gate).** `Tools/Production/NexoraTool.py` emits sorted
> canonical metadata from Clang's JSON AST, stores immutable content-addressed artifacts, isolates
> each import in a worker process, and exposes CI-safe validate/import/cook commandlets. Versioned
> external entity manifests and deterministic structural JSON diffs provide the scene foundation;
> `build.v2_production_toolchain` covers deterministic output, worker crashes, cache reuse, and the
> headless cook path.

Do first:

```text
Clang AST Reflection Generator
Canonical Metadata Output
Content-addressable DDC
Import Worker Process
Headless Commandlet
Externalized World Entity Files
Structural Scene Diff foundation
```

Reason: Subsequent Networking Schema, Distributed Cook, and Large World Build all depend on stable metadata and headless tools.

**Gate:**

```text
✓ Reflection output deterministic
✓ Import worker crash does not bring down the Editor
✓ Headless Cook / Validate can run in CI
✓ DDC same input → same artifact hash
```

---

## ✅ V2-M2 — GPUScene / Render Extraction V2

Construction:

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

Do not implement complete GPU culling yet.

**Gate:**

```text
✓ CPU extraction can stably update GPUScene
✓ Destroy / reuse does not produce GPU stale slots
✓ CPU reference path and GPUScene rendering can be compared
```

Delivered evidence: `GPUScene` provides stable generational object slots, categorized deterministic
dirty uploads, current/previous transforms, bounds, mesh/material resource indices, visibility and
LOD metadata, and fence-safe retirement. Its deterministic CPU reference snapshot permits complete
identity and render-data comparison before GPU culling is enabled. Contract tests cover create,
update, destroy/reuse, stale handles, dirty batches, transform history, fence reclamation, and the
reference snapshot.

---

## V2-M3 — GPU-Driven Rendering

Current evidence: the deterministic CPU reference implements frustum/distance/LOD culling,
conservative Hi-Z with explicit invalidation, visible-instance compaction, material/mesh/LOD
classification, and indirect-command generation. The portable command contract now records compute
dispatch and indirect drawing, compares backend output with the CPU reference, tracks normal-path
readback diagnostics, and makes RenderGraph emit explicit compute/graphics ownership barriers.
V2-M3 remains open because native compute/indirect execution and DX12/Vulkan/Metal target-tier parity
have not passed their target-host gates.

Order:

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

Then add:

```text
Temporal Upscaler Interface
Compute Skinning
Meshlet metadata
```

**Gate:**

```text
✓ Large numbers of instances no longer require CPU one-draw-per-object
✓ DX12 / Vulkan / Metal target tier parity
✓ No normal-path GPU readback
✓ RenderGraph owns queue / barrier / lifetime
✓ CPU fallback can perform correctness comparison
```

---

## V2-M4 — Large World V2

Construction:

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

**Gate:**

```text
✓ Origin rebase is invisible to gameplay identity
✓ Partition build deterministic
✓ HLOD switch does not produce holes
✓ Persistent cell unload/reload state is correct
✓ Changed regions can be incrementally rebuilt
✓ Character does not lose Collision when crossing Adaptive/3D Partition Cell boundaries（Occupied Cell Pinned extended to V2 Partition）
```

---

## V2-M5 — Dedicated Server / Transport Foundation

Do first:

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

Do not implement Prediction first.

**Gate:**

```text
✓ Linux headless server has no Renderer dependency
✓ Client / Server connect / disconnect is stable
✓ Loss / latency / jitter simulator is available
✓ Protocol mismatch clean reject
```

---

## V2-M6 — Replication / Interest / Prediction / Replay

Order:

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

**Gate:**

```text
✓ Client / Server local EntityID can be completely different
✓ Interest does not perform global replication
✓ Character prediction is playable under test latency
✓ Reconciliation can replay pending input
✓ Replay is sufficient to reproduce network bugs
```

---

## V2-M7 — Navigation / Crowd / AI V2

Construction:

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

**Gate:**

```text
✓ Thousands of AI agents do not perform synchronous pathfinding in the same frame
✓ Far AI can be throttled / dormant
✓ Crowd outputs CharacterIntent and does not directly modify Transform
✓ Learned Policy can be consumed through the same Action Interface
```

---

## V2-M8 — Animation V2

Construction:

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

**Gate:**

```text
✓ Compute / Vertex Skinning can be selected through the profile
✓ GPU crowd animation does not become gameplay event authority
✓ Motion Warping is ultimately resolved through CharacterMotor / Controller
✓ Pose Search database can be rebuilt deterministically
```

---

## V2-M9 — Timeline / UI V2 / Audio / Media V2

Construction:

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

**Gate:**

```text
✓ Timeline can scrub / seek
✓ Surface UI world ray → UV → UI hit is correct
✓ RichText uses the existing shaping / localization
✓ Media Streaming does not break the V1 local VideoPlayer contract
✓ DRM / Capture / Encoder can be completely stripped
```

---

## V2-M10 — Shared DDC / Distributed Build / LiveOps

Construction:

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

**Gate:**

```text
✓ Development can continue locally when Remote DDC is down
✓ Two workers same input → same artifact hash
✓ Patch cannot activate before verification is complete
✓ Old generation pins until refs drain
✓ Remote content cannot carry native executable code
```

---

## V2-M11 — Remote Tools / Device Profiling / Production Diagnostics

Construction:

```text
Remote Device Inspector
Remote Profiler
Unified Trace ID
Streaming Trace
Network Trace
Memory / GPU / IO correlation
Build Size / Feature report foundation
```

**Gate:**

```text
✓ Android / iOS / Desktop / Server can all be observed remotely
✓ A single streaming hitch can be traced across IO → cook artifact → GPU upload
✓ PluginID can be used for cost attribution
```

---

## V2-M12 — V2 Hardening / Reference Projects / Shipping

Execute:

```text
Massive Outdoor
Indoor Portal Dungeon
Network Arena
Crowd City
Mobile Stress
```

Test for extended periods:

```text
Streaming Soak
Network Soak
Memory Pressure
Thermal
Patch Rollback
Save Corruption
Server Reconnect
```

**Gate:**

```text
✓ All V2 reference projects pass
✓ 24h+ streaming soak has no unbounded growth
✓ Network mapping has no leak after disconnect
✓ Patch rollback can return to known-good
✓ When V2 features are disabled, V1-like project footprint does not grow abnormally
```

---

## V2 Construction Dependency Graph

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


# One Hundred Eight, V2 Development Phases

V2 should not all be developed in parallel at once.

Recommended order:

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

# One Hundred Nine, Phase V2-A — Production Foundation

Contents:

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

Gate:

```text
Reflection generated deterministically
Large Scene can use external entity storage
Editor survives importer worker crash
Headless cook works in CI
Scene diff understands entity/component/property changes
Shared metadata consumer remains compatible
```

---

# One Hundred Ten, Phase V2-B — GPU Driven Renderer

Contents:

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

Gate:

```text
100k+ static instances do not require one CPU draw submission each
GPU-driven path has backend parity on DX12 / Vulkan / Metal target tier
No hidden sync GPU readback in normal culling path
RenderGraph owns all transitions / queue sync
GPU culling can be disabled for debug and compared against CPU reference
```---

# 111. Phase V2-C — Large World V2

Contents:

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

Gate:

```text
Large test world streams for hours without cell leak
Origin rebasing does not visibly move gameplay world
Cell unload/reload preserves persistent gameplay delta
HLOD never leaves visible empty hole during switch
Partition build is deterministic
Changed region can rebuild without full world rebuild
```

---

# 112. Phase V2-D — Networking

Contents:

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

Gate:

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

# 113. Phase V2-E — AI / Navigation / Animation

Contents:

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

Gate:

```text
Thousands of AI do not synchronously pathfind in one frame
Far AI automatically reduces simulation cost
Crowd drives CharacterIntent, not transforms directly
Training simulation can run headless multiple worlds
GPU crowd animation does not become gameplay event authority
```

---

# 114. Phase V2-F — Runtime Feature Expansion

Contents:

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

Gate:

```text
Timeline can scrub in Editor
Camera / Animation / Audio tracks remain synchronized
Surface UI receives correct UV-mapped input
RichText uses existing localization / shaping pipeline
Audio portal routing works without per-source expensive raycast requirement
```

---

# 115. Phase V2-G — Distributed Build / LiveOps

Contents:

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

Gate:

```text
Remote DDC outage does not stop local development
Two build machines produce matching artifact hash for same input
Patch cannot activate before full validation
Old generation remains usable until refs drain
Remote content cannot introduce executable native code
```

---

# 116. Phase V2-H — Hardening

Contents:

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

Gate:

```text
24h+ streaming soak without unbounded residency growth
Network disconnect/reconnect leaves no leaked entity mapping
Crash report includes build / module / trace metadata
Patch rollback returns to known valid content
Mobile thermal policy behaves deterministically by profile
```

---

# 117. V2 Reference Test Projects

V2 does not rely only on unit tests.

At least establish the following internal reference projects.

## Project A — Massive Outdoor

```text
Large Terrain
Vegetation
Town
HLOD
Adaptive Streaming
Vehicle-speed traversal
```

Validation:

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

Validation:

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

Validate Networking.

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

# 118. V2 Performance Targets

Actual values are determined by hardware profiles; do not hard-code a single number as a cross-platform Contract.

However, V2 must establish benchmark classes.

For example, Desktop Reference:

```text
Large visible instance count
High draw-source count
Thousands of animated agents
Large streaming world
```

Mobile Reference:

```text
Thermal-stable sustained session
Memory budget respected
No emergency allocation storm
```

Each benchmark must record:

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

# 119. V2 CI Gates

Add:

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

# 120. V2 Fuzz / Robustness

For untrusted inputs:

```text
Network Packet
Save File
Remote Manifest
Data Overlay
WebView Message
```

Establish:

```text
Fuzz Test
Size Limit
Malformed Data Test
Timeout
```

Avoid allowing parsers / decoders to become crash surfaces.

---

# 121. V2 Memory Model

V2 adds extensive GPU / Network / Streaming systems, but must not therefore return to uncontrolled allocation.

Memory Tags:

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

The profiler can show:

```text
Current
Peak
Lifetime
Allocation Count
Fragmentation Estimate
```

GPU-driven temporary buffers:

```text
Frame / Ring / Pool
```

Do not use per-frame heap new/delete.

---

# 122. V2 Network Memory

Packet / Snapshot use:

```text
Packet Pool
Snapshot Arena
Bitstream Buffer Pool
```

The Network thread must not pass arbitrary STL object graphs across threads to gameplay.

Use:

```text
POD Message
Handle
Batch
```

---

# 123. V2 AI Memory

Blackboard / BT / Utility Runtime:

```text
Data-oriented Instance State
```

Static tree / utility definition:

```text
Shared immutable asset
```

Each NPC stores only:

```text
Runtime state
Blackboard values
Active node / action
```

Do not clone the entire graph object.

---

# 124. V2 Timeline Memory

Timeline Asset is immutable.

TimelinePlayer stores:

```text
Current Time
Binding Table
Track Runtime State
Active Section Cache
```

Do not duplicate all keyframe data.

---

# 125. V2 Threading

Adding new thread / task types does not mean adding new fixed OS Threads.

Continue using:

```text
Job System
IO Service
Render Thread / Backend needs
Audio Thread
Network IO thread/service
```

AI / Nav / HLOD runtime work:

```text
Jobs
```

Rather than one fixed thread each:

```text
AI Thread
Nav Thread
Streaming Thread
```

---

# 126. V2 Server Tick

Dedicated Server:

```text
Fixed Simulation Tick
```

Can be completely separated from client rendering.

The server has no:

```text
Render interpolation
GPU presentation
```

Configurable:

```text
Tick Rate
Max Catch-up
Network Snapshot Rate
AI Budget
Physics Budget
```

---

# 127. V2 Multi-World Server

Reserve capacity for AI training / server instances:

```text
Process
├─ World A
├─ World B
├─ World C
└─ ...
```

Each World:

```text
Own Entity Registry
PhysicsWorld
NavigationWorld
Gameplay state
Time
```

Shared:

```text
Immutable Assets
Data Tables
Global code
Optional resource cache
```

This simultaneously supports:

```text
Dungeon instances
Self-play simulation
Server instance hosting
```

---

# 128. V2 MMO Boundary

V2 Networking can support:

```text
Online RPG
Co-op
MOBA
Arena
Session-based game
Moderate persistent world server
```

However:

```text
Global seamless MMO shard
Cross-server handoff
Distributed authoritative simulation
Massive persistence cluster
```

Are not included in the V2 Core Definition of Done.

Only reserve:

```text
World Partition server hooks
NetworkEntity identity
Session transfer hooks
Persistent backend adapter
```

Avoid over-design.

---

# 129. V2 Database / Backend Boundary

The Engine does not include a game-backend database ORM.

Provide:

```text
IGamePersistenceBackend
```

Server gameplay can connect to:

```text
Custom Service
SQL service layer
Cloud backend
```

However, Engine Core does not directly allow gameplay threads to execute blocking SQL queries.

---

# 130. V2 Asset Security

Remote Bundle:

```text
Manifest
Hash
Signature
Size
Dependency
Version
```

Only after validation is complete may it enter:

```text
Verified Cache
```

Then:

```text
Atomic Activate
```

Broken Patch:

```text
Rollback
```

Fully integrate with existing generation pinning.

---

# 131. V2 Build Profiles

Build Profiles add:

```text
Client
DedicatedServer
Editor
Tool
Benchmark
TrainingHeadless
```

Each Profile can perform:

```text
Module Strip
Asset Strip
Shader Strip
Platform Feature
Logging Policy
Telemetry Policy
```

---

# 132. Training Headless Profile

```text
TrainingHeadless
```

Remove:

```text
Renderer
Runtime UI
Audio Output
Expensive presentation
```

Retain:

```text
Gameplay
Physics if needed
Navigation
AI
Data
Replay / Metrics
```

Can:

```text
Run faster than realtime
```

Provided that gameplay logic does not depend on wall clock.

---

# 133. V2 Determinism Policy

The Engine does not claim:

```text
All systems deterministic
```

Instead, define:

```text
Deterministic-capable subsystem
```

For example:

```text
DataTable
Gameplay RNG Service
Timer Tick
Selected AI logic
Rollback island
```

Uncontrollable:

```text
GPU Simulation
Rendering
Audio
General floating physics cross-platform
```

Make the distinction explicit.

---

# 134. V2 Random Service

Establish:

```text
RandomStream
```

Seed:

```text
World Seed
System Seed
Entity / Gameplay Seed
```

Support:

```text
Replay
AI Training
Test Reproduction
```

Gameplay should not extensively use process-global `rand()`.

---

# 135. V2 Testing API

Headless Test World:

```text
CreateWorld()
LoadScene()
StepFixedTicks(N)
InjectInput()
QueryState()
CaptureSnapshot()
DestroyWorld()
```

Used for:

```text
Gameplay regression
Network prediction
AI
Save
Scene lifecycle
```

---

# 136. V2 Editor Automation

Provide:

```text
EditorAutomation API
```

Capabilities:

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

Available to:

```text
CI
Internal Tool
AI coding agent
Test harness
```

However, permissions must be explicit; do not expose unrestricted OS operations to runtime content.

---

# 137. V2 AI-Assisted Tooling Boundary

The Engine may allow AI tools to:

```text
Read Scene Metadata
Read Asset Metadata
Create Editor Command
Generate Config
Run Validation
```

However, AI does not directly modify private engine memory.

All mutations go through:

```text
Editor Command / Transaction
```

Therefore they are:

```text
Undoable
Auditable
Validatable
```

---

# 138. V2 Documentation Generation

Reflection / Settings / Console / Plugin metadata can generate:

```text
API Reference
Property Reference
CVar Reference
Project Settings Reference
```

Avoid keeping documentation and runtime schemas entirely synchronized manually.

---

# 139. V2 Deprecation Policy

API:

```text
Deprecated
↓
Warning
↓
Migration Guide
↓
Removal no earlier than planned major boundary
```

Asset schema:

```text
Migration function
```

C ABI:

```text
Versioned function table
```

Do not make silent behavior changes.

---

# 140. V2 Non-Goals

V2 explicitly does not make the following core requirements:

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

If visual scripting is needed in the future:

```text
Plugin / Future
```

---

# 141. V2 Definition of Done

V2.0 may officially be called complete only if it at least satisfies:

```text
1. V1 Project can be upgraded through migration and recooked.
2. GPU-driven renderer is production usable on target desktop backends.
3. Mobile can use a compatible reduced path; all advanced GPU features are not required.
4. Large World V2 supports seamless streaming over long durations.
5. Adaptive partition / HLOD build is deterministic.
6. World origin shifts are invisible to gameplay identity.
7. Dedicated server supports headless builds.
8. Snapshot replication / interest / prediction are production usable.
9. Character prediction and reconciliation have a reference implementation.
10. Hierarchical navigation queries / crowd are production usable.
11. AI LOD / Utility AI are production usable.
12. Compute skinning / GPU crowd paths are production usable.
13. Timeline / cinematic can be fully authored / previewed / run at runtime.
14. Shared DDC and headless cook can be used in CI.
15. Scene / Prefab structural diff / merge can be used.
16. Remote device profiling can trace CPU/GPU/IO/Streaming/Network.
17. Asset patch / Data overlay support rollback.
18. No subsystem bypasses established lifetime / RenderGraph / ABI contracts.
19. All V2 reference projects pass CI / soak tests.
20. Documentation / migration / profiler / crash diagnostics support actual production.
```

---

# 142. Final V2 Architecture Summary

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

# 143. Conceptual Difference Between V1 → V2

V1:

```text
“Can the engine fully make a game?”
```

V2:

```text
“Can the same architecture remain viable with a larger world, more content,
more characters, a larger team, multiplayer servers, and long-term operation?”
```

Therefore, the core of V2 is not simply adding more effects.

The true V2 is:

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

# 144. V2 Highest-Priority Development Order

If V1.0 is stable, I recommend the following implementation priority:

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

Reason:

```text
Toolchain / Metadata
```

must be stabilized first, so that later:

```text
Networking
Large World
Distributed Build
```

are not built on unstable schemas.

---

# 145. V2 Final Design Principles

V2 continues to follow the engine’s most important overall direction:

```text
The Engine provides Capability
rather than forcing a Gameplay Workflow.
```

```text
High-level Framework
→ Optional

Low-level Control
→ Always available
```

```text
Editor can be highly convenient
Runtime must remain Data-Oriented / Budgeted / Explicit
```

```text
Small Game
→ Does not need to pay the cost of Large World / Network / GPU Crowd

Large Game
→ Does not need to switch to another Engine Architecture
```

Therefore, the same Engine V2 can support:

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

There is no need to create a separate core for each game genre.


---


# V2 Media / Video Expansion

V1 has completed:

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

V2 will expand the Media Framework into optional Streaming / Capture / Encode / DRM capabilities.

## Media Plugin Structure

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
```All are Optional; ordinary games only need V1 `Media.Video`.

## Adaptive Streaming

Formal model:

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

Supported:

```text
HLS
MPEG-DASH
```

ABR inputs:

```text
Measured Throughput
Buffer Duration
Decode Capability
Display Size
Thermal / Performance Profile
User Quality Policy
```

Output:

```text
Selected Representation
```

Do not simply switch quality up or down immediately based on the most recent download speed.

At minimum, use:

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

Cache Key:

```text
URL / Content ID
Range
ETag / Version
DRM state where applicable
```

Do not merge this with Asset Bundle Cache under the same semantic ownership, but VFS / disk quota infrastructure may be shared.

## DRM Boundary

`Media.DRM.Core` defines only:

```text
License Request
Session
Key Status
Secure Decode Requirement
Error Translation
```

Actual Widevine / FairPlay / PlayReady implementations are all Provider Plugins.

The Engine must not implement the DRM crypto protocol itself.

## Capture

`Media.Capture` may provide:

```text
Camera Capture
Microphone Capture integration
Screen / RenderTarget Capture
```

Capture frame:

```text
CaptureSource
↓
Frame Queue
↓
Optional Processing
↓
Encoder / Gameplay Consumer
```

Capture callbacks must not send large heap objects across the Stable ABI every frame.

Use:

```text
FrameHandle
PlaneView
Timestamp
```

## Encoder

`Media.Encoder`:

```text
VideoFrame
+
Audio PCM
↓
Encoder
↓
Container Writer
```

Platform hardware encoders should be preferred.

Uses:

```text
Replay Export
User Recording
UGC Capture
Tooling
```

V2 does not require all platforms to have the same codec encoder.

## Media Compositor

Optional:

```text
Media.Compositor
```

Uses:

```text
Video + UI overlay
Video + Camera
Multi-video
Subtitle burn-in for export
Transition
```

Realtime game presentation should still primarily be handled by Renderer / UI composition; not all screens are required to pass through Media Compositor.

## WebRTC

```text
Media.WebRTC
△ Optional Provider
```

Used for:

```text
Low-latency remote video
Remote tool
Cloud rendering experiment
Voice/video communication
```

Not included in V2 Core DoD.

## V2 Media DoD

```text
✓ HLS / DASH framework can be enabled through Plugin
✓ ABR has a buffer-aware policy
✓ DRM is a Provider Plugin
✓ Video Capture / Encoder can be completely stripped from a general client
✓ Streaming / DRM does not change the basic local-playback contract of the V1 VideoPlayer
✓ Media cache can be observed by the profiler / quota manager
✓ Headless / Dedicated Server does not include Media by default
```


---

# V2 Plugin / Third-party SDK Detailed Contract（Normative）

# Cross-Platform 3D Engine — V2 Plugin / Feature Module Modularization Plan

**Document Version: Draft v1.1**
**Corresponding Engine Generation: V2.x**
**Premise: Continue using the V1 Plugin Contract without changing the core rules.**

---

# I. V2 Modularization Objectives

V2 adds substantial Production / Large World / Networking / GPU advanced functionality.

Therefore, the core principle is even more important:

```text
Large-scale Production capabilities
≠
All games must carry them
```

V2 must achieve:

```text
Small Game
→ Does not pay the cost of Networking / HLOD V2 / Vendor SDK / ML / Distributed Build

Large Game
→ Enable the required Feature Plugin
```

---

# II. V1 Contracts Still Used by V2

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

Development:

```text
Dynamic / Modular
```

Shipping:

```text
Selected Plugins only
→ Static / Monolithic allowed
→ LTO / WPO
```

---

# III. V2 Core Does Not Add Heavyweight Optional Features

V2 Core still retains only:

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

Networking, ML, RT, Timeline, and similar features are not promoted to permanently existing Core components.

---

# IV. Networking Plugins

```text
Network.Core
Network.Transport.UDP
Network.Replication
Network.Interest
Network.Prediction
Network.Replay
```

Single-player game:

```text
Network.*
→ strip all
```

Multiplayer games may select only what they need:

```text
Network.Core
Network.Transport.UDP
Network.Replication
```

Prediction / Replay are not necessarily required.

---

# V. Dedicated Server

Not an ordinary runtime feature, but:

```text
BuildProfile.DedicatedServer
```

Typical:

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

# VI. Online Provider Plugins

```text
Online.Core
Online.Steam
Online.EOS
Online.PlayFab
Online.Custom
```

Compile only the providers in use.

Engine Core must not bind to any commercial service.

---

# VII. Voice Chat Plugin

```text
VoiceChat.Core
VoiceChat.Provider.*
```

Completely Optional.

If unused:

```text
codec
capture
network voice
platform voice SDK
```

None of these enter the package.

---

# VIII. Cloud Save Plugins

```text
CloudSave.Core
CloudSave.Provider.*
```

Can be independent of Local Save.

---

# IX. Temporal Upscaler Plugins

```text
Upscaler.Core
Upscaler.EngineTAAU
Upscaler.FSR
Upscaler.DLSS
Upscaler.XeSS
```

A project may use:

```text
EngineTAAU only
```

No vendor SDK is required.

---

# X. Virtual Texturing Plugins

```text
RenderFeature.VirtualTexture
Terrain.VirtualTexture
Material.VirtualTexture
```

Can be layered.

For example:

```text
Terrain VT
✓

General Material VT
✕
```

---

# XI. Mesh Shader Plugin

```text
RenderFeature.MeshShader
```

Capability-dependent.

On unsupported platforms:

```text
Do not compile at all
```

Meshlet Asset can still use:

```text
Indexed Indirect fallback
```

---

# XII. GPU-Driven Renderer Features

Can be split into:

```text
RenderFeature.GPUScene
RenderFeature.GPUCulling
RenderFeature.HiZ
RenderFeature.Indirect
RenderFeature.AsyncCompute
```

However, if the project selects:

```text
GPUDrivenRenderer
```

it is recommended that a meta-plugin pull in the required dependencies.

For example:

```text
RenderFeature.GPUDriven
depends:
→ GPUScene
→ GPUCulling
→ Indirect
```

---

# XIII. Compute Skinning Plugin

```text
Animation.ComputeSkinning
```

Games with few characters:

```text
Vertex Skinning only
```

may omit it.

---

# XIV. GPU Pose Sampling Plugin

```text
Animation.GPUPose
```

Required only for large crowds.

General RPG:

```text
Optional
```

---

# XV. Pose Search / Motion Matching Plugins

```text
Animation.PoseSearch
Animation.MotionMatching
```

Motion Matching depends on:

```text
Animation.PoseSearch
```

Projects that do not use Motion Matching do not need to carry the Pose DB runtime / editor tooling at all.

---

# XVI. GPU Cloth Plugin

```text
Physics.GPUCloth
```

Dependencies:

```text
Physics.Core
RenderGraph
```

If cloth is not used:

```text
strip
```

---

# XVII. Destruction Plugin

```text
Physics.Destruction
Physics.FractureEditor
```

Runtime / Editor are separated.

Fracture editor:

```text
Editor-only
```

---

# XVIII. Timeline / Cinematic Plugins

```text
Timeline.Core
Timeline.Camera
Timeline.Animation
Timeline.Audio
Timeline.UI
Timeline.Editor
```

A pure gameplay project may omit them entirely.

If only simple camera cinematics are required:

```text
Timeline.Core
Timeline.Camera
```

is sufficient.

---

# XIX. Camera Rig Plugin

```text
Camera.Rig
Camera.Cinematic
```

Camera Core must not be tightly bound to complex cinematics.

---

# XX. Surface UI Plugin

```text
UI.Surface
```

Uses:

```text
3D Monitor
Curved Screen
Vehicle Dashboard
World Terminal
```

Ordinary Screen UI does not require it.

---

# XXI. Advanced UI Plugins

```text
UI.Flex
UI.AdvancedGrid
UI.RichText
UI.StyleSheet
UI.Accessibility
```

`UI.Runtime` may exist without carrying all advanced functionality.

---

# XXII. Offscreen WebView Plugin

```text
UI.WebView.Offscreen
```

Independent of:

```text
UI.WebView.NativeOverlay
```

On unsupported platforms:

```text
Capability = Unsupported
```

Do not implement a high-cost fake fallback.

---

# XXIII. AI Utility Plugin

```text
AI.Utility
```

Optional.

Can coexist with:

```text
AI.BehaviorTree
```

---

# XXIV. GOAP Plugin

```text
AI.GOAP
```

Optional.

Do not make all AI projects bear the planner cost.

---

# XXV. ML Policy Runtime Plugins

```text
AI.Policy
AI.Policy.ONNX
```

Add only when learned policies are used.

---

# XXVI. Self-play Training Bridge

```text
AI.TrainingBridge
```

Only for:

```text
Editor
TrainingHeadless
Tool
```

Shipping Client:

```text
strip
```

---

# XXVII. Shared DDC Plugin

```text
Tool.DDC.Remote
```

Only for:

```text
Editor
CI
Cooker
Build Farm
```

0 bytes in Shipping.

---

# XXVIII. Distributed Build Plugins

```text
Tool.DistributedBuild
Tool.ShaderWorker
Tool.HLODWorker
Tool.CookWorker
```

All are Tool-only.

---

# XXIX. Source Control Plugins

```text
Editor.SourceControl.Git
Editor.SourceControl.Perforce
```

Load only the required provider.

---

# XXX. Scene Externalization Plugin

```text
Editor.SceneExternalization
```

Authoring-only.

Cook:

```text
External Entity Files
↓
Runtime Cell Blob
```

Shipping does not need the external authoring parser.

---

# XXXI. World Partition V2 Modules

Recommended split:

```text
WorldPartition.Core
WorldPartition.AdaptiveQuadtree
WorldPartition.Volume3D
WorldPartition.HLOD
WorldPartition.Impostor
WorldPartition.Editor
WorldPartition.Commandlet
```

Small games:

```text
WorldPartition.*
→ can all be omitted
```

General large maps:

```text
Core
HLOD
```

Add the following only when adaptive partitioning is genuinely required:

```text
AdaptiveQuadtree
```

---

# XXXII. Origin Rebasing Plugin

```text
World.LargeCoordinate
World.OriginRebase
```

LargeCoordinate foundation can be a Core-compatible capability.

Actual Rebasing:

```text
Optional Feature
```

Small maps do not require it.

---

# XXXIII. HLOD V2 Plugins

```text
HLOD.Core
HLOD.MeshMerge
HLOD.TextureBake
HLOD.Impostor
HLOD.Editor
HLOD.Worker
```

Runtime only needs:

```text
HLOD.Core
```

Builder:

```text
Editor / Cooker / Worker only
```

---

# XXXIV. Advanced Navigation Plugins

```text
Navigation.Hierarchical
Navigation.Crowd
Navigation.InfluenceField
```

Can be independent.

For example, an RPG:

```text
Hierarchical + Crowd
```

A strategy game may use:

```text
InfluenceField
```

---

# XXXV. Profiler / Remote Tools

```text
Tool.RemoteInspector
Tool.RemoteProfiler
Tool.NetworkProfiler
Tool.StreamingProfiler
```

Development-only.

Shipping:

```text
strip
or
secure restricted diagnostics mode
```

---

# XXXVI. DataTable V2 Modules

```text
DataTable.BinaryRuntime
DataTable.MemoryMapped
DataTable.LiveOverlay
```

If LiveOps is not required:

```text
LiveOverlay
→ strip
```

---

# XXXVII. LiveOps Modules

```text
LiveOps.Manifest
LiveOps.BundlePatch
LiveOps.DataOverlay
LiveOps.LocalizationPack
```

A standalone boxed product may omit them entirely.

---

# XXXVIII. Plugin and Shader Cook

Especially important in V2:

```text
Enabled Render Plugin
+
Used Material Feature
+
Target GPU Capability
↓
Shader Variant Set
```

For example, if the following are not enabled:

```text
MeshShader
RayTracing
VirtualTexture
DLSS
```

the related shader entries / PSOs should not appear in the build.

---

# XXXIX. Plugin and Backend SDK

Third-party SDKs follow only their corresponding Plugin.

For example:

```text
DLSS SDK
→ Upscaler.DLSS

FMOD SDK
→ Audio.FMOD

EOS SDK
→ Online.EOS
```

It must be possible for a project that has not selected them to avoid downloading / compiling them entirely.

---

# XL. V2 Build Profiles

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

Each profile may have a different Plugin set.

---

# XLI. Common V2 Combinations

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

Strip:

```text
Renderer
UI
Audio
VFX
PostProcess
WebView
```

---

# XLII. V2 Definition of Done

```text
1. Networking can be completely removed from a single-player build.
2. Vendor upscaler SDKs exist only when the corresponding plugin is enabled.
3. HLOD builder / DDC / distributed worker do not enter the shipping client.
4. The DedicatedServer profile does not depend on Renderer / UI / Audio.
5. GPU Driven features can be enabled through a meta-plugin.
6. Motion Matching / GOAP / ML Policy can be independently toggled.
7. World Partition V2 does not force small-map projects to carry adaptive partitioning.
8. Shader families for disabled plugins do not enter the cook.
9. Provider SDK ownership belongs entirely to the provider plugin.
10. Project.lock can pin plugin / provider versions.
```

---


# XLIV. V2 Third-party Plugin SDK Extensions

V2 continues to use the V1:

```text
PluginHost
Stable C ABI
Service Registry
Extension Registry
Bridge Plugin
```

and adds large-scale Production Extension Points.

---

# XLV. Render Feature Plugin SDK

Third parties can register:

```text
Render Feature
RenderGraph Pass Factory
GPU Resource Declaration
Shader Family
Material Extension
Debug Visualization
```

Process:

```text
Third-party Render Plugin
↓
Register Pass Factory
↓
RenderGraph
↓
RHI
```

Prohibited:

```text
Plugin
→ arbitrary vkCmd*
→ arbitrary ID3D12GraphicsCommandList*
```

Bypassing RenderGraph.

RenderGraph still controls:

```text
Barrier
Lifetime
Aliasing
Queue
Dependency
Synchronization
```

---

# XLVI. Custom Navigation Backend

V2 may provide:

```text
INavigationBackend
```

Third parties may implement:

```text
Grid Navigation
Voxel Navigation
Flying Navigation
Custom Crowd Navigation
```

Public boundary:

```text
NavHandle
QueryBatch
POD Result
```

Do not expose Recast native pointers.

---

# XLVII. Custom AI Decision Plugin

Decision Models can be extended independently:

```text
AI.BehaviorTree
AI.Utility
AI.GOAP
AI.Policy
AI.CustomDecision
```

They only need to connect to:

```text
Blackboard
Perception Snapshot
Action Interface
CharacterIntent / Gameplay Command
```

The Engine does not require all AI to use the same graph.

---

# XLVIII. Timeline Track Plugin

Third parties may register:

```text
Custom Timeline Track
Custom Clip
Custom Binding Resolver
Custom Editor Drawer
```

For example:

```text
Weather Track
Quest Track
Dialogue Track
Camera Lens Track
Custom Gameplay Parameter Track
```

Track evaluation must not hold raw Engine pointers.

---

# XLIX. Network / Online Provider Bridge

V2 officially permits:

```text
Custom Transport
Custom Online Provider
Custom Matchmaking
Custom Lobby
Custom Voice
Custom Cloud Save
```

Through:

```text
Provider Service API
```

rather than by modifying Network Core.

---

# L. Distributed Build Worker Plugin

The Build Farm can register:

```text
Custom Build Task
Custom Worker Capability
Custom Artifact Validator
```

For example:

```text
World Generator
Custom Nav Bake
Proprietary Texture Cooker
ML Data Preprocessor
```

Workers only output artifacts and do not directly modify the authoritative Asset DB.

---

# LI. Remote Tool Plugin

The Editor can allow third parties to add:

```text
Remote Device Panel
Network Debug Panel
Custom Profiler Track
Server Console Panel
Streaming Visualization
```

Use the Unified Trace / Remote Inspector Public API.

---

# LII. Plugin Package / Dependency Lock

The V2 Project Package System can manage third-party Plugins:

```text
Plugin Package
├─ Manifest
├─ Binary / Source
├─ Editor Part
├─ Runtime Part
├─ ThirdParty
└─ License Metadata
```

`Project.lock` pins:

```text
Plugin Version
Provider Version
Dependency Version
```

CI and development machines must resolve to the same dependency graph.

---

# LIII. Plugin Build Variants

The same Plugin may provide:

```text
Editor
DesktopClient
MobileClient
DedicatedServer
TrainingHeadless
```

with different implementations / stripping.

For example:

```text
MyOnlinePlugin
├─ Client
└─ Server
```

Not all profiles are required to use the same binary.

---

# LIV. Plugin Profiling / Cost Attribution

The profiler adds:

```text
PluginID
```

as a trace dimension.

The following can be measured:

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

Therefore, it can answer:

```text
What is the actual cost of this third-party Plugin?
```

---

# LV. V2 Third-party Plugin Gate

Additional Gates:

```text
11. Third-party Render Features can be integrated through RenderGraph extensions.
12. Custom Navigation Backends can replace Recast without changing the Gameplay API.
13. Custom AI Decision Models can connect to the same Perception / Blackboard / Action framework.
14. Timeline can load third-party Tracks.
15. Online / Matchmaking / Voice / Cloud providers can use Bridge Plugins.
16. Build Workers can register custom cooker tasks.
17. Project.lock can pin third-party Plugin dependencies.
18. The profiler can track CPU / memory / GPU / IO costs by PluginID.
19. DedicatedServer / Client can use different build variants for the same Plugin.
20. Third-party Plugins cannot bypass the RenderGraph / ABI / lifetime contract.
```


# XLIII. V2 Final Principles

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

The stronger V2 becomes, the less it can allow all games to bear the cost together.


---

# V2 Complete Scope / Gate

Building on what was completed in V1, V2 formally adds:

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
``````text
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

V3 begins only after V2 is complete.


---

# Appendix — V1 Contract Reference

The full text of the V1 Master Baseline is no longer reproduced in this document, to avoid divergence from the source documents and the need for repeated manual synchronization. V2 fully inherits all of V1’s Architecture Contracts (see “II. Core Contracts Unchanged in V2”). For the complete content, please refer to the source documents themselves:

```text
Cross-platform 3D Engine — V1 Complete Plan
→ Currently corresponds to Master Draft v1.2
→ Includes the Terrain Streaming Boundary Contract with the Character Framework

Cross-platform 3D Engine — V1 AI Implementation Technology and System Plan
→ Currently corresponds to AI Technical Draft v1.2
```

The detailed V1 Plugin / Third-party SDK / Video & Media Contracts and the two V1 Master Definition of Done appendices should likewise be consulted in the V1 documents themselves and are not reproduced here.

Recommended reading order: Before implementing or reviewing this document, first confirm whether the versions of the V1 documents listed above are the latest; this document (V2) describes only the additions and changes, and all existing systems not mentioned in this document shall be governed by the V1 documents.
