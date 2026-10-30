add_executable(NexoraTwoDTests TwoDTests.cpp)
target_link_libraries(NexoraTwoDTests PRIVATE Nexora::TwoD)
nexora_apply_defaults(NexoraTwoDTests)
add_test(NAME twod.runtime_contracts COMMAND NexoraTwoDTests)
add_executable(NexoraSpriteAssetTests SpriteAssetTests.cpp)
target_link_libraries(NexoraSpriteAssetTests PRIVATE Nexora::TwoD)
nexora_apply_defaults(NexoraSpriteAssetTests)
add_test(NAME twod.sprite_source COMMAND NexoraSpriteAssetTests)
add_test(NAME twod.windows_vm_transport COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/WindowsVmAcceptanceTests.py")
set_tests_properties(twod.windows_vm_transport PROPERTIES TIMEOUT 30 LABELS "twod;tools;vm")
if(WIN32)
  add_test(NAME twod.windows_vm_guest COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/WindowsVmAcceptanceTests.py" --guest-tests)
  set_tests_properties(twod.windows_vm_guest PROPERTIES TIMEOUT 60 LABELS "twod;tools;vm")
endif()
add_executable(NexoraTwoDGameplayTests GameplayBridgeTests.cpp)
target_link_libraries(NexoraTwoDGameplayTests PRIVATE Nexora::TwoD)
nexora_apply_defaults(NexoraTwoDGameplayTests)
add_test(NAME twod.gameplay_bridge COMMAND NexoraTwoDGameplayTests)
if(NEXORA_ENABLE_EDITOR)
  add_executable(NexoraProjectTwoDPlayAssetTests ProjectPlayAssetTests.cpp)
  target_link_libraries(NexoraProjectTwoDPlayAssetTests PRIVATE Nexora::Editor)
  nexora_apply_defaults(NexoraProjectTwoDPlayAssetTests)
  add_test(NAME twod.project_play_assets COMMAND NexoraProjectTwoDPlayAssetTests)
  add_executable(NexoraTwoDContentImportTests ContentImportTests.cpp)
  target_link_libraries(NexoraTwoDContentImportTests PRIVATE Nexora::Editor)
  nexora_apply_defaults(NexoraTwoDContentImportTests)
  add_test(NAME twod.content_import COMMAND NexoraTwoDContentImportTests)
  add_executable(NexoraTwoDPlayAssetTests PlayAssetTests.cpp)
  target_link_libraries(NexoraTwoDPlayAssetTests PRIVATE Nexora::Editor)
  nexora_apply_defaults(NexoraTwoDPlayAssetTests)
  add_test(NAME twod.play_assets COMMAND NexoraTwoDPlayAssetTests)
  add_executable(NexoraTwoDAuthoringTests AuthoringTests.cpp)
  target_link_libraries(NexoraTwoDAuthoringTests PRIVATE Nexora::Editor)
  nexora_apply_defaults(NexoraTwoDAuthoringTests)
  add_test(NAME twod.authoring_contracts COMMAND NexoraTwoDAuthoringTests)
  add_executable(NexoraTwoDPlayContextTests PlayContextTests.cpp)
  target_link_libraries(NexoraTwoDPlayContextTests PRIVATE Nexora::Editor)
  nexora_apply_defaults(NexoraTwoDPlayContextTests)
  add_test(NAME twod.play_context COMMAND NexoraTwoDPlayContextTests)
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND)
  find_package(X11 REQUIRED)
  if(NEXORA_ENABLE_EDITOR)
    add_executable(NexoraTwoDPlayContextNativeTests PlayContextTests.cpp)
    target_link_libraries(NexoraTwoDPlayContextNativeTests PRIVATE Nexora::Editor X11::X11)
    target_compile_definitions(NexoraTwoDPlayContextNativeTests PRIVATE NEXORA_PLAY_CONTEXT_NATIVE=1)
    nexora_apply_defaults(NexoraTwoDPlayContextNativeTests)
    add_test(NAME twod.play_context_native COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVulkanSceneAcceptance.py"
      --native-test "$<TARGET_FILE:NexoraTwoDPlayContextNativeTests>"
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/twod-play-context")
    set_tests_properties(twod.play_context_native PROPERTIES LABELS "twod;editor;vulkan;virtual-display"
      SKIP_RETURN_CODE 77 TIMEOUT 45)
  endif()
  add_executable(NexoraTwoDNativeTests NativeTests.cpp)
  target_link_libraries(NexoraTwoDNativeTests PRIVATE Nexora::TwoD X11::X11)
  nexora_apply_defaults(NexoraTwoDNativeTests)
  add_test(NAME twod.native_pixels COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVulkanSceneAcceptance.py"
    --native-test "$<TARGET_FILE:NexoraTwoDNativeTests>"
    --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/twod-native")
  set_tests_properties(twod.native_pixels PROPERTIES LABELS "twod;vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 45)
  if(TARGET NexoraZigTwoD)
    add_test(NAME gameplay.zig_twod_native COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/ZigNativeAcceptance.py" --app "$<TARGET_FILE:NexoraZigTwoD>"
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/zig-twod")
    set_tests_properties(gameplay.zig_twod_native PROPERTIES LABELS "twod;zig;vulkan;virtual-display"
      SKIP_RETURN_CODE 77 TIMEOUT 45)
  endif()
endif()
add_test(NAME twod.asset_tool COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/AssetToolTests.py" "$<TARGET_FILE:NexoraTwoDAssetCompiler>")
add_test(NAME twod.cpu_stress COMMAND NexoraTwoDBenchmark --frames 8 --report "${CMAKE_BINARY_DIR}/twod-cpu-stress.csv")
add_test(NAME twod.benchmark_trace COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/BenchmarkTraceAcceptance.py"
  --benchmark "$<TARGET_FILE:NexoraTwoDBenchmark>")
set_tests_properties(twod.benchmark_trace PROPERTIES TIMEOUT 60 LABELS "twod;tools")
add_test(NAME twod.integrated_stress COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/IntegratedStressAcceptance.py"
  --benchmark "$<TARGET_FILE:NexoraTwoDBenchmark>")
set_tests_properties(twod.integrated_stress PROPERTIES TIMEOUT 60 LABELS "twod;tools")
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND)
  set(twod_studio_args)
  if(TARGET NexoraTwoDStudio)
    list(APPEND twod_studio_args --studio "$<TARGET_FILE:NexoraTwoDStudio>")
  endif()
  add_test(NAME twod.cloud_native COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/CloudAcceptance.py"
    --benchmark "$<TARGET_FILE:NexoraTwoDBenchmark>" --compiler "$<TARGET_FILE:NexoraTwoDAssetCompiler>" ${twod_studio_args}
    --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/twod-cloud")
  set_tests_properties(twod.cloud_native PROPERTIES LABELS "twod;vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 120)
endif()
if(NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL)
  add_executable(NexoraTwoDPanelTests PanelTests.cpp)
  target_include_directories(NexoraTwoDPanelTests PRIVATE "${PROJECT_SOURCE_DIR}/Engine/EditorImGui/src")
  target_link_libraries(NexoraTwoDPanelTests PRIVATE Nexora::EditorImGui NexoraDearImGui)
  nexora_apply_defaults(NexoraTwoDPanelTests)
  add_test(NAME twod.graphical_controls COMMAND NexoraTwoDPanelTests)
endif()
add_executable(NexoraGameplayUiTests GameplayUiTests.cpp)
target_link_libraries(NexoraGameplayUiTests PRIVATE Nexora::TwoD)
nexora_apply_defaults(NexoraGameplayUiTests)
add_test(NAME twod.gameplay_ui COMMAND NexoraGameplayUiTests)
if(NEXORA_ZIG_EXECUTABLE)
  nexora_add_zig_test(gameplay.zig_ui_wire_layout
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-ui-cache"
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-ui-global-cache"
    "-I${PROJECT_SOURCE_DIR}/Engine/API/include" --dep ui
    "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplayUiLayout.zig"
    "-Mui=${PROJECT_SOURCE_DIR}/Bindings/Zig/ui.zig" -lc)
endif()
add_executable(NexoraGameplayContentTests GameplayContentTests.cpp)
target_link_libraries(NexoraGameplayContentTests PRIVATE Nexora::TwoD)
nexora_apply_defaults(NexoraGameplayContentTests)
add_test(NAME twod.gameplay_content COMMAND NexoraGameplayContentTests)
if(NEXORA_ZIG_EXECUTABLE)
  nexora_add_zig_test(gameplay.zig_content_wire_layout
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-content-cache"
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-content-global-cache"
    "-I${PROJECT_SOURCE_DIR}/Engine/API/include" --dep content
    "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplayContentLayout.zig"
    "-Mcontent=${PROJECT_SOURCE_DIR}/Bindings/Zig/content.zig" -lc)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_test(NAME twod.audio_device_lifecycle
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/AudioDeviceAcceptance.py"
      --probe "$<TARGET_FILE:NexoraTwoDAudioProbe>"
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/twod-audio")
  set_tests_properties(twod.audio_device_lifecycle PROPERTIES TIMEOUT 30 LABELS "twod;audio")
endif()

if(TARGET NexoraZigTwoD AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_test(NAME twod.cooked_product_services COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/ProductAcceptance.py" --app "$<TARGET_FILE:NexoraZigTwoD>"
    --compiler "$<TARGET_FILE:NexoraTwoDAssetCompiler>"
    --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/twod-product")
  set_tests_properties(twod.cooked_product_services PROPERTIES TIMEOUT 90 LABELS "twod;zig")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND)
    add_test(NAME twod.cooked_product_native COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/ProductAcceptance.py" --app "$<TARGET_FILE:NexoraZigTwoD>"
      --compiler "$<TARGET_FILE:NexoraTwoDAssetCompiler>" --native
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/twod-product-native")
    set_tests_properties(twod.cooked_product_native PROPERTIES TIMEOUT 90 SKIP_RETURN_CODE 77 LABELS "twod;zig;vulkan;virtual-display")
  endif()
endif()

if(TARGET NexoraTwoDPlayer AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_library(NexoraTwoDProjectFixture SHARED ProjectModule.cpp)
  add_library(NexoraTwoDHostFixture SHARED HostModule.cpp)
  target_include_directories(NexoraTwoDHostFixture PRIVATE "${PROJECT_SOURCE_DIR}/Engine/API/include")
  nexora_apply_defaults(NexoraTwoDHostFixture)
  if(MSVC)
    target_compile_options(NexoraTwoDHostFixture PRIVATE /utf-8)
  endif()
  add_test(NAME twod.host_fixture COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/HostAcceptance.py" --app "$<TARGET_FILE:NexoraTwoDPlayer>"
    --module "$<TARGET_FILE:NexoraTwoDHostFixture>")
  set_tests_properties(twod.host_fixture PROPERTIES TIMEOUT 90 LABELS "twod;host;storage;ui")
  # This independent gameplay module consumes only the public C ABI. Linking Foundation
  # retains an unnecessary engine DSO under sanitizers and breaks its relocated source copy.
  target_include_directories(NexoraTwoDProjectFixture PRIVATE "${PROJECT_SOURCE_DIR}/Engine/API/include")
  nexora_apply_defaults(NexoraTwoDProjectFixture)
  add_test(NAME twod.project_export COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/ProjectAcceptance.py" --app "$<TARGET_FILE:NexoraTwoDPlayer>"
    --module "$<TARGET_FILE:NexoraTwoDProjectFixture>")
  set_tests_properties(twod.project_export PROPERTIES TIMEOUT 90 LABELS "twod;project;export")
endif()
if(WIN32)
  add_test(NAME twod.windows_font_bake COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/FontBakeAcceptance.py")
  set_tests_properties(twod.windows_font_bake PROPERTIES TIMEOUT 60 LABELS "twod;font;windows")
endif()
if(TARGET NexoraTwoDPlayer AND TARGET NexoraTwoDBenchmark)
  add_test(NAME twod.backend_options COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/BackendAcceptance.py" --app "$<TARGET_FILE:NexoraTwoDPlayer>"
    --benchmark "$<TARGET_FILE:NexoraTwoDBenchmark>")
  set_tests_properties(twod.backend_options PROPERTIES TIMEOUT 60 LABELS "twod;presentation;cli")
endif()
if(TARGET NexoraTwoDPlayerHost)
  add_executable(NexoraTwoDGameEntryTests GameEntryTests.cpp)
  target_link_libraries(NexoraTwoDGameEntryTests PRIVATE NexoraTwoDPlayerHost)
  nexora_apply_defaults(NexoraTwoDGameEntryTests)
  add_test(NAME twod.game_entry COMMAND NexoraTwoDGameEntryTests)
  # Build the real optional entry on every test-enabled platform, even without a game target.
  nexora_add_2d_game(NexoraTwoDGameEntryFixture "game.nx2game")
  if(TARGET NexoraTwoDProjectFixture)
    add_test(NAME twod.game_entry_acceptance COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_CURRENT_SOURCE_DIR}/GameEntryAcceptance.py"
      --app "$<TARGET_FILE:NexoraTwoDGameEntryFixture>"
      --module "$<TARGET_FILE:NexoraTwoDProjectFixture>")
    set_tests_properties(twod.game_entry_acceptance PROPERTIES TIMEOUT 90 LABELS "twod;project;export")
  endif()
  add_executable(NexoraTwoDProjectManifestTests ProjectManifestTests.cpp)
  target_link_libraries(NexoraTwoDProjectManifestTests PRIVATE NexoraTwoDPlayerHost)
  nexora_apply_defaults(NexoraTwoDProjectManifestTests)
  add_test(NAME twod.project_manifest COMMAND NexoraTwoDProjectManifestTests)
endif()

if(TARGET NexoraTwoDProjectFixture AND CMAKE_SYSTEM_NAME STREQUAL "Linux" AND
   NEXORA_ENABLE_SLANG AND NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND)
  add_test(NAME twod.project_export_native COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/ProjectAcceptance.py" --app "$<TARGET_FILE:NexoraTwoDPlayer>"
    --module "$<TARGET_FILE:NexoraTwoDProjectFixture>" --native)
  set_tests_properties(twod.project_export_native PROPERTIES TIMEOUT 90 SKIP_RETURN_CODE 77
    LABELS "twod;project;export;vulkan;virtual-display")
endif()

add_executable(NexoraGameplayDisplayTests GameplayDisplayTests.cpp)
target_link_libraries(NexoraGameplayDisplayTests PRIVATE Nexora::TwoD)
nexora_apply_defaults(NexoraGameplayDisplayTests)
add_test(NAME twod.gameplay_display COMMAND NexoraGameplayDisplayTests)
if(NEXORA_ZIG_EXECUTABLE)
  nexora_add_zig_test(gameplay.zig_display_wire_layout
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-display-cache"
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-display-global-cache"
    "-I${PROJECT_SOURCE_DIR}/Engine/API/include" --dep nx
    "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplayDisplayLayout.zig"
    "-Mnx=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig" -lc)
endif()

add_executable(NexoraGameplayUiDrawTests GameplayUiDrawTests.cpp)
target_link_libraries(NexoraGameplayUiDrawTests PRIVATE Nexora::TwoD)
nexora_apply_defaults(NexoraGameplayUiDrawTests)
add_test(NAME twod.gameplay_ui_draw COMMAND NexoraGameplayUiDrawTests)
if(NEXORA_ENABLE_ZIG_GAMEPLAY)
  nexora_add_zig_test(gameplay.zig_ui_draw_wire_layout
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-ui-draw-cache"
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-ui-draw-global-cache"
    "-I${PROJECT_SOURCE_DIR}/Engine/API/include" --dep nx
    "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplayUiDrawLayout.zig"
    "-Mnx=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig" -lc)
endif()
add_test(NAME twod.portable_font_bake COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/PortableFontBakeTests.py")
set_tests_properties(twod.portable_font_bake PROPERTIES TIMEOUT 45 SKIP_RETURN_CODE 77 LABELS "twod;font;tools")

add_executable(NexoraGameplayAssetsTests GameplayAssetsTests.cpp)
target_link_libraries(NexoraGameplayAssetsTests PRIVATE Nexora::TwoD Nexora::Core)
nexora_apply_defaults(NexoraGameplayAssetsTests)
add_test(NAME twod.gameplay_assets COMMAND NexoraGameplayAssetsTests)
add_test(NAME twod.asset_residency_stress COMMAND NexoraGameplayAssetsTests --stress)
set_tests_properties(twod.asset_residency_stress PROPERTIES TIMEOUT 240 LABELS "twod;assets;stress")
if(NEXORA_ENABLE_ZIG_GAMEPLAY)
  nexora_add_zig_test(gameplay.zig_assets_wire_layout
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-assets-cache"
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-assets-global-cache"
    "-I${PROJECT_SOURCE_DIR}/Engine/API/include" --dep nx
    "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplayAssetsLayout.zig"
    "-Mnx=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig" -lc)
endif()

add_test(NAME twod.voice_inventory COMMAND "${Python3_EXECUTABLE}"
  "${CMAKE_CURRENT_SOURCE_DIR}/VoiceInventoryTests.py")

if(NEXORA_ZIG_EXECUTABLE)
  nexora_add_zig_test(gameplay.zig_game_script_adapter
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-script-cache"
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-script-global-cache"
    --dep nx "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameScriptAdapter.zig"
    "-Mnx=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig")
endif()
if(NEXORA_ZIG_EXECUTABLE)
  add_test(NAME gameplay.zig_script_project COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_CURRENT_SOURCE_DIR}/GameScriptProjectTests.py"
    --cmake "${CMAKE_COMMAND}" --zig "${NEXORA_ZIG_EXECUTABLE}")
  set_tests_properties(gameplay.zig_script_project PROPERTIES TIMEOUT 300)
endif()
add_test(NAME twod.sdk_portable_pe COMMAND "${Python3_EXECUTABLE}"
  -m unittest discover -s "${PROJECT_SOURCE_DIR}/Tools/SDK/tests" -v)

add_executable(NexoraGameplayEffectsTests GameplayEffectsTests.cpp)
target_link_libraries(NexoraGameplayEffectsTests PRIVATE Nexora::TwoD)
nexora_apply_defaults(NexoraGameplayEffectsTests)
add_test(NAME twod.gameplay_effects COMMAND NexoraGameplayEffectsTests)
if(NEXORA_ZIG_EXECUTABLE)
  nexora_add_zig_test(gameplay.zig_effects_wire_layout
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-effects-cache"
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-effects-global-cache"
    "-I${PROJECT_SOURCE_DIR}/Engine/API/include" --dep nx
    "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplayEffectsLayout.zig"
    "-Mnx=${PROJECT_SOURCE_DIR}/Bindings/Zig/nexora.zig" -lc)
endif()
