#pragma once

#include "Nexora/Editor/TwoDPlayAssets.h"
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/TwoD/Api.h"
#include "Nexora/TwoD/GameplayUi.h"
#include "Nexora/TwoD/Scene.h"
#include <memory>
#include <unordered_map>

namespace nexora::editor::preview {
// The GUI host outlives this owner. Sync/Clear run between GUI frames. Register may run while
// assembling a frame, before EndFrame/Render. Native cache slots retire behind surface fences.
class GameTwoDTextures final {
public:
  explicit GameTwoDTextures(imgui::EditorImGuiHost &host) : host_(host) {}
  ~GameTwoDTextures() { Clear(); }
  GameTwoDTextures(const GameTwoDTextures &) = delete;
  GameTwoDTextures &operator=(const GameTwoDTextures &) = delete;
  // Returns true when the Play owner changes, so the application can reset the view selection.
  bool Sync(std::shared_ptr<const TwoDPlayAssets> assets) {
    if (assets_ != assets) {
      Clear();
      assets_ = std::move(assets);
      return true;
    }
    return false;
  }
  void Clear() noexcept {
    for (const auto &[id, texture] : textures_) {
      static_cast<void>(id);
      static_cast<void>(host_.UnregisterTexture(texture));
    }
    textures_.clear();
    for (const auto &[source, texture] : ui_textures_) {
      static_cast<void>(source);
      static_cast<void>(host_.UnregisterTexture(texture));
    }
    ui_textures_.clear();
    assets_.reset();
  }
  [[nodiscard]] std::uint64_t Resolve(const twod::Atlas &atlas) {
    if (!assets_ || assets_->Atlas(atlas.id).get() != &atlas)
      return 0;
    if (const auto found = textures_.find(atlas.id); found != textures_.end())
      return found->second;
    if (textures_.size() == 64)
      return 0;
    const auto [entry, inserted] = textures_.try_emplace(atlas.id, 0);
    static_cast<void>(inserted);
    try {
      const auto texture =
          host_.RegisterNativeTexture(atlas.width, atlas.height, atlas.rgba, atlas.nearest);
      if (texture)
        entry->second = texture;
      else
        textures_.erase(entry);
      return texture;
    } catch (...) {
      textures_.erase(entry);
      throw;
    }
  }
  [[nodiscard]] std::size_t Size() const noexcept { return textures_.size(); }
  // Append HUD quads to the same Game draw list, retaining Editor popup/window ordering.
  // Native registrations copy immutable pixels and share the existing 64-slot GUI budget.
  bool AppendUi(twod::SpriteFrame &frame, const twod::GameplayUiFrame &ui) {
    if (frame.vertices.size() + ui.vertices.size() > 4096 * 4 ||
        frame.indices.size() + ui.indices.size() > 4096 * 6 ||
        frame.batches.size() + ui.commands.size() > 4096)
      return false;
    for (const auto &upload : ui.uploads) {
      if (ui_textures_.contains(upload.textureId))
        continue;
      const auto [entry, inserted] = ui_textures_.try_emplace(upload.textureId, 0);
      static_cast<void>(inserted);
      try {
        entry->second =
            host_.RegisterNativeTexture(upload.width, upload.height, upload.pixels, upload.nearest);
      } catch (...) {
        ui_textures_.erase(entry);
        throw;
      }
      if (!entry->second) {
        ui_textures_.erase(entry);
        return false;
      }
    }
    for (const auto &command : ui.commands)
      if (!ui_textures_.contains(command.textureId))
        return false;
    const auto vertices = static_cast<std::int32_t>(frame.vertices.size());
    const auto indices = static_cast<std::uint32_t>(frame.indices.size());
    frame.vertices.insert(frame.vertices.end(), ui.vertices.begin(), ui.vertices.end());
    frame.indices.insert(frame.indices.end(), ui.indices.begin(), ui.indices.end());
    for (auto command : ui.commands) {
      command.vertexOffset += vertices;
      command.indexOffset += indices;
      command.textureId = ui_textures_.at(command.textureId);
      frame.batches.push_back(command);
    }
    return true;
  }

private:
  imgui::EditorImGuiHost &host_;
  std::shared_ptr<const TwoDPlayAssets> assets_;
  std::unordered_map<runtime::AssetUuid, std::uint64_t, runtime::AssetUuidHash> textures_;
  std::unordered_map<std::uint64_t, std::uint64_t> ui_textures_;
};
} // namespace nexora::editor::preview
