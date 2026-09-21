include_guard(GLOBAL)

function(nexora_declare_features)
  option(NEXORA_FEATURE_EXAMPLE_PLUGIN "Build the example optional plugin" ON)
  if(NEXORA_FEATURE_EXAMPLE_PLUGIN)
    add_subdirectory("${PROJECT_SOURCE_DIR}/Plugins/Example" "${PROJECT_BINARY_DIR}/Plugins/Example")
  endif()
  option(NEXORA_FEATURE_API_SAMPLES
         "Build the API-M1~M4 Engine API Foundation sample (Samples/Api)" ON)
  if(NEXORA_FEATURE_API_SAMPLES)
    add_subdirectory("${PROJECT_SOURCE_DIR}/Samples/Api" "${PROJECT_BINARY_DIR}/Samples/Api")
  endif()
endfunction()
