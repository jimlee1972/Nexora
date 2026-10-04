add_executable(NexoraEditorImGuiTests EditorImGuiTests.cpp)
target_include_directories(NexoraEditorImGuiTests
  PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraEditorImGuiTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraEditorImGuiTests)
add_test(NAME editor.imgui_contract COMMAND NexoraEditorImGuiTests)

add_executable(NexoraMeshInspectorTests MeshInspectorTests.cpp)
target_include_directories(NexoraMeshInspectorTests
  PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraMeshInspectorTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraMeshInspectorTests)
add_test(NAME editor.mesh_inspector COMMAND NexoraMeshInspectorTests)

add_executable(NexoraSceneMeshPreviewTests SceneMeshPreviewTests.cpp)
target_include_directories(NexoraSceneMeshPreviewTests PRIVATE "${PROJECT_SOURCE_DIR}/Apps/Editor")
target_link_libraries(NexoraSceneMeshPreviewTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraSceneMeshPreviewTests)
add_test(NAME editor.scene_mesh_preview_geometry COMMAND NexoraSceneMeshPreviewTests)

add_executable(NexoraGameViewPreviewTests GameViewPreviewTests.cpp)
target_include_directories(NexoraGameViewPreviewTests PRIVATE "${PROJECT_SOURCE_DIR}/Apps/Editor" "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraGameViewPreviewTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraGameViewPreviewTests)
add_test(NAME editor.game_view_preview COMMAND NexoraGameViewPreviewTests)

add_executable(NexoraPlayGameplayModuleTests PlayGameplayModuleTests.cpp)
target_include_directories(NexoraPlayGameplayModuleTests PRIVATE "${PROJECT_SOURCE_DIR}/Apps/Editor")
target_link_libraries(NexoraPlayGameplayModuleTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraPlayGameplayModuleTests)
add_test(NAME editor.play_gameplay_module COMMAND NexoraPlayGameplayModuleTests)

add_library(NexoraPlayGameplayFixture MODULE PlayGameplayFixture.cpp)
target_link_libraries(NexoraPlayGameplayFixture PRIVATE Nexora::Foundation)
nexora_apply_defaults(NexoraPlayGameplayFixture)

add_executable(NexoraPlayInputStateTests PlayInputStateTests.cpp)
target_include_directories(NexoraPlayInputStateTests PRIVATE "${PROJECT_SOURCE_DIR}/Apps/Editor")
target_link_libraries(NexoraPlayInputStateTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraPlayInputStateTests)
add_test(NAME editor.play_input_state COMMAND NexoraPlayInputStateTests)

add_library(NexoraPlayInputFixture MODULE PlayGameplayFixture.cpp)
target_compile_definitions(NexoraPlayInputFixture PRIVATE NEXORA_FIXTURE_INPUT_ONLY=1)
target_link_libraries(NexoraPlayInputFixture PRIVATE Nexora::Foundation)
nexora_apply_defaults(NexoraPlayInputFixture)

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
  # Recovery relaunches the real Editor twice; a busy CI host can exceed 60 seconds
  # even when every bounded startup and recovery assertion succeeds.
  set_tests_properties(editor.linux_display_acceptance PROPERTIES TIMEOUT 90)
  add_test(
    NAME editor.linux_native_scene_preview
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeScenePreview.py"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_native_scene_preview PROPERTIES TIMEOUT 45)
  add_test(
    NAME editor.linux_native_scene_center
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeSceneCenter.py"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_native_scene_center PROPERTIES TIMEOUT 45)
  add_test(
    NAME editor.linux_native_scene_meshes
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeSceneCenter.py"
            --authored-meshes
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_native_scene_meshes PROPERTIES TIMEOUT 45)
  add_test(
    NAME editor.linux_native_game_view
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeGameView.py"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_native_game_view PROPERTIES TIMEOUT 45)
  add_test(
    NAME editor.linux_native_gameplay_module
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeGameView.py"
            --module "$<TARGET_FILE:NexoraPlayGameplayFixture>"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_native_gameplay_module PROPERTIES TIMEOUT 45)
  add_test(
    NAME editor.linux_native_game_input
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeGameView.py"
            --input-routing
            --module "$<TARGET_FILE:NexoraPlayInputFixture>"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_native_game_input PROPERTIES TIMEOUT 45)
endif()
