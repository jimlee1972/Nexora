set(NEXORA_ZIG_EXECUTABLE "" CACHE FILEPATH "Path to the Zig compiler")
if(NOT NEXORA_ZIG_EXECUTABLE)
  find_program(NEXORA_ZIG_EXECUTABLE NAMES zig REQUIRED)
endif()
if(NOT EXISTS "${NEXORA_ZIG_EXECUTABLE}")
  message(FATAL_ERROR "NEXORA_ZIG_EXECUTABLE does not exist: ${NEXORA_ZIG_EXECUTABLE}")
endif()

set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample${CMAKE_CXX_OUTPUT_EXTENSION}")
if(NOT CMAKE_CXX_OUTPUT_EXTENSION)
  if(WIN32)
    set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample.obj")
  else()
    set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample.o")
  endif()
endif()

set(NEXORA_ZIG_BUILD_OBJ_ARGS
  build-obj
  --dep nexora
  -Mroot=${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig
  -Mnexora=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig
  -O Debug -femit-bin=${NEXORA_ZIG_SAMPLE_OBJECT}
  # A plain `zig build-obj` doesn't link in Zig's own compiler-rt, so the
  # stack-check safety instrumentation it emits by default is left calling
  # an undefined `__zig_probe_stack` on Linux/macOS.
  # See https://github.com/ziglang/zig/issues/6817.
  -fno-stack-check)
if(WIN32)
  # -fno-stack-check above covers __zig_probe_stack, but large-stack-frame
  # functions still need a stack-probe thunk regardless of that safety
  # feature; the object references it as ___chkstk_ms (Zig's own name for
  # it), which only exists if Zig's compiler-rt is bundled into the object,
  # since link.exe doesn't provide it the way it would for MSVC-compiled
  # code. See https://github.com/ziglang/zig/issues/15107.
  list(APPEND NEXORA_ZIG_BUILD_OBJ_ARGS -fcompiler-rt)
else()
  # Without this, zig build-obj emits non-PIC code on Linux/macOS, which
  # cannot be linked into NexoraZigGameplayTests -- a normal PIE executable
  # on modern toolchains: "relocation R_X86_64_32S against `.rodata.cst16'
  # can not be used when making a PIE object; recompile with -fPIE".
  list(APPEND NEXORA_ZIG_BUILD_OBJ_ARGS -fPIC)
endif()

add_custom_command(
  OUTPUT "${NEXORA_ZIG_SAMPLE_OBJECT}"
  COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${NEXORA_ZIG_BUILD_OBJ_ARGS}
  DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig"
    "${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig"
    "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/nexora.h"
  COMMENT "Building the Zig gameplay ABI sample"
  VERBATIM)

set_source_files_properties("${NEXORA_ZIG_SAMPLE_OBJECT}" PROPERTIES
  EXTERNAL_OBJECT TRUE
  GENERATED TRUE)

add_custom_target(NexoraZigGameplayBuild DEPENDS "${NEXORA_ZIG_SAMPLE_OBJECT}")

set(NEXORA_ZIG_SAMPLE_OBJECT "${NEXORA_ZIG_SAMPLE_OBJECT}" PARENT_SCOPE)

if(BUILD_TESTING)
  add_executable(NexoraZigGameplayTests
    ZigGameplayTests.cpp
    "${NEXORA_ZIG_SAMPLE_OBJECT}")
  add_dependencies(NexoraZigGameplayTests NexoraZigGameplayBuild)
  target_link_libraries(NexoraZigGameplayTests PRIVATE Nexora::Runtime)
  if(WIN32)
    # Zig's std library calls raw NT syscalls directly (file I/O, debug-info
    # lookup for its panic handler, etc.); zig's own linker driver links
    # ntdll.lib for this automatically, but link.exe invoked from CMake does
    # not. See https://github.com/ziglang/zig/issues/18893.
    target_link_libraries(NexoraZigGameplayTests PRIVATE ntdll)
  endif()
  nexora_apply_defaults(NexoraZigGameplayTests)
  add_test(NAME gameplay.zig_abi_smoke COMMAND NexoraZigGameplayTests)
endif()
