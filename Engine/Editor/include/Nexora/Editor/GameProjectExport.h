#pragma once
#include "Nexora/Editor/ProjectBuildSettings.h"
#include "Nexora/Editor/StaticProjectExport.h"
#include "Nexora/Runtime/GameProjectPackage.h"

namespace nexora::editor {
class MeshAssetCatalog;
class MaterialAssetCatalog;
struct GameSceneCapture final {
  runtime::GameSceneEntry entry;
  SceneDocument::RuntimeSceneCapture capture;
};
struct GameProjectExportInput final {
  runtime::AssetUuid project, start_scene;
  std::vector<GameSceneCapture> scenes;
  std::vector<StaticMeshAsset> meshes;
  std::vector<StaticMaterialAsset> materials;
};
// Pure owning cooker; captures are borrowed for this call. Shared asset closures merge by full
// UUID and identical cooked bytes. Unsupported required behavior/identity conflicts reject.
[[nodiscard]] NEXORA_EDITOR_API std::optional<runtime::ByteBuffer>
CookGameProject(const GameProjectExportInput &, std::string *error = nullptr);
// Synchronous owner-thread capture from explicitly saved enabled scenes. No current-document
// replacement, settings writes or publication. Callers recheck source/settings freshness before
// publication; a returned input owns all scene and referenced mesh/material bytes.
[[nodiscard]] NEXORA_EDITOR_API std::optional<GameProjectExportInput>
CaptureGameProject(const ProjectWorkspace &, ProjectBuildSettings, const ContentBrowserModel &,
                   const MeshAssetCatalog &, const MaterialAssetCatalog &,
                   std::string *error = nullptr);
} // namespace nexora::editor
