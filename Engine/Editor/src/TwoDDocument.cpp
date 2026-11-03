#include "Nexora/Editor/TwoDDocument.h"
#include "Nexora/TwoD/AuthoredScene.h"
#include <algorithm>
#include <cstring>
namespace nexora::editor {
TwoDDocument::TwoDDocument() : scene_(world_, world_.LoadScene("2D document")) { saved_ = Save(); }
twod::Bytes TwoDDocument::Save() const {
  return twod::EncodeAuthoredScene(scene_, atlas_, clip_, tiles_);
}
bool TwoDDocument::Restore(std::span<const std::byte> bytes) {
  const auto decoded = twod::AuthoredScene::Decode(bytes);
  if (!decoded)
    return false;
  // Allocate copies before publication so failures cannot partially replace document owners.
  auto atlas = decoded->atlas;
  auto clip = decoded->clip;
  auto tiles = decoded->tiles;
  const auto snapshot = decoded->scene->Snapshot();
  if (!snapshot || !scene_.Restore(*snapshot))
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
  if (!applied || !twod::ValidateSceneReferences(scene_, atlas_, clip_, tiles_)) {
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
