#pragma once

#include <chrono>
#include <cmath>
#include <cstdint>
#include <limits>
#include <optional>

namespace nexora::rhi {
enum class GpuTimingSource : std::uint8_t {
  Unavailable,
  VulkanTimestamps,
  Dx12Timestamps,
  MetalCommandBuffer
};
struct SubmissionGpuTiming final {
  GpuTimingSource source{GpuTimingSource::Unavailable};
  // Completion token of the latest observed user command list; not necessarily contiguous.
  std::uint64_t completed_submission{};
  // No value means unavailable, never a CPU-duration substitute or fabricated zero.
  std::optional<double> milliseconds;
  friend bool operator==(const SubmissionGpuTiming &, const SubmissionGpuTiming &) = default;
};
namespace detail {
// CPU completion bounds establish a single-wrap interval for reduced-width native counters.
// This never replaces native timestamps with CPU duration.
inline std::optional<double> TimestampMilliseconds(std::uint64_t begin, std::uint64_t end,
                                                   unsigned valid_bits, double period_ns,
                                                   double completed_wall_ms) noexcept {
  if (!valid_bits || valid_bits > 64 || !std::isfinite(period_ns) || period_ns <= 0 ||
      !std::isfinite(completed_wall_ms) || completed_wall_ms < 0)
    return std::nullopt;
  const auto mask = valid_bits == 64 ? std::numeric_limits<std::uint64_t>::max()
                                     : (std::uint64_t{1} << valid_bits) - 1;
  const double quantum_ms = period_ns / 1000000;
  const double half_wrap_ms = static_cast<double>(mask / 2) * quantum_ms;
  if (quantum_ms <= 0 || !std::isfinite(half_wrap_ms) || completed_wall_ms >= half_wrap_ms)
    return std::nullopt;
  const auto delta = (end - begin) & mask;
  if (delta > mask / 2)
    return std::nullopt;
  const double measured_ms = static_cast<double>(delta) * quantum_ms;
  if (!std::isfinite(measured_ms) || measured_ms > completed_wall_ms + quantum_ms)
    return std::nullopt;
  return measured_ms;
}
inline double CompletionWallMilliseconds(std::chrono::steady_clock::time_point submitted) noexcept {
  return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - submitted)
      .count();
}
inline void PublishGpuTiming(SubmissionGpuTiming &timing, GpuTimingSource source,
                             std::uint64_t sequence, std::optional<double> milliseconds) noexcept {
  if (!sequence || sequence <= timing.completed_submission)
    return;
  if (source == GpuTimingSource::Unavailable ||
      (milliseconds && (!std::isfinite(*milliseconds) || *milliseconds < 0)))
    milliseconds.reset();
  timing = {source, sequence, milliseconds};
}
} // namespace detail
} // namespace nexora::rhi
