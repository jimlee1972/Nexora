include_guard(GLOBAL)

# Shared by command-line projects and future Editor build actions.
function(nexora_add_game_module target)
  cmake_parse_arguments(GAME "" "PROJECT;OUTPUT;ZIG" "" ${ARGN})
  if(NOT GAME_PROJECT OR NOT GAME_OUTPUT OR NOT GAME_ZIG)
    message(FATAL_ERROR "nexora_add_game_module requires PROJECT, OUTPUT and ZIG")
  endif()
  get_filename_component(sdk_root "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/../.." ABSOLUTE)
  get_filename_component(project_directory "${GAME_PROJECT}" DIRECTORY)
  file(READ "${GAME_PROJECT}" project)
  string(JSON schema GET "${project}" schema)
  if(NOT schema EQUAL 1)
    message(FATAL_ERROR "Unsupported Zig game project schema")
  endif()
  string(JSON module_name GET "${project}" module_name)
  string(JSON script GET "${project}" script)
  if(NOT module_name MATCHES "^[A-Za-z][A-Za-z0-9_]*$")
    message(FATAL_ERROR "Invalid Zig game module name")
  endif()
  string(REPLACE "\\" "/" script "${script}")
  if(IS_ABSOLUTE "${script}" OR script MATCHES "(^|/)\\.\\.(/|$)" OR script MATCHES "^[A-Za-z]:")
    message(FATAL_ERROR "Game script must be project-relative without parent traversal")
  endif()
  # Keep Unicode paths textual: some Windows CMake path-normalization APIs crash.
  # Absolute paths and every parent traversal were rejected above.
  set(script_path "${project_directory}/${script}")
  set(project_root "${project_directory}")
  if(NOT EXISTS "${script_path}")
    message(FATAL_ERROR "Game script must exist inside the project")
  endif()
  set(component "${script_path}")
  while(NOT component STREQUAL project_root)
    if(IS_SYMLINK "${component}")
      message(FATAL_ERROR "Game script path must not contain symbolic links")
    endif()
    get_filename_component(component "${component}" DIRECTORY)
  endwhile()
  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${GAME_PROJECT}")
  set(target_args)
  if(WIN32)
    list(APPEND target_args -Dtarget=x86_64-windows-msvc)
  endif()
  set(environment "${CMAKE_COMMAND}" -E env
    "ZIG_GLOBAL_CACHE_DIR=${CMAKE_BINARY_DIR}/zig-global-cache"
    "ZIG_LOCAL_CACHE_DIR=${CMAKE_BINARY_DIR}/zig-local-cache")
  if(APPLE)
    # Reuse the established Zig 0.14 macOS host-linker workaround.
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    find_program(NEXORA_GAME_HOST_CXX NAMES clang++ REQUIRED)
    file(MAKE_DIRECTORY "${GAME_OUTPUT}/lib")
    set(module_args --dep nexora --dep game
      "-Mroot=${sdk_root}/Tools/Zig/game_entry.zig" --dep nexora
      "-Mgame=${script_path}" "-Mnexora=${sdk_root}/Bindings/Zig/nexora.zig")
    set(test_runner "${sdk_root}/Tools/Build/RunZigTestWithHostLinker.py")
    add_custom_target(${target}
      COMMAND ${environment} "${GAME_ZIG}" build-obj -OReleaseSafe
        -fPIC -fcompiler-rt -fno-stack-check -mcpu=baseline ${module_args}
        "-femit-bin=${GAME_OUTPUT}/module.o"
      COMMAND "${NEXORA_GAME_HOST_CXX}" -dynamiclib "${GAME_OUTPUT}/module.o"
        -o "${GAME_OUTPUT}/lib/lib${module_name}.dylib"
      COMMAND ${environment} "${Python3_EXECUTABLE}" "${test_runner}"
        --zig "${GAME_ZIG}" --cxx "${NEXORA_GAME_HOST_CXX}"
        --output-dir "${GAME_OUTPUT}/entry-tests" -- ${module_args}
      COMMAND ${environment} "${Python3_EXECUTABLE}" "${test_runner}"
        --zig "${GAME_ZIG}" --cxx "${NEXORA_GAME_HOST_CXX}"
        --output-dir "${GAME_OUTPUT}/script-tests" -- --dep nexora
        "-Mroot=${script_path}" "-Mnexora=${sdk_root}/Bindings/Zig/nexora.zig"
      COMMAND "${CMAKE_COMMAND}" -E echo NEXORA_SCRIPT_TESTS_PASSED
      WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" VERBATIM)
    return()
  endif()
  set(arguments --build-file "${sdk_root}/Tools/Zig/build.zig"
    "-Dscript=${script_path}" "-Dname=${module_name}" ${target_args})
  add_custom_target(${target}
    COMMAND ${environment} "${GAME_ZIG}" build ${arguments}
      -Doptimize=ReleaseSafe --prefix "${GAME_OUTPUT}"
    COMMAND ${environment} "${GAME_ZIG}" build test ${arguments} --summary all
    COMMAND "${CMAKE_COMMAND}" -E echo NEXORA_SCRIPT_TESTS_PASSED
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" VERBATIM)
endfunction()
