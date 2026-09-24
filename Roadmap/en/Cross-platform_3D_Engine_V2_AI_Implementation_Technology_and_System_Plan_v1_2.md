# Cross-Platform 3D Engine — V2 AI Construction Technology and System Planning

**Document Version: AI Technical Draft v1.2**
**Corresponding Source: Cross-platform_3D_Engine_V2_Complete_Plan_v1_4.md**
**Purpose: AI implementation, Engine Programmer implementation, system decomposition, Code Review, CI Gate.**

> **Progress: 15%** (✅ V2-M0 and ✅ V2-M1 are accepted; V2-M2 through V2-M12 remain open.)



---

# Development Environment and IDE Baseline (Normative)

This project officially adopts:

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

may be used, but VS Code remains the primary working environment for daily Engine / Gameplay programming.

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

Do not place critical Build settings only in:

```text
.vscode/settings.json
.vscode/c_cpp_properties.json
Visual Studio .sln/.vcxproj
Xcode project manual settings
Developer local environment
```

After cloning the repository, CI and new development machines must be able to rebuild without relying on personal IDE settings.

## Repository VS Code Workspace

Recommended at the repository root:

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

Do not extensively maintain include paths manually.

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

Add the following for V2 / V3 as needed:

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

to store private developer-local paths and SDK overrides.

`CMakeUserPresets.json`:

```text
.gitignore
```

It must not be used as the source of settings required by CI.

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

It is acceptable to install only:

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

Primary editing:

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

However, the repository remains authoritative for Engine Source / CMake / Zig / Slang.

Do not manually create core build rules in the Xcode project that are known only to Xcode.

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

The ABI / API Level / STL / Vulkan capability of the Android build is configured by the CMake Preset / Toolchain.

Do not make Android Studio the only buildable path.

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

No remote native code download / replacement path may be provided.

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

Do not depend on an IDE-specific shader compiler.

Output:

```text
DXIL
SPIR-V
MSL
```

Shader build, reflection, and variant cook must be executable through:

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

Tasks must not redefine build logic; they should only invoke repository-owned commands such as:

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

Any function that:

```text
can only be executed by pressing a button in VS Code
```

is not considered complete.

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

## Modification Rules for AI Agents in VS Code Projects

AI Agents should prioritize modifying:

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

provided that the contents do not contain personal absolute paths, credentials, or secrets.

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

to facilitate reproducing issues for AI / developers.

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

They must be supplied through:

```text
OS keychain
CI Secret
Environment injection
Provider-specific secure store
```

## Development Environment Definition of Done

```text
✓ A new cloned repository can complete configure/build using documented commands
✓ VS Code is not a required Build dependency
✓ Visual Studio IDE is not required on Windows
✓ macOS/iOS does not depend on manually modified Xcode projects
✓ Android does not depend on an Android Studio-only build
✓ compile_commands.json is available to clangd
✓ CMakePresets.json can describe official build profiles
✓ VS Code tasks only wrap official CLIs
✓ CI uses the same CMake / toolchain contract as the local environment
✓ The AI Agent does not need to operate a GUI to build / test / cook
```



## V2 Development Environment Extensions

V2 adds the following Presets:

```text
linux-server-dev
linux-server-shipping
network-test
world-build
distributed-worker
benchmark
```

VS Code compound launch may start:

```text
Dedicated Server
+
Client A
+
Client B
```

This is only for Local Development Convenience.

Network CI must not depend on VS Code compound launch.

All V2 Distributed Build / Import Worker / Shader Worker components must be:

```text
CLI executable
```

They cannot be Editor-only in-process tools.

The Remote Device Inspector’s VS Code workflow may have helper tasks:

```text
Deploy
Attach
Start Remote Inspector
```

but the underlying layer must still use official command-line tools / protocols.


---

# Document Purpose

This document is not a product introduction or a high-level conceptual summary.

It is a “construction-grade technical specification + system planning document” for:

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
1. Architecture Contract of this version’s Master Plan
2. This AI technical construction document
3. ADR / Module Manifest / Schema
4. Code that has passed tests and CI
5. Comments / TODO
```

No AI Agent may independently overturn an upper-level Contract.

## Basic AI Agent Construction Rules

Before starting each task:

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
✕ Replacing an existing Handle / ID Contract with raw pointers
✕ Adding unbounded heap allocation to a hot path
✕ Adding an unplanned global singleton
✕ Secretly using wall-clock time as deterministic / fixed tick state
✕ Bypassing RenderGraph to submit official GPU workloads directly
✕ Allowing a Plugin to include Engine private headers
✕ Changing an Optional Feature into a Core Dependency for convenience
✕ Modifying large numbers of files unrelated to the task
```

