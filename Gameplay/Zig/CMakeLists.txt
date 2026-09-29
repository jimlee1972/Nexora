set(NEXORA_ZIG_EXECUTABLE "" CACHE FILEPATH "Path to the Zig compiler")
if(NOT NEXORA_ZIG_EXECUTABLE)
  # An empty cache entry prevents find_program from searching the environment.
  # Drop it first so CI-provisioned Zig installations can be discovered on PATH.
  unset(NEXORA_ZIG_EXECUTABLE CACHE)
  find_program(NEXORA_ZIG_EXECUTABLE NAMES zig)
endif()
if(NOT NEXORA_ZIG_EXECUTABLE)
  message(FATAL_ERROR "Zig compiler was not found; set NEXORA_ZIG_EXECUTABLE to the zig executable path")
endif()
if(NOT EXISTS "${NEXORA_ZIG_EXECUTABLE}")
  message(FATAL_ERROR "NEXORA_ZIG_EXECUTABLE does not exist: ${NEXORA_ZIG_EXECUTABLE}")
endif()

set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample${CMAKE_CXX_OUTPUT_EXTENSION}")
set(NEXORA_ZIG_SHARED_LIBRARY
    "${CMAKE_CURRENT_BINARY_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}NexoraZigGameplay${CMAKE_SHARED_LIBRARY_SUFFIX}")
if(NOT CMAKE_CXX_OUTPUT_EXTENSION)
  if(WIN32)
    set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample.obj")
  else()
    set(NEXORA_ZIG_SAMPLE_OBJECT "${CMAKE_CURRENT_BINARY_DIR}/zig_gameplay_sample.o")
  endif()
endif()

set(NEXORA_ZIG_OBJECT_OPTIMIZATION Debug)
if(CMAKE_BUILD_TYPE STREQUAL "Shipping")
  set(NEXORA_ZIG_OBJECT_OPTIMIZATION ReleaseSafe)
endif()

set(NEXORA_ZIG_BUILD_OBJ_ARGS
  build-obj
  --dep nexora
  -O ${NEXORA_ZIG_OBJECT_OPTIMIZATION}
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
  # Without this, zig build-obj emits non-PIE code on Linux/macOS, which
  # cannot be linked into NexoraZigGameplayTests, a normal PIE executable:
  # on modern toolchains: "relocation R_X86_64_32S against `.rodata.cst16'
  # can not be used when making a PIE object; recompile with -fPIE".
  # Standalone objects also need compiler-rt because build-obj does not bundle
  # __zig_probe_stack with the object by default.
  list(APPEND NEXORA_ZIG_BUILD_OBJ_ARGS -fPIE -fcompiler-rt)
endif()
list(APPEND NEXORA_ZIG_BUILD_OBJ_ARGS
  -Mroot=${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig
  -Mnexora=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig
  -femit-bin=${NEXORA_ZIG_SAMPLE_OBJECT})

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

if(APPLE)
  # Zig 0.14's Mach-O linker cannot resolve libSystem on the macOS CI SDK.
  # The PIC Zig object is already compiled; use the host linker for the dylib.
  add_custom_command(
    OUTPUT "${NEXORA_ZIG_SHARED_LIBRARY}"
    COMMAND "${CMAKE_CXX_COMPILER}" -dynamiclib "${NEXORA_ZIG_SAMPLE_OBJECT}"
            -o "${NEXORA_ZIG_SHARED_LIBRARY}"
    DEPENDS "${NEXORA_ZIG_SAMPLE_OBJECT}"
    COMMENT "Linking the Zig gameplay Development shared library with Apple Clang"
    VERBATIM)
else()
  set(NEXORA_ZIG_BUILD_LIB_ARGS
    build-lib
    --dep nexora
    -O Debug
    -Mroot=${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig
    -Mnexora=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig
    -dynamic -femit-bin=${NEXORA_ZIG_SHARED_LIBRARY})
  if(WIN32)
    list(APPEND NEXORA_ZIG_BUILD_LIB_ARGS -fcompiler-rt)
  endif()
  add_custom_command(
    OUTPUT "${NEXORA_ZIG_SHARED_LIBRARY}"
    COMMAND "${NEXORA_ZIG_EXECUTABLE}" ${NEXORA_ZIG_BUILD_LIB_ARGS}
    DEPENDS
      "${CMAKE_CURRENT_SOURCE_DIR}/src/game_module.zig"
      "${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig"
      "${PROJECT_SOURCE_DIR}/Engine/API/include/nexora/nexora.h"
    COMMENT "Building the Zig gameplay Development shared library"
    VERBATIM)
endif()
add_custom_target(NexoraZigGameplaySharedBuild DEPENDS "${NEXORA_ZIG_SHARED_LIBRARY}")

set(NEXORA_ZIG_SAMPLE_OBJECT "${NEXORA_ZIG_SAMPLE_OBJECT}" PARENT_SCOPE)
set(NEXORA_ZIG_SHARED_LIBRARY "${NEXORA_ZIG_SHARED_LIBRARY}" PARENT_SCOPE)

if(BUILD_TESTING)
  add_executable(NexoraZigGameplayTests
    ZigGameplayTests.cpp
    "${NEXORA_ZIG_SAMPLE_OBJECT}")
  add_dependencies(NexoraZigGameplayTests NexoraZigGameplayBuild)
  target_link_libraries(NexoraZigGameplayTests PRIVATE Nexora::Runtime)
  target_include_directories(NexoraZigGameplayTests PRIVATE "${PROJECT_SOURCE_DIR}/Tests/Gameplay")
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
