#include "GpuTiming.h"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  using namespace Nexora::Presentation;
  using detail::TimestampMilliseconds;
  Require(TimestampMilliseconds(100, 1100, 64, 1000, 10) == 1, "native tick conversion incorrect");
  Require(TimestampMilliseconds(65000, 464, 16, 1000, 10) == 1,
          "single counter wrap conversion incorrect");
  Require(TimestampMilliseconds(UINT64_MAX - 999, 0, 64, 1000, 10) == 1,
          "64-bit counter wrap incorrect");
  Require(TimestampMilliseconds(100, 100, 64, 1, 0) == 0,
          "zero interval confused with unavailable");
  const auto nan = std::numeric_limits<double>::quiet_NaN();
  const auto infinity = std::numeric_limits<double>::infinity();
  for (const unsigned bits : {0U, 65U, 1000U})
    Require(!TimestampMilliseconds(1, 2, bits, 1, 10), "invalid counter width accepted");
  for (const double period : {0.0, -1.0, nan, infinity, std::numeric_limits<double>::max()})
    Require(!TimestampMilliseconds(1, 2, 64, period, 10), "invalid period accepted");
  for (const double wall : {-1.0, nan, infinity})
    Require(!TimestampMilliseconds(1, 2, 64, 1, wall), "invalid completion bound accepted");
  Require(!TimestampMilliseconds(1000, 500, 64, 1, 10), "backward native timestamps accepted");
  Require(!TimestampMilliseconds(65000, 464, 16, 1000, 100),
          "ambiguous multiple-wrap window accepted");
  Require(!TimestampMilliseconds(0, 10000, 64, 1000, 1),
          "native time beyond CPU completion bound accepted");
  SurfaceGpuTiming timing;
  detail::PublishGpuTiming(timing, GpuTimingSource::VulkanTimestamps, 2, 1.25);
  const auto copied = timing;
  detail::PublishGpuTiming(timing, GpuTimingSource::VulkanTimestamps, 1, 2.5);
  detail::PublishGpuTiming(timing, GpuTimingSource::VulkanTimestamps, 2, 2.5);
  Require(timing.completedSubmission == 2 && timing.milliseconds == 1.25,
          "older slot completion replaced latest result");
  detail::PublishGpuTiming(timing, GpuTimingSource::VulkanTimestamps, 3, std::nullopt);
  Require(timing.completedSubmission == 3 && !timing.milliseconds && copied.milliseconds == 1.25,
          "unavailable result fabricated latest or mutated copied observation");
  detail::PublishGpuTiming(timing, GpuTimingSource::Dx12Timestamps, 4, nan);
  Require(!timing.milliseconds, "nonfinite result published as measured");
  detail::PublishGpuTiming(timing, GpuTimingSource::Unavailable, 5, 2.5);
  Require(!timing.milliseconds && timing.source == GpuTimingSource::Unavailable,
          "unsupported source advertised data");
}
} // namespace
int main() {
  try {
    Run();
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
