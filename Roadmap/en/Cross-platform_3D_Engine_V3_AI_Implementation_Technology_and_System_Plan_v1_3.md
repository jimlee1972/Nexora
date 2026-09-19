# Cross-Platform 3D Engine — V3 AI Construction Technology and System Planning

**Document Version: AI Technical Draft v1.3**
**Corresponding Source: Cross-platform_3D_Engine_V3_Complete_Plan_v1_4.md**
**Purpose: AI implementation, Engine Programmer implementation, system decomposition, Code Review, CI Gate.**



---

# Development Environment and IDE Baseline (Normative)

This project formally adopts:

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

Visual Studio IDE:

```text
Optional
```

It is not the authoritative source for project structure or Build settings.

Xcode on Apple platforms:

```text
Platform Toolchain / Signing / Deployment / Device Debug
```

may be used, but daily Engine / Gameplay code development still uses VS Code as the primary working environment.

## Separation of IDE and Build System

Formal principle:

```text
VS Code
≠ Build System
```

VS Code is responsible only for:

```text
Editing
Navigation
Debug Launch
Tasks
Terminal
AI Agent Workflow
```

The actual Build Contract is determined by:

```text
CMake
+
CMake Presets
+
Toolchain Files
+
Ninja / Platform Generator
```

It is prohibited to place critical Build settings only in:

```text
.vscode/settings.json
.vscode/c_cpp_properties.json
Visual Studio .sln/.vcxproj
Xcode project manual settings
Developer local environment
```

After cloning the repository, CI and new development machines must be able to rebuild without depending on personal IDE settings.

## Repository VS Code Workspace

At the repository root, it is recommended to have:

```text
Engine.code-workspace

.vscode/
├─ extensions.json
├─ settings.json
├─ tasks.json
└─ launch.json
```

`c_cpp_properties.json`:

```text
Avoid when clangd + compile_commands.json is sufficient
```

Add it only when required by a specific tool.

## Recommended VS Code Extensions

The repository’s:

```text
.vscode/extensions.json
```

is recommended to list:

```text
clangd
CMake Tools
Zig language support
C/C++ debugger support as needed
Shader / Slang syntax support if available
Git tooling optional
```

Extensions are only recommended dependencies and must not become necessary conditions for Build correctness.

## C++ Code Intelligence

Priority:

```text
clangd
```

Data source:

```text
compile_commands.json
```

CMake must support:

```cmake
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
```

When using MSVC / clang-cl on Windows, a compilation database usable by clangd should still be generated.

Extensive manual maintenance of include paths is prohibited.

## CMake Presets

The repository must provide:

```text
CMakePresets.json
```

Recommended:

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

Add the following as needed for V2 / V3:

```text
server
training-headless
build-worker
benchmark
```

Use:

```text
CMakeUserPresets.json
```

to store developers’ private local paths and SDK overrides.

`CMakeUserPresets.json`:

```text
.gitignore
```

must not be used as the source of configuration required by CI.

## Windows Toolchain

Primary:

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

It is possible to install only:

```text
Visual Studio Build Tools
```

Use of the Visual Studio IDE is not required.

DX12:

```text
Windows SDK
```

Vulkan uses the SDK or headers / loader package defined by the repository / CI.

## macOS / iOS Toolchain

Primary editing environment:

```text
VS Code
```

Toolchain:

```text
Apple Clang
Xcode SDK
CMake
Ninja where applicable
```

iOS:

```text
Xcode
→ signing
→ provisioning
→ device deployment
→ platform debugging when needed
```

However, Engine Source / CMake / Zig / Slang remain governed by the repository.

Manually creating core build rules known only to Xcode in the Xcode project is prohibited.

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

The ABI / API Level / STL / Vulkan capability for Android builds is configured by the CMake Preset / Toolchain.

Android Studio must not become the only buildable path.

Android Studio may be used for:

```text
Optional platform debugging / packaging inspection
```

## Zig Gameplay Toolchain

Gameplay:

```text
Zig
```

Workflow:

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

The Zig module must not depend on:

