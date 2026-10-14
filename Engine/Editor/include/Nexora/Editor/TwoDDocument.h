#pragma once
#include "Nexora/Editor/Api.h"
#include "Nexora/TwoD/Tilemap.h"
namespace nexora::editor {
// An isolated authoring document, using Runtime World and UndoStack. Owner-thread only;
// no disk IO or project permission bypass. The project host owns import/save publication.
class NEXORA_EDITOR_API TwoDDocument final {
public:
  TwoDDocument();
  TwoDDocument(const TwoDDocument &) = delete;
  TwoDDocument &operator=(const TwoDDocument &) = delete;
  [[nodiscard]] const runtime::World &World() const noexcept { return world_; }
  [[nodiscard]] const twod::SpriteScene &Scene() const noexcept { return scene_; }
  [[nodiscard]] const twod::Atlas &Atlas() const noexcept { return atlas_; }
  [[nodiscard]] const twod::Clip &Clip() const noexcept { return clip_; }
  [[nodiscard]] const twod::Tilemap &Tiles() const noexcept { return tiles_; }
  bool ReplaceAtlas(twod::Atlas atlas);
  bool ReplaceClip(twod::Clip clip);
  [[nodiscard]] std::optional<runtime::Id> AddSprite(twod::Sprite sprite,
                                                     runtime::Transform pose = {});
  bool EditSprite(runtime::Id id, twod::Sprite sprite, runtime::Transform pose);
  bool SetCollider(runtime::Id id, twod::Collider collider);
  bool Paint(std::span<const twod::Tile> tiles);
  bool SetChunkLoaded(twod::ChunkKey chunk, bool loaded);
  bool Undo();
  bool Redo();
  [[nodiscard]] twod::Bytes Save() const;
  // Reopen validates all assets/references in a staging World, then resets history.
  bool Reopen(std::span<const std::byte> bytes);
  [[nodiscard]] std::optional<twod::SpriteScene> StartPlay(runtime::World &isolatedPlay) const;
  [[nodiscard]] bool Dirty() const;
  void MarkSaved();
  void SetReadOnly(bool value) noexcept { readOnly_ = value; }
  [[nodiscard]] bool ReadOnly() const noexcept { return readOnly_; }
  [[nodiscard]] std::uint64_t Revision() const noexcept { return revision_; }

private:
  bool Transaction(const std::function<bool()> &change);
  bool Restore(std::span<const std::byte> bytes);
  runtime::World world_;
  twod::SpriteScene scene_;
  twod::Atlas atlas_;
  twod::Clip clip_;
  twod::Tilemap tiles_;
  runtime::UndoStack history_;
  twod::Bytes saved_;
  std::size_t historyCount_{}, historyBytes_{};
  std::uint64_t revision_{};
  bool readOnly_{}, editing_{}, dirty_{};
};
} // namespace nexora::editor
