include(NexoraTarget)
add_executable(NexoraRuntimeTests RuntimeTests.cpp)
target_link_libraries(NexoraRuntimeTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraRuntimeTests)
add_test(NAME runtime.v1_m4_m11_contracts COMMAND NexoraRuntimeTests)

add_executable(NexoraGameWorldTests GameWorldTests.cpp)
target_link_libraries(NexoraGameWorldTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraGameWorldTests)
add_test(NAME api.game_world_facade COMMAND NexoraGameWorldTests)

add_executable(NexoraGameplayHostBridgeTests GameplayHostBridgeTests.cpp)
target_link_libraries(NexoraGameplayHostBridgeTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraGameplayHostBridgeTests)
add_test(NAME api.gameplay_host_bridge COMMAND NexoraGameplayHostBridgeTests)

if(NEXORA_ENABLE_SHIPPING)
  add_executable(NexoraV1M12Tests V1M12Tests.cpp)
  target_link_libraries(NexoraV1M12Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M12Tests)
  add_test(NAME runtime.v1_m12_shipping COMMAND NexoraV1M12Tests)
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
  add_executable(NexoraV1M9Tests V1M9Tests.cpp)
  target_link_libraries(NexoraV1M9Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M9Tests)
  add_test(NAME runtime.v1_m9_presentation COMMAND NexoraV1M9Tests)
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
nexora_apply_defaults(NexoraGameplayModuleHostTests)
add_test(NAME runtime.gameplay_module_host COMMAND NexoraGameplayModuleHostTests)

if(NEXORA_ENABLE_ASSET_PIPELINE)
  add_executable(NexoraV1M5Tests V1M5Tests.cpp)
  target_link_libraries(NexoraV1M5Tests PRIVATE Nexora::Runtime)
  nexora_apply_defaults(NexoraV1M5Tests)
  add_test(NAME runtime.v1_m5_asset_pipeline COMMAND NexoraV1M5Tests)
else()
  add_test(NAME runtime.v1_m5_feature_strip
    COMMAND "${CMAKE_COMMAND}" -E echo "V1-M5 asset pipeline stripped")
endif()

if(NEXORA_ENABLE_EDITOR_SDK)
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
