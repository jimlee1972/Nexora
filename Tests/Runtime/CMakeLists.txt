include(NexoraTarget)
add_executable(NexoraShaderRuntimeTests ShaderRuntimeTests.cpp)
target_link_libraries(NexoraShaderRuntimeTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraShaderRuntimeTests)
add_test(NAME runtime.shader_artifact_contracts COMMAND NexoraShaderRuntimeTests)

add_executable(NexoraRuntimeTests RuntimeTests.cpp)
target_link_libraries(NexoraRuntimeTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraRuntimeTests)
add_test(NAME runtime.v1_m4_m11_contracts COMMAND NexoraRuntimeTests)

add_executable(NexoraGameWorldTests GameWorldTests.cpp)
target_link_libraries(NexoraGameWorldTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraGameWorldTests)
add_test(NAME api.game_world_facade COMMAND NexoraGameWorldTests)

add_executable(NexoraTransformTests TransformTests.cpp)
target_link_libraries(NexoraTransformTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraTransformTests)
add_test(NAME runtime.transform_rotation_scale COMMAND NexoraTransformTests)

add_executable(NexoraParentingTests ParentingTests.cpp)
target_link_libraries(NexoraParentingTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraParentingTests)
add_test(NAME runtime.entity_parenting COMMAND NexoraParentingTests)

add_executable(NexoraRenderSyncTests RenderSyncTests.cpp)
target_link_libraries(NexoraRenderSyncTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraRenderSyncTests)
add_test(NAME runtime.render_sync COMMAND NexoraRenderSyncTests)

add_executable(NexoraGameplayHostBridgeTests GameplayHostBridgeTests.cpp)
target_link_libraries(NexoraGameplayHostBridgeTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraGameplayHostBridgeTests)
add_test(NAME api.gameplay_host_bridge COMMAND NexoraGameplayHostBridgeTests)

if(NEXORA_ENABLE_SHIPPING)
  add_executable(NexoraV1M12Tests V1M12Tests.cpp)
  target_link_libraries(NexoraV1M12Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M12Tests)
  add_test(NAME runtime.v1_m12_shipping COMMAND NexoraV1M12Tests)

  if(NEXORA_ENABLE_NETWORK)
    add_executable(NexoraV2M12Tests V2M12Tests.cpp)
    target_link_libraries(NexoraV2M12Tests PRIVATE Nexora::Runtime Nexora::Network)
    nexora_apply_defaults(NexoraV2M12Tests)
    add_test(NAME runtime.v2_m12_hardening_reference_projects COMMAND NexoraV2M12Tests)
  endif()
