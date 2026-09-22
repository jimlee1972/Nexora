#include "Nexora/Core/Services.h"

#include <atomic>

namespace nexora::core {
namespace {
std::atomic<ProfilingMarker::SinkFunction> &ProfilingSinkStorage() noexcept {
  static std::atomic<ProfilingMarker::SinkFunction> sink{nullptr};
  return sink;
}
} // namespace

void ProfilingMarker::SetSink(SinkFunction sink) noexcept {
  ProfilingSinkStorage().store(sink, std::memory_order_release);
}

ProfilingMarker::~ProfilingMarker() {
  if (const auto sink = ProfilingSinkStorage().load(std::memory_order_acquire))
    sink(name_, ElapsedNanoseconds());
}
} // namespace nexora::core