```text
Visual Studio project
Xcode manually configured target
Android Studio-only Gradle source layout
```

Mobile Shipping:

```text
Build-time native code only
```

A remote native code download / replace path must not be provided.

## Slang Shader Toolchain

Shader authoring:

```text
VS Code
```

Canonical source:

```text
Slang
```

Compile:

```text
Command-line compiler / Engine shader tool
```

It must not depend on an IDE-specific shader compiler.

Output:

```text
DXIL
SPIR-V
MSL
```

Shader build, reflection, and variant cook must be executable by:

```text
CLI
CI
Headless Tool
```

## VS Code Tasks

`.vscode/tasks.json` is recommended to serve only as a command wrapper.

Standard tasks:

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

V2+ may add:

```text
Build Dedicated Server
Run Network Test
Build HLOD
Run Distributed Worker
```

V3 may add:

```text
Run Determinism Test
Run Training Headless
Run Simulation Worker
Run Distributed Region
```

Tasks should not redefine build logic; they should only invoke repository-owned commands such as:

```text
cmake
ctest
engine commandlet
zig
python tooling
```

## VS Code Launch Profiles

`.vscode/launch.json` is recommended to include:

```text
Engine Editor
Game Client
Unit Test
Tool / Commandlet
```

V2:

```text
Dedicated Server
Client + Server compound
```

V3:

```text
Region Server
Gateway
Training Worker
Simulation Worker
```

Debug launch args must be consistent with the official CLI.

## Command-line First

If any function:

```text
can only be executed by pressing a button in VS Code
```

it is not considered complete.

It must have a CLI for:

```text
configure
build
test
cook
validate
package
```

VS Code is only a UI shortcut.

This is a necessary condition shared by the AI Agent, CI, Build Farm, and Headless Tool.

## Rules for AI Agent Modifications in VS Code Projects

The AI Agent should prioritize modifying:

```text
Source
CMake
Tests
Schemas
Tool scripts
Repository-owned configuration
```

Avoid:

```text
Generated IDE project
Build output
Cache
Local user settings
CMakeUserPresets.json
.vscode local-only override
```

unless the task itself is VS Code workspace configuration.

When adding a Module, the AI must also check:

```text
CMake target
Public / Private dependency
Feature resolver
Plugin manifest if applicable
Tests
Compile commands
Shipping strip
```

When adding an executable, the AI must add:

```text
CLI
CMake target
Preset / profile integration if needed
Launch task only as convenience
CI coverage
```

## Content Not Committed to the Repository

`.gitignore` must cover at least:

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

However, the following must be committed:

```text
.vscode/extensions.json
.vscode/settings.json
.vscode/tasks.json
.vscode/launch.json
CMakePresets.json
CMake toolchain files
Engine.code-workspace
```

provided that the content contains no personal absolute paths, credentials, or secrets.

## Tool Version Pinning

The repository should be able to record or check:

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

CI should report:

```text
Toolchain fingerprint
```

to facilitate problem reproduction by AI / developers.

## Secret / Signing Separation

The following must not be written into:

```text
.vscode
CMakePresets.json
source tree
```

including:

```text
Signing certificate private data
API secret
Store password
Cloud credential
DRM secret
```

Pass them through:

```text
OS keychain
CI Secret
Environment injection
Provider-specific secure store
```

## Development Environment Definition of Done

```text
✓ A new clone of the repository can complete configure/build using documented commands
✓ VS Code is not a necessary Build dependency
✓ Visual Studio IDE is not required on Windows
✓ macOS/iOS does not depend on manually modified Xcode projects
✓ Android does not depend on an Android Studio-only build
✓ compile_commands.json is available to clangd
✓ CMakePresets.json can describe official build profiles
✓ VS Code tasks only wrap official CLIs
✓ CI uses the same CMake / toolchain contract as the local environment
✓ The AI Agent does not need to operate a GUI to build / test / cook
```



## V3 Development Environment Extensions

V3 adds:

```text
region-server-dev
gateway-dev
determinism-test
training-headless
simulation-worker
offline-renderer
```

