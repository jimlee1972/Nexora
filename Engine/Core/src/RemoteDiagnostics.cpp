#include "Nexora/Core/RemoteDiagnostics.h"

#include <algorithm>
#include <array>
#include <limits>
#include <ranges>

namespace nexora::core::diagnostics {
namespace {
constexpr std::array<std::byte, 4> kMagic{std::byte{'N'}, std::byte{'X'}, std::byte{'T'},
                                          std::byte{'R'}};
enum class PacketKind : std::uint8_t { Trace = 1, Metric = 2 };

void PutU16(std::vector<std::byte> &out, std::uint16_t value) {
  out.push_back(static_cast<std::byte>(value & 0xffU));
  out.push_back(static_cast<std::byte>((value >> 8U) & 0xffU));
}
void PutU32(std::vector<std::byte> &out, std::uint32_t value) {
  for (unsigned shift = 0; shift < 32; shift += 8)
    out.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}
void PutU64(std::vector<std::byte> &out, std::uint64_t value) {
  for (unsigned shift = 0; shift < 64; shift += 8)
    out.push_back(static_cast<std::byte>((value >> shift) & 0xffU));
}
bool PutString(std::vector<std::byte> &out, const std::string &value) {
  if (value.size() > std::numeric_limits<std::uint32_t>::max())
    return false;
  PutU32(out, static_cast<std::uint32_t>(value.size()));
  for (unsigned char c : value)
    out.push_back(static_cast<std::byte>(c));
  return true;
}

struct Reader final {
  std::span<const std::byte> bytes;
  std::size_t offset{};

  bool U8(std::uint8_t &value) {
    if (offset >= bytes.size())
      return false;
    value = std::to_integer<std::uint8_t>(bytes[offset++]);
    return true;
  }
  bool U16(std::uint16_t &value) {
    if (offset + 2 > bytes.size())
      return false;
    value = static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset])) |
            static_cast<std::uint16_t>(std::to_integer<std::uint8_t>(bytes[offset + 1])) << 8U;
    offset += 2;
    return true;
  }
  bool U32(std::uint32_t &value) {
    if (offset + 4 > bytes.size())
      return false;
    value = 0;
    for (unsigned shift = 0; shift < 32; shift += 8)
      value |= static_cast<std::uint32_t>(std::to_integer<std::uint8_t>(bytes[offset++])) << shift;
    return true;
  }
  bool U64(std::uint64_t &value) {
    if (offset + 8 > bytes.size())
      return false;
    value = 0;
    for (unsigned shift = 0; shift < 64; shift += 8)
      value |= static_cast<std::uint64_t>(std::to_integer<std::uint8_t>(bytes[offset++])) << shift;
    return true;
  }
  bool String(std::string &value) {
    std::uint32_t size{};
    if (!U32(size) || offset + size > bytes.size())
      return false;
    value.clear();
    value.reserve(size);
    for (std::uint32_t index = 0; index < size; ++index)
      value.push_back(static_cast<char>(std::to_integer<unsigned char>(bytes[offset + index])));
    offset += size;
    return true;
  }
};

void WriteHeader(std::vector<std::byte> &out, PacketKind kind) {
  out.insert(out.end(), kMagic.begin(), kMagic.end());
  PutU16(out, kDiagnosticsWireVersion);
  out.push_back(static_cast<std::byte>(kind));
}
bool ReadHeader(Reader &reader, PacketKind kind) {
  if (reader.bytes.size() < kMagic.size() ||
      !std::ranges::equal(reader.bytes.first(kMagic.size()), kMagic))
    return false;
  reader.offset = kMagic.size();
  std::uint16_t version{};
  std::uint8_t packet{};
  return reader.U16(version) && version == kDiagnosticsWireVersion && reader.U8(packet) &&
         packet == static_cast<std::uint8_t>(kind);
}
bool ValidCategory(std::uint8_t category) {
  return category <= static_cast<std::uint8_t>(TraceCategory::Memory);
}
} // namespace

std::vector<std::byte> EncodeTraceEvent(const TraceEvent &event) {
  if (!event.trace.Valid() || event.span_id == 0 || event.end_ns < event.begin_ns)
    return {};
  std::vector<std::byte> out;
  WriteHeader(out, PacketKind::Trace);
  PutU64(out, event.trace.high);
  PutU64(out, event.trace.low);
  PutU64(out, event.span_id);
  PutU64(out, event.parent_span_id);
  out.push_back(static_cast<std::byte>(event.category));
  PutU64(out, event.begin_ns);
  PutU64(out, event.end_ns);
  PutU64(out, event.bytes);
  if (!PutString(out, event.plugin_id) || !PutString(out, event.resource))
    return {};
  return out;
}

