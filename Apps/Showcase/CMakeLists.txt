include(NexoraTarget)

if(NOT NEXORA_ZIG_SAMPLE_OBJECT)
  message(FATAL_ERROR "NEXORA_ZIG_SAMPLE_OBJECT must be provided by Gameplay/Zig")
endif()

add_library(NexoraShowcaseProbes STATIC ShowcaseProbes.cpp)
target_include_directories(NexoraShowcaseProbes PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
nexora_apply_defaults(NexoraShowcaseProbes)

add_executable(NexoraShowcase
  main.cpp
  ShowcaseRooms.cpp
  "${NEXORA_ZIG_SAMPLE_OBJECT}")
add_dependencies(NexoraShowcase NexoraZigGameplayBuild)
target_link_libraries(NexoraShowcase PRIVATE
  NexoraShowcaseProbes
  Nexora::Runtime
  Nexora::Renderer
  Nexora::RHI
  Nexora::Presentation)
if(WIN32)
  # The Zig object may reference the NT syscall import directly. CMake's
  # link.exe invocation does not inherit the Zig driver's implicit ntdll link.
  target_link_libraries(NexoraShowcase PRIVATE ntdll)
endif()
nexora_apply_defaults(NexoraShowcase)
if(NEXORA_LINK_MODE STREQUAL "Modular")
  add_dependencies(NexoraShowcase NexoraZigGameplaySharedBuild)
  target_compile_definitions(NexoraShowcase PRIVATE
    NEXORA_ZIG_SHARED_LIBRARY_PATH="${NEXORA_ZIG_SHARED_LIBRARY}")
  add_custom_command(TARGET NexoraShowcase POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_if_different
            "${NEXORA_ZIG_SHARED_LIBRARY}" "$<TARGET_FILE_DIR:NexoraShowcase>"
    VERBATIM)
else()
  target_compile_definitions(NexoraShowcase PRIVATE NEXORA_ZIG_SHARED_LIBRARY_PATH="")
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)
set(NEXORA_SHOWCASE_RUNTIME_LIBRARY_ARGS)
if(WIN32 AND NEXORA_LINK_MODE STREQUAL "Modular")
  list(APPEND NEXORA_SHOWCASE_RUNTIME_LIBRARY_ARGS
       --runtime-libraries "$<TARGET_RUNTIME_DLLS:NexoraShowcase>")
elseif(UNIX AND NOT APPLE AND NEXORA_LINK_MODE STREQUAL "Modular")
  list(APPEND NEXORA_SHOWCASE_RUNTIME_LIBRARY_ARGS --runtime-libraries
    "$<TARGET_FILE:NexoraRuntime>" "$<TARGET_FILE:NexoraRenderer>" "$<TARGET_FILE:NexoraRHI>"
    "$<TARGET_FILE:NexoraCore>" "$<TARGET_FILE:NexoraFoundation>"
    "$<TARGET_FILE:NexoraPresentation>" "$<TARGET_FILE:NexoraWindow>")
elseif(WIN32 AND MSVC)
  list(APPEND NEXORA_SHOWCASE_RUNTIME_LIBRARY_ARGS --runtime-libraries)
