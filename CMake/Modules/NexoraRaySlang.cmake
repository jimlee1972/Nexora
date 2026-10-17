# Lower-profile compute fallback and independently selected optional ray profiles.
set(ray_source "${PROJECT_SOURCE_DIR}/Shaders/BoundedRays.slang")
set(ray_reference_source "${PROJECT_SOURCE_DIR}/Shaders/RayReference.slang")
set(ray_spirv "${shader_output_dir}/RayReference.spv")
set(ray_metal "${shader_output_dir}/RayReference.metal")
set(ray_outputs "${ray_spirv}" "${ray_metal}")
add_custom_command(OUTPUT "${ray_spirv}" "${ray_metal}"
  COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target spirv -profile glsl_450
    -entry computeMain -stage compute -fvk-use-entrypoint-name -o "${ray_spirv}" "${ray_reference_source}"
  COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target metal -profile sm_6_0
    -entry computeMain -stage compute -o "${ray_metal}" "${ray_reference_source}"
  DEPENDS "${ray_reference_source}" COMMENT "Compile bounded portable ray reference" VERBATIM)
if(WIN32)
  set(ray_library "${shader_output_dir}/BoundedRays.dxil")
  set(ray_query "${shader_output_dir}/InlineRayQuery.dxil")
  set(ray_reference_dxil "${shader_output_dir}/RayReference.dxil")
  set(ray_library_reflection "${ray_library}.reflection.json")
  set(ray_query_reflection "${ray_query}.reflection.json")
  set(ray_identity "${shader_output_dir}/RayArtifactIdentity.h")
  set(ray_manifest "${shader_output_dir}/BoundedRays.manifest.json")
  set(ray_negative_command)
  if(BUILD_TESTING)
    set(ray_negative "${shader_output_dir}/BoundedRaysWrongIdentity.dxil")
    list(APPEND ray_outputs "${ray_negative}")
    set(ray_negative_command COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target dxil -profile lib_6_3
      -D NEXORA_RAY_IDENTITY_NEGATIVE=1 -entry rayGeneration -stage raygeneration
      -entry rayMiss -stage miss -entry rayClosest -stage closesthit
      -o "${ray_negative}" "${ray_source}")
  endif()
  add_custom_command(OUTPUT "${ray_library}" "${ray_query}" "${ray_reference_dxil}"
      "${ray_library_reflection}" "${ray_query_reflection}" "${ray_identity}" "${ray_manifest}" ${ray_negative}
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target dxil -profile lib_6_3
      -entry rayGeneration -stage raygeneration -entry rayMiss -stage miss -entry rayClosest -stage closesthit
      -reflection-json "${ray_library_reflection}" -o "${ray_library}" "${ray_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target dxil -profile sm_6_5
      -entry inlineQuery -stage compute -reflection-json "${ray_query_reflection}"
      -o "${ray_query}" "${ray_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target dxil -profile sm_6_0
      -entry computeMain -stage compute -o "${ray_reference_dxil}" "${ray_reference_source}"
    ${ray_negative_command}
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateRayArtifacts.py"
      --library "${ray_library}" --query "${ray_query}"
      --library-reflection "${ray_library_reflection}" --query-reflection "${ray_query_reflection}"
      --source "${ray_source}" --compiler "${NEXORA_SLANGC_EXECUTABLE}"
      --header "${ray_identity}" --manifest "${ray_manifest}"
    DEPENDS "${ray_source}" "${ray_reference_source}" "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateRayArtifacts.py"
    COMMENT "Compile independently selected bounded DXR and inline-query artifacts" VERBATIM)
  list(APPEND ray_outputs "${ray_library}" "${ray_query}" "${ray_reference_dxil}" "${ray_identity}" "${ray_manifest}" "${ray_library_reflection}" "${ray_query_reflection}")
  set(NEXORA_RAY_IDENTITY_HEADER "${ray_identity}" PARENT_SCOPE)
  set(NEXORA_SLANG_RAY_NEGATIVE_OUTPUT "${ray_negative}" PARENT_SCOPE)
  set(NEXORA_SLANG_RAY_LIBRARY_OUTPUT "${ray_library}" PARENT_SCOPE)
  set(NEXORA_SLANG_RAY_QUERY_OUTPUT "${ray_query}" PARENT_SCOPE)
  set(NEXORA_SLANG_RAY_REFERENCE_DXIL_OUTPUT "${ray_reference_dxil}" PARENT_SCOPE)
endif()
add_custom_target(NexoraRaySlangArtifacts ALL DEPENDS ${ray_outputs})
add_dependencies(NexoraSlangArtifacts NexoraRaySlangArtifacts)
set(NEXORA_SLANG_RAY_REFERENCE_SPIRV_OUTPUT "${ray_spirv}" PARENT_SCOPE)
set(NEXORA_SLANG_RAY_REFERENCE_METAL_OUTPUT "${ray_metal}" PARENT_SCOPE)
