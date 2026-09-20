# Nexora rendering contracts (V1-M2 / V1-M3)

This slice establishes the backend-neutral contracts and a validation backend. It does **not** claim the hardware-backend milestone gates are complete.

## V1-M2 contract

- `Shaders/Triangle.slang` is the canonical shader source.
- `Triangle.reflection.json` is the reviewable, versioned canonical pipeline-layout fixture for
  DXIL, SPIR-V, and MSL contract validation. With `NEXORA_ENABLE_SLANG=ON`, CMake regenerates
  target reflection and checks it against this fixture and the C++ RHI layout hash.
- Public RHI types contain only descriptors, enums, and generational handles. DX12, Vulkan, and Metal native objects must remain in future private backend modules.
- Canonical reflection includes stable resource identity, binding, type, stage visibility, constant byte size, and a deterministic layout hash.

Slang cross-compilation is an optional build-time gate because not every environment has `slangc`.
When enabled, `build.shader_crosscompile` verifies non-empty DXIL, SPIR-V, and MSL outputs,
canonical reflection equality, the C++ layout hash, and the absence of backend-native types in
public RHI headers. This validates compilation output only; hardware triangle execution still
belongs to the future backend/device milestones and is not claimed here.

## V1-M3 contract

`RenderGraph` derives RAW/WAR/WAW dependencies, rejects cycles, topologically orders passes, computes transient lifetimes, and emits state transitions. `PipelineCache` coalesces identical asynchronous requests. The validation device rejects stale resources, invalid transitions, rendering-scope violations, missing pipelines, and presenting a non-Present resource.

Passes must declare each texture exactly once: a texture cannot be both read and written by the
same pass. Unused transients are lifetime-elided and never allocated. Command lists are single-use;
submission while a rendering scope is open, submission to another device, a second submission, or
recording after submission is rejected. Pipeline identity is the complete layout/shader/format key
(debug labels are deliberately excluded), so hash collisions cannot alias cache entries.

The executable test runs `Offscreen -> Main -> Present` through this contract and verifies pass, barrier, draw, submit, and present counts. Real DX12/Vulkan/Metal devices, descriptor allocators, fences, multi-queue synchronization, transient aliasing, and on-screen presentation remain subsequent backend work.

## Ownership and lifetime

`NexoraRenderer -> NexoraRHI -> NexoraCore`. RenderGraph owns transient textures only for one execution and waits idle before releasing them. Imported resources remain caller-owned. A `PipelineCache` must be destroyed before its device and job system; destruction waits outstanding creation jobs and releases cached pipelines.
