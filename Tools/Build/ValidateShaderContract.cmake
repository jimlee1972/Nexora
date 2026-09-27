cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()
set(metadata "${ROOT}/Shaders/Triangle.reflection.json")
file(READ "${metadata}" reflection)
foreach(field schema_version source entry_points bindings backends layout_hash)
  string(JSON value ERROR_VARIABLE error GET "${reflection}" "${field}")
  if(error)
    message(FATAL_ERROR "${metadata}: missing or invalid '${field}'")
  endif()
endforeach()
string(JSON schema GET "${reflection}" schema_version)
if(NOT schema EQUAL 1)
  message(FATAL_ERROR "${metadata}: unsupported schema version")
endif()
string(JSON layout_hash GET "${reflection}" layout_hash)
if(NOT layout_hash MATCHES "^[0-9]+$")
  message(FATAL_ERROR "${metadata}: layout_hash must be an unsigned integer")
endif()
string(JSON backend_count LENGTH "${reflection}" backends)
set(required dxil spirv msl)
set(found)
if(backend_count GREATER 0)
  math(EXPR last "${backend_count} - 1")
  foreach(index RANGE ${last})
    string(JSON backend GET "${reflection}" backends ${index})
    list(APPEND found "${backend}")
  endforeach()
endif()
foreach(backend IN LISTS required)
  if(NOT backend IN_LIST found)
    message(FATAL_ERROR "${metadata}: missing required backend '${backend}'")
  endif()
endforeach()
message(STATUS "Validated canonical triangle shader contract")

