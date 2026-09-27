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
  set(common_shader_source "${PROJECT_SOURCE_DIR}/Shaders/CommonSmoke.slang")
  set(shader_library_source "${PROJECT_SOURCE_DIR}/Shaders/ShaderLibrarySmoke.slang")
  set(pbr_shader_source "${PROJECT_SOURCE_DIR}/Shaders/PbrSmoke.slang")
  set(ui_shader_source "${PROJECT_SOURCE_DIR}/Shaders/UiSmoke.slang")
  set(common_module_sources
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Math.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Color.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Lighting.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Pbr.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Stylized.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Anime.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Vegetation.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Water.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Shadow.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/PostProcess.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Geometry.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/ForwardPlus.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Ui.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Unlit.slang"
    "${PROJECT_SOURCE_DIR}/Shaders/Nexora/Common/Variants.slang")
  set(common_include_args -I "${PROJECT_SOURCE_DIR}/Shaders")
  set(shader_output_dir "${PROJECT_BINARY_DIR}/Shaders")
  set(spirv_output "${shader_output_dir}/Triangle.spv")
  set(compute_spirv_output "${shader_output_dir}/GPUDriven.spv")
  set(metal_output "${shader_output_dir}/Triangle.metal")
  set(spirv_reflection "${shader_output_dir}/Triangle.spv.reflection.json")
  set(metal_reflection "${shader_output_dir}/Triangle.metal.reflection.json")
  set(canonical_reflection "${shader_output_dir}/Triangle.reflection.json")
  set(common_spirv_output "${shader_output_dir}/CommonSmoke.spv")
  set(common_metal_output "${shader_output_dir}/CommonSmoke.metal")
  set(shader_library_spirv_output "${shader_output_dir}/ShaderLibrarySmoke.spv")
  set(shader_library_metal_output "${shader_output_dir}/ShaderLibrarySmoke.metal")
  set(pbr_spirv_output "${shader_output_dir}/PbrSmoke.spv")
  set(pbr_metal_output "${shader_output_dir}/PbrSmoke.metal")
  set(ui_spirv_output "${shader_output_dir}/UiSmoke.spv")
  set(ui_metal_output "${shader_output_dir}/UiSmoke.metal")
  set(normalizer "${PROJECT_SOURCE_DIR}/Tools/Build/NormalizeShaderReflection.py")

  set(cross_compile_outputs
    "${spirv_output}" "${compute_spirv_output}" "${metal_output}" "${spirv_reflection}" "${metal_reflection}"
    "${canonical_reflection}" "${common_spirv_output}" "${common_metal_output}"
    "${shader_library_spirv_output}" "${shader_library_metal_output}"
    "${pbr_spirv_output}" "${pbr_metal_output}"
    "${ui_spirv_output}" "${ui_metal_output}")
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
    set(common_dxil_output "${shader_output_dir}/CommonSmoke.dxil")
    set(shader_library_dxil_output "${shader_output_dir}/ShaderLibrarySmoke.dxil")
    set(pbr_dxil_output "${shader_output_dir}/PbrSmoke.dxil")
    set(ui_dxil_output "${shader_output_dir}/UiSmoke.dxil")
    set(dxil_reflection "${shader_output_dir}/Triangle.dxil.reflection.json")
    list(APPEND cross_compile_outputs "${dxil_output}" "${dxil_vertex_output}"
         "${dxil_fragment_output}" "${compute_dxil_output}" "${dxil_reflection}"
         "${common_dxil_output}" "${shader_library_dxil_output}" "${pbr_dxil_output}"
         "${ui_dxil_output}")
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
    list(APPEND commands
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
              -target dxil
              -profile sm_6_6
              -entry commonMain
              -o "${common_dxil_output}"
              "${common_shader_source}")
    list(APPEND commands
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
              -target dxil
              -profile sm_6_6
              -entry shaderLibraryMain
              -o "${shader_library_dxil_output}"
              "${shader_library_source}"
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
              -target dxil
              -profile sm_6_6
              -entry pbrVertexMain
              -entry pbrFragmentMain
              -o "${pbr_dxil_output}"
              "${pbr_shader_source}"
      COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
              -target dxil
              -profile sm_6_6
              -entry uiVertexMain
              -entry uiFragmentMain
              -o "${ui_dxil_output}"
              "${ui_shader_source}")
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
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
            -target spirv
            -profile glsl_450
            -entry commonMain
            -fvk-use-entrypoint-name
            -o "${common_spirv_output}"
            "${common_shader_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
            -target metal
            -entry commonMain
            -o "${common_metal_output}"
            "${common_shader_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
            -target spirv
            -profile glsl_450
            -entry shaderLibraryMain
            -fvk-use-entrypoint-name
            -o "${shader_library_spirv_output}"
            "${shader_library_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
            -target metal
            -entry shaderLibraryMain
            -o "${shader_library_metal_output}"
            "${shader_library_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
            -target spirv
            -profile glsl_450
            -entry pbrVertexMain
            -entry pbrFragmentMain
            -fvk-use-entrypoint-name
            -o "${pbr_spirv_output}"
            "${pbr_shader_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
            -target metal
            -entry pbrVertexMain
            -entry pbrFragmentMain
            -o "${pbr_metal_output}"
            "${pbr_shader_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
            -target spirv
            -profile glsl_450
            -entry uiVertexMain
            -entry uiFragmentMain
            -fvk-use-entrypoint-name
            -o "${ui_spirv_output}"
            "${ui_shader_source}"
    COMMAND "${NEXORA_SLANGC_EXECUTABLE}" ${common_include_args}
            -target metal
            -entry uiVertexMain
            -entry uiFragmentMain
            -o "${ui_metal_output}"
            "${ui_shader_source}"
    COMMAND "${Python3_EXECUTABLE}" "${normalizer}" ${normalizer_args})

  add_custom_command(
    OUTPUT ${cross_compile_outputs}
    ${commands}
    DEPENDS "${shader_source}" "${compute_shader_source}" "${common_shader_source}"
            "${shader_library_source}" "${pbr_shader_source}" "${ui_shader_source}"
            ${common_module_sources} "${normalizer}"
    COMMENT "Compiling canonical Slang shaders and shared shader library"
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
  set(NEXORA_SLANG_COMMON_SPIRV_OUTPUT "${common_spirv_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_COMMON_METAL_OUTPUT "${common_metal_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_COMMON_DXIL_OUTPUT "${common_dxil_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_LIBRARY_SPIRV_OUTPUT "${shader_library_spirv_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_LIBRARY_METAL_OUTPUT "${shader_library_metal_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_LIBRARY_DXIL_OUTPUT "${shader_library_dxil_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_PBR_SPIRV_OUTPUT "${pbr_spirv_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_PBR_METAL_OUTPUT "${pbr_metal_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_PBR_DXIL_OUTPUT "${pbr_dxil_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_UI_SPIRV_OUTPUT "${ui_spirv_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_UI_METAL_OUTPUT "${ui_metal_output}" PARENT_SCOPE)
  set(NEXORA_SLANG_UI_DXIL_OUTPUT "${ui_dxil_output}" PARENT_SCOPE)
endfunction()
