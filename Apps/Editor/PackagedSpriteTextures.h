#pragma once
#include "Nexora/EditorImGui/EditorImGui.h"
#include "Nexora/TwoD/GameSpriteAssets.h"
#include <unordered_map>

namespace nexora::editor::preview {
// Whole immutable package admission, before borrowing its World or invoking any module.
// The GUI host owns copied pixels; native replacements retire behind the surface's fences.
class PackagedSpriteTextures final {
public:
  explicit PackagedSpriteTextures(imgui::EditorImGuiHost &host) : host_(host) {}
  ~PackagedSpriteTextures() {
    for (const auto &[asset, binding] : textures_) {
      static_cast<void>(asset);
      if (binding.native)
        static_cast<void>(host_.UnregisterTexture(binding.native));
    }
  }
  PackagedSpriteTextures(const PackagedSpriteTextures &) = delete;
  PackagedSpriteTextures &operator=(const PackagedSpriteTextures &) = delete;
  bool Prepare(const runtime::LoadedGameProject &project, const twod::GameSpriteAssets &assets) {
    if (!textures_.empty())
      return false;
    for (const auto &blob : project.SpriteAssets()) {
      if (blob.type != "sprite_atlas")
        continue;
      const auto atlas = assets.ResolveAtlas(blob.id);
      if (!atlas || textures_.size() == 64)
        return false;
      const auto [entry, inserted] = textures_.try_emplace(blob.id);
      if (!inserted)
        return false;
      entry->second.atlas = atlas;
      entry->second.native =
          host_.RegisterNativeTexture(atlas->width, atlas->height, atlas->rgba, atlas->nearest);
      if (!entry->second.native)
        return false;
    }
    return true;
  }
  [[nodiscard]] std::optional<twod::SpriteFrame> Remap(const twod::SpriteFrame &source) const {
    if (source.vertices.size() > 4096 * 4 || source.indices.size() > 4096 * 6 ||
        source.batches.size() > 4096)
      return {};
    std::unordered_map<std::uint64_t, std::uint64_t> bindings;
    for (const auto &binding : source.retainedAtlases) {
      if (!binding.atlas)
        return {};
      const auto found = textures_.find(binding.atlas->id);
      if (found == textures_.end() || !found->second.native ||
          found->second.atlas.get() != binding.atlas.get() ||
          !bindings.emplace(binding.texture, found->second.native).second)
        return {};
    }
    auto result = source;
    for (auto &command : result.batches) {
      const auto found = bindings.find(command.textureId);
      if (found == bindings.end())
        return {};
      command.textureId = found->second;
    }
    return result;
  }
  [[nodiscard]] std::size_t Size() const noexcept { return textures_.size(); }

private:
  struct Binding final {
    std::shared_ptr<const twod::Atlas> atlas;
    std::uint64_t native{};
  };
  imgui::EditorImGuiHost &host_;
  std::unordered_map<runtime::AssetUuid, Binding, runtime::AssetUuidHash> textures_;
};
} // namespace nexora::editor::preview
