#pragma once

#include "Nexora/Presentation/Surface.h"

namespace Nexora::Presentation::detail {

// Existing single-scene workflows stay exclusive. A pair is admitted only for two disjoint
// explicit direct LDR, non-shadow viewports after UI. Commit only after successful recording.
class SceneViewAdmission final {
public:
  static constexpr std::size_t kMaximumViews = 2;
  [[nodiscard]] std::size_t Count() const noexcept { return count_; }
  void Reset() noexcept {
    count_ = 0;
    pair_allowed_ = false;
  }
  [[nodiscard]] bool CanSubmit(const SceneDrawData &data, std::uint32_t width, std::uint32_t height,
                               bool after_ui) const noexcept {
    const auto viewport = ResolveSceneViewport(data.viewport, width, height);
    if (!viewport)
      return false;
    if (!count_)
      return true;
    return count_ < kMaximumViews && pair_allowed_ && PairEligible(data, after_ui) &&
           !SceneViewportsOverlap(first_, *viewport);
  }
  bool Commit(const SceneDrawData &data, std::uint32_t width, std::uint32_t height,
              bool after_ui) noexcept {
    if (!CanSubmit(data, width, height, after_ui))
      return false;
    if (!count_) {
      first_ = *ResolveSceneViewport(data.viewport, width, height);
      pair_allowed_ = PairEligible(data, after_ui);
    }
    ++count_;
    return true;
  }

private:
  static bool PairEligible(const SceneDrawData &data, bool after_ui) noexcept {
    return after_ui && !data.offscreen && !data.hdr && !data.shadow && data.viewport.width &&
           data.viewport.height;
  }
  SceneViewport first_{};
  std::size_t count_{};
  bool pair_allowed_{};
};
} // namespace Nexora::Presentation::detail
