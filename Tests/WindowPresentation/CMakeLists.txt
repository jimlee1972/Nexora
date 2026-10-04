include(NexoraTarget)
add_executable(NexoraWindowPresentationTests WindowPresentationTests.cpp)
target_link_libraries(NexoraWindowPresentationTests PRIVATE Nexora::Presentation)
if(WIN32)
  target_link_libraries(NexoraWindowPresentationTests PRIVATE imm32)
endif()
nexora_apply_defaults(NexoraWindowPresentationTests)
add_test(NAME window_presentation.contracts COMMAND NexoraWindowPresentationTests)

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
endif()

if(APPLE)
  add_executable(NexoraMetalSceneTests MetalSceneTests.mm)
  target_include_directories(NexoraMetalSceneTests PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Presentation/include")
  target_link_libraries(NexoraMetalSceneTests PRIVATE Nexora::Window "-framework Cocoa" "-framework Metal" "-framework QuartzCore")
  target_compile_options(NexoraMetalSceneTests PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  nexora_apply_defaults(NexoraMetalSceneTests)
  file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/artifacts")
  add_test(NAME window_presentation.metal_scene COMMAND NexoraMetalSceneTests
    "${CMAKE_BINARY_DIR}/artifacts/metal-scene.ppm")
  set_tests_properties(window_presentation.metal_scene PROPERTIES
    LABELS "showcase;metal;native-display" SKIP_RETURN_CODE 77 TIMEOUT 45)
endif()
