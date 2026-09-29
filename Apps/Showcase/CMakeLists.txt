include(NexoraTarget)

if(NOT NEXORA_ZIG_SAMPLE_OBJECT)
  message(FATAL_ERROR "NEXORA_ZIG_SAMPLE_OBJECT must be provided by Gameplay/Zig")
endif()

add_library(NexoraShowcaseProbes STATIC ShowcaseProbes.cpp)
target_include_directories(NexoraShowcaseProbes PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
nexora_apply_defaults(NexoraShowcaseProbes)

add_executable(NexoraShowcase
  main.cpp
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
            --profile Shipping --build-configuration "$<CONFIG>"
            --binary "$<TARGET_FILE:NexoraShowcase>"
            ${NEXORA_SHOWCASE_RUNTIME_LIBRARY_ARGS}
            --api-manifest "${CMAKE_SOURCE_DIR}/Engine/API/abi_manifest.json"
            --license "${CMAKE_SOURCE_DIR}/LICENSE"
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
  add_test(
    NAME showcase.v1_probe
    COMMAND NexoraShowcaseProbeTests)
  add_executable(NexoraShowcaseProbeTests
    "${CMAKE_SOURCE_DIR}/Tests/Showcase/ShowcaseProbeTests.cpp")
  target_link_libraries(NexoraShowcaseProbeTests PRIVATE NexoraShowcaseProbes)
  nexora_apply_defaults(NexoraShowcaseProbeTests)
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
    add_test(
      NAME showcase.linux_vulkan_virtual_display
      COMMAND "${Python3_EXECUTABLE}"
              "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVirtualDisplaySmoke.py"
              "$<TARGET_FILE:NexoraShowcase>")
    set_tests_properties(showcase.linux_vulkan_virtual_display PROPERTIES
      LABELS "showcase;vulkan;virtual-display"
      SKIP_RETURN_CODE 77
      TIMEOUT 45)
  endif()
endif()
