#include "Nexora/Editor/TwoDDocument.h"
#include <algorithm>
#include <cstring>
namespace nexora::editor {
namespace {
void Number(twod::Bytes &out, std::uint32_t v) {
  for (unsigned i = 0; i < 4; ++i)
    out.push_back(std::byte((v >> (i * 8)) & 255));
}
void Blob(twod::Bytes &out, std::span<const std::byte> bytes) {
  Number(out, static_cast<std::uint32_t>(bytes.size()));
  out.insert(out.end(), bytes.begin(), bytes.end());
}
struct Reader {
  std::span<const std::byte> data;
  std::size_t offset{};
  bool valid{true};
  std::uint32_t Number() {
    if (data.size() - offset < 4) {
      valid = false;
      return 0;
    }
    std::uint32_t v = 0;
    for (unsigned i = 0; i < 4; ++i)
      v |= std::uint32_t(std::to_integer<unsigned>(data[offset++])) << (8 * i);
    return v;
  }
  std::span<const std::byte> Blob() {
    const auto size = Number();
    if (!valid || size > data.size() - offset) {
      valid = false;
      return {};
    }
    auto b = data.subspan(offset, size);
    offset += size;
    return b;
  }
};
bool References(const twod::SpriteScene &scene, const twod::Atlas &atlas, const twod::Clip &clip,
                const twod::Tilemap &tiles) {
  for (const auto &[id, s] : scene.Sprites()) {
    (void)id;
    if (s.source.has_value() || s.atlas != atlas.id || !atlas.Find(s.region))
      return false;
  }
  if (!clip.frames.empty()) {
    if (clip.atlas != atlas.id)
      return false;
    for (const auto &f : clip.frames)
      if (!atlas.Find(f.region))
        return false;
  }
  if (!tiles.Tiles().empty()) {
    if (tiles.atlas != atlas.id)
      return false;
    for (const auto &[key, t] : tiles.Tiles()) {
      (void)key;
      if (!atlas.Find(t.region))
        return false;
    }
  }
  return true;
}
} // namespace
TwoDDocument::TwoDDocument() : scene_(world_, world_.LoadScene("2D document")) { saved_ = Save(); }
twod::Bytes TwoDDocument::Save() const {
  auto scene = scene_.Snapshot();
  if (!scene)
    return {};
  twod::Bytes out;
  Number(out, 0x4432444e);
  Number(out, 1);
  Blob(out, *scene);
  Blob(out, twod::EncodeAtlas(atlas_));
  Blob(out, twod::EncodeClip(clip_));
  Blob(out, tiles_.Encode());
  return out;
}
bool TwoDDocument::Restore(std::span<const std::byte> bytes) {
  if (bytes.size() > 64 * 1024 * 1024)
    return false;
  Reader r{bytes};
  if (r.Number() != 0x4432444e || r.Number() != 1)
    return false;
  const auto sceneBytes = r.Blob(), atlasBytes = r.Blob(), clipBytes = r.Blob(),
             tileBytes = r.Blob();
  if (!r.valid || r.offset != bytes.size() || sceneBytes.empty())
    return false;
  twod::Atlas atlas;
  twod::Clip clip;
  twod::Tilemap tiles;
  if (!atlasBytes.empty()) {
    auto a = twod::DecodeAtlas(atlasBytes);
    if (!a)
      return false;
    atlas = std::move(*a);
  }
  if (!clipBytes.empty()) {
    auto c = twod::DecodeClip(clipBytes);
    if (!c)
      return false;
    clip = std::move(*c);
  }
  if (!tileBytes.empty()) {
    auto t = twod::Tilemap::Decode(tileBytes);
    if (!t)
      return false;
    tiles = std::move(*t);
  }
  runtime::World staging;
  auto scene = twod::SpriteScene::Load(staging, sceneBytes);
  if (!scene || !References(*scene, atlas, clip, tiles) || !scene_.Restore(sceneBytes))
    return false;
  atlas_ = std::move(atlas);
  clip_ = std::move(clip);
  tiles_ = std::move(tiles);
  ++revision_;
  dirty_ = !std::equal(bytes.begin(), bytes.end(), saved_.begin(), saved_.end());
  return true;
}
bool TwoDDocument::Transaction(const std::function<bool()> &change) {
  if (readOnly_ || editing_)
    return false;
  auto before = Save();
  if (before.empty())
    return false;
  editing_ = true;
  bool applied = false;
  try {
    applied = change();
  } catch (...) {
    editing_ = false;
    static_cast<void>(Restore(before));
    throw;
  }
  editing_ = false;
  if (!applied || !References(scene_, atlas_, clip_, tiles_)) {
    static_cast<void>(Restore(before));
    return false;
  }
  auto after = Save();
  if (after.empty()) {
    static_cast<void>(Restore(before));
    return false;
  }
  if (before == after)
    return true;
  const auto cost = before.size() + after.size();
  if (cost > 64 * 1024 * 1024) {
    static_cast<void>(Restore(before));
    return false;
  }
  if (historyCount_ >= 256 || cost > 64 * 1024 * 1024 - historyBytes_) {
    history_ = runtime::UndoStack{};
    historyCount_ = 0;
    historyBytes_ = 0;
  }
  historyBytes_ += cost;
  dirty_ = after != saved_;
  history_.Record([this, before] { return !readOnly_ && !editing_ && Restore(before); },
                  [this, after] { return !readOnly_ && !editing_ && Restore(after); });
  ++historyCount_;
  ++revision_;
  return true;
}
bool TwoDDocument::ReplaceAtlas(twod::Atlas atlas) {
  if (!twod::ValidateAtlas(atlas))
    return false;
  return Transaction([&] {
    atlas_ = std::move(atlas);
    tiles_.atlas = atlas_.id;
    return true;
  });
}
bool TwoDDocument::ReplaceClip(twod::Clip clip) {
  if (!twod::ValidateClip(clip))
    return false;
  return Transaction([&] {
    clip_ = std::move(clip);
    return true;
  });
}
std::optional<runtime::Id> TwoDDocument::AddSprite(twod::Sprite sprite, runtime::Transform pose) {
  runtime::Id id{};
  const bool ok = Transaction([&] {
    if (sprite.source.has_value() || !runtime::IsValidTransform(pose) ||
        !twod::ValidateSprite(sprite))
      return false;
    id = world_.CreateEntity(scene_.SceneId()).id;
    runtime::WorldCommandBuffer commands;
    commands.SetTransform(id, pose);
    return commands.Apply(world_) && scene_.Set(id, sprite);
  });
  return ok ? std::optional(id) : std::nullopt;
}
bool TwoDDocument::EditSprite(runtime::Id id, twod::Sprite sprite, runtime::Transform pose) {
  return Transaction([&] {
    if (sprite.source.has_value() || !scene_.Find(id) || !scene_.Set(id, sprite))
      return false;
    runtime::WorldCommandBuffer c;
    c.SetTransform(id, pose);
    return c.Apply(world_);
  });
}
bool TwoDDocument::SetCollider(runtime::Id id, twod::Collider c) {
  return Transaction([&] { return scene_.SetCollider(id, c); });
}
bool TwoDDocument::Paint(std::span<const twod::Tile> tiles) {
  return Transaction([&] { return tiles_.Paint(tiles); });
}
bool TwoDDocument::SetChunkLoaded(twod::ChunkKey chunk, bool loaded) {
  return Transaction([&] { return tiles_.SetChunkLoaded(chunk, loaded); });
}
bool TwoDDocument::Undo() { return !readOnly_ && !editing_ && history_.Undo(); }
bool TwoDDocument::Redo() { return !readOnly_ && !editing_ && history_.Redo(); }
bool TwoDDocument::Reopen(std::span<const std::byte> bytes) {
  if (readOnly_ || editing_ || !Restore(bytes))
    return false;
  history_ = runtime::UndoStack{};
  historyCount_ = 0;
  historyBytes_ = 0;
  MarkSaved();
  return true;
}
std::optional<twod::SpriteScene> TwoDDocument::StartPlay(runtime::World &play) const {
  if (play.Kind() != runtime::WorldKind::Play)
    return std::nullopt;
  auto bytes = scene_.Snapshot();
  return bytes ? twod::SpriteScene::Load(play, *bytes) : std::nullopt;
}
bool TwoDDocument::Dirty() const { return dirty_; }
void TwoDDocument::MarkSaved() {
  saved_ = Save();
  dirty_ = false;
}
} // namespace nexora::editor
