#include "Nexora/Runtime/GameplayLifecycle.h"
#include <cmath>
#include <cstring>
namespace nexora::runtime {
GameplayLifecycle::GameplayLifecycle(Rumble output, void *context)
    : output_(output), context_(context) {
  state_.struct_size = sizeof(state_);
  state_.flags = 1;
}
GameplayLifecycle::~GameplayLifecycle() { (void)Stop(); }
bool GameplayLifecycle::Owner() const noexcept { return owner_ == std::this_thread::get_id(); }
bool GameplayLifecycle::Flag(uint32_t bit, bool value, bool interrupt) {
  if (!Owner())
    return false;
  if (bool(state_.flags & bit) == value)
    return true;
  state_.flags = value ? state_.flags | bit : state_.flags & ~bit;
  ++state_.sequence;
  if (interrupt)
    ++state_.interruption_sequence;
  return !interrupt || Stop();
}
bool GameplayLifecycle::Focus(bool value) { return Flag(1, value, !value); }
bool GameplayLifecycle::Minimize(bool value) { return Flag(2, value, value); }
bool GameplayLifecycle::Suspend(bool value) {
  if (!Owner())
    return false;
  const bool changed = bool(state_.flags & 4) != value;
  const bool ok = Flag(4, value, value);
  if (changed)
    ++(value ? state_.suspend_sequence : state_.resume_sequence);
  return ok;
}
bool GameplayLifecycle::Devices(uint32_t connected, uint32_t rumble) {
  if (!Owner() || (connected | rumble) > 15 || (rumble & ~connected))
    return false;
  const auto lost = state_.connected_mask & ~connected;
  for (uint32_t i = 0; i < 4; ++i)
    if (lost & (1u << i)) {
      ++state_.disconnect_sequence[i];
      running_[i] = false;
    }
  if (state_.connected_mask != connected || state_.rumble_mask != rumble)
    ++state_.sequence;
  state_.connected_mask = connected;
  state_.rumble_mask = rumble;
  return true;
}
bool GameplayLifecycle::Disconnected(uint32_t device) {
  if (!Owner() || device >= 4)
    return false;
  ++state_.disconnect_sequence[device];
  ++state_.sequence;
  running_[device] = false;
  return true;
}
bool GameplayLifecycle::RequestClose() {
  if (!Owner())
    return false;
  if (!guarded_) {
    exit_ = true;
    return true;
  }
  if (!state_.close_token) {
    state_.close_token = ++next_token_;
    ++state_.sequence;
  }
  return Stop();
}
bool GameplayLifecycle::Tick(Clock::time_point now) {
  if (!Owner())
    return false;
  bool ok = true;
  for (uint32_t i = 0; i < 4; ++i)
    if (running_[i] && (state_.flags != 1 || state_.close_token || now >= deadlines_[i])) {
      if (output_ && output_(context_, i, 0, 0) == 0)
        running_[i] = false;
      else
        ok = false; // Retry zero next tick; never replay a nonzero command.
    }
  return ok;
}
bool GameplayLifecycle::Stop() {
  if (!Owner())
    return false;
  for (auto &deadline : deadlines_)
    deadline = Clock::time_point::min();
  return Tick();
}
int32_t GameplayLifecycle::SetRumble(uint32_t device, float low, float high, uint32_t duration) {
  if (!Owner())
    return -3;
  if (device >= 4 || !std::isfinite(low) || !std::isfinite(high) || low < 0 || low > 1 ||
      high < 0 || high > 1 || duration > 5000 || ((low || high) && !duration))
    return -1;
  if (!output_ || !(state_.rumble_mask & (1u << device)))
    return -2;
  if ((low || high) && (state_.flags != 1 || state_.close_token || exit_))
    return -3;
  const auto result = output_(context_, device, low, high);
  if (result != 0)
    return result;
  running_[device] = low || high;
  deadlines_[device] = Clock::now() + std::chrono::milliseconds(duration);
  return 0;
}
NexoraGameplayLifecycleApiV1 GameplayLifecycle::Api() noexcept {
  return {sizeof(NexoraGameplayLifecycleApiV1),
          1,
          this,
          [](void *p, NexoraLifecycleStateV1 *out) -> int32_t {
            if (!p || !out)
              return -1;
            auto &s = *static_cast<GameplayLifecycle *>(p);
            if (!s.Owner())
              return -3;
            if (out->struct_size < sizeof(*out))
              return -2;
            const auto size = out->struct_size;
            std::memcpy(out, &s.state_, sizeof(*out));
            out->struct_size = size;
            return 0;
          },
          [](void *p, uint32_t enabled) -> int32_t {
            if (!p || enabled > 1)
              return -1;
            auto &s = *static_cast<GameplayLifecycle *>(p);
            if (!s.Owner())
              return -3;
            if (s.state_.close_token || s.exit_)
              return -3;
            s.guarded_ = enabled != 0;
            return 0;
          },
          [](void *p, uint64_t token, uint32_t accept) -> int32_t {
            if (!p || accept > 1)
              return -1;
            auto &s = *static_cast<GameplayLifecycle *>(p);
            if (!s.Owner())
              return -3;
            if (!token || token != s.state_.close_token)
              return -1;
            s.state_.close_token = 0;
            s.exit_ = accept != 0;
            ++s.state_.sequence;
            return 0;
          },
          [](void *p, uint32_t d, float l, float h, uint32_t ms) -> int32_t {
            return p ? static_cast<GameplayLifecycle *>(p)->SetRumble(d, l, h, ms) : -1;
          }};
}
} // namespace nexora::runtime
