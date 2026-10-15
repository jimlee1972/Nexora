# Zig 0.14's Mach-O linker cannot resolve libSystem on newer macOS SDKs.
# Keep the real Zig test runner, but let Apple Clang assemble and link it.
function(nexora_add_zig_test name)
  if(APPLE)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)
    set(sysroot_args)
    if(IS_DIRECTORY "${CMAKE_OSX_SYSROOT}")
      list(APPEND sysroot_args --sysroot "${CMAKE_OSX_SYSROOT}")
    endif()
    add_test(NAME "${name}" COMMAND "${Python3_EXECUTABLE}"
      "${PROJECT_SOURCE_DIR}/Tools/Build/RunZigTestWithHostLinker.py"
      --zig "${NEXORA_ZIG_EXECUTABLE}" --cxx "${CMAKE_CXX_COMPILER}"
      --output-dir "${CMAKE_CURRENT_BINARY_DIR}/${name}-host-link" ${sysroot_args}
      -- ${ARGN})
  else()
    add_test(NAME "${name}" COMMAND "${NEXORA_ZIG_EXECUTABLE}" test ${ARGN})
  endif()
endfunction()
