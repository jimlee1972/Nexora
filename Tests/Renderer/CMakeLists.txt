include(NexoraTarget)
add_executable(NexoraRendererTests RendererTests.cpp)
target_link_libraries(NexoraRendererTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraRendererTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraRendererTests)
add_test(NAME renderer.contracts COMMAND NexoraRendererTests)

add_executable(NexoraGPUSceneTests GPUSceneTests.cpp)
target_link_libraries(NexoraGPUSceneTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraGPUSceneTests)
add_test(NAME renderer.v2_gpu_scene COMMAND NexoraGPUSceneTests)

add_executable(NexoraGPUDrivenTests GPUDrivenPipelineTests.cpp)
target_link_libraries(NexoraGPUDrivenTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraGPUDrivenTests)
add_test(NAME renderer.v2_gpu_driven COMMAND NexoraGPUDrivenTests)

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
  if(NEXORA_ENABLE_NATIVE_BACKENDS)
    set_property(TEST renderer.contracts APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_SPIRV_PATH=${NEXORA_SLANG_SPIRV_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_SPIRV_PATH=${NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT}"
      "NEXORA_SLANG_METAL_PATH=${NEXORA_SLANG_METAL_OUTPUT}"
      "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
    if(WIN32)
      set_property(TEST renderer.contracts APPEND PROPERTY ENVIRONMENT
        "NEXORA_SLANG_DXIL_VERTEX_PATH=${NEXORA_SLANG_DXIL_VERTEX_OUTPUT}"
        "NEXORA_SLANG_DXIL_FRAGMENT_PATH=${NEXORA_SLANG_DXIL_FRAGMENT_OUTPUT}")
    endif()
    # VulkanDevice's constructor requires NEXORA_SLANG_SPIRV_PATH (it loads the
    # compiled shader module unconditionally), so TestNormalPathOnVulkan in
    # GPUDrivenPipelineTests.cpp needs the same environment renderer.contracts
    # gets, or CreateDevice(Vulkan) throws immediately and IsBackendAvailable
    # silently reports Vulkan as absent -- the test would then skip instead of
    # actually exercising the real GPU-driven dispatch/indirect-draw path.
    set_property(TEST renderer.v2_gpu_driven APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_SPIRV_PATH=${NEXORA_SLANG_SPIRV_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_SPIRV_PATH=${NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT}"
      "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  endif()
endif()
