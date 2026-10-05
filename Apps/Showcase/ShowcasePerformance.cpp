#include "ShowcasePerformance.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <numeric>
#include <sstream>
#include <stdexcept>

#if defined(_WIN32)
#define NOMINMAX
#include <Windows.h>
#ifndef PSAPI_VERSION
#define PSAPI_VERSION 2
#endif
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

namespace nexora::showcase {
std::optional<double> ProcessCpuMilliseconds() noexcept {
#if defined(_WIN32)
  FILETIME created{}, exited{}, kernel{}, user{};
  if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user))
    return {};
  const auto value = [](FILETIME time) {
    return static_cast<double>((static_cast<unsigned long long>(time.dwHighDateTime) << 32) |
                               time.dwLowDateTime) /
           10000.0;
  };
  return value(kernel) + value(user);
#else
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0)
    return {};
  return (usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) * 1000.0 +
         (usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) / 1000.0;
#endif
}

std::optional<double> PeakResidentBytes() noexcept {
#if defined(_WIN32)
  PROCESS_MEMORY_COUNTERS counters{};
  if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &counters, sizeof(counters)))
    return {};
  return static_cast<double>(counters.PeakWorkingSetSize);
#else
  rusage usage{};
  if (getrusage(RUSAGE_SELF, &usage) != 0)
    return {};
#if defined(__APPLE__)
  return static_cast<double>(usage.ru_maxrss);
#else
  return static_cast<double>(usage.ru_maxrss) * 1024.0;
#endif
#endif
}

void FrameProfiler::Begin() {
  begin_ = std::chrono::steady_clock::now();
  cpuBegin_ = ProcessCpuMilliseconds();
}

void FrameProfiler::End() {
  const auto end = std::chrono::steady_clock::now();
  const auto cpu = ProcessCpuMilliseconds();
  Record(std::chrono::duration<double, std::milli>(end - begin_).count(),
         cpu && cpuBegin_ ? std::optional{std::max(0.0, *cpu - *cpuBegin_)} : std::nullopt);
}

void FrameProfiler::Record(double frameMs, std::optional<double> processCpuMs) {
  if (!std::isfinite(frameMs) || frameMs <= 0 ||
      (processCpuMs && (!std::isfinite(*processCpuMs) || *processCpuMs < 0)))
    throw std::invalid_argument("Invalid Showcase performance sample");
  if (++observed_ <= warmupFrames)
    return;
  if (samples_.size() < maxSamples)
    samples_.push_back({frameMs, processCpuMs});
  else {
    samples_[next_] = {frameMs, processCpuMs};
    next_ = (next_ + 1) % maxSamples;
  }
}

std::string FrameProfiler::Report() const {
  std::ostringstream out;
  out << std::setprecision(9)
      << "{\"schema\":\"nexora.showcase.performance.v1\",\"scope\":\"native acquired frame; "
         "includes acquire/present and pacing\",\"warmup_frames\":60,\"observed_frames\":"
      << observed_ << ",\"sample_count\":" << samples_.size()
      << ",\"sample_capacity\":18000,\"sample_window\":\"latest frames after warmup\","
         "\"gpu_timing_ms\":null,\"gpu_timing_status\":\"UNAVAILABLE: no GPU timestamps\","
         "\"cpu_scope\":\"process-wide user+kernel CPU time; includes all threads\",";
  if (const auto memory = PeakResidentBytes())
    out << "\"peak_resident_bytes\":" << *memory << ',';
  else
    out << "\"peak_resident_bytes\":null,";
  if (samples_.empty()) {
    out << "\"status\":\"INSUFFICIENT_SAMPLES\",\"average_fps\":null,\"p95_frame_ms\":null,"
           "\"p99_frame_ms\":null,\"average_process_cpu_ms\":null}";
    return out.str();
  }
  std::vector<double> frames;
  double cpuSum{};
  std::size_t cpuCount{};
  for (const auto &sample : samples_) {
    frames.push_back(sample.frameMs);
    if (sample.processCpuMs) {
      cpuSum += *sample.processCpuMs;
      ++cpuCount;
    }
  }
  const double mean = std::accumulate(frames.begin(), frames.end(), 0.0) / frames.size();
  std::sort(frames.begin(), frames.end());
  const auto percentile = [&](double quantile) {
    return frames[static_cast<std::size_t>(std::ceil(quantile * frames.size())) - 1];
  };
  out << "\"status\":\"MEASURED\",\"average_fps\":" << 1000.0 / mean
      << ",\"average_frame_ms\":" << mean << ",\"p95_frame_ms\":" << percentile(0.95)
      << ",\"p99_frame_ms\":" << percentile(0.99) << ",\"average_process_cpu_ms\":";
  if (cpuCount == samples_.size())
    out << cpuSum / cpuCount;
  else
    out << "null";
  out << '}';
  return out.str();
}
} // namespace nexora::showcase
