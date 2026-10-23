include(NexoraTarget)
option(NEXORA_BUILD_VULKAN_SANITIZER_CONTROL "Build the Linux native ASan fault control" OFF)
if(NEXORA_BUILD_VULKAN_SANITIZER_CONTROL AND NOT
   (CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NEXORA_ENABLE_NATIVE_BACKENDS AND
    NEXORA_ENABLE_VULKAN_BACKEND))
  message(FATAL_ERROR "The Vulkan sanitizer control requires native Linux Vulkan")
endif()
add_executable(NexoraWindowPresentationTests WindowPresentationTests.cpp)
target_link_libraries(NexoraWindowPresentationTests PRIVATE Nexora::Presentation)
if(WIN32)
  target_link_libraries(NexoraWindowPresentationTests PRIVATE imm32)
endif()
nexora_apply_defaults(NexoraWindowPresentationTests)
add_test(NAME window_presentation.contracts COMMAND NexoraWindowPresentationTests)

add_executable(NexoraStagedUploadAllocationTests StagedUploadAllocationTests.cpp)
target_include_directories(NexoraStagedUploadAllocationTests PRIVATE
  "${CMAKE_SOURCE_DIR}/Engine/Presentation/src")
nexora_apply_defaults(NexoraStagedUploadAllocationTests)
add_test(NAME window_presentation.staged_upload_allocation COMMAND NexoraStagedUploadAllocationTests)

