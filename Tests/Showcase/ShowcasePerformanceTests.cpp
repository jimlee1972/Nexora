#include "ShowcasePerformance.h"

#include <cassert>
#include <limits>
#include <stdexcept>

int main() {
  nexora::showcase::FrameProfiler profiler;
  assert(profiler.Report().find("INSUFFICIENT_SAMPLES") != std::string::npos);
  for (int frame = 0; frame < 60; ++frame)
    profiler.Record(1000, 1000); // Warm-up must not distort reported steady-state timings.
  for (int frame = 0; frame < 100; ++frame)
    profiler.Record(frame < 95 ? 10 : 20, 5);
  const auto report = profiler.Report();
  assert(report.find("\"sample_count\":100") != std::string::npos);
  assert(report.find("\"average_frame_ms\":10.5") != std::string::npos);
  assert(report.find("\"p95_frame_ms\":10") != std::string::npos);
  assert(report.find("\"p99_frame_ms\":20") != std::string::npos);
  assert(report.find("\"average_process_cpu_ms\":5") != std::string::npos);
  assert(report.find("\"gpu_timing_ms\":null") != std::string::npos);
  for (const double value : {0.0, -1.0, std::numeric_limits<double>::quiet_NaN()}) {
    bool rejected = false;
    try {
      profiler.Record(value, {});
    } catch (const std::invalid_argument &) {
      rejected = true;
    }
    assert(rejected);
    assert(profiler.Report().find("\"observed_frames\":160") != std::string::npos);
    assert(profiler.Report().find("\"sample_count\":100") != std::string::npos);
  }
  profiler.Record(10, {});
  assert(profiler.Report().find("\"average_process_cpu_ms\":null") != std::string::npos);
  for (int frame = 0; frame < 18000; ++frame)
    profiler.Record(20, 7);
  assert(profiler.Report().find("\"sample_count\":18000") != std::string::npos);
  assert(profiler.Report().find("\"average_frame_ms\":20") != std::string::npos);
}
