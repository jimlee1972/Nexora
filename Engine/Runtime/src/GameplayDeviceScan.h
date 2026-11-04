#pragma once
#include <atomic>
#include <chrono>

namespace nexora::runtime::detail {
// Only Notify may run off the adapter's owner thread. Claim notifications before enumeration so
// an arrival/removal racing that enumeration remains pending for the following input frame.
class DeviceScanSchedule {
public:
  using Clock = std::chrono::steady_clock;
  void Notify() noexcept { pending_.store(true, std::memory_order_release); }
  bool Begin(Clock::time_point now, bool notificationsAvailable) noexcept {
    if (now < retryAt_)
      return false;
    if (notificationsAvailable)
      return pending_.exchange(false, std::memory_order_acq_rel);
    if (now < nextFallback_)
      return false;
    nextFallback_ = now + std::chrono::seconds(1);
    return true;
  }
  void RetryAfterFailure(Clock::time_point completedAt) noexcept {
    pending_.store(true, std::memory_order_release);
    retryAt_ = completedAt + std::chrono::seconds(1);
  }

private:
  std::atomic<bool> pending_{true};
  Clock::time_point retryAt_{}, nextFallback_{};
};
} // namespace nexora::runtime::detail
