#include "Nexora/Editor/EditorProduction.h"

#include <array>
#include <iostream>
#include <limits>
#include <stdexcept>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}
void Run() {
  using namespace nexora::editor;
  using namespace std::chrono_literals;
  const auto start = std::chrono::steady_clock::time_point{};
  ProfileSession profile{3};
  Require(!profile.ObserveMeshMemory(0, start, 4096, 96) &&
              !profile.ObserveMeshMemory(7, start, 4096, std::nullopt) &&
              !profile.MeshMemory().surface_domain && profile.MeshMemorySamples().empty(),
          "invalid source/pair fabricated observations");
  Require(profile.ObserveMeshMemory(7, start, 4096, 96) &&
              profile.MeshMemory().native_allocation_bytes == 4096 &&
              profile.MeshMemory().cpu_key_payload_bytes == 96 &&
              profile.MeshMemory().attempts == 1 && profile.MeshMemory().successful_samples == 1 &&
              profile.MeshMemorySamples().front().sequence == 1 &&
              profile.MeshMemorySamples().front().elapsed_ms == 0,
          "initial native observation failed");
  Require(!profile.ObserveMeshMemory(7, start + 100ms, 8192, 128) &&
              !profile.ObserveMeshMemory(7, start - 1ms, 8192, 128) &&
              profile.MeshMemory().native_allocation_bytes == 4096,
          "throttle/backward clocks replaced the current observation");
  Require(profile.ObserveMeshMemory(7, start + 250ms, std::nullopt, std::nullopt) &&
              !profile.MeshMemory().native_allocation_bytes &&
              profile.MeshMemory().observed_peak_native_bytes == 4096 &&
              profile.MeshMemory().observed_peak_cpu_bytes == 96 &&
              ValidateMeshMemorySamples(profile.MeshMemorySamples()),
          "unavailable observation reused success or lost peaks");
  Require(profile.ObserveMeshMemory(7, start + 500ms, 0, 0) &&
              profile.MeshMemory().native_allocation_bytes == 0 &&
              profile.MeshMemory().successful_samples == 2,
          "zero bytes became unavailable");
  profile.SetCapturing(false);
  Require(!profile.ObserveMeshMemory(7, start + 750ms, 8192, 256) &&
              profile.MeshMemorySamples().size() == 3 && profile.MeshMemory().attempts == 3,
          "paused ingestion changed history");
  profile.Clear();
  Require(!profile.Capturing() && profile.MeshMemory().surface_domain == 7 &&
              !profile.MeshMemory().native_allocation_bytes &&
              !profile.MeshMemory().observed_peak_native_bytes &&
              profile.MeshMemorySamples().empty() && !profile.MeshMemoryDroppedCount(),
          "Clear resumed capture or retained measurements");
  profile.SetCapturing(true);
  for (unsigned i = 0; i < 4; ++i)
    Require(profile.ObserveMeshMemory(7, start + 751ms + i * 250ms, i == 0 ? 8192 : 1024 * i,
                                      i == 0 ? 256 : 16 * i),
            "resumed observation rejected");
  Require(profile.MeshMemorySamples().size() == 3 && profile.MeshMemoryDroppedCount() == 1 &&
              profile.MeshMemorySamples().front().sequence == 2 &&
              profile.MeshMemorySamples().front().elapsed_ms == 250 &&
              profile.MeshMemory().observed_peak_native_bytes == 8192 &&
              profile.MeshMemory().observed_peak_cpu_bytes == 256,
          "bounded eviction lost elapsed time or observed peaks");
  Require(profile.Add({1, 2, 0, 0}) &&
              profile.ObserveGpuFrame(1, GpuProfileSource::VulkanTimestamps, true, {1, 3}) &&
              profile.SampleProcessMemory(
                  start, []() noexcept { return std::optional<std::uint64_t>{99}; }) &&
              profile.ObserveMeshMemory(8, start + 1000ms, 32, 8) &&
              profile.MeshMemorySamples().size() == 1 && !profile.MeshMemoryDroppedCount() &&
              profile.MeshMemorySamples().front().elapsed_ms == 0 &&
              profile.MeshMemory().observed_peak_native_bytes == 32 &&
              profile.ProcessMemory().resident_bytes == 99 && profile.GpuSamples().size() == 1 &&
              profile.Samples().size() == 1 && profile.Samples().front().memory_bytes == 0,
          "owner-domain reset merged streams or changed existing schemas/history");
  profile.SetCapturing(false);
  Require(!profile.ObserveMeshMemory(9, start + 2000ms, 64, 16) &&
              profile.MeshMemory().surface_domain == 9 &&
              !profile.MeshMemory().native_allocation_bytes && profile.MeshMemorySamples().empty(),
          "paused owner replacement retained old-surface data");
  Require(!profile.ObserveMeshMemory(10, start + 2250ms, 64, std::nullopt) &&
              profile.MeshMemory().surface_domain == 9,
          "malformed replacement discarded the prior domain");
  ProfileSession capped{10000}, no_history{0}, endpoints{1};
  for (std::uint64_t i = 1; i <= 605; ++i)
    Require(capped.ObserveMeshMemory(1, start + i * 250ms, i, i / 2), "capped sample rejected");
  Require(capped.MeshMemorySamples().size() == 600 && capped.MeshMemoryDroppedCount() == 5 &&
              capped.MeshMemorySamples().front().sequence == 6 &&
              ValidateMeshMemorySamples(capped.MeshMemorySamples()) &&
              no_history.ObserveMeshMemory(1, start, 1, 1) &&
              no_history.MeshMemorySamples().empty() &&
              no_history.MeshMemory().native_allocation_bytes == 1 &&
              endpoints.ObserveMeshMemory(1, std::chrono::steady_clock::time_point::min(), 1, 1) &&
              endpoints.ObserveMeshMemory(1, std::chrono::steady_clock::time_point::max(), 2, 2) &&
              !endpoints.ObserveMeshMemory(1, std::chrono::steady_clock::time_point::max(), 3, 3),
          "hard cap, zero-capacity observations or clock endpoints failed");
  std::array<MeshMemorySample, 2> samples{{{1, 0, 0, 0}, {2, 250, std::nullopt, std::nullopt}}};
  Require(ValidateMeshMemorySamples(samples), "valid gap/zero samples rejected");
  samples[1].sequence = 1;
  Require(!ValidateMeshMemorySamples(samples), "duplicate sequence admitted");
  samples[1] = {2, std::numeric_limits<double>::quiet_NaN(), 1, 1};
  Require(!ValidateMeshMemorySamples(samples), "nonfinite time admitted");
  samples[1] = {2, 250, 1, std::nullopt};
  Require(!ValidateMeshMemorySamples(samples) && !ValidateMeshMemorySamples({}),
          "partial metrics or empty history admitted");
}
} // namespace
int main() {
  try {
    Run();
    std::cout
        << "PASS: retained-mesh memory ownership, scope, clock, gaps, pause/Clear and bounds\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << error.what() << '\n';
    return 1;
  }
}
