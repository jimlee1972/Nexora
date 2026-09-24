cmake_minimum_required(VERSION 3.25)

project(Nexora VERSION 0.1.0 LANGUAGES C CXX)

option(NEXORA_ENABLE_ZIG_GAMEPLAY "Build the Zig gameplay ABI sample and NexoraShowcase" OFF)
option(NEXORA_ENABLE_SLANG "Compile and validate the canonical Slang shader" OFF)
option(NEXORA_ENABLE_MIMALLOC "Fetch mimalloc and use it as the TrackingAllocator backend" OFF)
option(NEXORA_ENABLE_NATIVE_BACKENDS "Build the platform RHI backends" ON)
option(NEXORA_ENABLE_WINDOW_PRESENTATION
       "Build the backend-neutral window and native-presentation contracts" ON)
option(NEXORA_ENABLE_SCENE_RENDERING "Build the V1-M4 scene presentation path" ON)
option(NEXORA_ENABLE_ASSET_PIPELINE "Build the V1-M5 authoring/cooking and bundle pipeline" ON)
option(NEXORA_ENABLE_EDITOR_SDK
       "Build the V1-M6 reflection, plugin host, scene editor, and prefab foundation" ON)
option(NEXORA_ENABLE_EDITOR "Build the standalone Nexora Editor preview" ON)
option(NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL
       "Build the Dear ImGui graphical Editor shell" OFF)
option(NEXORA_ENABLE_GAMEPLAY_SIMULATION
       "Build the V1-M8 physics, character, navigation, and AI foundation" ON)
option(NEXORA_ENABLE_PRESENTATION
       "Build the V1-M9 animation, audio, VFX, and media foundation" ON)
option(NEXORA_ENABLE_LARGE_WORLD
       "Build the V1-M10 large-world streaming, terrain, vegetation, and HLOD foundation" ON)
option(NEXORA_ENABLE_PLATFORM
       "Build the V1-M11 mobile platform and native WebView foundation" ON)
option(NEXORA_ENABLE_SHIPPING
       "Build the V1-M12 shipping, update, crash-report, and hardening foundation" ON)
set(NEXORA_SHIPPING_PROFILE "Full" CACHE STRING "Shipping content profile: Minimal, Full, Dedicated")
set_property(CACHE NEXORA_SHIPPING_PROFILE PROPERTY STRINGS Minimal Full Dedicated)
if(NOT NEXORA_SHIPPING_PROFILE MATCHES "^(Minimal|Full|Dedicated)$")
  message(FATAL_ERROR "NEXORA_SHIPPING_PROFILE must be Minimal, Full, or Dedicated")
endif()
if(CMAKE_BUILD_TYPE STREQUAL "Shipping")
  set(NEXORA_ENABLE_EDITOR OFF CACHE BOOL "Build the standalone Nexora Editor preview" FORCE)
  if(NEXORA_SHIPPING_PROFILE STREQUAL "Minimal")
    set(NEXORA_ENABLE_EDITOR_SDK OFF CACHE BOOL
        "Build the V1-M6 reflection, plugin host, scene editor, and prefab foundation" FORCE)
    set(NEXORA_FEATURE_EXAMPLE_PLUGIN OFF CACHE BOOL
        "Build the example optional plugin" FORCE)
  elseif(NEXORA_SHIPPING_PROFILE STREQUAL "Dedicated")
    set(NEXORA_ENABLE_EDITOR_SDK OFF CACHE BOOL
        "Build the V1-M6 reflection, plugin host, scene editor, and prefab foundation" FORCE)
    set(NEXORA_FEATURE_EXAMPLE_PLUGIN OFF CACHE BOOL
        "Build the example optional plugin" FORCE)
    set(NEXORA_ENABLE_PRESENTATION OFF CACHE BOOL
        "Build the V1-M9 animation, audio, VFX, and media foundation" FORCE)
    set(NEXORA_ENABLE_SCENE_RENDERING OFF CACHE BOOL
        "Build the V1-M4 scene presentation path" FORCE)
  endif()
