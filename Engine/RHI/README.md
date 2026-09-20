# Nexora RHI contracts (V1-M2)

## Completed

- `Shaders/Triangle.slang` is the canonical two-entry-point triangle shader.
- With `NEXORA_ENABLE_SLANG=ON`, the CMake pipeline compiles the source to SPIR-V and Metal
  source everywhere, plus DXIL on Windows (see "DXIL scope" below), emits Slang reflection JSON
  for each target, and normalizes the results into one backend-neutral metadata document.
- The normalized metadata is checked against `TrianglePipelineLayout()` and
  `ComputeLayoutHash()` by the `build.shader_crosscompile` test. The checked-in
  `Shaders/Triangle.reflection.json` remains the reviewable canonical fixture.
- Public RHI headers expose only backend-neutral descriptors, enums, handles, and interfaces.
  Native DX12, Vulkan, and Metal types are not part of the public contract.
- `NEXORA_ENABLE_SLANG` is disabled by default so environments without `slangc` retain the
  existing contract-only build path.
- `Tools/Build/NormalizeShaderReflection.py`'s constant-buffer size lookup has now been run
  against a real `slangc` (2026.18) SPIR-V and Metal reflection JSON, not just written against
  the documented schema: the initial key-name guesses (`uniformSize`/`byteSize`/`size` directly
  on the constant-buffer node) did not match, exactly as the module's original docstring warned
  they might, and were replaced with the byte size real slangc actually reports
  (`elementType.sizes[*].value`, with the flat keys and the container's own `sizes` kept as
  fallbacks). `build.shader_crosscompile` passing is now real end-to-end validation, not just a
  script that has never seen real slangc output.

## DXIL scope

The portable Slang release does not ship `dxcompiler` for Linux or macOS, so `slangc -target dxil`
cannot run there (`error[E00100]: failed to load downstream compiler 'dxc'`) -- this was confirmed
against slangc 2026.18, not assumed. DXIL is therefore only compiled and validated on Windows,
where `dxcompiler` is reliably available; `NEXORA_SLANG_DXIL_OUTPUT` and the crosscompile test's
`DXIL_ARTIFACT` are simply absent on other hosts rather than pointing at a path that was never
produced, and the canonical reflection's `backends` list only ever names backends actually
validated on that host (never a hardcoded `["dxil", "spirv", "msl"]` regardless of what ran).

## Deferred work

- DX12, Vulkan, and Metal `Device` / `CommandList` implementations remain a V1-M3 task.
- This slice validates shader compilation artifacts and reflection layout only; it does not claim
  that a real graphics device executes the triangle.
- SDK-backed runtime execution, pipeline creation, synchronization, and presentation remain
  platform-specific follow-up work.
- The DXIL leg of `build.shader_crosscompile` itself is still unverified on an actual Windows
  runner in this environment (no Windows toolchain available here); only the SPIR-V/Metal path
  was exercised end-to-end.
