#pragma once
#include "ScenePreviewCandidates.h"
#include <array>

namespace nexora::editor::preview {
// Application-owned Scene texture IDs. This is the Editor's only Scene RGBA upload producer;
// the independent GUI cache has its own IDs. Future Scene texture producers must share this owner.
// Keep this owner across project replacement; never recycle an immutable ID within a surface.
class SceneSpriteTextures final {
public:
  [[nodiscard]] bool Prepare(const NativeSceneMeshes &meshes) {
    decltype(bindings_) candidate;
    std::uint64_t next = next_;
    for (const auto &[entity, range] : meshes.sprite_ranges) {
      static_cast<void>(range);
      if (!meshes.sprites)
        return false;
      const auto found = meshes.sprites->entities.find(entity);
      if (found == meshes.sprites->entities.end())
        return false;
      const auto &atlas = found->second->assets.atlas;
      if (const auto present = candidate.find(atlas->id); present != candidate.end()) {
        if (present->second.atlas != atlas)
          return false;
        continue;
      }
      if (candidate.size() == 16)
        return false;
      const auto old = bindings_.find(atlas->id);
      const bool retain = old != bindings_.end() && old->second.atlas == atlas;
      if (!retain && next >= UINT64_MAX - 1)
        return false;
      candidate.emplace(atlas->id, Binding{atlas, retain ? old->second.id : next++});
    }
    std::array<std::uint64_t, 16> retired{};
    std::size_t count{};
    for (const auto &[asset, binding] : bindings_) {
      const auto found = candidate.find(asset);
      if (found == candidate.end() || found->second.id != binding.id)
        retired[count++] = binding.id;
    }
    if (count > pending_.size() - pending_count_)
      return false;
    for (std::size_t index = 0; index < count; ++index)
      pending_[pending_count_++] = retired[index];
    bindings_.swap(candidate);
    next_ = next;
    return true;
  }
  [[nodiscard]] std::uint64_t Resolve(const twod::Atlas &atlas) const noexcept {
    const auto found = bindings_.find(atlas.id);
    return found != bindings_.end() && found->second.atlas.get() == &atlas ? found->second.id : 0;
  }
  [[nodiscard]] std::vector<Nexora::Presentation::UiTextureUpload> Uploads() const {
    std::vector<Nexora::Presentation::UiTextureUpload> result;
    result.reserve(bindings_.size());
    for (const auto &[asset, binding] : bindings_) {
      static_cast<void>(asset);
      const auto &atlas = *binding.atlas;
      result.push_back({binding.id, atlas.width, atlas.height, atlas.width * 4, atlas.rgba,
                        atlas.nearest, true});
    }
    return result;
  }
  [[nodiscard]] bool Clear() noexcept {
    if (bindings_.size() > pending_.size() - pending_count_)
      return false;
    for (const auto &[asset, binding] : bindings_) {
      static_cast<void>(asset);
      pending_[pending_count_++] = binding.id;
    }
    bindings_.clear();
    return true;
  }
  // Call before BeginFrame and before final surface drain, including on project replacement.
  template <class Surface>
  [[nodiscard]] Nexora::Presentation::SurfaceStatus Retire(Surface &surface) {
    if (!pending_count_)
      return Nexora::Presentation::SurfaceStatus::Ready;
    const auto status = surface.ReleaseSceneTextures(std::span(pending_).first(pending_count_));
    if (status == Nexora::Presentation::SurfaceStatus::Ready)
      pending_count_ = 0;
    return status;
  }
  [[nodiscard]] std::size_t Size() const noexcept { return bindings_.size(); }

private:
  struct Binding final {
    std::shared_ptr<const twod::Atlas> atlas;
    std::uint64_t id{};
  };
  std::unordered_map<runtime::AssetUuid, Binding, runtime::AssetUuidHash> bindings_;
  std::uint64_t next_{1};
  std::array<std::uint64_t, 64> pending_{};
  std::size_t pending_count_{};
};
} // namespace nexora::editor::preview
