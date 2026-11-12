#include "Nexora/TwoD/AuthoredScene.h"
#include "Wire.h"
#include <algorithm>
#include <unordered_map>
namespace nexora::twod {
bool ValidateSceneReferences(const SpriteScene &scene, const Atlas &atlas, const Clip &clip,
                             const Tilemap &tiles) {
  return ValidateSceneReferences(scene, atlas, clip, tiles, {});
}
bool ValidateSceneReferences(const SpriteScene &scene, const Atlas &atlas, const Clip &clip,
                             const Tilemap &tiles, std::span<const SpriteAsset> sources) {
  if (sources.size() > 4096)
    return false;
  std::unordered_map<AssetId, const SpriteAsset *, runtime::AssetUuidHash> catalog;
  for (const auto &source : sources) {
    SpriteInstance witness;
    witness.id = 1;
    witness.matrix = runtime::ToMatrix({});
    if (source.id == atlas.id || source.id == clip.id ||
        !catalog.emplace(source.id, &source).second ||
        !ResolveSpriteAsset(source, atlas, std::move(witness)))
      return false;
  }
  for (const auto &[id, sprite] : scene.Sprites()) {
    (void)id;
    if (sprite.source ? !catalog.contains(*sprite.source)
                      : sprite.atlas != atlas.id || !atlas.Find(sprite.region))
      return false;
  }
  if (!clip.frames.empty()) {
    if (clip.atlas != atlas.id)
      return false;
    for (const auto &frame : clip.frames)
      if (!atlas.Find(frame.region))
        return false;
  }
  if (!tiles.Tiles().empty()) {
    if (tiles.atlas != atlas.id)
      return false;
    for (const auto &[key, tile] : tiles.Tiles()) {
      (void)key;
      if (!atlas.Find(tile.region))
        return false;
    }
  }
  return true;
}
Bytes EncodeAuthoredScene(const SpriteScene &scene, const Atlas &atlas, const Clip &clip,
                          const Tilemap &tiles) {
  return EncodeAuthoredScene(scene, atlas, clip, tiles, {});
}
Bytes EncodeAuthoredScene(const SpriteScene &scene, const Atlas &atlas, const Clip &clip,
                          const Tilemap &tiles, std::span<const SpriteAsset> sources) {
  auto snapshot = scene.Snapshot();
  if (!snapshot || !ValidateSceneReferences(scene, atlas, clip, tiles, sources))
    return {};
  detail::Writer writer;
  writer.U32(0x4432444e);
  writer.U32(sources.empty() ? 1 : 2);
  writer.Data(*snapshot);
  writer.Data(EncodeAtlas(atlas));
  writer.Data(EncodeClip(clip));
  writer.Data(tiles.Encode());
  if (!sources.empty()) {
    std::vector<const SpriteAsset *> sorted;
    for (const auto &source : sources)
      sorted.push_back(&source);
    std::sort(sorted.begin(), sorted.end(), [](const auto *a, const auto *b) {
      return a->id.high != b->id.high ? a->id.high < b->id.high : a->id.low < b->id.low;
    });
    writer.U32(static_cast<std::uint32_t>(sorted.size()));
    for (const auto *source : sorted)
      writer.Data(EncodeSpriteAsset(*source));
  }
  return std::move(writer.bytes);
}
std::shared_ptr<const AuthoredScene> AuthoredScene::Decode(std::span<const std::byte> bytes) {
  detail::Reader reader(bytes);
  if (reader.U32() != 0x4432444e)
    return {};
  const auto version = reader.U32();
  if (version != 1 && version != 2)
    return {};
  const auto sceneBytes = reader.Data(), atlasBytes = reader.Data(), clipBytes = reader.Data(),
             tileBytes = reader.Data();
  if (!reader.Valid() || sceneBytes.empty())
    return {};
  auto result = std::make_shared<AuthoredScene>();
  if (version == 2) {
    const auto count = reader.U32();
    if (!reader.Valid() || !count || count > 4096)
      return {};
    for (std::uint32_t i = 0; i < count; ++i) {
      auto source = DecodeSpriteAsset(reader.Data(96));
      if (!source)
        return {};
      if (!result->sources.empty()) {
        const auto previous = result->sources.back().id;
        if (source->id.high < previous.high ||
            (source->id.high == previous.high && source->id.low <= previous.low))
          return {};
      }
      result->sources.push_back(std::move(*source));
    }
  }
  if (!reader.Done())
    return {};
  if (!atlasBytes.empty()) {
    auto decoded = DecodeAtlas(atlasBytes);
    if (!decoded)
      return {};
    result->atlas = std::move(*decoded);
  }
  if (!clipBytes.empty()) {
    auto decoded = DecodeClip(clipBytes);
    if (!decoded)
      return {};
    result->clip = std::move(*decoded);
  }
  if (!tileBytes.empty()) {
    auto decoded = Tilemap::Decode(tileBytes);
    if (!decoded)
      return {};
    result->tiles = std::move(*decoded);
  }
  result->scene = SpriteScene::Load(result->world, sceneBytes);
  if (!result->scene || !ValidateSceneReferences(*result->scene, result->atlas, result->clip,
                                                 result->tiles, result->sources))
    return {};
  return result;
}
} // namespace nexora::twod
