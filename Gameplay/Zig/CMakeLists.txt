find_program(NEXORA_ZIG_EXECUTABLE NAMES zig REQUIRED)

set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample${CMAKE_CXX_OUTPUT_EXTENSION}")
if(NOT CMAKE_CXX_OUTPUT_EXTENSION)
  if(WIN32)
    set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample.obj")
  else()
    set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample.o")
  endif()
endif()

add_custom_command(
  OUTPUT "${NEXORA_ZIG_SAMPLE_OBJECT}"
  COMMAND "${NEXORA_ZIG_EXECUTABLE}" build-obj
          "${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig"
          -O Debug -femit-bin=${NEXORA_ZIG_SAMPLE_OBJECT}
  DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig"
  COMMENT "Building the Zig gameplay ABI sample"
  VERBATIM)

set_source_files_properties("${NEXORA_ZIG_SAMPLE_OBJECT}" PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE)

add_executable(NexoraZigGameplayTests
  ZigGameplayTests.cpp
  "${NEXORA_ZIG_SAMPLE_OBJECT}")
target_link_libraries(NexoraZigGameplayTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraZigGameplayTests)
add_test(NAME gameplay.zig_abi_smoke COMMAND NexoraZigGameplayTests)
