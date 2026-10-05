#include "Nexora/Editor/MeshAssetCatalog.h"

namespace nexora::editor {

bool MeshAssetCatalog::Publish(std::span<const AssetEntry> assets, std::uint64_t generation,
                               std::string *error) {
  const auto fail = [&](std::string message) {
    if (error)
      *error = std::move(message);
    return false;
  };
  if (generation == 0)
    return fail("Mesh catalog requires a nonzero project generation.");
  std::unordered_map<std::uint64_t, MeshAssetSnapshot> candidate;
  for (const auto &entry : assets) {
    if (entry.state != ImportState::Imported || !entry.mesh)
      continue;
    const auto resource = MeshResourceId(entry.id);
    if (resource == 0)
      return fail("Imported mesh has an invalid zero asset UUID: " + entry.relative_path);
    const auto [found, inserted] =
        candidate.emplace(resource, MeshAssetSnapshot{entry.id, resource, entry.mesh});
    if (!inserted)
      return fail("Mesh resource identity collision between " + found->second.asset.ToString() +
                  " and " + entry.id.ToString() + ". No mesh catalog was published.");
  }
  meshes_.swap(candidate);
  generation_ = generation;
  if (error)
    error->clear();
  return true;
}

bool MeshAssetCatalog::PublishContent(const ContentBrowserModel &content, std::string *error) {
  std::vector<AssetEntry> assets;
  for (const auto &item : content.Items()) {
    if (!item.mesh)
      continue;
    AssetEntry entry;
    entry.id = item.id;
    const auto path = item.path.generic_u8string();
    entry.relative_path.assign(path.begin(), path.end());
    entry.state = ImportState::Imported;
    entry.mesh = item.mesh;
    assets.push_back(std::move(entry));
  }
  return Publish(assets, content.ProjectGeneration(), error);
}

void MeshAssetCatalog::Clear() noexcept {
  meshes_.clear();
  generation_ = 0;
}

std::optional<MeshAssetSnapshot> MeshAssetCatalog::ResolveResource(std::uint64_t resource,
                                                                   std::uint64_t generation) const {
  if (generation == 0 || generation != generation_)
    return std::nullopt;
  const auto found = meshes_.find(resource);
  return found == meshes_.end() ? std::nullopt : std::optional{found->second};
}

std::optional<MeshAssetSnapshot> MeshAssetCatalog::ResolveAsset(runtime::AssetUuid asset,
                                                                std::uint64_t generation) const {
  auto result = ResolveResource(MeshResourceId(asset), generation);
  return result && result->asset == asset ? result : std::nullopt;
}

} // namespace nexora::editor
