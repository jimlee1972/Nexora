#pragma once
#include "Nexora/Runtime/Api.h"
#include "nexora/gameplay_lifecycle.h"
#include <array>
#include <chrono>
#include <thread>
namespace nexora::runtime {
// Backend and context borrow the provider lifetime. No worker, allocation or deferred save.
class NEXORA_RUNTIME_API GameplayLifecycle final {
public:
  using Clock = std::chrono::steady_clock;
  using Rumble = int32_t (*)(void *, uint32_t, float, float);
  explicit GameplayLifecycle(Rumble output = nullptr, void *context = nullptr);
  ~GameplayLifecycle();
  GameplayLifecycle(const GameplayLifecycle &) = delete;
  GameplayLifecycle &operator=(const GameplayLifecycle &) = delete;
  NexoraGameplayLifecycleApiV1 Api() noexcept;
  bool Focus(bool focused);
  bool Minimize(bool minimized);
  bool Suspend(bool suspended);
  bool Devices(uint32_t connected, uint32_t rumble);
  bool Disconnected(uint32_t device);
  bool RequestClose();
  bool Tick(Clock::time_point now = Clock::now());
  bool Stop();
  bool Guarded() const noexcept { return guarded_; }
  bool ExitRequested() const noexcept { return exit_; }

private:
  bool Owner() const noexcept;
  bool Flag(uint32_t bit, bool value, bool interrupt);
  int32_t SetRumble(uint32_t device, float low, float high, uint32_t duration);
  NexoraLifecycleStateV1 state_{};
  const std::thread::id owner_{std::this_thread::get_id()};
  Rumble output_{};
  void *context_{};
  std::array<bool, 4> running_{};
  std::array<Clock::time_point, 4> deadlines_{};
  uint64_t next_token_{};
  bool guarded_{}, exit_{};
};
} // namespace nexora::runtime
