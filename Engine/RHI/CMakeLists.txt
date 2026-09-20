include(NexoraTarget)
add_library(NexoraRHI ${NEXORA_MODULE_LIBRARY_TYPE}
  src/ShaderReflection.cpp
  src/ValidationDevice.cpp)
add_library(Nexora::RHI ALIAS NexoraRHI)
target_include_directories(NexoraRHI PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraRHI PUBLIC Nexora::Core)
target_compile_definitions(NexoraRHI PRIVATE NEXORA_RHI_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraRHI PUBLIC NEXORA_RHI_STATIC=1)
endif()
nexora_apply_defaults(NexoraRHI)
