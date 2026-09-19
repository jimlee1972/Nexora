# Cross-Platform 3D Engine — V1 AI Construction Technology and System Planning

**Document Version: AI Technical Draft v1.2**
**Corresponding Source: Cross-Platform3D_Engine_V1_Complete_Planning_v1_2.md**
**Purpose: AI implementation, Engine Programmer implementation, system decomposition, Code Review, CI Gate.**



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

should list:

```text
clangd
CMake Tools
Zig language support
C/C++ debugger support as needed
Shader / Slang syntax support if available
Git tooling optional
```

Extensions are recommended dependencies only and must not become necessary conditions for Build correctness.

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

Do not manually maintain large numbers of include paths.

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

It must not serve as the source of settings required by CI.

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

It is sufficient to install only:

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

Primary editor:

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

No remote native code download / replace path may be provided.

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

Shader build, reflection, and variant cook must be executable by:

```text
CLI
CI
Headless Tool
```

## VS Code Tasks

`.vscode/tasks.json` should be used only as a command wrapper.

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

`.vscode/launch.json` should include:

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

Debug launch arguments must be consistent with the official CLI.

## Command-line First

Any feature that:

```text
can only be executed by pressing a button in VS Code
```

is not considered complete.

It must provide a CLI for:

```text
configure
build
test
cook
validate
package
```

VS Code is merely a UI shortcut.

This is a necessary condition shared by the AI Agent, CI, Build Farm, and Headless Tool.

## AI Agent Modification Rules in the VS Code Project

The AI Agent should prioritize modifying:

```text
Source
CMake
Tests
Schemas
Tool scripts
Repository-owned configuration
```

Avoid modifying:

```text
Generated IDE project
Build output
Cache
Local user settings
CMakeUserPresets.json
.vscode local-only override
```

unless the task itself concerns VS Code workspace configuration.

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

to facilitate issue reproduction by AI / developers.

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
✓ VS Code is not a required Build dependency
✓ Visual Studio IDE is not required on Windows
✓ macOS/iOS does not depend on manually edited Xcode projects
✓ Android does not depend on an Android Studio-only build
✓ compile_commands.json is available to clangd
✓ CMakePresets.json can describe official build profiles
✓ VS Code tasks only wrap official CLIs
✓ CI uses the same CMake / toolchain contract as the local environment
✓ The AI Agent does not need to operate a GUI to build / test / cook
```



## V1 Development Environment Implementation Scope

V1-M0 must directly establish:

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

V1-M0 must not only create a Windows project and then add “cross-platform support later.”

The following must exist from M0:

```text
Windows configure smoke
macOS configure smoke
Android toolchain configure smoke
iOS toolchain configure smoke
```

Actual platform RHI / Zig executable smoke must be completed before M2.

Recommended V1 development defaults:

```text
Windows Host
→ VS Code + Ninja + MSVC/clang-cl

Editor
→ EngineEditor target

Gameplay
→ Zig module target
```

Run the Editor with:

```text
VS Code F5
→ launch EngineEditor
```

However, CI uses:

```text
cmake --preset ...
cmake --build --preset ...
ctest --preset ...
```


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
1. Architecture Contract of the Master Plan for this version
2. This AI technical construction document
3. ADR / Module Manifest / Schema
4. Code that has passed tests and CI
5. Comments / TODO
```

