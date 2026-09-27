#pragma once

#include "Nexora/Core/Api.h"

#include <cstddef>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace nexora::core::diagnostics {

inline constexpr std::uint16_t kDiagnosticsWireVersion = 1;

enum class TraceCategory : std::uint8_t {
  Cpu,
  Io,
  CookArtifact,
  GpuUpload,
  Network,
  Memory,
};

struct TraceId final {
  std::uint64_t high{};
  std::uint64_t low{};
  [[nodiscard]] bool Valid() const noexcept { return high != 0 || low != 0; }
  friend bool operator==(const TraceId &, const TraceId &) = default;
};

struct TraceEvent final {
  TraceId trace{};
  std::uint64_t span_id{};
  std::uint64_t parent_span_id{};
  TraceCategory category{TraceCategory::Cpu};
  std::uint64_t begin_ns{};
  std::uint64_t end_ns{};
  std::uint64_t bytes{};
  std::string plugin_id;
  std::string resource;
};

struct MetricSample final {
  TraceId trace{};
  std::uint64_t timestamp_ns{};
  std::uint64_t resident_memory_bytes{};
  std::uint64_t gpu_time_ns{};
  std::uint64_t io_bytes{};
  std::uint64_t network_bytes{};
  std::string device_id;
  std::string plugin_id;
};

[[nodiscard]] NEXORA_CORE_API std::vector<std::byte> EncodeTraceEvent(const TraceEvent &event);
[[nodiscard]] NEXORA_CORE_API std::optional<TraceEvent>
DecodeTraceEvent(std::span<const std::byte> bytes);
[[nodiscard]] NEXORA_CORE_API std::vector<std::byte> EncodeMetricSample(const MetricSample &sample);
[[nodiscard]] NEXORA_CORE_API std::optional<MetricSample>
DecodeMetricSample(std::span<const std::byte> bytes);

struct PluginCost final {
  std::uint64_t span_time_ns{};
  std::uint64_t bytes{};
  std::uint64_t gpu_time_ns{};
  std::uint64_t resident_memory_bytes{};
  std::uint64_t io_bytes{};
  std::uint64_t network_bytes{};
};

struct TraceCorrelation final {
  TraceId trace{};
  std::vector<TraceEvent> events;
  std::vector<MetricSample> metrics;
  std::unordered_map<std::string, PluginCost> plugin_costs;
  bool streaming_chain{};
};

class NEXORA_CORE_API TraceAggregator final {
public:
  bool Ingest(TraceEvent event);
  bool Ingest(MetricSample sample);
  [[nodiscard]] TraceCorrelation Correlate(TraceId trace) const;
  [[nodiscard]] std::size_t TraceCount() const noexcept { return traces_.size(); }

private:
  struct TraceBucket final {
    std::vector<TraceEvent> events;
    std::vector<MetricSample> metrics;
  };
  struct TraceIdHash final {
    std::size_t operator()(TraceId value) const noexcept;
  };
  std::unordered_map<TraceId, TraceBucket, TraceIdHash> traces_;
};

} // namespace nexora::core::diagnostics
