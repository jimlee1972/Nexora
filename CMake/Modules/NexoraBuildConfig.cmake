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
  set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Development Shipping)
  if(CMAKE_BUILD_TYPE AND NOT CMAKE_BUILD_TYPE MATCHES "^(Debug|Development|Shipping)$")
    message(FATAL_ERROR "CMAKE_BUILD_TYPE must be Debug, Development, or Shipping")
  endif()

  if(NEXORA_LINK_MODE STREQUAL "Modular")
    set(NEXORA_MODULE_LIBRARY_TYPE SHARED PARENT_SCOPE)
  else()
    set(NEXORA_MODULE_LIBRARY_TYPE STATIC PARENT_SCOPE)
  endif()

  add_compile_definitions(
    $<$<CONFIG:Debug>:NEXORA_BUILD_DEBUG=1>
    $<$<CONFIG:Development>:NEXORA_BUILD_DEVELOPMENT=1>
    $<$<CONFIG:Shipping>:NEXORA_BUILD_SHIPPING=1>)

  if(MSVC)
    add_compile_options(/W4 /WX /permissive- /EHsc)
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
      if(NEXORA_GIT_HEAD_FILE_RESULT EQUAL 0 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${NEXORA_GIT_HEAD_FILE}")
        set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
          "${CMAKE_CURRENT_SOURCE_DIR}/${NEXORA_GIT_HEAD_FILE}")
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
        if(NEXORA_GIT_REF_FILE_RESULT EQUAL 0 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${NEXORA_GIT_REF_FILE}")
          set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/${NEXORA_GIT_REF_FILE}")
        endif()
      endif()
    endif()
  endif()
  set(NEXORA_BUILD_ID "${NEXORA_BUILD_ID}" PARENT_SCOPE)
endfunction()
