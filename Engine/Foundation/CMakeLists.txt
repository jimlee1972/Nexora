include(NexoraTarget)

add_library(NexoraFoundation ${NEXORA_MODULE_LIBRARY_TYPE}
  src/BuildInfo.cpp)
add_library(Nexora::Foundation ALIAS NexoraFoundation)

configure_file(
  include/Nexora/Foundation/Version.h.in
  "${CMAKE_CURRENT_BINARY_DIR}/include/Nexora/Foundation/Version.h" @ONLY)

target_include_directories(NexoraFoundation
  PUBLIC
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>"
    "$<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}/include>")
target_compile_definitions(NexoraFoundation PRIVATE NEXORA_FOUNDATION_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraFoundation PUBLIC NEXORA_FOUNDATION_STATIC=1 PRIVATE NEXORA_MONOLITHIC=1)
endif()
nexora_apply_defaults(NexoraFoundation)
