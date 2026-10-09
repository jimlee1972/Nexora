add_executable(NexoraEditorImGuiTests EditorImGuiTests.cpp)
target_include_directories(NexoraEditorImGuiTests
  PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraEditorImGuiTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraEditorImGuiTests)
add_test(NAME editor.imgui_contract COMMAND NexoraEditorImGuiTests)

add_executable(NexoraConsoleDisplayTests ConsoleDisplayTests.cpp)
target_include_directories(NexoraConsoleDisplayTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraConsoleDisplayTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraConsoleDisplayTests)
add_test(NAME editor.console_display COMMAND NexoraConsoleDisplayTests)

add_executable(NexoraProcessMemoryDisplayTests ProcessMemoryDisplayTests.cpp)
target_include_directories(NexoraProcessMemoryDisplayTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraProcessMemoryDisplayTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraProcessMemoryDisplayTests)
add_test(NAME editor.process_memory_display COMMAND NexoraProcessMemoryDisplayTests)

add_executable(NexoraEditorContextLifetimeTests ContextLifetimeTests.cpp)
target_link_libraries(NexoraEditorContextLifetimeTests PRIVATE Nexora::EditorImGui NexoraDearImGui)
nexora_apply_defaults(NexoraEditorContextLifetimeTests)
add_test(NAME editor.imgui_context_lifetime COMMAND NexoraEditorContextLifetimeTests)

