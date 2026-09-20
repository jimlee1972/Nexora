# Cross-Platform 3D Engine — V1 Complete Planning Document
**Document Version: Master Draft v1.2**
**Engine Generation: V1.x — Production Foundation**
**Target Platforms: Windows / macOS / Android / iOS**

> This document is the **V1 Master Plan**. It integrates the previous complete engine architecture analysis, Architecture Completion Baseline, Scene/World/Streaming/HLOD, Input, UI, Physics, Animation, Audio, DataTable, Editor, Asset/Bundle, Build/CI, as well as the latest **Plugin SDK / Bridge Layer / Third-Party Extensions / Video & Media Framework**.
>
> If the earlier Plugin summary differs from the “V1 Plugin / Media Detailed Contract” at the end of the document, **the detailed Contract at the end shall prevail**.
>
> V1 is not defined as a “Prototype,” but as: an architecture capable of completing and releasing general 3D games, without blocking V2/V3 expansion for Large World, Networking, Distributed Simulation, GPU Simulation, and ML.


> v4.0 purpose: Building upon the Scene / World, Input, Runtime UI, Physics, Animation, DataTable, WebView designs completed in v3.33, complete the previously insufficiently detailed Transform, System Scheduler, Event, Camera, Lighting, PostProcess, Navigation, AI, Editor, Prefab, Undo, Reflection, Serialization, Asset Import, VFS, Save, Localization, Platform, Logging, Memory, TaskGraph, Timer, and Authoring Tools into an implementable formal Framework.


## I. Engine Positioning

Objectives:
- This engine adopts AI-Assisted Development, with AI providing extensive assistance in architecture design, code generation, test generation, documentation maintenance, and refactoring
- All AI output must pass automated verification Gates; “generation completed” must not be considered “feature completed”
- High-performance native C++20 3D Engine
- Engine Core remains C++20; Zig serves as the Primary Gameplay Language
- The Gameplay language integrates through a Language-Neutral Stable C ABI to avoid being locked to a single language
- Supports Windows / macOS / Android / iOS
- V1 Renderer formally implements DX12 + Vulkan + Metal
- RHI supports DX12 / Vulkan / Metal simultaneously from day one
- OpenGL is not used as the core rendering path
- Windows uses DX12 by default, with Vulkan switching permitted
- Editor uses a Node + Component workflow
- Runtime underlying layer uses Data-Oriented / SoA Component Pool
- Renderer and Scene Graph are separated
- World and Scene are separated; the same World supports Multiple / Additive Scene and Persistent Scene
- World Partition supports Stable Grid Cell + Loose Quadtree + Room / Portal Hybrid Streaming
- HLOD is generated offline by the Editor / Cooker; Runtime only performs selection and Streaming
- Proprietary Editor
- Proprietary Asset Pipeline
- Proprietary Runtime UI
- Built-in Terrain / Vegetation / spatial culling / Mesh LOD / Texture LOD & Streaming
- Image quality target is at least comparable to Unity URP
- Formally supports PBR / StylizedPBR / Anime / Vegetation / Water / Unlit Shading Model
- Data-Oriented VFX / Particle Framework: 1 VFX Instance + N Logical Emitters + Runtime Batch Fusion
- Animation Framework: Animation Graph + GPU Skinning + Skinned Instancing + Automatic Bone Animation Texture / GPU Crowd Animation
- StylizedPBR is used for animated environmental scenes, while Anime NPR is used for characters; both can be mixed within the same Forward+ / RenderGraph
- Mobile-first, while also supporting high desktop image quality
- Explicit control of CPU / GPU / RAM budgets
- Architecture retains future expansion capabilities for Networking, DX12, GPU Driven Rendering, and other features
- Bundle Hot Update / Remote Manifest / Cache / Rollback

Platforms and APIs:

```text
Windows  -> DX12（Default / Primary）
Windows  -> Vulkan（Selectable Secondary Backend）
Android  -> Vulkan（Primary）
macOS    -> Metal（Primary）
iOS      -> Metal（Primary）
```

Principles:
- Gameplay / Scene / Material must not directly depend on Vulkan / Metal types
- All GPU APIs pass through the RHI
- Original assets and Runtime assets are separated
- Runtime must not depend on the Editor
- The Source of Truth must be explicit
- Cache / Derived Data must not be confused with primary data


## II. Major Technology Choices

Languages:
- Engine Core / Renderer / RHI / Editor: C++20
- Primary Gameplay Language: Zig
- Engine ↔ Gameplay boundary: Language-Neutral Stable C ABI
- Architecture retains the capability to add Bindings for Rust / C# and other languages in the future, but V1 does not depend on them

Build:
- CMake
- Precompiled Headers (PCH)
- Separate PCH for Runtime / Renderer / Editor

Renderer:
- Direct3D 12（Windows Default）
- Vulkan（Windows switchable / Android Primary）
- Metal（macOS / iOS Primary）
- Unified RHI

Shader:
- Slang
- DX12   -> DXIL
- Vulkan -> SPIR-V
- Metal  -> MSL / Metal Shader

Notes:
- The Slang Metal backend must undergo PoC validation before formal adoption
- Do not assume that the Slang Metal path is as mature as the SPIR-V path

Editor UI:
- Dear ImGui
- Docking
- Multi-Viewport

Physics:
- Jolt Physics

Font:
- FreeType
- HarfBuzz

Audio:
- miniaudio

Navigation:
- Recast / Detour (V1 default backend; exists only in the private backend)

Localization / Unicode:
- ICU4C (V1 Locale / Plural / Number / Date / Bidi / Break Iterator backend; data is trimmed according to the project locales)

Persistent Allocator Backend:
- mimalloc (V1 default internal backend; replaceable and excluded from the public ABI)

Models:
- glTF / GLB
- FBX Importer（Editor Only）
- OBJ（Basic support）

Texture:
- PNG
- JPG
- TGA
- HDR
- EXR
- KTX2 / BasisU（May be added later）

Scene:
- Node + Component API

Runtime Data:
- Component Pools
- SoA
- Job System
- Dirty Flag
- GPU Instancing
- GPU Driven Rendering later


## Zig Gameplay / Native Scripting Contract

### Positioning

Zig is the **Primary Gameplay Language** of this engine’s V1.

The engine core remains:

```text
C++20 Engine Core
↓
Language-Neutral Stable C ABI
↓
Zig Gameplay Binding
↓
GameModule
```

Zig does not replace:

```text
Renderer / RHI
Asset Pipeline
Editor Core
Job System Core
Physics Backend
Audio Backend
Platform Backend
```

The above core remains primarily implemented in C++20.

Zig is primarily responsible for:

```text
Gameplay Logic
Game-specific Systems
Behavior
State Machine
AI Logic
Quest / Skill / Rule Logic
Game-side UI Logic
Project-specific Native Module
```

### Reasons for Selection

The primary objectives for Zig as the Gameplay Language:

- Native compilation.
- No tracing GC.
- Low Runtime / language overhead.
- Explicit allocator model compatible with the engine’s Arena / Pool / Lifetime Contract.
- Direct C ABI / C header interop.
- Suitable for Data-Oriented / Handle-based APIs.
- Suitable for rapid recompilation and GameModule reload during development.
- Does not require the Engine Core to accommodate language-specific models such as borrow-checker / trait-object ABI.
- Uses the same Gameplay API Contract on Windows / macOS / Android / iOS.

### Language-Neutral Gameplay ABI

Zig must not directly depend on the Engine C++ private class layout.

Formal boundary:

```text
Zig Gameplay
↓
Generated / Handwritten Safe Zig Wrapper
↓
Engine Gameplay C ABI
↓
C++ Public Gameplay API
↓
Engine Internal
```

Prioritize the following across the ABI:

```text
Opaque Handle
EntityID
AssetID / UUID
POD / C-compatible Struct
Explicit Span / View
Versioned Function Table
Error Code / Result Struct
```

Direct exposure across the ABI is prohibited:

```text
std::vector
std::string
C++ template type
SharedPtr<T>
Engine Internal class
C++ RTTI object
Exception
Backend native graphics object
```

### GameModule ABI

The Gameplay module uses a versioned C ABI entry point.

Concept:

```c
GameModule_Init(...)
GameModule_Shutdown(...)
GameModule_Update(...)
GameModule_OnEvent(...)
GameModule_SerializeState(...)
GameModule_DeserializeState(...)
```

The module handshake must validate at least:

```text
Engine API Version
Gameplay ABI Version
Build Configuration
Platform / Architecture
Module Build ID
Capability Flags
```

ABI mismatch:

```text
Reject Module Load
```

Partial initialization is prohibited.

### Hot Reload Contract

Hot Reload is permitted only through an explicit Module Boundary.

Standard process:

```text
Edit Zig Gameplay Code
↓
Compile New GameModule
↓
Stop New Gameplay Dispatch
↓
Reach Module-safe Barrier
↓
Serialize / Extract Module-owned Persistent State
↓
Shutdown Old Module
↓
Ensure No In-flight Call / Job / Callback References Old Module
↓
Unload Old Module
↓
Load New Module
↓
ABI Handshake
↓
Deserialize / Migrate State
↓
Resume Gameplay
```

Do not assume that “native DLL reload” automatically preserves all program state.

### Module-safe Barrier Definition

“Reach Module-safe Barrier” and “Ensure No In-flight Call / Job / Callback References Old Module” are not temporal assumptions; they must be bound to the existing Job System’s Completion Fence mechanism (see Thread-Safe Job System / Frame Synchronization Lifecycle), and must not rely solely on textual conventions.

```text
Identify all Job Class that may call into Zig GameModule
↓
Stop scheduling new instances of those Job Class
↓
Wait for Completion Fence of every in-flight instance
↓
Confirm no pending Callback / Event Dispatch still references old Module
↓
Module-safe Barrier reached
```

Rules:

- Any Job Class that may call into the Zig GameModule must be registered in its Task Handle so that Hot Reload can correctly enumerate the objects to await.
- Barrier determination must query the actual state of the Completion Fence; it must not substitute the assumption that a particular Frame has ended.
- If any Callback / Closure Context cannot be clearly classified, Hot Reload must be considered unsafe and aborted rather than silently skipped.

### Hot Reload Lifetime Rule

After module unload, the Engine must not retain:

```text
Old Zig function pointer
Old module-owned object pointer
Old allocator-owned allocation
Old callback closure / context pointer
Old module code address
Old vtable-equivalent dispatch table
```

All callbacks / function tables must be registered again after reload.

Persistent data across Reload should prioritize:

```text
Engine-owned Handle / Data
or
Versioned Serialized State
```

rather than a module-native object graph.

### Hot Reload Platform Policy

Desktop Editor:

```text
Windows
→ GameModule DLL reload

macOS
→ Development dylib / equivalent local development module reload
```

Mobile:

```text
Android / iOS
→ Gameplay code compiled into the App / native package at Build-time
→ Native gameplay code is not downloaded through Remote Bundle
```

iOS Shipping does not use runtime dynamic downloading / execution of new native code as the Gameplay Hot Update mechanism.

Remote Update still only permits:

```text
Asset
Data
Scene
Localization
Bundle Content
Gameplay Data / Config
```

Zig native binaries must not be placed in a Remote Bundle and then downloaded for execution.

### Memory / Allocator Contract

Zig Gameplay may use:

```text
Zig-local allocator
Engine-provided persistent allocator API
Engine temporary/frame API (only under an explicit lifetime contract)
```

However, it must comply with the existing Dynamic Module Allocator Contract:

```text
Module allocates
→ Module destroys

or

Engine allocator API allocates
→ Engine allocator API frees
```

Prohibited:

```text
Zig Module allocate
→ the C++ engine directly releases it with an incompatible allocator/free
```

When Frame-lifetime memory crosses the Module boundary, it may still only use the existing:

```text
FrameSpan<T>
FrameDataHandle
```

No exception may be introduced for Zig bindings.

### Gameplay API Data Model

Zig Gameplay should primarily operate on:

```text
EntityID
Component Handle
Data Component View
Event
Command
System Batch
```

rather than holding raw C++ Node / Component pointers.

Recommended layering for Gameplay Component / Behavior:

```text
Native Core Component
→ Transform / Render / Physics / Animation

Gameplay Data Component
→ Health / Faction / Inventory / Interactable / project-specific data

Zig Behavior / System
→ Event / Tick Registration / State Machine / Batch Update
```

Not every Gameplay feature is required to create a fixed C++ Component class.

### Tick / Event Policy

Zig Gameplay does not default to:

```text
Every Component
→ virtual Update()
```

Instead:

```text
Batch System
+
Event-driven
+
Explicit Tick Registration
+
Scheduler
```

Tick frequency may be classified as needed:

```text
Every Frame
Fixed Tick
10 Hz
1 Hz
Event-only
Sleeping
```

This avoids a large number of fine-grained Engine ↔ Zig ABI calls.

### Batch-first ABI

Prohibited:

```text
per Entity
per Component
per frame
multiple fine-grained calls across the C++ ↔ Zig ABI
```

Prioritize:

```text
Engine provides contiguous views / batches
↓
Zig processes batch
↓
Command / result batch returned
```

For example:

```text
UpdateAIBatch(...)
ProcessGameplayEvents(...)
UpdateSkillBatch(...)
```

### Reflection / Inspector Bridge

Canonical Engine Reflection Metadata is the Source of Truth.

If a Zig gameplay type requires:

```text
Inspector
Serialization
Prefab Override
Save
Property Animation
```

it must use:

```text
Zig Compile-time Metadata / Codegen
↓
Canonical Engine Reflection Schema
```

It must not establish an independent metadata world incompatible with C++ Reflection.

### Debug / Tooling

Target Editor development workflow:

```text
Double-click Zig gameplay source
↓
Open configured external IDE / editor
↓
Build GameModule
↓
Load / Reload
↓
Native Debugger Attach / Launch
```

At minimum, provide:

```text
Build Error → Editor Console
Source File / Line Mapping
GameModule Build Timing
Reload Timing
ABI Mismatch Diagnostic
State Migration Diagnostic
Gameplay Allocation Metrics
```

### Cross-platform Build Contract

CI must validate the Zig Gameplay Module on:

```text
Windows x64
macOS arm64
Android arm64
iOS arm64
```

and expand according to officially supported platform/toolchain profiles.

The Gameplay Public API must remain platform-independent.

Gameplay Zig code is prohibited from directly depending on:

```text
DX12
Vulkan
Metal
Android JNI internal backend
Objective-C++ Engine Internal
```

Platform-specific functionality must pass through the Engine Public Platform API.

### AI / Safety Gate

Because Zig does not provide compile-time ownership / borrow safety at Rust’s level, AI-generated Zig gameplay code must additionally rely on:

```text
Handle Generation Validation
Debug Allocator
Bounds / Safety Checks（Development）
ASan / UBSan (on supported platforms)
Module Lifetime Guard
Frame Lifetime Guard
Static Analysis
Unit / Integration Test
Long-run Reload Test
```

AI must not consider “Zig compiled successfully” proof of lifetime correctness.

### V1 Definition of Done

- C++ Engine Core and Zig Gameplay complete basic integration through a Stable C ABI.
- Zig can create, read, and modify public Gameplay Component Data.
- Zig can receive Events and register Explicit Ticks.
- The Desktop Editor can recompile and Reload the GameModule.
- Engine-owned Entity / Asset / Component Handles remain valid before and after Reload.
- Module-owned persistent state can be restored through versioned state migration.
- No old function pointer / callback / module allocation escape remains after Reload.
- Windows / macOS / Android / iOS CI can all compile a minimal Zig Gameplay sample.
- Shipping Mobile does not depend on runtime native-code download.
- The Profiler can observe Gameplay Update, ABI Call Count, Allocation, and Reload Cost.

## III. AI-Assisted Development Governance Principles

This engine explicitly adopts AI-Assisted Development.

AI’s roles:
- Architecture drafts
- Module implementation
- Refactoring
- Test generation
- Shader / RHI Backend implementation
- Editor Tool
- Serialization
- Documentation maintenance
- Bug analysis
- CI Script
- Platform adaptation

Principle:
AI-generated code ≠ completed code.
All output from any AI Session / Agent must comply with:
1. Established architecture
2. Coding Convention
3. Static Analysis
4. Unit / Integration Test
5. Platform Validation
6. Module-specific Gate
7. Definition of Done


### AI Autonomy Classification

A. AI may generate with a high degree of autonomy:
- Reflection templates
- Serialization
- Asset Metadata
- Inspector UI
- Runtime UI Component
- Editor Utility
- Mechanical implementation of the RHI Backend under established Interfaces
- Platform Wrapper
- Test Case
- Documentation
- Build Script
- Code Migration / Rename / Mechanical Refactor

B. AI may generate, but the result must pass tool validation before merging:
- Vulkan / Metal Barrier
- Resource State Transition
- Queue Synchronization
- Descriptor / Argument Buffer
- Slang Shader
- Render Graph
- Job System
- Lock-free / Concurrent Structure
- Memory Allocator
- GPU Driven Rendering
- Asset Streaming
- Save Migration

C. AI may not consider the following complete through static inference alone:
- Android GPU Driver compatibility
- Actual iOS / macOS Metal behavior
- GPU Crash
- Frame Pacing
- Thermal / Battery
- Mobile Memory Pressure
- Shader Compiler / Driver-specific issues

The above items must be validated through physical devices or corresponding platform tools.


### Mandatory Gates Before AI Merge

All AI-generated PRs must pass the following according to their scope of impact:

Basic Gate:
- clang-format
- clang-tidy
- Build
- Unit Test
- No New Warning
- API Convention Check

Renderer Gate:
- Shader Compile Test (compile only affected variants according to the dependency graph)
- Render Graph Validation
- Resource Lifetime Validation
- Golden Image Test
- GPU Validation Layer
- GPU Capture Smoke Test

Asset Gate:
- Import / Reimport Test
- Dependency Test
- Deterministic Build Test
- Runtime Asset Load Test
- Bundle Dependency DAG Check

UI Gate:
- Layout Test
- Multi-resolution Test
- Draw Call / Batch Regression Check
- Text / Font Fallback Test

Platform Gate:
- Native Build
- App Launch
- Suspend / Resume
- Save Path
- Crash Handler
- Memory Budget Smoke Test

VFX Gate:
- VFX Graph Compile Determinism
- Dependency DAG Cycle Fail
- GPU Buffer Fence-safe Reuse
- Soft Particle / Billboard / Mesh / Ribbon Golden Image
- VFX Feature Stripping Validation

Animation Gate:
- Animation Graph Compile Determinism
- Bind Pose Hash Mismatch Hard Fail
- Vertex / Compute Skinning Golden Image
- GPU Animation Bake Determinism
- GPUAnimationClip Dependency Invalidation Test

Merge is permitted only after all Gates pass.

### Consistency Across Multiple AI Sessions / Agents

All Sessions must share and comply with:
- ARCHITECTURE.md
- CODING_CONVENTION.md
- RHI_CONTRACT.md
- MEMORY_MODEL.md
- THREADING_MODEL.md
- ERROR_HANDLING.md
- ASSET_FORMAT.md
- TESTING_POLICY.md
- DEFINITION_OF_DONE.md
- GAMEPLAY_ABI.md
- ZIG_GAMEPLAY_GUIDE.md

Prohibited:
- An individual Session independently changing the core architecture
- Two different Ownership models for the same problem
- Some modules using Exceptions while others arbitrarily use Error Codes
- Some modules using Raw Pointer Ownership while others use Handles
- Modifying a core Contract without updating the architecture documentation

Major architecture changes must:
1. Update the Architecture Decision Record（ADR）first
2. Modify the Interface next
3. Have AI implement the Backend next
4. Finally pass the Regression Gate


## IV. Coding Convention / Static Analysis

This chapter contains mandatory specifications for all manually and AI-generated code.
Coding Style addresses not only formatting, but also Ownership, Error Handling, Threading, Resource Lifetime, and Hot Path design principles.

### 4.1 Naming Convention

```text
Class / Struct / Enum      PascalCase
Function / Method          PascalCase
Local Variable             camelCase
Member Variable            mCamelCase
Static Variable            sCamelCase
Global Constant            kPascalCase
Enum Value                 PascalCase
Template Parameter         PascalCase
File Name                  PascalCase.h / PascalCase.cpp
Namespace                  lowercase
```

Example:

```cpp
class TextureManager
{
public:
    TextureHandle CreateTexture(const TextureDesc& desc);

private:
    GraphicsDevice* mDevice = nullptr;
    uint32_t mTextureCount = 0;
};
```

### 4.2 Namespace

Use nested namespace shorthand:

```cpp
namespace engine::render
{

class RenderGraph
{
};

}
```

Do not place large numbers of Engine Types in the global namespace.

### 4.3 Header / Source Separation

```text
TextureManager.h
TextureManager.cpp
```

Header principles:
- Prefer forward declarations whenever possible
- Reduce unnecessary includes
- Do not place large implementations in headers
- Keep public APIs stable whenever possible

### 4.4 Smart Pointer Naming

The engine provides a standard smart pointer alias:

```cpp
template<typename T>
using SharedPtr = std::shared_ptr<T>;
```

A helper may be created as follows:

```cpp
template<typename T, typename... Args>
SharedPtr<T> MakeSharedPtr(Args&&... args)
{
    return std::make_shared<T>(std::forward<Args>(args)...);
}
```


Specifications:

- The Engine smart pointer alias uses only `SharedPtr<T>`
- `SharedPtr<T>` corresponds to `std::shared_ptr<T>`
- `MakeSharedPtr<T>()` is permitted to have only a single definition
- Non-owning references use `T*` or `T&`
- Engine Resource / GPU Resource still prioritize Handles; the resource management model must not change because of SharedPtr
### 4.5 Ownership Rules

```text
SharedPtr<T>
→ Shared Ownership
→ Used only by C++ objects that genuinely require Shared Lifetime

T*
→ Nullable Non-owning Reference
→ Does not handle release

T&
→ Non-null Non-owning Reference
→ Does not handle release

Handle<T>
→ Engine-managed Resource Reference
→ Actual lifetime managed by ResourceManager / corresponding Subsystem

EntityID
→ Runtime Entity Identity
→ Does not represent Memory Ownership
```

Core principles:

- Do not treat `SharedPtr<T>` as a “single ownership” tool
- Do not regard the Smart Pointer type itself as the sole expression of architectural Ownership
- Ownership is determined by the responsibility boundaries of `Subsystem / Manager / Pool / Resource Registry`
- Use `SharedPtr<T>` only for C++ objects that genuinely require shared lifetimes across objects / systems
- Always treat `T*` / `T&` as non-owning references
- Prefer Handle for GPU Resource / Mesh / Texture / Material; do not use SharedPtr to manage GPU resource lifetimes
- Prefer EntityID / NodeHandle for Entity / Node Runtime References; do not use SharedPtr
- Avoid frequent SharedPtr copies in general Runtime Hot Paths to reduce reference-count operations

### 4.6 Handle First

GPU / Asset / Runtime Resource use generation-based Handle:

```cpp
template<typename Tag>
struct Handle
{
    uint32_t index = kInvalidIndex;
    uint32_t generation = 0;
};
```

For example:

```cpp
TextureHandle texture;
MeshHandle mesh;
MaterialHandle material;
```

Gameplay / Material must not directly hold native resource pointers for extended periods.

### 4.7 Error Handling

Engine Core does not use Exceptions as general control flow.

Expected recoverable errors:

```cpp
Result<TextureHandle> LoadTexture(const AssetID& asset);
```

Usage:

```cpp
auto result = LoadTexture(asset);

if (!result)
{
    LOG_ERROR("Failed to load texture: {}", result.Error());
    return result.Error();
}
```

Unrecoverable errors:

```cpp
ENGINE_ASSERT(device != nullptr);
ENGINE_FATAL("Failed to create graphics device");
```

Rules:
- Expected Failure → Result / ErrorCode
- Programming Error → Assert
- Fatal Initialization Failure → Fatal Path
- Platform Error → Convert to Engine Error
- Prohibit arbitrary mixing of Exception / bool / magic integer error code across different modules

### 4.8 const / API Semantics

When the input is not modified:

```cpp
void CreateTexture(const TextureDesc& desc);
```

Accept a non-const reference only when it is actually modified.

Getters may be provided as needed:

```cpp
const TransformData& GetTransform() const;
TransformData& GetTransform();
```

### 4.9 Data-Oriented Component Style

Avoid large inheritance hierarchies:

```text
Component
└─ RenderComponent
   └─ MeshComponent
      └─ AnimatedMeshComponent
```

Prefer:

```text
Data
+
System
```

For example:

```cpp
struct MeshRendererData
{
    MeshHandle mesh;
    MaterialHandle material;
    uint32_t flags = 0;
};

class MeshRendererSystem
{
public:
    void Update(const FrameContext& frame);
};
```

### 4.10 Hot Loop Rules

Prohibit the use of virtual dispatch + pointer chasing in large-scale Component updates:

```cpp
for (Component* component : components)
{
    component->Update(dt);
}
```

Prefer directly running SoA / Chunk:

```cpp
auto positions = transformPool.Positions();
auto velocities = velocityPool.Values();

for (size_t i = 0; i < positions.size(); ++i)
{
    positions[i] += velocities[i] * dt;
}
```

Hot Loop principles:
- Avoid SharedPtr copies
- Avoid Heap Allocation
- Avoid RTTI lookup
- Avoid Virtual Dispatch
- Avoid Hash Lookup（when it can be resolved in advance）
- Prefer contiguous memory
- Prefer batch processing

### 4.11 Virtual Function Usage Scope

Allowed:
- RHI Backend Interface
- Editor Plugin Interface
- A small number of high-level subsystem abstractions

Avoid:
- Transform hot loop
- Animation hot loop
- Visibility / Culling hot loop
- Render submission hot loop

### 4.12 Enum / Strong Type

Always prefer `enum class` for enums:

```cpp
enum class TextureFormat : uint8_t
{
    RGBA8,
    BC7,
    ASTC
};
```

Do not use the same raw `uint32_t` for different resource IDs:

```cpp
TextureHandle texture;
MeshHandle mesh;
MaterialHandle material;
```

Avoid accidentally passing a Mesh Index to a Texture API.

### 4.13 Magic Number

Prohibited:

```cpp
if (distance > 500.0f)
```

Prefer:

```cpp
constexpr float kDefaultShadowDistance = 500.0f;
```

Or use Settings / Config.

### 4.14 RAII

Prefer RAII for OS / File / Lock / Temporary Native Resource.

```cpp
class File
{
public:
    explicit File(const char* path);
    ~File();

    File(const File&) = delete;
    File& operator=(const File&) = delete;
};
```

Do not depend on the caller remembering to manually Close / Release.

### 4.15 Move-only Resource

Objects with unique ownership or expensive copy costs shall be non-copyable:

```cpp
class CommandList
{
public:
    CommandList(const CommandList&) = delete;
    CommandList& operator=(const CommandList&) = delete;

    CommandList(CommandList&&) noexcept = default;
    CommandList& operator=(CommandList&&) noexcept = default;
};
```

### 4.16 Allocation Rules

Unrestricted per-frame heap allocation is prohibited in Hot Paths.

Prefer:
- reserve
- Frame Allocator
- Linear / Arena Allocator
- Object Pool
- Chunk Storage
- Fixed-capacity container（when appropriate）

Render Submission may use a Frame Arena:

```cpp
FrameVector<RenderItem> renderItems(frameAllocator);
```

### Frame Allocator Threading

Multiple Worker Threads must not share a single global Frame Arena without synchronization.

Default strategy:

```text
FrameAllocatorSet
├─ MainThread Arena
├─ RenderThread Arena
└─ Worker Arena[N]
```

Each Worker uses its own Thread-local / Worker-local Arena:

```cpp
FrameAllocator& allocator = FrameAllocators::CurrentWorker();
FrameVector<RenderItem> renderItems(allocator);
```

If a Job generates a large amount of temporary data, a Job-local Arena / Scratch Allocator may also be configured.

Principles:

- Do not use a global allocator lock in the Allocation Hot Path
- A Worker Arena must not be written by multiple Workers simultaneously
- The Scratch range may be discarded after the Job ends
- Reset all Arenas at Frame End after all Jobs / Render work using that Frame memory have completed
- A clear frame fence / job barrier must exist before reset
- Frame Arena objects must not escape to the next Frame
- Do not store a Frame Arena pointer in a Persistent Resource / Scene Object

Frame lifecycle:

```text
Begin Frame
↓
Acquire Per-thread Arenas
↓
Parallel Jobs Allocate Locally
↓
Join / Barrier
↓
Render Submission Complete
↓
Frame Fence / Lifetime Complete
↓
Reset Arenas
```

When results must be merged across Workers:

```text
Per-worker Output
↓
Prefix / Merge / Compaction
↓
Final Contiguous Buffer
```

Avoid concentrating all Worker allocations into a single locked allocator.

### 4.17 Logging

Standardize:

```cpp
LOG_TRACE(...)
LOG_INFO(...)
LOG_WARNING(...)
LOG_ERROR(...)
LOG_FATAL(...)
```

Prohibit scattered usage:

```cpp
printf("error");
```

Important GPU / Asset errors must include context:

```cpp
LOG_ERROR(
    "Failed to create pipeline. Shader={}, Variant={}",
    shaderName,
    variantKey);
```

### 4.18 Assert

Debug Assertion:

```cpp
ENGINE_ASSERT(index < mEntities.size());
```

With a message:

```cpp
ENGINE_ASSERT_MSG(
    state == ResourceState::ShaderResource,
    "Texture must be ShaderResource before sampling");
```

### 4.19 RHI Boundary Rules

The high-level Renderer must not contain:

```text
VkImage
VkBuffer
VkDescriptorSet
ID3D12Resource
D3D12_GPU_DESCRIPTOR_HANDLE
MTLTexture
MTLBuffer
```

The high level may use only:

```text
TextureHandle
BufferHandle
GraphicsPipelineHandle
CommandBuffer
ResourceState
ResourceIndex
```

Backend-specific types are allowed only in:

```text
RHI/D3D12/
RHI/Vulkan/
RHI/Metal/
```

### 4.20 Resource State

The high level must not write directly:

```cpp
VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
```

It must use:

```cpp
ResourceState::ShaderResource
```

The mapping is handled by each Backend.

### 4.21 Threading Convention

Every core API must be explicitly marked as:
- Thread-safe
- Main-thread-only
- Render-thread-only
- Worker-thread-safe

For example:

```cpp
// Main thread only.
void Scene::DestroyNode(NodeHandle node);

// Thread-safe.
AssetRequest AssetManager::LoadAsync(const AssetID& asset);

// Render thread only.
void Renderer::Submit(const RenderWorld& world);
```

Annotations may be introduced later:

```cpp
ENGINE_MAIN_THREAD
void DestroyNode(NodeHandle node);
```

### 4.22 Comment Style

Comments explain “why,” rather than restating the code.

Not recommended:

```cpp
// Increase i.
++i;
```

Recommended:

```cpp
// Increment the generation so stale handles fail validation
// when this slot is reused.
++mGenerations[index];
```

### 4.23 TODO Rules

Prohibited:

```cpp
// TODO fix later
```

Standardize as:

```cpp
// TODO(Renderer): Replace CPU culling with GPU Hi-Z path.
// Tracking: ENG-142
```

This facilitates searching by AI Agent, CI, and Issue Tracker.

### 4.24 Formatting

Controlled uniformly by `.clang-format`:
- Indentation
- Braces
- Line wrapping
- Include ordering
- Pointer / Reference spacing

Humans and AI must not independently use another format.

### 4.25 Static Analysis / CI Gate

Each PR must run at least:

```text
clang-format --check
↓
clang-tidy
↓
Compiler Warning Check
↓
Architecture / Include Rule Check
↓
Unit Test
↓
Sanitizer（on applicable platforms / modules）
```

Recommended tools:
- clang-format
- clang-tidy
- AddressSanitizer
- UndefinedBehaviorSanitizer
- ThreadSanitizer（for suitable subsystems）
- Vulkan Validation Layer
- DX12 Debug Layer / DRED

CI should also check prohibited patterns, such as:
- High-level Renderer includes Vulkan / DX12 / Metal native headers
- Gameplay uses native GPU resources
- A new SharedPtr is added to a Hot Loop
- An impermissible exception flow appears in Core Runtime
- Resource ownership is unclear

### 4.26 AI Coding Rule

Before generating or modifying code, the AI Agent must read:

```text
ARCHITECTURE.md
CODING_CONVENTION.md
RHI_CONTRACT.md
MEMORY_MODEL.md
THREADING_MODEL.md
ERROR_HANDLING.md
```

AI must not independently:
- Invent a new Smart Pointer alias
- Change the definition of SharedPtr
- Change the Error Handling Model
- Change a Handle into a SharedPtr
- Leak native graphics types at the high level of the RHI
- Establish different naming rules for a single module

If the Convention genuinely needs to change:
1. Create an ADR
2. Update the Coding Convention
3. Update the clang-tidy / CI Rule（if applicable）
4. Then modify Production Code

### 4.27 Coding Style Core Summary

```text
Ownership
→ Standardize SharedPtr usage
→ Strictly restrict SharedPtr
→ Handle manages Engine Resource

Runtime
→ Node API + EntityID
→ Component Pool / SoA
→ Do not repeatedly perform lookups in Hot Loops

Renderer
→ ResourceHandle / ResourceIndex
→ Do not expose Backend Native Types

Error
→ Result / ErrorCode
→ Use Assert / Fatal for programming errors

Memory
→ RAII
→ Frame / Arena / Pool
→ Avoid per-frame heap allocation

AI Development
→ clang-format
→ clang-tidy
→ Architecture Gate
→ Unit / Integration / Platform Test
```



### Smart Pointer Policy

Smart pointers uniformly continue to use the `SharedPtr<T>` defined in 4.4; the alias is not redeclared here.

Usage:

```cpp
SharedPtr<GraphicsDevice> graphicsDevice;
SharedPtr<EditorDocument> document;
SharedPtr<AsyncRequest> request;
```

Creation:

```cpp
auto device = std::make_shared<GraphicsDevice>();
```

Rules:
- Smart pointers use only `SharedPtr<T>`
- Do not additionally define `SharedPtr<T>`
- Use `T*` or `T&` for non-owning references
- Continue to prefer `Handle<T>` for Engine Resources
- Use `EntityID` for Entities
- RHI / GPU Resources must not bypass the Resource Manager / Handle System because of SharedPtr
- Do not repeatedly copy SharedPtr in Hot Loops to avoid unnecessary reference count operations



### Precompiled Header (PCH) Policy

PCH is an official standard feature of the V0.1 Build System, used to reduce the repeated Header Parse cost of large C++ projects.

Recommended structure:

```text
Engine/
├─ EnginePCH.h
├─ Core/
├─ Scene/
├─ Asset/
└─ ...

Renderer/
├─ RendererPCH.h
├─ RHI/
└─ ...

Editor/
├─ EditorPCH.h
└─ ...
```

Basic rules:

```text
EnginePCH.h
→ STL
→ Stable, low-change Engine base types
→ Common platform-independent Headers

RendererPCH.h
→ Common stable Headers for the Renderer
→ Math / Handle / Resource Description
→ Do not directly include all Backend Native Headers

EditorPCH.h
→ Dear ImGui
→ Common STL for the Editor
→ Editor Framework Common Header
```

May be placed in the PCH:
- `<cstdint>`
- `<cstdlib>`
- `<memory>`
- `<string>`
- `<string_view>`
- `<vector>`
- `<array>`
- `<span>`
- `<unordered_map>`
- `<functional>`
- `<algorithm>`
- Other high-frequency, low-change standard Headers
- Stable and widely used shared Engine Headers

Not recommended for the shared PCH:
- Frequently modified Gameplay Headers
- Frequently modified Component Headers
- Large Third-party Headers used by only a few modules
- Vulkan Native Headers
- Direct3D 12 Native Headers
- Metal Backend-specific Headers
- Single-platform-specific Headers

Backend Native Headers should be restricted to the corresponding Backend:

```text
RHI/D3D12/
→ d3d12.h
→ dxgi*.h

RHI/Vulkan/
→ vulkan.h

RHI/Metal/
→ Metal / Objective-C++ Headers
```

Purpose:
- Avoid invalidating the entire Engine PCH when a Backend Header is modified
- Avoid Platform Native Types leaking into shared modules
- Control the rebuild scope

CMake:

```cmake
target_precompile_headers(EngineRuntime PRIVATE
    EnginePCH.h
)

target_precompile_headers(Renderer PRIVATE
    RendererPCH.h
)

target_precompile_headers(Editor PRIVATE
    EditorPCH.h
)
```

PCH principles:
- PCH is a compilation performance tool, not a means of solving dependency design problems
- The existence of PCH does not permit arbitrary Header-to-Header includes
- Continue to prefer Forward Declaration
- Maintain Module Boundaries
- Reduce Public Header Dependencies
- PCH contents must change infrequently

CI:
- Validate PCH Builds in both Debug / Release
- Provide an optional `ENGINE_USE_PCH=OFF` Build
- Periodically run a Non-PCH Build to prevent source code from secretly depending on Headers provided by the PCH
- The Non-PCH Build must succeed to confirm the include completeness of every Translation Unit

The following may be evaluated later:
- Unity / Jumbo Build
- C++20 Modules

However, they are not mandatory in V0.1, to avoid increasing Build System complexity simultaneously with PCH / Incremental Build.

## Five. Engine Core

Directory concept:

```text
Engine/
├─ Core/
│  ├─ Application
│  ├─ Memory
│  ├─ Threading
│  ├─ JobSystem
│  ├─ FileSystem
│  ├─ Logging
│  ├─ UUID
│  ├─ Reflection
│  ├─ Serialization
│  ├─ Time
│  ├─ Testing
│  └─ Crash
├─ Scene/
├─ Renderer/
├─ RHI/
├─ Asset/
├─ Animation/
├─ Physics/
├─ Audio/
├─ UI/
├─ Localization/
├─ Save/
├─ Platform/
└─ Editor/
```

Core requirements:
- Runtime must not depend on Editor
- Debug / Release / Shipping
- Handle-based resource management
- Asynchronous asset loading
- Thread-safe Job System
- UUID Asset Reference
- Reflection support for Inspector and Serialization
- Crash Log / Dump infrastructure
- Memory Budget tracking


## Six. Scene / World Lifecycle / Node Architecture

This engine formally distinguishes:

```text
Engine
↓
World
↓
Scene
↓
Entity / Node
↓
Component
```

Core Contract:

```text
World
≠ Scene
```

```text
World
→ Runtime Simulation Context

Scene
→ Loadable / Serializable Content Ownership Unit
```

A Scene is not directly equivalent to a Physics World, Navigation World, Render World, or Streaming Cell.

### World

`World` is an independently simulatable Runtime Universe.

```text
World
├─ Scene Registry
├─ Entity Registry
├─ System Scheduler
├─ PhysicsWorld
├─ NavigationWorld
├─ Render Extraction Context
├─ Audio Context
├─ Event Queue
└─ Time / Simulation State
```

World Type:

```text
WorldType
├─ Editor
├─ Play
├─ Preview
├─ Bake
└─ Server      ← Future
```

Formal rule:

```text
EditorWorld
≠
PlayWorld
```

Play Mode:

```text
EditorWorld
↓
Clone / Build Runtime World
↓
PlayWorld
↓
Simulation
```

Stop:

```text
PlayWorld Destroy
↓
EditorWorld retains the original editing state
```

Avoid Runtime-generated Entities, Physics State, Animation State, and Gameplay State contaminating the Editor Authoring World.

### World Lifecycle

```text
WorldState

Creating
↓
Ready
↓
Running
↕
Paused
↓
Stopping
↓
Destroying
↓
Destroyed
```

World Destroy must have deterministic shutdown:

```text
Stop accepting new gameplay jobs
↓
Drain / Cancel outstanding work
↓
Deactivate scenes
↓
Destroy scenes
↓
Destroy Physics / Navigation / Runtime subsystem state
↓
Destroy World
```

`Pause` differs from `Scene Deactivate`:

```text
World Pause
→ Simulation Time stops / changes policy

Scene Deactivate
→ Scene content leaves active simulation participation
```

### Multiple Scene / Additive Scene

A single World may load and activate multiple Scenes simultaneously.

```text
World
├─ Persistent.scene
├─ Town.scene
├─ Forest.scene
├─ Mountain.scene
└─ Dungeon.scene
```

The states may simultaneously be:

```text
Persistent.scene  → Active
Town.scene        → Active
Forest.scene      → Active
Mountain.scene    → LoadedInactive / Prefetched
Dungeon.scene     → Unloaded
```

Formal Contract:

```text
World
→ May contain N loaded Scenes

N Scenes
→ May be Active simultaneously
```

Supported:

```text
SceneLoadMode
├─ Single
└─ Additive
```

`Single` is not an independent Runtime architecture; it may be regarded as:

```text
Load New Scene
↓
Activate
↓
Unload previous non-persistent scenes
```

The underlying implementation is based on Additive Scene capability.

### Persistent Scene

Do not adopt implicit rules of the `DontDestroyOnLoad` type that secretly move arbitrary Objects.

Formal structure:

```text
World
├─ PersistentScene
└─ Gameplay Scenes
```

For example:

```text
PersistentScene
├─ Player
├─ Camera
├─ GlobalAudio
├─ GlobalUI
└─ Game-level persistent entities
```

When crossing regions:

```text
Gameplay Scene A
→ unload

PersistentScene
→ remains

Gameplay Scene B
→ load
```

If an Entity needs to change ownership, use an explicit:

```text
MoveEntityToScene()
```

Rather than a special lifetime flag.

### Scene State Machine

```text
SceneState

Unloaded
↓
Loading
↓
LoadedInactive
↓
Activating
↓
Active
↓
Deactivating
↓
LoadedInactive
↓
Unloading
↓
Unloaded
```

Error:

```text
Loading / Activating
↓
Failed
```

`LoadedInactive` means:

- The Scene Asset has been read.
- Entity / Component Runtime Data has been created.
- References have been resolved within the currently resolvable scope.
- Required Physics / Navigation / Render registration is ready.
- Gameplay has not yet begun updating.

### Async Scene Load

Scene Load formally follows an asynchronous pipeline:

```text
Scene Asset
↓
Async IO
↓
Deserialize / Runtime Blob Read
↓
Resolve Asset Dependencies
↓
Create Entity Storage
↓
Create Components
↓
Resolve References
↓
Prepare Physics / Navigation / Render / Audio
↓
LoadedInactive
↓
Activation Barrier
↓
Active
```

The following is prohibited:

```text
LoadScene()
↓
Main Thread blocks synchronously for several seconds
↓
Partially completed Scene is directly exposed to Gameplay
```

### Atomic Scene Activation

The visibility of a Scene to the Simulation must be atomic.

Prohibited:

```text
Camera has appeared
Collider has not yet been created
AI has updated
Navigation is not yet Ready
```

Formal process:

```text
Build
↓
Validate
↓
Ready
↓
Safe Frame / Simulation Barrier
↓
Atomic Activate
```

Only after Activation may Gameplay / Physics / Animation / AI / Audio / Render Extraction formally see the Scene.

### Safe Scene Unload

Scene Unload:

```text
Active
↓
Deactivating
↓
Stop Gameplay Participation
↓
Unregister Physics / Navigation / Render / Audio
↓
Cancel / Drain Scene-owned Jobs
↓
Release Asset References
↓
Destroy Entity Runtime Data
↓
Unloaded
```

A Scene must not be directly freed while Jobs / Render Frames / Physics Steps / Async callbacks still hold its transient data.
### Scene Transition Transaction

Scene Transition does not hard-code Fade or Loading UI.

Scene Core provides:

```text
Prepare
Activate
Deactivate
Commit
Rollback
```

For example, A → B:

```text
Scene A Active
↓
Begin Transition
↓
Async Load B
↓
Build B Inactive
↓
Validate B
↓
Presentation Fade / Transition
↓
Activation Barrier
↓
Activate B
↓
Switch Camera / Input policy
↓
Deactivate A
↓
Presentation Fade In
↓
Unload A when safe
```

If B fails:

```text
Load / Validation Failed
↓
Rollback
↓
Keep Scene A
```

Presentation transition is controlled by Gameplay / Runtime UI and may use Fade, Loading Screen, Cloud Transition, etc.

### Scene Load Progress

Provide real stage progress rather than a single fake percentage:

```text
SceneLoadProgress
├─ IO
├─ Dependency
├─ Deserialize
├─ Instantiate
├─ Prepare
└─ Finalize
```

Loading UI may display an aggregated view.

### Scene / Entity Active State

Formally distinguish:

```text
SceneActive
EntityActiveSelf
EntityActiveInHierarchy
ComponentEnabled
```

Effective active:

```text
EffectiveActive
=
SceneActive
&& EntityActiveInHierarchy
&& ComponentEnabled
```

Parent Disable should not directly synchronize large numbers of Entities through deep recursive callbacks.

Recommended:

```text
Hierarchy Dirty
↓
Activation Propagation Job
↓
Effective State Change List
↓
System Lifecycle Dispatch
```

### Component Lifecycle

May provide:

```text
OnCreate
OnEnable
OnDisable
OnDestroy
```

But this must not introduce:

```text
virtual Update() per component per frame
```

Formally:

```text
Lifecycle Hook
✓

Per-component virtual Update
✕
```

System / Component Registry is responsible for batched lifecycle dispatch.

### Structural Change / WorldCommandBuffer

While a Simulation Job is reading or writing SoA, arbitrary direct changes to the storage structure are not permitted.

Formally:

```text
WorldCommandBuffer
├─ CreateEntity
├─ DestroyEntity
├─ AddComponent
├─ RemoveComponent
├─ Reparent
├─ SetActive
└─ MoveEntityToScene
```

Process:

```text
Gameplay / Jobs
↓
WorldCommandBuffer
↓
Structural Barrier
↓
Apply
```

### Scene Root

A Scene owns a root entity list and does not create a fake SceneRootEntity.

```text
Scene
├─ Root Entity A
├─ Root Entity B
└─ Root Entity C
```

```text
Parent = InvalidEntityID
→ Root Entity
```

### Hierarchy Ownership Rule

Entity hierarchy may not cross Scene ownership:

```text
Scene A Entity
↓ parent
Scene B Entity
```

This is formally prohibited.

```text
Hierarchy Parent
→ Must belong to same Scene
```

Cross-Scene logical relationships use the Reference / Attachment System rather than hierarchy ownership.

### Move Entity Between Scenes

Provide:

```text
MoveEntityToScene()
```

This is an ownership transfer, not an ordinary Reparent.

The following must be handled:

- Children policy.
- Scene-local serialization ownership.
- Runtime registration.
- Persistent identity.
- Cross-scene references.
- Streaming cell reassignment.
- Editor Undo / Redo。

V1 may restrict Runtime usage scenarios; the Editor must support it.

### Persistent Identity / Runtime Identity

Persisted:

```text
UUID128
```

Runtime:

```text
EntityID
= Index + Generation
```

After Scene unload, the old EntityID becomes invalid.

When the same persistent Object is loaded again:

```text
Same UUID
→ may resolve to a different EntityID
```

Formally:

```text
Persistent UUID
≠ Runtime EntityID
```

### Cross-Scene Reference

Serialized cross-scene references do not store raw EntityID.

Formally:

```text
SceneObjectRef
├─ SceneAssetUUID / SceneInstance identity
└─ ObjectUUID
```

Runtime:

```text
SceneObjectRef
↓
SceneReferenceResolver
↓
EntityID / SceneObjectHandle
```

When the target Scene is not loaded, the reference may remain unresolved.

Scene unload:

```text
Resolved EntityID
→ invalidated
```

The logical reference still exists.

Reload:

```text
UUID
↓
Resolve new EntityID
```

A cross-scene logical reference does not automatically equate to a hard load dependency.

```text
Build / Load Dependency
≠
Logical Object Reference
```

This avoids the reference graph inadvertently pulling the entire world into residency.

### Scene Reference Policy

May support:

```text
SceneReferencePolicy
├─ Required
├─ Optional
├─ Weak
└─ LoadOnDemand      ← Restricted / Future-sensitive
```

`Required` may block Scene activation.

`Optional / Weak` allow unresolved references when the target is not loaded.

`LoadOnDemand` is not the default behavior for arbitrary cross-references, avoiding implicit streaming dependencies.

### Scene Serialization

Authoring Scene Asset:

```text
Scene Asset
├─ Scene Metadata
├─ Entity UUID
├─ Hierarchy
├─ Component Types
├─ Component Serialized Data
└─ Asset / Object References
```

Runtime:

```text
Scene Authoring Data
↓
Validate
↓
Cook
↓
Runtime Scene Blob
↓
Fast Instantiate
```

Shipping Runtime should prioritize reading the cooked format and not depend on heavyweight Editor reflection JSON paths.

### Scene Schema Version / Migration

Formally:

```text
SceneSchemaVersion
```

Process:

```text
Old Scene
↓
Deserializer
↓
Migration Chain
↓
Current Schema
```

After the Editor saves, it updates to the current schema.

Shipping Runtime should preferably accept only the cooked current runtime format.

When a plugin or component is missing, the Editor must not directly discard the data and may retain:

```text
MissingComponentProxy
├─ Original TypeID
└─ Serialized Payload
```

After the plugin is restored, it may be resolved again.

---

### Seamless World / Multi-Scene Streaming

A large seamless world operates within the same `World` using multiple Scenes + Streaming Cells.

```text
World
├─ Persistent Scene
├─ Town Scene
├─ Forest Scene
└─ Mountain Scene
```

As the player moves:

```text
Town Active
+
Forest Prefetch
↓
Forest LoadedInactive
↓
Forest Activate
↓
Town + Forest overlap Active
↓
Player continues
↓
Town retires / unloads when safe
```

Formally:

```text
Scene Boundary
≠
Rendering Boundary
≠
Physics Boundary
≠
Navigation Boundary
≠
Gameplay Zone Boundary
```

World-level subsystem:

```text
World
├─ PhysicsWorld
├─ NavigationWorld
├─ RenderWorld
├─ Audio Context
├─ Entity Registry
└─ System Scheduler
```

Scene load / unload only Register / Unregister content with these World subsystems.

Do not create an independent PhysicsWorld / NavigationWorld for each Additive Scene.

### Logical Scene != Streaming Cell

This is the core Contract of a Large World:

```text
Scene
→ Authoring / Ownership / Serialization Unit

StreamingCell
→ Runtime Residency Unit
```

A single Scene may be cooked into many Streaming Cells.

```text
KyotoTown.scene
├─ Cell 0,0
├─ Cell 0,1
├─ Cell 0,2
└─ ...
```

The Editor may maintain a single logical Scene; Runtime physical partitioning should not force the Editor hierarchy to be divided into numerous Scene Assets.

### Streaming Cell

Formally:

```text
StreamingCell
= Runtime Residency Unit
```

A Cell is not necessarily a fixed grid square.

It may originate from:

```text
StreamingCellType
├─ SpatialGrid
├─ Volume
├─ Room
└─ Explicit
```

Runtime common metadata:

```text
CellID
Bounds
Dependencies
Priority
ResidencyState
ContentReferences
Neighbors
```

### Streaming Cell State

Scene State and Cell Residency State are separate.

```text
StreamingCellState

Unloaded
↓
Requested
↓
IOResident
↓
BuiltInactive
↓
Active
↓
Retiring
↓
Unloaded
```

Compound residency may also be distinguished:

```text
CellResidency
├─ Metadata
├─ CPU Content
├─ Physics
├─ Navigation
├─ GPU Resources
└─ Simulation
```

Formally:

```text
Simulation Residency
≠
Visual Residency
```

### World Partition Policy

World Partition is not tied to a single Grid.

```text
WorldPartitionPolicy
├─ Grid
├─ Loose Quadtree
├─ Volume
├─ Room / Portal
├─ Explicit Region
└─ Custom
```

Recommended:

```text
Outdoor
→ Fixed Grid Cells + Loose Quadtree Spatial Index

Indoor
→ Room / Portal Graph

Special Area
→ Explicit / Volume Cell

Outdoor ↔ Indoor
→ Streaming Gateway
```

A single World may use a hybrid approach.

### Fixed Grid Cell

Outdoor V1 uses a stable fixed grid as the primary residency/cook unit:

```text
CellX = floor(WorldX / CellSize)
CellZ = floor(WorldZ / CellSize)
```

Advantages:

- Deterministic.
- Direct coordinate lookup.
- Cache-friendly.
- Easy Bundle patching / streaming debugging.
- Runtime does not need to traverse all Cells in the world.

### Loose Quadtree

V1 formally supports Loose Quadtree as hierarchical spatial acceleration.

Positioning:

```text
Loose Quadtree
= Spatial Acceleration
+ Streaming Candidate Index
+ HLOD Hierarchy Foundation
```

It is not:

```text
Quadtree Node
= Streaming Cell
```

Formally:

```text
StreamingCell
→ Stable Residency Unit

QuadtreeNode
→ Runtime / Cook Spatial Index
```

Quadtree uses:

- Radius / Box / Frustum cell queries.
- Streaming Source candidate queries.
- Camera visibility candidate queries.
- Large object spatial registration.
- HLOD hierarchy.
- Editor region queries.

When a large Object spans grid cells, Loose Bounds are used to reduce duplicate registration.

### Adaptive Quadtree

V1 does not require an adaptive quadtree to directly determine variable-size Cell identity.

```text
Quadtree Spatial Index
→ V1

Adaptive Quadtree Cell Generation
→ Future / V2
```

This avoids having V1 simultaneously handle:

- Variable cell patch identity.
- HLOD stitching.
- Terrain / Nav / Physics ownership complexity.
- Neighbor resolution.
- Deterministic cell ID churn.

### Room / Portal Graph

Room / Portal is formally supported for Indoor / Dungeon environments.

```text
Room A
   │
 Portal
   │
Room B
   │
 Portal
   │
Room C
```

Room:

```text
Room
├─ Bounds / Volume
├─ RoomID
├─ Portals[]
└─ Environment / Metadata
```

Portal:

```text
Portal
├─ RoomA
├─ RoomB
├─ Plane
├─ Convex Polygon
├─ State
└─ Traversal Flags
```

Formally:

```text
Room
= Spatial / Connectivity Concept

StreamingCell
= Residency Concept
```

Possible configurations:

```text
1 Room → 1 Cell
N Small Rooms → 1 Cell
1 Huge Room → N Cells
```

### Portal Role

A Portal is a connectivity edge, not a Gameplay Door.

```text
Portal
≠ Gameplay Door
```

A Gameplay Door may control Portal state:

```text
Door Open
↓
Portal SetOpen(true)
```

However, a Portal may also represent:

- Door opening.
- Corridor opening.
- Cave opening.
- Stair opening.
- Elevator exit.
- Window.
- Area transition opening.

A Portal may provide subsystem-specific traversal information:

```text
Portal Capability
├─ VisibleThrough
├─ StreamThrough
├─ AudioThrough
├─ NavThrough
└─ AIThrough
```

Different subsystems do not have to use exactly the same rules.

### Portal Visibility

A Portal Culling foundation may be established:

```text
Camera
↓
Find Current Room
↓
Traverse Visible Portals
↓
Clip Frustum against Portal Polygon
↓
Visible Room Set
↓
Render Extraction
```

Full aggressive Portal Frustum optimization may be strengthened later through profile-driven optimization, but the Room/Portal connectivity foundation is included in V1.

### Portal Streaming

Indoor Streaming does not use only linear distance.

```text
Current Room
↓
Portal Graph Distance
↓
Streaming Demand
```

For example:

```text
Graph Distance 0
→ Active

Distance 1
→ Active / High Priority

Distance 2
→ Prefetch

Farther
→ Unloaded / Low
```

This avoids loading every room that is only a few meters away behind a wall but is actually far away in terms of path distance due to a pure radius query.

Portal state may affect streaming:

```text
Closed / Locked
→ lower / block traversal demand according to policy

Opening
→ prefetch room behind portal
```

### Outdoor ↔ Indoor Streaming Gateway

Support:

```text
Outdoor Grid / Quadtree
↓
Streaming Gateway
↓
Indoor Room / Portal Graph
```

Near an entrance:

```text
Gateway Proximity
↓
Prefetch Interior Entry Cell
```

After entering indoors:

```text
Portal Graph
→ controls deeper interior residency
```

Outdoor preload may maintain hysteresis, eliminating the need for a loading screen when leaving.

### Streaming Source

Formally:

```text
StreamingSource
```

Sources may include:

```text
Player
Camera
Party Member
Vehicle
Teleport Destination
Cutscene Camera
Gameplay Request
Editor Pin
```

Each source may have:

```text
Position
Radius / Shape
Priority
Velocity
LookDirection
PrefetchDistance
DesiredResidency
```

High-speed movement may use forward-biased / swept volume prefetch.

### Streaming Demand

Partition / Portal does not directly call Bundle load.

They uniformly generate:

```text
StreamingDemand
├─ Source
├─ CellID
├─ Priority
├─ Reason
└─ DesiredState
```

Streaming Manager:

```text
Multiple Sources
↓
Merge Demands
↓
Priority / Budget
↓
Actual Residency
```

Reason may be used by the profiler:

```text
Why is this cell resident?
```

### Streaming Priority / Hysteresis

At minimum, support:

```text
StreamingPriority
├─ Critical
├─ High
├─ Normal
├─ Low
└─ Speculative
```

Avoid boundary thrashing:

```text
Load / Activate Distance
<
Unload Distance
```

Also:

```text
MinimumResidentTime
```

Speculative requests that have not completed should be cancellable.

### Streaming Cell != Bundle

Formally:

```text
StreamingCell
→ Runtime Residency Unit

Bundle
→ Physical Packaging / Download / Versioning Unit
```

A 1:1 relationship is not mandatory.

One Cell may reference multiple Bundles; one Bundle may also contain content for multiple related Cells.

The Cooker may optimize based on locality, but the semantics are not tightly coupled.

### Streaming Group / KeepTogether

Auto partitioning must allow author overrides.

For example:

```text
HLODGroup / StreamingGroup
BossArena
Castle
Large Landmark
```

May be marked:

```text
KeepTogether = true
```

This prevents large objects or gameplay-critical clusters from being split into incomplete residency by auto partitioning.

---

### HLOD

HLOD is formally included in the Large World / World Partition Framework.

```text
LOD
→ Single Object

HLOD
→ Multiple Objects / Spatial Region
```

HLOD is not equivalent to Mesh LOD.

For example:

```text
House A
House B
Trees
Fence
Props
↓
HLOD Builder
↓
Village_Block_01_Proxy
```

### HLOD Generation

HLOD is generated offline by the Editor / Cooker.

```text
Authoring Scene
↓
World Partition / Spatial Hierarchy
↓
HLOD Builder
├─ Spatial Clustering
├─ Mesh Merge
├─ Geometry Simplification
├─ Material Merge
├─ Texture Atlas / Bake
├─ Impostor Build
└─ Custom Proxy
↓
HLOD Assets
↓
Asset / Bundle System
```

Runtime does not perform heavy mesh simplification / texture baking during normal gameplay.

Formally:

```text
HLOD Generation
→ Editor / Cooker Offline

HLOD Runtime
→ Selection + Streaming only
```

### HLOD Hierarchy

Loose Quadtree may serve as the HLOD hierarchy foundation.

```text
Root
└─ Far HLOD
   ├─ Region HLOD
   │  ├─ Leaf HLOD
   │  └─ Leaf HLOD
   └─ Region HLOD
      └─ ...
```

At Runtime, select the appropriate level based on:

- Projected screen size.
- Distance.
- Quality profile.
- Performance policy.
- Memory residency.

### HLOD / Cell Residency

HLOD Node and Streaming Cell are separate:

```text
StreamingCell
→ Full-detail runtime content

HLODNode
→ Far visual representation
```

For example:

```text
Cell 1 ┐
Cell 2 ├─ HLOD Node A
Cell 3 ┤
Cell 4 ┘
```

At a distance:

```text
Simulation      ✕
Physics         ✕
Navigation      ✕
Full Geometry   ✕
HLOD Proxy      ✓
```

When approaching:

```text
Ensure Full Cells Ready
↓
Crossfade / Switch
↓
Hide HLOD Proxy
```

When leaving:

```text
Ensure HLOD Ready
↓
Switch
↓
Retire Full Cells
```

Formally:

```text
Full Simulation Residency
≠
Far Visual Residency
```

### HLOD Participation

Objects may be configured as:

```text
HLODParticipation
├─ Auto
├─ Include
├─ Exclude
└─ CustomProxy
```

Typically:

```text
Static Building
→ Auto

Large Landmark
→ CustomProxy / Group Override

Player / NPC / Dynamic Vehicle / Animated Boss
→ Exclude
```

Vegetation may use a dedicated approach:

```text
Near
→ Full Mesh / GPU Instancing

Mid
→ Lower LOD Instancing

Far
→ Cluster Impostor / HLOD
```

The hierarchy is shared, but the proxy generation strategy may differ.

### HLOD Determinism / Incremental Build

The same inputs:

```text
Source Assets
+
Partition Settings
+
HLOD Settings
+
HLODBuilderVersion
```

must produce deterministic output / build hash.

Incremental rebuild:

```text
Changed Object
↓
Affected Cell / HLOD Leaf
↓
Affected Parent HLOD Chain
↓
Rebuild only impacted nodes
```

Avoid rebuilding the entire World HLOD when only a local object is modified.

### HLOD Editor

The Editor must provide at least:

```text
HLOD Preview
HLOD Level Visualization
HLOD Group Override
Custom Proxy Assignment
Rebuild Selected Region
Show Source ↔ Proxy Relation
Memory / Triangle Estimate
```

Partition View should display:

```text
Loaded
Prefetched
Active
Pinned
HLOD-only
Memory
```

### World Partition / Portal Editor Tools

At minimum:

```text
Partition View
Portal Graph View
Streaming Source Preview
Cell Residency Debug
Load / Unload Region
Pin Region
Rebuild Cell
Validate Seam
Preview Streaming
```

Portal Graph View displays:

```text
Room A
  │
Portal
  │
Room B
```

And allows observation of:

```text
Open / Closed
Visibility
Streaming
Audio
Navigation / AI flags
```

### Partition / Seam Validation

The Editor / CI should check:

Portal:

```text
Valid convex portal polygon
Exactly / correctly connected rooms
Portal plane / room boundary relation
Disconnected graph
Invalid overlap
Unreachable region
```

Cell seam:

```text
Terrain edge mismatch
Nav seam missing
Physics gap
HLOD seam
Probe / lighting discontinuity
Vegetation / biome gap
```

Unified:

```text
WorldPartitionValidator
```

### Large World Coordinate Foundation

Large seamless worlds require avoiding single-precision errors over long distances.

Formal requirements:

```text
Simulation / World Position
→ High Precision Representation

Renderer
→ Camera-relative float
```

Possible approach:

```text
WorldPosition
= High-precision region/cell coordinate
+ Local float position
```

Or CPU simulation double + renderer camera-relative float.

The final concrete representation will be determined in detail when the Transform Framework is discussed.

However, the API must reserve:

```text
WorldPosition
↔ LocalWorldPosition
↔ RenderRelativePosition
```

Gameplay must not scatter double → float casts throughout the codebase.

### Scene Streaming / Runtime State Boundary

A Scene Asset is a content template, not Save State.

Formally:

```text
Scene Asset
+
Persistent World State
↓
Runtime Scene
```

For example, states such as a chest being opened, a Boss being dead, or a bridge being destroyed must not automatically revert to the Scene Asset defaults because of Cell unload / reload.

Detailed Persistent World State / Save ownership is discussed in detail in the Save Framework chapter.

### Scene / World Profiler

At minimum, display:

```text
Loaded Worlds
Loaded Scenes
Active Scenes
Entities / Scene
Components / Scene
Scene Memory
Cell Residency
Streaming Sources
Streaming Demand
Cell Churn
Prefetch Hit / Miss
Emergency Stall
Asset Residency
Load / Activation / Unload Time
Cross-scene unresolved refs
HLOD Residency / Switch
Retired Scene Generations
```

It must be possible to answer:```text
Why is this Scene / Cell / HLOD resident?
```

### Scene / World V1 Scope

V1：

```text
✓ World / WorldType
✓ EditorWorld / PlayWorld separation
✓ Multiple loaded / active Scenes
✓ Additive Scene
✓ Persistent Scene
✓ Async Scene Load
✓ LoadedInactive
✓ Atomic Activation
✓ Safe Unload
✓ Scene Transition Transaction / Rollback
✓ Scene Load Progress
✓ Scene / Entity / Component Active State
✓ Component Lifecycle
✓ WorldCommandBuffer / Structural Barrier
✓ Scene UUID / Entity UUID / Runtime EntityID separation
✓ Cross-scene logical reference
✓ Same-scene hierarchy rule
✓ MoveEntityToScene
✓ Scene Serialization / Schema Migration

✓ StreamingCell
✓ Fixed Grid Cells
✓ Loose Quadtree Spatial Index
✓ Explicit / Volume Cells
✓ Room / Portal Graph
✓ Outdoor ↔ Indoor Gateway
✓ Multiple Streaming Sources
✓ Streaming Demand / Priority / Hysteresis
✓ Cell / Bundle separation
✓ Streaming Group / KeepTogether

✓ Offline HLOD Builder
✓ HLOD Hierarchy Foundation
✓ HLOD Streaming / Selection
✓ HLOD Participation / Custom Proxy
✓ Deterministic / Incremental HLOD Build
✓ Editor Partition / Portal / HLOD Debug
✓ Partition / Seam Validation

✓ Large-world coordinate foundation
```

Future / profile-driven：

```text
△ Adaptive Quadtree Cell Generation
△ Octree / 3D adaptive partition
△ Automatic Room Detection
△ Full aggressive Portal Frustum Culling
△ Runtime Scene Structural Diff / Patch
△ World Origin Rebasing
△ Distributed MMO World Partition
△ Seamless Network World Travel
```

Core Summary:

```text
World
→ Simulation Universe

Scene
→ Content Ownership / Serialization

StreamingCell
→ Runtime Residency

Loose Quadtree
→ Spatial Acceleration / HLOD Hierarchy

Room / Portal
→ Indoor Connectivity

HLOD
→ Far Visual Representation

Bundle
→ Physical Packaging
```

And:

```text
Scene
≠ StreamingCell
≠ GameplayZone
≠ PhysicsWorld
≠ NavigationWorld
≠ RenderWorld
```

---

Use Node + Component as the external interface:

```text
Scene
└─ Node
   ├─ EntityID
   ├─ Name
   ├─ Parent
   └─ Children
```

Components are not stored directly as complete OOP Objects inside Nodes.

For example:

EntityID = 125

TransformPool
[125] -> Transform Data

MeshRendererPool
[125] -> MeshRenderer Data

RigidBodyPool
[125] -> RigidBody Data

Gameplay API：

```cpp
Node* player = scene.CreateNode("Player");
```

```cpp
player->AddComponent<MeshRenderer>();
player->AddComponent<Animator>();
player->AddComponent<CharacterController>();
```

The Node API is merely a convenient outer-layer interface.

The underlying implementation actually uses:

```text
Node
↓
EntityID
↓
Component Pool
↓
SoA Data
```

Design goals:
- Retain a familiar Node / Component workflow for Editor / Gameplay
- Runtime data does not require duplicate GameObject <-> ECS synchronization
- Avoid duplicate Transform / Renderer / Physics data
- The Component Pool is directly the Source of Truth


## VII. Node / SoA Data Flow

Core principles:
- The Node itself is responsible for Identity / Hierarchy
- The Component Pool is responsible for the actual Runtime Data
- The Render World is Derived Data, not the Source of Truth

Architecture:

```text
Node API
   ↓
EntityID
   ↓
Component Registry
   ↓
┌────────────────┬────────────────┬────────────────┐
│ TransformPool  │ RenderPool     │ PhysicsPool    │
│ SoA            │ SoA            │ SoA            │
└────────────────┴────────────────┴────────────────┘
```

Transform SoA example:

```text
TransformPool
├─ EntityIDs[]
├─ Positions[]
├─ Rotations[]
├─ Scales[]
├─ WorldMatrices[]
└─ DirtyFlags[]
```

External writes are allowed:

```cpp
player->transform().SetPosition(...);
```

In actuality, this returns a TransformHandle,
which modifies the corresponding Pool Index through the Handle.

Render Extraction:

```text
Scene Runtime Data
   ↓
Visibility / Extraction
   ↓
Render World / Frame Data
   ↓
Renderer
```

The Render World is merely per-Frame cached / derived data.


## VIII. Entity ID / UUID Identity Strategy

The engine completely separates “persistent identity” from “Runtime identity.”

### Persistent Identity

Identifiers for Scene / Prefab / Asset and other items that must be preserved across execution stages use:

```text
128-bit UUID
```

Uses:
- Scene Serialization
- Prefab Reference
- Asset Reference
- Editor Reference
- Save / Persistent Reference
- Stable identification across Sessions

UUIDs are not used as the primary indices of Hot Loops.

### Runtime EntityID

Runtime uses a 64-bit packed EntityID:

```text
EntityID (uint64_t)

[ Generation : 32-bit ][ Index : 32-bit ]
```

```cpp
using EntityID = uint64_t;

inline uint32_t GetEntityIndex(EntityID id)
{
    return static_cast<uint32_t>(id);
}

inline uint32_t GetEntityGeneration(EntityID id)
{
    return static_cast<uint32_t>(id >> 32);
}

inline EntityID MakeEntityID(uint32_t index, uint32_t generation)
{
    return (static_cast<uint64_t>(generation) << 32) | index;
}
```

### Entity Creation and Reclamation

Uses:
- Generation Array
- Free List
- O(1) Create
- O(1) Destroy
- O(1) Validation

Creation flow:

```text
Create Entity
↓
Is an Index available in the Free List?
├─ Yes -> Reuse Index
└─ No  -> Expand with a new Index
↓
Read generations[index]
↓
Compose EntityID
```

Reclamation flow:

```text
Destroy Entity
↓
generation[index]++
↓
Return index to the Free List
```

Indices are reused and do not simply keep increasing until exhaustion during long-term execution.

### Stale Handle Protection

For example, the old Entity:

```text
Index      = 42
Generation = 3
```

After deletion, Index 42 is reused:

```text
Index      = 42
Generation = 4
```

The old `{42, 3}` is determined to be invalid because the generation does not match, preventing stale reference / use-after-destroy.

```cpp
bool IsAlive(EntityID id)
{
    const uint32_t index = GetEntityIndex(id);
    const uint32_t generation = GetEntityGeneration(id);

    return index < generations.size()
        && generations[index] == generation;
}
```

### EntityID and Node / Component Pool

The Node is responsible only for:
- Identity
- Name
- Parent / Child
- Editor Hierarchy

Runtime Component data is mapped by EntityID to the Component Pool:

```text
Node
↓
EntityID
↓
Component Registry
↓
Component Pool / SoA
```

EntityID does not represent the data itself; it is only a Runtime Handle.

### Do Not Repeatedly Look Up EntityID in Hot Loops

EntityID is mainly used for:
- Creation / deletion
- External references
- Component Lookup
- Editor / Gameplay API

High-frequency System updates should not repeatedly execute the following for every item:

```text
EntityID
↓
Validation
↓
Component Lookup
↓
Data
```

Instead, directly iterate over the SoA / Chunk:

```cpp
auto positions = transformPool.Positions();
auto velocities = velocityPool.Values();

for (size_t i = 0; i < positions.size(); ++i)
{
    positions[i] += velocities[i] * dt;
}
```

Final strategy:

```text
Persistent Identity
→ 128-bit UUID

Runtime Identity
→ packed uint64 EntityID
   ├─ 32-bit Generation
   └─ 32-bit Index

Hot Loop
→ Direct SoA / Chunk Iteration
```

Goals:
- O(1) creation / reclamation
- O(1) access and validation
- Stale Handle safety
- High Cache Locality
- Do not use UUID for Runtime Hot Path Lookup
- Do not let EntityID Lookup become an inner-loop cost within Systems


### Generation Overflow Policy

A 32-bit Generation is extremely ample for ordinary Scene Entities, but overflow behavior must still be defined.

Slot reuse:

```cpp
uint32_t nextGeneration = generation + 1u;

ENGINE_ASSERT_MSG(
    nextGeneration != 0u,
    "Entity generation overflow detected");

generation = nextGeneration;
```

Formal rules:

- Generation `0` is reserved as the Invalid / Never-issued state
- In Debug / Development Builds, wrap-around directly triggers Assert / Fatal
- Shipping Builds must not silently treat a wrapped generation as a normally reusable ID
- If generation exhaustion is actually encountered, the slot should be retired and must not be returned to the free-list

More importantly:

```text
High-frequency transient object
≠
Scene Entity
```

The following types do not use Scene `EntityID` by default:

- GPU Particle
- CPU Particle
- Bullet / Projectile swarm (when involving large quantities with short lifetimes)
- Decal particle
- Temporary VFX element
- Per-frame render item
- Short-lived simulation micro-object

These should use:

```text
Dedicated Pool
Batch System
SoA Storage
Generation Handle（if needed）
```

This avoids high-frequency create / destroy operations polluting the Scene Entity allocator, and avoids treating the Entity system as a universal container for all temporary objects.

## IX. Source of Truth Rules

Data                         Source of Truth
Node Hierarchy               Scene World
Transform                    Transform Pool
Mesh Renderer                Render Component Pool
Physics                      Physics Component Pool
Gameplay Component           Component Pool
Render Submission            Derived / Cached
GPU Buffer                   Derived / Cached
Inspector                    Reflection View
Serialized Scene             Persistent Representation
Prefab                       Persistent Representation
Asset Import Cache           Derived / Cached

Every module must clearly know:
- Which data is the primary data
- Which data is merely a cache
- In which direction updates are allowed
- Which data must not be synchronized bidirectionally


## X. Reflection System

V1 will not initially implement a large Clang-based code generator.

V1 uses:
- Runtime Metadata
- Template
- Macro Registration

Usage:

```cpp
class Light : public Component
{
```
public:
    float intensity = 1.0f;
    bool castShadow = true;

    REFLECT_BEGIN(Light)
        REFLECT_PROPERTY(intensity)
        REFLECT_PROPERTY(castShadow)
    REFLECT_END()
};

Metadata：

```text
TypeInfo: Light
├─ intensity
│  ├─ type = Float
│  ├─ editable = true
│  └─ attributes...
└─ castShadow
   ├─ type = Bool
   └─ attributes...
```

The Reflection Database simultaneously serves:
- Inspector
- Serialization
- Undo / Redo
- Prefab Override
- Copy / Paste
- Property Animation
- Runtime Debugging

Later versions may add:
- Clang-based Header Tool
- *.generated.cpp
- Annotation Scan
- Automatic Metadata generation

Principles:
- First ensure architectural stability in V1
- Avoid investing in a large Header Tool before the Engine is complete


## XI. Overall Renderer Architecture

Flow:

```text
Scene / Node
    ↓
```
```text
Render World
    ↓
```
```text
Visibility / Culling
    ↓
```
```text
Sorting / Batching
    ↓
```
```text
Render Graph
    ↓
Renderer
    ↓
RHI
   ├─ D3D12
   ├─ Vulkan
   └─ Metal
```

Strictly prohibited:
Scene -> Vulkan API
Scene -> Metal API

The upper layers of the Renderer must not know about:
- VkImage
- VkBuffer
- VkDescriptorSet
- MTLTexture
- D3D12 Descriptor Handle


## XII. RHI Design

Core abstractions:

- GraphicsDevice
- CommandBuffer
- Buffer
- Texture
- Sampler
- Pipeline
- Shader
- Fence
- SwapChain
- Queue
- ResourceState
- ResourceBinding
- RenderPass / RenderingInfo

API concepts:

```cpp
cmd->BeginRendering(...);
cmd->SetPipeline(...);
cmd->BindResources(...);
cmd->DrawIndexed(...);
cmd->EndRendering();
```

Backend：

```text
RHI/
├─ D3D12/
├─ Vulkan/
└─ Metal/
```

Goals:
When extending or correcting any Backend, do not rewrite:
- Scene
- Material
- Render Graph
- Renderer
- Editor
- Asset


## XIII. Resource State / Barrier

Native ResourceState:

Undefined
CopySource
CopyDestination
ShaderResource
RenderTarget
DepthWrite
DepthRead
Present
Storage
IndirectArgument

Vulkan Image Layout must not leak into the upper layers of the Renderer.

The Render Graph is responsible for deriving:

Undefined
 -> RenderTarget
 -> ShaderResource
 -> Present

Each of DX12 / Vulkan / Metal maps these independently.


## XIV. Binding / Descriptor Architecture

This is one of the most core modules of the RHI and is not regarded as a simple API wrapper.

Strategy:
- Hybrid Bindless-first
- Retain a fallback path for older devices at the same time

High-level model:

Global Resource Table
        +
Per-Frame Constants
        +
Per-Pass Constants
        +
Per-Material Data
        +
Per-Draw Data

Resource Registry:
- Texture
- Buffer
- Sampler

Each GPU Resource obtains:
- Stable Resource Handle
- GPU Resource Index

Material does not store native Descriptors.

For example:

```cpp
struct MaterialGPU
{
    uint32_t baseColorTexture;
    uint32_t normalTexture;
    uint32_t ormTexture;
```

    float metallic;
    float roughness;
};

The Shader obtains the Texture through the ResourceIndex.

Vulkan:
- Descriptor Set
- Descriptor Indexing
- Large Resource Arrays

Metal:
- Argument Buffer
- Resource Table

DX12:
- Shader-visible Descriptor Heap
- Descriptor Table
- Root Signature Mapping

Prohibited:
- The upper layers directly seeing VkDescriptorSet
- Material creating a set of Descriptors for every Draw
- Binding the API to the traditional slot-based model


## XV. Binding Tier

It cannot be assumed that all mobile phones support full Bindless.

Define Binding Capability Tier:

Tier 1:
- Traditional / Limited Resource Binding
- Suitable for older or more restricted devices

Tier 2:
- Large Indexed Resource Tables

Tier 3:
- Full Bindless / GPU-driven Friendly

Perform Capability Query at startup:

```cpp
if (device.bindingTier >= BindingTier::Bindless)
{
    UseBindlessRenderer();
}
else
{
    UseFallbackBinding();
}
```

The Renderer must be able to select the Binding Path according to the Tier.


## XVI. Draw Data / GPU Driven Reservation

DrawData：

```cpp
struct DrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
};
```

GPU：

```text
DrawData
 ↓
MaterialIndex
 ↓
```
```text
Material Buffer
 ↓
```
```text
Texture ResourceIndex
 ↓
```
Global Resource Table

This directly supports the future:
- GPU Culling
- Indirect Draw
- GPU Driven Renderer
- GPU LOD Selection


## XVII. Render Graph

Introduced from V1.

Functions:
- Pass Dependency
- Resource Lifetime
- Temporary Render Target
- Barrier
- Synchronization
- Render Target Reuse
- Debug Visualization

Typical flow:

```text
Shadow Pass
↓
```
```text
Depth Prepass (optional)
↓
```
```text
Light Culling
↓
```
```text
Forward+ Opaque
↓
Sky
↓
Transparent
↓
SSAO
↓
Bloom
↓
```
```text
Tone Mapping
↓
```
Runtime UI


## XVIII. Rendering Roadmap

Primary Renderer:
- Forward+

Reasons:
- Suitable for Android / iOS
- Lower bandwidth
- MSAA-friendly
- Natural Transparent integration
- Can also maintain high quality on desktop

V1:
- CPU Frustum Culling
- Material Sorting
- GPU Instancing
- Forward+
- Shadow

V2:
- Compute Culling
- Indirect Draw
- GPU Occlusion Culling
- GPU LOD Selection
- GPU Driven Rendering



## XVIII-A. VFX / Particle Framework

The particle / effects system of this engine does not use “each effect consists of a large number of independent Scene Nodes + ParticleSystem Components” as its core Runtime model.

The Editor may retain the intuitive workflow in which “one complete effect consists of N Emitters,” but the Runtime must use a Data-Oriented VFX Program.

Formal model:

```text
Scene
└─ VFXComponent
   └─ VFXInstance

VFX Asset
├─ System Parameters
├─ Logical Emitters[]
│  ├─ Spawn Modules
│  ├─ Update Modules
│  ├─ Render Modules
│  ├─ Events
│  └─ Local Parameters
└─ Timeline / Sequencing

        ↓ Compile

VFX Program
├─ Dependency DAG
├─ Stateless Emitters
├─ Stateful CPU Emitters
├─ Stateful GPU Emitters
├─ Fused Simulation Batches
├─ Renderer Batches
└─ Resource Layout
```

### One Effect Can Contain Multiple Logical Emitters

For example:

```text
Explosion
├─ Flash
├─ Fire
├─ Smoke
├─ Sparks
├─ Debris
├─ Shockwave
└─ Decal
```

The above items can be clearly layered in the Editor, like Unity / Niagara, but:

- A Logical Emitter is not a Scene Node.
- A Logical Emitter is not an EntityID.
- A Logical Emitter is not an independent Component lifecycle object.
- One `VFXComponent` corresponds to one `VFXInstance`.
- A `VFXInstance` may contain N Logical Emitters.
- N Logical Emitters do not represent N Compute Dispatches or N Draw Calls.

### Runtime Batch Fusion

Logical Emitters meeting compatible conditions may be fused during the Cook / Compile stage.

For example:

```text
Fire Emitter
Spark Emitter
Ember Emitter
↓
Same Simulation Domain
Same Renderer Domain
Compatible Material / Blend
↓
GPU Simulation Batch A
```

Formal relationship:

```text
N Logical Emitters
→ M Simulation Batches
→ K Render Batches

Usually:
M <= N
K <= N
```

Fusion conditions must include at least:

```text
Simulation Domain
Renderer Domain
Material / Shader Compatibility
Blend Mode
Collision Mode
Sort Requirement
Event Dependency
Resource Layout
```

Correctness must not be compromised in pursuit of the number of Batches.

### Simulation Domain

Formal definition:

```text
SimulationDomain
├─ Stateless
├─ CPU
└─ GPU
```

#### Stateless Emitter

Applicable to:

```text
Torch Flame
Ambient Dust
Rain
Snow
Simple Spark
Simple Looping Aura
```

If particle state can be derived from:

```text
ParticleState = F(
    InstanceSeed,
    ParticleID,
    Time,
    Parameters
)
```

then persistent per-particle state is not required.

Goals:

- Reduce CPU Tick.
- Reduce persistent memory.
- Suitable for large-scale background / environmental effects.
- Suitable for mobile profiles.

#### CPU Particle

Applicable to:

```text
Low particle count
Gameplay-relevant interaction
Precise CPU event
Physics query
Readback-required state
```

The data structure uses Dedicated Particle SoA:

```text
ParticleSoA
├─ Position[]
├─ Velocity[]
├─ Age[]
├─ Lifetime[]
├─ Size[]
├─ Rotation[]
├─ Color[]
├─ CustomData[]
└─ AliveIndices[]
```

CPU particle updates use the Job System; per-particle virtual Update is not allowed.

#### GPU Particle

Applicable to:

```text
Smoke
Fire
Snow
Rain
Leaves
Magic particles
Sparks
Environment FX
```

Typical flow:

```text
Spawn Command Buffer
↓
GPU Particle Pool
↓
Compute Simulation
↓
Alive / Dead Compaction
↓
Culling
↓
Optional Sort
↓
Indirect Draw
```

### VFX Instance Shared State

Logical Emitters within the same complete effect share:

```text
VFXInstance
├─ Transform
├─ Time
├─ Seed
├─ Visibility
├─ LOD
├─ Quality
├─ ParameterBlock
└─ Runtime Handles
```

Avoid having each Emitter independently duplicate:

```text
Transform
Time
Visibility State
Quality State
Common Parameters
```

### VFX Module Graph

Editor workflow:

```text
Spawn
↓
Initialize
↓
Update
↓
Output
```

The built-in Module Library must include at least:

```text
Spawn
├─ Rate
├─ Burst
├─ Shape
└─ Initial Velocity

Update
├─ Gravity
├─ Drag
├─ Noise
├─ Attraction
├─ Color Over Life
├─ Size Over Life
├─ Rotation Over Life
└─ Collision

Render
├─ Sprite
├─ Stretched Sprite
├─ Mesh
├─ Ribbon
├─ Trail
└─ Decal / Projected FX
```

The Runtime does not execute the Module Stack through a high-cost string-based interpreter.

Formal flow:

```text
Editor VFX Graph
↓
VFX Compiler
↓
Constant Folding
↓
Dead Module Elimination
↓
Module Fusion
↓
Dependency Analysis
↓
CPU Program / GPU Compute Program
↓
Cooked VFXProgram
```

### Dead Module Elimination

If a feature is explicitly disabled in the Asset / Quality Profile:

```text
Collision Disabled
Distortion Disabled
Ribbon Disabled
```

the corresponding Module must not enter the Runtime Program.

The VFX Compiler must support:

```text
Constant Folding
Unused Parameter Removal
Dead Module Elimination
Compatible Module Fusion
Feature Stripping
```

### Renderer Domain

Formal Renderer types:

```text
RendererDomain
├─ Billboard
├─ StretchedBillboard
├─ Mesh
├─ Ribbon
├─ Trail
├─ Decal
└─ FutureVolume
```The same Simulation Data may be attached to multiple Renderer Outputs, but the Compiler must analyze whether sharing the cost is reasonable.

### VFX Material / Shader

VFX uses the existing Material / Shader Framework and does not establish an independent Renderer.

Supported:

```text
Unlit
Additive
Alpha Blend
Alpha Cutout
Distortion
Soft Particle
Lit Particle
Stylized / Anime VFX
```

Anime-style VFX may support:

```text
Hard Ramp
Dissolve
Fresnel
Emission
Distortion
Stylized Trail
Stylized Rim
```

### Soft Particle

V1 recommends officially supporting:

```text
Particle Depth
-
Scene Depth
↓
Depth Fade
↓
Particle Alpha
```

This avoids hard seams where the Billboard intersects scene geometry.

### Collision Tier

Not all GPU particles may use full Physics.

Official tiers:

```text
CollisionTier

Tier 0
→ None

Tier 1
→ Depth Buffer Collision

Tier 2
→ Heightfield / SDF / Simplified Scene Collision

Tier 3
→ CPU Physics Query
```

For example:

```text
Rain
→ Depth Collision

Spark
→ Depth / Heightfield

Gameplay Projectile
→ Gameplay / Physics System
```

A Gameplay projectile is not considered an ordinary VFX particle.

### Gameplay and Visual Separation

Official principle:

```text
Gameplay Projectile / Damage Logic
→ Gameplay System / Physics

Visual Trail / Spark / Impact
→ VFX System
```

VFX must not be the default source for gameplay-authoritative hit detection.

### Event / SubEmitter

Supported:

```text
Particle Spawn Event
Particle Death Event
Collision Event
Custom VFX Event
Gameplay-triggered Event
```

A SubEmitter does not create a new Scene Node / Particle Component.

Flow:

```text
Emitter Event
↓
Event Buffer
↓
Spawn Commands
↓
Target Logical Emitter
```

Emitter dependencies must establish a DAG during the Compile phase.

```text
Rocket
└─ Death Event
   ↓
Explosion Emitter
```

Runtime dynamic searches for Emitters by string name are prohibited.

GPU → CPU event readback must be restricted.

Priority:

```text
GPU Visual Event
→ GPU-side consume

Gameplay Authoritative Event
→ CPU-side explicit path
```

### Bounds / Visibility Policy

Each VFX Asset must specify a Visibility Simulation Policy:

```text
Always Simulate
Reduce Frequency
Pause Simulation
Catch-up On Visible
Kill When Invisible
```

It must not be assumed that all off-screen VFX can be safely stopped.

### Simulation Rate Decoupling

The particle Simulation Rate does not need to equal the Render FPS.

For example:

```text
Render
120 Hz

VFX Simulation
60 / 30 / 15 Hz
```

The following is allowed:

```text
Low-frequency simulation
+
Render interpolation
```

Particularly suitable for:

```text
Smoke
Fog particles
Ambient dust
Distant environment FX
```

### VFX Budget Manager

The system must not only limit Max Particle Count.

Official budget:

```text
VFXBudgetManager
├─ CPU Time Budget
├─ GPU Simulation Budget
├─ GPU Render Budget
├─ Active VFX Instance Count
├─ Active Logical Emitter Count
├─ Particle Count
├─ Draw Count
├─ Overdraw Estimate
└─ Memory Budget
```

Each VFX Asset may set a Priority:

```text
Critical
Gameplay
Character
Environment
Cosmetic
Background
```

When the Budget is exceeded, the degradation order may be controlled by Policy:

```text
Background
→ Lower Spawn Rate

Environment
→ Lower Simulation Rate

Cosmetic
→ Disable Collision / Distortion

Gameplay / Critical
→ Preserve
```

Integrate with the existing:

```text
PerformancePolicyManager
```

to adjust according to:

```text
Thermal
Battery
Frame Time
Memory Pressure
Quality Profile
Platform Profile
```

Adjust:

```text
Spawn Rate
Max Particle Count
Simulation Frequency
Collision Tier
Distortion
Shadow
Ribbon Quality
```

### Dedicated Particle Handles

Particles and short-lived VFX do not use Scene EntityID.

Use dedicated:

```text
ParticleHandle
VFXInstanceHandle
EmitterRuntimeHandle
```

Generation validation must be retained.

### VFX Memory

CPU particles:

```text
Dedicated SoA Pool
```

GPU particles:

```text
GPU Particle Pool
Alive List
Dead List
Spawn Buffer
Event Buffer
Indirect Args Buffer
```

Existing FrameAllocator may be used for frame-temporary storage, but:

- FramePtr / FrameSpan must not be retained across Frames.
- GPU resource reuse must wait for the corresponding GPU Fence.
- Persistent particle state must not be placed in the Frame Arena.

### RenderGraph Integration

GPU VFX must be expressed through RenderGraph:

```text
Spawn Pass
Simulation Pass
Compaction Pass
Optional Sort Pass
Render Pass
```

This allows RenderGraph to track:

```text
Buffer State
Resource Lifetime
Barrier
Queue Ownership
Async Compute Opportunity
```

VFX must not privately bypass RenderGraph and directly inject untrackable GPU work.

### Async Compute

If permitted by the Backend / Platform Profile, the following may be placed in Async Compute:

```text
Particle Simulation
Compaction
Culling
```

This must be determined by the Scheduler / RenderGraph. VFX Assets may not bind themselves to a backend queue.

### VFX Profiler

At minimum, display:

```text
Active VFX Instances
Logical Emitters
Runtime Simulation Batches
Render Batches
CPU Particle Count
GPU Particle Count
Spawn Count / Frame
CPU Simulation Time
GPU Simulation Time
GPU Render Time
Overdraw Estimate
Sort Cost
Collision Cost
Event Count
Memory
```

The Profiler must be able to display:

```text
Logical Emitters
→ Actual Runtime Batches
```

to verify that Fusion is effective.

### VFX CI / Validation

CI must include at least:

- VFX Graph compile determinism.
- Dependency DAG cycle fail.
- Dead Module Elimination test.
- Stateless / CPU / GPU representative asset.
- CPU particle lifetime / handle generation validation.
- GPU buffer lifetime / fence-safe reuse.
- VFX feature stripping.
- Soft Particle Golden Image.
- Billboard / Mesh / Ribbon / Trail representative Golden Image.
- Runtime Batch Fusion correctness.
- Off-screen simulation policy regression.
- VFX Budget degradation policy test.
- Android / iOS mobile particle budget test.
- DX12 / Vulkan / Metal representative GPU simulation test.

### VFX DoD

The V1 VFX Framework must at least complete:

```text
1 VFXComponent
→ 1 VFXInstance

1 VFXInstance
→ N Logical Emitters

N Logical Emitters
→ M Runtime Simulation Batches
→ K Render Batches
```

and be able to produce representative effects:

```text
Explosion
├─ Flash
├─ Fire
├─ Smoke
├─ Sparks
└─ Shockwave
```

Requirements:

- Each Logical Emitter can still be adjusted independently in the Editor.
- Runtime does not create 5 Scene Particle Components.
- GPU-compatible Emitters can be fused.
- The mapping between Logical Emitters and Runtime Batches can be displayed.
- Dynamic degradation can be performed under the mobile profile.
- Gameplay logic and visual particles are separated.



## XVIII-B. Animation Framework

The engine's Animation System uses a Data-Oriented Runtime and layers animation logic, Pose Evaluation, Skinning, and GPU Crowd Animation.

Official flow:

```text
Gameplay / AI
↓
Animation Parameters
↓
Animation Graph / State Machine
↓
Pose Evaluation
↓
Local Pose
↓
Global Pose
↓
Skinning Data
↓
GPU Skinning / GPU Animation Path
```

### Core Assets

```text
Animation Asset
├─ Skeleton
├─ Skinned Mesh
├─ Animation Clip
├─ Animation Graph
├─ Bone Mask
├─ Retarget Profile
├─ Animation Profile
└─ GPU Animation Cooked Data
```

Source Assets are not used as the direct Shipping Runtime format.

```text
FBX / glTF / Source Animation
↓
Importer
↓
Canonical Skeleton / Clip
↓
Animation Cooker
↓
Runtime Animation Assets
```

### Skeleton / Rig

A Skeleton Asset must contain at least:

```text
Skeleton
├─ Bone Hierarchy
├─ Parent Index
├─ Bind Pose
├─ Inverse Bind Pose
├─ Bone Name / Stable Bone ID
├─ Skeleton UUID
└─ Optional Skeleton LOD Mapping
```

Runtime does not represent the Skeleton using a Scene Node / Component for each bone.

The Skeleton is a shared Asset:

```text
1 Skeleton Asset
→ N Character Instances
```

### Animation Clip

Runtime Clips must support at least:

```text
Translation Track
Rotation Track
Scale Track
Root Motion Track
Animation Events
Compression Metadata
```

The Cook stage supports:

```text
Track Optimization
Key Reduction
Compression
Constant Track Folding
Unused Bone Track Removal
Platform-specific Cook
```

### Animation Graph

The Editor may use:

```text
State Machine
Blend Tree
Layer
Bone Mask
Additive
Pose Cache
IK
Root Motion Node
Custom Animation Node
```

However, Runtime does not directly execute Editor Graph Objects.

Official flow:

```text
Animation Graph
↓
Animation Compiler
↓
AnimationProgram
```

AnimationProgram must contain at least:

```text
State Table
Transition Table
Parameter Table
Pose Ops
Blend Ops
Layer Ops
Event Ops
Runtime Metadata
```

Parameters may not be driven by high-frequency string lookup.

Use:

```text
AnimationParameterID
StateID
TransitionID
ClipID
BoneMaskID
```

### State Machine / Blend Tree

V1 must support at least:

```text
State Machine
1D Blend
2D Blend
Direct Blend
Cross Fade
Transition Conditions
Trigger / Bool / Float / Int Parameters
```

Typical:

```text
Idle
↓ Speed
Walk
↓ Speed
Run
```

And:

```text
Grounded == false
→ Jump

AttackTrigger
→ Attack
```

### Layer / Bone Mask / Additive

Supported:

```text
Base Layer
Upper Body Layer
Face Layer
Additive Layer
```

Composition:

```text
Final Pose
=
Base
+ Masked Override
+ Additive
```

Bone Masks must be Cooked into compact bone ranges / bitsets / index tables; runtime string-name searches must not be used.

### Pose Cache

If multiple Animation Nodes require the same intermediate Pose in the same Frame:

```text
Evaluate Once
↓
Pose Cache
↓
Reuse
```

The Profiler must provide:

```text
Pose Cache Hit
Pose Cache Miss
Pose Memory
Duplicate Evaluation Avoided
```

### Root Motion

The Animation System only outputs:

```text
RootMotionDelta
```

It does not secretly modify the Scene Node directly.

Flow:

```text
Animation
↓
Extract RootMotionDelta
↓
Gameplay / Character Movement
↓
Apply / Reject / Warp
```

This facilitates:

```text
Prediction
Networking
Motion Warping
Gameplay Authority
```

### IK / Procedural Pose

V1 recommends:

```text
Two Bone IK
Look At
Aim IK
Foot IK
```

Later:

```text
FABRIK
CCD
Pose Warping
Motion Warping
```

IK must be controlled by Animation LOD; distant characters may skip it or update it at a lower frequency.

### Retargeting

Officially support:

```text
Source Skeleton
↓
Retarget Profile
↓
Target Skeleton
```

The Retarget Profile must contain at least:

```text
Bone Mapping
Retarget Pose
Root Mapping
Scale Rules
Twist Rules
IK Bone Mapping
```

High-cost Runtime Retargeting is not the default path for large Crowds.

Priority is given to completing processable data in advance during:

```text
Import / Cook
```

### Secondary Motion

The following may be added after the animation Pose:

```text
Spring Bone
Simple Chain Dynamics
Physics-driven Bone
Cloth Interface
Accessory / Hair / Tail Dynamics
```

Flow:

```text
Base Animation Pose
↓
IK / Procedural
↓
Secondary Motion
↓
Final Pose
```

### GPU Skinning

V1 officially supports GPU Skeletal Mesh Skinning.

Basic flow:

```text
CPU Animation Graph
↓
CPU Pose Evaluation
↓
Final Bone Matrices
↓
Global Skinning Buffer
↓
GPU Vertex Skinning
```

Use the official Draw Data:

```cpp
struct alignas(16) SkinnedDrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t skinningMatrixOffset;
};
```

Rules:

- Use `skinningMatrixOffset`.
- Do not use per-draw `skinningBufferIndex`.
- Do not create an independent Constant Buffer for each character.
- Do not apply blanket 256-byte per-character padding.
- GPU storage alignment is handled according to the actual Backend / Buffer Contract.

Global Skinning Buffer:

```text
Character A
→ offset 0

Character B
→ offset N

Character C
→ offset M
```

### Compute Skinning

V2 supports:

```text
Bone Matrix Buffer
+
Bind Pose Vertex Buffer
↓
Compute Skinning
↓
Skinned Vertex Buffer
```

Suitable for:

```text
Shadow Pass
Depth Pass
Main Pass
Outline Pass
Motion Vector Pass
```

The same skinning result is shared.

Official Skinning Mode:

```text
SkinningMode
├─ Auto
├─ VertexShader
└─ Compute
```

`Auto` may select based on:

```text
Vertex Count
Render Pass Count
Skeleton Size
Distance
Platform
GPU Budget
```

### Skinned Mesh Instancing

Officially support:

```text
1 Skeleton Asset
1 Skinned Mesh
1 Material
→ N Character Instances
```

Each Instance may have:

```text
World Transform
Animation State
Animation Time
Pose / Pose Handle
Skinning Matrix Offset
Material Params
LOD
```

Official Instance Data:

```cpp
struct SkinnedInstanceData
{
    uint32_t transformIndex;
    uint32_t skinningMatrixOffset;
    uint32_t materialIndex;
    uint32_t flags;
};
```

GPU usage:

```text
InstanceID
↓
SkinnedInstanceData
↓
skinningMatrixOffset
↓
Bone Transform
```

Supported:

```text
GPU Instancing
Indirect Draw
GPU Culling
LOD Selection
```

N Instances do not represent N Draw Calls.

### Animation Sharing

Characters with the same Skeleton / Clip and similar Time may share a Pose.

```text
1000 NPC
↓
Pose Bucketing
↓
50 Unique Poses
```

Instance:

```text
Instance
→ poseIndex
```

rather than:

```text
Instance
→ dedicated full bone matrices
```

Animation Sharing may integrate with:

```text
Skeleton LOD
GPU Instancing
Indirect Draw
GPU Culling
```

### Animation Update Rate LOD

Animation logic / Pose Update does not need to equal the Render FPS.

For example:

```text
Near
→ 60 Hz

Mid
→ 30 Hz

Far
→ 15 Hz

Very Far
→ Pose Hold / Shared Pose / Impostor
```

Rendering may perform interpolation.

### Skeleton LOD

Supported:

```text
LOD0
→ Full Skeleton

LOD1
→ Reduced Skeleton

LOD2
→ Crowd Skeleton
```

The following may be removed:

```text
Finger
Facial
Accessory
Secondary Bones
```

However, Mesh Skin Weights / Bone Remapping must be correctly generated by the Cook Pipeline.

### GPU Animation / Bone Animation Texture

The Bone Animation Texture-style GPU Animation path is officially supported.

The high-level API does not hard-code storage as Texture2D.

Use the abstraction:

```text
GPUAnimationPoseStorage
```

The Backend may use:

```text
Texture2D
Texture2DArray
StructuredBuffer
StorageBuffer
```

Official flow:

```text
Source Animation Clip
↓
Animation Cooker
↓
Sample Clip
↓
Evaluate Skeleton Hierarchy
↓
Global Bone Transform
↓
Inverse Bind Pose
↓
Final Skin Matrix
↓
Pack
↓
GPUAnimationClip
+
GPUAnimationPoseStorage
```

### Automatic Bone Animation Texture Baking

The Bone Animation Texture must be automatically generated by the Asset / Cook Pipeline.

Users do not need to perform:

```text
Manual Export Texture
Manual Frame Layout
Manual Bone Packing
Manual Atlas Maintenance
```

Official workflow:

```text
FBX / glTF / Animation Clip
↓
Import
↓
Animation Profile
↓
Animation Cooker
↓
CPU Runtime Clip
+
GPU Animation Clip
```

The Source Clip is not deleted because GPU Data is Baked.

### Animation Runtime Mode

Asset / Profile:

```text
AnimationRuntimeMode
├─ Auto
├─ Skeletal
├─ BoneTexture
└─ VAT
```

`Auto` may select the Runtime Path based on:

```text
Character Importance
Distance
Instance Count
Skeleton Complexity
Animation Feature Requirement
Platform Profile
Performance Budget
```

### Animation Profile

Bake settings are centralized in the Animation Profile; each Clip does not need to repeat the settings.

Example:

```text
CrowdAnimationProfile

Runtime:
Skeletal + BoneTexture

GPU Animation:
Encoding = Matrix3x4
SampleRate = 30 Hz
Interpolation = On
SimpleCrossFade = On
SkeletonLOD = On
Compression = Auto
```

The Profile may be applied to an entire Asset Folder / Character Class / Cook Rule.

### Platform-specific GPU Animation Cook

Different Platform Profiles may Cook:

```text
Windows High
→ 30 / 60 Hz

macOS High
→ 30 / 60 Hz

Android High
→ 30 Hz

Android Low
→ 15 Hz + Compression

iOS
→ 30 Hz + Compression
```

Runtime does not need to carry data for all Platforms.

### GPU Animation Encoding

V1:

```text
Matrix3x4
```

Reasons:

```text
Simple
Stable
Low shader complexity
Direct final skinning matrix
```

V2 may add:

```text
CompressedTRS
DualQuaternion
QuantizedTRS
```

### Final Skin Matrix Bake

Crowd Bone Animation Texture V1 defaults to directly Baking:

```text
Final Skin Matrix
```

rather than Local Bone Transform.

Flow:

```text
Local Pose
↓
Hierarchy Solve
↓
Global Bone Matrix
↓
Inverse Bind Pose
↓
Final Skin Matrix
↓
Bake
```

Runtime:

```text
Sample Final Skin Matrix
↓
Skin Vertex
```

Advantages:

```text
No runtime hierarchy solve
Low GPU control-flow complexity
Good for massive crowd
```

Limitations:

```text
No full runtime IK
No arbitrary procedural bone edit
No full runtime retarget
```

If greater flexibility is required in the future, the following may be added:

```text
LocalTRS GPU Animation Mode
```

But this is not the V1 Crowd default.

### GPU Animation Clip Metadata

At minimum:

```cpp
struct GPUAnimationClipMeta
{
    uint32_t poseOffset;
    uint32_t frameCount;
    float sampleRate;
    float duration;
};
```

Actual Cook Metadata should also include:

```text
Skeleton ID
Bind Pose Hash
Encoding
Bone Count
Skeleton LOD
Compression Version
Cook Version
```

### GPU Animation Instance Data

Large Crowd Runtime Instances require only a small amount of data.

For example:

```cpp
struct GPUAnimationInstanceData
{
    uint32_t clipIndex;
    uint32_t frame0;
    uint32_t frame1;
    float frameBlend;

    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t lod;
    uint32_t flags;
};
```

Or Runtime may only input:

```text
clipIndex
normalizedTime
```

and let the GPU derive:

```text
frameFloat
frame0
frame1
blend
```

### Frame Interpolation

GPU Animation supports:

```text
Frame 0
Frame 1
↓
Interpolation
↓
Final Pose
```

This reduces visual stuttering when lowering the Bake Sample Rate.

### Simple Cross Fade

Crowd GPU Animation supports limited simple transitions:

```text
Clip A + Time A
Clip B + Time B
Transition Weight
```

GPU:

```text
Sample Pose A
Sample Pose B
↓
Blend
↓
Skin
```

The GPU Crowd Path is not required to execute a complete arbitrary Animation Graph.

Official distinction:

```text
Full Animation Path
→ CPU Graph
→ CPU Pose
→ GPU Skinning

GPU Crowd Path
→ Simple State
→ GPUAnimationClip
→ GPU Pose Sampling
→ GPU Skinning
```

### Clip Atlas / Pose Storage Packing

The Animation Cooker automatically packs:

```text
Idle
Walk
Run
Attack
Hit
Death
...
```

into:

```text
GPUAnimationPoseStorage
```

Users are not required to manually maintain a Texture Atlas.

The Cooker must maintain:

```text
Clip Offset
Frame Count
Bone Count
Encoding
Alignment
Version
```

### Bake Dependency / Invalidation

GPU Animation Cooked Data must depend on:

```text
Source Clip Hash
Skeleton UUID
Skeleton Layout
Bind Pose Hash
Bone Count
Sample Rate
Encoding
Compression Settings
Skeleton LOD Mapping
Cook Version
```

Any change:

```text
Invalidate GPU Animation Asset
↓
Automatic Rebuild
```

This avoids mismatches between Skeleton and BAT data.

### Bake Validation

The Cook must validate:

```text
Skeleton ID Match
Bone Count Match
Bind Pose Hash Match
Clip Duration
Frame Count
No NaN / Inf
Matrix Validity
Encoding Validity
Storage Bounds
```Failure:

```text
Cook Hard Fail
```

It must not be discovered only at Runtime.

### Character Animation LOD

Recommended:

```text
LOD0
→ CPU Animation Graph
→ Unique Pose
→ Vertex / Compute Skinning

LOD1
→ Bone Animation Texture
→ 30 Hz
→ Per-instance Time

LOD2
→ Bone Animation Texture
→ 15 Hz
→ Pose Bucket / Animation Sharing

LOD3
→ VAT / Impostor
```

This is a recommended Profile; all games are not required to use these exact values.

### VAT

VAT serves as a more aggressive Animation Path for large quantities / long distances / VFX.

```text
Vertex Position
Normal
Optional Tangent
↓
Bake per frame
↓
Vertex Animation Storage
```

Applicable to:

```text
Background Crowd
Very Far Character
Fixed Animation Creature
Cloth / Destruction / VFX Mesh
```

VAT does not replace general Skeletal Animation.

### Animation Events

Use:

```text
AnimationEventID
Payload
Time
```

Avoid high-frequency string Callbacks.

For example:

```text
FootstepLeft
FootstepRight
WeaponTrailOn
WeaponTrailOff
AttackWindowStart
AttackWindowEnd
```

Gameplay-critical authority should not depend entirely on Visual Animation Events.

### Gameplay / Animation Separation

Principles:

```text
Gameplay
→ Determines Attack / Movement / Ability

Animation
→ Presents / synchronizes character actions
```

Animation State must not serve as the sole source of all Gameplay Truth.

### Animation Memory

Persistent:

```text
Skeleton Asset
Animation Clip
Animation Program
Pose Pool
GPU Animation Pose Storage
Skinning Buffer
```

Transient:

```text
Frame Pose Scratch
Blend Scratch
IK Scratch
Upload Staging
```

Transient data must comply with the existing FramePtr / FrameSpan lifetime contract.

### Job System Integration

Animation Evaluation uses:

```text
Animation Graph Jobs
Clip Sampling Jobs
Pose Blend Jobs
IK Jobs
Global Pose Jobs
Skinning Upload Jobs
```

The necessary Pose must be completed before Render Extraction.

This must be consistent with the existing Frame Barrier / Module-safe Barrier.

### RenderGraph / GPU Integration

Compute Skinning / GPU Animation can be processed through RenderGraph:

```text
GPU Animation Sample
↓
Optional Compute Skinning
↓
Depth / Shadow / Main / Outline
```

GPU resource lifetime / barrier / queue ownership are managed by RenderGraph.

### Animation Profiler

At minimum:

```text
Active Animators
Active Skeletons
Unique Poses
Shared Poses
Animation Sharing Hit Rate
Graph Evaluation Time
Clip Sampling Time
Pose Blend Time
IK Time
Retarget Time
Pose Cache Hit Rate
Bone Count
Skeleton LOD Distribution
Skinning Matrix Upload Bytes
GPU Animation Instances
GPU Animation Pose Storage Size
Vertex Skinning GPU Time
Compute Skinning GPU Time
GPU Animation Sampling Time
```

### Animation CI / Validation

CI must include at least:

- Skeleton import determinism.
- Clip compression regression.
- Animation Graph compile determinism.
- State Machine transition test.
- Blend Tree representative test.
- Pose Cache correctness.
- Root Motion extraction regression.
- Retarget representative skeleton test.
- Skeleton LOD bone remap validation.
- CPU Pose vs reference Golden Pose.
- Vertex Skinning representative Golden Image.
- Compute Skinning representative Golden Image.
- Skinned Instancing N-instance test.
- Animation Sharing correctness.
- GPU Animation Bake determinism.
- GPUAnimationClip dependency invalidation.
- Bind Pose Hash mismatch hard fail.
- Bone Animation Texture / Pose Storage frame interpolation test.
- GPU Crowd simple crossfade test.
- DX12 / Vulkan / Metal representative GPU animation test.
- Android / iOS crowd animation performance gate.

### Animation DoD

V1 Animation Framework must at least complete:

```text
Skeleton
+
Animation Clip
+
Animation Graph
+
State Machine / Blend Tree
+
Layer / Bone Mask
+
Pose Cache
+
Root Motion
+
Basic IK
+
GPU Vertex Skinning
+
Skinned Mesh Instancing
+
Animation LOD
```

The GPU Crowd / Bone Animation Texture path must at least complete:

```text
Automatic Bake
GPUAnimationClip
GPUAnimationPoseStorage
Final Skin Matrix Encoding
Frame Interpolation
Per-instance Animation Time
GPU Instancing
Indirect Draw Integration
Animation Sharing / Pose Bucket foundation
```

When the user normally imports:

```text
Skeleton + Animation Clips
```

the Cook Pipeline can automatically generate GPU Crowd Animation Data; manually creating an Animation Texture is not required.


## 19. Rendering Quality Targets

V1 target:
At least Unity URP level.

PBR:
- Cook-Torrance
- GGX
- Smith Geometry
- Schlick Fresnel
- Metallic / Roughness Workflow

Material Inputs:
- Base Color
- Metallic
- Roughness
- Normal
- AO
- Emission
- Alpha

Lighting:
- Directional Light
- Point Light
- Spot Light
- Forward+ Light Culling

Shadow:
- Cascaded Shadow Map
- PCF
- Shadow Bias
- Normal Bias

Environment:
- HDR
- IBL
- Reflection Probe
- Prefiltered Environment
- BRDF LUT

Post:
- SSAO
- Bloom
- FXAA
- Tone Mapping
- Color Grading
- Fog

Subsequent advanced effects:
- TAA
- Contact Shadow
- PCSS
- SSR
- Volumetric Fog
- Water
- Terrain Decal
- Terrain Virtual Texturing
- Advanced Vegetation Shading / Impostor Refinement

### Shading Model Framework

This engine does not force all materials into a single PBR Shader, nor does it apply a single Toon Shader to the entire world.

Formal Shading Models:

```text
ShadingModel
├─ PBR
├─ StylizedPBR
├─ Anime
├─ Vegetation
├─ Water
└─ Unlit
```

Common principles:

- All Shading Models share the same Renderer / Forward+ / RenderGraph.
- They share the Shadow, Light, Fog, Reflection, Lightmap / Probe, Resource Binding, and Shader Variant systems.
- Do not create separate parallel Renderers for Anime / StylizedPBR / Vegetation / Water.
- Shading Models are differences at the Material / Shader Feature level and do not change the RHI architecture.
- Each Shading Model must support Shader Variant Stripping according to the Build Profile / Asset Usage.

### PBR

Purpose:

```text
Realistic or near-realistic materials
→ Metal
→ Plastic
→ Stone
→ Industrial objects
→ Realistic scenes
```

V1 Contract:

```text
Cook-Torrance
+ GGX
+ Smith Geometry
+ Schlick Fresnel
+ Metallic / Roughness Workflow
```

It must also explicitly define:

```text
Linear Lighting
sRGB Texture Sampling
Energy Conservation
HDR Lighting
IBL
Diffuse Irradiance
Prefiltered Specular Environment
BRDF LUT
```

### StylizedPBR

`StylizedPBR` is the primary Scene Shading Model for “anime-style / Painterly scenes.”

Purpose:

```text
Architecture
Rock
Props
General Environment Mesh
Stylized Open-world Scene
```

The core is not to completely discard PBR, but rather:

```text
PBR Foundation
↓
Art-directed Material
↓
Stylized Light / Shadow Response
↓
Baked Lighting / Probe
↓
Fog / Atmosphere / Color Grading
↓
Stylized Scene Final Look
```

Supported items:

```text
Base Color
Normal
AO
Roughness
Metallic
Emission
Lightmap
Reflection Probe
Shadow Tint
Light Tint
Contrast Curve
Saturation Control
Stylized Roughness
Optional Lighting Ramp
```

Principles:

- Retain physical material fundamentals such as GGX / Roughness / Reflection.
- A configurable Stylization Curve may be applied to Direct Lighting.
- Shadows need not only be linearly darkened; Shadow Tint / Material-specific dark color may be used.
- Do not use character-style Face SDF / Hair Highlight logic to process general scenes.
- Static Scenes should prioritize integration with Lightmap / Probe / Baked GI data.
- Dynamic Main Light can still provide Directional Light / Shadow Map.
- Different Lighting LODs may be used at near, middle, and far distances, but the data sources and switching rules must be explicit.

### Anime Character Shading

`Anime` is a character-specific NPR / Anime Shading Framework and is not equivalent to ordinary two-step Toon.

Formal architecture:

```text
Anime
├─ Body
│  ├─ Multi-Ramp Diffuse
│  ├─ AO
│  ├─ Material Region
│  └─ Stylized Specular
│
├─ Face
│  ├─ Face SDF Shadow
│  ├─ Face Orientation
│  └─ Face-specific Color
│
├─ Hair
│  ├─ Toon Diffuse
│  ├─ Hair Highlight
│  ├─ Optional Flow / Anisotropic Direction
│  └─ Bangs Shadow
│
├─ Eye
│  ├─ Iris
│  ├─ Highlight
│  └─ Optional Parallax
│
├─ Rim
│
└─ Outline
   ├─ Smoothed Normal
   ├─ Width Mask
   └─ Material-dependent Color
```

Minimum V1 requirements:

```text
Multi-Ramp Material Lighting
Face SDF
Hair-specific Shading
Per-material Outline
```

#### Multi-Ramp Lighting

```text
NdotL
↓
Lighting Remap / Half-Lambert when needed
↓
Material Region / Ramp Index
↓
Ramp Texture / Curve
↓
Light / Mid / Shadow Color
```

Material regions must at least be distinguishable as:

```text
Skin
Hair
Cloth
Metal
Leather / Hard Surface
```

Different regions may have different:

```text
Ramp
Shadow Color
Specular Response
Rim Response
Outline Color
```

#### Anime Control Map

The channel definitions are not directly bound to any specific game assets; the engine uses its own Canonical Packing.

Recommended for V1:

```text
AnimeControlMap

R = AO
G = Specular Mask
B = Material Region / Ramp Index
A = Outline Width
```

If the information is insufficient, a second texture may be used:

```text
AnimeDetailMap

R = Hair Highlight Mask
G = Face / Special Mask
B = Rim Mask
A = Emission / Custom
```

The Asset Importer / Material Inspector must display the semantic meaning of each channel to avoid magic channels.

#### Face SDF

A general character Face does not directly depend entirely on the Mesh Normal’s NdotL.

```text
Face Forward / Right
+
Main Light Direction
↓
Relative Horizontal Light Angle
↓
Face SDF Sample
↓
Threshold / Smooth Transition
↓
Artist-authored Face Shadow
```

Goals:

- Avoid realistic but animation-inappropriate fragmented shadows formed by the bridge of the nose / eye sockets.
- Maintain stable animated facial shadows for front / left / right / backlighting.
- Face SDF is only an Anime Face feature and must not contaminate the general PBR pipeline.

#### Hair Shading

Hair does not share exactly the same Specular as general Cloth / Skin.

Supported features may include:

```text
Hair Diffuse
Hair Shadow
Hair Highlight Mask
Directional / Flow-based Highlight
Optional Stylized Anisotropic Specular
```

V1 may initially use:

```text
Artist-authored Highlight Mask
+
Directional Highlight Offset
+
Threshold / Ramp
```

Flow Map / anisotropic model may be added later.

#### Rim Light

Rim must not merely be a fixed ring of white light around the entire character.

```text
1 - NdotV
↓
Threshold / Power
↓
Light Direction
↓
Material / Rim Mask
↓
Rim Color
```

Supported:

```text
Backlight stronger
Front light weaker
Per-material intensity
Per-region mask
```

#### Outline

Character contours should primarily support Geometry Outline / Inverted Hull.

```text
Original Mesh
↓
Smoothed Normal Extrusion
↓
Cull Front
↓
Render Back Faces
↓
Outline
```

Outline may be jointly controlled by:

```text
Material Region
Vertex Color / Mask
Distance Compensation
Outline Width
Outline Color
```

Screen-space Outline may serve as a scene or Debug / Style extension, but does not replace Character Geometry Outline.

### Vegetation Shading

Vegetation is a dedicated Shading Model, rather than a general PBR Mesh with Alpha.

```text
Vegetation
├─ Alpha Cutout
├─ GPU Instancing
├─ Wind Vertex Animation
├─ Vertex Weight / Variation
├─ AO
├─ Optional Normal
├─ Transmission / Back Lighting
├─ Light Probe / SH
├─ Shadow
└─ LOD / Billboard
```

Wind:

```text
World Position
+
Time
+
Wind Direction
+
Noise
+
Vertex Weight
↓
Vertex Offset
```

Vertex Color / Asset Data may define:

```text
Root Weight
Tip Weight
Bend Weight
Variation
```

Leaf translucency:

```text
Back Lighting
+
Thickness / Material Parameter
+
Transmission Color
```

Avoid making backlit vegetation directly become black blocks.

### Water Shading

Water is an independent Shading Model / Render Feature.

V1 / advanced roadmap:

```text
Water
├─ Animated Normal
├─ Fresnel
├─ Shallow / Deep Color
├─ Scene Depth Fade
├─ Reflection Probe
├─ Optional SSR
├─ Refraction
├─ Foam
└─ Optional Caustics
```

Core:

```text
Scene Depth
↓
Shallow / Deep Blend

Fresnel
↓
Reflection Weight

Normal
↓
Reflection / Refraction Distortion
```

Reflection fallback:

```text
SSR Hit
→ SSR

SSR Miss / SSR Disabled
→ Reflection Probe / Environment
```

### Scene Lighting / Baked Lighting

Large anime-style scenes cannot rely only on large numbers of realtime lights.

Static Environment must support:

```text
Directional Sun
+
Shadow Map
+
Lightmap / Baked Lighting
+
Light Probe / SH
+
Reflection Probe
+
AO
```

Lighting LOD may use the following according to the profile:

```text
Near
→ Full / High-quality Baked Lighting

Mid
→ Lower-cost Baked / Probe representation

Far
→ Probe / SH / Vertex-baked fallback when appropriate
```

This defines engine capabilities and does not force all projects to use a fixed three-level strategy.

### Environment / Atmosphere

Part of the scene style is provided by the Environment Shader / Render Feature:

```text
Sky
Cloud
Distance Fog
Height Fog
Atmospheric Color
Cloud Shadow
Color Grading
Tone Mapping
```

Cloud Shadow may use a low-cost World-space projected mask:

```text
World Position XZ
↓
Scrolling Cloud Shadow Texture
↓
Directional Light Modulation
```

The geometric sky clouds and ground cloud shadows are not required to be exactly consistent on a per-pixel basis.

### Scene / Character Combination

Recommended anime-style world:

```text
World

Terrain
→ Terrain / StylizedPBR

Buildings
→ StylizedPBR

Rock / Props
→ StylizedPBR

Vegetation
→ Vegetation

Water
→ Water

Character
→ Anime

Sky / Fog / Cloud
→ Environment Render Features
```

Goals:

```text
Characters have clear Anime NPR recognition
+
The scene retains the material quality and sense of space of Stylized PBR
+
Unified light sources, shadows, fog, reflections, and RenderGraph are shared
```

### Shading Model CI / Golden Image Gate

At least establish the following Golden Scenes:

```text
PBR Material Sphere
StylizedPBR Architecture / Rock
Anime Character Body
Anime Face SDF
Anime Hair Highlight
Anime Outline
Vegetation Back Lighting
Water Reflection / Depth Fade
Mixed Scene：StylizedPBR Environment + Anime Character
```

CI validation:

- Representative Golden Images for DX12 / Vulkan / Metal.
- Direct / Shadow / Fog / Probe integration.
- Shading Model Variant compile / strip.
- Anime Face SDF left/right light-direction tests.
- Outline width must not become obviously uncontrolled as distance changes.
- Vegetation Wind / Transmission must not break Shadow / Instancing.
- Correct fallback when Water SSR is unavailable.
- Mixed Scene must not require a second Renderer.

## 20. Shader System

Primary language:
- Slang

Goals:
- HLSL-like syntax
- Module
- Generic
- Interface
- Reflection
- Specialization
- Cross-platform output

Pipeline:

```text
Slang
├─ DXIL   -> DX12
├─ SPIR-V -> Vulkan
└─ MSL    -> Metal
```

Critical risk:
The Slang -> Metal path must complete a PoC first.

PoC validation items:
- Vertex Shader
- Fragment Shader
- Compute Shader
- PBR
- Skinning
- Instancing
- Forward+ Compute
- Shadow
- Texture Array
- Argument Buffer
- Specialization Constant
- Reflection
- Windows DX12
- Windows Vulkan
- macOS Apple Silicon
- Physical iPhone device
- Xcode GPU Capture / Frame Debugger

Only after all items pass should Slang be designated as the sole Shader Frontend.

### Shader Source of Truth / Metal Fallback

Formal specification:

```text
Slang
→ Sole Shader Source of Truth
```

Maintaining another independent hand-written MSL Shader as a normal Fallback is prohibited, to avoid:

- Resource Binding Layout divergence
- Reflection Table divergence
- Material Parameter Layout divergence
- Shader Feature / Variant Key divergence
- Inconsistent DX12 / Vulkan / Metal behavior

Preferred Metal path:

```text
Slang
↓
MSL
↓
Apple Metal Compiler
```

If direct Slang output to MSL is obstructed in an actual version or for a specific Feature, the unified Fallback is:

```text
Slang
↓
SPIR-V
↓
SPIRV-Cross
↓
MSL
↓
Apple Metal Compiler
```

The following is not allowed:

```text
Independent Hand-written MSL
→ Bypassing Slang Reflection / Binding Contract
```

### Unified Reflection / Binding Contract

Regardless of whether Metal uses Direct MSL or SPIR-V → SPIRV-Cross → MSL, the Engine upper layers must use the same canonical shader metadata.

Canonical metadata must at least include:

```text
Resource ID
Resource Type
Set / Space
Binding
Array Count
Access
Stage Visibility
Argument Buffer Group
Material Parameter Layout
Push / Root Constant-equivalent metadata
Specialization Constant
Vertex Input / Fragment Output
```

Pipeline:

```text
Slang Source
↓
Canonical Reflection / Binding Metadata
├─ DX12 Mapping
├─ Vulkan Mapping
└─ Metal Mapping
     ├─ Direct Slang → MSL
     └─ SPIR-V → SPIRV-Cross → MSL
```

The Metal Backend must establish an explicit remap table that maps canonical Resource Binding to:

```text
[[buffer(N)]]
[[texture(N)]]
[[sampler(N)]]
Argument Buffer [[id(N)]]
```

Therefore, `MaterialGPU` / Resource Registry / ResourceIndex must not be aware of which compiler path the Metal fallback actually uses.

### Metal Fallback Gate

CI / Shader Gate must simultaneously validate:

- Direct Slang → MSL Reflection Mapping
- Fallback SPIR-V → SPIRV-Cross → MSL Mapping
- Resource Count
- Resource Type
- Logical Binding ID
- Argument Buffer Layout
- Material Parameter Offset / Size
- Pipeline Creation
- Golden Image (representative Shader)

If the two Metal paths produce different results for the canonical reflection contract:

```text
Build / CI Fail
```

rather than adding special cases to Runtime or the Material System.



## Shader Variant Management

Shader Variants must avoid fully precompiling the Cartesian product of all combinations.

Possible Variant dimensions:

```text
Graphics Backend
× Graphics Quality Tier
× Binding Tier
× Material Feature
× Geometry Feature
× Lighting Feature
× Platform Capability
```

Features are categorized using Bitmasks:

```text
MaterialFeature
├─ NormalMap
├─ Emission
├─ AlphaTest
├─ TerrainLayerBlend
├─ ShadingModelPBR
├─ ShadingModelStylizedPBR
├─ ShadingModelAnime
├─ ShadingModelVegetation
├─ ShadingModelWater
├─ ShadingModelUnlit
├─ AnimeFaceSDF
├─ AnimeHairHighlight
├─ AnimeOutline
├─ VegetationTransmission
├─ WaterSSR
└─ FutureMaterialFeatures

GeometryFeature
├─ Skinning
├─ Instancing
├─ Morph
└─ VegetationWind

LightingFeature
├─ Shadow
├─ IBL
├─ Fog
└─ AdditionalLights
```

Variant Key:

```text
ShaderVariantKey
=
Hash(
    ShaderID,
    Backend,
    QualityTier,
    BindingTier,
    FeatureMask
)
```

Core strategies:

1. Compile only Variants that are actually used: scan Scenes, Prefabs, Materials, Terrain Materials, Vegetation Assets, Quality Profiles, and Platform Capabilities.
2. Do not precompile all possible Feature combinations; when features such as Normal Map, Skinning, or Terrain Layer Blend are unused, the corresponding Variants are not generated.
3. Static / Dynamic Feature separation: only features that change the Resource Layout or Pipeline State, or have significant performance differences, should use Static Variants; small features should preferably use Dynamic Branch / Data-driven control.
4. Variant Cache: Editor On-demand Compile, Disk Cache, Pipeline Cache, Build Precompile, Stable Cache Key, invalidated according to the Shader Source Hash.
5. Incremental Shader Compile: establish a Shader dependency graph and record Shader Source, Included Module, Feature Definition, Material Usage, and Target Backend. After a Shader change, recompile only affected Variants.
6. Variant Budget: configure a Variant Budget for each Shader; CI monitors Total / Newly Added / Compiled / Stripped Variants, Compile Time, and Pipeline Count. Abnormal increases may produce a Warning or Fail.

CI Shader Gate:

```text
Changed Shader / Module
↓
Dependency Resolve
↓
Affected Variant Set
↓
Compile DX12 / Vulkan / Metal Targets
↓
Reflection Validation
↓
Pipeline Creation Smoke Test
↓
Golden Image / Feature Test（when needed）
```
### Metal Shader Fallback Risk Policy

Both direct Metal MSL and SPIRV-Cross fallback must be pre-validated in CI / device lab.

Golden Image / Reflection mismatch:

```text
CI Detect
↓
Mark Direct Path Unsupported for that Engine/Shader Feature Profile
↓
Build Pipeline selects validated fallback
```

It is prohibited to “dynamically switch the compiler pipeline on its own” in Shipping Runtime due to a one-off visual difference.

Fallback selection should be:

```text
Build-time / Cook-time validated policy
```

And record:

```text
Engine Version
Slang Version
SPIRV-Cross Version
Device / OS Profile
Shader Feature Profile
```


### Variant Budget Accounting

The Variant Budget for a single Shader does not use a fixed engine-wide constant,
but is jointly determined by:

```text
Shader Profile
+
Platform Profile
+
Quality Tier
```

CI / Build Report must track at least:

```text
Theoretical Variant Count
After-Pruning Count
Project Used Count
Cooked Count
Budget
```

For example:

```text
TerrainPBR

Theoretical : 8192
After Pruning: 1240
Project Used : 386
Cooked       : 386
Budget       : 512
```

These values are used to identify different types of problems:

```text
Theoretical too high
→ Feature definition combination explosion

After-Pruning still too high
→ Insufficient Mutually Exclusive / Constraint Rules

Project Used too high
→ Excessively scattered actual Feature usage in the project

Cooked > Project Used
→ Stripping / Cook Pipeline anomaly
```

### CI Gate

Each Shader / Platform Profile has its own:

```text
Variant Budget
```

When exceeded:

```text
Cooked Count > Budget
→ Hard Fail
```

Trend / Warning thresholds may also be configured for:

```text
Theoretical
After-Pruning
Project Used
```

to identify growth trends early.

It is prohibited to hardcode architecture-level:

```text
All Shaders <= 256
```

or similar globally fixed numbers.

The actual Budget should be configured separately for:

```text
Mobile Low
Mobile High
Desktop
Platform / Quality Profile
```

## XXI. Material System

### Material / Shading Model Layering

Material is not extended by creating a completely independent Renderer for every visual effect.

Formal layering:

```text
Material
├─ Shading Model
├─ Surface / Blend Mode
├─ Feature Mask
├─ Texture Handles
├─ Parameters
└─ Render State
```

Initial Shading Models:

- PBR
- StylizedPBR
- Anime
- Vegetation
- Water
- Unlit

Other specialized Material / Render Features:

- Transparent
- Sky / Atmosphere
- Terrain
- UI

Surface / Blend Mode is separate from Shading Model, for example:

```text
Opaque
Masked
Transparent
Additive / Special when explicitly supported
```

Avoid mistaking:

```text
Transparent
```

for a Lighting Model at the same level as:

```text
PBR / Anime / StylizedPBR
```

Material stores:
- Shader ID
- Texture Resource Handles
- Parameters
- Render State

It does not store:
- VkDescriptorSet
- Metal Argument Encoder State
- DX12 Descriptor Heap Pointer

The Material Setter can be:

material.SetTexture("BaseColor", texture);

But internally it is actually:
Texture
-> Resource Registry
-> ResourceIndex
-> MaterialGPU


### Material Shading Model Contract

The Material Asset must explicitly record:

```text
ShadingModel
SurfaceMode
FeatureMask
ShaderProfile
Quality Overrides（if any）
```

`ShadingModel` does not use arbitrary string runtime lookup; after Cook, it is converted into a stable enum / ID.

Recommended:

```cpp
enum class ShadingModel : uint8_t
{
    PBR,
    StylizedPBR,
    Anime,
    Vegetation,
    Water,
    Unlit
};
```

High-level Gameplay / Asset APIs must not directly know about Backend Pipeline State.

### Common Lighting Inputs

Shading Models may share:

```text
Main Directional Light
Forward+ Additional Light List
Shadow Data
IBL / Reflection Probe
Lightmap / Probe
Fog
Camera / View
Environment Parameters
```

However, each Model may choose to use only a subset.

For example:

```text
Anime
→ Main Light + Additional Light policy + Shadow + Fog

StylizedPBR
→ Main Light + Forward+ + IBL + Lightmap + Probe + Fog

Vegetation
→ Main Light + Probe + Shadow + Transmission + Fog

Water
→ Main Light + Environment + Reflection + Fog
```

Shared data must not force every Shading Model to perform all calculations.

### Material Region / Control Map Metadata

A Packed Control Texture must have Asset Metadata describing Channel Semantics.

For example:

```text
Texture Semantic = AnimeControlMap
R = AO
G = SpecularMask
B = MaterialRegion
A = OutlineWidth
```

Importer / Inspector / Cook Pipeline must share the same Semantic Schema.

It is prohibited to rely only on an “RGBA convention” outside the documentation without metadata.

### Shading Model Stripping

The Build Scanner must analyze:

```text
Scene
Prefab
Material
Terrain
Vegetation
Water
Quality Profile
Platform Profile
```

If the project does not use:

```text
Anime
Water
Vegetation
StylizedPBR
```

the corresponding Shader / Variant / Asset Cook paths may be removed.




## JSON / Serialization Framework

The Engine provides a built-in general-purpose JSON Parse / Generate Framework.

The underlying implementation uses:

```text
yyjson
```

Formal boundary:

```text
Engine Systems / Gameplay / Tools
↓
Engine JSON API
↓
yyjson
```

Engine Runtime / Editor / Gameplay must not distribute:

```text
yyjson_doc*
yyjson_val*
yyjson_mut_doc*
```

`yyjson_*` types are only permitted to exist in:

```text
Serialization/Json/Private/
```

### JSON Core API

V1 must provide at least:

```text
JsonDocument
MutableJsonDocument
JsonValue
JsonObject
JsonArray
JsonReader
JsonWriter
JsonError
JsonAllocator
```

Read concept:

```cpp
Result<JsonDocument> ParseJson(StringView text);
```

```cpp
JsonDocument document = Json::Parse(buffer);
JsonObject root = document.Root().AsObject();

uint32_t version = root.GetUInt("version");
StringView name = root.GetString("name");
bool enabled = root.GetBool("enabled");
```

Write concept:

```cpp
JsonWriter writer;
writer.BeginObject();
writer.Key("id");
writer.UInt(10001);
writer.Key("attack");
writer.Float(120.0f);
writer.EndObject();
```

Mutable DOM may be used by Editor / Tool to create and modify a JSON tree:

```text
MutableJsonDocument
↓
Mutable JsonObject / JsonArray
↓
JsonWriter
```

### JSON Write Mode

Officially supported:

```text
JsonWriteMode
├─ Compact
├─ Pretty
└─ Deterministic
```

Usage:

```text
Editor Save
→ Pretty

Runtime / Internal
→ Compact

CI / Generated Asset
→ Deterministic
```

The Deterministic Writer must guarantee:

```text
Stable Property Order
Stable Float Formatting
UTF-8
LF Newline
Locale Independent
```

The same logical data should produce identical deterministic JSON content / hash on Windows / macOS / CI.

### JSON Encoding / Strictness

The Engine JSON standard is:

```text
UTF-8 Only
Strict JSON
```

Runtime JSON does not accept by default:

```text
// comment
/* comment */
NaN
Infinity
-Infinity
undefined
```

When comments are required, use Schema / Editor metadata, or an explicitly permitted `_comment` field; do not create a non-standard JSON dialect.

If JSON5 is required in the future, it should be an independent Authoring Import Format and must not change the Runtime JSON Contract.

### JSON Number Validation

JSON natively has only `number`; the Engine / DataTable Schema must validate the target type:

```text
int32
uint32
int64
uint64
float
double
```

Silent casts are prohibited:

```text
uint32 field = -1
→ Error

integer ID = 1.5
→ Error

overflow / underflow
→ Error
```

The Writer must not output NaN / Infinity.

### JSON Error Reporting

JSON Parse / Deserialize Errors must contain at least:

```text
Error Code
Source File
Line
Column / Offset
JSON Path（when available）
Readable Message
Expected Type（when available）
Actual Type（when available）
```

Data Table errors may additionally include:

```text
Table
Row Key
Field
```

For example:

```text
Character.json:128:17
Table: Character
Row: 10001
Field: skill
JSON Path: $.rows[10].skill
Expected: SkillID / uint32
Found: string "Fireball"
```

### JSON Memory

- yyjson allocation can use the Engine Allocator.
- Large Parses may use a dedicated JSON Arena.
- The JSON DOM is not required to have the same lifetime as Runtime Data.
- After Parse → typed/compact representation, the JSON Arena may be released as a whole.
- Hot Loops must not directly query `JsonValue`.
- Avoid parsing JSON every Frame.

Typical:

```text
Asset Buffer
↓
JSON Arena
↓
JsonDocument
↓
Typed / Compact Runtime Data
↓
Destroy JsonDocument
↓
Release Entire JSON Arena
```

### JSON Threading

- `JsonDocument` ownership must be explicit.
- Do not assume that the same Mutable Document can be arbitrarily modified by multiple threads.
- Background Parse may run in the Job System.
- After Parse completes, publish immutable / typed data.
- Real-time / hot-path threads such as Audio / Render / Physics must not execute JSON parsing.

### JSON Usage Scope

Primarily:

```text
Engine Config
Project Settings
Editor Settings
Build Settings
Bundle Manifest
Remote Content Manifest
Asset Metadata
Localization Metadata
Data Table Runtime Asset
Save / Debug Data（depending on system requirements）
```

JSON is not the primary format for large GPU-ready / streaming payloads.

The following should still prioritize Binary Runtime Format:

```text
Mesh
Animation
Skeleton
Texture Runtime Payload
Terrain Chunk
Vegetation Chunk
Navigation Data
Large Scene Runtime Data
GPU-ready Resource Data
```

Format division:

```text
Human-readable Config / Data Table
→ JSON

Runtime-heavy / Streaming / GPU-ready Asset
→ Binary
```

Data Table JSON is a Runtime JSON Asset explicitly permitted in V1; after loading, it is converted into typed Runtime DataTable, and the Gameplay hot loop does not directly operate on the JSON DOM.

Third-party:

```text
yyjson
→ ThirdParty/LICENSES
→ Version Locked
→ Source / License / Redistribution Notice
```


## Data Table Framework

### Positioning

A Data Table is defined as:

```text
Game Design / Configuration Data
```

For example:

```text
Character
Skill
Item
Monster
Stage
Quest
Drop Table
Shop
Level Curve
Difficulty
Game Config
Platform Profile
Gameplay Balance
```

A Data Table does not represent:

```text
Runtime Mutable State
Save State
Entity / Component Runtime State
```

For example:

```text
Current HP
Current Inventory
Quest Progress
Current Buff
Transform
Runtime AI State
```

These should exist in Runtime Component / Gameplay State / Save System.

Formal principles:

```text
DataTable
→ Configuration Source

Runtime Component / Gameplay State
→ Simulation Source
```

### Authoring / Runtime Pipeline

The V1 official Runtime DataTable Asset uses JSON.

```text
                    Authoring
         ┌─────────────┼─────────────┐
         │             │             │
        JSON          CSV           XLSX
         │             │             │
         └─────────────┼─────────────┘
                       ↓
              Canonical Table Data
                       ↓
                 JSON Generator
                       ↓
                  *.json Asset
                       ↓
              Asset / Bundle System
                       ↓
               Engine JSON Parser
                       ↓
               Schema Validation
                       ↓
             Reference Validation
                       ↓
                Typed Table Build
                       ↓
          DataTable Preprocess Pipeline
                       ↓
                    Finalize
                       ↓
             Immutable Runtime Table
                       ↓
               DataTableRegistry
                       ↓
          ┌────────────┴────────────┐
          ↓                         ↓
        C++                       Zig
     Typed API                Stable C ABI
```

CSV / XLSX are Authoring / Import Formats; Shipping Runtime is not required to include a CSV / Excel parser.

Excel recommendation:

```text
1 Sheet
→ 1 Logical DataTable
```

For example:

```text
GameData.xlsx
├─ Character
├─ Skill
├─ Item
└─ Stage
```

Import:

```text
XLSX / CSV
↓
Canonical Table Data
↓
Engine JsonWriter
↓
Character.json / Skill.json / Item.json / Stage.json
```

Excel Formulas output only evaluated results; Runtime does not include an Excel Formula Engine.

The Importer must check:

```text
#REF!
#VALUE!
#DIV/0!
```

### Data Table JSON Format

Example:

```json
{
  "table": "Character",
  "schemaVersion": 1,
  "contentVersion": 17,
  "rows": [
    {
      "id": 10001,
      "key": "character.warrior",
      "name": "LOC_CHARACTER_WARRIOR",
      "maxHP": 1000,
      "attack": 120,
      "skill": 20001
    }
  ]
}
```

Runtime principles:

```text
JSON
→ Parse Once
→ Typed Table Build
→ Preprocess
→ Immutable Runtime Containers
```

Gameplay must not use:

```text
json["rows"][...]["attack"]
```

as the normal Runtime access path.

### Schema = Source of Truth

JSON only describes primitive JSON types; the DataTable Schema defines the actual Engine Type / Constraint.

```text
DataTableSchema
├─ TableID
├─ SchemaVersion
├─ PrimaryKey
├─ Columns[]
├─ SecondaryIndices[]
├─ Preprocess[]
├─ Validators[]
└─ StrictFields
```

A Column may describe:

```text
Name
Type
Required
Default
Min / Max
Enum
TableRowRef
AssetRef
LocalizationKey
Attributes
```

V1 basic types:

```text
bool
int32
uint32
int64
uint64
float
double
String
StringID
Enum
Vec2
Vec3
Vec4
AssetRef
TableRowRef
LocalizationKey
Array<T>
```

`Variant / Any / arbitrary Map<string, Any>` is not the default data modeling approach in V1, to prevent the Data Table from evolving into a second scripting language.

### Primary Key System

Int Keys are not enforced globally.

```text
DataTablePrimaryKeyType
├─ UInt32
├─ UInt64
└─ String
```

The Primary Key Type of each Table is determined by its Schema.

Typical:

```text
Persistent Gameplay Content Identity
→ Usually UInt32 Strong ID

Named Configuration / Profile / Rule
→ Usually String Primary Key

Both required
→ UInt32 Primary ID + String Unique Secondary Key
```

For example:

```text
CharacterTable
→ UInt32

SkillTable
→ UInt32

ItemTable
→ UInt32

GameConfigTable
→ String

DifficultyTable
→ String

GraphicsProfileTable
→ String
```

### Numeric Strong ID

Numeric Content IDs are not represented as bare `uint32_t` for every domain in C++ Gameplay.

```cpp
struct CharacterID
{
    uint32_t value;
};

struct SkillID
{
    uint32_t value;
};
```

Therefore:

```text
CharacterID
≠
SkillID
```

Even if their underlying representations are identical.

The default may be:

```text
0 = Invalid
1..N = Valid
```

`uint64` is used only when a larger identity space / distributed identity contract is genuinely required.

### String Primary Key

String Keys are suitable for:

```text
PlayerMoveSpeed
Normal / Hard / Nightmare
Graphics.High
Windows / Android / iOS
Gameplay / MainMenu
Forest / Snow
```

Default String Key Contract:

```text
UTF-8
Exact Match
Case Sensitive
```

Automatic, undeclared:

```text
tolower
trim
locale-dependent normalization
```

is prohibited.

If a Table requires normalization, it must be explicitly declared by the Schema.

Technical Keys are recommended to use:

```text
character.warrior
skill.fireball
achievement.first_boss
graphics.high
```

Player-facing text still uses Localization; localized strings are not used as content identities.

### Persistent String Identity / Rename

If a String Primary Key itself has already been used by Save / Server / Content references:

```text
Rename Key
= Breaking Data Change
```

When necessary, support:

```text
Alias / Migration
```

For example:

```json
{
  "key": "achievement.kill_first_boss",
  "aliases": [
    "achievement.first_boss"
  ]
}
```

Alias migration must be an explicit policy; renames must not be guessed automatically.

### Runtime String Optimization

Logical String Keys may use:

```text
String Pool
+
Hash Index
+
Collision-safe String Verification
```

or runtime interning:

```text
String
↓
Runtime StringID
```

However:

```text
Runtime StringID
≠
Persistent Identity
```

Unless there is an additional stable serialization contract, Runtime StringIDs must not be written directly to Save / Network / Persistent Storage.

### Primary ID + String Secondary Key

For large Content Tables, the following is recommended:

```json
{
  "id": 10001,
  "key": "character.warrior"
}
```

Responsibilities:

```text
10001
→ Persistent Runtime Identity

character.warrior
→ Editor / Debug / Search / Script-friendly Name
```

Both may be provided:

```text
Find(CharacterID{10001})
FindByKey("character.warrior")
```

### Row Identity

The formal identity is always the Primary Key, not:

```text
JSON Array Index
Excel Row Number
Runtime RowIndex
Current Sort Order
```

Reordering rows does not change content identity.

Duplicate Primary Keys must:

```text
Hard Fail
```

First Wins / Last Wins is prohibited.

### Secondary Index

The Schema may declare:

```text
SecondaryIndex
├─ Numeric
├─ String
├─ Enum
└─ Composite
```

For example, ItemTable:

```text
Primary
→ ItemID

Secondary
├─ key unique
├─ category
├─ rarity
└─ price sorted
```

Duplicate Secondary Unique Keys are also Validation Errors.

### Cross Table Reference

The Schema supports:

```text
TableRowRef<TargetTable>
```

For example:

```text
Character.skill
→ SkillID
→ SkillTable
```

Validation:

```text
Referenced Row exists?
├─ Yes
└─ No → Error
```

String Key Tables may also use:

```text
difficulty
→ TableRowRef<DifficultyTable, String>
```

Cross-table data errors must not wait until Gameplay Runtime to be discovered.

### Asset Reference

DataTable supports:

```text
AssetRef<Prefab>
AssetRef<Texture>
AssetRef<AudioEvent>
...
```

Runtime identity uses the Asset UUID / Asset Handle contract, not the source path as the persistent identity.

```text
DataTable
↓
Asset UUID
↓
Asset Dependency Graph
```

Referenced Asset missing → Validation / Build Fail.

### Localization Reference

Player-facing text uses:

```text
LocalizationKey / LocalizationStringID
```

For example:

```json
{
  "name": "LOC_CHARACTER_WARRIOR"
}
```

Flow:

```text
DataTable
↓
LocalizationKey
↓
Localization System
↓
ZH-TW / ZH-CN / JA / EN
```

### Strict Field Validation

Data Tables default to:

```text
StrictFields = true
```

Unknown Field:

```text
moveSpeeed
→ Error
```

Missing Field:

```text
Required = true
→ Error

Required = false + Default
→ Use Schema Default
```

Individual Loaders must not arbitrarily provide hidden defaults.

Duplicate JSON Object Keys in DataTable JSON should be treated as Errors, without relying on First / Last Wins.

### Typed Runtime Row

After JSON Parse / Validate, convert to a typed row.

For example:

```cpp
struct CharacterRow
{
    CharacterID id;
    StringID key;
    LocalizationStringID name;
    AssetID prefab;
    float maxHP;
    float attack;
    SkillID skill;
};
```

Generic API:

```text
Editor
Inspector
Debug
Tooling
```

Typed Generated API:

```text
C++ Gameplay
Zig Gameplay
Runtime Systems
```

Gameplay must not use string column name hot lookup:

```text
row.GetFloat("Attack")
```

as the primary access path.

### DataTable-specific Schema Codegen

The canonical DataTable Schema can generate:

```text
C++ Row Struct
C ABI Struct / View
Zig Type / Wrapper
JSON Reader
JSON Writer
Editor Metadata
Validation Metadata
```This is a narrow-scope Schema Codegen dedicated to DataTable; it does not mean that V1 has completed general-purpose Reflection Codegen for the entire Engine.

C++ / Zig / Editor must not each maintain mutually independent Row Layout definitions.

### DataTable Preprocess Pipeline

After Data Table loading, user-defined / schema-defined preprocessing is officially supported.

Execution timing:

```text
JSON Parse
↓
Schema Validation
↓
Typed Table Build
↓
DataTable Preprocess Pipeline
↓
Finalize
↓
Immutable Runtime DataTable
↓
Publish
```

Preprocess occurs when:

```text
Typed Data has been created
but has not yet been Published to Gameplay
```

### Process Stage

V1 defines:

```text
DataTableProcessStage
├─ Normalize
├─ Resolve
├─ Derive
├─ Index
├─ Optimize
└─ Finalize
```

Typical order:

```text
Parse
↓
Validate
↓
Normalize
↓
Resolve
↓
Derive
↓
Index
↓
Optimize
↓
Finalize
↓
Publish
```

### Built-in Preprocess Capability

At minimum, support:

```text
Physical Sort
Sorted View
Primary Index
Secondary Index
Group Index
Filter / Partition
Reference Resolve
Derived Column / Derived Data
Weighted Table
Range Table
Tag Index
Search Index
Custom Processor
```

### Physical Sort vs Sorted View

Sort is divided into:

```text
PhysicalSort
SortedView
```

`PhysicalSort`:

```text
Directly reorder canonical Rows
```

Suitable for Tables that genuinely require sequential iteration / range query / cache locality.

`SortedView`:

```text
Canonical Rows remain unchanged
↓
Create a RowIndex[] View
```

For example:

```text
Rows
Row0: Level 30
Row1: Level 10
Row2: Level 20

ByLevel
[1, 2, 0]
```

V1 gives `SortedView` priority by default because the same Table can simultaneously contain:

```text
ByLevel
ByAttack
ByPrice
ByName
```

without duplicating Row data.

### Group Index

For example:

```text
MonsterTable
↓
ByType
├─ Normal[]
├─ Elite[]
└─ Boss[]
```

Or:

```text
ByBiome
├─ Forest[]
├─ Snow[]
└─ Cave[]
```

Groups store RowIndex / ArrayRef and do not duplicate complete Rows.

### Derived Data

Preprocess can create data derived purely from configuration:

```text
BaseDamage + AttackSpeed
→ DPS
```

However, it is prohibited to mix the following into DataTable Preprocess:

```text
Current Buff
Current Player Level
Current Equipment
Runtime State
```

Formally:

```text
Preprocess
= Derived Configuration Data

not
= Runtime Gameplay State
```

### Weighted Table

Drop / Spawn / Encounter and similar tables can be built at Load time:

```text
Weight
↓
Cumulative Weight / Alias Table
```

This avoids rebuilding the sampling structure at every Runtime call.

### Range Table

For example:

```text
EXP → Level
Score → Reward Tier
Distance → Config Tier
```

Can be preprocessed into:

```text
Sorted Range
+
Binary Search Index
```

### Reference Resolve

Preprocess can resolve Stable IDs into:

```text
Generation-aware RowIndex / Handle
```

It is not recommended to write permanent raw C++ pointers across Tables into Runtime Rows, to avoid compromising Hot Reload / Generation Safety.

### Declarative Preprocess

Common Preprocess operations can be declared directly by the Schema:

```json
{
  "preprocess": [
    {
      "type": "index",
      "field": "id",
      "unique": true
    },
    {
      "type": "sortView",
      "name": "ByLevel",
      "field": "level",
      "order": "ascending"
    },
    {
      "type": "group",
      "name": "ByType",
      "field": "type"
    }
  ]
}
```

Formal principles:

```text
Common Processing
→ Schema Declarative

Game-specific Processing
→ Custom Processor
```

### Custom Processor

Provide the conceptual interfaces:

```text
IDataTableProcessor
DataTableBuildContext
DataTableProcessResult
```

Custom Processors can be used for:

```text
MonsterTableProcessor
DropTableProcessor
StageTableProcessor
EconomyTableProcessor
```

Processors operate only on build-time / loading-time builder data and do not directly mutate a Runtime Table that has already been Published.

### Processor DAG

Processor order depends on more than registration order.

```text
Build Skill Index
↓
Resolve Character Skill Reference
↓
Build Character Secondary Index
```

Use:

```text
DataTable Processor DAG
```

Cycle:

```text
A → B → C → A
→ Hard Fail
```

### Processor Determinism / Version

Mandatory requirements:

```text
Same JSON
+
Same Schema
+
Same Processor Version
↓
Same Runtime Data
```

Processors must not depend on:

```text
Current Time
Random Device
Thread Scheduling
Machine Locale
Unspecified Hash Iteration Order
```

If sort keys are identical, a stable tie-breaker is required, for example:

```text
Level ASC
then PrimaryKey ASC
```

The build key must include at least:

```text
Source Hash
Schema Version
Processor ID
Processor Version
```

When the Processor algorithm changes, the related DataTable cache / derived data must be invalidated.

### Preprocess Memory / Job System

Preprocess may use:

```text
Build Arena
Scratch Allocator
```

Finalize:

```text
Compact Persistent Runtime Data
↓
Release Build Arena
```

Different Tables without dependencies can be preprocessed in parallel; when dependencies exist, scheduling follows the Processor / Table DAG.

### Runtime Container Model

After Data Table loading is complete, it is stored in a Runtime Container rather than retaining the JSON DOM as the primary data source.

Formal model:

```text
DataTableRuntime
=
Contiguous Rows
+
Indices
+
Views
+
Pools
+
Derived Data
```

One Table:

```text
CharacterTableRuntime
│
├─ Rows[]
│
├─ PrimaryIndex
│  └─ PrimaryKey → RowIndex
│
├─ SecondaryIndices
│  ├─ StringKey → RowIndex
│  ├─ Type → RowIndex[]
│  └─ Rarity → RowIndex[]
│
├─ SortedViews
│  ├─ ByLevel[]
│  └─ ByAttack[]
│
├─ GroupIndices
│
├─ StringPool[]
├─ ArrayPool[]
└─ DerivedData[]
```

### Canonical Rows

V1 Row storage defaults to:

```text
Contiguous RowMajor / AoS
```

Internally, it may conceptually use:

```text
EngineArray<Row>
```

or use `std::vector<Row>` in the V1 private implementation.

The Public API / DLL ABI / Zig ABI does not expose STL container types.

### Primary Index

Rows themselves do not need to be duplicated into a Hash Map.

```text
PrimaryKey
↓
PrimaryIndex
↓
RowIndex
↓
Rows[RowIndex]
```

For example:

```text
10001 → 0
10002 → 1
10003 → 2
```

The Hash / Dense Index strategy can be selected according to Key distribution / Table profile, but the public contract does not bind to a specific STL container.

### String Index

String Primary / Secondary Keys:

```text
Hash Table
+
StringPool
+
Collision-safe compare
```

Each Row is not required to hold an independent `std::string` allocation.

### Sorted View Storage

A Sorted View stores only:

```text
RowIndex[]
```

For example:

```text
ByLevel = [1, 2, 0]
```

It does not duplicate complete Rows.

### Group Storage

Groups are recommended to use:

```text
GroupKey
↓
ArrayRef<RowIndex>
↓
Shared RowIndex Pool
```

This avoids large numbers of small `vector` allocations for individual groups.

### Variable-length Field Storage

Array fields are not recommended to use `std::vector<T>` for each Row.

Use:

```cpp
struct ArrayRef
{
    uint32_t offset;
    uint32_t count;
};
```

For example:

```cpp
struct CharacterRow
{
    CharacterID id;
    ArrayRef skills;
};
```

The data is stored in:

```text
Shared ArrayPool
```

### String Pool

Variable Strings can use:

```text
StringPool
+
StringRef / StringID
```

This avoids one heap `std::string` per Row.

### Future Packed Memory Block

V1 does not require a completely single allocation from the beginning, but the Runtime Layout should allow later consolidation into:

```text
RuntimeDataTable Memory Block

Header
Rows
Primary Index
Secondary Indices
Sorted Views
Group / RowIndex Pool
Array Pool
String Pool
Derived Data
```

Benefits:

```text
Load
Unload
Generation Swap
Memory Tracking
Cache Locality
```

### DataTableRegistry

Runtime Registry:

```text
DataTableRegistry
├─ CharacterTable
├─ SkillTable
├─ ItemTable
├─ StageTable
├─ GameConfigTable
└─ ...
```

Gameplay does not directly operate on JSON Assets; it queries through the typed Table API / Registry.

### Runtime Immutable

After Finalize / Publish:

```text
DataTable
→ Immutable
```

Prohibited:

```text
characterTable[10001].attack += 50
```

Runtime modifiers should exist in Component / Gameplay State.

### Hot-loop Policy

DataTable is not used as high-frequency simulation storage.

Not recommended:

```text
10000 Characters
×
Every Frame DataTable Lookup
```

Recommended:

```text
Spawn / Initialize
↓
Resolve DataTable Row
↓
Copy Required Runtime Config
↓
Component SoA
↓
Simulation Hot Loop
```

DataTable is mainly used for:

```text
Initialization
Configuration
Rules
Occasional Lookup
```

### Zig Stable C ABI

Zig does not directly see:

```text
JsonDocument
JsonValue
C++ Template
std::vector
std::unordered_map
```

Formally:

```text
DataTableRegistry
↓
Stable C ABI
↓
Generated Zig Typed Wrapper
```

For large numbers of lookups, use a batch API:

```text
ID[]
↓
ResolveRowsBatch
↓
Typed Row View[]
```

For frequent String Key lookups:

```text
Resolve String Key Once
↓
DataTableKeyHandle / Runtime StringID
↓
Repeated Lookup
```

This avoids passing `const char*` across the ABI and rehashing / comparing it each time.

### Reference Graph Semantics

Formally distinguish:

```text
Build Dependency
≠
Logical Row Reference
```

For example:

```text
Character → Skill
Skill → Character
```

A Logical Row reference cycle is not necessarily illegal.

Only when an actual Asset Build / Load Dependency cycle is created should it Hard Fail according to the Asset / Bundle DAG rules.

### Hot Reload / Generation

Data Table JSON is standard Asset / Bundle content and reuses the existing:

```text
UUID
Version
Hash
Bundle
Remote Update
Generation Pinning
Atomic Activation
Rollback
```

Hot Reload:

```text
Character.json Changed
↓
Parse
↓
Schema Validation
↓
Reference Validation
↓
Typed Table Build
↓
Preprocess Pipeline
↓
Finalize
↓
Success?
├─ No
│  ↓
│ Keep Generation N
│
└─ Yes
   ↓
   Publish Generation N+1
```

It is prohibited to directly mutate Generation N containers currently being read by the Runtime.

```text
Build completely new DataTable Generation
↓
Validate
↓
Safe / Atomic Publish
```

New lookups:

```text
→ N+1
```

Old pinned views:

```text
→ N
```

N is reclaimed only after the relevant pins / read contexts reach zero.

Long-term retention of raw row pointers by Gameplay is still discouraged; when necessary configuration can be copied during initialization, copying is preferred.

### SchemaVersion / ContentVersion

Separate:

```text
SchemaVersion
ContentVersion
```

When the structure / field contract changes:

```text
SchemaVersion++
```

For purely numerical / content adjustments:

```text
ContentVersion++
```

### DataTable Residency

V1 does not establish an independent `DataTableResidencyScope`.

DataTable JSON / Runtime Table directly uses the existing:

```text
Asset Residency
Bundle Residency
Generation Pinning
```

Audio has `AudioResidencyScope` because Voice / Streaming / Fade have special lifetimes; DataTable currently does not need to duplicate the same model.

### Custom Validation

Support:

```text
IDataTableValidator
```

For example:

```text
SkillValidator
StageValidator
DropTableValidator
EconomyValidator
```

Can validate:

```text
Cooldown > 0
Drop Probability valid
Boss ID exists
Shop Item exists
Required Asset exists
Localization Key exists
```

Directly integrate with CI / Asset Gate.

### DataTable Profiler / Debug

Track at least:

```text
Loaded Tables
Loaded Generations
Rows / Table
Table Memory
JSON Parse Time
Validation Time
Typed Build Time
Preprocess Time
Lookup Count
Lookup Miss
Batch Lookup Count
String Index Lookup
Reload Count
Old Generation Pin Count
Primary / Secondary Index Memory
Sorted View Memory
Derived Data Memory
String Pool Memory
Array Pool Memory
```

Development Mode can detect high-frequency repeated Table lookups and prompt copying necessary settings into Runtime Components.

### DataTable CI / Gate

At minimum:

```text
JSON Parse / Generate
UTF-8 Validation
Deterministic JSON Write
Schema Validation
UInt32 Primary Key
UInt64 Primary Key
String Primary Key
Duplicate Primary Key Fail
Unique Secondary Key Fail
Unknown Field Fail
Missing Required Field Fail
Number Overflow / Type Fail
Cross Table Reference Validation
Asset UUID Reference Validation
Localization Reference Validation
Sorted View
Group Index
Weighted Table
Range Table
Custom Processor
Processor DAG Cycle Fail
Processor Determinism
Processor Version Cache Invalidation
Hot Reload Failure Keeps Old Generation
Generation Pinning
Runtime Container Index Correctness
C++ / C ABI / Zig Generated Layout Validation
No yyjson Backend Type Leak
No STL Container Across Stable ABI
```

### DataTable V1 Definition of Done

V1 must complete at least:

```text
Engine JSON Parse / Generate Framework
+
JSON Runtime DataTable Asset
+
Schema Validation
+
UInt32 / UInt64 / String Primary Key
+
Secondary Index
+
Typed Runtime Row
+
Contiguous Rows
+
Primary / Secondary Runtime Indices
+
String Pool / Array Pool
+
Sorted View / Group Index
+
Preprocess Pipeline
+
Declarative Preprocess
+
Custom Processor
+
Immutable Runtime Table
+
DataTableRegistry
+
Hot Reload / Generation Swap
+
Asset / Bundle Integration
+
C++ / Zig Typed API
+
Profiler / CI
```

Future optional optimization:

```text
JSON
↓
Binary Table Cook
```

It should be introduced only when profiling proves that JSON load / parse / memory for large Tables has become an actual bottleneck; it is not a prerequisite for V1.

## Twenty-Two, Asset Pipeline

Three layers:

Assets/
- Original art assets

Library/
- Import Cache
- Engine Runtime Format

GameData/
- Bundles after Build

Process:

```text
Source Asset
↓
Importer
↓
UUID
```
```text
Engine Runtime Asset
↓
```
```text
Platform Compression
↓
```
Asset Bundle


## Twenty-Three, Asset UUID

Every Asset has a UUID.

For example:

Character.fbx
Character.fbx.meta

meta:

uuid: ...
type: Model

References use UUIDs, not paths.

Advantages:
- Moving assets does not break Scenes / Prefabs / Materials
- Dependencies are easy to track
- Hot Updates are easy to manage


## Twenty-Four, Asset Database

Stores:
- UUID
- Path
- Type
- Dependencies
- Import Settings
- Hash
- Generated Artifacts

Supports:
- Incremental Import
- Dependency Tracking
- Reimport
- Cache
- Thumbnail
- Search
- Asset Reference


## Twenty-Five, FBX / Model Assets

Sources:
- FBX
- glTF
- GLB
- OBJ

FBX rules:
- FBX is an Editor Import Format only
- Runtime does not include the FBX SDK
- Android / iOS Shipping Runtime does not parse FBX
- Everything is converted to Engine Runtime Format during Build

Process:

Windows/macOS Editor

```text
Hero.fbx
↓
```
```text
FBX Importer
↓
```
```text
Intermediate Scene
↓
```
```text
Engine Asset Compiler
↓
Hero.mesh
Hero.skeleton
Idle.anim
Run.anim
Attack.anim
```

Runtime reads only:
- .mesh
- .skeleton
- .anim

Benefits:
- Reduced Binary Size
- Reduced Runtime Dependency
- Reduced licensing and deployment complexity
- Improved loading speed


## Twenty-Six, Texture Pipeline

Sources:
- PNG
- JPG
- TGA
- HDR
- EXR

Runtime Compression:

Windows:
- BC7
- BC5
- BC1 / BC3 as needed

Android:
- ASTC

macOS:
- ASTC / BC, according to hardware strategy

iOS:
- ASTC

Import Settings:
- Texture Type
- sRGB
- Mipmap
- Normal Map
- Max Size
- Compression
- Platform Override


## Twenty-Seven, Asset Bundle

After Build:

base.bundle
ui.bundle
character.bundle
map01.bundle
audio.bundle

Functions:
- Compression
- Async Loading
- Streaming
- Dependency
- Patch
- Hot Update
- DLC / Remote Asset (later)



### Streaming Bundle Types

- Terrain Chunk Bundle
- Vegetation Cluster Bundle
- Large Map Cell Bundle

These Bundles share UUID, Dependency Graph, Compression, Async IO, and Asset Registry with ordinary Bundles; the only difference is the Load Policy: ordinary Bundles are oriented toward feature / level scope, while Streaming Bundles are oriented toward spatial location / Residency scope.


### Bundle Dependency DAG Gate

The Bundle dependency graph must be a DAG (Directed Acyclic Graph).

Build / Asset Packaging stage:

```text
Collect Bundle Dependencies
↓
Build Directed Graph
↓
Topological Sort
↓
Cycle Detection
```

If the following exists:

```text
A.bundle → B.bundle
B.bundle → A.bundle
```

Or a longer cycle:

```text
A → B → C → A
```

It must result in:

```text
Build Fail
```

And output the complete cycle path, for example:

```text
Bundle dependency cycle:
characters.bundle
→ shared_fx.bundle
→ common_materials.bundle
→ characters.bundle
```

Runtime must not attempt to “tolerate” circular dependencies through lazy load / reference count, to avoid:

- Streaming unload failing to converge
- Bundle lifetime reference cycles
- Hot Update activation deadlocks
- Memory failing to be released
- Uncertain load order

Asset Gate / CI must include:

```text
Bundle Dependency DAG Check
```


## Bundle Hot Update / Remote Content

The Bundle System must natively support Asset / Data hot updates.

Core model:

```text
Built-in Bundle
+
Downloaded Cache Bundle
+
Remote Manifest
```

Runtime Asset resolution priority:

```text
Downloaded Cache
↓
Built-in Bundle
```

Therefore, when the App contains an older built-in Bundle, as long as a verified newer version exists in the Cache, the Runtime uses the Cache version.

Example:

```text
Built-in:
ui.bundle v1

Cache:
ui.bundle v3

Runtime:
→ use ui.bundle v3
```

### Remote Manifest

Each Content Build generates a version Manifest:

```json
{
  "contentVersion": "1.2.5",
  "bundles": {
    "map01": {
      "version": 7,
      "hash": "...",
      "size": 125829120,
      "dependencies": [
        "shared_environment"
      ]
    }
  }
}
```

The Manifest must contain at least:

- Bundle ID
- Bundle Version
- Content Hash
- File Size
- Dependencies
- Optional / Required
- Minimum App Version (when required)
- Platform / Quality / Compression Variant (when required)

Update process:

```text
Download Remote Manifest
↓
Compare Local Manifest
↓
Check Version / Hash
↓
Determine Required Bundles
↓
Download Temporary File
↓
Verify Hash
↓
Verify Bundle Header / Version
↓
Atomic Commit
↓
Activate New Bundle
```

### Download and Atomic Replace

It is prohibited to directly overwrite the formal Bundle currently in use during download.

Process:

```text
map01.bundle.tmp
↓
Download Complete
↓
Hash Verification
↓
Bundle Validation
↓
Check Target Bundle State
↓
Atomic Activate / Commit
```

If the target Bundle is still in any of the following states:

```text
Loaded
Open File Descriptor
Memory-mapped
Streaming Read In-flight
Asset Decode In-flight
```

It must not be directly replaced in place.

BundleManager must first:

```text
Stop New Loads
↓
Wait / Cancel In-flight IO
↓
Unload Bundle Index
↓
Release File Handle
↓
Unmap Memory
↓
Verify No Active Mapping / Descriptor
```

Only afterward may the new version be Activated.

A conservative, consistent cross-platform strategy is adopted:

```text
Can Safely Activate Now
→ Atomic Rename / Version Switch

Cannot Safely Activate Now
→ Mark Pending Update
→ Keep Current Bundle Active
→ Activate During Next App Pre-init
```

The formal strategy prioritizes versioned cache to avoid same-name in-place replacement of an active Bundle:

```text
Cache/map01/v6/map01.bundle
Cache/map01/v7/map01.bundle
```

An old version is marked `SafeToDelete` only after:

```text
No Open Handle
No mmap
No In-flight IO
No Active Asset Dependency
```

Actual deletion may be deferred to background cleanup or the next App Pre-init.

The Active Manifest is switched only after the new version has been fully verified and can be safely activated:

```text
ActiveVersion: v6
↓
Validate v7
↓
Switch Manifest Pointer
↓
ActiveVersion: v7
```This avoids differences in `open` / mapped file rename/delete semantics across different OSes from affecting Runtime correctness.

If downloading or validation fails:

```text
.tmp / candidate version
→ Delete / Resume Later

Existing Valid Bundle
→ Remain unchanged
```

Avoid update failures or asset corruption caused by network interruptions, insufficient disk space, open handles, or unreleased mmap resources.

### Rollback

Bundle Cache must support a rollback strategy with at least one usable previous version.

```text
Current Bundle
↓
Update Candidate
↓
Validation Failed
↓
Rollback
↓
Previous Valid Bundle
```

Possible approaches:

```text
map01.bundle
map01.bundle.prev
```

Or version directories:

```text
Cache/
└─ map01/
   ├─ v6/
   └─ v7/
```

Only update the Active Manifest after Bundle activation is complete.

### Hot Update Scope

Bundles can update:

- Texture
- Mesh
- Material
- Shader Data
- Animation
- UI Prefab
- Scene Data
- Localization
- Audio
- Terrain Chunk
- Vegetation Chunk
- JSON Config
- Binary Runtime Data

Bundles are not directly used as a cross-platform hot-update mechanism for Native C++ Executables / Libraries.

```text
Native Engine / Native Gameplay Code
→ App Update

Asset / Data
→ Bundle Hot Update
```

If Gameplay Logic hot updates are required in the future, the Script / VM layer should be evaluated separately rather than relying on downloaded Native C++ Code.

### Full Bundle Update

V1:

```text
Bundle Hash Changed
↓
Download Full New Bundle
```

Advantages:

- Simple architecture
- Easy validation
- Clear Rollback
- Easier Build Pipeline stabilization
- Cross-platform consistency

V1 does not require Binary Delta Patch implementation.

### Delta Patch

Later versions may add:

```text
Old Bundle
+
Patch
↓
New Bundle
```

Suitable for:

- Large maps
- Large amounts of Audio
- Terrain / Vegetation Data
- Long-term live-service games

However, the Patch must validate:

- Source Bundle Hash
- Patch Hash
- Result Bundle Hash

If any item does not match:

```text
Fallback
→ Full Bundle Download
```

### Bundle Download Cache

Downloaded Bundles are stored in a platform-writable directory.

The Cache Manager manages:

- Current Cache Size
- Cache Budget
- LRU
- Last Used
- Bundle Version
- Bundle Hash
- Pin / Required Bundle
- Download-in-progress
- Previous Valid Version

Downloaded Bundles:

```text
Download
→ Disk Cache
```

Does not mean:

```text
→ RAM
→ GPU Memory
```

The three layers remain separate:

```text
DownloadBundle
→ Network → Disk

LoadBundle
→ Bundle Metadata / Index

LoadAsset
→ Disk → RAM / GPU
```

### Streaming and Hot Update

Streaming Bundles can share the same Manifest / Cache system with hot updates.

```text
Player Position
↓
Streaming Cell Required
↓
Check Cache
├─ Exists + Valid
│  → Load
└─ Missing / Old
   → Download
   → Verify
   → Activate
   → Load
```

When leaving a Cell:

```text
Unload Asset
↓
RAM / GPU Release
↓
Bundle remains in Disk Cache
```

When entering again, if the Version / Hash has not changed, it does not need to be downloaded again.

### Content Compatibility

The Remote Manifest can configure:

```text
MinimumAppVersion
MaximumAppVersion（when required）
ContentSchemaVersion
```

Avoid allowing a new Bundle to use the following, which an older App does not recognize:

- Asset Format
- Shader Feature
- Serialization Schema
- Component Type
- Runtime Opcode / Script Data

If incompatible:

```text
Do Not Activate Content
→ Request App Update
```

### Security and Integrity

At minimum for V1:

- Cryptographic Hash Verification
- HTTPS Download
- Atomic File Commit
- Manifest Validation
- Bundle Header Validation
- Rollback

Later commercial live-service versions may evaluate:

- Manifest Signature
- Bundle Signature
- Encryption（for content protection only; not considered a complete anti-cracking solution）

### V1 Definition of Done

Bundle Hot Update V1 completion criteria:

- Build can generate a Remote Manifest
- Runtime can obtain the Remote Manifest
- Local / Remote Bundles can be compared
- Full Bundles can be downloaded
- Resume or safe re-download is supported
- Hash verification
- Atomic Commit
- Cache Bundle takes priority over Built-in
- Bundle Dependency is resolved correctly
- Update failure does not damage the existing version
- Rollback operates correctly
- Downloaded Bundles remain usable after restart
- Unchanged Bundles are not downloaded repeatedly
- Download / LoadBundle / LoadAsset behavior is completely separated
- The Bundle Dependency Graph must pass the DAG Check
- Active / mapped Bundles are not directly replaced in place
- New Bundles that cannot be safely activated can be deferred until the next App Pre-init
- The Active Manifest points only to versions that have completed validation and can be loaded safely

## Plugin System

The Engine reserves a formal Plugin System, but Plugins are not Remote Content.

Core rules:

```text
Plugin
→ Installed together with the Editor / App
or
→ Compiled / packaged together at Build time

Not through Remote Bundle
Not downloaded from CDN
Not used as Native Code Hot Update
```

Therefore:

```text
Bundle Hot Update
→ Asset / Data

Plugin System
→ Local Installed / Build-time Module
```

The two are completely separate.

### Plugin Types

Primarily divided into:

```text
Editor Plugin
Runtime Plugin
```

Editor Plugin priority support:

- Custom Inspector
- Asset Importer
- Asset Processor
- Build Step
- Editor Window
- Menu / Toolbar
- Scene Gizmo
- Profiler Panel
- Terrain Tool
- Animation Tool
- Third-party Content Tool
- Platform SDK Integration Tool

Runtime Plugin extensions:

- Physics Backend
- Audio Backend
- Rendering Feature
- File System
- Network Transport
- Gameplay Module
- Platform Integration
- Third-party Runtime SDK

### Editor Plugin

Editor Plugins may use Dynamic Modules.

Example:

```text
Editor/
└─ Plugins/
   ├─ TerrainTools
   ├─ SpineImporter
   └─ CustomProfiler
```

Windows:

```text
.dll
```

macOS:

```text
.dylib / framework
```

Editor startup flow:

```text
Editor Startup
↓
PluginManager
↓
Discover Local Plugins
↓
Validate Manifest
↓
Resolve Dependencies
↓
Load Module
↓
Initialize
```

### Runtime Plugin

Runtime Plugins use the following by default:

```text
Build-time selectable module
```

Rather than:

```text
Runtime remote downloaded module
```

Different platforms may use the following as required:

```text
Windows
→ DLL / Static Library

Android
→ .so / Static Library

macOS
→ dylib / Framework / Static Library

iOS
→ Static Library / Framework
→ Signed and submitted together with the App
```

Whether to use a Dynamic Module is determined by platform capabilities and deployment strategy.

### Plugin API Boundary

Plugins must not arbitrarily depend on Engine Private Headers.

Correct:

```text
Plugin
↓
Public Engine API
↓
Engine Core
```

Avoid:

```text
Plugin
→ Engine Private Internal Header
→ Backend-specific Internal State
```

Purposes:

- Reduce coupling between Plugins and Engine Core
- Improve Engine version upgrade stability
- Keep ABI / API boundaries clear
- Prevent third-party Plugins from breaking Internal Invariants
- Give AI Agents a fixed Contract when generating Plugins

### Plugin Interface

Editor Plugin:

```cpp
class IEditorPlugin
{
public:
    virtual ~IEditorPlugin() = default;

    virtual void OnLoad(EditorContext& context) = 0;
    virtual void OnUnload() = 0;
};
```

Runtime Plugin may use:

```cpp
class IRuntimePlugin
{
public:
    virtual ~IRuntimePlugin() = default;

    virtual void OnLoad(RuntimeContext& context) = 0;
    virtual void OnUnload() = 0;
};
```

The actual ABI boundary must avoid directly exposing unstable STL Container Layouts; across DLL / dylib Boundaries, a stable C ABI or explicit Engine ABI Contract should be used.

### Plugin Manifest

Each Plugin should include a descriptor file.

Example:

```json
{
  "name": "TerrainTools",
  "version": "1.0.0",
  "type": "Editor",
  "engineApi": 1,
  "entry": "TerrainTools",
  "dependencies": [],
  "platforms": [
    "Windows",
    "macOS"
  ]
}
```

At minimum, record:

- Plugin Name
- Plugin Version
- Plugin Type
- Engine API Version
- Entry Module
- Dependencies
- Supported Platforms
- Enabled / Disabled
- Optional Load Order（when required）

### PluginManager

Core responsibilities:

```text
PluginManager
├─ Discover
├─ Read Manifest
├─ Validate Engine API Version
├─ Validate Platform
├─ Resolve Dependencies
├─ Detect Dependency Cycle
├─ Load
├─ Initialize
├─ Shutdown
└─ Unload
```

The Editor should also support:

```text
Enable Plugin
Disable Plugin
Reload Plugin（when safe）
Show Dependency Error
Show API Version Mismatch
```

### Version Compatibility

Plugins must have clearly defined:

```text
Engine API Version
Plugin Version
Dependency Version
Platform Support
```

For example:

```json
{
  "engineApi": 2,
  "dependencies": [
    "RendererCore >= 1.3"
  ]
}
```

If incompatible:

```text
Do Not Load
↓
Editor / Runtime displays a clear error
```

Forced loading when the API is incompatible is prohibited.

### Plugin and Bundle

The following boundary must be maintained:

```text
Plugin
→ Code / Extension Module

Bundle
→ Asset / Data
```

Plugins may:

```text
Reference Bundles
Register Asset Importers
Add Asset Types
Add Build Processors
```

However, the Plugin itself must not be packaged inside a Remote Bundle for download and execution.

The Remote Manifest does not manage Native Plugins.

### Security / Deployment

Plugin sources:

```text
Engine Built-in
Project Local
Installed Third-party
```

Not supported:

```text
Remote CDN Native Plugin
Downloaded DLL
Downloaded dylib
Downloaded .so
```

Official Builds package only:

- Enabled Plugins
- Target Platform Supported Plugins
- Dependency Complete Plugins

Unused Plugins are not included in the final Runtime Package.

### V1 Scope

V1:

- Plugin Manifest
- PluginManager
- Editor Plugin
- Enable / Disable
- Dependency Resolution
- Engine API Version Validation
- Windows / macOS Editor Dynamic Module
- Runtime Build-time Plugin Module
- Plugin Build Integration

Later:

- Plugin Hot Reload（Editor only, if safe）
- Plugin SDK
- Plugin Template Generator
- Plugin Package Manager（Local / Installed only）
- Plugin Marketplace Integration（installation flow only; does not represent Remote Runtime Execution）

### Definition of Done

Plugin System V1 completion criteria:

- Editor can scan local Plugins
- Manifest can be parsed
- Plugins can be enabled / disabled
- Dependency Resolution is correct
- Dependency Cycles can be detected
- Loading is rejected when the Engine API Version does not match
- Editor Plugins can register at least one Editor Extension
- Runtime Plugins can be selected or excluded at Build time
- Plugins do not require Remote Bundles to start
- Plugins are not dynamically downloaded from CDN
- Plugin and Bundle Hot Update paths are completely separate




## Public API / Internal API / Backend Boundary

The Engine provides users with a stable Public API.

Core principle:

```text
Game / Plugin / Editor Extension
↓
Public Engine API
↓
Subsystem API
↓
Internal Implementation
↓
Platform / Backend
```

General Gameplay and general Plugins do not need to access low-level implementations.

### Public Engine API

The Public API includes at least:

```text
Core
Scene
Asset
Rendering
Audio
Input
UI
WebView
Physics
Animation
Localization
Save
Plugin
Platform Services
```

For example:

```cpp
auto texture = Assets::Load<Texture>(assetId);
Audio::Play(eventId);
Jobs::Dispatch(...);
FileSystem::ReadAsync(...);
Time::DeltaTime();
Platform::OpenURL(url);
```

Users do not need to know about:

```text
Bundle Offset
File IO Backend
Decompression
GPU Upload
Descriptor Allocation
Resource State Transition
Native Audio Object
Platform WebView Object
Thread Scheduler Implementation
```

### Internal API

The Internal API is only for use by Engine Modules:

```text
Entity Pool
Resource Registry
Render Graph Internals
Job Scheduler Internals
Allocator Internals
Bundle Reader
Streaming Scheduler
Asset Cooker
Shader Compiler Pipeline
```

### Backend API

The Backend exists only in platform / RHI implementations:

```text
DX12
Vulkan
Metal
Android JNI
Objective-C++
OS Native API
```

The following Native Types must not be exposed to Gameplay:

```text
ID3D12*
Vk*
MTL*
JNIEnv*
WKWebView*
FMOD Native Types
```

### Header / Folder Boundary

Recommended:

```text
Engine/
├─ Public/
│  ├─ Core/
│  ├─ Scene/
│  ├─ Asset/
│  ├─ Render/
│  ├─ Audio/
│  ├─ UI/
│  └─ ...
│
├─ Internal/
│  ├─ Resource/
│  ├─ RenderGraph/
│  ├─ Streaming/
│  └─ ...
│
└─ Backends/
   ├─ DX12/
   ├─ Vulkan/
   ├─ Metal/
   └─ Platform/
```

Rules:

- Game Code may only include `Engine/Public`
- General Plugins may depend only on Public Plugin/API Contracts
- Internal Headers must not be installed into the Game SDK include path
- Backend Headers must not be included by Public Headers
- Public APIs should not depend on Backend Native Types
- Public APIs should remain as stable as possible; Internals may be freely refactored

## Feature / Module-based Build and Runtime Stripping

The Engine adopts a modular Build architecture and treats Optional Subsystem Stripping as a formal Architecture Contract:

```text
Engine Supports Feature
≠
Every Game Must Package Feature
```

Core principle:

```text
Engine Core
→ Always present

Subsystem / Plugin / Rendering Feature
→ May be selected by project and Build Profile for compilation, linking, and packaging
```

Purposes:

- Reduce Runtime Package Size
- Reduce Native Library Size
- Reduce the number of Shader Variants
- Reduce unnecessary Runtime Dependencies
- Reduce startup and initialization costs
- Allow 2D / lightweight projects to avoid carrying complete 3D Features
- Allow large 3D projects to enable complete functionality

### Feature State

Each Optional Feature uses three states:

```text
Auto
Enabled
Disabled
```

Semantics:

```text
Auto
→ Build Pipeline automatically determines based on Project / Scene / Prefab / Asset Dependency

Enabled
→ Force compilation / linking / packaging

Disabled
→ Force exclusion
→ If Build Scanner detects an actual dependency, Build Fail
```

This mode is safer than a simple Checkbox, avoiding incorrect stripping of dynamic usage or hidden dependencies.

### Compile-time Module Stripping

Features that can be completely excluded from the Runtime at compile time:

```text
Spine Plugin
FMOD Plugin
WebView
Terrain
Vegetation
Physics / Jolt
Audio
Localization
Save / Persistent
Networking Transport
Remote Bundle / Hot Update
Streaming
Navigation / Pathfinding
Particle / VFX
Skeletal Animation
GPU Skinning
Post Processing
Shadow System
Hi-Z / Occlusion
GPU Driven Rendering
Runtime Profiler
Debug Renderer
Third-party SDK Plugins
```

CMake example:

```cmake
if(ENGINE_ENABLE_SPINE)
    add_subdirectory(Plugins/Spine)
endif()

if(ENGINE_ENABLE_FMOD)
    add_subdirectory(Plugins/FMOD)
endif()

if(ENGINE_ENABLE_WEBVIEW)
    add_subdirectory(Runtime/WebView)
endif()

if(ENGINE_ENABLE_TERRAIN)
    add_subdirectory(Runtime/Terrain)
endif()
```

When completely Disabled:

```text
Do Not Compile
↓
Do Not Link
↓
Do Not Package
↓
No Runtime Initialization
```

### Build-time Dependency Stripping

Even if the Engine Source supports a Feature, Runtime Data can be stripped as long as the project has no actual dependency.

For example:

```text
Terrain System Available
+
Project Has No Terrain
↓
No Terrain Runtime Asset
No Terrain Shader Variant
No Terrain Material Data
```

Spine:

```text
Spine Plugin Installed
+
No Spine Asset
↓
Strip Spine Runtime
```

FMOD:

```text
FMOD Plugin Installed
+
Project Uses MiniAudio
↓
Do Not Package FMOD Runtime / Library / Bank
```

### Feature Usage Scanner

The Build Pipeline must provide:

```text
FeatureUsageScanner
```

Scan sources:

```text
Scene
Prefab
Asset Dependency
Component Type
Material
Shader Feature
Build Settings
Plugin Dependency
Serialized Runtime Reference
```

Example output:

```text
Detected Feature Usage

Spine             Not Used
FMOD              Not Used
Terrain           Used
Vegetation        Used
WebView           Not Used
SkeletalAnimation Used
Physics           Used
```

The Build Report must record for each Feature:

```text
State
Detected Usage
Included / Stripped
Reason
Package Size Contribution（when available）
```

### Dynamic Code Dependency

Auto Detect cannot assume that Static Asset Scan covers all situations.

For example:

```cpp
WebView::Create(...);
```

Or:

```cpp
PluginManager::Load(...);
```

May be triggered dynamically by Gameplay Code.

Therefore:

- Dynamic-only Features should be explicitly set to `Enabled`
- Code Generation / Reflection may provide Feature Dependency Metadata
- If a Feature is set to `Disabled` but a dependency is detected, Build must Fail
- Silent stripping followed by a Runtime crash is not allowed

### Build Settings

Recommended:

```text
Features
────────────────────────────

Rendering
Forward+             Enabled
Shadows              Auto
Post Processing      Auto
GPU Driven           Auto
Terrain              Auto
Vegetation           Auto
Hi-Z Occlusion       Auto

Animation
Skeletal Animation   Auto
Spine                Auto

Physics
Jolt Physics         Auto

Audio
Audio                Enabled
Backend              MiniAudio
FMOD                 Disabled

Platform Services
WebView              Auto
Networking           Auto
Localization         Auto

Content
Bundle System        Enabled
Remote Content       Auto
Asset Streaming      Auto

Diagnostics
Runtime Profiler     Disabled
Debug Renderer       Disabled

Optimization
Strip Unused Features        Enabled
Strip Unused Plugins         Enabled
Strip Unused Shader Variants Enabled
```

### Plugin Stripping

Optional Plugins are not included in the Runtime Package by default.

Only those that are:

```text
Enabled
+
Target Platform Compatible
+
Required
```

are added to the final Build.

For example:

```text
Spine Plugin Disabled
↓
No Spine Runtime
No Spine Native / Third-party Dependency
No Spine Asset Import Runtime
No Spine Shader Variant
```

Editor-only Plugins never enter the Game Runtime Package, unless they also have an independent Runtime Module and that Runtime Module is enabled.

### Rendering Feature Stripping

The Build Pipeline can remove the following based on Feature Usage:

```text
Shadow Pass
Post-process Pass
Skinning Pipeline
Terrain Pipeline
Vegetation Pipeline
GPU Driven Pipeline
Occlusion Pipeline
Unused Material Feature
Unused Shader Variant
```

Shader Variant Keys and Feature Stripping must be integrated.

For example:

```text
SkeletalAnimation = Disabled
↓
No Skinning Feature Bit
↓
No Skinning Shader Variants
```

```text
Shadows = Disabled
↓
No Shadow Pass
No Shadow Receiver Variant
No Shadow Caster Variant
```

### Build Profile

Different Build Profiles may use different Feature Sets.

For example:

```text
Windows Full
→ Terrain
→ Vegetation
→ WebView
→ High Quality Post
→ GPU Driven

Android Lite
→ Reduced Post
→ Optional Vegetation
→ No Runtime Profiler

Internal QA
→ Runtime Profiler
→ Debug Renderer
→ Validation Features

Release
→ Strip Debug / Development Features
```

Feature State belongs to:

```text
Project Default
+
Build Profile Override
```

### Core Modules

The following belong to the Engine Foundation and should not be arbitrarily stripped by general projects:

```text
Core Types
Memory / Allocator Foundation
Job System Foundation
File System
Logging / Error Foundation
Platform Foundation
Asset Runtime Foundation
Resource Manager
RHI Core
Render Graph Core
Basic Renderer Path
Scene / Entity Identity Foundation
Serialization Foundation
```

Implementations may be optimized for platforms, but general Feature Stripping must not break the core Contract.

### Diagnostics Stripping

Development / QA:

```text
Profiler
Validation Layer
Debug Renderer
Render Debug Views
Verbose Logging
Capture Metadata
```

Release can strip most Development-only functionality.

However:

```text
Crash Handling
Fatal Error Reporting
Essential Logging
```

The minimum Runtime support should still be retained.

### Package Report

Each Build should output the following:

```text
Build Feature Report

Included:
- Terrain
- Vegetation
- MiniAudio
- Localization

Stripped:
- Spine
- FMOD
- WebView
- Runtime Profiler

Shader Variants:
Before Strip: 18,420
After Strip:   4,315

Optional Module Size:
...
```Enable developers to understand “why a feature was included in the package” and “how much was actually saved by stripping.”

### CI Validation

CI must validate at least:

```text
Full Feature Build
Minimal Feature Build
Representative Mobile Build
Representative Desktop Build
```

Minimal Build is used to confirm that Optional Modules do not secretly form hard dependencies.

This matrix must be incorporated into the actual Pipeline of “45. Automated Testing / CI” in sync; it must not exist only in the Build System specification.

Execution levels:

```text
PR
→ Representative Profile + dependency failure checks

Nightly / Scheduled
→ Minimal Profile + Max / Full Feature Profile

Release
→ Shipping / Platform Feature Matrix
```

The following must also be tested:

```text
Feature = Disabled + Asset Dependency Exists
→ Build Must Fail

Plugin = Disabled + Runtime Reference Exists
→ Build Must Fail
```

### V1 Scope

V1 must complete:

- Feature State: Auto / Enabled / Disabled
- CMake Optional Modules
- Plugin Stripping
- Feature Usage Scanner
- Build Dependency Validation
- Shader Variant Stripping Integration
- Editor Build Settings UI
- Build Feature Report
- Minimal / Full Build CI

V1 does not need to make every Subsystem strippable, but architecturally all non-Core systems must avoid forming unnecessary hard dependencies.

### Definition of Done

Completion criteria:

- Unused Optional Plugins are not included in the Runtime Package
- Unused Optional Native Libraries are not linked
- Unused Features do not generate related Runtime Assets
- Unused Rendering Features do not generate related Shader Variants
- Build fails when a `Disabled` Feature is referenced
- `Auto` can scan major Scene / Prefab / Asset Dependencies
- Build Profiles can override Feature State
- Build Report can list Included / Stripped / Reason
- Minimal Build can compile and launch successfully
- Full Feature Build can compile and launch successfully


## Optional Runtime Subsystem Architecture

Except for Foundation Core, all High-level Subsystems must in principle be designed as Optional Runtime Modules.

Core rule:

```text
Subsystem Supported By Engine
≠
Subsystem Must Exist In Final Game Package
```

Unused Subsystems:

```text
Do Not Compile
↓
Do Not Link
↓
Do Not Initialize
↓
Do Not Package Native Library
↓
Do Not Cook Related Assets
↓
Do Not Generate Related Shader Variants
```

### Foundation Core

Foundation Core is the Runtime skeleton and is not arbitrarily strippable by ordinary projects:

```text
Core Types
Memory / Allocator Foundation
File System
Logging / Error Foundation
Job System Foundation
Platform Foundation
Asset Runtime Foundation
Resource Manager
Entity Identity Foundation
Serialization Foundation
RHI Core
Render Graph Core
Basic Renderer Path
```

### Optional High-level Subsystem

In principle, all of the following are Optional Runtime Modules:

```text
Physics / Jolt
Audio
FMOD
Animation
Spine
Terrain
Vegetation
WebView
Networking
Localization
Save / Persistent
Remote Content
Streaming
Navigation
Particle / VFX
Skeletal Animation
GPU Skinning
Post Processing
Shadow System
Hi-Z / Occlusion
GPU Driven Rendering
Runtime Profiler
Debug Renderer
Third-party SDK Integrations
```

### API Exists, Implementation Optional

The Public API may exist in the Engine SDK, while the Runtime Implementation is added only when required.

For example:

```text
Engine SDK
├─ Physics API
├─ Audio API
├─ WebView API
├─ Terrain API
└─ Networking API
```

A particular Game Build:

```text
Included:
Core
Renderer
UI
MiniAudio
Localization

Stripped:
Terrain
Vegetation
Jolt
Spine
FMOD
WebView
Networking
Runtime Profiler
```

### Dependency Rule

Optional Subsystems must not form unnecessary hard dependencies.

Avoid:

```text
Scene
→ Jolt Physics
```

It should be:

```text
Scene
→ Physics Public Contract

PhysicsModule
→ Optional Implementation
```

Likewise:

```text
AudioManager
→ IAudioBackend

MiniAudioBackend
FMODBackend
→ Optional Implementations
```

### Third-party Dependency Stripping

Third-party Libraries should enter the Build only when the corresponding Subsystem is enabled.

For example:

```text
Spine Disabled
→ No Spine Runtime

FMOD Disabled
→ No FMOD DLL / SO / Framework

Physics Disabled
→ No Jolt

WebView Disabled
→ No WebView Platform Integration Module

Networking Disabled
→ No Network Transport Runtime
```

Third-party SDK support in the Engine Source Tree must not cause those SDKs to automatically enter every game package.

### CMake Module Graph

The Build System should be organized by Target / Module.

For example:

```text
EngineCore
RendererCore
RuntimeUI
PhysicsJolt
AudioMini
AudioFMOD
SpineRuntime
TerrainRuntime
VegetationRuntime
WebViewRuntime
NetworkingRuntime
```

CMake selects according to Feature Resolution:

```cmake
if(ENGINE_ENABLE_PHYSICS)
    target_link_libraries(GameRuntime PRIVATE PhysicsJolt)
endif()

if(ENGINE_ENABLE_WEBVIEW)
    target_link_libraries(GameRuntime PRIVATE WebViewRuntime)
endif()
```

A Disabled Module should not merely be turned off through a runtime `if`; it should be removed from the Build Graph wherever possible.

### Final Build Pipeline

```text
Project
↓
Feature Usage Scan
↓
Build Profile
↓
Resolve Required Subsystems
↓
Resolve Plugin Dependencies
↓
Generate CMake Module Graph
↓
Compile Required Modules Only
↓
Link Required Libraries Only
↓
Cook Required Assets Only
↓
Generate Required Shader Variants Only
↓
Copy Required Runtime Libraries Only
↓
Final Package
```

### Build Size Optimization

Build Report should be able to list:

```text
Module
Included / Stripped
Reason
Native Binary Size
Third-party Library Size
Asset Size
Shader Variant Count
```

The goal is to answer:

```text
Why is this module in the package?
How much size does it contribute?
Can it be stripped?
```

### Hard Architecture Rule

Formal specification:

> Except for Foundation Core, all High-level Subsystems must avoid forming non-removable implicit Runtime Dependencies; if a project does not use a subsystem, it must be strippable at the compile / link / package / asset / shader levels.

This rule belongs to the Architecture Contract, not merely Build Optimization.


## Modular Development Build / Monolithic Shipping Build

The Engine Build System uses two modes:

```text
Development / Editor
→ Modular Build

Shipping / Release
→ Modular or Monolithic
```

Formal principle:

> Prioritize shorter iteration time during development; retain optimization and deployment flexibility for release.

### Core Library

Engine Core should exist in the form of precompiled Libraries.

Recommended:

```text
EngineCore
RendererCore
PlatformCore
```

Windows may use:

```text
EngineCore.lib
RendererCore.lib
```

macOS / iOS / Android use the platform-appropriate:

```text
Static Library
Framework
Archive
```

Core characteristics:

- Frequently used
- Architecturally stable
- Low-level Foundation
- Unsuitable for frequent crossings of Dynamic Library Boundaries
- Shareable by multiple Subsystems

### Dynamic Subsystem

Large, independent, loosely coupled Subsystems may exist as Dynamic Modules in Development Builds.

Windows:

```text
Physics.dll
Audio.dll
WebView.dll
Spine.dll
FMOD.dll
Terrain.dll
Vegetation.dll
Networking.dll
```

macOS:

```text
.dylib / Framework
```

Android:

```text
.so
```

iOS:

```text
Primarily Static Library / Framework as permitted by the platform
```

### Development Build

Development / Editor defaults:

```text
ENGINE_MONOLITHIC_BUILD = OFF
```

Behavior:

```text
Game Code Changed
↓
Recompile Game Module
↓
Relink Game
↓
Reuse Prebuilt Engine Core / Subsystem Modules
```

If the Subsystem Public ABI has not changed:

```text
Physics
Audio
WebView
Terrain
...
```

Gameplay modifications do not require recompilation.

Purpose:

- Reduce the number of full Builds
- Accelerate Play / Iteration
- Allow modules to compile independently
- Reuse Module Cache
- Allow Editor Plugins / Runtime Plugins to update independently

### Shipping Build

Release / Shipping may select:

```text
ENGINE_MONOLITHIC_BUILD = ON
```

Monolithic:

```text
Game Runtime
+
Required Subsystems
↓
Single / Reduced Binary Set
```

Advantages:

- The Linker can perform more complete optimization
- Reduce the number of Dynamic Libraries
- Simplify deployment
- Reduce Loader / Symbol management complexity
- Facilitate Whole Program Optimization / LTO

However, Shipping is not required to be Monolithic.

Build Profile may select:

```text
Runtime Link Mode
[ Modular ▼ ]

Options:
- Modular
- Monolithic
```

### Suggested Module Classification

Suitable for Dynamic Modules:

```text
Physics
Audio
FMOD
Spine
WebView
Networking
Editor Plugin
Third-party SDK
Large Optional Tooling Runtime
```

Generally not recommended to split into many DLLs:

```text
Entity Foundation
Resource Manager
Render Graph Core
Renderer Hot Path
Job System Core
Memory Allocator
RHI Core
```

Reasons:

- High call frequency
- ABI boundary cost
- Extensive shared state
- Complex Memory Ownership
- Excessive splitting increases maintenance and deployment costs

### ABI Boundary

Dynamic Subsystems must have a clear ABI Contract.

Arbitrary transfer across DLL / dylib / so boundaries is prohibited for:

```text
Unstable STL Container Layout
Backend Native Object
Allocator-private Object
Internal Concrete Class
```

Prefer:

```text
C ABI
Opaque Handle
Plain Struct
Versioned Interface
Engine-owned Buffer/View
```

For example:

```cpp
struct PhysicsApiV1
{
    PhysicsWorldHandle (*CreateWorld)(const PhysicsWorldDesc*);
    void (*DestroyWorld)(PhysicsWorldHandle);
};
```

Alternatively, use a stable Engine Interface Contract.

### Memory Ownership Across Module Boundary

Across Dynamic Modules:

```text
Allocate In Module A
→ Free In Module A
```

Avoid:

```text
Allocate In DLL
→ Free In EXE
```

unless the same Engine Allocator Contract is explicitly used.

All cross-Module resources should preferentially use:

```text
Handle
Span / View
Engine Allocator API
```

### Module Versioning

Each Dynamic Subsystem should have:

```text
Module Name
Module Version
Engine API Version
ABI Version
Build Configuration
Target Platform
```

Validate upon loading:

```text
Engine ABI
vs
Subsystem ABI
```

If incompatible:

```text
Do Not Load
→ Report Clear Error
```

### Feature Stripping Integration

Modular Build must integrate with Feature / Subsystem Stripping.

For example:

```text
WebView = Disabled
↓
Do Not Build WebView Module
Do Not Link WebView
Do Not Copy WebView DLL
```

```text
FMOD = Disabled
↓
Do Not Build FMOD Module
Do Not Deploy FMOD Runtime
```

Therefore:

```text
Optional Subsystem
+
Modular Build
+
Feature Stripping
```

share the same dependency graph.

### CMake

Recommended:

```cmake
option(ENGINE_MONOLITHIC_BUILD
       "Link optional runtime modules into main runtime"
       OFF)
```

Concept:

```cmake
if(ENGINE_MONOLITHIC_BUILD)
    add_library(PhysicsModule STATIC ...)
else()
    add_library(PhysicsModule SHARED ...)
endif()
```

The actual target type remains determined by the platform and subsystem characteristics.

### Build Cache

Stable Subsystems should allow:

```text
Prebuilt Binary Cache
Compiler Cache
CI Artifact Cache
```

The Cache Key must include at least:

```text
Source Revision
Compiler Version
Compiler Flags
Target Platform
Architecture
Build Configuration
Public ABI Version
Feature Defines
```

Avoid using an incorrect Binary Cache.

### Editor

The Editor itself should preferably use Modular:

```text
Editor.exe
├─ EngineCore
├─ Renderer
├─ Asset Tools
├─ UI Editor
└─ Optional Plugins / Subsystems
```

This helps with:

- Rapid development of Editor Tools
- Plugin Reload
- Reduced single Link time
- Independent testing of tool modules

### CI

CI must validate at least:

```text
Modular Development Build
Monolithic Shipping Build
Minimal Modular Build
Full-feature Modular Build
Representative Shipping Build
```

It must also check:

- Module dependency graph
- ABI version
- Missing DLL / dylib / so
- Incorrect runtime library deployment
- Disabled Subsystems must not appear in the package
- Monolithic Builds must not retain unnecessary Dynamic Runtime Modules

### V1 Scope

V1:

- Prebuilt Core Libraries
- CMake Module Graph
- Dynamic Subsystem on supported desktop platforms
- Module ABI Version
- Feature Stripping integration
- `ENGINE_MONOLITHIC_BUILD`
- Modular Development Build
- Monolithic Shipping Build
- Build Cache foundation

Platform strategy:

```text
Windows
→ Prioritize complete validation of DLL Modular Build

macOS
→ dylib / Framework according to deployment requirements

Android
→ .so Module where appropriate

iOS
→ Primarily Static / Framework deployment
```

### Definition of Done

Completion criteria:

- Modifying Gameplay does not unconditionally recompile all Subsystems
- Unmodified ABI-compatible Subsystem Binaries can be reused directly
- When an Optional Subsystem is Disabled, its Runtime Module is not generated
- Modular Development Build executes successfully
- Monolithic Shipping Build executes successfully
- Loading is rejected when the ABI is incompatible
- Cross-Module Memory Ownership rules are explicit
- Build Report can list the source and size of each Runtime Module

## Cross-platform In-Game WebView

The Engine includes a cross-platform Runtime WebView System.

WebView is a general-purpose In-Game Platform Service and is not tied to Login / OAuth.

Primary uses:

- Game announcements
- Event pages
- Customer support center
- FAQ
- Privacy policy
- Terms of use
- Community / event pages
- HTML-based Tool / Content
- Account-related pages
- Other In-Game Web Content

If an Auth Plugin requires a Browser / Web-based Flow, it may use the Authentication Flow permitted by the corresponding platform, but the general WebView and Authentication System must remain decoupled.

### Architecture

```text
Runtime UI / UIDocument
    │
    ├─ Image
    ├─ Label
    ├─ Button
    └─ WebViewElement
           │
           ▼
      WebViewManager / WebView Service
           │
           ▼
      Platform Backend
      ├─ Windows → Microsoft WebView2
      ├─ Android → android.webkit.WebView
      ├─ iOS     → WKWebView
      └─ macOS   → WKWebView
```

### Runtime API

Recommended:

```cpp
struct WebViewDesc
{
    Rect rect;
    bool visible = true;
    bool enableJavaScript = true;
};

WebViewHandle webView = WebView::Create(desc);

webView->LoadUrl("https://example.com/event");
webView->Show();
webView->Hide();
webView->Reload();
webView->GoBack();
webView->GoForward();
webView->SetRect(rect);
webView->Close();
```

### JavaScript Bridge

WebView must support bidirectional JS ↔ Engine communication.

Web → Engine:

```javascript
Engine.postMessage("claim_reward");
```

Engine:

```cpp
webView->OnMessage([](StringView message)
{
    if (message == "claim_reward")
    {
        // Game logic
    }
});
```

Engine → Web:

```cpp
webView->EvaluateJavaScript(
    "window.updatePlayerLevel(25);"
);
```

Data flow:

```text
C++ Game
↕
WebView Bridge
↕
JavaScript / HTML
```

Message Bridge should support:

- String Message
- JSON Message
- Request / Response ID (later)
- Error Callback
- Domain / Origin Validation

### Coordinate System

The WebView API must not be directly bound to a platform's Physical Pixel coordinates.

At minimum, support:

```text
UI Logical Coordinates
Normalized Coordinates
```

Optional support:

```text
Physical Pixel Coordinates
```

The Platform Backend must handle:

- Windows DPI Scaling
- macOS Retina Scale
- Android Density
- iOS Display Scale
- Safe Area
- Orientation
- Window Resize
- Fullscreen
- Multi-window (later)

The WebView Rect must be alignable with the Runtime UI RectTransform / Anchor System.

Important: In V1, `WebViewElement` is a **Native Overlay Proxy in the UIElement Tree**, not a normal `UIRenderItem` / Canvas Renderable.

```text
RectTransform / Anchor
→ Calculate WebView Screen-space Rect
→ Platform Backend
→ Native View Bounds
```

It does not generate a normal Runtime UI Mesh and does not enter UI Batch / Draw Call / Render Graph Pass.

### Native Overlay Strategy

V1 uses:

```text
Native Overlay WebView
```

Concept:

```text
Game Rendering Surface
+
Native WebView Layer
```

By platform:

```text
Windows
→ Game Window + WebView2

Android
→ Game Surface + Android WebView

iOS
→ Metal View + WKWebView

macOS
→ Metal View + WKWebView
```

### Native Overlay and Canvas Behavior Boundaries

In V1, Native Overlay is composited by the OS / Platform UI compositor rather than drawn by the Engine Renderer.

Therefore, although WebView may be attached to the Canvas / UI Hierarchy for convenient Layout and lifecycle management, **it must not be assumed to have exactly the same Rendering behavior as Image / Text / Button**.

V1 limitations:

```text
Supported / Integrated
✓ RectTransform Position / Size
✓ Anchor
✓ Safe Area
✓ Show / Hide
✓ Screen-space Layout
✓ Window Resize / Orientation
✓ Limited z-order management among Native Overlays

Not Canvas-rendered
✗ UI Batch
✗ Canvas Draw Call
✗ Render Graph UI Pass
✗ Stencil Mask / Canvas Mask
✗ 3D Depth Test
✗ Interleaved depth ordering with 3D Objects
✗ Arbitrary interleaved ordering with ordinary Canvas Elements
✗ RectTransform Rotation
✗ Skew / Perspective Transform
✗ Non-rectangular Clip
✗ Canvas Shader / Material Effect
```

z-order principle:

```text
Game / Canvas Render Surface
↓
Native Overlay Layer
↓
WebView
```

V1 does not guarantee:

```text
Canvas Image
↓
WebView
↓
Canvas Text
```

arbitrary interleaved ordering in which “WebView is placed between two Canvas draw elements.”

If complete Canvas Mask, Transform, Material, Depth, and ordering behavior is required, the following future capability must be used:

```text
Offscreen WebView
→ Render Texture
→ Canvas / 3D Renderer
```

Runtime UI Profiler Batch / Draw Call / Overdraw statistics **do not treat Native WebView as a Canvas Draw Call**; WebView should instead list diagnostic information such as Native Overlay count / visible area / platform cost.

V1 does not require implementation of:

```text
WebView
↓
Render To Texture
↓
GPU Texture
↓
3D / UI Quad
```

Reasons:

- Offscreen Rendering has significant cross-platform differences
- Keyboard / IME is complex
- Video Playback is complex
- Touch / Mouse Input Routing is complex
- Hardware Acceleration behavior differs
- WebView → Texture synchronization is costly

### Future: Offscreen WebView

The following may be evaluated later:

```text
Offscreen WebView
→ Render Texture
→ Runtime UI Texture
→ 3D World Screen
```

Use cases:

- Computer screens in a 3D world
- In-game browser materials
- Special HUDs
- VR / AR Surfaces

This feature is not a V1 requirement.

### Runtime UI WebViewElement

WebView formally exists as `WebViewElement` in the UIDocument / UIElement Tree for sharing Layout, Anchor, Safe Area, Visibility, and lifecycle management:

```text
UIDocument / UIElement Tree
├─ Image              → UIRenderItem
├─ Label              → UIRenderItem
├─ Button             → UIRenderItem
└─ WebViewElement     → Native Overlay Proxy
```

Formal Contract:

```text
WebViewElement
= UIElement

Native WebView
≠ UIRenderItem
≠ UI Batch Item
≠ RenderGraph UI Pass
```

In V1, `WebViewElement` should be clearly marked:

```text
Rendering Mode: Native Overlay
```

The Editor Inspector should indicate Native Overlay limitations to prevent designers from mistakenly assuming support for ordinary Canvas Mask / Sort / Transform.

Inspector recommendations:

```text
URL
Visible
Enable JavaScript
Allow Back Navigation
User Agent
Allowed Domains
RectTransform
Anchor
Safe Area Mode
Rendering Mode: Native Overlay (V1)
Native Overlay Z Order
Input Hit-Test Mode
Viewport Sync Mode
```

The following ordinary Canvas Inspector capabilities should be Disabled / Hidden or displayed as Unsupported for V1 WebView:

```text
Stencil / Mask
Canvas Material
Rotation / Skew / Perspective
3D Depth
Canvas Batch / Sorting Interleave
```

### Native Overlay Input / Viewport Sync

V1 `WebViewElement` must provide:

```text
Input Hit-Test Mode
├─ Block
└─ Pass-Through
```

Semantics:

```text
Block
→ WebView Native View receives pointer / touch input within its bounds
→ Engine UI / Game does not receive the same hit event

Pass-Through
→ WebView does not intercept ordinary pointer / touch input
→ Events are passed to the Engine UI / Game
```The platform implementation must explicitly handle:

- Touch
- Mouse
- Pointer Capture
- Focus
- Keyboard / IME Focus
- Visibility Change
- View Destroy / Recreate

The precise capabilities of `Pass-Through` depend on the platform Native View API implementation, but Public API behavior must be consistent; if a platform cannot fully support a specific interaction mode, this must be explicitly reported in Capability Query / Editor Validation, and different behavior must not be presented silently.

### Viewport Sync Logic

Native Overlay follows only the **Screen-space Absolute Bounding Box** calculated from RectTransform:

```text
RectTransform / Anchor / Safe Area
↓
Canvas Layout Resolve
↓
Screen-space Absolute Bounding Box
↓
DPI / Retina / Density Conversion
↓
Native View Bounds
```

Synchronization events must include at least:

- Window Resize
- Orientation Change
- Safe Area Change
- DPI / Display Scale Change
- Canvas Scale Change
- Fullscreen / Windowed Change
- UI Layout Dirty
- WebView Show / Hide

V1 WebView does not accept Canvas Transforms that require non-axis-aligned rectangles.

Editor must prohibit or mark as Disabled for V1 WebView:

```text
Mask / Stencil
Alpha Gradient / Canvas Material Fade
Rotation
Skew
Perspective
3D Depth Sorting
Arbitrary Canvas Interleave
Non-rectangular Clip
```

If fade-in/fade-out is required, V1 may only use the overall visibility / opacity capabilities explicitly supported by the platform and consistent across platforms; general Canvas Vertex Alpha semantics must not be applied to Native Overlay.

### Modal / Fullscreen Engine UI Interaction

Native Overlay WebView is generally located above the Engine render surface. Therefore, an explicit policy is required when fullscreen Engine Modal / Pause UI is displayed.

V1 fallback strategy:

```text
Show Fullscreen Engine Modal
↓
WebViewService SuspendOverlay()
↓
Native WebView Hide()
↓
Release / Transfer Input Focus
↓
Engine Modal Receives Input
```

After the Modal is closed:

```text
Restore WebView visibility if previously visible
↓
Re-sync viewport
↓
Restore focus only when explicitly requested
```

`SetOpacity(0)` must not be considered reliable V1 behavior on all platforms; it may only be used as an additional capability if the backend provides consistent and verified native opacity.

Editor must display a warning for “Canvas elements layered above Native WebView” to prevent designers from mistakenly assuming that ordinary Canvas z-order can overlay Native Overlay.

### WebView Render Mode

Public API reservation:

```cpp
enum class WebViewRenderMode
{
    NativeOverlay,
    OffscreenTexture
};
```

V1:

```text
NativeOverlay
→ Supported
```

Future:

```text
OffscreenTexture
→ Optional Backend Capability
```

V1 Native Overlay:

```text
ScreenSpace
→ Supported

WorldAnchored
→ Can be projected into a Screen-space Rect

True WorldSpace
→ Not directly supported by Native Overlay
```

Actual 3D Web Content will require in the future:

```text
Offscreen WebView
↓
GPU Texture
↓
WorldSpace UI / Mesh
```

### WebView Local / Remote Content

Supported:

```text
LoadURL
LoadLocalAsset
Reload
Back
Forward
Stop
```

Local HTML / CSS / JS / Image can be used as general Asset / Bundle Content.

For example:

```text
ui/web/help/index.html
```

Can be used for:

- Help
- News
- Terms
- Patch Notes
- Event Page
- Complex formatted content

Core Gameplay UI must not depend on Remote Web Content in order to function.

### WebView Storage / Cache Policy

WebView Browser Storage is separate from Engine Asset Cache / AudioResidencyScope.

Formal:

```text
WebViewCachePolicy
├─ Default
├─ NoCache
├─ Session
└─ Persistent
```

And:

```text
WebViewStorageProfile
├─ Shared
├─ Isolated
└─ Ephemeral
```

Management scope includes:

- HTTP Cache
- Cookie
- Local Storage
- Web Storage

Logout / Account switching must be able to clear or switch the corresponding Storage Profile.

### WebView Trust / Bridge Policy

Formal:

```text
WebViewTrustLevel
├─ LocalTrusted
├─ TrustedOrigin
└─ UntrustedRemote
```

Bridge permissions are jointly determined by:

```text
Trust Level
+
Allowed Origins
+
Allowed Message Types
```

`UntrustedRemote` defaults to:

```text
Engine Bridge Disabled
```

### JSON Message Bridge

The JS ↔ Engine Bridge uses a message-based contract, with the Engine JSON Framework preferred for data formatting.

For example, Web → Engine:

```json
{
  "type": "shop.buy",
  "requestId": 123,
  "payload": {
    "itemId": 10001
  }
}
```

Engine → Web:

```json
{
  "requestId": 123,
  "ok": true
}
```

Formal principles:

```text
Web Content
→ Untrusted Input

Bridge
→ Explicit Message Interface

Sensitive Action
→ Engine-side Revalidation
```

Web Content is prohibited from obtaining:

- Native Pointer
- C++ Function Address
- Arbitrary Engine API
- Private Backend Object

### WebView Gesture Ownership

WebView and Runtime UI / Game Input must use a Single-owner policy.

```text
Pointer inside active WebView bounds
→ WebView owns gesture

Pointer outside
→ Engine UI / Game owns gesture
```

The same:

- Drag
- Scroll
- Pinch
- Long Press

must not be consumed simultaneously by both WebView and Engine ScrollView.

It is not recommended to place a self-scrollable WebView inside another Engine ScrollView while expecting both sides to process the same gesture simultaneously.

### WebView Editor Integration

`WebViewElement` is displayed together with other UIElements in the Editor Hierarchy.

The Scene View may use the following by default:

```text
WebView Placeholder
```

For example:

```text
WebView
URL: https://example.com
Rendering: Native Overlay
```

Only when needed should the following be activated:

```text
Preview WebView
```

This avoids unconditionally creating large numbers of Platform Native WebViews in the Editor Scene View.


### Navigation / Security

WebView must provide:

- Navigation Started
- Navigation Completed
- Navigation Failed
- URL Changed
- Close Requested
- External Link Request

Configurable:

```text
Allowed Domains
Blocked Domains
Open External Browser Rules
JavaScript Enabled / Disabled
Local File Access
Mixed Content Policy
```

Important:

- Arbitrary Web Content must not directly obtain Engine Private API
- JS Bridge must be an explicitly registered Message Interface
- Sensitive Action must be revalidated by the Engine
- WebView Content is not considered trusted input

### WebView and Authentication

The architecture remains separate:

```text
WebView System
→ General In-Game Web Content

Authentication Provider System
→ Google / Apple / Future Providers
```

The OAuth / Sign-In flow uses the login methods permitted by the platform through the Auth Provider.

The existence of an Engine WebView must not force all Authentication to use a general Embedded WebView.

### Plugin Integration

Plugins may use the WebView Service:

```text
Editor / Runtime Plugin
↓
Public WebView API
↓
Platform WebView Backend
```

For example:

- Customer Support Plugin
- News / Event Plugin
- Account Plugin
- Third-party SDK UI

Plugins must not directly operate:

```text
WKWebView
Android WebView
WebView2 Native Object
```

Platform Native WebView remains managed by the Engine Backend.

### Lifetime

WebView uses:

```text
WebViewHandle
```

The WebViewManager manages the actual Native Object Lifetime.

```text
Create
↓
WebViewHandle
↓
WebViewManager
↓
Platform Native WebView
```

Avoid having Gameplay / Plugin directly retain Backend Native Pointer.

### Threading

- WebView UI operations must switch back to the Platform UI Thread according to platform rules
- When the Game Thread calls the WebView API, dispatch is performed by the Backend
- JS Callback must not directly modify the Scene on an arbitrary Platform Thread
- Callback should be forwarded to the Event Queue / Main Thread
- Close / Destroy must handle callbacks currently in progress

### V1 Scope

V1 must support:

- Windows WebView2
- Android WebView
- iOS WKWebView
- macOS WKWebView
- Load URL
- Show / Hide
- Close
- Rect / Resize
- Safe Area
- Orientation
- Back / Forward
- Reload
- JavaScript Execution
- JS → C++ Message Bridge
- Navigation Callback
- Error Callback
- External Browser
- Runtime UI `WebViewElement`

V1:

```text
Native Overlay
```

Future:

```text
Offscreen / Render-to-Texture WebView
```

### Definition of Done

WebView V1 completion criteria:

- All four target platforms use the same Public API
- WebView can be created and closed In-Game
- Runtime UI can control the WebView Rect
- DPI / Density / Retina coordinates are correct
- Safe Area is correct
- Orientation / Window Resize are correct
- URL Load works correctly
- Back / Forward / Reload work correctly
- JS Execution works correctly
- JS ↔ Engine Message Bridge works correctly
- Native Callback correctly returns to the Engine Event Queue
- External Link can switch to the System Browser
- Platform Native Type is not exposed to Gameplay / Plugin
- WebView System is not tightly coupled to Authentication System
- Native Overlay does not participate in Canvas Batch / Render Graph UI Pass
- Editor can clearly display limitations such as Mask / Depth / Rotation / Arbitrary Canvas Sort
- The RectTransform of WebViewElement is used only for Screen-space Native View Layout and does not imply complete UIRenderItem / Canvas Render Semantics

## 28. Editor

Technology:
- C++
- Dear ImGui
- Docking
- Multi-Viewport

Main windows:
- Hierarchy
- Scene View
- Game View
- Inspector
- Asset Browser
- Console
- Profiler
- Build Settings

Requirements:
- Scene View can be detached from the main window
- Game View can be detached from the main window
- Multi-monitor work
- Gizmo
- Grid Snap
- Rotation Snap
- Local / World
- Perspective / Orthographic


## 29. Editor Play Mode

Architecture:

```text
Edit World
↓ Play
```
```text
Runtime World Clone
↓
Play
```

Stop:
Runtime World Dispose

Avoid Play Mode modifications contaminating the original Scene.

Can be added later:
- Apply Runtime Changes


## 30. Prefab

Prefab features:
- Prefab Asset
- Prefab Instance
- Override Tracking
- Nested Prefab (later)
- Apply
- Revert

Inspector:

Overrides
- Transform.Position
- Material
- Script Property

Features:
- Apply Selected
- Revert Selected
- Apply All
- Revert All


## 31. Undo / Redo

Required for V1.

Command Pattern:

ICommand
- Execute
- Undo

Covers:
- Move
- Rotate
- Scale
- Rename
- Add / Remove Component
- Delete Node
- Property Change
- Material Change
- Prefab Change


## 32. Runtime UI Framework

Dear ImGui is used only for Editor / Debug / Development Tool.

Formal layering:

```text
Engine UI
├─ Runtime UI
│  └─ engine::ui
│
└─ Editor UI
   └─ engine::editor
      └─ Dear ImGui
```

Core principles:

```text
Dear ImGui
→ Editor / Debug / Development Tools

Custom Retained Mode UI
→ Player-facing Runtime UI
```

The two may share:

- Renderer / RHI
- Font
- Texture
- Input
- Asset System
- Localization

However, they do not share a high-level Widget Framework.

### Runtime UI Architecture

```text
UI JSON / UIDocument Asset
↓
UIDocument
↓
UIElement Tree
├─ Widget
├─ Layout
├─ Style
├─ Binding
├─ Animation
└─ Event
↓
Layout / Text Shaping
↓
Input / Hit Test / Focus
↓
UI Render Extraction
↓
Clip / Sort / Batch
↓
UI Renderer
↓
RenderGraph
```

Runtime UI uses Retained Mode.

### UIElement / UI Tree

`UIElement` is the Runtime UI’s own Lightweight Hierarchy Node.

```text
UIDocument
└─ UIElement
   ├─ Panel
   │  ├─ Image
   │  └─ Label
   └─ Button
      └─ Label
```

Formal Contract:

```text
UIElement
≠ SceneNode
≠ EntityID
```

Prohibited:

```cpp
class UIElement : public SceneNode
```

The UI Tree uses an independent:

```text
UIElementID
```

Conceptual API:

```cpp
namespace engine::ui
{
    class UIElement
    {
    public:
        UIElementID GetID() const;
        UIElementID GetParent() const;
        UIElementID GetFirstChild() const;
        UIElementID GetNextSibling() const;
    };
}
```

Runtime may use lightweight hierarchy data:

```text
UIElementPool
├─ UIElementID[]
├─ Parent[]
├─ FirstChild[]
├─ NextSibling[]
├─ Type[]
├─ Flags[]
├─ Layout[]
├─ ComputedRect[]
├─ Style[]
└─ RenderState[]
```

Widget-specific data may be separated into:

```text
UIImagePool
UILabelPool
UIButtonPool
UIScrollViewPool
...
```

That is:

```text
Public API
→ Node-like UIElement

Runtime Storage
→ Handle + Pool / Data-Oriented
```

### Scene ↔ UI Boundary

Scene manages only the boundary of UIDocument; each Widget is not turned into a Scene Entity.

```text
Scene Entity
└─ UIComponent
   └─ UIDocument
      └─ N UIElements
```

Formal:

```text
1 Scene Entity
→ 1 UIDocument
→ N UIElements
```

Creating 500 Scene Entities for 500 Widgets is prohibited.

`UIDocument` is the formal boundary between the Scene and the Runtime UI Tree.

### Editor Hierarchy Integration

Runtime UI does not inherit from SceneNode, but the Editor must be able to edit it together with the Scene in the same Hierarchy.

The Editor uses a unified:

```text
EditorObjectHandle
├─ SceneEntity
├─ UIElement
├─ Asset
└─ ...
```

And:

```text
EditorObjectAdapter
├─ SceneEntityAdapter
├─ UIElementAdapter
└─ AssetAdapter
```

Unified capabilities must include at least:

```text
GetName
GetParent
GetChildren
Rename
Delete
Duplicate
CanReparent
GetInspector
GetIcon
GetSelectionBounds
```

The Editor Hierarchy may present:

```text
Scene
├─ Camera
├─ Player
└─ UIRoot
   └─ UIComponent
      └─ MainHUD
         ├─ PlayerStatus
         │  ├─ HPBackground
         │  ├─ HPFill
         │  └─ PlayerName
         └─ SkillBar
            ├─ Skill01
            └─ Skill02
```

The actual underlying structure remains:

```text
Scene Entity
      │
      └─ UIComponent
              │
              ▼
         UIDocument
              │
              ▼
         UIElement Tree
```

### Editor Scene View / Gizmo

Scene View displays the following simultaneously:

```text
3D Scene
+
Runtime UI Preview
+
Scene Gizmo
+
UI Rect Gizmo
```

Selection policy:

```text
Scene Entity
→ Move / Rotate / Scale Gizmo

UIElement
→ Rect / Anchor / Pivot / Resize / Margin / Padding Gizmo
```

The Editor can automatically switch UI editing tools according to Selection.

### UI Render Space

Formal:

```cpp
enum class UIRenderSpace
{
    ScreenSpace,
    WorldAnchored,
    WorldSpace
};
```

The three Render Spaces share:

```text
Widget
Layout
Style
Event
Binding
Animation
Text
```

The differences are limited to:

```text
Coordinate Mapping
Render Extraction
Input Hit Test
Depth / Occlusion
```

### ScreenSpace UI

General uses:

- HUD
- Menu
- Inventory
- Shop
- Settings
- Pause
- Chat

Process:

```text
UIDocument
↓
Layout
↓
Screen-space Geometry
↓
UI Render Pass
```

### WorldAnchored UI

Applicable to:

- NPC Name
- Enemy HP Bar
- Quest Marker
- Damage Number
- Interaction Prompt

Process:

```text
World Position
↓
Camera Projection
↓
Screen Position
↓
Screen-space UI
```

WorldAnchored UI takes priority over actual WorldSpace geometry unless the requirement genuinely needs a 3D surface.

### WorldSpace UI

UI that truly exists in the 3D world:

- Cockpit Display
- World Terminal
- Elevator Panel
- VR / AR Panel
- In-world Computer
- 3D Shop Display

Process:

```text
UIDocument
↓
UIElement Tree
↓
UILayout
↓
UI Geometry
↓
UIDocument World Transform
↓
View / Projection
↓
3D Scene
```

Scene:

```text
Computer Entity
└─ UIComponent
   └─ UIDocument
      ├─ Label
      ├─ Button
      └─ Slider
```

Only the UIDocument boundary holds the Scene World Transform; internal Widgets remain UIElements.

### WorldSpace Coordinate Mapping

```text
Logical UI Space
↓
UILayout
↓
ComputedRect
↓
UI Local Transform
↓
UIDocument World Scale
↓
World Transform
```

For example:

```text
Design Size
1920 × 1080

World Size
2.0m × 1.125m
```

Layout still uses Logical UI Unit; each Widget is not converted into a general 3D Transform.

### WorldSpace Depth / Occlusion

Formal:

```text
WorldUIOcclusionMode
├─ DepthTest
├─ AlwaysVisible
└─ Custom
```

For example:

```text
World Terminal
→ DepthTest

Quest Marker
→ WorldAnchored / AlwaysVisible
```

### WorldSpace UI Input

ScreenSpace:

```text
Pointer Position
↓
2D UI Hit Test
```

WorldSpace:

```text
Mouse / Touch / Controller Ray
↓
Camera Ray
↓
UIDocument Plane / Surface
↓
Ray Intersection
↓
Convert to UI Local Coordinate
↓
Normal UI Hit Test
↓
UI Event
```

Input Adapter:

```text
UI Input
├─ ScreenPointerAdapter
└─ WorldPointerAdapter
```

General WorldSpace UI interaction does not create a Jolt Collider for every Widget.

```text
WorldSpace UI Hit Test
≠
Physics RigidBody / Collider
```

A Collider is created additionally only when Gameplay genuinely requires physical interaction.

### Surface UI / RenderTexture

May be supported later:

```text
UIDocument
↓
RenderTexture
↓
3D Mesh Material
```

Applicable to:

- Curved Monitor
- Cylinder Display
- VR Curved Panel
- Irregular Mesh Surface

Positioning:

```text
WorldSpace Direct Geometry
→ V1

RenderTexture Surface UI
→ Optional / V2
```

### Widget V1

V1 supports at least:

```text
UIElement
├─ Panel
├─ Image
├─ Label
├─ Button
├─ Toggle
├─ Slider
├─ ProgressBar
├─ InputField
├─ ScrollView
├─ ScrollBar
├─ Mask
└─ Layout
```

Later:

```text
Dropdown
TabView
TreeView
RichText
```

### RectTransform / Layout

Runtime UI uses its own:

```text
UIRectTransform
├─ AnchorMin
├─ AnchorMax
├─ Pivot
├─ AnchoredPosition
├─ Size
├─ MinSize
├─ MaxSize
├─ Margin
└─ Padding
```

Layout:

```text
HorizontalLayout
VerticalLayout
GridLayout
OverlayLayout
```

May add later:

```text
Flex-like Layout
```

Formal distinction:

```text
Declared Layout
≠
Computed Layout
```

Process:

```text
Declared Layout
↓
Measure
↓
Layout Solver
↓
ComputedRect
```

### Nine-Slice

Formal core capability:

```text
UIImageMode
├─ Simple
├─ Sliced
├─ Tiled
└─ Filled
```

Nine-Slice is used for:

- Dialog Bubble
- Panel
- Button
- Window Background

### Text / Font Integration

Uses:

```text
FreeType
+
HarfBuzz
```

Process:

```text
UTF-8
↓
Localization
↓
HarfBuzz Shaping
↓
Line Breaking
↓
FreeType Glyph
↓
Dynamic Glyph Atlas
↓
UI Renderer
```

Supports:

- Traditional Chinese
- Simplified Chinese
- Japanese
- English
- Kerning
- Ligature
- Wrap
- Line Break

Text Measure must integrate with the Layout Solver:

```text
Available Width
↓
Text Measure
↓
Line Breaking
↓
Preferred Height
↓
Layout Solver
↓
Final Rect
```

### UI Input / Event

Process:

```text
Platform Input
↓
Engine Input System
↓
UI Hit Test
↓
UI Event System
```

Events must include at least:

```text
PointerDown
PointerUp
PointerMove
Click
DoubleClick
Scroll
Drag
Drop
Focus
Blur
Submit
Cancel
```

Mouse / Touch / Pen are unified as:

```text
PointerEvent
```

### Event Routing

Supports:

```text
Capture
↓
Target
↓
Bubble
```

For example:

```text
ScrollView
└─ Button
```

ScrollView can handle Drag, while Button can handle Click.

### Focus / Navigation

Formal:

```text
UIFocusSystem
```

Supports:

```text
Keyboard
Gamepad
Remote Controller
```

Actions:

```text
NavigateUp
NavigateDown
NavigateLeft
NavigateRight
Submit
Cancel
```

### ScrollView Virtualization

Formal support:

```text
VirtualizedListView
```

For example:

```text
10,000 Data Rows
↓
Visible 15 Rows
↓
Approximately 20 UI Item Instances reused
```

Core:

```text
ItemProvider
ItemRecycler
VisibleRange
ScrollOffset
```

10,000 UIElements must not be created for 10,000 data entries.

### Data Binding

Runtime UI does not directly bind to Gameplay Object implementation.

Formal:

```text
Gameplay
↓
ViewModel
↓
UIBindingContext
↓
UIElement
```

For example:

```text
PlayerStatusViewModel
├─ HP
├─ MaxHP
├─ Level
└─ PlayerName
```UI:

```text
HPBar
← HP / MaxHP

NameLabel
← PlayerName
```

Responsibilities of DataTable and Runtime UI:

```text
DataTable
→ Configuration

Runtime Component
→ Current Runtime State

ViewModel
→ UI Presentation Data
```

UI must not directly modify DataTable.

### JSON UI / UIDocument Asset

Officially supported:

```text
*.ui.json
```

Pipeline:

```text
UI JSON
↓
Engine JSON Framework
↓
UIDocument Loader
↓
Typed UI Tree
↓
Runtime
```

As with DataTable:

```text
JSON
→ Load / Parse Once

Runtime
→ Typed Runtime Data
```

The JSON DOM must not be operated on every frame.

Large-scale UI:

- MainHUD
- Inventory
- Shop
- MainMenu

Using an external `*.ui.json` Asset is recommended.

The Scene `UIComponent` stores:

```text
UIDocument Asset UUID
```

Advantages:

- Independent Editing
- Reuse
- Bundle
- Hot Reload
- Version Control

### UI Style

Officially:

```text
UIStyle
```

At minimum:

```text
Font
FontSize
TextColor
Background
Sprite
NineSlice
Padding
Margin
Opacity
```

State:

```text
Normal
Hover
Pressed
Disabled
Focused
```

Can be extended later:

```text
UIStyleSheet
```

V1 does not require complete CSS.

### UI Animation / Tween

Runtime UI provides its own:

```text
UIAnimation
/
UITween
```

At minimum, support:

```text
Position
Scale
Rotation
Opacity
Color
Size
Layout Parameter
```

General UI Tween does not depend on Skeleton Animation.

### UI Render Extraction

One Draw Call per Widget is prohibited.

Pipeline:

```text
UITree
↓
UIRenderItem[]
↓
Sort
↓
Clip
↓
Batch
↓
UIRenderer
↓
RenderGraph
```

Batch targets:

```text
Image
Nine-Slice
Glyph
Simple Shape
```

Group by the following states:

```text
Texture
Material
Blend
Clip
RenderSpace
DepthState
```

### Clip / Mask

V1:

```text
Rect Clip
```

Later:

```text
RoundedRect
Stencil
Texture Mask
```

### Logical UI Resolution

Officially:

```text
Logical UI Resolution
≠
Physical Pixel Resolution
```

For example:

```text
Design Resolution
1920 × 1080
```

Converted to the actual device through:

```text
UIScalePolicy
```

Safe Area must support:

- iPhone Notch
- Dynamic Island
- Android Cutout
- Tablet Safe Area

UI Layout obtains it through:

```text
SafeAreaRect
```

and does not allow Gameplay UI to query the OS directly.

### UI and Dynamic Resolution

Officially:

```text
3D Scene
→ Dynamic Resolution

Screen UI
→ Native / Logical UI Resolution

↓
Composite
```

UI does not reduce its resolution together with 3D Dynamic Resolution.

### Multiple UIDocument

A Scene may contain multiple:

```text
Scene
├─ MainHUD
│  └─ UIComponent
├─ PauseMenu
│  └─ UIComponent
└─ InteractionUI
   └─ UIComponent
```

Each UIDocument may have:

```text
RenderSpace
RenderLayer
InputPriority
Visibility
```

### UI Hot Reload

```text
MainHUD.ui.json Changed
↓
Parse
↓
Validate
↓
Build New UIDocument
↓
Success?
├─ No → Keep Old
└─ Yes
   ↓
   Replace / Rebind
```

An erroneous UI Asset must not clear the currently valid UIDocument.

### Runtime UI Performance

Design goals:

- UI Batch
- Glyph Atlas
- Dirty Update
- Layout Dirty Propagation
- Recycled List / Virtualized List
- Texture Atlas
- Material Batch
- Clip / Mask Optimization
- Static UI Cache
- Dynamic Vertex Buffer

Pipeline:

```text
UI Tree
↓
Dirty Check
↓
Layout / Transform
↓
Geometry Build
↓
Sort / Batch
↓
GPU
```

Large Lists:

```text
10,000 data entries
↓
Retain only approximately 10~30 visible Item Instances
```

### Runtime UI Namespace

Runtime:

```cpp
engine::ui
```

When necessary:

```cpp
engine::ui::layout
engine::ui::text
engine::ui::render
engine::ui::input
engine::ui::binding
```

Do not create an independent:

```text
engine::ui3d
```

Because ScreenSpace / WorldAnchored / WorldSpace share the same Runtime UI Framework.

Editor:

```cpp
engine::editor
```

Dear ImGui backend:

```cpp
engine::editor::imgui
```

Prohibited:

```cpp
engine::ui::imgui
```

### Runtime UI V1 Scope

V1:

```text
✓ UIDocument
✓ UIElement / UIElementID
✓ UI Tree
✓ UIComponent

✓ Panel
✓ Image
✓ Nine-Slice
✓ Label
✓ Button
✓ Toggle
✓ Slider
✓ ProgressBar
✓ InputField
✓ ScrollView
✓ VirtualizedListView

✓ RectTransform
✓ Anchor
✓ Pivot
✓ Layout
✓ Safe Area

✓ FreeType + HarfBuzz Text
✓ Localization

✓ Pointer Event
✓ Capture / Target / Bubble
✓ Focus
✓ Keyboard / Gamepad Navigation

✓ ViewModel / Data Binding

✓ JSON UI
✓ UI Style
✓ UI Animation / Tween

✓ Rect Clip
✓ UI Render Extraction
✓ UI Batch Renderer
✓ RenderGraph Integration

✓ ScreenSpace
✓ WorldAnchored
✓ WorldSpace
✓ WorldSpace Input Ray Mapping
✓ Depth / Occlusion
```

Future:

```text
△ RichText
△ Advanced Mask
△ CSS-like Style
△ Flex Layout
△ RenderTexture Surface UI
△ Curved UI
△ Advanced World-space UI
△ UI Particle / Special Effects
```

### Runtime UI Definition of Done

V1 must at least verify:

- UIElement does not depend on SceneNode / EntityID.
- UIDocument can be mounted by a Scene `UIComponent`.
- Editor Hierarchy can simultaneously present Scene Entities and UIElements.
- UIElement Reparent / Delete / Duplicate support Undo / Redo.
- ScreenSpace / WorldAnchored / WorldSpace share Widget / Layout / Event.
- WorldSpace UI ray mapping is correct.
- General WorldSpace UI hit-testing does not require a Jolt Collider.
- Layout / Text Measure / Localization can be stably re-laid out.
- VirtualizedListView does not create an equivalent number of Widgets based on the data count.
- JSON UI parse failure preserves the old UIDocument.
- UI Batch / Clip / Glyph Atlas are observable in the profiler.
- UI Logical Resolution is completely decoupled from 3D Dynamic Resolution.

Core Contract:

```text
Runtime UI
→ Custom Retained Mode Framework

Editor UI
→ Dear ImGui

UIElement
→ Lightweight UI Hierarchy Node

UIElement
≠ SceneNode
≠ EntityID

UIDocument
→ Scene ↔ UI Boundary

Editor Hierarchy
→ Scene Hierarchy + Mounted UI Hierarchies

ScreenSpace / WorldAnchored / WorldSpace
→ Same Widget / Layout / Style / Event / Binding

UI JSON
→ Parse Once → Typed UI Tree

UI Resolution
→ Independent from 3D Dynamic Resolution
```


## Thirty-Three, Runtime UI Performance

The capabilities in this section have been integrated into the Runtime UI Performance, VirtualizedListView, Render Extraction, and V1 DoD sections of the “Runtime UI Framework.”

## Thirty-Four, Text System

Use:
- FreeType
- HarfBuzz

Support:
- TTF / OTF
- Unicode
- CJK
- Ligature
- Complex Script
- Font Fallback
- Glyph Atlas

Render text using Glyph Batches to avoid one Draw Call per character.


## Thirty-Five, Localization

Included in V1.

Minimum functionality:
- Key-based String Table
- Locale switching
- Font Fallback
- Language-specific Font
- Basic Formatting
- Basic Plural Rule support
- Editor Preview

Example:

ui.menu.start
ui.menu.settings
item.weapon.sword

Display text must not be hard-coded in Gameplay.


## Thirty-Six, Save / Persistent Data

Included in V1.

Features:
- Platform-specific Save Path
- JSON / Binary Save
- Version Number
- Save Migration
- Atomic Write
- Temp File + Replace
- Corruption Detection
- User Settings
- Game Progress

Principles:
- Save Data must not directly use the Scene Serialize Format
- A version upgrade mechanism is required


## Thirty-Seven, Networking

V1 does not include a high-level Networking Framework.

Reasons:
- Requirements vary greatly between different games
- Photon / custom UDP / TCP / WebSocket / third-party SDKs may be used
- Deciding too early would lock the architecture in place

V1:
- Retain the Platform Socket / Transport Interface
- Do not implement the Gameplay Network Layer

Future:
- Reliable UDP
- Replication
- Snapshot
- Prediction
- Reconciliation
- Lobby / Matchmaking Adapter

Clearly state:
Networking is not an omission; it is intentionally excluded from V1.


## Thirty-Eight, Animation

This section is a summary; the complete Runtime Contract is defined in the “Animation Framework” chapter.

V1:
- Skeleton
- Animation Clip
- Animator / Animation Graph
- State Machine
- Basic Cross Fade / Blend Tree
- Basic Layers / Avatar Mask
- Root Motion
- Animation Event
- GPU Vertex Skinning
- BAT Crowd Animation

Future / Advanced:
- Advanced IK Graph
- Control-Rig-like Authoring
- Compute Skinning



### GPU Skinning Resource Binding

GPU Skinning does not establish an independent special Binding path. Skinning Matrix / Bone Palette uses the unified Resource Registry.

```text
Animator
↓
Pose Evaluation
↓
Packed Bone Matrices
↓
Global Skinning Matrix Buffer
├─ Resource Identity → Resource Registry → GPU Buffer ResourceIndex
└─ Per-character Range → skinningMatrixOffset
                         ↓
                  SkinnedDrawData
                         ↓
                      Shader
```

```cpp
struct SkinnedDrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t skinningMatrixOffset;
};
```

`skinningMatrixOffset` is the matrix element offset within the Global Skinning Matrix Buffer. It is not a GPU `ResourceIndex`, descriptor index, or byte offset.

The Global Skinning Matrix Buffer itself is managed by the Resource Registry and mapped through the Engine Resource Binding abstraction to DX12 SRV / Descriptor Heap, Vulkan Buffer Descriptor / Descriptor Indexing, and Metal Argument Buffer / Resource Table. The Animation System is responsible only for Pose / Skinning Data; GPU Buffer ownership and lifetime are managed by the Renderer / Resource Manager.


## Dynamic Skinning Data / Transient GPU Buffer Architecture

The Animation / Skinning Runtime adopts a centralized, linearly allocated Dynamic GPU Data Strategy.

Core objective:

```text
Per-character Pose
↓
Packed Bone Matrices
↓
Shared Dynamic GPU Buffer
↓
Per-draw Offset
```

Avoid:

- Creating an independent GPU Buffer for each character
- Large numbers of small GPU allocations per frame
- Descriptor fragmentation
- High-frequency create / destroy GPU resource operations
- Unnecessary CPU pointer chasing

### SkinnedDrawData

Logical Draw Payload:

```cpp
struct alignas(16) SkinnedDrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t skinningMatrixOffset;
};

static_assert(sizeof(SkinnedDrawData) == 16);
```

Semantics:

```text
transformIndex
→ element index within the Transform Buffer

materialIndex
→ element index within the MaterialGPU Buffer

meshIndex
→ Mesh / Geometry metadata index

skinningMatrixOffset
→ matrix offset within the Global Skinning Matrix Buffer
```

Important:

`transformIndex` / `materialIndex` / `meshIndex` are element indices of data Buffers and are not equivalent to Bindless Descriptor Indices.

### Frame Resource Indices

The Bindless / Resource Registry index of the resource itself should be managed separately by the Frame Context.

For example:

```cpp
struct FrameResourceIndices
{
    uint32_t transformBuffer;
    uint32_t skinningBuffer;
    uint32_t materialBuffer;
};
```

Concept:

```text
ResourceIndex
→ Locate the GPU Resource

ElementIndex
→ Locate the data within that Resource
```

The two must not be mixed.

### Frames In Flight

The Dynamic Skinning Buffer must support at least 2~3 Frames In Flight.

For example:

```text
Frame 0 Buffer
Frame 1 Buffer
Frame 2 Buffer
```

However:

```text
Triple Buffering
≠
Zero Synchronization
```

Before the CPU reuses a Frame Slot, it must confirm that the GPU has finished using it.

Frame pipeline:

```text
Acquire Frame Slot
↓
Check / Wait Frame Fence
↓
Reset Linear Cursor
↓
CPU Writes
↓
Submit GPU Work
↓
Signal Fence
```

Do not merely use:

```text
frameIndex % 3
→ reset
```

without confirming GPU lifetime.

### Dynamic GPU Buffer Strategy

The high-level architecture does not hard-code CPU-visible GPU memory for all platforms.

Unified abstraction:

```text
DynamicGpuBufferPool
↓
Backend Chooses Memory Strategy
```

UMA / Unified Memory devices:

```text
CPU
↓
Persistent Mapped Shared Buffer
↓
GPU
```

May apply to:

- Apple Silicon
- Most Mobile UMA architectures
- Other coherent / shared memory platforms

Discrete GPU:

```text
CPU
↓
Upload / Staging Ring Buffer
↓
GPU Copy
↓
Device-local Buffer
↓
Shader Read
```

May apply to:

- Windows discrete GPUs
- Some Vulkan desktop devices

The RHI must determine:

- Host Visible
- Host Coherent
- Device Local
- Upload / Staging
- Flush / Invalidate Requirement
- Copy Scheduling

The Gameplay / Animation System must not be aware of the underlying memory type.

### Vulkan / Metal Memory Visibility

CPU `memcpy()` does not mean that the GPU is immediately able to see the data on every backend.

Vulkan:

```text
Host-visible + non-coherent
→ Backend must perform the appropriate Flush
```

Metal:

```text
Storage Mode / Platform
→ Metal backend handles shared / managed synchronization
```

The above behavior must be encapsulated in the RHI / Dynamic GPU Buffer Backend.

### Resource Registry / Bindless Mapping

The Shader must not directly depend on the DX12-specific:

```text
ResourceDescriptorHeap[index]
```

as the cross-platform contract.

Official architecture:

```text
Canonical ResourceIndex
↓
RHI Binding Mapping
├─ DX12
│  └─ Descriptor Heap
├─ Vulkan
│  └─ Descriptor Indexing / Large Descriptor Set
└─ Metal
   └─ Argument Buffer / Resource Table
```

Slang Shader uses the bindless abstraction / generated helper provided by the Engine.

Concept:

```slang
StructuredBuffer<float4x4>
GetSkinningBuffer(uint resourceIndex);
```

rather than treating the DX12 SM 6.6 intrinsic as a general-purpose API.

### Shader Access Model

Conceptual shader:

```text
FrameResourceIndices.skinningBuffer
↓
Get Skinning Buffer Resource
↓
SkinnedDrawData.skinningMatrixOffset
↓
+ Vertex Bone Index
↓
Bone Matrix
```

Transform:

```text
FrameResourceIndices.transformBuffer
↓
Transform Buffer Resource
↓
SkinnedDrawData.transformIndex
↓
World Matrix
```

Therefore:

```text
Resource Binding
and
Resource Internal Indexing
```

remain separate.

### Draw Data Transport

`SkinnedDrawData` retains a 16-byte logical payload.

The Backend may independently choose the transport method:

```text
DX12
→ Root Constants
→ or Draw Data Buffer

Vulkan
→ Push Constants
→ or Draw Data Buffer

Metal
→ Constant Buffer
→ or Argument-buffer referenced Draw Data
```

Important:

```text
Logical DrawData Layout
→ Unified

Physical Transport
→ Backend-specific
```

Push Constant / Root Constant / Argument Buffer must not be treated as completely equivalent APIs.

### Dynamic Skinning Pool

Concept:

```cpp
class DynamicSkinningBufferPool
{
public:
    void BeginFrame(FrameContext& frame);
    SkinningRange AllocateMatrices(
        Span<const Matrix4x4> matrices);

    ResourceIndex GetSkinningBufferResourceIndex() const;
};
```

`SkinningRange`:

```cpp
struct SkinningRange
{
    uint32_t matrixOffset;
    uint32_t matrixCount;
};
```

### V1 Allocation Strategy

V1 permits the use of an Atomic Linear Cursor:

```text
Atomic Fetch-Add
↓
Reserve Matrix Range
↓
Parallel Copy
```

Purpose:

- Simple to implement
- Easy to verify
- Does not create per-character GPU buffers

However, Atomic Cursor is only the V1 baseline.

### Advanced Allocation Strategy

As the number of Workers increases, it may be changed to:

```text
Animation Jobs
↓
Collect Required Matrix Counts
↓
Prefix Sum / Batched Range Reservation
↓
Assign Non-overlapping Range Per Worker
↓
Parallel Copy
```

For example:

```text
Worker 0 → [0, 1200)
Worker 1 → [1200, 2700)
Worker 2 → [2700, 3900)
```

Advantages:

- No global atomic contention on the Hot Path
- Each Worker writes to an independent range
- Consistent with the Thread-local Frame Allocator strategy

### Capacity / Budget

Do not define:

```text
100000 matrices
```

as a permanent hard limit.

Use:

```text
Initial Capacity
Soft Budget
Hard Budget
Peak Usage
Overflow Policy
```

For example:

```cpp
struct SkinningBufferBudget
{
    uint32_t initialMatrixCapacity;
    uint32_t maxMatrixCapacity;
};
```

The profiler must track:

```text
Current Matrix Count
Peak Matrix Count
Capacity
Soft Budget
Hard Budget
Overflow Count
Upload Bytes
Copy Bytes
```

### Overflow Policy

Development:

```text
Assert
+
Profiler Warning
+
Capture offending workload
```

Shipping must not rely only on Assert.

Depending on the project strategy:

```text
Grow Next Frame
Temporary Fallback Allocation
Reduce Animation Update Rate
Apply Animation LOD
Drop Lower-priority Skinning Work
```

Only if recovery is truly impossible should it be Fatal.

### Animation LOD Integration

The Skinning Buffer Budget can be integrated with Animation LOD.

For example:

```text
Skinning Budget Pressure
↓
Far Character
↓
Lower Animation Update Rate
↓
Reduce Bone Evaluation
↓
Lower Skinning Matrix Usage
```

### Threading

Pose Evaluation:

```text
Worker-local
```

GPU Data Write:

```text
Per-worker reserved range
```

Rules:

- A Worker must not write to another Worker’s range.
- Do not share an unsynchronized linear cursor unless atomic reservation is used.
- All CPU Jobs must finish before resetting the Frame Slot.
- The corresponding Frame Fence must be awaited before GPU reuse.
- A frame-local mapped pointer must not escape into the next frame.

### GPU Resource Lifetime

Dynamic Skinning Buffer:

```text
ResourceManager / RHI
→ Owns GPU Resource

Animation / Renderer
→ Holds ResourceIndex / Range
```

Do not use:

```text
SharedPtr<GPUBuffer>
```

as Gameplay / Draw Item ownership.

### Profiler

Add to the profiler:

```text
Animation / Skinning
├─ Evaluated Characters
├─ Evaluated Bones
├─ Matrix Count
├─ Buffer Capacity
├─ Upload Bytes
├─ Copy Bytes
├─ Atomic Reservation Cost
├─ Worker Range Usage
├─ Overflow Count
└─ GPU Skinning Time
```

### V1 Definition of Done

- All character bone matrices can be packed into a shared Dynamic Skinning Buffer.
- Each draw stores only `skinningMatrixOffset`.
- Transform / Material / Mesh element indices are clearly separated from ResourceIndex.
- The Frames-in-flight slot is validated using a GPU fence before reuse.
- UMA / Discrete GPU can use different backend upload strategies.
- The Shader does not directly depend on the DX12-only `ResourceDescriptorHeap` contract.
- Slang accesses the skinning buffer through the Engine Resource Binding abstraction.
- V1 Atomic Linear Allocation operates correctly.
- The profiler can display matrix capacity / peak / overflow.
- Overflow has a strategy other than Assert-only in Shipping Build.

### Future

- Prefix-sum / Batched Range Reservation
- GPU Pose Evaluation
- Compute Skinning
- GPU Animation Sampling
- Mesh Shader / GPU-driven skinning integration

## Thirty-Nine, Physics

Use:
- Jolt Physics

Features:
- RigidBody
- Static Body
- Collider
- Trigger
- Raycast
- Character Controller
- Physics Scene

### Physics Execution Domain

Physics adopts a Hybrid CPU / GPU architecture; Gameplay-authoritative Physics is primarily CPU / Jolt-based, while the GPU handles workloads suitable for massively parallel computation, tolerant of latency, or purely visual.

```text
PhysicsExecutionDomain
├─ CPUAuthoritative
├─ CPUBatched
├─ GPUVisual
└─ GPUDeferred
```

Definitions:

```text
CPUAuthoritative
→ Gameplay RigidBody
→ Character Controller
→ Contact / Trigger
→ Immediate Hit / Query
→ Gameplay-critical Constraint

CPUBatched
→ Large CPU Query Batch
→ Job System
→ AI / Character / World Query

GPUVisual
→ VFX Particle Physics
→ Cloth
→ Rope / Chain
→ Debris
→ Visual Secondary Motion

GPUDeferred
→ Crowd / Background Query
→ Heightfield / SDF Query
→ One-or-more-frame latency allowed
```Formal Principles:

```text
Gameplay Physics
→ CPU / Jolt authoritative

Visual / Massive / Deferred Physics
→ GPU acceleration optional
```

Do not treat “GPU is faster” as the default answer for all Physics workloads; if the result must be immediately consumed by the CPU in the same frame, it must not be forcibly moved to the GPU and cause synchronization / readback stalls.

### GPU Physics Framework

GPU Physics is not another complete Gameplay Physics World, but an optional acceleration domain of the existing Physics Framework.

```text
Physics Framework
├─ CPU Physics Runtime
│  └─ Jolt Backend
└─ GPU Physics Runtime
   ├─ Particle
   ├─ Cloth
   ├─ Rope
   ├─ Debris
   ├─ SoftBody（later）
   └─ Deferred Query
```

GPU Physics data uses a dedicated pool / buffer and does not create large numbers of Scene Entities / Components.

```text
GPUPhysicsPool
├─ Position[]
├─ PreviousPosition[]
├─ Velocity[]
├─ Constraint[]
├─ CollisionData[]
├─ ActiveIndices[]
└─ Optional CustomData[]
```

### GPU Physics Simulation Flow

Typical Compute flow:

```text
Spawn / Initialize
↓
Integrate
↓
Collision
↓
Constraint Solve
↓
Compaction
↓
Output Buffer
↓
Render / VFX / Secondary Motion
```

GPU Physics Passes must be managed by RenderGraph / GPU scheduler for resource lifetime, barriers, queue ownership, and async compute opportunities.

The Physics subsystem must not privately submit GPU work that is not tracked by RenderGraph / scheduler.

### GPU Collision Sources

GPU Visual Physics may use the following according to function and platform:

```text
Depth Buffer Collision
Heightfield Collision
SDF Collision
Simplified Scene Collision
```

Collision sources must be explicitly marked with their precision and gameplay authority.

```text
GPU Visual Collision
≠
Gameplay Authoritative Collision
```

### GPU Cloth

GPU Cloth foundation is officially supported.

Flow:

```text
Character Skeleton / Attachment
↓
Cloth Attachment Points
↓
GPU Cloth Simulation
↓
Constraint Solve
↓
Collision
↓
Final Cloth Vertices
↓
Render
```

Applicable to:

```text
Cape
Skirt
Sleeve
Flag
Long Cloth
Hair / Accessory mesh where appropriate
```

GPU Cloth is visual secondary simulation by default; if cloth state directly determines gameplay collision / rules, a CPU-authoritative representation or dedicated gameplay proxy must exist.

### GPU Rope / Chain

Purely visual Rope / Chain may use GPU particles + distance constraint solver.

```text
Rope Particles
+
Distance / Bend Constraints
↓
GPU Solver
```

If the Rope participates in:

```text
Gameplay Pulling
Character Suspension
Mechanism Activation
Force Feedback
```

it must not rely solely on the GPU visual result as authority.

### GPU Debris

Large quantities of visual debris do not create large numbers of Jolt RigidBodies.

```text
Explosion
↓
GPU Debris Pool
↓
Position / Velocity
↓
Depth / Heightfield / SDF Collision
↓
Indirect Render
```

Only gameplay-relevant debris creates CPU / Jolt bodies.

The following may be evaluated later:

```text
GPU Debris
↓ Player / Gameplay relevance
Promote to CPU Jolt Body
↓
CPU Authoritative

Far / irrelevant again
↓
Demote / Destroy
```

Promotion / Demotion is not a required V1 capability.

### Immediate Query vs Deferred GPU Query

Physics Queries are divided into:

```text
ImmediateQuery
→ CPU / Jolt
→ Same-frame result

DeferredBatchQuery
→ CPU Job or GPU
→ Result may arrive later
```

ImmediateQuery is used for:

```text
Player Movement
Jump / Grounding
Weapon Hit
Gameplay Collision
Character Controller
```

Deferred GPU Query may be used for:

```text
Background Crowd
Vegetation Interaction
Ambient Agents
Non-critical Ground Probe
Large Heightfield / SDF Query
```

Queries requiring same-frame Gameplay decisions must not be placed on the GPU readback path.

### Batch Query

Consistent with the existing Zig Stable C ABI / Batch-first principles, Physics supports:

```text
RaycastBatch
SphereCastBatch
CapsuleCastBatch
BoxCastBatch
OverlapBatch
DeferredQueryBatch
```

Avoid:

```text
for each NPC
    Zig → Engine ABI → Raycast()
```

Prefer:

```text
Zig / Gameplay
↓
QueryBatch
↓
Physics Job / GPU Deferred Path
↓
ResultBatch
```

### GPU Broadphase Policy

GPU Broadphase / GPU RigidBody World are not core V1 capabilities.

Reasons:

```text
CPU ↔ GPU synchronization
Readback latency
Debug complexity
Platform divergence
Mobile GPU / thermal pressure
```

For hundreds to thousands of gameplay rigid bodies, prioritize the Jolt multi-threaded CPU path.

Future R&D:

```text
AABB[]
↓
GPU Broadphase
↓
Candidate Pairs
↓
CPU or GPU Narrowphase
```

Introduce this only after actual profiling proves that CPU broadphase has become a bottleneck.

### Mobile / Thermal Policy

GPU Physics must integrate with the existing `PerformancePolicyManager`.

Adjustable parameters:

```text
Simulation Rate
Solver Iterations
Particle / Cloth Count
Collision Tier
GPU Debris Count
Deferred Query Budget
Async Compute Usage
```

Example:

```text
High
→ Full GPU Cloth
→ Higher solver iterations

Medium
→ Lower simulation rate
→ Fewer iterations

Low
→ Simplified spring / bone motion
→ Disable expensive GPU Cloth / Debris
```

Mobile devices must not become unstable because GPU Physics causes Rendering / VFX / Animation / Skinning to compete for the same GPU budget.

### CPU / GPU Authority Boundary

All Physics data must be marked with its authority.

```text
CPU Authoritative
→ Gameplay Truth

GPU Visual
→ Presentation / Secondary Motion

GPU Deferred
→ Advisory / Delayed Query Result
```

Prohibited:

```text
GPU visual result
→ silently becomes gameplay truth
```

If a GPU result must be returned to Gameplay, it must go through an explicit readback queue / result generation / frame latency contract.

### GPU Readback Contract

Profiler / Runtime must track:

```text
Readback Bytes
Readback Count
Readback Latency
GPU→CPU Stall Count
```

Physics APIs must not allow hidden synchronous readback.

```text
Request GPU result
↓
Async Result Handle / Deferred Result Buffer
↓
Consume only when ready
```

If the caller requires a same-frame immediate result, it must use the CPU Query path.

### GPU Physics Memory

GPU Physics Memory is included in the global Memory Budget:

```text
GPU Physics Memory
├─ Particle State
├─ Cloth State
├─ Constraint Buffer
├─ Collision Data
├─ Debris State
├─ Deferred Query Buffer
├─ Readback Buffer
└─ Indirect Args
```

Under Memory Pressure, the following may be performed:

```text
Reduce non-critical cloth particles
Reduce solver iterations
Reduce debris count
Disable distant GPU visual simulation
Reduce deferred-query budget
```

CPU-authoritative Gameplay Physics correctness must not be compromised by Memory Pressure.

### GPU Physics Profiler

The Physics Profiler separates CPU / GPU categories.

```text
Physics CPU
├─ Jolt Step
├─ Broadphase
├─ Narrowphase
├─ Solver
├─ Active / Sleeping Bodies
├─ Contacts
├─ Constraints
└─ Queries

Physics GPU
├─ Particle Simulation
├─ Cloth
├─ Rope
├─ Debris
├─ GPU Collision
├─ Deferred Queries
├─ Solver Iterations
├─ GPU Time
├─ Readback Bytes
└─ Readback Latency
```

### GPU Physics V1 Scope

V1:

```text
✓ CPU Jolt Physics
✓ CPU Batch Query
✓ GPU VFX Collision
✓ GPU Cloth foundation
✓ GPU Debris optional
✓ GPU Deferred Query interface
```

Later:

```text
△ GPU Broadphase
△ GPU Soft Body
△ GPU Crowd Physics
△ GPU RigidBody Simulation
△ Physics Promotion / Demotion
```

GPU RigidBody World must not block V1.


### Character Framework

Character control is not treated as a single Physics `CharacterController` Component, but is formally divided into:

```text
Input / AI / Gameplay
↓
CharacterIntent
↓
CharacterMotor
↓
CharacterController
↓
Physics Query / Collision Resolve
↓
CharacterMotionResult
↓
Scene Transform
+
Animation Parameters
+
Gameplay Events
```

Core principle:

```text
CharacterController
≠
Movement Gameplay Logic
```

`CharacterController` is responsible for collision, ground, slopes, steps, penetration recovery, and platform interaction; `CharacterMotor` is responsible for movement feel, speed, jumping, gravity, external forces, Root Motion, and Facing Policy.

#### CharacterController Responsibilities

Formal responsibilities:

```text
CharacterController
├─ Shape
├─ Collision Layer / Mask
├─ Ground Detection
├─ Slope Handling
├─ Step Handling
├─ Ground Snap
├─ Penetration Recovery
├─ Moving Platform Support
├─ Dynamic Body Interaction
└─ Motion Resolve
```

Primary inputs:

```text
Current Transform
Desired Displacement
Desired Rotation
Delta Time
```

Primary outputs:

```text
Resolved Position
Resolved Rotation
Actual Displacement
Resolved Velocity
Ground State
Ground Normal
Ground Body
Ground Point Velocity
Hit Result[]
```

The Controller is not responsible for:

```text
Walk Speed
Run Speed
Sprint Rules
Stamina
Attack State
Skill State
```

The above belong to Gameplay / CharacterMotor.

#### CharacterMotor

`CharacterMotor` is responsible for character movement policy.

```text
CharacterMotor
├─ Ground Acceleration
├─ Ground Deceleration
├─ Air Acceleration
├─ Gravity
├─ Jump
├─ Max Speed
├─ Crouch
├─ Sprint
├─ External Velocity
├─ Root Motion Integration
└─ Facing Policy
```

Different Motor implementations may exist:

```text
ActionCharacterMotor
FPSCharacterMotor
PlatformerMotor
SwimmingMotor
FlyingMotor
AICharacterMotor
```

Different games may share the same CharacterController while replacing the CharacterMotor.

#### CharacterIntent

Zig Gameplay / AI should prioritize submitting POD `CharacterIntent` rather than directly executing fine-grained Physics Queries.

Concept:

```cpp
struct CharacterIntent
{
    float moveX;
    float moveY;
    float facingX;
    float facingY;
    uint32_t flags;
};
```

Flags may represent:

```text
Jump
Crouch
Sprint
Dash
```

Flow:

```text
Zig / AI
↓
CharacterIntent
↓
Native Character System
↓
Batch Update
```

Zig must not repeatedly execute the following across the ABI for every character every Frame:

```text
Raycast
GroundCheck
MoveCapsule
ResolveWall
StepUp
```

The existing Batch-first Stable C ABI principle should be maintained.

#### Runtime Data-Oriented Layout

The Editor may use:

```text
Node
├─ Transform
├─ CharacterController
├─ Animator
└─ Gameplay
```

The Runtime underlying layer uses a Data-Oriented Pool:

```text
CharacterControllerPool
├─ EntityID[]
├─ CharacterHandle[]
├─ ShapeHandle[]
├─ Position[]
├─ Velocity[]
├─ DesiredVelocity[]
├─ GroundState[]
├─ GroundNormal[]
├─ GroundBody[]
├─ GroundVelocity[]
├─ ConfigIndex[]
├─ Flags[]
└─ ...
```

Prohibited:

```text
N CharacterController Objects
→ N virtual Update()
```

Prefer:

```text
CharacterSystem
→ Process Character Batch
```

#### Jolt Backend Mapping

Gameplay / Public API must not expose Jolt-specific types.

```text
Engine CharacterController
↓
Character Backend Adapter
↓
Jolt Character / CharacterVirtual-style Path
```

Player / NPC Characters use the Character Controller path by default; only general physics objects use Dynamic RigidBody.

```text
Character
≠
Dynamic RigidBody
```

Gameplay sees only:

```text
CharacterControllerHandle
CharacterMoveRequest
CharacterMotionResult
```

#### Ground State

A single `bool grounded` must not be used as the complete ground state.

Formal states:

```text
CharacterGroundState
├─ InAir
├─ OnGround
├─ OnSteepGround
├─ Sliding
└─ Unsupported
```

Also provide:

```text
GroundNormal
GroundDistance
GroundBody
GroundPointVelocity
```

A character touching a slope does not necessarily mean that it can stand on it.

#### Slope Handling

Character Config must contain at least:

```text
MaxWalkableSlope
```

Flow:

```text
Ground Contact
↓
Slope Test
├─ Walkable
└─ Too Steep
   ↓
   Sliding / Unsupported
```

Do not determine Grounded solely by whether the “collision point is beneath the feet.”

#### Step Up / Step Down

The following are officially supported:

```text
StepHeight
StepForwardDistance
GroundSnapDistance
```

Flow:

```text
Forward Motion
↓
Hit Low Obstacle
↓
Can Step?
├─ No → Block
└─ Yes
   ↓
   Move Up
   ↓
   Move Forward
   ↓
   Move Down
```

Used for:

```text
Stair
Curb
Small Rock
Small Height Difference
```

#### Ground Snap

When a character walks down a small slope / stair, repeated transitions should be avoided:

```text
Airborne
Grounded
Airborne
Grounded
```

When:

```text
Character moving downward
+
Ground within snap distance
```

Ground Snap may be executed.

If the character is:

```text
Jumping upward
```

Ground Snap is temporarily suspended.

#### Moving Platform

Moving Platform is a required V1 character-control capability.

Supported:

```text
Elevator
Moving Platform
Rotating Platform
Ship
Vehicle Surface
```

The CharacterController must obtain:

```text
GroundBody
GroundLinearVelocity
GroundAngularVelocity
GroundPointVelocity
```

Actual character movement:

```text
Own Motion
+
Platform Point Motion
↓
Resolved Motion
```

Rotating platforms must use contact-point velocity and must not only add world translation.

#### Terrain Streaming Boundary Contract

The Character Controller is a persistent kinematic object, unlike Particles / Projectiles, which can tolerate brief disappearance or delayed spawning; when crossing Terrain / World Partition Streaming Cell boundaries, the existing rule that “static bodies follow Chunk load/unload” cannot simply be reused. An independent contract is required.

Formal principle:

```text
Character Physical Footprint
= Capsule Bounds + Safety Margin
  (Ground Probe Distance, Max Step, Max Fall Distance)
↓
Occupied Cell Set
```

The Physics Collision Representation (HeightFieldShape / Static Body) within the Occupied Cell Set is considered Pinned, independently of that Cell’s Render / Vegetation Residency—even if the visual LOD has been downgraded or unloaded due to distance, the Collision beneath the character must not be reclaimed. Physics Collision Residency and Render Residency are tracked separately, following the existing Double Budget principle of Streaming Residency; the two must not be coupled.

The Cell Unload rule is formally revised to:

```text
Static Physics Body Unload conditions
= Cell is not in any Character's Occupied Cell Set
AND
  Cell is not in any Character's Adjacent Prefetch Set
```

While a character remains in the Occupied Cell Set, that Cell’s HeightFieldShape must not enter `PendingUnload` / `Evicting`.

Cell Load rules (when the character moves faster than Streaming can complete, such as teleporting or rapidly moving into an unloaded area):

```text
Character enters a Cell that is not yet Ready
↓
CharacterGroundState = StreamingPending
↓
Suspend normal Ground Snap / Step / Slide evaluation
↓
Use Last-Known Ground (if available) or Hold Position (if unavailable)
↓
Do not Free Fall directly because there is currently no Collision
```

`CharacterGroundState` formally adds a sixth state:

```text
CharacterGroundState
├─ InAir
├─ OnGround
├─ OnSteepGround
├─ Sliding
├─ Unsupported
└─ StreamingPending      ← Added: Ground Cell is not yet Ready
```

Character Movement is formally designated as a High-priority Streaming Source, connecting to the existing Streaming Priority Preemption / IO Concurrency Contract: requests for Occupied Cells and Adjacent Prefetch Cells have higher priority than general Camera Frustum Streaming Demand.

The Teleport Contract likewise specifies: if the Teleport destination Cell is not Ready, a synchronous / high-priority Load must first be triggered, and `StreamingPending` must be maintained until loading is complete; the character must not be placed directly at a world coordinate without Collision.

V1 scope limitation: this contract covers only Persistent Kinematic Characters (Player / NPC, using the CharacterController path); non-gameplay-authoritative objects such as GPU Physics / Ragdoll / Debris are not applicable.

#### Jump Policy

Jump gameplay policy belongs to CharacterMotor.

```text
Grounded
+
Jump Request
↓
Detach Ground
↓
Vertical Velocity
↓
CharacterMotor
↓
CharacterController Resolve
```

Common features:

```text
Jump Buffer
Coyote Time
```

These may be implemented by the Gameplay Motor and are not hard-coded into the Physics backend.

#### Gravity Ownership

Gravity velocity is managed by CharacterMotor / CharacterState:

```text
VerticalVelocity += Gravity * dt
```

The final displacement is still handed to the CharacterController for collision resolution.

Principle:

```text
Motor
→ owns desired motion

Controller
→ owns collision resolution
```

#### Desired vs Resolved Velocity

The Runtime must distinguish between:

```text
DesiredVelocity
ResolvedVelocity
```

For example, when hitting a wall:

```text
DesiredVelocity = 5 m/s
ResolvedVelocity = 0 or wall-slide velocity
```

Animation should generally use `ResolvedVelocity` to prevent the character from continuing to play a full-speed run animation while hitting a wall.

#### Root Motion Integration

Formally integrate with the Animation Framework’s `RootMotionDelta`:

```text
Animation RootMotionDelta
+
Gameplay Motor Motion
+
External Velocity
+
Gravity
↓
CharacterMoveRequest
↓
CharacterController
↓
Physics Resolve
↓
CharacterMotionResult
```

The Controller must output:

```text
RequestedMotion
ActualMotion
MotionError
```

For example:

```text
Requested Root Motion = 2.0 m
Actual Motion = 0.6 m
```

When there is a wall ahead, Animation Root Motion must not pass through the collision.

`MotionError` may be provided to:

```text
Animation
Motion Warping
Gameplay
```

#### External Velocity / Knockback

A character can still be affected by the following without being a Dynamic RigidBody:

```text
Explosion
Knockback
Launch
Moving Platform
Gameplay Force
```

CharacterMotor maintains:

```text
ExternalVelocity
```

Composition:

```text
Locomotion Velocity
+
External Velocity
+
Root Motion
+
Gravity
↓
CharacterController
```

It is not required to temporarily switch to a Dynamic RigidBody for Knockback.

#### Character ↔ Dynamic Body Interaction

Support:

```text
Character pushes Dynamic Body
Dynamic Body pushes Character
```

Character Config may contain:

```text
CanPushBodies
CanBePushed
MaxPushForce
MassInteractionPolicy
```

When a character pushes a physics object, impulse / force must be limited to prevent the character from unconditionally pushing objects with extremely large mass.

When a Dynamic Body hits a Character, the Controller must handle:

```text
External Body Velocity
Penetration Recovery
Character Displacement
```

It must not assume that a Character is always an immovable obstacle.

#### Crouch / Capsule Resize

Crouching is not merely Animation.

```text
Standing Capsule
↓
Crouch
↓
Short Capsule
```

Standing up:

```text
Check Overhead Clearance
↓
Enough Room?
├─ Yes → Standing Capsule
└─ No  → Keep Crouching
```

Shape Resize should preserve the feet position by default, rather than scaling around the capsule center in place and causing the character’s feet to float.

#### Teleport Contract

Formally distinguish:

```text
Move()
Teleport()
```

Teleport:

```text
Set Position
↓
Reset or Preserve Velocity by Policy
↓
Invalidate Ground Support
↓
Re-evaluate Collision
```

Large displacements must not automatically be treated as ordinary Character Movement.

#### Facing Policy

Character facing is separated from movement direction.

```text
FacingPolicy
├─ MovementDirection
├─ CameraDirection
├─ TargetDirection
├─ RootMotion
└─ ExplicitGameplay
```

The CharacterController is responsible only for collision-shape orientation; Gameplay / CharacterMotor determines the character’s visual / gameplay Facing.

Do not hard-bind:

```text
Facing == Velocity Direction
```

#### Fixed Tick / Render Interpolation

CharacterController integrates with the Physics Fixed Tick:

```text
Input / AI
↓
CharacterIntent Buffer
↓
Fixed Physics Tick
↓
CharacterMotor
↓
CharacterController
↓
Resolved Character State
↓
Render Interpolation
```

The Character Transform stores:

```text
PreviousPhysicsTransform
CurrentPhysicsTransform
```

Rendering uses interpolation to prevent visual character jitter when Physics runs at 60 Hz and Rendering at 90/120 Hz.

#### Networking-ready State

V1 does not implement complete Networking, but the Character Framework must not block future prediction / re-simulation.

The state must be able to form a clear snapshot:

```text
CharacterMotorState
├─ Position
├─ Rotation
├─ Velocity
├─ DesiredVelocity
├─ ExternalVelocity
├─ GroundState
├─ GroundBody
└─ Motor-specific State
```CharacterMotor does not depend on extensive hidden global state, facilitating the following in the future:

```text
Input
↓
Prediction
↓
Character Motor
↓
Re-simulation
```

#### Character Query Budget / LOD

A large number of NPCs must not execute unlimited Queries multiple times per character.

Support Character LOD / Query Budget:

```text
Near
→ Full Ground / Step / Interaction

Mid
→ Standard Controller

Far
→ Reduced Query / Lower Tick

Very Far
→ Navigation Proxy / No Detailed Controller
```

However:

```text
Player
Boss
Gameplay-critical NPC
```

must not be automatically downgraded due to distance into an incorrect Gameplay Simulation.

Character Priority / LOD must be integrated with the existing PerformancePolicyManager.

#### Navigation Integration

Navigation / NavMesh must not directly modify the Transform.

Formal flow:

```text
Navigation
↓
Desired Velocity
↓
CharacterIntent / CharacterMotor
↓
CharacterController
↓
Actual Movement
```

Avoid:

```text
Nav Agent
→ Direct Set Transform
```

Player / AI may share the same Character collision / movement pipeline.

#### Animation Integration

The Character System outputs:

```text
Speed
PlanarSpeed
VerticalVelocity
Grounded
GroundState
Slope
Acceleration
TurnRate
DesiredDirection
ActualDirection
```

The Animator consumes:

```text
CharacterMotionState
↓
Animation Parameters
```

Animation must not directly become the Gameplay movement authority.

Root Motion is an explicit exception, but it must still be resolved through the CharacterController.

#### Character Controller Component / Config

The editor-facing CharacterControllerComponent must contain at least:

```text
Shape
├─ Capsule Radius
├─ Standing Height
└─ Crouching Height

Collision
├─ Layer
├─ Mask
└─ Contact Offset

Ground
├─ Max Slope
├─ Step Height
├─ Ground Snap
└─ Ground Probe

Interaction
├─ Push Bodies
├─ Can Be Pushed
└─ Push Force Policy
```

The Gameplay side may use:

```text
CharacterMotorComponent
CharacterMotorState
```

The Engine provides a standard Motor implementation; the project may create custom Motor policies through Zig Gameplay.

#### Character Framework Profiler

The Profiler must track at least:

```text
Active Characters
Character Fixed Tick Time
Ground Query Count
Step Query Count
Character Batch Count
Moving Platform Count
Push Interaction Count
Penetration Recovery Count
Character LOD Distribution
Reduced Query Count
Root Motion Requested / Actual Distance
Motion Error
Character ABI Batch Count
```

#### Character Framework CI / Validation

CI must include at least:

- Ground / InAir / Steep Ground state transitions.
- Slope Limit.
- Step Up / Step Down.
- Ground Snap.
- Jump detach / landing.
- Moving translation platform.
- Rotating platform contact-point velocity.
- Dynamic Body pushes Character.
- Character pushes Dynamic Body.
- Crouch / stand clearance.
- Capsule resize feet preservation.
- Teleport vs Move semantics.
- Root Motion collision clamp.
- Desired / Resolved velocity correctness.
- Fixed Tick + Render interpolation regression.
- Character Batch ABI.
- Character LOD must not downgrade gameplay-critical character.
- Jolt backend-specific types must not leak into Gameplay / Stable C ABI.
- When a Character standing at a Streaming Cell boundary triggers Chunk unload, Collision must not be removed (Occupied Cell Pinned validation).
- Teleporting / high-speed movement into a Cell that is not yet Ready must enter `StreamingPending`, rather than directly entering Free Fall or passing through geometry.

#### Character Framework V1 Definition of Done

V1 must complete at least:

```text
CharacterIntent
+
CharacterMotor
+
CharacterController
+
Jolt Backend Adapter
+
Ground / Slope
+
Step Up / Down
+
Ground Snap
+
Jump
+
Moving Platform
+
Root Motion Resolve
+
External Velocity / Knockback
+
Dynamic Body Interaction
+
Crouch / Resize
+
Teleport
+
Fixed Tick
+
Render Interpolation
+
Terrain Streaming Boundary Contract
+
Batch-first Zig ABI
```

Final architectural relationship:

```text
Input / AI
↓
CharacterIntent
↓
CharacterMotor
├─ Locomotion
├─ Jump
├─ Gravity
├─ External Velocity
├─ Root Motion
└─ Facing
↓
CharacterController
├─ Capsule Collision
├─ Ground
├─ Slope
├─ Step
├─ Snap
├─ Moving Platform
├─ Dynamic Body Interaction
└─ Penetration Recovery
↓
Jolt Physics
↓
CharacterMotionResult
├─ Actual Position
├─ Actual Velocity
├─ Ground State
├─ Ground Normal
├─ Ground Body
└─ Hit Events
↓
Scene Transform
+
Animation
+
Gameplay
```

Formal architectural decisions:

```text
CharacterController
→ Native Core System

CharacterMotor
→ Replaceable Gameplay Policy
```


### Terrain Collision

Terrain Collision uses Jolt `HeightFieldShape`.

Source of Truth:

```text
Terrain Heightmap
= Persistent / Authoritative Height Source

        ↓ Build / Runtime Chunk Resolve

Jolt HeightFieldShape
= Physics Derived Representation
```

Maintaining another independently editable Physics Heightmap is prohibited. After the Terrain Heightmap is modified, only the Physics HeightField representation of affected Chunks is rebuilt or updated. Physics collision chunks are aligned with Terrain streaming chunks; when a Chunk is unloaded, the corresponding static physics body is removed, and when loaded, the corresponding HeightFieldShape is created. Terrain Collision is static by default in V1. A Chunk occupied by a Kinematic Character is an exception; its unload/load sequence is additionally governed by the Character Framework’s Terrain Streaming Boundary Contract (see above), and does not follow the general static body rules.

## 40. Audio

Adopted:
- miniaudio

Features:
- 2D Audio
- 3D Positional Audio
- Listener
- Streaming Music
- Audio Groups
- Volume
- Loop
- Audio Residency Scope




### Audio Residency Scope

The Audio Resource Runtime formally supports `AudioResidencyScope`, used to manage the Runtime Residency / Lifetime of a group of Audio Resources, rather than creating multiple mutually independent physical Cache instances.

Core usage:

```text
Enter Scene / Zone / Encounter
↓
Create / Activate AudioResidencyScope
↓
Preload / Resolve Required Audio
↓
Runtime Play
↓
Leave Scene / Zone / Encounter
↓
Release AudioResidencyScope
↓
Only resources no longer referenced or pinned become evictable
```

Typical Scopes:

```text
AudioResidencyScope
├─ Global
├─ UI
├─ Music
├─ Scene
├─ Zone
├─ Character
├─ Encounter
├─ Cutscene
└─ Temporary
```

For example:

```text
Global
→ Common UI
→ Common Notification
→ Shared System SFX

Scene:Map01
→ Ambient
→ Environment SFX
→ Local Creature SFX
→ Scene-specific Voice

Encounter:Boss01
→ Boss Attack SFX
→ Boss Voice
→ Boss Music / Stingers

UI:Shop
→ Shop UI SFX
```

When leaving Map01:

```text
ReleaseResidencyScope(Scene:Map01)
```

must not affect Audio Resources still referenced by:

```text
Global
UI
Music
Player
Other Active Scope
```

#### One Registry, Multiple Residency Owners

Prohibited:

```text
Global Cache
Scene Cache
UI Cache
→ same AudioClip decoded / loaded independently multiple times
```

Formal architecture:

```text
                 AudioResourceRegistry
                         │
          ┌──────────────┼──────────────┐
          │              │              │
       Audio A        Audio B        Audio C
          ▲              ▲
          │              │
   ┌──────┴──────┐       │
   │             │       │
Global Scope   Scene Scope
```

There is only one canonical runtime entry for each actual Audio Resource; `AudioResidencyScope` only adds / removes Residency References.

The same AudioClip / AudioEvent may be referenced simultaneously by multiple Scopes.

```text
Scene Scope Release
↓
Scene Residency Ref--
↓
Other Scope Ref still exists?
├─ Yes → Keep Resident
└─ No  → Continue Pin / Eviction Check
```

#### Audio Residency Scope Handle

Runtime uses a generation-based handle:

```text
AudioResidencyScopeHandle
```

Conceptual flow:

```text
Create Scope
↓
Associate Audio Assets / Events
↓
Preload / Load
↓
Use
↓
Release Scope
```

Scope Release does not mean immediate free:

```text
Release Scope
→ remove residency ownership
→ resource becomes evictable only when safe
```

#### Resource Pinning

Active Voice, Streaming, Preload transactions, and similar operations must be able to pin Audio Resources.

Track at least:

```text
ScopeRefCount
VoicePinCount
StreamingPinCount
PreloadPinCount
```

Basic Eviction conditions:

```text
CanEvict =
ScopeRefCount == 0
&& VoicePinCount == 0
&& StreamingPinCount == 0
&& PreloadPinCount == 0
```

Therefore:

```text
Scene Scope Released
+
Voice still playing
→ Audio Resource remains valid
```

Memory still used by the Audio Thread / Active Voice must not be released directly due to a scene transition.

#### Scope Release Policy

Scope Release supports explicit policies:

```text
AudioScopeReleasePolicy
├─ StopImmediately
├─ FadeOutAndRelease
├─ LetActiveVoicesFinish
└─ DetachActiveVoices
```

Recommended defaults:

```text
Scene / Zone
→ FadeOutAndRelease

Temporary OneShot
→ LetActiveVoicesFinish

UI / Global Shared
→ LetActiveVoicesFinish or explicit policy
```

`FadeOutAndRelease`:

```text
Release Scope
↓
Reject new play from released scope
↓
Active Voices
→ Fade Out
↓
Stop
↓
Voice Pin released
↓
Unused Resources become evictable
```

Fade must be executed by the Audio Runtime / Audio Thread, not by Gameplay manually modifying volume every Frame.

#### Residency Data Granularity

Audio Residency must not only track “whether the file is loaded”; it should distinguish:

```text
Audio Runtime Residency
├─ Event Metadata
├─ Compressed Resident Data
├─ Decoded PCM
├─ Streaming Read Buffer
├─ Decode Buffer
└─ Preload State
```

Formal support:

```text
AudioResidencyMode
├─ MetadataOnly
├─ Compressed
├─ DecodedPCM
└─ Streaming
```

Typical usage:

```text
UI Click
→ DecodedPCM

Common Short SFX
→ DecodedPCM / Compressed by profile

Dialogue
→ Compressed / Streaming

BGM
→ Streaming
```

#### Scene Transition / Preload

Audio Residency Scope is formally integrated with Scene transition.

```text
Current:
Scene:Map01

Preload:
Scene:Map02
```

Transition:

```text
Create Scope(Map02)
↓
Preload Required Audio
↓
Required Audio Ready
↓
Activate Map02
↓
Release Scope(Map01, FadeOutAndRelease)
```

This avoids synchronously decoding / loading important Audio only when it is played for the first time in the new scene.

#### Nested / Sub-scopes

Logical parent-child Scopes are supported:

```text
Scene:Map01
├─ Ambient
├─ NPC
├─ BossArea
└─ Cutscene01
```

For example:

```text
Enter Boss Area
↓
Activate BossArea Scope
↓
Preload Boss Audio

Boss Encounter End
↓
Release BossArea Scope
```

It is not necessary to wait for the entire Scene to unload.

Parent-child relationships primarily serve tools, batch preload / release, and the Profiler; the actual Resource Registry remains based on canonical resources + ref/pin tracking.

#### Bundle Boundary

`AudioResidencyScope` is not equivalent to a Bundle.

```text
Bundle
→ Physical Packaging / Download / Versioning

AudioResidencyScope
→ Runtime Residency / Lifetime Policy
```

An Audio Scope may establish a preload set through Bundle dependencies, but the same Audio Asset may still be shared by other Bundles / Scopes.

The Scope design must not create a second Asset Identity system.

#### Hot Update / Generation Pinning

Audio Residency Scope must comply with the existing Bundle Generation Pinning.

For example:

```text
AudioClip Generation N
↓
Active Voice
```

Bundle update:

```text
Generation N+1 activated
```

Then:

```text
New Play
→ N+1

Existing Voice
→ continues N until finished / released
```

Only after all of the following from the old generation reach zero may reclamation occur:

```text
Scope Ref
Voice Pin
Streaming Pin
Load Context
```

#### Residency Priority

A Scope may carry a residency priority:

```text
AudioResidencyPriority
├─ Pinned
├─ Critical
├─ High
├─ Normal
├─ Low
└─ Speculative
```

Examples:

```text
Global UI
→ Pinned

Player
→ Critical

Current Scene
→ High

Next Scene Preload
→ Normal

Far Zone
→ Low

Speculative Prefetch
→ Speculative
```

Memory Pressure eviction may prioritize:

```text
Speculative
↓
Unused Far Zone
↓
Released Scene
↓
Low-priority Decoded PCM
```

However, Active Voice / Critical pinned data must not be evicted.

#### Memory Budget Integration

Audio Residency Scope is the ownership / residency layer of the existing global Audio Memory Budget.

The Profiler / Budget must classify at least:

```text
Audio Memory
├─ Event Metadata
├─ Resident PCM
├─ Compressed Audio
├─ Streaming Buffers
├─ Decode Buffers
├─ Voice State
└─ Backend Memory
```

Memory Pressure may:

```text
Release low-priority preload refs
↓
Evict unused decoded PCM
↓
Evict unused compressed data
↓
Keep metadata where useful
```

“Clear the entire Audio Cache” must not be used as a general Memory Pressure strategy.

#### Audio Residency Profiler

The Profiler adds:

```text
Audio Residency
├─ Scope Count
├─ Scope Name / Type
├─ Scope Priority
├─ Resource Count
├─ PCM Memory
├─ Compressed Memory
├─ Streaming Memory
├─ Active Voice Pins
├─ Streaming Pins
├─ Preload Count
├─ Evictable Memory
└─ Pending Release
```

The Editor may provide a Scope View, for example:

```text
Scope              PCM       Compressed    Voices
--------------------------------------------------
Global             12 MB       3 MB           5
Player              8 MB       4 MB           7
Scene:Map01        34 MB      18 MB          21
Boss01             15 MB       8 MB           0
Map02 Preload       6 MB      11 MB           0
```

#### Audio Residency CI / Validation

At minimum, verify:

- When the same Audio Asset is referenced by multiple Scopes, only one canonical runtime resource exists.
- Releasing one Scope does not unload a resource still used by other Scopes.
- An Active Voice pin prevents premature unload.
- A Streaming pin prevents streaming data from being released prematurely.
- The resource is not reclaimed before `FadeOutAndRelease` completes.
- A released Scope does not accept new play requests.
- Scene A → Scene B preload / release does not produce a synchronous audio load spike.
- Bundle Generation N / N+1 active voice pinning is correct.
- Memory Pressure evicts only Audio Resources that can be safely reclaimed.
- The Audio Thread does not execute blocking free / file IO / asset lookup due to Scope Release.

#### Audio Residency Scope V1 Definition of Done

V1 must complete at least:

```text
AudioResourceRegistry
+
AudioResidencyScopeHandle
+
Global / Scene / Character / UI Scope
+
Multiple-Scope Shared Resource Ref Tracking
+
Voice / Streaming Pin
+
Release Policy
+
FadeOutAndRelease
+
Residency Priority
+
Memory Budget Integration
+
Profiler
+
Bundle Generation Pinning
```

Formal lifecycle:

```text
Create Scope
↓
Preload / Load Resources
↓
Play
↓
Voice / Streaming Pins Resources
↓
Release Scope
↓
Optional Fade Out
↓
Remove Residency References
↓
Wait Active Pins
↓
Mark Resource Evictable
↓
Memory Budget / Cache Manager Evicts
```


## Audio Backend / FMOD Plugin

The Audio System adopts a replaceable Backend architecture.

Default:

```text
miniaudio
→ Built-in
→ Default
```

Optional:

```text
FMOD Plugin
→ Optional Professional Audio Backend
```

Core architecture:

```text
Gameplay
↓
AudioManager
↓
IAudioBackend
├─ MiniAudioBackend
└─ FMODBackend
```

Gameplay / Runtime UI / Scene System must not directly depend on the FMOD API.

Recommended shared API:

```cpp
AudioHandle Audio::Play(AudioEventID eventId);
void Audio::Stop(AudioHandle handle);
void Audio::SetVolume(AudioHandle handle, float volume);
void Audio::SetParameter(AudioHandle handle, StringView name, float value);
```

### FMOD Plugin

FMOD is integrated as a Local Installed / Build-time Plugin.

```text
Plugins/
└─ FMOD/
   ├─ Runtime/
   ├─ Editor/
   ├─ ThirdParty/
   └─ FMOD.plugin.json
```

The Plugin itself:

```text
Not through Remote Bundle
Not downloaded from CDN
Not used as Native Code Hot Update
```

The FMOD native library is deployed together with the platform through App Build / Packaging.

### FMOD Core / Studio

The FMOD Backend may be divided into:

```text
FMOD Core API
FMOD Studio API
```

If the project uses FMOD Studio, it may support:

- Event
- Mixer
- Snapshot
- Parameter
- Adaptive Music
- Bus / VCA
- Bank
- Live Update
- Profiler
- Event Browser
- Event Preview

### Editor Integration

The FMOD Editor Plugin may provide:

```text
FMOD Studio Project
Bank Builder
Event Browser
Event Preview
Bank Dependency View
Live Update
Profiler Integration
```

Inspector example:

```text
Audio Event

Event:
[event:/Character/Attack ▼]

Auto Play     [ ]
Loop          [ ]
3D Spatial    [✓]
```

Avoid requiring Gameplay / Designers to manually enter arbitrary Event strings.

### FMOD Asset Type

Banks generated by FMOD Studio should be managed as an independent Asset Type.

```text
FMOD Studio Project
↓
FMOD Build
↓
.bank
↓
Asset Database
↓
Bundle
```

The Asset Database should track:

- Bank UUID
- Bank Version
- Event Dependency
- Platform Variant
- Bundle Assignment

### FMOD Bank and Bundle

An FMOD Bank is an Asset / Data, and therefore may be managed by the Bundle System.

For example:

```text
audio_common.bundle
└─ Master.bank

map01_audio.bundle
└─ Map01.bank
```

Therefore:

```text
FMOD Native Library
→ Plugin / App Build

FMOD Bank
→ Bundle / Asset
→ Can use Remote Content Hot Update
```

When a Bank is hot-updated through a Bundle, it must still go through:

```text
Remote Manifest
↓
Download
↓
Hash Verification
↓
Atomic Commit
↓
Activate
```

The Runtime must handle the safe timing of Bank Reload / Unload to prevent an old Bank still in use by a Voice / Event Instance from being unloaded.

### Build Settings

Project Build Settings may provide:

```text
Audio Backend
[ MiniAudio ▼ ]

Options:
- MiniAudio
- FMOD
```

If FMOD is selected:

- Verify that the FMOD Plugin is installed.
- Verify that the SDK / Library is available.
- Verify the target-platform Library.
- Verify the Bank Build Output.
- Verify the License / Third-party Notice configuration.

If FMOD is unavailable:

```text
Build Fail
→ Explicit error
```

It must not silently fall back and cause audio content inconsistency.

### Backend Boundary

FMOD Native Types must not be exposed to Gameplay:

```text
FMOD::Studio::EventInstance
FMOD::Sound
FMOD::Channel
```

They may exist only in:

```text
FMODBackend
FMOD Plugin Internal
```

The upper layers uniformly use:

```text
AudioHandle
AudioEventID
AudioBusID
AudioParameter
```

### Platform Support

The FMOD Plugin is compiled only for platforms actually supported by the SDK and enabled by the project.

Targets:

```text
Windows
macOS
Android
iOS
```

Platform Backend / Library paths are managed by the Plugin Build Script.

### Memory / Streaming

The FMOD Backend must integrate with the Engine Memory Budget / Streaming diagnostics.

The Profiler must display at least:

- Loaded Banks
- Bank Memory
- Active Voices
- Virtual Voices
- Streaming Audio
- Audio CPU Time
- Decode Cost
- Audio Heap / Buffer Usage

Large BGM / Voice content should prioritize Streaming rather than being loaded entirely into RAM at once.

### Audio Profiler Integration

Engine Profiler:

```text
Audio
├─ Backend
├─ Active Voices
├─ Virtual Voices
├─ Playing Events
├─ Loaded Banks
├─ Streaming
├─ Decode CPU
└─ Memory
```

When using FMOD, additional development-tool capabilities may be provided for jumping to / correlating with the FMOD Studio Profiler.

### Licensing

miniaudio:

```text
Built-in Default
→ Maintain the License / Notice according to the actual locked version
```FMOD:

```text
Optional Third-party Plugin
→ FMOD License must be confirmed according to the actual project / commercial usage
```

Third-party manifest must record:

- FMOD Version
- SDK Source
- License Type
- Runtime / Editor Components
- Redistribution Requirements
- Notice Requirements

The Engine does not assume that all projects using FMOD are subject to the same licensing conditions.

### Future Audio Backend

`IAudioBackend` should allow the future addition of:

```text
WwiseBackend
CustomAudioBackend
PlatformSpecificBackend
```

However, V1 does not need to support multiple professional Middleware solutions simultaneously.

### V1 Scope

V1 Audio:

```text
MiniAudioBackend
→ Built-in / Default
```

FMOD:

```text
Optional Plugin
→ Architecture Ready
→ Integration when selected
```

FMOD Plugin completion criteria:

- FMOD Backend can be registered with AudioManager
- Build Settings can select FMOD
- FMOD Native Types do not leak into the Public API
- FMOD Studio Banks can be added to the Asset Database
- Banks can be packaged into Bundles
- Remote Bundles can update Banks
- Plugin Native Libraries do not use Remote Update
- The Editor can browse / select Events
- Runtime can play / stop Events
- Parameters can be configured
- Bank Load / Unload functions correctly
- The Profiler can display basic FMOD status

## Forty-One, Input Framework

Input is formally an independent Subsystem:

```text
engine::input
```

Core positioning:

```text
Engine Input
≠ Predefined Move / Jump / Attack systems
```

`Move / Look / Jump / Attack` may only appear in Samples / Documentation and must not become Engine Reserved Actions.

The Engine defines:

```text
Device
Control
Raw Event
Device State
Binding Infrastructure
Action Value Type
Context
Routing Layer
Processor Infrastructure
```

The Game / Developer defines:

```text
Action Names
Action IDs
Action Maps
Bindings
Gameplay Commands
Gameplay Meaning
UI Input Routing Policy
```

### Input Architecture

```text
Platform Input Backend
↓
Timestamped RawInputEvent Queue
↓
Input Device Manager
↓
Device / Control State
↓
┌──────────────────────────────────────────────┐
│ Developer may consume Raw / Device API here │
└──────────────────────────────────────────────┘
↓
Optional Binding / Action System
↓
Input Context / Input Routing Layer
↓
Immutable InputSnapshot / TickInput
↓
┌───────────────┬────────────────┬───────────────┐
│               │                │               │
Gameplay      Runtime UI       Editor         Tools
│               │                │
Game-defined   UI Event       Dear ImGui
Command
```

Input must allow developers to stop at and use any layer:

```text
Level 0 → Raw Input Event
Level 1 → Device / Control State
Level 2 → Optional Action Mapping
Level 3 → Game-defined Input Command
```

The `InputAction System` is a high-level convenience layer, not a mandatory Gameplay Interface.

### Platform Backend

Upper layers must not depend on:

```text
WM_KEYDOWN
WM_MOUSEMOVE
Android MotionEvent
UIKit Touch
NSEvent
GCController native object
```

Platform Backend:

```text
IInputBackend
├─ WindowsInputBackend
├─ MacOSInputBackend
├─ AndroidInputBackend
└─ IOSInputBackend
```

The Platform backend is only responsible for normalizing OS events into Engine Input Data.

### Input Device Model

V1 Devices:

```text
InputDevice
├─ Keyboard
├─ Mouse
├─ Touchscreen
├─ Gamepad
└─ Pen / Pointer（when supported by the platform）
```

Future / Custom:

```text
Steering Wheel
HOTAS
Dance Pad
MIDI Controller
VR Controller
Arcade Controller
Custom Hardware
```

Every physical device has:

```text
InputDeviceID
```

Do not assume that there is only Keyboard 0 / Gamepad 0.

### Input Control

The underlying unified structure is:

```text
InputDevice
└─ InputControl
```

Conceptual Control Paths:

```text
<Keyboard>/w
<Keyboard>/space
<Mouse>/delta
<Mouse>/buttonLeft
<Gamepad>/leftStick
<Gamepad>/buttonSouth
<Touchscreen>/primaryTouch
```

The Editor Binding UI may use `InputControlPath`; after Runtime Cook, stable IDs / compact binding data are used.

### Raw Input Event

Conceptual data:

```cpp
struct RawInputEvent
{
    InputDeviceID device;
    InputControlID control;
    InputEventType type;
    uint64_t timestamp;
    InputValue value;
};
```

Raw events must retain their timestamps. Platform callbacks must not directly call Gameplay / Zig.

Flow:

```text
OS Callback
↓
Bounded RawInputEvent Queue
↓
Input::BeginFrame
↓
Device State Update
↓
Action Resolve
↓
Immutable Snapshot
```

### Low-level Device API

Developers may completely bypass the Action System and directly obtain:

```text
KeyboardState
MouseState
GamepadState
TouchSnapshot
DeviceSnapshot
RawInputEvent Stream
```

For example, custom implementations of:

- Fighting game Command Buffers.
- Rhythm game timestamp judgement.
- Special input hardware.
- Custom RTS / Simulation input models.

Low-level flexibility does not mean a large number of small cross-ABI calls; Zig / other gameplay languages should still prioritize batch snapshot / data view usage.

### Keyboard Physical Input ≠ Text Input

The following must be formally separated:

```text
PhysicalKey / LogicalKey
→ Gameplay / Shortcut

Text Input / IME
→ Text Editing
```

`InputField` must not compose text by handling KeyDown itself.

IME must support at least:

```text
CompositionStart
CompositionUpdate
CompositionEnd
TextCommit
```

Traditional Chinese / Simplified Chinese / Japanese / English input method composition must be supported.

### Input Action System（Optional）

The Engine provides:

```text
InputAction
InputActionID
InputActionMap
InputBinding
InputProcessor
InputInteraction
InputContext
```

However, it does not provide fixed Gameplay Action enums.

Forbidden:

```cpp
enum class EngineInputAction
{
    Move,
    Jump,
    Attack
};
```

Authoring names may be:

```text
"character.walk"
"camera.orbit"
"vehicle.throttle"
"spell.cast"
"editor.frame_selection"
"custom.foo"
```

Cook / Runtime uses:

```cpp
struct InputActionID
{
    uint32_t value;
};
```

### Input Action Value Type

The Engine only defines data types:

```text
InputActionValueType
├─ Button
├─ Axis1D
├─ Axis2D
└─ Axis3D
```

It does not define Gameplay semantics.

For example:

```text
ship.throttle    → Axis1D
camera.orbit     → Axis2D
editor.move      → Axis3D
spell.cast       → Button
```

### Binding / Processor / Interaction

Binding:

```text
Simple Binding
Composite Binding
Modifier / Chord Binding
```

Built-in Engine Processors:

```text
DeadZone
Normalize
Scale
Invert
Clamp
ResponseCurve
Sensitivity
```

Analog Dead Zone must support at least:

```text
Axial
Radial
```

Developers may register custom `IInputProcessor` implementations.

Interactions may provide common behaviors:

```text
Press
Release
Hold
Tap
MultiTap
Repeat
```

However, games may implement Charge / ComboWindow / RhythmTiming and similar behaviors themselves; they are not required to use Engine Interactions.

### Action Source Resolve

The same Action may be bound to multiple device types simultaneously:

```text
Keyboard
Mouse
Gamepad
Touch
VirtualControl
CustomDevice
```

Multi-source Resolve Policy:

```text
InputSourceResolvePolicy
├─ LatestActive
├─ MaxMagnitude
├─ Priority
├─ AdditiveClamp
└─ Custom
```

Different physical sources may first apply their own processors before entering the Action Resolver.

For example:

```text
Mouse Delta
↓ MouseSensitivity

Gamepad RightStick
↓ DeadZone / Curve / Sensitivity

Touch Look Region
↓ TouchSensitivity

→ camera.look
```

Do not mechanically add raw Mouse Delta and raw Stick values directly.

### Multiple Devices Simultaneously

Formal Contract:

```text
Multiple Input Devices
→ May be active simultaneously
```

Not:

```text
One Active Device at a time
```

The same `InputUser` may simultaneously own:

```text
Keyboard
+
Mouse
+
Gamepad
+
Touchscreen
+
Virtual Controls
```

For example, in the same frame:

```text
Keyboard WASD
→ movement action

Mouse Delta
→ camera action

Gamepad Button
→ skill action
```

All may be active simultaneously.

```text
Device Activity
≠ Device Exclusivity
```

`LastActiveDevice` is only used as a UI Prompt / Glyph / Presentation Hint and must not automatically disable other devices.

### InputUser / Local Multiplayer

Formal structure:

```text
Physical Device(s)
↓
InputUser
↓
Action Map / Routing
↓
Player
```

For example:

```text
InputUser 0
├─ Keyboard
├─ Mouse
└─ Gamepad #1

InputUser 1
└─ Gamepad #2
```

One User may own multiple Devices; a Device disconnect only clears that Device's state and should not reset other Devices.

### Gamepad

The upper layer uses logical controls for Gamepads:

```text
South
East
West
North
LeftStick
RightStick
LeftTrigger
RightTrigger
```

UI Glyphs may display corresponding Xbox / PlayStation / Nintendo icons according to the actual Device Family.

### Mouse / Cursor

Formal support:

```text
CursorMode
├─ Normal
├─ Hidden
├─ Confined
└─ Locked
```

Relative Mouse Motion and Absolute Cursor Position must be separated.

FPS / Camera systems use platform-supported relative motion and must not fabricate it through `currentPosition - previousPosition`.

### Multi-touch

Multi-touch is a V1 core capability.

```text
Touchscreen
↓
Multiple Independent Pointer Streams
↓
PointerID 0..N
```

Each Touch must retain at least:

```text
PointerID
Position
Delta
StartPosition
TouchPhase
Pressure（if supported）
Timestamp
CaptureOwner
```

Formal definition:

```text
TouchPhase
├─ Began
├─ Moved
├─ Stationary
├─ Ended
└─ Cancelled
```

`PointerID` must remain stable throughout the Began → Ended / Cancelled lifetime; a Touch array index must not be used as a permanent identity.

Touch device capabilities:

```text
HasTouch
MaxTouchPoints
HasPressure
HasStylus
```

Do not hard-code a fixed maximum number of fingers; report it through platform capabilities.

### Per-pointer Capture / Ownership

Pointer Capture must be per `PointerID`:

```text
Pointer #0 → VirtualJoystick
Pointer #1 → CameraLookRegion
Pointer #2 → SkillButton
```

Formal definition:

```text
Each Pointer
→ One Owner at a time
```

Not:

```text
All Touch
→ One Global Owner
```

Widgets may declare:

```text
PointerAcceptance
├─ PrimaryOnly
├─ Single
└─ Multiple
```

UI / Gameplay / WebView / Virtual Control must follow the single-owner / no-duplicate-delivery contract.

### Gesture

Gestures are built on Pointer Streams:

```text
Raw Touch
↓
Pointer Events
↓
Gesture Recognizer
↓
Gesture Events
```

Supported gestures may include:

```text
Tap
DoubleTap
LongPress
Pan
Swipe
Pinch
Rotate
TwoFingerPan
```

Advanced Gestures may be conditionally included according to V1 schedule constraints, but the underlying Multi-touch / PointerID / Capture capabilities must be completed first.

### Virtual Control Framework

Virtual Control is a formal input source between the Runtime UI and Input System:

```text
Touch / Pointer
↓
Runtime UI
↓
VirtualControl
↓
Raw Virtual Value or User-defined InputAction
↓
Game-defined Input Logic
```

Formal types:

```text
VirtualControl
├─ VirtualJoystick
├─ VirtualButton
├─ VirtualDPad
├─ VirtualTrigger
└─ VirtualTouchRegion
```

Virtual Controls must not be tied to any Gameplay semantics.

Forbidden:

```text
VirtualJoystick = Move
VirtualButton = Attack
```

Formal definition:

```text
VirtualJoystick
→ Raw Axis2D output
or
→ User-selected Axis2D Action

VirtualButton
→ Raw Button output
or
→ User-selected Button Action
```

May be used for:

```text
character.move
camera.orbit
vehicle.steering
spell.direction
menu.radial_select
custom.foo
```

### VirtualJoystick

Supported modes:

```text
VirtualJoystickMode
├─ Fixed
├─ Floating
└─ Dynamic
```

Output:

```text
Vec2 [-1, +1]
```

Processing flow:

```text
Touch Position
↓
Relative to Joystick Center
↓
Inner Dead Zone
↓
Clamp Radius
↓
Normalize / Remap
↓
Response Curve
↓
Axis2D Output
```

Constraint:

```text
Circular
Square
Horizontal
Vertical
```

The default for a general character joystick is Circular, preventing diagonal magnitude > 1.

Supported features:

```text
ActivationRegion
RecenterOnRelease
Visual Return Tween
```

When released, the Gameplay Output must immediately return to zero; the visual thumb may separately Tween back to the center.

### VirtualButton / VirtualDPad / VirtualTouchRegion

`VirtualButton` may output:

```text
Pressed
Held
Released
```

It may also support Hold / Repeat / Charge source semantics, but the final Gameplay meaning is defined by the user.

`VirtualDPad` outputs Axis2D.

`VirtualTouchRegion` may be a transparent UIElement used for:

```text
Camera Look
Drag Direction
Touch Delta
Custom Gesture Region
```

### Virtual Controls + Multi-touch

Virtual Controls must fully support Multi-touch / Pointer Capture.

For example:

```text
Finger #0 → Left VirtualJoystick
Finger #1 → Right Camera Look Region
Finger #2 → Skill Button
Finger #3 → Another Skill Button
```

They may operate simultaneously.

Virtual Controls themselves are Runtime UI Elements and therefore naturally support:

```text
Anchor
Safe Area
UIScalePolicy
Orientation Layout
Visibility Policy
```

Optional:

```text
VirtualControlVisibilityPolicy
├─ Always
├─ TouchOnly
├─ Auto
└─ Manual
```

`Auto` only changes Presentation / Visibility and does not change whether other Devices are available.

### Virtual Control User Override

Developers may allow players to customize:

```text
Position
Scale
Opacity
Joystick Radius
Sensitivity
DeadZone
```

Save these to the User Input Profile rather than modifying the original UI / Input Asset.

### Input Context

`InputContext` controls which action maps / routing layers / input routes are currently active.

For example:

```text
Gameplay
↓
Inventory
↓
ModalDialog
```

The Context Stack may provide:

```text
InputConsumePolicy
├─ PassThrough
├─ ConsumeMatched
└─ ConsumeAll
```

Input Context resolves “who is currently authorized to receive input”; it is not equivalent to the UI Input Matrix.

### Input Layer / Routing Layer

The following coarse-grained concept is formally added:

```text
InputLayerID
```

It may be regarded as:

```text
Input Routing Layer
```

Its purpose is to determine which UI Layers / interaction domains Input may route to.

It is not an InputAction.

```text
InputAction
→ Fine-grained “what to do”

InputLayer
→ Coarse-grained “where it may be sent”
```

Do not create:

```text
MoveInput
JumpInput
AttackInput
Skill1Input
...
```

This action-per-layer pattern must not be used.

Input Layer names are entirely defined by the project, for example:

```text
PlayerGameplay
PlayerUI
TouchGameplay
WorldInteraction
Debug
Player1
Player2
CustomFoo
```

### UI Layer

UI Layers are also defined by the project and are responsible for Input Routing Targets; they are not equivalent to Render Layers.

```text
UI Render Layer
≠
UI Input Layer
```

For example:

```text
HUD
Menu
Modal
WorldUI
Overlay
Debug
VirtualControls
P1_UI
P2_UI
```

Do not create a UI Layer for every screen; Inventory / Shop / Settings may share `Menu`.

### UI Input Interaction Matrix

A Project-level Editor setting similar to the Unity Physics Layer Collision Matrix is formally added. However, Input → UI is directional, so the data is not a symmetric matrix.

The Editor displays the complete matrix:

```text
UI Input Interaction Matrix

                         UI Layer
                 HUD   Menu   Modal   WorldUI   Debug   WebView
Input Layer
────────────────────────────────────────────────────────────────
PlayerInput        ✓      ✕       ✕        ✓        ✕       ✕
MenuInput          ✕      ✓       ✓        ✕        ✕       ✓
TouchGameplay      ✓      ✕       ✕        ✕        ✕       ✕
DebugInput         ✓      ✓       ✓        ✓        ✓       ✕
WorldInteract      ✕      ✕       ✕        ✓        ✕       ✕
```

Semantics:

```text
Input Layer → UI Layer
```

Not:

```text
Layer A ↔ Layer B
```

Therefore, although the Editor appearance may resemble the Unity Physics Matrix, it must not store only a symmetric half-matrix.

### UI Input Matrix Runtime Representation

Authoring:

```text
InputLayer × UILayer Matrix
```

After Cook:

```text
InputLayerID
→ Allowed UILayerMask
```

Conceptual representation:

```cpp
struct UIInputLayerRule
{
    UILayerMask allowedLayers;
};
```

If the number of UI Layers is within 64, use directly:

```cpp
using UILayerMask = uint64_t;
```

Runtime determination is a bit test and must not use string / List search.

### UI Input Routing Pipeline

Formal flow:

```text
Platform Pointer / Navigation
↓
engine::input
↓
Input Context Stack
↓
Active InputLayer
↓
UI Input Interaction Matrix
↓
Allowed UILayerMask
↓
UIDocument Input Priority
↓
PassThrough / ConsumeOnHit / BlockBelow
↓
Hit Test
↓
Pointer Capture
↓
Capture / Target / Bubble
↓
UIElement
```

The Matrix resolves:

```text
Can this Input Layer affect this UI Layer?
```

Priority / Policy resolves:

```text
If multiple UI documents are eligible, who receives first?
```

### UIDocument Input Policy

Each UIDocument must support configuring at least:

```text
UILayer
InputPriority
InputPolicy
ReceivePointer
ReceiveNavigation
```

Policy:

```text
UIInputLayerPolicy
├─ PassThrough
├─ ConsumeOnHit
└─ BlockBelow
```

For example, Modal:

```text
UILayer = Modal
InputPriority = 1000
InputPolicy = BlockBelow
```

This allows lower-level HUD / Menu layers to continue rendering without receiving Input.

```text
Visible
≠
Interactive
```

### Project Matrix + Local Override

The UI Input Matrix is the Project Default.

Special UIDocuments may select:

```text
Input Routing Override
├─ UseProjectMatrix
├─ OverrideAllow
└─ OverrideDeny
```

Overrides should be treated as exception tools and their extensive use is discouraged, to prevent routing rules from being scattered across UI Assets again.

The Editor Inspector must display the source of the effective rule.

### Local Multiplayer + UI Matrix

The Matrix can handle multiplayer UI isolation:

```text
                  P1_UI   P2_UI   SharedUI
P1Input              ✓       ✕        ✓
P2Input              ✕       ✓        ✓
KeyboardDebug        ✓       ✓        ✓
```

Therefore, Gamepad / Keyboard routes for different players can be directly restricted to designated UI domains.

### WorldSpace UI Routing

WorldSpace UI follows the Matrix as well:

```text
Camera Ray
↓
InputLayer
↓
UI Input Matrix
↓
Allowed WorldUI Documents
↓
Nearest Valid Surface
↓
Local UI Hit Test
```

For example:

```text
WorldInteract → WorldUI
MenuInput     → Menu
```

### WebView Routing

`WebViewElement` must also map to a UI Input Layer / routing policy.

If the Matrix / Context makes the WebView non-interactive, the Native backend must synchronize this by:

```text
Disable Hit Test
or
Hide / Suspend Interaction
```

It is not sufficient for the Engine Hit Test to ignore it while the OS Native View continues receiving Pointer input.

The platform limitations of Native WebView remain subject to the capabilities of its backend.

### UI / Gameplay Focus

The following must be formally distinguished:

```text
Window Focus
Keyboard Focus
UI Focus
Pointer Capture
Gameplay Input Focus
```

For example, when a Text InputField obtains Keyboard Focus, `W` should enter text input and should not simultaneously drive a Gameplay Action unless explicitly permitted by the Context / Routing Policy.

### Frame Snapshot / Fixed Tick

Render Frames and Simulation Ticks are separated:

```text
Timestamped Raw Events
↓
Frame Input Snapshot
↓
Simulation Tick Sampling
↓
TickInput / Game-defined InputCommand
```

A brief Press / Release must not be lost because it falls between two Fixed Ticks.

`InputSnapshot` is Immutable within the current frame / tick.

### Game-defined Input Command

The Engine does not fix a `PlayerInputCommand` schema.

For example, RPG:

```text
RPGPlayerInput
├─ movement
├─ aim
├─ dodge
├─ normalAttack
└─ requestedSkill
```

Vehicle:

```text
VehicleInputCommand
├─ steering
├─ throttle
├─ brake
└─ handBrake
```

RTS may completely bypass the Character Framework.

Formal definition:

```text
Input Framework
≠ Character Framework
```

Character games may connect their own systems:

```text
Game-defined Input
↓
CharacterIntent
↓
CharacterMotor
↓
CharacterController
```

AI / Replay / Network systems may also generate the same game-defined command / CharacterIntent.

### Zig / Stable C ABI

Zig may use either of two data layers:

```text
Resolved Action Snapshot
or
Device Snapshot / Raw Event Batch
```However, Batch-first must be maintained:

```text
Native Input
↓
POD Snapshot / Span
↓
Stable C ABI
↓
Zig
```

Avoid large numbers of per-frame:

```text
Input_GetAction("...")
Input_IsKeyDown(...)
```

small calls across the ABI.

### Rebinding / Input Asset

Action Mapping can use the Asset System, for example:

```text
Default.input.json
UI.input.json
Vehicle.input.json
```

Process:

```text
JSON
↓
Engine JSON Framework
↓
Schema Validation
↓
Typed Input Mapping
↓
Runtime
```

User Rebind uses:

```text
Default Binding
+
User Override
```

The original Asset is not modified directly.

### Device Hot Plug / Suspend / Resume

Formal events:

```text
DeviceConnected
DeviceDisconnected
```

Disconnect must clear the device's Held / Pointer / Capture state and must not cause stuck input.

Mobile background:

```text
Background
↓
Cancel Active Pointer / Gesture
↓
Clear unsafe Held State
↓
Suspend
```

Resume:

```text
Re-enumerate Devices
↓
Rebuild Device State
```

### Haptics

The Device subsystem provides:

```text
Gamepad Rumble
Mobile Vibration
```

The API can be abstracted as:

```text
PlayHaptic(device, pattern)
StopHaptic(device)
```

Virtual Control may request haptics, but actual output still goes through the Device / Haptics backend.

### Input Recording / Replay Foundation

It is recommended that V1 establish the basic capability:

```text
Resolved Game-defined InputCommand / TickInput
↓
Recorder
↓
Replay
```

Prioritize recording resolved input commands rather than platform-specific raw keyboard messages.

Uses:

- Bug reproduction.
- Character Controller testing.
- Automated gameplay testing.
- Network prediction / rollback foundation.
- AI / human control comparison.

### Input Debugger / Profiler

The Editor must at least be able to inspect:

```text
Connected Devices
Device Controls
Raw Events
Action Values
Active Contexts
Active Input Layers
UI Input Matrix Result
Consumed / Blocked Route
Pointer Captures
Focus Owner
InputUser Assignments
Gamepad Dead Zones
Virtual Control Values
LastActiveDevice
```

Routing Debug must be able to answer:

```text
Why did this UI receive the input?
Why was this UI blocked?
Which Context / InputLayer / Matrix rule / Priority decided it?
```

### Input Namespace

Formal namespace:

```cpp
engine::input
```

When necessary:

```cpp
engine::input::device
engine::input::action
engine::input::gesture
engine::input::haptics
```

Avoid excessively deep namespaces.

Virtual UI Widgets are located at:

```cpp
engine::ui::VirtualJoystick
engine::ui::VirtualButton
engine::ui::VirtualDPad
engine::ui::VirtualTouchRegion
```

Their output enters `engine::input`; they do not call Gameplay directly.

### Input V1 Scope

```text
✓ Platform Input Backend abstraction
✓ RawInputEvent + Timestamp
✓ InputDevice / InputControl / DeviceSnapshot
✓ Keyboard / Mouse / Touchscreen / Gamepad
✓ Simultaneous Keyboard + Mouse + Gamepad + Touch
✓ Custom Device registration foundation

✓ Optional InputAction System
✓ User-defined InputAction / InputActionMap
✓ Button / Axis1D / Axis2D / Axis3D
✓ Binding / Composite / Modifier
✓ Processor / Custom Processor
✓ Context Stack
✓ Rebinding / User Override

✓ Multi-touch
✓ Stable PointerID lifetime
✓ Per-pointer Capture / Ownership
✓ PointerAcceptance policy
✓ Text Input / IME
✓ Cursor Mode / Relative Mouse

✓ VirtualJoystick
✓ Fixed / Floating / Dynamic
✓ VirtualButton
✓ VirtualDPad
✓ VirtualTouchRegion
✓ Virtual Controls + Multi-touch
✓ Safe Area / UIScale integration
✓ User Virtual Control layout override

✓ InputLayer / Routing Layer
✓ Project UI Input Interaction Matrix
✓ Matrix Cook → UILayerMask bitset
✓ UIDocument InputPriority / Policy
✓ Project Matrix + exceptional local override
✓ Local Multiplayer UI routing
✓ WorldSpace UI routing
✓ WebView interaction routing

✓ InputUser / Multi-device assignment
✓ Device Hot Plug
✓ Haptics foundation
✓ Frame / Fixed Tick Snapshot
✓ Zig Batch Snapshot / Stable C ABI
✓ Input Debugger / Routing Debugger
✓ InputCommand recording / replay foundation
```

Advanced Gesture Recognizer:

```text
△ Full advanced gesture library depending on V1 schedule
```

However, the Multi-touch / PointerID / Capture foundation is mandatory for V1.

### Input Definition of Done

V1 must at least verify:

- The Engine has no Reserved `Move / Jump / Attack` Actions.
- Developers can completely avoid the Action System and directly read Raw / Device Snapshots.
- Keyboard + Mouse + Gamepad can be simultaneously active in the same InputUser and the same frame.
- Touch supports simultaneous, independent operation of multiple Pointers.
- VirtualJoystick + CameraLookRegion + multiple VirtualButtons can operate simultaneously.
- Pointer Capture is per PointerID, with no duplicate delivery.
- UI / Gameplay / WebView ownership rules are observable and have no double consumption.
- Input Context and InputLayer / UILayer routing have separate responsibilities.
- The UI Input Interaction Matrix can be configured directly in the Editor.
- The Matrix is Runtime Cooked into a compact mask; string lookups are not performed on the hot path.
- Modal UI can BlockBelow without affecting lower-layer Render.
- Local Multiplayer can restrict P1/P2 Input to operate only on designated UI Layers.
- WorldSpace UI can be filtered according to Input Layer / UI Matrix.
- When WebView route access is denied, Native hit-testing is synchronously disabled.
- Text Input / IME does not accidentally trigger Gameplay physical key actions.
- Device disconnect / app suspend does not leave a stuck Held state.
- Fixed Tick does not lose transient Press / Release events.
- Zig uses batch snapshots and does not depend on large numbers of per-action ABI calls.
- Input Debugger can display the complete routing rationale from Context → InputLayer → Matrix → UIDocument → Element.

Core Contract:

```text
Physical Input
≠ Gameplay Action

InputAction System
→ Optional Convenience Layer

Developer
→ May consume Raw / Device State directly

Multiple Devices
→ Simultaneously Active

Touchscreen
→ Multi-touch by Default

Each Pointer
→ Independent Owner / Capture

Virtual Controls
→ Generic Input Sources, not Gameplay-specific controls

InputAction
→ Fine-grained meaning

InputLayer
→ Coarse-grained routing category

UI Render Layer
≠ UI Input Layer

Input Layer → UI Layer
→ Project UI Input Interaction Matrix

Editor Matrix
→ Authoring friendly

Runtime Matrix
→ Precompiled UILayerMask / Bit Test

Game-defined InputCommand
→ Gameplay semantic boundary
```

## Forty-Two, Platform Layer

```text
Platform/
├─ Windows
├─ macOS
├─ Android
└─ iOS
```

Responsibilities:
- Window
- App Lifecycle
- Native Handle
- File System
- Clipboard
- Dialog
- Thread
- Input
- Touch
- Device Info
- Save Path
- Crash Path


## Forty-Three, Memory Budget

Included beginning with V0.x; not deferred until before launch.

Tracked categories:
- Texture Memory
- Texture Streaming Resident Mips
- Terrain Memory
- Vegetation Instance / Cluster Memory
- Mesh Memory
- Animation Memory
- GPU Animation Pose Storage（Bone Animation Texture / Structured Buffer）
- VFX / Particle Memory（CPU Particle SoA、GPU Particle Pool、Spawn / Event / Indirect Args Buffer）
- Audio Memory
- UI Atlas
- GPU Buffer
- Render Target
- Transient Render Graph Resources
- Physics
- Script / Gameplay
- Asset Cache

The Memory Budget of `VFXBudgetManager` is subsystem-level detailed tracking under this classification. It remains subject to the global High / Emergency Watermark strategy in this section. Its triggering timing and intensity must be consistent with those of other categories and must not establish an independent decision-making logic separate from the global Watermark.

However, the “degradation mechanisms” of the two must not share the same semantics and must be executed separately:

```text
Discrete Resident Asset
（Texture / Terrain Chunk / Vegetation Cluster / GPU Animation Pose Storage）
→ According to Evictable / Pinned / Priority / Last Used
→ Individual resource eviction（LRU-style unload）

VFX Particle Pool / Live Simulation
（CPU Particle SoA、GPU Particle Pool）
→ No individually Evictable / Pinned resources can be evicted
→ Through the existing Priority Tier of VFXBudgetManager
   （Critical / Gameplay / Character / Environment / Cosmetic / Background）
→ Execute VFX's own degradation action list
   （Lower Spawn Rate / Lower Simulation Rate / Disable Collision-Distortion）
```

The Global Watermark only determines “when and how severe.” The actual degradation mechanisms are handled by each category’s existing mechanisms. Semantics such as Evictable / Pinned for individual resource eviction must not be applied to the VFX Particle Pool, which has no discrete evictable items.

The Profiler must display:
- Current
- Peak
- Budget
- Over Budget Warning

Mobile must have Quality Tier / Memory Tier.

For example:
Low Memory Device
Medium Memory Device
High Memory Device

Asset Streaming and Texture Max Size must be adjustable according to the Tier.


## Forty-Four, Crash Reporting

V1 first implements Crash Infrastructure.

Required:
- Crash Log
- Stack Trace
- Build Version
- Platform
- GPU
- Renderer Backend
- Windows Graphics API（DX12 / Vulkan）
- Last Loaded Scene
- Last Render Pass
- Memory Snapshot summary

Windows:
- MiniDump

Apple:
- Crash Log / Symbolication support

Android:
- Native Crash Log

Third-party Crash Backend:
- V1 need not be bound to one
- Reserve adapters for Sentry / Backtrace / self-hosted services


## Forty-Five, Automated Testing / CI

Begins with V0.x.

Unit Test:
- Math
- UUID
- Serialization
- Reflection
- Asset Import
- Resource Handle
- Container / Utility

Engine Test:
- Scene Load
- Prefab
- Animation
- Physics
- Save Migration

Renderer Test:
- Smoke Test
- Shader Compile Test（Only compile affected Variants according to the Dependency Graph）
- Golden Image / Image Regression（DX12 / Vulkan / Metal）
- Cross-Backend Golden Image Comparison
- Render Graph Validation
- GPU Resource Lifetime Test

CI:
- Windows DX12 Build
- Windows Vulkan Build
- Windows DX12 Smoke Test
- Windows Vulkan Smoke Test
- macOS Build
- Android Build
- Shader Compile
- Unit Test
- Asset Import Test

Feature / Module Stripping Build Matrix:

```text
PR CI
→ Representative Default Profile
→ Stripping dependency validation
→ Disabled + detected dependency must fail

Nightly / Scheduled CI
→ Minimal Profile Build
→ Max / Full Feature Profile Build
→ Representative Mobile Minimal Profile
→ Representative Desktop Full Profile

Release Branch
→ Shipping Profile
→ Required platform-specific Feature Matrix
→ Modular / Monolithic packaging validation
```

The Minimal Profile is used to verify:

- Optional Subsystems have no hidden hard dependencies.
- Disabled Plugins / Subsystems are not Compile / Link / Packaged.
- Assets / Shader Variants for stripped Features are not generated.
- The minimum Runtime can start successfully.

The Max / Full Feature Profile is used to verify:

- The Dependency Graph is correct when Optional Subsystems are enabled simultaneously.
- Module Registration / ABI / Build Order are correct.
- Feature combinations do not produce compilation gaps due to Stripping Defines.

It is not required to run the complete Feature combination matrix for every PR; PRs use representative Profiles, while Minimal / Max Profiles must be run regularly in Nightly / Scheduled CI at minimum. The actual frequency is adjusted according to CI cost and historical failure rate.

iOS:
- At minimum, perform compilation verification.
- Device testing may be conducted independently.


## Forty-Six, Device Compatibility Test Matrix

The correctness of Android Vulkan cannot rely solely on emulators or AI inference.
The V0.6 Mobile Milestone must establish a physical-device matrix.

Minimum GPU Vendor coverage:
- Qualcomm Adreno
- ARM Mali
- Imagination PowerVR（if representative devices remain in the minimum supported market）

At least one representative physical device per category.
If the project enters the commercial release phase, expand to:
- Low Tier
- Mid Tier
- High Tier
- Recent Flagship
- Devices at the minimum supported OS / Driver boundary

Required test items:
- Vulkan Instance / Device creation
- Swapchain
- Descriptor Indexing
- Binding Tier Fallback
- Compute Shader
- Forward+ Light Culling
- ASTC
- MSAA
- Depth / Stencil
- Render Pass / Dynamic Rendering paths
- Synchronization
- Suspend / Resume
- Background / Foreground
- Orientation Change
- Thermal Throttling
- Frame Pacing
- Memory Pressure
- GPU Crash / Device Lost
- Long-duration Stability

Special attention:
- Adreno Driver workarounds
- Mali Tile-Based Rendering performance
- PowerVR Tile-Based Rendering behavior
- Descriptor / Bindless boundaries
- Compute Workgroup limitations
- FP16 behavior
- Texture Format support

All vendor-specific workarounds:
- Must be placed in the Capability / Backend layer.
- Must include Device / Driver conditions.
- Must not be scattered across Gameplay / Material / Scene.



## Terrain System

Terrain is an officially built-in Engine module and is not implemented as a single large Mesh.

Core architecture:

```text
Terrain
├─ Heightmap
├─ Chunk / Tile
├─ Quadtree
├─ Terrain LOD
├─ Layer / Splat Map
├─ Normal / Macro Texture
├─ Collision
├─ Streaming
└─ Vegetation Placement / Mask
```

Features:
- Heightmap Terrain
- Chunk-based Streaming
- Quadtree Spatial Partition
- Screen-space Error LOD
- Terrain Layer Blend
- Splat / Weight Map
- Height Blend
- Detail Normal
- Macro Variation
- Triplanar（Optional）
- Terrain Collision
- Terrain Hole（Future）
- Terrain Decal（Future）
- Virtual Texturing（Future）

LOD principles:
- Not based only on Distance.
- Primarily based on Screen-space Error.
- Supports Geomorphing / Stitching to avoid Terrain Chunk LOD cracks and popping.

Terrain Render Path:
```text
Terrain Chunks
↓
Quadtree Query
↓
Frustum / Distance Culling
↓
LOD Selection
↓
Material / Layer Resolve
↓
GPU Draw
```

Future GPU-driven:
```text
Terrain Candidate Chunks
↓
GPU Culling
↓
GPU LOD Selection
↓
Indirect Draw
```



### Terrain Streaming Asset Layout

Terrain Chunks still use the unified UUID + Asset Database and do not establish a second Asset Identity system; however, Build Package uses a Streaming-friendly Bundle Layout.

```text
map01/
├─ map01_core.bundle
├─ terrain/
│  ├─ terrain_00_00.bundle
│  ├─ terrain_00_01.bundle
│  └─ ...
└─ vegetation/
   ├─ vegetation_00_00.bundle
   ├─ vegetation_00_01.bundle
   └─ ...
```

Each Terrain Chunk / Vegetation Cluster:
- Has its own UUID.
- The Asset Database records spatial coordinate / dependency.
- A Bundle is only physical packaging and streaming granularity; it does not change the Asset reference model.

Runtime:

```text
Player / Camera Position
↓
Streaming Grid / Radius
↓
Required Chunk UUID Set
↓
Bundle Resolver
↓
Async Load / Unload
↓
Terrain Chunk + Collision + Vegetation Residency
```

A Terrain Chunk can be split into Height Data, Render Patch, Material/Layer Data, Collision Heightfield Derived Data, and Vegetation Placement/Cluster Data. Core Map Data remains resident; Terrain Chunks have residency according to position; Vegetation can be unloaded more aggressively; Texture Mip residency is managed independently by the Texture Streaming System.

## Vegetation / SpeedTree-like System

The Engine provides a SpeedTree-like Runtime Vegetation System,
but V1 does not create a complete SpeedTree modeling tool.

Tree Asset:

```text
Tree Asset
├─ Trunk Mesh
├─ Branch Mesh
├─ Leaf Mesh / Cards
├─ Materials
├─ Wind Parameters
├─ LOD0
├─ LOD1
├─ LOD2
└─ Billboard / Impostor
```

Supports:
- Tree LOD
- Billboard / Impostor
- GPU Instancing
- Vegetation Cluster
- GPU Culling
- Indirect Draw（later）
- Global Wind
- Branch Bend
- Leaf Flutter
- Instance Wind Phase
- Terrain Vegetation Mask
- Density Map
- Random Scale / Rotation
- Species Variation

Vegetation categories:

```text
Large Tree
→ Mesh + LOD + Billboard

Bush
→ Mesh / Card + Instancing

Grass
→ Cluster / GPU-generated Instances
```

Large quantities of grass and vegetation must not be created as hundreds of thousands of independent Nodes in the Scene.

Forest Culling:

```text
Forest
├─ Cluster A
├─ Cluster B
└─ Cluster C

Cluster Frustum Culling
↓
Hi-Z Occlusion
↓
Instance Culling
↓
LOD Selection
↓
Indirect Draw
```

If third-party SpeedTree Asset / Runtime support is added in the future:
- A separate format / SDK / License Review must be performed.
- The Engine Runtime core must not depend on a proprietary format.


## Spatial Culling / Spatial World

The Scene Graph is not equivalent to the Spatial Structure.

Formally separated into:

```text
Scene Graph
= Logic / Transform / Prefab

Spatial World
= Spatial Queries / Culling

Render World
= Renderable derived data for the current Frame
```

Recommended structure:

```text
Static Environment
→ BVH

Dynamic Entities
→ Spatial Hash / Uniform Grid

Terrain
→ Quadtree

Vegetation
→ Cluster
```

Culling Pipeline:

```text
All Renderables
↓
Layer / Visibility Mask
↓
Distance Culling
↓
Spatial Query
↓
Frustum Culling
↓
Occlusion Culling
↓
LOD Selection
↓
Visible Render List
```

CPU Stage:

```text
Visibility Mask
↓
Distance Culling
↓
Spatial Query
   ├─ Static BVH
   ├─ Dynamic Grid
   ├─ Terrain Quadtree
   └─ Vegetation Cluster
↓
Frustum Culling
↓
Coarse LOD
↓
Upload Candidate List
```

GPU Stage（later）:

```text
Candidate Objects
↓
Hi-Z Occlusion
↓
Fine LOD Selection
↓
Instance Compaction
↓
Indirect Command Generation
↓
DrawIndirect
```

Occlusion:
- Hi-Z / Hierarchical Z
- Conservative Test
- Bounding Expansion
- Temporal Hysteresis
- Newly-visible object safeguard

Indoor scenes may optionally use:
- Room / Portal Culling

Bounding Volume:
- Bounding Sphere: fast Reject
- AABB: primary precise test
- OBB: used only for necessary objects


## Mesh LOD System

Mesh LOD is an official built-in Engine capability.

Supports:

```text
LOD0 → Highest Detail
LOD1 → Medium
LOD2 → Low
LOD3 → Very Low / Billboard
```

LOD selection:
- Primarily uses Projected Screen Size / Screen Percentage.
- Does not depend only on Camera Distance.
- Considers object size, FOV, and resolution.

Asset Import:
- Manual LOD
- Auto LOD Generation
- LOD Validation

Importer：

```text
Mesh LOD

Generate LOD      [✓]

LOD0  100%
LOD1   50%
LOD2   20%
LOD3    5%

Screen Threshold
LOD0  0.20
LOD1  0.08
LOD2  0.02
```

Runtime:

```text
RenderObject
↓
LOD Selection
↓
MeshHandle
```

LOD stability mechanisms:
- Hysteresis
- Dither Crossfade
- Avoid LOD oscillation

Advanced LOD:

```text
Geometry LOD
+
Material LOD
+
Shadow LOD
+
Animation LOD
```

Skinned Mesh:
- Mesh Triangle LOD
- Skeleton / Bone LOD
- Animation Update Rate LOD
- Can be reduced to 30Hz / 15Hz at long distances.
- Very Far can use an Impostor.

Shadow:
- The Shadow Pass can use a lower Mesh LOD than the Main Camera.
- Distant objects can stop casting shadows.

V1:
- CPU Screen-space LOD Selection

Later:
- GPU LOD Selection
- GPU Culling + LOD + Indirect Draw


## Texture LOD / Mipmap / Streaming

Texture LOD is a core built-in Engine capability.

Required for V1:
- Mipmap Generation
- GPU Automatic Mip Selection
- Mip Bias
- Platform Max Texture Size
- Quality Tier
- sRGB / Linear
- Normal Map Mip Handling

Importer:

```text
Texture Settings

Generate MipMaps   [✓]
Streaming          [✓]

Max Size           4096
Min Resident Mip   256
Mip Bias           0
Priority           Normal
```

Mipmap:

```text
4096
↓
2048
↓
1024
↓
512
↓
256
↓
...
```

Texture Streaming:

```text
Far
→ 256 / 512 mip resident

Nearer
→ load 1024

Near
→ load 2048 / 4096
```

Streaming decision criteria:
- Camera Distance
- Projected Screen Size
- Mesh LOD
- Texture Priority
- Current Quality Tier
- Memory Tier
- Current GPU Memory Budget

Runtime:

```text
Asset Bundle
↓
Async IO
↓
Decompress
↓
Staging / Upload Buffer
↓
GPU Texture
↓
Resource Registry
↓
TextureHandle
```

Streaming System:
- Async
- Budget-aware
- Eviction
- Residency Tracking
- Priority
- Preload
- Placeholder Texture
- Graceful DegradationTexture LOD and Mesh LOD can cooperate:

```text
Mesh LOD0 → 2K / 4K mip
Mesh LOD1 → 1K mip
Mesh LOD2 → 512 mip
Mesh LOD3 → 256 / Billboard
```

However, one-to-one binding is not mandatory; Runtime projected size and memory budget are the final criteria.

Mobile:
- ASTC
- Strict Memory Budget
- Avoid loading the highest mip that is not needed at once
- Re-evaluate residency after Background / Foreground



## Forty-Seven, Profiler

V1:
- FPS
- CPU Frame
- GPU Frame
- Draw Calls
- Triangle Count
- Texture Memory
- Texture Streaming Resident / Requested Mip
- Terrain Visible Chunks
- Terrain LOD Distribution
- Vegetation Visible Clusters / Instances
- Mesh LOD Distribution
- Texture Streaming Resident Mips
- Terrain Memory
- Vegetation Instance / Cluster Memory
- Buffer Memory
- Asset Memory
- Node Count
- UI Draw Calls
- Binding Tier
- Active Graphics API
- Render Pass Cost

Later:
- CPU Timeline
- GPU Pass Timeline
- Job System View
- Render Graph Inspector
- Asset Streaming Inspector
- Memory Budget Graph


## Forty-Eight, Build Pipeline

Windows:
- x64
- DX12 (Default)
- Vulkan (Selectable)
- BC Texture
- EXE
- Editor / Build Settings can switch Backend
- Command line can override Backend

Android:
- ARM64
- Vulkan
- ASTC
- APK / AAB

macOS:
- Apple Silicon
- Universal (as required)
- Metal
- .app

iOS:
- ARM64
- Metal
- ASTC
- Xcode Project



### Windows Graphics API Selection Strategy

Editor:
```text
Project Settings
└─ Graphics
   ├─ Default API: Direct3D 12
   └─ Available APIs:
      ├─ Direct3D 12
      └─ Vulkan
```

Build Profile can override:
```text
Windows-DX12
Windows-Vulkan
```

Runtime Debug can override:
```text
-gameapi=dx12
-gameapi=vulkan
```

Default principles:
- Windows Release / Shipping: DX12 preferred
- Android: Vulkan
- macOS / iOS: Metal
- Do not use OpenGL fallback


## DX12 / Vulkan Windows Dual-Backend Strategy

Officially supported on Windows:
- Direct3D 12
- Vulkan

Default:
```text
Windows Default Graphics API
→ DX12
```

Switching method:
```text
Editor Build Settings
[Graphics API]
- Direct3D 12   ← Default
- Vulkan
```

Runtime / Debug command line:
```text
-gameapi=dx12
-gameapi=vulkan
```

Startup strategy:
1. When no API is specified, initialize DX12 first
2. If the user / Build settings specify Vulkan, initialize Vulkan directly
3. Debug Build may allow automatically attempting Vulkan after DX12 initialization fails
4. Whether Shipping Build allows automatic fallback is determined by project settings
5. The Backend selection result must be written to the Log / Crash Report

Official RHI Backends:
```text
RHI/
├─ D3D12/
│  ├─ D3D12Device
│  ├─ D3D12Buffer
│  ├─ D3D12Texture
│  ├─ D3D12Pipeline
│  ├─ D3D12CommandList
│  ├─ D3D12DescriptorManager
│  ├─ D3D12SwapChain
│  └─ D3D12Fence
├─ Vulkan/
└─ Metal/
```

DX12 must fully support:
- Device / Adapter Selection
- Swap Chain
- Graphics / Compute / Copy Queue
- Command Allocator / Command List
- Fence / Synchronization
- Resource Barrier
- Descriptor Heap
- Root Signature
- Pipeline State Object
- Upload / Readback Heap
- Texture / Buffer
- Render Target / Depth Stencil
- Indirect Draw
- Timestamp Query
- Debug Layer
- DRED / Device Removed Diagnostics
- Pipeline Cache / PSO Cache
- Slang -> DXIL
- Golden Image / GPU Capture Test

Windows DX12 and Vulkan must share:
- Scene
- Render World
- Render Graph
- Material
- Shader Feature System
- Asset
- UI
- Culling
- Lighting
- Post Processing

The following must not occur:
- DX12-specific logic penetrating Scene / Material
- Vulkan-specific layout penetrating the higher-level Renderer
- Maintaining two separate sets of Gameplay / Material data for the two Backends


## Fifty, Performance Strategy

CPU:
- Job System
- Data-Oriented
- SoA
- Dirty Update
- Batch
- Async Loading
- Minimize Allocations

Spatial / Visibility:
- Static BVH
- Dynamic Grid / Spatial Hash
- Terrain Quadtree
- Vegetation Cluster
- Frustum Culling
- Hi-Z Occlusion (later)
- Screen-space LOD

GPU:
- Forward+
- Instancing
- Material Sorting
- Texture Compression
- Hybrid Bindless-first
- GPU Culling (later)
- Indirect Draw (later)
- Occlusion Culling (later)
- Mesh LOD
- Terrain LOD
- Vegetation LOD
- Texture Mip / Streaming

Mobile:
- ASTC
- FP16 preferred
- Bandwidth Optimization
- Tile-Based GPU Friendly
- Control Overdraw
- Reduce Render Targets
- Adjustable Graphics Quality Tier
- Adjustable Memory Tier


## Fifty-One, Graphics Quality Tier

Low:
- Reduced Shadow
- No SSAO
- Lower Texture
- Limited Lights
- FXAA

Medium:
- Cascaded Shadow
- SSAO
- Bloom
- Standard PBR

High:
- Better Shadow
- Higher Texture
- TAA
- Better Reflection
- More Lights
- Advanced Post


## Fifty-Two, V1 Scope Matrix

This table is the scope planning table for the Engine Planning Document and does not represent implementation progress.

Symbol definitions:

```text
✅
→ Included in the V1 planning scope (Committed Scope)
→ Does not mean implementation has been completed, testing has been passed, or executable code already exists
→ Actual completion status is determined by whether the Gate / DoD of the corresponding Roadmap Phase has been passed

❌
→ Explicitly excluded from the V1 scope
→ Not an omission, but an intentional decision (the reason should be explained in the corresponding section, such as Networking)

△
→ Partially included / conditionally included / deferred to a later version
→ The explanatory text specifies the concrete conditions (for example, “later GPU-driven phase” or “V2”)
```

The actual implementation progress of any item must be based on whether the Gate of the corresponding Phase in the Roadmap (the engine phased development roadmap) has been passed. This table only expresses the planning scope and must not be used as the basis for progress tracking.

Renderer DX12                    ✅ Windows Default
Renderer Vulkan                  ✅ Windows / Android
Renderer Metal                   ✅ macOS / iOS
Windows DX12/Vulkan Switch       ✅
OpenGL Backend                   ❌

Node / Component                 ✅
World / Scene Lifecycle            ✅
EditorWorld / PlayWorld Separation ✅
Multiple / Additive Scene          ✅
Persistent Scene                   ✅
Async Scene Load / Atomic Activate ✅
WorldCommandBuffer                 ✅
Cross-Scene Logical Reference      ✅
Scene Serialization / Migration    ✅
World Partition Foundation         ✅
Fixed Grid Streaming Cells         ✅
Loose Quadtree Spatial Index       ✅
Room / Portal Graph                ✅
Outdoor / Indoor Gateway           ✅
Streaming Source / Demand          ✅
Streaming Priority / Hysteresis    ✅
Offline HLOD Builder               ✅
HLOD Runtime Selection / Streaming ✅
Adaptive Quadtree Cell Generation  △ Future / V2
Octree / 3D Adaptive Partition     △ Future
SoA Component Pool               ✅
Render Graph                     ✅
Forward+                         ✅
Hybrid Bindless-first            ✅
Binding Fallback                 ✅

Editor                           ✅
Prefab                           ✅
Undo / Redo                      ✅
Runtime UI                       ✅
UIDocument / UIElement Tree       ✅
UIElement ≠ SceneNode             ✅
UI VirtualizedListView            ✅
UI JSON / Hot Reload              ✅
UI Data Binding / ViewModel       ✅
UI ScreenSpace                    ✅
UI WorldAnchored                  ✅
UI WorldSpace                     ✅
WebViewElement Native Overlay     ✅
Offscreen Texture WebView         △ V2 / Optional
Input Framework                   ✅
Raw / Device-level Input          ✅
Optional Action Mapping           ✅
Simultaneous Multi-device Input   ✅
Multi-touch / Per-pointer Capture ✅
Virtual Controls                  ✅
UI Input Interaction Matrix       ✅
InputLayer → UILayer Routing      ✅
Input Rebinding / User Override   ✅
Input Fixed-Tick Snapshot         ✅
Input Replay Foundation           ✅

Transform System                   ✅
Large-world Position Foundation    ✅
System Scheduler / Access DAG      ✅
Typed Engine Event Framework       ✅
Camera Framework                   ✅
Lighting / Shadow Framework        ✅
PostProcess Volume Framework       ✅
Navigation / Recast-Detour         ✅
Streaming NavMesh Tiles            ✅
AI Blackboard / Behavior Tree      ✅
AI Perception / Update LOD         ✅
GOAP                               △ Future / Plugin
Editor Document / Adapter Model    ✅
Nested Prefab / Variant            ✅
Prefab Structural Override         ✅
Transaction Undo / Redo            ✅
Stable Reflection Type/Property ID ✅
Generic Serialization Framework    ✅
Cooked Binary Runtime Blob         ✅
Asset Importer Registry / Local DDC ✅
Mesh Optimization / LOD Cook       ✅
VFS / Async IO                     ✅
Persistent World State / Save      ✅
ICU-backed Localization            ✅
Text Editing / IME Model           ✅
Platform Service Framework         ✅
Structured Async Logging           ✅
Persistent Allocator Framework     ✅
High-level TaskGraph                ✅
Timer Scheduler                     ✅
Audio Event Authoring               ✅
Animation Authoring / Graph Editor  ✅
Physics Authoring / Collision Matrix ✅
Project Manifest / Settings Layers  ✅
Runtime Developer Console           ✅

Terrain System                    ✅
Vegetation / SpeedTree-like       ✅ Runtime System
Spatial World / Culling           ✅
Mesh LOD                          ✅
Texture Mipmap LOD                ✅
Texture Streaming                 ✅
Hi-Z GPU Occlusion                △ Later GPU-driven phase
Localization                     ✅
Save System                      ✅
Engine JSON Parse / Generate      ✅ yyjson backend + Engine abstraction
Data Table JSON Runtime Asset     ✅
Data Table Schema / Validation    ✅
Data Table Key System             ✅ UInt32 / UInt64 / String
Data Table Preprocess Pipeline    ✅
Data Table Runtime Containers     ✅ Rows + Index / View / Pool
DataTable Schema Codegen          ✅ Narrow DataTable-only codegen
Audio Residency Scope            ✅
Audio Scope Fade/Release Policy  ✅

Networking Framework             ❌
Low-level Network Interface      △ Future boundary / transport plugin

Crash Infrastructure             ✅
Cloud Crash Backend              △ Optional
Memory Budget System              ✅
Automated Tests                  ✅
CI                               ✅

Shader Slang/Vulkan              ✅
Shader Slang/Metal               Confirm after PoC
Reflection Macro Metadata        ✅
Reflection Codegen               ❌ Future

FBX Runtime                      ❌
FBX Editor Import                ✅

VFX / Particle Framework          ✅
GPU Particle Simulation           ✅
Runtime Batch Fusion              ✅
VFX Async Compute                 △ Later GPU-driven phase
Animation Graph / State Machine   ✅
GPU Vertex Skinning               ✅
Skinned Mesh Instancing           ✅
GPU Crowd Animation (BAT)         ✅
Compute Skinning                  △ V2
Physics / Jolt CPU Core           ✅
Character Controller Framework    ✅
Character Motor / Intent           ✅
Moving Platform Support            ✅
Root Motion Character Resolve      ✅
CPU Physics Batch Query           ✅
GPU VFX Collision                 ✅
GPU Cloth Foundation              ✅
GPU Debris                        △ Optional
GPU Deferred Physics Query        ✅ Interface / Conditional Runtime
GPU Broadphase                    △ Future / Profile-driven
GPU RigidBody World               △ Future R&D
Shading Model Framework           ✅ PBR / StylizedPBR / Anime / Vegetation / Water / Unlit


## Fifty-Three, Version Planning

V0.1 - Engine Foundation
- CMake Build System
- EnginePCH / RendererPCH / EditorPCH
- PCH On / Off Build Validation
- Windows
- DX12 Backend Skeleton
- Vulkan Backend Skeleton
- DX12 Default Backend Selector
- Vulkan Switchable
- Window
- RHI
- Triangle
- Texture
- Mesh
- Camera
- Node
- EntityID
- Component Pool
- Transform
- Scene
- Unit Test Foundation
- Memory Tracking Foundation

V0.2 - RHI / Shader PoC
- Official activation of the first version of the AI Code Gate
- clang-format / clang-tidy CI
- DX12 Descriptor Heap / Root Signature
- Vulkan Resource Binding
- Windows DX12 / Vulkan Backend Parity Test
- Binding Tier
- Render Graph Skeleton
- Slang -> SPIR-V
- Slang -> Metal PoC
- Reflection Metadata
- Resource Registry
- GPU Resource Index

V0.3 - Editor
- Dear ImGui
- Hierarchy
- Inspector
- Scene View
- Asset Browser
- Transform Gizmo
- Scene Save / Load
- Undo / Redo

V0.4 - Actual 3D Renderer
- PBR
- Spatial World Foundation
- Static BVH / Dynamic Grid
- Frustum Culling
- Mesh LOD V1
- Texture Mipmap LOD
- Material
- Directional Light
- Shadow
- glTF / FBX Import
- Texture Import
- Render Graph
- Forward+

V0.5 - Capable of Making Games
- Prefab
- Animation
- Physics
- Audio
- Input
- Runtime UI
- Play Mode
- Localization
- Save System

V0.6 - Mobile / Apple
- Android Vulkan
- Terrain Chunk / Quadtree Foundation
- Texture Streaming Mobile Validation
- Android Physical GPU Vendor Compatibility Matrix
- Adreno / Mali / PowerVR (as required by the market) real-device validation
- macOS Metal
- iOS Metal
- Touch
- Lifecycle
- ASTC
- Mobile Memory Tier

V0.7 - Performance / Tools
- GPU Instancing
- Terrain System
- Vegetation / SpeedTree-like Runtime System
- Vegetation Cluster
- Texture Streaming
- Mesh / Material / Shadow / Animation LOD Integration
- LOD
- Async Asset Loading
- Asset Bundle
- Job System
- Profiler
- Crash Infrastructure
- CI Enhancement

V0.8+
- Compute Culling
- Hi-Z Occlusion
- GPU LOD Selection
- Vegetation GPU Culling
- Terrain GPU-driven Rendering
- Indirect Rendering
- GPU Driven
- TAA
- Terrain
- Vegetation
- Particle
- NavMesh
- Advanced Shadow

V1.0
- Windows / macOS / Android / iOS
- Capable of fully Building / Publishing a 3D game
- Basic completeness of Renderer / Editor / Asset / UI / Physics / Audio / Animation / Localization / Save
- Networking Framework explicitly not included in V1



## Thread-Safe Job System / Frame Synchronization Lifecycle

The Job System uses a Work-Stealing Scheduler.

Priority Class:

```text
High
→ Render Extraction / Frame-critical work

Normal
→ Animation / Physics / Gameplay jobs

Low
→ Background Asset IO / Import / Non-critical work
```

Priority does not mean starvation is permitted; the Scheduler must have a starvation prevention / aging policy.

### Task Dependency Model

A Job must be able to describe:

```text
Task Handle
Dependency List
Priority
Frame Lifetime
Scratch Arena
Completion Fence
```

Frame Execution Graph:

```text
Frame Begin
↓
Input / Gameplay
↓
Physics / Animation Parallel Jobs
↓
Frame Barrier
↓
Render Extraction Parallel Jobs
↓
Render Graph Compile
↓
Command Recording / Submit
↓
Frame End Barrier
↓
Arena Reset
```

Rules:

- A Job must not access scratch memory beyond its lifetime barrier
- Before the Frame Arena is reset, all jobs using that arena must be completed
- Before the render thread reuses a frame slot, it must confirm the GPU fence
- A Background IO Job must not hold a frame-local pointer
- A worker-local arena may only be used within the ownership scope of the corresponding Worker / Job

CI / Development Assertion:

```text
Frame-memory Escape
Thread Ownership Violation
Use-after-reset
Dependency Cycle
Job Lifetime Violation
```

Any one of these occurring is considered an Architecture Contract failure.

## Streaming Asset Residency State Machine / Memory Pressure

Streaming Assets must have explicit Residency States.

```text
Unloaded
↓
PendingDiskIO
↓
RAMResident
↓
PendingUpload
↓
VRAMResident
↓
Ready
```

Supplementary states may include:

```text
PendingUnload
Evicting
Failed
Cancelled
```

### Double Budget Control

Track independently:

```text
RAM Budget
VRAM Budget
```

At minimum, each asset category must track:

```text
Current
Peak
Budget
High Watermark
Emergency Watermark
Evictable
Pinned
Last Used
Priority
```

Watermarks must not be hard-coded as fixed cross-platform values; for example, 85% / 95% are only initial recommendations, and the actual values are determined by the Platform / Device Tier Profile.

High Watermark:

```text
Prefer:
LOD downgrade
Mip reduction
Streaming priority reduction
Evict cold optional resources
```

Emergency Watermark:

```text
Aggressive LRU eviction
Unload non-pinned resource
Reduce texture residency
Reduce animation / vegetation budget
```

The goal is to avoid:

- Mobile Low Memory Kill
- OS Memory Pressure termination
- Desktop swap / paging storm
- VRAM overcommit stutter

Residency State Transition must be asynchronous and cancellable. Gameplay is prohibited from directly assuming that an asset is immediately Ready after the `Load()` call completes.

## Unified Input System / Event Routing

The complete Input architecture uses “Forty-One, Input Framework” as the sole authoritative definition.

This section retains only the cross-system routing summary:

```text
Platform Raw Event
↓
engine::input
↓
Input Context Stack
↓
InputLayer / Routing Layer
↓
┌─────────────────────────────┬─────────────────────────────┐
│                             │                             │
UI Input Interaction Matrix   Gameplay / Tool Routing
│                             │
Allowed UILayerMask           Game-defined Input Logic
│
UIDocument Priority / Policy
↓
UI / WebView / WorldSpace UI Hit Test
↓
Per-Pointer Ownership / Capture
↓
Capture / Target / Bubble
```

Officially supported sources:

```text
Keyboard
Mouse
Touch / Multi-touch
Gamepad
Pen / Pointer
Virtual Control
Custom Device
```

Important Contract:

```text
Action Mapping is Optional
Multiple Devices may be active simultaneously
One Pointer / Gesture Sequence → One Owner
UI Input Matrix is directional: InputLayer → UILayer
Native Overlay / WebView must not receive duplicate delivery
```

## Time / Tick Model

Uses:

```text
Fixed Simulation Tick
+
Variable Render Frame
```

Physics / deterministic-like simulation uses Fixed Delta.

Concept:

```cpp
while (accumulator >= kFixedDeltaTime)
{
    PhysicsWorld::Step(kFixedDeltaTime);
    accumulator -= kFixedDeltaTime;
}

float alpha = accumulator / kFixedDeltaTime;
RenderWorld::ExtractTransforms(alpha);
```

RenderWorld uses:

```text
PreviousTransform
CurrentTransform
Interpolation Alpha
```

to obtain a smooth render transform.

Rules:

- Physics Tick and Display Refresh Rate are decoupled
- 120 / 144 Hz monitors do not change the physics step
- Prevent high-refresh physics jitter
- The maximum catch-up steps per frame must be limited to avoid the spiral of death
- Background / pause / suspend must define an accumulator reset / clamp policy
- TimeScale and FixedDeltaTime are managed separately
## Font Rendering Pipeline / Dynamic Glyph Atlas / MSDF

FreeType + HarfBuzz are responsible for:

```text
Font Rasterization
+
Text Shaping
```

### CJK

CJK uses the following by default:

```text
Dynamic Glyph Rasterization
↓
Dynamic Texture Atlas
↓
LRU Eviction
```

Reasons:

- The number of CJK glyphs is enormous
- Precomputing MSDF for the entire set would make the Bundle excessively large
- Multilingual fallback fonts require dynamic glyph residency

An atlas size such as 2048x2048 is only a profile default and must not be hardcoded as an architectural limitation.

The following must be supported:

```text
Multiple Atlas Pages
LRU
Glyph Pinning
Prewarm
Fallback Font
Atlas Rebuild / Eviction
```

### Latin / Icon Font

The following may be selected:

```text
Precomputed MSDF
```

Applicable to:

- Latin UI
- Icon Font
- Glyphs requiring large-scale zooming
- High-DPI UI

The Text System may select the following according to Font Asset / Locale:

```text
Dynamic Raster Atlas
MSDF
```

All languages are not required to use the same rendering mode.

## Crash Reporting / MiniDump / Symbolication

Crash Infrastructure uses a Backend abstraction and does not tightly bind Engine Core to a single third-party SDK.

```text
CrashService
↓
ICrashBackend
├─ Platform Native
├─ Crashpad-compatible Backend
└─ Future Telemetry Provider
```

Platform targets:

```text
Windows
→ Minidump / PDB Symbol

macOS / iOS
→ Crash Report / dSYM Symbolication

Android
→ Native Tombstone / Native Crash Capture / symbol files
```

The following must be handled:

```text
Fatal Signal
Unhandled C++ Exception
Engine Fatal
GPU Device Lost metadata
Last Log Ring Buffer
Build ID
App Version
Module List
```

Complex non-async-signal-safe operations are prohibited inside the crash handler.

Crash upload uses:

```text
Crash Capture
↓
Local Persist
↓
Next Launch
↓
When permitted by User / Product Policy
↓
Telemetry Upload
```

### Symbolication Pipeline

Shipping Build must preserve:

```text
PDB
dSYM
Unstripped ELF / symbol mapping
Build ID
Module UUID
```

CI / Release Artifact Server maps symbols using Build ID.

The Shipping package may strip symbols, but the symbol archive must not be lost.

Crashpad / Breakpad may be used as candidate implementations; the actual version and license must be confirmed during third-party integration.

## Terrain Virtual Texturing / Vegetation Wind Extension

### Terrain Virtual Texturing

Virtual Texturing is an advanced optional feature for large Terrain and must not block V1 Terrain.

Target architecture:

```text
Visible Terrain
↓
VT Feedback
↓
Feedback Resolve / Compute
↓
Requested Virtual Tiles
↓
Async Streaming
↓
Physical Texture Pool
↓
Page Table Update
```

RHI / Render Graph is responsible for:

- Feedback Buffer lifetime
- Compute Resolve Pass
- Physical Texture Pool resource state
- Async upload synchronization
- Page Table update scheduling

If the platform supports sparse resources, platform capabilities may be used; otherwise, a software-managed physical atlas fallback is permitted.

### Vegetation Wind

Vegetation wind should primarily be completed in the GPU vertex / compute path:

```text
Global Wind Field
+
Procedural Noise
+
Pivot / Hierarchical Branch Data
↓
Vertex Displacement
```

Avoid updating large numbers of vegetation transforms on the CPU every frame.

The Wind Feature must participate in Shader Variant / Feature Stripping, but variant growth should be controlled, with runtime parameters preferred over unnecessary static permutations.

## Systemic Risk Register

### Risk 1: Slang / Metal Compiler & Driver Divergence

Mitigation:

- Dual-path CI validation for Direct MSL and SPIRV-Cross MSL
- Canonical Reflection consistency
- Golden Image testing on physical macOS / iOS devices
- Build-time validated fallback policy
- Independent hand-written MSL fallbacks are not allowed

### Risk 2: AI-generated Lifetime / Memory Bugs

Mitigation:

- clang-tidy memory / lifetime rules
- ASan / UBSan regression build
- Handle generation validation
- Frame / Pool / Arena boundary guard
- Allocator leak reporting
- Ownership contract review

### Risk 3: Mobile GPU Synchronization / UMA Coherency

Mitigation:

- Render Graph automatically derives Resource State / Barrier
- RHI backend controls flush / invalidate
- Vulkan Validation Layers
- Metal API validation / GPU capture
- Android vendor device matrix
- RenderDoc / vendor profiler as development diagnostic tools

The degree of automation depends on tool and platform capabilities; it must not be assumed that all profiler captures can be fully unattended in CI.

### Risk 4: Remote Bundle File Lifetime / State Desync

Mitigation:

- Versioned cache directory
- No active bundle in-place replacement
- Active Manifest pointer switch
- SafeToDelete state
- mmap / FD / in-flight IO validation
- Pre-init deferred activation / cleanup

### Risk 5: Native WebView Z-order / Input Conflict

Mitigation:

- Explicit Native Overlay marking
- Input Hit-Test Mode
- Viewport Sync
- Canvas z-order limitation warning
- Fullscreen Modal automatically Suspend / Hide WebView
- Focus handoff

## PSO Cache / Pipeline Warmup Strategy

Under modern low-level APIs, Runtime PSO creation may cause significant frame jank. Therefore, PSO creation and caching must be treated as core Renderer capabilities.

### Core Rule

```text
Hot Render Loop
→ Implicit synchronous creation of expensive PSOs is not allowed
```

Recommended flow:

```text
Scene / Prefab / Material Feature Scan
↓
Required Shader Variant Set
↓
Required PSO Key Set
↓
Load Compatible Pipeline Cache
↓
Async PSO Pre-create / Warmup
↓
Enter Gameplay
```

### PSO Key

PSO Key must include at least:

```text
Shader Variant Key
Render Pass / Attachment Format
Depth / Stencil State
Raster State
Blend State
Vertex Layout
Topology
Sample Count
Binding Tier
Backend
Platform
Quality Tier
```

### Platform Cache Strategy

Pipeline cache is not considered a cross-device universal binary.

DX12:

```text
Cached PSO / Pipeline Library
→ Managed according to driver / adapter / build compatibility
```

Vulkan:

```text
VkPipelineCache
→ vendorID must be validated
→ deviceID
→ pipelineCacheUUID
→ driver-compatible identity
```

Metal:

```text
MTLBinaryArchive
→ Managed according to actual MTLDevice / OS / build profile compatibility
```

Therefore:

```text
GameData/
└─ PipelineCache/
   ├─ DX12/
   ├─ Vulkan/
   └─ Metal/
```

The actual cache key should include:

```text
Engine Build ID
Shader Compiler Version
Backend
GPU Vendor
GPU Device / Family
Driver / OS Version
Pipeline Cache UUID / equivalent compatibility token
Quality Tier
Binding Tier
```

If the cache is incompatible:

```text
Ignore Cache
↓
Rebuild / Warmup
↓
Write New Compatible Cache
```

### Warmup

Loading Screen / Scene Transition:

```text
Gather PSO Requests
↓
Async Create
↓
Track Progress
↓
Gameplay Start
```

The following is allowed:

```text
Background Lazy Warmup
```

However, unlimited synchronous compilation is not allowed in high-frequency draw paths.

### Runtime Fallback

If an unprewarmed PSO is still encountered during gameplay:

```text
Development
→ Log + Hitch Marker + Capture Key

Shipping
→ Async create when possible
→ Temporary fallback material / skip draw / controlled stall policy
```

The strategy depends on content type, but it must be recorded by Profiler / Telemetry.

### CI / Renderer Gate

Track:

```text
Total PSO Count
Warmup PSO Count
Runtime-created PSO Count
PSO Creation Time
Cache Hit Rate
Cache Miss Rate
Worst PSO Creation Time
```

The gate may be configured as:

```text
Runtime synchronous PSO creation count
→ Must be below the profile threshold
```

Thresholds are calibrated using measured baselines; fixed numbers must not be hardcoded in advance.

## Android GPU Workaround Database

The Vulkan Backend includes:

```text
GPUWorkaroundDatabase
```

At startup, read:

```text
GPU Vendor ID
Device ID
Driver Version
API Version
Feature / Extension Set
```

Then generate:

```text
Capability Profile
+
Workaround Flags
```

For example:

```text
Disable Dynamic Rendering
Force Conservative Barrier
Disable Specific Extension
Prefer Legacy Render Pass
Disable Descriptor Feature
Limit Async Compute
Adjust Present Mode
```

Workarounds must be isolated in:

```text
RHI/Vulkan/Capability
```

Gameplay / Renderer high-level code is prohibited from directly identifying GPU models.

### Fallback Tier

```text
Tier A
→ Full Feature

Tier B
→ Reduced Feature / Workaround

Tier C
→ Restricted Compatibility Mode

Blacklisted
→ Refuse Launch / Show Unsupported Device
```

The database must:

- Have a version
- Be updateable at build time
- Be generated from physical-device validation results
- Give every workaround an issue / reason / affected range
- Avoid crudely applying all devices under a single vendor name

## OS Memory Pressure / Device Loss Recovery

Core Platform / Memory establishes:

```cpp
enum class MemoryPressureLevel
{
    Normal,
    Warning,
    Critical
};
```

Event:

```text
OnMemoryPressure(Level)
```

### Warning

```text
Evict cold texture mips
Evict unused mesh residency
Trim streaming cache
Release optional decoded audio buffers
Compact subsystem caches
```

### Critical

```text
Aggressive LRU eviction
Release unused bundles
Release transient caches
Reduce texture / animation / vegetation budgets
Release optional render targets
Force lower quality residency profile
```

Note:

Frame Arena reset must still comply with frame/job lifetime and must not arbitrarily reset an arena that is still in use due to a memory pressure callback.

### Device Lost / GPU Recovery

Unified events are required:

```text
OnDeviceLost
OnDeviceRestored
```

DX12:

```text
Device Removed / Reset
→ Capture diagnostics
→ Tear down GPU resources
→ Recreate device where supported
→ Rebuild Resource Registry backing objects
```

Metal / Mobile:

```text
Background / interruption / command failure
→ Preserve CPU-side asset identity
→ Recreate transient GPU state as needed
```

Not all GPU failures are recoverable; when recovery is impossible, follow the crash / graceful restart policy.

All GPU Resources must distinguish between:

```text
Persistent Asset Identity
vs
Backend Native Object
```

to facilitate restoration.

## Cache Line / False Sharing Protection

Multithreaded control structures must avoid false sharing.

Applicable to:

```text
Worker Queue Head / Tail
Worker Allocator Cursor
Hot Atomic Counter
Per-thread Statistics
Job Completion Counter
```

The following may be used:

```cpp
alignas(std::hardware_destructive_interference_size)
```

However, this value must not be hardcoded as a cross-platform ABI constant.

Reason:

```text
std::hardware_destructive_interference_size
→ implementation-defined
```

If the compiler / standard library does not provide it, the Engine may use a platform profile fallback, such as 64 bytes; this fallback is only for performance layout and not for serialization / network / file ABI.

### Job Slice

SoA / Chunk partitioning should avoid multiple Workers writing to the same cache line.

Principle:

```text
Partition Boundary
→ align to element/cache-friendly granularity
```

However, Chunk Capacity is not required to simply equal a multiple of cache-line bytes; it should be jointly determined according to:

```text
Element Size
Write Pattern
SIMD Width
Cache Line
Batch Size
```

CI / Benchmark:

```text
False Sharing Microbenchmark
Worker contention counters
Cache miss / coherence profiling
```

Padding should be applied only to data that is genuinely hot and written across threads, avoiding working-set expansion caused by padding everything.

## Reflection Metadata Schema / AI Generation Boundary

The Reflection system must separate:

```text
C++ Declaration Syntax
```

from:

```text
Canonical Reflection Metadata
```

### V1

V1 may retain lightweight Macro / constexpr registration, but:

- Macros only perform the thinnest annotation / registration
- Large amounts of serialization code must not be expanded inside macros
- Prefer C++20 constexpr / type traits
- Error messages must remain readable

### Canonical Metadata Schema

Define a unified schema from the first version.

For example:

```json
{
  "type": "TransformComponent",
  "version": 1,
  "fields": [
    {
      "name": "position",
      "type": "Vector3",
      "flags": ["Serialize", "EditorVisible"]
    }
  ]
}
```

The formal schema must define at least:

```text
Type ID
Type Name
Version
Base Type
Field ID
Field Name
Field Type
Array / Container
Serialization Flags
Editor Flags
Default Value
Range / Attribute
Migration Metadata
```

### Generation Pipeline

```text
V1 Macro / constexpr
        │
        ▼
Canonical Metadata
        │
        ├─ Serialization
        ├─ Inspector
        ├─ Asset Dependency
        └─ AI Tooling

V2 Clang Header Tool
        │
        ▼
Same Canonical Metadata Schema
```

Therefore, when the AST generator is replaced in the future:

```text
Producer Changes
Consumer Contract Does Not
```

CI must:

- Validate metadata schema
- Detect duplicate type/field IDs
- Detect incompatible schema migration
- Compare generated metadata deterministically

## Architecture Risk / Gate Matrix

| Module System | Core Risk | Protection / Validation Gate | Supplementary Mechanism |
| --- | --- | --- | --- |
| Renderer / RHI | Runtime PSO creation causing frame jank | Renderer Gate + runtime PSO creation metrics | Compatible disk PSO cache + loading-screen warmup |
| Vulkan Backend | Android GPU / Driver differences | Physical Device Validation Gate | `GPUWorkaroundDatabase` + capability override |
| Memory / Core | OS memory pressure / GPU loss | Memory Pressure + Device Recovery smoke test | `OnMemoryPressure` + `OnDeviceLost/Restored` |
| Threading / SoA | False sharing / cache contention | Threading benchmark / profiler checklist | Cache-aware padding + partition strategy |
| Reflection / AI | Macro / generated metadata drift | Metadata schema validation + clang-tidy | Canonical JSON metadata contract |

## Shader CI / Fallback Final Policy

Shader compiler path comparison does not use binary-equality judgment.

Formal gate comparisons:

```text
Canonical Reflection
Resource Mapping
Constant / Parameter Layout
Stage Visibility
Specialization Metadata
Argument Buffer Mapping
Representative Golden Image
```

Metal:

```text
Primary
Slang → Direct MSL → Apple Metal Compiler

Fallback
Slang → SPIR-V → SPIRV-Cross → MSL → Apple Metal Compiler
```

Fallback selection:

```text
CI / Build / Cook
→ validate both paths
→ choose validated path
```

Prohibited:

```text
Shipping Runtime
→ dynamic compiler-path switching
```

If Direct MSL has:

- Compile Failure
- Canonical Reflection mismatch
- Binding Contract mismatch
- Golden Image regression exceeding profile tolerance

then Build/Cook selects the validated SPIRV-Cross fallback.

DXIL / SPIR-V / MSL binaries are not compared for equality.

## RHI Modern Dynamic Rendering Contract

Public RHI exposes only modern dynamic rendering semantics:

```cpp
BeginRendering(const RenderingInfo& info);
EndRendering();
```

High-level Renderer / Render Graph does not know about:

```text
VkRenderPass
VkFramebuffer
ID3D12GraphicsCommandList*
MTLRenderPassDescriptor native lifetime
```

Backend mapping:

```text
Vulkan
→ Primary: vkCmdBeginRendering / Dynamic Rendering
→ Fallback: legacy render pass path only inside Vulkan backend capability/workaround layer

DX12
→ BeginRenderPass where supported / appropriate
→ backend may fall back to explicit RTV/DSV command sequence

Metal
→ transient MTLRenderPassDescriptor builder
→ MTLRenderCommandEncoder
```

Formal rules:

- The Public RHI contract does not expose the legacy Vulkan RenderPass model
- The Backend capability layer may retain a compatibility fallback
- Android `GPUWorkaroundDatabase` may select the fallback path
- Render Graph always depends only on unified `RenderingInfo`
- Dynamic resolution / transient attachment reuse must not require the High-level layer to create permanent framebuffer objects

## Bundle Versioned Storage / Generation Pinning

Remote Bundle uses:

```text
Versioned Storage
+
Bundle Generation
+
Load Context Pinning
```

Directory:

```text
Cache/
├─ characters/
│  ├─ v1.0.1/
│  │  └─ characters.bundle
│  └─ v1.0.2/
│     └─ characters.bundle
```

Update flow:

```text
Download v1.0.2
↓
Verify
↓
Create New Bundle Generation
↓
Validate dependency set
↓
Atomic Active Generation Switch
```

Important:

```text
Existing Load Context
→ pin old generation

New Load Context
→ use new generation
```

Arbitrary mixed reading within the same Load Context is prohibited:

```text
Material → old bundle generation
Texture  → new bundle generation
```

This avoids a Mixed-Version Dependency Graph.

Load Context must include at least:

```text
Scene Load
Prefab Load Group
Streaming Cell Transaction
Bundle Dependency Resolution Transaction
```

Old generation:

```text
RefCount > 0
→ remain valid

RefCount = 0
+ no mmap
+ no open FD
+ no in-flight IO
→ SafeToDelete
```

Deletion may be deferred:

```text
Background Cleanup
or
Next App Pre-init
```

The Active Manifest only records which generation new Load Contexts use by default; it does not force existing Contexts to switch immediately.

## Frame Arena Reset Ownership

The core condition for Frame Arena Reset is lifetime safety; it is not tied to the Main Thread.

Allowed:

```text
Main Thread
or
FrameAllocatorManager
```

to perform a unified Reset after the Life Barrier.

Required conditions:

```text
All jobs using arena completed
No escaped pointers
Render extraction finished
Relevant CPU frame lifetime ended
```

GPU-backed frame slots additionally require:

```text
GPU fence completed before reuse
```

Prohibited:

- Resetting before a Job has completed
- Background jobs retaining frame-local pointers
- Persistent components / assets retaining arena memory
- Replacing actual lifetime validation with Thread Affinity

## Terrain / Vegetation Physics Integration

Terrain Heightmap:

```text
Source of Truth
```

Jolt:

```text
HeightFieldShape
→ Derived Physics Representation
```

Physics chunk size is not hardcoded to a fixed 64x64.

Instead:

```text
TerrainChunkProfile
├─ Render Chunk Size
├─ Physics Chunk Size
└─ Streaming Cell Size
```

The three may differ and are adjusted according to:

- CPU cost
- collision query locality
- streaming granularity
- memory
- platform tier

### Vegetation Physics Proxy

High-level contract:

```text
Vegetation Physics Proxy System
↓
Near-field collider activation
↓
Jolt backend representation
```

The Jolt backend may select according to the profile:

```text
Static Body
Compound Shape
Batched / grouped collider representation
```

High-level Terrain / Vegetation does not directly depend on Jolt-specific Shape organization.

Distant vegetation:

```text
Rendering only
```

Only the necessary Physics Proxy is created within the player / gameplay interaction radius.

## Refined AI CI Gates

### Architecture Gate — Hard Fail

Failure conditions:

```text
Gameplay / Public layer includes:
- d3d12.h
- vulkan.h
- Metal native headers

Hot Path:
- heap allocation
- malloc / new
- SharedPtr copy where forbidden by hot-path policy
```

Custom AST Checker / clang-tidy must be able to check namespace / module boundaries.

### Shader Gate — Hard Fail

```text
Canonical Reflection mismatch
Resource Mapping mismatch
Constant Layout mismatch
Argument Buffer mapping mismatch
```

Golden Image is validated according to profile tolerance.

### Asset Gate — Hard Fail

```text
Deterministic Cook Hash mismatch
Bundle Dependency DAG cycle
Invalid Bundle Generation dependency set
```
### Memory Gate — Hard Fail

```text
ASan failure
UBSan failure
Frame Arena escape
Use-after-reset
Allocator boundary violation
Handle generation validation failure
```

### Performance Gate

Dedicated Performance Runner:

```text
Hot-path allocation > 0
→ Hard Fail

Regression > calibrated threshold
→ Hard Fail
```

Shared CI Runner:

```text
Frame-time / timing fluctuation
→ Trend / Warning only
```

Do not issue unreliable hard failures for a general shared runner due to virtualized host jitter.

### Platform Gate — Hard Fail

```text
Blacklisted GPU Feature enabled
Capability Profile says unsupported but feature forced on
Known critical workaround omitted
```

### Gate Principle

```text
Architecture Contract
→ Automated Check where feasible
```

Hardware behavior that cannot be fully automated must be included in the Device Lab / Manual Validation Checklist, rather than pretending that CI has covered it.


Additional Hard Gates:

- FrameLifetime AST Storage Check
- Metal Binding Snapshot Semantic Diff
- Render Graph Transient Aliasing Validation
- Dynamic Module Allocator / ABI Boundary Check
- Bundle Pending-Delete Reference Drain Validation


v3.8 Additional Gates / Metrics:

- PipelineLayoutMetadata Determinism Check
- Component Sparse/Dense Integrity Check
- Render Graph Redundant Barrier Metric
- AI Change Scope / Risk Class Policy Check
- Texture Cook Determinism / Platform Format Validation


v3.9 Additional Hard Gate:

- Frame Memory × Module Boundary Lifetime Check


v3.10 Additional Gates:

- Unified Architecture Dependency Graph Check
- Shader Generated C++ Layout Determinism / Offset Check
- Third-party Worker Oversubscription Policy Check
- Runtime Plugin ABI Handshake Smoke Test
- Streaming Priority / IO Budget Scheduler Test


v3.11 Additional Gates / Metrics:

- Terrain LOD Seam Regression Scene
- Input Gesture Single-Owner / Duplicate Delivery Test
- Simultaneous Keyboard / Mouse / Gamepad Input Test
- Multi-touch Stable PointerID / Per-pointer Capture Test
- VirtualJoystick + LookRegion + VirtualButton Concurrent Touch Test
- UI Input Interaction Matrix Routing Test
- InputLayer → UILayer Runtime Mask Cook Test
- Local Multiplayer P1/P2 UI Isolation Test
- Fixed Tick Short Press / Release Preservation Test
- IME / Physical Key Separation Test
- Profiler Trace Schema / Correlation Validation
- CI Cache Key Isolation / Wrong-config Reuse Test


v3.13 PSO Residency Gates / Metrics:

- Runtime PSO Residency Long-run Stability Test
- PSO Fence-safe Eviction Validation
- PSO Eviction Miss Must Not Block Render Thread


v3.14 Additional Gates:

- Variant Budget Stage Accounting Check (Theoretical / Pruned / Used / Cooked)
- Multi-Queue PSO Fence-safe Destruction Validation

## v3.6 Architecture Decision Matrix

| Module / Domain | v3.6 Final Decision | Core Value |
| --- | --- | --- |
| Shader CI / Fallback | Canonical Reflection comparison; select fallback during Build/Cook; do not dynamically switch compiler paths in Shipping | Stable binding contract |
| RHI Rendering | Public API exposes only modern `BeginRendering` semantics; Backend retains compatibility fallback | Clean high-level API while preserving driver survival space |
| Dynamic Skinning | Retain Global Skinning Buffer + `skinningMatrixOffset` | Reduce descriptor pressure |
| Frame Arena | Reset by Main Thread or AllocatorManager after the Life Barrier | Centered on lifetime correctness |
| Bundle Update | Versioned Storage + Generation Pinning | Avoid mixed-version dependencies |
| Terrain / Vegetation | Configurable chunk profile + abstract physics proxy | Decouple render/physics/streaming |
| Performance CI | Dedicated Runner hard gate; Shared Runner trend only | Reduce false positives |

## Frame Memory Lifetime Safety Contract

Frame-lifetime memory uses types without Ownership semantics:

```cpp
template<typename T>
class FramePtr;

template<typename T>
class FrameSpan;
```

The following is prohibited:

```text
FrameUniquePtr
```

This prevents developers or AI Agents from mistakenly assuming that Frame Memory has a general owning pointer / destructor lifetime.

### Runtime Guard

Debug / Development Builds must maintain:

```text
Allocator Region Metadata
Frame Generation Counter
Poison-on-reset
Use-after-reset Guard
```

Each Frame Region must record at least:

```text
Region Begin
Region End
Frame Generation
Owning Arena
Owning Thread / Worker
Reset State
```

`FramePtr<T>` / `FrameSpan<T>` may validate the following when dereferenced in Debug:

```text
Pointer belongs to active frame region
Generation matches active frame
Arena not reset
Thread access policy valid
```

After reset, a poison pattern / guard page may be used, when permitted by the platform, to improve error observability.

### Static / AST Rule

Custom Clang-Tidy / AST Checker must prohibit:

```text
Persistent Scene Component
Resource Manager
Asset Object
Long-lived Subsystem State
Global / Static Storage
```

from storing:

```text
FramePtr<T>
FrameSpan<T>
FrameAllocator-backed container
Any type explicitly tagged FrameLifetime
```

Architecture Gate:

```text
Frame-lifetime type stored in persistent field
→ Hard Fail
```

---

### Module Boundary Extension

Frame lifetime safety likewise applies to:

```text
Runtime Plugin
Dynamic Module
Third-party SDK Bridge
Deferred Callback
Async Worker Queue
```

Frame lifetime annotations must not be lost because data crosses a DLL / so / dylib boundary.

## Native Overlay WebView Logical Screen Contract

The WebView V1 positioning baseline is elevated to a formal API Contract:

```text
OS Window / Logical Screen Space
```

rather than:

```text
Render Graph Internal Resolution
Dynamic Resolution Render Size
3D Viewport Scale
```

For example:

```text
Logical Screen: 1920 x 1080
3D Dynamic Resolution: 70%
Internal Render Size: 1344 x 756

WebView Layout:
→ still calculated using 1920 x 1080 logical coordinates
```

Viewport Sync:

```text
RectTransform / Anchor / Safe Area
↓
Logical Screen Rect
↓
DPI / Retina / Density Conversion
↓
Native View Bounds
```

Changes to Dynamic Resolution / Render Scale:

```text
→ do not directly change the WebView logical rect
```

Editor:

- If the Canvas / View uses Dynamic Resolution, the WebView Inspector displays a Native Overlay warning
- Runtime automatically performs logical-to-native bounds remapping
- The Render Graph viewport must not be used as the WebView layout reference

---

## Bundle Pending-Delete / Mapping Lifetime Contract

Bundle Hot Update uses:

```text
Versioned Storage
+
Generation Pinning
+
Pending Delete Queue
```

An old Generation is not forcibly deleted at a fixed frame boundary.

States:

```text
Active
↓
Retired
↓
PendingDelete
↓
SafeToDelete
↓
Deleted
```

Necessary conditions for entering `SafeToDelete`:

```text
LoadContext RefCount == 0
AsyncIO RefCount == 0
MappedView RefCount == 0
OpenFileHandle Count == 0
Decode / Decompress Job Count == 0
Streaming Transaction Count == 0
```

Only when all counts reach zero may the following be performed:

```text
munmap / UnmapViewOfFile
close / CloseHandle
delete directory
```

New Bundle:

```text
Downloaded
↓
Verified
↓
New Generation Created
↓
Active Generation Pointer Switch
```

Old version:

```text
Read-only until all references drained
↓
PendingDelete Queue
↓
Background cleanup or next Pre-init
```

This does not rely on the assumption that “the next Frame is guaranteed to be safe.”

---

## GPU Skinning Alignment Contract

Global Skinning Buffer uses:

```text
Structured / Storage Buffer
```

`SkinnedDrawData`:

```cpp
struct alignas(16) SkinnedDrawData
{
    uint32_t transformIndex;
    uint32_t materialIndex;
    uint32_t meshIndex;
    uint32_t skinningMatrixOffset;
};
```

`skinningMatrixOffset`:

```text
Matrix Element Index
```

Not:

```text
Byte Offset
Descriptor Index
Per-character Constant Buffer Offset
```

Therefore, 256-byte padding per character is not mandatory.

Concept:

```cpp
byteOffset =
    skinningMatrixOffset * sizeof(Matrix4x4);
```

Alignment responsibility:

```text
DynamicGpuBufferPool
+
RHI Backend
```

Determined by Resource Type and Device Limits:

```text
Storage Buffer alignment
Uniform / Constant Buffer alignment
Copy alignment
Backend-specific resource alignment
```

If a backend uses dynamic storage/uniform descriptors, it should follow the actual limits reported by the device.

High-level Animation / Renderer code must not hardcode fixed alignment constants.

---

## Shader Binding Snapshot / Render Graph Aliasing Gate

### Slang Metal Binding Snapshot

In addition to Canonical Reflection, the Metal Shader Gate adds a normalized binding snapshot.

Two paths:

```text
Direct:
Slang → MSL

Fallback:
Slang → SPIR-V → SPIRV-Cross → MSL
```

Each produces:

```text
AST-normalized / Reflection-normalized Binding Snapshot
```

Compare:

```text
Logical Resource ID
Resource Type
Binding Group
Array Count
Access
Stage
Argument Buffer Group
Argument Buffer Member ID
Constant Layout
Specialization Metadata
```

Any inconsistency:

```text
Shader Gate → Hard Fail
```

Note:

```text
Do not compare whether the MSL text is identical
Do not compare whether the binary is identical
```

Only the canonical / normalized semantic contract is compared.

### Transient Aliasing Gate

The Render Graph must perform the following for Transient Resource Reuse:

```text
Lifetime Interval Validation
Read / Write Overlap Validation
Alias Compatibility Validation
Barrier / Transition Validation
```

The following is prohibited:

```text
Resource A still live
+
same memory reused by Resource B
```

Or:

```text
Aliased resource overlapping read/write lifetime
```

Development / CI:

```text
Transient Aliasing Hazard
→ Hard Fail
```

The Render Graph compiler should output readable diagnostics:

```text
Pass A
Resource X
Lifetime [3, 7]

Pass B
Resource Y
Lifetime [6, 9]

Aliased Heap Range overlap detected
```

---

## Dynamic Module Engine Allocator Contract

The cross-DLL / dylib / so boundary in Modular Development Builds adopts a strict allocator contract.

Cross-Module Public APIs should preferably use:

```text
Handle
POD
Span / View
StringView
Opaque ID
Versioned C ABI Struct
```

Avoid passing across Modules:

```text
std::vector
std::string ownership
SharedPtr-managed internal object
Allocator-private container
Concrete internal class
```

### Heap Ownership Rule

By default:

```text
Module A allocates
→ Module A destroys
```

Or:

```text
All participating modules
→ Explicit Engine Allocator API
```

Prohibited:

```text
DLL A CRT new
↓
EXE / DLL B CRT delete
```

If cross-Module allocation is required:

```cpp
IMemoryAllocator* allocator = MemoryManager::GetAllocator(...);
```

However, the Public API should not require Gameplay to directly operate a low-level allocator.

Recommended pattern:

```text
CreateObject()
→ returns Handle

DestroyObject(handle)
→ routed back to owning module
```

Or:

```text
Module-provided Destroy function
```

### CI / ABI Gate

Check:

```text
Exported API contains forbidden STL owning type
Cross-module allocator mismatch
Missing destroy function
ABI version mismatch
```

If any condition is met:

```text
Architecture / ABI Gate → Hard Fail
```

### Frame / Transient Memory Boundary

Although Cross-Module Public APIs may use `Span / View`, this is limited to:

```text
Stable / persistent backing storage
+
explicitly documented lifetime
```

If the backing storage comes from a Frame Arena / Transient Allocator:

```text
Ordinary Span / View
→ Forbidden

FrameSpan / FrameDataHandle
→ Required
```

If a Plugin needs to retain data, it must copy it to Plugin-owned persistent memory.

## Component Storage Contract / Sparse Set + Dense SoA

General Scene Component Pools use the following by default:

```text
Sparse Set / Sparse Array
+
Dense Entity List
+
Dense SoA Component Data
```

Core data flow:

```text
EntityID
↓
Sparse Index
↓
Dense Slot
↓
Dense SoA Component Data
```

### Lookup

```text
EntityID.index
↓
sparse[index]
↓
dense slot
```

Validation:

```text
denseEntities[slot] == EntityID
```

May provide:

```text
HasComponent → O(1)
GetComponent → O(1)
Iteration → Dense Sequential Memory
Remove → Swap-and-Pop
```

### Default, Not Universal

This structure is:

```text
General Scene Component Pool
→ Default Contract
```

It is not the only Storage structure for every Subsystem.

The following data may use specialized structures:

- Singleton data
- RenderWorld temporary arrays
- Particle / VFX batches
- GPU-driven instance data
- highly dense specialized arrays
- Terrain / Vegetation cluster data

Core principles:

```text
Hot Iteration
→ Dense

Random Entity Lookup
→ Sparse
```

Avoid using a single heavyweight OOP object model as runtime hot-path storage.

---

## Offline Pipeline Layout Metadata

The Shader Binding Pipeline is formally defined as:

```text
Slang Source
↓
Shader Compiler
↓
Canonical Reflection
↓
Pipeline Layout Builder
↓
PipelineLayoutMetadata
↓
Cooked Runtime Asset
↓
Runtime Direct Indexed Access
```

`PipelineLayoutMetadata` must contain at least:

```text
Shader ID
Variant Key
Resource ID
Resource Type
Set / Space
Binding
Array Count
Access
Stage Visibility
Argument Buffer Group
Argument Buffer Member ID
Constant / Parameter Layout
Push / Root / Constant Transport Metadata
Specialization Metadata
Vertex Input
Fragment Output
Binding Tier
```

### Runtime Rule

The following is prohibited:

```text
Runtime
→ string lookup
→ dynamic hash binding assembly
→ build descriptor layout on demand
```

Runtime should:

```text
Load PipelineLayoutMetadata
↓
Direct index / precomputed mapping
↓
Create / lookup compatible PSO
```

Objectives:

- Reduce runtime CPU overhead
- Avoid binding contract drift
- Allow DX12 / Vulkan / Metal to share the same canonical logical layout
- Allow AI / CI to directly diff metadata

### Determinism

The same:

```text
Shader Source
Compiler Version
Variant Key
Backend Profile
```

should produce deterministic metadata.

Inconsistency:

```text
Shader / Asset Gate
→ Hard Fail
```

---

## Worker Frame Arena Page / Chunk Growth Policy

`FrameAllocatorSet`:

```text
MainThread Arena
RenderThread Arena
Worker Arena[N]
```

Remains unchanged.

The Worker Arena capacity strategy is supplemented as follows:

```text
Primary Chunk
↓ overflow
Additional Chunk
↓ overflow
Additional Chunk
```

Allocation hot path:

```text
Thread-local
+
O(1) pointer bump
```

Do not introduce a global allocator lock.

### Alignment

Each allocation provides the actual data requirement:

```text
Requested Alignment
```

For example:

```text
16-byte
32-byte
SIMD / Backend-required alignment
```

Do not hardcode a single alignment value as the permanent ABI for all objects.

### Budget

Each Arena / Worker Profile tracks:

```text
Initial Capacity
Current Reserved
Current Used
Peak Used
Soft Limit
Hard Limit
Overflow Count
Chunk Count
```

### Reuse

After Frame completion:

```text
Reset cursors
↓
Reuse existing chunks
```

There is no need to free allocated pages every Frame.

Spike:

```text
Normal usage: 4 MB
Temporary spike: 12 MB
Next frames: reuse pages
Long-term low usage: trim excessive pages
```

### Trim Policy

Avoid:

```text
Every overflow → system heap allocation
Free / realloc every frame
```

Trimming may be performed during:

```text
Memory pressure
Scene transition
Long idle window
Periodic maintenance
```

When the Hard Limit is exceeded:

```text
Development
→ Assert + Capture

Shipping
→ controlled fallback / fail-safe policy
```

Defined per subsystem.

---

## Performance Policy Manager

Mobile / Laptop runtime establishes a unified:

```text
PerformancePolicyManager
```

Inputs:

```text
Thermal State
Battery / Power State
CPU Frame Time
GPU Frame Time
Frame Pacing
Memory Pressure
Display Refresh Rate
Device Tier
User Quality Setting
```

Outputs:

```text
Target FPS
Dynamic Resolution Scale
Shadow Quality
LOD Bias
Post Processing Quality
Vegetation Density
Animation Update Rate
Streaming Budget Bias
```

### Platform Timing

Android:

```text
Frame Pacing abstraction
→ may integrate an Android Frame Pacing / Swappy-type backend
```

iOS:

```text
Display timing abstraction
→ CADisplayLink / platform display timing backend
```

The Public Engine does not directly depend on a specific third-party name.

### Thermal Policy Example

```text
Normal
→ target 60 FPS
→ render scale 1.0

Warning
→ keep target if possible
→ lower internal resolution

Serious
→ reduce GPU features
→ lower resolution
→ optional lower target FPS

Critical
→ aggressive quality reduction
→ lower target FPS
```

Specific FPS / Scale values are not hardcoded at the architecture layer; they are determined by:

```text
Quality Profile
+
Device Tier
+
Game Policy
```

Gameplay must not directly read the OS thermal API and adjust visual quality independently.

---

## AI Change Scope / Risk Policy

AI Agent change restrictions are centered on:

```text
Risk
+
Architecture Boundary
+
Primary Goal
```

A simple fixed Diff Line Count is not used as the primary Hard Limit.

### Session Contract

For one AI Session / PR, in principle:

```text
Primary Goal
→ 1

Primary Subsystem
→ Prefer 1
```

Cross-Subsystem modifications are permitted for:

```text
An explicit Vertical Slice
or
A necessary Architecture Change
```

### Risk Class

```text
Risk A
→ Docs / Tests / Generated Metadata / Mechanical Changes

Risk B
→ Normal Subsystem Implementation

Risk C
→ RHI
→ Allocator
→ Job System
→ Render Graph
→ Asset Format
→ ABI / Plugin Boundary
→ Serialization Compatibility
```

Risk C:

- Must have narrow scope
- Must have a dedicated CI Gate
- Must undergo Architecture Contract review
- Architectural changes require an ADR

### Diff Size

Diff Size:

```text
Soft Signal
```

Not the only Hard Gate.

For example:

```text
2000 lines generated metadata
→ may be low risk

20 lines allocator lifetime change
→ may be high risk
```

### Cross-module Change

Avoid having a single AI PR substantially modify all of the following simultaneously without a clear reason:

```text
RHI
+
Editor UI
+
Asset Cooker
+
Physics
+
Runtime UI
```

If it is a Vertical Slice:

```text
It may cross modules,
but each boundary must have a corresponding test / gate
```

---

## Render Graph Barrier Optimization Policy

The existing Render Graph Resource Lifetime / Barrier / Aliasing Contract remains in effect.

Add a Transition Planner:

```text
Logical Resource State
↓
Render Graph Compiler
↓
Transition Planner
↓
Barrier Merge / Optimization
↓
Backend Translation
```

Validation:

```text
Missing Barrier
→ Hard Fail

Invalid Transition
→ Hard Fail

Aliasing Hazard
→ Hard Fail

Illegal Read / Write Overlap
→ Hard Fail

Redundant Barrier
→ Metric / Warning
```Redundant Barrier defaults to not Hard Fail.

Reasons:

```text
Driver Workaround
Conservative Backend Policy
Debug Validation Mode
```

May intentionally generate additional synchronization.

The Profiler should track:

```text
Barrier Count
Merged Barrier Count
Redundant Barrier Estimate
Transition Count
Alias Barrier Count
```

---

## Texture Cook Profile / Platform Transcoding

Runtime Texture Format does not use:

```text
Desktop = all BC7
Mobile = all ASTC with the same Block Size
```

Instead, it is determined according to:

```text
Texture Semantic
Quality Tier
Platform
GPU Capability
Memory Budget
```

Desktop example:

```text
Color / Albedo
→ BC7

Normal
→ BC5 or profile-selected BC format

Single-channel Mask
→ BC4

Two-channel Data
→ BC5

HDR
→ BC6H
```

Mobile:

```text
ASTC
→ block size by texture class / quality profile
```

For example:

```text
UI / Hero Texture
→ higher quality ASTC profile

Terrain / Background
→ more aggressive ASTC block size
```

Asset Pipeline:

```text
Source Texture
↓
Import
↓
Canonical Texture Intermediate
↓
Platform Cook
├─ Windows BC*
├─ Android ASTC
├─ iOS ASTC
└─ macOS capability-selected
↓
Bundle
```

Runtime does not directly depend on PNG / TGA / source format.

---

## Golden Image Contract Retained

The existing Golden Image architecture is retained; no duplicate rules are established.

The following are formally retained:

```text
SSIM
+
Perceptual Diff
+
Pixel Error Guardrail
```

And tolerance is configured according to:

```text
Same Backend
Cross Backend
PBR
Shadow
Post Processing
Compression
Temporal
Platform / Driver Family
```

Threshold:

```text
Calibrated using actual measurements from the Device Lab / Baseline
```

rather than requiring pixel-perfect results across GPUs.

## Frame Memory × Module Boundary Contract

The lifetime rules for frame-lifetime memory must cross the Engine / Runtime Plugin / Dynamic Module boundaries.

### Core Rule

Memory originating from:

```text
Frame Arena
Transient Allocator
Per-frame Scratch Buffer
Worker Frame Arena
Render Extraction Scratch
```

must not be disguised as ordinary:

```text
Span<T>
View<T>
Raw Pointer
```

when passed across Modules.

Transient frame data crossing Modules may use only:

```text
FrameSpan<T>
FrameDataHandle
```

### FrameSpan Contract

```text
Ownership
→ None

Lifetime
→ Current callback / declared frame scope only

Retention
→ Forbidden

Async Capture
→ Forbidden

Persistent Storage
→ Forbidden

Implicit Conversion to Span/View
→ Forbidden
```

For example:

```cpp
void MyPlugin::OnRenderExtract(
    FrameSpan<const RenderItem> items)
{
    Process(items);       // OK

    // m_items = items;   // Forbidden
}
```

If a Plugin needs to retain data across callbacks or frames:

```text
FrameSpan<T>
↓
Explicit Copy
↓
Plugin-owned Persistent Storage
```

For example:

```cpp
m_items.assign(items.begin(), items.end());
```

### FrameDataHandle

For high-risk third-party Runtime Plugins, the following may be preferred:

```text
FrameDataHandle
```

Concept:

```text
FrameDataHandle
├─ Index
└─ FrameGeneration
```

Resolve:

```text
FrameDataHandle
↓
Engine API
↓
Validate FrameGeneration
↓
FrameSpan<const T>
```

If the Handle has expired:

```text
Resolve
→ Invalid / Error
```

Returning a stale pointer is forbidden.

### Synchronous Consumption Rule

V1 specification:

```text
FrameSpan callback
→ synchronous consumption only
```

A Plugin must not:

- Store `FrameSpan` in a member
- Capture it in a deferred lambda
- Pass it to a background thread
- Put it into an async queue
- Convert it to an ordinary `Span/View`
- Store its backing pointer in a persistent object

If async / deferred processing is required:

```text
Plugin must copy
```

V1 does not provide a `FrameAsyncLease` type, in order to reduce lifetime complexity.

### Dynamic Module Public API Rule

Cross-Module Public APIs are divided into:

```text
Persistent / Stable Data
→ Handle
→ POD
→ Stable Span / View with documented lifetime

Transient Frame Data
→ FrameSpan<T>
→ FrameDataHandle
```

Ordinary `Span/View` must not carry Frame Arena / Transient Allocator backing storage.

### Static Analysis / CI

Custom AST / Clang-Tidy must check:

```text
FramePtr / FrameSpan
→ Persistent member
→ Hard Fail

FramePtr / FrameSpan
→ Async/deferred lambda capture
→ Hard Fail

FramePtr / FrameSpan
→ converted to ordinary Span/View
→ Hard Fail

FramePtr / FrameSpan
→ exported without lifetime annotation
→ Hard Fail

Frame-lifetime backing storage
→ exposed through generic Public API
→ Hard Fail
```

If a Runtime Plugin is third-party and cannot run the complete AST Checker:

```text
Debug Runtime Guard
+
Frame Generation Validation
+
Region Metadata Validation
```

must still detect lifetime violations.

### Relationship to Systemic Risk

This Contract directly corresponds to:

```text
AI-generated Lifetime / Memory Bugs
```

and connects the following three existing architectures:

```text
Frame Memory Lifetime Safety
+
Dynamic Module Allocator / ABI Contract
+
AI Architecture / Memory Gate
```

## Unified Architecture Dependency Graph Gate

Existing boundary rules for RHI / Plugin / Public API / ABI and others are unified into a shared:

```text
Architecture Dependency Graph
```

The purpose is not to establish another set of duplicate specifications, but to allow all existing Dependency Direction Contracts to use the same static-analysis foundation.

### Layer Direction

Example:

```text
Game / Gameplay
↓
Engine Public API
↓
Subsystem API
↓
Engine Internal
↓
Platform / Backend
```

Reverse dependencies are forbidden, for example:

```text
Core
→ Gameplay

Renderer Internal
→ Editor Feature

Engine Public
→ Vulkan / DX12 / Metal native header
```

### Tooling

The Architecture Gate may jointly generate the Dependency Graph through:

```text
Include Graph Scanner
+
Clang AST / Compile Database
+
Module Rule Manifest
```

Each module defines:

```text
Module Name
Layer
Allowed Dependencies
Forbidden Dependencies
Public Include Roots
Private Include Roots
```

CI:

```text
Forbidden Edge
→ Hard Fail
```

and outputs the complete dependency path, avoiding reporting only a single include.

---

## Third-party Job System Adapter Contract

If a third-party Runtime Library has its own Task Scheduler / Thread Pool, it must not default to independently creating a complete Worker Pool unrelated to the Engine.

Add:

```text
JobSystemTaskAdapter
```

### Goal

Avoid simultaneously creating large numbers of threads through:

```text
Engine Job Workers
+
Jolt Workers
+
Middleware Workers
+
Plugin Workers
```

which causes:

```text
CPU Oversubscription
Context Switch
Cache Thrash
Frame Pacing Regression
```

### Preferred Integration

If the third-party API supports a custom job/task interface:

```text
Third-party Task
↓
JobSystemTaskAdapter
↓
Engine Job System
```

The Adapter must map:

```text
Task Priority
Dependency / Completion
Worker-safe Scratch
Cancellation where supported
Thread Affinity requirement
```

### Exception

If a third-party library requires using its own threads:

```text
Dedicated Thread Budget
+
Affinity / Priority Policy
+
Profiler Visibility
```

must be centrally recorded by the Engine; background threads must not be created without limits.

### Middleware Examples

Applicable to:

```text
Jolt
Audio / FMOD related worker integration where supported
Asset Decoder
Networking Library
Runtime Plugin
Third-party SDK
```

Whether the Engine Job System can actually be shared depends on the capabilities of the third-party API; libraries without support are not forced to use unsafe wrappers.

The Profiler must track at least:

```text
Engine Worker Count
External Worker Count
Runnable Task Count
Context Switch Trend
Third-party Job Time
Oversubscription Warning
```

---

## Shader Reflection → C++ Layout Generation

In addition to validation, Canonical Reflection serves as the single generation source for C++ Shader Data Layouts.

Pipeline:

```text
Slang Source
↓
Slang Reflection
↓
Canonical Reflection Metadata
↓
C++ Layout Generator
↓
Generated Shader Parameter Header
```

For example, it generates:

```cpp
struct alignas(16) GeneratedMaterialParams
{
    // generated fields...
};
```

as well as:

```cpp
static_assert(sizeof(GeneratedMaterialParams) == kExpectedSize);
```

### Source of Truth

The following long-term dual maintenance is forbidden:

```text
Shader CBuffer layout
+
Manually maintained independent C++ struct
```

The C++ generated header must be generated from canonical reflection.

### CI

The Shader Gate validates:

```text
Generated Header Hash
Canonical Reflection Layout
Field Offset
Field Size
Alignment
Array Stride
Matrix Layout
```

Any inconsistency:

```text
Hard Fail
```

The generated file should be deterministic and may use:

```text
Build-time generation
or
Cook-time generation + checked-in snapshot
```

but must not be generated dynamically by the Runtime.

---

## Streaming Priority Preemption / IO Concurrency Contract

The existing Asset UUID / Virtual Indirection / Residency State Machine remains unchanged.

Add the following Streaming Scheduler behavior:

```text
Priority-aware Queue
+
Preemption Policy
+
Per-platform IO Concurrency Budget
```

### Priority

For example:

```text
Critical
→ Player teleport destination
→ Required collision / terrain

High
→ Near-camera texture / mesh

Normal
→ Normal scene streaming

Low
→ Prefetch / speculative content
```

### Preemption

When a high-priority request arrives:

```text
Queued Low Priority Work
→ may be reordered / deferred
```

If the underlying IO cannot be cancelled:

```text
Do not force unsafe cancellation
→ limit new low-priority dispatch
→ prioritize next dispatch slot
```

### IO Concurrency

Disk/decompression/upload work must not be issued concurrently without limits.

The Profile defines:

```text
Max Disk IO In-flight
Max Decode Jobs
Max Upload In-flight
Max Remote Download In-flight
```

Actual values are adjusted according to:

```text
Platform
Storage Type
Device Tier
Thermal / Power State
```

Scheduler Profiler:

```text
Queue Depth by Priority
Average Wait Time
Preemption Count
In-flight IO
Decode Saturation
Upload Saturation
Starvation Count
```

Starvation prevention is required to prevent Low Priority work from never completing.

---

## Runtime Plugin ABI Handshake

ABI checks during Build / CI are retained; a handshake is performed again when the Plugin DLL / so / dylib is loaded at Runtime.

A stable C ABI entry point is recommended:

```cpp
extern "C" PluginInitResult Plugin_Init(
    const EnginePluginHost* host,
    const PluginInitInfo* initInfo);
```

### Runtime Validation

Before and after `Plugin_Init()`, validate at least:

```text
Engine API Version
Plugin API Version
ABI Version
Build Configuration Compatibility
Platform / Architecture
Plugin Manifest ID
Binary / Manifest Hash or Build ID
Required Capability Set
```

If the Plugin Binary was manually replaced, has an incorrect version, or does not match the manifest:

```text
Reject Load
→ Clear Error
```

Entering a partially initialized state is forbidden.

### Stable Boundary

Handshake structs must be:

```text
POD
Versioned
Size-tagged where appropriate
C ABI compatible
```

The following must not be passed directly across Modules at the entry point:

```text
std::string
std::vector
SharedPtr
Internal concrete class
FrameSpan without explicit callback lifetime
```

### Failure Safety

Plugin load process:

```text
Discover
↓
Manifest Validate
↓
Binary Identity Validate
↓
Plugin_Init Handshake
↓
Capability Validate
↓
Initialize
```

If any step fails:

```text
No partial activation
```

## Terrain Height Precision / LOD Seam Contract

The Terrain Heightmap is the Source of Truth, and its storage precision must be explicitly defined by the Terrain Asset Profile.

### Height Format

V1 recommends supporting:

```text
R16_UNORM
FP16 / R16F
```

The selection is determined according to:

```text
World Height Range
Required Vertical Precision
Compression / Storage Budget
Platform Capability
```

The rule “FP16 is always superior to R16_UNORM” must not be written as a fixed rule.

For example, for large-area Terrain whose height range can be normalized in advance:

```text
R16_UNORM
→ provides stable and predictable quantization precision
```

When special height representation or processing is required:

```text
FP16
→ profile-selectable
```

The Importer / Cooker should record:

```text
Min Height
Max Height
Scale
Bias
Height Encoding
```

to prevent the Runtime from guessing.

### LOD Seam Prevention

The Terrain Quadtree / Chunk across LOD boundaries must provide seam prevention.

Allowed strategies:

```text
Edge Stitching
Skirt
LOD Morphing
```

or combinations thereof.

The high-level Terrain Contract does not bind the implementation to a single method.

Required condition:

```text
Adjacent LOD Levels
→ no visible gap / crack
```

If LOD Morphing is used:

```text
Current LOD Height
↔ Parent / Coarser LOD Height
→ smooth transition
```

When Streaming temporarily causes neighboring Chunks to have different LOD / residency states, a seam-safe fallback must still be maintained.

Profiler / Debug View may display:

```text
Terrain LOD
Chunk Boundary
Stitch / Skirt State
Morph Factor
Missing Neighbor
```

---

## Vegetation Interaction Field

Existing Vegetation Wind remains:

```text
Global Wind
+
Procedural Noise
+
Pivot / Hierarchical Bend
```

Add simplified gameplay interaction:

```text
Vegetation Interaction Field
```

Sources may include:

```text
Player
Character
Vehicle
Explosion / Force Event
```

The Runtime writes a small number of interaction primitives into:

```text
Interaction Buffer
```

The Shader / Compute generates local Bend according to:

```text
Position
Radius
Strength
Direction
Falloff
Lifetime
```

### Core Rule

Large quantities of vegetation must not create per-Instance CPU Transform Updates due to interaction.

Preference:

```text
Small interaction primitive set
↓
GPU evaluation
↓
Vertex / Compute deformation
```

Physics Proxy and visual bend are separated:

```text
Visual Bend
≠
Full rigid-body simulation
```

Only nearby vegetation that is genuinely gameplay-relevant should create a Physics Proxy.

---

## Native Overlay Gesture Ownership / Forwarding Contract

Input Pipeline:

```text
Platform Raw Event
↓
engine::input
↓
Input Context / InputLayer
↓
UI Input Interaction Matrix / Route Policy
↓
Native Overlay / Runtime UI / Gameplay
```

The rules for Block / Pass-Through / ConsumeOnHit / BlockBelow and per-pointer ownership remain in place.

If specific gesture forwarding is supported in the future, the same event must not be consumed simultaneously by both Native and Engine.

### Ownership Rule

Each pointer / gesture sequence must have a unique Owner at any given time:

```text
Native Overlay
or
Engine UI / Gameplay
```

The following is allowed:

```text
Explicit Ownership Transfer
```

The following is forbidden:

```text
Duplicate Delivery
```

### Gesture Forwarding

A following option may be established:

```text
Gesture Forward Policy
```

For example:

```text
Single Tap
Pinch
Two-finger Pan
Wheel / Trackpad Gesture
```

However, forwarding must be:

```text
Native Recognize
↓
Normalized Engine Gesture Event
↓
Ownership Transfer / Synthetic Event
```

rather than passing the same raw touch stream to both sides simultaneously.

If the platform cannot support this reliably:

```text
Capability Query
→ Feature Disabled / Editor Warning
```

V1 still defaults to clear Rect + Block / Pass-Through behavior; complex partial-forwarding is an optional capability.

---

## Profiler Trace Export Contract

In addition to real-time display in the Editor, the Engine Profiler adds offline Trace Export.

Minimum support:

```text
Engine JSON Trace
+
Chrome Trace Event compatible JSON
```

and retains future:

```text
Perfetto-compatible trace
```

integration capability.

A Trace must be able to include at least:

```text
CPU Thread Timeline
Job Events
Render Pass Events
GPU Timing
Frame Markers
Asset Streaming Events
Memory Budget Events
Bundle IO
PSO Creation
Shader Compile
WebView / Platform Events
```

Each event should include:

```text
Timestamp
Duration
Thread / Queue
Category
Name
Frame ID
Optional Correlation ID
```

### Correlation

Cross-CPU / GPU / IO work should preferably use:

```text
Correlation ID
```

For example:

```text
Asset Request
→ Disk IO
→ Decode
→ GPU Upload
→ Ready
```

so that the complete lifecycle can be linked in the offline trace.

### Shipping

The Shipping Profiler may be truncated by default.

Development / Profile Build:

```text
Trace Capture Enabled
```

Release:

```text
Feature Stripping
→ optional / disabled by default
```

---

## CI Build Cache / Matrix Parallelization Policy

A large Feature Matrix should not rebuild all content from zero for every Job.

CI Build Infrastructure establishes:

```text
Compiler Cache
+
Artifact Cache
+
Parallel Matrix Jobs
```

Possible options include:

```text
sccache
ccache
compiler-native cache where appropriate
```

The specific tool is determined by the platform and Compiler Profile.

### Cache Key

The Build Cache Key must include at least:

```text
Source Revision
Compiler Version
Compiler Flags
Platform
Architecture
Build Configuration
Public ABI Version
Feature Set
PCH / Module Inputs
```

Shader / Asset Cooker Caches must use their own independent deterministic keys.

The following is forbidden:

```text
incorrect cross-config cache reuse
```

### CI Matrix

PR Gate:

```text
Representative Matrix
+
Fast Architecture / Shader / Asset Gates
```

Nightly:

```text
Wider Backend / Feature Matrix
```

Release:

```text
Full Shipping Matrix
```

Parallelization dimensions may include:

```text
Platform
Backend
Feature Profile
Minimal / Full Build
Sanitizer Profile
```

### Time Budget

PR feedback latency should be continuously monitored, but:

```text
15–20 minutes
```

is only an operational target / KPI, not a mandatory architectural guarantee.

CI Profiler tracks:

```text
Total Pipeline Time
Queue Time
Compile Time
Cache Hit Rate
Cache Miss Rate
Slowest Matrix Job
Artifact Upload / Download Time
```

## Runtime PSO Residency Budget / Fence-safe Eviction

PSO Warmup addresses “when to create,” but does not address “the resident quantity after extended execution.”

The Runtime must have an explicit Budget and reclamation mechanism for created PSOs / Pipeline Objects, avoiding unlimited accumulation after extended play across Scenes / Terrain Chunks / Vegetation Materials.

This mechanism is a different-level Gate from the `Variant Compile Budget` in the CI phase, which monitors the total number of compiled Variants; they must not be conflated:

```text
CI Variant Compile Budget
→ Build / Compile phase
→ monitors “how many Variants are compiled”

Runtime PSO Residency Budget
→ Runtime execution phase
→ monitors “the number and estimated cost of PSOs currently resident in the GPU / Driver”
```

### Residency Tracking

Use the existing Streaming Asset Residency State Machine model. Treat PSOs / Pipeline Objects as a resource category under the same Residency / Budget Framework; do not create a parallel system.

Track at least:

```text
Current Resident Count
Peak
Budget
High Watermark
Emergency Watermark
Evictable
Pinned
Last Used Frame
Last-Used Fence Value
Estimated Cost
```

`Pinned` applies, for example, to:

```text
Loading / Error Fallback Pipeline
Core UI
Always-required Debug / Bootstrap Pipeline
```

Whether these remain resident is determined by the Shipping Profile.

### Fence-safe Eviction

It is forbidden to determine reclaimability solely according to CPU-side Last-Used Time:

```text
Mark Candidate (LRU)
↓
Check Last-Used Fence Value
↓
GPU Fence Completed?
├─ No  → retain, defer until the next check
└─ Yes → allow Destroy
```

A PSO must not be Destroyed while any In-flight Command Buffer may still reference it.

Fence semantics use the same GPU completion contract as:

```text
Frame Arena GPU Frame Reuse
Dynamic GPU Buffer Reuse
Deferred GPU Resource Destruction
```

### Eviction Miss Handling

If a PSO is needed again after being evicted, existing rules must not be violated:

```text
Hot Render Loop
→ implicit synchronous creation of an expensive PSO is not allowed
```

Therefore:

```text
PSO Miss
↓
Async Recreate
↓
Existing Warmup / Pipeline Creation Queue
↓
Temporary Fallback Policy
```

The permitted transition strategy must be explicitly declared by the Render Feature / Material / Pass:

```text
Compatible Fallback Pipeline
or
Defer / Skip Draw for bounded frames
```Do not arbitrarily use a “similar Variant” as a replacement,
unless that fallback has been explicitly verified to have compatible:

```text
Vertex Layout
Render Target Format
Depth / Stencil Contract
Resource Layout
Shader Semantic
```

The Render Thread must not synchronously block while waiting for PSO reconstruction to complete.

### Watermark Behavior

```text
High Watermark
→ Preferentially reclaim Evictable and not recently used PSOs
→ Reduce the amount of unnecessary Warmup pre-creation

Emergency Watermark
→ More aggressively reclaim cold PSOs
→ Allow temporarily triggering Eviction Miss Fallback
```

Must not reclaim:

```text
Pinned
In-flight
Currently Building
Required by active critical render path
```

### Platform Note

Some Mobile GPU Drivers may experience additional driver-side memory / internal object pressure when large numbers of Pipeline Objects coexist.

Therefore, the Device Compatibility Matrix should record the following for representative devices:

```text
Recommended Resident PSO Budget
Observed Stable Peak
PSO Creation Cost
Driver / OS Version
Warmup Behavior
Eviction / Recreate Behavior
```

This is not merely a matter of CPU RAM / GPU VRAM;
driver-side stability and frame-time behavior must also be observed.

### GPU Queue / Fence Timeline Contract

The method for determining `LastUsedFenceValue` depends on the RHI submission timeline model.

If the engine maps all submissions that reference PSOs to a single Global GPU Completion Timeline:

```text
Single Unified Timeline
→ one LastUsedFenceValue
→ completedFence >= lastUsedFenceValue
→ Safe to Destroy
```

In this case, there is no need to additionally check each Frames-in-Flight slot.

If Queues such as Graphics / Compute use independent Fence Timelines:

```text
PSO Last Use
├─ Graphics Queue → LastUsedGraphicsFence
└─ Compute Queue  → LastUsedComputeFence
```

the Destroy condition must require all relevant queues to have completed:

```text
Graphics Completed >= LastUsedGraphicsFence
AND
Compute Completed >= LastUsedComputeFence
→ Safe to Destroy
```

The Copy Queue usually does not directly reference Graphics/Compute PSOs,
but if exceptions exist in future Backend / Pipeline types, they must still be included according to the actual resource usage contract.

Core principles:

```text
CPU Last-Used Frame
→ only LRU ordering signal

GPU Completion Fence
→ actual destruction safety condition
```

`FrameID` or CPU time must not be used in place of GPU completion verification.

### Profiler

The Profiler must display at least:

```text
Resident PSO Count
Peak Resident Count
Pinned Count
Evictable Count
PSO Budget
High / Emergency Watermark
Eviction Count
Eviction Deferred by Fence
Eviction Miss Count
Async Recreate Count
Runtime-created PSO Count
Worst Recreate Time
```

### V1 Definition of Done

- Runtime PSO Residency tracks Current / Peak / Budget / Watermark
- Eviction is performed only after GPU Fence completion is confirmed
- In-flight / Pinned PSOs must not be Destroyed
- Eviction Miss follows the Async path; the Render Thread does not synchronously block
- Fallback must be explicitly declared and layout / semantic compatible
- Watermark-triggered behavior can be observed in the Profiler
- At least one representative Mobile device verifies that the Resident PSO count stably converges during long-duration play across multiple Scenes


## v4.0, Formal System Decisions Not Yet Discussed in Detail (Architecture Completion Baseline)

This section supplements the items in the earlier “system list not yet discussed in detail” that have not yet formed complete Runtime Contracts.

If this section differs in detail from earlier brief descriptions:

```text
v4.0 Architecture Completion Baseline
→ Takes precedence over early summary text
```

However, it does not overturn already established core Contracts, such as:

```text
C++20 Native Core
Zig Primary Gameplay + Stable C ABI
Node + Component Public Workflow
EntityID + Sparse Set + Dense SoA Runtime
SceneGraph ≠ SpatialWorld ≠ RenderWorld
DX12 / Vulkan / Metal
Slang-only Shader Source
RenderGraph-owned GPU Hazards
SharedPtr only for true shared lifetime
UIElement ≠ SceneNode
World ≠ Scene ≠ StreamingCell
CPU Physics = Gameplay Truth
```

---

### A. Transform System

Formally adopt:

```text
Hierarchy Identity
+
Local TRS
+
High-precision World Position Foundation
+
Data-Oriented Transform Storage
```

Runtime storage:

```text
TransformStorage
├─ EntityID[]
├─ Parent[]
├─ FirstChild[]
├─ NextSibling[]
├─ LocalPosition[]
├─ LocalRotation[]
├─ LocalScale[]
├─ WorldTransform[]
├─ WorldVersion[]
├─ LocalVersion[]
└─ Flags[]
```

Formally distinguish:

```text
Local Transform
≠
World Position Representation
≠
Render-relative Transform
```

The Large World V1 foundation adopts:

```text
WorldPosition
= High-precision Region / Cell Coordinate
+ Local float position
```

The Renderer uses:

```text
WorldPosition
↓
Camera-relative conversion
↓
float GPU Transform
```

This avoids comprehensively converting all GPU / SIMD paths to double.

Hierarchy propagation:

```text
Local Changed
↓
LocalVersion++
↓
Hierarchy Dirty Queue
↓
Level / Depth ordered batch
↓
Parallel World Transform Propagation
↓
WorldVersion++
```

Reparent must use a structural command:

```text
Reparent(entity, newParent, KeepLocal | KeepWorld)
```

Rules:

- Parent must have the same Scene ownership as the Entity.
- `KeepWorld` must recalculate Local TRS.
- Hierarchy cycles are prohibited.
- Negative Scale may exist in the render transform, but Physics / certain skinning / tangent paths must have explicit restrictions and validation.
- `StaticTransform` is only an optimization hint; if modified, it automatically invalidates the static cache and must not produce undefined behavior.
- Transform traversal must not use a recursive virtual callback per Node.

V1 DoD:

- A 100k+ transform hierarchy can be updated in batches.
- Parent change / KeepWorld / KeepLocal have CI cases.
- Hierarchy cycles hard-fail.
- Large World → Render-relative conversion has precision tests.
- Scene unload leaves no dangling parent handles.

---

### B. System Scheduler / Component Update Model

Do not adopt:

```text
Component::Update()
Component::FixedUpdate()
Component::LateUpdate()
```

as a large-scale per-object virtual-dispatch model.

Formally adopt:

```text
World
↓
SystemRegistry
↓
SystemGraph
↓
JobSystem
```

System declarations:

```text
Reads<ComponentType>
Writes<ComponentType>
Reads<ResourceType>
Writes<ResourceType>
Phase
Priority
DeterminismPolicy
```

The Scheduler builds a DAG according to read/write conflicts:

```text
TransformRead + AnimationWrite
PhysicsWrite
GameplayRead/Write
...
↓
Dependency Graph
↓
Parallel Jobs
```

Formal Phases:

```text
FrameBegin
Input
VariableGameplay
FixedPrePhysics
FixedPhysics
FixedPostPhysics
Animation
LateGameplay
RenderExtraction
FrameEnd
```

Not every game must use all phases.

Structural Change:

```text
Create / Destroy / AddComponent / RemoveComponent / Reparent
→ WorldCommandBuffer
→ Structural Barrier
```

Zig Gameplay Systems use batch queries / views:

```text
Native Component View
↓ Stable C ABI
Zig System Batch
↓ Command Buffer
```

Avoid crossing the ABI once per Entity.

Deterministic mode:

- Fixed chunk partitioning.
- stable iteration order (only for systems declared to require determinism).
- deterministic reduction API.
- Deterministic systems must not implicitly read wall-clock / random device / unordered iteration.

V1 DoD:

- System read/write conflicts can automatically generate dependencies.
- Systems without conflicts can run in parallel.
- Structural mutation can only be applied at a safe barrier.
- The Profiler can display System → Job dependencies / waits.

---

### C. Engine Event / Message Framework

Input Events and UI Events do not replace an engine-wide typed event framework.

Formal structure:

```text
EventBus
├─ Immediate Local Dispatch
├─ Deferred Queue
├─ Cross-thread Queue
└─ Cross-ABI Event Stream
```

Event identity:

```text
EventTypeID
→ Stable numeric ID
```

The hot path does not use string event names.

Immediate events are limited to:

- The same thread.
- A clearly bounded call stack.
- Listeners must not cause unsafe structural mutation.

Deferred event:

```text
Producer
↓
Typed Event Queue
↓
Named Delivery Phase
↓
Consumer Systems
```

Cross-thread:

```text
Worker / Platform Thread
↓
MPSC / Thread-local staged queue
↓
World Event Merge
↓
Delivery Phase
```

Zig:

```text
Native Event Batch
↓
POD Event Records
↓ Stable C ABI
Zig
```

Do not retain raw Gameplay DLL callback/function pointers across Hot Reload.

Listener lifetime:

```text
SubscriptionHandle = Index + Generation
```

The module reload barrier must first revoke module subscriptions.

V1 DoD:

- Typed events.
- Immediate + Deferred.
- Cross-thread enqueue.
- A World-local event bus is the default; the global bus is only for platform/engine events.
- Zig event batches.
- A stale listener generation cannot be invoked.

---

### D. Camera Framework

A Camera is a Scene Component, but the Runtime Renderer uses the extracted `CameraView`.

```text
CameraComponent
↓
CameraSystem
↓
CameraView[]
↓
RenderWorld
↓
Renderer
```

Supported:

```text
Projection
├─ Perspective
└─ Orthographic
```

Camera data:

```text
Transform / WorldPosition
Projection Settings
Viewport
RenderTarget
CullingMask
ClearPolicy
Priority
Stack / Composition Policy
DynamicResolutionPolicy
PostProcessContext
```

Multiple Camera: formally supported in V1.

Camera Stack definition:

```text
Base Camera
↓
Overlay Cameras
↓
Composite Target
```

An Overlay is not equivalent to Runtime UI; it can be used for a weapon camera, mini-map, or special pass.

TAA:

```text
Logical Projection      ← non-jittered
Render Projection       ← jittered
```

`WorldToScreen()` / `ScreenToRay()` use the non-jittered projection by default to avoid input-picking jitter.

Culling Mask uses a project-defined render/culling layer bitmask and must not be mixed with the Physics Layer / UI Input Layer.

The Editor Camera is an Editor-owned view and does not need to become a PlayWorld Camera Entity.

V1 DoD:

- Perspective / Orthographic.
- Multiple camera / viewport / render target.
- Basic camera stack composition.
- ScreenToRay / WorldToScreen precision tests.
- TAA jitter does not contaminate gameplay picking.
- Dynamic Resolution per-camera policy.

---

### E. Lighting / Shadow Framework

A Light is a Scene Component; the Renderer uses a data-oriented LightRegistry.

```text
LightComponent
↓
Light Extraction
↓
LightRegistry / GPU Light Buffer
↓
Forward+ / Clustered Lighting
```

V1 Light Types:

```text
Directional
Point
Spot
```

Subsequent:

```text
Rect / Area Light
→ Bake / Advanced Runtime
```

Light mobility:

```text
Static
Mixed
Dynamic
```

Shadow:

```text
ShadowManager
├─ Directional Cascades
├─ Spot Shadow Atlas
├─ Point Shadow Atlas / Cubemap Strategy
├─ Cached Static Shadow
└─ Dynamic Shadow Budget Scheduler
```

Each Light does not permanently hold a fixed large shadow map.

Shadow allocation is based on:

```text
Importance
Screen Coverage
Distance
Mobility
Quality Tier
Recent Visibility
```

The Forward+ light list is generated by a GPU/CPU clustered builder; Gameplay does not access renderer-native data.

Probes:

```text
LightProbe / SH Volume
ReflectionProbe
```

Bake output follows the Asset Pipeline / Streaming Cell.

Large World:

```text
Probe Residency
→ follows World Partition / Streaming demand
```

V1 DoD:

- Directional/Point/Spot.
- CSM.
- Spot/point shadow atlas.
- Shadow cache foundation.
- Light culling / Forward+.
- Runtime sampling of baked probes / reflection probes.
- The Profiler displays shadow atlas occupancy / update cost.

---

### F. Post Processing Framework

Formally adopt Volume + Profile:

```text
PostProcessVolume
↓
PostProcessProfile
↓
Per-Camera Volume Resolve
↓
Blended Settings
↓
RenderGraph Passes
```

Volume:

```text
Global
Local Box / Sphere / Custom Volume
Priority
BlendDistance
Weight
```

A Profile is an Asset and can be reused.

V1 effects:

```text
Exposure
Tone Mapping
Bloom
TAA
FXAA fallback
SSAO
Color Grading
Vignette
```

Conditional / subsequent:

```text
Depth of Field
Motion Blur
SSR / advanced screen-space effects
```

The RenderGraph creates only the passes actually required; disabled effects retain no complete runtime cost.

Camera stack rule: the Base Camera determines the primary PostProcess; an Overlay may choose to inherit or use its own limited profile.

Environment volumes may provide Fog / Exposure / Color / Wind / Audio ambience and other cross-region blend sources, but each subsystem resolves its own corresponding data rather than creating one gigantic God Volume.

V1 DoD:

- Volume priority / blend is deterministic.
- Per-camera profile resolve.
- Quality tiers can strip effects.
- The Profiler displays effect timing / temporary RT memory.

---

### G. Navigation / Pathfinding Framework

V1 Default Backend:

```text
Recast
+
Detour
```

But it exists only in:

```text
Navigation/Private/RecastDetour
```

The Public API does not expose backend types such as `dtNavMesh*`.

World-level:

```text
World
└─ NavigationWorld
   ├─ NavMeshSet
   ├─ NavTile Registry
   ├─ Query Pool
   ├─ OffMesh Links
   └─ Dynamic Obstacle State
```

Core process:

```text
Scene / Cell Geometry
↓
Editor Nav Bake
↓
Tiled NavMesh Asset
↓
Bundle / Streaming Cell
↓
NavigationWorld Register Tile
```

V1:

```text
NavMesh Agent Type
Query Filter
Path Query
Async Path Query
OffMesh Link
Runtime Obstacle
Streaming Nav Tile
Debug Draw
```

Runtime obstacles do not, by default, perform frequent full rebakes:

- dynamic obstacle / tile-cache-style local updates。
- Large-scale geometry changes can mark affected tiles for asynchronous rebuild (with a budget).

A Navigation Agent does not directly drive the Transform:

```text
Navigation Path
↓
Desired Velocity / Steering
↓
Game-defined AI / CharacterIntent
↓
CharacterMotor
↓
CharacterController
```

A Room / Portal Graph can serve as a high-level path hint:

```text
Room Graph
→ coarse route
NavMesh
→ local precise route
```

V1 Crowd:

```text
Basic local avoidance / crowd service
→ Optional per agent
```

It is not required that all NPCs use the same Crowd solver.

V1 DoD:

- Tiled bake / load / unload.
- Cross-tile path.
- Cell unload leaves no stale nav poly ref.
- Async query cancellation.
- OffMesh Link.
- Character Controller integration sample.
- Nav profiler / debug view.

---

### H. Game AI Framework

The Engine provides generic AI capabilities and does not assume gameplay semantics such as “enemy,” “player,” or “MOBA.”

```text
AI Agent
├─ Perception
├─ Blackboard
├─ Decision Runtime
├─ Navigation Adapter
└─ Action Adapter
```

V1 Decision framework:

```text
Behavior Tree      ✅
Blackboard         ✅
FSM Utility        ✅ lightweight
Utility Scoring    ✅ reusable scorer layer
GOAP               △ Future / Plugin
ML Policy          ✅ Interface/Foundation
```

Behavior Trees do not use one C++ heap object per node per agent.

```text
BT Asset
↓
Editor Compiler
↓
Compact Node Program / Tables
↓
Per-Agent Execution State
```

Blackboard:

```text
Typed Slots
Bool / Int / Float / Vec / EntityRef / AssetRef / User-defined POD
```

Slot layouts are fixed at cook time; the hot loop does not look up keys in a string dictionary.

Perception:

```text
Stimulus Registry
↓
Spatial Query / Physics Batch Query
↓
Budgeted Perception Update
```

V1 perception: Sight / Hearing event abstraction; each AI is not required to raycast every frame.

AI LOD:

```text
Critical
Full
ReducedRate
Lightweight
Dormant
```

This is separate from Render LOD.

ML / Self-play interface:

```text
Observation Buffer
↓
Policy Interface
↓
Action Buffer
```

The Runtime Engine does not include a built-in training framework; the Training bridge / self-play runner is provided as tools / an external process.

This allows RPG / Strategy / MOBA / Simulation to share the underlying foundation.

V1 DoD:

- BT compiler/runtime.
- Typed blackboard.
- Perception budget.
- Navigation/action adapters.
- AI update LOD.
- Deterministic debug trace.
- The ML policy interface does not depend on a specific ML runtime.

---

### I. Editor Framework

Editor UI: Dear ImGui Docking / Multi-Viewport.

However, the Editor Architecture is not tightly coupled to the ImGui widget tree.

```text
EditorApplication
├─ EditorDocumentManager
├─ EditorSelectionService
├─ EditorObjectAdapter Registry
├─ InspectorRegistry
├─ PropertyDrawerRegistry
├─ EditorCommand / Undo Service
├─ DragDrop Service
├─ Clipboard Service
├─ Gizmo Service
├─ Asset Browser
├─ Search Service
├─ Project Settings
└─ Editor Plugin Host
```

Document Model:

```text
EditorDocument
├─ SceneDocument
├─ PrefabDocument
├─ MaterialDocument
├─ DataTableDocument
├─ AnimationDocument
└─ AudioEventDocument
```

A Document has:

```text
Dirty State
Save / SaveAs
Undo Domain
Selection Context
External-change Detection
Reload / Merge Policy
```

Selection is not a `Node*`:

```text
EditorObjectHandle
→ Adapter
```

Inspector:

```text
Reflection Metadata
+
Custom PropertyDrawer
+
Custom Component Inspector
```

Scene View:

- Perspective / Orthographic.
- Move/rotate/scale gizmo.
- Grid / angle / surface snap.
- Local/world/pivot modes.
- Mixed Scene + Runtime UI hierarchy editing.
- World Partition / Portal / HLOD overlays.

Editor Plugin:

```text
RegisterWindow
RegisterInspector
RegisterPropertyDrawer
RegisterImporter
RegisterMenuCommand
RegisterToolbar
RegisterAssetEditor
```

Plugin unload must first revoke Editor registry entries.

V1 DoD:

- Multiple documents.
- Dock layout persistence.
- Selection/inspector adapter.
- Scene gizmo.
- Asset browser/search.
- Settings pages.
- DnD/clipboard.
- Plugin extension points.

---

### J. Prefab Framework

A Prefab is a persistent composition asset, not a special runtime entity type.

```text
PrefabAsset
├─ LocalObjectID
├─ Hierarchy
├─ Components
├─ NestedPrefabInstance
└─ Default Properties
```

In a Scene:

```text
PrefabInstance
├─ PrefabAssetUUID
├─ InstanceRootUUID
├─ LocalID → Instance UUID Map
└─ OverrideSet
```

Formally supported:

```text
Nested Prefab      ✅ V1
Prefab Variant     ✅ V1
Property Override  ✅
Add Component      ✅
Remove Component   ✅
Add Child          ✅
Remove Child       ✅
```

Override identity:

```text
PrefabLocalObjectID
+
Stable PropertyID / PropertyPath
```

Do not use a hierarchy index as the override identity.

Variant:

```text
Base Prefab
↓
Variant Override Set
↓
Instance Override Set
```

Rebase:

```text
Old Base
+
Instance Override
+
New Base
↓
Deterministic Rebase
```

If a base property changes and the instance has no override → accept the new value.

If the instance already has an override → retain the override.

If the target component/property is deleted from the source or its type is incompatible → `PrefabConflict`; it must not be silently discarded.

Runtime Cook:

```text
Authoring Prefab Instance
↓
Cook
↓
Optimized Scene / Spawn Data
```

Shipping runtime does not need to retain the complete Editor override machinery for the prefab workflow.

Hot Reload: Editor / Development may rebuild prefab instances; Gameplay runtime state is not automatically overwritten unconditionally and requires an explicit policy.

---

### K. Undo / Redo Framework

Formally adopt a Transaction Journal, not merely a single `ICommand` stack.

```text
EditorTransaction
├─ Property Deltas
├─ Structural Commands
├─ Asset Changes
└─ Selection Restore Metadata
```

Continuous dragging:

```text
MouseDown
↓ Begin Transaction
N × transient changes
↓ coalesce
MouseUp
↓ Commit single undo step
```

Supports:

- Property Change.
- Multi-object Edit.
- Create/Delete Entity.
- Add/Remove Component.
- Reparent.
- Prefab Apply/Revert.
- UI Edit.
- Material / DataTable / Asset editor transaction.

Snapshots are used only for complex operations that are difficult to express as deltas; ordinary properties do not duplicate the entire Scene.

Memory:

```text
UndoMemoryBudget
→ oldest transaction eviction
```

Cross-Document mutations must be placed in the same transaction group or explicitly split; “half undo” is not allowed.

Undo/Redo is Editor-only; Shipping does not carry history.

---

### L. Reflection Framework

Maintain the existing V1 decision:

```text
V1
→ Runtime Metadata + Template / Macro Registration

Future
→ Optional Clang-based Metadata Generator
```

However, v4.0 completes stable identity.

```text
TypeDescriptor
├─ TypeGUID / Stable TypeID
├─ Name
├─ Base Type
├─ Size / Alignment
├─ Properties[]
├─ Attributes[]
└─ Lifecycle / Factory Hooks where allowed
```

Property:

```text
PropertyDescriptor
├─ Stable PropertyID
├─ Name
├─ Type
├─ Offset / Accessor
├─ Container Metadata
└─ Attributes
```

Attributes:

```text
Range
Step
Tooltip
Header
ReadOnly
Hidden
AssetType
Enum
Multiline
Color
Units
EditorOnly
```

Reflection does not depend on C++ RTTI for stable serialized identity.

PropertyID / TypeID renames must have an alias/migration mechanism; using only the current string hash as the sole long-term persistent contract is prohibited.

Plugin registration:

```text
Module Load
→ Register TypeDescriptor

Module Unload Barrier
→ No live reflected instance / callback
→ Unregister Descriptor
```

Zig only obtains a flat metadata view / generated C descriptor; C++ templates / STL are not exposed.

Uses of Reflection:

- Inspector.
- Serialization.
- Prefab Override.
- Undo/Redo.
- Property Animation.
- Runtime Debug.

Hot-loop simulation does not depend on reflection lookup.

---

### M. General Serialization Framework

The Engine JSON Framework is one Serialization backend; this does not mean that all runtime formats are JSON.

Formally:

```text
Serialization
├─ JSON Authoring / Human-readable
├─ Binary Runtime Blob
└─ Specialized Asset Formats
```

General APIs:

```text
ArchiveReader / ArchiveWriter
SchemaVersion
ObjectReferenceResolver
MigrationRegistry
```

Authoring:

```text
Scene / Prefab / Settings
→ Deterministic JSON where practical
```

Shipping Runtime:

```text
Authoring Data
↓ Cook
Relocatable Binary Blob
↓
Fast Runtime Load
```

Runtime binary principles:

- Canonical little-endian format.
- Do not directly dump C++ struct memory.
- Offset/index-based references.
- Explicit alignment.
- Version / magic / checksum.
- Platform-specific GPU cooked payloads may be stored separately.

Object graph loading:

```text
Phase 1: Allocate / Create Identity
Phase 2: Deserialize Data
Phase 3: Resolve References
Phase 4: Validate / Finalize
```

Therefore, cyclic logical references are supported without depending on deserialize call order.

For unknown fields / missing plugins, the Editor preserves recoverable payloads; Shipping hard-fails on unknown required schemas.

Migration:

```text
Version N
→ N+1
→ ...
→ Current
```

Migration must be deterministic and testable through CI fixtures.

---

### N. Asset Import Framework / Derived Data Cache

Formal Pipeline:

```text
Source Asset
↓
Importer Registry
↓
Import Settings
↓
Intermediate Canonical Data
↓
Asset Compiler / Cooker
↓
Runtime Artifact
↓
DDC / Library
```

Importer interface:

```text
IAssetImporter
├─ Supported Source Types
├─ ImporterVersion
├─ Read Import Settings
├─ Discover Dependencies
├─ Import
└─ Diagnostics
```

DDC key:

```text
SourceContentHash
+
ImporterVersion
+
ImportSettingsHash
+
DependencyHashes
+
TargetProfile
=
DerivedDataKey
```

File watcher: Editor only; debounce/coalesce changes.

Reimport:

```text
New Import
↓
Validate
↓ success
Atomic Publish New Artifact

Failure
→ Keep previous known-good artifact
```

Large / untrusted third-party importers may use an out-of-process Import Worker; a worker crash must not bring down the Editor.

Parallel import uses a Job / worker pool, but publishing the same UUID must be a serialized transaction.

DDC:

```text
Local DDC       ✅ V1
Shared Remote DDC △ Future / Studio Feature
```

---

### O. Mesh / Model Pipeline

Authoring source:

```text
glTF / GLB   → Primary open interchange
FBX          → Editor import only
OBJ          → Basic static mesh support
```

Recommended private importer backends:

```text
glTF → fastgltf / equivalent wrapped backend
FBX  → ufbx / equivalent wrapped backend
```

The public Asset Pipeline does not expose backend types.

Canonical intermediate mesh:

```text
MeshSourceData
├─ Positions
├─ Normals
├─ Tangents
├─ UV Sets
├─ Colors
├─ Indices
├─ Skin Weights
├─ Morph Targets
└─ Submeshes / Material Slots
```

Tangent: MikkTSpace-compatible generation.

Optimization:

```text
Index Reorder
Vertex Fetch Optimization
Vertex Dedup
LOD Simplification
Bounds / Cone data
```

A meshoptimizer-like backend may be used, but remains wrapped in the private cooker layer.

V1 Runtime Mesh:

```text
StaticMeshRuntime
SkinnedMeshRuntime
```

Skin influence:

```text
V1 default max 4 influences / vertex
Optional import preserve 8 → cook profile decides
```

Compression:

- Quantized UVs / weights where quality permits.
- Packed normals/tangents.
- Position quantization only when bounds/error budget permits.

LOD:

```text
Manual LOD
Auto-generated LOD
Hybrid
```

Collision cook:

```text
Primitive
Convex Hull
Compound Convex
Triangle Mesh (static)
VHACD-like decomposition △ optional offline
```

Meshlet: Future / GPU-driven profile-driven; not a V1 runtime requirement.

---

### P. Virtual File System / Async IO

Formally establish:

```text
engine::io
```

VFS logical roots:

```text
engine://
project://
bundle://
cache://
user://
temp://
```

Shipping may strip the `project://` source mount.

Path contract:

- UTF-8 logical path.
- `/` canonical separator.
- Normalize `.` / `..`.
- Logical path comparison rules are defined by the VFS and do not directly depend on host filesystem case behavior.

Mount:

```text
DirectoryMount
BundleMount
MemoryMount
PlatformPackageMount
```

Async IO:

```text
IORequest
├─ Priority
├─ Offset / Size
├─ Destination / Buffer Policy
├─ CancellationToken
└─ Completion Handle
```

The IO Scheduler supports:

- Request merge / coalescing.
- Priority preemption.
- Cancellation.
- Aligned reads.
- Bounded concurrent requests.
- Streaming deadline hints.

Memory Mapping is used only for immutable files with stable lifetimes and must be managed by the Bundle generation / pending-delete contract.

Atomic Write:

```text
Write temp
↓
Flush / Validate
↓
Atomic Replace
```

Used for Save, Settings, and Cache metadata.

---

### Q. Save / Persistent Data Framework

Save does not use Scene Serialization as a runtime save dump.

Formally:

```text
SaveManager
├─ UserProfile
├─ SaveSlot
├─ WorldState
├─ UserSettings
└─ CloudAdapter
```

Save schema:

```text
SaveFileHeader
├─ FormatVersion
├─ GameBuildVersion
├─ SchemaVersion
├─ SlotID
├─ Timestamp Metadata
├─ Payload Hash
└─ Chunk Directory
```

The payload uses typed chunks / stable IDs rather than directly serializing an arbitrary live pointer graph.

Async save:

```text
Gameplay State
↓
Safe Snapshot / Copy
↓
Background Serialize + Compress
↓
Atomic Write
```

The IO thread does not directly read live component storage that is currently changing.

Reliability:

```text
slot.tmp
↓
checksum / parse verify
↓
slot.save
+
slot.backup
```

Corruption: if the main file is damaged → backup fallback → explicit error; no silent reset.

Compression: zstd optional by default.

Encryption: optional project/platform adapter; treated only as data protection and not claimed to prevent capable local attackers. Integrity may use an authenticated hash / platform key service.

Persistent World State:

```text
Scene Asset Defaults
+
Persistent Object State by Stable Identity
↓
Runtime Scene
```

Chests, Bosses, bridges, and NPC progression do not revert to defaults because a Streaming Cell is unloaded/reloaded.

Cloud Save: `ICloudSaveBackend` plugin; V1 local first, cloud backend optional.

Server-authoritative game: character assets/economy and similar data are determined by the Server contract; Local Save is not treated as authoritative.

---

### R. Localization Framework

Formal data:

```text
LocalizationCatalog
├─ Stable LocalizationKeyID
├─ Source Key
└─ Locale Entries
```

Display text is never used as persistent identity.

Locale:

```text
zh-TW
zh-CN
ja-JP
en-US
...
```

Example fallback chain:

```text
zh-HK
→ zh-TW
→ zh
→ Project Default
```

V1 Locale backend: ICU4C behind the Engine abstraction; Project Cook carries only the data required by declared locales to control binary/data size.

Uses:

- Plural categories.
- Number formatting.
- Date/time formatting.
- Bidi.
- Grapheme / line break iterator.

Message formatting:

```text
Key
+
Named Parameters
+
Plural / Select Rules
→ Localized String
```

Position-only formats such as `%s %d` are discouraged as long-term cross-language contracts.

Runtime language switch:

```text
LocalizationGeneration++
```

Even if a UI component is currently disabled, it must check the generation when re-enabled / laid out; correct updating must not depend on having received the event at that moment.

Font fallback:

```text
Locale Font Profile
→ Ordered Font Families
→ Dynamic Glyph Atlas
```

Localization bundles may be separately downloaded / distributed as DLC by locale.

Missing translation: Development displays an explicit marker + logs once; Shipping uses the fallback chain and does not return a silent empty string.

---

### S. Text Editing / IME Framework

`InputField` uses an independent `TextEditModel`; editing state is not scattered across Widgets.

```text
TextEditModel
├─ UTF-8 Text Buffer
├─ Selection
├─ Caret
├─ Composition Range
├─ Undo Stack
├─ Validation
└─ Input Constraints
```

Formally distinguish:

```text
Physical Key Event
≠ Text Input Event
≠ IME Composition
```

IME platform adapter:

```text
CompositionStart
CompositionUpdate
CompositionEnd
TextCommit
```

Caret / selection movement uses Unicode grapheme clusters as the user-visible unit, not UTF-8 bytes or UTF-16 code units as the UX cursor step.

Clipboard: Platform service.

Supports:

- Single/multiline.
- Password display masking.
- Max grapheme / byte policy.
- Validation callback / regex-like project validator.
- Mobile keyboard type / return key type.
- Candidate window rect / IME caret position.
- Copy/cut/paste/select-all.
- Local text undo/redo.

The password model does not treat the masked string as the actual data source.

---

### T. Platform Layer

The Platform Layer maintains a thin abstraction + native service adapter and does not create a huge God interface.

```text
engine::platform
├─ Application
├─ Window
├─ Display
├─ Clipboard
├─ Dialog
├─ URL / DeepLink
├─ Permission
├─ DeviceInfo
├─ Power / Thermal
├─ SafeArea / Orientation
├─ Haptics
├─ Notification
└─ NativeHandle Bridge
```

V1 is implemented using native backends:

```text
Windows → Win32 / platform native APIs
macOS   → Cocoa / Objective-C++ bridge
iOS     → UIKit / Objective-C++ bridge
Android → Android native / Java-JNI bridge where required
```

Native types such as JNI / Objective-C objects / HWND are not exposed to the Gameplay ABI.

Application lifecycle:

```text
Foreground
Background
Suspend
Resume
LowMemory
ThermalChanged
OrientationChanged
```

LowMemory / Thermal enter `PerformancePolicyManager` / `MemoryBudgetManager`, rather than being queried independently by each subsystem.

Editor-only: Native File Picker / Message Dialog may directly use platform services; Shipping depends on project permission/feature stripping.

---

### U. Logging / Console / Diagnostics

Formally:

```text
engine::log
```

Log Record:

```text
Timestamp
Severity
CategoryID
ThreadID
JobID optional
WorldID optional
Message
Structured Fields optional
```

Severity:

```text
Trace
Debug
Info
Warning
Error
Fatal
```

Sinks:

```text
Debugger / Stdout
Rotating File
Editor Console
Crash Ring Buffer
Remote Dev Sink △
```

Multi-threaded producers do not write directly to files with mutex-heavy operations; use a thread-local / MPSC queue → logging worker.

Rate limit:

```text
LogOnce
LogEveryN
RateLimit(key, interval)
```

Avoid producing hundreds of thousands of logs per frame in an error loop.

Shipping: Compile-time + runtime category filtering; retain Warning/Error/Fatal and explicitly permitted categories.

Fatal logs must synchronously flush the crash ring / critical sink before handing off to Crash Reporting.

Zig uses a Stable C ABI logging batch / function; category IDs are managed by the engine registry.

---

### V. General Memory Allocator Framework

The existing Frame Arena / Module Allocator contracts are retained, with persistent memory completed.

V1 Default Internal Persistent Allocator:

```text
mimalloc
```

However, the public engine contract is:

```text
EngineAllocator / MemoryResource
```

The backend is replaceable.

Memory classes:

```text
Persistent Heap
Pool / Slab
Frame Arena
Job Scratch
Streaming Buffer
GPU Upload Ring
Readback Buffer
Module Boundary Allocator
```

C++ internal containers may use a designated allocator through `std::pmr` / engine memory resources; the Public ABI does not expose PMR/STL types.

Allocation Tag:

```text
Subsystem
Asset Type
World
Lifetime Class
Callsite Hash (Debug)
```

Debug build:

- Leak tracking.
- Double-free detection where practical.
- Guard / poison modes.
- Arena generation validation.
- High watermark.

Global forced overrides of all third-party `new/delete` are not recommended; third parties may be incorporated into statistics through adapters or allocator hooks.

---

### W. Advanced Job / Task Graph Model

The underlying Work-Stealing Job System is retained.

Formally at the upper layer:

```text
TaskGraph
├─ TaskID
├─ Dependencies
├─ Priority
├─ Affinity
├─ CancellationToken
├─ CompletionFence
└─ DebugName / Category
```

Affinity:

```text
AnyWorker
MainThread
RenderThread / RenderSubmission domain
IO domain handled by IO Scheduler
PlatformThread explicit only where required
```

TaskGraph does not replace RenderGraph; GPU hazards/order are still managed by RenderGraph.

The recurring SystemGraph may compile/cache dependency topology, changing only data / chunk jobs each frame to avoid continuously rebuilding large amounts of metadata.

Cancellation is cooperative; cancellation does not mean that a thread may be arbitrarily killed.

Frame task metadata uses the Frame Arena and must not escape after the barrier.

Profiler: task dependencies, queue latency, execution, stealing, waiting, and critical path.

---

### X. Time / Timer / Scheduler

Formally distinguish:

```text
RealClock        → OS monotonic time
GameClock        → World time scale / pause
UnscaledClock    → UI / transition convenience
FixedTickClock   → integer simulation tick
```

Each World has its own `WorldTimeState`.

```text
TimeScale
PausePolicy
FixedDelta
Accumulator
TickIndex
MaxCatchupSteps
```

Timer framework:

```text
TimerScheduler
├─ OneShot
├─ Repeating
├─ GameTime
├─ UnscaledTime
└─ FixedTickTimer
```

For large numbers of timers, V1 uses a bucket/timing-wheel-like scheduler rather than scanning the entire table every frame.

Deterministic timer:

```text
ExpiryTick = currentTick + N
```

Do not use floating wall-clock comparison.

Timer callbacks may not retain Zig module raw function pointers across reloads; Gameplay timers may dispatch stable EventID / GameplayCommandID.

Background / resume has explicit policies for each clock; Fixed accumulator clamping prevents a spiral of death.

---

### Y. Audio Authoring / Event Editor

The Audio Runtime already has Event / Voice / Bus / Residency; the Editor completes Authoring.

```text
AudioEvent Asset
↓
Audio Event Graph Editor
↓
Cooked Event Program
↓
Audio Runtime
```

V1 native event nodes:

```text
PlayClip
Random
Sequence
Switch
BlendByParameter
Loop
Delay
SetParameter / BusSend basic
```

V1 does not pursue a complete FMOD Studio-level graph.

Editor:

- Waveform.
- Loop / trim marker.
- Audition / preview.
- Random/sequence preview seed.
- Bus graph.
- Snapshot editor.
- Loudness analysis / normalization metadata.
- Streaming/decompress residency preview.

FMOD plugin: if FMOD Events are used, the Engine AudioEvent API maps to the FMOD backend; FMOD authoring graphs are not required to be converted into native graphs.

---

### Z. Animation Editor / Authoring Tools

The Runtime Animation Framework is established; the Editor formally adds:

```text
Skeleton Viewer
Animation Clip Preview
Event Track Editor
Root Motion Visualization
Retarget Profile Editor
Blend Preview
Animation Graph / State Machine Editor
BAT Bake Preview / Debug
```

Animation Graph authoring:

```text
Graph Asset
↓
Validate
↓
Compile
↓
Compact Runtime Program
```

The Runtime does not traverse Editor node objects.

Retarget profile: Skeleton bone semantic mapping + scale/translation rules; profile assets can be reused.

The BAT tool displays: sample rate, matrix format, atlas/chunk usage, estimated memory, and root/event CPU authority.

V1 considers Animation Graph / State Machine committed (consistent with the Scope Matrix); Advanced IK graph / control-rig-like authoring may be Future.

---

### AA. Physics Editor / Debug Tools

The Editor formally provides:

```text
Collider Authoring
Compound Collider
Physics Material Editor
Collision Layer Matrix
Character Controller Debug
Contact / Manifold Debug
Query Debug
Constraint Debug
GPU Physics Debug
Physics Profiler
```

The Physics Layer Matrix is similar to the UI Input Matrix, but Physics collision pairs are symmetric rules and may use a triangular editor representation.

Project settings:

```text
ObjectLayer
BroadPhaseLayer Mapping
Collision Pair Matrix
Query Mask Defaults
```

After cooking, these become bitmask/filter tables; Jolt native filter objects are not exposed to Gameplay.

Character debug displays the capsule, ground probe, slope, step test, ground normal, moving platform velocity, and requested/actual motion.

GPU Physics debug must indicate:

```text
CPU Authoritative
GPU Visual
GPU Deferred
```

This prevents developers from mistakenly treating visual results as gameplay truth.

---
### AB. Project / Package / Settings System

Formal Project Manifest:

```text
<ProjectName>.engineproj
```

Uses deterministic JSON, containing:

```text
ProjectID
EngineVersionRange
DefaultScene
EnabledPlugins
FeatureFlags
TargetPlatforms
BuildProfiles
DefaultQualityProfile
DeclaredLocales
ProjectSettings Assets
```

Also includes a lock file:

```text
Project.lock
```

Stores the plugin/package exact resolved version / source hash to ensure CI reproducibility.

Settings layering:

```text
Engine Defaults
↓
Project Settings
↓
Platform Override
↓
Quality Profile
↓
Build Profile / Command Line
```

Editor User Preference is not included in Project Runtime Settings:

```text
User Preference
→ local per-user storage
```

Project Settings pages:

- Input Settings / Input Action Assets.
- UI Input Matrix.
- Physics Layer Matrix.
- Graphics / Quality.
- Audio.
- Localization.
- World Partition defaults.
- Memory budgets.
- Feature / Plugin list.

Settings use schema + reflection metadata to automatically generate a basic Inspector; complex pages may use a custom editor.

Feature flags must be connected to Feature Stripping / CI matrix, rather than being simple runtime bools.

---

### AC. Networking Future Contract

V1 still maintains:

```text
High-level Networking Framework
→ Explicitly out of V1
```

However, v4.0 defines a non-breaking future boundary.

```text
engine::net          ← optional future module
```

Layers:

```text
Transport
↓
Connection / Channel
↓
Replication / Snapshot
↓
Prediction / Reconciliation
↓
Game-defined Network Model
```

Core future identity:

```text
NetworkEntityID
≠ EntityID
≠ SceneObjectRef
```

Input already uses tick-based `InputCommand`, which can directly serve as a prediction/replay foundation.

World / Scene Streaming does not equal Network Authority; Region server / instance world can be mapped later and is not hardcoded into Scene Core.

Dedicated Server: Future headless build that does not create RenderWorld / Audio device / Runtime UI.

Transport is not initially bound to a specific provider; UDP/QUIC/WebSocket/third-party implementations can be plugins.

Lobby / Matchmaking belongs to the service adapter and is not placed in the replication core.

---

### AD. Runtime Debug / Developer Console

The Development build officially provides:

```text
DeveloperConsole
CVar Registry
Command Registry
Runtime Inspector
DebugDraw
Performance Overlay
Remote Dev Console
RenderGraph Inspector
```

CVar:

```text
Bool / Int / Float / String / Enum
Flags: ReadOnly / Cheat / Archive / RestartRequired / ShippingAllowed
```

The CVar ID is a stable registry ID; hot paths may cache the handle.

Command: typed arguments + help metadata; the core console does not use arbitrary eval scripts.

DebugDraw:

```text
Line / Ray / Box / Sphere / Capsule / Frustum / Text
```

thread-safe enqueue → frame debug buffer → renderer.

Runtime Inspector uses Reflection, but is Development only by default; the Shipping remote console must be completely stripped or be an explicitly authenticated project feature.

The remote console does not listen on the public network by default.

RenderGraph Inspector can inspect passes, resource lifetime, barriers, aliases, and timing, but does not allow modification of backend native objects.

---

### AE. Formal Cross-System Relationships

The final relationships among the above Frameworks:

```text
Platform
├─ App / Window / Device Services
├─ Raw Input
└─ File / OS Services
        │
        ▼
Core
├─ Memory
├─ Job / Task Graph
├─ Time
├─ Logging
├─ Event
├─ Reflection
├─ Serialization
└─ VFS / Async IO
        │
        ▼
World
├─ Scene / Entity / Transform
├─ System Scheduler
├─ PhysicsWorld
├─ NavigationWorld
├─ AI Runtime
├─ Audio Context
└─ Render Extraction
        │
        ├───────────────┐
        ▼               ▼
Asset / Data         Runtime UI
Pipeline              / Input
        │               │
        ▼               ▼
Renderer / RHI      Gameplay / Zig
```

The core principles remain:

```text
High-level Framework
→ Optional Convenience

Low-level Capability
→ Still Available
```

For example:

```text
InputAction optional
AI BehaviorTree optional
Prefab Editor feature not runtime requirement
PostProcess effect quality-strip capable
Networking future module not core dependency
```

---

### AF. v4.0 V1 / Future Decision Summary

Newly officially included in V1:

```text
Transform System / Large-world position foundation
System Scheduler / access-declared DAG
Typed Event Framework
Camera Framework
Lighting / Shadow Framework
PostProcess Volume Framework
Recast/Detour Navigation backend
Behavior Tree + Blackboard + Perception AI foundation
Full Editor service architecture
Nested Prefab + Variant + structural overrides
Transaction-based Undo / Redo
Stable Reflection Type/Property identity
Generic Serialization + cooked binary blob
Importer Registry + Local DDC
Mesh cook / optimization / collision cook
VFS + Async IO
Save / Persistent World State
ICU-backed Localization
TextEdit / IME framework
Expanded Platform Services
Structured Async Logging
Persistent Allocator framework
TaskGraph high-level model
Timer Scheduler
Audio Authoring V1
Animation Authoring / Graph Editor V1
Physics Authoring / Collision Matrix
Project Manifest / Settings Layering
Runtime Developer Console
```

Future / Optional:

```text
Clang Reflection Generator
Shared Remote DDC
GOAP
Advanced Crowd Simulation
Area Light advanced runtime
Advanced DOF / Motion Blur profile
Adaptive Quadtree Cell Generation
Octree World Partition
World Origin Rebasing if needed
High-level Networking / Replication
Offscreen WebView
Meshlet pipeline
Cloud Save backend
Authenticated Remote Dev Console
```

---


---

# V1 Implementation Milestones

> This section describes the “actual implementation order” and coexists with the Phase Roadmap below.
> Phases describe feature groups and version Gates; Milestones describe which dependency chain the engineering team should establish first.
> Principle: **Lay the foundation first, use Vertical Slices, make every step executable and verifiable, and avoid having ten systems simultaneously remain half-finished.**

## V1-M0 — Repository / Build / CI Skeleton

**Purpose:** First establish the Build Contract on which all subsequent engineering must depend.

Implementation:

```text
CMake
Build Configuration
Platform Toolchain
Module Graph
Feature Flag / Feature Stripping
Plugin Manifest skeleton
Debug / Development / Shipping
Modular Dev / Monolithic Shipping
clang-format / clang-tidy
Warnings-as-Errors
CI
```

Platforms:

```text
Windows
macOS
Android
iOS
```

This phase only requires “minimum compilability, startup, and executable tests”; no game features are implemented.

**Gate:**

```text
✓ Minimum Host can be built on all four platforms
✓ Both Modular / Monolithic can be linked
✓ Module dependency cycles can be blocked by CI
✓ Disabled Features can be excluded from the build graph
✓ Build ID / Engine Version / ABI Version can be inspected
```

---

## V1-M1 — Core Runtime Foundation

**Purpose:** Make the Engine itself “come alive” first.

Implementation:

```text
Engine Init / Shutdown
Memory / EngineAllocator
mimalloc backend
Memory Tag / Statistics
Job System
TaskGraph foundation
Logging
Crash Ring
Time / Fixed Tick foundation
Timer
Typed Event
VFS
Async IO foundation
Platform Core
Handle / Generation ID
```

First execution result:

```text
Engine.exe
↓
Engine Init
↓
Workers Start
↓
VFS Mount
↓
Main Loop
↓
Clean Shutdown
```

**Gate:**

```text
✓ Repeated Init / Shutdown produces no leaks
✓ Job dependency / fence / cancel smoke test
✓ Frame Arena does not escape across frames
✓ Basic VFS mount/read/cancel is available
✓ Structured logs can be collected correctly across threads
```

---

## V1-M2 — Slang / RHI Cross-Platform PoC

**Purpose:** Validate the most dangerous cross-platform assumptions of the entire Renderer as early as possible.

Implementation:

```text
One Slang Shader
├─ DXIL → DX12
├─ SPIR-V → Vulkan
└─ MSL → Metal
```

Also implement:

```text
Canonical Shader Reflection
Pipeline Layout Metadata
Zig Stable C ABI PoC
Zig Android / iOS cross-compile smoke test
```

**Gate:**

```text
✓ The same Slang Triangle runs on DX12 / Vulkan / Metal
✓ Canonical Reflection layout is consistent
✓ Backend-native bindings are not exposed to upper layers
✓ The minimum Zig GameModule can build on all four platforms
```

If this Gate fails, the RHI / Shader / Zig boundaries must be adjusted here; the risk cannot be deferred to later stages.

---

## V1-M3 — Renderer Mainline

**Purpose:** Complete a genuinely extensible Renderer mainline.

Order:

```text
DX12 Reference RHI
↓
Vulkan parity
↓
Metal parity
↓
Resource / Descriptor
↓
PSO
↓
Command / Fence
↓
RenderGraph
↓
Transient Resource
↓
Barrier / Dependency
↓
Frame Pipeline
```

**Gate:**

```text
✓ Offscreen Pass → Main Pass → Present
✓ RenderGraph automatically manages Barriers
✓ DX12 / Vulkan / Metal execute the same basic workload
✓ PSO cache / async creation foundation is available
✓ GPU resource lifetime can be checked by the validator
```

---

## V1-M4 — First Engine Vertical Slice

**Purpose:** Connect “Data → Scene → Render” for the first time.

Minimum chain:

```text
World
↓
Scene
↓
Entity / Node / Component
↓
Transform
↓
Camera
↓
Light
↓
MeshRenderer
↓
Material
↓
Slang
↓
RenderGraph
↓
Screen
```

Establish concurrently:

```text
EditorWorld / PlayWorld
Scene Lifecycle
Additive Scene
Persistent Scene
WorldCommandBuffer
System Scheduler
Forward+ foundation
Shadow foundation
PostProcess foundation
Large-world coordinate foundation
```

**Gate:**

```text
✓ A model can be loaded and displayed
✓ Camera / Light / MeshRenderer function
✓ Results are consistent after Scene Save / Load
✓ Two Additive Scenes can be Active simultaneously
✓ LoadedInactive → Active → Safe Unload operates correctly
```

---

## V1-M5 — Asset / Serialization / Cooker / Bundle

**Purpose:** Upgrade “displaying a model” into a formal content pipeline.

Implementation:

```text
Source
↓
Importer
↓
Canonical Intermediate
↓
Cook
↓
Runtime Binary
↓
Asset Registry
↓
Bundle
↓
Runtime Load
```

Priority:

```text
glTF
↓
Texture
↓
Material
↓
Scene
↓
Prefab
↓
FBX
↓
Audio / Font / Spine importer as needed
```

Also complete:

```text
UUID
Dependency DAG
Generic Serialization
Runtime Blob
DDC
Bundle Manifest
Generation Pinning
Hash
Rollback
Streaming Residency
DataTable foundation
```

**Gate:**

```text
✓ Runtime does not depend on Source Assets
✓ Asset DAG cycles → build fail
✓ Import failures retain the previous good artifact
✓ Bundle update / verify / rollback operate correctly
✓ The active generation is not forcibly disrupted by a new generation
```

---

## V1-M6 — Reflection / Editor / Prefab / Plugin SDK

**Purpose:** Enable the engine to begin producing content “for real.”

Implementation:

```text
Reflection Metadata
Hierarchy
Scene View
Game View
Inspector
Property Drawer
Asset Browser
Gizmo
Undo Transaction
Prefab
Nested Prefab
Variant
Project Settings
Profiler foundation
```

Also complete the foundation for third-party extensions:

```text
PluginHost
Service Registry
Extension Registry
Stable C Plugin ABI
C++ Plugin SDK
Bridge Plugin
Custom Asset Type
Editor Extension
```

**Gate:**

```text
✓ A third-party Plugin can be written without modifying Engine source
✓ Plugins do not need to include private Engine headers
✓ The Editor can create / modify / Undo a Scene
✓ Basic Prefab override / rebase is available
✓ ABI mismatches are rejected before Load
```

---

## V1-M7 — Input / Runtime UI / Text / Localization

**Purpose:** Give the project a complete player interaction interface for the first time.

Implementation:

```text
Raw Input
InputUser
Optional Action Map
Keyboard / Mouse / Gamepad / Touch
Stable PointerID
Virtual Control
UI Routing Matrix
```

Then:

```text
UIDocument
UIElement
RectTransform
Layout
Image / Label / Button
ScrollView
VirtualizedList
Nine-Slice
FreeType
HarfBuzz
IME
Localization
ViewModel / Binding
```

**Gate:**

```text
✓ Multiple input devices can work simultaneously
✓ Multi-touch has no duplicate delivery
✓ VirtualizedList does not create an equal number of Elements to the data volume
✓ Disabled UI can update according to LocalizationGeneration when restored
✓ UI Logical Resolution is separated from 3D Dynamic Resolution
```

---

## V1-M8 — Physics → Character → Navigation → AI

**Purpose:** Establish Gameplay Simulation according to the actual dependency relationships.

Formal order:

```text
Jolt Physics
↓
Query
↓
CharacterController
↓
CharacterMotor
↓
CharacterIntent
↓
Recast / Detour
↓
Navigation DesiredVelocity
↓
Blackboard / BT / Perception
```

Do not reverse the order by implementing AI first and adding character movement later.

**Gate:**

```text
✓ Character Requested Motion / Actual Motion are separated
✓ Ground / Step / Slope / Snap function correctly
✓ Root / external velocity can be resolved through the Motor
✓ Nav does not directly write to Transform
✓ AI does not directly control the Physics Body
```

---

## V1-M9 — Animation / Audio / VFX / Video

**Purpose:** Complete Presentation / Character Runtime.

Animation:

```text
Skeleton
Clip
Graph
StateMachine
Blend
Root Motion
GPU Vertex Skinning
BAT foundation
```

Audio:

```text
Audio.Core
MiniAudio
AudioEvent
Bus
Streaming
Residency
```

VFX:

```text
CPU Particle SoA
GPU Particle foundation
Sprite / Mesh / Trail
```

Video:

```text
Media.Core
VideoPlayer
Hardware Decode Backend
VideoTexture
UI.VideoElement
Subtitle
A/V Sync
Video Audio → Audio.Core
```

**Gate:**

```text
✓ Animation Graph can drive a character
✓ Active Audio Voice is not unloaded incorrectly
✓ Video does not block the Gameplay main thread
✓ VideoTexture can be used by UI / Material
✓ Dedicated / Headless can completely strip Media / Audio / VFX
```

---

## V1-M10 — Large Scene / Streaming / Terrain / Vegetation / HLOD

**Purpose:** Expand to a large world formally only after all foundational systems have matured.

Implementation:

```text
StreamingCell
Stable Fixed Grid
Loose Quadtree
Room / Portal
Streaming Source
Demand
Priority
Hysteresis
Prefetch
Offline HLOD
Terrain
Vegetation
LOD
Culling
```

**Gate:**

```text
✓ Cell / Bundle identities are separated
✓ Far HLOD can exist while the Full Cell is unloaded
✓ Terrain / Vegetation do not create large numbers of Scene Entities
✓ Room / Portal can drive prefetch
✓ Streaming RAM / VRAM budgets are observable
✓ Characters do not lose Collision across Streaming Cell boundaries（Occupied Cell Pinned）
```

---

## V1-M11 — Mobile / Platform / WebView

**Purpose:** Complete product-level platform integration rather than performing the first Port only at the end.

Implementation:

```text
App Lifecycle
Permissions
Safe Area
Orientation
Thermal
Haptics
Clipboard
Deep Link
IME
Native Share
Platform Login extension
Android GPU Workaround
WebView2 / Android WebView / WKWebView
```

**Gate:**

```text
✓ Complete runs on physical iOS / Android devices
✓ Background / Resume operate correctly
✓ WebView ownership / touch routing operate correctly
✓ WebView is not included in the RenderGraph UI pass
✓ Thermal / memory pressure can trigger policy
```

---

## V1-M12 — Shipping / Packaging / Hardening

**Purpose:** Turn “can make a game” into “can deliver one.”

Implementation:

```text
Minimal Profile
Full Profile
Monolithic Shipping
LTO / WPO
Shader Strip
Asset Strip
Plugin Strip
SDK Strip
Bundle Update
Rollback
Crash Report
Soak Test
Device Matrix
```

**Gate:**

```text
✓ Disabled Plugins are not included in the package
✓ Disabled Shader Families are not included in cooked shaders
✓ Dedicated / Headless has no presentation baggage
✓ Shipping Profiles can start on all four platforms
✓ Update / rollback / restart remain usable
✓ Long-run Scene / Streaming / Audio / Asset generation produces no leaks
```

---

## V1 Implementation Dependency Graph

```text
M0 Build / CI
 │
 ▼
M1 Core Runtime
 │
 ├──────────────┐
 ▼              ▼
M2 RHI PoC   ABI / Plugin Skeleton
 │
 ▼
M3 Renderer
 │
 ▼
M4 Scene Vertical Slice
 │
 ▼
M5 Asset / Cook / Bundle
 │
 ▼
M6 Editor / Reflection / Plugin SDK
 │
 ├─────────────┬─────────────┐
 ▼             ▼             ▼
M7 UI/Input   M8 Gameplay   M9 Media/Animation
               │             │
               └──────┬──────┘
                      ▼
               M10 Large World
                      │
                      ▼
                M11 Platform
                      │
                      ▼
                M12 Shipping
```

## V1 Renderer Scope Revision

The following is formally adopted during implementation:

```text
Required for V1:
CPU Frustum Culling
Static BVH / Spatial Grid
Instancing
LOD
Occlusion foundation
RenderGraph
Streaming / HLOD
```

```text
V1.x Optional / PoC:
Indirect Draw
Hi-Z prototype
GPU Culling prototype
```

```text
V2 Production:
GPUScene
Compute Culling
GPU LOD
Instance Compaction
Indirect Generation
Async Compute
```

This prevents V1 from being slowed down by the V2 GPU-Driven goal.


## Engine Milestone Roadmap

This Roadmap adopts the strategy of “foundations first, Vertical Slices, incremental expansion, and continuous validation.”

The basic infrastructure capability of compile-time Feature Stripping is designed in Phase 0 and, as features are developed in each phase, the corresponding CI validation gates (CI Gates) are progressively completed.

### Phase 0: Foundation / Build / Memory / Shader & RHI PoC

**Core Objectives:**

Establish the infrastructure and memory isolation mechanisms, and validate the consistency of cross-platform Shader Canonical Reflection.

**Key Tasks:**

- **Build System Setup**
  - Configure CMake to support both Modular (development) and Monolithic (release) compilation modes.
  - Establish the Compile-time Feature Stripping tag architecture.
  - Establish the basic Module Graph / Dependency Rule.
  - Enable PCH, clang-format, clang-tidy, Warnings-as-Errors, and basic CI from this phase onward.

- **Job System Foundation**
  - Work-Stealing Scheduler foundation.
  - Task dependency / priority / frame barrier contract.
  - Frame End Barrier and scratch lifetime assertion.
  - High-level TaskGraph metadata / CancellationToken / CompletionFence foundation.
  - System Scheduler access declaration prototype.

- **Persistent Memory / Diagnostics Foundation**
  - EngineAllocator / MemoryResource abstraction; mimalloc as the V1 internal default backend.
  - Allocation Tag / subsystem statistics.
  - Structured async Logging / rotating file / crash ring buffer.
  - VFS mount table / Async IO Scheduler foundation.
  - Real / Game / Fixed clock + TimerScheduler foundation.

- **Memory Isolation**
  - Implement `FrameAllocatorSet`.
  - Main / Render / Worker each use a dedicated Arena.
  - Add cross-Frame object escape checks.
  - Add Thread Ownership / Reset Timing Assertion.

- **Cross-Backend Shader Contract (PoC Gate)**
  - Use the same Slang Source to establish the following compilation chain:

```text
Slang Source
├─ DX12
│  └─ DXIL
├─ Vulkan
│  └─ SPIR-V
└─ Metal
   ├─ Direct
   │  └─ MSL
   └─ Fallback
      └─ SPIR-V → SPIRV-Cross → MSL
```

- **Canonical Reflection Gate**
  The Canonical Reflection Contract output by all four paths must be consistent, validating at minimum:

```text
Resource ID
Binding
Type
Stage
Constant Layout
Texture / Sampler
Argument Buffer Mapping
Material Parameter Layout
```

- Frame Memory type-safe wrapper (`FramePtr / FrameSpan`) and AST persistent-storage prohibition.

- Worker Arena Page / Chunk Growth Policy and budget/trim telemetry。
- AI Change Scope / Risk Class baseline and ADR policy.

- Unified Architecture Dependency Graph Tool / module rule manifest baseline.

- Shader Reflection → generated C++ layout prototype.

- Shader Variant stage accounting (Theoretical / Pruned / Used / Cooked / Budget).

FRAGMENT END- **Gameplay Language Boundary Foundation**
  - Define a Language-Neutral Stable C ABI.
  - Opaque Handle / POD / versioned function table / allocator ownership contract.
  - Zig binding header / codegen PoC.
  - Prohibit C++ private classes / STL owning types from crossing the Gameplay ABI.
  - Establish a minimal Zig `GameModule_Init / Update / Shutdown` smoke test.
  - **Zig Mobile Toolchain PoC**: Verify that Zig cross-compilation to `aarch64-android` / `aarch64-ios` can successfully produce a linkable minimal static/dynamic library, and that it can be loaded and called by a minimal Host App on the corresponding platform. This verifies the feasibility of the Zig toolchain itself; it does not include complete Engine Mobile integration (see Phase 6).

- **Shading Model Framework PoC**
  - PBR / StylizedPBR / Anime / Unlit enum + metadata.
  - Shared lighting input contract.
  - Minimal Anime Ramp / Face SDF prototype.
  - Minimal StylizedPBR scene material prototype.
  - Variant / stripping key includes ShadingModel。

**Gate:**

- Minimal Modular / Monolithic Build can pass.
- Frame Allocator Thread Ownership test passes.
- Frame-memory escape is not allowed.
- DX12 / Vulkan / Metal PoC can use the same Slang Shader to successfully draw a Triangle.
- The Canonical Reflection of the Metal Direct and Fallback paths must be identical.

- Windows / macOS / Android / iOS can all successfully compile and execute the minimal Zig GameModule / Toolchain smoke test, and ABI mismatch can be rejected. If Android / iOS cannot pass at this stage, the selection of Zig as the Primary Gameplay Language must be reevaluated; it must not remain undiscovered until a subsequent Phase.

---

### Phase 1: DX12 Reference RHI + Render Graph + Frame Pipeline

**Core Objectives:**

Complete the first full reference RHI (DX12), and establish the Render Graph and dual-thread rendering backbone.

**Key Tasks:**

- **Reference RHI (DX12)**
  - Prioritize completion of Direct3D12.
  - Serve as the RHI Reference Implementation.

- **Secondary RHI (Vulkan)**
  - Used to reverse-validate that the RHI abstraction has not become DX12-biased.
  - Verify whether Resource State / Barrier / Descriptor abstraction is sufficiently generic.

- **Metal RHI (Catch-up)**
  - Complete the basic interfaces based on the Phase 0 Shader / RHI PoC.
  - Verify Argument Buffer and Resource Table mapping.

- **PSO Cache Foundation**
  - PSO Key / async create / compatible cache identity.
  - Runtime synchronous PSO creation telemetry.

- **Render Graph & Pipeline**
  - Automatic lifecycle management for Transient Resources.
  - Dynamic Barrier insertion.
  - Pass Dependency Topological Sort.
  - Main / Render dual-thread synchronization.
  - Render Command transfer and Frame Fence.

- Public RHI adopts `BeginRendering / EndRendering` modern semantics; Backend capability fallback exists only internally.

- Render Graph Transient Aliasing Hazard Validator.

- Offline `PipelineLayoutMetadata` generation / direct runtime binding path.
- Transition Planner / Barrier Merge / redundant-barrier metric.

- Runtime PSO Residency Budget / LRU / Fence-safe Eviction foundation.

- PSO Fence-safe Eviction supports unified timeline and independent multi-queue timeline.

**Gate:**

- Offscreen Pass + Main Pass operate normally.
- Render Graph automatically handles Barrier / Resource Transition.
- DX12 is a complete Reference Backend.
- Vulkan can run the same basic Render Graph workload.
- Metal can run the same basic workload.

---

### Phase 2: First Vertical Slice

**Scope:**

```text
Scene
+
Component
+
Mesh
+
Material
+
Camera
+
Light
```

**Core Objectives:**

Establish the first truly usable complete chain in the engine:

```text
Data
↓
Scene
↓
Render
```

**Key Tasks:**

- **Core Scene / World Pipeline**
  - World / Scene ownership model.
  - EditorWorld / PlayWorld separation.
  - Scene Graph.
  - Node / Component.
  - Transform hierarchy.
  - Multiple / Additive Scene foundation.
  - Persistent Scene.
  - Scene state machine: Unloaded → Loading → LoadedInactive → Active → Unloading.
  - Async load + atomic activation foundation.
  - WorldCommandBuffer / structural barrier foundation.
  - Camera.
  - Light.
  - Transform batch propagation / Large-world coordinate foundation.
  - Access-declared System Scheduler / WorldCommandBuffer integration.
  - Typed Engine Event Framework.
  - CameraView extraction / multi-camera / viewport / screen-ray.
  - Lighting registry / Forward+ / Shadow Manager foundation.
  - PostProcess Volume / Profile foundation.

- **Mesh & Material Pipeline**
  - MeshRenderer
  - Material
  - Slang Shader
  - RHI Resource Binding
  - Basic PBR / Lit

- **Asset Import Slice**

```text
FBX / glTF Source
↓
Importer
↓
Runtime Mesh
↓
Asset Database
↓
Scene
↓
Renderer
```

- General Scene Component Pool adopts Sparse Set + Dense SoA default storage.

**Gate:**

- Editor / Runtime can load models.
- Camera / Light / MeshRenderer can be created in a Scene.
- Material can bind a Slang Shader.
- Runtime can display the same model after Build.
- Asset UUID / dependency can be fully resolved.
- Scene loading can collect required PSOs and execute warmup.
- EditorWorld and PlayWorld states are isolated.
- At least two Additive Scenes can be Active simultaneously and share the same Physics / Render World foundation.
- Scene can complete the full lifecycle of LoadedInactive → atomic Active → safe Unload.

---

### Phase 3: Asset Pipeline + Bundle + Streaming Foundation + Hot Update

**Core Objectives:**

Build the asset database, Bundle packaging, remote Manifest, and hot update mechanisms.

#### Phase 3A: Asset Core & DAG

- Asset Database
- UUID
- Runtime Binary Format
- Streaming Residency State Machine
- RAM / VRAM Budget Foundation
- World Partition foundation.
- Stable Fixed Grid Streaming Cells.
- Loose Quadtree spatial index.
- Room / Portal graph foundation.
- Outdoor ↔ Indoor Streaming Gateway.
- Multiple Streaming Sources / StreamingDemand.
- Streaming Priority / Hysteresis / Prefetch.
- Offline HLOD Builder foundation.
- HLOD assets enter Asset / Bundle pipeline.
- VFS / Async IO production integration.
- Importer Registry / deterministic Local DDC.
- Generic Serialization cook → relocatable runtime blob.
- Mesh optimization / collision / LOD cooker.
- Project Manifest / Settings / Build Profile integration.
- Bundle Builder
- Bundle Dependency Graph
- Bundle Dependency DAG Check
- Cycle Detection → Build Fail

#### Phase 3B: Remote & Hot Update

- Remote Manifest
- Versioned Cache
- Bundle Download
- Hash Verification
- Atomic Activation
- Active Manifest
- Pending Update
- Rollback
- `.prev` / versioned directory strategy
- mmap / open handle safe-unload rule
- Pre-init deferred activation

#### Phase 3C: Hot Update Verification

Use a generic Binary Asset to verify the complete process:

```text
Build
↓
Publish
↓
Manifest Compare
↓
Download
↓
Verify
↓
Activate
↓
Reload
↓
Rollback
```

This phase must not depend on third-party Middleware such as FMOD / Spine to complete Bundle verification.

- Bundle Versioned Storage + Generation Pinning / Load Context isolation.
- World Partition Build produces deterministic Cell / HLOD build output under the same Scene / Partition / Builder Version.
- Streaming Cell and Bundle identity are separated.
- Streaming Source can trigger Prefetch → BuiltInactive → Active and has hysteresis.
- Room / Portal Graph can drive Indoor prefetch.
- HLOD can maintain far visual representation when the Full Cell is non-resident.

- Bundle Pending Delete Queue / async mapping reference drain.

- Multi-platform Texture Cook Profile（BC*/ASTC by semantic/profile）。

- Streaming Scheduler priority preemption / per-platform IO concurrency budget.

#### Phase 3D: JSON / DataTable Foundation

- Engine JSON Framework: yyjson private backend + JsonDocument / JsonWriter abstraction.
- JSON Parse / Generate / Deterministic Write.
- DataTable JSON Runtime Asset.
- DataTable Schema / Strict Validation.
- Primary Key: UInt32 / UInt64 / String.
- Secondary Index / Strong ID / String Key Runtime Index.
- Typed Table Build.
- DataTable Preprocess Pipeline: Normalize / Resolve / Derive / Index / Optimize / Finalize.
- Sorted View / Group Index / Weighted Table / Range Table foundation.
- Custom Processor + Processor DAG / version / deterministic contract.
- Runtime Container: Contiguous Rows + Indices + Views + StringPool / ArrayPool + DerivedData.
- Immutable Table / DataTableRegistry.
- DataTable Generation N → N+1 safe publish / rollback.
- DataTable-specific C++ / C ABI / Zig typed schema codegen foundation.

**Gate:**

- Bundle DAG has no cycles.
- Download / LoadBundle / LoadAsset are completely separated.
- Runtime can safely update Binary Asset.
- DataTable JSON can Parse → Validate → Preprocess → Finalize → Publish typed runtime table.
- UInt32 / UInt64 / String Primary Key lookup is correct; Duplicate Key must Fail.
- Sorted View / Secondary Index does not modify Row Identity.
- DataTable Preprocess determinism / Processor DAG cycle validation passes.
- When DataTable Hot Reload fails, the old Generation is retained without damaging Runtime.
- DataTable Public / Stable ABI does not expose yyjson / STL container.
- Active mmap / file handle does not cause an invalid replacement.
- Update failure can Rollback.
- Downloaded content can be reused after restart.

---

### Phase 4: Editor + Runtime UI + Plugin System

**Core Objectives:**

Establish the toolchain editor, Runtime UI Framework, C++ / Native Plugin extension mechanism, and Zig Gameplay Module development workflow.

**Key Tasks:**

- **Editor**
  - EditorDocument / Selection / EditorObjectAdapter architecture.
  - Inspector / PropertyDrawer registry.
  - Transaction Undo / Redo.
  - Nested Prefab / Variant / Override conflict workflow.
  - Project Settings / Plugin extension points.
  - Runtime Debug / CVar / DebugDraw tooling.
  - Hierarchy
  - Inspector
  - Asset Browser
  - Scene View
  - Game View
  - Console
  - Build Settings
  - Profiler Foundation

- **Runtime UI Framework**
  - Custom Retained Mode Runtime UI; Dear ImGui only for Editor / Debug.
  - `UIDocument` / `UIElementID` / UI Tree / `UIComponent`.
  - `UIElement ≠ SceneNode ≠ EntityID`.
  - Editor Hierarchy Adapter: Scene Entity + Mounted UI Hierarchy.
  - RectTransform / Anchor / Pivot / Layout / Safe Area.
  - Panel / Image / Label / Button / Toggle / Slider / ProgressBar / InputField.
  - ScrollView / VirtualizedListView.
  - Nine-Slice.
  - FreeType + HarfBuzz Text / Localization.
  - Pointer Event / Capture-Target-Bubble / Focus / Gamepad Navigation.
  - ViewModel / Data Binding.
  - JSON UIDocument Asset / Hot Reload.
  - UI Style / UI Animation / Tween.
  - UI Render Extraction / Batch / Clip / RenderGraph.
  - ScreenSpace / WorldAnchored / WorldSpace.
  - WorldSpace UI Ray Mapping / Depth / Occlusion.
  - Multi-resolution / Safe Area Preview.

- **Plugin Foundation**
  - Editor Plugin
  - Runtime Build-time Plugin
  - Static / Dynamic Module
  - Plugin Manifest
  - Dependency Resolution
  - Engine API / ABI Validation

- **Zig Gameplay Tooling**
  - Primary Gameplay Language: Zig.
  - Stable C ABI Binding.
  - GameModule build / load / reload.
  - Build error → Editor Console source mapping.
  - Explicit Tick / Event / Batch API.
  - Reflection metadata bridge.
  - Versioned state serialize / migrate / restore.
  - Reload safe barrier / old callback cleanup.
  - External IDE / native debugger launch or attach workflow.

**Architecture Boundary:**

```text
Plugin
→ Local Installed / Build-time Native Code

Bundle
→ Asset / Data / Remote Content
```

Native Plugin is never downloaded through a Remote Bundle.

- **Reflection Tooling**
  - Canonical Reflection Metadata Schema.
  - V1 Macro/constexpr and V2 Clang Tool share the same consumer contract.

- Dynamic Module Engine Allocator Contract / ABI-safe Public API.

- Frame Memory × Module Boundary Contract: FrameSpan / FrameDataHandle / no async retention.

- Runtime Plugin `Plugin_Init()` ABI/version/hash handshake.

**Gate:**

- Editor can load Plugin.
- Runtime Optional Module can be enabled / removed by Build Profile.
- Plugin API does not expose Engine Private Header.
- Runtime UI can create basic game interfaces.
- UIElement does not depend on SceneNode / EntityID; UIDocument can be mounted on a Scene UIComponent.
- Editor Hierarchy can simultaneously edit Scene Entity and UIElement.
- ScreenSpace / WorldAnchored / WorldSpace share the Widget / Layout / Event Framework.
- VirtualizedListView does not create an equivalent number of UIElements based on total data volume.
- Runtime UI Logical Resolution is completely decoupled from 3D Dynamic Resolution.

- Zig GameModule can be built and loaded from the Editor.
- After modifying Zig gameplay code, it can be safely reloaded without retaining old module code pointers / callbacks.
- When state migration fails, reload can be rejected while retaining the old module or safely stopping the Play Session.
- Gameplay ABI does not expose C++ STL / private type.

---

### Phase 5: Animation + Physics + Audio + FMOD / Spine Optional Plugins

**Core Objectives:**

Integrate dynamic systems and Middleware, and use third-party assets to verify Asset / Bundle Extensibility.

**Key Tasks:**

- **Animation**
  - Skeleton
  - Animation Clip
  - Crossfade
  - Root Motion
  - GPU Skinning foundation
  - Dynamic Skinning Buffer / packed bone matrices / per-draw offset

- **Physics**
  - Collider / Physics Material / Collision Matrix editor tools.
  - Character / Contact / Query debug visualization.
  - Jolt CPU-authoritative Physics.
  - Collision / Rigidbody / Trigger.
  - Character / Query foundation.
  - Immediate Query / CPU Batch Query.
  - Physics Execution Domain: CPUAuthoritative / CPUBatched / GPUVisual / GPUDeferred.
  - GPU VFX Collision integration.
  - GPU Cloth foundation.
  - GPU Deferred Query interface.
  - Physics CPU/GPU profiler foundation.
  - Character Framework: CharacterIntent → CharacterMotor → CharacterController.
  - Ground / Slope / Step / Snap.
  - Moving Platform / Dynamic Body Interaction.
  - Root Motion Resolve / External Velocity / Knockback.
  - Crouch / Capsule Resize / Teleport semantics.
  - Fixed Tick / Render Interpolation.
  - Batch-first Zig Character ABI.

- **Time / Tick Integration**
  - Fixed Simulation Tick
  - Variable Render Frame
  - Transform interpolation
  - Catch-up clamp / spiral-of-death protection
  - WorldTimeState / GameTime / UnscaledTime / FixedTickTimer.

- **Navigation / AI**
  - Recast / Detour tiled NavMesh backend.
  - NavTile streaming / async path query / OffMesh Link / runtime obstacle.
  - Navigation → DesiredVelocity → CharacterIntent integration.
  - Blackboard / Behavior Tree compiled runtime.
  - Perception budget / AI update LOD.
  - ML Policy interface foundation.

- **Audio**
  - Audio Event Authoring: waveform / event graph / bus graph / snapshot / preview.
  - MiniAudioBackend
  - AudioManager
  - Streaming Audio
  - Audio Handle
  - AudioResourceRegistry
  - AudioResidencyScope: Global / Scene / Character / UI.
  - Shared Resource Residency Ref / Voice Pin / Streaming Pin.
  - Scene transition preload / release.
  - Scope Release Policy / FadeOutAndRelease.
  - Audio Residency Priority / Memory Budget integration.
  - Audio Residency Profiler.

- **FMOD Optional Plugin**
  - FMOD Studio
  - Event
  - Bank
  - Parameter
  - Bank safe unload / reload
  - FMOD Bank as a special Bundle Asset

- **Spine Optional Plugin**
  - Runtime
  - Editor importer
  - Animation preview
  - Skin / Event
  - Bundle Asset integration

- `JobSystemTaskAdapter` for Jolt / middleware / plugin task integration.

- **VFX / Particle Runtime Foundation**
  - VFX Asset / Logical Emitter / Module Graph.
  - CPU Particle SoA.
  - Stateless Emitter.
  - Basic GPU Particle Pool.
  - Sprite / Mesh / Trail renderer.
  - Soft Particle.
  - VFX Instance Shared ParameterBlock.
  - Gameplay / Visual separation.

- **Animation Framework Foundation**
  - Skeleton Viewer / Clip Preview / Event Track / Retarget / Graph Editor authoring.
  - Skeleton / Animation Clip runtime format.
  - Animation Graph Compiler.
  - State Machine / Blend Tree / Layer / Bone Mask.
  - Pose Cache.
  - Root Motion.
  - Basic IK.
  - GPU Vertex Skinning.
  - Global Skinning Buffer / skinningMatrixOffset.
  - Skinned Mesh Instancing.
  - Animation Update LOD / Skeleton LOD foundation.

**Gate:**

- MiniAudio path can operate independently.
- Releasing the Scene AudioResidencyScope does not affect Global / UI / Music / other active Scope.
- Active Voice / Streaming Pin can prevent premature unload of Audio Resource.
- Scene A → Scene B Audio preload / release does not produce a synchronous load spike.
- Audio Resource must not be reclaimed before FadeOutAndRelease completes.
- When FMOD is Disabled, FMOD is neither Linked nor Packaged.
- When Spine is Disabled, Spine Runtime is neither Linked nor Packaged.
- FMOD Bank can go through Bundle Update.
- Plugin Native Library does not go through Remote Update.

---

### Phase 6: WebView + Platform Services + Mobile Integration

**Core Objectives:**

Complete mobile integration, Native Platform Service, and Overlay Layout.

**Key Tasks:**

- **Native Overlay WebView**
  - Runtime UI `WebViewElement` / `UIElement` integration.
  - Windows WebView2.
  - Android WebView.
  - iOS / macOS WKWebView.
  - Native Overlay V1; OffscreenTexture future capability.
  - Local HTML Asset / Remote HTTPS.
  - JSON Message Bridge / Origin Whitelist / Trust Policy.
  - Cache / Cookie / Storage Profile.
  - Gesture single-owner routing.
  - Screen-space absolute bounding box
  - Viewport Sync
  - Safe Area
  - DPI / Retina / Density
  - Orientation

- **Input Hit-Test**

```text
Block
Pass-Through
```

- **Unified Input Framework**
  - Raw / Device-level API; Action Mapping is an optional convenience layer.
  - User-defined InputAction / ActionMap; Engine retains no Move / Jump / Attack semantics.
  - Keyboard + Mouse + Gamepad + Touch can simultaneously operate on the same InputUser.
  - Multi-touch + stable PointerID + per-pointer Capture / Ownership.
  - VirtualJoystick / VirtualButton / VirtualDPad / VirtualTouchRegion.
  - InputContext + coarse-grained InputLayer / Routing Layer.
  - Project-level UI Input Interaction Matrix: `InputLayer → UILayer`.
  - Matrix Cook → compact `UILayerMask` bitset.
  - UIDocument Priority / PassThrough / ConsumeOnHit / BlockBelow.
  - Local Multiplayer UI routing.
  - UI / WorldSpace UI / WebView / Gameplay single-owner routing.
  - Fixed Tick Input Snapshot / Zig batch C ABI / Replay foundation.- **Android GPU Compatibility**
  - GPUWorkaroundDatabase
  - Capability Override
  - Restricted / Blacklist Tier

- **Platform Services**
  - App Lifecycle / Permissions / Deep Link / Clipboard / Dialog.
  - Battery / Thermal / Safe Area / Orientation / Haptics.
  - Localization ICU backend + locale data cook.
  - TextEdit / IME / grapheme / mobile keyboard integration.
  - External Browser
  - Clipboard
  - Native Share
  - Authentication extension points
  - Google / Apple Login integration path
  - Platform SDK bridge
  - Touch / Input adaptation

- WebView is positioned only in Logical Screen Space and is completely decoupled from Dynamic Resolution.

- `PerformancePolicyManager`: Thermal / Frame Pacing / Dynamic Resolution / Target FPS.

- Native Overlay Gesture Ownership / optional Forwarding capability validation.

**Gate:**

- WebView does not participate in Canvas Batch / Render Graph UI Pass.
- Unsupported behavior such as Mask / Depth / Rotation is explicitly restricted in the Editor.
- Hit-Test behavior is consistent across platforms.
- The UI Input Interaction Matrix can be configured in the Editor and correctly Cooked into the Runtime Mask.
- Keyboard / Mouse / Gamepad can operate simultaneously and are not mutually disabled due to LastActiveDevice.
- Multi-touch / Virtual Control per-pointer ownership has no duplicate delivery.
- iOS / Android physical-device validation passes.

---

### Phase 7: LOD + Culling + Terrain + Vegetation + Streaming

**Core Objectives:**

Implement large-scene optimization and dynamic asset loading.

**Key Tasks:**

- **LOD**
  - Mesh LOD
  - Screen-space LOD
  - Hysteresis
  - Dither Crossfade
  - Material / Shadow / Animation LOD

- **Culling**
  - Frustum Culling
  - Static BVH
  - Dynamic Spatial Hash / Grid
  - Terrain Quadtree
  - Vegetation Cluster
  - Occlusion foundation

- **Terrain**
  - Heightmap
  - Chunk
  - Quadtree
  - Screen-space Error
  - Terrain Collision
  - Streaming

- **Vegetation**
  - Tree Asset
  - GPU Instancing
  - Cluster Culling
  - Wind
  - Billboard
  - Streaming Cell

- Terrain Height Encoding Profile（R16_UNORM / FP16）and LOD Seam Prevention.
- Vegetation Interaction Field（GPU-driven local bend）。

- **Stylized Scene Shading**
  - StylizedPBR Architecture / Rock / Prop material.
  - Static Lightmap / Probe / Reflection Probe integration.
  - Shadow Tint / Light Tint / Stylization Curve.
  - Lighting LOD foundation.
  - Environment Fog / Height Fog / Cloud Shadow integration.

- **Vegetation Shading**
  - Alpha Cutout.
  - Wind Vertex Animation.
  - Transmission / Back Lighting.
  - Probe / SH lighting.
  - Instancing-compatible material path.

- **Water Foundation**
  - Shallow / Deep Color.
  - Depth Fade.
  - Fresnel.
  - Reflection Probe fallback.
  - Refraction / Foam extension points.

**Gate:**

- Large Scene can be dynamically loaded / unloaded according to Streaming Radius.
- Terrain / Vegetation does not create a large number of Scene Nodes.
- Memory Budget can track Streaming Residency.
- StylizedPBR / Vegetation / Water do not create independent Renderers.
- Vegetation Back Lighting / Wind pass visual and performance testing on the representative mobile profile.
- Static Environment can simultaneously use Main Directional Light + Lightmap/Probe + Fog.

---

### Phase 8: GPU Driven + Advanced Rendering

**Core Objectives:**

Advance the high-end rendering pipeline and GPU-driven architecture.

**Key Tasks:**

- **GPU Driven Pipeline**
  - GPU Culling
  - Instance Compaction
  - Draw Indexed Indirect
  - Hi-Z Pass
  - GPU LOD Selection

- **Advanced Rendering**
  - Advanced Post-processing
  - Specialized Render Feature API
  - Custom Render Pass
  - Custom Compute Pass

- **Anime Shading Framework**
  - Multi-Ramp Material Lighting
  - Material Region / AnimeControlMap
  - Face SDF Shadow
  - Hair-specific Highlight
  - Stylized Specular
  - Material-aware Rim Light
  - Geometry Outline / Smoothed Normal
  - Eye feature extension
  - Bangs Shadow extension

- **Advanced Environment**
  - SSR + Reflection Probe fallback
  - Advanced Water Reflection / Refraction / Foam
  - Atmosphere / Fog refinement
  - Anime-friendly Tone Mapping / Color Grading profile

Can be evaluated later:

```text
Ray Tracing
Dynamic Global Illumination
Advanced Temporal Techniques
```

The above items must not block the foundational commercial release version.

- **Advanced VFX**
  - GPU Simulation / Compaction / Indirect Draw.
  - Runtime Batch Fusion.
  - Optional Sort.
  - Async Compute integration.
  - Ribbon / Decal / Distortion.
  - Event Buffer / SubEmitter DAG.
  - Collision Tier.
  - VFX Budget Manager.
  - Visibility / Simulation Rate LOD.
  - Profiling / overdraw / batch-fusion diagnostics.

- **GPU Crowd Animation**
  - Automatic Bone Animation Texture / GPU Pose Storage Cook.
  - GPUAnimationClip metadata / dependency invalidation.
  - Final Skin Matrix bake path.
  - Frame interpolation.
  - Per-instance animation time.
  - Simple GPU crossfade.
  - Animation Sharing / Pose Bucketing.
  - GPU Culling + Indirect Draw integration.
  - Compute Skinning / multi-pass reuse.
  - VAT / Impostor extension path.

- **Advanced GPU Physics**
  - GPU Cloth refinement / async compute scheduling.
  - GPU Debris pool / indirect rendering.
  - GPU Deferred Query implementation.
  - Heightfield / SDF / simplified-scene GPU collision.
  - PerformancePolicyManager / thermal / memory budget integration.
  - Optional GPU Rope / Chain.
  - Optional GPU SoftBody R&D.
  - GPU Broadphase is introduced only when the profile proves it necessary.
  - GPU RigidBody World remains Future R&D and is not a V1 prerequisite.

**Gate:**

- The GPU-driven path and CPU fallback path can be compared.
- The Profiler can display the cost of GPU Culling / Indirect Draw / Hi-Z.
- Advanced Render Feature does not break the Renderer Core Contract.
- Anime / StylizedPBR / Vegetation / Water share the same Forward+ / RenderGraph foundation.
- Representative Golden Images for Anime Face SDF, Hair Highlight, and Outline pass.
- Mixed Scene（StylizedPBR Environment + Anime Character）passes on DX12 / Vulkan / Metal.
- Water / Scene Reflection fallback is correct when SSR is unavailable / disabled.

---

### Phase 9: Feature Stripping + Packaging + Full Shipping Matrix

**Core Objectives:**

Complete full-channel commercial release validation, final module stripping, and release optimization.

The Architecture for Feature Stripping is established in Phase 0; Phase 9 is responsible for complete validation and Shipping Hardening.

**Key Tasks:**

- **Feature Stripping Validation**
  - Auto / Enabled / Disabled
  - Optional Subsystem
  - Plugin
  - Native Library
  - Runtime Asset
  - Shader Variant
  - Build Dependency

If:

```text
Feature = Disabled
+
Code / Asset Dependency Exists
```

then:

```text
CI / Build Fail
```

- **Shipping Optimization**
  - Monolithic Build
  - LTO
  - Strip Debug Symbol from shipping package
  - Disable unnecessary dynamic exports
  - Shipping asset cook
  - Shader Variant final strip

- **Shipping Matrix**
  - Windows DX12
  - Windows Vulkan
  - macOS Metal
  - Android Vulkan
  - iOS Metal
  - Representative device / quality / binding tier

- CI Build Cache / Matrix Parallelization and pipeline latency telemetry.
- Profiler Chrome Trace / JSON export shipping-strip validation.

- Long-duration multi-Scene / Mobile PSO Resident convergence and eviction/recreate stability validation.

- **Zig Gameplay Shipping Validation**
  - Windows / macOS / Android / iOS Gameplay Module build.
  - Mobile Shipping does not include a runtime native-code download path.
  - The Zig runtime / support code retains only the actually required portions.
  - Shipping symbols / debug metadata are archived separately by platform.
  - Gameplay ABI version / Build ID and App package consistency validation.

**Gate:**

- Minimal Profile Build succeeds.
- Full Feature Profile Build succeeds.
- Shipping Monolithic Build succeeds.
- Disabled Subsystem does not exist in the package.
- Unnecessary DLL / dylib / so / Framework is not included in the package.
- Shader Variant / Asset / Module stripping Report is correct.

- The minimal Zig Gameplay sample for all four platforms can be successfully built and launched in the Shipping Profile.

---

## Core Execution Principles (Architecture & CI Enforcement Guidelines)

### 1. CI Gate as an Architecture Contract

CI is not the final validation tool after development is complete, but an architectural protective barrier.

Formal specification:

> Whenever an Architecture Contract is added, an Automated CI Gate capable of automatically validating it must be established at the same time.

Includes:

- Bundle DAG circular-dependency scanning
- Shader Canonical Reflection consistency comparison
- Subsystem Feature Stripping blocking validation
- Frame Allocator Thread Ownership
- Frame-memory escape assertion
- Dynamic Skinning Frame-slot fence validation
- Skinning ResourceIndex / element-index contract validation
- RHI Resource Lifetime Validation
- Render Graph Barrier / Dependency Validation
- Plugin API / ABI Compatibility
- Minimal / Full Feature Build Matrix
- PSO Runtime Creation / Warmup metrics
- Reflection Metadata Schema Validation
- Memory Pressure / Device Recovery Smoke Test
- Android Workaround Profile Validation

- Zig Gameplay ABI Compatibility / Layout Check
- Zig GameModule Reload Lifetime / stale callback validation
- Zig Gameplay Cross-platform Build Matrix
- Gameplay ABI fine-grained call-count / allocation regression metric

- Shading Model Variant / Stripping Validation
- Anime Face SDF / Hair / Outline Golden Image Regression
- StylizedPBR Lightmap / Probe / Fog Integration Test
- Vegetation Transmission / Wind / Instancing Regression
- Water Reflection Fallback Validation
- Mixed Scene Single-Renderer Contract Validation

CI execution levels:

```text
PR
→ Representative Profile
→ Fast Architecture Gates

Nightly / Scheduled
→ Minimal Profile
→ Full Feature Profile
→ Wider Backend Matrix

Release
→ Full Shipping Matrix
→ Representative Physical Devices
```

### 2. Strictly Maintain Plugin and Bundle Boundaries

Native Code:

```text
Zig GameModule Binary
.dll
.so
.dylib
Framework
Static Library
```

Always belongs to:

```text
Plugin / App Build / Installation
```

Remote Bundle is limited to:

```text
Asset
Data
Runtime Binary Asset
FMOD Bank
Spine Asset
Localization
Scene
Terrain / Vegetation Chunk
```

Any solution that places Native Code into a Remote Bundle and downloads and executes it at Runtime is prohibited.

### 3. Prioritize Real Hardware Validation

Simulator / Desktop Simulation cannot be regarded as final Hardware Validation.

Physical-device validation should be prioritized for:

- Metal Argument Buffer
- Android Descriptor Indexing
- Android GPU Vendor Compatibility
- WebView Hit-Test
- Safe Area / Orientation
- Touch / IME
- Suspend / Resume
- Thermal
- Frame Pacing
- Memory Pressure
- Bundle mmap / file lifetime
- Mobile GPU synchronization

### 4. Vertical Slice Before Feature Accumulation

Each major Subsystem should establish an executable End-to-End Slice as early as possible.

Avoid:

```text
Renderer is half done
Asset is half done
Editor is half done
but there is no complete workflow
```

Prioritize maintaining the following as continuously executable:

```text
Import
↓
Asset
↓
Scene
↓
Render
↓
Editor
↓
Build
↓
Runtime
```

### 5. Design Feature Stripping from Phase 0, Harden in Phase 9

Feature Stripping does not begin implementation only in Phase 9.

```text
Phase 0
→ Module / Feature Architecture

Phase 1~8
→ Add a Stripping Rule whenever a Subsystem is added

Phase 9
→ Full Matrix / Shipping Validation / Size Optimization
```

Avoid attempting to remove hard dependencies that have already formed only after the engine is complete.

## v4.0 Architecture Completion Status

This version has converted the core items in the previous “system list not yet discussed in detail” into formal design decisions; items still marked Future / Optional are not considered omissions.

```text
Core Runtime foundation
→ Transform / System Scheduler / Event / Time / Memory / IO / Logging

World foundation
→ Scene / Streaming / Navigation / AI / Save State

Presentation foundation
→ Camera / Lighting / Post / Runtime UI / Audio / Animation

Tooling foundation
→ Editor / Prefab / Undo / Reflection / Import / Authoring Tools / Project Settings
```

## 54. Final Architecture Summary

Development Model:
AI-Assisted Development
AI generation + automated Gates + physical-device validation + Definition of Done

Language:
Engine Core / Renderer / Editor = C++20
Primary Gameplay Language = Zig
Engine ↔ Gameplay = Language-Neutral Stable C ABI

Gameplay Scripting:
Zig native GameModule
No tracing GC
Batch-first API + Explicit Tick + Event-driven
Desktop Editor supports build/reload
Mobile code remains build-time native code

Gameplay Hot Reload:
Safe Barrier
→ State Serialize / Migration
→ Unload Old Module
→ ABI Handshake
→ Load New Module
→ State Restore
No stale function pointer / callback / module allocation escape

Future Language Binding:
Stable ABI keeps Rust / C
# / other bindings possible without redesigning Engine Core


Build:
CMake

PCH:
EnginePCH + RendererPCH + EditorPCH

Graphics:
DX12 + Vulkan + Metal

Windows:
DX12 Default / Vulkan Selectable

Shader:
Slang
Metal requires PoC validation

Renderer:
Forward+ + Render Graph

Shading Models:
PBR
StylizedPBR
Anime
Vegetation
Water
Unlit

Stylized Scene:
StylizedPBR + Lightmap/Probe + Reflection Probe + Fog/Atmosphere + Color Grading

Anime Character:
Multi-Ramp + Face SDF + Hair-specific Shading + Stylized Specular + Rim + Per-material Geometry Outline

Vegetation Shader:
Alpha Cutout + Wind + Transmission/Back Lighting + Probe/SH + GPU Instancing

Water Shader:
Depth Fade + Shallow/Deep Color + Fresnel + Reflection Probe + optional SSR/Refraction/Foam

Scene / Character Mix:
Environment → StylizedPBR
Character → Anime
Shared Forward+ / Shadow / Fog / Reflection / RenderGraph

VFX / Particle:
One VFXComponent → One VFXInstance → N Logical Emitters → M Simulation Batches → K Render Batches
Stateless + CPU SoA + GPU Simulation
VFX Compiler performs DAG validation / dead-module elimination / compatible emitter fusion
Gameplay projectile and visual VFX remain separated
VFX Budget Manager integrates with PerformancePolicyManager


Animation:
Skeleton / Clip / Animation Graph / State Machine / Blend Tree / Layer / Pose Cache
CPU Animation Logic + GPU Skinning
Global Skinning Buffer with skinningMatrixOffset
Skinned Mesh Instancing + Animation Sharing + Skeleton LOD
Automatic GPUAnimationClip / Bone Animation Texture Cook
GPUAnimationPoseStorage abstraction instead of hardcoded Texture2D
Crowd path supports per-instance time / frame interpolation / simple crossfade / indirect draw


Spatial:
BVH + Dynamic Grid + Terrain Quadtree + Vegetation Cluster
World Partition uses Stable Grid Cells + Loose Quadtree spatial index
Indoor uses Room / Portal Graph; Outdoor ↔ Indoor via Streaming Gateway

Terrain:
Chunk / Quadtree / LOD / Streaming

Vegetation:
SpeedTree-like Runtime / Wind / LOD / Billboard / GPU Instancing

Mesh LOD:
Screen-space LOD + Hysteresis + Dither Crossfade

Texture:
Mipmap LOD + Budget-aware Texture Streaming

Binding:
Hybrid Bindless-first
Binding Tier + Fallback

Scene / World:
World = Runtime Simulation Universe
Scene = Content Ownership / Serialization Unit
EditorWorld ≠ PlayWorld
Multiple / Additive Scene + Persistent Scene
Async Load → LoadedInactive → Atomic Activation → Safe Unload
WorldCommandBuffer + Structural Barrier
Cross-scene reference uses persistent logical identity, not raw EntityID
Scene ≠ StreamingCell ≠ GameplayZone ≠ PhysicsWorld / NavigationWorld / RenderWorld

World Streaming:
StreamingCell = Runtime Residency Unit
Stable Fixed Grid Cells + Loose Quadtree Spatial Index
Room / Portal Graph for Indoor
Multiple Streaming Sources → StreamingDemand → Priority / Budget → Residency
Cell / Bundle identity remains separate
Large-world coordinate foundation + camera-relative rendering

HLOD:
Editor / Cooker offline generation
Loose Quadtree hierarchy foundation
HLOD Node ≠ Streaming Cell
Runtime only selects / streams HLOD representation
Full Simulation Residency ≠ Far Visual Residency
Deterministic + Incremental HLOD build

Runtime Components:
Component Pool / SoA

Render World:
Derived Frame Data

Reflection:
V1 Runtime Metadata + Macro
Future Codegen

Editor:
Dear ImGui

Runtime UI:
Custom Retained Mode Framework
UIDocument + UIElementID Tree; UIElement ≠ SceneNode / EntityID
Scene UIComponent mounts UIDocument; Editor Hierarchy mounts UI tree through adapter
ScreenSpace + WorldAnchored + WorldSpace share Widget / Layout / Style / Event / Binding
JSON UI + ViewModel/Data Binding + VirtualizedListView + UI Batch / RenderGraph
Logical UI Resolution independent from 3D Dynamic Resolution

Asset:
UUID + Asset Database + Importer + Bundle

FBX:
Editor Only

Physics:
Jolt CPU-authoritative core
Character Framework: CharacterIntent → CharacterMotor → CharacterController → CharacterMotionResult
CharacterController = Native Core System; CharacterMotor = Replaceable Gameplay Policy
Ground / Slope / Step / Snap / Moving Platform / Root Motion / Crouch / Teleport / Dynamic Body Interaction
Hybrid Physics Execution Domain: CPUAuthoritative / CPUBatched / GPUVisual / GPUDeferred
GPU acceleration targets Cloth / VFX Collision / Debris / Deferred Query
Gameplay-critical Immediate Query remains CPU / Jolt
GPU Broadphase / GPU RigidBody World remain profile-driven Future R&D

Audio:
miniaudio built-in default backend
AudioResourceRegistry + AudioResidencyScope
Global / Scene / Zone / Character / Encounter / UI residency ownership
Scope Release + Voice/Streaming Pin + FadeOutAndRelease
Bundle Generation Pinning + Audio Memory Budget integration

Fonts:
FreeType + HarfBuzz

Localization:
V1

Save:
V1

Networking:
V1 does not include a high-level Framework

Testing:
Introduced starting from V0.x

CI:
Introduced starting from V0.x

Crash:
V1 Crash Infrastructure

Memory:
V0.x Memory Budget

Target:
Windows / macOS / Android / iOS

Visual Quality Target:
At least Unity URP level
And officially support a mixed anime-style rendering path combining StylizedPBR scenes + Anime Character NPR

FRAGMENT ENDPerformance Target:
For a fixed game genre, achieve lower CPU / RAM / Render Overhead than a general-purpose engine as much as possible at the same visual quality.

END

Shader Variant:
Feature Bitmask + Used-Variant Compile + Incremental Cache + CI Variant Budget

Golden Image:
Perceptual Diff / SSIM + Pixel Error Guardrail

Terrain Streaming:
UUID / Asset Database + Spatial Streaming Bundle

Terrain Physics:
Terrain Heightmap Source of Truth -> Jolt HeightFieldShape Derived Data

GPU Skinning:
Resource Registry -> GPU Buffer ResourceIndex


Coding Style:
Modern C++ / RAII / Handle / Data-Oriented Hot Path

Smart Pointer:
SharedPtr<T>
SharedPtr limited to genuine Shared Lifetime only

Ownership:
Engine Resource preferably uses Handle; Entity uses EntityID; Raw Pointer / Reference represents Non-owning Borrow only


JSON:
yyjson private backend + Engine JsonDocument / JsonValue / JsonWriter abstraction
Parse + Generate + Pretty / Compact / Deterministic Write
UTF-8 / Strict JSON

Data Table:
JSON Runtime Asset + Schema Validation + Typed Runtime Table
Primary Key: UInt32 / UInt64 / String
Preprocess: Sort View / Index / Group / Resolve / Derived / Weighted / Range / Custom Processor
Runtime Container: Contiguous Rows + Indices + Views + StringPool / ArrayPool + DerivedData
Immutable + DataTableRegistry + Generation Hot Reload

Data Format:
Human-readable metadata/config/DataTable → JSON
Runtime-heavy / streaming / GPU-ready assets → Binary
Optional Binary DataTable Cook only if profiling later proves necessary


Bundle Hot Update:
Remote Manifest + Download Cache + Hash Verification + Atomic Commit + Rollback

Update Strategy:
V1 Full Bundle Update
Future Delta Patch


Plugin System:
Editor Plugin + Runtime Build-time Module

Plugin Distribution:
Local Installed / Build-time only
No Remote Native Plugin Download

Plugin / Bundle Boundary:
Plugin → Code Extension
Bundle → Asset / Data Hot Update


WebView:
Cross-platform In-Game Runtime WebView
WebViewElement = UIElement Native Overlay Proxy; Native WebView ≠ UIRenderItem
Windows WebView2 + Android WebView + iOS/macOS WKWebView
Logical UI Rect + Safe Area + single-owner gesture routing
Local HTML Asset / Remote HTTPS + JSON Message Bridge
Origin / Trust Policy + Cache / Cookie / Storage Profile

V1 Rendering:
Native Overlay

Future:
Offscreen / Render-to-Texture WebView for true WorldSpace / Surface Web UI

WebView / Auth Boundary:
WebView → General In-Game Web Content
Auth Provider → Platform-compliant Login Flow


Audio Backend:
miniaudio → Built-in Default
FMOD → Optional Plugin / Professional Audio Middleware

Audio Residency:
AudioResourceRegistry + AudioResidencyScope
Scope controls runtime residency/lifetime; Bundle controls physical packaging/versioning
Active Voice / Streaming / Generation Pin prevents premature eviction

FMOD Distribution:
Native Plugin → Local Installed / Build-time
FMOD Bank → Asset Bundle / Remote Content Hot Update


Feature Stripping:
Auto / Enabled / Disabled
Compile-time Module + Build-time Dependency + Shader Variant Stripping

Optional Runtime:
Only Enabled / Required / Platform-compatible modules are packaged

Core:
Foundation modules remain mandatory


API Boundary:
Game / General Plugin → Public API only
Internal / Backend implementation hidden

Subsystem Packaging:
Foundation Core mandatory
High-level Subsystem optional and strippable

Unused Subsystem:
No Compile + No Link + No Package + No Asset Cook + No Shader Variants


Build Linking Mode:
Development / Editor → Modular
Shipping / Release → Modular or Monolithic

Core:
Prebuilt Libraries

Optional Subsystem:
Dynamic Module where appropriate during development
Static / Monolithic integration available for shipping


WebView V1 Semantics:
WebViewElement participates in UIDocument/UIElement hierarchy and RectTransform layout
Native WebView is not UIRenderItem / UI Batch / RenderGraph UI Pass
No Stencil Mask / 3D Depth / arbitrary Canvas interleaving for Native Overlay

Stripping CI Matrix:
PR representative profile
Nightly Minimal + Max/Full Feature profiles
Release platform/shipping matrix


Shader Source of Truth:
Slang only
Metal fallback → Slang → SPIR-V → SPIRV-Cross → MSL
No independent hand-written MSL fallback

Bundle Dependency:
DAG required; cycle = Build Fail
Mapped/active bundle update may defer activation to next Pre-init

Frame Allocator:
Per-thread / per-worker arena
Frame-end reset after job/render lifetime fence

Entity Generation:
32-bit generation with overflow assert/retire policy
High-frequency transient objects use dedicated pools, not Scene EntityID


Roadmap Strategy:
Foundation → Reference RHI → First Vertical Slice → Asset/Bundle → Editor/UI/Plugin → Subsystems → Platform/Mobile → World Streaming → GPU Driven → Shipping Harden

CI Philosophy:
Architecture Contract = Automated Gate

Feature Stripping:
Designed from Phase 0, continuously enforced, fully hardened in Phase 9


Dynamic Skinning:
Packed bone matrices + per-draw matrix offset + frames-in-flight

GPU Upload:
UMA → persistent/shared buffer
Discrete GPU → upload ring + device-local buffer

Binding:
Canonical ResourceIndex separated from buffer element index


Job System:
Work-stealing + priority + dependency graph + frame barriers

Asset Residency:
Explicit state machine + separate RAM / VRAM budgets + pressure policy

Input:
Raw / Device-level API + optional user-defined Action Mapping
Keyboard / Mouse / Gamepad / Touch may be active simultaneously
Multi-touch + per-pointer capture + Virtual Controls
InputContext + InputLayer → UILayer Interaction Matrix
UI / WorldSpace UI / WebView / Gameplay single-owner routing
Fixed Tick Snapshot + Zig batch C ABI + replay foundation

Time Model:
Fixed simulation tick + variable render interpolation

Font Rendering:
CJK dynamic glyph atlas + optional MSDF for Latin/Icon

Crash Infrastructure:
Backend abstraction + dump capture + symbolication pipeline


PSO:
Compatible per-platform/device cache + async warmup + runtime creation telemetry

Android GPU Compatibility:
GPUWorkaroundDatabase + capability override + fallback tier

Memory Pressure:
OnMemoryPressure + resource eviction tiers + device loss recovery

Reflection Metadata:
Canonical schema shared by Macro/constexpr V1 and Clang AST V2


Modern Rendering Contract:
Public RHI = BeginRendering/EndRendering; backend-only compatibility fallback

Bundle Generation:
Versioned storage + load-context generation pinning

CI Performance:
Dedicated runner hard gate; shared runner trend/warning


Frame Lifetime Safety:
FramePtr / FrameSpan + generation/region guard + AST persistent-storage prohibition

WebView Coordinate:
OS logical screen space only; independent from render scale

Bundle Deletion:
Versioned generations + pending-delete queue + reference drain

Module Allocator:
Handle/POD/Span boundary + allocate/destroy in owning module or explicit Engine allocator contract


Component Storage:
Sparse Set / Sparse Array + Dense SoA as default Scene Component Pool

Pipeline Metadata:
Offline PipelineLayoutMetadata; runtime direct indexed binding

Worker Arena Growth:
Thread-local page/chunk growth + reuse + budget + trim

Performance Policy:
Thermal + frame pacing + dynamic resolution + target FPS

AI Change Scope:
Goal/risk/subsystem based governance; diff size is a soft signal


Frame Module Boundary:
FrameSpan / FrameDataHandle only for transient cross-module data; no retention / async capture / lifetime erasure


Dependency Graph Gate:
Unified module-layer dependency graph; forbidden dependency edge = hard fail

Third-party Jobs:
JobSystemTaskAdapter + external worker budget / oversubscription profiling

Shader C++ Layout:
Canonical reflection generates deterministic C++ parameter headers

Streaming Scheduler:
Priority preemption + platform IO/decode/upload concurrency budgets

Plugin Handshake:
Runtime C-ABI Plugin_Init version/build/hash/capability validation


Terrain Precision:
R16_UNORM / FP16 profile + stitching/skirt/LOD morph seam prevention

Vegetation Interaction:
GPU interaction field for local bend without per-instance CPU transforms

Gesture Ownership:
Native/Engine input has single owner; optional explicit forwarding only

Profiler Export:
JSON / Chrome Trace compatible offline capture with correlation IDs

CI Build Infrastructure:
compiler/artifact caches + parallel matrix + latency/cache-hit telemetry


PSO Residency:
Shared Residency framework + runtime budget/watermarks + LRU candidate selection + fence-safe destroy + async recreation; CI Variant Budget remains a separate build-time gate


Variant Accounting:
Per-shader/platform profile tracks Theoretical / After-Pruning / Project Used / Cooked / Budget

PSO Fence Timeline:
single global timeline uses one LastUsedFenceValue; independent queues require all relevant queue fences complete


---

# Appendix A — V1 Plugin / Third-party SDK / Video & Media Detailed Contract (Normative)

# Cross-Platform 3D Engine — V1 Plugin / Feature Module Modularization Plan

**Document Version: Draft v1.2**
**Corresponding Engine Generation: V1.x**
**Objective: Establish a Plugin / Feature Module architecture in V1 that can be extended over the long term to V2 / V3.**

---

# I. V1 Modularization Objectives

V1 must comply with the following from the outset:

```text
Feature exists in Engine Repository
≠
Feature exists in every Game Build
```

Official Shipping objective:

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

Not:

```text
bool enabled = false;
```

---

# II. Plugin Types

```text
Plugin / Feature Module
├─ Core Module
├─ Feature Module
├─ Backend Plugin
├─ Editor / Cooker Plugin
└─ Provider Plugin
```

### Core Module
Always present:

```text
EngineCore
Math
Memory
JobSystem
TaskGraph Core
Reflection Core
Serialization Core
VFS Core
Asset Registry
Resource Registry
World Core
Scene Core
Entity / Component Core
Transform Core
Event Core
Time Core
Logging Core
Platform Core
Plugin Manager
RHI Core
RenderGraph Core
Shader Metadata Core
```

### Feature Module
May be completely unused by a game:

```text
Physics
Character
Navigation
AI
Audio
Video / Media
Runtime UI
WebView
Terrain
Vegetation
VFX
Localization
Save
DataTable
Timeline foundation if introduced later
```

### Backend Plugin
The framework exists, but the implementation is replaceable:

```text
Physics.Jolt
Audio.Miniaudio
Audio.FMOD
RHI.DX12
RHI.Vulkan
RHI.Metal
```

### Editor / Cooker Plugin
Not included in Shipping:

```text
Importer.FBX
Importer.glTF
Importer.Texture
Importer.Audio
Importer.Font
Importer.Spine
Editor.Scene
Editor.Prefab
Editor.Animation
Editor.Physics
Editor.Audio
Editor.UI
Editor.DataTable
Editor.WorldPartition
Editor.Profiler
```

### Provider Plugin
Third-party services:

```text
Crash Reporting Provider
Telemetry Provider
Cloud Save Provider
Platform SDK Provider
```

---

# III. Plugin Manifest

Every Plugin has:

```text
PluginManifest
├─ PluginID
├─ Version
├─ EngineVersionRange
├─ Type
├─ Runtime / Editor / Server / Client
├─ Dependencies
├─ OptionalDependencies
├─ Platforms
├─ FeatureFlags
├─ ABIVersion
└─ HotReloadPolicy
```

Dependency cycles are prohibited:

```text
Plugin A → B → C → A
```

CI hard fail.

---

# IV. Shipping Build Principles

```text
Project Feature Selection
↓
Dependency Resolve
↓
Compile Selected Modules
↓
Link Selected Modules
↓
Cook Selected Assets
↓
Cook Selected Shaders
↓
Package Selected Runtime Data
```

Development / Editor:

```text
Dynamic Module
→ Hot Reload / Faster iteration
```

Shipping:

```text
Selected Plugins only
→ Static / Monolithic Link allowed
→ LTO / WPO
→ Strip unused
```

Pluginization does not mean the final product must necessarily contain many DLLs.

---

# V. Physics Plugin

```text
Physics.Core
Physics.Jolt
```

`Physics.Core` defines:

```text
PhysicsWorld
RigidBody
Collider
PhysicsQuery
PhysicsMaterial
CharacterController Interface
```

V1 default:

```text
Physics.Jolt
```

If the game does not need Physics at all:

```text
Physics.Core
Physics.Jolt
```

Both can be stripped.

---

# VI. Character Framework Plugin

```text
Character.Core
Character.DefaultMotor
```

Suitable for:

```text
Action RPG
MOBA
FPS
TPS
Platformer
```

Not needed for:

```text
Card Game
Pure Strategy
Server Simulation without characters
```

and therefore may be omitted.

---

# VII. Navigation Plugin

```text
Navigation.Core
Navigation.RecastDetour
```

V1 default:

```text
Recast / Detour
```

If the game has no AI pathfinding:

```text
Navigation.*
→ strip
```

---

# VIII. AI Plugin

Split into:

```text
AI.Core
AI.Blackboard
AI.BehaviorTree
AI.Perception
```

V1 does not force the entire package to be included.

For example:

```text
AI.Core + Blackboard
```

may exist without using Behavior Tree.

---

# IX. Audio Plugin

```text
Audio.Core
Audio.Miniaudio
Audio.FMOD
```

V1 Default:

```text
Audio.Miniaudio
```

FMOD:

```text
Optional Backend Plugin
```

Dedicated / Headless:

```text
Audio.*
→ strip
```

---

# X. Runtime UI Plugin

```text
UI.Runtime
```

Completely independent of:

```text
Editor UI
→ Dear ImGui
```

Therefore:

```text
Dedicated Server
Headless Tool
Training Build
```

can all completely remove Runtime UI.

---

# XI. WebView Plugin

```text
UI.WebView
UI.WebView.Windows
UI.WebView.Android
UI.WebView.iOS
UI.WebView.macOS
```

Strongly Optional.

For a game without WebView:

```text
Native WebView bridge
JS bridge
WebView asset support
Platform web code
```

none of these enter the Build.

---

# XII. Spine Plugin

```text
Animation.Spine
```

Completely Optional.

For projects that only perform 3D Skeleton Animation:

```text
Spine Runtime
→ strip
```

---

# XIII. Terrain Plugin

```text
Terrain.Core
Terrain.Renderer
Terrain.Editor
```

Optional.

Does not require:

```text
Every 3D game to include Terrain
```

---

# XIV. Vegetation Plugin

```text
Vegetation.Core
Vegetation.Render
Vegetation.Editor
```

Not tightly coupled to Terrain.

May be placed in:

```text
Mesh World
Procedural World
Terrain World
```

---

# XV. VFX Plugin

```text
VFX.Core
VFX.GPU
VFX.Editor
```

Minimal projects may omit it entirely.

VFX transient particles still do not use Scene EntityID.

---

# XVI. Localization Plugin

```text
Localization.Core
Localization.ICU
Localization.Editor
```

A single-language game may omit ICU.

However:

```text
UTF-8 Text Core
```

is not a Localization Plugin and remains a fundamental text capability.

---

# XVII. Save Plugin

```text
Save.Core
Save.Local
Save.Cloud.Provider.*
```

V1 default:

```text
Save.Local
```

If the project does not need local save:

```text
Save.*
→ strip
```

---

# XVIII. DataTable Plugin

```text
DataTable.Core
DataTable.Editor
DataTable.CSVImporter
DataTable.XLSXImporter
```

Runtime contains only:

```text
DataTable.Core
```

CSV / XLSX importer:

```text
Editor-only
```

---

# XIX. Asset Importer Plugins

All Editor-only:

```text
Importer.FBX
Importer.glTF
Importer.Texture
Importer.Audio
Importer.Font
Importer.Spine
```

Shipping Runtime:

```text
0 importer code
```

Reads Cooked Asset only.

---

# XX. Material / Render Feature Plugins

Render Features can be modularized:

```text
RenderFeature.PBR
RenderFeature.Stylized
RenderFeature.Anime
RenderFeature.Water
RenderFeature.Vegetation
RenderFeature.PostProcess
```

The Cooker generates Shader / PSO only according to:

```text
Enabled Features
+
Actually Used Materials
+
Platform Capability
```

---

# XXI. Post Processing Plugins

```text
PostProcess.Core
PostProcess.Bloom
PostProcess.SSAO
PostProcess.DOF
PostProcess.MotionBlur
PostProcess.ColorGrading
```

Effects that are not needed:

```text
Do not compile
Do not Cook Shader
Do not create PSO
```

---

# XXII. Editor Plugin

V1 Editor is minimally split into:

```text
Editor.Scene
Editor.Prefab
Editor.UI
Editor.Animation
Editor.Physics
Editor.Audio
Editor.DataTable
Editor.WorldPartition
Editor.Profiler
```

Editor extensions may only use the official Extension API:

```text
Window
Inspector
Property Drawer
Asset Editor
Importer
Menu
Toolbar
Gizmo
Validator
Build Step
Profiler Panel
```

---

# XXIII. Plugin and Scene / Prefab

Plugin Component Serialize:

```text
TypeID
PluginID
Version
Payload
```

When a Plugin is missing:

```text
Editor
→ MissingComponentProxy
→ Preserve payload
```

Shipping Cook:

```text
Referenced runtime plugin missing
→ hard fail
```

---

# XXIV. Plugin and Zig

Zig Gameplay does not directly obtain a C++ Plugin pointer.

```text
Plugin Feature
↓
Versioned Stable C Function Table
↓
Zig Binding
```

Plugin reload:

```text
Stop new calls
↓
Drain Jobs
↓
Drain Callbacks
↓
Invalidate Handles
↓
Unload
```

---

# XXV. Hot Reload Policy

```text
HotReloadPolicy
├─ Never
├─ EditorOnly
├─ DevelopmentOnly
└─ SafeRuntime
```

For example:

```text
RHI Backend
→ Never / restart preferred

FBX Importer
→ EditorOnly

Gameplay Zig
→ SafeRuntime
```

---

# XXVI. V1 Build Profile

```text
Editor
DesktopClient
MobileClient
DedicatedServerFoundation
Tool
```

Different Profiles have different Plugin Sets.

---

# XXVII. Common V1 Combinations

### General RPG

```text
Physics.Jolt
Character
Navigation.RecastDetour
AI.BehaviorTree
AI.Perception
Audio.Miniaudio
Media.Video
UI.Runtime
Localization
Save.Local
DataTable
Terrain
Vegetation
VFX
```
### Parkour

```text
Physics.Jolt
Character
Audio.Miniaudio
Media.Video
UI.Runtime
Localization
DataTable
VFX
```

Can strip:

```text
Navigation
AI
Terrain
Vegetation
WebView
Spine
```

### Headless Tool

```text
World
Scene
Asset
Data
```

Remove all Presentation modules.

---

# 28. V1 Definition of Done

V1 Plugin Framework completion criteria:

```text
1. Project can explicitly Enable / Disable Plugin.
2. Dependency graph can be validated and has no cycles.
3. Disabled Runtime Plugin is not included in the Shipping link.
4. Disabled Render Feature does not Cook the corresponding shader variants.
5. Editor-only Plugin is not included in Shipping.
6. Importer code is not included in Runtime.
7. Scene / Prefab can retain data for Missing Plugin.
8. Zig Plugin API maintains a Stable C ABI.
9. Dedicated / Headless Profile can strip UI / Audio / Renderer-dependent modules.
10. Both Development dynamic / Shipping monolithic modes can be built.
11. Media / Video can be an Optional Feature and be completely stripped from Headless / Server.
```

---


# 30. Developer-Customized Plugin / Bridge SDK

V1 must provide formal third-party Plugin development capabilities.

Core principles:

```text
Third-party Plugin
→ Bridge Layer / Public SDK
→ Engine Capability

Third-party Plugin
✕ Direct Engine Private Access
```

Overall architecture:

```text
Game / Third-party Plugin
        │
        ▼
┌─────────────────────────────┐
│      Engine Plugin SDK      │
│      / Bridge Layer         │
├─────────────────────────────┤
│ Stable C ABI                │
│ C++ Convenience SDK         │
│ PluginHost                  │
│ Service Registry            │
│ Extension Registry          │
│ Handle / POD API            │
│ Event / Command API         │
│ Asset / Type Registry       │
└──────────────┬──────────────┘
               │
               ▼
         Engine Public API
               │
               ▼
         Engine Internals
```

V1 supports two primary Native Plugin entry points:

```text
1. Stable C ABI Plugin
2. C++ Convenience Plugin SDK
```

The C++ SDK is built on the Stable C ABI / Public Service Contract and does not allow third parties to depend on the Engine private ABI.

---

# 31. Stable C Plugin ABI

Plugin export:

```c
typedef struct EnginePluginHostAPI EnginePluginHostAPI;
typedef struct EnginePluginExports EnginePluginExports;

ENGINE_PLUGIN_EXPORT
bool EnginePlugin_Load(
    const EnginePluginHostAPI* host,
    EnginePluginExports* out_plugin);

ENGINE_PLUGIN_EXPORT
void EnginePlugin_Unload(void);
```

`EnginePluginHostAPI` contains only:

```text
Versioned Function Tables
Opaque Handles
POD Structs
Stable IDs
Allocator API
Logging API
Service Query API
Extension Registration API
```

Prohibited across the ABI:

```text
std::string
std::vector
std::function
RTTI object
C++ exception
Engine private class
Backend native pointer
```

---

# 32. C++ Plugin SDK

For improved usability, the following may be provided:

```cpp
class IEnginePlugin
{
public:
    virtual bool OnLoad(const PluginHost& host) = 0;
    virtual void OnUnload() = 0;
};
```

However, this layer belongs only to the Public Plugin SDK.

Formal distinction:

```text
C++ Convenience SDK
→ May change only within declared compatibility policy

Stable C ABI
→ Canonical low-level compatibility boundary
```

Third parties seeking the most stable cross-compiler / cross-language compatibility should prioritize use of the Stable C ABI.

---

# 33. PluginHost

Plugins do not directly locate the Engine singleton.

Formal usage:

```text
PluginHost
```

Capabilities:

```text
GetService()
RegisterService()
RegisterExtension()
RegisterAssetType()
RegisterImporter()
RegisterCooker()
RegisterEditorExtension()
RegisterEventSink()
RegisterCommand()
GetAllocator()
GetLogger()
```

Prohibited:

```text
Engine::GetSingleton()->PrivateSubsystem->...
```

---

# 34. Service Registry

The Engine core and Plugins interact through:

```text
ServiceID
+
Versioned Service API
```

For example:

```text
SERVICE_RENDER
SERVICE_PHYSICS
SERVICE_NAVIGATION
SERVICE_AUDIO
SERVICE_INPUT
SERVICE_UI
SERVICE_ASSET
```

Plugin:

```text
GetService(ServiceID)
```

If the capability does not exist:

```text
Unsupported / nullptr
```

The entire engine is not forced to pull that feature into the build.

---

# 35. Services Provided by Plugins

Third parties not only consume Engine Services; they can also provide new backends / providers.

For example:

```text
Physics.Core
↓
Physics.CustomVendor Plugin
↓
Vendor Physics SDK
```

Or:

```text
Navigation.Core
↓
MyNavigationBackend
```

Or:

```text
Audio.Core
↓
CustomAudioBackend
```

Therefore:

```text
Framework
→ Stable Service Contract

Backend
→ Plugin
```

---

# 36. Bridge Plugin

V1 formally defines:

```text
Bridge Plugin
```

Purpose:

```text
Engine Framework
↓
Bridge Plugin
↓
Third-party SDK
```

The Bridge is responsible for:

```text
Type Conversion
Handle Translation
Lifetime Translation
Thread Boundary
Callback Translation
Error Translation
Version Adaptation
Memory Ownership
```

For example:

```text
Audio.Core
↓
Audio.FMOD
↓
FMOD SDK
```

Gameplay sees only:

```text
AudioEventID
AudioHandle
```

and never `FMOD::Studio::*`.

---

# 37. Custom Asset Plugin

Third parties can register their own Asset Types:

```text
.quest
.dialogue
.voxel
.worldgen
.mydata
```

A Plugin can provide:

```text
Asset Type
Importer
Cooker
Runtime Loader
Inspector
Asset Editor
Validator
```

Process:

```text
*.quest
↓
Quest Importer
↓
Quest Authoring Asset
↓
Cook
↓
Quest Runtime Asset
```

The Engine Core does not need to know all game data types in advance.

---

# 38. Editor Extension SDK

Third parties can add:

```text
Window
Inspector
Property Drawer
Asset Editor
Menu
Toolbar
Gizmo
Validator
Build Processor
Profiler Panel
Scene Overlay
```

All Editor mutations must go through:

```text
Editor Command
Transaction
Document API
```

To ensure:

```text
Undoable
Auditable
Validatable
```

---

# 39. Plugin Manifest Extensions

Each developer Plugin:

```text
plugin.json
```

At minimum:

```json
{
  "id": "com.company.quest",
  "version": "1.2.0",
  "engine": ">=1.0 <2.0",
  "type": "feature",
  "runtime": true,
  "editor": true,
  "dependencies": [
    "engine.asset",
    "engine.ui"
  ]
}
```

It may additionally declare:

```text
Capabilities
Platforms
Client / Server
EditorOnly
Required Services
Optional Services
HotReloadPolicy
ABIVersion
ThirdPartyLibraries
```

---

# 40. Capability Negotiation

Plugins must not assume that all platforms have the same capabilities.

They may require:

```text
ComputeShader
RayTracing
MeshShader
WebView
Network
Touch
GPUStorageBuffer
```

At Load / Build time:

```text
Plugin Requirements
↓
Target Capabilities
↓
Compatible?
```

When incompatible:

```text
Disable
or
Build Fail
```

This is determined by Project Policy.

---

# 41. ABI / Version Handshake

Before Plugin Load:

```text
Engine
↓
Plugin Handshake
├─ ABI Version
├─ Engine Version
├─ Build Configuration
├─ Platform
├─ Architecture
└─ Capabilities
```

Mismatch:

```text
Reject Load
```

It must not wait until undefined behavior / crash to discover the mismatch.

---

# 42. Memory Ownership Bridge

Across modules, the following is not allowed:

```text
Plugin malloc
↓
Engine free
```

Nor:

```text
Engine new
↓
Plugin delete
```

Formally provide:

```text
EngineAllocatorAPI
```

or:

```text
Caller Owns Memory
Callee Copies
```

Each API must explicitly indicate ownership.

---

# 43. Callback / Lifetime Safety

Plugin callbacks must not retain raw C++ object pointers.

Use:

```text
CallbackID
UserToken
Generation
```

Unload:

```text
Stop New Calls
↓
Cancel / Drain Callbacks
↓
Wait Plugin Jobs
↓
Unregister Services
↓
Invalidate Handles
↓
Unload Binary
```

Use the same lifetime discipline as Zig Gameplay Hot Reload.

---

# 44. Plugin API Tiers

Recommended separation:

```text
Stable Plugin SDK
Internal Plugin API
```

Stable Plugin SDK is for:

```text
Game Team
Third-party Developer
Middleware Vendor
```

Internal API is for:

```text
Engine Team
RHI Backend
Low-level Renderer
Memory / Job internals
```

Third parties must not depend on the Internal API.

---

# 45. Game Plugin

The game's own large subsystems can also be made into Plugins:

```text
Game.Combat
Game.Quest
Game.Dialogue
Game.Crafting
Game.WorldEvent
```

A Game Plugin can provide simultaneously:

```text
Runtime
Editor
Asset Type
Zig API
Validation
Cook Step
```

This avoids placing all game logic into a single huge module.

---

# 46. V1 Third-party Plugin Gate

The V1 Plugin SDK completion criteria additionally include:

```text
11. Third parties can write Native Plugins without modifying Engine Source.
12. Stable C ABI can be used by C / C++ / Zig / Rust and similar native languages.
13. Plugins can query Engine Services.
14. Plugins can register their own Services.
15. Plugins can register Asset Types / Importers / Cookers / Editor Tools.
16. When a Plugin is missing, Scene / Prefab can retain the serialized payload.
17. ABI mismatches are rejected before Load.
18. After Plugin unload, there are no dangling callbacks / jobs / handles.
19. Plugins can use Development Dynamic and Shipping Static/Monolithic modes.
20. Third-party Plugins do not need to include Engine private headers.
```



# 47. V1 Video / Media Framework

V1 formally adds the Video Playback Framework.

Positioning:

```text
Video
→ Optional Runtime Feature Module

Headless / Dedicated Server
→ Can be fully stripped
```

Video is not directly hard-bound to Runtime UI, nor directly hard-bound to Scene.

Core architecture:

```text
VideoSource
↓
Media Demuxer
↓
Video Decoder ──────────────┐
↓                           │
Decoded Video Frames        │
↓                           │
Video Frame Queue           │
↓                           │
GPU Import / Upload         │
↓                           │
VideoTexture                │
↓                           │
Renderer / UI / Material    │
                            │
Audio Decoder ──────────────┘
↓
PCM Queue
↓
Audio.Core
↓
Audio Bus / Mixer / Device
```

Formally:

```text
Video Decode
≠ Renderer

Video Audio
≠ Separate Audio Device

Video UI
≠ Special Native Overlay by default
```

Video audio must enter the existing `Audio.Core` in order to use:

```text
Bus
Volume
Fade
Mute
Snapshot
Duck
Output Device
```

---

# 48. Video Plugin Modules

Recommended split:

```text
Media.Core
Media.Video

Media.Backend.WindowsMF
Media.Backend.AndroidMediaCodec
Media.Backend.AppleAVFoundation

Media.Backend.FFmpeg        ← Optional fallback / tooling-dependent

UI.VideoElement             ← Optional
Editor.VideoImporter
Editor.VideoTranscoder      ← Optional
Editor.VideoPreview
```

Formally:

```text
Media.Video
→ Framework / Player / State / Clock / Queue

Backend Plugin
→ Demux / Decode / Native Surface Integration
```

If the project has no videos:

```text
Media.*
UI.VideoElement
Editor.Video*
```

can all be excluded from Shipping.

---

# 49. V1 Backend Policy

V1 prioritizes platform-native hardware-decoding backends.

```text
Windows
→ Media Foundation backend

Android
→ MediaCodec backend

iOS / macOS
→ AVFoundation / VideoToolbox based backend
```

The following may also be provided:

```text
Media.Backend.FFmpeg
```

As:

```text
Desktop fallback
Unsupported codec fallback
Editor probing / transcoding support
```

However, FFmpeg does not become an Engine Core dependency.

The Project can determine whether to include it based on licensing, platform, and actual codec requirements.

---

# 50. V1 Portable Video Profile

The V1 cross-platform “minimum portable profile” is defined as:

```text
Container
→ MP4

Video
→ H.264 / AVC

Audio
→ AAC
```

Other codecs:

```text
H.265 / HEVC
VP9
AV1
Opus
```

Use:

```text
Capability-driven / Backend-specific
```

The Engine does not promise that all platforms will support the same codecs.

Runtime can query:

```text
VideoCodecCapabilities
```

Including:

```text
DecodeSupported
HardwareDecode
MaxResolution
MaxFPS
HDRSupport
AlphaSupport
SeekSupport
```

---

# 51. Video Source

```text
VideoSource
├─ VideoAsset
├─ VFS File
├─ Local File
└─ URL
```

V1 formally supports:

```text
Cooked Local Video Asset
VFS / Bundle-resolved file
Local file where platform policy permits
HTTP / HTTPS progressive source if backend supports
```

V1 does not define the following as cross-platform mandatory capabilities:

```text
HLS
MPEG-DASH
DRM
Live Streaming
WebRTC Video
```

These are reserved for V2+ / Provider Plugin.

---

# 52. Video Asset

Authoring:

```text
Source Video
↓
VideoImporter
↓
VideoAsset Metadata
```

Metadata includes at least:

```text
Duration
Width
Height
FrameRate
Container
VideoCodec
AudioCodec
AudioTrackCount
SubtitleTrackCount
ColorSpace
HDR Metadata if present
Seekability
```

Runtime:

```text
VideoAssetID
→ resolve source / cooked representation
```

Gameplay does not use the original path as the persistent identity.

---

# 53. Video Import / Cook

The Video Importer belongs to the Editor / Cooker.

Process:

```text
Source Video
↓
Probe
↓
Validate
↓
Optional Transcode
↓
Platform Video Profile
↓
Cooked Video Asset
↓
Bundle / Package
```

Configurable options:

```text
Keep Source Encoding
Force Portable Profile
Platform Override
Resolution Limit
Bitrate Target
Audio Track Policy
Subtitle Policy
```

The Runtime does not contain a transcoder.

---

# 54. Video Transcoding

V1 may provide:

```text
Editor.VideoTranscoder
```

However, it is an Optional Tool Plugin.

Purpose:

```text
Convert unsupported source
Generate H.264/AAC MP4 baseline
Generate platform-specific derivative
Generate preview proxy
```

Transcoder implementation may use:

```text
External Tool
FFmpeg Tool Plugin
Platform Encoder
```

The Engine Runtime does not depend on it.

---

# 55. VideoPlayer

Runtime public object:

```text
VideoPlayer
```

Primary API:

```text
Open(VideoSource)
Prepare()
Play()
Pause()
Stop()
Seek(Time)
SetLoop(bool)
SetPlaybackRate(float)
SetVolume(float)
SetMuted(bool)
Close()
```

Queries:

```text
State
Duration
CurrentTime
BufferedRange
VideoSize
FrameRate
HasAudio
HasSubtitles
```

---

# 56. Video State Machine

```text
Closed
↓
Opening
↓
Preparing
↓
Ready
↓
Playing
↔
Paused
↓
Seeking / Buffering
↓
Playing
↓
Ended
```

Any major state may enter:

```text
Error
```

`Stop()`:

```text
Playback position → start / policy-defined
Decoder may remain prepared
```

`Close()`:

```text
Release decoder / source / frame queue
```

---

# 57. Video Events

Provide typed events:

```text
VideoReady
VideoStarted
VideoPaused
VideoBufferingStarted
VideoBufferingEnded
VideoSeekCompleted
VideoFirstFrameReady
VideoEnded
VideoError
```

Do not provide by default:

```text
OnEveryDecodedFrame Gameplay Callback
```

This avoids per-frame callbacks across the ABI / thread boundary.

When frame processing is required, use the formal VideoFrame consumer / render extension API.

---

# 58. A/V Sync

If the video has an audio track:

```text
Audio Clock
→ Master Clock
```

Video:

```text
Decoded Frame PTS
↓
Compare Master Clock
↓
Present
Drop
Hold
```

If there is no audio:

```text
Monotonic Media Clock
→ Master
```

Formally:

```text
Playback Time
≠ Render Frame Count
```

Fluctuations in Render FPS must not alter video playback speed.

---

# 59. Seek

Seek process:

```text
Request Time
↓
Find Keyframe / Backend Seek Point
↓
Flush Decode Queue
↓
Decoder Seek
↓
Decode Forward
↓
First Valid Target Frame
↓
Seek Complete
```

The V1 public contract is:

```text
Time-based seek
```

It does not claim that all codecs / backends can provide completely frame-exact random seeking.

Editor Preview may provide more precise stepping when supported by the backend.

---

# 60. Playback Rate

V1 supports:

```text
0.5x ~ 2.0x
```

as the recommended portable range.

Backend capabilities may impose limits.

Audio:

```text
Rate change
→ Time Stretch if backend / audio pipeline supports

otherwise
→ configurable mute or pitch-shift policy
```

Platform differences must not silently produce unpredictable results.

---

# 61. VideoFrame Runtime Representation

Decoded frames are not uniformly forced to CPU RGBA.

Formally:

```text
VideoFrame
├─ Timestamp
├─ Duration
├─ Width / Height
├─ PixelFormat
├─ ColorSpace
├─ NativeSurfaceHandle optional
└─ Plane Views / GPU Import Metadata
```

Common pixel formats:

```text
NV12
P010
YUV420
RGBA fallback
```

---

# 62. GPU Decode Surface / VideoTexture

Priority path:

```text
Native Decoder Surface
↓
GPU Import / External Texture
↓
YUV sampling / conversion shader
↓
VideoTexture
```

Avoid:

```text
Decode
↓
CPU YUV → RGBA conversion
↓
CPU copy
↓
GPU upload every frame
```

If the backend does not support zero-copy:

```text
Staging Upload Fallback
```

However, the profiler must indicate this.

---

# 63. YUV → RGB / Color Management

Video Render supports:

```text
BT.601
BT.709
BT.2020 where backend/profile supports
Limited / Full Range
SDR
HDR metadata path where supported
```

Process:

```text
Decoded YUV
↓
Color Metadata
↓
Video Conversion Shader
↓
Linear / Renderer Expected Space
↓
UI / Material / Composition
```

All video frames must not be assumed to be sRGB RGBA.

---

# 64. VideoTexture

```text
VideoTexture
```

is a Runtime Media Resource.

It can be consumed by:

```text
UI.VideoElement
Material
World-space Screen
Fullscreen Presenter
Custom Render Feature
```

Formally:

```text
VideoTexture
≠ Persistent Asset Texture

VideoAsset
→ persistent source asset

VideoTexture
→ runtime decoded presentation resource
```

---

# 65. Render Modes

V1 supports at least:

```text
1. Runtime UI VideoElement
2. Fullscreen Video Presenter
3. Material / Mesh VideoTexture
4. World-space UI Video
```

The same decoder output can be used by different presentation layers through VideoTexture.

Whether multiple consumers are allowed:

```text
Explicit ref / presentation policy
```

Decode is not duplicated by default.

---

# 66. Runtime UI Integration

Add:

```text
UI.VideoElement
```

Inherits from:

```text
UIElement
```

However, it is responsible only for presentation / control binding.

It does not decode independently.

```text
UI.VideoElement
↓
VideoPlayerHandle
↓
VideoTexture
```

Supports:

```text
Fit
Fill
Stretch
NativeAspect
Letterbox
Crop
Opacity
Clip
UI Transform
```

---

# 67. World-space / Scene Integration

Scene may contain:

```text
VideoPlayerComponent
```

However, it is only an optional scene wrapper:

```text
Scene Entity
└─ VideoPlayerComponent
   └─ VideoPlayerHandle
```

It does not mean that Video must depend on Scene.

A world-space monitor can use:

```text
VideoTexture
↓
Material Parameter
↓
Mesh
```

---

# 68. Audio Integration

Video audio track:

```text
Decoder
↓
PCM Queue
↓
Audio External Stream Source
↓
Audio Bus
↓
Mixer
```

Configurable:

```text
Output Bus
Volume
Mute
Fade
Spatialization = usually off
```

If a world monitor requires spatial audio:

```text
Video Audio
→ optional AudioEmitter routing
```

rather than creating a second audio system.

---

# 69. Subtitle Framework

V1 Video supports timed subtitles.Authoring importer readable:

```text
WebVTT
SRT
```

Unified after Cook:

```text
SubtitleTrackAsset
```

Runtime:

```text
Media Clock
↓
Subtitle Cue
↓
Localization / Text Formatting
↓
Runtime UI
```

Subtitle Text is not directly baked into the video image.

---

# Seventy: Localized Subtitle / Audio Track

Video Asset can be associated with:

```text
Subtitle Tracks by Locale
Optional Audio Tracks by Locale
```

For example:

```text
VideoAsset
├─ zh-TW subtitle
├─ ja-JP subtitle
├─ en-US subtitle
└─ optional localized audio
```

The Localization system determines the preferred track.

If the track does not exist:

```text
Fallback Chain
```

---

# Seventy-One: Video Controls

`UI.VideoElement` does not mandatorily include controls.

The Engine may provide optional:

```text
VideoControlsWidget
```

Includes:

```text
Play / Pause
Seek Bar
Current Time
Duration
Mute
Volume
Subtitle Toggle
Fullscreen
```

The game may also implement its own UI entirely.

---

# Seventy-Two: Thread Model

```text
Main / Gameplay Thread
→ Commands only

Media IO Worker
→ Read / Demux

Decode Worker / Native Decoder
→ Decode

Audio Thread
→ Consume PCM only

Render Thread / RenderGraph
→ Present VideoFrame / VideoTexture
```

Prohibited:

```text
Main Thread synchronous decode
Audio Thread file IO
Render Thread blocking network read
```

---

# Seventy-Three: Frame Queue

V1 uses a bounded frame queue.

For example:

```text
2 ~ 6 decoded frames
```

Adjusted according to:

```text
Resolution
Codec
Latency Mode
Memory Budget
```

Formal term:

```text
Bounded Queue
```

This prevents video preload from consuming unlimited RAM / VRAM.

---

# Seventy-Four: Video Decode Budget

Add:

```text
VideoManager
```

Responsible for:

```text
Active Decoder Count
Priority
Frame Queue Budget
CPU Decode Budget
GPU Decode Capability
Surface Memory
Background Suspension
```

Each player has:

```text
Priority
```

For example:

```text
Critical Cutscene
UI Video
World Monitor
Background Decorative Video
```

Under resource pressure, a low-priority player may:

```text
Reduce Queue
Pause Decode
Suspend
```

But it must not affect the Critical Cutscene.

---

# Seventy-Five: Multiple Video Playback

The V1 Framework supports multiple `VideoPlayer` instances.

However, the actual number of simultaneous hardware decoders is:

```text
Platform Capability-dependent
```

Therefore:

```text
VideoManager
↓
Capability / Budget
↓
Hardware Decode
Software Fallback
Suspend
Reject according to policy
```

Do not assume that mobile devices can hardware-decode a large number of videos simultaneously.

---

# Seventy-Six: Preload / Prepare

A cutscene may:

```text
Open
↓
Prepare
↓
Demux ready
↓
Decoder ready
↓
First frame ready
↓
Ready
```

Gameplay calls:

```text
Play()
```

only when actually needed.

This avoids stuttering on the first frame.

---

# Seventy-Seven: Streaming / Buffering

For URL Sources:

```text
Network / Platform Media Source
↓
Buffer
↓
Demux
↓
Decode
```

Runtime provides:

```text
BufferedRange
IsBuffering
```

V1 does not implement a complete adaptive bitrate streaming stack itself.

HLS / DASH:

```text
V2+ / Provider Plugin
```

---

# Seventy-Eight: App Lifecycle

Mobile:

```text
App Background
↓
Video policy
├─ Pause
├─ Suspend Decoder
└─ Release Native Surface
```

Resume:

```text
Restore Decoder
↓
Seek to Saved Media Time if required
↓
Resume according to policy
```

Audio focus changes also follow the Audio / Platform lifecycle.

---

# Seventy-Nine: Device Loss / Surface Loss

The video backend must handle:

```text
GPU Device Loss
Swapchain recreation
Native Surface Loss
App suspend
```

The `VideoPlayer` logical playback state is separated from the native decoder surface lifetime.

---

# Eighty: Video Error Model

Unified:

```text
VideoError
├─ SourceNotFound
├─ UnsupportedContainer
├─ UnsupportedCodec
├─ DecoderUnavailable
├─ NetworkError
├─ DecodeError
├─ GPUImportError
├─ SeekFailed
└─ UnknownBackendError
```

Retain the backend diagnostic code, but do not directly expose the OS-specific error enum to Gameplay.

---

# Eighty-One: Profiler / Debug

The profiler displays:

```text
Active Video Players
Decoder Backend
Hardware / Software Decode
Resolution
FPS
Decode Time
Frame Queue Depth
Dropped Frames
Repeated Frames
A/V Drift
Buffered Time
CPU Copy Count
GPU Upload Bytes
Native Surface Import
Audio Queue
Memory per Player
```

The debug overlay may display:

```text
PTS
Master Clock
Drift
Frame Drop
Backend
Codec
```

---

# Eighty-Two: Video Logging

Log Categories:

```text
Media
Video
VideoDecode
VideoIO
VideoAudio
VideoSubtitle
```

Per-frame spam is prohibited.

The same error must have:

```text
Rate Limit
```

---

# Eighty-Three: Video Hot Reload

Video asset metadata / subtitle / presentation settings:

```text
Editor Hot Reload
✓
```

Native decoder backend binary:

```text
Development restart preferred
```

Replacement of the content of a video source currently being played:

```text
Build New Generation
↓
Current Player pins old generation
↓
New Open uses new generation
```

Maintain the Asset Generation contract.

---

# Eighty-Four: Video and Bundle

```text
VideoAsset
→ normal Asset identity

Cooked video payload
→ Bundle / package content
```

However, large videos should support:

```text
Streamable payload
```

rather than mandatorily loading the entire video into RAM at once.

Bundle residency:

```text
Video metadata
≠ Entire compressed media bytes resident in RAM
```

---

# Eighty-Five: Video Compression / Package Policy

Video is already compressed media.

The Bundle should not mandatorily apply costly whole-file compression again, causing:

```text
The entire large file must be decompressed before playback
```

The Cooker may mark:

```text
MediaPayload
→ Stored / Streamable / Seekable
```

The VFS must allow range read / seek.

---

# Eighty-Six: Video and VFS / Async IO

The video backend requires:

```text
Seekable Stream Interface
Async Read
Range Read
Priority
Cancellation
```

Large local videos are not required to be materialized into RAM at once.

---

# Eighty-Seven: DRM Policy

V1 Core:

```text
DRM
✕
```

If the product requires it in the future:

```text
Media.DRM.Provider.*
```

as a Provider Plugin.

Avoid hard-coding:

```text
FairPlay
Widevine
PlayReady
```

into `Media.Core`.

---

# Eighty-Eight: Video Capture / Recording

V1:

```text
Gameplay Video Recording
✕
Camera Capture
✕
Webcam
✕
Screen Capture Encoding
✕
Live Broadcast
✕
```

These are not necessary parts of the Playback Framework.

They may be separated later into:

```text
Media.Capture
Media.Encoder
Media.Streaming
```

to prevent unlimited V1 scope expansion.

---

# Eighty-Nine: Alpha Video

V1 does not treat alpha video as a portable baseline.

It may support:

```text
Backend / Codec Capability
```

or adopt:

```text
Separate Alpha Track / Texture
```

as a special Render Feature.

Do not claim that the MP4/H.264 baseline inherently provides cross-platform alpha.

---

# Ninety: HDR Video

The V1 architecture reserves:

```text
ColorSpace Metadata
P010 / 10-bit Surface
HDR Metadata
```

However, the V1 release gate does not require complete HDR video output on all platforms.

It may be enabled according to:

```text
Platform Capability
Display HDR
Renderer HDR Path
```

---

# Ninety-One: Video Security

Remote URL:

```text
HTTPS preferred
```

Backend / Platform policy may restrict protocols.

WebView and Video URL trust policies are separated.

Video decoder input is treated as:

```text
Untrusted Media Data
```

The following are required:

```text
Size Limit
Duration sanity
Metadata validation
Backend error isolation
```

For third-party software decoders, platforms that can place them in an independent worker process are recommended and may be strengthened later.

---

# Ninety-Two: Video Plugin ABI

The Backend Plugin public contract uses:

```text
MediaBackendAPI
```

Exposes:

```text
Open
Close
Prepare
Decode / AcquireFrame
Seek
Capability Query
Native Surface Import Descriptor
Audio Packet / PCM Interface
```

Does not expose:

```text
IMFMediaSession*
AVPlayer*
MediaCodec*
AVSampleBuffer*
```

to the Engine Gameplay / UI API.

---

# Ninety-Three: Video Platform Capability

```text
VideoCapabilities
├─ Containers
├─ Codecs
├─ HardwareDecode
├─ MaxResolution
├─ MaxConcurrentDecoders
├─ HDR
├─ ExternalTextureImport
├─ URLPlayback
└─ PlaybackRate
```

Project Settings / Device Profile may select the following based on capabilities:

```text
Preferred Codec
Fallback Asset
Resolution Variant
```

---

# Ninety-Four: Video V1 Build Profiles

### Desktop Client

```text
Media.Core
Media.Video
Media.Backend.WindowsMF or Apple backend
UI.VideoElement optional
```

### Android

```text
Media.Core
Media.Video
Media.Backend.AndroidMediaCodec
UI.VideoElement optional
```

### iOS

```text
Media.Core
Media.Video
Media.Backend.AppleAVFoundation
UI.VideoElement optional
```

### Dedicated Server

```text
Media.*
→ strip
```

### Training Headless

```text
Media.*
→ strip
```

---

# Ninety-Five: Video V1 Definition of Done

The V1 Video Framework is complete only when it has at least:

```text
1. Can play the Cooked local MP4/H.264/AAC baseline.
2. Official backends exist for Windows / Android / iOS / macOS.
3. Video playback does not block the Gameplay Main Thread.
4. Audio tracks pass through the Audio.Core mixer rather than a second audio device.
5. VideoTexture can be used by Runtime UI and Material / World Screen.
6. UI.VideoElement can maintain aspect and support Fit / Fill / Crop.
7. Supports Play / Pause / Stop / Seek / Loop.
8. Has a bounded frame queue and memory budget.
9. Hardware decode / software fallback status can be viewed in the profiler.
10. Supports a subtitles canonical runtime track.
11. App background / resume does not cause a dangling native surface.
12. Large videos can use VFS seek / range read and do not require the entire file to enter RAM.
13. A disabled Media Plugin has zero / near-zero cost in Shipping.
14. DedicatedServer / TrainingHeadless does not include the Video backend.
15. The Video backend does not expose OS native decoder objects to the Gameplay ABI.
16. Asset generation updates do not disrupt the old generation currently being played.
17. Unsupported codec / decoder failure has a unified error model.
18. A/V sync is managed using the media clock rather than render frame count.
19. Shader / GPU conversion supports YUV metadata and does not assume CPU RGBA.
20. Video Import / Transcode tooling is not included in the Runtime build.
```

---

# Ninety-Six: Video V2+ Reservations

The V1 architecture must reserve, but does not require implementation in V1:

```text
HLS
MPEG-DASH
Adaptive Bitrate
DRM Provider
Live Streaming
WebRTC
Video Capture
Video Encoder
Replay Video Export
Advanced HDR
360 / VR Video
Frame Processing Graph
Video Compositing
```

These will be added later through:

```text
Media.Streaming
Media.DRM.*
Media.Capture
Media.Encoder
Media.Compositor
```

without modifying the basic V1 `VideoPlayer` contract.



# Twenty-Nine: Final V1 Principles

```text
Feature exists
≠
Feature shipped
```

```text
Plugin disabled
→ Zero / near-zero shipping cost
```

```text
Build-time selection first
Runtime dynamic loading only where useful
```

This Plugin Contract must be stabilized in V1. V2 / V3 should only add more optional modules rather than overturning it again.


---

# Appendix B — V1 Master Definition of Done

V1 completion must satisfy all of the following simultaneously:

```text
Runtime
✓ Formalized World / Scene / Entity / Transform lifecycle
✓ Multiple / Additive Scene
✓ Persistent Scene
✓ Async Load / Atomic Activation / Safe Unload
✓ Data-oriented Component Storage
✓ System Scheduler / Job / Task Graph
✓ Time / Timer / Event Framework
```

```text
Rendering
✓ DX12 / Vulkan / Metal RHI
✓ RenderGraph
✓ Slang-only canonical shader source
✓ Forward+
✓ PBR / Stylized / Anime / Vegetation / Water / Unlit
✓ Shadow / Probe / PostProcess
✓ LOD / Texture Streaming
✓ HLOD V1
✓ Terrain / Vegetation
✓ VFX
```

```text
Gameplay Framework
✓ Zig Stable C ABI
✓ Hot Reload barrier
✓ Input Raw + Optional Actions
✓ Character Framework
✓ Physics / Jolt
✓ Navigation / Recast-Detour
✓ AI Blackboard / BT / Perception foundation
✓ Animation Graph / GPU Skinning / BAT
✓ Audio
✓ Runtime UI
✓ WebView optional
✓ Video / Media optional
✓ DataTable
✓ Save / Localization
```

```text
World
✓ Grid Streaming Cell
✓ Loose Quadtree spatial index
✓ Room / Portal
✓ Streaming Source / Demand / Budget
✓ Offline HLOD
✓ Large-world coordinate foundation
```

```text
Toolchain
✓ Editor
✓ Prefab / Undo
✓ Reflection / Serialization
✓ Asset Import / Cook
✓ Bundle / Patch / Generation Pinning
✓ Profiler
✓ Crash / Logging
✓ CI / Device Matrix
✓ Project / Package / Settings
```

```text
Modularity
✓ Feature Plugin
✓ Backend Plugin
✓ Editor / Cooker Plugin
✓ Provider Plugin
✓ Third-party Plugin SDK
✓ Stable C Plugin ABI
✓ PluginHost / Service Registry
✓ Bridge Layer
✓ Disabled Plugin = no shipping code / shader / asset / SDK
```

Only after all the above Gates pass may the project proceed to V2.