std::optional<TraceEvent> DecodeTraceEvent(std::span<const std::byte> bytes) {
  Reader reader{bytes};
  if (!ReadHeader(reader, PacketKind::Trace))
    return std::nullopt;
  TraceEvent event;
  std::uint8_t category{};
  if (!reader.U64(event.trace.high) || !reader.U64(event.trace.low) || !reader.U64(event.span_id) ||
      !reader.U64(event.parent_span_id) || !reader.U8(category) || !ValidCategory(category) ||
      !reader.U64(event.begin_ns) || !reader.U64(event.end_ns) || !reader.U64(event.bytes) ||
      !reader.String(event.plugin_id) || !reader.String(event.resource) ||
      reader.offset != bytes.size())
    return std::nullopt;
  event.category = static_cast<TraceCategory>(category);
  if (!event.trace.Valid() || event.span_id == 0 || event.end_ns < event.begin_ns)
    return std::nullopt;
  return event;
}

std::vector<std::byte> EncodeMetricSample(const MetricSample &sample) {
  if (!sample.trace.Valid() || sample.device_id.empty())
    return {};
  std::vector<std::byte> out;
  WriteHeader(out, PacketKind::Metric);
  PutU64(out, sample.trace.high);
  PutU64(out, sample.trace.low);
  PutU64(out, sample.timestamp_ns);
  PutU64(out, sample.resident_memory_bytes);
  PutU64(out, sample.gpu_time_ns);
  PutU64(out, sample.io_bytes);
  PutU64(out, sample.network_bytes);
  if (!PutString(out, sample.device_id) || !PutString(out, sample.plugin_id))
    return {};
  return out;
}

std::optional<MetricSample> DecodeMetricSample(std::span<const std::byte> bytes) {
  Reader reader{bytes};
  if (!ReadHeader(reader, PacketKind::Metric))
    return std::nullopt;
  MetricSample sample;
  if (!reader.U64(sample.trace.high) || !reader.U64(sample.trace.low) ||
      !reader.U64(sample.timestamp_ns) || !reader.U64(sample.resident_memory_bytes) ||
      !reader.U64(sample.gpu_time_ns) || !reader.U64(sample.io_bytes) ||
      !reader.U64(sample.network_bytes) || !reader.String(sample.device_id) ||
      !reader.String(sample.plugin_id) || reader.offset != bytes.size() || !sample.trace.Valid() ||
      sample.device_id.empty())
    return std::nullopt;
  return sample;
}

std::size_t TraceAggregator::TraceIdHash::operator()(TraceId value) const noexcept {
  const auto mixed = value.high ^ (value.low + 0x9e3779b97f4a7c15ULL + (value.high << 6U) +
                                   (value.high >> 2U));
  return static_cast<std::size_t>(mixed);
}

bool TraceAggregator::Ingest(TraceEvent event) {
  if (!event.trace.Valid() || event.span_id == 0 || event.end_ns < event.begin_ns)
    return false;
  auto &events = traces_[event.trace].events;
  if (std::ranges::any_of(events,
                          [&](const auto &existing) { return existing.span_id == event.span_id; }))
    return false;
  events.push_back(std::move(event));
  return true;
}

bool TraceAggregator::Ingest(MetricSample sample) {
  if (!sample.trace.Valid() || sample.device_id.empty())
    return false;
  traces_[sample.trace].metrics.push_back(std::move(sample));
  return true;
}

TraceCorrelation TraceAggregator::Correlate(TraceId trace) const {
  TraceCorrelation result;
  result.trace = trace;
  const auto found = traces_.find(trace);
  if (found == traces_.end())
    return result;
  result.events = found->second.events;
  result.metrics = found->second.metrics;
  std::ranges::stable_sort(result.events, [](const auto &left, const auto &right) {
    if (left.begin_ns != right.begin_ns)
      return left.begin_ns < right.begin_ns;
    return left.span_id < right.span_id;
  });
  std::ranges::stable_sort(result.metrics, {}, &MetricSample::timestamp_ns);

  std::optional<std::uint64_t> io_time;
  std::optional<std::uint64_t> cook_time;
  std::optional<std::uint64_t> gpu_time;
  std::string streaming_resource;
  for (const auto &event : result.events) {
    if (!event.plugin_id.empty()) {
      auto &cost = result.plugin_costs[event.plugin_id];
      cost.span_time_ns += event.end_ns - event.begin_ns;
      cost.bytes += event.bytes;
    }
    if (event.category == TraceCategory::Io && !io_time) {
      io_time = event.begin_ns;
      streaming_resource = event.resource;
    } else if (event.category == TraceCategory::CookArtifact && io_time && !cook_time &&
               event.begin_ns >= *io_time &&
               (streaming_resource.empty() || event.resource == streaming_resource)) {
      cook_time = event.begin_ns;
    } else if (event.category == TraceCategory::GpuUpload && cook_time && !gpu_time &&
               event.begin_ns >= *cook_time &&
               (streaming_resource.empty() || event.resource == streaming_resource)) {
      gpu_time = event.begin_ns;
    }
  }
  for (const auto &sample : result.metrics) {
    if (sample.plugin_id.empty())
      continue;
    auto &cost = result.plugin_costs[sample.plugin_id];
    cost.gpu_time_ns += sample.gpu_time_ns;
    cost.resident_memory_bytes = std::max(cost.resident_memory_bytes, sample.resident_memory_bytes);
    cost.io_bytes += sample.io_bytes;
    cost.network_bytes += sample.network_bytes;
  }
  result.streaming_chain = io_time && cook_time && gpu_time;
  return result;
}

} // namespace nexora::core::diagnostics