A VS Code compound launch can start the following on a single machine:

```text
World Directory
Gateway
Region A
Region B
Test Client
```

for Handoff / Epoch / Failure testing.

However, production distributed topology must not depend on the IDE.

ML / Simulation Farm:

```text
VS Code
→ edit / debug / launch sample
```

Actual Training:

```text
CLI
Headless
Worker Process
Farm
```

The Determinism Test must be executable without a GUI through:

```text
ctest
or
engine commandlet
```

The V3 AI Agent must not hard-code server process orchestration in `.vscode/launch.json` merely because VS Code supports compound launches.


---

# Document Purpose

This document is neither a product introduction nor a high-level conceptual summary.

It is a “construction-level technical specification + system planning document” for:

```text
Codex / AI Coding Agent
Engine Programmer
Tool Programmer
Technical Lead
Reviewer
CI / Build Engineer
```

## Authority Order

When information conflicts:

```text
1. Architecture Contract of the Master Plan in this version
2. This AI technical construction document
3. ADR / Module Manifest / Schema
4. Code that has passed tests and CI
5. Comments / TODO
```

No AI Agent may independently overturn a higher-level Contract.

## Basic AI Agent Construction Rules

Before each task begins:

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

Prohibited:

```text
✕ Crossing a Module Private Boundary for the sake of rapid completion
✕ Exposing backend native types through the public API
✕ Replacing an existing Handle / ID Contract with a raw pointer
✕ Adding unbounded heap allocation on a hot path
✕ Adding an unplanned global singleton
✕ Secretly using wall-clock time as deterministic / fixed tick state
✕ Bypassing RenderGraph to submit official GPU workloads directly
✕ Allowing a Plugin to include an Engine private header
✕ Changing an Optional Feature into a Core Dependency for convenience
✕ Modifying a large number of files unrelated to the task
```

Every AI task output must include at least:

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

# Common Naming and Directory Principles

Recommended repository:

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

Each Runtime Module:

```text
<Module>/
├─ Public/
├─ Private/
├─ Tests/
└─ CMakeLists.txt
```

Rules:

```text
Public/
→ Only stable public contracts may be placed here

Private/
→ backend / implementation / third-party wrapper

Tests/
→ module contract tests
```

---

# Basic C++ / ABI Contract

C++:

```text
C++20
```

Smart pointer rule:

```cpp
template<typename T>
using SharedPtr = std::shared_ptr<T>;
```

Only this ownership alias is permitted.

Prohibited:

```text
UniquePtr alias
WeakPtr alias
```

Non-owning:

```text
T*
T&
Span/View
Handle
ID
```

Engine / GPU / Entity:

```text
Handle / ResourceID / EntityID / UUID
```

are preferred over shared ownership.

Across a Stable ABI:

```text
Opaque Handle
POD Struct
Span = pointer + count
Versioned Function Table
Explicit Allocator Ownership
Explicit String Encoding = UTF-8
```

Prohibited across an ABI:

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

# Error Handling

Runtime API principles:

```text
Expected / Result
ErrorCode
Diagnostic Context
```

The following must not be used as the sole handling mechanism for recoverable input errors:

```text
assert
```

Assert is used for:

```text
Programmer Error
Invariant Violation
Impossible Internal State
```

User / Asset / Network / Plugin input errors:

```text
Return Error
Log Structured Diagnostic
Keep Previous Known-good State when applicable
```

---

# Basic Threading Rules

Official threads/services:

```text
Main / Gameplay
Render
Worker Pool
IO Service
Audio Thread
Platform-specific Media / Network services
```

Avoid:

```text
One permanent OS thread per subsystem
```

AI / Nav / Streaming / Asset work should preferentially use:

```text
TaskGraph / JobSystem
```

Cross-thread payloads should preferentially be:

```text
POD
Handle
Batch
Command Buffer
```

Do not pass mutable object graphs across threads.

---

# Basic Memory Rules

Categories:

```text
Persistent Heap
Pool / Slab
Frame Arena
Job Scratch
Streaming
GPU Upload / Readback
Module Boundary
```

