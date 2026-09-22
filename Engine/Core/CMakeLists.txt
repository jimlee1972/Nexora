include(NexoraTarget)
add_library(NexoraCore ${NEXORA_MODULE_LIBRARY_TYPE}
  src/Engine.cpp
  src/JobSystem.cpp
  src/Log.cpp
  src/Memory.cpp
  src/Platform.cpp
  src/Services.cpp
  src/TaskGraph.cpp
  src/Time.cpp
  src/Timer.cpp
  src/Vfs.cpp)
add_library(Nexora::Core ALIAS NexoraCore)
target_include_directories(NexoraCore PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraCore PUBLIC Nexora::Foundation)
target_compile_definitions(NexoraCore PRIVATE NEXORA_CORE_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraCore PUBLIC NEXORA_CORE_STATIC=1)
endif()
if(NEXORA_ENABLE_MIMALLOC)
  target_compile_definitions(NexoraCore PRIVATE NEXORA_USE_MIMALLOC=1)
  target_link_libraries(NexoraCore PRIVATE ${NEXORA_MIMALLOC_TARGET})
endif()
nexora_apply_defaults(NexoraCore)
