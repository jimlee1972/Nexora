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
  set(compute_shader_source "${PROJECT_SOURCE_DIR}/Shaders/GPUDriven.slang")
  set(shader_output_dir "${PROJECT_BINARY_DIR}/Shaders")
  set(spirv_output "${shader_output_dir}/Triangle.spv")
  set(compute_spirv_output "${shader_output_dir}/GPUDriven.spv")
  set(metal_output "${shader_output_dir}/Triangle.metal")
  set(spirv_reflection "${shader_output_dir}/Triangle.spv.reflection.json")
  set(metal_reflection "${shader_output_dir}/Triangle.metal.reflection.json")
  set(canonical_reflection "${shader_output_dir}/Triangle.reflection.json")
  set(normalizer "${PROJECT_SOURCE_DIR}/Tools/Build/NormalizeShaderReflection.py")

  set(cross_compile_outputs
    "${spirv_output}" "${compute_spirv_output}" "${metal_output}" "${spirv_reflection}" "${metal_reflection}"
    "${canonical_reflection}")
  set(normalizer_args
    --source "${shader_source}"
    --output "${canonical_reflection}"
    --spirv-reflection "${spirv_reflection}"
    --metal-reflection "${metal_reflection}")
  set(commands COMMAND "${CMAKE_COMMAND}" -E make_directory "${shader_output_dir}")

  # DXIL needs Microsoft's dxcompiler, which the portable Slang release does
  # not ship for Linux/macOS (see Engine/RHI/README.md): it is only compiled
  # and validated on Windows, where dxcompiler is reliably available.
  if(WIN32)
    set(dxil_output "${shader_output_dir}/Triangle.dxil")
    set(dxil_vertex_output "${shader_output_dir}/Triangle.vertex.dxil")
    set(dxil_fragment_output "${shader_output_dir}/Triangle.fragment.dxil")
    set(compute_dxil_output "${shader_output_dir}/GPUDriven.dxil")
    set(dxil_reflection "${shader_output_dir}/Triangle.dxil.reflection.json")
    list(APPEND cross_compile_outputs "${dxil_output}" "${dxil_vertex_output}"
         "${dxil_fragment_output}" "${compute_dxil_output}" "${dxil_reflection}")
    list(APPEND normalizer_args --dxil-reflection "${dxil_reflection}")
    list(APPEND commands
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}"
              # Multiple entry points produce a DXIL library; SM 6.6 keeps DXC validation enabled.
              -target dxil
              -profile sm_6_6
              -entry vertexMain
              -entry fragmentMain
              -reflection-json "${dxil_reflection}"
              -o "${dxil_output}"
              "${shader_source}")
    list(APPEND commands
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}"
              -target dxil
              -profile sm_6_6
              -entry vertexMain
              -o "${dxil_vertex_output}"
              "${shader_source}"
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}"
              -target dxil
              -profile sm_6_6
              -entry fragmentMain
              -o "${dxil_fragment_output}"
              "${shader_source}"
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}"
              -target dxil
              -profile sm_6_6
              -entry computeMain
              -o "${compute_dxil_output}"
              "${compute_shader_source}")
  endif()

  list(APPEND commands
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
            -target spirv
            -profile glsl_450
            -entry computeMain
            -fvk-use-entrypoint-name
            -o "${compute_spirv_output}"
            "${compute_shader_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}"
            -target metal
            -entry vertexMain
            -entry fragmentMain
            -reflection-json "${metal_reflection}"
            -o "${metal_output}"
            "${shader_source}"
    COMMAND "${Python3_EXECUTABLE}" "${normalizer}" ${normalizer_args})

  add_custom_command(
    OUTPUT ${cross_compile_outputs}
    ${commands}
    DEPENDS "${shader_source}" "${compute_shader_source}" "${normalizer}"
    COMMENT "Compiling and normalizing the canonical Slang triangle"
    VERBATIM)

  add_custom_target(NexoraSlangArtifacts ALL DEPENDS ${cross_compile_outputs})

  set(NEXORA_SLANG_ARTIFACT_TARGET NexoraSlangArtifacts PARENT_SCOPE)
  set(NEXORA_SLANG_DXIL_OUTPUT "${dxil_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_DXIL_VERTEX_OUTPUT "${dxil_vertex_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_DXIL_FRAGMENT_OUTPUT "${dxil_fragment_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_COMPUTE_DXIL_OUTPUT "${compute_dxil_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_SPIRV_OUTPUT "${spirv_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_COMPUTE_SPIRV_OUTPUT "${compute_spirv_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_METAL_OUTPUT "${metal_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_CANONICAL_REFLECTION "${canonical_reflection}" PARENT_SCOPE)
endfunction()
