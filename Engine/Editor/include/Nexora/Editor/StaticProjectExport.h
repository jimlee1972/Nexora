#pragma once

#include "Nexora/Editor/EditorWorkspace.h"
#include "Nexora/Editor/MaterialImport.h"
#include "Nexora/Editor/MeshImport.h"
#include "Nexora/Runtime/ProjectPackage.h"

namespace nexora::editor {
struct StaticMeshAsset final {
  runtime::AssetUuid asset;
  MeshGeometry geometry;
};
struct StaticMaterialAsset final {
  runtime::AssetUuid asset;
  MaterialAsset material;
};
struct StaticProjectExportInput final {
  runtime::AssetUuid project;
  runtime::AssetUuid scene_asset;
  SceneDocument::RuntimeSceneCapture capture;
  std::vector<StaticMeshAsset> meshes;
  std::vector<StaticMaterialAsset> materials;
};

// Pure StaticView producer: owning captured inputs, exact mesh/scalar-material dependency closure,
// shared Runtime codecs and package validation. No source lookup, file publication, freshness
// authorization, gameplay execution or GPU work. Unused supplied assets do not enter the package.
// Invalid/unsupported/bounded input returns nullopt; ordinary allocation exceptions propagate.
[[nodiscard]] NEXORA_EDITOR_API std::optional<runtime::ByteBuffer>
CookStaticProject(const StaticProjectExportInput &, std::string *error = nullptr);
// The same validated owning producer before outer StaticView encoding. Multi-scene builders
// merge these exact asset closures; existing wire bytes and opaque policy are unchanged.
[[nodiscard]] NEXORA_EDITOR_API std::optional<runtime::StaticProjectPackage>
CookStaticProjectData(const StaticProjectExportInput &, std::string *error = nullptr);
[[nodiscard]] NEXORA_EDITOR_API std::optional<runtime::StaticProjectPackage>
CookStaticProjectData(runtime::AssetUuid project, runtime::AssetUuid scene,
                      const SceneDocument::RuntimeSceneCapture &, std::span<const StaticMeshAsset>,
                      std::span<const StaticMaterialAsset>, std::string *error = nullptr);
} // namespace nexora::editor
