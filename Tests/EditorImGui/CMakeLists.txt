add_executable(NexoraEditorImGuiTests EditorImGuiTests.cpp)
target_include_directories(NexoraEditorImGuiTests
  PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraEditorImGuiTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraEditorImGuiTests)
add_test(NAME editor.imgui_contract COMMAND NexoraEditorImGuiTests)

if(WIN32)
  add_executable(NexoraEditorWindowsDpiImeTests WindowsDpiImeTests.cpp)
  target_include_directories(NexoraEditorWindowsDpiImeTests
    PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
  target_link_libraries(NexoraEditorWindowsDpiImeTests
    PRIVATE Nexora::EditorImGui imm32 user32)
  nexora_apply_defaults(NexoraEditorWindowsDpiImeTests)
  add_test(NAME editor.windows_dpi_ime_contract COMMAND NexoraEditorWindowsDpiImeTests)
endif()

find_program(NEXORA_XVFB_EXECUTABLE Xvfb)
find_program(NEXORA_XDOTOOL_EXECUTABLE xdotool)
if(NEXORA_XVFB_EXECUTABLE AND NEXORA_XDOTOOL_EXECUTABLE)
  add_test(
    NAME editor.linux_display_acceptance
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxDisplayAcceptance.py"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_display_acceptance PROPERTIES TIMEOUT 60)
  add_test(
    NAME editor.linux_native_scene_preview
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeScenePreview.py"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_native_scene_preview PROPERTIES TIMEOUT 45)
endif()