else()
  add_test(NAME runtime.v1_m12_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V1-M12 shipping implementation stripped")
endif()

add_executable(NexoraV1M7Tests V1M7Tests.cpp)
target_link_libraries(NexoraV1M7Tests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraV1M7Tests)
add_test(NAME runtime.v1_m7_input_ui_localization COMMAND NexoraV1M7Tests)

if(NEXORA_ENABLE_GAMEPLAY_SIMULATION)
  add_executable(NexoraV1M8Tests V1M8Tests.cpp)
  target_link_libraries(NexoraV1M8Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M8Tests)
  add_test(NAME runtime.v1_m8_gameplay_simulation COMMAND NexoraV1M8Tests)
else()
  add_test(NAME runtime.v1_m8_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V1-M8 gameplay simulation stripped")
endif()

if(NEXORA_ENABLE_AI_RUNTIME_BRIDGE)
  add_executable(NexoraAIIntentAdapterTests AIIntentAdapterTests.cpp)
  target_link_libraries(NexoraAIIntentAdapterTests PRIVATE Nexora::AIIntegration)
  nexora_apply_defaults(NexoraAIIntentAdapterTests)
  add_test(NAME runtime.v2_m7_ai_character_intent_bridge COMMAND NexoraAIIntentAdapterTests)
else()
  add_test(NAME runtime.v2_m7_ai_character_intent_bridge_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V2-M7 AI Runtime bridge stripped")
endif()

if(NEXORA_ENABLE_LARGE_WORLD)
  add_executable(NexoraV1M10Tests V1M10Tests.cpp)
  target_link_libraries(NexoraV1M10Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M10Tests)
  add_test(NAME runtime.v1_m10_large_world COMMAND NexoraV1M10Tests)
else()
  add_test(NAME runtime.v1_m10_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V1-M10 large world stripped")
endif()

if(NEXORA_ENABLE_PLATFORM)
  add_executable(NexoraV1M11Tests V1M11Tests.cpp)
  target_link_libraries(NexoraV1M11Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M11Tests)
  add_test(NAME runtime.v1_m11_platform COMMAND NexoraV1M11Tests)
else()
  add_test(NAME runtime.v1_m11_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V1-M11 platform integration stripped")
endif()

if(NEXORA_ENABLE_PRESENTATION)
  if(NEXORA_ENABLE_ANIMATION_V2)
    add_executable(NexoraSyncGroupGraphTests SyncGroupGraphTests.cpp)
    target_link_libraries(NexoraSyncGroupGraphTests PRIVATE Nexora::Runtime Nexora::Animation)
    nexora_apply_defaults(NexoraSyncGroupGraphTests)
    add_test(NAME animation.v2_m8_sync_group_graph COMMAND NexoraSyncGroupGraphTests)
  endif()

  add_executable(NexoraV1M9Tests V1M9Tests.cpp)
  target_link_libraries(NexoraV1M9Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M9Tests)
  add_test(NAME runtime.v1_m9_presentation COMMAND NexoraV1M9Tests)

  add_executable(NexoraV2M9Tests V2M9Tests.cpp)
  target_link_libraries(NexoraV2M9Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV2M9Tests)
  add_test(NAME runtime.v2_m9_timeline_ui_audio_media COMMAND NexoraV2M9Tests)
else()
  add_test(NAME runtime.v1_m9_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V1-M9 presentation stripped")
endif()

add_executable(NexoraV1M4Tests V1M4Tests.cpp)
target_link_libraries(NexoraV1M4Tests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraV1M4Tests)
add_test(NAME runtime.v1_m4_vertical_slice COMMAND NexoraV1M4Tests)

add_executable(NexoraGameplayModuleHostTests GameplayModuleHostTests.cpp)
target_link_libraries(NexoraGameplayModuleHostTests PRIVATE Nexora::Runtime)
target_include_directories(NexoraGameplayModuleHostTests PRIVATE "${PROJECT_SOURCE_DIR}/Tests/Gameplay")
nexora_apply_defaults(NexoraGameplayModuleHostTests)
add_test(NAME runtime.gameplay_module_host COMMAND NexoraGameplayModuleHostTests)

foreach(generation IN ITEMS First Second Failing)
  add_library(NexoraDynamicGameplay${generation} MODULE DynamicGameplayModule.cpp)
  target_link_libraries(NexoraDynamicGameplay${generation} PRIVATE Nexora::Foundation)
  nexora_apply_defaults(NexoraDynamicGameplay${generation})
endforeach()
target_compile_definitions(NexoraDynamicGameplayFirst PRIVATE NEXORA_TEST_GENERATION=1)
target_compile_definitions(NexoraDynamicGameplaySecond PRIVATE NEXORA_TEST_GENERATION=2)
target_compile_definitions(NexoraDynamicGameplayFailing PRIVATE
  NEXORA_TEST_GENERATION=3 NEXORA_TEST_FAIL_RESTORE=1)

add_executable(NexoraDynamicGameplayModuleTests DynamicGameplayModuleTests.cpp)
target_link_libraries(NexoraDynamicGameplayModuleTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraDynamicGameplayModuleTests)
add_test(NAME runtime.dynamic_gameplay_module
  COMMAND NexoraDynamicGameplayModuleTests
    $<TARGET_FILE:NexoraDynamicGameplayFirst>
    $<TARGET_FILE:NexoraDynamicGameplaySecond>
    $<TARGET_FILE:NexoraDynamicGameplayFailing>)

if(NEXORA_ENABLE_ASSET_PIPELINE)
  add_executable(NexoraV1M5Tests V1M5Tests.cpp)
  target_link_libraries(NexoraV1M5Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M5Tests)
  add_test(NAME runtime.v1_m5_asset_pipeline COMMAND NexoraV1M5Tests)

  add_executable(NexoraBundleMountTests BundleMountTests.cpp)
  target_link_libraries(NexoraBundleMountTests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraBundleMountTests)
  add_test(NAME api.bundle_mount COMMAND NexoraBundleMountTests)
else()
  add_test(NAME runtime.v1_m5_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V1-M5 asset pipeline stripped")
endif()

if(NEXORA_ENABLE_EDITOR_SDK)
  find_package(Threads REQUIRED)
  add_executable(NexoraRuntimeConsoleTests RuntimeConsoleTests.cpp)
  target_link_libraries(NexoraRuntimeConsoleTests PRIVATE Nexora::Runtime Threads::Threads)
  nexora_apply_defaults(NexoraRuntimeConsoleTests)
  add_test(NAME runtime.console_record_admission COMMAND NexoraRuntimeConsoleTests)
  add_executable(NexoraV1M6Tests V1M6Tests.cpp)
  target_link_libraries(NexoraV1M6Tests PRIVATE Nexora::Runtime)
  if(WIN32)
    target_compile_definitions(NexoraV1M6Tests PRIVATE _CRT_SECURE_NO_WARNINGS)
  endif()
  nexora_apply_defaults(NexoraV1M6Tests)
  add_test(NAME runtime.v1_m6_editor_sdk COMMAND NexoraV1M6Tests)
  # The example plugin is the real .so/.dll PluginHost loads to prove the
  # stable-ABI contract end-to-end; it is optional (NEXORA_FEATURE_EXAMPLE_PLUGIN),
  # so only wire its path in when the target actually exists.
  if(TARGET NexoraExamplePlugin)
    add_dependencies(NexoraV1M6Tests NexoraExamplePlugin)
    set_property(TEST runtime.v1_m6_editor_sdk APPEND PROPERTY ENVIRONMENT
      "NEXORA_EXAMPLE_PLUGIN_PATH=$<TARGET_FILE:NexoraExamplePlugin>")
  endif()
else()
  add_test(NAME runtime.v1_m6_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V1-M6 editor SDK stripped")
endif()

if(NEXORA_ENABLE_ASSET_PIPELINE)
  add_executable(NexoraCookedSceneAssetTests CookedSceneAssetTests.cpp)
  target_link_libraries(NexoraCookedSceneAssetTests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraCookedSceneAssetTests)
  add_test(NAME runtime.cooked_scene_assets COMMAND NexoraCookedSceneAssetTests)

  add_executable(NexoraProjectPackageTests ProjectPackageTests.cpp)
  target_link_libraries(NexoraProjectPackageTests PRIVATE Nexora::Runtime)
  if(MINGW)
    target_link_options(NexoraProjectPackageTests PRIVATE -municode)
  endif()
  nexora_apply_defaults(NexoraProjectPackageTests)
  add_test(NAME runtime.static_project_package COMMAND NexoraProjectPackageTests)
  if(NEXORA_BUILD_PROJECT_PLAYER)
    add_test(NAME runtime.static_project_player_cli
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/ProjectPlayerCli.py"
        "$<TARGET_FILE:NexoraProjectPackageTests>" "$<TARGET_FILE:NexoraProjectPlayer>")
  endif()
endif()
