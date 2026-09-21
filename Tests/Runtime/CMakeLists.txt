include(NexoraTarget)
add_executable(NexoraRuntimeTests RuntimeTests.cpp)
target_link_libraries(NexoraRuntimeTests PRIVATE Nexora::Runtime)
nexora_apply_defaults(NexoraRuntimeTests)
add_test(NAME runtime.v1_m4_m12_contracts COMMAND NexoraRuntimeTests)

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
