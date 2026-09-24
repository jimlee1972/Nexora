include(NexoraTarget)
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

# NexoraApiFoundationSample (Samples/Api) is optional (NEXORA_FEATURE_API_SAMPLES),
# so only wire its smoke test in when the target actually exists -- same
# pattern Tests/Runtime/CMakeLists.txt uses for the optional example plugin.
if(TARGET NexoraApiFoundationSample)
  add_test(NAME samples.api_foundation COMMAND NexoraApiFoundationSample)
endif()
