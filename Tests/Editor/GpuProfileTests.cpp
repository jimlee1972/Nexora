#include "Nexora/Editor/EditorProduction.h"
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
  constexpr auto source = GpuProfileSource::VulkanTimestamps;
  ProfileSession profile{2};
  Require(profile.ObserveGpuFrame(1, source, true, {1, 1.25}), "first completion rejected");
  const auto copied = profile.GpuTiming();
  Require(
      !profile.ObserveGpuFrame(1, source, true, {1, 9}) &&
          !profile.ObserveGpuFrame(1, source, true, {0, 9}) &&
          !profile.ObserveGpuFrame(0, source, true, {2, 9}) &&
          !profile.ObserveGpuFrame(1, source, true, {2, -1}) &&
          !profile.ObserveGpuFrame(1, source, true, {2, std::numeric_limits<double>::quiet_NaN()}),
      "invalid/stale/native-domain input accepted");
  Require(profile.GpuSamples().size() == 1 && profile.GpuTiming().milliseconds == 1.25,
          "rejected completion mutated history");
  Require(profile.ObserveGpuFrame(1, source, true, {2, 0}) &&
              profile.ObserveGpuFrame(1, source, true, {3, std::nullopt}) &&
              profile.GpuSamples().size() == 2 && profile.GpuSamples().front().milliseconds == 0 &&
              !profile.GpuTiming().milliseconds && profile.GpuTiming().observed_peak_ms == 1.25 &&
              profile.GpuDroppedCount() == 1 && copied.milliseconds == 1.25,
          "bounded/optional/copied history incorrect");
  profile.SetCapturing(false);
  Require(!profile.ObserveGpuFrame(1, source, true, {4, 9}) && profile.GpuSamples().size() == 2,
          "pause ingested completion");
  profile.SetCapturing(true);
  Require(!profile.ObserveGpuFrame(1, source, true, {4, 9}), "resume readmitted paused result");
  profile.Clear();
  Require(profile.GpuSamples().empty() && profile.GpuDroppedCount() == 0 &&
              !profile.GpuTiming().milliseconds && !profile.GpuTiming().observed_peak_ms &&
              !profile.ObserveGpuFrame(1, source, true, {4, 9}),
          "Clear reset completion watermark or retained measurements");
  Require(profile.ObserveGpuFrame(1, source, true, {5, 2}),
          "fresh completion after clear rejected");
  Require(profile.ObserveGpuFrame(2, GpuProfileSource::Dx12Timestamps, false, {1, 3}) &&
              profile.GpuSamples().size() == 1 &&
              profile.GpuTiming().source == GpuProfileSource::Dx12Timestamps &&
              !profile.GpuTiming().software_rasterizer && profile.GpuTiming().observed_peak_ms == 3,
          "native surface domain/source change retained old stream");
  Require(!profile.ObserveGpuFrame(2, GpuProfileSource::Unavailable, false, {2, 3}) &&
              !profile.GpuTiming().milliseconds && profile.GpuSamples().empty(),
          "unavailable source fabricated data");
  ProfileSession capped{10000}, no_history{0};
  for (std::uint64_t i = 1; i <= 605; ++i)
    Require(capped.ObserveGpuFrame(1, source, true, {i, 1}), "valid capped sample rejected");
  Require(capped.GpuSamples().size() == 600 && capped.GpuDroppedCount() == 5 &&
              capped.GpuSamples().front().submission == 6,
          "hard history cap failed");
  Require(no_history.ObserveGpuFrame(1, source, true, {1, 1}) && no_history.GpuSamples().empty() &&
              no_history.GpuTiming().milliseconds == 1,
          "zero-capacity observation incorrect");
  Require(profile.Add({1, 1, 0, 0}) && profile.Samples().front().gpu_ms == 0 &&
              profile.Samples().front().memory_bytes == 0,
          "native completion fabricated schema-1 wall-frame metrics");
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