endif()
if(WIN32 AND MSVC)
  if(CMAKE_VS_PLATFORM_NAME STREQUAL "Win32")
    set(NEXORA_SHOWCASE_REDIST_ARCH x86)
  elseif(CMAKE_VS_PLATFORM_NAME STREQUAL "ARM64")
    set(NEXORA_SHOWCASE_REDIST_ARCH arm64)
  else()
    set(NEXORA_SHOWCASE_REDIST_ARCH x64)
  endif()

  set(NEXORA_SHOWCASE_CRT_DIRECTORIES)
  if(CMAKE_GENERATOR_INSTANCE)
    file(TO_CMAKE_PATH "${CMAKE_GENERATOR_INSTANCE}"
         NEXORA_SHOWCASE_GENERATOR_ROOT)
    file(GLOB NEXORA_SHOWCASE_GENERATOR_CRT_DIRECTORIES
         LIST_DIRECTORIES true
         CONFIGURE_DEPENDS
         "${NEXORA_SHOWCASE_GENERATOR_ROOT}/VC/Redist/MSVC/*/${NEXORA_SHOWCASE_REDIST_ARCH}/Microsoft.VC*.CRT")
    list(APPEND NEXORA_SHOWCASE_CRT_DIRECTORIES
         ${NEXORA_SHOWCASE_GENERATOR_CRT_DIRECTORIES})
  endif()
  if(DEFINED ENV{VCToolsRedistDir})
    file(TO_CMAKE_PATH "$ENV{VCToolsRedistDir}"
         NEXORA_SHOWCASE_ENV_REDIST_ROOT)
    file(GLOB NEXORA_SHOWCASE_ENV_CRT_DIRECTORIES
         LIST_DIRECTORIES true
         CONFIGURE_DEPENDS
         "${NEXORA_SHOWCASE_ENV_REDIST_ROOT}/${NEXORA_SHOWCASE_REDIST_ARCH}/Microsoft.VC*.CRT")
    list(APPEND NEXORA_SHOWCASE_CRT_DIRECTORIES
         ${NEXORA_SHOWCASE_ENV_CRT_DIRECTORIES})
  endif()
  if(CMAKE_CXX_COMPILER)
    get_filename_component(NEXORA_SHOWCASE_COMPILER_DIR
                           "${CMAKE_CXX_COMPILER}" DIRECTORY)
    get_filename_component(NEXORA_SHOWCASE_TOOLSET_DIR
                           "${NEXORA_SHOWCASE_COMPILER_DIR}/../../../.." ABSOLUTE)
    get_filename_component(NEXORA_SHOWCASE_VC_DIR
                           "${NEXORA_SHOWCASE_TOOLSET_DIR}/../../.." ABSOLUTE)
    file(TO_CMAKE_PATH "${NEXORA_SHOWCASE_VC_DIR}"
         NEXORA_SHOWCASE_VC_ROOT)
    file(GLOB NEXORA_SHOWCASE_COMPILER_CRT_DIRECTORIES
         LIST_DIRECTORIES true
         CONFIGURE_DEPENDS
         "${NEXORA_SHOWCASE_VC_ROOT}/Redist/MSVC/*/${NEXORA_SHOWCASE_REDIST_ARCH}/Microsoft.VC*.CRT")
    list(APPEND NEXORA_SHOWCASE_CRT_DIRECTORIES
         ${NEXORA_SHOWCASE_COMPILER_CRT_DIRECTORIES})
  endif()
  list(REMOVE_DUPLICATES NEXORA_SHOWCASE_CRT_DIRECTORIES)
  list(SORT NEXORA_SHOWCASE_CRT_DIRECTORIES COMPARE NATURAL ORDER DESCENDING)
  if(NOT NEXORA_SHOWCASE_CRT_DIRECTORIES)
    message(FATAL_ERROR
      "Could not locate the MSVC ${NEXORA_SHOWCASE_REDIST_ARCH} redistributable from "
      "CMAKE_GENERATOR_INSTANCE, VCToolsRedistDir, or CMAKE_CXX_COMPILER")
  endif()
  list(GET NEXORA_SHOWCASE_CRT_DIRECTORIES 0 NEXORA_SHOWCASE_CRT_DIRECTORY)
  foreach(NEXORA_SHOWCASE_CRT_NAME IN ITEMS
          msvcp140.dll
          vcruntime140.dll
          vcruntime140_1.dll)
    set(NEXORA_SHOWCASE_CRT_PATH
        "${NEXORA_SHOWCASE_CRT_DIRECTORY}/${NEXORA_SHOWCASE_CRT_NAME}")
    if(NOT EXISTS "${NEXORA_SHOWCASE_CRT_PATH}")
      message(FATAL_ERROR "Missing MSVC runtime library: ${NEXORA_SHOWCASE_CRT_PATH}")
    endif()
    list(APPEND NEXORA_SHOWCASE_RUNTIME_LIBRARY_ARGS
         "${NEXORA_SHOWCASE_CRT_PATH}")
  endforeach()