Every large subsystem must have:

```text
Memory Tag
Current
Peak
Allocation Count
Budget
```

Frame memory:

```text
Must not cross Frames
Must not be retained long-term by async jobs
Must not be retained by Plugins
```

---

# Test Hierarchy

Every System must consider at least:

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

The CI Gate is an Architecture Contract, not acceptance testing to be added at the end.


---


# V3 Technical Positioning

V3 = **Distributed / Simulation / Next-Gen**.

The core V3 principle:

```text
Higher capability ceiling
≠ Larger default Build
```

All heavyweight V3 capabilities are Optional Capability Domains.

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

Only state registered in the deterministic domain is subject to the deterministic contract.

## State Schema

```text
Stable TypeID
Stable PropertyID
Canonical Serialization
Canonical Endianness
Stable Iteration
Explicit RNG
```

Prohibited:

```text
raw pointer
unordered_map iteration as gameplay order
wall clock
OS random
GPU async result
```

---

# V3 Deterministic Scheduler

Parallel execution is permitted, but results must not be affected by thread timing.

Strategy:

```text
Stable Chunk Partition
↓
Local Command Buffer
↓
Stable Merge Order
↓
Stable Reduction
```

Prohibited:

```text
race winner = gameplay outcome
```

---

# V3 State Hash

Each tick may perform:

```text
Canonical State Bytes
↓
Hash
```

Debugging may be refined to:

```text
World Hash
System Hash
Component Type Hash
Entity Range Hash
```

for rapid desync localization.

---

# V3 Rollback

Snapshot strategy:

```text
Full Snapshot every K ticks
+
Delta / page copy between
```

The specific approach depends on the state size profile.

Rollback:

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

Presentation smoothing is not part of deterministic state.

The Deterministic Domain defaults to Bounded / Pre-loaded (fully Resident before Tick 0 and not participating in Streaming Unload). If it must cover a V2 Streamed large world, an ADR must be created and a Deterministic Residency Log implemented (recording Cell Residency every Tick). `Restore T` must first restore the Residency for the corresponding Tick before Replay; otherwise, Cell Collision during Replay will differ from the original Tick and determinism will be broken.

---

# V3 Lockstep

Server-coordinated default:

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

Late input policy:

```text
Drop
Delay
Rollback
```

It is defined by the game profile and cannot be silent.

---

# V3 Deterministic Physics

An independent Plugin.

Goal:

```text
Small / medium deterministic simulation
```

Jolt full feature parity is not a goal.

Recommended first-version shapes:

```text
Circle / Sphere
AABB / Box
Capsule
Simple Convex
```

Fixed-point or verified deterministic math.

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

These must never be mixed.

Mappings must be explicit.

---

# V3 World Directory

API:

```text
ResolveRegionOwner
AcquireLease
RenewLease
ReleaseLease
WatchRegion
ListNeighbors
```

Data:

```text
RegionID
OwnerNode
Epoch
LeaseExpiry
BuildCompatibility
Health
```

The Directory does not store gameplay entity blobs.

---

# V3 Authority Epoch

Every authoritative message contains:

```text
RegionID
Epoch
```

Receiver:

```text
if epoch != current
→ reject stale
```

This prevents split-brain.

---

# V3 Entity Handoff Protocol

Phases:

```text
Prepare
TransferState
PreCreateGhost
Validate
CommitAuthority
RouteClient
RetireSource
```

Messages must be idempotent.

Each Handoff:

```text
HandoffID
SourceRegion
TargetRegion
EntityGlobalID
SourceEpoch
TargetExpectedEpoch
StateRevision
```

Before CommitAuthority, it must be confirmed that the Target Region’s role Occupied Cell Set (see V1 Terrain Streaming Boundary Contract) has reached a `StreamingPending`-compatible state. If it is not Ready, the Ownership Switch must be delayed, while the role maintains Authority/Collision on the Source side, avoiding a situation in which both sides simultaneously have no Collision.

---

# V3 Border Ghost

A Ghost is not authoritative.

