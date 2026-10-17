function(nexora_configure_work_graphs)
  if(NOT NEXORA_ENABLE_D3D12_WORK_GRAPHS)
    return()
  endif()
  if(NOT WIN32 OR NOT NEXORA_ENABLE_NATIVE_BACKENDS OR NOT NEXORA_ENABLE_SLANG)
    message(FATAL_ERROR "Work Graphs opt-in requires Windows native D3D12 and the existing Slang compute fallback")
  endif()
  include(CheckCXXSourceCompiles)
  check_cxx_source_compiles("#include <d3d12.h>
    #if !defined(__ID3D12GraphicsCommandList10_INTERFACE_DEFINED__) || \
        !defined(__ID3D12Device5_INTERFACE_DEFINED__) || \
        !defined(__ID3D12StateObjectProperties1_INTERFACE_DEFINED__) || \
        !defined(__ID3D12WorkGraphProperties_INTERFACE_DEFINED__)
    #error Work Graphs SDK interfaces are unavailable
    #endif
    int main() { return 0; }" NEXORA_WINDOWS_WORK_GRAPH_SDK_SUPPORTED)
  if(NOT NEXORA_WINDOWS_WORK_GRAPH_SDK_SUPPORTED)
    message(FATAL_ERROR "Work Graphs opt-in requires SDK list10 interfaces; baseline builds can leave the option off")
  endif()
  file(TO_CMAKE_PATH "$ENV{WindowsSdkVerBinPath}" work_graph_sdk_bin)
  file(TO_CMAKE_PATH "$ENV{WindowsSdkDir}/bin/$ENV{WindowsSDKVersion}" work_graph_sdk_fallback)
  find_program(NEXORA_DXC_EXECUTABLE NAMES dxc.exe dxc
    HINTS "${work_graph_sdk_bin}/x64" "${work_graph_sdk_fallback}/x64"
    DOC "Optional SDK DXC for Work Graph SM6.8 libraries")
  if(NOT NEXORA_DXC_EXECUTABLE)
    message(FATAL_ERROR "Work Graphs opt-in requires SDK dxc.exe or explicit NEXORA_DXC_EXECUTABLE")
  endif()
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  set(work_graph_source "${PROJECT_SOURCE_DIR}/Shaders/WorkGraphVisibleExpansion.hlsl")
  set(work_graph_output "${PROJECT_BINARY_DIR}/Shaders/WorkGraphVisibleExpansion.dxil")
  set(work_graph_manifest "${PROJECT_BINARY_DIR}/Shaders/WorkGraphVisibleExpansion.manifest.json")
  set(work_graph_outputs "${work_graph_output}" "${work_graph_manifest}")
  set(work_graph_negative_command)
  if(BUILD_TESTING)
    set(work_graph_wrong_abi "${PROJECT_BINARY_DIR}/Shaders/WorkGraphWrongAbi.dxil")
    list(APPEND work_graph_outputs "${work_graph_wrong_abi}")
    set(work_graph_negative_command COMMAND "${NEXORA_DXC_EXECUTABLE}" -T lib_6_8 -HV 2021
      -D NEXORA_WORK_GRAPH_WRONG_ABI=1 -Fo "${work_graph_wrong_abi}" "${work_graph_source}")
  endif()
  add_custom_command(OUTPUT ${work_graph_outputs}
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${PROJECT_BINARY_DIR}/Shaders"
    COMMAND "${NEXORA_DXC_EXECUTABLE}" -T lib_6_8 -HV 2021
      -Fo "${work_graph_output}" "${work_graph_source}"
    ${work_graph_negative_command}
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/RecordWorkGraphArtifact.py"
      --source "${work_graph_source}" --artifact "${work_graph_output}"
      --compiler "${NEXORA_DXC_EXECUTABLE}" --output "${work_graph_manifest}"
    DEPENDS "${work_graph_source}" "${PROJECT_SOURCE_DIR}/Tools/Build/RecordWorkGraphArtifact.py"
    COMMENT "Compile optional SDK DXC SM6.8 Work Graph library" VERBATIM)
  add_custom_target(NexoraWorkGraphDxcArtifacts ALL
    DEPENDS ${work_graph_outputs})
  set(NEXORA_WORK_GRAPH_ARTIFACT_TARGET NexoraWorkGraphDxcArtifacts PARENT_SCOPE)
  set(NEXORA_WORK_GRAPH_DXIL_OUTPUT "${work_graph_output}" PARENT_SCOPE)
  set(NEXORA_WORK_GRAPH_WRONG_ABI_OUTPUT "${work_graph_wrong_abi}" PARENT_SCOPE)
endfunction()
