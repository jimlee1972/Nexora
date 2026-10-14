#pragma once

#include "Nexora/Foundation/GameplayABI.h"
#include <array>
#include <cmath>
#include <span>

namespace nexora::editor::preview {

// Opaque world-space lines only: 0xRRGGBBAA, alpha 255, finite bounded coordinates/duration.
[[nodiscard]] inline bool ValidPlayDebugLine(const NexoraDebugLine &line) noexcept {
  if ((line.rgba & 255U) != 255U || !std::isfinite(line.duration_seconds) ||
      line.duration_seconds < 0 || line.duration_seconds > 3600)
    return false;
  const std::array<double, 6> coordinates{line.start.x, line.start.y, line.start.z,
                                          line.end.x,   line.end.y,   line.end.z};
  for (const auto coordinate : coordinates)
    if (!std::isfinite(coordinate) || std::abs(coordinate) > 99999)
      return false;
  const double x = static_cast<double>(line.end.x) - line.start.x,
               y = static_cast<double>(line.end.y) - line.start.y,
               z = static_cast<double>(line.end.z) - line.start.z;
  return x * x + y * y + z * z > 1e-12;
}

// Serialized module-owner storage, no allocation, World borrow or wall-clock dependency.
class PlayDebugLines final {
public:
  static constexpr std::size_t kCapacity = 256;
  int32_t Add(const NexoraDebugLine &line) noexcept {
    if (!ValidPlayDebugLine(line))
      return NEXORA_GAMEPLAY_ERROR_INVALID_ARGUMENT;
    if (size_ == kCapacity)
      return NEXORA_GAMEPLAY_ERROR_LIFECYCLE;
    lines_[size_] = line;
    remaining_[size_++] = line.duration_seconds;
    return NEXORA_GAMEPLAY_OK;
  }
  // Before each playing application frame: zero-duration lines retire, positive durations use
  // gameplay delta only. FixedUpdate/paused Step/rendering do not age or consume this queue.
  void Advance(double seconds) noexcept {
    std::size_t kept = 0;
    for (std::size_t i = 0; i < size_; ++i) {
      if (remaining_[i] <= seconds)
        continue;
      lines_[kept] = lines_[i];
      remaining_[kept++] = remaining_[i] - seconds;
    }
    size_ = kept;
  }
  void Clear() noexcept { size_ = 0; }
  [[nodiscard]] std::span<const NexoraDebugLine> Lines() const noexcept {
    return {lines_.data(), size_};
  }

private:
  std::array<NexoraDebugLine, kCapacity> lines_{};
  std::array<double, kCapacity> remaining_{};
  std::size_t size_{};
};
} // namespace nexora::editor::preview
