# Included inside nexora_configure_slang: all tools/options are the existing Slang ones.
set(tess_source "${PROJECT_SOURCE_DIR}/Shaders/Tessellation.slang")
set(tess_spirv "${shader_output_dir}/Tessellation.spv")
set(tess_spirv_reflection "${shader_output_dir}/Tessellation.spv.reflection.json")
set(pre_spirv "${shader_output_dir}/PreTessellated.spv")
set(pre_spirv_reflection "${shader_output_dir}/PreTessellated.spv.reflection.json")
set(pre_metal "${shader_output_dir}/PreTessellated.metal")
set(pre_metal_reflection "${shader_output_dir}/PreTessellated.metal.reflection.json")
set(tess_manifest "${shader_output_dir}/Tessellation.reflection.json")
set(tess_outputs "${tess_spirv}" "${tess_spirv_reflection}" "${pre_spirv}"
    "${pre_spirv_reflection}" "${pre_metal}" "${pre_metal_reflection}" "${tess_manifest}")
set(tess_commands
  COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/Shaders"
    -target spirv -profile glsl_450 -entry tessControlMain -entry tessEvaluationMain
    -fvk-use-entrypoint-name -fvk-b-shift 0 0 -reflection-json "${tess_spirv_reflection}"
    -o "${tess_spirv}" "${tess_source}"
  COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/Shaders"
    -target spirv -profile glsl_450 -entry preTessellatedVertexMain
    -fvk-use-entrypoint-name -fvk-b-shift 0 0 -reflection-json "${pre_spirv_reflection}"
    -o "${pre_spirv}" "${tess_source}"
  COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/Shaders"
    -target metal -entry preTessellatedVertexMain -entry fragmentMain
    -reflection-json "${pre_metal_reflection}" -o "${pre_metal}" "${tess_source}")
set(tess_validation_args --source "${tess_source}" --include "${shader_source}"
    --native "${tess_spirv_reflection}" --fallback-spirv "${pre_spirv_reflection}"
    --fallback-metal "${pre_metal_reflection}" --output "${tess_manifest}"
    --artifact "${tess_spirv}" --artifact "${pre_spirv}" --artifact "${pre_metal}")
if(WIN32)
  set(tess_control_dxil "${shader_output_dir}/Tessellation.control.dxil")
  set(tess_evaluation_dxil "${shader_output_dir}/Tessellation.evaluation.dxil")
  set(pre_dxil "${shader_output_dir}/PreTessellated.dxil")
  foreach(entry IN ITEMS control evaluation fallback)
    if(entry STREQUAL "control")
      set(tess_entry tessControlMain)
      set(tess_dxil "${tess_control_dxil}")
    elseif(entry STREQUAL "evaluation")
      set(tess_entry tessEvaluationMain)
      set(tess_dxil "${tess_evaluation_dxil}")
    else()
      set(tess_entry preTessellatedVertexMain)
      set(tess_dxil "${pre_dxil}")
    endif()
    set(tess_dxil_reflection "${tess_dxil}.reflection.json")
    list(APPEND tess_outputs "${tess_dxil}" "${tess_dxil_reflection}")
    list(APPEND tess_commands
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/Shaders"
        -target dxil -profile sm_6_0 -entry "${tess_entry}"
        -reflection-json "${tess_dxil_reflection}" -o "${tess_dxil}" "${tess_source}")
    list(APPEND tess_validation_args "--${entry}-dxil" "${tess_dxil_reflection}"
        --artifact "${tess_dxil}")
  endforeach()
endif()
set(tess_validator "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateTessellationReflection.py")
add_custom_command(OUTPUT ${tess_outputs}
  COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_output_dir}"
  ${tess_commands}
  COMMAND "${Python3_EXECUTABLE}" "${tess_validator}" ${tess_validation_args}
  DEPENDS "${tess_source}" "${shader_source}" "${tess_validator}" "${normalizer}"
  COMMENT "Compiling bounded tessellation stages and baked mesh fallback"
  VERBATIM)
add_custom_target(NexoraTessellationSlangArtifacts ALL DEPENDS ${tess_outputs})
add_dependencies(NexoraSlangArtifacts NexoraTessellationSlangArtifacts)
set(NEXORA_SLANG_TESSELLATION_SPIRV_OUTPUT "${tess_spirv}" PARENT_SCOPE)
set(NEXORA_SLANG_PRETESSELLATED_SPIRV_OUTPUT "${pre_spirv}" PARENT_SCOPE)
set(NEXORA_SLANG_PRETESSELLATED_METAL_OUTPUT "${pre_metal}" PARENT_SCOPE)
set(NEXORA_SLANG_TESSELLATION_CONTROL_DXIL_OUTPUT "${tess_control_dxil}" PARENT_SCOPE)
set(NEXORA_SLANG_TESSELLATION_EVALUATION_DXIL_OUTPUT "${tess_evaluation_dxil}" PARENT_SCOPE)
set(NEXORA_SLANG_PRETESSELLATED_DXIL_OUTPUT "${pre_dxil}" PARENT_SCOPE)
