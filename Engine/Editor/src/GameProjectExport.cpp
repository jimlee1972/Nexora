#include "Nexora/Editor/GameProjectExport.h"
#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/MeshAssetCatalog.h"
#if defined(NEXORA_HAS_2D)
#include "Nexora/Editor/TwoDAssetImport.h"
#include "Nexora/TwoD/GameSpriteAssets.h"
#endif

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
#if defined(NEXORA_HAS_2D)
  if (!twod::GameSpriteAssets::Load(input.sprite_assets, error))
    return {};
#else
  if (!input.sprite_assets.empty())
    return Reject<runtime::ByteBuffer>(error, "Game Sprite export requires the TwoD feature.");
#endif
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
            opaque.type_name != kMaterialAssetReferenceName &&
            !runtime::ReadGameSpriteSourceReference(opaque.type, opaque.type_name,
                                                    std::as_bytes(std::span(opaque.data))))
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
    auto single = CookStaticProjectData(input.project, scene.entry.id, scene.capture, input.meshes,
                                        input.materials, error);
    if (!single)
      return std::nullopt;
    for (auto &blob : single->assets)
      if (blob.id == scene.entry.id) {
        std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> roots;
        for (const auto &node : scene.capture.nodes)
          for (const auto &opaque : node.opaque)
            if (const auto source = runtime::ReadGameSpriteSourceReference(
                    opaque.type, opaque.type_name, std::as_bytes(std::span(opaque.data))))
              roots.insert(*source);
        blob.dependencies.insert(blob.dependencies.end(), roots.begin(), roots.end());
      }
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
  for (const auto &blob : input.sprite_assets) {
    if (!assets.emplace(blob.id, package.assets.size()).second ||
        package.assets.size() == runtime::kMaximumProjectPackageAssets)
      return Reject<runtime::ByteBuffer>(error,
                                         "Duplicate or excessive game Sprite asset identity.");
    const auto bytes = 68 + blob.type.size() + blob.dependencies.size() * 16 + blob.payload.size();
    if (bytes > runtime::kMaximumProjectPackageBytes - package_bytes)
      return Reject<runtime::ByteBuffer>(error,
                                         "Game Sprite union exceeds the package byte budget.");
    package_bytes += bytes;
    package.assets.push_back(blob);
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
  GameProjectExportInput input{{project.high, project.low}, settings.start_scene, {}, {}, {}, {}};
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> used_meshes, used_materials;
#if defined(NEXORA_HAS_2D)
  std::unordered_set<runtime::AssetUuid, runtime::AssetUuidHash> used_sprites;
#endif
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
#if defined(NEXORA_HAS_2D)
    std::size_t sprite_references{};
#endif
    for (const auto &node : capture->nodes)
      for (const auto &opaque : node.opaque) {
        if (opaque.type == runtime::kGameSpriteSourceReferenceType ||
            opaque.type_name == runtime::kGameSpriteSourceReferenceName) {
#if defined(NEXORA_HAS_2D)
          const auto source = runtime::ReadGameSpriteSourceReference(
              opaque.type, opaque.type_name, std::as_bytes(std::span(opaque.data)));
          if (!source || ++sprite_references > 4096)
            return Reject<GameProjectExportInput>(error, "Unsupported saved Sprite reference.");
          used_sprites.insert(*source);
          continue;
#else
          return Reject<GameProjectExportInput>(error, "Saved Sprite references require TwoD.");
#endif
        }
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
#if defined(NEXORA_HAS_2D)
  if (used_sprites.size() > runtime::kMaximumProjectPackageAssets)
    return Reject<GameProjectExportInput>(error, "Saved Sprite roots exceed 4096 assets.");
  const auto source_roots = used_sprites;
  std::unordered_map<runtime::AssetUuid, const ContentItem *, runtime::AssetUuidHash> indexed;
  for (const auto &item : content.Items())
    if (item.twod)
      indexed.emplace(item.id, &item);
  std::vector<runtime::AssetUuid> pending(used_sprites.begin(), used_sprites.end());
  runtime::AssetCooker cooker;
  runtime::DerivedDataCache cache;
  std::size_t sprite_bytes{};
  for (std::size_t i = 0; i < pending.size(); ++i) {
    const auto found = indexed.find(pending[i]);
    const auto *item = found == indexed.end() ? nullptr : found->second;
    if (!item || !item->twod || item->twod->Id() != item->id || item->twod->Type() != item->type ||
        item->thumbnail != ThumbnailState::Ready)
      return Reject<GameProjectExportInput>(error, "A saved Sprite dependency is unavailable.");
    runtime::CanonicalAsset canonical;
    canonical.id = item->id;
    if (const auto source = item->twod->SpriteSource()) {
      canonical.type = "sprite_source";
      canonical.payload = twod::EncodeSpriteAsset(*source);
      canonical.dependencies.push_back(source->atlas);
      if (used_sprites.insert(source->atlas).second)
        pending.push_back(source->atlas);
    } else if (const auto atlas = item->twod->Atlas()) {
      canonical.type = "sprite_atlas";
      canonical.payload = twod::EncodeAtlas(*atlas);
    } else
      return Reject<GameProjectExportInput>(error, "Unsupported saved Sprite dependency type.");
    if (pending.size() > runtime::kMaximumProjectPackageAssets || canonical.payload.empty() ||
        canonical.payload.size() > 64 * 1024 * 1024 - sprite_bytes)
      return Reject<GameProjectExportInput>(error, "Saved Sprite closure is invalid or oversized.");
    sprite_bytes += canonical.payload.size();
    auto blob = cooker.Cook(canonical, "portable", "game-sprite-v1", cache);
    if (!blob)
      return Reject<GameProjectExportInput>(error, "Saved Sprite dependency could not be cooked.");
    input.sprite_assets.push_back(std::move(*blob));
  }
  const auto typed = twod::GameSpriteAssets::Load(input.sprite_assets, error);
  if (!typed)
    return {};
  for (const auto root : source_roots)
    if (!typed->Source(root))
      return Reject<GameProjectExportInput>(error,
                                            "Saved Sprite reference does not name a source.");
#endif
  return input;
}
} // namespace nexora::editor
