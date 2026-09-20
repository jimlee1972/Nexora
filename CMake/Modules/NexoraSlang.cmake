include_guard(GLOBAL)

function(nexora_configure_slang)
  if(NOT NEXORA_ENABLE_SLANG)
    return()
  endif()

  find_program(NEXORA_SLANGC_EXECUTABLE NAMES slangc)
  if(NOT NEXORA_SLANGC_EXECUTABLE)
    message(FATAL_ERROR
      "NEXORA_ENABLE_SLANG=ON but slangc was not found. Install a Slang binary distribution "
      "and add its bin directory to PATH, or set NEXORA_SLANGC_EXECUTABLE.")
  endif()

  find_package(Python3 COMPONENTS Interpreter REQUIRED)

  set(shader_source "${PROJECT_SOURCE_DIR}/Shaders/Triangle.slang")
  set(shader_output_dir "${PROJECT_BINARY_DIR}/Shaders")
  set(dxil_output "${shader_output_dir}/Triangle.dxil")
  set(spirv_output "${shader_output_dir}/Triangle.spv")
  set(metal_output "${shader_output_dir}/Triangle.metal")
  set(dxil_reflection "${shader_output_dir}/Triangle.dxil.reflection.json")
  set(spirv_reflection "${shader_output_dir}/Triangle.spv.reflection.json")
  set(metal_reflection "${shader_output_dir}/Triangle.metal.reflection.json")
  set(canonical_reflection "${shader_output_dir}/Triangle.reflection.json")
  set(normalizer "${PROJECT_SOURCE_DIR}/Tools/Build/NormalizeShaderReflection.py")

  add_custom_command(
    OUTPUT
      "${dxil_output}"
      "${spirv_output}"
      "${metal_output}"
      "${dxil_reflection}"
      "${spirv_reflection}"
      "${metal_reflection}"
      "${canonical_reflection}"
    COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_output_dir}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}"
            # Multiple entry points produce a DXIL library; SM 6.6 keeps DXC validation enabled.
            -target dxil
            -profile sm_6_6
            -entry vertexMain
            -entry fragmentMain
            -reflection-json "${dxil_reflection}"
            -o "${dxil_output}"
            "${shader_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}"
            -target spirv
            -profile glsl_450
            -entry vertexMain
            -entry fragmentMain
            -fvk-use-entrypoint-name
            -reflection-json "${spirv_reflection}"
            -o "${spirv_output}"
            "${shader_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}"
            -target metal
            -entry vertexMain
            -entry fragmentMain
            -reflection-json "${metal_reflection}"
            -o "${metal_output}"
            "${shader_source}"
    COMMAND "${Python3_EXECUTABLE}"
            "${normalizer}"
            --source "${shader_source}"
            --output "${canonical_reflection}"
            --dxil-reflection "${dxil_reflection}"
            --spirv-reflection "${spirv_reflection}"
            --metal-reflection "${metal_reflection}"
    DEPENDS "${shader_source}" "${normalizer}"
    COMMENT "Compiling and normalizing the canonical Slang triangle"
    VERBATIM)

  add_custom_target(NexoraSlangArtifacts ALL
    DEPENDS
      "${dxil_output}"
      "${spirv_output}"
      "${metal_output}"
      "${canonical_reflection}")

  set(NEXORA_SLANG_ARTIFACT_TARGET NexoraSlangArtifacts PARENT_SCOPE)
  set(NEXORA_SLANG_DXIL_OUTPUT "${dxil_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_SPIRV_OUTPUT "${spirv_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_METAL_OUTPUT "${metal_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_CANONICAL_REFLECTION "${canonical_reflection}" PARENT_SCOPE)
endfunction()
