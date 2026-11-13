#include "Nexora/TwoD/GameSpriteAssets.h"
#include <algorithm>
#include <atomic>
#include <unordered_set>

namespace nexora::twod {
std::optional<GameSpriteAssets> GameSpriteAssets::Load(std::span<const runtime::RuntimeBlob> blobs,
                                                       std::string *error) {
  if (error)
    error->clear();
  const auto reject = [&](const char *text) -> std::optional<GameSpriteAssets> {
    if (error)
      *error = text;
    return {};
  };
  if (blobs.size() > 4096)
    return reject("Game Sprite asset count exceeds 4096.");
  GameSpriteAssets result;
  std::unordered_set<AssetId, runtime::AssetUuidHash> identities;
  std::size_t bytes{};
  static std::atomic<std::uint64_t> next_texture{std::uint64_t{1} << 57};
  for (const auto &blob : blobs) {
    if (blob.id == AssetId{} || !identities.insert(blob.id).second || blob.schema_version != 1 ||
        blob.payload.size() > 64 * 1024 * 1024 - bytes ||
        !runtime::AssetCooker::Deserialize(runtime::AssetCooker::Serialize(blob)))
      return reject("Game Sprite identity, integrity or 64 MiB payload budget is invalid.");
    bytes += blob.payload.size();
    if (blob.type == "sprite_atlas") {
      auto atlas = DecodeAtlas(blob.payload);
      if (!atlas || atlas->id != blob.id || !blob.dependencies.empty())
        return reject("Game Sprite Atlas payload disagrees with its identity/dependencies.");
      result.atlases_.emplace(blob.id, std::make_shared<const Atlas>(std::move(*atlas)));
      const auto texture = next_texture.fetch_add(1);
      if (texture >= (std::uint64_t{1} << 58))
        return reject("Game Sprite texture identities exhausted.");
      result.textures_.emplace(blob.id, texture);
    } else if (blob.type == "sprite_source") {
      auto source = DecodeSpriteAsset(blob.payload);
      if (!source || source->id != blob.id || blob.dependencies.size() != 1 ||
          blob.dependencies.front() != source->atlas)
        return reject("Game Sprite source payload disagrees with its identity/dependencies.");
      result.sources_.emplace(blob.id, std::make_shared<const SpriteAsset>(std::move(*source)));
    } else
      return reject("Unsupported game Sprite asset type.");
  }
  // One region index per Atlas: avoid a full Atlas validation/linear region scan per source.
  using Regions = std::unordered_map<std::uint32_t, const Region *>;
  std::unordered_map<AssetId, Regions, runtime::AssetUuidHash> regions;
  for (const auto &[id, atlas] : result.atlases_)
    for (const auto &region : atlas->regions)
      regions[id].emplace(region.id, &region);
  for (const auto &[id, source] : result.sources_) {
    static_cast<void>(id);
    const auto atlas = regions.find(source->atlas);
    if (atlas == regions.end())
      return reject("Game Sprite source is missing its Atlas.");
    const auto region = atlas->second.find(source->region);
    if (region == atlas->second.end() || region->second->width != source->trimWidth ||
        region->second->height != source->trimHeight)
      return reject("Game Sprite source region/trim closure is invalid.");
  }
  return result;
}

std::shared_ptr<const SpriteAsset> GameSpriteAssets::Source(AssetId id) const {
  const auto found = sources_.find(id);
  return found == sources_.end() ? nullptr : found->second;
}
std::shared_ptr<const Atlas> GameSpriteAssets::ResolveAtlas(AssetId id) const {
  const auto found = atlases_.find(id);
  return found == atlases_.end() ? nullptr : found->second;
}

std::optional<SpriteFrame> GameSpriteAssets::Extract(const runtime::GameSceneView &view,
                                                     std::uint32_t width,
                                                     std::uint32_t height) const {
  if (!width || !height)
    return {};
  if (sources_.empty())
    return SpriteFrame{};
  std::vector<SpriteInstance> sprites;
  for (const auto &instance : view.instances) {
    const auto poses = view.world.SceneWorldPoses(instance.scene);
    if (!poses || !instance.source)
      return {};
    std::unordered_map<runtime::Id, runtime::TransformMatrix> matrices;
    for (const auto &pose : *poses)
      matrices.emplace(pose.id, pose.matrix);
    for (const auto &record : instance.source->SceneData().opaque) {
      if (record.type != runtime::kGameSpriteSourceReferenceType &&
          record.type_name != runtime::kGameSpriteSourceReferenceName)
        continue;
      const auto id =
          runtime::ReadGameSpriteSourceReference(record.type, record.type_name, record.data);
      const auto entity = instance.entities.find(record.entity);
      if (!id || !Source(*id) || entity == instance.entities.end())
        return {};
      const auto matrix = matrices.find(entity->second);
      // Gameplay may intentionally destroy an authored Sprite. Its stale reference stays in the
      // immutable template, while the absent live entity contributes no geometry.
      if (matrix == matrices.end())
        continue;
      if (sprites.size() == 100000)
        return {};
      Sprite sprite;
      sprite.source = *id;
      sprites.push_back({entity->second, sprite, matrix->second});
    }
  }
  std::ranges::sort(sprites, {}, &SpriteInstance::id);
  return ExtractSprites(
      sprites, Camera{{}, 20, 0, width, height},
      [&](AssetId id) -> std::optional<AtlasBinding> {
        const auto atlas = ResolveAtlas(id);
        const auto texture = textures_.find(id);
        return atlas && texture != textures_.end()
                   ? std::optional{AtlasBinding{atlas, texture->second}}
                   : std::nullopt;
      },
      [&](AssetId id) { return Source(id); });
}
} // namespace nexora::twod
