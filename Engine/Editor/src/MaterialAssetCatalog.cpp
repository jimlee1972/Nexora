#include "Nexora/Editor/MaterialAssetCatalog.h"
#include "Nexora/Editor/ProjectContent.h"

#include <algorithm>

namespace nexora::editor {

bool MaterialAssetCatalog::PublishContent(const ContentBrowserModel &content, std::string *error) {
  if (!content.ProjectGeneration()) {
    if (error)
      *error = "Material catalog requires a nonzero project generation.";
    return false;
  }
  decltype(materials_) candidate;
  candidate.reserve(std::min(content.Items().size(), kMaximumWorkspaceMaterials));
  for (const auto &item : content.Items()) {
    if (!item.material)
      continue;
    if (candidate.size() == kMaximumWorkspaceMaterials || item.id == runtime::AssetUuid{} ||
        item.type != ".nmaterial" || !ValidateMaterialAsset(*item.material).valid ||
        candidate.contains(item.id)) {
      if (error)
        *error = "Material catalog rejected an invalid typed asset or duplicate UUID.";
      return false;
    }
    candidate.emplace(item.id, item.material);
  }
  materials_.swap(candidate);
  generation_ = content.ProjectGeneration();
  if (error)
    error->clear();
  return true;
}

void MaterialAssetCatalog::Clear() noexcept {
  materials_.clear();
  generation_ = 0;
}

std::optional<MaterialAssetSnapshot>
MaterialAssetCatalog::ResolveAsset(runtime::AssetUuid asset, std::uint64_t generation) const {
  if (!generation || generation != generation_ || (asset == runtime::AssetUuid{}))
    return std::nullopt;
  const auto found = materials_.find(asset);
  return found == materials_.end() ? std::nullopt
                                   : std::optional{MaterialAssetSnapshot{asset, found->second}};
}

OpaqueComponent MaterialAssetReference(runtime::AssetUuid asset) {
  OpaqueComponent component{
      kMaterialAssetReferenceType, std::string(kMaterialAssetReferenceName), {1}};
  for (const auto word : {asset.high, asset.low})
    for (unsigned byte = 0; byte < 8; ++byte)
      component.data.push_back(static_cast<std::uint8_t>(word >> (byte * 8)));
  return component;
}

namespace {
std::optional<runtime::AssetUuid> DecodeReference(runtime::TypeId type, std::string_view type_name,
                                                  std::size_t byte_count,
                                                  std::span<const std::uint8_t> bytes) {
  if (type != kMaterialAssetReferenceType || type_name != kMaterialAssetReferenceName ||
      byte_count != 17 || bytes.size() != 17 || bytes[0] != 1)
    return std::nullopt;
  runtime::AssetUuid asset;
  for (unsigned byte = 0; byte < 8; ++byte) {
    asset.high |= static_cast<std::uint64_t>(bytes[1 + byte]) << (byte * 8);
    asset.low |= static_cast<std::uint64_t>(bytes[9 + byte]) << (byte * 8);
  }
  return (asset == runtime::AssetUuid{}) ? std::nullopt : std::optional{asset};
}
} // namespace

std::optional<runtime::AssetUuid> ReadMaterialAssetReference(const OpaqueComponentInfo &info) {
  return DecodeReference(info.type, info.type_name, info.byte_count, info.preview);
}

std::optional<runtime::AssetUuid> ReadMaterialAssetReference(const SceneDocument &scene,
                                                             SceneDocument::NodeKey key) {
  const auto components = scene.InspectOpaqueComponents(key);
  if (!components)
    return std::nullopt;
  for (const auto &component : *components)
    if (component.type == kMaterialAssetReferenceType)
      return ReadMaterialAssetReference(component);
  return std::nullopt;
}

bool AssignMaterialAsset(SceneDocument &scene, SceneDocument::NodeKey key, runtime::AssetUuid asset,
                         std::uint64_t expected_generation, const ProjectContentSession &content,
                         const MaterialAssetCatalog &catalog, bool editable) {
  const auto selected = scene.Selection();
  const auto *item = content.Browser().Find(asset);
  const auto resolved = catalog.ResolveAsset(asset, expected_generation);
  if (!editable || !content.Writable() || selected.size() != 1 || selected[0] != key.id ||
      !scene.MeshRenderer(key) || content.Browser().ProjectGeneration() != expected_generation ||
      !item || !item->material || !resolved || resolved->material != item->material)
    return false;
  const auto components = scene.OpaqueComponents(key);
  if (!components)
    return false;
  for (const auto &component : *components)
    if ((component.type == kMaterialAssetReferenceType ||
         component.type_name == kMaterialAssetReferenceName) &&
        !DecodeReference(component.type, component.type_name, component.data.size(),
                         component.data))
      return false;
  return scene.SetOpaqueComponent(key, MaterialAssetReference(asset));
}

} // namespace nexora::editor