endif()
if(NEXORA_ENABLE_EDITOR AND (NOT NEXORA_ENABLE_EDITOR_SDK OR NOT NEXORA_ENABLE_ASSET_PIPELINE))
  message(FATAL_ERROR "NEXORA_ENABLE_EDITOR requires the Editor SDK and asset pipeline")
endif()
if(NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL AND
   (NOT NEXORA_ENABLE_EDITOR OR NOT NEXORA_ENABLE_WINDOW_PRESENTATION))
  message(FATAL_ERROR
          "NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL requires Editor and Window Presentation")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake/Modules")

include(NexoraBuildConfig)
include(NexoraFeatures)
include(NexoraModuleGraph)
include(NexoraSlang)
include(NexoraMimalloc)

# Fetch mimalloc before nexora_configure_build() applies our warnings-as-errors
# flags directory-wide; mimalloc's own C sources aren't -Wpedantic clean.
nexora_configure_mimalloc()

nexora_configure_build()
nexora_declare_features()
nexora_validate_module_graph("${CMAKE_CURRENT_SOURCE_DIR}/Config/Modules/modules.json")
nexora_configure_slang()

if(NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL)
  include(FetchContent)
  FetchContent_Declare(
    nexora_imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.9b-docking
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(nexora_imgui)
endif()

if(NEXORA_ENABLE_NATIVE_BACKENDS AND (WIN32 OR (UNIX AND NOT APPLE)))
  include(FetchContent)
  set(VULKAN_HEADERS_ENABLE_MODULE OFF CACHE BOOL "" FORCE)
  set(VULKAN_HEADERS_ENABLE_TESTS OFF CACHE BOOL "" FORCE)
  set(VULKAN_HEADERS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(
    nexora_vulkan_headers
    GIT_REPOSITORY https://github.com/KhronosGroup/Vulkan-Headers.git
    GIT_TAG v1.3.296
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(nexora_vulkan_headers)
  set(NEXORA_VULKAN_INCLUDE_DIR
      "${nexora_vulkan_headers_SOURCE_DIR}/include"
      CACHE INTERNAL "Vulkan header include directory")
endif()

add_subdirectory(Engine/Foundation)
add_subdirectory(Engine/Core)
if(NEXORA_ENABLE_WINDOW_PRESENTATION)
  add_subdirectory(Engine/Window)
endif()
add_subdirectory(Engine/RHI)
if(NEXORA_ENABLE_WINDOW_PRESENTATION)
  add_subdirectory(Engine/Presentation)
endif()
add_subdirectory(Engine/Renderer)
add_subdirectory(Engine/Runtime)
if(NEXORA_ENABLE_EDITOR)
  add_subdirectory(Engine/Editor)
  if(NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL)
    add_subdirectory(Engine/EditorImGui)
  endif()
  add_subdirectory(Apps/Editor)
endif()
add_subdirectory(Apps/Host)

include(CTest)

if(NEXORA_ENABLE_ZIG_GAMEPLAY)
  add_subdirectory(Gameplay/Zig)
  add_subdirectory(Apps/Showcase)
endif()

if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  add_subdirectory(Tests/Foundation)
  add_subdirectory(Tests/Core)
  if(NEXORA_ENABLE_WINDOW_PRESENTATION)
    add_subdirectory(Tests/WindowPresentation)
  endif()
  add_subdirectory(Tests/API)
  add_subdirectory(Tests/Renderer)
  add_subdirectory(Tests/Runtime)
  if(NEXORA_ENABLE_EDITOR)
    add_subdirectory(Tests/Editor)
    if(NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL)
      add_subdirectory(Tests/EditorImGui)
    endif()
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
  add_test(
    NAME build.showcase_package
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Package/TestPackageShowcase.py")
  add_test(
    NAME build.showcase_package_launch_safety
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Package/TestLaunchShowcasePackage.py")
  add_test(
    NAME build.v2_migration_scanner
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Migration/TestScanV1Project.py")
  add_test(
    NAME build.v2_production_toolchain
    COMMAND "${Python3_EXECUTABLE}"
            "${CMAKE_CURRENT_SOURCE_DIR}/Tools/Production/TestNexoraTool.py")
endif()
