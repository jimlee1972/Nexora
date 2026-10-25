#include "Nexora/Editor/GameProjectExport.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"

#include <algorithm>
#include <unordered_set>

namespace nexora::editor {
namespace {
template <class T> std::optional<T> Reject(std::string *error, std::string message) {
  if (error)
    *error = std::move(message);
  return std::nullopt;
}
} // namespace
std::optional<runtime::ByteBuffer> CookGameProject(const GameProjectExportInput &input,
                                                   std::string *error) {
  if (error)
    error->clear();
#if !NEXORA_ASSET_PIPELINE_ENABLED
  static_cast<void>(input);
  return Reject<runtime::ByteBuffer>(error, "Game export requires the Asset Pipeline feature.");
#else
  if (input.scenes.empty() || input.scenes.size() > runtime::kMaximumGameScenes)
    return Reject<runtime::ByteBuffer>(error, "Game export requires 1..256 enabled scenes.");
  ProjectBuildSettings admission{input.start_scene, {}};
  std::size_t package_bytes = 64;
  for (const auto &scene : input.scenes) {
    if (scene.entry.path.size() > ProjectBuildSettings::kMaximumPathBytes)
      return Reject<runtime::ByteBuffer>(error, "Game scene locator exceeds 1024 bytes.");
    admission.scenes.push_back({scene.entry.id, scene.entry.path, true});
    package_bytes += 20 + scene.entry.path.size();
    for (const auto &node : scene.capture.nodes)
      for (const auto &opaque : node.opaque)
        if (opaque.type != kMaterialAssetReferenceType &&
            opaque.type_name != kMaterialAssetReferenceName)
          return Reject<runtime::ByteBuffer>(error, "Unsupported required opaque game component.");
  }
  if (!ValidateProjectBuildSettings(admission, error))
    return std::nullopt;
  runtime::GameProjectPackage package{input.project, input.start_scene, {}, {}};
  std::unordered_map<runtime::AssetUuid, std::size_t, runtime::AssetUuidHash> assets;
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> scenes;
  std::size_t snapshots = 0;
  for (const auto &scene : input.scenes) {
    if (!scenes.insert(scene.entry.id).second ||
        scene.capture.runtime_snapshot.size() >
            runtime::kCookedSceneMaximumSnapshotBytes - snapshots)
      return Reject<runtime::ByteBuffer>(error,
                                         "Duplicate scene or aggregate snapshot budget exceeded.");
    snapshots += scene.capture.runtime_snapshot.size();
    const auto single = CookStaticProjectData(input.project, scene.entry.id, scene.capture,
                                              input.meshes, input.materials, error);
    if (!single)
      return std::nullopt;
    package.scenes.push_back(scene.entry);
    for (const auto &blob : single->assets) {
      const auto found = assets.find(blob.id);
      if (found != assets.end()) {
        if (runtime::AssetCooker::Serialize(package.assets[found->second]) !=
            runtime::AssetCooker::Serialize(blob))
          return Reject<runtime::ByteBuffer>(error,
                                             "Conflicting cooked bytes for a shared asset UUID.");
      } else {
        if (assets.size() == runtime::kMaximumProjectPackageAssets)
          return Reject<runtime::ByteBuffer>(error, "Game export exceeds 4096 unique assets.");
        const auto bytes =
            68 + blob.type.size() + blob.dependencies.size() * 16 + blob.payload.size();
        if (bytes > runtime::kMaximumProjectPackageBytes - package_bytes)
          return Reject<runtime::ByteBuffer>(error,
                                             "Game asset union exceeds 256 MiB before copying.");
        package_bytes += bytes;
        assets.emplace(blob.id, package.assets.size());
        package.assets.push_back(blob);
      }
    }
  }
  return runtime::EncodeGameProjectPackage(package, error);
#endif
}
std::optional<GameProjectExportInput>
CaptureGameProject(const ProjectWorkspace &workspace, ProjectBuildSettings settings,
                   const ContentBrowserModel &content, const MeshAssetCatalog &meshes,
                   const MaterialAssetCatalog &materials, std::string *error) {
  if (error)
    error->clear();
  ProjectBuildSettingsSession session(workspace);
  if (!session.Reload(error) || !session.Reconcile(settings, content, error) ||
      !session.ValidateFiles(settings, error))
    return std::nullopt;
  const auto project = workspace.Project().id;
  GameProjectExportInput input{{project.high, project.low}, settings.start_scene, {}, {}, {}};
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> used_meshes, used_materials;
  std::size_t text_bytes = 0, geometry_bytes = 0, entity_count = 0;
  for (const auto &entry : settings.scenes) {
    if (!entry.enabled)
      continue;
    runtime::World world;
    const auto id = world.LoadScene("Game build capture");
    SceneDocument document(world, id);
    const auto relative =
        std::filesystem::path(std::u8string(entry.path.begin(), entry.path.end()));
    if (!document.Reload(workspace.Root() / relative))
      return Reject<GameProjectExportInput>(error, "A saved game scene could not be loaded: " +
                                                       entry.path);
    auto capture = document.CaptureRuntimeScene(error);
    if (!capture ||
        capture->runtime_snapshot.size() > runtime::kCookedSceneMaximumSnapshotBytes - text_bytes)
      return Reject<GameProjectExportInput>(error,
                                            "Game snapshot capture failed or exceeds 64 MiB.");
    text_bytes += capture->runtime_snapshot.size();
    const auto count = world.FindScene(capture->scene)->entities.size();
    if (count > runtime::kCookedSceneMaximumEntities - entity_count)
      return Reject<GameProjectExportInput>(error,
                                            "Game capture exceeds 100000 aggregate entities.");
    entity_count += count;
    for (const auto &entity : world.FindScene(capture->scene)->entities) {
      if (!entity.mesh_renderer)
        continue;
      const auto mesh = meshes.ResolveResource(entity.mesh_data.mesh, content.ProjectGeneration());
      if (!mesh)
        return Reject<GameProjectExportInput>(error, "A saved game scene has an unresolved mesh.");
      if (used_meshes.insert(mesh->asset).second) {
        const auto size = mesh->geometry->vertices.size() * 48 + mesh->geometry->indices.size() * 2;
        if (size > runtime::kMaximumProjectGeometryBytes - geometry_bytes)
          return Reject<GameProjectExportInput>(
              error, "Game capture exceeds the 128 MiB geometry budget.");
        geometry_bytes += size;
        input.meshes.push_back({mesh->asset, *mesh->geometry});
      }
    }
    for (const auto &node : capture->nodes)
      for (const auto &opaque : node.opaque) {
        if (opaque.type != kMaterialAssetReferenceType &&
            opaque.type_name != kMaterialAssetReferenceName)
          return Reject<GameProjectExportInput>(
              error, "A saved game scene requires unsupported opaque behavior.");
        OpaqueComponentInfo info{opaque.type, opaque.type_name, opaque.data.size(), opaque.data,
                                 node.key.id};
        const auto uuid = ReadMaterialAssetReference(info);
        const auto material =
            uuid ? materials.ResolveAsset(*uuid, content.ProjectGeneration()) : std::nullopt;
        if (!material)
          return Reject<GameProjectExportInput>(
              error, "A saved game scene has an unresolved scalar material.");
        if (used_materials.insert(*uuid).second)
          input.materials.push_back({*uuid, *material->material});
      }
    input.scenes.push_back({{entry.id, entry.path}, std::move(*capture)});
  }
  return input;
}
} // namespace nexora::editor
