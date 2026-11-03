#include "Nexora/TwoD/AuthoredScene.h"
#include "Wire.h"
namespace nexora::twod {
bool ValidateSceneReferences(const SpriteScene &scene, const Atlas &atlas, const Clip &clip,
                             const Tilemap &tiles) {
  for (const auto &[id, sprite] : scene.Sprites()) {
    (void)id;
    if (sprite.source || sprite.atlas != atlas.id || !atlas.Find(sprite.region))
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
  auto snapshot = scene.Snapshot();
  if (!snapshot || !ValidateSceneReferences(scene, atlas, clip, tiles))
    return {};
  detail::Writer writer;
  writer.U32(0x4432444e);
  writer.U32(1);
  writer.Data(*snapshot);
  writer.Data(EncodeAtlas(atlas));
  writer.Data(EncodeClip(clip));
  writer.Data(tiles.Encode());
  return std::move(writer.bytes);
}
std::shared_ptr<const AuthoredScene> AuthoredScene::Decode(std::span<const std::byte> bytes) {
  detail::Reader reader(bytes);
  if (reader.U32() != 0x4432444e || reader.U32() != 1)
    return {};
  const auto sceneBytes = reader.Data(), atlasBytes = reader.Data(), clipBytes = reader.Data(),
             tileBytes = reader.Data();
  if (!reader.Done() || sceneBytes.empty())
    return {};
  auto result = std::make_shared<AuthoredScene>();
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
  if (!result->scene ||
      !ValidateSceneReferences(*result->scene, result->atlas, result->clip, result->tiles))
    return {};
  return result;
}
} // namespace nexora::twod
