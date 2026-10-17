include(NexoraTarget)
add_library(NexoraRHI ${NEXORA_MODULE_LIBRARY_TYPE}
  src/Capabilities.cpp
  src/DxilArtifact.cpp
  src/SpirvArtifact.cpp
  src/ShaderReflection.cpp
  src/ValidationDevice.cpp
  src/DeviceFactory.cpp)

if(WIN32 AND NEXORA_ENABLE_NATIVE_BACKENDS)
  target_sources(NexoraRHI PRIVATE src/D3D12Device.cpp)
  target_link_libraries(NexoraRHI PRIVATE d3d12 dxgi d3dcompiler)
  if(NEXORA_ENABLE_VULKAN_BACKEND)
    target_sources(NexoraRHI PRIVATE src/VulkanDevice.cpp)
    target_include_directories(NexoraRHI PRIVATE "${NEXORA_VULKAN_INCLUDE_DIR}")
  endif()
elseif(APPLE AND NEXORA_ENABLE_NATIVE_BACKENDS)
  enable_language(OBJCXX)
  target_sources(NexoraRHI PRIVATE src/MetalDevice.mm)
  target_compile_options(NexoraRHI PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_link_libraries(NexoraRHI PRIVATE "-framework Metal" "-framework Foundation")
elseif(UNIX AND NOT APPLE AND NEXORA_ENABLE_NATIVE_BACKENDS AND
       NEXORA_ENABLE_VULKAN_BACKEND)
  target_sources(NexoraRHI PRIVATE src/VulkanDevice.cpp)
  target_include_directories(NexoraRHI PRIVATE "${NEXORA_VULKAN_INCLUDE_DIR}")
  target_link_libraries(NexoraRHI PRIVATE ${CMAKE_DL_LIBS})
endif()
if(WIN32 AND NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_SLANG)
  add_dependencies(NexoraRHI NexoraRaySlangArtifacts NexoraSamplerFeedbackSlangArtifacts)
  target_include_directories(NexoraRHI PRIVATE "${PROJECT_BINARY_DIR}/Shaders")
  target_compile_definitions(NexoraRHI PRIVATE NEXORA_HAS_PINNED_RAY_ARTIFACTS=1 NEXORA_HAS_PINNED_FEEDBACK_ARTIFACTS=1)
else()
  target_compile_definitions(NexoraRHI PRIVATE NEXORA_HAS_PINNED_RAY_ARTIFACTS=0 NEXORA_HAS_PINNED_FEEDBACK_ARTIFACTS=0)
endif()
add_library(Nexora::RHI ALIAS NexoraRHI)
target_include_directories(NexoraRHI PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraRHI PUBLIC Nexora::Core)
target_compile_definitions(NexoraRHI PRIVATE NEXORA_RHI_EXPORTS)
target_compile_definitions(NexoraRHI PRIVATE NEXORA_ENABLE_D3D12_WORK_GRAPHS=$<BOOL:${NEXORA_ENABLE_D3D12_WORK_GRAPHS}>)
if(WIN32)
  target_compile_definitions(NexoraRHI PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()
if(NEXORA_ENABLE_NATIVE_BACKENDS)
  target_compile_definitions(NexoraRHI PRIVATE NEXORA_ENABLE_NATIVE_BACKENDS=1)
endif()
if(NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND)
  target_compile_definitions(NexoraRHI PRIVATE NEXORA_ENABLE_VULKAN_BACKEND=1)
endif()
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraRHI PUBLIC NEXORA_RHI_STATIC=1)
endif()
nexora_apply_defaults(NexoraRHI)

if(NEXORA_ENABLE_D3D12_PREVIEW)
  if(NOT WIN32 OR NOT NEXORA_ENABLE_NATIVE_BACKENDS)
    message(FATAL_ERROR "D3D12 preview requires Windows native backends")
  endif()
  set(NEXORA_D3D12_PREVIEW_ROOT "" CACHE PATH "Verified PrepareD3D12Preview.py package directory")
  if(NOT EXISTS "${NEXORA_D3D12_PREVIEW_ROOT}/agility/build/native/include/d3d12.h")
    message(FATAL_ERROR "Run Tools/Build/PrepareD3D12Preview.py and set NEXORA_D3D12_PREVIEW_ROOT")
  endif()
  foreach(preview_file IN ITEMS "agility/build/native/bin/x64/D3D12Core.dll"
      "warp/build/native/bin/x64/d3d10warp.dll" "dxc/bin/x64/dxc.exe" "dxc/inc/hlsl/dx/linalg.h")
    if(NOT EXISTS "${NEXORA_D3D12_PREVIEW_ROOT}/${preview_file}")
      message(FATAL_ERROR "Preview package is incomplete: ${preview_file}")
    endif()
  endforeach()
  target_link_libraries(NexoraRHI PRIVATE dxguid)
  target_include_directories(NexoraRHI BEFORE PRIVATE "${NEXORA_D3D12_PREVIEW_ROOT}/agility/build/native/include")
endif()
target_compile_definitions(NexoraRHI PRIVATE NEXORA_ENABLE_D3D12_PREVIEW=$<BOOL:${NEXORA_ENABLE_D3D12_PREVIEW}>)
if(NEXORA_ENABLE_D3D12_PREVIEW)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(modern_dxc "${NEXORA_D3D12_PREVIEW_ROOT}/dxc/bin/x64/dxc.exe")
  set(modern_output "${PROJECT_BINARY_DIR}/Shaders/Modern")
  add_custom_command(OUTPUT "${modern_output}/ModernShaderArtifactIdentity.h"
    "${modern_output}/ModernLongVector.dxil" "${modern_output}/ModernLinearAlgebra.dxil"
    "${modern_output}/ModernGroupShared.dxil" "${modern_output}/ModernShaders.manifest.json"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/RecordModernShaderArtifacts.py"
      --compiler "${modern_dxc}" --source-root "${PROJECT_SOURCE_DIR}/Shaders"
      --output "${modern_output}" --hlsl-include "${NEXORA_D3D12_PREVIEW_ROOT}/dxc/inc/hlsl"
    DEPENDS "${PROJECT_SOURCE_DIR}/Tools/Build/RecordModernShaderArtifacts.py"
      "${PROJECT_SOURCE_DIR}/Shaders/ModernLongVector.hlsl"
      "${PROJECT_SOURCE_DIR}/Shaders/ModernLinearAlgebra.hlsl"
      "${PROJECT_SOURCE_DIR}/Shaders/ModernGroupShared.hlsl"
    VERBATIM)
  add_custom_target(NexoraModernShaderArtifacts ALL DEPENDS "${modern_output}/ModernShaderArtifactIdentity.h")
  add_dependencies(NexoraRHI NexoraModernShaderArtifacts)
  target_include_directories(NexoraRHI PRIVATE "${modern_output}")
endif()

if(NEXORA_ENABLE_D3D12_SER)
  if(NOT NEXORA_ENABLE_D3D12_PREVIEW OR NOT NEXORA_ENABLE_SLANG)
    message(FATAL_ERROR "SER opt-in requires the isolated preview build and Slang reference/ray artifacts")
  endif()
  set(ser_output "${PROJECT_BINARY_DIR}/Shaders/SER")
  add_custom_command(OUTPUT "${ser_output}/SerArtifactIdentity.h" "${ser_output}/ReorderedRays.dxil"
      "${ser_output}/ReorderedRaysWrongIdentity.dxil" "${ser_output}/ReorderedRays.manifest.json"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/RecordSerArtifacts.py"
      --compiler "${modern_dxc}" --source "${PROJECT_SOURCE_DIR}/Shaders/ReorderedRays.hlsl"
      --output "${ser_output}"
    DEPENDS "${PROJECT_SOURCE_DIR}/Shaders/ReorderedRays.hlsl" "${PROJECT_SOURCE_DIR}/Tools/Build/RecordSerArtifacts.py"
    VERBATIM)
  add_custom_target(NexoraSerShaderArtifacts ALL DEPENDS "${ser_output}/SerArtifactIdentity.h")
  add_dependencies(NexoraRHI NexoraSerShaderArtifacts)
  target_include_directories(NexoraRHI PRIVATE "${ser_output}")
endif()
target_compile_definitions(NexoraRHI PRIVATE NEXORA_ENABLE_D3D12_SER=$<BOOL:${NEXORA_ENABLE_D3D12_SER}>)

if(NEXORA_ENABLE_D3D12_OPACITY_MICROMAPS AND NOT NEXORA_ENABLE_D3D12_SER)
  message(FATAL_ERROR "Opacity micromaps require the pinned preview SER ray recipe")
endif()
target_compile_definitions(NexoraRHI PRIVATE NEXORA_ENABLE_D3D12_OPACITY_MICROMAPS=$<BOOL:${NEXORA_ENABLE_D3D12_OPACITY_MICROMAPS}>)
