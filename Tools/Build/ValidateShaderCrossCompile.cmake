cmake_minimum_required(VERSION 3.25)
if(NOT DEFINED ROOT)
  message(FATAL_ERROR "ROOT is required")
endif()
foreach(var CANONICAL_REFLECTION SPIRV_ARTIFACT METAL_ARTIFACT REFLECTION_VERIFIER)
  if(NOT DEFINED ${var})
    message(FATAL_ERROR "${var} is required")
  endif()
endforeach()

# DXIL is only produced on Windows (see NexoraSlang.cmake); DXIL_ARTIFACT is
# omitted, not just empty, on other hosts.
set(artifacts "${SPIRV_ARTIFACT}" "${METAL_ARTIFACT}" "${CANONICAL_REFLECTION}")
if(DEFINED DXIL_ARTIFACT)
  list(APPEND artifacts "${DXIL_ARTIFACT}")
endif()
foreach(artifact ${artifacts})
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

if(DEFINED DXIL_ARTIFACT)
  message(STATUS "Validated Slang DXIL/SPIR-V/MSL cross-compile and canonical reflection layout")
else()
  message(STATUS "Validated Slang SPIR-V/MSL cross-compile and canonical reflection layout (DXIL is Windows-only)")
endif()
