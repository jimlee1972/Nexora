include(NexoraTarget)
add_library(NexoraRuntime ${NEXORA_MODULE_LIBRARY_TYPE}
  src/GameplayModuleHost.cpp
  src/Runtime.cpp)
add_library(Nexora::Runtime ALIAS NexoraRuntime)
target_include_directories(NexoraRuntime PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraRuntime PUBLIC Nexora::Renderer)
target_compile_definitions(NexoraRuntime PRIVATE NEXORA_RUNTIME_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraRuntime PUBLIC NEXORA_RUNTIME_STATIC=1)
endif()
nexora_apply_defaults(NexoraRuntime)
