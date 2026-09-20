cmake_minimum_required(VERSION 3.25)

project(Nexora VERSION 0.1.0 LANGUAGES CXX)

option(NEXORA_ENABLE_ZIG_GAMEPLAY "Build the Zig gameplay ABI sample" OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules")

include(NexoraBuildConfig)
include(NexoraFeatures)
include(NexoraModuleGraph)

nexora_configure_build()
nexora_declare_features()
nexora_validate_module_graph("${CMAKE_CURRENT_SOURCE_DIR}/Config/Modules/modules.json")

add_subdirectory(Engine/Foundation)
add_subdirectory(Engine/Core)
add_subdirectory(Engine/RHI)
add_subdirectory(Engine/Renderer)
add_subdirectory(Engine/Runtime)
add_subdirectory(Apps/Host)

include(CTest)
if(BUILD_TESTING)
  add_subdirectory(Tests/Foundation)
  add_subdirectory(Tests/Core)
  add_subdirectory(Tests/Renderer)
  add_subdirectory(Tests/Runtime)
  if(NEXORA_ENABLE_ZIG_GAMEPLAY)
    add_subdirectory(Gameplay/Zig)
  endif()
  add_test(
    NAME build.module_graph
    COMMAND "${CMAKE_COMMAND}" -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Build/ValidateModuleGraph.cmake")
  add_test(
    NAME build.shader_contract
    COMMAND "${CMAKE_COMMAND}" -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Build/ValidateShaderContract.cmake")
  add_test(
    NAME build.plugin_manifests
    COMMAND "${CMAKE_COMMAND}" -DROOT=${CMAKE_CURRENT_SOURCE_DIR}
            -P "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Build/ValidatePluginManifests.cmake")
endif()
