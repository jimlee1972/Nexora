include(NexoraTarget)
add_library(NexoraEditorCore ${NEXORA_MODULE_LIBRARY_TYPE}
  src/AdditiveSceneSession.cpp
  src/AssetImport.cpp
  src/ContentBrowser.cpp
  src/EditorProduction.cpp
  src/ProjectContent.cpp
  src/ProjectWorkspace.cpp
  src/ProjectInputBindings.cpp
  src/PlayApply.cpp
  src/EditorWorkspace.cpp
  src/MaterialImport.cpp
  src/MaterialAssetCatalog.cpp
  src/MeshImport.cpp
  src/MeshAssetCatalog.cpp
  src/ShaderAuthoring.cpp
  src/SceneAuthoring.cpp
  src/SceneFiles.cpp
  src/SceneSaveBatch.cpp
  src/StaticProjectExport.cpp
  src/StaticProjectExportJob.cpp
  src/ViewportMath.cpp)
add_library(Nexora::Editor ALIAS NexoraEditorCore)
target_include_directories(NexoraEditorCore PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraEditorCore PUBLIC Nexora::Core Nexora::Foundation Nexora::Runtime Nexora::Renderer)
target_compile_definitions(NexoraEditorCore PRIVATE NEXORA_EDITOR_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraEditorCore PUBLIC NEXORA_EDITOR_STATIC=1)
endif()
nexora_apply_defaults(NexoraEditorCore)
