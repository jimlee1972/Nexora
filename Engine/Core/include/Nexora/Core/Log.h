#pragma once

#include "Nexora/Core/Api.h"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace nexora::core {

enum class LogLevel : std::uint8_t { Trace, Debug, Info, Warning, Error, Fatal };

struct LogRecord final {
  std::chrono::system_clock::time_point timestamp;
  LogLevel level{LogLevel::Info};
  std::string category;
  std::string message;
  std::uint64_t thread_id{};
};

class NEXORA_CORE_API AsyncLogService final {
public:
  explicit AsyncLogService(std::size_t crash_ring_capacity = 256);
  ~AsyncLogService();
  AsyncLogService(const AsyncLogService &) = delete;
  AsyncLogService &operator=(const AsyncLogService &) = delete;

  void Start();
  void Stop();
  void Write(LogLevel level, std::string category, std::string message);
  void Flush();
  [[nodiscard]] std::vector<LogRecord> CrashRingSnapshot() const;

private:
  struct Implementation;
  std::unique_ptr<Implementation> implementation_;
};

} // namespace nexora::core
