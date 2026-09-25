include(NexoraTarget)
add_executable(NexoraNetworkTests NetworkTests.cpp)
target_link_libraries(NexoraNetworkTests PRIVATE Nexora::Network)
nexora_apply_defaults(NexoraNetworkTests)
add_test(NAME network.v2_m5_transport COMMAND NexoraNetworkTests)

add_test(NAME network.v2_m5_headless_dependency
  COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/VerifyHeadlessDependency.py"
          "${PROJECT_SOURCE_DIR}/Config/Modules/modules.json")
