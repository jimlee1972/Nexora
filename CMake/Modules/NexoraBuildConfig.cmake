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
endfunction()