The data retains only:

```text
GlobalEntityID
Transform summary
Velocity
Bounds
Relevance summary
```

It does not execute the complete gameplay system.

---

# V3 Persistence

Region:

```text
Checkpoint
+
Journal
```

Each journal op:

```text
OperationID
Revision
SchemaVersion
Payload
Checksum
```

Recovery must be idempotent.

---

# V3 Rolling Deploy

Server DLL hot swapping is not performed.

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

For temporary cross-version operation:

```text
Compatibility Window
```

must be explicitly supported by the schema / protocol.

---

# V3 GPU Physics Domain

```text
GPUPhysicsWorld
```

Data:

```text
Body SoA
Shape Data
Broadphase Data
Constraint Data
Solver State
Readback Summary
```

CPU interaction:

```text
Command Batch → GPU
Summary Batch ← GPU later
```Do not stall with an immediate per-body query.

---

# V3 GPU Authority

The Project must be marked as:

```text
Visual
Approximate
AuthoritativeIsland
```

Only AuthoritativeIsland affects the corresponding gameplay domain.

If the Dedicated Server has no GPU:

```text
Must not depend on client-only GPU authority
```

Therefore, key simulation for an online authoritative game requires a server-capable profile or CPU equivalent policy.

---

# V3 GPU Fluid

Optional:

```text
SPH
Grid
Hybrid
```

The first-version API does not expose solver private buffers.

Consumer:

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

RenderGraph Pass:

```text
Build / Update AS
Ray Pass
Denoise
Composite
```

No RT:

```text
No AS allocation
No RT shader cook
```

---

# V3 RT Effects

Feature modules:

```text
RT.Shadow
RT.Reflection
RT.AO
RT.GI
```

Each has:

```text
Quality Profile
Ray Budget
Fallback
Denoiser
```

---

# V3 Path Tracer

Use cases:

```text
Golden Reference
Editor Lookdev
Photo Mode
Offline Capture
```

Do not share all heuristics with the realtime renderer; different integrators are allowed.

However, Material semantics must share canonical material data.

---

# V3 Mesh Shader / Cluster

Cook:

```text
Mesh
↓
Cluster / Meshlet
↓
Hierarchy
↓
Stream Pages optional
```

Runtime:

```text
Mesh Shader
or
Indexed Indirect Fallback
```

Content must not have only a single vendor path.

---

# V3 Work Graph

Optional experimental.

Only the following are allowed:

```text
Capability Check
Feature Plugin
Fallback Path
```

Gameplay / asset schema must not depend on it in order to exist.

---

# V3 ML Environment

Interface:

```cpp
struct EnvironmentAPI {
    ResetResult reset(uint64_t seed);
    StepResult step(ActionBatchView actions);
    ObservationBatchView observations() const;
};
```

The actual Stable ABI uses C POD / function table.

Environment:

```text
No JSON per tick
No per-agent cross-language callback
```

Use batch buffers.

---

# V3 Observation Schema

Field:

```text
Scalar
Vector
Discrete
Mask
EntitySet
TensorHandle
```

Each schema:

```text
SchemaID
Version
Shape
Type
Normalization
Semantic Name
```

The policy artifact is bound to the schema hash.

---

# V3 Action Schema

```text
Discrete
MultiDiscrete
Continuous
Hybrid
ActionMask
```

Action conversion:

```text
AI Action / CharacterIntent / Gameplay Command
```

It must not directly set arbitrary engine memory.

---

# V3 Trainer Core

Common services:

```text
Rollout Buffer
Replay Buffer
Optimizer Adapter
Checkpoint
Metric Sink
Evaluator
Policy Store
```

Algorithm Plugin:

```text
PPO
SAC
DQN
```

Trainer metadata:

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

Minimum technical requirements:

```text
Vectorized rollout
GAE
Clipped objective
Value loss
Entropy
Mini-batch epochs
Checkpoint
```

Do not write PPO into EngineCore; it is `ML.Training.PPO`.

---

# V3 SAC

Required:

```text
Replay Buffer
Actor
Twin Critic
Target network
Entropy temperature
```

