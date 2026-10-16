#pragma once
#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <span>

namespace nexora::runtime::detail {
// Owner-thread bounded PCM16 copies. The driver may borrow a slot until Pending becomes false.
// Stop/reset every native borrower before Reset/destruction; addresses never move.
class GameplayAudioQueue final {
public:
  static constexpr std::size_t Slots = 4;
  static constexpr std::uint32_t MaximumFrames = 4096;
  struct Driver {
    void *context{};
    bool (*pending)(void *, std::size_t){};
    bool (*submit)(void *, std::size_t, std::uint32_t){};
    bool (*wait)(void *, std::uint32_t milliseconds){};
  };
  GameplayAudioQueue() = default;
  GameplayAudioQueue(const GameplayAudioQueue &) = delete;
  GameplayAudioQueue &operator=(const GameplayAudioQueue &) = delete;
  [[nodiscard]] std::span<std::int16_t> Buffer(std::size_t slot) { return buffers_[slot]; }
  void Reset() noexcept { next_ = 0; }
  bool Write(std::span<const float> stereo, std::uint32_t frames, Driver driver) {
    if (!frames || frames > MaximumFrames || stereo.size() != frames * 2 || !driver.pending ||
        !driver.submit || !driver.wait ||
        !std::all_of(stereo.begin(), stereo.end(), [](float v) { return std::isfinite(v); }))
      return false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(2);
    while (driver.pending(driver.context, next_)) {
      const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(
                            deadline - std::chrono::steady_clock::now())
                            .count();
      if (left <= 0 || !driver.wait(driver.context, static_cast<std::uint32_t>(left)))
        return false;
    }
    auto &buffer = buffers_[next_];
    for (std::size_t i = 0; i < stereo.size(); ++i)
      buffer[i] = static_cast<std::int16_t>(std::clamp(stereo[i] * 32768.F, -32768.F, 32767.F));
    if (!driver.submit(driver.context, next_, frames))
      return false;
    next_ = (next_ + 1) % Slots;
    return true; // Submitted storage remains owned; do not wait for its playback here.
  }

private:
  std::array<std::array<std::int16_t, MaximumFrames * 2>, Slots> buffers_{};
  std::size_t next_{};
};
} // namespace nexora::runtime::detail
