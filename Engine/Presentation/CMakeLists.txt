include(NexoraTarget)
add_library(NexoraPresentation ${NEXORA_MODULE_LIBRARY_TYPE}
  src/Presentation.cpp
  src/RenderSurface.cpp)
add_library(Nexora::Presentation ALIAS NexoraPresentation)
target_include_directories(NexoraPresentation PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraPresentation PUBLIC Nexora::RHI Nexora::Window)
target_compile_definitions(NexoraPresentation PRIVATE NEXORA_PRESENTATION_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraPresentation PUBLIC NEXORA_PRESENTATION_STATIC=1)
endif()
if(APPLE)
  enable_language(OBJCXX)
  target_sources(NexoraPresentation PRIVATE src/MetalSurface.mm)
  target_link_libraries(NexoraPresentation PRIVATE "-framework Cocoa" "-framework Metal"
                                                    "-framework QuartzCore")
  target_compile_options(NexoraPresentation PRIVATE $<$<COMPILE_LANGUAGE:OBJCXX>:-fobjc-arc>)
  target_compile_definitions(NexoraPresentation PRIVATE NEXORA_HAS_METAL_PRESENTATION=1)
endif()
nexora_apply_defaults(NexoraPresentation)
if(WIN32)
  target_sources(NexoraPresentation PRIVATE src/Dx12Surface.cpp)
  target_link_libraries(NexoraPresentation PRIVATE d3d12 d3dcompiler dxgi)
endif()
if(NEXORA_ENABLE_NATIVE_BACKENDS AND (WIN32 OR CMAKE_SYSTEM_NAME STREQUAL "Linux"))
  find_library(NEXORA_VULKAN_LIBRARY NAMES vulkan vulkan.so.1 REQUIRED)
  target_sources(NexoraPresentation PRIVATE src/VulkanSurface.cpp)
  target_include_directories(NexoraPresentation PRIVATE "${NEXORA_VULKAN_INCLUDE_DIR}")
  target_link_libraries(NexoraPresentation PRIVATE "${NEXORA_VULKAN_LIBRARY}")
  if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # X11::X11 is an IMPORTED (non-GLOBAL) target that CMake's own
    # FindX11.cmake creates without the GLOBAL keyword, so it is only
    # visible in the directory scope that called find_package(X11) --
    # Engine/Window's CMakeLists.txt, a sibling directory, not an ancestor
    # of this one. Relying on that call alone leaves X11::X11 undefined
    # here; call find_package(X11) again so the target exists in this
    # scope too (CMake no-ops a repeat find_package after the first
    # success, so this is safe to call unconditionally).
    find_package(X11 REQUIRED)
    target_link_libraries(NexoraPresentation PRIVATE X11::X11)
  endif()
  target_compile_definitions(NexoraPresentation PRIVATE NEXORA_HAS_VULKAN_PRESENTATION=1)
  if(NEXORA_ENABLE_EDITOR_GRAPHICAL_SHELL)
    file(READ "${nexora_imgui_SOURCE_DIR}/backends/imgui_impl_vulkan.cpp" _imgui_vulkan)
    string(REGEX MATCH
      "static uint32_t __glsl_shader_vert_spv\\[\\][^{]*\\{[^}]+\\};"
      _imgui_vertex_spirv "${_imgui_vulkan}")
    string(REGEX MATCH
      "static uint32_t __glsl_shader_frag_spv\\[\\][^{]*\\{[^}]+\\};"
      _imgui_fragment_spirv "${_imgui_vulkan}")
    if(NOT _imgui_vertex_spirv OR NOT _imgui_fragment_spirv)
      message(FATAL_ERROR "Pinned Dear ImGui Vulkan shaders could not be extracted")
    endif()
    string(REPLACE "static uint32_t" "inline constexpr std::uint32_t"
      _imgui_vertex_spirv "${_imgui_vertex_spirv}")
    string(REPLACE "static uint32_t" "inline constexpr std::uint32_t"
      _imgui_fragment_spirv "${_imgui_fragment_spirv}")
    set(_imgui_shader_header "${CMAKE_CURRENT_BINARY_DIR}/generated/EditorUiVulkanShaders.h")
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
    file(WRITE "${_imgui_shader_header}"
      "#pragma once\n#include <cstdint>\nnamespace Nexora::Presentation {\n${_imgui_vertex_spirv}\n${_imgui_fragment_spirv}\n}\n")
    target_include_directories(NexoraPresentation PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
    target_compile_definitions(NexoraPresentation PRIVATE NEXORA_HAS_NATIVE_UI_SHADERS=1)
  endif()
endif()