add_executable(NexoraSceneGeometryUploadKeyTests SceneGeometryUploadKeyTests.cpp)
target_include_directories(NexoraSceneGeometryUploadKeyTests PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Presentation/src")
target_link_libraries(NexoraSceneGeometryUploadKeyTests PRIVATE Nexora::Presentation)
nexora_apply_defaults(NexoraSceneGeometryUploadKeyTests)
add_test(NAME window_presentation.scene_geometry_upload_key COMMAND NexoraSceneGeometryUploadKeyTests)

add_executable(NexoraSceneGeometryResidencyTests SceneGeometryResidencyTests.cpp)
target_include_directories(NexoraSceneGeometryResidencyTests PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Presentation/src")
target_link_libraries(NexoraSceneGeometryResidencyTests PRIVATE Nexora::Presentation)
nexora_apply_defaults(NexoraSceneGeometryResidencyTests)
add_test(NAME window_presentation.scene_geometry_residency COMMAND NexoraSceneGeometryResidencyTests)

add_executable(NexoraSceneInstanceTests SceneInstanceTests.cpp)
target_include_directories(NexoraSceneInstanceTests PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Presentation/src")
target_link_libraries(NexoraSceneInstanceTests PRIVATE Nexora::Presentation)
nexora_apply_defaults(NexoraSceneInstanceTests)
add_test(NAME window_presentation.scene_instance_contract COMMAND NexoraSceneInstanceTests)

add_executable(NexoraSceneViewAdmissionTests SceneViewAdmissionTests.cpp)
target_include_directories(NexoraSceneViewAdmissionTests PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Presentation/src")
target_link_libraries(NexoraSceneViewAdmissionTests PRIVATE Nexora::Presentation)
nexora_apply_defaults(NexoraSceneViewAdmissionTests)
add_test(NAME window_presentation.scene_view_admission COMMAND NexoraSceneViewAdmissionTests)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND NEXORA_ENABLE_NATIVE_BACKENDS AND
   NEXORA_ENABLE_VULKAN_BACKEND)
  find_package(X11 REQUIRED)
  add_executable(NexoraVulkanSceneTests VulkanSceneTests.cpp)
  target_link_libraries(NexoraVulkanSceneTests PRIVATE Nexora::Presentation X11::X11)
  nexora_apply_defaults(NexoraVulkanSceneTests)
  add_test(NAME window_presentation.vulkan_scene
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVulkanSceneAcceptance.py"
      --native-test "$<TARGET_FILE:NexoraVulkanSceneTests>"
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/showcase-linux-vulkan-scene")
  set_tests_properties(window_presentation.vulkan_scene PROPERTIES
    LABELS "showcase;vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 45)

  if(NEXORA_BUILD_VULKAN_SANITIZER_CONTROL)
    add_executable(NexoraVulkanSanitizerControl VulkanSceneTests.cpp)
    target_compile_definitions(NexoraVulkanSanitizerControl PRIVATE NEXORA_TEST_VULKAN_LEAK_CONTROL=1)
    target_link_libraries(NexoraVulkanSanitizerControl PRIVATE Nexora::Presentation X11::X11)
    nexora_apply_defaults(NexoraVulkanSanitizerControl)
    add_test(NAME window_presentation.vulkan_sanitizer_leak_control
      COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/Tools/Build/VerifyVulkanSanitizerControl.py"
        --native-test "$<TARGET_FILE:NexoraVulkanSanitizerControl>"
        --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/vulkan-sanitizer-positive-control")
    set_tests_properties(window_presentation.vulkan_sanitizer_leak_control PROPERTIES
      LABELS "vulkan;virtual-display;sanitizer-positive-control" TIMEOUT 45)
  endif()

  add_executable(NexoraVulkanPbrTests VulkanPbrTests.cpp)
  target_link_libraries(NexoraVulkanPbrTests PRIVATE Nexora::Presentation X11::X11)
  nexora_apply_defaults(NexoraVulkanPbrTests)
  add_test(NAME window_presentation.vulkan_pbr
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVulkanSceneAcceptance.py"
      --native-test "$<TARGET_FILE:NexoraVulkanPbrTests>"
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/showcase-linux-vulkan-pbr")
  set_tests_properties(window_presentation.vulkan_pbr PROPERTIES
    LABELS "showcase;vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 45)

  add_executable(NexoraVulkanSceneUploadTests VulkanSceneUploadTests.cpp)
  target_include_directories(NexoraVulkanSceneUploadTests PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Presentation/include" "${NEXORA_VULKAN_INCLUDE_DIR}")
  target_link_libraries(NexoraVulkanSceneUploadTests PRIVATE Nexora::RHI Nexora::Window X11::X11 "${NEXORA_VULKAN_LIBRARY}")
  nexora_apply_defaults(NexoraVulkanSceneUploadTests)
  add_test(NAME window_presentation.vulkan_scene_upload_reuse
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVulkanSceneAcceptance.py"
      --native-test "$<TARGET_FILE:NexoraVulkanSceneUploadTests>"
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/vulkan-scene-upload-reuse")
  set_tests_properties(window_presentation.vulkan_scene_upload_reuse PROPERTIES
    LABELS "vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 45)
endif()

if(NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND AND
   (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  add_executable(NexoraVulkanSwapchainTests VulkanSwapchainTests.cpp)
  target_include_directories(NexoraVulkanSwapchainTests PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Presentation/include" "${NEXORA_VULKAN_INCLUDE_DIR}")
  target_compile_definitions(NexoraVulkanSwapchainTests PRIVATE NEXORA_PRESENTATION_STATIC=1)
  target_link_libraries(NexoraVulkanSwapchainTests PRIVATE
    Nexora::RHI Nexora::Window "${NEXORA_VULKAN_LIBRARY}")
  nexora_apply_defaults(NexoraVulkanSwapchainTests)
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    find_package(X11 REQUIRED)
    target_link_libraries(NexoraVulkanSwapchainTests PRIVATE X11::X11)
    add_test(NAME window_presentation.vulkan_swapchain
      COMMAND "${Python3_EXECUTABLE}"
        "${CMAKE_SOURCE_DIR}/Tests/Showcase/LinuxVulkanSceneAcceptance.py"
        --native-contract "$<TARGET_FILE:NexoraVulkanSwapchainTests>"
        --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/vulkan-swapchain")
    set_tests_properties(window_presentation.vulkan_swapchain PROPERTIES
      LABELS "vulkan;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 90)
  else()
    add_test(NAME window_presentation.vulkan_swapchain COMMAND NexoraVulkanSwapchainTests)
    set_tests_properties(window_presentation.vulkan_swapchain PROPERTIES
      LABELS "vulkan;native-display" RUN_SERIAL TRUE TIMEOUT 90)
  endif()
endif()

if(WIN32)
  add_executable(NexoraDx12SwapchainTests Dx12SwapchainTests.cpp)
  target_include_directories(NexoraDx12SwapchainTests PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Presentation/include")
  target_compile_definitions(NexoraDx12SwapchainTests PRIVATE NEXORA_PRESENTATION_STATIC=1)
  target_link_libraries(NexoraDx12SwapchainTests PRIVATE Nexora::RHI Nexora::Window
    d3d12 d3dcompiler dxgi)
  nexora_apply_defaults(NexoraDx12SwapchainTests)
  add_test(NAME window_presentation.dx12_swapchain COMMAND NexoraDx12SwapchainTests)
  set_tests_properties(window_presentation.dx12_swapchain PROPERTIES
    LABELS "dx12;native-display" RUN_SERIAL TRUE TIMEOUT 90)

  add_executable(NexoraDx12PbrTests VulkanPbrTests.cpp)
  target_link_libraries(NexoraDx12PbrTests PRIVATE Nexora::Presentation user32 gdi32)
  nexora_apply_defaults(NexoraDx12PbrTests)
  add_test(NAME window_presentation.dx12_pbr COMMAND NexoraDx12PbrTests)
  set_tests_properties(window_presentation.dx12_pbr PROPERTIES LABELS "showcase;dx12;native-display" TIMEOUT 90)
  if(NEXORA_ENABLE_NATIVE_BACKENDS AND NEXORA_ENABLE_VULKAN_BACKEND)
    add_executable(NexoraWindowsVulkanPbrTests VulkanPbrTests.cpp)
    target_compile_definitions(NexoraWindowsVulkanPbrTests PRIVATE NEXORA_TEST_VULKAN=1)
    target_link_libraries(NexoraWindowsVulkanPbrTests PRIVATE Nexora::Presentation user32 gdi32)
    nexora_apply_defaults(NexoraWindowsVulkanPbrTests)
    add_test(NAME window_presentation.windows_vulkan_pbr COMMAND NexoraWindowsVulkanPbrTests)
    set_tests_properties(window_presentation.windows_vulkan_pbr PROPERTIES
      LABELS "showcase;vulkan;native-display" RUN_SERIAL TRUE TIMEOUT 90)
  endif()
endif()

if(APPLE)
  add_executable(NexoraMetalSceneTests MetalSceneTests.mm)
  target_include_directories(NexoraMetalSceneTests PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Presentation/include")
  target_link_libraries(NexoraMetalSceneTests PRIVATE Nexora::RHI Nexora::Window "-framework Cocoa" "-framework Metal" "-framework QuartzCore")
  target_compile_options(NexoraMetalSceneTests PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  nexora_apply_defaults(NexoraMetalSceneTests)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts")
  add_test(NAME window_presentation.metal_scene COMMAND NexoraMetalSceneTests
    "${CMAKE_BINARY_DIR}/artifacts/metal-scene.ppm")
  set_tests_properties(window_presentation.metal_scene PROPERTIES
    LABELS "showcase;metal;native-display" SKIP_RETURN_CODE 77 TIMEOUT 45)
endif()

add_test(NAME window_presentation.hlsl_embedding
  COMMAND "${Python3_EXECUTABLE}"
    "${CMAKE_SOURCE_DIR}/Tests/WindowPresentation/EmbeddedHlslTests.py")

if(NEXORA_ENABLE_SLANG AND NEXORA_SLANGC_EXECUTABLE)
  add_test(NAME window_presentation.pbr_shader_generation
    COMMAND "${Python3_EXECUTABLE}"
      "${CMAKE_SOURCE_DIR}/Engine/Presentation/shaders/GeneratePbrShaders.py"
      --compiler "${NEXORA_SLANGC_EXECUTABLE}" --check)
  add_test(NAME window_presentation.tone_shader_generation
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_SOURCE_DIR}/Engine/Presentation/shaders/GenerateToneShaders.py"
      --compiler "${NEXORA_SLANGC_EXECUTABLE}" --check)
endif()

add_executable(NexoraGpuTimingTests GpuTimingTests.cpp)
target_include_directories(NexoraGpuTimingTests PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Presentation/src")
target_link_libraries(NexoraGpuTimingTests PRIVATE Nexora::Presentation)
nexora_apply_defaults(NexoraGpuTimingTests)
add_test(NAME window_presentation.gpu_timing COMMAND NexoraGpuTimingTests)

# X11 physical held-key transitions and committed text are independent contracts.
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  find_package(X11 REQUIRED)
  add_executable(NexoraX11KeyRepeatMatcherTests X11KeyRepeatMatcherTests.cpp)
  target_include_directories(NexoraX11KeyRepeatMatcherTests PRIVATE
    "${CMAKE_SOURCE_DIR}/Engine/Window/src")
  target_link_libraries(NexoraX11KeyRepeatMatcherTests PRIVATE X11::X11)
  nexora_apply_defaults(NexoraX11KeyRepeatMatcherTests)
  add_test(NAME window.x11_key_repeat_matcher COMMAND NexoraX11KeyRepeatMatcherTests)

  add_executable(NexoraX11HeldKeyProbe X11HeldKeyProbe.cpp)
  target_link_libraries(NexoraX11HeldKeyProbe PRIVATE Nexora::Window)
  nexora_apply_defaults(NexoraX11HeldKeyProbe)

  # Loaded only into the test child; this is not linked into Window or the Editor.
  add_library(NexoraX11RepeatCapabilityControl SHARED X11RepeatCapabilityControl.cpp)
  target_link_libraries(NexoraX11RepeatCapabilityControl PRIVATE X11::X11 ${CMAKE_DL_LIBS})
  nexora_apply_defaults(NexoraX11RepeatCapabilityControl)
  add_test(NAME window.x11_held_key_native
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/LinuxX11HeldKeyTests.py"
      --probe "$<TARGET_FILE:NexoraX11HeldKeyProbe>"
      --control "$<TARGET_FILE:NexoraX11RepeatCapabilityControl>"
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/x11-held-key")
  set_tests_properties(window.x11_held_key_native PROPERTIES
    LABELS "native-input;x11;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 45 RUN_SERIAL TRUE)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
  add_executable(NexoraX11MetricsTests X11MetricsTests.cpp)
  target_link_libraries(NexoraX11MetricsTests PRIVATE Nexora::Window X11::X11)
  nexora_apply_defaults(NexoraX11MetricsTests)
  set(NEXORA_XVFB_EXECUTABLE "" CACHE FILEPATH "Optional Xvfb path for native Window metrics tests")
  set(x11_metrics_args)
  if(NEXORA_XVFB_EXECUTABLE)
    list(APPEND x11_metrics_args --xvfb "${NEXORA_XVFB_EXECUTABLE}")
  endif()
  add_test(NAME window.x11_display_metrics_native
    COMMAND "${Python3_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/LinuxX11Metrics.py"
      --probe "$<TARGET_FILE:NexoraX11MetricsTests>"
      --evidence-dir "${CMAKE_BINARY_DIR}/artifacts/x11-display-metrics" ${x11_metrics_args})
  set_tests_properties(window.x11_display_metrics_native PROPERTIES
    LABELS "x11;display;virtual-display" SKIP_RETURN_CODE 77 TIMEOUT 30 RUN_SERIAL TRUE)
endif()
