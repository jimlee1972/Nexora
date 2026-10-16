#pragma once

#include "Nexora/Editor/TwoDPlayAssets.h"
#include "Nexora/EditorImGui/EditorImGui.h"
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

private:
  imgui::EditorImGuiHost &host_;
  std::shared_ptr<const TwoDPlayAssets> assets_;
  std::unordered_map<runtime::AssetUuid, std::uint64_t, runtime::AssetUuidHash> textures_;
};
} // namespace nexora::editor::preview
