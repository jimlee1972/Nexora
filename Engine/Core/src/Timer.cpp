#include "Nexora/Core/Timer.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>

namespace nexora::core {
TimerHandle TimerScheduler::Schedule(double delay, std::function<void()> callback,
                                     double repeat_interval) {
  if (!std::isfinite(delay) || delay < 0 || !std::isfinite(repeat_interval) ||
      repeat_interval < 0 || !callback) {
    throw std::invalid_argument("invalid timer descriptor");
  }
  const auto handle = handles_.Create();
  timers_.push_back({handle, time_ + delay, repeat_interval, std::move(callback)});
  return handle;
}

bool TimerScheduler::Cancel(TimerHandle handle) {
  if (!handles_.Destroy(handle))
    return false;
  std::erase_if(timers_, [handle](const Timer &timer) { return timer.handle == handle; });
  return true;
}

void TimerScheduler::Advance(double delta) {
  if (!std::isfinite(delta) || delta < 0)
    throw std::invalid_argument("timer delta is invalid");
  time_ += delta;
  std::vector<std::function<void()>> callbacks;
  for (auto &timer : timers_) {
    if (timer.deadline <= time_) {
      callbacks.push_back(timer.callback);
      if (timer.interval > 0) {
        do {
          timer.deadline += timer.interval;
        } while (timer.deadline <= time_);
      } else {
        (void)handles_.Destroy(timer.handle);
        timer.callback = {};
      }
    }
  }
  std::erase_if(timers_, [](const Timer &timer) { return !timer.callback; });
  for (const auto &callback : callbacks)
    callback();
}
} // namespace nexora::core
