set(NEXORA_DEAR_IMGUI_LIBRARY_TYPE STATIC)
if(NEXORA_LINK_MODE STREQUAL "Modular")
  # Dear ImGui stores the current context in a process-global symbol.  A
  # static copy embedded in both NexoraEditorImGui.dll and a host/test EXE
  # creates two independent GImGui values across the module boundary.
  set(NEXORA_DEAR_IMGUI_LIBRARY_TYPE SHARED)
endif()

add_library(NexoraDearImGui ${NEXORA_DEAR_IMGUI_LIBRARY_TYPE}
  "${nexora_imgui_SOURCE_DIR}/imgui.cpp"
  "${nexora_imgui_SOURCE_DIR}/imgui_demo.cpp"
  "${nexora_imgui_SOURCE_DIR}/imgui_draw.cpp"
  "${nexora_imgui_SOURCE_DIR}/imgui_tables.cpp"
  "${nexora_imgui_SOURCE_DIR}/imgui_widgets.cpp")
target_include_directories(NexoraDearImGui SYSTEM PUBLIC "${nexora_imgui_SOURCE_DIR}")
# WindowEvent carries Unicode scalar values; keep supplementary characters across InputText.
# This changes ImWchar-dependent layouts, so every direct consumer needs the same definition.
target_compile_definitions(NexoraDearImGui PUBLIC IMGUI_USE_WCHAR32=1)
set_target_properties(NexoraDearImGui PROPERTIES POSITION_INDEPENDENT_CODE ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "AppleClang")
  # Pinned ImGui passes an unused scalar's address to a zero-length text replacement.
  # New AppleClang diagnoses it; retain the vendor warning without relaxing engine targets.
  include(CheckCXXCompilerFlag)
  check_cxx_compiler_flag("-Wno-error=uninitialized-const-pointer"
    NEXORA_IMGUI_HAS_CONST_POINTER_WARNING)
  if(NEXORA_IMGUI_HAS_CONST_POINTER_WARNING)
    target_compile_options(NexoraDearImGui PRIVATE "-Wno-error=uninitialized-const-pointer")
  endif()
endif()
if(NEXORA_LINK_MODE STREQUAL "Modular")
  if(WIN32)
    target_compile_definitions(NexoraDearImGui
      PRIVATE "IMGUI_API=__declspec(dllexport)"
      INTERFACE "IMGUI_API=__declspec(dllimport)")
  endif()
endif()

include(NexoraTarget)
add_library(NexoraEditorImGui ${NEXORA_MODULE_LIBRARY_TYPE} src/EditorImGui.cpp
  src/EditorSystemFont.cpp)
add_library(Nexora::EditorImGui ALIAS NexoraEditorImGui)
target_include_directories(NexoraEditorImGui
  PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraEditorImGui
  PUBLIC Nexora::Editor Nexora::Runtime Nexora::Presentation Nexora::Window
  PRIVATE NexoraDearImGui)
target_compile_definitions(NexoraEditorImGui PRIVATE NEXORA_EDITOR_IMGUI_EXPORTS)
if(BUILD_TESTING)
  target_compile_definitions(NexoraEditorImGui PRIVATE NEXORA_EDITOR_IMGUI_TEST_ACCESS=1)
endif()
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraEditorImGui PUBLIC NEXORA_EDITOR_IMGUI_STATIC=1)
endif()
nexora_apply_defaults(NexoraEditorImGui)
