include_guard(GLOBAL)

function(nexora_validate_module_graph manifest)
  execute_process(
    COMMAND "${CMAKE_COMMAND}" -DROOT=${PROJECT_SOURCE_DIR} -DMANIFEST=${manifest}
            -P "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateModuleGraph.cmake"
    RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
  if(NOT result EQUAL 0)
    message(FATAL_ERROR "Module graph validation failed:\n${output}${error}")
  endif()
endfunction()
