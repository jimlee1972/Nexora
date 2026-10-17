# Optional native D3D12 output under the existing Slang option; other targets use Triangle.
if(WIN32)
  set(mesh_source "${PROJECT_SOURCE_DIR}/Shaders/MeshTriangle.slang")
  set(mesh_dxil "${shader_output_dir}/MeshTriangle.dxil")
  set(mesh_reflection "${mesh_dxil}.reflection.json")
  set(mesh_manifest "${shader_output_dir}/MeshTriangle.reflection.json")
  add_custom_command(OUTPUT "${mesh_dxil}" "${mesh_reflection}" "${mesh_manifest}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" -I "${PROJECT_SOURCE_DIR}/Shaders"
      -target dxil -profile sm_6_5 -entry meshMain -stage mesh
      -reflection-json "${mesh_reflection}" -o "${mesh_dxil}" "${mesh_source}"
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateMeshReflection.py"
      --source "${mesh_source}" --include "${shader_source}"
      --reflection "${mesh_reflection}" --artifact "${mesh_dxil}" --output "${mesh_manifest}"
    DEPENDS "${mesh_source}" "${shader_source}"
      "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateMeshReflection.py"
      "${PROJECT_SOURCE_DIR}/Tools/Build/NormalizeShaderReflection.py"
    COMMENT "Compile and validate bounded D3D12 mesh shader" VERBATIM)
  add_custom_target(NexoraMeshSlangArtifacts ALL
    DEPENDS "${mesh_dxil}" "${mesh_reflection}" "${mesh_manifest}")
  add_dependencies(NexoraSlangArtifacts NexoraMeshSlangArtifacts)
  set(NEXORA_SLANG_MESH_DXIL_OUTPUT "${mesh_dxil}" PARENT_SCOPE)
endif()
