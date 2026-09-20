#pragma once

#include "Nexora/Core/Api.h"

#include "Nexora/Core/Handle.h"

#include <cstdint>
#include <functional>
#include <vector>

namespace nexora::core {
struct TimerTag;
using TimerHandle = Handle<TimerTag>;

class NEXORA_CORE_API TimerScheduler final {
public:
  [[nodiscard]] TimerHandle Schedule(double delay, std::function<void()> callback,
                                     double repeat_interval = 0.0);
  [[nodiscard]] bool Cancel(TimerHandle handle);
  void Advance(double delta);
  [[nodiscard]] double Time() const noexcept { return time_; }

private:
  struct Timer final {
    TimerHandle handle;
    double deadline;
    double interval;
    std::function<void()> callback;
  };
  HandlePool<TimerTag> handles_;
  std::vector<Timer> timers_;
  double time_{};
};
} // namespace nexora::core