add_executable(NexoraEditorSceneFrameTests SceneFrameTests.cpp)
target_include_directories(NexoraEditorSceneFrameTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src" "${PROJECT_SOURCE_DIR}/Apps/Editor")
target_link_libraries(NexoraEditorSceneFrameTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraEditorSceneFrameTests)
add_test(NAME editor.scene_mesh_frame_selection COMMAND NexoraEditorSceneFrameTests)

add_executable(NexoraNativeSceneToolTests NativeSceneToolTests.cpp)
target_include_directories(NexoraNativeSceneToolTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraNativeSceneToolTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraNativeSceneToolTests)
add_test(NAME editor.native_scene_tools COMMAND NexoraNativeSceneToolTests)

add_executable(NexoraSceneSelectAllTests SceneSelectAllTests.cpp)
target_include_directories(NexoraSceneSelectAllTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src" "${PROJECT_SOURCE_DIR}/Apps/Editor")
target_link_libraries(NexoraSceneSelectAllTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraSceneSelectAllTests)
add_test(NAME editor.scene_select_all COMMAND NexoraSceneSelectAllTests)

add_executable(NexoraSceneMovePlaneTests SceneMovePlaneTests.cpp)
target_include_directories(NexoraSceneMovePlaneTests PRIVATE "${PROJECT_SOURCE_DIR}/Apps/Editor")
target_link_libraries(NexoraSceneMovePlaneTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraSceneMovePlaneTests)
add_test(NAME editor.scene_move_planes COMMAND NexoraSceneMovePlaneTests)

add_executable(NexoraInspectorComponentResetTests ComponentResetTests.cpp)
target_include_directories(NexoraInspectorComponentResetTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraInspectorComponentResetTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraInspectorComponentResetTests)
add_test(NAME editor.inspector_component_reset COMMAND NexoraInspectorComponentResetTests)

add_executable(NexoraInspectorComponentValueTests ComponentValueTests.cpp)
target_include_directories(NexoraInspectorComponentValueTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraInspectorComponentValueTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraInspectorComponentValueTests)
add_test(NAME editor.inspector_component_values COMMAND NexoraInspectorComponentValueTests)

add_executable(NexoraBuiltinCreationInputTests BuiltinCreationTests.cpp)
target_include_directories(NexoraBuiltinCreationInputTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraBuiltinCreationInputTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraBuiltinCreationInputTests)
add_test(NAME editor.builtin_creation_input COMMAND NexoraBuiltinCreationInputTests)

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

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_test(NAME editor.linux_validation_output
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/LinuxDisplayAcceptanceTests.py")
endif()

find_program(NEXORA_XVFB_EXECUTABLE Xvfb)
find_program(NEXORA_XDOTOOL_EXECUTABLE xdotool)
if(NEXORA_XVFB_EXECUTABLE AND NEXORA_XDOTOOL_EXECUTABLE)
  add_test(
    NAME editor.linux_scene_files
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/LinuxSceneFiles.py"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_scene_files PROPERTIES TIMEOUT 60)
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
  add_test(NAME editor.linux_process_memory
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/LinuxProcessMemory.py"
            --editor "$<TARGET_FILE:NexoraEditor>" --xvfb "${NEXORA_XVFB_EXECUTABLE}")
  set_tests_properties(editor.linux_process_memory PROPERTIES TIMEOUT 45)
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
  add_test(
    NAME editor.linux_native_project_input
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeGameView.py"
            --input-routing --project-bindings
            --module "$<TARGET_FILE:NexoraPlayInputFixture>"
            --editor "$<TARGET_FILE:NexoraEditor>"
            --xvfb "${NEXORA_XVFB_EXECUTABLE}"
            --xdotool "${NEXORA_XDOTOOL_EXECUTABLE}")
  set_tests_properties(editor.linux_native_project_input PROPERTIES TIMEOUT 45)
endif()

add_executable(NexoraProfilerExportTests ProfilerExportTests.cpp)
target_include_directories(NexoraProfilerExportTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraProfilerExportTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraProfilerExportTests)
add_test(NAME editor.profiler_export COMMAND NexoraProfilerExportTests)
add_test(NAME editor.profiler_json_schema
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/TestProfilerJson.py" "$<TARGET_FILE:NexoraProfilerExportTests>")
add_test(NAME editor.profiler_json_schema_optimized
  COMMAND "${Python3_EXECUTABLE}" -O "${CMAKE_CURRENT_SOURCE_DIR}/TestProfilerJson.py" "$<TARGET_FILE:NexoraProfilerExportTests>")

add_executable(NexoraPlayApplyTests PlayApplyTests.cpp)
target_include_directories(NexoraPlayApplyTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraPlayApplyTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraPlayApplyTests)
add_test(NAME editor.play_apply_changes COMMAND NexoraPlayApplyTests)

add_executable(NexoraPointerDpiTests PointerDpiTests.cpp)
target_include_directories(NexoraPointerDpiTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraPointerDpiTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraPointerDpiTests)
add_test(NAME editor.pointer_dpi COMMAND NexoraPointerDpiTests)

add_executable(NexoraUnknownInspectorTests UnknownInspectorTests.cpp)
target_include_directories(NexoraUnknownInspectorTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraUnknownInspectorTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraUnknownInspectorTests)
add_test(NAME editor.unknown_component_inspector COMMAND NexoraUnknownInspectorTests)

add_executable(NexoraComponentMultiEditTests ComponentMultiEditTests.cpp)
target_include_directories(NexoraComponentMultiEditTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraComponentMultiEditTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraComponentMultiEditTests)
add_test(NAME editor.component_multi_edit COMMAND NexoraComponentMultiEditTests)

add_executable(NexoraTransformInputTests TransformInputTests.cpp)
target_include_directories(NexoraTransformInputTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraTransformInputTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraTransformInputTests)
add_test(NAME editor.transform_input COMMAND NexoraTransformInputTests)

add_executable(NexoraComponentDraftTests ComponentDraftTests.cpp)
target_include_directories(NexoraComponentDraftTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraComponentDraftTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraComponentDraftTests)
add_test(NAME editor.component_draft_lifecycle COMMAND NexoraComponentDraftTests)

add_executable(NexoraSceneAccessTests SceneAccessTests.cpp)
target_include_directories(NexoraSceneAccessTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraSceneAccessTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraSceneAccessTests)
add_test(NAME editor.scene_authoring_access COMMAND NexoraSceneAccessTests)

add_executable(NexoraGameCameraTests GameCameraTests.cpp)
target_include_directories(NexoraGameCameraTests PRIVATE "${PROJECT_SOURCE_DIR}/Apps/Editor" "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraGameCameraTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraGameCameraTests)
add_test(NAME editor.game_camera_selection COMMAND NexoraGameCameraTests)

add_executable(NexoraCameraAlignmentTests CameraAlignmentTests.cpp)
target_include_directories(NexoraCameraAlignmentTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraCameraAlignmentTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraCameraAlignmentTests)
add_test(NAME editor.camera_scene_view_alignment COMMAND NexoraCameraAlignmentTests)

add_executable(NexoraMeshCreationTests MeshCreationTests.cpp)
target_include_directories(NexoraMeshCreationTests PRIVATE "${PROJECT_SOURCE_DIR}/Apps/Editor" "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraMeshCreationTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraMeshCreationTests)
add_test(NAME editor.mesh_scene_instantiation COMMAND NexoraMeshCreationTests)

add_executable(NexoraMeshDropTests MeshDropTests.cpp)
target_include_directories(NexoraMeshDropTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraMeshDropTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraMeshDropTests)
add_test(NAME editor.mesh_scene_drop COMMAND NexoraMeshDropTests)

add_executable(NexoraSelectionDeleteTests SelectionDeleteTests.cpp)
target_include_directories(NexoraSelectionDeleteTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraSelectionDeleteTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraSelectionDeleteTests)
add_test(NAME editor.selection_delete_transaction COMMAND NexoraSelectionDeleteTests)

add_executable(NexoraClipboardForestTests ClipboardForestTests.cpp)
target_include_directories(NexoraClipboardForestTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraClipboardForestTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraClipboardForestTests)
add_test(NAME editor.clipboard_initialized_forest COMMAND NexoraClipboardForestTests)

add_executable(NexoraCutClipboardTests CutClipboardTests.cpp)
target_include_directories(NexoraCutClipboardTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraCutClipboardTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraCutClipboardTests)
add_test(NAME editor.cut_clipboard_lifecycle COMMAND NexoraCutClipboardTests)

add_executable(NexoraInspectorMeshDropTests InspectorMeshDropTests.cpp)
target_include_directories(NexoraInspectorMeshDropTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraInspectorMeshDropTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraInspectorMeshDropTests)
add_test(NAME editor.inspector_mesh_drop COMMAND NexoraInspectorMeshDropTests)

add_executable(NexoraHierarchySelectAllTests HierarchySelectAllTests.cpp)
target_include_directories(NexoraHierarchySelectAllTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraHierarchySelectAllTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraHierarchySelectAllTests)
add_test(NAME editor.hierarchy_select_all COMMAND NexoraHierarchySelectAllTests)

add_executable(NexoraKeyboardRenameTests KeyboardRenameTests.cpp)
target_include_directories(NexoraKeyboardRenameTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraKeyboardRenameTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraKeyboardRenameTests)
add_test(NAME editor.keyboard_rename_lifecycle COMMAND NexoraKeyboardRenameTests)

add_executable(NexoraSceneFileInputTests SceneFileTests.cpp)
target_include_directories(NexoraSceneFileInputTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src" "${PROJECT_SOURCE_DIR}/Apps/Editor")
target_link_libraries(NexoraSceneFileInputTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraSceneFileInputTests)
add_test(NAME editor.scene_file_input COMMAND NexoraSceneFileInputTests)

if(NEXORA_ENABLE_NATIVE_BACKENDS AND
   (WIN32 OR APPLE OR (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NEXORA_ENABLE_VULKAN_BACKEND)))
  add_executable(NexoraEditorNativeSurfaceTests NativeSurfaceTests.cpp)
  target_include_directories(NexoraEditorNativeSurfaceTests
    PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
  target_link_libraries(NexoraEditorNativeSurfaceTests PRIVATE Nexora::EditorImGui)
  nexora_apply_defaults(NexoraEditorNativeSurfaceTests)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NEXORA_XVFB_EXECUTABLE)
    add_test(NAME editor.native_surface_lifetime COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/LinuxNativeSurfaceAcceptance.py"
      --native-test "$<TARGET_FILE:NexoraEditorNativeSurfaceTests>"
      --xvfb "${NEXORA_XVFB_EXECUTABLE}")
    set_tests_properties(editor.native_surface_lifetime PROPERTIES TIMEOUT 45)
  elseif(WIN32 OR APPLE)
    add_test(NAME editor.native_surface_lifetime COMMAND NexoraEditorNativeSurfaceTests)
    set_tests_properties(editor.native_surface_lifetime PROPERTIES TIMEOUT 45 SKIP_RETURN_CODE 77)
  endif()
endif()

add_executable(NexoraContentShortcutTests ContentShortcutTests.cpp)
target_include_directories(NexoraContentShortcutTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraContentShortcutTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraContentShortcutTests)
add_test(NAME editor.content_keyboard_shortcuts COMMAND NexoraContentShortcutTests)

add_executable(NexoraGameInputBindingsTests GameInputBindingsTests.cpp)
target_include_directories(NexoraGameInputBindingsTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraGameInputBindingsTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraGameInputBindingsTests)
add_test(NAME editor.game_input_bindings COMMAND NexoraGameInputBindingsTests)

add_executable(NexoraHierarchyNavigationTests HierarchyNavigationTests.cpp)
target_include_directories(NexoraHierarchyNavigationTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraHierarchyNavigationTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraHierarchyNavigationTests)
add_test(NAME editor.hierarchy_keyboard_navigation COMMAND NexoraHierarchyNavigationTests)

add_executable(NexoraMaterialInspectorTests MaterialInspectorTests.cpp)
target_include_directories(NexoraMaterialInspectorTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
target_link_libraries(NexoraMaterialInspectorTests PRIVATE Nexora::EditorImGui)
nexora_apply_defaults(NexoraMaterialInspectorTests)
add_test(NAME editor.material_inspector COMMAND NexoraMaterialInspectorTests)
