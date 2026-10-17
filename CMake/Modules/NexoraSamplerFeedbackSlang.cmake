set(feedback_source "${PROJECT_SOURCE_DIR}/Shaders/SamplerFeedback.slang")
set(feedback_pack_source "${PROJECT_SOURCE_DIR}/Shaders/PackSamplerFeedback.slang")
set(feedback_reference_source "${PROJECT_SOURCE_DIR}/Shaders/SamplerFeedbackReference.slang")
set(feedback_spirv "${shader_output_dir}/SamplerFeedbackReference.spv")
set(feedback_metal "${shader_output_dir}/SamplerFeedbackReference.metal")
set(feedback_outputs "${feedback_spirv}" "${feedback_metal}")
add_custom_command(OUTPUT "${feedback_spirv}" "${feedback_metal}"
  COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target spirv -profile glsl_450
    -entry computeMain -stage compute -fvk-use-entrypoint-name -o "${feedback_spirv}" "${feedback_reference_source}"
  COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target metal -profile sm_6_0
    -entry computeMain -stage compute -o "${feedback_metal}" "${feedback_reference_source}"
  DEPENDS "${feedback_reference_source}" COMMENT "Compile bounded feedback compute reference" VERBATIM)
if(WIN32)
  set(feedback_write "${shader_output_dir}/SamplerFeedback.dxil")
  set(feedback_pack "${shader_output_dir}/PackSamplerFeedback.dxil")
  set(feedback_reference "${shader_output_dir}/SamplerFeedbackReference.dxil")
  set(feedback_write_reflection "${feedback_write}.reflection.json")
  set(feedback_pack_reflection "${feedback_pack}.reflection.json")
  set(feedback_identity "${shader_output_dir}/SamplerFeedbackArtifactIdentity.h")
  set(feedback_manifest "${shader_output_dir}/SamplerFeedback.manifest.json")
  add_custom_command(OUTPUT "${feedback_write}" "${feedback_pack}" "${feedback_reference}"
    "${feedback_write_reflection}" "${feedback_pack_reflection}" "${feedback_identity}" "${feedback_manifest}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target dxil -profile sm_6_5
      -entry writeFeedback -stage compute -reflection-json "${feedback_write_reflection}"
      -o "${feedback_write}" "${feedback_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target dxil -profile sm_6_0
      -entry packFeedback -stage compute -reflection-json "${feedback_pack_reflection}"
      -o "${feedback_pack}" "${feedback_pack_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -fp-mode precise -target dxil -profile sm_6_0
      -entry computeMain -stage compute -o "${feedback_reference}" "${feedback_reference_source}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateSamplerFeedbackArtifacts.py"
      --write "${feedback_write}" --pack "${feedback_pack}"
      --write-reflection "${feedback_write_reflection}" --pack-reflection "${feedback_pack_reflection}"
      --source "${feedback_source}" --pack-source "${feedback_pack_source}"
      --compiler "${NEXORA_SLANGC_EXECUTABLE}" --header "${feedback_identity}" --manifest "${feedback_manifest}"
    DEPENDS "${feedback_source}" "${feedback_pack_source}" "${feedback_reference_source}"
      "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateSamplerFeedbackArtifacts.py"
    COMMENT "Compile and pin optional sampler feedback/decoded-pack shaders" VERBATIM)
  list(APPEND feedback_outputs "${feedback_write}" "${feedback_pack}" "${feedback_reference}"
    "${feedback_write_reflection}" "${feedback_pack_reflection}" "${feedback_identity}" "${feedback_manifest}")
  set(NEXORA_SLANG_FEEDBACK_WRITE_OUTPUT "${feedback_write}" PARENT_SCOPE)
  set(NEXORA_SLANG_FEEDBACK_PACK_OUTPUT "${feedback_pack}" PARENT_SCOPE)
  set(NEXORA_SLANG_FEEDBACK_REFERENCE_DXIL_OUTPUT "${feedback_reference}" PARENT_SCOPE)
endif()
add_custom_target(NexoraSamplerFeedbackSlangArtifacts ALL DEPENDS ${feedback_outputs})
add_dependencies(NexoraSlangArtifacts NexoraSamplerFeedbackSlangArtifacts)
set(NEXORA_SLANG_FEEDBACK_REFERENCE_SPIRV_OUTPUT "${feedback_spirv}" PARENT_SCOPE)
set(NEXORA_SLANG_FEEDBACK_REFERENCE_METAL_OUTPUT "${feedback_metal}" PARENT_SCOPE)
