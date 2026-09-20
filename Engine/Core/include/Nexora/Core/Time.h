#pragma once

#include "Nexora/Core/Api.h"

#include <cstdint>

namespace nexora::core {

struct WorldTimeState final {
  double game_time{};
  double unscaled_time{};
  std::uint64_t fixed_tick{};
  float time_scale{1.0F};
  bool paused{false};
};

struct TickResult final {
  std::uint32_t fixed_ticks{};
  double alpha{};
};

class NEXORA_CORE_API FixedTickClock final {
public:
  FixedTickClock(double fixed_step, std::uint32_t max_catch_up_ticks, double accumulator_clamp);
  [[nodiscard]] TickResult Advance(double unscaled_delta);
  [[nodiscard]] const WorldTimeState &State() const noexcept { return state_; }
  void SetTimeScale(float scale);
  void SetPaused(bool paused) noexcept { state_.paused = paused; }

private:
  WorldTimeState state_;
  double fixed_step_;
  std::uint32_t max_catch_up_ticks_;
  double accumulator_clamp_;
  double accumulator_{};
};

} // namespace nexora::core
