#include "Nexora/Editor/EditorProduction.h"

#include <chrono>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
std::optional<std::uint64_t> reading;
std::uint64_t reads = 0;
std::optional<std::uint64_t> Read() noexcept {
  ++reads;
  return reading;
}
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  using namespace std::chrono_literals;
  using nexora::editor::ProfileSession;
  ProfileSession profile{2};
  const auto start = std::chrono::steady_clock::time_point{};
  reading = 42;
  Require(profile.Add({1, 1.5, 0, 0}) && profile.SampleProcessMemory(start, Read),
          "first memory observation rejected");
  const auto copied = profile.ProcessMemory();
  Require(copied.resident_bytes == 42 && copied.observed_peak_bytes == 42 && copied.attempts == 1 &&
              copied.successful_samples == 1,
          "owning process memory snapshot incorrect");
  Require(!profile.SampleProcessMemory(start, Read) &&
              !profile.SampleProcessMemory(start - 1ms, Read) &&
              !profile.SampleProcessMemory(start + 249ms, Read) && reads == 1,
          "throttle or backward clock invoked OS reader");
  reading = 21;
  Require(profile.SampleProcessMemory(start + 250ms, Read) &&
              profile.ProcessMemory().resident_bytes == 21 &&
              profile.ProcessMemory().observed_peak_bytes == 42 && copied.resident_bytes == 42,
          "latest/peak or copied ownership incorrect");
  reading.reset();
  Require(
      profile.SampleProcessMemory(start + 500ms, Read) && !profile.ProcessMemory().resident_bytes &&
          profile.ProcessMemory().observed_peak_bytes == 42 &&
          profile.ProcessMemory().attempts == 3 && profile.ProcessMemory().successful_samples == 2,
      "failed observation fabricated a measurement or erased historical peak");
  Require(profile.MemorySamples().size() == 2 && profile.MemoryDroppedCount() == 1 &&
              profile.MemorySamples().front().sequence == 2 &&
              profile.MemorySamples().front().elapsed_ms == 250 &&
              profile.MemorySamples().back().sequence == 3 &&
              profile.MemorySamples().back().elapsed_ms == 500 &&
              !profile.MemorySamples().back().resident_bytes,
          "bounded history lost time/order/unavailable attempts");
  profile.SetCapturing(false);
  Require(!profile.SampleProcessMemory(start + 1s, Read) && reads == 3 &&
              !profile.Add({2, 1, 0, 0}),
          "paused capture sampled OS or ingested frames");
  profile.Clear();
  const auto cleared = profile.ProcessMemory();
  Require(!cleared.resident_bytes && !cleared.observed_peak_bytes && cleared.attempts == 0 &&
              cleared.successful_samples == 0 && profile.Samples().empty() &&
              profile.MemorySamples().empty() && profile.MemoryDroppedCount() == 0 &&
              !profile.SampleProcessMemory(start + 1s, Read),
          "clear did not release observations or resumed paused capture");
  profile.SetCapturing(true);
  reading = std::numeric_limits<std::uint64_t>::max();
  Require(profile.SampleProcessMemory(start + 1s, Read) &&
              profile.ProcessMemory().resident_bytes == reading &&
              profile.ProcessMemory().observed_peak_bytes == reading,
          "resume did not sample immediately or truncated byte precision");
  reading = 0;
  Require(profile.SampleProcessMemory(start + 1250ms, Read) &&
              profile.ProcessMemory().resident_bytes == 0,
          "valid zero was confused with unavailable");
  Require(profile.MemorySamples().front().resident_bytes ==
                  std::numeric_limits<std::uint64_t>::max() &&
              profile.MemorySamples().back().resident_bytes == 0,
          "history truncated bytes or confused zero/unavailable");
  profile.Clear();
  Require(profile.SampleProcessMemory(start, Read), "clear did not reset monotonic sample window");
  profile.Clear();
  Require(profile.SampleProcessMemory(std::chrono::steady_clock::time_point::max(), Read) &&
              !profile.SampleProcessMemory(std::chrono::steady_clock::time_point::max(), Read),
          "clock boundary overflowed throttle");
  profile.Clear();
  Require(profile.SampleProcessMemory(std::chrono::steady_clock::now()),
          "default production observation did not run");
#if defined(__linux__) || defined(_WIN32) || defined(__APPLE__)
  Require(profile.ProcessMemory().resident_bytes && *profile.ProcessMemory().resident_bytes > 0,
          "production session did not use real OS memory sampler");
#endif
  ProfileSession endpoint;
  Require(endpoint.SampleProcessMemory(std::chrono::steady_clock::time_point::min(), Read) &&
              endpoint.SampleProcessMemory(std::chrono::steady_clock::time_point::max(), Read) &&
              nexora::editor::ValidateProcessMemorySamples(endpoint.MemorySamples()),
          "clock endpoint subtraction overflowed");
  ProfileSession gap;
  Require(gap.SampleProcessMemory(start, Read), "gap seed failed");
  gap.SetCapturing(false);
  Require(!gap.SampleProcessMemory(start + 1s, Read), "pause sampled");
  gap.SetCapturing(true);
  Require(gap.SampleProcessMemory(start + 2s, Read) &&
              gap.MemorySamples().back().elapsed_ms == 2000,
          "pause gap erased elapsed time");
  ProfileSession capped{10000}, disabled{0};
  for (int i = 0; i < 605; ++i)
    Require(capped.SampleProcessMemory(start + i * 250ms, Read), "capped sample rejected");
  Require(capped.MemorySamples().size() == 600 && capped.MemoryDroppedCount() == 5 &&
              capped.MemorySamples().front().sequence == 6,
          "hard memory retention cap failed");
  Require(disabled.SampleProcessMemory(start, Read) && disabled.MemorySamples().empty(),
          "zero history capacity contract failed");
  Require(profile.Add({2, 2.5, 0, 0}) && profile.Samples().back().memory_bytes == 0 &&
              profile.Samples().back().gpu_ms == 0,
          "memory observation fabricated schema-1 frame metrics");
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
