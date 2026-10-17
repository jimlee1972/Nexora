set(modern_reference_outputs)
foreach(kind IN ITEMS LongVector LinearAlgebra GroupShared)
  set(source "${PROJECT_SOURCE_DIR}/Shaders/${kind}Reference.slang")
  set(spirv "${shader_output_dir}/${kind}Reference.spv")
  set(metal "${shader_output_dir}/${kind}Reference.metal")
  add_custom_command(OUTPUT "${spirv}" "${metal}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target spirv -profile glsl_450
      -entry computeMain -stage compute -fvk-use-entrypoint-name -o "${spirv}" "${source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target metal -profile sm_6_0
      -entry computeMain -stage compute -o "${metal}" "${source}"
    DEPENDS "${source}" COMMENT "Compile lower-profile ${kind} compute equivalent" VERBATIM)
  list(APPEND modern_reference_outputs "${spirv}" "${metal}")
  if(kind STREQUAL "LongVector")
    set(key LONG_VECTOR)
  elseif(kind STREQUAL "LinearAlgebra")
    set(key LINEAR_ALGEBRA)
  else()
    set(key GROUP_SHARED)
  endif()
  set(NEXORA_SLANG_${key}_REFERENCE_SPIRV_OUTPUT "${spirv}" PARENT_SCOPE)
  set(NEXORA_SLANG_${key}_REFERENCE_METAL_OUTPUT "${metal}" PARENT_SCOPE)
  if(WIN32)
    set(dxil "${shader_output_dir}/${kind}Reference.dxil")
    add_custom_command(OUTPUT "${dxil}"
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target dxil -profile sm_6_0
        -entry computeMain -stage compute -o "${dxil}" "${source}"
      DEPENDS "${source}" COMMENT "Compile baseline DXIL ${kind} equivalent" VERBATIM)
    list(APPEND modern_reference_outputs "${dxil}")
    set(NEXORA_SLANG_${key}_REFERENCE_DXIL_OUTPUT "${dxil}" PARENT_SCOPE)
  endif()
endforeach()
add_custom_target(NexoraModernReferenceSlangArtifacts ALL DEPENDS ${modern_reference_outputs})
add_dependencies(NexoraSlangArtifacts NexoraModernReferenceSlangArtifacts)
