# Nexora RHI contracts (V1-M2)

## Completed

- `Shaders/Triangle.slang` is the canonical two-entry-point triangle shader.
- With `NEXORA_ENABLE_SLANG=ON`, the CMake pipeline compiles the source to DXIL, SPIR-V, and
  Metal source, emits Slang reflection JSON for each target, and normalizes the results into one
  backend-neutral metadata document.
- The normalized metadata is checked against `TrianglePipelineLayout()` and
  `ComputeLayoutHash()` by the `build.shader_crosscompile` test. The checked-in
  `Shaders/Triangle.reflection.json` remains the reviewable canonical fixture.
- Public RHI headers expose only backend-neutral descriptors, enums, handles, and interfaces.
  Native DX12, Vulkan, and Metal types are not part of the public contract.
- `NEXORA_ENABLE_SLANG` is disabled by default so environments without `slangc` retain the
  existing contract-only build path.

## Deferred work

- `Tools/Build/NormalizeShaderReflection.py` locates the constant-buffer size in each backend's
  slangc `-reflection-json` output by searching for a documented-but-unconfirmed set of key names
  (see the module docstring). It has not yet been run against a real `slangc` install; it fails
  loudly rather than reporting a false pass if the key names do not match, but the first CI run
  with a real Slang toolchain should be treated as the actual validation of this parsing logic.
- DX12, Vulkan, and Metal `Device` / `CommandList` implementations remain a V1-M3 task.
- This slice validates shader compilation artifacts and reflection layout only; it does not claim
  that a real graphics device executes the triangle.
- SDK-backed runtime execution, pipeline creation, synchronization, and presentation remain
  platform-specific follow-up work.
