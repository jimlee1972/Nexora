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
  // GPU completion samples never substitute for CPU/wall time. Defaults ignore them.
  profiler.RecordCompletedGpu(999, 9);
  assert(profiler.Report().find("\"gpu_timing_ms\":null") != std::string::npos);
  nexora::showcase::FrameProfiler gpu(true);
  for (std::uint64_t submission = 1; submission <= 60; ++submission)
    gpu.RecordCompletedGpu(submission, 1000);
  assert(gpu.Report().find("\"gpu_timing_sample_count\":0") != std::string::npos);
  gpu.RecordCompletedGpu(61, 10);
  gpu.RecordCompletedGpu(61, 999); // Duplicate latest completion must not count twice.
  gpu.RecordCompletedGpu(60, 999); // Older drained slot must not regress the stream.
  for (const auto invalid :
       {std::optional<double>{}, std::optional<double>{0}, std::optional<double>{-1},
        std::optional<double>{std::numeric_limits<double>::quiet_NaN()},
        std::optional<double>{std::numeric_limits<double>::infinity()},
        std::optional<double>{60001}})
    gpu.RecordCompletedGpu(62, invalid);
  gpu.RecordCompletedGpu(0, 20);
  gpu.RecordCompletedGpu(62, 20);
  const auto gpuReport = gpu.Report();
  assert(gpuReport.find("\"gpu_timing_sample_count\":2") != std::string::npos);
  assert(gpuReport.find("\"gpu_timing_ms\":15") != std::string::npos);
  assert(gpuReport.find("\"gpu_p95_ms\":20") != std::string::npos);
  assert(gpuReport.find("\"gpu_completed_submission\":62") != std::string::npos);
  assert(gpuReport.find("\"average_fps\":null") != std::string::npos);
  for (std::uint64_t submission = 63; submission < 18063; ++submission)
    gpu.RecordCompletedGpu(submission, 30);
  assert(gpu.Report().find("\"gpu_timing_sample_count\":18000") != std::string::npos);
  assert(gpu.Report().find("\"gpu_timing_ms\":30") != std::string::npos);
}