set(compute_shader "${ROOT}/Shaders/GPUDriven.slang")
file(READ "${compute_shader}" compute_source)
foreach(symbol computeMain computeCandidates computeVisible computeIndirect computeStatistics)
  string(FIND "${compute_source}" "${symbol}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "${compute_shader}: missing required compute contract '${symbol}'")
  endif()
endforeach()
message(STATUS "Validated GPU-driven compute shader contract")

set(common_module "${ROOT}/Shaders/Nexora/Common.slang")
set(common_smoke "${ROOT}/Shaders/CommonSmoke.slang")
set(shader_library_smoke "${ROOT}/Shaders/ShaderLibrarySmoke.slang")
set(pbr_smoke "${ROOT}/Shaders/PbrSmoke.slang")
set(ui_smoke "${ROOT}/Shaders/UiSmoke.slang")
file(READ "${common_module}" common_module_source)
file(READ "${common_smoke}" common_smoke_source)
file(READ "${shader_library_smoke}" shader_library_smoke_source)
file(READ "${pbr_smoke}" pbr_smoke_source)
file(READ "${ui_smoke}" ui_smoke_source)
foreach(module_symbol IN ITEMS
    "module \"Nexora.Common\";"
    "__include \"Nexora/Common/Math.slang\";"
    "__include \"Nexora/Common/Color.slang\";"
    "__include \"Nexora/Common/Lighting.slang\";"
    "__include \"Nexora/Common/Pbr.slang\";"
    "__include \"Nexora/Common/Stylized.slang\";"
    "__include \"Nexora/Common/Anime.slang\";"
    "__include \"Nexora/Common/Vegetation.slang\";"
    "__include \"Nexora/Common/Water.slang\";"
    "__include \"Nexora/Common/Shadow.slang\";"
    "__include \"Nexora/Common/PostProcess.slang\";"
    "__include \"Nexora/Common/Geometry.slang\";"
    "__include \"Nexora/Common/ForwardPlus.slang\";"
    "__include \"Nexora/Common/Ui.slang\";"
    "__include \"Nexora/Common/Unlit.slang\";"
    "__include \"Nexora/Common/Variants.slang\";")
  string(FIND "${common_module_source}" "${module_symbol}" position)
  if(position EQUAL -1)
    message(FATAL_ERROR "${common_module}: missing shared module contract '${module_symbol}'")
  endif()
endforeach()
file(GLOB common_sources "${ROOT}/Shaders/Nexora/Common/*.slang")
foreach(symbol IN ITEMS
    NexoraSafeNormalize
    NexoraSrgbToLinear
    NexoraLinearToSrgb
    NexoraRec709Luminance
    NexoraAcesApproximate
    NexoraFresnelSchlick
    NexoraEvaluateDirectBrdf)
  set(symbol_found FALSE)
  foreach(source IN LISTS common_sources)
    file(READ "${source}" source_contents)
    string(FIND "${source_contents}" "${symbol}" position)
    if(NOT position EQUAL -1)
      set(symbol_found TRUE)
      break()
    endif()
  endforeach()
  if(NOT symbol_found)
    message(FATAL_ERROR "Nexora.Common: missing reusable shader function '${symbol}'")
  endif()
  string(FIND "${common_smoke_source}" "${symbol}(" smoke_position)
  if(smoke_position EQUAL -1)
    message(FATAL_ERROR
      "${common_smoke}: shared function '${symbol}' is not covered by the compile smoke")
  endif()
endforeach()
string(FIND "${common_smoke_source}" "import Nexora.Common;" import_position)
if(import_position EQUAL -1)
  message(FATAL_ERROR "${common_smoke}: must import the canonical Nexora.Common module")
endif()

file(GLOB shader_library_sources "${ROOT}/Shaders/Nexora/Common/*.slang")
set(shader_library_smokes "${shader_library_smoke_source}\n${pbr_smoke_source}\n${ui_smoke_source}")
foreach(symbol IN ITEMS
    NexoraDecodeTangentNormal
    NexoraComputePbrF0
    NexoraFresnelSchlickRoughness
    NexoraEvaluatePbrIbl
    NexoraEvaluatePbr
    NexoraEvaluateAlphaTest
    NexoraEvaluateStylizedRamp
    NexoraApplyStylizedResponse
    NexoraEvaluateStylizedPbr
    NexoraEvaluateAnimeRamp
    NexoraEvaluateFaceSdfShadow
    NexoraEvaluateAnimeDiffuse
    NexoraEvaluateAnimeHairHighlight
    NexoraEvaluateAnimeRim
    NexoraExtrudeOutlinePosition
    NexoraEvaluateVegetationWind
    NexoraEvaluateVegetationAlpha
    NexoraEvaluateVegetationTransmission
    NexoraEvaluateWaterDepthFade
    NexoraEvaluateWaterColor
    NexoraEvaluateWaterFresnel
    NexoraEvaluateWaterReflection
    NexoraEvaluateWaterSurface
    NexoraApplyShadowBias
    NexoraEvaluatePcf4
    NexoraSelectShadowCascade
    NexoraApplyShadowTint
    NexoraEvaluateSsao
    NexoraApplyExposure
    NexoraApplyBloom
    NexoraApplyColorGrade
    NexoraApplyDistanceFog
    NexoraEvaluateFxaa
    NexoraSkinPosition
    NexoraSkinNormal
    NexoraTransformInstance
    NexoraEvaluatePointLightAttenuation
    NexoraSelectForwardPlusDepthSlice
    NexoraTransformUiPosition
    NexoraEvaluateNineSliceUv
    NexoraCompositeUiColor
    NexoraEvaluateUnlit
    NexoraEvaluateUnlitColor
    NexoraMakeShaderVariantKey)
  set(symbol_found FALSE)
  foreach(source IN LISTS shader_library_sources)
    file(READ "${source}" source_contents)
    string(FIND "${source_contents}" "${symbol}" position)
    if(NOT position EQUAL -1)
      set(symbol_found TRUE)
      break()
    endif()
  endforeach()
  if(NOT symbol_found)
    message(FATAL_ERROR "Nexora.Common: missing reusable shader function '${symbol}'")
  endif()
  string(FIND "${shader_library_smokes}" "${symbol}(" smoke_position)
  if(smoke_position EQUAL -1)
    message(FATAL_ERROR
      "Nexora.Common: shared function '${symbol}' is not covered by a compile smoke")
  endif()
endforeach()

foreach(shader_contract IN ITEMS
    "${shader_library_smoke}|shaderLibraryMain|compute"
    "${pbr_smoke}|pbrVertexMain|vertex"
    "${pbr_smoke}|pbrFragmentMain|fragment"
    "${ui_smoke}|uiVertexMain|vertex"
    "${ui_smoke}|uiFragmentMain|fragment")
  string(REPLACE "|" ";" contract_parts "${shader_contract}")
  list(GET contract_parts 0 source)
  list(GET contract_parts 1 entry)
  list(GET contract_parts 2 stage)
  file(READ "${source}" contract_source)
  string(FIND "${contract_source}" "${entry}" entry_position)
  if(entry_position EQUAL -1)
    message(FATAL_ERROR "${source}: missing ${stage} entry point '${entry}'")
  endif()
endforeach()
foreach(resource_contract IN ITEMS "${pbr_smoke}|TextureCube" "${pbr_smoke}|Texture2DArray"
                                  "${pbr_smoke}|pbrEmissionTexture"
                                  "${ui_smoke}|NexoraUiFrame"
                                  "${shader_library_smoke}|StructuredBuffer"
                                  "${shader_library_smoke}|RWStructuredBuffer"
                                  "${shader_library_smoke}|Texture2D<float>")
  string(REPLACE "|" ";" resource_parts "${resource_contract}")
  list(GET resource_parts 0 source)
  list(GET resource_parts 1 resource)
  file(READ "${source}" resource_source)
  string(FIND "${resource_source}" "${resource}" resource_position)
  if(resource_position EQUAL -1)
    message(FATAL_ERROR "${source}: missing shader resource contract '${resource}'")
  endif()
endforeach()
message(STATUS "Validated PBR, stylized, anime, vegetation, water, post, geometry, Forward+, UI, Unlit, and variant shader contracts")
