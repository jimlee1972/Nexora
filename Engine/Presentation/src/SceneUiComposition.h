#pragma once
#include "Nexora/Presentation/Surface.h"
#include <cstring>

namespace Nexora::Presentation::detail {
[[nodiscard]] NEXORA_PRESENTATION_API std::uint64_t NextSceneTextureDomain() noexcept;

// No descriptor, image or library pointer escapes the surface. At most two independent LDR
// views may be prepared before the one UI pass. One sampled view can instead precede a disjoint
// direct view. HDR/shadow and untagged offscreen workflows retain their exclusive admission.
class SceneUiComposition final {
public:
  SceneUiComposition() = default;
  SceneUiComposition(const SceneUiComposition &) = delete;
  SceneUiComposition &operator=(const SceneUiComposition &) = delete;
  struct Plan final {
    bool valid{}, sampled{};
    SceneViewport bounds{};
    std::array<bool, 2> views{};
  };
  void Begin(std::uint64_t frame) noexcept {
    frame_ = frame;
    references_ = {};
    latest_.reset();
    sampled_ = false;
  }
  [[nodiscard]] bool Publish(std::size_t view) noexcept {
    if (!domain_ || !frame_ || view >= references_.size() || references_[view])
      return false;
    latest_ = SceneTextureReference{domain_, frame_, static_cast<std::uint32_t>(view)};
    references_[view] = latest_;
    return true;
  }
  [[nodiscard]] std::optional<SceneTextureReference> Reference() const noexcept { return latest_; }
  void Complete() noexcept { sampled_ = true; }
  [[nodiscard]] bool Sampled() const noexcept { return sampled_; }
  [[nodiscard]] Plan Validate(const UiDrawData &data, std::uint32_t width,
                              std::uint32_t height) const noexcept {
    if (!width || !height)
      return {};
    Plan plan{true};
    const auto stride = data.indices32Bit ? 4U : 2U;
    for (const auto &command : data.commands) {
      if (command.sceneTexture == SceneTextureReference{})
        continue;
      const auto view = command.sceneTexture.view;
      if (sampled_ || view >= references_.size() || !references_[view] ||
          command.sceneTexture != *references_[view] || command.textureId ||
          command.instanceCount || command.firstInstance || !command.elementCount ||
          command.elementCount % 3 || data.indices.size() % stride ||
          command.indexOffset > data.indices.size() / stride ||
          command.elementCount > data.indices.size() / stride - command.indexOffset ||
          command.clipX < 0 || command.clipY < 0 || !command.clipWidth || !command.clipHeight ||
          std::uint32_t(command.clipX) > width ||
          command.clipWidth > width - std::uint32_t(command.clipX) ||
          std::uint32_t(command.clipY) > height ||
          command.clipHeight > height - std::uint32_t(command.clipY))
        return {};
      for (std::size_t i = 0; i < command.elementCount; ++i) {
        const auto offset = (std::size_t(command.indexOffset) + i) * stride;
        std::uint32_t index{};
        if (data.indices32Bit)
          std::memcpy(&index, data.indices.data() + offset, 4);
        else {
          std::uint16_t small{};
          std::memcpy(&small, data.indices.data() + offset, 2);
          index = small;
        }
        const auto vertex = std::int64_t(index) + command.vertexOffset;
        if (vertex < 0 || std::uint64_t(vertex) >= data.vertices.size())
          return {};
        const auto &value = data.vertices[std::size_t(vertex)];
        for (float f : {value.position[0], value.position[1], value.uv[0], value.uv[1]})
          if (!std::isfinite(f) || std::abs(f) > 1e12F)
            return {};
      }
      const SceneViewport clip{std::uint32_t(command.clipX), std::uint32_t(command.clipY),
                               command.clipWidth, command.clipHeight};
      if (!plan.sampled)
        plan.bounds = clip;
      else {
        const auto right = std::max(plan.bounds.x + plan.bounds.width, clip.x + clip.width);
        const auto bottom = std::max(plan.bounds.y + plan.bounds.height, clip.y + clip.height);
        plan.bounds.x = std::min(plan.bounds.x, clip.x);
        plan.bounds.y = std::min(plan.bounds.y, clip.y);
        plan.bounds.width = right - plan.bounds.x;
        plan.bounds.height = bottom - plan.bounds.y;
      }
      plan.sampled = true;
      plan.views[view] = true;
    }
    return plan;
  }

private:
  const std::uint64_t domain_{NextSceneTextureDomain()};
  std::uint64_t frame_{};
  std::array<std::optional<SceneTextureReference>, 2> references_{};
  std::optional<SceneTextureReference> latest_;
  bool sampled_{};
};
} // namespace Nexora::Presentation::detail