endif()
if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32)
  string(TOLOWER "${CMAKE_SYSTEM_NAME}" NEXORA_SHOWCASE_EVIDENCE_PLATFORM)
endif()
if(NEXORA_LINK_MODE STREQUAL "Modular")
  add_custom_target(NexoraShowcasePackageDevelopment
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Tools/Package/PackageShowcase.py"
            --profile Development --build-configuration "$<CONFIG>"
            --binary "$<TARGET_FILE:NexoraShowcase>"
            --gameplay-module "${NEXORA_ZIG_SHARED_LIBRARY}"
            ${NEXORA_SHOWCASE_RUNTIME_LIBRARY_ARGS}
            --api-manifest "${CMAKE_SOURCE_DIR}/Engine/API/abi_manifest.json"
            --license "${CMAKE_SOURCE_DIR}/LICENSE"
            --content "${CMAKE_SOURCE_DIR}/Content/Showcase" --archive
            --output "${CMAKE_BINARY_DIR}/package/NexoraShowcase-Development"
    DEPENDS NexoraShowcase
    COMMAND_EXPAND_LISTS
    VERBATIM)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32)
    add_custom_target(NexoraShowcasePackageDevelopmentEvidence
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Tools/Package/LaunchShowcasePackage.py"
              --package "${CMAKE_BINARY_DIR}/package/NexoraShowcase-Development"
              --evidence "${CMAKE_BINARY_DIR}/package/NexoraShowcase-Development/launch-evidence-${NEXORA_SHOWCASE_EVIDENCE_PLATFORM}.json"
      DEPENDS NexoraShowcasePackageDevelopment
      VERBATIM)
  endif()
endif()
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  add_custom_target(NexoraShowcasePackageShipping
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Tools/Package/PackageShowcase.py"
            --profile Shipping --shipping-profile "${NEXORA_SHIPPING_PROFILE}" --build-configuration "$<CONFIG>"
            --binary "$<TARGET_FILE:NexoraShowcase>"
            ${NEXORA_SHOWCASE_RUNTIME_LIBRARY_ARGS}
            --api-manifest "${CMAKE_SOURCE_DIR}/Engine/API/abi_manifest.json"
            --license "${CMAKE_SOURCE_DIR}/LICENSE"
            --content "${CMAKE_SOURCE_DIR}/Content/Showcase" --archive
            --output "${CMAKE_BINARY_DIR}/package/NexoraShowcase-Shipping"
    DEPENDS NexoraShowcase
    COMMAND_EXPAND_LISTS
    VERBATIM)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux" OR WIN32)
    add_custom_target(NexoraShowcasePackageShippingEvidence
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Tools/Package/LaunchShowcasePackage.py"
              --package "${CMAKE_BINARY_DIR}/package/NexoraShowcase-Shipping"
              --evidence "${CMAKE_BINARY_DIR}/package/NexoraShowcase-Shipping/launch-evidence-${NEXORA_SHOWCASE_EVIDENCE_PLATFORM}.json"
      DEPENDS NexoraShowcasePackageShipping
      VERBATIM)
  endif()
endif()

