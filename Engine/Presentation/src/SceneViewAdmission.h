#pragma once

#include "Nexora/Presentation/Surface.h"

namespace Nexora::Presentation::detail {

// Existing single-scene workflows stay exclusive. Two disjoint explicit direct LDR, non-shadow
// views may follow UI; an opt-in sampled offscreen view reserves its UI clips instead of the
// entire target. Commit only after successful recording.
class SceneViewAdmission final {
public:
  static constexpr std::size_t kMaximumViews = 2;
  [[nodiscard]] std::size_t Count() const noexcept { return count_; }
  void Reset() noexcept {
    count_ = 0;
    pair_allowed_ = false;
    sample_pair_eligible_ = false;
  }
  [[nodiscard]] bool CanSubmit(const SceneDrawData &data, std::uint32_t width, std::uint32_t height,
                               bool after_ui) const noexcept {
    if (data.sampledByUi && (!data.offscreen || data.hdr))
      return false;
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
      sample_pair_eligible_ = data.sampledByUi && data.offscreen && !data.hdr && !data.shadow;
    }
    ++count_;
    return true;
  }
  // The exclusive offscreen attachment was sampled inside the one UI submission. Conservatively
  // reserve every sampled command's clip bounds; only a disjoint direct LDR view may follow.
  void SampledOffscreen(SceneViewport bounds) noexcept {
    if (count_ == 1 && sample_pair_eligible_ && !pair_allowed_ && bounds.width && bounds.height) {
      first_ = bounds;
      pair_allowed_ = true;
      sample_pair_eligible_ = false;
    }
  }

private:
  static bool PairEligible(const SceneDrawData &data, bool after_ui) noexcept {
    return after_ui && !data.offscreen && !data.hdr && !data.shadow && data.viewport.width &&
           data.viewport.height;
  }
  SceneViewport first_{};
  std::size_t count_{};
  bool pair_allowed_{};
  bool sample_pair_eligible_{};
};
} // namespace Nexora::Presentation::detail
