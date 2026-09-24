include(NexoraTarget)
add_library(NexoraEditorCore ${NEXORA_MODULE_LIBRARY_TYPE}
  src/EditorProduction.cpp
  src/EditorWorkspace.cpp)
add_library(Nexora::Editor ALIAS NexoraEditorCore)
target_include_directories(NexoraEditorCore PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraEditorCore PUBLIC Nexora::Runtime)
target_compile_definitions(NexoraEditorCore PRIVATE NEXORA_EDITOR_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraEditorCore PUBLIC NEXORA_EDITOR_STATIC=1)
endif()
nexora_apply_defaults(NexoraEditorCore)
