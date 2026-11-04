include_guard(GLOBAL)

# Serialize arguments as CMake bracket literals rather than shell/batch text. The extra first
# newline is discarded by CMake's bracket parser, preserving a leading newline in an argument.
function(_nexora_game_argument output value)
  set(delimiter "=")
  while(TRUE)
    string(FIND "${value}" "]${delimiter}]" collision)
    if(collision EQUAL -1)
      break()
    endif()
    string(APPEND delimiter "=")
  endwhile()
  set(${output} "[${delimiter}[\n${value}]${delimiter}]" PARENT_SCOPE)
endfunction()

function(_nexora_game_append_command script)
  file(APPEND "${script}" "execute_process(\n  COMMAND")
  math(EXPR last "${ARGC} - 1")
  foreach(index RANGE 1 ${last})
    _nexora_game_argument(argument "${ARGV${index}}")
    file(APPEND "${script}" " ${argument}")
  endforeach()
  _nexora_game_argument(directory "${CMAKE_BINARY_DIR}")
  file(APPEND "${script}" "\n  WORKING_DIRECTORY ${directory}\n")
  file(APPEND "${script}" [=[  RESULT_VARIABLE _nexora_game_result
)
if(NOT "${_nexora_game_result}" STREQUAL "0")
  message(FATAL_ERROR "Zig SDK command failed: ${_nexora_game_result}")
endif()
]=])
endfunction()

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
    list(APPEND target_args -target x86_64-windows-msvc)
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
  # Private snapshots have different SDK paths. Bootstrapping `zig build` at each path
  # recompiles its build runner before it can even diagnose an invalid authored script.
  # Compile the same engine-owned entry and both test roots directly. Every module keeps
  # build.zig's target/optimization settings; caches and output remain private to the caller.
  foreach(optimization IN ITEMS ReleaseSafe Debug)
    set(module_args_${optimization}
      --dep nexora --dep game -O${optimization} ${target_args}
      "-Mroot=${sdk_root}/Tools/Zig/game_entry.zig"
      --dep nexora -O${optimization} ${target_args} "-Mgame=${script_path}"
      -O${optimization} ${target_args} "-Mnexora=${sdk_root}/Bindings/Zig/nexora.zig")
  endforeach()
  if(WIN32)
    set(module_file "${GAME_OUTPUT}/bin/${module_name}.dll")
    file(MAKE_DIRECTORY "${GAME_OUTPUT}/bin")
  else()
    set(module_file "${GAME_OUTPUT}/lib/lib${module_name}.so")
    file(MAKE_DIRECTORY "${GAME_OUTPUT}/lib")
  endif()
  set(test_directory "${CMAKE_CURRENT_BINARY_DIR}/nexora-game-tests/${target}")
  file(MAKE_DIRECTORY "${test_directory}")
  # Joining all three expanded SDK commands can make Ninja emit a UTF-8 .bat file. cmd.exe
  # then interprets its Unicode working-directory line through the active code page. Keep the
  # Ninja command short; execute each native argv vector directly through CMake instead.
  string(SHA256 command_id "${target}")
  set(command_script "${CMAKE_CURRENT_BINARY_DIR}/nexora-game-${command_id}.cmake")
  file(WRITE "${command_script}" "# Engine-owned private game build commands.\n")
  _nexora_game_append_command("${command_script}" ${environment} "${GAME_ZIG}"
    build-lib -dynamic -fcompiler-rt --name "${module_name}"
    "-femit-bin=${module_file}" ${module_args_ReleaseSafe})
  _nexora_game_append_command("${command_script}" ${environment} "${GAME_ZIG}" test
    "-femit-bin=${test_directory}/entry${CMAKE_EXECUTABLE_SUFFIX}" ${module_args_Debug})
  _nexora_game_append_command("${command_script}" ${environment} "${GAME_ZIG}" test
    "-femit-bin=${test_directory}/script${CMAKE_EXECUTABLE_SUFFIX}"
    --dep nexora -ODebug ${target_args} "-Mroot=${script_path}"
    -ODebug ${target_args} "-Mnexora=${sdk_root}/Bindings/Zig/nexora.zig")
  _nexora_game_append_command("${command_script}" "${CMAKE_COMMAND}" -E echo
    NEXORA_SCRIPT_TESTS_PASSED)
  add_custom_target(${target}
    COMMAND "${CMAKE_COMMAND}" -P "${command_script}"
    WORKING_DIRECTORY "${CMAKE_BINARY_DIR}" VERBATIM)
endfunction()
