#include "Nexora/Core/RemoteDiagnostics.h"

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
void Require(bool value, const char *message) {
  if (!value)
    throw std::runtime_error(message);
}

int RunTests() {
  using namespace nexora::core::diagnostics;

  const TraceId trace{0x1111222233334444ULL, 0xaaaabbbbccccddddULL};
  const TraceEvent io{trace, 1, 0, TraceCategory::Io, 100, 150, 4096,
                      "streaming.plugin", "asset://hero"};
  const TraceEvent cook{trace, 2, 1, TraceCategory::CookArtifact, 160, 220, 2048,
                        "streaming.plugin", "asset://hero"};
  const TraceEvent gpu{trace, 3, 2, TraceCategory::GpuUpload, 230, 260, 2048,
                       "streaming.plugin", "asset://hero"};
  const TraceEvent network{trace, 4, 0, TraceCategory::Network, 180, 200, 512,
                           "network.plugin", "session://42"};

  const auto encoded = EncodeTraceEvent(io);
  const auto decoded = DecodeTraceEvent(encoded);
  Require(decoded && decoded->trace == trace && decoded->span_id == 1 &&
              decoded->category == TraceCategory::Io && decoded->plugin_id == "streaming.plugin" &&
              decoded->resource == "asset://hero",
          "trace wire round-trip failed");
  auto truncated = encoded;
  truncated.pop_back();
  Require(!DecodeTraceEvent(truncated), "truncated trace packet was accepted");
  auto bad_magic = encoded;
  bad_magic.front() = std::byte{0};
  Require(!DecodeTraceEvent(bad_magic), "trace packet with invalid magic was accepted");

  const MetricSample metric{trace, 240, 64ULL * 1024ULL * 1024ULL, 700, 8192, 1024,
                            "linux-server", "streaming.plugin"};
  const auto metric_wire = EncodeMetricSample(metric);
  const auto metric_round_trip = DecodeMetricSample(metric_wire);
  Require(metric_round_trip && metric_round_trip->trace == trace &&
              metric_round_trip->device_id == "linux-server" &&
              metric_round_trip->gpu_time_ns == 700,
          "metric wire round-trip failed");

  TraceAggregator aggregator;
  Require(aggregator.Ingest(gpu) && aggregator.Ingest(network) && aggregator.Ingest(io) &&
              aggregator.Ingest(cook) && aggregator.Ingest(metric),
          "headless trace aggregation rejected valid packets");
  Require(!aggregator.Ingest(io), "duplicate span id was accepted");

  const auto correlation = aggregator.Correlate(trace);
  Require(correlation.events.size() == 4 && correlation.metrics.size() == 1 &&
              correlation.events.front().span_id == 1 && correlation.streaming_chain,
          "trace correlation did not order spans or detect IO/cook/GPU chain");
  const auto stream_cost = correlation.plugin_costs.at("streaming.plugin");
  Require(stream_cost.span_time_ns == 140 && stream_cost.bytes == 8192 &&
              stream_cost.gpu_time_ns == 700 &&
              stream_cost.resident_memory_bytes == 64ULL * 1024ULL * 1024ULL &&
              stream_cost.io_bytes == 8192 && stream_cost.network_bytes == 1024,
          "PluginID cost attribution failed");
  const auto network_cost = correlation.plugin_costs.at("network.plugin");
  Require(network_cost.span_time_ns == 20 && network_cost.bytes == 512,
          "network trace attribution failed");

  const TraceId other{9, 9};
  Require(aggregator.Ingest({other, 1, 0, TraceCategory::Memory, 1, 2, 0,
                             "memory.plugin", "heap://main"}) &&
              aggregator.TraceCount() == 2,
          "aggregator did not isolate trace ids");

  const TraceId mismatch{7, 7};
  TraceAggregator mismatch_aggregator;
  Require(mismatch_aggregator.Ingest(
              {mismatch, 1, 0, TraceCategory::Io, 1, 2, 1, "p", "asset://a"}) &&
              mismatch_aggregator.Ingest(
                  {mismatch, 2, 1, TraceCategory::CookArtifact, 3, 4, 1, "p", "asset://b"}) &&
              mismatch_aggregator.Ingest(
                  {mismatch, 3, 2, TraceCategory::GpuUpload, 5, 6, 1, "p", "asset://b"}) &&
              !mismatch_aggregator.Correlate(mismatch).streaming_chain,
          "unrelated resources were falsely correlated into one streaming hitch");
  return 0;
}
} // namespace

int main() {
  try {
    return RunTests();
  } catch (const std::exception &) {
    return 1;
  }
}
