include(NexoraTarget)
add_test(NAME api.scene_baseline_guard
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/tests/test_api_scene_contract.py")
add_executable(NexoraApiFoundationTests ApiFoundationTests.cpp)
target_link_libraries(NexoraApiFoundationTests PRIVATE Nexora::Foundation)
nexora_apply_defaults(NexoraApiFoundationTests)
add_test(NAME api.foundation_math_types COMMAND NexoraApiFoundationTests)

add_executable(NexoraApiCoreContractTests ApiCoreContractTests.cpp)
target_link_libraries(NexoraApiCoreContractTests PRIVATE Nexora::Core)
nexora_apply_defaults(NexoraApiCoreContractTests)
add_test(NAME api.core_services_handle_and_vfs_contract COMMAND NexoraApiCoreContractTests)

add_executable(NexoraApiM6CConsumer ApiM6CConsumer.c)
target_include_directories(NexoraApiM6CConsumer PRIVATE "${PROJECT_SOURCE_DIR}/Engine/API/include")
set_target_properties(NexoraApiM6CConsumer PROPERTIES C_STANDARD 11 C_STANDARD_REQUIRED YES
                                                        C_EXTENSIONS NO)
add_test(NAME api.m6_c_consumer COMMAND NexoraApiM6CConsumer)

add_test(NAME api.m6_manifest_compatibility
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/ValidateApiM6.py"
          "${PROJECT_SOURCE_DIR}")

add_test(NAME api.audio_state_baseline_guard
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/tests/test_api_audio_state_contract.py")

# NexoraApiFoundationSample (Samples/Api) is optional (NEXORA_FEATURE_API_SAMPLES),
# so only wire its smoke test in when the target actually exists -- same
# pattern Tests/Runtime/CMakeLists.txt uses for the optional example plugin.
if(TARGET NexoraApiFoundationSample)
  add_test(NAME samples.api_foundation COMMAND NexoraApiFoundationSample)
endif()
if(TARGET NexoraEditorReflectedArraySample)
  add_test(NAME samples.editor_reflected_arrays COMMAND NexoraEditorReflectedArraySample)
  set_tests_properties(samples.editor_reflected_arrays PROPERTIES TIMEOUT 30)
endif()

if(NEXORA_ZIG_EXECUTABLE)
  nexora_add_zig_test(api.gameplay_scene_zig_abi
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-global-cache"
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-local-cache"
    -I "${PROJECT_SOURCE_DIR}/Engine/API/include"
    --dep scene "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplaySceneAbi.zig"
    "-Mscene=${PROJECT_SOURCE_DIR}/Bindings/Zig/scene.zig" -lc)
  nexora_add_zig_test(api.gameplay_input_zig_abi
      --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-global-cache"
      --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-local-cache"
      -I "${PROJECT_SOURCE_DIR}/Engine/API/include"
      --dep input "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplayInputAbi.zig"
      "-Minput=${PROJECT_SOURCE_DIR}/Bindings/Zig/input.zig" -lc)
endif()

add_test(NAME api.lifecycle_baseline_guard
  COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/Tools/Build/tests/test_api_lifecycle_contract.py")
if(NEXORA_ZIG_EXECUTABLE)
  nexora_add_zig_test(api.gameplay_lifecycle_zig_abi
    --global-cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-global-cache"
    --cache-dir "${CMAKE_CURRENT_BINARY_DIR}/zig-local-cache"
    -I "${PROJECT_SOURCE_DIR}/Engine/API/include"
    --dep lifecycle "-Mroot=${CMAKE_CURRENT_SOURCE_DIR}/GameplayLifecycleAbi.zig"
    "-Mlifecycle=${PROJECT_SOURCE_DIR}/Bindings/Zig/lifecycle.zig" -lc)
endif()
