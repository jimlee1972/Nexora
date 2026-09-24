add_library(NexoraDearImGui STATIC
  "${nexora_imgui_SOURCE_DIR}/imgui.cpp"
  "${nexora_imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${nexora_imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${nexora_imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${nexora_imgui_SOURCE_DIR}/imgui_widgets.cpp")
target_include_directories(NexoraDearImGui SYSTEM PUBLIC "${nexora_imgui_SOURCE_DIR}")
set_target_properties(NexoraDearImGui PROPERTIES POSITION_INDEPENDENT_CODE ON)

include(NexoraTarget)
add_library(NexoraEditorImGui ${NEXORA_MODULE_LIBRARY_TYPE} src/EditorImGui.cpp)
add_library(Nexora::EditorImGui ALIAS NexoraEditorImGui)
target_include_directories(NexoraEditorImGui
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraEditorImGui
  PUBLIC Nexora::Editor Nexora::Presentation Nexora::Window
  PRIVATE NexoraDearImGui)
target_compile_definitions(NexoraEditorImGui PRIVATE NEXORA_EDITOR_IMGUI_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraEditorImGui PUBLIC NEXORA_EDITOR_IMGUI_STATIC=1)
endif()
nexora_apply_defaults(NexoraEditorImGui)
