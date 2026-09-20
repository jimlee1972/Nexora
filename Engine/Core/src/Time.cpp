#include "Nexora/Core/Time.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace nexora::core {
FixedTickClock::FixedTickClock(double fixed_step, std::uint32_t max_catch_up_ticks,
                               double accumulator_clamp)
    : fixed_step_(fixed_step), max_catch_up_ticks_(max_catch_up_ticks),
      accumulator_clamp_(accumulator_clamp) {
  if (!std::isfinite(fixed_step) || fixed_step <= 0 || max_catch_up_ticks == 0 ||
      !std::isfinite(accumulator_clamp) || accumulator_clamp < fixed_step) {
    throw std::invalid_argument("invalid fixed tick configuration");
  }
}

TickResult FixedTickClock::Advance(double unscaled_delta) {
  if (!std::isfinite(unscaled_delta) || unscaled_delta < 0) {
    throw std::invalid_argument("time delta must be finite and non-negative");
  }
  state_.unscaled_time += unscaled_delta;
  if (!state_.paused) {
    const auto scaled = unscaled_delta * static_cast<double>(state_.time_scale);
    state_.game_time += scaled;
    accumulator_ = std::min(accumulator_ + scaled, accumulator_clamp_);
  }
  std::uint32_t ticks = 0;
  while (accumulator_ >= fixed_step_ && ticks < max_catch_up_ticks_) {
    accumulator_ -= fixed_step_;
    ++state_.fixed_tick;
    ++ticks;
  }
  return {ticks, accumulator_ / fixed_step_};
}

void FixedTickClock::SetTimeScale(float scale) {
  if (!std::isfinite(scale) || scale < 0)
    throw std::invalid_argument("time scale must be non-negative");
  state_.time_scale = scale;
}
} // namespace nexora::core