Each AI task output must include at least:

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

For Engine / GPU / Entity:

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

Prohibited across the ABI:

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

Do not use:

```text
assert
```

as the sole handling mechanism for recoverable input errors.

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

AI / Nav / Streaming / Asset work should prioritize:

```text
TaskGraph / JobSystem
```

Cross-thread payloads should prioritize:

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

Each large subsystem must have:

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
Must not be saved by Plugins
```

---

# Test Hierarchy

Each System must consider at least:

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

The CI Gate is an Architecture Contract, not an acceptance item to be added at the end.


---


# V2 Technical Positioning

V2 = **Scale-Up / Production**.

All V2 technologies are built on the V1 Contract.

Do not, for the sake of implementing V2:

```text
Rewrite World/Scene identity
Break the Stable C ABI
Bypass RenderGraph
Turn an Optional Feature into Mandatory
```

---

# V2 Delta Module Graph

Added / strengthened:

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

Inputs:

```text
C++ Public Reflection Headers
Attributes / Macros
```

Canonical outputs:

```text
TypeDescriptor
PropertyDescriptor
Migration Alias
Replication Metadata
Editor Metadata
```

The Producer may be replaced, but the Consumer Contract does not change.

The Generator must be:

```text
Deterministic
Incremental
Versioned
CI-validatable
```

The Output must not generate unstable IDs based on source file absolute paths.

---

# V2 Content-addressable DDC

Key:

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

L1:

```text
Local DDC
```

L2:

```text
Shared Remote DDC
```

Lookup:

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

A remote outage must not prevent Local Build.

---

# V2 Import Worker

The Importer may run out-of-process.

IPC:

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

The Coordinator validates:

```text
Hash
Schema
Output type
```

Only then may it publish.

---

# V2 GPUScene

Core data:

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

The actual layout may be adjusted according to GPU packing, but the semantics are fixed.

Slot:

```text
index + generation
```

destroy:

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

CPU must not submit every object individually.

The following must be retained for debugging:

```text
CPU Reference Culling
```

It may be used for correctness diffs.

---

# V2 Hi-Z

Source:

```text
Depth
↓
Mip Pyramid
```

Conservative cases:

```text
Camera teleport
Fast camera rotation
New occluder
Large moving object
```

Invalidation / relaxed policy must be provided.

Do not cause objects to flicker out of existence due to aggressive occlusion.

---

# V2 Async Compute

RenderGraph queues:

```text
Graphics
Compute
Copy
```

The Compiler is responsible for:

```text
Queue Assignment
Cross Queue Fence
Ownership
Overlap
```

The Device profile may:

```text
Collapse Compute → Graphics
```

---

# V2 Temporal Upscaler

Interface:

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

Backend:

```text
Engine TAAU
FSR Plugin
DLSS Plugin
XeSS Plugin
```

Gameplay must not know the vendor.

---

# V2 Virtual Texturing

Terrain first:

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

Manager:

```text
Tile Budget
Priority
Residency
Eviction
Upload
```

General material VT is optional.

---

# V2 Adaptive World Partition

V1 stable grid → V2 adaptive authoring/cook partition.

Still maintain:

```text
Cell Identity
≠ Quadtree Node identity
```

CellKey source:

```text
Scene UUID
Partition Domain
Stable Spatial Key
```

Rebuilding should not unnecessarily replace all IDs.

Character Occupied Cell (see V1 Character streaming boundary) is extended to apply: the Physics Collision Pinned determination is based on the “smallest Adaptive Cell covered by the Character Footprint”; changes to the partition shape do not affect the determination logic.

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

All subsystems must go through:

```text
WorldCoordinateService
```

Do not independently cache an assumption that the float world origin is permanently unchanged.

---

# V2 Persistent Cell State

```text
Scene Defaults
+
Persistent Delta
↓
Materialized Runtime Cell
```

The Delta stores only persistent gameplay state.

Prohibited:

```text
dump entire runtime memory
```

Delta capture and Collision Unload are two separate matters: while the Cell remains in any Character’s Occupied Cell Set, Persistent Delta may be captured normally, but Physics Collision must remain Pinned, and Runtime Unload must wait until the Character leaves.

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

Game Rules must not be hard-coded into Net Core.

---

# V2 Transport

Built-in:

```text
UDP-oriented
```

Channel:

```text
Unreliable
UnreliableSequenced
ReliableOrdered
ReliableUnordered
```

Packet:

```text
Protocol
ConnectionID
Sequence
Ack
AckBits
Channel
Payload
```

Required:

```text
MTU
Fragmentation
Reassembly
Rate Control
Timeout
```

Crypto must use a mature library/provider; do not invent primitives.

FRAGMENT END---

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

Client / Server local IDs may be completely different.

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

Slow reflection traversal is prohibited on the hot path.

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

Global replication of all entities is not allowed.

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

Do not assume Jolt cross-platform bitwise determinism.

---

# V2 Dedicated Server

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

Retain：

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

Purpose:

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

Long distance：

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

May span multiple frames.

Do not run synchronous A* for all NPCs in the same frame.

---

# V2 Crowd

Data-oriented：

```text
CrowdAgentPool
```

Output：

```text
Desired Motion
```

Then enter：

```text
CharacterIntent
```

Do not set the transform directly.

---

# V2 Utility AI

```text
Action
├─ Consideration[]
├─ Weight
├─ Curve
└─ Score
```

Evaluate in batches.

Do not allocate a heap-based virtual object for every consideration.

---

# V2 Perception LOD

```text
Near → full
Mid → reduced frequency
Far → coarse/event
Dormant → none
```

Use batched physics queries.

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

The Trainer remains in the tool/headless environment.

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

Suitable for reuse across multiple passes.

GPU Pose：

```text
Clip Data
↓
GPU sample
↓
Pose / Matrix
```

RootMotion / Event remain CPU-authoritative.

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

The Builder must be deterministic.

Motion Matching is an optional framework.

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

Do not use raw pointers.

Runtime：

```text
Evaluate(Time)
↓
Track Commands
```

Editor Scrub does not need to perform a complete gameplay simulation.

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

Must have：

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

Prohibited：

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

Workers must not directly mutate the Asset DB.

---

# V2 Remote Inspector

Transport：

```text
Development-only secure channel
```

Observable：

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

Disabled by default in shipping builds.

---

# V2 AI Construction Rules

For any V2 AI task, first answer：

```text
Does this change V1 Contract?
```

If so：

```text
Must have an ADR + explicit migration
```

If not：

```text
must implement as extension / optional module
```

GPU work：

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

AI can work in parallel on：

```text
Clang Generator
DDC
Import Worker
Commandlet
Scene Externalization
```

However, the Canonical Metadata schema must have a single owner.

## M2-M3 Renderer

Break down into：

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

Each layer must first have a CPU reference / test vector.

## M5-M6 Network

Break down into：

```text
Transport
Handshake
Codec
Snapshot
Interest
Prediction
Replay
```

Do not have a single Agent modify everything at once.

## M10 Distributed Build

Each worker task must have：

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

Must prove：

```text
V1 is not broken
V2 Feature can be disabled independently
GPU-driven has a fallback
Large World build is deterministic
Networking has a failure path
Server has no presentation dependency
Toolchain can run headlessly
Distributed Build is reproducible
LiveOps can roll back
```


---

# Milestone Index


```text
✅ V2-M0  Migration / Baseline
✅ V2-M1  Clang Reflection / DDC / Headless Toolchain
V2-M2  GPUScene
V2-M3  GPU-driven Renderer
V2-M4  Large World V2
V2-M5  Dedicated Server / Transport
V2-M6  Replication / Interest / Prediction / Replay
V2-M7  Navigation / Crowd / AI V2
V2-M8  Animation V2
V2-M9  Timeline / UI V2 / Audio / Media V2
V2-M10 Shared DDC / Distributed Build / LiveOps
V2-M11 Remote Tools / Diagnostics
V2-M12 Hardening / Shipping
```


---

# AI Final Submission Format

When each construction task is completed, report：

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
allocation / ownership / lifetime。

## Serialization
version / migration / compatibility。

## Tests
Tests executed and their results.

## Performance
Whether any hot path / allocation / sync point was added.

## Risks
Remaining risks.

## Next
The next smallest work package.
```

Do not reply only with：

```text
Done
```

Verifiable evidence must be provided.
