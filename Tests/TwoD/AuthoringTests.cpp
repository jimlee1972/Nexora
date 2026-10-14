#include "Nexora/Editor/TwoDDocument.h"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace nexora;
namespace {
void Check(bool value, const char *reason) {
  if (!value)
    throw std::runtime_error(reason);
}
void Run() {
  twod::Image image{1, 2, 2, twod::Bytes(16, std::byte{255})};
  auto atlas = twod::PackAtlas({17, 23}, std::span(&image, 1), 16);
  Check(bool(atlas), "atlas");
  editor::TwoDDocument d;
  Check(!d.Dirty(), "initial dirty");
  Check(d.ReplaceAtlas(*atlas), "replace atlas");
  twod::Sprite sprite;
  sprite.atlas = atlas->id;
  sprite.region = 1;
  auto id = d.AddSprite(sprite, {2, 3, 0});
  Check(bool(id), "add sprite");
  Check(d.SetCollider(*id, {}), "set collider");
  auto shapes = d.Scene().CollisionShapes();
  Check(shapes && shapes->size() == 1 && (*shapes)[0].center.x == 2 && (*shapes)[0].center.y == 3,
        "collider world position");
  auto before = d.Save();
  sprite.pivot = {0.2F, 0.8F};
  sprite.flipX = true;
  sprite.layer = 4;
  sprite.order = -3;
  sprite.color[3] = 0.4F;
  Check(d.EditSprite(*id, sprite, {5, 6, 0}), "sprite edit");
  auto edited = d.Save();
  Check(before != edited, "edit no change");
  Check(d.Undo() && d.Save() == before, "undo");
  Check(d.Redo() && d.Save() == edited, "redo");
  const auto invalidBefore = d.Save();
  sprite.region = 99;
  Check(!d.EditSprite(*id, sprite, {}) && d.Save() == invalidBefore, "bad edit rollback");
  const std::array tiles{twod::Tile{-1, 0, 0, 1, true}, twod::Tile{0, 0, 1, 1, false},
                         twod::Tile{32, 32, 0, 1, true}};
  Check(d.Paint(tiles), "tile paint");
  Check(twod::Tilemap::Chunk(-1, 0) == twod::ChunkKey{-1, 0}, "negative chunk");
  auto tileBytes = d.Tiles().Encode();
  auto reopenedTiles = twod::Tilemap::Decode(tileBytes);
  Check(reopenedTiles && reopenedTiles->Encode() == tileBytes, "tile reopen");
  Check(d.Tiles().CollisionShapes()->size() == 2, "tile collision");
  Check(d.SetChunkLoaded({-1, 0}, false) && d.Tiles().CollisionShapes()->size() == 1,
        "chunk unload collision");
  Check(d.Undo() && d.Tiles().CollisionShapes()->size() == 2, "chunk undo");
  auto saved = d.Save();
  d.MarkSaved();
  Check(!d.Dirty(), "mark saved");
  editor::TwoDDocument reopen;
  Check(reopen.Reopen(saved) && reopen.Save() == saved && !reopen.Dirty(), "document reopen");
  auto corrupt = saved;
  corrupt.push_back(std::byte{0});
  Check(!reopen.Reopen(corrupt) && reopen.Save() == saved, "malformed reopen publishes");
  for (std::size_t i = 0; i < saved.size(); ++i)
    Check(!reopen.Reopen(std::span(saved).first(i)), "truncated reopen");
  reopen.SetReadOnly(true);
  Check(!reopen.AddSprite(sprite) && !reopen.Undo() && !reopen.Reopen(saved) &&
            reopen.Save() == saved,
        "read-only mutation");
  runtime::World play(runtime::WorldKind::Play);
  auto playScene = d.StartPlay(play);
  Check(playScene && playScene->Sprites().size() == 1, "isolated play");
  auto playSprite = *playScene->Find(*id);
  playSprite.region = 1;
  playSprite.color[0] = 0;
  Check(playScene->Set(*id, playSprite) && d.Save() == saved, "play mutates editor");
  Check(!playScene->Restore(*playScene->Snapshot()), "Play editor replacement");
  auto tileSnapshot = d.Tiles().Encode();
  for (std::size_t i = 0; i < tileSnapshot.size(); ++i)
    Check(!twod::Tilemap::Decode(std::span(tileSnapshot).first(i)), "tile truncation");
  auto dup = tiles;
  dup[1] = dup[0];
  Check(!d.Paint(dup) && d.Save() == saved, "duplicate tile batch mutation");
  twod::Camera camera;
  camera.width = 320;
  camera.height = 240;
  camera.verticalSize = 10;
  auto binding = std::make_shared<const twod::Atlas>(*atlas);
  auto frame = d.Tiles().Extract(camera, [&](auto) -> std::optional<twod::AtlasBinding> {
    return twod::AtlasBinding{binding, 18};
  });
  Check(frame && frame->statistics.visible == 2 && frame->statistics.culled == 1, "tile culling");
  twod::Clip clip{{10, 10}, atlas->id, true, {{1, 0.1, "event"}}};
  Check(d.ReplaceClip(clip), "clip authoring");
  Check(d.Undo() && d.Save() == saved, "clip undo");
  editor::TwoDDocument history;
  Check(history.ReplaceAtlas(*atlas), "history atlas");
  sprite.region = 1;
  auto historyId = history.AddSprite(sprite);
  Check(bool(historyId), "history sprite");
  for (unsigned i = 1; i <= 260; ++i)
    Check(history.EditSprite(*historyId, sprite, {double(i), 0, 0}),
          "history budget blocks valid edits");
  Check(history.Undo() && history.World().FindEntity(*historyId)->transform.x == 259,
        "latest history after rollover");
  Check(history.Redo() && history.World().FindEntity(*historyId)->transform.x == 260,
        "latest redo after rollover");
}
} // namespace
int main() {
  try {
    Run();
    std::cout << "2D authoring, tilemap and isolated Play passed\n";
    return 0;
  } catch (const std::exception &e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
