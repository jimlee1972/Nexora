#pragma once

#include <chrono>
#include <optional>
#include <string>
#include <vector>

namespace nexora::showcase {
// Process-wide user+kernel CPU time and peak resident/working-set bytes. These are host
// observations, not Renderer GPU timings or TrackingAllocator-only memory estimates.
[[nodiscard]] std::optional<double> ProcessCpuMilliseconds() noexcept;
[[nodiscard]] std::optional<double> PeakResidentBytes() noexcept;

class FrameProfiler final {
public:
  void Begin();
  void End();
  // Samples are owned; invalid/nonfinite samples are rejected before affecting warm-up/state.
  void Record(double frameMs, std::optional<double> processCpuMs);
  [[nodiscard]] std::string Report() const;

private:
  struct Sample final {
    double frameMs{};
    std::optional<double> processCpuMs;
  };
  static constexpr std::size_t warmupFrames = 60, maxSamples = 18000;
  std::chrono::steady_clock::time_point begin_;
  std::optional<double> cpuBegin_;
  std::vector<Sample> samples_;
  std::size_t observed_{}, next_{};
};
} // namespace nexora::showcase
