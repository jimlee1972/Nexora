#pragma once

#include "Nexora/Editor/Api.h"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace nexora::editor {

enum class CapabilityState { Implemented, ReadOnly, Unavailable };

struct ToolDescriptor final {
  std::string id;
  std::string title;
  CapabilityState state{CapabilityState::Unavailable};
  std::string reason;
};

class NEXORA_EDITOR_API SpecializedToolRegistry final {
public:
  bool Register(ToolDescriptor descriptor);
  [[nodiscard]] const ToolDescriptor *Find(std::string_view id) const noexcept;
  [[nodiscard]] std::span<const ToolDescriptor> Tools() const noexcept { return tools_; }

private:
  std::vector<ToolDescriptor> tools_;
};

struct BuildProfile final {
  std::string name;
  std::string target;
  std::string configuration;
  std::string command;
};

struct BuildArtifact final {
  std::string path;
  std::string checksum;
  std::uint64_t bytes{};
};

struct BuildManifest final {
  std::uint32_t schema_version{1};
  BuildProfile profile;
  std::vector<BuildArtifact> artifacts;
};

class NEXORA_EDITOR_API BuildFrontend final {
public:
  [[nodiscard]] static bool Validate(const BuildManifest &manifest, std::string *error = nullptr);
  [[nodiscard]] static bool Write(const BuildManifest &manifest, const std::filesystem::path &path,
                                  std::string *error = nullptr);
};

struct FrameSample final {
  std::uint64_t frame{};
  double cpu_ms{};
  double gpu_ms{};
  std::uint64_t memory_bytes{};
};

// Owning snapshot of Editor wall timing only. GPU/memory fields remain zero/unavailable.
struct FrameProcessingCapture final {
  std::vector<FrameSample> samples;
  std::uint64_t older_frames_dropped{};
};

// Copied process-wide observations, separate from the schema-1 wall-time capture.
struct ProcessMemoryObservation final {
  std::optional<std::uint64_t> resident_bytes;
  std::optional<std::uint64_t> observed_peak_bytes;
  std::uint64_t attempts{};
  std::uint64_t successful_samples{};
};

struct ProcessMemorySample final {
  std::uint64_t sequence{};
  double elapsed_ms{};
  std::optional<std::uint64_t> resident_bytes;
};

struct ProcessMemoryCapture final {
  std::vector<ProcessMemorySample> samples;
  std::uint64_t older_samples_dropped{};
};

// Shared by persistence and GUI publication; no mutation or allocation.
[[nodiscard]] NEXORA_EDITOR_API bool
ValidateProcessMemorySamples(std::span<const ProcessMemorySample> samples) noexcept;

enum class GpuProfileSource : std::uint8_t {
  Unavailable,
  VulkanTimestamps,
  Dx12Timestamps,
  MetalCommandBuffer
};
struct GpuProfileSample final {
  std::uint64_t submission{};
  std::optional<double> milliseconds;
};
struct GpuProfileObservation final {
  GpuProfileSource source{GpuProfileSource::Unavailable};
  bool software_rasterizer{};
  std::uint64_t completed_submission{};
  std::optional<double> milliseconds;
  std::optional<double> observed_peak_ms;
};

class NEXORA_EDITOR_API ProfileSession final {
public:
  explicit ProfileSession(std::size_t capacity = 600) : capacity_(capacity) {}
  bool Add(FrameSample sample);
  void Clear() noexcept;
  void SetCapturing(bool enabled) noexcept { capturing_ = enabled; }
  [[nodiscard]] bool Capturing() const noexcept { return capturing_; }
  [[nodiscard]] std::uint64_t DroppedCount() const noexcept { return dropped_; }
  [[nodiscard]] std::span<const FrameSample> Samples() const noexcept { return samples_; }
  [[nodiscard]] std::optional<FrameSample> Peak() const noexcept;
  using ProcessMemoryReader = std::optional<std::uint64_t> (*)() noexcept;
  static constexpr auto kProcessMemorySampleInterval = std::chrono::milliseconds(250);
  // Application authoring thread only. The default reader observes the real current process;
  // widgets never call it. Failed reads publish unavailable while retaining observed peak.
  bool SampleProcessMemory(std::chrono::steady_clock::time_point now,
                           ProcessMemoryReader reader = nullptr) noexcept;
  [[nodiscard]] ProcessMemoryObservation ProcessMemory() const noexcept { return memory_; }
  static constexpr std::size_t kMaximumMemorySamples = 600;
  [[nodiscard]] std::span<const ProcessMemorySample> MemorySamples() const noexcept {
    return {memory_samples_.data(), memory_sample_count_};
  }
  [[nodiscard]] std::uint64_t MemoryDroppedCount() const noexcept { return memory_dropped_; }

  // Consume copied completed native results on the owner thread, never native handles.
  // Domain changes clear GPU history; paused ingestion still advances its completion watermark.
  bool ObserveGpuFrame(std::uint64_t domain, GpuProfileSource source, bool software_rasterizer,
                       GpuProfileSample sample) noexcept;
  static constexpr std::size_t kMaximumGpuSamples = 600;
  [[nodiscard]] GpuProfileObservation GpuTiming() const noexcept { return gpu_; }
  [[nodiscard]] std::span<const GpuProfileSample> GpuSamples() const noexcept {
    return {gpu_samples_.data(), gpu_sample_count_};
  }
  [[nodiscard]] std::uint64_t GpuDroppedCount() const noexcept { return gpu_dropped_; }

private:
  std::size_t capacity_{};
  std::uint64_t dropped_{};
  bool capturing_{true};
  std::vector<FrameSample> samples_;
  ProcessMemoryObservation memory_;
  std::array<ProcessMemorySample, kMaximumMemorySamples> memory_samples_{};
  std::size_t memory_sample_count_{};
  std::uint64_t memory_dropped_{};
  std::optional<std::chrono::steady_clock::time_point> memory_origin_;

  GpuProfileObservation gpu_;
  std::uint64_t gpu_domain_{};
  std::uint64_t gpu_watermark_{};
  std::array<GpuProfileSample, kMaximumGpuSamples> gpu_samples_{};
  std::size_t gpu_sample_count_{};
  std::uint64_t gpu_dropped_{};
  std::optional<std::chrono::steady_clock::time_point> next_memory_sample_;
  std::optional<std::chrono::steady_clock::time_point> last_memory_sample_;
};

struct NEXORA_EDITOR_API ExtensionPolicy final {
  bool require_signature{true};
  std::vector<std::string> trusted_publishers;
  [[nodiscard]] bool Allows(std::string_view publisher, bool signature_valid) const noexcept;
};

class NEXORA_EDITOR_API TelemetryConsent final {
public:
  static constexpr std::size_t kMaximumEvents = 1024;
  static constexpr std::size_t kMaximumEventBytes = 1024;
  // Revoking consent releases all retained events; enabling never restores an old queue.
  void Set(bool enabled) noexcept {
    enabled_ = enabled;
    if (!enabled)
      events_.clear();
  }
  [[nodiscard]] bool Enabled() const noexcept { return enabled_; }
  bool Record(std::string event);
  [[nodiscard]] std::span<const std::string> Events() const noexcept { return events_; }

private:
  bool enabled_{};
  std::vector<std::string> events_;
};

class NEXORA_EDITOR_API VirtualHierarchy final {
public:
  explicit VirtualHierarchy(std::size_t count) : count_(count) {}
  [[nodiscard]] std::pair<std::size_t, std::size_t> Visible(std::size_t first,
                                                            std::size_t capacity) const noexcept;

private:
  std::size_t count_{};
};

} // namespace nexora::editor
