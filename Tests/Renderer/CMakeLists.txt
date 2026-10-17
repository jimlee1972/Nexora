include(NexoraTarget)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_executable(NexoraSubmissionGpuTimingTests SubmissionGpuTimingTests.cpp)
target_link_libraries(NexoraSubmissionGpuTimingTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraSubmissionGpuTimingTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraSubmissionGpuTimingTests)
add_test(NAME renderer.submission_gpu_timing COMMAND NexoraSubmissionGpuTimingTests)
set_tests_properties(renderer.submission_gpu_timing PROPERTIES TIMEOUT 120)
if(NEXORA_ENABLE_NATIVE_BACKENDS AND (WIN32 OR APPLE OR NEXORA_ENABLE_VULKAN_BACKEND))
  target_compile_definitions(NexoraSubmissionGpuTimingTests PRIVATE NEXORA_TEST_NATIVE_GPU_TIMING=1)
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraSubmissionGpuTimingTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    if(WIN32)
      set(timing_environment
        "NEXORA_SLANG_DXIL_VERTEX_PATH=${NEXORA_SLANG_DXIL_VERTEX_OUTPUT}"
        "NEXORA_SLANG_DXIL_FRAGMENT_PATH=${NEXORA_SLANG_DXIL_FRAGMENT_OUTPUT}"
        "NEXORA_SLANG_COMPUTE_DXIL_PATH=${NEXORA_SLANG_COMPUTE_DXIL_OUTPUT}")
    elseif(APPLE)
      set(timing_environment
        "NEXORA_SLANG_METAL_PATH=${NEXORA_SLANG_METAL_OUTPUT}"
        "NEXORA_SLANG_COMPUTE_METAL_PATH=${NEXORA_SLANG_COMPUTE_METAL_OUTPUT}")
    else()
      set(timing_environment
        "NEXORA_SLANG_SPIRV_PATH=${NEXORA_SLANG_SPIRV_OUTPUT}"
        "NEXORA_SLANG_COMPUTE_SPIRV_PATH=${NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT}")
    endif()
    set_property(TEST renderer.submission_gpu_timing APPEND PROPERTY ENVIRONMENT
      ${timing_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  endif()
endif()

add_test(NAME renderer.metal_source_contract
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/MetalSourceContract.py"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/MetalDevice.mm")
add_test(NAME renderer.indirect_range_source_contract
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/IndirectRangeSourceContract.py"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/ValidationDevice.cpp"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/VulkanDevice.cpp"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/D3D12Device.cpp"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/MetalDevice.mm")
add_executable(NexoraDxilArtifactTests DxilArtifactTests.cpp)
target_link_libraries(NexoraDxilArtifactTests PRIVATE Nexora::RHI)
if(WIN32)
  target_compile_definitions(NexoraDxilArtifactTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraDxilArtifactTests)
add_test(NAME renderer.dxil_artifact_readiness COMMAND NexoraDxilArtifactTests)
if(WIN32 AND NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_SLANG)
  add_dependencies(NexoraDxilArtifactTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  set_property(TEST renderer.dxil_artifact_readiness APPEND PROPERTY ENVIRONMENT
    "NEXORA_SLANG_DXIL_VERTEX_PATH=${NEXORA_SLANG_DXIL_VERTEX_OUTPUT}"
    "NEXORA_SLANG_DXIL_FRAGMENT_PATH=${NEXORA_SLANG_DXIL_FRAGMENT_OUTPUT}"
    "NEXORA_SLANG_COMPUTE_DXIL_PATH=${NEXORA_SLANG_COMPUTE_DXIL_OUTPUT}"
    "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
endif()

add_executable(NexoraRendererTests RendererTests.cpp)
target_link_libraries(NexoraRendererTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraRendererTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraRendererTests)
add_test(NAME renderer.contracts COMMAND NexoraRendererTests)

add_executable(NexoraRenderGraphBufferTests RenderGraphBufferTests.cpp)
target_link_libraries(NexoraRenderGraphBufferTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraRenderGraphBufferTests)
add_test(NAME renderer.graph_buffer_dependencies COMMAND NexoraRenderGraphBufferTests)

add_executable(NexoraParallelRenderGraphTests ParallelRenderGraphTests.cpp)
target_link_libraries(NexoraParallelRenderGraphTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraParallelRenderGraphTests)
add_test(NAME renderer.parallel_graph_recording COMMAND NexoraParallelRenderGraphTests)
set_tests_properties(renderer.parallel_graph_recording PROPERTIES TIMEOUT 60)

add_executable(NexoraGraphicsCapabilityTests GraphicsCapabilityTests.cpp)
target_link_libraries(NexoraGraphicsCapabilityTests PRIVATE Nexora::Renderer)
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND TARGET Nexora::Window)
  find_package(X11 REQUIRED)
  target_include_directories(NexoraGraphicsCapabilityTests PRIVATE ${X11_INCLUDE_DIR})
  target_compile_definitions(NexoraGraphicsCapabilityTests PRIVATE NEXORA_TEST_X11_HEADERS=1)
endif()
nexora_apply_defaults(NexoraGraphicsCapabilityTests)
add_test(NAME renderer.graphics_capabilities COMMAND NexoraGraphicsCapabilityTests)

add_executable(NexoraSceneFrameTests SceneFrameTests.cpp)
target_link_libraries(NexoraSceneFrameTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraSceneFrameTests)
add_test(NAME renderer.scene_frame_contracts COMMAND NexoraSceneFrameTests)

add_executable(NexoraMaterialTests MaterialTests.cpp)
target_link_libraries(NexoraMaterialTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraMaterialTests)
add_test(NAME renderer.material_contracts COMMAND NexoraMaterialTests)

add_executable(NexoraGoldenImageTests GoldenImageTests.cpp)
target_link_libraries(NexoraGoldenImageTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraGoldenImageTests)
add_test(NAME renderer.golden_image_acceptance COMMAND NexoraGoldenImageTests)

add_executable(NexoraGPUSceneTests GPUSceneTests.cpp)
target_link_libraries(NexoraGPUSceneTests PRIVATE Nexora::Renderer)
nexora_apply_defaults(NexoraGPUSceneTests)
add_test(NAME renderer.v2_gpu_scene COMMAND NexoraGPUSceneTests)

add_executable(NexoraGPUDrivenTests GPUDrivenPipelineTests.cpp)
target_link_libraries(NexoraGPUDrivenTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraGPUDrivenTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraGPUDrivenTests)
add_test(NAME renderer.v2_gpu_driven COMMAND NexoraGPUDrivenTests)
if(APPLE AND NEXORA_ENABLE_NATIVE_BACKENDS)
  set_tests_properties(renderer.v2_gpu_driven PROPERTIES LABELS "metal;native-acceptance")
endif()

if(NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND AND NOT APPLE)
  add_executable(NexoraVulkanTextureRecordingTests VulkanTextureRecordingTests.cpp)
  target_link_libraries(NexoraVulkanTextureRecordingTests PRIVATE Nexora::Renderer)
  if(WIN32)
    target_compile_definitions(NexoraVulkanTextureRecordingTests PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  nexora_apply_defaults(NexoraVulkanTextureRecordingTests)
  add_test(NAME renderer.vulkan_texture_recording COMMAND NexoraVulkanTextureRecordingTests)
  add_test(NAME renderer.vulkan_graphics_resource_reuse COMMAND NexoraVulkanTextureRecordingTests --resource-reuse)
  set_tests_properties(renderer.vulkan_texture_recording renderer.vulkan_graphics_resource_reuse PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "vulkan;native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraVulkanTextureRecordingTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.vulkan_texture_recording renderer.vulkan_graphics_resource_reuse APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_SPIRV_PATH=${NEXORA_SLANG_SPIRV_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_SPIRV_PATH=${NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT}"
      "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  endif()
  add_executable(NexoraVulkanDescriptorIsolationTests VulkanDescriptorIsolationTests.cpp)
  target_link_libraries(NexoraVulkanDescriptorIsolationTests PRIVATE Nexora::Renderer)
  if(WIN32)
    target_compile_definitions(NexoraVulkanDescriptorIsolationTests PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  nexora_apply_defaults(NexoraVulkanDescriptorIsolationTests)
  add_test(NAME renderer.vulkan_descriptor_isolation COMMAND NexoraVulkanDescriptorIsolationTests)
  add_test(NAME renderer.vulkan_recording_gpu_timing COMMAND NexoraVulkanDescriptorIsolationTests --gpu-timing)
  add_test(NAME renderer.vulkan_recording_resource_reuse COMMAND NexoraVulkanDescriptorIsolationTests --resource-reuse)
  add_test(NAME renderer.vulkan_recording_benchmark COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RecordingBenchmarkReport.py" "$<TARGET_FILE:NexoraVulkanDescriptorIsolationTests>")
  add_test(NAME renderer.vulkan_recording_benchmark_reuse COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RecordingBenchmarkReport.py" "$<TARGET_FILE:NexoraVulkanDescriptorIsolationTests>" --reuse)
  set_tests_properties(renderer.vulkan_descriptor_isolation renderer.vulkan_recording_gpu_timing renderer.vulkan_recording_resource_reuse renderer.vulkan_recording_benchmark renderer.vulkan_recording_benchmark_reuse PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "vulkan;native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraVulkanDescriptorIsolationTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.vulkan_descriptor_isolation renderer.vulkan_recording_gpu_timing renderer.vulkan_recording_resource_reuse renderer.vulkan_recording_benchmark renderer.vulkan_recording_benchmark_reuse APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_SPIRV_PATH=${NEXORA_SLANG_SPIRV_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_SPIRV_PATH=${NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT}"
      "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  endif()
endif()

# One pixel/failure/lifetime oracle for all native color-target recording paths.
if(NEXORA_ENABLE_NATIVE_BACKENDS AND (WIN32 OR APPLE))
  if(WIN32)
    set(graphics_backend d3d12)
    set(graphics_definition NEXORA_TEST_D3D12=1 _CRT_SECURE_NO_WARNINGS)
    set(graphics_environment
      "NEXORA_SLANG_DXIL_VERTEX_PATH=${NEXORA_SLANG_DXIL_VERTEX_OUTPUT}"
      "NEXORA_SLANG_DXIL_FRAGMENT_PATH=${NEXORA_SLANG_DXIL_FRAGMENT_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_DXIL_PATH=${NEXORA_SLANG_COMPUTE_DXIL_OUTPUT}")
  else()
    set(graphics_backend metal)
    set(graphics_definition NEXORA_TEST_METAL=1)
    set(graphics_environment
      "NEXORA_SLANG_METAL_PATH=${NEXORA_SLANG_METAL_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_METAL_PATH=${NEXORA_SLANG_COMPUTE_METAL_OUTPUT}")
  endif()
  add_executable(NexoraNativeTextureRecordingTests VulkanTextureRecordingTests.cpp)
  target_link_libraries(NexoraNativeTextureRecordingTests PRIVATE Nexora::Renderer)
  target_compile_definitions(NexoraNativeTextureRecordingTests PRIVATE ${graphics_definition})
  nexora_apply_defaults(NexoraNativeTextureRecordingTests)
  add_test(NAME renderer.${graphics_backend}_texture_recording COMMAND NexoraNativeTextureRecordingTests)
  if(WIN32)
    add_test(NAME renderer.d3d12_graphics_enhanced_barriers COMMAND NexoraNativeTextureRecordingTests --enhanced-barriers)
    set_tests_properties(renderer.d3d12_graphics_enhanced_barriers PROPERTIES
      SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "d3d12;native-acceptance")
    if(NEXORA_ENABLE_SLANG)
      set_property(TEST renderer.d3d12_graphics_enhanced_barriers APPEND PROPERTY ENVIRONMENT
        ${graphics_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
    endif()
  endif()
  set_tests_properties(renderer.${graphics_backend}_texture_recording PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "${graphics_backend};native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraNativeTextureRecordingTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.${graphics_backend}_texture_recording APPEND PROPERTY ENVIRONMENT
      ${graphics_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  endif()
endif()

# Share the same workload and failure/CPU oracle across native Vulkan and D3D12.
if(NEXORA_ENABLE_NATIVE_BACKENDS AND WIN32)
  add_executable(NexoraD3D12RecordingIsolationTests VulkanDescriptorIsolationTests.cpp)
  target_link_libraries(NexoraD3D12RecordingIsolationTests PRIVATE Nexora::Renderer)
  target_compile_definitions(NexoraD3D12RecordingIsolationTests PRIVATE
    NEXORA_TEST_D3D12=1 _CRT_SECURE_NO_WARNINGS)
  nexora_apply_defaults(NexoraD3D12RecordingIsolationTests)
  add_test(NAME renderer.d3d12_recording_isolation COMMAND NexoraD3D12RecordingIsolationTests)
  add_test(NAME renderer.d3d12_recording_gpu_timing COMMAND NexoraD3D12RecordingIsolationTests --gpu-timing)
  add_test(NAME renderer.d3d12_recording_enhanced_barriers COMMAND NexoraD3D12RecordingIsolationTests --enhanced-barriers)
  add_test(NAME renderer.d3d12_recording_benchmark COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RecordingBenchmarkReport.py" "$<TARGET_FILE:NexoraD3D12RecordingIsolationTests>")
  set_tests_properties(renderer.d3d12_recording_isolation renderer.d3d12_recording_gpu_timing renderer.d3d12_recording_benchmark renderer.d3d12_recording_enhanced_barriers PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "d3d12;native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraD3D12RecordingIsolationTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.d3d12_recording_isolation renderer.d3d12_recording_gpu_timing renderer.d3d12_recording_benchmark renderer.d3d12_recording_enhanced_barriers APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_COMPUTE_DXIL_PATH=${NEXORA_SLANG_COMPUTE_DXIL_OUTPUT}"
      "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  endif()
endif()

if(NEXORA_ENABLE_NATIVE_BACKENDS AND APPLE)
  add_executable(NexoraMetalRecordingIsolationTests VulkanDescriptorIsolationTests.cpp)
  target_link_libraries(NexoraMetalRecordingIsolationTests PRIVATE Nexora::Renderer)
  target_compile_definitions(NexoraMetalRecordingIsolationTests PRIVATE NEXORA_TEST_METAL=1)
  nexora_apply_defaults(NexoraMetalRecordingIsolationTests)
  add_test(NAME renderer.metal_recording_isolation COMMAND NexoraMetalRecordingIsolationTests)
  add_test(NAME renderer.metal_recording_gpu_timing COMMAND NexoraMetalRecordingIsolationTests --gpu-timing)
  add_test(NAME renderer.metal_recording_benchmark COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/RecordingBenchmarkReport.py" "$<TARGET_FILE:NexoraMetalRecordingIsolationTests>")
  add_test(NAME renderer.metal_missing_compute_artifact
    COMMAND NexoraMetalRecordingIsolationTests --missing-compute-artifact)
  set_tests_properties(renderer.metal_missing_compute_artifact PROPERTIES
    TIMEOUT 30 LABELS "metal;native-acceptance"
    ENVIRONMENT "NEXORA_SLANG_COMPUTE_METAL_PATH=${CMAKE_CURRENT_BINARY_DIR}/missing-GPUDriven.metal")
  set_tests_properties(renderer.metal_recording_isolation renderer.metal_recording_gpu_timing renderer.metal_recording_benchmark PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "metal;native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraMetalRecordingIsolationTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.metal_missing_compute_artifact APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_METAL_PATH=${NEXORA_SLANG_METAL_OUTPUT}")
    set_property(TEST renderer.metal_recording_isolation renderer.metal_recording_gpu_timing renderer.metal_recording_benchmark APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_METAL_PATH=${NEXORA_SLANG_METAL_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_METAL_PATH=${NEXORA_SLANG_COMPUTE_METAL_OUTPUT}"
      "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  endif()
endif()

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
    "-DCOMPUTE_METAL_ARTIFACT=${NEXORA_SLANG_COMPUTE_METAL_OUTPUT}"
    "-DCOMMON_SPIRV_ARTIFACT=${NEXORA_SLANG_COMMON_SPIRV_OUTPUT}"
    "-DCOMMON_METAL_ARTIFACT=${NEXORA_SLANG_COMMON_METAL_OUTPUT}"
    "-DSHADER_LIBRARY_SPIRV_ARTIFACT=${NEXORA_SLANG_LIBRARY_SPIRV_OUTPUT}"
    "-DSHADER_LIBRARY_METAL_ARTIFACT=${NEXORA_SLANG_LIBRARY_METAL_OUTPUT}"
    "-DPBR_SPIRV_ARTIFACT=${NEXORA_SLANG_PBR_SPIRV_OUTPUT}"
    "-DPBR_METAL_ARTIFACT=${NEXORA_SLANG_PBR_METAL_OUTPUT}"
    "-DUI_SPIRV_ARTIFACT=${NEXORA_SLANG_UI_SPIRV_OUTPUT}"
    "-DUI_METAL_ARTIFACT=${NEXORA_SLANG_UI_METAL_OUTPUT}"
    "-DREFLECTION_VERIFIER=$<TARGET_FILE:NexoraShaderReflectionContractTests>")
  if(NEXORA_SLANG_DXIL_OUTPUT)
    list(APPEND shader_crosscompile_args "-DDXIL_ARTIFACT=${NEXORA_SLANG_DXIL_OUTPUT}")
    list(APPEND shader_crosscompile_args
      "-DCOMMON_DXIL_ARTIFACT=${NEXORA_SLANG_COMMON_DXIL_OUTPUT}")
    list(APPEND shader_crosscompile_args
      "-DSHADER_LIBRARY_DXIL_ARTIFACT=${NEXORA_SLANG_LIBRARY_DXIL_OUTPUT}"
      "-DPBR_DXIL_ARTIFACT=${NEXORA_SLANG_PBR_DXIL_OUTPUT}"
      "-DUI_DXIL_ARTIFACT=${NEXORA_SLANG_UI_DXIL_OUTPUT}")
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
        "NEXORA_SLANG_DXIL_FRAGMENT_PATH=${NEXORA_SLANG_DXIL_FRAGMENT_OUTPUT}"
        "NEXORA_SLANG_COMPUTE_DXIL_PATH=${NEXORA_SLANG_COMPUTE_DXIL_OUTPUT}")
    endif()
    if(WIN32)
      set_property(TEST renderer.v2_gpu_driven APPEND PROPERTY ENVIRONMENT
        "NEXORA_SLANG_COMPUTE_DXIL_PATH=${NEXORA_SLANG_COMPUTE_DXIL_OUTPUT}"
        "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
    endif()
    if(APPLE)
      set_property(TEST renderer.v2_gpu_driven APPEND PROPERTY ENVIRONMENT
        "NEXORA_SLANG_METAL_PATH=${NEXORA_SLANG_METAL_OUTPUT}"
        "NEXORA_SLANG_COMPUTE_METAL_PATH=${NEXORA_SLANG_COMPUTE_METAL_OUTPUT}"
        "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
    endif()
    # VulkanDevice's constructor requires NEXORA_SLANG_SPIRV_PATH (it loads the
    # compiled shader module unconditionally), so TestNormalPathOnVulkan in
    # GPUDrivenPipelineTests.cpp needs the same environment renderer.contracts
    # gets, or CreateDevice(Vulkan) throws immediately and IsBackendAvailable
    # silently reports Vulkan as absent -- the test would then skip instead of
    # actually exercising the real GPU-driven dispatch/indirect-draw path.
    if(NEXORA_ENABLE_VULKAN_BACKEND AND NOT APPLE)
      set_property(TEST renderer.v2_gpu_driven APPEND PROPERTY ENVIRONMENT
        "NEXORA_SLANG_SPIRV_PATH=${NEXORA_SLANG_SPIRV_OUTPUT}"
        "NEXORA_SLANG_COMPUTE_SPIRV_PATH=${NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT}"
        "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
      # Linux software-rasterizer (Mesa lavapipe) golden only; other backends keep target-host baselines.
      add_executable(NexoraVulkanGoldenTests VulkanGoldenTests.cpp)
      target_link_libraries(NexoraVulkanGoldenTests PRIVATE Nexora::Renderer)
      target_compile_definitions(NexoraVulkanGoldenTests PRIVATE
        "NEXORA_GOLDEN_BASELINE=\"${CMAKE_CURRENT_SOURCE_DIR}/Golden/triangle_vulkan_lavapipe_64x64.rgba\"")
      nexora_apply_defaults(NexoraVulkanGoldenTests)
      add_test(NAME renderer.vulkan_golden_triangle COMMAND NexoraVulkanGoldenTests)
      set_property(TEST renderer.vulkan_golden_triangle APPEND PROPERTY ENVIRONMENT
        "NEXORA_SLANG_SPIRV_PATH=${NEXORA_SLANG_SPIRV_OUTPUT}"
        "NEXORA_SLANG_COMPUTE_SPIRV_PATH=${NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT}"
        "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
    endif()
  endif()
endif()

add_executable(NexoraEnhancedBarrierProjectionTests EnhancedBarrierProjectionTests.cpp)
target_link_libraries(NexoraEnhancedBarrierProjectionTests PRIVATE Nexora::RHI)
target_include_directories(NexoraEnhancedBarrierProjectionTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/RHI/src")
nexora_apply_defaults(NexoraEnhancedBarrierProjectionTests)
add_test(NAME renderer.enhanced_barrier_projection COMMAND NexoraEnhancedBarrierProjectionTests)

# Real bounded native patches and baked fallback, plus always-portable metadata/admission tests.
add_executable(NexoraTessellationTests TessellationTests.cpp)
target_link_libraries(NexoraTessellationTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraTessellationTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraTessellationTests)
add_test(NAME renderer.tessellation COMMAND NexoraTessellationTests)
set_tests_properties(renderer.tessellation PROPERTIES TIMEOUT 120)
if(NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS)
  add_dependencies(NexoraTessellationTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  set_property(TEST renderer.tessellation APPEND PROPERTY ENVIRONMENT "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  if(WIN32)
    set_property(TEST renderer.tessellation APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_DXIL_VERTEX_PATH=${NEXORA_SLANG_DXIL_VERTEX_OUTPUT}"
      "NEXORA_SLANG_DXIL_FRAGMENT_PATH=${NEXORA_SLANG_DXIL_FRAGMENT_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_DXIL_PATH=${NEXORA_SLANG_COMPUTE_DXIL_OUTPUT}"
      "NEXORA_SLANG_TESSELLATION_CONTROL_DXIL_PATH=${NEXORA_SLANG_TESSELLATION_CONTROL_DXIL_OUTPUT}"
      "NEXORA_SLANG_TESSELLATION_EVALUATION_DXIL_PATH=${NEXORA_SLANG_TESSELLATION_EVALUATION_DXIL_OUTPUT}"
      "NEXORA_SLANG_PRETESSELLATED_DXIL_PATH=${NEXORA_SLANG_PRETESSELLATED_DXIL_OUTPUT}"
      "NEXORA_REQUIRE_NATIVE_TESSELLATION=1")
  elseif(APPLE)
    set_property(TEST renderer.tessellation APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_METAL_PATH=${NEXORA_SLANG_METAL_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_METAL_PATH=${NEXORA_SLANG_COMPUTE_METAL_OUTPUT}"
      "NEXORA_SLANG_PRETESSELLATED_METAL_PATH=${NEXORA_SLANG_PRETESSELLATED_METAL_OUTPUT}")
  elseif(NEXORA_ENABLE_VULKAN_BACKEND)
    set_property(TEST renderer.tessellation APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_SPIRV_PATH=${NEXORA_SLANG_SPIRV_OUTPUT}"
      "NEXORA_SLANG_COMPUTE_SPIRV_PATH=${NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT}"
      "NEXORA_SLANG_TESSELLATION_SPIRV_PATH=${NEXORA_SLANG_TESSELLATION_SPIRV_OUTPUT}"
      "NEXORA_SLANG_PRETESSELLATED_SPIRV_PATH=${NEXORA_SLANG_PRETESSELLATED_SPIRV_OUTPUT}"
      "NEXORA_REQUIRE_NATIVE_TESSELLATION=1")
  endif()
endif()

add_executable(NexoraShadingRateTests ShadingRateTests.cpp)
target_link_libraries(NexoraShadingRateTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraShadingRateTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraShadingRateTests)
add_test(NAME renderer.shading_rate COMMAND NexoraShadingRateTests)
set_tests_properties(renderer.shading_rate PROPERTIES TIMEOUT 120)
if(NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS AND
   (WIN32 OR APPLE OR NEXORA_ENABLE_VULKAN_BACKEND))
  add_dependencies(NexoraShadingRateTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  set_property(TEST renderer.shading_rate APPEND PROPERTY ENVIRONMENT
    ${timing_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
endif()

add_executable(NexoraMeshShaderTests MeshShaderTests.cpp)
target_link_libraries(NexoraMeshShaderTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraMeshShaderTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraMeshShaderTests)
add_test(NAME renderer.mesh_shader COMMAND NexoraMeshShaderTests)
set_tests_properties(renderer.mesh_shader PROPERTIES TIMEOUT 120)
if(NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS AND
   (WIN32 OR APPLE OR NEXORA_ENABLE_VULKAN_BACKEND))
  add_dependencies(NexoraMeshShaderTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  set_property(TEST renderer.mesh_shader APPEND PROPERTY ENVIRONMENT
    ${timing_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  if(WIN32)
    set_property(TEST renderer.mesh_shader APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_MESH_DXIL_PATH=${NEXORA_SLANG_MESH_DXIL_OUTPUT}")
  endif()
endif()

if(WIN32 AND NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_SLANG)
  add_test(NAME renderer.shading_rate_enhanced_barriers COMMAND NexoraShadingRateTests --enhanced-barriers)
  add_test(NAME renderer.mesh_shader_enhanced_barriers COMMAND NexoraMeshShaderTests --enhanced-barriers)
  set_tests_properties(renderer.shading_rate_enhanced_barriers renderer.mesh_shader_enhanced_barriers
    PROPERTIES TIMEOUT 120 LABELS "d3d12;native-acceptance")
  set_property(TEST renderer.shading_rate_enhanced_barriers APPEND PROPERTY ENVIRONMENT
    ${timing_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  set_property(TEST renderer.mesh_shader_enhanced_barriers APPEND PROPERTY ENVIRONMENT
    ${timing_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1"
    "NEXORA_SLANG_MESH_DXIL_PATH=${NEXORA_SLANG_MESH_DXIL_OUTPUT}")
endif()

add_executable(NexoraWorkGraphTests WorkGraphTests.cpp)
target_link_libraries(NexoraWorkGraphTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraWorkGraphTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraWorkGraphTests)
add_test(NAME renderer.work_graph COMMAND NexoraWorkGraphTests)
set_tests_properties(renderer.work_graph PROPERTIES TIMEOUT 120)
if(NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS AND
   (WIN32 OR APPLE OR NEXORA_ENABLE_VULKAN_BACKEND))
  add_dependencies(NexoraWorkGraphTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  set_property(TEST renderer.work_graph APPEND PROPERTY ENVIRONMENT
    ${timing_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
endif()
if(NEXORA_ENABLE_D3D12_WORK_GRAPHS)
  add_test(NAME renderer.work_graph_enhanced_barriers COMMAND NexoraWorkGraphTests --enhanced-barriers)
  set_tests_properties(renderer.work_graph_enhanced_barriers PROPERTIES TIMEOUT 120 LABELS "d3d12;native-acceptance")
  set_property(TEST renderer.work_graph_enhanced_barriers APPEND PROPERTY ENVIRONMENT
    ${timing_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  add_dependencies(NexoraWorkGraphTests ${NEXORA_WORK_GRAPH_ARTIFACT_TARGET})
  set_property(TEST renderer.work_graph renderer.work_graph_enhanced_barriers APPEND PROPERTY ENVIRONMENT
    "NEXORA_D3D12_WORK_GRAPH_DXIL_PATH=${NEXORA_WORK_GRAPH_DXIL_OUTPUT}"
    "NEXORA_D3D12_WORK_GRAPH_WRONG_ABI_DXIL_PATH=${NEXORA_WORK_GRAPH_WRONG_ABI_OUTPUT}")
endif()

add_executable(NexoraRayTracingTests RayTracingTests.cpp)
target_link_libraries(NexoraRayTracingTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraRayTracingTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraRayTracingTests)
add_test(NAME renderer.ray_tracing COMMAND NexoraRayTracingTests)
set_tests_properties(renderer.ray_tracing PROPERTIES TIMEOUT 120)
if(NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS AND
   (WIN32 OR APPLE OR NEXORA_ENABLE_VULKAN_BACKEND))
  add_dependencies(NexoraRayTracingTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  if(WIN32)
    set(ray_environment
      "NEXORA_SLANG_RAY_REFERENCE_DXIL_PATH=${NEXORA_SLANG_RAY_REFERENCE_DXIL_OUTPUT}"
      "NEXORA_SLANG_RAY_LIBRARY_DXIL_PATH=${NEXORA_SLANG_RAY_LIBRARY_OUTPUT}"
      "NEXORA_SLANG_RAY_QUERY_DXIL_PATH=${NEXORA_SLANG_RAY_QUERY_OUTPUT}"
      "NEXORA_SLANG_RAY_NEGATIVE_DXIL_PATH=${NEXORA_SLANG_RAY_NEGATIVE_OUTPUT}")
    add_test(NAME renderer.ray_tracing_enhanced_barriers COMMAND NexoraRayTracingTests --enhanced-barriers)
    set_tests_properties(renderer.ray_tracing_enhanced_barriers PROPERTIES TIMEOUT 120 LABELS "d3d12;native-acceptance")
    set_property(TEST renderer.ray_tracing_enhanced_barriers APPEND PROPERTY ENVIRONMENT
      ${timing_environment} ${ray_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  elseif(APPLE)
    set(ray_environment "NEXORA_SLANG_RAY_REFERENCE_METAL_PATH=${NEXORA_SLANG_RAY_REFERENCE_METAL_OUTPUT}")
  else()
    set(ray_environment "NEXORA_SLANG_RAY_REFERENCE_SPIRV_PATH=${NEXORA_SLANG_RAY_REFERENCE_SPIRV_OUTPUT}")
  endif()
  set_property(TEST renderer.ray_tracing APPEND PROPERTY ENVIRONMENT
    ${timing_environment} ${ray_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
endif()

if(NEXORA_ENABLE_D3D12_WORK_GRAPHS)
  add_dependencies(NexoraRayTracingTests ${NEXORA_WORK_GRAPH_ARTIFACT_TARGET})
  set_property(TEST renderer.ray_tracing renderer.ray_tracing_enhanced_barriers APPEND PROPERTY ENVIRONMENT
    "NEXORA_D3D12_WORK_GRAPH_DXIL_PATH=${NEXORA_WORK_GRAPH_DXIL_OUTPUT}")
endif()

add_executable(NexoraSamplerFeedbackTests SamplerFeedbackTests.cpp)
target_link_libraries(NexoraSamplerFeedbackTests PRIVATE Nexora::Renderer)
if(WIN32)
  target_compile_definitions(NexoraSamplerFeedbackTests PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
nexora_apply_defaults(NexoraSamplerFeedbackTests)
add_test(NAME renderer.sampler_feedback COMMAND NexoraSamplerFeedbackTests)
set_tests_properties(renderer.sampler_feedback PROPERTIES TIMEOUT 120)
if(NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_SLANG AND
   (WIN32 OR APPLE OR NEXORA_ENABLE_VULKAN_BACKEND))
  add_dependencies(NexoraSamplerFeedbackTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  if(WIN32)
    set(feedback_environment
      "NEXORA_SLANG_FEEDBACK_WRITE_DXIL_PATH=${NEXORA_SLANG_FEEDBACK_WRITE_OUTPUT}"
      "NEXORA_SLANG_FEEDBACK_PACK_DXIL_PATH=${NEXORA_SLANG_FEEDBACK_PACK_OUTPUT}"
      "NEXORA_SLANG_FEEDBACK_REFERENCE_DXIL_PATH=${NEXORA_SLANG_FEEDBACK_REFERENCE_DXIL_OUTPUT}"
      "NEXORA_SLANG_FEEDBACK_WRONG_IDENTITY_DXIL_PATH=${NEXORA_SLANG_RAY_QUERY_OUTPUT}")
    add_test(NAME renderer.sampler_feedback_enhanced_barriers COMMAND NexoraSamplerFeedbackTests --enhanced-barriers)
    set_tests_properties(renderer.sampler_feedback_enhanced_barriers PROPERTIES TIMEOUT 120 LABELS "d3d12;native-acceptance")
    set_property(TEST renderer.sampler_feedback_enhanced_barriers APPEND PROPERTY ENVIRONMENT
      ${timing_environment} ${feedback_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
  elseif(APPLE)
    set(feedback_environment "NEXORA_SLANG_FEEDBACK_REFERENCE_METAL_PATH=${NEXORA_SLANG_FEEDBACK_REFERENCE_METAL_OUTPUT}")
  else()
    set(feedback_environment "NEXORA_SLANG_FEEDBACK_REFERENCE_SPIRV_PATH=${NEXORA_SLANG_FEEDBACK_REFERENCE_SPIRV_OUTPUT}")
  endif()
  set_property(TEST renderer.sampler_feedback APPEND PROPERTY ENVIRONMENT
    ${timing_environment} ${feedback_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1")
endif()

if(NEXORA_ENABLE_D3D12_WORK_GRAPHS)
  add_dependencies(NexoraSamplerFeedbackTests ${NEXORA_WORK_GRAPH_ARTIFACT_TARGET})
  set_property(TEST renderer.sampler_feedback renderer.sampler_feedback_enhanced_barriers APPEND PROPERTY ENVIRONMENT
    "NEXORA_D3D12_WORK_GRAPH_DXIL_PATH=${NEXORA_WORK_GRAPH_DXIL_OUTPUT}")
endif()

if(NEXORA_ENABLE_D3D12_PREVIEW)
  add_executable(NexoraModernShaderTests ModernShaderTests.cpp)
  target_compile_definitions(NexoraModernShaderTests PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_link_libraries(NexoraModernShaderTests PRIVATE Nexora::Renderer)
  nexora_apply_defaults(NexoraModernShaderTests)
  set_target_properties(NexoraModernShaderTests PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/preview-runtime")
  add_dependencies(NexoraModernShaderTests NexoraModernShaderArtifacts)
  add_custom_command(TARGET NexoraModernShaderTests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:NexoraModernShaderTests> "$<TARGET_FILE_DIR:NexoraModernShaderTests>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${NEXORA_D3D12_PREVIEW_ROOT}/warp/build/native/bin/x64/d3d10warp.dll" "$<TARGET_FILE_DIR:NexoraModernShaderTests>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${NEXORA_D3D12_PREVIEW_ROOT}/agility/build/native/bin/x64" "$<TARGET_FILE_DIR:NexoraModernShaderTests>/Agility"
    COMMAND_EXPAND_LISTS VERBATIM)
  add_test(NAME renderer.modern_shaders_preview COMMAND NexoraModernShaderTests)
  set_tests_properties(renderer.modern_shaders_preview PROPERTIES TIMEOUT 180
    WORKING_DIRECTORY "$<TARGET_FILE_DIR:NexoraModernShaderTests>"
    ENVIRONMENT "NEXORA_D3D12_PREVIEW_SDK_PATH=./Agility/;NEXORA_MODERN_LONG_VECTOR_DXIL_PATH=${PROJECT_BINARY_DIR}/Shaders/Modern/ModernLongVector.dxil;NEXORA_MODERN_LINEAR_ALGEBRA_DXIL_PATH=${PROJECT_BINARY_DIR}/Shaders/Modern/ModernLinearAlgebra.dxil;NEXORA_MODERN_GROUP_SHARED_DXIL_PATH=${PROJECT_BINARY_DIR}/Shaders/Modern/ModernGroupShared.dxil")
endif()

if(NEXORA_ENABLE_D3D12_PREVIEW AND NEXORA_ENABLE_SLANG)
  add_executable(NexoraSamplerFeedbackPreviewTests SamplerFeedbackTests.cpp)
  target_compile_definitions(NexoraSamplerFeedbackPreviewTests PRIVATE
    _CRT_SECURE_NO_WARNINGS NEXORA_TEST_FEEDBACK_PREVIEW=1)
  target_link_libraries(NexoraSamplerFeedbackPreviewTests PRIVATE Nexora::Renderer)
  nexora_apply_defaults(NexoraSamplerFeedbackPreviewTests)
  set_target_properties(NexoraSamplerFeedbackPreviewTests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_BINARY_DIR}/feedback-preview-runtime")
  add_dependencies(NexoraSamplerFeedbackPreviewTests ${NEXORA_SLANG_ARTIFACT_TARGET})
  add_custom_command(TARGET NexoraSamplerFeedbackPreviewTests POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different $<TARGET_RUNTIME_DLLS:NexoraSamplerFeedbackPreviewTests> "$<TARGET_FILE_DIR:NexoraSamplerFeedbackPreviewTests>"
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${NEXORA_D3D12_PREVIEW_ROOT}/warp/build/native/bin/x64/d3d10warp.dll" "$<TARGET_FILE_DIR:NexoraSamplerFeedbackPreviewTests>"
    COMMAND "${CMAKE_COMMAND}" -E copy_directory "${NEXORA_D3D12_PREVIEW_ROOT}/agility/build/native/bin/x64" "$<TARGET_FILE_DIR:NexoraSamplerFeedbackPreviewTests>/Agility"
    COMMAND_EXPAND_LISTS VERBATIM)
  add_test(NAME renderer.sampler_feedback_preview COMMAND NexoraSamplerFeedbackPreviewTests)
  add_test(NAME renderer.sampler_feedback_preview_enhanced COMMAND NexoraSamplerFeedbackPreviewTests --enhanced-barriers)
  set_tests_properties(renderer.sampler_feedback_preview renderer.sampler_feedback_preview_enhanced PROPERTIES
    TIMEOUT 180 WORKING_DIRECTORY "$<TARGET_FILE_DIR:NexoraSamplerFeedbackPreviewTests>")
  set_property(TEST renderer.sampler_feedback_preview renderer.sampler_feedback_preview_enhanced PROPERTY ENVIRONMENT
    ${timing_environment} ${feedback_environment} "NEXORA_REQUIRE_NATIVE_BACKENDS=1"
    "NEXORA_D3D12_PREVIEW_SDK_PATH=./Agility/")
  if(NEXORA_ENABLE_D3D12_WORK_GRAPHS)
    add_dependencies(NexoraSamplerFeedbackPreviewTests ${NEXORA_WORK_GRAPH_ARTIFACT_TARGET})
    set_property(TEST renderer.sampler_feedback_preview renderer.sampler_feedback_preview_enhanced
      APPEND PROPERTY ENVIRONMENT "NEXORA_D3D12_WORK_GRAPH_DXIL_PATH=${NEXORA_WORK_GRAPH_DXIL_OUTPUT}")
  endif()
endif()
