include(NexoraTarget)
add_library(NexoraRenderer ${NEXORA_MODULE_LIBRARY_TYPE}
  src/FramePipeline.cpp
  src/GPUDrivenPipeline.cpp
  src/GPUScene.cpp
  src/PipelineCache.cpp
  src/RenderGraph.cpp)
add_library(Nexora::Renderer ALIAS NexoraRenderer)
target_include_directories(NexoraRenderer PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraRenderer PUBLIC Nexora::RHI)
target_compile_definitions(NexoraRenderer PRIVATE NEXORA_RENDERER_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraRenderer PUBLIC NEXORA_RENDERER_STATIC=1)
endif()
nexora_apply_defaults(NexoraRenderer)