No AI Agent may independently overturn a higher-level Contract.

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
✕ Exposing backend native types in a public API
✕ Replacing an existing Handle / ID Contract with a raw pointer
✕ Adding unbounded heap allocation to a hot path
✕ Adding an unplanned global singleton
✕ Using wall-clock time as deterministic / fixed tick state
✕ Bypassing RenderGraph to submit official GPU workloads directly
✕ Allowing a Plugin to include Engine private headers
✕ Changing an Optional Feature into a Core Dependency for convenience
✕ Modifying large numbers of files unrelated to the task
```

Each AI task output must contain at least:

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

as the only handling mechanism for recoverable input errors.

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
must not cross Frames
must not be retained long-term by async jobs
must not be retained by Plugins
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


# V1 Technical Positioning

V1 = **Production Foundation**.

Goal:

```text
A single Engine Architecture
→ can be used to actually create, Build, Profile, Package, and release general 3D games
```

V1 does not pursue:

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

Optional Modules must be removable by the Build Profile.

---

# V1 Build System Technical Specification

## CMake Target Layering

Recommended:

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

Public dependencies may only be specified through:

```cmake
target_link_libraries(Target
  PUBLIC  ...
  PRIVATE ...
)
```

Prohibited:

```text
A Public target leaking dependencies because of private implementation include paths
```

## Build Profile

At minimum:

```text
Editor
DesktopClient
MobileClient
DedicatedServerFoundation
Tool
Benchmark
```

Each profile generates:

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

Do not create an unbounded God Singleton.

Recommended:

```cpp
struct EngineServices {
    IMemoryService* memory;
    IJobSystem* jobs;
    IVirtualFileSystem* vfs;
    ILogService* log;
    IPluginHost* plugins;
};
```

Separate global process services from World-local services.

```text
Process
├─ EngineServices
└─ Worlds[]
```

---

# V1 ID / Handle System

## EntityID

Recommended 64-bit:

```text
index
generation
```

Rules:

```text
generation == 0
→ invalid
```

destroy:

```text
generation++
```

generation wrap:

```text
retire slot
```

Do not allow a stale handle to become valid again.

## UUID128

Persistent identity:

```text
Scene Object
Asset
Prefab Local Object where required
```

Do not use UUID as a dense index on runtime hot paths.

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

The Job System must provide:

```text
Work Stealing
Priority
Completion Fence
Cancellation
Wait / Help Execute
Debug Metadata
```

Do not:

```text
busy-spin wait on the Gameplay thread
```

---

# V1 TaskGraph / SystemGraph

Recurring World System:

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

Compiler:

```text
System Descriptors
↓
Conflict Analysis
↓
DAG
↓
Cached Schedule
```

Structural changes:

```text
WorldCommandBuffer
```

are committed only at a barrier.

---

# V1 Time

Clocks:

```text
RealClock
GameClock
UnscaledClock
FixedTickClock
```

World:

```cpp
struct WorldTimeState {
    double game_time;
    double unscaled_time;
    uint64_t fixed_tick;
    float time_scale;
    bool paused;
};
```

The fixed accumulator must have:

```text
MaxCatchUpTicks
AccumulatorClamp
```

to prevent spiral-of-death.

---

# V1 Event Framework

Typed:

```text
EventTypeID
```

Modes:

```text
Immediate
Deferred
CrossThread
CrossABI Batch
```

Subscription:

```text
SubscriptionHandle(index,generation)
```

Plugin unload:

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

Rule:

```text
World ≠ Scene
```

World owns simulation services:

```text
PhysicsWorld
NavigationWorld
RenderWorld
Audio Context
```

Scene owns loadable / serializable content.

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

Completion of async work does not mean direct transition to Active.

Activation:

```text
Safe Barrier
↓
Atomic register to world services
```

---

# V1 Transform System

Data-oriented storage:

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
```Update:

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

Reparent:

```text
KeepLocal
KeepWorld
```

Restrictions:

```text
No hierarchy cycle
Parent same Scene
```

Large World:

```text
High Precision World Position
+
Local Float
+
Camera-relative GPU transform
```

---

# V1 Component Storage

Default:

```text
Sparse Set
+
Dense Entity List
+
SoA Fields
```

Prohibited:

```text
Component::Update virtual call per entity
```

System batch iterate component view.

High frequency particles / debris:

```text
Dedicated Pool
```

Do not use Scene EntityID.

---

# V1 Reflection

Canonical metadata:

```text
TypeDescriptor
PropertyDescriptor
Attribute
```

Type:

```text
Stable TypeGUID / TypeID
```

Property:

```text
Stable PropertyID
```

rename:

```text
Alias / Migration
```

Do not rely solely on the current property string hash in a way that breaks old data.

V1 metadata producer:

```text
Macro / constexpr / runtime registration
```

The Consumer contract must be reusable by the V2 Clang generator.

---

# V1 Serialization

Authoring:

```text
JSON
```

Runtime:

```text
Cooked Relocatable Binary
```

Prohibited:

```text
raw struct dump
raw pointer
native STL memory image
```

Binary:

```text
Magic
Version
Schema
Section Table
Offset / Index Ref
Alignment
Checksum
```

Object graph:

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

DDC key:

```text
Source Hash
Importer Version
Import Settings
Dependency Hashes
Target Profile
```

failed import:

```text
Keep Previous Known-good Artifact
```

---

# V1 Mesh / Model

Primary open interchange:

```text
glTF / GLB
```

FBX:

```text
Editor-only importer
```

Runtime Mesh:

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

Tangents:

```text
MikkTSpace compatible
```

Skin influence:

```text
Runtime default max 4
```

The cook policy may compress 8 influences from the source to 4.

---

# V1 Bundle / Generation

Bundle:

```text
Asset / Data only
```

Native code:

```text
Never remote bundle
```

Generation:

```text
Active Generation
Pending Generation
Pinned Old Generation
Pending Delete
```

Update:

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

Stores only:

```text
Configuration / Design Data
```

Does not store:

```text
Mutable Runtime State
Save State
Entity State
```

Authoring:

```text
JSON / CSV / XLSX
```

Runtime V1:

```text
Canonical JSON / typed immutable runtime structure
```

Schema:

```text
Source of Truth
```

Hot reload:

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

Backend:

```text
DX12
Vulkan
Metal
```

No OpenGL.

RHI public:

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

Prohibited from Public RHI exposure:

```text
ID3D12Resource*
VkImage
MTLTexture*
```

## RenderGraph

Pass declarations:

```text
Reads
Writes
CreateTransient
Queue
```

Compiler:

```text
Dependency
Topological Sort
Lifetime
Aliasing
Barrier
Queue Sync
```

All production Renderer Features:

```text
must register through RenderGraph
```

---

# V1 Shader

Source:

```text
Slang only
```

Outputs:

```text
DXIL
SPIR-V
MSL
```

Canonical Reflection:

```text
Resource ID
Binding
Type
Stage
Constant Layout
Material Parameter Layout
```

Variant:

```text
Theoretical
↓ prune
Used
↓ cook
Budget
```

Do not impose a fixed global 256-variant hard limit.

---

# V1 Material / Shading

Shading Model:

```text
PBR
StylizedPBR
Anime
Vegetation
Water
Unlit
```

Shared by the Renderer:

```text
Forward+
RenderGraph
Shadow
Probe
Fog
Post
```

Do not build a separate renderer for Anime / Vegetation.

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

Supports:

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

Gameplay projection uses the following by default:

```text
Non-jittered matrix
```

---

# V1 Lighting / Shadow

Light:

```text
Directional
Point
Spot
```

Forward+ registry.

Shadow Manager:

```text
CSM
Spot Atlas
Point strategy
Static cache
Dynamic budget
```

Shadow importance:

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

V1:

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

The Raw API always exists.

Action Mapping:

```text
Optional Convenience
```

Do not have:

```text
Engine semantic Move / Jump / Attack
```

Supports simultaneously:

```text
Keyboard
Mouse
Gamepad
Touch
simultaneously
```

Pointer:

```text
Stable PointerID
Per-pointer Capture
Single Owner
```

UI Routing:

```text
InputLayer → UILayer
```

The directional matrix is not symmetric.

---

# V1 Runtime UI

```text
UIElement ≠ SceneNode ≠ EntityID
```

Boundary:

```text
UIDocument
```

UI storage:

```text
Handle / Pool / data-oriented
```

Supports:

```text
ScreenSpace
WorldAnchored
WorldSpace
```

Text pipeline:

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

Pointer:

```text
Capture
Target
Bubble
```

---

# V1 Localization / Text Edit

Localization backend:

```text
ICU4C private backend
```

stable key:

```text
LocalizationKeyID
```

language switch:

```text
LocalizationGeneration++
```

When disabled UI is re-enabled, it must refresh if the generation has changed.

TextEdit:

```text
UTF-8 Buffer
Selection
Caret
Composition
Local Undo
Validation
```

The caret uses grapheme clusters, not byte indices.

---

# V1 Physics

Gameplay truth:

```text
CPU Authoritative / Jolt
```

Domain:

```text
CPUAuthoritative
CPUBatched
GPUVisual
GPUDeferred
```

GPU Visual does not write back to gameplay truth.

Query:

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

At minimum, the production fields are:

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

Character does not use a Dynamic RigidBody by default.

Streaming Boundary (see Master Plan Terrain Streaming Boundary Contract):

```text
Occupied Cell
= Capsule + Safety Margin
↓
Physics Collision → Pinned
Unaffected by Render / HLOD Residency unloading
```

When the Cell is not Ready or the character moves at high speed / Teleports into an unloaded area:

```text
GroundState = StreamingPending
```

Temporarily pause Ground Snap/Step/Slide; do not directly Free Fall or cause clipping due to missing Collision.

---

# V1 Navigation

Backend:

```text
Recast / Detour private
```

Public:

```text
NavigationWorld
AgentType
Filter
AsyncPathRequest
PathResult
OffMeshLink
Obstacle
```

Nav does not directly modify Transform.

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

V1:

```text
Behavior Tree
Blackboard
FSM Utility foundation
Perception
AI LOD
ML Policy Interface foundation
```

BT:

```text
Asset
↓ compile
Compact Program / Tables
```

Do not clone a node object tree per agent.

---

# V1 Animation

```text
Skeleton Evaluation
≠ Skinning
```

CPU:

```text
Graph
Blend
IK
Root Motion
Event
```

GPU:

```text
Vertex Skinning
Compute foundation
BAT
```

Global Skinning Buffer:

```text
ResourceIndex
```

Instance:

```text
skinningMatrixOffset
```

Important:

```text
skinningMatrixOffset = matrix element offset
```

It is not a ResourceIndex / descriptor / bytes.

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

Default backend:

```text
miniaudio
```

FMOD:

```text
Optional Plugin
```

The audio thread prohibits:

```text
Heap Allocation
File IO
Mutex-heavy lock
Scene lookup
Zig callback
```

Residency:

```text
AudioResourceRegistry
AudioResidencyScope
```

---

# V1 Media / Video

Optional:

```text
Media.Core
Media.Video
```

Backends:

```text
Windows Media Foundation
Android MediaCodec
Apple AVFoundation / VideoToolbox
FFmpeg optional fallback/tooling
```

Portable baseline:

```text
MP4
H.264
AAC
```

Runtime:

```text
VideoPlayer
VideoFrame Queue
VideoTexture
UI.VideoElement
SubtitleTrack
```

A/V:

```text
Audio Clock = master when audio exists
```

Video audio:

```text
PCM
↓
Audio.Core
```

Do not create a second audio device implementation.

---

# V1 Plugin SDK

Third parties:

```text
Stable C ABI
C++ Convenience SDK
PluginHost
Service Registry
Extension Registry
Bridge Plugin
```

A Plugin may:

```text
Register Service
Register Asset Type
Register Importer
Register Cooker
Register Editor Extension
Register Command
Register Render Feature foundation
```

A Plugin may not:

```text
include Engine Private/
```

Memory ownership across modules must be explicit.

---

# V1 Large World Foundation

```text
Logical Scene
≠ Streaming Cell
```

V1 Partition:

```text
Stable Fixed Grid Cell
+
Loose Quadtree acceleration
```

Indoor:

```text
Room / Portal
```

Streaming:

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

Cell:

```text
Unloaded
Requested
IOResident
BuiltInactive
Active
Retiring
```

Character-Occupied Cell (a Kinematic Character exists within the Cell or its Adjacent Prefetch range):

```text
Physics Collision Residency
→ Pinned, tracked independently of Render/HLOD Residency
→ Must not enter Retiring
```

The Cell may enter normal Retiring only after it leaves the Occupied / Adjacent Prefetch Set of all Characters.

HLOD:

```text
Offline Build
Runtime selection / streaming only
```

---

# V1 Editor

Service:

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

Selection:

```text
EditorObjectHandle
```

Not a raw Node pointer.

Undo:

```text
Transaction Journal
```

---

# V1 AI Construction Task Template

For each Issue / Agent Task, the following is recommended:

```md
## Goal
State in one sentence the observable behavior after completion.

## Owning Module
Engine/...

## Allowed Files
List the directories that may be modified.

## Forbidden Boundaries
Modules / ABIs that must not be touched.

## Inputs
Existing interfaces / schemas / assets.

## Outputs
New interfaces / runtime behavior.

## Invariants
Contracts that must not be broken.

## Threading
The thread / job on which it executes.

## Memory
Allocator / lifetime / ownership.

## Serialization / ABI
Whether the schema / ABI version is changed.

## Tests
Unit / integration / golden / perf.

## Done
Automatically verifiable conditions.
```

---

# V1 Milestone AI Work Packages

## M0

AI may work in parallel on:

```text
Build Profiles
Module Graph Validator
Feature Resolver
CI bootstrap
Formatting / lint
```

Do not modify the same root CMake target definition in parallel unless ownership has first been divided.

## M1

Work packages:

```text
Memory
Logging
Time
Event
Job
VFS
```

Each must first have its own unit tests, followed by Engine Init integration.

## M2

Work packages:

```text
Slang Compiler Wrapper
Canonical Reflection
DX12 PoC
Vulkan PoC
Metal PoC
Zig ABI PoC
```

There may be only one Canonical Reflection owner to prevent the three backends from defining their own schemas.

## M3

Divide the Renderer into:

```text
RHI Resource
Descriptor
PSO
Command
Fence
RenderGraph
Frame Pipeline
```

RenderGraph barrier ownership must have a single responsible owner.

## M4-M12

Each milestone must include at least:

```text
One Vertical Slice Test
One Failure Test
One Performance Baseline
One Feature-strip Test
```

---

# V1 Mandatory CI Gates

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
Character Streaming Boundary
Audio Residency
Media Decode
Streaming Cell
Shipping Matrix
```

---

# V1 Definition of Technical Complete

AI must not determine completion by saying “the feature appears to work.”

The following are required:

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

# AI Final Submission Format

When each construction task is completed, report:

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
Whether there were public API / ABI / schema changes.

## Threading
New job / queue / thread interactions.

## Memory
Allocation / ownership / lifetime.

## Serialization
Version / migration / compatibility.

## Tests
Which tests were run and their results.

## Performance
Whether any hot path / allocation / sync point was added.

## Risks
Remaining risks.

## Next
The next smallest work package.
```

Do not reply only with:

```text
Done
```

Verifiable evidence must be provided.