if(BUILD_TESTING)
  find_package(Python3 REQUIRED COMPONENTS Interpreter)
  find_program(NEXORA_SHOWCASE_GLSLANG NAMES glslangValidator)
  if(NEXORA_SHOWCASE_GLSLANG)
    add_test(NAME showcase.embedded_vulkan_shaders COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/Engine/Presentation/shaders/GenerateShaders.py"
      --compiler "${NEXORA_SHOWCASE_GLSLANG}" --check)
  endif()
  add_test(
    NAME showcase.v1_probe
    COMMAND NexoraShowcaseProbeTests)
  add_executable(NexoraShowcaseProbeTests
    "${CMAKE_SOURCE_DIR}/Tests/Showcase/ShowcaseProbeTests.cpp")
  target_link_libraries(NexoraShowcaseProbeTests PRIVATE NexoraShowcaseProbes)
  nexora_apply_defaults(NexoraShowcaseProbeTests)
  add_executable(NexoraShowcaseRoomTests
    "${CMAKE_SOURCE_DIR}/Tests/Showcase/ShowcaseRoomTests.cpp" ShowcaseRooms.cpp)
  target_include_directories(NexoraShowcaseRoomTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
  target_link_libraries(NexoraShowcaseRoomTests PRIVATE NexoraShowcaseProbes Nexora::Runtime Nexora::Presentation)
  nexora_apply_defaults(NexoraShowcaseRoomTests)
  add_test(NAME showcase.runtime_rooms COMMAND NexoraShowcaseRoomTests)
  add_test(NAME showcase.guided_tour_cli COMMAND NexoraShowcase --headless --tour=v1 --frames=4 --no-reload)
  add_test(NAME showcase.probe_export COMMAND NexoraShowcase --headless --probe=v1.M8 --frames=2
    --report=${CMAKE_CURRENT_BINARY_DIR}/probe-export.json --markdown=${CMAKE_CURRENT_BINARY_DIR}/probe-export.md)
  add_test(
    NAME showcase.zig_headless
    COMMAND NexoraShowcase --headless --validate-v1 --frames=4)
  add_test(
    NAME showcase.feature_gallery
    COMMAND NexoraShowcase --headless --scene=tour --frames=1 --no-reload)
  add_test(
    NAME showcase.zig_dynamic
    COMMAND NexoraShowcase --headless --frames=4 --gameplay-module=dynamic)
  add_test(
    NAME showcase.capability_fallbacks
    COMMAND NexoraShowcase --headless --scene=tour --frames=1 --no-reload
            --capabilities=minimal)
  set_tests_properties(showcase.feature_gallery PROPERTIES
    PASS_REGULAR_EXPRESSION
      "\"id\": \"math\".*\"id\": \"scene\".*\"id\": \"gameplay\".*\"id\": \"presentation\".*\"id\": \"streaming\"")
  set_tests_properties(showcase.capability_fallbacks PROPERTIES
    PASS_REGULAR_EXPRESSION
      "Gameplay Lab.*CONTRACT ONLY.*Presentation Lab.*CONTRACT ONLY.*Streaming Lab.*UNAVAILABLE")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    add_test(NAME showcase.linux_vulkan_interaction
      COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxShowcaseInteraction.py"
        "$<TARGET_FILE:NexoraShowcase>")
    set_tests_properties(showcase.linux_vulkan_interaction PROPERTIES
      LABELS "showcase;vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 45)
    add_test(
      NAME showcase.linux_vulkan_evidence_policy
      COMMAND "${Python3_EXECUTABLE}"
              "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVirtualDisplaySmokeTests.py")
    add_test(
      NAME showcase.linux_vulkan_virtual_display
      COMMAND "${Python3_EXECUTABLE}"
              "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVirtualDisplaySmoke.py"
              "$<TARGET_FILE:NexoraShowcase>"
              --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/showcase-linux-vulkan")
    set_tests_properties(showcase.linux_vulkan_virtual_display PROPERTIES
      LABELS "showcase;vulkan;virtual-display"
      SKIP_RETURN_CODE 77
      TIMEOUT 45)
    if(NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND)
      add_test(NAME showcase.linux_vulkan_rendering_room
        COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVulkanSceneAcceptance.py"
          --showcase "$<TARGET_FILE:NexoraShowcase>"
          --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/showcase-linux-vulkan-rendering-room")
      set_tests_properties(showcase.linux_vulkan_rendering_room PROPERTIES
        LABELS "showcase;vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 45)
      add_test(NAME showcase.linux_vulkan_camera_input
        COMMAND "${Python3_EXECUTABLE}"
          "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVulkanSceneAcceptance.py"
          --showcase "$<TARGET_FILE:NexoraShowcase>" --interactive
          --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/showcase-linux-vulkan-camera-input")
      set_tests_properties(showcase.linux_vulkan_camera_input PROPERTIES
        LABELS "showcase;vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 45)
    endif()
  endif()
endif()