Primarily suitable for continuous action.

---

# V3 DQN

Required:

```text
Replay Buffer
Target Network
Epsilon / exploration policy
Discrete action
```

Prioritized replay can be added later and must not block the initial version.

---

# V3 Self-play

Policy Pool:

```text
Main
Historical
Exploit
Evaluation
```

Matchmaker:

```text
Rating
Sampling Policy
Scenario
Seed
```

Results must not store only win/loss; evaluation metrics must also be preserved.

---

# V3 Curriculum

Stage data:

```text
Conditions
Environment Overrides
Opponent Distribution
Reward Config
Observation Noise
```

Stage transition:

```text
Metric-based
Manual
Schedule
```

---

# V3 Simulation Farm

Coordinator data:

```text
JobID
BuildID
EnvironmentID
PolicyVersion
RequiredCapability
LeaseTTL
Priority
```

Worker:

```text
Register
Lease Job
Run Episodes
Upload Rollout / Metrics
Heartbeat
Complete
```

Worker crash:

```text
Lease expiry
↓
reassign
```

---

# V3 Dataset

Chunk format:

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

Compression / sharding can be profile-driven.

---

# V3 Massive Crowd

Simulation tiers:

```text
Full Individual
Reduced Individual
Aggregate
Dormant
```

Transition:

```text
Individual → Aggregate
```

The following must first converge:

```text
Count
State Distribution
Resource / Faction summary
```

Materialize:

```text
Aggregate
↓ deterministic/seeded spawn policy
Individuals
```

Avoid duplication.

---

# V3 Security Boundary

Network / distributed:

```text
Authenticate
Authorize
Validate Size
Validate Version
Replay Protection where needed
Rate Limit Hook
Audit
```

Plugin:

```text
Privilege Metadata
Trust Level
```

However, a native plugin is not a secure sandbox.

---

# V3 Artifact Provenance

Artifact metadata:

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

An unstable timestamp must not be included in the deterministic artifact hash.

---

# V3 Build Size Analyzer

Output:

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

Dependency chain:

```text
Feature A
→ Plugin B
→ SDK C
→ Shader Family D
```

---

# V3 AI Implementation Rules

Classify every AI task first:

```text
Deterministic
Distributed
GPU
ML
Tooling
```

If it spans more than two high-risk Domains:

```text
Split the Task
```

For example:

```text
Do not have one task simultaneously handle
Region Handoff + Persistence + Client Prediction + UI
```

A Distributed task must specify:

```text
Authority Owner
Epoch Behavior
Retry / Idempotency
Failure State
Recovery
```

A Deterministic task must specify:

```text
Input Order
Iteration Order
RNG Source
Snapshot State
Hash State
```

A GPU task must specify:

```text
Queue
Resource Lifetime
Readback Latency
Fallback
Capability
```

An ML task must specify:

```text
Schema
Batch Shape
Checkpoint
Reproducibility
Runtime Export
```

---

# V3 Milestone AI Work Packages

## M1-M2 Determinism

Can be split into:

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

Hash / Serialization schema has a single owner.

## M3-M4 Distributed

Can be split into:

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

Handoff protocol schema has a single owner.

## M5-M8 GPU / Renderer

Divide into:

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

Every feature must have a capability/fallback test.

## M9-M11 ML

Divide into:

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

Do not couple Trainer Core with PPO.

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

Completion means:

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

# AI Final Submission Format

When each implementation task is completed, report:

```md
# Result

## Summary
What was completed.

## Files Changed
- path
- path

## Architecture Contract
Which existing contracts were followed.

## API / ABI
Whether there were any public API / ABI / schema changes.

## Threading
New job / queue / thread interactions.

## Memory
allocation / ownership / lifetime.

## Serialization
version / migration / compatibility.

## Tests
Which tests were run and their results.

## Performance
Whether any hot path / allocation / sync point was added.

## Risks
Remaining risks.

## Next
The next smallest work package.
```

Do not reply only:

```text
Done
```

Verifiable evidence must be provided.