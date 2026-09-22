#pragma once

#include "Nexora/Core/Api.h"
#include "Nexora/Core/EventBus.h"
#include "Nexora/Core/JobSystem.h"
#include "Nexora/Core/Log.h"
#include "Nexora/Core/Time.h"

#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

namespace nexora::core {

[[nodiscard]] inline std::uint64_t MonotonicNanoseconds() noexcept {
  return static_cast<std::uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(
                                        std::chrono::steady_clock::now().time_since_epoch())
                                        .count());
}

// PCG32 with an explicitly versioned sequence, suitable for deterministic replay.
class RandomStream final {
public:
  static constexpr std::uint32_t kVersion = 1;
  explicit RandomStream(std::uint64_t seed = 0, std::uint64_t sequence = 1) {
    Seed(seed, sequence);
  }
  void Seed(std::uint64_t seed, std::uint64_t sequence = 1) {
    state_ = 0;
    increment_ = (sequence << 1U) | 1U;
    (void)NextU32();
    state_ += seed;
    (void)NextU32();
  }
  [[nodiscard]] std::uint32_t NextU32() {
    const auto old = state_;
    state_ = old * 6364136223846793005ULL + increment_;
    const auto shifted = static_cast<std::uint32_t>(((old >> 18U) ^ old) >> 27U);
    const auto rotation = static_cast<std::uint32_t>(old >> 59U);
    return (shifted >> rotation) | (shifted << ((0U - rotation) & 31U));
  }
  [[nodiscard]] float NextFloat() {
    return static_cast<float>(NextU32() >> 8U) * (1.0F / 16777216.0F);
  }

private:
  std::uint64_t state_{}, increment_{};
};

class Configuration final {
public:
  bool Set(std::string key, std::string value) {
    if (key.empty())
      return false;
    std::lock_guard lock{mutex_};
    values_.insert_or_assign(std::move(key), std::move(value));
    return true;
  }
  [[nodiscard]] std::optional<std::string> Get(std::string_view key) const {
    std::lock_guard lock{mutex_};
    auto it = values_.find(std::string{key});
    return it == values_.end() ? std::nullopt : std::optional<std::string>{it->second};
  }
  bool Remove(std::string_view key) {
    std::lock_guard lock{mutex_};
    return values_.erase(std::string{key}) != 0;
  }

private:
  mutable std::mutex mutex_;
  std::unordered_map<std::string, std::string> values_;
};

// A scoped timer that, on destruction, hands (name, elapsed nanoseconds) to a
// globally registered sink if one is set. This is the marker *emission*
// hook the roadmap asks for, not a profiler: aggregation, a HUD, a
// chrome://tracing JSON writer, or forwarding onto EventBus all belong
// inside the function you register with SetSink, not in this class.
//
// The sink storage lives in Services.cpp (an exported NEXORA_CORE_API
// function), not as a function-local static in this header: in the Modular
// link mode every shared library that includes this header is built with
// hidden inline visibility, so a header-only static would give each DSO
// (Core, Runtime, a plugin, the host) its own separate copy -- a sink
// registered by the host would silently never see markers created inside
// Core or a plugin. Routing through one exported function keeps it truly
// global across module boundaries.
class NEXORA_CORE_API ProfilingMarker final {
public:
  using SinkFunction = void (*)(std::string_view name, std::uint64_t nanoseconds);

  // A plain function pointer (not std::function) so the sink can be read
  // lock-free from every marker's destructor. The default, nullptr, makes
  // this a zero-overhead scoped timer exactly like before this hook existed.
  // Not meant to be changed concurrently with steady-state marker traffic:
  // set it once during startup, before other threads start creating markers.
  static void SetSink(SinkFunction sink) noexcept;

  explicit ProfilingMarker(std::string_view name) noexcept
      : name_(name), start_(MonotonicNanoseconds()) {}
  ~ProfilingMarker();
  ProfilingMarker(const ProfilingMarker &) = delete;
  ProfilingMarker &operator=(const ProfilingMarker &) = delete;
  [[nodiscard]] std::string_view Name() const noexcept { return name_; }
  [[nodiscard]] std::uint64_t ElapsedNanoseconds() const noexcept {
    return MonotonicNanoseconds() - start_;
  }

private:
  std::string_view name_;
  std::uint64_t start_;
};
} // namespace nexora::core
