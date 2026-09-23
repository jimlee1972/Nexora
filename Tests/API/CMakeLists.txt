include(NexoraTarget)
add_executable(NexoraApiFoundationTests ApiFoundationTests.cpp)
target_link_libraries(NexoraApiFoundationTests PRIVATE Nexora::Foundation)
nexora_apply_defaults(NexoraApiFoundationTests)
add_test(NAME api.foundation_math_types COMMAND NexoraApiFoundationTests)

add_executable(NexoraApiCoreContractTests ApiCoreContractTests.cpp)
target_link_libraries(NexoraApiCoreContractTests PRIVATE Nexora::Core)
nexora_apply_defaults(NexoraApiCoreContractTests)
add_test(NAME api.core_services_handle_and_vfs_contract COMMAND NexoraApiCoreContractTests)

# NexoraApiFoundationSample (Samples/Api) is optional (NEXORA_FEATURE_API_SAMPLES),
# so only wire its smoke test in when the target actually exists -- same
# pattern Tests/Runtime/CMakeLists.txt uses for the optional example plugin.
if(TARGET NexoraApiFoundationSample)
  add_test(NAME samples.api_foundation COMMAND NexoraApiFoundationSample)
endif()
