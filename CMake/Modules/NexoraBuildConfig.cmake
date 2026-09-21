include_guard(GLOBAL)

function(nexora_configure_build)
  set(NEXORA_LINK_MODE "Modular" CACHE STRING "Module linkage: Modular or Monolithic")
  set_property(CACHE NEXORA_LINK_MODE PROPERTY STRINGS Modular Monolithic)
  if(NOT NEXORA_LINK_MODE MATCHES "^(Modular|Monolithic)$")
    message(FATAL_ERROR "NEXORA_LINK_MODE must be Modular or Monolithic")
  endif()

  if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Development CACHE STRING "Build type" FORCE)
  endif()
  if(DEFINED CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE STREQUAL "")
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Development Shipping)
  endif()
  if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Development|Shipping)$")
    message(FATAL_ERROR "CMAKE_BUILD_TYPE must be Debug, Development, or Shipping")
  endif()

  if(CMAKE_BUILD_TYPE STREQUAL "Shipping")
    include(CheckIPOSupported)
    check_ipo_supported(RESULT NEXORA_IPO_SUPPORTED OUTPUT NEXORA_IPO_ERROR)
    if(NOT NEXORA_IPO_SUPPORTED)
      message(FATAL_ERROR "Shipping requires LTO/IPO support: ${NEXORA_IPO_ERROR}")
    endif()
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION TRUE PARENT_SCOPE)
  endif()

  if(NEXORA_LINK_MODE STREQUAL "Modular")
    set(NEXORA_MODULE_LIBRARY_TYPE SHARED PARENT_SCOPE)
  else()
    set(NEXORA_MODULE_LIBRARY_TYPE STATIC PARENT_SCOPE)
  endif()

  if(WIN32)
    # Each module and test executable otherwise lands in its own per-target
    # build directory (Engine/Foundation/, Tests/Core/, ...). Windows has no
    # rpath: a .exe finds a dependency DLL only via its own directory or
    # PATH, so with NEXORA_LINK_MODE=Modular, ctest can't load
    # NexoraFoundation.dll etc. unless every DLL and EXE share one
    # directory. Linux/macOS get an automatic build-tree RPATH from CMake
    # and keep their existing per-module layout.
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin" PARENT_SCOPE)
  endif()

  add_compile_definitions(
    $<$<CONFIG:Debug>:NEXORA_BUILD_DEBUG=1>
    $<$<CONFIG:Development>:NEXORA_BUILD_DEVELOPMENT=1>
    $<$<CONFIG:Shipping>:NEXORA_BUILD_SHIPPING=1>)

  if(MSVC)
    # C4251 ("class needs to have dll-interface") fires on every private
    # STL member of an exported PIMPL-style class (JobHandle::state_,
    # Engine::implementation_, etc.); those members are never touched
    # across the DLL boundary directly, only through the class's own
    # exported methods, and every Modular target here is built by the same
    # compiler/runtime in one job, so the mismatch this warns about cannot
    # actually occur.
    # Runtime tests intentionally exercise UTF-8 text, IME composition, and
    # localized UI strings.  Do not let the machine's active code page change
    # their meaning (or turn otherwise valid UTF-8 source into C4819/C2001).
    add_compile_options(/W4 /WX /wd4251 /permissive- /EHsc /utf-8)
  else()
    add_compile_options(-Wall -Wextra -Wpedantic -Werror)
  endif()

  set(NEXORA_BUILD_ID "unknown")
  find_package(Git QUIET)
  if(GIT_FOUND)
    execute_process(
      COMMAND "${GIT_EXECUTABLE}" rev-parse --short=12 HEAD
      WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
      OUTPUT_VARIABLE NEXORA_GIT_COMMIT
      OUTPUT_STRIP_TRAILING_WHITESPACE
      RESULT_VARIABLE NEXORA_GIT_RESULT
      ERROR_QUIET)
    if(NEXORA_GIT_RESULT EQUAL 0 AND NOT NEXORA_GIT_COMMIT STREQUAL "")
      set(NEXORA_BUILD_ID "${NEXORA_GIT_COMMIT}")

      # Reconfigure automatically when HEAD moves (checkout/commit) so a
      # plain `cmake --build` after switching commits regenerates
      # Version.h instead of reporting a stale build id.
      execute_process(
        COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE NEXORA_GIT_HEAD_FILE
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE NEXORA_GIT_HEAD_FILE_RESULT
        ERROR_QUIET)
      if(NEXORA_GIT_HEAD_FILE_RESULT EQUAL 0)
        # git rev-parse --git-path returns an absolute path from a linked
        # worktree (its HEAD lives under the main repo's .git/worktrees/),
        # and a path relative to the source dir otherwise.
        if(IS_ABSOLUTE "${NEXORA_GIT_HEAD_FILE}")
          set(NEXORA_GIT_HEAD_PATH "${NEXORA_GIT_HEAD_FILE}")
        else()
          set(NEXORA_GIT_HEAD_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${NEXORA_GIT_HEAD_FILE}")
        endif()
        if(EXISTS "${NEXORA_GIT_HEAD_PATH}")
          set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${NEXORA_GIT_HEAD_PATH}")
        endif()
      endif()

      execute_process(
        COMMAND "${GIT_EXECUTABLE}" symbolic-ref -q HEAD
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        OUTPUT_VARIABLE NEXORA_GIT_SYMBOLIC_REF
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE NEXORA_GIT_SYMREF_RESULT
        ERROR_QUIET)
      if(NEXORA_GIT_SYMREF_RESULT EQUAL 0)
        execute_process(
          COMMAND "${GIT_EXECUTABLE}" rev-parse --git-path "${NEXORA_GIT_SYMBOLIC_REF}"
          WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
          OUTPUT_VARIABLE NEXORA_GIT_REF_FILE
          OUTPUT_STRIP_TRAILING_WHITESPACE
          RESULT_VARIABLE NEXORA_GIT_REF_FILE_RESULT
          ERROR_QUIET)
        if(NEXORA_GIT_REF_FILE_RESULT EQUAL 0)
          if(IS_ABSOLUTE "${NEXORA_GIT_REF_FILE}")
            set(NEXORA_GIT_REF_PATH "${NEXORA_GIT_REF_FILE}")
          else()
            set(NEXORA_GIT_REF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/${NEXORA_GIT_REF_FILE}")
          endif()
          if(EXISTS "${NEXORA_GIT_REF_PATH}")
            set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${NEXORA_GIT_REF_PATH}")
          endif()
        endif()
      endif()
    endif()
  endif()
  set(NEXORA_BUILD_ID "${NEXORA_BUILD_ID}" PARENT_SCOPE)
endfunction()
