include(NexoraTarget)
add_executable(NexoraRendererTests RendererTests.cpp)
target_link_libraries(NexoraRendererTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraRendererTests)
add_test(NAME renderer.contracts COMMAND NexoraRendererTests)

if(NEXORA_ENABLE_SLANG)
  add_executable(NexoraShaderReflectionContractTests ShaderReflectionCrossCompileTests.cpp)
  target_link_libraries(NexoraShaderReflectionContractTests PRIVATE Nexora::RHI)
  nexora_apply_defaults(NexoraShaderReflectionContractTests)
  add_dependencies(NexoraShaderReflectionContractTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  # DXIL is only produced on Windows (see CMake/Modules/NexoraSlang.cmake);
  # NEXORA_SLANG_DXIL_OUTPUT is empty elsewhere, so DXIL_ARTIFACT is left
  # undefined rather than passed as an empty path.
  set(shader_crosscompile_args
    "-DROOT=${PROJECT_SOURCE_DIR}"
    "-DCANONICAL_REFLECTION=${NEXORA_SLANG_CANONICAL_REFLECTION}"
    "-DSPIRV_ARTIFACT=${NEXORA_SLANG_SPIRV_OUTPUT}"
    "-DMETAL_ARTIFACT=${NEXORA_SLANG_METAL_OUTPUT}"
    "-DREFLECTION_VERIFIER=$<TARGET_FILE:NexoraShaderReflectionContractTests>")
  if(NEXORA_SLANG_DXIL_OUTPUT)
    list(APPEND shader_crosscompile_args "-DDXIL_ARTIFACT=${NEXORA_SLANG_DXIL_OUTPUT}")
  endif()
  add_test(
    NAME build.shader_crosscompile
    COMMAND "${CMAKE_COMMAND}" ${shader_crosscompile_args}
            -P "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateShaderCrossCompile.cmake")
endif()
