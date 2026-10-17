include(NexoraTarget)

find_package(Python3 REQUIRED COMPONENTS Interpreter)
add_test(NAME renderer.metal_source_contract
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/MetalSourceContract.py"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/MetalDevice.mm")
add_test(NAME renderer.indirect_range_source_contract
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/IndirectRangeSourceContract.py"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/ValidationDevice.cpp"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/VulkanDevice.cpp"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/D3D12Device.cpp"
          "${CMAKE_SOURCE_DIR}/Engine/RHI/src/MetalDevice.mm")
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
  set_tests_properties(renderer.vulkan_texture_recording PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "vulkan;native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraVulkanTextureRecordingTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.vulkan_texture_recording APPEND PROPERTY ENVIRONMENT
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
  set_tests_properties(renderer.vulkan_descriptor_isolation PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "vulkan;native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraVulkanDescriptorIsolationTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.vulkan_descriptor_isolation APPEND PROPERTY ENVIRONMENT
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
  set_tests_properties(renderer.d3d12_recording_isolation PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "d3d12;native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraD3D12RecordingIsolationTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.d3d12_recording_isolation APPEND PROPERTY ENVIRONMENT
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
  add_test(NAME renderer.metal_missing_compute_artifact
    COMMAND NexoraMetalRecordingIsolationTests --missing-compute-artifact)
  set_tests_properties(renderer.metal_missing_compute_artifact PROPERTIES
    TIMEOUT 30 LABELS "metal;native-acceptance"
    ENVIRONMENT "NEXORA_SLANG_COMPUTE_METAL_PATH=${CMAKE_CURRENT_BINARY_DIR}/missing-GPUDriven.metal")
  set_tests_properties(renderer.metal_recording_isolation PROPERTIES
    SKIP_RETURN_CODE 77 TIMEOUT 120 LABELS "metal;native-acceptance")
  if(NEXORA_ENABLE_SLANG)
    add_dependencies(NexoraMetalRecordingIsolationTests ${NEXORA_SLANG_ARTIFACT_TARGET})
    set_property(TEST renderer.metal_missing_compute_artifact APPEND PROPERTY ENVIRONMENT
      "NEXORA_SLANG_METAL_PATH=${NEXORA_SLANG_METAL_OUTPUT}")
    set_property(TEST renderer.metal_recording_isolation APPEND PROPERTY ENVIRONMENT
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
