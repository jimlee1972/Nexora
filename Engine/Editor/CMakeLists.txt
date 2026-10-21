include(NexoraTarget)
add_library(NexoraEditorCore ${NEXORA_MODULE_LIBRARY_TYPE}
  src/AdditiveSceneSession.cpp
  src/AdditiveSceneComposition.cpp
  src/AssetImport.cpp
  src/BuildProcess.cpp
  src/GitStatusProvider.cpp
  src/GitDiffProvider.cpp
  src/ChromeTrace.cpp
  src/ProjectChromeTrace.cpp
  src/ContentBrowser.cpp
  src/EditorProduction.cpp
  src/ExtensionTrust.cpp
  src/SignedExtensionHost.cpp
  src/PluginManager.cpp
  src/ProjectContent.cpp
  src/ProjectWorkspace.cpp
  src/ProjectInputBindings.cpp
  src/ReflectedInspector.cpp
  src/PrefabDocumentSession.cpp
  src/PrefabComparison.cpp
  src/PrefabPropertyPlan.cpp
  src/TargetedPrefabPropertyPlan.cpp
  src/PrefabAssets.cpp
  src/PrefabRebase.cpp

  src/PrefabInstantiation.cpp
  src/PrefabPlacement.cpp
  src/ProjectPrefabPlacement.cpp
  src/PrefabPlacementInspection.cpp
  src/PrefabPlacementOverrides.cpp
  src/PrefabPlacementSource.cpp
  src/PrefabPlacementRebase.cpp
  src/PlayApply.cpp
  src/EditorWorkspace.cpp
  src/PropertySnapshot.cpp
  src/MaterialToolDocument.cpp
  src/MaterialToolSourceSession.cpp
  src/MaterialImport.cpp
  src/MaterialAssetCatalog.cpp
  src/MeshImport.cpp
  src/MeshThumbnail.cpp
  src/MeshAssetCatalog.cpp
  src/NativeTool.cpp
  src/ShaderAuthoring.cpp
  src/SceneAuthoring.cpp
  src/SceneComparison.cpp
  src/SceneMerge.cpp
  src/SceneMergeCandidateJob.cpp
  src/SceneMergeBuilder.cpp
  src/SceneComparisonJob.cpp
  src/SceneFiles.cpp
  src/SceneSaveBatch.cpp
  src/StaticProjectExport.cpp
  src/StaticProjectExportJob.cpp
  src/ViewportMath.cpp)
add_library(Nexora::Editor ALIAS NexoraEditorCore)
target_include_directories(NexoraEditorCore PUBLIC "$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>")
target_link_libraries(NexoraEditorCore PUBLIC Nexora::Core Nexora::Foundation Nexora::Runtime Nexora::Renderer Nexora::Cryptography)
target_compile_definitions(NexoraEditorCore PRIVATE NEXORA_EDITOR_EXPORTS)
if(NEXORA_LINK_MODE STREQUAL "Monolithic")
  target_compile_definitions(NexoraEditorCore PUBLIC NEXORA_EDITOR_STATIC=1)
endif()
nexora_apply_defaults(NexoraEditorCore)

if(NEXORA_ENABLE_2D)
  target_sources(NexoraEditorCore PRIVATE src/TwoDDocument.cpp src/TwoDPlayContext.cpp src/TwoDPlayAssets.cpp src/TwoDAssetImport.cpp src/ProjectTwoDPlayAssets.cpp)
  target_link_libraries(NexoraEditorCore PUBLIC Nexora::TwoD)
  target_compile_definitions(NexoraEditorCore PUBLIC NEXORA_HAS_2D=1)
endif()
