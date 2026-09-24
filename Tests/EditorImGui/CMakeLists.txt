add_executable(NexoraEditorImGuiTests EditorImGuiTests.cpp)
target_link_libraries(NexoraEditorImGuiTests PRIVATE Nexora::EditorImGui NexoraDearImGui)
nexora_apply_defaults(NexoraEditorImGuiTests)
add_test(NAME editor.imgui_contract COMMAND NexoraEditorImGuiTests)

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
endif()
