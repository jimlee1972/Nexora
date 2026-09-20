cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()
foreach(var CANONICAL_REFLECTION DXIL_ARTIFACT SPIRV_ARTIFACT METAL_ARTIFACT REFLECTION_VERIFIER)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "${var} is required")
  endif()
endforeach()

foreach(artifact "${DXIL_ARTIFACT}" "${SPIRV_ARTIFACT}" "${METAL_ARTIFACT}" "${CANONICAL_REFLECTION}")
  if(NOT EXISTS "${artifact}")
    message(FATAL_ERROR "Expected slang cross-compile artifact does not exist: ${artifact}")
  endif()
  file(SIZE "${artifact}" artifact_size)
  if(artifact_size EQUAL 0)
    message(FATAL_ERROR "Slang cross-compile artifact is empty: ${artifact}")
  endif()
endforeach()

execute_process(
  COMMAND "${REFLECTION_VERIFIER}" "${CANONICAL_REFLECTION}"
  RESULT_VARIABLE verifier_result
  OUTPUT_VARIABLE verifier_output
  ERROR_VARIABLE verifier_error)
if(NOT verifier_result EQUAL 0)
  message(FATAL_ERROR
    "Canonical reflection does not match TrianglePipelineLayout():\n${verifier_output}${verifier_error}")
endif()

message(STATUS "Validated Slang DXIL/SPIR-V/MSL cross-compile and canonical reflection layout")
